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
#include <functional>
#include <limits>
#include <QAction>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QNetworkProxy>
#include <QTabWidget>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include "convert.h"
#include "datastructs.h"
#include "environmentalmotion.h"
#define private public
#include "autopilotsimulator.h"
#undef private
#include "generatorvesselmovement.h"
#include "movevesselsimulator.h"
#include "appsettings.h"
#include "mainwindow.h"
#include "themes.h"
#include "generatorengine.h"
#include "cogsog129026.h"

namespace {
void near(double a,double b,double tolerance=.001) {QVERIFY2(std::abs(a-b)<tolerance,qPrintable(QString("%1 != %2").arg(a,0,'g',15).arg(b,0,'g',15)));}
struct Fixture {
    QTemporaryDir directory;GeneratorVesselMovement widget{1,"Vessel Movement",nullptr};
    AutoPilotSimulator *a=widget.findChild<AutoPilotSimulator*>();
    QSpinBox *factor=widget.findChild<QSpinBox*>("spinBoxMultSpeedByX");
    QCheckBox *enabled=widget.findChild<QCheckBox*>("checkBoxAutoPilotEnabled");
    qint64 clock=0;
    Fixture() {
        QFile file(directory.filePath("route.GPX"));if(!file.open(QIODevice::WriteOnly))qFatal("Fixture write failed");
        file.write("<gpx><rte><rtept lat='42' lon='-70'/><rtept lat='42.1' lon='-70'/></rte></gpx>");file.close();
        widget.findChild<QLineEdit*>("lineEditFilePath")->setText(file.fileName());
        a->elapsedNow=[this]{return clock;};widget.resize(750,750);widget.show();
    }
    void start(){enabled->click();a->setSpeed(10);}
};
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode mode) override{return QIODevice::open(mode|QIODevice::Unbuffered);}
    void close() override{QIODevice::close();}
    qint64 bytesToWrite()const override{return 0;}
protected:qint64 writeData(const char *data,qint64 size)override{output.append(data,size);return size;}
};
}
class AutopilotMultiplierUiTests : public QObject {
    Q_OBJECT
private slots:
    void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");}
    void runtimeControlAndNoFeedback() {
        Fixture f;f.start();QVERIFY(f.a->isRunning());const auto baseSpin=f.widget.findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed")->value();
        // Drain genuine initial UI speed commands before choosing the fixture base speed.
        QCoreApplication::processEvents();f.a->setSpeed(10);
        f.clock=1000;f.a->calculateNextCoordinate();near(f.a->totalDistanceTraveled,10);
        const auto p=f.a->position;const auto index=f.a->currentIndex;
        f.factor->setValue(2);QCOMPARE(f.a->position,p);QCOMPARE(f.a->currentIndex,index);near(f.widget.SOG(),20);
        const double eta=f.a->vesselWpBearing.RemainingTimeS;
        for(int i=0;i<8;++i){QCoreApplication::processEvents();near(f.a->getSpeed(),10);near(f.widget.SOG(),20);}
        f.clock=2000;f.a->calculateNextCoordinate();near(f.a->totalDistanceTraveled,30);
        near(f.widget.WaypointClosingVelocity(),20);QVERIFY(f.a->vesselWpBearing.RemainingTimeS<eta);
        f.factor->setValue(1);near(f.widget.SOG(),10);f.clock=3000;f.a->calculateNextCoordinate();near(f.a->totalDistanceTraveled,40);
        QCOMPARE(f.widget.findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed")->value(),baseSpin);
        f.enabled->click();QVERIFY(!f.a->isRunning());near(f.widget.SOG(),0);const auto stopped=f.a->position;
        f.factor->setValue(10);QCoreApplication::processEvents();QCOMPARE(f.a->position,stopped);QVERIFY(!f.enabled->isChecked());
    }
    void actualRoutePollingDoesNotBecomeSpeedInput() {
        Fixture f;
        f.widget.findChild<QLineEdit*>("lineEditFilePath")->setText(QDir::currentPath()+"/routes/kml/mr_haverhill.kml");
        f.widget.findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed")->setValue(50);
        QCoreApplication::processEvents();f.factor->setValue(2);f.enabled->click();
        QVERIFY(f.a->isRunning());const double base=f.a->getSpeed();
        f.a->environment.enabled=true;f.a->environment.source=[] {EnvironmentalConditions c;c.windValid=true;c.windSpeed=8;c.windFrom=M_PI/2;return c;};
        for(int i=1;i<=34;++i) {
            f.clock=i*500;f.a->calculateNextCoordinate();
            QMetaObject::invokeMethod(&f.widget,"PollTimerElapsed",Qt::DirectConnection);
            QCoreApplication::processEvents();
            QVERIFY2(std::abs(f.a->getSpeed()-base)<1e-9,qPrintable(QString("poll %1 changed base %2 to %3; distance %4 m").arg(i).arg(base).arg(f.a->getSpeed()).arg(f.a->totalDistanceTraveled)));
        }
        QVERIFY(f.a->totalDistanceTraveled<2*base*17+50);
        f.enabled->click();const auto stopped=f.a->position;
        QMetaObject::invokeMethod(&f.widget,"PollTimerElapsed",Qt::DirectConnection);QCoreApplication::processEvents();
        f.clock+=1000;f.a->calculateNextCoordinate();QCOMPARE(f.a->position,stopped);
    }
    void actualRouteMatchedSpeedPolling_data() {
        QTest::addColumn<int>("weather");QTest::addColumn<int>("factor");
        for(int weather:{0,1,2})for(int factor:{1,2,4})
            QTest::newRow(qPrintable(QString("weather%1-factor%2").arg(weather).arg(factor)))<<weather<<factor;
    }
    void actualRouteMatchedSpeedPolling() {
        QFETCH(int,weather);QFETCH(int,factor);Fixture f;
        f.widget.findChild<QLineEdit*>("lineEditFilePath")->setText(QDir::currentPath()+"/routes/kml/mr_haverhill.kml");
        auto *speed=f.widget.findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed");speed->setValue(100.0/factor);
        f.factor->setValue(factor);f.widget.setEnvironmentalInfluence(weather!=0);
        f.widget.setEnvironmentalSource([weather] {EnvironmentalConditions c;c.windValid=weather==1;c.wavesValid=weather==2;
            c.windSpeed=8;c.windFrom=M_PI/2;c.waveHeight=2;c.wavePeriod=8;c.waveFrom=.7;return c;});
        f.enabled->click();QVERIFY(f.a->isRunning());near(f.a->getSpeed(),44.704/factor);
        for(int tick=1;tick<=286;++tick) {
            f.clock=tick*500;f.a->calculateNextCoordinate();
            QMetaObject::invokeMethod(&f.widget,"PollTimerElapsed",Qt::DirectConnection);QCoreApplication::processEvents();
            near(f.a->getSpeed(),44.704/factor);QVERIFY(f.enabled->isChecked());QVERIFY(f.a->position.isValid());
            QVERIFY(f.a->totalDistanceTraveled<=45*143+1);QVERIFY(std::abs(f.a->vesselWpBearing.Xte)<30);
        }
        QCOMPARE(speed->value(),100.0/factor);near(f.a->vesselWpBearing.TimeTraveledS,143);
        QVERIFY(f.widget.SOG()>44 && f.widget.SOG()<45.5);
        // Explicit disable freezes traversal and polling never restarts it.
        f.enabled->click();const auto p=f.a->position;const auto total=f.a->totalDistanceTraveled;
        f.clock+=500;QMetaObject::invokeMethod(&f.widget,"PollTimerElapsed",Qt::DirectConnection);QCoreApplication::processEvents();f.a->calculateNextCoordinate();
        QCOMPARE(f.a->position,p);near(f.a->totalDistanceTraveled,total);QVERIFY(!f.enabled->isChecked());
        f.enabled->click();QVERIFY(f.a->isRunning());near(f.a->totalDistanceTraveled,0);
        // Completion drives the existing checkbox lifecycle without retaining
        // a running traversal behind an unchecked Enable control.
        for(int tick=0;tick<2000 && f.a->isRunning();++tick){f.clock+=1000;f.a->calculateNextCoordinate();}
        QVERIFY(!f.a->isRunning());QVERIFY(!f.enabled->isChecked());near(f.widget.SOG(),0);
    }
    void invariantFailureStopsVisibleRun() {
        Fixture f;f.start();f.widget.setEnvironmentalInfluence(true);
        f.widget.setEnvironmentalSource([] {EnvironmentalConditions c;c.windValid=true;c.windSpeed=8;return c;});
        f.a->totalDistanceTraveled=std::numeric_limits<double>::infinity();
        QSignalSpy status(&f.widget,&GeneratorVesselMovement::ShowStatusBarMessage);
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression("Autopilot navigation invariant failed: ground distance/position invariant.*"));
        f.a->advance(1);QVERIFY(!f.a->isRunning());QVERIFY(!f.enabled->isChecked());
        QVERIFY(!status.isEmpty());QVERIFY(status.last()[0].toString().contains("invariant failed"));
    }
    void selectedSpeedSourceRemainsAuthoritative() {
        Fixture f;auto *source=f.widget.findChild<QComboBox*>("comboBoxSpeedSource");
        source->setCurrentIndex(AutopilotSpeedSource::ThrottlePosition);f.widget.setThrottlePosition(30);
        f.enabled->click();QVERIFY(f.a->isRunning());const double throttleSpeed=f.a->getSpeed();
        f.widget.findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed")->setValue(90);near(f.a->getSpeed(),throttleSpeed);
        QMetaObject::invokeMethod(&f.widget,"PollTimerElapsed",Qt::DirectConnection);QCoreApplication::processEvents();near(f.a->getSpeed(),throttleSpeed);
        f.widget.setThrottlePosition(60);QVERIFY(f.a->getSpeed()!=throttleSpeed);f.enabled->click();
        source->setCurrentIndex(AutopilotSpeedSource::RouteFile);f.factor->setValue(2);f.enabled->click();
        const double routeSpeed=f.a->route[0].speedMs;
        f.widget.findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed")->setValue(45);
        QMetaObject::invokeMethod(&f.widget,"PollTimerElapsed",Qt::DirectConnection);QCoreApplication::processEvents();near(f.a->getSpeed(),routeSpeed);
        f.enabled->click();
    }
    void persistenceAndInvalidLayouts() {
        Fixture f;QCOMPARE(f.factor->value(),1);f.factor->setValue(5);auto saved=f.widget.layoutConfiguration();QCOMPARE(saved["autopilotMultiplier"].toInt(),5);
        GeneratorVesselMovement restored(2,"Restored",nullptr);restored.restoreLayoutConfiguration(saved);
        QCOMPARE(restored.layoutConfiguration()["autopilotMultiplier"].toInt(),5);QVERIFY(!restored.findChild<AutoPilotSimulator*>()->isRunning());
        saved.remove("autopilotMultiplier");restored.restoreLayoutConfiguration(saved);QCOMPARE(restored.layoutConfiguration()["autopilotMultiplier"].toInt(),1);
        for(const auto &invalid:QList<QJsonValue>{0,-2,11,1.5,QString("2"),QJsonValue::Null}) {
            saved["autopilotMultiplier"]=invalid;restored.restoreLayoutConfiguration(saved);QCOMPARE(restored.layoutConfiguration()["autopilotMultiplier"].toInt(),1);
        }
        f.factor->setValue(0);QCOMPARE(f.factor->value(),1);f.factor->setValue(100);QCOMPARE(f.factor->value(),10);
    }
    void liveModelLayoutAndViewer() {
        MainWindow window;window.show();auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        const auto confirm=[] {QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox*>(w))box->button(QMessageBox::Yes)->click();});};
        confirm();QVERIFY(controller->loadLayoutFromJson(":/defaults/defaultLayout.json"));
        auto *movement=window.findChild<GeneratorVesselMovement*>();QVERIFY(movement);
        for(auto *engine:window.findChildren<GeneratorEngine*>())engine->setEngineProfile("MerCruiser 7.4 MPI");
        movement->findChild<QLineEdit*>("lineEditFilePath")->setText(QDir::currentPath()+"/routes/gpx/test_route1.gpx");
        auto *factor=movement->findChild<QSpinBox*>("spinBoxMultSpeedByX");factor->setValue(2);
        auto *a=movement->findChild<AutoPilotSimulator*>();a->elapsedNow=[] {return 0;};
        movement->findChild<QCheckBox*>("checkBoxAutoPilotEnabled")->click();QVERIFY(a->isRunning());QCoreApplication::processEvents();a->setSpeed(10);a->advance(1);
        for(int tick=0;tick<8;++tick) {
            QMetaObject::invokeMethod(movement,"PollTimerElapsed",Qt::DirectConnection);
            QCoreApplication::processEvents();near(movement->SOG(),20);near(a->getSpeed(),10);
        }
        auto *instrument=window.findChild<CogSog129026*>();QVERIFY(instrument);near(instrument->SOG(),20);
        auto *model=window.findChild<PgnTableController*>()->getPgnTableModel();bool found=false;
        for(const auto &row:model->getPgnParams(instrument->Id()))if(row[0]=="SOG"){near(row[1].toDouble(),20);found=true;}QVERIFY(found);
        factor->setValue(5);QTemporaryDir directory;const auto saved=directory.filePath("layout.json");QVERIFY(controller->saveLayoutToJson(saved));
        QFile file(saved);QVERIFY(file.open(QIODevice::ReadOnly));auto root=QJsonDocument::fromJson(file.readAll()).object();file.close();
        auto groups=root["layouts"].toArray();bool changed=false;
        for(int i=0;i<groups.size();++i){auto group=groups[i].toObject();auto widgets=group["widgets"].toArray();
            for(int j=0;j<widgets.size();++j){auto item=widgets[j].toObject();if(item["type"]=="GeneratorVesselMovement") {auto config=item["configuration"].toObject();config["autopilotMultiplier"]=0;item["configuration"]=config;widgets[j]=item;changed=true;}}
            group["widgets"]=widgets;groups[i]=group;
        }
        QVERIFY(changed);root["layouts"]=groups;const auto invalid=directory.filePath("invalid.json");QFile bad(invalid);QVERIFY(bad.open(QIODevice::WriteOnly));bad.write(QJsonDocument(root).toJson());bad.close();
        const auto position=a->position;QVERIFY(!controller->loadLayoutFromJson(invalid));QCOMPARE(a->position,position);QVERIFY(a->isRunning());QCOMPARE(factor->value(),5);
        controller->setMode(Viewer);QVERIFY(!a->isRunning());controller->setMode(Sender);QVERIFY(!a->isRunning());QCOMPARE(factor->value(),5);
        confirm();QVERIFY(controller->loadLayoutFromJson(saved));movement=window.findChild<GeneratorVesselMovement*>();QVERIFY(movement);
        QCOMPARE(movement->findChild<QSpinBox*>("spinBoxMultSpeedByX")->value(),5);QVERIFY(!movement->findChild<AutoPilotSimulator*>()->isRunning());
        QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
    void manualMovementIsolation() {
        GeneratorVesselMovement a(1,"A",nullptr),b(2,"B",nullptr);
        b.findChild<QSpinBox*>("spinBoxMultSpeedByX")->setValue(10);
        for(auto *w:{&a,&b}) {
            w->setLatitude(42);w->setLongitude(-70);w->setHeading(0);
            w->findChild<QCheckBox*>("checkBoxManualMovementEnabled")->click();
            w->setTransmissionGear(N2kTG_Forward);w->setTransmissionGearStbd(N2kTG_Forward);w->setThrottlePosition(40);w->setThrottlePositionStbd(40);
            QMetaObject::invokeMethod(w->findChild<MoveVesselSimulator*>(),"updateVesselPosition",Qt::DirectConnection);
        }
        near(a.SOG(),b.SOG());near(a.Latitude(),b.Latitude(),1e-8);near(a.Longitude(),b.Longitude(),1e-8);
        QVERIFY(!a.findChild<AutoPilotSimulator*>()->isRunning());QVERIFY(!b.findChild<AutoPilotSimulator*>()->isRunning());
    }
    void failedImportAndOperationalStop() {
        Fixture f;f.start();QCoreApplication::processEvents();f.a->setSpeed(10);f.clock=1000;f.a->calculateNextCoordinate();f.factor->setValue(2);
        const auto p=f.a->position;const auto distance=f.a->totalDistanceTraveled;
        QVERIFY(!f.a->loadRoute(f.directory.filePath("missing.KML")));QCOMPARE(f.a->position,p);near(f.a->totalDistanceTraveled,distance);QCOMPARE(f.factor->value(),2);QVERIFY(f.enabled->isChecked());
        f.widget.stopOperationalActivity();QVERIFY(!f.a->isRunning());f.factor->setValue(5);QCoreApplication::processEvents();QVERIFY(!f.a->isRunning());QVERIFY(!f.enabled->isChecked());
    }
    void queuedInputCannotCrossStopRestart() {
        Fixture f;f.widget.findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed")->setValue(12);QCoreApplication::processEvents();
        f.enabled->click();QVERIFY(f.a->isRunning());QCoreApplication::processEvents();const auto base=f.a->getSpeed();QVERIFY(base>0);
        f.widget.setSOG(0); // Measurement notification must not command propulsion, now or after restart.
        f.enabled->click();QVERIFY(!f.a->isRunning());f.enabled->click();QVERIFY(f.a->isRunning());
        near(f.a->getSpeed(),base);QCoreApplication::processEvents();near(f.a->getSpeed(),base);
        f.factor->setValue(2);QCoreApplication::processEvents();near(f.a->getSpeed(),base);near(f.widget.SOG(),2*base);
        f.enabled->click();QVERIFY(!f.a->isRunning());
    }
    void sendingAuthorizationAndF12() {
        AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Nmea0183/Nmea0183Enabled",true);settings.setValue("Nmea0183/Nmea0183Port","MEMORY");settings.sync();
        auto *port=new MemoryPort;MainWindow window(nullptr,nullptr,port);window.show();
        auto *start=window.findChild<QAction*>("actionStart_Sending");auto *worker=window.findChild<NmeaSenderWorker*>();QVERIFY(start);QVERIFY(worker);QVERIFY(!worker->isSending());
        Fixture f;f.start();f.factor->setValue(2);QCoreApplication::processEvents();QVERIFY(!worker->isSending());QVERIFY(port->output.isEmpty());
        auto *model=window.findChild<PgnTableModel*>();QVERIFY(model);
        model->addPgn(99001,"Fixture","HDT",true,{{"Heading","1","rad","double"}}, {},250);
        start->trigger();QVERIFY(worker->isSending());f.factor->setValue(5);QVERIFY(worker->isSending());
        QTRY_VERIFY_WITH_TIMEOUT(!port->output.isEmpty(),1500);
        QTest::keyClick(&window,Qt::Key_F12);QVERIFY(!worker->isSending());QVERIFY(!start->isChecked());
        f.factor->setValue(1);QCoreApplication::processEvents();QVERIFY(!worker->isSending());
        f.enabled->click();QVERIFY(!f.a->isRunning());
    }
    void renderedTooltips() {
        Fixture f;
        f.widget.findChild<QLineEdit*>("lineEditFilePath")->setText(QDir::currentPath()+"/routes/kml/mr_haverhill.kml");
        f.widget.findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed")->setValue(50);f.factor->setValue(2);
        f.widget.setEnvironmentalInfluence(true);f.widget.setEnvironmentalSource([]{EnvironmentalConditions c;c.windValid=true;c.windSpeed=8;c.windFrom=M_PI/2;return c;});
        f.enabled->click();f.clock=17000;f.a->calculateNextCoordinate();
        auto *tabs=f.widget.findChild<QTabWidget*>();QVERIFY(tabs);
        for(int i=0;i<tabs->count();++i)if(tabs->tabText(i).contains("Autopilot",Qt::CaseInsensitive))tabs->setCurrentIndex(i);
        for(const auto &theme:{QString("Dark"),QString("Light")}) {
            qApp->setPalette(Themes::palette(theme));qApp->setStyleSheet(Themes::styleSheet(theme));f.factor->setValue(2);QTest::qWait(30);
            QVERIFY(f.factor->toolTip().contains("base speed"));const auto out=qEnvironmentVariable("M226_EVIDENCE");
            if(!out.isEmpty()){QDir().mkpath(out);QVERIFY(f.widget.grab().save(out+"/autopilot-"+theme+".png"));}
        }
        qApp->setStyleSheet({});
    }
};
int main(int argc,char**argv){QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));AutopilotMultiplierUiTests tests;return QTest::qExec(&tests,argc,argv);}
#include "autopilot_multiplier_ui_tests.moc"
