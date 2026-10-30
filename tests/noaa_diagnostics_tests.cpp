/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2026 RF StateSide LLC
 *
 * This file is part of RayNmeaSimulator.
 *
 * RayNmeaSimulator is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Affero General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any
 * later version.
 *
 * RayNmeaSimulator is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General
 * Public License along with RayNmeaSimulator.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include <QtTest>
#include <QTemporaryDir>
#include <QNetworkProxyFactory>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSslError>
#include <QComboBox>
#include <QPushButton>
#include <QDialog>
#include <QMutex>
#include <cstring>
#include "nationaldatabuoycenter.h"
#include "generatorweather.h"
namespace {
struct Record { QtMsgType level; QString text; };
QList<Record> records; QMutex logMutex; QtMessageHandler previousHandler;
void capture(QtMsgType type, const QMessageLogContext &context, const QString &text) {
    if (QString::fromLatin1(context.category)=="raynmea.weather.ndbc") {
        QMutexLocker lock(&logMutex); records.append({type,text});
    }
    if (previousHandler) previousHandler(type,context,text);
}
QString logs(int level=-1) {
    QMutexLocker lock(&logMutex); QStringList lines;
    for (const auto &r:records) if(level<0 || r.level==level) lines<<r.text;
    return lines.join('\n');
}
class OfflineProxy final : public QNetworkProxyFactory {
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &q) override {
        if(q.peerHostName()=="127.0.0.1" || q.peerHostName()=="localhost") return {QNetworkProxy(QNetworkProxy::NoProxy)};
        return {QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9)};
    }
};
QByteArray observation(int age=0, const QByteArray &fields="280 7 9 0.7 3 3.4 268 1006.1 16.5 14.4 12.6 MM MM MM") {
    return "#YY MM DD hh mm WDIR WSPD GST WVHT DPD APD MWD PRES ATMP WTMP DEWP VIS PTDY TIDE\n#units\n"+
        QDateTime::currentDateTimeUtc().addSecs(-age).toString("yyyy MM dd HH mm ").toUtf8()+fields+"\n";
}
struct Plan {
    QNetworkReply::NetworkError error=QNetworkReply::NoError;
    int status=200; QByteArray body=observation(); bool hang=false, tls=false;
};
class FixtureReply final : public QNetworkReply {
    QByteArray body; qint64 offset=0;
public:
    FixtureReply(const QNetworkRequest &r, Plan plan, QObject *parent):QNetworkReply(parent),body(plan.body) {
        setRequest(r); setUrl(r.url()); open(QIODevice::ReadOnly);
        QTimer::singleShot(0,this,[this,plan] {
            if (isFinished() || plan.hang) return;
            if (plan.status) setAttribute(QNetworkRequest::HttpStatusCodeAttribute,plan.status);
            if (plan.tls) emit sslErrors({QSslError(QSslError::SelfSignedCertificate)});
            if (plan.error!=NoError) setError(plan.error,"TOP-SECRET-NDBC Authorization: Bearer private-value https://secret.example/?api_key=TOP-SECRET-NDBC");
            setFinished(true); emit readyRead(); emit finished();
        });
    }
    void abort() override {
        if(isFinished()) return;
        setError(OperationCanceledError,"TOP-SECRET-NDBC cancelled URL"); setFinished(true); emit finished();
    }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return body.size()-offset+QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *buffer,qint64 max) override {
        const auto n=qMin(max,body.size()-offset); if(n<=0)return -1;
        std::memcpy(buffer,body.constData()+offset,size_t(n)); offset+=n;return n;
    }
};
class FixtureManager final : public QNetworkAccessManager {
public:
    QList<Plan> plans; QList<QPointer<QNetworkReply>> replies; QList<QNetworkRequest> requests;
protected:
    QNetworkReply *createRequest(Operation,const QNetworkRequest &r,QIODevice*) override {
        requests<<r; Q_ASSERT(!plans.isEmpty()); auto *reply=new FixtureReply(r,plans.takeFirst(),this); replies<<reply; return reply;
    }
};
const QUrl privateStations("https://secret.example/stations?api_key=TOP-SECRET-NDBC");
const QUrl privateData("https://secret.example/data/?api_key=TOP-SECRET-NDBC");
}
class NoaaDiagnosticsTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { previousHandler=qInstallMessageHandler(capture); }
    void init() { QMutexLocker lock(&logMutex); records.clear(); }
    void cleanup() {
        const auto text=logs();
        for(const QString secret:{"TOP-SECRET-NDBC","api_key","Authorization","Bearer","secret.example","<html"})
            QVERIFY2(!text.contains(secret),"Diagnostic leaked synthetic private data");
    }
    void cleanupTestCase() { qInstallMessageHandler(previousHandler); }
    void networkErrors_data() {
        QTest::addColumn<int>("code");QTest::addColumn<QString>("cause");QTest::addColumn<bool>("tls");
        QTest::newRow("dns")<<int(QNetworkReply::HostNotFoundError)<<QString("dns-error")<<false;
        QTest::newRow("refused")<<int(QNetworkReply::ConnectionRefusedError)<<QString("connection-refused")<<false;
        QTest::newRow("closed")<<int(QNetworkReply::RemoteHostClosedError)<<QString("connection-closed")<<false;
        QTest::newRow("tls")<<int(QNetworkReply::SslHandshakeFailedError)<<QString("tls-error")<<true;
        QTest::newRow("timeout")<<int(QNetworkReply::TimeoutError)<<QString("timeout")<<false;
        QTest::newRow("connectivity")<<int(QNetworkReply::TemporaryNetworkFailureError)<<QString("network-error")<<false;
    }
    void networkErrors() {
        QFETCH(int,code);QFETCH(QString,cause);QFETCH(bool,tls);
        FixtureManager manager;manager.plans<<Plan{QNetworkReply::NetworkError(code),0,{},false,tls};
        NationalDataBuoyCenter p(nullptr,privateStations,privateData,&manager);QSignalSpy errors(&p,&NationalDataBuoyCenter::errorOccurred);
        p.fetchStationData("44098");QTRY_COMPARE(errors.count(),1);
        const auto warning=logs(QtWarningMsg);QVERIFY(warning.contains("stage=observation station=44098"));
        QVERIFY(warning.contains("result="+cause));QVERIFY(warning.contains("elapsed_ms="));QVERIFY(warning.contains("http=none network="));
        QVERIFY(QRegularExpression("request=[1-9][0-9]*").match(warning).hasMatch());
        if(tls)QVERIFY(warning.contains("tls_codes="));
        QCOMPARE(manager.requests[0].transferTimeout(),15000);
    }
    void httpAndParserFailures() {
        FixtureManager manager;
        manager.plans<<Plan{QNetworkReply::ServiceUnavailableError,503,"TOP-SECRET-NDBC"}
                     <<Plan{QNetworkReply::ContentNotFoundError,404,{}}
                     <<Plan{QNetworkReply::NoError,200,"<html>TOP-SECRET-NDBC</html>"}
                     <<Plan{QNetworkReply::NoError,200,"<html>TOP-SECRET-NDBC</html>"}
                     <<Plan{QNetworkReply::NoError,200,observation(0,"MM MM MM MM MM MM MM MM MM MM MM MM MM MM")};
        NationalDataBuoyCenter p(nullptr,privateStations,privateData,&manager);QSignalSpy errors(&p,&NationalDataBuoyCenter::errorOccurred);
        p.getNearestStations(42,-70,80);QTRY_COMPARE(errors.count(),1);
        QVERIFY(logs(QtWarningMsg).contains("stage=discovery"));QVERIFY(logs(QtWarningMsg).contains("http=503"));QVERIFY(logs(QtWarningMsg).contains("result=http-error"));
        p.fetchStationData("44098");QTRY_COMPARE(errors.count(),2);QVERIFY(logs(QtWarningMsg).contains("http=404"));
        p.getNearestStations(42,-70,80);QTRY_COMPARE(errors.count(),3);QVERIFY(logs(QtWarningMsg).contains("detail=invalid-station-xml"));
        p.fetchStationData("44098");QTRY_COMPARE(errors.count(),4);QVERIFY(logs(QtWarningMsg).contains("detail=invalid-standard-met"));
        p.fetchStationData("44098");QTRY_COMPARE(errors.count(),5);QVERIFY(logs(QtWarningMsg).contains("result=no-usable-observations detail=no-valid-measurements"));
    }
    void explicitCancellationAndSuperseding() {
        FixtureManager manager;manager.plans<<Plan{QNetworkReply::NoError,0,{},true}<<Plan{QNetworkReply::NoError,0,{},true};
        NationalDataBuoyCenter p(nullptr,privateStations,privateData,&manager);
        QSignalSpy errors(&p,&NationalDataBuoyCenter::errorOccurred),found(&p,&NationalDataBuoyCenter::nearestStationsFound);
        p.getNearestStations(0,0,80);p.getNearestStations(42,-70,80);p.cancel();QTest::qWait(20);
        QCOMPARE(errors.count(),0);QCOMPARE(found.count(),0);QVERIFY(logs(QtWarningMsg).isEmpty());
        QVERIFY(logs(QtInfoMsg).contains("reason=superseded"));QVERIFY(logs(QtInfoMsg).contains("reason=explicit"));
        const auto starts=logs(QtDebugMsg).split('\n').filter("result=started");QCOMPARE(starts.size(),2);QVERIFY(starts[0]!=starts[1]);
    }
    void boundedTimeout() {
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server,&QTcpServer::newConnection,&server,[&] {
            auto *socket=server.nextPendingConnection();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            connect(socket,&QTcpSocket::readyRead,socket,[socket] { socket->readAll(); }); // Hold the response, exercising Qt's real deadline.
        });
        const QUrl url(QString("http://127.0.0.1:%1/").arg(server.serverPort()));
        NationalDataBuoyCenter p(nullptr,url,url);QSignalSpy errors(&p,&NationalDataBuoyCenter::errorOccurred);
        QElapsedTimer elapsed;elapsed.start();p.fetchStationData("44098");
        QTRY_COMPARE_WITH_TIMEOUT(errors.count(),1,18000);
        QVERIFY(elapsed.elapsed()>=14000);QVERIFY(elapsed.elapsed()<18000);
        QVERIFY(logs(QtWarningMsg).contains("result=timeout"));QVERIFY(logs(QtWarningMsg).contains("network=OperationCanceledError(5)") || logs(QtWarningMsg).contains("network=TimeoutError(4)"));
    }
    void successPartialAndStale() {
        FixtureManager manager;
        manager.plans<<Plan{QNetworkReply::NoError,200,"<stations><station id='44098' name='Fixture' lat='42' lon='-70'/></stations>"}
                     <<Plan{QNetworkReply::NoError,200,observation(0,"MM MM MM 0 0 0 0 MM MM MM MM MM MM MM")}
                     <<Plan{QNetworkReply::NoError,200,observation(20000)};
        NationalDataBuoyCenter p(nullptr,privateStations,privateData,&manager);
        QSignalSpy found(&p,&NationalDataBuoyCenter::nearestStationsFound),received(&p,&NationalDataBuoyCenter::observationFetched);
        p.getNearestStations(42,-70,80);QTRY_COMPARE(found.count(),1);
        p.fetchStationData("44098");QTRY_COMPARE(received.count(),1);QVERIFY(logs(QtWarningMsg).isEmpty());
        QCOMPARE(received[0][0].value<WeatherObservation>().waveHeight,0.);
        QVERIFY(logs(QtDebugMsg).contains("result=completed"));QVERIFY(logs(QtDebugMsg).contains("missing_fields="));
        p.fetchStationData("44098");QTRY_COMPARE(received.count(),2);QVERIFY(logs(QtWarningMsg).contains("result=stale-observation"));
        QVERIFY(!received[1][0].value<WeatherObservation>().fresh()); // Widget remains the authority for rejecting stale input.
    }
    void invalidInputSanitized() {
        NationalDataBuoyCenter p;QSignalSpy errors(&p,&NationalDataBuoyCenter::errorOccurred);
        p.fetchStationData("TOP-SECRET-NDBC?api_key=TOP-SECRET-NDBC");p.getNearestStations(qQNaN(),0,80);
        QCOMPARE(errors.count(),2);QVERIFY(logs(QtWarningMsg).contains("station=<invalid>"));QVERIFY(logs(QtWarningMsg).contains("result=invalid-search-input"));
    }
    void widgetCancellationAndSelection() {
        GeneratorWeather w(1,"Weather");auto *source=w.findChild<QComboBox*>("comboBoxApiSource");auto *fetch=w.findChild<QPushButton*>("pushButtonFetchWeather");
        auto *provider=w.findChild<NationalDataBuoyCenter*>();source->setCurrentIndex(2);
        fetch->click();w.setLongitude(-70);QVERIFY(logs(QtInfoMsg).contains("reason=coordinates-changed"));
        fetch->click();w.setProperty("GeneratorModeActive",false);QVERIFY(logs(QtInfoMsg).contains("reason=mode-changed"));
        w.setProperty("GeneratorModeActive",true);fetch->click();provider->cancel();
        Station station{};station.id="44098";station.name="Fixture";station.latitude=42;station.longitude=-70;
        QTimer::singleShot(0,&w,[&] { auto *d=w.findChild<QDialog*>("weatherStationPicker");QVERIFY(d);d->reject(); });
        w.nearestStationsFound({station});QVERIFY(logs(QtInfoMsg).contains("stage=selection"));
        fetch->click();provider->cancel();
        QTimer::singleShot(0,&w,[&] { auto *d=w.findChild<QDialog*>("weatherStationPicker");QVERIFY(d);d->accept(); });
        w.nearestStationsFound({station});provider->cancel();QVERIFY(logs(QtDebugMsg).contains("result=selected"));
        fetch->click(); // Ends the still-active workflow; no network failure is manufactured.
        QVERIFY(logs(QtWarningMsg).isEmpty());
        fetch->click();provider->cancel();
        QTimer::singleShot(0,&w,[&] {source->setCurrentIndex(0);});
        w.nearestStationsFound({station});QVERIFY(logs(QtDebugMsg).contains("result=ignored-superseded-selection"));
        source->setCurrentIndex(2);fetch->click();provider->cancel();w.nearestStationsFound({});
        QVERIFY(logs(QtWarningMsg).contains("stage=selection"));QVERIFY(logs(QtWarningMsg).contains("result=no-nearby-stations"));
    }
    void widgetSourceChangeAndExpiry() {
        GeneratorWeather w(1,"Weather");auto *source=w.findChild<QComboBox*>("comboBoxApiSource");auto *fetch=w.findChild<QPushButton*>("pushButtonFetchWeather");
        source->setCurrentIndex(2);fetch->click();source->setCurrentIndex(0);
        QVERIFY(logs(QtInfoMsg).contains("reason=source-changed"));QVERIFY(logs(QtWarningMsg).isEmpty());
        source->setCurrentIndex(2);WeatherObservation o;o.provider="NOAA NDBC";o.station="44098";
        o.observed=QDateTime::currentDateTimeUtc();o.retrieved=o.observed;o.maximumAgeSeconds=1;o.waveHeight=2;w.acceptObservation(o);
        QCOMPARE(w.WaveHeight(),2.);QTRY_COMPARE_WITH_TIMEOUT(w.WaveHeight(),N2kDoubleNA,2500);
        QVERIFY(logs(QtWarningMsg).contains("stage=freshness"));QVERIFY(logs(QtWarningMsg).contains("result=expired"));
    }
    void realHttpLoopback() {
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));int status=503;
        connect(&server,&QTcpServer::newConnection,&server,[&] {
            auto *s=server.nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
            connect(s,&QTcpSocket::readyRead,s,[&,s] {s->readAll();const QByteArray body=status==200?observation():QByteArray("TOP-SECRET-NDBC");
                s->write("HTTP/1.1 "+QByteArray::number(status)+" Status\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);s->disconnectFromHost();});
        });
        QUrl url(QString("http://127.0.0.1:%1/").arg(server.serverPort()));NationalDataBuoyCenter p(nullptr,url,url);
        QSignalSpy errors(&p,&NationalDataBuoyCenter::errorOccurred),received(&p,&NationalDataBuoyCenter::observationFetched);
        p.fetchStationData("44098");QTRY_COMPARE(errors.count(),1);QVERIFY(logs(QtWarningMsg).contains("http=503"));QVERIFY(logs(QtWarningMsg).contains("result=http-error"));
        status=200;p.fetchStationData("44098");QTRY_COMPARE(received.count(),1);
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile(QDir::currentPath()+"/ndbc-diagnostics-XXXXXX");if(!profile.isValid())return 2;
    qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";
    QList<char*> pointers;for(auto &a:args)pointers<<a.data();int count=pointers.size();pointers<<nullptr;
    QApplication app(count,pointers.data());app.setStyle("Fusion");
    if(argc==2 && QByteArray(argv[1])=="--live-noaa") {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::DefaultProxy);NationalDataBuoyCenter provider;QEventLoop loop;bool received=false;
        QObject::connect(&provider,&NationalDataBuoyCenter::observationFetched,&loop,[&](const WeatherObservation &o) {
            received=true;qInfo()<<"Controlled NOAA qualification: received observation; fresh="<<o.fresh();loop.quit();
        });
        QObject::connect(&provider,&NationalDataBuoyCenter::errorOccurred,&loop,[&] {qInfo()<<"Controlled NOAA qualification: request failed; diagnostic above; no automatic retry.";loop.quit();});
        QTimer::singleShot(20000,&loop,&QEventLoop::quit);provider.fetchStationData("44098");loop.exec();provider.cancel();return received?0:1;
    }
    QNetworkProxyFactory::setApplicationProxyFactory(new OfflineProxy);
    NoaaDiagnosticsTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "noaa_diagnostics_tests.moc"
