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
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFontDatabase>
#include <QNetworkProxy>
#include <QTabWidget>
#include <QTemporaryDir>
#include "maincontroller.h"
#include "mainwindow.h"
#include "uihelper.h"
#include "N2kMessages.h"

namespace {
const QStringList statusNames { "CheckTransmission", "OverTemperature", "LowOilPressure", "LowOilLevel", "SailDrive" };
const QStringList statusLabels { "Check Transmission", "Over Temperature", "Low Oil Pressure", "Low Oil Level", "Sail Drive" };
void settle()
{
    for (int i = 0; i < 8; ++i)
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
}
struct Workspace {
    QMdiArea area;
    QTableView table;
    QTreeView tree;
    PgnTableController tables { &table };
    PgnTreeController trees { &tree };
    MainController controller { &tables, &trees, &area, nullptr };
    Workspace()
    {
        MyVessel vessel { };
        vessel.NumEngines = 2;
        vessel.EngineProfile = "MerCruiser 7.4 MPI";
        controller.setMyVessel(vessel);
        controller.setUserPrefs(UserUnitPrefs { });
        controller.setUserOptionPrefs(UserOptionPrefs { });
        for (const QString group :
            { "Generate", "Navigation", "Propulsion", "Environment", "Tanks", "Electrical", "Other", "Communication" })
            controller.addSubWindowForGroup(group, group);
    }
};
unsigned char modelStatus(PgnTableModel* model, int id)
{
    for (const auto& row : model->getPgnParams(id))
        if (row[0] == "DiscreteStatus1")
            return row[1].toUInt();
    return 0xff;
}
}
class AcceptanceFollowupTests : public QObject {
    Q_OBJECT
private slots:
    void transmissionFlags_data()
    {
        QTest::addColumn<int>("bit");
        for (int bit = 0; bit < 5; ++bit)
            QTest::newRow(qPrintable(statusNames[bit])) << bit;
    }
    void transmissionFlags()
    {
        QFETCH(int, bit);
        tN2kMsg msg;
        SetN2kTransmissionParameters(msg, 0, N2kTG_Neutral, 1000, 300, bit == 0, bit == 1, bit == 2, bit == 3, bit == 4);
        unsigned char instance, status;
        tN2kTransmissionGear gear;
        double pressure, temperature;
        QVERIFY(ParseN2kPGN127493(msg, instance, gear, pressure, temperature, status));
        QCOMPARE(status, static_cast<unsigned char>(1u << bit));
        GeneratorTransLowerUnit generator(1, "Test");
        TransmissionDynamic127493 instrument(2, "Test", nullptr);
        const QList<PgnWidgetBase*> widgets { &generator, &instrument };
        for (auto* widget : widgets) {
            widget->resize(380, 420);
            widget->show();
            auto* tabs = widget->findChild<QTabWidget*>();
            QVERIFY(tabs);
            auto* page = widget->findChild<QWidget*>("tabStatus");
            QVERIFY(page);
            tabs->setCurrentWidget(page);
            QCoreApplication::processEvents();
            auto* check = page->findChild<QCheckBox*>("checkBox" + statusNames[bit]);
            QVERIFY(check);
            QCOMPARE(check->text(), statusLabels[bit]);
            QVERIFY(check->isVisibleTo(widget));
            QVERIFY(check->isEnabled());
            QVERIFY(!check->isChecked());
            QSignalSpy changed(widget, SIGNAL(DiscreteStatus1Changed()));
            QVERIFY(widget->setProperty("DiscreteStatus1", 0xe0));
            changed.clear();
            check->click();
            QCOMPARE(widget->property("DiscreteStatus1").toUInt(), 0xe0u | status);
            QCOMPARE(changed.size(), 1);
            check->click();
            QCOMPARE(widget->property("DiscreteStatus1").toUInt(), 0xe0u);
            QCOMPARE(changed.size(), 2);
            QVERIFY(widget->setProperty("DiscreteStatus1", status));
            for (int flag = 0; flag < 5; ++flag)
                QCOMPARE(page->findChild<QCheckBox*>("checkBox" + statusNames[flag])->isChecked(), flag == bit);
            QVERIFY(!widget->findChild<QCheckBox*>("checkBoxWarningLevel2"));
        }
        QCOMPARE(generator.findChild<QWidget*>("tabStatus")->findChildren<QCheckBox*>().size(), 5);
        if (bit == 4 && !qEnvironmentVariable("RAYNMEA_TEST_SNAPSHOT").isEmpty())
            QVERIFY(generator.grab().save(qEnvironmentVariable("RAYNMEA_TEST_SNAPSHOT")));
    }
    void transmissionModelAndLayout()
    {
        Workspace w;
        QVERIFY(w.controller.loadLayoutFromJson(":/defaults/defaultLayout.json"));
        settle();
        QMap<int, GeneratorTransLowerUnit*> generators;
        QMap<int, TransmissionDynamic127493*> instruments;
        for (auto* g : w.area.findChildren<GeneratorTransLowerUnit*>())
            generators[g->EngineInstance()] = g;
        for (auto* i : w.area.findChildren<TransmissionDynamic127493*>())
            instruments[i->EngineInstance()] = i;
        QCOMPARE(generators.size(), 2);
        QCOMPARE(instruments.size(), 2);
        generators[0]->setDiscreteStatus1(0xa0);
        generators[0]->findChild<QCheckBox*>("checkBoxCheckTransmission")->click();
        generators[1]->setDiscreteStatus1(0x42);
        settle();
        QCOMPARE(modelStatus(w.tables.getPgnTableModel(), generators[0]->Id()), static_cast<unsigned char>(0xa1));
        QCOMPARE(instruments[0]->DiscreteStatus1(), static_cast<unsigned char>(0xa1));
        QCOMPARE(instruments[1]->DiscreteStatus1(), static_cast<unsigned char>(0x42));
        auto* model = w.tables.getPgnTableModel();
        model->setCurrentPgn(generators[0]->Id());
        int statusRow = -1;
        const auto parameters = model->getPgnParams(generators[0]->Id());
        for (int row = 0; row < parameters.size(); ++row)
            if (parameters[row][0] == "DiscreteStatus1")
                statusRow = row;
        QVERIFY(statusRow >= 0);
        QVERIFY(model->setData(model->index(statusRow, 1), 0xe4));
        settle();
        QVERIFY(generators[0]->findChild<QCheckBox*>("checkBoxLowOilPressure")->isChecked());
        QVERIFY(!generators[0]->findChild<QCheckBox*>("checkBoxOverTemperature")->isChecked());
        QCOMPARE(instruments[0]->DiscreteStatus1(), static_cast<unsigned char>(0xe4));
        QTemporaryDir dir;
        auto path = dir.filePath("status.json");
        QVERIFY(w.controller.saveLayoutToJson(path));
        Workspace restored;
        QVERIFY(restored.controller.loadLayoutFromJson(path));
        settle();
        for (auto* g : restored.area.findChildren<GeneratorTransLowerUnit*>()) {
            const unsigned char expected = g->EngineInstance() == 0 ? 0xe4 : 0x42;
            QCOMPARE(g->DiscreteStatus1(), expected);
            QVERIFY(!g->EngineRunning());
            QCOMPARE(modelStatus(restored.tables.getPgnTableModel(), g->Id()), expected);
            for (int bit = 0; bit < 5; ++bit)
                QCOMPARE(g->findChild<QCheckBox*>("checkBox" + statusNames[bit])->isChecked(), bool(expected & (1u << bit)));
        }
        for (auto* i : restored.area.findChildren<TransmissionDynamic127493*>())
            QCOMPARE(i->DiscreteStatus1(), static_cast<unsigned char>(i->EngineInstance() == 0 ? 0xe4 : 0x42));
    }
    void clearGroup_data()
    {
        QTest::addColumn<QString>("active");
        QTest::addColumn<QString>("selection");
        QTest::addColumn<bool>("accept");
        QTest::newRow("navigation-cancel") << QString("Navigation") << QString("Navigation") << false;
        QTest::newRow("generate-default") << QString("Generate") << QString("Generate") << true;
        QTest::newRow("propulsion-default") << QString("Propulsion") << QString("Propulsion") << true;
        QTest::newRow("choose-environment") << QString("Navigation") << QString("Environment") << true;
        QTest::newRow("changed-selection-cancel") << QString("Navigation") << QString("Generate") << false;
        QTest::newRow("empty-group-cancel") << QString("Other") << QString("Other") << false;
    }
    void clearGroup()
    {
        QFETCH(QString, active);
        QFETCH(QString, selection);
        QFETCH(bool, accept);
        MainWindow window;
        auto* controller = window.findChild<MainController*>();
        QVERIFY(controller);
        QVERIFY(controller->loadLayoutFromJson(":/defaults/defaultLayout.json"));
        window.show();
        auto* area = window.findChild<QMdiArea*>("mdiAreaMain");
        QVERIFY(area);
        for (auto* sub : area->subWindowList())
            if (sub->windowTitle() == active)
                area->setActiveSubWindow(sub);
        QCOMPARE(area->activeSubWindow()->windowTitle(), active);
        QMap<QString, QList<QPointer<QWidget>>> before;
        for (const auto& group : controller->getGroupList())
            for (auto* widget : *controller->getListWidgetsOnSubForm(group))
                before[group].append(widget);
        auto* model = window.findChild<PgnTableController*>()->getPgnTableModel();
        const int count = model->getPgnList().size();
        bool inspected = false;
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) {
                QFAIL("Clear Group did not open a dialog");
                return;
            }
            QTimer::singleShot(0, dialog, &QDialog::reject); // Close cleanly if an assertion below fails.
            auto* combo = dialog->findChild<QComboBox*>();
            if (!combo) {
                dialog->reject();
                QFAIL("Missing group selector");
                return;
            }
            QCOMPARE(combo->currentText(), active);
            QCOMPARE(model->getPgnList().size(), count);
            for (const auto& group : before)
                for (const auto& widget : group)
                    QVERIFY(widget);
            inspected = true;
            combo->setCurrentText(selection);
            if (accept)
                dialog->accept();
            else
                dialog->reject();
        });
        window.findChild<QAction*>("actionClear")->trigger();
        QVERIFY(inspected);
        settle();
        for (auto it = before.cbegin(); it != before.cend(); ++it)
            for (const auto& widget : it.value())
                QCOMPARE(widget.isNull(), accept && it.key() == selection);
        QCOMPARE(model->getPgnList().size(), count - (accept ? before[selection].size() : 0));
        if (accept)
            QVERIFY(controller->getListWidgetsOnSubForm(selection)->isEmpty());
        window.close();
    }
    void pickerUnknownInitialGroup()
    {
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->findChild<QComboBox*>()->currentText(), QString("Navigation"));
            dialog->reject();
        });
        QVERIFY(UiHelper::ShowGroupPicker("Pick", "Group", { "Navigation", "Generate" }, "Unknown").isEmpty());
    }
};
int main(int argc, char** argv)
{
    QList<QByteArray> storage;
    for (int i = 0; i < argc; ++i)
        storage.append(argv[i]);
    storage.append("--no-nmea");
    QList<char*> args;
    for (auto& arg : storage)
        args.append(arg.data());
    int appArgc = args.size();
    args.append(nullptr);
    QApplication app(appArgc, args.data());
    // Optional explicit font for legible offscreen captures; normal tests need no font file.
    const auto fontPath = qEnvironmentVariable("RAYNMEA_TEST_FONT");
    if (!fontPath.isEmpty()) {
        const int fontId = QFontDatabase::addApplicationFont(fontPath);
        if (fontId < 0) return 2;
        app.setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).first(), 10));
    }
    QTemporaryDir profile;
    if (!profile.isValid())
        return 1;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    AcceptanceFollowupTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "acceptance_followup_tests.moc"
