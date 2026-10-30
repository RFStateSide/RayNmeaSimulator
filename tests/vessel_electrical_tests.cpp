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
#include "N2kMessages.h"
#include "appsettings.h"
#include "batterystatus127508.h"
#include "chargerstatus127507.h"
#include "dcstatus127506.h"
#include "dialogsetup.h"
#include "mainwindow.h"
#include "pgntreecontroller.h"
#include "preferences.h"
#include "signalkpublisher.h"
#include "vesselelectricalsystem.h"
#include "generatortranslowerunit.h"
#include <QTableView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QScrollArea>
#include <QSpinBox>
#include <QTemporaryDir>
namespace {
void near(double a, double b, double epsilon = 1e-6) {
    QVERIFY2(qAbs(a - b) <= epsilon, qPrintable(QString("%1 != %2").arg(a, 0, 'g', 17).arg(b, 0, 'g', 17)));
}
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
    VesselElectricalSystem *system = window.findChild<VesselElectricalSystem *>();
    int nextId = 100;
    Workspace() {
        window.resize(1400, 900);
        window.show();
        QTest::qWait(20);
    }
    double value(QWidget *w, QString name) {
        for (auto row : model->findPgn(w->property("Id").toInt())->paramDetails)
            if (row[0] == name)
                return row[1].toDouble();
        return qQNaN();
    }
    bool edit(QWidget *w, QString name, double v) {
        model->setCurrentPgn(w->property("Id").toInt());
        for (int i = 0; i < model->rowCount(); ++i)
            if (model->data(model->index(i, 0)).toString() == name)
                return model->setData(model->index(i, 1), QString::number(v, 'g', 17));
        return false;
    }
    QWidget *add(QString pgn, int instance = 0) {
        QTimer choose;
        choose.setInterval(1);
        QObject::connect(&choose, &QTimer::timeout, [instance] {
            for (auto *w : QApplication::topLevelWidgets())
                if (auto *d = qobject_cast<QDialog *>(w);
                    d && d->windowTitle() == "Select Battery Instance") {
                    if (auto *spin = d->findChild<QSpinBox *>())
                        spin->setValue(instance);
                    d->accept();
                }
        });
        choose.start();
        controller->addWidgetAsComboboxTextChanged(pgn, pgn, "Electrical");
        choose.stop();
        return controller->layoutWidgets("Electrical").last();
    }
    GeneratorEngine *engine(int instance) {
        QFile file(":/defaults/defaultLayout.json");
        file.open(QIODevice::ReadOnly);
        for (auto group : QJsonDocument::fromJson(file.readAll()).object()["layouts"].toArray())
            for (auto entry : group.toObject()["widgets"].toArray()) {
                auto o = entry.toObject();
                if (o["type"] != "GeneratorEngine")
                    continue;
                WidgetConfig c{};
                c.idX = nextId;
                nextId += 1000;
                c.widgetType = "GeneratorEngine";
                c.pgn = "-127489";
                c.label = "Engine";
                c.rowSpan = 9;
                c.columnSpan = 3;
                c.interval = 1000;
                c.properties["engineProfile"] = "MerCruiser 7.4 MPI";
                for (auto item : o["parameters"].toArray()) {
                    QList<QString> row;
                    for (auto v : item.toArray())
                        row << v.toString();
                    if (row[0] == "EngineInstance")
                        row[1] = QString::number(instance);
                    c.parameters << row;
                }
                controller->restoreLayoutFromWidgetConfig(c, "Generate");
                for (auto *w : controller->layoutWidgets("Generate"))
                    if (w->property("Id").toInt() == c.idX)
                        return qobject_cast<GeneratorEngine *>(w);
            }
        return nullptr;
    }
    void start(GeneratorEngine *engine, double volts = 14.4) {
        engine->setEngineRunning(true);
        for (auto *timer : engine->findChildren<QTimer *>())
            timer->stop();
        engine->setAltenatorVoltage(volts);
        system->findChild<QTimer *>("electricalPollTimer")->stop();
    }
    void advance(double seconds) {
        system->advance(seconds);
        system->findChild<QTimer *>("electricalPollTimer")->stop();
    }
    void simulate(QWidget *w, bool on = true) {
        auto *check = w->findChild<QCheckBox *>("electricalSimulationToggle");
        QVERIFY(check);
        check->setChecked(on);
        system->findChild<QTimer *>("electricalPollTimer")->stop();
    }
};
} // namespace
class VesselElectricalTests : public QObject {
    Q_OBJECT
  private slots:
    void init() {
        AppSettings s(APP_COMPANY, APP_NAME);
        s.clear();
        s.setValue("Nmea2000/InterfaceType", "None");
        s.setValue("Preference/StyleSheetName", "Dark");
        QLocale::setDefault(QLocale::c());
    }
    void setupRoundTrips() {
        Workspace w;
        auto *setup = w.window.findChild<DialogSetup *>();
        setup->show();
        QTest::qWait(20);
        auto *nominal = setup->findChild<QComboBox *>("electricalNominal");
        QVERIFY(nominal);
        QCOMPARE(nominal->currentText(), "12");
        auto *batteryChoice = setup->findChild<QComboBox *>("electricalBattery");
        auto *engineChoice = setup->findChild<QComboBox *>("electricalEngine");
        QCOMPARE(batteryChoice->itemText(0), QString("1"));
        QCOMPARE(batteryChoice->itemText(1), QString("2"));
        QCOMPARE(batteryChoice->itemData(0).toInt(), 0);
        QCOMPARE(batteryChoice->itemData(1).toInt(), 1);
        QCOMPARE(engineChoice->currentText(), QString("1"));
        batteryChoice->setCurrentIndex(1);
        QCOMPARE(engineChoice->currentText(), QString("2"));
        batteryChoice->setCurrentIndex(0);
        near(setup->findChild<QDoubleSpinBox *>("electricalCapacity")->value(), 100);
        near(setup->findChild<QDoubleSpinBox *>("electricalTemperature")->value(), 68);
        nominal->setCurrentIndex(1);
        near(setup->findChild<QDoubleSpinBox *>("electricalVoltage")->value(), 25.2);
        setup->findChild<QDoubleSpinBox *>("electricalCapacity")->setValue(120.123456);
        setup->findChild<QSpinBox *>("electricalCount")->setValue(1);
        setup->findChild<QComboBox *>("electricalEngine")->setCurrentIndex(2);
        setup->findChild<QDoubleSpinBox *>("electricalTemperature")->setValue(71.25);
        QVERIFY(QMetaObject::invokeMethod(setup, "on_pushButtonSave_clicked"));
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.beginGroup("Vessel");
        const auto original = readVesselSettings(settings);
        near(original.Electrical.capacityC, 120.123456 * 3600);
        QCOMPARE(original.Electrical.engineForBattery[0], 1);
        QCOMPARE(original.Electrical.batteryCount, 1);
        QCOMPARE(original.Electrical.nominalVoltage, 24);
        for (int i = 0; i < 12; ++i) {
            setup->findChild<QComboBox *>("comboBoxUnitsTemp")->setCurrentIndex(i % 3);
            QVERIFY(QMetaObject::invokeMethod(setup, "on_pushButtonSave_clicked"));
            QVERIFY(QMetaObject::invokeMethod(setup, "on_pushButtonLoad_clicked"));
            settings.sync();
            QCOMPARE(readVesselSettings(settings).Electrical, original.Electrical);
        }
        setup->reject();
    }
    void selectionWithActiveGenerators() {
        Workspace w;
        const QString layout = qEnvironmentVariable("M216C_TEST_LAYOUT", ":/defaults/defaultLayout.json");
        answer();
        QVERIFY(w.controller->loadLayoutFromJson(layout));
        QTest::qWait(50);
        auto *movement = w.window.findChild<GeneratorVesselMovement *>();
        QVERIFY(movement);
        for (auto *engine : w.window.findChildren<GeneratorEngine *>())
            engine->setEngineProfile("MerCruiser 7.4 MPI");
        auto *b = w.add("127508", 0);
        w.add("127508", 1);
        w.add("127506");
        w.add("127506");
        w.add("127507");
        auto *tree = w.window.findChild<PgnTreeController *>();
        auto *view = tree->getTreeView();
        view->setCurrentIndex(tree->getTreeModel()->indexFor(movement));
        QTest::qWait(30);
        auto *manual = movement->findChild<QCheckBox *>("checkBoxManualMovementEnabled");
        QVERIFY(manual);
        manual->click();
        movement->setStartEngineEnabled(true);
        movement->setTransmissionGear(N2kTG_Forward);
        movement->setTransmissionGearStbd(N2kTG_Forward);
        movement->setThrottlePosition(40);
        movement->setThrottlePositionStbd(60);
        QVERIFY(w.controller->selectLayoutWidget(movement));
        const int selected = movement->Id();
        QSignalSpy resets(w.model, &QAbstractItemModel::modelReset);
        for (int i = 0; i < 15; ++i) {
            QTest::qWait(100);
            QVERIFY2(w.model->isPgnCurrentPgn(selected),
                     "A periodic generator/model update changed the selected Property Editor widget");
            QCOMPARE(tree->getTreeModel()->node(view->currentIndex())->widget.data(), movement);
        }
        QCOMPARE(resets.count(), 0);
        auto *trans = w.window.findChild<GeneratorTransLowerUnit *>();
        QVERIFY(trans);
        // A delayed focus notification from a widget which no longer owns focus.
        QMetaObject::invokeMethod(trans, "WidgetHasFocus", Qt::QueuedConnection, Q_ARG(int, trans->Id()));
        QVERIFY(w.controller->selectLayoutWidget(movement));
        QTest::qWait(30);
        QVERIFY(w.model->isPgnCurrentPgn(selected));
        view->setCurrentIndex(tree->getTreeModel()->indexFor(b));
        QTest::qWait(30);
        QVERIFY(w.model->isPgnCurrentPgn(b->property("Id").toInt()));
        auto *editor = b->findChild<QLineEdit *>("lineEditBatteryVoltage");
        QVERIFY(editor);
        editor->setFocus();
        QTest::qWait(30);
        QVERIFY(w.model->isPgnCurrentPgn(b->property("Id").toInt()));
        w.controller->selectLayoutWidget(b, "BatteryVoltage");
        auto *table = w.window.findChild<QTableView *>("tableView");
        table->setFocus();
        table->edit(table->currentIndex());
        QTest::qWait(20);
        auto *pending = table->viewport()->findChild<QLineEdit *>();
        QVERIFY(pending);
        pending->setFocus();
        QTest::keyClick(pending, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(pending, "12.34");
        QTest::qWait(500);
        QCOMPARE(pending->text(), QString("12.34"));
        QVERIFY(pending->hasFocus());
        const double liveVoltage = w.value(b, "BatteryVoltage");
        QVERIFY(qAbs(liveVoltage - 12.34) > .01);
        QTest::keyClick(pending, Qt::Key_Escape);
        near(w.value(b, "BatteryVoltage"), liveVoltage);
        table->edit(table->currentIndex());
        QTest::qWait(20);
        pending = table->viewport()->findChild<QLineEdit *>();
        QVERIFY(pending);
        near(pending->text().toDouble(), w.value(b, "BatteryVoltage"));
        pending->setFocus();
        QTest::keyClick(pending, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(pending, "12.34");
        QTest::keyClick(pending, Qt::Key_Return);
        QTest::qWait(300);
        near(w.value(b, "BatteryVoltage"), 12.34);
        QVERIFY(!b->property("ElectricalSimulation").toBool());
    }
    void defaultsAndNoOverwrite() {
        Workspace w;
        MyVessel v;
        v.Electrical.capacityC = 720000;
        v.Electrical.initialSoc = 65;
        v.Electrical.initialVoltage = 12.3;
        w.controller->setMyVessel(v);
        auto *battery = w.add("127508");
        auto *dc = w.add("127506");
        auto *charger = w.add("127507");
        near(w.value(battery, "BatteryVoltage"), 12.3);
        near(w.value(battery, "BatteryTemperature"), 293.15);
        near(w.value(dc, "StateOfCharge"), 65);
        near(w.value(dc, "Capacity"), 720000);
        near(w.value(charger, "BatteryInstance"), 0);
        QVERIFY(battery->property("ElectricalSimulation").toBool());
        QVERIFY(w.edit(battery, "BatteryVoltage", 12.1));
        v.Electrical.initialVoltage = 13;
        v.Electrical.capacityC = 360000;
        w.controller->setMyVessel(v);
        near(w.value(battery, "BatteryVoltage"), 12.1);
        near(w.value(dc, "Capacity"), 720000);
        v.Electrical.batteryCount = 1;
        v.Electrical.simulateNew = false;
        w.controller->setMyVessel(v);
        auto *unconfigured = w.add("127508", 7);
        QVERIFY(!unconfigured->property("ElectricalSimulation").toBool());
        near(w.value(unconfigured, "BatteryTemperature"), 293.15);
    }
    void starterChargingStop() {
        Workspace w;
        auto *engine = w.engine(0);
        QVERIFY(engine && engine->profileReady());
        auto *battery = w.add("127508");
        auto *dc = w.add("127506");
        auto *charger = w.add("127507");
        w.start(engine);
        QVERIFY(w.value(battery, "BatteryVoltage") < 11);
        near(w.value(battery, "BatteryCurrent"), -180);
        w.advance(.5);
        near(w.value(battery, "BatteryCurrent"), -180);
        w.advance(.6);
        QVERIFY(w.value(battery, "BatteryCurrent") > 0);
        near(w.value(charger, "Enabled"), 1);
        w.advance(3600);
        QVERIFY(w.value(dc, "StateOfCharge") > 80);
        near(w.value(dc, "Capacity"), 360000);
        QVERIFY(w.value(battery, "BatteryVoltage") > 13);
        engine->setEngineRunning(false);
        w.advance(5);
        near(w.value(battery, "BatteryCurrent"), 0);
        QVERIFY(w.value(battery, "BatteryVoltage") < 13);
        near(w.value(charger, "ChargeState"), 0);
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void independentAndDuplicates() {
        Workspace w;
        auto *e0 = w.engine(0);
        auto *e1 = w.engine(1);
        auto *b0 = w.add("127508", 0);
        auto *b1 = w.add("127508", 1);
        auto *dc0 = w.add("127506");
        auto *dc1 = w.add("127506");
        auto *duplicate = w.add("127508", 0);
        w.start(e0, 13.5);
        w.start(e1, 14.5);
        w.advance(2);
        QVERIFY(w.value(b1, "BatteryCurrent") > w.value(b0, "BatteryCurrent"));
        near(w.value(duplicate, "BatteryCurrent"), w.value(b0, "BatteryCurrent"));
        const double s0 = w.value(dc0, "StateOfCharge"), s1 = w.value(dc1, "StateOfCharge");
        w.advance(900);
        QVERIFY(w.value(dc1, "StateOfCharge") - s1 >= w.value(dc0, "StateOfCharge") - s0);
        e0->setEngineRunning(false);
        w.advance(5);
        near(w.value(b0, "BatteryCurrent"), 0);
        QVERIFY(w.value(b1, "BatteryCurrent") > 0);
        near(w.value(duplicate, "BatteryVoltage"), w.value(b0, "BatteryVoltage"));
    }
    void singleBattery24VAndAssociations() {
        Workspace w;
        MyVessel vessel;
        vessel.Electrical.nominalVoltage = 24;
        vessel.Electrical.initialVoltage = 25.2;
        vessel.Electrical.batteryCount = 1;
        vessel.Electrical.engineForBattery[0] = 1;
        w.controller->setMyVessel(vessel);
        auto *e0 = w.engine(0);
        auto *e1 = w.engine(1);
        auto *b = w.add("127508");
        w.add("127506");
        w.start(e0);
        w.advance(3);
        near(w.value(b, "BatteryCurrent"), 0);
        near(w.value(b, "BatteryVoltage"), 25.2);
        w.start(e1, 28.8);
        near(w.value(b, "BatteryCurrent"), -90);
        QVERIFY(w.value(b, "BatteryVoltage") < 21);
        w.advance(5);
        QVERIFY(w.value(b, "BatteryVoltage") > 26);
        QVERIFY(w.value(b, "BatteryCurrent") > 0);
        e1->setEngineRunning(false);
        w.advance(5);
        near(w.value(b, "BatteryCurrent"), 0);
        near(w.value(b, "BatteryVoltage"), 25.2, .1);
    }
    void generatedAlternator() {
        Workspace w;
        MyVessel v;
        v.Electrical.nominalVoltage = 24;
        v.Electrical.initialVoltage = 25.2;
        w.controller->setMyVessel(v);
        auto *e = w.engine(0);
        e->setEngineProfile("Volvo Penta D4 300 Diesel");
        QVERIFY(e->profileReady());
        auto *b = w.add("127508");
        w.add("127506");
        w.start(e, 0);
        for (int i = 0; i < 4; ++i)
            QVERIFY(QMetaObject::invokeMethod(e, "PollTimerElapsed"));
        QVERIFY(e->AltenatorVoltage() > 25.2);
        w.advance(20);
        near(w.value(b, "BatteryVoltage"), e->AltenatorVoltage(), .01);
        QVERIFY(w.value(b, "BatteryCurrent") > 0);
        e->setEnabled(false);
        w.advance(5);
        near(w.value(b, "BatteryCurrent"), 0);
        e->setEnabled(true);
        w.advance(2);
        QVERIFY(w.value(b, "BatteryCurrent") > 0);
        e->setEngineRunning(false);
        w.advance(5);
        w.start(e, 28);
        near(w.value(b, "BatteryCurrent"), -90);
    }
    void incompatibleAlternator() {
        Workspace w;
        MyVessel v;
        v.Electrical.nominalVoltage = 24;
        v.Electrical.initialVoltage = 25.2;
        w.controller->setMyVessel(v);
        auto *e = w.engine(0);
        auto *b = w.add("127508");
        w.add("127506");
        w.start(e, 14.4);
        w.advance(10);
        near(w.value(b, "BatteryCurrent"), 0);
        QVERIFY(w.value(b, "BatteryVoltage") < 26);
    }
    void duplicateEngineAndRebinding() {
        Workspace w;
        auto *e0 = w.engine(0);
        auto *replacement = w.engine(0);
        auto *b = w.add("127508");
        w.add("127506");
        w.start(e0);
        w.advance(2);
        near(w.value(b, "BatteryCurrent"), 0);
        w.start(replacement);
        w.advance(2);
        QVERIFY(w.value(b, "BatteryCurrent") > 0);
        replacement->setEngineInstance(1);
        QTest::qWait(10);
        w.advance(5);
        QVERIFY(w.value(b, "BatteryCurrent") > 0);
        e0->setEngineRunning(false);
        w.advance(5);
        near(w.value(b, "BatteryCurrent"), 0);
        w.controller->removeWidgetFromSubForm(replacement, "Generate");
        w.advance(10);
        QVERIFY(std::isfinite(w.value(b, "BatteryVoltage")));
    }
    void manualPrecedenceAndModes() {
        Workspace w;
        auto *e = w.engine(0);
        auto *b = w.add("127508");
        auto *dc = w.add("127506");
        w.start(e);
        w.advance(2);
        QVERIFY(w.edit(b, "BatteryVoltage", 11.9));
        QVERIFY(!b->property("ElectricalSimulation").toBool());
        QVERIFY(!dc->property("ElectricalSimulation").toBool());
        w.advance(1000);
        near(w.value(b, "BatteryVoltage"), 11.9);
        w.simulate(b);
        w.simulate(dc);
        w.advance(2);
        QVERIFY(w.value(b, "BatteryVoltage") > 11.9);
        QVERIFY(TestDialogs::enterViewerKeepingLayout(w.window));
        QVERIFY(!e->EngineRunning());
        const double voltage = w.value(b, "BatteryVoltage");
        w.advance(500);
        near(w.value(b, "BatteryVoltage"), voltage);
        w.window.findChild<QAction *>("actionSimulator_Tx")->setChecked(true);
        w.advance(500);
        near(w.value(b, "BatteryVoltage"), voltage);
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void layoutOwnership() {
        Workspace w;
        auto *b = w.add("127508");
        QVERIFY(w.edit(b, "BatteryVoltage", 11.8));
        QTemporaryDir dir(QDir::currentPath() + "/m216c-layout-XXXXXX");
        auto file = dir.filePath("layout.json");
        QVERIFY(w.controller->saveLayoutToJson(file));
        answer();
        QVERIFY(w.controller->loadLayoutFromJson(file));
        b = w.window.findChild<BatteryStatus127508 *>();
        near(w.value(b, "BatteryVoltage"), 11.8);
        QVERIFY(!b->property("ElectricalSimulation").toBool());
        w.simulate(b);
        QVERIFY(w.controller->saveLayoutToJson(file));
        answer();
        QVERIFY(w.controller->loadLayoutFromJson(file));
        b = w.window.findChild<BatteryStatus127508 *>();
        near(w.value(b, "BatteryVoltage"), 11.8);
        QVERIFY(b->property("ElectricalSimulation").toBool());
        w.advance(100);
        near(w.value(b, "BatteryVoltage"), 11.8);
        QFile f(file);
        f.open(QIODevice::ReadOnly);
        auto root = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        auto groups = root["layouts"].toArray();
        for (int i = 0; i < groups.size(); ++i) {
            auto group = groups[i].toObject();
            auto widgets = group["widgets"].toArray();
            for (int j = 0; j < widgets.size(); ++j) {
                auto item = widgets[j].toObject();
                item.remove("electricalSimulation");
                widgets[j] = item;
            }
            group["widgets"] = widgets;
            groups[i] = group;
        }
        root["layouts"] = groups;
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(QJsonDocument(root).toJson());
        f.close();
        answer();
        QVERIFY(w.controller->loadLayoutFromJson(file));
        QVERIFY(!w.window.findChild<BatteryStatus127508 *>()->property("ElectricalSimulation").toBool());
    }
    void boundsAndTiming() {
        Workspace w;
        auto *e = w.engine(0);
        auto *b = w.add("127508");
        auto *dc = w.add("127506");
        QVERIFY(w.edit(dc, "Capacity", 3600));
        QVERIFY(w.edit(dc, "StateOfCharge", 50));
        w.simulate(b);
        w.simulate(dc);
        w.start(e, 14.6);
        w.advance(1);
        for (int i = 0; i < 10000; ++i)
            w.advance(.01);
        QVERIFY(w.value(dc, "StateOfCharge") >= 0 && w.value(dc, "StateOfCharge") <= 100);
        near(w.value(dc, "Capacity"), 3600);
        const auto soc = w.value(dc, "StateOfCharge");
        w.advance(qQNaN());
        w.advance(-1);
        near(w.value(dc, "StateOfCharge"), soc);
        QVERIFY(w.edit(dc, "Capacity", N2kDoubleNA));
        QVERIFY(w.edit(dc, "StateOfCharge", 255));
        w.simulate(b);
        w.simulate(dc);
        w.advance(500);
        near(w.value(dc, "StateOfCharge"), 255);
        QVERIFY(std::isfinite(w.value(b, "BatteryVoltage")));
        e->setEngineRunning(false);
        w.advance(5);
        near(w.value(b, "BatteryCurrent"), 0);
    }
    void elapsedConservation() {
        Workspace w;
        auto *e = w.engine(0);
        auto *b = w.add("127508");
        auto *dc = w.add("127506");
        QVERIFY(w.edit(dc, "StateOfCharge", 50));
        w.simulate(b);
        w.simulate(dc);
        w.start(e, 14.6);
        w.advance(1);
        const double current = w.value(b, "BatteryCurrent");
        for (int i = 0; i < 10000; ++i)
            w.advance(.25);
        near(w.value(dc, "StateOfCharge"), qRound(50. - 180. / 360000 * 100 + current * 2500 / 360000 * 100));
        near(w.value(dc, "Capacity"), 360000);
    }
    void modelTreeAndOutput() {
        Workspace w;
        auto *e = w.engine(0);
        auto *b = qobject_cast<BatteryStatus127508 *>(w.add("127508"));
        auto *dc = w.add("127506");
        w.start(e);
        w.advance(3);
        QTest::qWait(30);
        auto *tree = w.window.findChild<PgnTreeController *>()->getTreeModel();
        QTRY_VERIFY(tree->indexFor(b, "BatteryVoltage").isValid());
        near(b->BatteryVoltage(), w.value(b, "BatteryVoltage"));
        near(b->BatteryCurrent(), w.value(b, "BatteryCurrent"));
        QVERIFY(tree->data(tree->indexFor(b, "BatteryVoltage").siblingAtColumn(1)).toString().contains("V"));
        tN2kMsg msg;
        SetN2kPGN127508(msg, b->BatteryInstance(), b->BatteryVoltage(), b->BatteryCurrent(),
                        b->BatteryTemperature(), 0);
        double voltage, current, temp;
        unsigned char instance, sid;
        QVERIFY(ParseN2kPGN127508(msg, instance, voltage, current, temp, sid));
        near(voltage, b->BatteryVoltage(), .01);
        near(current, b->BatteryCurrent(), .1);
        bool found = false;
        for (auto value : SignalKPublisher::values(msg)) {
            auto object = value.toObject();
            if (object["path"] == "electrical.batteries.0.voltage") {
                near(object["value"].toDouble(), voltage, .01);
                found = true;
            }
        }
        QVERIFY(found);
        near(w.value(dc, "Capacity"), 360000);
    }
    void rendering() {
        Workspace w;
        auto *setup = w.window.findChild<DialogSetup *>();
        w.add("127508");
        w.add("127506");
        w.add("127507");
        for (QString theme : {"Dark", "Light"}) {
            AppSettings s(APP_COMPANY, APP_NAME);
            s.setValue("Preference/StyleSheetName", theme);
            s.sync();
            w.window.HotReloadSettings();
            setup->show();
            setup->resize(850, 700);
            QTest::qWait(30);
            auto *group = setup->findChild<QWidget *>("vesselElectricalGroup");
            QVERIFY(group);
            setup->findChild<QScrollArea *>("vesselScroll")->ensureWidgetVisible(group);
            const auto dir = qEnvironmentVariable("M216C_EVIDENCE");
            if (!dir.isEmpty()) {
                QDir().mkpath(dir);
                QVERIFY(setup->grab().save(dir + "/" + theme + "-setup.png"));
                QVERIFY(w.window.findChild<BatteryStatus127508 *>()->grab().save(dir + "/" + theme +
                                                                                 "-battery.png"));
                QVERIFY(w.window.findChild<DcStatus127506 *>()->grab().save(dir + "/" + theme + "-dc.png"));
                QVERIFY(w.window.findChild<ChargerStatus127507 *>()->grab().save(dir + "/" + theme +
                                                                                 "-charger.png"));
            }
            setup->reject();
        }
    }
};
int main(int argc, char **argv) {
    QTemporaryDir profile(QDir::currentPath() + "/m216c-profile-XXXXXX");
    if (!profile.isValid())
        return 2;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QList<QByteArray> args;
    for (int i = 0; i < argc; ++i)
        args << argv[i];
    args << "--no-nmea";
    QList<char *> ptr;
    for (auto &a : args)
        ptr << a.data();
    int count = ptr.size();
    ptr << nullptr;
    QApplication app(count, ptr.data());
    app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    VesselElectricalTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "vessel_electrical_tests.moc"
