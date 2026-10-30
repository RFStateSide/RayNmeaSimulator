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
#include <QTabWidget>
#include <QMessageBox>
#include <QAction>
#include <QUrlQuery>
#include <cstring>
#include "nationaldatabuoycenter.h"
#include "generatorweather.h"
#include "mainwindow.h"
#include "appsettings.h"
#include "environmentalinstruments.h"
#include "pgntreecontroller.h"
#include "signalkpublisher.h"
#include "themes.h"
#include "N2kMessages.h"
namespace {
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
    bool aborted=false;
    void complete(const QByteArray &bytes,int status=200) {body=bytes;offset=0;setAttribute(QNetworkRequest::HttpStatusCodeAttribute,status);if(status>=400)setError(ContentAccessDenied,"Fixture HTTP failure");setFinished(true);emit readyRead();emit finished();}
    void abort() override {
        aborted=true;
        if(property("holdAbort").toBool())return;
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
namespace {
QCheckBox *automatic(GeneratorWeather &w) { return w.findChild<QCheckBox*>("weatherAutoRefresh"); }
QCheckBox *retention(GeneratorWeather &w) { return w.findChild<QCheckBox*>("weatherRetainObservations"); }
QTimer *scheduler(GeneratorWeather &w) { return w.findChild<QTimer*>("weatherRefreshTimer"); }
QComboBox *source(GeneratorWeather &w) { return w.findChild<QComboBox*>("comboBoxApiSource"); }
QPushButton *fetch(GeneratorWeather &w) { return w.findChild<QPushButton*>("pushButtonFetchWeather"); }
void tick(GeneratorWeather &w) { QMetaObject::invokeMethod(scheduler(w),"timeout",Qt::DirectConnection); }
WeatherObservation sample(QString station="44098", int age=0, int maximum=7200) {
    WeatherObservation o; o.provider="NOAA NDBC";o.station=station;o.location="Fixture "+station;
    o.observed=QDateTime::currentDateTimeUtc().addSecs(-age);o.retrieved=QDateTime::currentDateTimeUtc();
    o.maximumAgeSeconds=maximum;o.windSpeed=7;o.waveHeight=2;o.dominantWavePeriod=6;return o;
}
QByteArray stations() { return "<stations><station id='iosn3' name='Near' lat='42' lon='-70'/><station id='44098' name='Next' lat='42.1' lon='-70'/><station id='abc12' name='Third' lat='42.2' lon='-70'/><station id='def34' name='Fourth' lat='42.3' lon='-70'/></stations>"; }
void confirm() { QTimer::singleShot(0, [] {for(auto *w:QApplication::topLevelWidgets())if(auto *b=qobject_cast<QMessageBox*>(w))b->button(QMessageBox::Yes)->click();}); }
}

namespace {
QByteArray owm(double lat=42,double lon=-70,int age=0) {
    return QJsonDocument(QJsonObject{{"cod",200},{"coord",QJsonObject{{"lat",lat},{"lon",lon}}},
        {"main",QJsonObject{{"temp",289.5},{"humidity",65},{"pressure",1008}}},
        {"wind",QJsonObject{{"speed",7},{"deg",270}}},{"dt",QDateTime::currentDateTimeUtc().addSecs(-age).toSecsSinceEpoch()}}).toJson();
}
FixtureReply *pending(FixtureManager &manager) {return static_cast<FixtureReply*>(manager.replies.last().data());}
void configure(GeneratorWeather &w,int provider,bool autoOn=false) {source(w)->setCurrentIndex(provider);w.setOpenWeatherMapApiKey("synthetic-key");w.setLatitude(42);w.setLongitude(-70);automatic(w)->setChecked(autoOn);}
void selectStation(GeneratorWeather &w,QTimer &picker) {picker.setInterval(1);QObject::connect(&picker,&QTimer::timeout,&w,[&w]{if(auto *d=w.findChild<QDialog*>("weatherStationPicker"))d->accept();});picker.start();}
void hold(FixtureManager &manager){manager.plans<<Plan{QNetworkReply::NoError,200,{},true};}
}
class WeatherMovementTests : public QObject {
    Q_OBJECT
private slots:
    void retrieval_data() {
        QTest::addColumn<int>("provider");QTest::addColumn<bool>("moving");QTest::addColumn<bool>("scheduled");
        for(int p:{1,2})for(bool moving:{false,true})for(bool scheduled:{false,true})
            QTest::newRow(qPrintable(QString("%1-%2-%3").arg(p==1?"OWM":"NDBC",moving?"moving":"stationary",scheduled?"automatic":"manual")))<<p<<moving<<scheduled;
    }
    void retrieval() {
        QFETCH(int,provider);QFETCH(bool,moving);QFETCH(bool,scheduled);FixtureManager manager;GeneratorWeather w(1,"Weather",nullptr,&manager);configure(w,provider,scheduled);
        hold(manager);QTimer picker;selectStation(w,picker);
        if(scheduled)tick(w);else fetch(w)->click();QCOMPARE(manager.requests.size(),1);auto *first=pending(manager);first->setProperty("holdAbort",true);
        QTimer travel;int steps=0;connect(&travel,&QTimer::timeout,&w,[&]{++steps;w.setLatitude(42+steps*.00001);w.setLongitude(-70+steps*.00001);});if(moving)travel.start(1);
        if(moving) QTRY_VERIFY_WITH_TIMEOUT(steps>2,1000);
        QVERIFY(!first->aborted);QVERIFY(!first->isFinished());
        if(provider==2){hold(manager);first->complete(stations());QTRY_COMPARE(manager.requests.size(),2);first=pending(manager);QTest::qWait(25);}
        first->complete(provider==1?owm():observation());QTRY_VERIFY(w.hasFreshObservation());travel.stop();if(moving)QVERIFY(steps>2);
        QCOMPARE(w.WindSpeed(),7.);QVERIFY(w.lastSuccessfulFetch().isValid());QCOMPARE(manager.requests.size(),provider==1?1:2);
        // Request/observation location stays tied to the initial request/station.
        QCOMPARE(w.fieldOrigin("WindSpeed").latitude,42.);
        if(provider==1){QUrlQuery q(manager.requests.first().url());QCOMPARE(q.queryItemValue("lat"),QString("42"));QCOMPARE(q.queryItemValue("lon"),QString("-70"));}
    }
    void relocation_data(){QTest::addColumn<int>("provider");QTest::newRow("OWM")<<1;QTest::newRow("NDBC")<<2;}
    void relocation() {
        QFETCH(int,provider);FixtureManager manager;GeneratorWeather w(1,"Weather",nullptr,&manager);configure(w,provider,true);w.setWindSpeed(3);hold(manager);tick(w);
        auto *old=pending(manager);old->setProperty("holdAbort",true);
        w.setLatitude(43);QVERIFY(old->aborted);QVERIFY(scheduler(w)->isActive());
        old->complete(provider==1?owm():stations());QTest::qWait(10);QCOMPARE(w.WindSpeed(),3.);QCOMPARE(manager.requests.size(),1);
        for(int i=0;i<50;++i)w.setLatitude(43+i*.00001);QCOMPARE(manager.requests.size(),1);QVERIFY(scheduler(w)->remainingTime()>50000);
        w.cancelWeather();
    }
    void outOfOrderAndRecovery_data(){relocation_data();}
    void outOfOrderAndRecovery() {
        QFETCH(int,provider);FixtureManager manager;GeneratorWeather w(1,"Weather",nullptr,&manager);configure(w,provider);
        QTimer picker;selectStation(w,picker);hold(manager);fetch(w)->click();auto *old=pending(manager);old->setProperty("holdAbort",true);fetch(w)->click();QVERIFY(old->aborted);
        hold(manager);fetch(w)->click();auto *current=pending(manager);
        if(provider==2){hold(manager);current->complete(stations());QTRY_COMPARE(manager.requests.size(),3);current=pending(manager);}
        current->complete(provider==1?owm():observation());QTRY_COMPARE(w.WindSpeed(),7.);const auto success=w.lastSuccessfulFetch();
        old->complete(provider==1?owm(42,-70,8000):stations());QTest::qWait(10);QCOMPARE(w.WindSpeed(),7.);QCOMPARE(w.lastSuccessfulFetch(),success);
    }
    void failureRateLimitAndRecovery_data(){relocation_data();}
    void failureRateLimitAndRecovery() {
        QFETCH(int,provider);FixtureManager manager;GeneratorWeather w(1,"Weather",nullptr,&manager);configure(w,provider,true);hold(manager);tick(w);auto *reply=pending(manager);
        reply->complete({},429);QTRY_VERIFY(w.observationStatus().contains("failed"));QVERIFY(scheduler(w)->isActive());QVERIFY(scheduler(w)->remainingTime()>50000);
        for(int i=0;i<30;++i)w.setLongitude(-70+i*.00001);QCOMPARE(manager.requests.size(),1);
        hold(manager);tick(w);reply=pending(manager);if(provider==2){hold(manager);reply->complete(stations());QTRY_COMPARE(manager.requests.size(),3);reply=pending(manager);}
        reply->complete(provider==1?owm():observation());QTRY_COMPARE(w.WindSpeed(),7.);QVERIFY(scheduler(w)->isActive());
    }
    void timeoutAndNoOverlap() {
        FixtureManager manager;GeneratorWeather w(1,"Weather",nullptr,&manager);configure(w,1,true);hold(manager);tick(w);
        for(int i=0;i<30;++i){w.setLatitude(42+i*.00001);tick(w);}QCOMPARE(manager.requests.size(),1);
        auto *deadline=w.findChild<QTimer*>("weatherCycleDeadline");QCOMPARE(deadline->interval(),60000);QVERIFY(QMetaObject::invokeMethod(deadline,"timeout",Qt::DirectConnection));QVERIFY(pending(manager)->aborted);QVERIFY(scheduler(w)->isActive());
    }
    void unrelatedAndStaleLocationRejected() {
        FixtureManager manager;GeneratorWeather w(1,"Weather",nullptr,&manager);configure(w,1);w.setWindSpeed(3);hold(manager);fetch(w)->click();pending(manager)->complete(owm(0,0));QTest::qWait(5);QCOMPARE(w.WindSpeed(),3.);QVERIFY(!w.lastSuccessfulFetch().isValid());
        hold(manager);fetch(w)->click();pending(manager)->complete(owm(42,-70,8000));QTest::qWait(5);QCOMPARE(w.WindSpeed(),3.);QVERIFY(!w.lastSuccessfulFetch().isValid());
        hold(manager);fetch(w)->click();pending(manager)->complete(owm());QTRY_COMPARE(w.WindSpeed(),7.);
        w.setLongitude(-60);QCOMPARE(w.WindSpeed(),N2kDoubleNA); // provider-owned, geographically obsolete
    }
    void invalidRequestAndDateline() {
        FixtureManager manager;GeneratorWeather w(1,"Weather",nullptr,&manager);configure(w,1);w.setLatitude(91);fetch(w)->click();QCOMPARE(manager.requests.size(),0);
        w.setLatitude(0);w.setLongitude(179.999);hold(manager);fetch(w)->click();auto *reply=pending(manager);w.setLongitude(-179.999);QVERIFY(!reply->isFinished());reply->complete(owm(0,179.999));QTRY_COMPARE(w.WindSpeed(),7.);
    }
};
int main(int argc,char **argv){QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);QNetworkProxyFactory::setApplicationProxyFactory(new OfflineProxy);WeatherMovementTests t;return QTest::qExec(&t,argc,argv);}
#include "weather_movement_tests.moc"
