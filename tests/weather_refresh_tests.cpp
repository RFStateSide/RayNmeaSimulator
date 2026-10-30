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
class WeatherRefreshTests : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None"); }
    void defaultsAndIntervals() {
        FixtureManager manager;GeneratorWeather w(1,"Weather",nullptr,&manager);
        QVERIFY(!w.automaticRefresh());QVERIFY(w.retainObservations());QCOMPARE(w.refreshIntervalMinutes(),15);
        automatic(w)->setChecked(true);QVERIFY(!scheduler(w)->isActive()); // None
        source(w)->setCurrentIndex(2);QCOMPARE(manager.requests.size(),0);
        auto *interval=w.findChild<QComboBox*>("weatherRefreshInterval");
        for(int i=0;i<5;++i){interval->setCurrentIndex(i);QCOMPARE(scheduler(w)->interval(),interval->currentData().toInt()*60000);QVERIFY(scheduler(w)->isActive());}
        automatic(w)->setChecked(false);QVERIFY(!scheduler(w)->isActive());
    }
    void snapshotAndNoOverlap() {
        FixtureManager manager;manager.plans<<Plan{QNetworkReply::NoError,0,{},true};
        GeneratorWeather w(1,"Weather",nullptr,&manager);source(w)->setCurrentIndex(2);automatic(w)->setChecked(true);w.setLatitude(42);w.setLongitude(-70);
        QSignalSpy discovery(&w,&GeneratorWeather::getNearestStations);tick(w);QCOMPARE(manager.requests.size(),1);
        QCOMPARE(discovery[0][0].toDouble(),42.);QCOMPARE(discovery[0][1].toDouble(),-70.);
        w.setLatitude(42.001);w.setLongitude(-70.001);tick(w);QCOMPARE(manager.requests.size(),1);QVERIFY(!manager.replies[0]->isFinished());
        fetch(w)->click();QVERIFY(manager.replies[0]->isFinished());QVERIFY(scheduler(w)->isActive());
        manager.plans<<Plan{QNetworkReply::NoError,0,{},true};tick(w);QCOMPARE(discovery[1][0].toDouble(),42.001);QCOMPARE(discovery[1][1].toDouble(),-70.001);
        w.cancelWeather();
    }
    void noaa404FallbackAndPartial() {
        FixtureManager manager;manager.plans<<Plan{QNetworkReply::NoError,200,stations()}<<Plan{QNetworkReply::ContentNotFoundError,404,{}}
            <<Plan{QNetworkReply::NoError,200,observation(0,"MM MM MM 0 6 5 270 MM MM MM MM MM MM MM")};
        GeneratorWeather w(1,"Weather",nullptr,&manager);source(w)->setCurrentIndex(2);automatic(w)->setChecked(true);w.setLatitude(42);w.setLongitude(-70);tick(w);
        QTRY_COMPARE(w.WaveHeight(),0.);QCOMPARE(manager.requests.size(),3);QVERIFY(manager.requests[1].url().path().endsWith("iosn3.txt"));QVERIFY(manager.requests[2].url().path().endsWith("44098.txt"));
        QCOMPARE(w.fieldOrigin("WaveHeight").station,QString("44098"));QVERIFY(w.hasFreshObservation());QVERIFY(scheduler(w)->isActive());QCOMPARE(w.WindSpeed(),N2kDoubleNA);
        QVERIFY(w.lastSuccessfulFetch().isValid());QCOMPARE(w.latestObservationTime(),w.observation().observed);QVERIFY(w.latestObservationSource().contains("44098"));QVERIFY(w.nextAutomaticRefresh().isValid());
    }
    void stationAvailabilityPreference() {
        FixtureManager manager;const auto catalog=QByteArray("<stations><station id='iosn3' met='n' lat='42' lon='-70'/><station id='44098' met='y' lat='42.1' lon='-70'/></stations>");
        manager.plans<<Plan{QNetworkReply::NoError,200,catalog}<<Plan{};
        GeneratorWeather w(1,"Weather",nullptr,&manager);source(w)->setCurrentIndex(2);automatic(w)->setChecked(true);w.setLatitude(42);w.setLongitude(-70);tick(w);
        QTRY_VERIFY(w.hasFreshObservation());QCOMPARE(manager.requests.size(),2);QVERIFY(manager.requests[1].url().path().endsWith("44098.txt"));
    }
    void boundedFallbackAndRetryNextCycle() {
        FixtureManager manager;manager.plans<<Plan{QNetworkReply::NoError,200,stations()};
        for(int i=0;i<3;++i)manager.plans<<Plan{QNetworkReply::ContentNotFoundError,404,{}};
        GeneratorWeather w(1,"Weather",nullptr,&manager);source(w)->setCurrentIndex(2);automatic(w)->setChecked(true);w.setLatitude(42);w.setLongitude(-70);tick(w);
        QTRY_VERIFY(w.observationStatus().contains("No usable NOAA"));QCOMPARE(manager.requests.size(),4);QVERIFY(scheduler(w)->isActive());
        manager.plans<<Plan{QNetworkReply::NoError,200,stations()}<<Plan{};tick(w);QTRY_VERIFY(w.hasFreshObservation());
        QCOMPARE(manager.requests.size(),6);QVERIFY(manager.requests[5].url().path().endsWith("iosn3.txt")); // 404 is not permanent
    }
    void networkFailureDoesNotProbeOtherStations() {
        FixtureManager manager;manager.plans<<Plan{QNetworkReply::NoError,200,stations()}<<Plan{QNetworkReply::HostNotFoundError,0,{}};
        GeneratorWeather w(1,"Weather",nullptr,&manager);source(w)->setCurrentIndex(2);automatic(w)->setChecked(true);w.setLatitude(42);w.setLongitude(-70);tick(w);
        QTRY_VERIFY(w.observationStatus().contains("request failed"));QCOMPARE(manager.requests.size(),2);QVERIFY(scheduler(w)->isActive());
    }
    void manualSelectionHasNoFallback() {
        FixtureManager manager;manager.plans<<Plan{QNetworkReply::NoError,200,stations()}<<Plan{QNetworkReply::ContentNotFoundError,404,{}};
        GeneratorWeather w(1,"Weather",nullptr,&manager);source(w)->setCurrentIndex(2);automatic(w)->setChecked(true);w.setLatitude(42);w.setLongitude(-70);
        QTimer picker;picker.setInterval(1);connect(&picker,&QTimer::timeout,&w,[&]{if(auto*d=w.findChild<QDialog*>("weatherStationPicker"))d->accept();});picker.start();fetch(w)->click();
        QTRY_VERIFY(w.observationStatus().contains("request failed"));QVERIFY(w.observationStatus().contains("iosn3"));QVERIFY(w.observationStatus().contains("Observation feed unavailable"));QCOMPARE(manager.requests.size(),2);QVERIFY(scheduler(w)->isActive());
    }
    void provenanceRetentionAndExpiry() {
        GeneratorWeather w(1,"Weather");source(w)->setCurrentIndex(2);auto a=sample("buoyA",0,1);w.acceptObservation(a);
        auto b=sample("buoyB");b.waveHeight=N2kDoubleNA;b.dominantWavePeriod=N2kDoubleNA;b.windSpeed=12;w.acceptObservation(b);
        QCOMPARE(w.WindSpeed(),12.);QCOMPARE(w.WaveHeight(),2.);QVERIFY(w.fieldOrigin("WaveHeight").retained);
        QCOMPARE(w.fieldOrigin("WaveHeight").station,QString("buoyA"));QCOMPARE(w.fieldOrigin("WaveHeight").observed,a.observed);QCOMPARE(w.fieldOrigin("WaveHeight").retrieved,a.retrieved);
        QCOMPARE(w.fieldOrigin("WindSpeed").station,QString("buoyB"));QVERIFY(!w.fieldOrigin("WindSpeed").retained);
        QTRY_COMPARE_WITH_TIMEOUT(w.WaveHeight(),N2kDoubleNA,2200);QCOMPARE(w.WindSpeed(),12.);QVERIFY(w.fieldOrigin("WaveHeight").stale);
        QVERIFY(w.fieldDescription("WaveHeight").contains("buoyA"));QVERIFY(w.fieldDescription("WaveHeight").contains("Stale"));
    }
    void retainOffManualZeroAndProviderChanges() {
        GeneratorWeather w(1,"Weather");source(w)->setCurrentIndex(2);
        QSignalSpy metadata(&w,&GeneratorWeather::ObservationMetadataChanged);w.setSeaTemperature(289);QVERIFY(metadata.count()>0);
        w.setHumidity(0);w.acceptObservation(sample());QCOMPARE(w.Humidity(),0.);
        w.setWaveHeight(2); // same value still explicitly claims manual ownership
        auto o=sample();o.waveHeight=N2kDoubleNA;o.windSpeed=N2kDoubleNA;retention(w)->setChecked(false);w.acceptObservation(o);
        QCOMPARE(w.WaveHeight(),2.);QCOMPARE(w.WindSpeed(),N2kDoubleNA);QCOMPARE(w.Humidity(),0.);
        retention(w)->setChecked(true);w.acceptObservation(sample());source(w)->setCurrentIndex(1);
        auto b=sample();b.provider="OpenWeatherMap";b.station.clear();b.location="Fixture city";b.waveHeight=N2kDoubleNA;w.acceptObservation(b);
        QCOMPARE(w.WaveHeight(),2.);QCOMPARE(w.fieldOrigin("WaveHeight").provider,QString("NOAA NDBC"));QVERIFY(w.fieldOrigin("WaveHeight").retained);
        retention(w)->setChecked(false);QCOMPARE(w.WaveHeight(),N2kDoubleNA);
    }
    void failureRetentionAndModeLifecycle() {
        FixtureManager manager;GeneratorWeather w(1,"Weather",nullptr,&manager);source(w)->setCurrentIndex(2);automatic(w)->setChecked(true);w.acceptObservation(sample());
        w.acceptObservation(sample("stale",20000));QVERIFY(w.fieldOrigin("WaveHeight").retained);
        w.handleWeatherError("Fixture network unavailable");QCOMPARE(w.WaveHeight(),2.);QVERIFY(w.fieldOrigin("WaveHeight").retained);
        manager.plans<<Plan{QNetworkReply::NoError,0,{},true};tick(w);w.setProperty("GeneratorModeActive",false);
        QVERIFY(manager.replies[0]->isFinished());QVERIFY(!scheduler(w)->isActive());QCOMPARE(w.WaveHeight(),N2kDoubleNA); // Clear provider ownership before it can resume as current
        w.acceptObservation(sample("late"));QVERIFY(w.fieldOrigin("WaveHeight").stale);
        w.setProperty("GeneratorModeActive",true);QVERIFY(scheduler(w)->isActive());QCOMPARE(manager.requests.size(),1);
        source(w)->setCurrentIndex(0);QVERIFY(!scheduler(w)->isActive());QCOMPARE(w.WaveHeight(),N2kDoubleNA); // stale provider data cannot resume
    }
    void keyChangeAndDestruction() {
        FixtureManager manager;manager.plans<<Plan{QNetworkReply::NoError,0,{},true};
        auto *w=new GeneratorWeather(1,"Weather",nullptr,&manager);source(*w)->setCurrentIndex(1);automatic(*w)->setChecked(true);w->setOpenWeatherMapApiKey("synthetic-only");
        w->setLatitude(42);w->setLongitude(-70);tick(*w);QCOMPARE(manager.requests.size(),1);w->setLatitude(44);
        QCOMPARE(QUrlQuery(manager.requests[0].url()).queryItemValue("lat"),QString("42"));
        w->setOpenWeatherMapApiKey("synthetic-replacement");QVERIFY(manager.replies[0]->isFinished());QVERIFY(scheduler(*w)->isActive());
        manager.plans<<Plan{QNetworkReply::NoError,0,{},true};tick(*w);QPointer<QNetworkReply> reply=manager.replies.last();delete w;QVERIFY(!reply || reply->isFinished());
    }
    void staleCandidatesDeadlineAndReordering() {
        FixtureManager manager;
        manager.plans<<Plan{QNetworkReply::NoError,200,stations()}<<Plan{QNetworkReply::NoError,200,observation(20000)}<<Plan{};
        GeneratorWeather w(1,"Weather",nullptr,&manager);source(w)->setCurrentIndex(2);automatic(w)->setChecked(true);w.setLatitude(42);w.setLongitude(-70);tick(w);
        QTRY_VERIFY(w.hasFreshObservation());QCOMPARE(w.fieldOrigin("WaveHeight").station,QString("44098"));
        manager.plans<<Plan{QNetworkReply::NoError,0,{},true};tick(w);auto old=manager.replies.last();
        auto *deadline=w.findChild<QTimer*>("weatherCycleDeadline");QVERIFY(deadline);QCOMPARE(deadline->interval(),60000);
        QMetaObject::invokeMethod(deadline,"timeout",Qt::DirectConnection);QVERIFY(old->isFinished());QVERIFY(scheduler(w)->isActive());
        const auto count=manager.requests.size();source(w)->setCurrentIndex(0);QTest::qWait(20);QCOMPARE(manager.requests.size(),count);QVERIFY(!scheduler(w)->isActive());QCOMPARE(w.WaveHeight(),N2kDoubleNA);
    }
    void persistedOriginsDoNotRenewAge() {
        GeneratorWeather w(1,"Weather");source(w)->setCurrentIndex(2);auto o=sample("originA",0,1);w.acceptObservation(o);
        auto b=sample("originB");b.waveHeight=N2kDoubleNA;w.acceptObservation(b);const auto config=w.layoutConfiguration();
        QVERIFY(GeneratorWeather::validLayoutConfiguration(config));
        GeneratorWeather restored(2,"Weather");restored.setWaveHeight(w.WaveHeight());restored.restoreLayoutConfiguration(config);
        QCOMPARE(restored.WaveHeight(),2.);QCOMPARE(restored.fieldOrigin("WaveHeight").station,QString("originA"));
        QCOMPARE(restored.fieldOrigin("WaveHeight").observed,o.observed);QVERIFY(restored.fieldOrigin("WaveHeight").retained);
        QTRY_COMPARE_WITH_TIMEOUT(restored.WaveHeight(),N2kDoubleNA,2200);
        GeneratorWeather late(3,"Weather");late.setWaveHeight(2);late.restoreLayoutConfiguration(config);QCOMPARE(late.WaveHeight(),N2kDoubleNA);
        auto invalid=config;auto metadata=config["origins"].toObject();auto field=metadata["WaveHeight"].toObject();field["maximumAgeSeconds"]=-1;metadata["WaveHeight"]=field;invalid["origins"]=metadata;
        QVERIFY(!GeneratorWeather::validLayoutConfiguration(invalid));
    }
    void layoutAndInspectionOutput() {
        MainWindow window;window.show();auto*c=window.findChild<MainController*>();confirm();QVERIFY(c->loadLayoutFromJson(":/defaults/defaultLayout.json"));
        auto*w=window.findChild<GeneratorWeather*>();QVERIFY(w);source(*w)->setCurrentIndex(2);automatic(*w)->setChecked(true);
        w->findChild<QComboBox*>("weatherRefreshInterval")->setCurrentIndex(3);retention(*w)->setChecked(false);w->setWaveHeight(3.25);
        QTemporaryDir dir(QDir::currentPath()+"/m218-layout-XXXXXX");auto path=dir.filePath("weather.json");QVERIFY(c->saveLayoutToJson(path));confirm();QVERIFY(c->loadLayoutFromJson(path));
        w=window.findChild<GeneratorWeather*>();QVERIFY(w->automaticRefresh());QCOMPARE(w->refreshIntervalMinutes(),30);QVERIFY(!w->retainObservations());QCOMPARE(source(*w)->currentIndex(),2);QCOMPARE(w->WaveHeight(),3.25);QVERIFY(scheduler(*w)->remainingTime()>1700000);
        auto*model=window.findChild<PgnTableController*>()->getPgnTableModel();auto*tree=window.findChild<PgnTreeController*>()->getTreeModel();
        retention(*w)->setChecked(true);auto a=sample("originA",0,1);a.humidity=55;w->acceptObservation(a);auto b=sample("originB");b.waveHeight=N2kDoubleNA;b.humidity=N2kDoubleNA;w->acceptObservation(b);
        c->addWidgetAsComboboxTextChanged("130306","Wind","Environment");c->addWidgetAsComboboxTextChanged("130313","Humidity","Environment");
        EnvironmentalInstrument*wind=nullptr,*humidity=nullptr;for(auto*e:window.findChildren<EnvironmentalInstrument*>()){if(e->Pgn()==130306)wind=e;else if(e->Pgn()==130313)humidity=e;}
        QVERIFY(wind && humidity);w->setWindSpeed(8);QTRY_COMPARE(wind->value("WindSpeed"),8.);
        QTRY_VERIFY(tree->indexFor(w,"WaveHeight").isValid());QVERIFY(tree->data(tree->indexFor(w,"WaveHeight"),Qt::ToolTipRole).toString().contains("originA"));
        model->setCurrentPgn(w->Id());bool found=false;for(int row=0;row<model->rowCount();++row)if(model->data(model->index(row,0)).toString()=="WaveHeight"){QVERIFY(model->data(model->index(row,1),Qt::ToolTipRole).toString().contains("Retained"));found=true;}QVERIFY(found);
        QTRY_COMPARE_WITH_TIMEOUT(w->Humidity(),N2kDoubleNA,2200);QTRY_COMPARE(humidity->value("ActualHumidity"),N2kDoubleNA);
        tN2kMsg msg;SetN2kPGN130313(msg,0,0,N2khs_OutsideHumidity,humidity->value("ActualHumidity"),N2kDoubleNA);bool null=false;for(auto v:SignalKPublisher::values(msg)){auto o=v.toObject();if(o["path"]=="environment.outside.relativeHumidity")null=o["value"].isNull();}QVERIFY(null);
        QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
        confirm();QVERIFY(c->loadLayoutFromJson(":/defaults/defaultLayout.json"));w=window.findChild<GeneratorWeather*>();QVERIFY(!w->automaticRefresh());QVERIFY(w->retainObservations());QCOMPARE(w->refreshIntervalMinutes(),15);
    }
    void themes() {
        for(const QString theme:{"Dark","Light"}){
            qApp->setPalette(Themes::palette(theme));qApp->setStyleSheet(Themes::styleSheet(theme));GeneratorWeather w(1,"Weather");w.resize(430,450);w.show();source(w)->setCurrentIndex(2);w.acceptObservation(sample());
            auto*tabs=w.findChild<QTabWidget*>();tabs->setCurrentIndex(tabs->count()-1);QTest::qWait(30);
            QVERIFY(automatic(w)->isVisible());QVERIFY(automatic(w)->height()>=automatic(w)->fontMetrics().height());
            QVERIFY(retention(w)->isVisible());QVERIFY(retention(w)->width()>220);
            const auto evidence=qEnvironmentVariable("M218_EVIDENCE");
            if(!evidence.isEmpty()) QVERIFY(w.grab().save(evidence+"/m218-"+theme+".png"));
        }
    }
};
int main(int argc,char**argv) {
    QTemporaryDir profile(QDir::currentPath()+"/m218-profile-XXXXXX");if(!profile.isValid())return 2;
    qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*>ptr;for(auto&a:args)ptr<<a.data();int n=ptr.size();ptr<<nullptr;
    QApplication app(n,ptr.data());app.setStyle("Fusion");
    if (argc==2 && QByteArray(argv[1])=="--live-refresh") {
        GeneratorWeather w(1,"Weather");source(w)->setCurrentIndex(2);automatic(w)->setChecked(true);
        w.setLatitude(42.97);w.setLongitude(-70.62);QEventLoop loop;bool success=false;
        auto *provider=w.findChild<NationalDataBuoyCenter*>();
        QObject::connect(provider,&NationalDataBuoyCenter::errorOccurred,&loop,[&]{if(scheduler(w)->isActive())loop.quit();});
        QObject::connect(provider,&NationalDataBuoyCenter::observationFetched,&loop,[&](const WeatherObservation&o){if(o.fresh()){success=true;qInfo().noquote()<<"Controlled scheduled NOAA cycle: fresh station="+o.station;loop.quit();}});
        QObject::connect(fetch(w),&QPushButton::clicked,&loop,&QEventLoop::quit);
        QTimer::singleShot(61000,&loop,&QEventLoop::quit);tick(w);loop.exec();w.cancelWeather();return success?0:1;
    }
    QNetworkProxyFactory::setApplicationProxyFactory(new OfflineProxy);
    WeatherRefreshTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "weather_refresh_tests.moc"
