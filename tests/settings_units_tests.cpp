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

#include "signalk_testserver.h"
#include "mainwindow.h"
#include "appsettings.h"
#include "credentialstore.h"
#ifdef Q_OS_MACOS
#include "macos_credential_test_api.h"
#endif
#include "preferences.h"
#include "coordinateeditor.h"
#include "cogsog129026.h"
#include "latlong129025.h"
#include "n2khelper.h"
#include "temperatureextended130316.h"
#include "fluidlevel127505.h"
#include "enginedynamic127489.h"
#include "speed128259.h"
#include "navigation129284.h"
#include "generatorvesselmovement.h"
#include "generatorweather.h"
#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QLineEdit>
#include <QNetworkProxy>
#include <QPushButton>
#include <QScopeGuard>
#include <cmath>

class SettingsUnitsTests : public QObject {
    Q_OBJECT
    void near(double actual, double expected, double tolerance = 1e-8)
    {
        QVERIFY2(std::abs(actual - expected) <= tolerance * qMax(1.0, std::abs(expected)), "Physical value or conversion differs");
    }
    QLineEdit* edit(QObject& object, const char* name) { return object.findChild<QLineEdit*>(name); }
private slots:
    void init()
    {
        AppSettings s(APP_COMPANY, APP_NAME);
        s.clear();
    }
    void cleanup()
    {
        CredentialStore::remove("SignalKToken");
        CredentialStore::remove("OpenWeatherMapApiKey");
        CredentialStore::remove("synthetic-test");
    }
    void conversionFamilies_data()
    {
        QTest::addColumn<QString>("family");
        for (const auto* name : { "temperature", "pressure", "atmosphere", "flow", "volume", "speed", "depth", "distanceFar",
                 "distanceNear", "economy", "turn" })
            QTest::newRow(name) << QString(name);
    }
    void conversionFamilies()
    {
        QFETCH(QString, family);
        using Fn = double (*)(double, UserUnitPrefs);
        struct Conversion {
            const char* name;
            uint8_t UserUnitPrefs::* field;
            int count;
            double canonical;
            Fn to;
            Fn from;
        };
#define CONVERSION(name, field, count, value, suffix)                                                                                      \
    {                                                                                                                                      \
        name, &UserUnitPrefs::field, count, value, N2kHelper::convertN2k##suffix##ToUserUnits, N2kHelper::convertUser##suffix##ToN2kUnits  \
    }
        const Conversion list[] = { CONVERSION("temperature", tempUnitIndex, 3, 300.0, Temperature),
            CONVERSION("pressure", pressureUnitIndex, 4, 150000.0, Pressure),
            CONVERSION("atmosphere", atmosphericPressureUnitIndex, 5, 101325.0, AtmosphericPressure),
            CONVERSION("flow", flowRateUnitIndex, 2, 42.0, FlowRate), CONVERSION("volume", volumeUnitIndex, 2, 378.5411784, Volume),
            CONVERSION("speed", speedUnitIndex, 4, 12.5, Speed), CONVERSION("depth", depthUnitIndex, 3, 12.0, Depth),
            CONVERSION("distanceFar", distanceUnitIndex, 3, 1852.0, DistanceFar),
            CONVERSION("distanceNear", distanceUnitIndex, 3, 0.3048, DistanceNear),
            CONVERSION("economy", fuelEconomyUnitIndex, 6, 0.75, FuelEconomy),
            CONVERSION("turn", rateOfTurnUnitIndex, 2, 0.15, RateOfTurn) };
#undef CONVERSION
        for (const auto& c : list)
            if (family == c.name)
                for (int index = 0; index < c.count; ++index) {
                    UserUnitPrefs prefs;
                    prefs.*(c.field) = index;
                    near(c.from(c.to(c.canonical, prefs), prefs), c.canonical);
                }
    }
    void knownPhysicalValues()
    {
        UserUnitPrefs p;
        p.distanceUnitIndex = 0;
        near(N2kHelper::convertN2kDistanceFarToUserUnits(1852, p), 1.852);
        p.distanceUnitIndex = 1;
        near(N2kHelper::convertN2kDistanceFarToUserUnits(1852, p), 1);
        near(N2kHelper::convertN2kDistanceNearToUserUnits(0.3048, p), 1);
        p.distanceUnitIndex = 2;
        near(N2kHelper::convertUserDistanceFarToN2kUnits(1, p), 1609.344);
        near(Convert::LkmToLnm(1), 1.852);
        near(Convert::LkmToUSGalNm(1), 1.852 / 3.785411784);
        near(Convert::LkmToNmL(1), 1 / 1.852);
        near(Convert::LkmToMPG(1), 3.785411784 / 1.609344);
        near(Convert::M3sToLkm(0.00001, 10), 1);
        near(Convert::LkmToM3s(1, 10), 0.00001);
        near(Convert::KnotsToMetersPerSecond(1), 1852.0 / 3600.0);
        near(Convert::GphToLph(1), 3.785411784);
        QLabel label;
        QCOMPARE(Convert::formatDistanceNearFar(12, 0, &label), QString("12"));
        QCOMPARE(label.text(), QString("m"));
    }
    void invalidPreferencesUseDefaults()
    {
        AppSettings s(APP_COMPANY, APP_NAME);
        s.beginGroup("Preference");
        s.setValue("UnitsTemp", -1);
        s.setValue("UnitsSpeed", 260);
        s.setValue("UnitsDistance", "bad");
        const auto p = readUnitPreferences(s);
        const UserUnitPrefs defaults;
        QCOMPARE(p.tempUnitIndex, defaults.tempUnitIndex);
        QCOMPARE(p.speedUnitIndex, defaults.speedUnitIndex);
        QCOMPARE(p.distanceUnitIndex, defaults.distanceUnitIndex);
    }
    void invalidVesselAndOptionSettingsUseDefaults()
    {
        AppSettings s(APP_COMPANY, APP_NAME);
        s.beginGroup("Vessel");
        s.setValue("Latitude", 91);
        s.setValue("WotSpeed", "bad");
        s.setValue("Length", -1);
        s.setValue("NumEngines", 0);
        auto vessel = readVesselSettings(s);
        near(vessel.Latitude, 42.813903);
        near(vessel.WotSpeed, MyVessel().WotSpeed);
        near(vessel.Length, MyVessel().Length);
        QCOMPARE(vessel.NumEngines, MyVessel().NumEngines);
        s.endGroup();
        s.beginGroup("Preference");
        s.setValue("WidgetColumns", -1);
        s.setValue("WidgetBorderWidth", 9);
        const auto options = readOptionPreferences(s);
        QCOMPARE(options.widgetColumns, 2);
        QCOMPARE(options.borderWidth, 1);
    }
    void coordinatesAndDateTimeFormats()
    {
        UserUnitPrefs p;
        for (int i = 0; i < 3; ++i) {
            p.latLongUnitIndex = i;
            near(N2kHelper::convertUserLatitudeToN2kUnits(N2kHelper::convertN2kLatitudeToUserUnits(-42.813903, p), p), -42.813903, 1e-5);
            near(N2kHelper::convertUserLongitudeToN2kUnits(N2kHelper::convertN2kLongitudeToUserUnits(170.867977, p), p), 170.867977, 1e-5);
            p.dateUnitIndex = i;
            QCOMPARE(N2kHelper::convertUserFormattedDateToN2k(N2kHelper::convertN2kDateToUserFormat(20000, p), p), uint16_t(20000));
        }
        for (int i = 0; i < 4; ++i) {
            p.timeUnitIndex = i;
            near(N2kHelper::convertUserFormattedTimeToN2k(N2kHelper::convertN2kTimeToUserFormat(43200, p), p), 43200);
        }
    }
    void existingWidgetsRefreshWithoutChangingCanonicalValues()
    {
        CogSog129026 course(9, "Course", nullptr);
        course.setCOG(Convert::DegreesToRadians(90));
        UserUnitPrefs coursePrefs;
        coursePrefs.speedUnitIndex = 0;
        course.setUserPrefs(coursePrefs);
        near(edit(course, "lineEditCog")->text().toDouble(), 90);
        TemperatureExtended130316 temp(1, "Temp", nullptr);
        temp.setActualTemperature(300);
        FluidLevel127505 tank(2, "Tank", nullptr);
        tank.setCapacity(378.5411784);
        EngineDynamic127489 engine(3, "Engine");
        engine.setFuelRate(37.85411784);
        engine.setEngineOilPress(150000);
        engine.setEngineOilTemp(300);
        Speed128259 speed(4, "Speed", nullptr);
        speed.setGroundReferenced(12.5);
        Navigation129284 nav(5, "Navigation", nullptr);
        nav.setDistanceToWaypoint(1852);
        GeneratorWeather weather(6, "Weather");
        weather.setAirTemperature(300);
        weather.setWindSpeed(12.5);
        QSignalSpy changed(&temp, &TemperatureExtended130316::ActualTemperatureChanged);
        UserUnitPrefs prefs;
        prefs.tempUnitIndex = 1;
        prefs.volumeUnitIndex = 0;
        prefs.flowRateUnitIndex = 0;
        prefs.speedUnitIndex = 0;
        prefs.distanceUnitIndex = 1;
        for (PgnWidgetBase* widget : QList<PgnWidgetBase*> { &temp, &tank, &engine, &speed, &nav, &weather })
            widget->setUserPrefs(prefs);
        near(edit(temp, "lineEdit")->text().toDouble(), 26.9, 0.01);
        near(edit(tank, "lineEditCapacity")->text().toDouble(), 378.5, 0.01);
        near(edit(engine, "lineEditFuelRate")->text().toDouble(), 37.9, 0.01);
        QCOMPARE(speed.findChild<QLabel*>("labelUnitsGroundReferenced")->text(), QString("m/s"));
        near(edit(speed, "lineEditGroundReferenced")->text().toDouble(), 12.5);
        near(edit(nav, "lineEditDistanceToWaypoint")->text().toDouble(), 1);
        near(edit(weather, "lineEditWindSpeed")->text().toDouble(), 12.5);
        QCOMPARE(temp.ActualTemperature(), 300.0);
        QCOMPARE(tank.Capacity(), 378.5411784);
        QCOMPARE(changed.size(), 0);
        prefs.speedUnitIndex = 3;
        speed.setUserPrefs(prefs);
        QCOMPARE(speed.findChild<QLabel*>("labelUnitsGroundReferenced")->text(), QString("knots"));
        near(speed.GroundReferenced(), 12.5);
    }
    void movementPreferencesPreserveManualSpeedAndMeasurements()
    {
        GeneratorVesselMovement movement(1, "Movement", nullptr);
        movement.setSOG(12.5);
        movement.setLatitude(42.8);
        movement.findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed")->setValue(20.0);
        const auto before = movement.layoutConfiguration();
        const auto sog = movement.SOG();
        QSignalSpy changed(&movement, &GeneratorVesselMovement::SOGChanged);
        UserUnitPrefs prefs;
        prefs.speedUnitIndex = 0;
        prefs.latLongUnitIndex = 0;
        movement.setUserPrefs(prefs);
        near(movement.SOG(), sog);
        QCOMPARE(changed.size(), 0);
        near(movement.layoutConfiguration()["speed"].toDouble(), before["speed"].toDouble(), 0.001);
        near(movement.Latitude(), 42.8);
        for (int format = 0; format < 3; ++format) {
            prefs.latLongUnitIndex = format;
            movement.setUserPrefs(prefs);
            auto* latitude = edit(movement, "lineEditLatitude");
            const QString text = N2kHelper::convertN2kLatitudeToUserUnits(-43.5, prefs);
            latitude->setText(text);
            QVERIFY(latitude->hasAcceptableInput());
            QVERIFY(QMetaObject::invokeMethod(&movement, "on_lineEditLatitude_textEdited", Q_ARG(QString, text)));
            near(movement.Latitude(), -43.5);
        }
    }
    void credentialCreateReplaceRemoveAndFailedWrite()
    {
        QString error;
        if (!CredentialStore::write("synthetic-test", "synthetic-value", &error)) {
#ifdef Q_OS_LINUX
            QVERIFY(!CredentialStore::read("synthetic-test").success);
            QVERIFY(!error.isEmpty());
            QSKIP("Desktop Secret Service unavailable; fail-closed behavior verified");
#else
            QFAIL("Synthetic secure write failed");
#endif
        }
        auto r = CredentialStore::read("synthetic-test");
        QVERIFY(r.success && r.found);
        QVERIFY(r.value == "synthetic-value");
        QVERIFY(CredentialStore::write("synthetic-test", "replacement"));
        QVERIFY(!CredentialStore::write("synthetic-test", QString(3000, 's'), &error));
        QVERIFY(!error.contains(QString(20, 's')));
        QVERIFY(CredentialStore::read("synthetic-test").value == "replacement");
        QVERIFY(!CredentialStore::write("synthetic-test", "line\nbreak", &error));
        QVERIFY(CredentialStore::remove("synthetic-test"));
        r = CredentialStore::read("synthetic-test");
        QVERIFY(r.success && !r.found);
        QVERIFY(CredentialStore::remove("synthetic-test"));
    }
#ifdef Q_OS_MACOS
    void macosBackendFailureRollsBackAndPreservesSettings()
    {
        using namespace MacCredentialTest;
        reset();
        const auto original = QStringLiteral("retained-synthetic-token");
        const auto staged = QStringLiteral("staged-synthetic-token");
        QVERIFY(CredentialStore::write("SignalKToken", original));
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Unrelated/OwnerValue", "preserve");
        settings.sync();
        MainWindow window;
        auto* dialog = window.findChild<DialogSetup*>();
        dialog->show();
        QCoreApplication::processEvents();
        auto* token = edit(*dialog, "signalKToken");
        auto* weather = edit(*dialog, "lineEditOpenWeatherMapKey");
        token->setText(staged);
        weather->setText("staged-synthetic-weather");
        QSignalSpy reload(dialog, &DialogSetup::ReloadSettings);
        failNext(Operation::Add, errSecAuthFailed, "OpenWeatherMapApiKey");
        dialog->findChild<QPushButton*>("pushButtonSave")->click();
        QCOMPARE(reload.size(), 0);
        const auto retained = CredentialStore::read("SignalKToken");
        QVERIFY(retained.success && retained.found && retained.value == original);
        QVERIFY(!CredentialStore::read("OpenWeatherMapApiKey").found);
        QVERIFY(token->text() == staged);
        const auto notice = dialog->findChild<QLabel*>("credentialStorageStatus")->text();
        QVERIFY(notice.contains("denied") && !notice.contains(original) && !notice.contains(staged));
        QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
        settings.sync();
        QVERIFY(settings.value("Unrelated/OwnerValue").toString() == "preserve");
        QVERIFY(!settings.contains("Credentials/SignalKTokenConfigured"));
        QFile file(settings.fileName());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto content = file.readAll();
        QVERIFY(!content.contains(original.toUtf8()) && !content.contains(staged.toUtf8()));
        file.close();
        // Retry after the transient failure, then Cancel discards subsequent drafts.
        dialog->findChild<QPushButton*>("pushButtonSave")->click();
        QCOMPARE(reload.size(), 1);
        QVERIFY(CredentialStore::read("SignalKToken").value == staged);
        token->setText("discarded-synthetic-token");
        dialog->reject();
        QVERIFY(CredentialStore::read("SignalKToken").value == staged);
        QVERIFY(validQueries());
    }
    void macosUnavailableReadDoesNotOverwriteCredentials()
    {
        using namespace MacCredentialTest;
        reset();
        MainWindow window;
        auto* dialog = window.findChild<DialogSetup*>();
        dialog->show();
        QCoreApplication::processEvents();
        edit(*dialog, "signalKToken")->setText("staged-synthetic-token");
        QSignalSpy reload(dialog, &DialogSetup::ReloadSettings);
        failNext(Operation::Read, errSecNotAvailable, "SignalKToken");
        dialog->findChild<QPushButton*>("pushButtonSave")->click();
        QCOMPARE(reload.size(), 0);
        QVERIFY(dialog->findChild<QLabel*>("credentialStorageStatus")->text().contains("unavailable"));
        QVERIFY(!CredentialStore::read("SignalKToken").found);
        QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
        dialog->reject();
        QVERIFY(validQueries());
    }
#endif
    void credentialSaveCancelFailureAndRestart()
    {
        QString error;
        if (!CredentialStore::write("SignalKToken", "initial-synthetic-token", &error)) {
#ifdef Q_OS_LINUX
            QSKIP("Desktop Secret Service unavailable; tested separately");
#else
            QFAIL("Secure credential fixture unavailable");
#endif
        }
        {
            MainWindow window;
            auto* dialog = window.findChild<DialogSetup*>();
            dialog->show();
            QCoreApplication::processEvents();
            auto* token = edit(*dialog, "signalKToken");
            auto* weather = edit(*dialog, "lineEditOpenWeatherMapKey");
            QVERIFY(token->text() == "initial-synthetic-token");
            token->setText("unsaved-synthetic-token");
            dialog->reject();
            dialog->show();
            QVERIFY(token->text() == "initial-synthetic-token");
            token->setText("replacement-synthetic-token");
            weather->setText(QString(3000, 's'));
            QSignalSpy reload(dialog, &DialogSetup::ReloadSettings);
            dialog->findChild<QPushButton*>("pushButtonSave")->click();
            QCOMPARE(reload.size(), 0);
            QVERIFY(CredentialStore::read("SignalKToken").value == "initial-synthetic-token");
            QCOMPARE(dialog->findChild<QLabel*>("credentialStorageStatus")->text(),
                     QString("Credential must be a single line of at most 2500 UTF-8 bytes."));
            weather->setText("synthetic-weather-key");
            dialog->findChild<QPushButton*>("pushButtonSave")->click();
            QCOMPARE(reload.size(), 1);
            AppSettings s(APP_COMPANY, APP_NAME);
            QVERIFY(!s.contains("Nmea2000/SignalKToken"));
            QVERIFY(!s.contains("APIs/OpenWeatherMapApiKey"));
            QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
            dialog->reject();
        }
        {
            MainWindow restarted;
            auto* dialog = restarted.findChild<DialogSetup*>();
            dialog->show();
            QCoreApplication::processEvents();
            QVERIFY(edit(*dialog, "signalKToken")->text() == "replacement-synthetic-token");
            QVERIFY(edit(*dialog, "lineEditOpenWeatherMapKey")->text() == "synthetic-weather-key");
            edit(*dialog, "signalKToken")->clear();
            edit(*dialog, "lineEditOpenWeatherMapKey")->clear();
            dialog->findChild<QPushButton*>("pushButtonSave")->click();
            QVERIFY(!CredentialStore::read("SignalKToken").found);
            QVERIFY(!CredentialStore::read("OpenWeatherMapApiKey").found);
            QVERIFY(!restarted.findChild<NmeaSenderWorker*>()->isSending());
            dialog->reject();
        }
    }
#ifdef Q_OS_LINUX
    void linuxSecretToolPipeContract()
    {
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        QFile tool(fixture.filePath("secret-tool"));
        QVERIFY(tool.open(QIODevice::WriteOnly));
        tool.write("#!/bin/sh\ndir=${0%/*}\ncase \"$1\" in\nstore) /bin/cat > \"$dir/value\" ;;\nlookup) if test -f \"$dir/value\"; then "
                   "/bin/cat \"$dir/value\"; else exit 1; fi ;;\nclear) /bin/rm -f \"$dir/value\" ;;\nesac\n");
        tool.close();
        QVERIFY(tool.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const auto oldPath = qgetenv("PATH");
        qputenv("PATH", fixture.path().toUtf8());
        const auto restore = qScopeGuard([oldPath] { qputenv("PATH", oldPath); });
        QVERIFY(CredentialStore::write("synthetic-test", "synthetic-no-newline"));
        QFile stored(fixture.filePath("value"));
        QVERIFY(stored.open(QIODevice::ReadOnly));
        QVERIFY(stored.readAll() == "synthetic-no-newline");
        stored.close();
        const auto result = CredentialStore::read("synthetic-test");
        QVERIFY(result.success && result.found);
        QVERIFY(result.value == "synthetic-no-newline");
        QVERIFY(CredentialStore::remove("synthetic-test"));
        const auto missing = CredentialStore::read("synthetic-test");
        QVERIFY(missing.success && !missing.found);
        QVERIFY(tool.open(QIODevice::WriteOnly | QIODevice::Truncate));
        tool.write("#!/bin/sh\necho synthetic-sensitive-error >&2\nexit 2\n");
        tool.close();
        const auto failure = CredentialStore::read("synthetic-test");
        QVERIFY(!failure.success);
        QVERIFY(!failure.error.contains("synthetic-sensitive-error"));
    }
    void unavailableStorePreservesSettingsAndStagedInput()
    {
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", "/nonexistent");
        const auto restorePath = qScopeGuard([path] { qputenv("PATH", path); });
        AppSettings s(APP_COMPANY, APP_NAME);
        s.setValue("Nmea2000/SignalKToken", "synthetic-legacy");
        s.sync();
        MainWindow window;
        auto* dialog = window.findChild<DialogSetup*>();
        dialog->show();
        QCoreApplication::processEvents();
        auto* token = edit(*dialog, "signalKToken");
        token->setText("staged-synthetic-token");
        QSignalSpy reload(dialog, &DialogSetup::ReloadSettings);
        dialog->findChild<QPushButton*>("pushButtonSave")->click();
        QCOMPARE(reload.size(), 0);
        QVERIFY(token->text() == "staged-synthetic-token");
        QVERIFY(dialog->findChild<QLabel*>("credentialStorageStatus")->text().contains("unavailable"));
        s.sync();
        QVERIFY(s.value("Nmea2000/SignalKToken").toString() == "synthetic-legacy");
        dialog->reject();
    }
#endif
    void setupUnitsSaveReloadCancelAndLegacyRecovery()
    {
        AppSettings s(APP_COMPANY, APP_NAME);
        s.setValue("Nmea2000/SignalKToken", "synthetic-legacy-ciphertext");
        s.setValue("APIs/OpenWeatherMapApiKey", "synthetic-legacy-weather");
        s.setValue("Vessel/Length", 10.668);
        s.setValue("Vessel/CruisingSpeed", 12.51712);
        s.setValue("Vessel/PortFuelTankSize", 378.5411784);
        s.setValue("Vessel/Beam", 3.6576);
        s.setValue("Preference/UnitsLatLong", 0);
        s.setValue("Vessel/Latitude", 42.813903);
        s.setValue("Unrelated/OwnerValue", "preserve");
        s.sync();
        MainWindow window;
        auto* dialog = window.findChild<DialogSetup*>();
        QVERIFY(dialog);
        dialog->show();
        QCoreApplication::processEvents();
        auto* token = edit(*dialog, "signalKToken");
        QVERIFY(token->text().isEmpty());
        QVERIFY(dialog->findChild<QLabel*>("credentialStorageStatus")->text().contains("Legacy credentials"));
        auto combo = [dialog](const char* name) { return dialog->findChild<QComboBox*>(name); };
        for (int repeat = 0; repeat < 3; ++repeat)
            for (int i = 0; i < 3; ++i) {
                combo("comboBoxUnitsSpeed")->setCurrentIndex(i);
                combo("comboBoxUnitsVolume")->setCurrentIndex(i % 2);
                combo("comboBoxUnitsDistance")->setCurrentIndex(i);
                combo("comboBoxUnitsLatLong")->setCurrentIndex(i);
            }
        dialog->findChild<QPushButton*>("pushButtonSave")->click();
        s.sync();
        near(s.value("Vessel/Length").toDouble(), 10.668);
        near(s.value("Vessel/CruisingSpeed").toDouble(), 12.51712);
        near(s.value("Vessel/PortFuelTankSize").toDouble(), 378.5411784);
        near(s.value("Vessel/Latitude").toDouble(), 42.813903);
        QVERIFY(s.value("Nmea2000/SignalKToken").toString() == "synthetic-legacy-ciphertext");
        QCOMPARE(s.value("Unrelated/OwnerValue").toString(), QString("preserve"));
        combo("comboBoxUnitsDistance")->setCurrentIndex(0);
        dialog->findChild<QDoubleSpinBox*>("doubleSpinBoxBeam")->setValue(4.25);
        dialog->findChild<QPushButton*>("pushButtonSave")->click();
        s.sync();
        near(s.value("Vessel/Beam").toDouble(), 4.25);
        combo("comboBoxUnitsDistance")->setCurrentIndex(2);
        dialog->reject();
        dialog->show();
        QCOMPARE(combo("comboBoxUnitsDistance")->currentIndex(), 0);
        dialog->findChild<QPushButton*>("pushButtonLoad")->click();
        QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
        // Saved global units apply to both current and subsequently created widgets.
        QTimer::singleShot(0, [] {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* box = qobject_cast<QMessageBox*>(widget); box && box->objectName() == "layoutOperationDialog")
                    box->button(box->standardButtons().testFlag(QMessageBox::Discard) ? QMessageBox::Discard : QMessageBox::Yes)->click();
        });
        window.findChild<QAction*>("actionLoad_Default")->trigger();
        for (auto* widget : window.findChildren<PgnWidgetBase*>())
            QCOMPARE(widget->UserPrefs().distanceUnitIndex, uint8_t(0));
        QTimer::singleShot(0, [] {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* box = qobject_cast<QMessageBox*>(widget); box && box->objectName() == "viewerEntryDialog")
                    box->findChild<QAbstractButton*>("viewerKeepExisting")->click();
        });
        window.findChild<QAction*>("actionViewer_Rx")->trigger();
        window.findChild<QAction*>("actionSimulator_Tx")->trigger();
        QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
        dialog->reject();
    }
};
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir profile(QDir::currentPath() + "/m29-profile-XXXXXX");
    if (!profile.isValid()) {
        qCritical("Cannot create isolated test profile");
        return 1;
    }
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    // MainWindow honors QApplication::arguments; no serial or CAN profiles are enabled in this test.
    SettingsUnitsTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "settings_units_tests.moc"
