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

#include "appsettings.h"
#include "apppaths.h"
#include "depth128267.h"
#include "gnsspositiondata129029.h"
#include "helper.h"
#include "maincontroller.h"
#include "mainwindow.h"
#include "n2khelper.h"
#include "navigation129284.h"
#include "outsideenvironmental130310.h"
#include "systemtime126992.h"
#include "trimtab130576.h"
#include "uihelper.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

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
    PgnTableModel* model() { return tables.getPgnTableModel(); }
};
static QString fixture() { return ":/defaults/defaultLayout.json"; }
static void settle()
{
    for (int i = 0; i < 8; ++i)
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
}
static QJsonObject readJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return { };
    return QJsonDocument::fromJson(file.readAll()).object();
}
static void writeJson(const QString& path, const QJsonObject& root)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson());
}
static void answer(QMessageBox::StandardButton choice)
{
    QTimer::singleShot(0, [choice] {
        for (auto* widget : QApplication::topLevelWidgets())
            if (auto* box = qobject_cast<QMessageBox*>(widget))
                box->button(choice)->click();
    });
}
static QList<PgnWidgetBase*> instruments(Workspace& w)
{
    QList<PgnWidgetBase*> list;
    for (const auto& group : w.controller.getGroupList())
        for (auto* widget : *w.controller.getListWidgetsOnSubForm(group))
            if (auto* base = qobject_cast<PgnWidgetBase*>(widget))
                list.append(base);
    return list;
}
static QMap<QString, QPair<int, int>> positions(Workspace& w)
{
    QMap<QString, QPair<int, int>> result;
    for (auto* widget : instruments(w))
        for (auto* grid : w.area.findChildren<QGridLayout*>())
            if (grid->indexOf(widget->parentWidget()) >= 0) {
                int r, c, rs, cs;
                grid->getItemPosition(grid->indexOf(widget->parentWidget()), &r, &c, &rs, &cs);
                result[widget->windowTitle() + QString::number(widget->Id())] = { r, c };
            }
    return result;
}

class LayoutCorrectnessTests : public QObject {
    Q_OBJECT
private slots:
    void defaultAndNamedFields()
    {
        Workspace w;
        QVERIFY(w.controller.loadLayoutFromJson(fixture()));
        QCOMPARE(w.model()->getPgnList().size(), 35);
        settle();
        auto* gnss = w.area.findChild<GnssPositionData129029*>();
        QVERIFY(gnss);
        QCOMPARE(gnss->nReferenceStations(), 2);
        QCOMPARE(gnss->AgeOfCorrection(), .5);
        gnss->setnReferenceStations(5);
        gnss->setReferenceStationType(N2kGNSSt_GLONASS);
        gnss->setAgeOfCorrection(1.75);
        auto* depth = w.area.findChild<Depth128267*>();
        depth->setRange(1234);
        auto* outside = w.area.findChild<OutsideEnvironmental130310*>();
        outside->setSID(11);
        QTemporaryDir dir;
        const auto path = dir.filePath("named.json");
        QVERIFY(w.controller.saveLayoutToJson(path));
        Workspace restored;
        QVERIFY(restored.controller.loadLayoutFromJson(path));
        settle();
        QCOMPARE(restored.area.findChild<GnssPositionData129029*>()->nReferenceStations(), 5);
        QCOMPARE(restored.area.findChild<GnssPositionData129029*>()->AgeOfCorrection(), 1.75);
        QCOMPARE(restored.area.findChild<GnssPositionData129029*>()->ReferenceStationType(), N2kGNSSt_GLONASS);
        QCOMPARE(restored.area.findChild<Depth128267*>()->Range(), 1234.);
        QCOMPARE(restored.area.findChild<OutsideEnvironmental130310*>()->SID(), 11);
    }
    void currentFamilyRoundTrips()
    {
        Workspace w;
        QVERIFY(w.controller.loadLayoutFromJson(fixture()));
        // These families are outside Default, or had conflicting construction/restore field names.
        for (const QString pgn : { "126992", "GGA", "RMC", "HDG", "HDT", "GLL", "VTG", "ZDA" })
            w.controller.addWidgetAsComboboxTextChanged(pgn, pgn, "Navigation");
        QTimer::singleShot(0, [] {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* dialog = qobject_cast<QDialog*>(widget))
                    dialog->accept();
        });
        w.controller.addWidgetAsComboboxTextChanged("000000", "Spacer", "Other");
        for (const QString pgn : { "127250", "127258", "129283" })
            w.controller.addWidgetAsComboboxTextChanged(pgn, pgn, "Navigation");
        w.controller.addWidgetAsComboboxTextChanged("130576", "Trim", "Propulsion");
        w.controller.addWidgetAsComboboxTextChanged("127506", "DC", "Electrical");
        QTimer::singleShot(0, [] {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* dialog = qobject_cast<QDialog*>(widget))
                    dialog->accept();
        });
        w.controller.addWidgetAsComboboxTextChanged("130312", "Temperature", "Environment");
        auto* clock = w.area.findChild<SystemTime126992*>();
        QVERIFY(clock);
        w.controller.updateRcvdPgn(clock->Id(), { "SystemDate", "20000", "SystemTime", "12345.75" });
        QCOMPARE(clock->SystemDate(), uint16_t(20000));
        QCOMPARE(clock->SystemTime(), 12345.75);
        settle();
        w.controller.setMode(Viewer);
        QTemporaryDir dir;
        auto path = dir.filePath("families.json");
        QVERIFY(w.controller.saveLayoutToJson(path));
        Workspace restored;
        restored.controller.setMode(Viewer);
        QVERIFY(restored.controller.loadLayoutFromJson(path));
        QCOMPARE(restored.model()->getPgnList().size(), w.model()->getPgnList().size());
        for (const auto& data : w.model()->getPgnList()) {
            const auto copy = restored.model()->getPgnDataToSend(data.idX);
            if (copy.paramDetails != data.paramDetails)
                qInfo() << data.pgn << data.label;
            QCOMPARE(copy.paramDetails, data.paramDetails);
        }
    }
    void generatorOrderIndependentLoad()
    {
        auto root = readJson(fixture());
        auto groups = root["layouts"].toArray();
        QJsonArray reversed;
        for (auto i = groups.size(); i > 0; --i)
            reversed.append(groups[i - 1]);
        root["layouts"] = reversed;
        QTemporaryDir dir;
        const auto path = dir.filePath("reordered.json");
        writeJson(path, root);
        Workspace w;
        QVERIFY(w.controller.loadLayoutFromJson(path));
        QCOMPARE(w.model()->getPgnList().size(), 35);
        settle();
        for (auto* engine : w.area.findChildren<GeneratorEngine*>()) {
            engine->setEngineSpeed(1000 + engine->EngineInstance());
            QVERIFY(!engine->EngineRunning());
        }
        settle();
        for (auto* rapid : w.area.findChildren<EngineRapid127488*>())
            QCOMPARE(rapid->EngineSpeed(), 1000. + rapid->EngineInstance());
    }
    void layoutPreferencesAndDottedNames()
    {
        Workspace w;
        UserOptionPrefs prefs { };
        prefs.widgetColumns = 4;
        w.controller.setUserOptionPrefs(prefs);
        w.controller.removeAllWidgets();
        w.controller.addWidgetAsComboboxTextChanged("HDT", "Heading", "Navigation");
        QTemporaryDir dir;
        auto path = dir.filePath("layout.v1.json");
        QVERIFY(w.controller.saveLayoutToJson(path));
        QCOMPARE(readJson(path)["numberOfWidgetColumns"].toInt(), 4);
        QVERIFY(QDir().mkpath(AppPaths::layoutsDirectory()));
        const auto pickerPath=AppPaths::layoutsDirectory()+"/layout.v1.json";
        QVERIFY(QFile::copy(path,pickerPath));
        QTimer::singleShot(0, [pickerPath] {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* dialog = qobject_cast<QDialog*>(widget)) {
                    if(auto *items=dialog->findChild<QComboBox*>())items->setCurrentIndex(items->findData(pickerPath));
                    dialog->accept();
                }
        });
        QCOMPARE(UiHelper::ShowLayoutPicker("Pick", "Layout"),pickerPath);
        QVERIFY(QFile::remove(pickerPath));
        UserUnitPrefs units { };
        units.tempUnitIndex = 1;
        w.controller.setUserPrefs(units);
        for (auto* base : instruments(w))
            QCOMPARE(base->UserPrefs().tempUnitIndex, 1);
        Workspace restored;
        QVERIFY(restored.controller.loadLayoutFromJson(path));
        QVERIFY(restored.controller.saveLayoutToJson(path));
        QCOMPARE(readJson(path)["numberOfWidgetColumns"].toInt(), 4);
    }
    void rapidModeReturnDropsOldCalls()
    {
        Workspace w;
        QVERIFY(w.controller.loadLayoutFromJson(fixture()));
        settle();
        auto* engine = w.area.findChild<GeneratorEngine*>();
        EngineRapid127488* rapid = nullptr;
        for (auto* candidate : w.area.findChildren<EngineRapid127488*>()) {
            if (candidate->EngineInstance() == engine->EngineInstance())
                rapid = candidate;
        }
        QVERIFY(rapid);
        auto* movement = w.area.findChild<GeneratorVesselMovement*>();
        auto* nav = w.area.findChild<Navigation129284*>();
        QVERIFY(movement && nav);
        movement->setDistanceToWaypoint(500);
        engine->setEngineSpeed(3200);
        w.controller.setMode(Viewer);
        w.controller.updateRcvdPgn(nav->Id(), { "DistanceToWaypoint", "777" });
        w.controller.updateRcvdPgn(rapid->Id(), { "EngineSpeed", "1700" });
        w.controller.setMode(Sender);
        settle();
        QCOMPARE(rapid->EngineSpeed(), 1700.);
        QCOMPARE(nav->DistanceToWaypoint(), 777.);
        // A subsequent valid generation has exactly one instrument notification.
        QSignalSpy updates(rapid, &EngineRapid127488::EngineSpeedChanged);
        engine->setEngineSpeed(2500);
        settle();
        QCOMPARE(rapid->EngineSpeed(), 2500.);
        QCOMPARE(updates.size(), 1);
    }
    void invalidReplacement_data()
    {
        QTest::addColumn<QString>("kind");
        for (const QString kind :
            { "missing", "malformed", "array", "version", "columns", "type", "pgn", "duplicate", "parameters", "group", "span" })
            QTest::newRow(qPrintable(kind)) << kind;
    }
    void invalidReplacement()
    {
        QFETCH(QString, kind);
        Workspace w;
        w.controller.addWidgetAsComboboxTextChanged("HDT", "Unsaved", "Navigation");
        auto* original = w.controller.getListWidgetsOnSubForm("Navigation")->first();
        QTemporaryDir dir;
        const auto path = dir.filePath("invalid.json");
        auto root = readJson(fixture());
        auto groups = root["layouts"].toArray();
        auto group = groups[0].toObject();
        auto widgets = group["widgets"].toArray();
        auto item = widgets[0].toObject();
        if (kind == "version")
            root["formatVersion"] = 999;
        if (kind == "columns")
            root["numberOfWidgetColumns"] = 0;
        if (kind == "type")
            item["type"] = "Unknown";
        if (kind == "pgn")
            item["pgn"] = "127488";
        if (kind == "duplicate")
            widgets.append(item);
        if (kind == "parameters")
            item["parameters"] = QJsonArray { };
        if (kind == "group")
            group["groupName"] = "Invalid";
        if (kind == "span")
            item["rowSpan"] = -1;
        widgets[0] = item;
        group["widgets"] = widgets;
        groups[0] = group;
        root["layouts"] = groups;
        writeJson(path, root);
        if (kind == "missing")
            QFile::remove(path);
        if (kind == "malformed" || kind == "array") {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(kind == "array" ? "[]" : "{bad");
        }
        QSignalSpy replacing(&w.controller, &MainController::layoutAboutToReplace);
        QVERIFY(!w.controller.loadLayoutFromJson(path));
        QCOMPARE(replacing.size(), 0);
        QCOMPARE(w.model()->getPgnList().size(), 1);
        QCOMPARE(w.controller.getListWidgetsOnSubForm("Navigation")->first(), original);
    }
    void cancelAndManualReplacement()
    {
        Workspace w;
        w.controller.addWidgetAsComboboxTextChanged("HDT", "Custom", "Navigation");
        QPointer<QWidget> original = w.controller.getListWidgetsOnSubForm("Navigation")->first();
        answer(QMessageBox::No);
        QVERIFY(!w.controller.loadLayoutFromJson(fixture()));
        QVERIFY(original);
        answer(QMessageBox::Yes);
        QVERIFY(w.controller.loadLayoutFromJson(fixture()));
        QVERIFY(original.isNull());
        QCOMPARE(w.model()->getPgnList().size(), 35);
    }
    void immediateEditsAndAtomicFailure()
    {
        Workspace w;
        QVERIFY(w.controller.loadLayoutFromJson(fixture()));
        settle();
        auto* engine = w.area.findChild<EngineDynamic127489*>();
        QVERIFY(engine);
        QVERIFY(
            QMetaObject::invokeMethod(engine, "on_lineEditEngineOilPress_textEdited", Qt::DirectConnection, Q_ARG(QString, QString("12"))));
        QVERIFY(
            QMetaObject::invokeMethod(engine, "on_lineEditEngineHours_textEdited", Qt::DirectConnection, Q_ARG(QString, QString("999"))));
        auto* outside = w.area.findChild<OutsideEnvironmental130310*>();
        QVERIFY(QMetaObject::invokeMethod(
            outside, "on_lineEditAmbientAirTemperature_textEdited", Qt::DirectConnection, Q_ARG(QString, QString("70"))));
        QVERIFY(QMetaObject::invokeMethod(
            outside, "on_lineEditAtmosphericPressure_textEdited", Qt::DirectConnection, Q_ARG(QString, QString("14"))));
        QTemporaryDir dir;
        const auto path = dir.filePath("edits.json");
        QVERIFY(w.controller.saveLayoutToJson(path));
        Workspace restored;
        QVERIFY(restored.controller.loadLayoutFromJson(path));
        settle();
        EngineDynamic127489* other = nullptr;
        for (auto* candidate : restored.area.findChildren<EngineDynamic127489*>())
            if (candidate->Id() == engine->Id())
                other = candidate;
        QVERIFY(other);
        QCOMPARE(other->EngineHours(), 999. * 3600.); // Editor hours; canonical model seconds.
        QVERIFY(qAbs(other->EngineOilPress() - N2kHelper::convertUserPressureToN2kUnits(12, engine->UserPrefs())) < .01);
        QCOMPARE(restored.area.findChild<OutsideEnvironmental130310*>()->OutsideAmbientAirTemperature(),
            outside->OutsideAmbientAirTemperature());
        QCOMPARE(restored.area.findChild<OutsideEnvironmental130310*>()->AtmosphericPressure(), outside->AtmosphericPressure());
        QFile saved(path);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto bytes = saved.readAll();
        saved.close();
        QSignalSpy status(&w.controller, &MainController::ShowStatusBarMessage);
        QVERIFY(!w.controller.saveLayoutToJson(dir.filePath("missing/edits.json")));
        QVERIFY(status.last().first().toString().contains("failed"));
        QVERIFY(!w.controller.saveLayoutToJson(dir.path()));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(saved.readAll(), bytes);
    }
    void enabledAllFamilies()
    {
        Workspace w;
        QVERIFY(w.controller.loadLayoutFromJson(fixture()));
        settle();
        QMap<int, bool> expected;
        for (auto* base : instruments(w)) {
            const bool enabled = base->Id() % 2;
            base->PgnWidgetBase::setEnabled(enabled);
            expected[base->Id()] = enabled;
        }
        QTemporaryDir dir;
        const auto path = dir.filePath("enabled.json");
        QVERIFY(w.controller.saveLayoutToJson(path));
        Workspace restored;
        UserOptionPrefs prefs { };
        prefs.newWidgetsEnabled = false;
        restored.controller.setUserOptionPrefs(prefs);
        QVERIFY(restored.controller.loadLayoutFromJson(path));
        settle();
        for (auto* base : instruments(restored)) {
            QCOMPARE(base->PgnWidgetBase::isEnabled(), expected[base->Id()]);
            QCOMPARE(restored.model()->isPgnEnabled(base->Id()), expected[base->Id()]);
            if (auto* check = base->findChild<QCheckBox*>("checkBoxEnabled"))
                QCOMPARE(check->isChecked(), expected[base->Id()]);
        }
    }
    void columnsAndFlow_data()
    {
        QTest::addColumn<int>("columns");
        for (int i = 1; i <= 4; ++i)
            QTest::newRow(qPrintable(QString::number(i))) << i;
    }
    void columnsAndFlow()
    {
        QFETCH(int, columns);
        Workspace w;
        for (int i = 0; i < 5; ++i)
            w.controller.addWidgetAsComboboxTextChanged("HDT", QString("Heading%1").arg(i), "Navigation");
        auto list = instruments(w);
        w.controller.removeWidgetFromSubForm(list[0], "Navigation");
        w.controller.addWidgetAsComboboxTextChanged("HDT", "New", "Navigation");
        w.controller.setNumberOfWidgetWide(columns);
        const auto before = positions(w);
        for (const auto& p : before)
            QVERIFY(p.second < columns * 3);
        QTemporaryDir dir;
        const auto path = dir.filePath("columns.json");
        QVERIFY(w.controller.saveLayoutToJson(path));
        QCOMPARE(readJson(path)["numberOfWidgetColumns"].toInt(), columns);
        Workspace restored;
        QVERIFY(restored.controller.loadLayoutFromJson(path));
        QCOMPARE(positions(restored), before);
    }
    void identityAndModes()
    {
        Workspace w;
        QVERIFY(w.controller.loadLayoutFromJson(fixture()));
        settle();
        auto widgets = instruments(w);
        const auto placement = positions(w);
        GeneratorEngine *port = nullptr, *stbd = nullptr;
        EngineRapid127488* rapid = nullptr;
        for (auto* e : w.area.findChildren<GeneratorEngine*>()) {
            if (e->EngineInstance() == 0)
                port = e;
            else if (e->EngineInstance() == 1)
                stbd = e;
        }
        for (auto* e : w.area.findChildren<EngineRapid127488*>())
            if (e->EngineInstance() == 0)
                rapid = e;
        QVERIFY(port && stbd && rapid);
        w.model()->setCurrentPgn(rapid->Id());
        QVERIFY(w.model()->setData(w.model()->index(0, 1), 1));
        settle();
        QCOMPARE(rapid->EngineInstance(), 1);
        port->setEngineSpeed(1111);
        stbd->setEngineSpeed(2222);
        settle();
        QCOMPARE(rapid->EngineSpeed(), 2222.);
        auto* movement = w.area.findChild<GeneratorVesselMovement*>();
        QVERIFY(movement);
        for (int cycle = 0; cycle < 4; ++cycle) {
            port->setEngineRunning(true);
            port->setEngineSpeed(1234);
            stbd->setEngineSpeed(2345); // Queue updates before switching.
            w.controller.setMode(Viewer);
            QVERIFY(!port->EngineRunning());
            QVERIFY(!stbd->EngineRunning());
            auto* weather = w.area.findChild<GeneratorWeather*>();
            const double air = weather->AirTemperature();
            weather->weatherFetched("late", "late", 333, 99, 1013, 20, 90);
            QCOMPARE(weather->AirTemperature(), air);
            w.controller.updateRcvdPgn(rapid->Id(), { "EngineSpeed", "3456" });
            settle();
            QCOMPARE(rapid->EngineSpeed(), 3456.);
            stbd->setEngineSpeed(4567);
            settle();
            QCOMPARE(rapid->EngineSpeed(), 3456.);
            w.controller.setMode(Sender);
            settle();
            QCOMPARE(rapid->EngineSpeed(), 3456.);
            QVERIFY(!port->EngineRunning());
            QVERIFY(!movement->StartEngineEnabled());
            QCOMPARE(instruments(w), widgets);
            QCOMPARE(positions(w), placement);
            stbd->setEngineSpeed(5000 + cycle);
            settle();
            QCOMPARE(rapid->EngineSpeed(), 5000. + cycle);
        }
        auto* trans = w.area.findChild<TransmissionDynamic127493*>();
        QVERIFY(trans);
        w.model()->setCurrentPgn(trans->Id());
        QVERIFY(w.model()->setData(w.model()->index(0, 1), 1 - trans->EngineInstance()));
        settle();
        auto lower = w.area.findChildren<GeneratorTransLowerUnit*>();
        for (auto* g : lower)
            g->setTransmissionGear(g->EngineInstance() % 2 ? N2kTG_Reverse : N2kTG_Forward);
        settle();
        for (auto* i : w.area.findChildren<TransmissionDynamic127493*>())
            QCOMPARE(i->TransmissionGear(), i->EngineInstance() % 2 ? N2kTG_Reverse : N2kTG_Forward);
    }
    void routeAndFreshProcess()
    {
        Workspace w;
        QVERIFY(w.controller.loadLayoutFromJson(fixture()));
        settle();
        auto* movement = w.area.findChild<GeneratorVesselMovement*>();
        movement->restoreLayoutConfiguration(
            { { "route", "missing-route.gpx" }, { "reverse", true }, { "speedSource", 1 }, { "speed", 4.5 } });
        w.controller.setNumberOfWidgetWide(3);
        auto* rapid = w.area.findChild<EngineRapid127488*>();
        rapid->setEnabled(false);
        QTemporaryDir dir;
        const auto path = dir.filePath("fresh.json"), output = dir.filePath("summary.json");
        QVERIFY(w.controller.saveLayoutToJson(path));
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), { "--verify-layout", path, output });
        QVERIFY(child.waitForFinished(30000));
        QCOMPARE(child.exitCode(), 0);
        auto result = readJson(output);
        QCOMPARE(result["count"].toInt(), 35);
        QCOMPARE(result["columns"].toInt(), 3);
        QCOMPARE(result["route"].toObject(), movement->layoutConfiguration());
        QVERIFY(!result["running"].toBool());
        QCOMPARE(result["disabledId"].toInt(), rapid->Id());
    }
};
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir profile(QDir::tempPath() + "/ray-layout-XXXXXX");
    if (!profile.isValid())
        return 2;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    if (app.arguments().size() == 4 && app.arguments()[1] == "--verify-layout") {
        Workspace w;
        if (!w.controller.loadLayoutFromJson(app.arguments()[2]))
            return 3;
        settle();
        auto* movement = w.area.findChild<GeneratorVesselMovement*>();
        bool running = false;
        for (auto* e : w.area.findChildren<GeneratorEngine*>())
            running |= e->EngineRunning();
        QTemporaryDir dir;
        auto path = dir.filePath("copy.json");
        if (!w.controller.saveLayoutToJson(path))
            return 4;
        int disabled = -1;
        for (auto* r : w.area.findChildren<EngineRapid127488*>())
            if (!r->PgnWidgetBase::isEnabled())
                disabled = r->Id();
        writeJson(app.arguments()[3],
            { { "disabledId", disabled }, { "count", w.model()->getPgnList().size() },
                { "columns", readJson(path)["numberOfWidgetColumns"] }, { "route", movement->layoutConfiguration() },
                { "running", running } });
        return 0;
    }
    LayoutCorrectnessTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "layout_correctness_tests.moc"
