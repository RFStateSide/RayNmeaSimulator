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
#include <QComboBox>
#include <QCheckBox>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkProxy>
#include <QProcess>
#include <QSlider>
#include <QTemporaryDir>
#include <memory>
#include "apppaths.h"
#include "appsettings.h"
#include "dialogsetup.h"
#include "mainwindow.h"
#include "runtimeresources.h"
#include "generatorengine.h"
#include "helper.h"
#include "convert.h"

namespace {
QByteArray read(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }
void write(const QString &path, const QByteArray &bytes) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath())); QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(bytes), bytes.size());
}
QString catalog() { return AppPaths::resourcesDirectory() + "/Data/engine_profiles.json"; }
QString qss(const QString &name) { return AppPaths::themesDirectory() + "/" + name + "/" + name + ".qss"; }
}
class EditableResourcesTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> profile;
private slots:
    void init() {
        profile = std::make_unique<QTemporaryDir>(QDir::currentPath() + "/build/m220-editable-XXXXXX");
        QVERIFY(profile->isValid()); qputenv("RAYNMEA_TEST_HOME", profile->path().toUtf8());
        qunsetenv("RAYNMEA_TEST_ENGINE_PROFILES");
    }
    void firstRunAndRepeatedInitialization() {
        MainWindow w;
        const auto folders = QDir(":/stylesheets").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QCOMPARE(folders.size(), 5);
        for (const auto &folder : folders) QVERIFY2(Themes::available().contains(folder=="Darkeum-Refined"?QString("Darkeum"):folder), qPrintable(folder));
        QDirIterator files(":/stylesheets", QDir::Files, QDirIterator::Subdirectories);
        int count = 0;
        while (files.hasNext()) {
            const auto source = files.next();
            if (source == ":/stylesheets/Common.qss" || source == ":/stylesheets/palettes.json") continue;
            const auto target = AppPaths::themesDirectory() + source.mid(QString(":/stylesheets").size());
            QCOMPARE(read(target), read(source)); QVERIFY(QFileInfo(target).isWritable()); ++count;
        }
        QCOMPARE(count, 5); QCOMPARE(read(catalog()), read(":/Data/engine_profiles.json"));
        const auto before = QFileInfo(catalog()).lastModified();
        Themes::initializeEditableResources(); QCOMPARE(RuntimeResources::engineProfiles(), catalog());
        QCOMPARE(QFileInfo(catalog()).lastModified(), before);
        QVERIFY(!QFileInfo::exists(AppPaths::layoutsDirectory()));
        QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
    void customizedFilesAndAssetsSurvive() {
        write(qss("darkeum"), "QLabel { color: #123456; }");
        const auto image = AppPaths::themesDirectory() + "/darkeum/arrow-down.png";
        write(image, "custom image bytes");
        Themes::initializeEditableResources(); Themes::initializeEditableResources();
        QCOMPARE(read(qss("darkeum")), QByteArray("QLabel { color: #123456; }"));
        QCOMPARE(read(image), QByteArray("custom image bytes"));
        QVERIFY(!QFileInfo::exists(AppPaths::themesDirectory() + "/darkeum/check.png"));
        QVERIFY(Themes::available().contains("darkeum")); // Modified external theme remains custom.
        write(catalog(), read(":/Data/engine_profiles.json") + "\n ");
        const auto bytes = read(catalog()); QCOMPARE(RuntimeResources::engineProfiles(), catalog()); QCOMPARE(read(catalog()), bytes);
    }
    void legacyPrecedenceAndCompatibility() {
        const auto legacy = AppPaths::legacyDirectory() + "/Themes/Dark/Dark.qss";
        write(legacy, "QLabel { color: #fedcba; }");
        const auto previous = AppPaths::dataDirectory() + "/themes/Light/Light.qss";
        write(previous, "QLabel { color: #abcdef; }");
        const auto engine = AppPaths::legacyDirectory() + "/Data/engine_profiles.json";
        write(engine, read(":/Data/engine_profiles.json"));
        MainWindow w; QCOMPARE(Themes::source("Dark"), legacy); QCOMPARE(Themes::source("Light"), previous);
        QVERIFY(!QFileInfo::exists(qss("Dark"))); QVERIFY(!QFileInfo::exists(qss("Light")));
        QCOMPARE(RuntimeResources::engineProfiles(), engine); QVERIFY(!QFileInfo::exists(catalog()));
        write(catalog(), read(engine)); QCOMPARE(RuntimeResources::engineProfiles(), catalog());
        write(qss("Dark"), "QLabel { color: #123456; }"); QCOMPARE(Themes::source("Dark"), qss("Dark"));
    }
    void discoveryReopenAndNativeRendering() {
        MainWindow w; auto *setup = w.findChild<DialogSetup*>(); QVERIFY(setup);
        write(qss("Owner"), "QLabel { background: #123456; color: white; }");
        setup->show(); QTest::qWait(30);
        auto *combo = setup->findChild<QComboBox*>("comboBoxAppTheme"); QVERIFY(combo); QVERIFY(combo->findText("Owner") >= 0);
        setup->reject();
        QLabel label("Editable theme preview"); label.resize(300, 70); label.setStyleSheet(Themes::styleSheet("Owner")); label.show();
        QTest::qWait(30); QCOMPARE(label.grab().toImage().pixelColor(2, 2), QColor("#123456"));
        write(qss("Owner"), "QLabel { background: #654321; color: white; }");
        label.setStyleSheet(Themes::styleSheet("Owner")); QTest::qWait(30);
        QCOMPARE(label.grab().toImage().pixelColor(2, 2), QColor("#654321"));
        const auto evidence = qEnvironmentVariable("M220_EVIDENCE");
        if (!evidence.isEmpty()) { QDir().mkpath(evidence); QVERIFY(label.grab().save(evidence + "/edited-theme.png")); }
        write(qss("NewTheme"), "QLabel { color: green; }"); setup->show(); QTest::qWait(20);
        QVERIFY(combo->findText("NewTheme") >= 0); setup->reject();
    }
    void assetsFallbackAndDarkLight() {
        Themes::initializeEditableResources();
        QVERIFY(Themes::source("darkeum").isEmpty());
        QCOMPARE(Themes::styleSheet("darkeum"),Themes::styleSheet("Dark"));
        write(qss("Owner"), "QLabel { image: url(icon.png); }");
        QVERIFY(Themes::styleSheet("Owner").contains(AppPaths::themesDirectory() + "/Owner/icon.png"));
        write(qss("Broken"), "QLabel { color: red;"); QVERIFY(!Themes::available().contains("Broken"));
        QCOMPARE(Themes::styleSheet("Broken"), Themes::styleSheet("Dark"));
        QCOMPARE(Themes::styleSheet("Missing"), Themes::styleSheet("Dark"));
        for (const auto &theme : {QString("Dark"), QString("Light")}) {
            MainWindow w; AppSettings settings(APP_COMPANY, APP_NAME); settings.setValue("Preference/StyleSheetName", theme); settings.sync();
            w.HotReloadSettings(); w.resize(1100, 750); w.show(); QTest::qWait(40);
            const auto evidence = qEnvironmentVariable("M220_EVIDENCE");
            if (!evidence.isEmpty()) { QDir().mkpath(evidence); QVERIFY(w.grab().save(evidence + "/" + theme + ".png")); }
            QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isChecked());
        }
    }
    void catalogExtensionAndEngineCurves() {
        auto doc = QJsonDocument::fromJson(read(RuntimeResources::engineProfiles())); auto engines = doc.object()["engines"].toArray();
        QVERIFY(engines.size() > 2); auto added = engines.first().toObject(); added["name"] = "User Engine"; engines.append(added);
        auto root = doc.object(); root["engines"] = engines; write(catalog(), QJsonDocument(root).toJson());
        QVERIFY(helper::getEngineList(RuntimeResources::engineProfiles()).contains("User Engine"));
        bool twelve = false, twentyFour = false;
        for (const auto &entry : engines) {
            const auto engine = entry.toObject(); const int volts = engine["alternatorVoltage"].toInt();
            if ((volts == 12 && !twelve) || (volts == 24 && !twentyFour)) {
                GeneratorEngine widget(1, "Engine"); widget.setEngineProfile(engine["name"].toString()); QVERIFY(widget.profileReady());
                const auto pairs = engine["rpm_gph_pairs"].toArray(); QVERIFY(pairs.size() >= 2);
                widget.findChild<QCheckBox*>("checkBoxEng1Rand")->setChecked(false);
                widget.setEngineRunning(true); for (auto *timer : widget.findChildren<QTimer*>()) timer->stop();
                auto *slider = widget.findChild<QSlider*>("verticalSliderThrottle"); QVERIFY(slider);
                const int rpm = (pairs[0].toObject()["rpm"].toInt() + pairs[1].toObject()["rpm"].toInt()) / 2;
                slider->setValue(rpm); for (int tick = 0; tick < 4; ++tick) QVERIFY(QMetaObject::invokeMethod(&widget, "PollTimerElapsed"));
                const auto a = pairs[0].toObject(), b = pairs[1].toObject();
                const double expected = a["gph"].toDouble() + (b["gph"].toDouble() - a["gph"].toDouble())
                    * (rpm - a["rpm"].toDouble()) / (b["rpm"].toDouble() - a["rpm"].toDouble());
                QVERIFY(qAbs(widget.FuelRate() - Convert::GphToLph(expected)) < 1e-8); widget.setEngineRunning(false);
                if (volts == 12) twelve = true; else twentyFour = true;
            }
        }
        QVERIFY(twelve); QVERIFY(twentyFour);
        MainWindow w; w.findChild<QAction*>("actionLoad_Default")->trigger();
        for (auto *engine : w.findChildren<GeneratorEngine*>()) { QVERIFY(engine->profileReady()); QVERIFY(!engine->EngineRunning()); }
    }
    void invalidCatalogIsPreserved() {
        const auto invalid = QByteArray("{broken JSON"); write(catalog(), invalid);
        QCOMPARE(RuntimeResources::engineProfiles(), catalog()); QVERIFY(helper::getEngineList(catalog()).isEmpty());
        QCOMPARE(read(catalog()), invalid);
        QVERIFY(QFile::remove(catalog())); QVERIFY(QDir().mkpath(catalog()));
        QCOMPARE(RuntimeResources::engineProfiles(), catalog()); QVERIFY(helper::getEngineList(catalog()).isEmpty()); QVERIFY(QFileInfo(catalog()).isDir());
    }
    void restartPersistence() {
        write(qss("Owner"), "QLabel { color: #123456; }");
        write(catalog(), read(":/Data/engine_profiles.json"));
        AppSettings settings(APP_COMPANY, APP_NAME); settings.setValue("Preference/StyleSheetName", "Owner"); settings.sync();
        for (int i = 0; i < 2; ++i) { QProcess child; child.start(QCoreApplication::applicationFilePath(), {"--resource-probe"});
            QVERIFY(child.waitForFinished(30000)); QCOMPARE(child.exitCode(), 0); }
    }
    void routeExamplesAndCustomization() {
        const auto custom = AppPaths::routesDirectory() + "/gpx/test_route1.gpx"; write(custom, "user route bytes");
        const auto old = AppPaths::legacyDirectory() + "/routes/kml/seabrook.kml"; write(old, "legacy route bytes");
        AppPaths::initializeRouteExamples(); AppPaths::initializeRouteExamples();
        QCOMPARE(read(custom), QByteArray("user route bytes")); QCOMPARE(read(old), QByteArray("legacy route bytes"));
        QVERIFY(!QFileInfo::exists(AppPaths::routesDirectory() + "/kml/seabrook.kml"));
        QCOMPARE(AppPaths::routeDialogDirectory(), AppPaths::routesDirectory() + "/gpx");
        const auto other = profile->path() + "/arbitrary/MyRoute.kml"; write(other, "another route");
        QCOMPARE(AppPaths::routeDialogDirectory(other), QFileInfo(other).absolutePath());
        QVERIFY(QFile::remove(custom)); AppPaths::initializeRouteExamples();
        QCOMPARE(read(custom), read(":/routes/gpx/test_route1.gpx"));
        QDirIterator files(":/routes", QDir::Files, QDirIterator::Subdirectories); int count = 0;
        while (files.hasNext()) { const auto source = files.next(); const auto relative = source.mid(QString(":/routes/").size());
            if (relative != "kml/seabrook.kml") QCOMPARE(read(AppPaths::routesDirectory() + "/" + relative), read(source));
            ++count;
        }
        QCOMPARE(count, 5);
    }
};
int main(int argc, char **argv) {
    QList<QByteArray> args; for (int i = 0; i < argc; ++i) args << argv[i]; args << "--no-nmea";
    QList<char*> pointers; for (auto &arg : args) pointers << arg.data(); int count = pointers.size(); pointers << nullptr;
    QApplication app(count, pointers.data()); QApplication::setApplicationName(APP_NAME); QApplication::setOrganizationName(APP_COMPANY);
    QApplication::setStyle("Fusion");
    if (app.arguments().contains("--resource-probe")) {
        MainWindow window;
        AppSettings settings(APP_COMPANY, APP_NAME);
        return settings.value("Preference/StyleSheetName").toString() == "Owner"
            && Themes::source("Owner").startsWith(AppPaths::themesDirectory())
            && !helper::getEngineList(RuntimeResources::engineProfiles()).isEmpty()
            && !window.findChild<QAction*>("actionStart_Sending")->isChecked() ? 0 : 1;
    }
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    EditableResourcesTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "editable_resources_tests.moc"
