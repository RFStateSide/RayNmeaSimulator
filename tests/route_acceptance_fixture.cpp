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

// Disposable owner acceptance UI. This file is never linked into the application.
#include <QtTest>
#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QNetworkProxy>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QVBoxLayout>
#include <qt_windows.h>
#include <stdexcept>
#include "appsettings.h"
#include "apppaths.h"
#include "credentialstore.h"
#include "mainwindow.h"
#include "expansioninstruments.h"

// An in-memory backend: even test-namespaced OS credentials are never accessed.
namespace
{
QMap<QString, QString> credentials;
}
CredentialStore::Result CredentialStore::read(const QString &name)
{
    return {true, credentials.contains(name), credentials.value(name), {}};
}
bool CredentialStore::write(const QString &name, const QString &value, QString *error)
{
    if (error)
        error->clear();
    credentials[name] = value;
    return true;
}
bool CredentialStore::remove(const QString &name, QString *error)
{
    if (error)
        error->clear();
    credentials.remove(name);
    return true;
}
QString CredentialStore::backendName()
{
    return "Disposable acceptance memory";
}

namespace
{
// Same process-local HKCU isolation used by route_import_ui_tests, including
// Qt's native FileDialog preferences which AppSettings alone does not redirect.
class IsolatedUserRegistry
{
    QString path = QString("Software\\RayNmeaRouteAcceptance-%1").arg(GetCurrentProcessId());
    HKEY hive = nullptr;

  public:
    IsolatedUserRegistry()
    {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(path.utf16()), 0, nullptr, REG_OPTION_VOLATILE,
                            KEY_ALL_ACCESS, nullptr, &hive, nullptr) != ERROR_SUCCESS ||
            RegOverridePredefKey(HKEY_CURRENT_USER, hive) != ERROR_SUCCESS)
            qFatal("Cannot isolate acceptance registry");
    }
    ~IsolatedUserRegistry()
    {
        if (RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) != ERROR_SUCCESS)
            qFatal("Cannot restore acceptance registry mapping");
        RegCloseKey(hive);
        if (RegDeleteTreeW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(path.utf16())) != ERROR_SUCCESS)
            qFatal("Cannot remove disposable acceptance registry");
    }
};
// Existing expansion_tests in-memory serial pattern; no physical port is opened.
class MemoryPort final : public QSerialPort
{
    QByteArray input;

  public:
    qint64 bytesWritten = 0;
    explicit MemoryPort(QObject *parent = nullptr) : QSerialPort(parent)
    {
    }
    bool open(OpenMode mode) override
    {
        return QIODevice::open(mode | QIODevice::Unbuffered);
    }
    void close() override
    {
        QIODevice::close();
    }
    qint64 bytesAvailable() const override
    {
        return input.size() + QIODevice::bytesAvailable();
    }
    qint64 bytesToWrite() const override
    {
        return 0;
    }
    void feed(const QByteArray &bytes)
    {
        input += bytes;
        emit readyRead();
    }

  protected:
    qint64 readData(char *data, qint64 size) override
    {
        const auto count = qMin(size, qint64(input.size()));
        memcpy(data, input.constData(), count);
        input.remove(0, count);
        return count;
    }
    qint64 writeData(const char *, qint64 size) override
    {
        bytesWritten += size;
        return size;
    }
};
QByteArray sentence(const QByteArray &body)
{
    unsigned char checksum = 0;
    for (const auto c : body)
        checksum ^= static_cast<unsigned char>(c);
    return "$" + body + "*" + QByteArray::number(checksum, 16).rightJustified(2, '0').toUpper() + "\r\n";
}
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

int main(int argc, char **argv)
{
    IsolatedUserRegistry registry;
    QTemporaryDir profile(QDir::tempPath() + "/RayNmea-M239-route-XXXXXX");
    require(profile.isValid(), "Cannot create disposable profile");
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QStandardPaths::setTestModeEnabled(true);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    const bool smoke = app.arguments().contains("--smoke");
    try
    {
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Nmea2000/Nmea2000Enabled", false);
        settings.setValue("Nmea2000/InterfaceType", "None");
        settings.setValue("Nmea0183/Nmea0183Enabled", true);
        settings.setValue("Nmea0183/Nmea0183Port", "MEMORY-ACCEPTANCE");
        settings.setValue("Vessel/NumEngines", 1);
        settings.sync();
        auto *serial = new MemoryPort;
        MainWindow window(nullptr, new MemoryPort, serial, nullptr,
                          [](QObject *parent) { return new MemoryPort(parent); });
        window.setWindowTitle("M2.39 DISPOSABLE ROUTE ACCEPTANCE - memory ports only");
        window.resize(1400, 900);
        window.show();
        auto *controller = window.findChild<MainController *>();
        auto *worker = window.findChild<NmeaSenderWorker *>();
        auto *model = window.findChild<PgnTableModel *>();
        require(controller && worker && model, "Missing existing application infrastructure");
        // Only startup prompts are automated: decline optional engine sources and
        // retain the existing VesselMovement control when entering Viewer.
        QTimer startupDialogs;
        QObject::connect(&startupDialogs, &QTimer::timeout, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box)
                return;
            if (box->objectName() == "viewerEntryDialog")
                box->findChild<QAbstractButton *>("viewerKeepExisting")->click();
            else if (auto *no = box->button(QMessageBox::No))
                no->click();
        });
        startupDialogs.start(10);
        controller->addGeneratorWidget("Vessel Movement (PGN -129025)");
        auto *movement = window.findChild<GeneratorVesselMovement *>();
        require(movement, "Existing VesselMovement control missing");
        require(window.findChildren<GeneratorEngine *>().isEmpty(), "Unexpected engine generator");
        require(window.findChildren<GeneratorTransLowerUnit *>().isEmpty(), "Unexpected transmission generator");
        window.findChild<QAction *>("actionViewer_Rx")->setChecked(true);
        startupDialogs.stop();
        require(window.currentMode() == Viewer, "Viewer entry failed");
        auto *pilot = movement->findChild<AutoPilotSimulator *>();
        auto *manual = movement->findChild<QCheckBox *>("checkBoxManualMovementEnabled");
        auto *enabled = movement->findChild<QCheckBox *>("checkBoxAutoPilotEnabled");
        require(pilot && manual && enabled, "Missing existing navigation state");
        const auto latitude = movement->Latitude(), longitude = movement->Longitude();
        QSignalSpy navigation(movement, &GeneratorVesselMovement::UseGenVessel);
        QSignalSpy running(pilot, &AutoPilotSimulator::RunningStateChanged);
        QSignalSpy loaded(pilot, &AutoPilotSimulator::routeLoaded);
        QSignalSpy transmitted(worker, &NmeaSenderWorker::sendSnt);
        // Actual readyRead -> Viewer framing/checksum/parser -> RcvdSnt ->
        // NmeaReceiver -> dynamic widget discovery and ReceivedRoutes. No cache injection.
        const QByteArray incoming = sentence("GPWPL,4200.0000000,N,07000.0000000,W,DEMO-A") +
                                    sentence("GPWPL,4200.6000000,N,07000.0000000,W,DEMO-B") +
                                    sentence("GPWPL,4200.6000000,N,07000.6000000,W,DEMO-C") +
                                    sentence("GPRTE,2,2,c,M239-DEMO,DEMO-C") +
                                    sentence("GPRTE,2,1,c,M239-DEMO,DEMO-A,DEMO-B");
        auto broadcast = [&] {
            if (window.currentMode() == Viewer)
                serial->feed(incoming);
        };
        broadcast();
        QTest::qWait(100);
        const auto routes = controller->receivedRoutes.collection();
        require(routes.size() == 1 && routes.first().complete && routes.first().waypoints.size() == 3,
                "Viewer did not assemble complete fixture route");
        require(routes.first().waypoints[0].name == "DEMO-A" && routes.first().waypoints[1].name == "DEMO-B" &&
                    routes.first().waypoints[2].name == "DEMO-C",
                "Route waypoint order is incorrect");
        require(model->getPgnList().size() == 6, "Expected one existing movement control, 3 WPL and 2 RTE widgets");
        const auto activated = [](const QSignalSpy &events) {
            for (const auto &arguments : events)
                if (!arguments.isEmpty() && arguments.first().toBool())
                    return true;
            return false;
        };
        auto safe = [&] {
            return !pilot->isRunning() && !manual->isChecked() && !enabled->isChecked() && !worker->isSending() &&
                   !movement->StartEngineEnabled() && !activated(navigation) && !activated(running) &&
                   loaded.isEmpty() && transmitted.isEmpty() && serial->bytesWritten == 0 &&
                   movement->Latitude() == latitude && movement->Longitude() == longitude;
        };
        require(safe(), "Reception activated navigation or Sending");
        require(AppPaths::ensureDirectory(AppPaths::routesDirectory() + "/gpx"), "Cannot prepare private route folder");
        const auto routeFile = AppPaths::routesDirectory() + "/gpx/M239-disposable-route.gpx";
        QFile file(routeFile);
        require(file.open(QIODevice::WriteOnly), "Cannot write private route fixture");
        file.write("<gpx version='1.1'><rte><name>Disposable file route</name><rtept lat='42' lon='-70'/>"
                   "<rtept lat='42.01' lon='-70.01'/></rte></gpx>");
        file.close();
        auto *dock = new QDockWidget("Disposable owner acceptance", &window);
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        auto *panel = new QWidget(dock);
        auto *layout = new QVBoxLayout(panel);
        auto *instructions =
            new QLabel("1. Viewer has received M239-DEMO: GP, 3 ordered waypoints (DEMO-A/B/C).\n"
                       "2. Click 'Generate / show VesselMovement', then its existing Select Route button.\n"
                       "3. Check both Select Route File and Select Chartplotter Route; select M239-DEMO.\n"
                       "4. The monitor must stay PASS. Do not enable Autopilot, Manual Movement or Sending.\n"
                       "Received candidates expire 30 seconds after leaving Viewer. Return to Viewer, Keep Existing,\n"
                       "then Generate to refresh. The chosen snapshot survives cache expiry.\n"
                       "Use the disposable GPX in the file chooser's initial route directory. Close when finished.",
                       panel);
        instructions->setWordWrap(true);
        layout->addWidget(instructions);
        auto *showMovement = new QPushButton("Generate / show VesselMovement", panel);
        layout->addWidget(showMovement);
        QObject::connect(showMovement, &QPushButton::clicked, &window, [&] {
            window.findChild<QAction *>("actionSimulator_Tx")->setChecked(true);
            movement->findChild<QTabWidget *>("tabWidget")
                ->setCurrentWidget(movement->findChild<QWidget *>("tabAutopilot"));
            for (auto *sub : window.findChildren<QMdiSubWindow *>())
                if (sub->isAncestorOf(movement))
                    window.findChild<QMdiArea *>()->setActiveSubWindow(sub);
        });
        auto *monitor = new QLabel(panel);
        monitor->setWordWrap(true);
        layout->addWidget(monitor);
        dock->setWidget(panel);
        window.addDockWidget(Qt::TopDockWidgetArea, dock);
        QTimer freshness;
        QObject::connect(&freshness, &QTimer::timeout, &window, broadcast);
        freshness.start(5000);
        bool failed = false;
        QTimer observation;
        const auto update = [&] {
            failed |= !safe();
            int complete = 0;
            for (const auto &route : controller->receivedRoutes.collection())
                if (route.complete)
                    ++complete;
            monitor->setText(QString("%1 | Mode: %2 | Complete received routes: %3 | Autopilot: %4 | Manual: %5 | "
                                     "Sending: %6 | Position: %7\n"
                                     "Profile: %8 (temporary; removed on exit). No physical ports or OS credentials.")
                                 .arg(failed ? "FAIL - activation/state change observed"
                                             : "PASS - selection has not activated anything",
                                      window.currentMode() == Viewer ? "Viewer" : "Generate")
                                 .arg(complete)
                                 .arg(pilot->isRunning() ? "ON" : "OFF", manual->isChecked() ? "ON" : "OFF",
                                      worker->isSending() ? "ON" : "OFF",
                                      movement->Latitude() == latitude && movement->Longitude() == longitude
                                          ? "unchanged"
                                          : "CHANGED",
                                      profile.path()));
        };
        QObject::connect(&observation, &QTimer::timeout, &window, update);
        observation.start(100);
        update();
        if (smoke)
        {
            // Exercise both real choices; selection still must not load/start navigation.
            showMovement->click();
            auto choose = [&](const QString &operation, bool fileChoice) {
                bool firstSeen = false, secondSeen = false;
                QTimer::singleShot(0, [&] {
                    auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                    if (!dialog)
                        return;
                    firstSeen = true;
                    dialog->setTextValue(operation);
                    dialog->accept();
                    QTimer::singleShot(0, [&] {
                        if (fileChoice)
                        {
                            if (auto *picker = qobject_cast<QFileDialog *>(QApplication::activeModalWidget()))
                            {
                                secondSeen = true;
                                picker->selectFile(routeFile);
                                QMetaObject::invokeMethod(picker, "accept", Qt::DirectConnection);
                            }
                        }
                        else if (auto *picker = QApplication::activeModalWidget();
                                 picker && picker->objectName() == "chartplotterRouteDialog")
                        {
                            auto *routes = picker->findChild<QListWidget *>("routeServiceRoutes");
                            auto *select = picker->findChild<QPushButton *>("routeServiceSelect");
                            if (routes && routes->count() == 1 && select)
                            {
                                secondSeen = true;
                                routes->setCurrentRow(0);
                                select->click(); // Passive snapshot only; no Discover/Refresh/Retrieve.
                            }
                        }
                    });
                });
                QMetaObject::invokeMethod(movement, "on_pushButtonSelectRouteFile_clicked");
                require(firstSeen && secondSeen, "Route picker choices missing");
                require(safe(), "Route selection activated navigation or Sending");
            };
            choose("Select Route File", true);
            choose("Select Chartplotter Route", false);
            require(!movement->findChild<QLabel *>("receivedRouteStatus")->isHidden(),
                    "Received snapshot not selected");
            require(loaded.isEmpty(), "Selection loaded active navigation");
            update();
            const auto evidence = qEnvironmentVariable("M239_ACCEPTANCE_EVIDENCE");
            if (!evidence.isEmpty())
                window.grab().save(evidence);
            qInfo("PASS: Viewer wire reception/discovery, complete ordered route, file/chartplotter selection, no "
                  "load/Autopilot/movement/Sending/TX");
            return 0; // Destruction without normal Layout-close prompts; temporary profile/registry cleaned.
        }
        return app.exec();
    }
    catch (const std::exception &error)
    {
        qCritical() << "FAIL disposable acceptance:" << error.what();
        return 1;
    }
}
