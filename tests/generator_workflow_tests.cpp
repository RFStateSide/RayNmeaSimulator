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
#include <QJsonDocument>
#include <QNetworkProxy>
#include "maincontroller.h"
#include "nmeareceiver.h"
#include "N2kMessages.h"
#include "windlassstatus.h"
#include <algorithm>

namespace {
struct Workspace {
    QMdiArea area;
    QTableView table;
    QTreeView tree;
    PgnTableController tables{&table};
    PgnTreeController trees{&tree};
    MainController controller{&tables, &trees, &area, nullptr};
    Nmea2000Handler handler;
    Nmea0183Handler sentences;
    NmeaReceiver receiver{&controller, &tables, &handler, &sentences};
    Workspace() {
        controller.setMode(Viewer); handler.setMode(Viewer);
        controller.setMyVessel(MyVessel{}); controller.setUserPrefs(UserUnitPrefs{});
        controller.setUserOptionPrefs(UserOptionPrefs{});
        for (const QString group : {"Generate","Navigation","Propulsion","Environment","Tanks","Electrical","Other","Communication"})
            controller.addSubWindowForGroup(group, group);
    }
    PgnTableModel *model() { return tables.getPgnTableModel(); }
    QString value(int id, const QString &field) {
        for (const auto &row : model()->getPgnParams(id)) if (row[0] == field) return row[1];
        return {};
    }
};
void settle() { for(int i=0;i<4;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall); }
void engine(Workspace &w,int instance,int source,double rpm=1200) {tN2kMsg m;SetN2kEngineParamRapid(m,instance,rpm,N2kDoubleNA,N2kInt8NA);m.Source=source;w.handler.nmea2000MessageReceived(m);}
}
#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QSpinBox>
#include <QMessageBox>
#include <QAbstractButton>
#include "mainwindow.h"
#include "latlong129025.h"
#include "vesselheading127250.h"

// Drive the production modal dialogs; never substitute the creation/binding path.
struct DialogAnswers {
    QTimer timer;
    bool yes = false;
    int instance = 0;
    QStringList prompts;
    DialogAnswers() {
        QObject::connect(&timer,&QTimer::timeout,[this]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(auto *box=qobject_cast<QMessageBox*>(dialog)) {
                prompts << box->windowTitle()+": "+box->text();
                box->button(yes ? QMessageBox::Yes : QMessageBox::No)->click();
            } else if(dialog) if(auto *spin=dialog->findChild<QSpinBox*>()) {
                spin->setValue(instance+1);dialog->accept();
            }
        });
        timer.start(0);
    }
};
struct MetaCalls : QObject {
    int count=0;
    bool eventFilter(QObject*,QEvent *e) override {if(e->type()==QEvent::MetaCall)++count;return false;}
};
void captured(Workspace &w) {
    for(int i=0;i<2;++i) {
        engine(w,i,23,1200+100*i);tN2kMsg m;
        SetN2kEngineDynamicParam(m,i,100000.,310.,320.,13.,4.,1000.,N2kDoubleNA,N2kDoubleNA,N2kInt8NA,N2kInt8NA,tN2kEngineDiscreteStatus1{},tN2kEngineDiscreteStatus2{});m.Source=23;w.handler.nmea2000MessageReceived(m);
        SetN2kTransmissionParameters(m,i,N2kTG_Neutral,120000,305);m.Source=23;w.handler.nmea2000MessageReceived(m);
    }
    tN2kMsg m;SetN2kLatLonRapid(m,43.1234567,-71.7654321);m.Source=23;w.handler.nmea2000MessageReceived(m);
    SetN2kPGN127250(m,1,1.2,0.01,0.02,N2khr_true);m.Source=23;w.handler.nmea2000MessageReceived(m);
    settle();
}
GeneratorVesselMovement *movement(Workspace &w) {
    w.controller.addGeneratorWidget("Vessel Movement (PGN -129025)");
    return w.area.findChild<GeneratorVesselMovement*>();
}
template<class T> T *instanceWidget(Workspace &w,int instance) {
    for(auto *widget:w.area.findChildren<T*>())if(widget->EngineInstance()==instance)return widget;
    return nullptr;
}
void sources(Workspace &w,DialogAnswers &answers) {
    for(int i=0;i<2;++i) {
        answers.instance=i;
        w.controller.addGeneratorWidget("Engine / Outboard (PGN -127489)");
        w.controller.addGeneratorWidget("Transmission / Lower Unit (PGN -127493)");
    }
}
class GeneratorWorkflowTests : public QObject {
    Q_OBJECT
private slots:
    void movementOrders_data() {
        QTest::addColumn<bool>("sourceFirst");
        QTest::newRow("generator-first")<<true;
        QTest::newRow("instruments-first-snapshot")<<false;
    }
    void movementOrders() {
        QFETCH(bool,sourceFirst);Workspace w;DialogAnswers answers;
        if(!sourceFirst) {
            captured(w);QTemporaryDir dir;const auto f=dir.filePath("rx-default.json");
            const auto before=w.model()->getPgnList();QVERIFY(w.controller.saveLayoutToJson(f));
            answers.yes=true;QVERIFY(w.controller.loadLayoutFromJson(f));answers.yes=false;
            const auto after=w.model()->getPgnList();QCOMPARE(after.size(),before.size());
            for(const auto &saved:before){const auto loaded=w.model()->getPgnDataToSend(saved.idX);QCOMPARE(loaded.paramDetails,saved.paramDetails);QCOMPARE(loaded.enabled,saved.enabled);QVERIFY(loaded.source.isEmpty());}
            QVERIFY(w.controller.layoutWidgets("Generate").isEmpty());
            const auto roundTrip=dir.filePath("round-trip.json");QVERIFY(w.controller.saveLayoutToJson(roundTrip));QFile first(f),second(roundTrip);QVERIFY(first.open(QIODevice::ReadOnly));QVERIFY(second.open(QIODevice::ReadOnly));QCOMPARE(QJsonDocument::fromJson(second.readAll()).object().value("layouts"),QJsonDocument::fromJson(first.readAll()).object().value("layouts"));
            answers.prompts.clear();
        }
        w.controller.setMode(Sender);const auto preserved=w.model()->getPgnList();auto *g=movement(w);QVERIFY(g);settle();
        for(const auto &saved:preserved)QCOMPARE(w.model()->getPgnDataToSend(saved.idX).paramDetails,saved.paramDetails);
        QCOMPARE(answers.prompts.size(),4); // two legitimate optional sources per engine
        for(const auto &prompt:answers.prompts){QVERIFY(prompt.contains("Optional"));QVERIFY(prompt.contains("navigation connections"));}
        QVERIFY(w.area.findChildren<GeneratorEngine*>().isEmpty());
        QVERIFY(w.area.findChildren<GeneratorTransLowerUnit*>().isEmpty());
        if(sourceFirst) {
            // Manual instrument creation in Generate uses the already-existing source.
            for(const auto &pgn:{"129025","127250"})w.controller.addWidgetAsComboboxTextChanged(pgn,pgn,"Navigation");
        }
        auto *position=w.area.findChild<LatLong129025*>();auto *heading=w.area.findChild<VesselHeading127250*>();QVERIFY(position);QVERIFY(heading);
        settle();const int size=w.model()->getPgnList().size();
        QSignalSpy positions(position,&LatLong129025::LatitudeChanged);
        QSignalSpy headings(heading,&VesselHeading127250::HeadingChanged);
        MetaCalls calls;position->installEventFilter(&calls);
        g->setLatitude(44.25);g->setLongitude(-70.5);g->setHeading(0.8);settle();
        QCOMPARE(position->Latitude(),44.25);QCOMPARE(position->Longitude(),-70.5);QCOMPARE(heading->Heading(),0.8);
        QCOMPARE(positions.size(),1);QCOMPARE(headings.size(),1);const int deliveries=calls.count;QVERIFY(deliveries>0);
        if(!sourceFirst)for(int i=0;i<2;++i){QCOMPARE(instanceWidget<EngineRapid127488>(w,i)->EngineSpeed(),1200.+100*i);QCOMPARE(instanceWidget<TransmissionDynamic127493>(w,i)->OilPressure(),120000.);}
        const int prompts=answers.prompts.size();
        for(int i=0;i<3;++i){w.controller.setMode(Viewer);w.controller.setMode(Sender);}
        calls.count=0;positions.clear();g->setLatitude(44.5);g->setLongitude(-70.75);g->setHeading(0.9);settle();
        QCOMPARE(position->Latitude(),44.5);QCOMPARE(positions.size(),1);QCOMPARE(calls.count,deliveries);
        calls.count=0;QVERIFY(QMetaObject::invokeMethod(g,"LatitudeChanged",Qt::DirectConnection));settle();QCOMPARE(calls.count,1);
        QCOMPARE(answers.prompts.size(),prompts);QCOMPARE(w.model()->getPgnList().size(),size);
        w.controller.setMode(Viewer);g->setLatitude(45);settle();QCOMPARE(position->Latitude(),44.5);
        w.controller.setMode(Sender);g->setLatitude(45.5);w.controller.removeWidgetFromSubForm(g,"Generate");settle();QCOMPARE(position->Latitude(),44.5);
        g=movement(w);QVERIFY(g);g->setLatitude(46.25);settle();QCOMPARE(position->Latitude(),46.25);
    }
    void engineOrders_data() {movementOrders_data();}
    void engineOrders() {
        QFETCH(bool,sourceFirst);Workspace w;DialogAnswers answers;
        if(sourceFirst){w.controller.setMode(Sender);sources(w,answers);}
        if(sourceFirst) {
            for(int i=0;i<2;++i){answers.instance=i;for(const auto &pgn:{"127488","127489","127493"})w.controller.addWidgetAsComboboxTextChanged(pgn,pgn,"Propulsion");}
        } else {captured(w);w.controller.setMode(Sender);sources(w,answers);}
        const int promptsBeforeMovement=answers.prompts.size();QVERIFY(movement(w));QCOMPARE(answers.prompts.size(),promptsBeforeMovement);settle();
        const int count=w.model()->getPgnList().size();
        for(int i=0;i<2;++i) {
            auto *engine=instanceWidget<GeneratorEngine>(w,i);auto *trans=instanceWidget<GeneratorTransLowerUnit>(w,i);
            auto *rapid=instanceWidget<EngineRapid127488>(w,i);auto *dynamic=instanceWidget<EngineDynamic127489>(w,i);auto *instrument=instanceWidget<TransmissionDynamic127493>(w,i);
            QVERIFY(engine);QVERIFY(trans);QVERIFY(rapid);QVERIFY(dynamic);QVERIFY(instrument);
            engine->setEnabled(true);trans->setEnabled(true);
            QSignalSpy speed(rapid,&EngineRapid127488::EngineSpeedChanged);MetaCalls calls;rapid->installEventFilter(&calls);
            engine->setEngineSpeed(1800+i*500);engine->setEngineOilPress(150000+i*10000);trans->setOilPressure(160000+i*10000);settle();
            QCOMPARE(rapid->EngineSpeed(),1800.+i*500);QCOMPARE(dynamic->EngineOilPress(),150000.+i*10000);QCOMPARE(instrument->OilPressure(),160000.+i*10000);QCOMPARE(speed.size(),1);
            const int deliveries=calls.count;QVERIFY(deliveries>0);
            calls.count=0;QVERIFY(QMetaObject::invokeMethod(engine,"EngineSpeedChanged",Qt::DirectConnection));settle();QCOMPARE(calls.count,1);
            w.controller.setMode(Viewer);w.controller.setMode(Sender);calls.count=0;speed.clear();engine->setEngineSpeed(1850+i*500);settle();QCOMPARE(speed.size(),1);QCOMPARE(calls.count,deliveries);
        }
        QCOMPARE(w.model()->getPgnList().size(),count);
        auto *old=instanceWidget<GeneratorEngine>(w,0);old->setEngineSpeed(3000);w.controller.removeWidgetFromSubForm(old,"Generate");settle();QCOMPARE(instanceWidget<EngineRapid127488>(w,0)->EngineSpeed(),1850.);
        answers.instance=0;w.controller.addGeneratorWidget("Engine / Outboard (PGN -127489)");instanceWidget<GeneratorEngine>(w,0)->setEngineSpeed(1950);settle();QCOMPARE(instanceWidget<EngineRapid127488>(w,0)->EngineSpeed(),1950.);QCOMPARE(instanceWidget<EngineRapid127488>(w,1)->EngineSpeed(),2350.);
    }
    void lowerUnitWithoutEngine_data() {movementOrders_data();}
    void lowerUnitWithoutEngine() {
        QFETCH(bool,sourceFirst);Workspace w;if(!sourceFirst)captured(w);w.controller.setMode(Sender);DialogAnswers answers;
        QObject::disconnect(&answers.timer,nullptr,nullptr,nullptr);
        QObject::connect(&answers.timer,&QTimer::timeout,[&]{
            auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(auto *box=qobject_cast<QMessageBox*>(d)) {
                answers.prompts<<box->text();
                answers.instance=box->windowTitle().endsWith("2")?1:0;
                box->button(box->windowTitle().contains("GeneratorTransLowerUnit")?QMessageBox::Yes:QMessageBox::No)->click();
            } else if(d)if(auto *spin=d->findChild<QSpinBox*>()){spin->setValue(answers.instance+1);d->accept();}
        });
        auto *g=movement(w);QVERIFY(g);g->setEnabled(true);QCOMPARE(answers.prompts.size(),4);
        QVERIFY(w.area.findChildren<GeneratorEngine*>().isEmpty());QCOMPARE(w.area.findChildren<GeneratorTransLowerUnit*>().size(),2);
        for(auto *t:w.area.findChildren<GeneratorTransLowerUnit*>())t->setEnabled(true);
        if(sourceFirst)for(int i=0;i<2;++i){answers.instance=i;w.controller.addWidgetAsComboboxTextChanged("127493","Transmission","Propulsion");}
        g->setTransmissionGear(N2kTG_Forward);g->setTransmissionGearStbd(N2kTG_Reverse);settle();
        QCOMPARE(instanceWidget<GeneratorTransLowerUnit>(w,0)->TransmissionGear(),N2kTG_Forward);
        QCOMPARE(instanceWidget<GeneratorTransLowerUnit>(w,1)->TransmissionGear(),N2kTG_Reverse);
        QCOMPARE(instanceWidget<TransmissionDynamic127493>(w,0)->TransmissionGear(),N2kTG_Forward);
        QCOMPARE(instanceWidget<TransmissionDynamic127493>(w,1)->TransmissionGear(),N2kTG_Reverse);
        auto *old=instanceWidget<GeneratorTransLowerUnit>(w,0);g->setTransmissionGear(N2kTG_Neutral);w.controller.removeWidgetFromSubForm(old,"Generate");settle();
        QCOMPARE(instanceWidget<TransmissionDynamic127493>(w,0)->TransmissionGear(),N2kTG_Forward);
        answers.instance=0;w.controller.addGeneratorWidget("Transmission / Lower Unit (PGN -127493)");auto *replacement=instanceWidget<GeneratorTransLowerUnit>(w,0);QVERIFY(replacement);replacement->setEnabled(true);
        g->setTransmissionGear(N2kTG_Reverse);settle();QCOMPARE(instanceWidget<TransmissionDynamic127493>(w,0)->TransmissionGear(),N2kTG_Reverse);
        QSignalSpy status(g,&GeneratorVesselMovement::ShowStatusBarMessage);auto *manual=g->findChild<QCheckBox*>("checkBoxManualMovementEnabled");QVERIFY(manual);manual->click();QVERIFY(!manual->isChecked());QVERIFY(!status.isEmpty());
    }
    void yesOptionalSourcesAndReplacement() {
        Workspace w;captured(w);QTemporaryDir dir;const auto f=dir.filePath("rx.json");QVERIFY(w.controller.saveLayoutToJson(f));w.controller.setMode(Sender);
        DialogAnswers answers;answers.yes=true;
        // Each production source-selection dialog defaults to its next instance.
        QObject::disconnect(&answers.timer,nullptr,nullptr,nullptr);
        QObject::connect(&answers.timer,&QTimer::timeout,[&]{auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(auto *box=qobject_cast<QMessageBox*>(d)){answers.prompts<<box->text();box->button(QMessageBox::Yes)->click();}else if(d && d->findChild<QSpinBox*>())d->accept();});
        auto *g=movement(w);QVERIFY(g);QCOMPARE(answers.prompts.size(),4);QCOMPARE(w.area.findChildren<GeneratorEngine*>().size(),2);QCOMPARE(w.area.findChildren<GeneratorTransLowerUnit*>().size(),2);
        for(int i=0;i<2;++i){QVERIFY(instanceWidget<GeneratorEngine>(w,i));QVERIFY(instanceWidget<GeneratorTransLowerUnit>(w,i));}
        const auto instrumentCount=w.model()->getPgns("127488").size();QCOMPARE(instrumentCount,2);
        g->setEnabled(true);g->setTransmissionGear(N2kTG_Forward);g->setTransmissionGearStbd(N2kTG_Reverse);settle();
        QCOMPARE(instanceWidget<TransmissionDynamic127493>(w,0)->TransmissionGear(),N2kTG_Forward);QCOMPARE(instanceWidget<TransmissionDynamic127493>(w,1)->TransmissionGear(),N2kTG_Reverse);
        QPointer<GeneratorVesselMovement> stale=g;g->setLatitude(46);QVERIFY(w.controller.loadLayoutFromJson(f));settle();QVERIFY(stale.isNull());QVERIFY(w.controller.layoutWidgets("Generate").isEmpty());QCOMPARE(w.area.findChild<LatLong129025*>()->Latitude(),43.1234567);
        answers.yes=false; // new creation below uses a separate No dialog responder
    }
    void sendingNeverAuthorized() {
        MainWindow window;auto *controller=window.findChild<MainController*>();auto *sender=window.findChild<NmeaSenderWorker*>();QVERIFY(controller);QVERIFY(sender);
        auto *start=window.findChild<QAction*>("actionStart_Sending");QVERIFY(start);DialogAnswers answers;
        controller->removeAllWidgets();controller->setMode(Sender);controller->addGeneratorWidget("Vessel Movement (PGN -129025)");
        QVERIFY(!start->isChecked());QVERIFY(!sender->isSending());
        QTemporaryDir dir;const auto f=dir.filePath("working.json");QVERIFY(controller->saveLayoutToJson(f));answers.yes=true;QVERIFY(controller->loadLayoutFromJson(f));QVERIFY(!start->isChecked());QVERIFY(!sender->isSending());
        controller->setMode(Viewer);controller->setMode(Sender);QVERIFY(!start->isChecked());QVERIFY(!sender->isSending());
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    GeneratorWorkflowTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "generator_workflow_tests.moc"
