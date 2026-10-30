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
#include <QChart>
#include <QChartView>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSlider>
#include <QLineSeries>
#include <QScatterSeries>
#include <QTabWidget>
#include <QValueAxis>
#include <QTableWidget>
#include <QTemporaryDir>
#include "engineprofilecatalog.h"
#include "engineprofiledialog.h"
#include "apppaths.h"
#include "appsettings.h"
#include "mainwindow.h"
#include "runtimeresources.h"
#include "convert.h"
#include "themes.h"

namespace {
QByteArray read(const QString &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
void write(const QString &path, const QByteArray &bytes) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath())); QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(bytes), bytes.size());
}
QJsonObject original() { return QJsonDocument::fromJson(read(":/Data/engine_profiles.json")).object(); }
QJsonObject withFirst(const QJsonObject &profile, QJsonObject doc = original()) {
    auto all = doc["engines"].toArray(); all[0] = profile; doc["engines"] = all; return doc;
}
void answer(QMessageBox::StandardButton which) {
    QTimer::singleShot(0, [which] {
        for (auto *w : QApplication::topLevelWidgets()) if (auto *box = qobject_cast<QMessageBox *>(w)) box->button(which)->click();
    });
}
void click(QObject *root, const char *name) { auto *b = root->findChild<QPushButton *>(name); Q_ASSERT(b); b->click(); }
void fileDialog(const QString &path) {
    QTimer::singleShot(0, [path] {
        for (auto *w : QApplication::topLevelWidgets()) if (auto *box = qobject_cast<QFileDialog *>(w)) {
            box->selectFile(path); static_cast<QDialog *>(box)->accept();
        }
    });
}
void openEditor(MainWindow &window) { window.findChild<QAction *>("actionEngineProfiles")->trigger(); }
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode mode) override { return QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { QIODevice::close(); }
    qint64 bytesToWrite() const override { return 0; }
protected:
    qint64 writeData(const char *data, qint64 size) override { output.append(data, size); return size; }
};
}

class EngineProfileEditorTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> home;
    QString target;
private slots:
    void init() {
        home = std::make_unique<QTemporaryDir>(QDir::currentPath() + "/build/m222-test-XXXXXX");
        QVERIFY(home->isValid()); qputenv("RAYNMEA_TEST_HOME", home->path().toUtf8());
        target = AppPaths::resourcesDirectory() + "/Data/engine_profiles.json";
        qputenv("RAYNMEA_TEST_ENGINE_PROFILES", target.toUtf8());
        write(target, read(":/Data/engine_profiles.json"));
        AppSettings settings(APP_COMPANY, APP_NAME); settings.clear();
        settings.setValue("Nmea2000/InterfaceType", "None"); settings.setValue("Preference/StyleSheetName", "Dark");
    }
    void cleanup() { QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); qunsetenv("RAYNMEA_TEST_ENGINE_PROFILES"); qunsetenv("RAYNMEA_TEST_HOME"); home.reset(); }
    void originalCatalogAndOptionalFields() {
        EngineProfileCatalog c; QString error; QVERIFY2(c.open(&error), qPrintable(error));
        QCOMPARE(c.document()["engines"].toArray().size(), 25);
        bool fractional = false, absent = false, twelve = false, twentyFour = false;
        for (const auto &v : c.document()["engines"].toArray()) {
            const auto p = v.toObject(); fractional |= p["horsepower"].toDouble() == 9.9;
            absent |= !p.contains("fuelInjected"); twelve |= p["alternatorVoltage"].toInt() == 12; twentyFour |= p["alternatorVoltage"].toInt() == 24;
        }
        QVERIFY(fractional && absent && twelve && twentyFour);
        QVERIFY(c.save(c.document(), &error)); QCOMPARE(c.document(), original());
        EngineProfileDialog d([] { return QStringList(); });
        auto *list = d.findChild<QListWidget *>("engineProfiles");
        for (int row = 0; row < list->count(); ++row) list->setCurrentRow(row);
        click(&d, "saveCatalog"); QCOMPARE(QJsonDocument::fromJson(read(target)).object(), original());
    }
    void extensionsAndAlternateCurvePreserved() {
        auto doc = original(), p = doc["engines"].toArray()[0].toObject();
        auto points = p["rpm_gph_pairs"].toArray(); auto row = points[0].toObject(); row["ownerNote"] = "measured"; points[0] = row;
        p.remove("rpm_gph_pairs"); p["rpm_gph_data"] = points; p["customSpecification"] = QJsonArray{1, "owner"};
        p.remove("fuelType"); doc = withFirst(p); doc["catalogOwner"] = "fixture"; write(target, QJsonDocument(doc).toJson());
        EngineProfileDialog d([] { return QStringList(); }); d.findChild<QLineEdit *>("horsepower")->setText("9.9"); click(&d, "saveCatalog");
        auto expected = p; expected["horsepower"] = 9.9;
        auto result = QJsonDocument::fromJson(read(target)).object(); QCOMPARE(result["engines"].toArray()[0].toObject(), expected);
        QCOMPARE(result["catalogOwner"].toString(), QString("fixture"));
    }
    void validation_data() {
        QTest::addColumn<QString>("kind");
        for (const auto &kind : {"duplicateRPM", "fractionalRPM", "negativeRPM", "tooLargeRPM", "nonfiniteText", "negativeGPH", "unordered", "shortCurve", "wrongFieldType", "duplicateName", "emptyName", "reservedName", "invalidHorsepower", "noEngines"}) QTest::newRow(kind) << QString(kind);
    }
    void validation() {
        QFETCH(QString, kind); auto doc = original(), p = doc["engines"].toArray()[0].toObject(); auto points = p["rpm_gph_pairs"].toArray();
        auto first = points[0].toObject();
        if (kind == "duplicateRPM") first["rpm"] = points[1].toObject()["rpm"];
        if (kind == "fractionalRPM") first["rpm"] = 500.5;
        if (kind == "negativeRPM") first["rpm"] = -1;
        if (kind == "tooLargeRPM") first["rpm"] = 100001;
        if (kind == "nonfiniteText") first["gph"] = "nan";
        if (kind == "negativeGPH") first["gph"] = -0.01;
        points[0] = first;
        if (kind == "unordered") { auto v = points[0]; points[0] = points[1]; points[1] = v; }
        if (kind == "shortCurve") points = QJsonArray{first};
        p["rpm_gph_pairs"] = points;
        if (kind == "wrongFieldType") p["fuelInjected"] = "false";
        if (kind == "emptyName") p["name"] = "  ";
        if (kind == "reservedName") p["name"] = "None";
        if (kind == "invalidHorsepower") p["horsepower"] = -1;
        if (kind == "duplicateName") p["name"] = doc["engines"].toArray()[1].toObject()["name"];
        doc = withFirst(p);
        if (kind == "noEngines") doc.remove("engines");
        EngineProfileCatalog c; QString error; QVERIFY(c.open(&error)); const auto before = read(target);
        QVERIFY(!c.save(doc, &error)); QVERIFY2(!error.isEmpty(), qPrintable(kind)); QCOMPARE(read(target), before);
    }
    void invalidJsonAndInvalidRestore() {
        EngineProfileCatalog c; QString error; QVERIFY(c.open(&error)); const auto before = read(target);
        for (const auto &bytes : {QByteArray("{broken"), QByteArray("[]"), QByteArray("{\"engines\":[{}]}")}) {
            QVERIFY(!c.restore(bytes, &error)); QVERIFY(!error.isEmpty()); QCOMPARE(read(target), before);
        }
    }
    void backupRestoreRecoveryAndExternalEdits() {
        EngineProfileCatalog c; QString error; QVERIFY(c.open(&error)); auto before = read(target);
        const auto backup = home->path() + "/backup.json";
        QVERIFY(c.backup(backup, false, &error)); QCOMPARE(read(backup), before);
        QVERIFY(!c.backup(backup, false, &error)); QVERIFY(!c.backup(target, true, &error));
        auto doc = c.document(), p = doc["engines"].toArray()[0].toObject(); p["horsepower"] = 123.5;
        QVERIFY(c.save(withFirst(p), &error)); QCOMPARE(read(c.lastBackup()), before);
        const auto changed = read(target); QVERIFY(c.restore(read(backup), &error)); QCOMPARE(read(target), before); QCOMPARE(read(c.lastBackup()), changed);
        write(target, "{malformed owner catalog"); QVERIFY(!c.open(&error));
        QVERIFY(!c.save(original(), &error)); QCOMPARE(read(target), QByteArray("{malformed owner catalog"));
        QVERIFY(c.restore(before, &error)); QCOMPARE(read(c.lastBackup()), QByteArray("{malformed owner catalog"));
        write(target, changed); QVERIFY(!c.save(doc, &error)); QVERIFY(error.contains("outside")); QCOMPARE(read(target), changed);
    }
    void ioFailurePreservesSavedCatalog() {
        EngineProfileCatalog c; QString error; QVERIFY(c.open(&error)); const auto before = read(target);
        const auto blocked = home->path() + "/blocked"; write(blocked, "file not directory");
        QVERIFY(!c.backup(blocked + "/backup.json", false, &error)); QCOMPARE(read(target), before); QCOMPARE(read(blocked), QByteArray("file not directory"));
        { QLockFile lock(target + ".lock"); QVERIFY(lock.tryLock());
          QVERIFY(!c.save(c.document(), &error)); QVERIFY(error.contains("lock")); QCOMPARE(read(target), before); }
        // Atomic replacement failure: directory at the target path, never removed
        // or truncated. External-state checks reject it before destructive I/O.
        const auto saved = target + ".original"; QVERIFY(QFile::rename(target, saved)); QVERIFY(QDir().mkpath(target));
        QVERIFY(!c.save(c.document(), &error)); QCOMPARE(read(saved), before); QVERIFY(QFileInfo(target).isDir());
    }
    void legacyCatalogLocalCopyAndRestart() {
        qunsetenv("RAYNMEA_TEST_ENGINE_PROFILES"); QVERIFY(QFile::remove(target));
        const auto old = AppPaths::legacyDirectory() + "/resources/Data/engine_profiles.json";
        auto doc = original(); doc["legacyOwner"] = "preserve"; const auto bytes = QJsonDocument(doc).toJson(); write(old, bytes);
        QCOMPARE(RuntimeResources::engineProfiles(), old);
        EngineProfileCatalog c; QString error; QVERIFY(c.open(&error)); QVERIFY(c.needsLocalCopy());
        QVERIFY(!QFileInfo::exists(target)); QVERIFY(c.save(doc, &error)); QCOMPARE(read(old), bytes); QCOMPARE(c.sourcePath(), target);
        EngineProfileCatalog reopened; QVERIFY(reopened.open(&error)); QCOMPARE(reopened.document(), doc); QCOMPARE(RuntimeResources::engineProfiles(), target);
    }
    void legacyCopyRequiresConfirmationAndMissingSetupSelection() {
        qunsetenv("RAYNMEA_TEST_ENGINE_PROFILES"); QVERIFY(QFile::remove(target));
        const auto old = AppPaths::legacyDirectory() + "/resources/Data/engine_profiles.json"; const auto bytes = read(":/Data/engine_profiles.json"); write(old, bytes);
        auto *d = new EngineProfileDialog([] { return QStringList(); }); d->show(); click(d, "duplicateProfile");
        answer(QMessageBox::No); click(d, "saveCatalog"); QVERIFY(!QFileInfo::exists(target)); QCOMPARE(read(old), bytes);
        answer(QMessageBox::Yes); click(d, "saveCatalog"); QVERIFY(QFileInfo::exists(target)); QCOMPARE(read(old), bytes); QVERIFY(d->close());
        AppSettings settings(APP_COMPANY, APP_NAME); settings.setValue("Vessel/EngineProfile", "Missing original name"); settings.sync();
        DialogSetup setup; setup.show(); QTest::qWait(10);
        auto *combo = setup.findChild<QComboBox *>("comboBoxVesselEngine"); QCOMPARE(combo->currentText(), QString("Missing original name")); QVERIFY(combo->toolTip().contains("unavailable"));
        setup.refreshEngineProfiles(); QCOMPARE(combo->currentText(), QString("Missing original name")); QCOMPARE(settings.value("Vessel/EngineProfile").toString(), QString("Missing original name"));
    }
    void draftOperationsAndCancel() {
        auto *d = new EngineProfileDialog([] { return QStringList(); }); QPointer<EngineProfileDialog> guard = d; d->show();
        const auto before = read(target); auto *list = d->findChild<QListWidget *>("engineProfiles");
        click(d, "duplicateProfile"); QCOMPARE(list->count(), 26); const auto firstCopy = d->findChild<QLineEdit *>("profileName")->text();
        click(d, "duplicateProfile"); QCOMPARE(list->count(), 27); QVERIFY(firstCopy != d->findChild<QLineEdit *>("profileName")->text());
        click(d, "newProfile"); QCOMPARE(list->count(), 28); click(d, "saveCatalog");
        QVERIFY(d->findChild<QLabel *>("catalogStatus")->text().contains("at least two")); QCOMPARE(read(target), before);
        answer(QMessageBox::No); click(d, "deleteProfile"); QCOMPARE(list->count(), 28);
        answer(QMessageBox::Yes); click(d, "deleteProfile"); QCOMPARE(list->count(), 27);
        answer(QMessageBox::Cancel); QVERIFY(!d->close()); QVERIFY(guard && d->isVisible());
        answer(QMessageBox::Discard); QVERIFY(d->close()); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); QVERIFY(!guard); QCOMPARE(read(target), before);
    }
    void unsavedCloseSaveRespectsRunningGuard() {
        bool running = true;
        auto *d = new EngineProfileDialog([&] { return running ? QStringList{"Test engine"} : QStringList(); }); QPointer<EngineProfileDialog> guard = d; d->show();
        const auto before = read(target); click(d, "duplicateProfile"); d->findChild<QLineEdit *>("profileName")->setText("Close saved fixture");
        answer(QMessageBox::Save); QVERIFY(!d->close()); QVERIFY(guard && d->isVisible()); QCOMPARE(read(target), before);
        running = false; answer(QMessageBox::Save); QVERIFY(d->close());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); QVERIFY(!guard);
        const auto saved = QJsonDocument::fromJson(read(target)).object()["engines"].toArray(); QCOMPARE(saved.last().toObject()["name"].toString(), QString("Close saved fixture"));
    }
    void newProfileSurvivesProcessRestart() {
        EngineProfileCatalog c; QString error; QVERIFY(c.open(&error)); auto doc = c.document(); auto all = doc["engines"].toArray();
        auto p = all[0].toObject(); p["name"] = "Restart fixture"; all.append(p); doc["engines"] = all; QVERIFY(c.save(doc, &error));
        QProcess child; child.start(QCoreApplication::applicationFilePath(), {"--catalog-read-child"});
        QVERIFY(child.waitForFinished(15000)); QCOMPARE(child.exitCode(), 0); QVERIFY(child.readAllStandardOutput().contains("CATALOG_RESTART_PASS"));
    }
    void createEditCurveRenameDelete() {
        auto *d = new EngineProfileDialog([] { return QStringList(); }); d->show();
        auto *list = d->findChild<QListWidget *>("engineProfiles"); auto *table = d->findChild<QTableWidget *>("fuelCurve");
        click(d, "newProfile"); d->findChild<QLineEdit *>("profileName")->setText("Owner fixture");
        d->findChild<QLineEdit *>("horsepower")->setText("123.75");
        d->findChild<QComboBox *>("fuelInjected")->setCurrentIndex(0);
        click(d, "addCurveRow"); table->setItem(0, 0, new QTableWidgetItem("2000")); table->setItem(0, 1, new QTableWidgetItem("2.5"));
        click(d, "addCurveRow"); table->setItem(1, 0, new QTableWidgetItem("1000")); table->setItem(1, 1, new QTableWidgetItem("0"));
        click(d, "sortCurve"); QCOMPARE(table->item(0, 0)->text(), QString("1000"));
        click(d, "saveCatalog"); auto doc = QJsonDocument::fromJson(read(target)).object(); QCOMPARE(doc["engines"].toArray().size(), 26);
        auto created = doc["engines"].toArray().last().toObject(); QCOMPARE(created["horsepower"].toDouble(), 123.75); QVERIFY(!created.contains("fuelInjected"));
        d->findChild<QLineEdit *>("profileName")->setText("Renamed fixture");
        answer(QMessageBox::No); click(d, "saveCatalog"); QCOMPARE(QJsonDocument::fromJson(read(target)).object(), doc);
        answer(QMessageBox::Yes); click(d, "saveCatalog"); QCOMPARE(QJsonDocument::fromJson(read(target)).object()["engines"].toArray().last().toObject()["name"].toString(), QString("Renamed fixture"));
        answer(QMessageBox::Yes); click(d, "deleteProfile"); QCOMPARE(list->count(), 25);
        answer(QMessageBox::Yes); click(d, "saveCatalog"); QCOMPARE(QJsonDocument::fromJson(read(target)).object(), original());
        QVERIFY(d->close());
    }
    void runningGuardAndGeneratorCacheRefresh() {
        GeneratorEngine engine(101, "Engine"); engine.setEngineProfile(original()["engines"].toArray()[0].toObject()["name"].toString());
        const auto name = engine.engineProfile(); QVERIFY(engine.profileReady()); engine.setEngineRunning(true);
        for (auto *t : engine.findChildren<QTimer *>()) t->stop();
        const auto before = read(target); auto *d = new EngineProfileDialog([&] { return engine.EngineRunning() ? QStringList{"Test engine"} : QStringList(); }); d->show();
        d->findChild<QLineEdit *>("horsepower")->setText("123"); click(d, "saveCatalog");
        QVERIFY(d->findChild<QLabel *>("catalogStatus")->text().contains("Stop all")); QCOMPARE(read(target), before); QVERIFY(engine.EngineRunning()); QVERIFY(!engine.reloadEngineProfiles()); QCOMPARE(engine.engineProfile(), name);
        engine.setEngineRunning(false); QSignalSpy saved(d, &EngineProfileDialog::catalogSaved); click(d, "saveCatalog"); QCOMPARE(saved.count(), 1);
        QVERIFY(engine.reloadEngineProfiles()); QCOMPARE(engine.engineProfile(), name); QVERIFY(!engine.EngineRunning()); QVERIFY(engine.profileReady()); QVERIFY(d->close());
    }
    void fuelInterpolationUnchanged() {
        auto p = original()["engines"].toArray()[0].toObject(); p["name"] = "Interpolation fixture";
        p["rpm_gph_pairs"] = QJsonArray{QJsonObject{{"rpm", 1000}, {"gph", 1}}, QJsonObject{{"rpm", 3000}, {"gph", 5}}};
        auto doc = original(); auto all = doc["engines"].toArray(); all.append(p); doc["engines"] = all;
        EngineProfileCatalog c; QString error; QVERIFY(c.open(&error)); QVERIFY(c.save(doc, &error));
        GeneratorEngine engine(103, "Engine"); engine.setEngineProfile("Interpolation fixture"); engine.setEngineRunning(true);
        for (auto *t : engine.findChildren<QTimer *>()) t->stop();
        engine.findChild<QCheckBox *>("checkBoxEng1Rand")->setChecked(false);
        engine.findChild<QSlider *>("verticalSliderThrottle")->setValue(2000);
        for (int i = 0; i < 4; ++i) QVERIFY(QMetaObject::invokeMethod(&engine, "PollTimerElapsed", Qt::DirectConnection));
        QCOMPARE(engine.EngineSpeed(), 2000.0); QVERIFY(qAbs(engine.FuelRate() - Convert::GphToLph(3)) < 1e-10);
        engine.setEngineRunning(false);
        all.removeLast(); doc["engines"] = all; QVERIFY(c.save(doc, &error)); QVERIFY(engine.reloadEngineProfiles());
        QCOMPARE(engine.engineProfile(), QString("Interpolation fixture")); QVERIFY(!engine.profileReady()); engine.setEngineRunning(true); QVERIFY(!engine.EngineRunning());
    }
    void modelessSingletonSelectionAndShutdown() {
        MainWindow w; w.resize(1300, 850); w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto *sending = w.findChild<QAction *>("actionStart_Sending"); QVERIFY(!sending->isChecked());
        openEditor(w); auto *d = w.findChild<EngineProfileDialog *>(); QVERIFY(d); QPointer<EngineProfileDialog> guard = d;
        QCOMPARE(d->windowModality(), Qt::NonModal); QVERIFY(!d->isModal()); QVERIFY(w.isEnabled());
        openEditor(w); QCOMPARE(w.findChildren<EngineProfileDialog *>().size(), 1);
        QVERIFY(w.findChildren<GeneratorEngine *>().isEmpty());
        click(d, "saveCatalog"); QVERIFY(!d->findChild<QLabel *>("catalogStatus")->text().startsWith("Not saved:"));
        answer(QMessageBox::Yes); click(d, "restoreBundled");
        // Real main-window mode operations remain available with the editor open.
        w.findChild<QAction *>("actionViewer_Rx")->trigger(); QVERIFY(w.findChild<QAction *>("actionViewer_Rx")->isChecked());
        w.findChild<QAction *>("actionSimulator_Tx")->trigger(); QVERIFY(!sending->isChecked());
        const auto before = read(target); click(d, "duplicateProfile"); d->findChild<QLineEdit *>("profileName")->setText("Concurrent draft");
        QCOMPARE(read(target), before);
        answer(QMessageBox::Cancel); QVERIFY(!w.close()); QVERIFY(w.isVisible() && d->isVisible()); QVERIFY(!sending->isChecked());
        answer(QMessageBox::Discard); QVERIFY(w.close()); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); QVERIFY(!guard); QCOMPARE(read(target), before);
    }
    void modelessConcurrentSendingAndGeneration() {
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Nmea0183/Nmea0183Enabled", true); settings.setValue("Nmea0183/Nmea0183Port", "MEMORY"); settings.sync();
        auto *port = new MemoryPort; MainWindow w(nullptr, nullptr, port); w.show();
        auto *model = w.findChild<PgnTableController *>()->getPgnTableModel();
        model->addPgn(501, "Fixture", "HDT", true, {{"Heading", "1", "rad", "double"}}, {}, 250);
        auto *sending = w.findChild<QAction *>("actionStart_Sending"); QVERIFY(sending->isEnabled()); sending->trigger(); QVERIFY(sending->isChecked());
        auto *engine = new GeneratorEngine(502, "Concurrent engine", &w);
        engine->setEngineProfile("MerCruiser 7.4 MPI"); engine->setEngineRunning(true); QVERIFY(engine->EngineRunning());
        openEditor(w); auto *d = w.findChild<EngineProfileDialog *>(); QVERIFY(d && !d->isModal()); QVERIFY(sending->isChecked());
        QSignalSpy generation(engine, &GeneratorEngine::EngineSpeedChanged);
        click(d, "duplicateProfile"); d->findChild<QLineEdit *>("profileName")->setText("Unsaved concurrent copy");
        QTRY_VERIFY_WITH_TIMEOUT(!port->output.isEmpty(), 1500); QTRY_VERIFY_WITH_TIMEOUT(generation.count() > 0, 1500);
        QVERIFY(sending->isChecked()); QVERIFY(engine->EngineRunning());
        click(d, "saveCatalog"); QVERIFY(d->findChild<QLabel *>("catalogStatus")->text().contains("Stop all"));
        answer(QMessageBox::Cancel); QVERIFY(!w.close()); QVERIFY(sending->isChecked()); QVERIFY(engine->EngineRunning());
        engine->setEngineRunning(false); click(d, "saveCatalog");
        QVERIFY(!d->findChild<QLabel *>("catalogStatus")->text().startsWith("Not saved:")); QVERIFY(sending->isChecked());
        const auto afterSave = port->output.size(); QTRY_VERIFY_WITH_TIMEOUT(port->output.size() > afterSave, 1500);
        QVERIFY(d->close()); QVERIFY(sending->isChecked());
        w.findChild<QAction *>("actionStop_Sending")->trigger(); QVERIFY(!sending->isChecked());
    }
    void movementControlsDoNotFalselyHideRunningEngines() {
        MainWindow w; w.findChild<MainController *>()->loadLayoutFromJson(":/defaults/defaultLayout.json");
        auto engines = w.findChildren<GeneratorEngine *>(); QCOMPARE(engines.size(), 2);
        for (auto *e : engines) e->setEngineProfile("MerCruiser 7.4 MPI");
        auto *movement = w.findChild<GeneratorVesselMovement *>(); QVERIFY(movement);
        auto *manual = movement->findChild<QCheckBox *>("checkBoxManualMovementEnabled"); QVERIFY(manual);
        manual->click(); QTRY_VERIFY(engines[0]->EngineRunning() && engines[1]->EngineRunning());
        manual->click(); QTRY_VERIFY(!engines[0]->EngineRunning() && !engines[1]->EngineRunning());
        manual->click(); QTRY_VERIFY(engines[0]->EngineRunning() && engines[1]->EngineRunning());
        // Disable publishing bindings first, then stop motion: the queued engine
        // stop is gated by movement Enable, while engine generators keep running.
        auto *movementSignals = movement->findChild<QCheckBox *>("checkBoxEnabled"); QVERIFY(movementSignals->isChecked()); movementSignals->click();
        manual->click(); QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QVERIFY(!movement->isEnabled()); QVERIFY(!manual->isChecked());
        QVERIFY(engines[0]->EngineRunning() && engines[1]->EngineRunning());
        openEditor(w); auto *d = w.findChild<EngineProfileDialog *>(); click(d, "duplicateProfile");
        d->findChild<QLineEdit *>("profileName")->setText("Preserved running draft"); const auto before = read(target);
        for (int attempt = 0; attempt < 2; ++attempt) {
            click(d, "saveCatalog"); click(d, "restoreCatalog"); click(d, "restoreBundled");
            const auto status = d->findChild<QLabel *>("catalogStatus")->text();
            QVERIFY(status.contains("engine instance 0") && status.contains("engine instance 1"));
            QVERIFY(status.contains("uncheck Start Engine")); QCOMPARE(read(target), before);
            QCOMPARE(d->findChild<QLineEdit *>("profileName")->text(), QString("Preserved running draft"));
        }
        for (auto *e : engines) { auto *start = e->findChild<QCheckBox *>("checkBoxStartEngine"); QVERIFY(start->isChecked()); start->click(); }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QVERIFY(!engines[0]->EngineRunning() && !engines[1]->EngineRunning());
        click(d, "saveCatalog"); QVERIFY(read(target) != before); QCOMPARE(d->findChild<QListWidget *>("engineProfiles")->count(), 26);
        QVERIFY(!w.findChild<QAction *>("actionStart_Sending")->isChecked());
        answer(QMessageBox::Yes); click(d, "restoreBundled"); QCOMPARE(read(target), read(":/Data/engine_profiles.json")); QVERIFY(d->close());
    }
    void dropdownsPreserveCustomOptionalAndCommonValues() {
        auto doc = original(), p = doc["engines"].toArray()[0].toObject(); p["fuelType"] = "Owner custom fuel"; p["alternatorVoltage"] = 36;
        doc = withFirst(p); write(target, QJsonDocument(doc).toJson());
        auto *d = new EngineProfileDialog([] { return QStringList(); });
        auto *fuel = d->findChild<QComboBox *>("fuelType"), *voltage = d->findChild<QComboBox *>("alternatorVoltage");
        QVERIFY(fuel && fuel->isEditable()); QVERIFY(voltage && voltage->isEditable());
        QVERIFY(fuel->findText("Gasoline") >= 0 && fuel->findText("Diesel") >= 0);
        QCOMPARE(fuel->currentText(), QString("Owner custom fuel")); QCOMPARE(voltage->currentText(), QString("36"));
        click(d, "saveCatalog"); QCOMPARE(QJsonDocument::fromJson(read(target)).object(), doc);
        for (int volts : {12, 24, 32, 48}) {
            const int idx = voltage->findData(volts); QVERIFY(idx >= 0); QCOMPARE(voltage->itemText(idx), QString("%1 VDC").arg(volts));
            voltage->setCurrentIndex(idx); fuel->setEditText("Diesel"); click(d, "saveCatalog");
            auto result = QJsonDocument::fromJson(read(target)).object()["engines"].toArray()[0].toObject(); QCOMPARE(result["alternatorVoltage"].toInt(), volts); QCOMPARE(result["fuelType"].toString(), QString("Diesel"));
        }
        voltage->setEditText("37"); click(d, "saveCatalog"); QCOMPARE(QJsonDocument::fromJson(read(target)).object()["engines"].toArray()[0].toObject()["alternatorVoltage"].toInt(), 37);
        d->findChild<QCheckBox *>("fuelTypeSpecified")->setChecked(false); d->findChild<QCheckBox *>("alternatorVoltageSpecified")->setChecked(false);
        click(d, "saveCatalog"); auto result = QJsonDocument::fromJson(read(target)).object()["engines"].toArray()[0].toObject(); QVERIFY(!result.contains("fuelType") && !result.contains("alternatorVoltage"));
        QVERIFY(d->close());
    }
    void graphUsesDraftPointsAndRejectsInvalidLines() {
        auto *d = new EngineProfileDialog([] { return QStringList(); }); auto *view = d->findChild<QChartView *>("fuelCurveGraph"); QVERIFY(view);
        QLineSeries *line = nullptr; QScatterSeries *dots = nullptr;
        for (auto *series : view->chart()->series()) { if (auto *s = qobject_cast<QLineSeries *>(series)) line = s; if (auto *s = qobject_cast<QScatterSeries *>(series)) dots = s; }
        QVERIFY(line && dots); auto *table = d->findChild<QTableWidget *>("fuelCurve"); QCOMPARE(line->count(), table->rowCount()); QCOMPARE(dots->count(), table->rowCount());
        const auto before = read(target); table->item(0, 1)->setText("0.125"); QCOMPARE(line->at(0).y(), 0.125); QCOMPARE(dots->at(0).y(), 0.125); QCOMPARE(read(target), before);
        table->item(1, 0)->setText(table->item(0, 0)->text()); QCOMPARE(line->count(), 0); QVERIFY(dots->count() > 0); QVERIFY(d->findChild<QLabel *>("fuelGraphStatus")->text().contains("invalid"));
        table->item(1, 0)->setText("-1"); QCOMPARE(line->count(), 0); QCOMPARE(dots->count(), table->rowCount() - 1);
        table->item(1, 0)->setText("1000"); table->item(1, 1)->setText("nan"); QCOMPARE(line->count(), 0); QCOMPARE(dots->count(), table->rowCount() - 1);
        table->item(1, 1)->setText("0"); QCOMPARE(line->count(), table->rowCount()); QCOMPARE(line->at(1).y(), 0.0);
        click(d, "newProfile"); QCOMPARE(line->count(), 0); QCOMPARE(dots->count(), 0);
        click(d, "addCurveRow"); table->setItem(0, 0, new QTableWidgetItem("500")); table->setItem(0, 1, new QTableWidgetItem("1"));
        QCOMPARE(line->count(), 0); QCOMPARE(dots->count(), 1); QCOMPARE(read(target), before);
        answer(QMessageBox::Discard); QVERIFY(d->close());
    }
    void backupDefaultsRememberedDestinationAndCancel() {
        auto *d = new EngineProfileDialog([] { return QStringList(); }); const auto before = read(target);
        const auto expected = home->path() + "/Documents/RFStateSide/RayNmeaSimulator/Backups/EngineProfiles";
        QTimer::singleShot(0, [expected] { for (auto *w : QApplication::topLevelWidgets()) if (auto *f = qobject_cast<QFileDialog *>(w)) {
            QCOMPARE(f->directory().absolutePath(), expected); f->reject();
        }});
        click(d, "backupCatalog"); AppSettings settings(APP_COMPANY, APP_NAME); QVERIFY(!settings.contains("EngineProfiles/BackupDirectory")); QCOMPARE(read(target), before);
        const auto selected = home->path() + "/chosen elsewhere"; QVERIFY(QDir().mkpath(selected));
        fileDialog(selected + "/profiles.json"); click(d, "backupCatalog"); QCOMPARE(read(selected + "/profiles.json"), before);
        QCOMPARE(settings.value("EngineProfiles/BackupDirectory").toString(), selected); QVERIFY(d->close()); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        d = new EngineProfileDialog([] { return QStringList(); });
        QTimer::singleShot(0, [selected] { for (auto *w : QApplication::topLevelWidgets()) if (auto *f = qobject_cast<QFileDialog *>(w)) {
            QCOMPARE(f->directory().absolutePath(), selected); f->reject();
        }});
        click(d, "backupCatalog"); QCOMPARE(read(target), before);
        for (const char *id : {"backupCatalog", "restoreCatalog", "restoreBundled", "saveCatalog"}) {
            auto *button = d->findChild<QPushButton *>(id); QVERIFY(!button->toolTip().isEmpty()); QVERIFY(!button->text().contains("Catalog", Qt::CaseInsensitive));
        }
        QVERIFY(d->close());
    }
    void explicitBackupRestoreDialogs() {
        auto *d = new EngineProfileDialog([] { return QStringList(); }); d->show(); const auto before = read(target);
        const auto backup = home->path() + "/chosen-backup.json";
        fileDialog(backup); click(d, "backupCatalog"); QCOMPARE(read(backup), before);
        d->findChild<QLineEdit *>("horsepower")->setText("999.25"); click(d, "saveCatalog"); const auto modified = read(target); QVERIFY(modified != before);
        QTimer::singleShot(0, [backup] { for (auto *w : QApplication::topLevelWidgets()) if (auto *f = qobject_cast<QFileDialog *>(w)) {
            f->selectFile(backup); static_cast<QDialog *>(f)->accept(); answer(QMessageBox::No);
        }});
        click(d, "restoreCatalog"); QCOMPARE(read(target), modified);
        QTimer::singleShot(0, [backup] { for (auto *w : QApplication::topLevelWidgets()) if (auto *f = qobject_cast<QFileDialog *>(w)) {
            f->selectFile(backup); static_cast<QDialog *>(f)->accept(); answer(QMessageBox::Yes);
        }});
        click(d, "restoreCatalog"); QCOMPARE(read(target), before);
        d->findChild<QLineEdit *>("horsepower")->setText("999.25"); click(d, "saveCatalog");
        answer(QMessageBox::Yes); click(d, "restoreBundled"); QCOMPARE(read(target), read(":/Data/engine_profiles.json"));
        QVERIFY(d->close());
    }
    void mainWindowRefreshAndSavedLayoutNames() {
        MainWindow w; auto *controller = w.findChild<MainController *>();
        controller->loadLayoutFromJson(QStringLiteral(":/defaults/defaultLayout.json"));
        auto engines = w.findChildren<GeneratorEngine *>(); QVERIFY(!engines.isEmpty());
        engines[0]->setEngineProfile("MerCruiser 7.4 MPI"); const auto chosen = engines[0]->engineProfile();
        const auto layout = home->path() + "/existing-layout.json"; QVERIFY(controller->saveLayoutToJson(layout)); const auto layoutBytes = read(layout);
        openEditor(w); auto *d = w.findChild<EngineProfileDialog *>(); click(d, "duplicateProfile");
        d->findChild<QLineEdit *>("profileName")->setText("Live copy"); click(d, "saveCatalog");
        QVERIFY(engines[0]->findChild<QComboBox *>("comboBoxVesselEngine")->findText("Live copy") >= 0); QCOMPARE(engines[0]->engineProfile(), chosen);
        auto *setup = w.findChild<DialogSetup *>(); QVERIFY(setup->findChild<QComboBox *>("comboBoxVesselEngine")->findText("Live copy") >= 0);
        QCOMPARE(read(layout), layoutBytes); QVERIFY(!w.findChild<QAction *>("actionStart_Sending")->isChecked());
        QVERIFY(d->close());
    }
    void renderedThemes() {
        for (const QString theme : {"Dark", "Light"}) {
            QApplication::setPalette(Themes::palette(theme));
            auto *d = new EngineProfileDialog([] { return QStringList(); }); d->setStyleSheet(Themes::styleSheet(theme)); d->show();
            QVERIFY(QTest::qWaitForWindowExposed(d)); QTest::qWait(30);
            const auto evidence = qEnvironmentVariable("M222_EVIDENCE");
            if (!evidence.isEmpty()) { QVERIFY(QDir().mkpath(evidence)); QVERIFY(d->grab().save(evidence + "/engine-editor-" + theme + ".png")); }
            d->findChild<QTabWidget *>("fuelCurveTabs")->setCurrentIndex(1); d->resize(1100, 780); QTest::qWait(30);
            auto *chart = d->findChild<QChartView *>("fuelCurveGraph")->chart(); QCOMPARE(chart->theme(), theme == "Dark" ? QChart::ChartThemeDark : QChart::ChartThemeLight);
            if (!evidence.isEmpty()) QVERIFY(d->grab().save(evidence + "/engine-graph-" + theme + ".png"));
            QVERIFY(d->findChild<QTableWidget *>("fuelCurve")->columnWidth(1) > 80); QVERIFY(d->close());
        }
    }
};

int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QList<QByteArray> args; for (int i = 0; i < argc; ++i) args << argv[i];
    QList<char *> pointers; for (auto &a : args) pointers << a.data(); int count = pointers.size(); pointers << nullptr;
    QApplication app(count, pointers.data()); app.setStyle("Fusion"); app.setOrganizationName(APP_COMPANY); app.setApplicationName(APP_NAME);
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    if (app.arguments().contains("--catalog-read-child")) {
        EngineProfileCatalog catalog; QString error; if (!catalog.open(&error)) return 2;
        GeneratorEngine engine(199, "Restart engine"); engine.setEngineProfile("Restart fixture");
        if (!engine.profileReady() || engine.EngineRunning()) return 3;
        fprintf(stdout, "CATALOG_RESTART_PASS\n"); return 0;
    }
    EngineProfileEditorTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "engine_profile_editor_tests.moc"
