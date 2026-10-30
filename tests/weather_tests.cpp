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
#include <QTcpServer>
#include <QTcpSocket>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QtMath>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include "credentialstore.h"
#include "dialogsetup.h"
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include "mainwindow.h"
#include "appsettings.h"
#include "pgntreecontroller.h"
#include "generatorweather.h"
#include "temperaturewidget130312.h"
#include "temperatureextended130316.h"
#include "outsideenvironmental130310.h"
#include "signalkpublisher.h"
#include "themes.h"
#include "N2kMessages.h"
#ifdef M217A_LIVE_QUALIFICATION
QString weatherLiveKey();
#endif
namespace {
class OfflineProxy final : public QNetworkProxyFactory {
  public:
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override {
        if (query.peerHostName() == "127.0.0.1" || query.peerHostName() == "localhost" ||
            query.peerHostName() == "::1")
            return {QNetworkProxy(QNetworkProxy::NoProxy)};
        return {QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9)};
    }
};
QByteArray owm(QDateTime time = QDateTime::currentDateTimeUtc()) {
    return QJsonDocument(
               QJsonObject{{"cod", 200},
                           {"name", "Boston"},
                           {"dt", time.toSecsSinceEpoch()},
                           {"coord", QJsonObject{{"lat", 42.35}, {"lon", -70.65}}},
                           {"main", QJsonObject{{"temp", 293.15}, {"humidity", 0}, {"pressure", 1006.1}}},
                           {"wind", QJsonObject{{"speed", 0}, {"deg", 0}}}})
        .toJson();
}
QByteArray noaa(QString measurements = "280 7 9 0.7 3 3.4 268 1006.1 16.5 14.4 12.6 MM MM MM") {
    return "#YY MM DD hh mm WDIR WSPD GST WVHT DPD APD MWD PRES ATMP WTMP DEWP VIS PTDY TIDE\n#yr mo dy hr "
           "mn degT m/s m/s m sec sec degT hPa degC degC degC nmi hPa ft\n" +
           QDateTime::currentDateTimeUtc().toString("yyyy MM dd HH mm ").toUtf8() + measurements.toUtf8() +
           "\n";
}
void near(double a, double b, double tolerance = 1e-6) {
    QVERIFY2(qAbs(a - b) <= tolerance, qPrintable(QString("%1 != %2").arg(a, 0, 'g', 17).arg(b, 0, 'g', 17)));
}
struct Server : QTcpServer {
    QList<QPointer<QTcpSocket>> requests;
    Server() {
        listen(QHostAddress::LocalHost);
        QObject::connect(this, &QTcpServer::newConnection, [this] {
            auto *s = nextPendingConnection();
            QObject::connect(s, &QTcpSocket::readyRead, [this, s] {
                if (s->property("received").toBool())
                    return;
                s->readAll();
                s->setProperty("received", true);
                requests << s;
            });
        });
    }
    QUrl url(QString path = "/") {
        return QUrl(QString("http://127.0.0.1:%1%2").arg(serverPort()).arg(path));
    }
    void send(int index, QByteArray body, int status = 200) {
        auto s = requests.value(index);
        if (s && s->state() == QAbstractSocket::ConnectedState) {
            s->write("HTTP/1.1 " + QByteArray::number(status) + " OK\r\nContent-Length: " +
                     QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            s->disconnectFromHost();
        }
    }
};
void answer() {
    QTimer::singleShot(0, [] {
        for (auto *w : QApplication::topLevelWidgets())
            if (auto *box = qobject_cast<QMessageBox *>(w))
                box->button(QMessageBox::Yes)->click();
    });
}
struct Workspace {
    MainWindow window;
    MainController *controller = window.findChild<MainController *>();
    PgnTableModel *model = window.findChild<PgnTableController *>()->getPgnTableModel();
    Workspace() {
        window.resize(1400, 950);
        window.show();
    }
    GeneratorWeather *weather() {
        answer();
        controller->loadLayoutFromJson(":/defaults/defaultLayout.json");
        auto *w = window.findChild<GeneratorWeather *>();
        if (!w) {
            QTimer decline;
            decline.setInterval(1);
            QObject::connect(&decline, &QTimer::timeout, [] {
                for (auto *w : QApplication::topLevelWidgets())
                    if (auto *box = qobject_cast<QMessageBox *>(w))
                        box->button(QMessageBox::No)->click();
            });
            decline.start();
            controller->addWidgetAsComboboxTextChanged("-130311", "Weather", "Generate");
            w = window.findChild<GeneratorWeather *>();
        }
        return w;
    }
    double value(QWidget *w, QString name) {
        for (auto row : model->findPgn(w->property("Id").toInt())->paramDetails)
            if (row[0] == name)
                return row[1].toDouble();
        return qQNaN();
    }
};
// Separate processes share only this disposable profile/OS credential identity.
// Return codes describe failures; credentials are never written to test output.
int credentialWorker(const QString& stage) {
    if (stage == "cleanup") return CredentialStore::remove("OpenWeatherMapApiKey") ? 0 : 20;
    Workspace w;
    auto* weather=w.weather(); if(!weather) return 21;
    auto* setup=w.window.findChild<DialogSetup*>(); setup->show();
    auto* edit=setup->findChild<QLineEdit*>("lineEditOpenWeatherMapKey");
    const bool cleared=stage=="clear" || stage=="check-cleared";
    const QString expected=cleared ? QString() : stage=="replace" || stage=="check-replaced" ?
        "synthetic-weather-replacement" : "synthetic-weather-restart";
    if(stage=="save" || stage=="replace" || stage=="clear") {
        edit->setText(expected); setup->findChild<QPushButton*>("pushButtonSave")->click();
    }
    const auto stored=CredentialStore::read("OpenWeatherMapApiKey");
    if(!stored.success) return 20;
    if(stored.value!=expected || stored.found==cleared) return 22;
    setup->findChild<QPushButton*>("pushButtonLoad")->click();
    if(edit->text()!=expected || edit->echoMode()!=QLineEdit::Password) return 23;
    setup->reject();
    weather->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(1);
    QSignalSpy errors(weather->findChild<OpenWeatherMap*>(),&OpenWeatherMap::errorOccurred);
    weather->findChild<QPushButton*>("pushButtonFetchWeather")->click();
    bool missing=weather->observationStatus().contains("Configure an OpenWeatherMap");
    for(const auto& error:errors) missing |= error[0].toString().contains("Configure an OpenWeatherMap");
    weather->cancelWeather();
    return missing==cleared ? 0 : 43;
}
} // namespace
class WeatherTests : public QObject {
    Q_OBJECT
  public:
    QString liveKey;
  private slots:
    void init() {
        AppSettings s(APP_COMPANY, APP_NAME);
        s.clear();
        s.setValue("Nmea2000/InterfaceType", "None");
        s.setValue("Preference/StyleSheetName", "Dark");
    }
    void credentialProcessRestarts() {
        QTemporaryDir shared(QDir::currentPath()+"/m217a-credential-process-XXXXXX");
        QVERIFY(shared.isValid());
        auto run=[&](const QString& stage) {
            QProcess process;
            process.start(QCoreApplication::applicationFilePath(), {"--credential-worker",stage,shared.path()});
            if(!process.waitForFinished(30000)) { process.kill();process.waitForFinished();return 99; }
            return process.exitStatus()==QProcess::NormalExit ? process.exitCode() : 98;
        };
        // Always remove only the disposable credential, including on a failed assertion.
        const auto cleanup=qScopeGuard([&]{ run("cleanup"); });
        for(const QString& stage:{"save","check","check","replace","check-replaced","clear","check-cleared"}) {
            const int result=run(stage);
#ifdef Q_OS_LINUX
            if(stage=="save" && result==20) QSKIP("Desktop Secret Service unavailable; isolated backend fixture tested separately.");
#endif
            QVERIFY2(result==0,qPrintable(QString("Credential subprocess %1 failed, code %2 (no secret output)").arg(stage).arg(result)));
        }
    }
    void credentialSetupVisibility() {
        QString error;
        if(!CredentialStore::write("OpenWeatherMapApiKey","synthetic-visibility-key",&error)) {
#ifdef Q_OS_LINUX
            QSKIP("Desktop Secret Service unavailable; isolated backend fixture tested separately.");
#else
            QFAIL("Isolated OS credential store unavailable");
#endif
        }
        const auto cleanup=qScopeGuard([]{CredentialStore::remove("OpenWeatherMapApiKey");});
        Workspace w;
        auto* setup=w.window.findChild<DialogSetup*>(); setup->show();
        auto* tabs=setup->findChild<QTabWidget*>("tabWidget");
        tabs->setCurrentWidget(setup->findChild<QWidget*>("tabApiKeys"));
        auto* edit=setup->findChild<QLineEdit*>("lineEditOpenWeatherMapKey");
        auto* eye=setup->findChild<QPushButton*>("pushButtonShowOpenWeatherMapKey");
        QVERIFY(edit && eye && !eye->isHidden());
        QVERIFY(edit->text()=="synthetic-visibility-key");
        QCOMPARE(edit->echoMode(),QLineEdit::Password); QVERIFY(!eye->isChecked());
        eye->setFocus(); QTest::keyClick(eye,Qt::Key_Space);
        QCOMPARE(edit->echoMode(),QLineEdit::Normal); QVERIFY(eye->isChecked());
        QVERIFY(edit->text()=="synthetic-visibility-key"); QVERIFY(eye->toolTip().contains("Hide"));
        QTest::keyClick(eye,Qt::Key_Space); QCOMPARE(edit->echoMode(),QLineEdit::Password);
        eye->click(); setup->reject(); setup->show();
        QCOMPARE(edit->echoMode(),QLineEdit::Password); QVERIFY(!eye->isChecked());
        edit->setText("unsaved-synthetic"); eye->click();
        setup->findChild<QPushButton*>("pushButtonLoad")->click();
        QVERIFY(edit->text()=="synthetic-visibility-key"); QCOMPARE(edit->echoMode(),QLineEdit::Password);
        edit->setText("cancelled-synthetic"); setup->reject(); setup->show();
        QVERIFY(edit->text()=="synthetic-visibility-key");
        QSignalSpy reload(setup,&DialogSetup::ReloadSettings);
        edit->setText(QString(3000,'s')); setup->findChild<QPushButton*>("pushButtonSave")->click();
        QCOMPARE(reload.count(),0); QVERIFY(CredentialStore::read("OpenWeatherMapApiKey").value=="synthetic-visibility-key");
        QVERIFY(setup->findChild<QLabel*>("credentialStorageStatus")->text().contains("not saved"));
        edit->setText("synthetic-close-key"); setup->findChild<QPushButton*>("pushButtonClose")->click();
        QVERIFY(!setup->isVisible()); QVERIFY(CredentialStore::read("OpenWeatherMapApiKey").value=="synthetic-close-key");
        setup->show(); QCOMPARE(edit->echoMode(),QLineEdit::Password);
        const auto evidence=qEnvironmentVariable("M217A_EVIDENCE");
        if(!evidence.isEmpty()) {
            QDir().mkpath(evidence);
            for(const QString& theme:{"Dark","Light"}) {
                setup->setStyleSheet({}); qApp->setPalette(Themes::palette(theme));
                setup->setStyleSheet(Themes::styleSheet(theme));setup->setPalette(Themes::palette(theme));
                tabs->setCurrentWidget(setup->findChild<QWidget*>("tabApiKeys"));QTest::qWait(20);
                QVERIFY(setup->grab().save(evidence+"/"+theme+"-api-key-synthetic.png"));
            }
        }
        setup->reject();
    }
    void openWeatherParsing() {
        WeatherObservation o;
        QString e;
        QVERIFY(OpenWeatherMap::parse(owm(), o, e));
        near(o.air, 293.15);
        near(o.pressure, 100610);
        near(o.humidity, 0);
        near(o.windSpeed, 0);
        near(o.windAngle, 0);
        QCOMPARE(o.sea, N2kDoubleNA);
        QVERIFY(o.fresh());
        QVERIFY(o.observed.isValid());
        QVERIFY(!OpenWeatherMap::parse("{", o, e));
        QVERIFY(!OpenWeatherMap::parse("[]", o, e));
        QVERIFY(!OpenWeatherMap::parse("{}", o, e));
        QVERIFY(!OpenWeatherMap::parse(R"({"cod":401,"main":{"temp":290}})", o, e));
        QVERIFY(OpenWeatherMap::parse(
            R"({"main":{"temp":280,"humidity":"bad","pressure":null},"wind":{"speed":0}})", o, e));
        near(o.air, 280);
        QCOMPARE(o.humidity, N2kDoubleNA);
        QCOMPARE(o.pressure, N2kDoubleNA);
        QVERIFY(!o.invalid.isEmpty());
        QVERIFY(!o.fresh());
    }
    void noaaParsing() {
        WeatherObservation o;
        QString e;
        QVERIFY(NationalDataBuoyCenter::parse(noaa(), o, e));
        near(o.sea, 287.55);
        near(o.air, 289.65);
        near(o.pressure, 100610);
        near(o.waveHeight, .7);
        near(o.windSpeed, 7);
        near(o.windAngle, qDegreesToRadians(280.));
        QVERIFY(o.humidity > 0 && o.humidity < 100);
        QVERIFY(o.fresh());
        QVERIFY(NationalDataBuoyCenter::parse(noaa("0 0 MM MM MM MM MM MM MM 0 MM MM MM MM"), o, e));
        near(o.sea, 273.15);
        near(o.windSpeed, 0);
        near(o.windAngle, 0);
        QCOMPARE(o.air, N2kDoubleNA);
        QCOMPARE(o.pressure, N2kDoubleNA);
        QCOMPARE(o.humidity, N2kDoubleNA);
        QVERIFY(!NationalDataBuoyCenter::parse(noaa("0 0 MM MM MM MM MM MM MM 0 MM MM"), o, e));
        QVERIFY(!NationalDataBuoyCenter::parse(noaa("MM MM MM MM MM MM MM MM MM MM MM MM MM MM"), o, e));
        QVERIFY(NationalDataBuoyCenter::parse(noaa("0 0 MM MM MM MM MM bad nope 10 MM MM MM MM"), o, e));
        QCOMPARE(o.air, N2kDoubleNA);
        QVERIFY(o.invalid.contains("PRES"));
        QVERIFY(o.invalid.contains("ATMP"));
    }
    void openWeatherOptionalMarineFields() {
        auto root=QJsonDocument::fromJson(owm()).object(); auto wind=root["wind"].toObject();
        wind["gust"]=12.5;root["wind"]=wind;root["visibility"]=8500;
        WeatherObservation o;QString error;
        QVERIFY(OpenWeatherMap::parse(QJsonDocument(root).toJson(),o,error));
        near(o.windGust,12.5);near(o.visibility,8500);QCOMPARE(o.waveHeight,N2kDoubleNA);
        QCOMPARE(o.meanWaveDirection,N2kDoubleNA);
    }
    void marineParsing() {
        WeatherObservation o; QString error;
        QVERIFY(NationalDataBuoyCenter::parse(noaa("280 7 9 1.8 6 4.7 157 1006.1 16.5 14.4 12.6 2 -1.2 -2"), o, error));
        near(o.waveHeight, 1.8); near(o.dominantWavePeriod, 6); near(o.averageWavePeriod, 4.7);
        near(o.meanWaveDirection, qDegreesToRadians(157.)); near(o.windGust, 9);
        near(o.dewPoint, 285.75); near(o.visibility, 3704); near(o.pressureTendency, -120); near(o.tide, -.6096);
        for (int missing : {8,9,10,11}) {
            auto columns = QString("MM MM MM 1.8 6 4.7 157 MM MM MM MM MM MM MM").split(' ');
            columns[missing-5] = "MM";
            QVERIFY(NationalDataBuoyCenter::parse(noaa(columns.join(' ')), o, error));
            QCOMPARE((missing==8?o.waveHeight:missing==9?o.dominantWavePeriod:missing==10?o.averageWavePeriod:o.meanWaveDirection), N2kDoubleNA);
            QVERIFY(o.any());
        }
        QVERIFY(NationalDataBuoyCenter::parse(noaa("MM MM MM 0 0 0 0 MM MM MM MM MM MM MM"), o, error));
        near(o.waveHeight, 0); near(o.dominantWavePeriod, 0); near(o.averageWavePeriod, 0); near(o.meanWaveDirection, 0);
        QVERIFY(NationalDataBuoyCenter::parse(noaa("MM MM MM bad -2 4.7 999 MM MM MM MM MM MM MM"), o, error));
        QCOMPARE(o.waveHeight, N2kDoubleNA); QCOMPARE(o.dominantWavePeriod, N2kDoubleNA); QCOMPARE(o.meanWaveDirection, N2kDoubleNA);
        near(o.averageWavePeriod, 4.7);
    }
    void marineIntegration() {
        Workspace w; auto* weather=w.weather(); QVERIFY(weather);
        weather->findChild<QCheckBox*>("weatherRetainObservations")->setChecked(false);
        weather->setEnabled(true);
        weather->setAirTemperature(N2kDoubleNA);
        weather->setAtmosphericPressure(N2kDoubleNA);
        weather->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(2);
        WeatherObservation o; QString error;
        QVERIFY(NationalDataBuoyCenter::parse(noaa("MM MM MM 1.8 6 4.7 157 MM MM MM MM MM MM MM"), o, error));
        weather->acceptObservation(o);
        QTRY_COMPARE(w.value(weather,"WaveHeight"),1.8);
        near(w.value(weather,"DominantWavePeriod"),6); near(w.value(weather,"AverageWavePeriod"),4.7);
        near(w.value(weather,"MeanWaveDirection"),qDegreesToRadians(157.));
        QCOMPARE(weather->AirTemperature(),N2kDoubleNA); QVERIFY(weather->hasFreshObservation());
        auto prefs=weather->UserPrefs(); prefs.depthUnitIndex=1; weather->setUserPrefs(prefs);
        near(weather->findChild<QLineEdit*>("lineEditWaveHeight")->text().toDouble(),1.8/.3048,.01);
        auto* tree=w.window.findChild<PgnTreeController*>()->getTreeModel();
        QTRY_VERIFY(tree->indexFor(weather,"WaveHeight").isValid());
        QVERIFY(tree->data(tree->indexFor(weather,"WaveHeight").siblingAtColumn(1)).toString().contains("ft"));
        QVERIFY(tree->data(tree->indexFor(weather,"MeanWaveDirection").siblingAtColumn(1)).toString().contains("true (from)"));
        w.window.findChild<PgnTreeController*>()->getTreeView()->setCurrentIndex(tree->indexFor(weather,"WaveHeight"));
        weather->findChild<QTabWidget*>()->setCurrentIndex(1);
        QTest::qWait(30);
        auto* waveEdit=weather->findChild<QLineEdit*>("lineEditWaveHeight"); QVERIFY(waveEdit);
        waveEdit->setFocus(); waveEdit->selectAll(); QTest::keyClicks(waveEdit,"10");
        near(weather->WaveHeight(),3.048); near(w.value(weather,"WaveHeight"),3.048);
        QTest::keyClick(waveEdit,Qt::Key_Return);
        w.controller->selectLayoutWidget(weather,"WaveHeight");
        QCOMPARE(w.model->findPgn(weather->Id())->pgn,QString("-130311"));
        w.model->updatePgnParameter(weather->Id(),"WaveHeight",QString("2.5"),true);
        QTRY_COMPARE(weather->WaveHeight(),2.5);
        weather->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(1);
        weather->acceptObservation([]{ WeatherObservation x; QString e; OpenWeatherMap::parse(owm(),x,e); return x;}());
        near(weather->WaveHeight(),2.5); // Manual value survives absent OpenWeatherMap wave data.
        QCOMPARE(weather->DominantWavePeriod(),N2kDoubleNA);
        QTemporaryDir dir(QDir::currentPath()+"/m217a-marine-XXXXXX"); const auto path=dir.filePath("weather.json");
        QVERIFY(w.controller->saveLayoutToJson(path));
        answer(); QVERIFY(w.controller->loadLayoutFromJson(path));
        weather=w.window.findChild<GeneratorWeather*>(); QVERIFY(weather); near(weather->WaveHeight(),2.5);
        // The bundled Default carries the established pre-extension Weather schema.
        answer(); QVERIFY(w.controller->loadLayoutFromJson(":/defaults/defaultLayout.json"));
        weather=w.window.findChild<GeneratorWeather*>(); QVERIFY(weather);
        QCOMPARE(w.value(weather,"WaveHeight"),N2kDoubleNA); QCOMPARE(weather->WaveHeight(),N2kDoubleNA);
        GeneratorWeather isolated(999,"Weather"); isolated.findChild<QCheckBox*>("weatherRetainObservations")->setChecked(false); isolated.findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(2);
        o.maximumAgeSeconds=1; o.observed=QDateTime::currentDateTimeUtc(); isolated.acceptObservation(o);
        QVERIFY(isolated.hasFreshObservation());
        QTRY_COMPARE_WITH_TIMEOUT(isolated.WaveHeight(),N2kDoubleNA,2500);
        QVERIFY(!isolated.hasFreshObservation());
        isolated.acceptObservation([]{WeatherObservation x;QString e;NationalDataBuoyCenter::parse(noaa(),x,e);return x;}());
        isolated.setWaveHeight(3); isolated.cancelWeather(); near(isolated.WaveHeight(),3);
        QCOMPARE(isolated.DominantWavePeriod(),N2kDoubleNA);
        isolated.setProperty("GeneratorModeActive",false);
        isolated.setWaveHeight(4); isolated.acceptObservation(o); near(isolated.WaveHeight(),4);
        isolated.setProperty("GeneratorModeActive",true); near(isolated.WaveHeight(),4);
        QVERIFY(!isolated.hasFreshObservation());
    }
    void stationParsing() {
        QList<Station> stations;
        QString error;
        QVERIFY(NationalDataBuoyCenter::parseStationsXml(
            R"(<stations><station id="44013" name="Boston" lat="42.346" lon="-70.651" met="y"/><station id="bad" lat="oops" lon="0"/><station id="wave" lat="0" lon="0" met="n"/></stations>)",
            stations, error));
        QCOMPARE(stations.size(), 2);
        QCOMPARE(stations[0].id, QString("44013"));
        QVERIFY(!NationalDataBuoyCenter::parseStationsXml("<stations><station id='44013' lat='0' lon='0'/>",
                                                          stations, error));
        QVERIFY(stations.isEmpty());
        QVERIFY(!NationalDataBuoyCenter::parseStationsXml("<html/>", stations, error));
    }
    void freshness() {
        WeatherObservation o;
        QString error;
        QVERIFY(OpenWeatherMap::parse(owm(QDateTime::currentDateTimeUtc().addSecs(-8000)), o, error));
        QVERIFY(!o.fresh());
        QVERIFY(OpenWeatherMap::parse(owm(QDateTime::currentDateTimeUtc().addSecs(700)), o, error));
        QVERIFY(!o.fresh());
    }
    void repliesAndCancellation() {
        Server server;
        OpenWeatherMap provider(nullptr, server.url());
        provider.setApiKey("synthetic-fixture-key");
        QSignalSpy received(&provider, &OpenWeatherMap::observationFetched),
            errors(&provider, &OpenWeatherMap::errorOccurred);
        provider.fetchWeather(42, -70);
        QTRY_COMPARE(server.requests.size(), 1);
        provider.fetchWeather(43, -71);
        QTRY_COMPARE(server.requests.size(), 2);
        server.send(1, owm());
        QTRY_COMPARE(received.count(), 1);
        server.send(0, owm(QDateTime::currentDateTimeUtc().addSecs(-200)));
        QTest::qWait(30);
        QCOMPARE(received.count(), 1);
        QCOMPARE(errors.count(), 0);
        provider.fetchWeather(42, -70);
        QTRY_COMPARE(server.requests.size(), 3);
        provider.cancel();
        server.send(2, owm());
        QTest::qWait(30);
        QCOMPARE(received.count(), 1);
        provider.fetchWeather(42, -70);
        QTRY_COMPARE(server.requests.size(), 4);
        provider.setApiKey("new-synthetic-key");
        server.send(3, owm());
        QTest::qWait(30);
        QCOMPARE(received.count(), 1);
        provider.fetchWeather(42, -70);
        QTRY_COMPARE(server.requests.size(), 5);
        server.send(4, "{}", 401);
        QTRY_COMPARE(errors.count(), 1);
        auto *doomed = new OpenWeatherMap(nullptr, server.url());
        doomed->setApiKey("fake");
        doomed->fetchWeather(42, -70);
        QTRY_COMPARE(server.requests.size(), 6);
        delete doomed;
        server.send(5, owm());
        QTest::qWait(20);
    }
    void noaaRequests() {
        Server server;
        NationalDataBuoyCenter provider(nullptr, server.url("/stations"), server.url("/data/"));
        QSignalSpy stations(&provider, &NationalDataBuoyCenter::nearestStationsFound),
            received(&provider, &NationalDataBuoyCenter::observationFetched),
            errors(&provider, &NationalDataBuoyCenter::errorOccurred);
        provider.getNearestStations(42.35, -70.65, 80);
        QTRY_COMPARE(server.requests.size(), 1);
        server.send(
            0,
            R"(<stations><station id="44013" name="Boston" lat="42.346" lon="-70.651"/><station id="far" lat="10" lon="20"/></stations>)");
        QTRY_COMPARE(stations.count(), 1);
        QCOMPARE(stations[0][0].value<QList<Station>>().size(), 1);
        provider.fetchStationData("44013");
        QTRY_COMPARE(server.requests.size(), 2);
        server.send(1, noaa());
        QTRY_COMPARE(received.count(), 1);
        QCOMPARE(received[0][0].value<WeatherObservation>().station, QString("44013"));
        provider.fetchStationData("../bad");
        QCOMPARE(errors.count(), 1);
    }
    void noaaOverlap() {
        Server server;
        NationalDataBuoyCenter p(nullptr, server.url(), server.url());
        QSignalSpy found(&p, &NationalDataBuoyCenter::nearestStationsFound);
        p.getNearestStations(0, 0, 80);
        QTRY_COMPARE(server.requests.size(), 1);
        p.getNearestStations(42, -70, 80);
        QTRY_COMPARE(server.requests.size(), 2);
        server.send(1, R"(<stations><station id="44013" lat="42" lon="-70" name="Boston"/></stations>)");
        QTRY_COMPARE(found.count(), 1);
        server.send(0, R"(<stations><station id="old" lat="0" lon="0"/></stations>)");
        QTest::qWait(30);
        QCOMPARE(found.count(), 1);
        QCOMPARE(found[0][0].value<QList<Station>>()[0].id, QString("44013"));
    }
    void widgetCancellation() {
        auto *w = new GeneratorWeather(1, "Weather");
        auto *source = w->findChild<QComboBox *>("comboBoxApiSource");
        auto *fetch = w->findChild<QPushButton *>("pushButtonFetchWeather");
        w->setOpenWeatherMapApiKey("fixture");
        source->setCurrentIndex(1);
        fetch->click();
        QVERIFY(fetch->text().contains("Cancel"));
        w->setLongitude(-70);
        QVERIFY(!fetch->text().contains("Cancel"));
        fetch->click();
        w->setProperty("GeneratorModeActive", false);
        QVERIFY(w->observationStatus().contains("Viewer"));
        w->setProperty("GeneratorModeActive", true);
        fetch->click();
        w->setOpenWeatherMapApiKey("new-fixture");
        QVERIFY(!fetch->text().contains("Cancel"));
        fetch->click();
        delete w;
        QTest::qWait(30);
    }
    void stationPickerCancellation() {
        QList<Station> list;
        Station station{};
        station.id = "44013";
        station.name = "Boston";
        station.latitude = 42.346;
        station.longitude = -70.651;
        list << station;
        auto *w = new GeneratorWeather(1, "Weather");
        auto *source = w->findChild<QComboBox *>("comboBoxApiSource");
        source->setCurrentIndex(2);
        w->findChild<QPushButton *>("pushButtonFetchWeather")->click();
        bool closed = false;
        QTimer::singleShot(0, w, [&] {
            auto *dialog = w->findChild<QDialog *>("weatherStationPicker");
            source->setCurrentIndex(0);
            closed = dialog && !dialog->isVisible();
        });
        QTimer watchdog;
        watchdog.setInterval(500);
        connect(&watchdog, &QTimer::timeout, [] {
            for (auto *widget : QApplication::topLevelWidgets())
                if (auto *d = qobject_cast<QDialog *>(widget); d && d->windowTitle() == "Nearest Stations")
                    d->reject();
        });
        watchdog.start();
        w->nearestStationsFound(list);
        QVERIFY(closed);
        delete w;
        QPointer<GeneratorWeather> doomed = new GeneratorWeather(2, "Weather");
        doomed->findChild<QComboBox *>("comboBoxApiSource")->setCurrentIndex(2);
        doomed->findChild<QPushButton *>("pushButtonFetchWeather")->click();
        QTimer::singleShot(0, [doomed] { delete doomed.data(); });
        doomed->nearestStationsFound(list);
        QVERIFY(!doomed);
        watchdog.stop();
    }
    void timeout() {
        Server server;
        OpenWeatherMap provider(nullptr, server.url());
        provider.setApiKey("fixture");
        QSignalSpy errors(&provider, &OpenWeatherMap::errorOccurred);
        provider.fetchWeather(42, -70);
        QTRY_COMPARE(server.requests.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 18000);
        QVERIFY(errors[0][0].toString().contains("timed out") || errors[0][0].toString().contains("network"));
    }
    void widgetPolicy() {
        GeneratorWeather w(1, "Weather");
        w.findChild<QCheckBox*>("weatherRetainObservations")->setChecked(false);
        w.show();
        auto *source = w.findChild<QComboBox *>("comboBoxApiSource");
        auto *fetch = w.findChild<QPushButton *>("pushButtonFetchWeather");
        QSignalSpy requested(&w, &GeneratorWeather::fetchWeather);
        QVERIFY(!fetch->isEnabled());
        QCOMPARE(w.findChildren<QThread *>().size(), 0);
        source->setCurrentIndex(1);
        WeatherObservation o;
        QString error;
        QVERIFY(OpenWeatherMap::parse(owm(), o, error));
        w.acceptObservation(o);
        near(w.AirTemperature(), 293.15);
        QCOMPARE(w.SeaTemperature(), N2kDoubleNA);
        QVERIFY(w.hasFreshObservation());
        QCOMPARE(w.findChild<QLineEdit *>("lineEditSeaTemperature")->text(), QString("Unavailable"));
        w.setAirTemperature(301);
        source->setCurrentIndex(0);
        near(w.AirTemperature(), 301);
        QCOMPARE(w.AtmosphericPressure(), N2kDoubleNA);
        QVERIFY(!w.hasFreshObservation());
        QCOMPARE(requested.count(), 0);
        source->setCurrentIndex(1);
        w.acceptObservation(o);
        w.setLatitude(42.35);
        QCOMPARE(w.AirTemperature(), N2kDoubleNA);
        QVERIFY(w.observationStatus().contains("Location changed"));
        w.acceptObservation(o);
        w.handleWeatherError("Offline fixture failed");
        QCOMPARE(w.AirTemperature(), N2kDoubleNA);
        QVERIFY(w.observationStatus().contains("failed"));
        w.acceptObservation(o);
        w.setProperty("GeneratorModeActive", false);
        w.setAirTemperature(302);
        w.acceptObservation(o);
        near(w.AirTemperature(), 302);
        w.setProperty("GeneratorModeActive", true);
        QVERIFY(!w.hasFreshObservation());
        near(w.AirTemperature(), 302);
        o.observed = o.observed.addSecs(-10000);
        w.acceptObservation(o);
        near(w.AirTemperature(), 302); // Stale responses cannot overwrite a manual value.
        QVERIFY(w.observationStatus().contains("Stale"));
        w.setOpenWeatherMapApiKey("fixture");
        fetch->click();
        QCOMPARE(requested.count(), 1);
        fetch->click();
        QVERIFY(w.observationStatus().contains("Cancelled"));
    }
    void instrumentBindings() {
        Workspace w;
        auto *weather = w.weather();
        QVERIFY(weather);
        weather->setEnabled(true);
        auto *outside = w.window.findChild<OutsideEnvironmental130310 *>();
        if (!outside) {
            w.controller->addWidgetAsComboboxTextChanged("130310", "Outside", "Environment");
            outside = w.window.findChild<OutsideEnvironmental130310 *>();
        }
        QVERIFY(outside);
        for (QString pgn : {"130312", "130316"}) {
            QTimer picker;
            picker.setInterval(1);
            connect(&picker, &QTimer::timeout, [] {
                for (auto *widget : QApplication::topLevelWidgets())
                    if (auto *d = qobject_cast<QDialog *>(widget)) {
                        if (auto *box = qobject_cast<QMessageBox *>(d)) {
                            box->button(QMessageBox::No)->click();
                            continue;
                        }
                        d->accept();
                    }
            });
            picker.start();
            w.controller->addWidgetAsComboboxTextChanged(pgn, pgn, "Environment");
            picker.stop();
        }
        auto *a = w.window.findChild<TemperatureWidget130312 *>();
        auto *b = w.window.findChild<TemperatureExtended130316 *>();
        QVERIFY(a && b);
        a->setTemperatureSource(N2kts_SeaTemperature);
        b->setTempSource(N2kts_SeaTemperature);
        weather->setAirTemperature(290);
        weather->setSeaTemperature(280);
        QTRY_COMPARE(a->ActualTemperature(), 280.);
        QTRY_COMPARE(b->ActualTemperature(), 280.);
        weather->setSeaTemperature(281);
        QTRY_COMPARE(a->ActualTemperature(), 281.);
        QTRY_COMPARE(b->ActualTemperature(), 281.);
        near(outside->WaterTemperature(), 281);
        a->setTemperatureSource(N2kts_EngineRoomTemperature);
        a->setActualTemperature(305);
        b->setTempSource(N2kts_OutsideTemperature);
        weather->setAirTemperature(295);
        QTRY_COMPARE(b->ActualTemperature(), 295.);
        near(a->ActualTemperature(), 305);
        near(w.value(weather, "AirTemperature"), 295);
        near(w.value(outside, "WaterTemperature"), 281);
        weather->setSeaTemperature(N2kDoubleNA);
        QTRY_COMPARE(outside->WaterTemperature(), N2kDoubleNA);
        QCOMPARE(outside->findChild<QLineEdit *>("lineEditWaterTemperature")->text(), QString("Unavailable"));
        QTRY_VERIFY(w.window.findChild<PgnTreeController *>()
                        ->getTreeModel()
                        ->indexFor(weather, "AirTemperature")
                        .isValid());
        auto *tree = w.window.findChild<PgnTreeController *>()->getTreeModel();
        QVERIFY(tree->data(tree->indexFor(weather, "AirTemperature").siblingAtColumn(1))
                    .toString()
                    .contains("°F"));
        tN2kMsg msg;
        SetN2kPGN130310(msg, 0, outside->WaterTemperature(), outside->OutsideAmbientAirTemperature(), 100610);
        bool found = false;
        for (auto v : SignalKPublisher::values(msg)) {
            auto x = v.toObject();
            if (x["path"] == "environment.outside.temperature") {
                near(x["value"].toDouble(), 295, .01);
                found = true;
            }
        }
        QVERIFY(found);
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void rendering() {
        QWidget surface;
        surface.setAutoFillBackground(true);
        QVBoxLayout layout(&surface);
        layout.setContentsMargins(0, 0, 0, 0);
        GeneratorWeather weather(1, "Weather", &surface);
        layout.addWidget(&weather);
        surface.resize(400, 520);
        surface.show();
        weather.findChild<QComboBox *>("comboBoxApiSource")->setCurrentIndex(2);
        WeatherObservation o;
        QString error;
        QVERIFY(NationalDataBuoyCenter::parse(noaa(), o, error));
        weather.acceptObservation(o);
        const QPalette previousPalette = qApp->palette();
        for (QString theme : {"Dark", "Light"}) {
            surface.setStyleSheet({});
            qApp->setPalette(Themes::palette(theme));
            surface.setStyleSheet(Themes::styleSheet(theme));
            surface.setPalette(Themes::palette(theme));
            QTest::qWait(20);
            QCOMPARE(surface.palette().color(QPalette::Window), Themes::palette(theme).color(QPalette::Window));
            const auto dir = qEnvironmentVariable("M217A_EVIDENCE");
            if (!dir.isEmpty()) {
                QDir().mkpath(dir);
                auto* tabs=weather.findChild<QTabWidget*>(); QVERIFY(tabs);
                for(int page=0;page<3;++page) {
                    tabs->setCurrentIndex(page); QTest::qWait(20);
                    const QString name=page==0?"weather":page==1?"Marine":"More";
                    QVERIFY(surface.grab().save(dir + "/" + theme + "-" + name + ".png"));
                }
            }
        }
        qApp->setPalette(previousPalette);
    }
    void liveProviders() {
#ifndef M217A_LIVE_QUALIFICATION
        QSKIP("Live qualification uses the separate, ignored read-only credential harness.");
#else
#ifndef M217A_NOAA_ONLY
        QVERIFY2(!liveKey.isEmpty(), "Configured weather credential unavailable; no key was exported.");
#endif
        Workspace w;
        auto *weather = w.weather();
        QVERIFY(weather);
        auto *outside = w.window.findChild<OutsideEnvironmental130310 *>();
        if (!outside) {
            w.controller->addWidgetAsComboboxTextChanged("130310", "Outside", "Environment");
            outside = w.window.findChild<OutsideEnvironmental130310 *>();
        }
        QVERIFY(outside);
        if(auto* movement=w.window.findChild<GeneratorVesselMovement*>()){movement->setLatitude(42.35);movement->setLongitude(-70.65);}
        QTest::qWait(80); // Drain initial vessel-to-weather callbacks before choosing the request snapshot.
        weather->setLatitude(42.35);
        weather->setLongitude(-70.65);
        weather->setOpenWeatherMapApiKey(liveKey);
        auto *source = weather->findChild<QComboBox *>("comboBoxApiSource");
        auto *fetch = weather->findChild<QPushButton *>("pushButtonFetchWeather");
#ifndef M217A_NOAA_ONLY
        source->setCurrentIndex(1);
        QSignalSpy owmReply(weather->findChild<OpenWeatherMap *>(), &OpenWeatherMap::observationFetched);
        fetch->click();
        QTRY_VERIFY_WITH_TIMEOUT(owmReply.count() > 0 || !weather->observationStatus().contains("Fetching"),
                                 18000);
        qInfo() << "LIVE_OWM_STATUS" << weather->observationStatus();
        if (owmReply.count()) {
            const auto o = weather->observation();
            qInfo() << "LIVE_OWM" << o.location << o.observed << o.retrieved << o.air << o.pressure
                    << o.humidity << o.windSpeed << o.windAngle << o.fresh();
        }
        if (owmReply.count() && weather->hasFreshObservation()) {
            QTRY_COMPARE(outside->OutsideAmbientAirTemperature(), weather->AirTemperature());
            QTRY_COMPARE(outside->AtmosphericPressure(), weather->AtmosphericPressure());
            near(w.value(weather, "AirTemperature"), weather->AirTemperature());
            qInfo() << "LIVE_OWM_MODEL_AND_OUTSIDE_PASS";
        }
#endif
        source->setCurrentIndex(2);
        auto *noaaProvider = weather->findChild<NationalDataBuoyCenter *>();
        QSignalSpy stationReply(noaaProvider, &NationalDataBuoyCenter::nearestStationsFound),
            noaaReply(noaaProvider, &NationalDataBuoyCenter::observationFetched);
        QTimer picker;
        picker.setInterval(20);
        connect(&picker, &QTimer::timeout, [] {
            for (auto *widget : QApplication::topLevelWidgets())
                if (auto *d = qobject_cast<QDialog *>(widget); d && d->windowTitle() == "Nearest Stations") {
                    auto *c = d->findChild<QComboBox *>();
                    int i = c->findData(qEnvironmentVariable("M217A_NOAA_STATION", "44013"));
                    if (i >= 0)
                        c->setCurrentIndex(i);
                    d->accept();
                }
        });
        picker.start();
        fetch->click();
        QTRY_VERIFY_WITH_TIMEOUT(noaaReply.count() > 0 ||
                                     (!weather->observationStatus().contains("Fetching") &&
                                      !weather->observationStatus().contains("Choose")),
                                 33000);
        picker.stop();
        qInfo() << "LIVE_NOAA_STATUS" << weather->observationStatus() << "station discoveries"
                << stationReply.count();
        if (noaaReply.count()) {
            const auto o = weather->observation();
            qInfo() << "LIVE_NOAA" << o.station << o.location << o.observed << o.retrieved << o.air << o.sea
                    << o.pressure << o.waveHeight << o.fresh();
            qInfo() << "LIVE_NOAA_MARINE" << o.dominantWavePeriod << o.averageWavePeriod << o.meanWaveDirection;
            QCOMPARE(o.station, qEnvironmentVariable("M217A_NOAA_STATION", "44013"));
            if (o.fresh()) {
                QTRY_COMPARE(weather->WaveHeight(), o.waveHeight);
                QTRY_COMPARE(weather->DominantWavePeriod(), o.dominantWavePeriod);
                QTRY_COMPARE(weather->AverageWavePeriod(), o.averageWavePeriod);
                QTRY_COMPARE(weather->MeanWaveDirection(), o.meanWaveDirection);
                near(w.value(weather, "WaveHeight"), o.waveHeight);
                qInfo() << "LIVE_NOAA_MARINE_MODEL_PASS";
            } else {
                QCOMPARE(weather->WaveHeight(), N2kDoubleNA);
                QVERIFY(!weather->hasFreshObservation());
                qInfo() << "LIVE_NOAA_STALE_OBSERVATION_REJECTED";
            }
        }
        if (noaaReply.count() && weather->hasFreshObservation()) {
            QTRY_COMPARE(outside->WaterTemperature(), weather->SeaTemperature());
            QTRY_COMPARE(outside->OutsideAmbientAirTemperature(), weather->AirTemperature());
            QTRY_COMPARE(outside->AtmosphericPressure(), weather->AtmosphericPressure());
            near(w.value(outside, "WaterTemperature"), weather->SeaTemperature());
            qInfo() << "LIVE_NOAA_MODEL_AND_OUTSIDE_PASS";
        }
        const auto evidence = qEnvironmentVariable("M217A_EVIDENCE");
        if (!evidence.isEmpty()) {
            QDir().mkpath(evidence);
            weather->grab().save(evidence + "/live-noaa-weather.png");
            weather->findChild<QTabWidget*>()->setCurrentIndex(1);
            weather->grab().save(evidence + "/live-noaa-marine.png");
            outside->grab().save(evidence + "/live-noaa-outside.png");
        }
#ifndef M217A_NOAA_ONLY
        QVERIFY2(owmReply.count() == 1,
                 "OpenWeatherMap did not return a qualified observation; see redacted live status.");
#endif
        QVERIFY2(stationReply.count() == 1 && noaaReply.count() == 1,
                 "NOAA discovery/observation qualification incomplete; see status.");
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
#endif
    }
};
int main(int argc, char **argv) {
    if(argc==4 && QByteArray(argv[1])=="--credential-worker") {
        qputenv("RAYNMEA_TEST_HOME",QByteArray(argv[3]));
        int count=2; char standalone[]="--no-nmea"; char* args[]={argv[0],standalone,nullptr};
        QApplication app(count,args);app.setStyle("Fusion");
        QNetworkProxyFactory::setApplicationProxyFactory(new OfflineProxy);
        return credentialWorker(QString::fromLocal8Bit(argv[2]));
    }
    QTemporaryDir profile(QDir::currentPath() + "/m217a-profile-XXXXXX");
    if (!profile.isValid())
        return 2;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QList<QByteArray> args;
    bool live = false;
    for (int i = 0; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--live") {
            live = true;
            continue;
        }
        args << argv[i];
    }
    args << "--no-nmea";
    QList<char *> ptr;
    for (auto &a : args)
        ptr << a.data();
    int count = ptr.size();
    ptr << nullptr;
    QApplication app(count, ptr.data());
    app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(
        QNetworkProxy(live ? QNetworkProxy::DefaultProxy : QNetworkProxy::NoProxy));
    if (!live)
        QNetworkProxyFactory::setApplicationProxyFactory(new OfflineProxy);
    WeatherTests tests;
#if defined(M217A_LIVE_QUALIFICATION) && !defined(M217A_NOAA_ONLY)
    if (live)
        tests.liveKey = weatherLiveKey();
#endif
    QStringList testArgs;
    for (int i = 0; i < argc; ++i)
        if (QByteArray(argv[i]) != "--live")
            testArgs << QString::fromLocal8Bit(argv[i]);
    if (live)
        testArgs.insert(1, "liveProviders");
    return QTest::qExec(&tests, testArgs);
}
#include "weather_tests.moc"
