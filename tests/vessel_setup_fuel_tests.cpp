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

#include "viewer_test_driver.h"
#include <QtTest>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QSlider>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "dialogsetup.h"
#include "appsettings.h"
#include "preferences.h"
#include "convert.h"
#include "fluidlevel127505.h"
#include "pgntreecontroller.h"
#include "signalkpublisher.h"
#include "themes.h"
#include "N2kMessages.h"

namespace {
void near(double actual, double expected, double tolerance=1e-9) {
    QVERIFY2(qAbs(actual-expected)<=tolerance,qPrintable(QString("%1 != %2").arg(actual,0,'g',17).arg(expected,0,'g',17)));
}
void confirm() { QTimer::singleShot(0, [] {
    for(auto* w:QApplication::topLevelWidgets()) if(auto* m=qobject_cast<QMessageBox*>(w)) m->button(QMessageBox::Yes)->click();
}); }
struct Workspace {
    MainWindow window;
    MainController* controller=window.findChild<MainController*>();
    PgnTableModel* model=window.findChild<PgnTableController*>()->getPgnTableModel();
    PgnTreeController* tree=window.findChild<PgnTreeController*>();
    Workspace() { window.resize(1400,900);window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));QTest::qWait(20); }
    double value(QWidget* w,const QString& field) {
        for(const auto& row:model->findPgn(w->property("Id").toInt())->paramDetails) if(row[0]==field)return row[1].toDouble();
        return qQNaN();
    }
    void edit(QWidget* w,const QString& field,double value) {
        model->setCurrentPgn(w->property("Id").toInt());
        for(int row=0;row<model->rowCount();++row)
            if(model->data(model->index(row,0)).toString()==field) { model->setData(model->index(row,1),value);return; }
    }
    FluidLevel127505* tank(int id,int instance,double capacity,double level,int type=N2kft_Fuel) {
        WidgetConfig c{};c.idX=id;c.widgetType="FluidLevel127505";c.pgn="127505";c.label="Tank "+QString::number(id);c.rowSpan=3;c.columnSpan=3;c.interval=250;
        c.parameters={{"Instance",QString::number(instance),"","unsigned char"},{"FluidType",QString::number(type),"enum","tN2kFluidType"},
                      {"Level",QString::number(level,'g',17),"%","double"},{"Capacity",QString::number(capacity,'g',17),"L","double"}};
        controller->restoreLayoutFromWidgetConfig(c,"Tanks");
        for(auto* w:controller->layoutWidgets("Tanks"))if(w->property("Id").toInt()==id)return qobject_cast<FluidLevel127505*>(w);
        return nullptr;
    }
    GeneratorEngine* engine(int id,int instance) {
        QFile f(":/defaults/defaultLayout.json");f.open(QIODevice::ReadOnly);
        for(const auto group:QJsonDocument::fromJson(f.readAll()).object()["layouts"].toArray()) for(const auto entry:group.toObject()["widgets"].toArray()) {
            const auto o=entry.toObject();if(o["type"]!="GeneratorEngine")continue;
            WidgetConfig c{};c.idX=id;c.widgetType="GeneratorEngine";c.pgn="-127489";c.label="Engine "+QString::number(instance);c.rowSpan=9;c.columnSpan=3;c.interval=1000;
            c.properties.insert("engineProfile","MerCruiser 7.4 MPI");
            for(const auto row:o["parameters"].toArray()) {QList<QString> p;for(const auto v:row.toArray())p<<v.toString();if(p[0]=="EngineInstance")p[1]=QString::number(instance);c.parameters<<p;}
            controller->restoreLayoutFromWidgetConfig(c,"Generate");
            for(auto* w:controller->layoutWidgets("Generate"))if(w->property("Id").toInt()==id)return qobject_cast<GeneratorEngine*>(w);
        }
        return nullptr;
    }
};
void runEngine(GeneratorEngine* e,double rate) {
    e->setEngineRunning(true);
    for(auto* t:e->findChildren<QTimer*>())t->stop(); // Deterministic elapsed-time fixture; production uses its normal poll.
    e->setFuelRate(rate);
}
void advance(GeneratorEngine* e,double seconds) { QVERIFY(QMetaObject::invokeMethod(e,"consumeElapsedFuel",Qt::DirectConnection,Q_ARG(double,seconds))); }
QDoubleSpinBox* spin(DialogSetup* d,const char* name){return d->findChild<QDoubleSpinBox*>(name);}
void reload(DialogSetup* d){QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonLoad_clicked"));}
void save(DialogSetup* d){QSignalSpy saved(d,&DialogSetup::ReloadSettings);QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));QCOMPARE(saved.count(),1);}
}

class VesselSetupFuelTests:public QObject {
    Q_OBJECT
private slots:
    void init() {
        AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");s.setValue("Preference/StyleSheetName","Dark");
        QLocale::setDefault(QLocale::c());
    }
    void setupPrecisionAndRoundTrips() {
        Workspace w;auto* d=w.window.findChild<DialogSetup*>();d->show();QTest::qWait(30);
        auto* capacity=spin(d,"spinBoxPortFuelTankSize");auto* quantity=spin(d,"spinBoxPortFuelLevel");
        QCOMPARE(capacity->text(),QString("100.00"));QCOMPARE(quantity->text(),QString("90.00"));
        capacity->setValue(100);quantity->setValue(90);spin(d,"doubleSpinBoxLength")->setValue(35.125);spin(d,"doubleSpinBoxWotSpeed")->setValue(40.125);
        save(d);reload(d);
        AppSettings s(APP_COMPANY,APP_NAME);near(s.value("Vessel/PortFuelTankSize").toDouble(),Convert::USGallonsToLiters(100));near(s.value("Vessel/PortFuelLevel").toDouble(),Convert::USGallonsToLiters(90));
        // Saving Setup refreshes the controller used by normal Add Widget.
        QTimer::singleShot(0,[]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QDialog*>(widget);dialog&&dialog->windowTitle()=="Select Fluid Source")dialog->accept();});
        w.controller->addWidgetAsComboboxTextChanged("127505","Fuel","Tanks");
        auto* initialized=w.window.findChild<FluidLevel127505*>();QVERIFY(initialized);
        near(initialized->Capacity(),s.value("Vessel/PortFuelTankSize").toDouble());near(initialized->Level(),90);
        // Retain more fractional precision than the compact idle display.
        capacity->setFocus();auto* edit=capacity->findChild<QLineEdit*>();edit->selectAll();QTest::keyClicks(edit,"100.123456");QTest::keyClick(edit,Qt::Key_Return);quantity->setFocus();
        save(d);const double precise=Convert::USGallonsToLiters(100.123456);near(s.value("Vessel/PortFuelTankSize").toDouble(),precise,1e-8);
        const double originalCapacity=s.value("Vessel/PortFuelTankSize").toDouble();
        const double originalLength=s.value("Vessel/Length").toDouble(),originalSpeed=s.value("Vessel/WotSpeed").toDouble();
        d->findChild<QWidget*>("pushButtonLoad")->setFocus();
        for(int i=0;i<20;++i) {
            capacity->setFocus();quantity->setFocus();d->findChild<QWidget*>("pushButtonLoad")->setFocus();
            reload(d);d->findChild<QComboBox*>("comboBoxUnitsVolume")->setCurrentIndex(i%2);d->findChild<QComboBox*>("comboBoxUnitsDistance")->setCurrentIndex(i%3);d->findChild<QComboBox*>("comboBoxUnitsSpeed")->setCurrentIndex(i%4);
            save(d);s.sync();QCOMPARE(s.value("Vessel/PortFuelTankSize").toDouble(),originalCapacity);QCOMPARE(s.value("Vessel/Length").toDouble(),originalLength);QCOMPARE(s.value("Vessel/WotSpeed").toDouble(),originalSpeed);
        }
        d->findChild<QComboBox*>("comboBoxUnitsVolume")->setCurrentIndex(0);d->findChild<QComboBox*>("comboBoxUnitsDistance")->setCurrentIndex(0);
        capacity->setValue(381.123456);spin(d,"doubleSpinBoxLength")->setValue(10.1234567);save(d);reload(d);
        d->findChild<QComboBox*>("comboBoxUnitsVolume")->setCurrentIndex(1);d->findChild<QComboBox*>("comboBoxUnitsDistance")->setCurrentIndex(2);
        save(d);s.sync();near(s.value("Vessel/PortFuelTankSize").toDouble(),381.123456,1e-8);near(s.value("Vessel/Length").toDouble(),10.1234567);
        spin(d,"doubleSpinBoxLength")->setValue(33.125);save(d);s.sync();near(s.value("Vessel/Length").toDouble(),Convert::FeetToMeters(33.125));
        // Legacy shortened canonical values must be displayed cleanly, not guessed/requantized.
        s.setValue("Vessel/PortFuelTankSize",378.541);s.setValue("Vessel/PortFuelLevel",340.6869);s.setValue("Preference/UnitsVolume",1);s.sync();reload(d);
        QCOMPARE(capacity->text(),QString("100.00"));QCOMPARE(quantity->text(),QString("90.00"));save(d);s.sync();QCOMPARE(s.value("Vessel/PortFuelTankSize").toDouble(),378.541);
        d->reject();
    }
    void defaultsAndRestoration() {
        Workspace w;MyVessel vessel;vessel.PortFuelTankSize=Convert::USGallonsToLiters(100);vessel.PortFuelTankLevel=Convert::USGallonsToLiters(90);vessel.StbdFuelTankSize=200.5;vessel.StbdFuelTankLevel=50.125;w.controller->setMyVessel(vessel);
        // A non-fuel instance must not claim Port's fuel identity.
        auto* water=w.tank(1,0,55,40,N2kft_Water);QVERIFY(water);
        for(int count=0;count<2;++count) {
            QTimer::singleShot(0,[]{for(auto* w:QApplication::topLevelWidgets())if(auto* d=qobject_cast<QDialog*>(w);d&&d->windowTitle()=="Select Fluid Source")d->accept();});
            w.controller->addWidgetAsComboboxTextChanged("127505","Fuel","Tanks");
        }
        auto tanks=w.controller->layoutWidgets("Tanks");QCOMPARE(tanks.size(),3);
        auto* port=qobject_cast<FluidLevel127505*>(tanks[1]);auto* starboard=qobject_cast<FluidLevel127505*>(tanks[2]);
        QCOMPARE(port->Instance(),0);near(port->Capacity(),vessel.PortFuelTankSize);near(port->Level(),90);QCOMPARE(starboard->Instance(),1);near(starboard->Capacity(),200.5);near(starboard->Level(),25);
        near(w.value(port,"Capacity"),vessel.PortFuelTankSize);near(water->Capacity(),55);near(water->Level(),40);
        // Restored/manual values are authoritative, including instance 0.
        auto* manual=w.tank(30,0,123.456789,37.125);near(manual->Capacity(),123.456789);near(manual->Level(),37.125);
        QTemporaryDir dir(QDir::currentPath()+"/m216a-layout-XXXXXX");const auto path=dir.filePath("fuel.json");QVERIFY(w.controller->saveLayoutToJson(path));
        w.controller->setMyVessel(MyVessel{});confirm();QVERIFY(w.controller->loadLayoutFromJson(path));
        for(auto* t:w.window.findChildren<FluidLevel127505*>())if(t->Id()==30){near(t->Capacity(),123.456789);near(t->Level(),37.125);}
        // Setup quantities above capacity and zero capacity have safe initialization.
        w.controller->removeAllWidgets();vessel.PortFuelTankSize=0;vessel.StbdFuelTankSize=10;vessel.StbdFuelTankLevel=20;w.controller->setMyVessel(vessel);
        for(int i=0;i<2;++i){QTimer::singleShot(0,[]{for(auto* w:QApplication::topLevelWidgets())if(auto* d=qobject_cast<QDialog*>(w);d&&d->windowTitle()=="Select Fluid Source")d->accept();});w.controller->addWidgetAsComboboxTextChanged("127505","Fuel","Tanks");}
        auto list=w.window.findChildren<FluidLevel127505*>();near(list[0]->Level(),0);near(list[0]->Capacity(),0);near(list[1]->Level(),100);
    }
    void independentConsumptionAndBounds() {
        Workspace w;auto* port=w.tank(1,0,100,90);auto* stbd=w.tank(2,1,200,50);auto* water=w.tank(3,0,100,90,N2kft_Water);auto* other=w.tank(4,2,100,80);
        auto* e0=w.engine(10,0);auto* e1=w.engine(11,1);QVERIFY(e0&&e1&&e0->profileReady()&&e1->profileReady());runEngine(e0,10);runEngine(e1,30);
        advance(e0,3600);advance(e1,3600);near(port->Level(),80,.001);near(stbd->Level(),35,.001);near(water->Level(),90);near(other->Level(),80);
        auto* sender=w.window.findChild<NmeaSenderWorker*>();QVERIFY(!sender->isSending());QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
        e0->setEngineRunning(false);double stopped=port->Level();advance(e0,3600);near(port->Level(),stopped);
        e1->setFuelRate(0);double zero=stbd->Level();advance(e1,3600);near(stbd->Level(),zero);
        e1->setFuelRate(1000);advance(e1,3600);near(stbd->Level(),0);advance(e1,3600);near(stbd->Level(),0);
        e1->setFuelRate(qQNaN());advance(e1,3600);near(stbd->Level(),0);
        runEngine(e0,10);e0->PgnWidgetBase::setEnabled(false);stopped=port->Level();advance(e0,3600);near(port->Level(),stopped);e0->PgnWidgetBase::setEnabled(true);
        w.edit(port,"Level",25);w.edit(port,"Capacity",40);advance(e0,3600);near(port->Level(),0,.001);near(w.value(port,"Level"),port->Level());
    }
    void liveModelTreeAndOutput() {
        Workspace w;auto* tank=w.tank(1,0,100,90);auto* engine=w.engine(10,0);runEngine(engine,36);QTest::qWait(40);
        auto* tree=w.tree->getTreeModel();QPersistentModelIndex index(tree->indexFor(tank,"Level"));QVERIFY(index.isValid());QSignalSpy resets(tree,&QAbstractItemModel::modelReset);
        advance(engine,1000);QTest::qWait(80);near(tank->Level(),80,.001);near(w.value(tank,"Level"),tank->Level());
        // Preserve the accepted one-decimal Object Tree percentage presentation;
        // the canonical value is independently checked above and encoded below.
        QCOMPARE(tree->data(QModelIndex(index).siblingAtColumn(1)).toString(),QString::number(tank->Level(),'f',1)+" %");QVERIFY(index.isValid());QCOMPARE(resets.count(),0);
        auto* sender=w.window.findChild<NmeaSenderWorker*>();QSignalSpy sent(sender,&NmeaSenderWorker::sendPgn);QCOMPARE(sent.count(),0);sender->startSending();sender->sendNmeaMessage();sender->stopSending();
        bool found=false;for(const auto& call:sent)if(call[0].toInt()==127505){const auto params=call[1].toStringList();for(const auto& p:params)if(p.startsWith("Level:")){near(p.mid(6).toDouble(),tank->Level(),1e-8);found=true;}}QVERIFY(found);
        tN2kMsg message;SetN2kPGN127505(message,tank->Instance(),tank->FluidType(),tank->Level(),tank->Capacity());
        bool mapped=false;for(const auto item:SignalKPublisher::values(message)){const auto o=item.toObject();if(o["path"]=="tanks.fuel.0.currentLevel"){near(o["value"].toDouble(),.8,.0001);mapped=true;}}QVERIFY(mapped);
        // Instrument Enable controls publication, not the local tank's physical quantity.
        tank->setEnabled(false);advance(engine,100);near(tank->Level(),79,.001);near(w.value(tank,"Level"),tank->Level());
    }
    void generatedRatesAndInstanceMapping() {
        Workspace w;auto* port=w.tank(1,0,1000,90);auto* starboard=w.tank(2,1,1000,90,N2kft_FuelGasoline);
        auto* e0=w.engine(10,0);auto* e1=w.engine(11,1);runEngine(e0,0);runEngine(e1,0);
        e0->findChild<QSlider*>("verticalSliderThrottle")->setValue(1800);e1->findChild<QSlider*>("verticalSliderThrottle")->setValue(3000);
        for(int i=0;i<4;++i){QVERIFY(QMetaObject::invokeMethod(e0,"PollTimerElapsed"));QVERIFY(QMetaObject::invokeMethod(e1,"PollTimerElapsed"));}
        const double rate0=e0->FuelRate(),rate1=e1->FuelRate();QVERIFY(rate0>0&&rate1>rate0);
        const double start0=port->Level(),start1=starboard->Level();advance(e0,3600);advance(e1,3600);
        near(port->Level(),start0-rate0/10,.001);near(starboard->Level(),start1-rate1/10,.001);
        w.controller->removeWidgetFromSubForm(e1,"Generate");e0->setEngineInstance(1);const double portBefore=port->Level(),starBefore=starboard->Level();advance(e0,3600);
        near(port->Level(),portBefore);near(starboard->Level(),starBefore-rate0/10,.001);
        e0->setEngineInstance(2);const double unchanged=starboard->Level();advance(e0,3600);near(starboard->Level(),unchanged);
        auto* duplicate=w.engine(12,0);runEngine(duplicate,10);e0->setEngineInstance(0);
        // The established registry selects one generator for this duplicate ID.
        const double beforeDuplicate=port->Level();advance(e0,3600);near(port->Level(),beforeDuplicate);advance(duplicate,3600);near(port->Level(),beforeDuplicate-1,.001);
    }
    void modesRemovalAndReload() {
        Workspace w;auto* tank=w.tank(1,0,100,90);auto* engine=w.engine(10,0);runEngine(engine,10);advance(engine,360);double level=tank->Level();
        QVERIFY(TestDialogs::enterViewerKeepingLayout(w.window));QVERIFY(!engine->EngineRunning());level=tank->Level();advance(engine,3600);near(tank->Level(),level);
        w.window.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);QVERIFY(!engine->EngineRunning());advance(engine,3600);near(tank->Level(),level);
        for(int i=0;i<3;++i){runEngine(engine,10);advance(engine,36);engine->setEngineRunning(false);}near(tank->Level(),level-.3,.002);
        auto* retained=tank;w.window.HotReloadSettings();QCOMPARE(tank,retained);near(tank->Capacity(),100); // no Setup refill
        QTemporaryDir dir(QDir::currentPath()+"/m216a-layout-XXXXXX");auto path=dir.filePath("fuel.json");QVERIFY(w.controller->saveLayoutToJson(path));
        QPointer<GeneratorEngine> stale(engine);confirm();QVERIFY(w.controller->loadLayoutFromJson(path));QVERIFY(!stale);auto* restored=w.window.findChild<GeneratorEngine*>();QVERIFY(restored&&!restored->EngineRunning());
        auto* restoredTank=w.window.findChild<FluidLevel127505*>();const double saved=restoredTank->Level();advance(restored,3600);near(restoredTank->Level(),saved);
        w.controller->removeWidgetFromSubForm(restoredTank,"Tanks");runEngine(restored,10);advance(restored,3600); // no stale callback or implicit tank recreation
        QVERIFY(w.window.findChildren<FluidLevel127505*>().isEmpty());
    }
    void elapsedTimeAndRepeatedSteps() {
        GeneratorEngine engine(1,"Timing");engine.setEngineProfile("MerCruiser 7.4 MPI");runEngine(&engine,3600);QSignalSpy usage(&engine,&GeneratorEngine::fuelConsumed);
        QElapsedTimer elapsed;elapsed.start();for(int i=0;i<8;++i){QTest::qWait(25);QVERIFY(QMetaObject::invokeMethod(&engine,"accountFuelUsage"));}engine.setEngineRunning(false);
        double liters=0;for(const auto& call:usage)liters+=call[1].toDouble();near(liters,elapsed.nsecsElapsed()/1e9,.035);
        runEngine(&engine,3.6);usage.clear();for(int i=0;i<10000;++i)advance(&engine,.25);liters=0;for(const auto& call:usage)liters+=call[1].toDouble();near(liters,2.5,1e-9);
    }
    void nativeSetupRendering() {
        Workspace w;auto* d=w.window.findChild<DialogSetup*>();
        for(const QString theme:{"Dark","Light"}) {
            AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.sync();w.window.HotReloadSettings();d->show();QTest::qWait(40);
            QCOMPARE(d->findChildren<QGroupBox*>(QRegularExpression("vessel.*Group")).size(),6);
            d->findChild<QWidget*>("pushButtonLoad")->setFocus();
            const QString dir=qEnvironmentVariable("M216A_EVIDENCE");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(d->grab().save(dir+"/"+theme+"-setup.png"));}
            d->reject();
        }
    }
};
int main(int argc,char** argv) {
    QTemporaryDir profile(QDir::currentPath()+"/m216a-profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*> pointers;for(auto& a:args)pointers<<a.data();int count=pointers.size();pointers<<nullptr;
    QApplication app(count,pointers.data());app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    VesselSetupFuelTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "vessel_setup_fuel_tests.moc"
