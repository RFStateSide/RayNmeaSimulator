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
#include <QPushButton>
#include <QLabel>
#include <QAction>
#include <QCheckBox>
#include <QTableWidget>
#include <QNetworkProxy>
#include <QFile>
#include <QScopeGuard>
#include <QJsonDocument>
#include "applicationlog.h"
#include <vector>
#include <memory>
#include "nmeadevicediscovery.h"
#include "nmea2000diagnostics.h"
#include "nmea2000diagnosticsdialog.h"
#include "mainwindow.h"
#include "appsettings.h"
#include "NMEA2000.h"
#include "N2kMessages.h"

namespace
{
tN2kMsg claim(int address, int unique = 1, int manufacturer = 1851)
{
    tNMEA2000::tDeviceInformation d;
    d.SetName(0);
    d.SetUniqueNumber(unique);
    d.SetManufacturerCode(manufacturer);
    d.SetDeviceClass(60);
    d.SetDeviceFunction(140);
    d.SetIndustryGroup(4);
    tN2kMsg m;
    m.SetPGN(60928);
    m.Source = address;
    auto n = d.GetName();
    for (int i = 0; i < 8; ++i)
        m.AddByte(n >> (8 * i));
    return m;
}
tN2kMsg product(int address, const char *model = "Controlled model")
{
    tN2kMsg m;
    SetN2kPGN126996(m, 2100, 105, model, "v1", "hw1", "fixture-only", 0, 1);
    m.Source = address;
    return m;
}
struct Rig {
    Nmea2000Diagnostics registry;
    NmeaDeviceDiscovery discovery{&registry};
    QList<tN2kMsg> sent;
    bool available = true, accept = true;
    int opened = 0, released = 0;
    Rig()
    {
        discovery.open = [this] {
            ++opened;
            return QString();
        };
        discovery.ready = [this] { return available; };
        discovery.source = [] { return 44; };
        discovery.send = [this](const tN2kMsg &m) {
            sent << m;
            return accept;
        };
        discovery.release = [this] { ++released; };
    }
    void rx(const tN2kMsg &m)
    {
        registry.observe(m);
        discovery.ingest(m);
    }
    void start()
    {
        discovery.start();
        discovery.advance(0);
    }
};
class Backend : public SocketCanBackend
{
  public:
    std::function<void(const tN2kMsg &)> rx;
    QList<tN2kMsg> sent;
    bool responds = true;
    void configure(const SocketCanConfig &, std::function<void(const tN2kMsg &)> f) override
    {
        rx = f;
    }
    void service() override {}
    bool ready() const override
    {
        return true;
    }
    int sourceAddress() const override
    {
        return 44;
    }
    int openingError() const override
    {
        return 0;
    }
    bool close() override
    {
        return true;
    }
    bool send(const tN2kMsg &m) override
    {
        sent << m;
        if (!responds)
            return true;
        unsigned long requested = 0;
        if (!ParseN2kPGNISORequest(m, requested))
            return false;
        if (requested == 60928)
            for (int s : {1, 10, 150, 204})
                rx(claim(s, s, s == 10 ? 419 : 1851));
        if (requested == 126996)
            rx(product(m.Destination));
        return true;
    }
};
} // namespace
class DeviceDiscoveryTests : public QObject
{
    Q_OBJECT
  private slots:
    void isoSequenceAndMultipleProducts()
    {
        Rig r;
        r.start();
        QCOMPARE(r.sent.size(), 1);
        unsigned long p = 0;
        QVERIFY(ParseN2kPGNISORequest(r.sent[0], p));
        QCOMPARE(p, 60928ul);
        QCOMPARE(r.sent[0].Destination, 255);
        for (int s : {1, 10, 150, 204})
            r.rx(claim(s, s, s == 10 ? 419 : 1851));
        for (int i = 1; i <= 4; ++i) {
            r.discovery.advance(i * 250);
            QCOMPARE(r.sent.size(), i + 1);
            const auto m = r.sent.last();
            QVERIFY(ParseN2kPGNISORequest(m, p));
            QCOMPARE(p, 126996ul);
            QVERIFY(m.Destination <= 251);
            r.rx(product(m.Destination));
        }
        r.discovery.advance(12000);
        QVERIFY(!r.discovery.busy());
        QCOMPARE(r.released, 1);
        QVERIFY(r.discovery.status().contains("4 identities"));
        QVERIFY(r.discovery.status().contains("4 models"));
        QCOMPARE(r.registry.sourceObservation(10).manufacturer, 419);
        QCOMPARE(r.registry.sourceObservation(1).model, QString("Controlled model"));
        for (const auto &m : r.sent) {
            QCOMPARE(m.PGN, 59904ul);
            QCOMPARE(m.DataLen, 3);
        }
    }
    void partialAndNoResponses()
    {
        Rig r;
        r.registry.observe(product(17));
        r.start();
        r.rx(claim(1));
        r.discovery.advance(250);
        r.discovery.advance(12000);
        QCOMPARE(r.sent.size(), 2);
        QVERIFY(r.discovery.status().contains("1 observed addresses without a fresh claim"));
        QVERIFY(r.discovery.status().contains("1 identities without a product reply"));
        QVERIFY(r.registry.sourceObservation(1).model.isEmpty());
        r.discovery.start();
        r.discovery.advance(0);
        r.discovery.advance(12000);
        QVERIFY(r.discovery.status().contains("0 identities"));
        QCOMPARE(r.released, 2);
    }
    void productRequiresFreshClaim()
    {
        Rig r;
        r.start();
        r.rx(product(1));
        QVERIFY(r.discovery.status().contains("requested Address Claims"));
        r.rx(claim(1));
        QVERIFY(r.discovery.status().contains("0 Product Information observations"));
        r.rx(product(1));
        QVERIFY(r.discovery.status().contains("1 Product Information observations"));
        r.discovery.advance(250);
        QCOMPARE(r.sent.size(), 1); // Do not request metadata already observed this run.
    }
    void unavailableAndTimeout()
    {
        Rig r;
        r.discovery.open = [] { return QString("Unsupported transport fixture"); };
        r.discovery.start();
        QVERIFY(!r.discovery.busy());
        QVERIFY(r.sent.isEmpty());
        QVERIFY(r.discovery.status().contains("Unsupported"));
        r.discovery.open = [] { return QString(); };
        r.available = false;
        r.discovery.start();
        r.discovery.advance(5000);
        QVERIFY(!r.discovery.busy());
        QVERIFY(r.discovery.status().contains("5 seconds"));
    }
    void cancellationDuringNotificationDoesNotOpenOrResend()
    {
        Rig r;
        const auto connection = connect(&r.discovery, &NmeaDeviceDiscovery::changed, &r.discovery, [&] {
            if (r.discovery.busy())
                r.discovery.cancel();
        });
        r.discovery.start();
        QCOMPARE(r.opened, 0);
        QCOMPARE(r.released, 1);
        QVERIFY(r.sent.isEmpty());
        disconnect(connection);
        r.discovery.send = [&](const tN2kMsg &) {
            r.discovery.cancel();
            return true;
        };
        r.start();
        QVERIFY(!r.discovery.busy());
        QCOMPARE(r.released, 2);
        QVERIFY(r.discovery.status().contains("cancelled"));
    }
    void cancellationRepeatAndFailure()
    {
        Rig r;
        r.start();
        r.discovery.start();
        QCOMPARE(r.opened, 1);
        r.discovery.cancel();
        r.discovery.advance(1000);
        QCOMPARE(r.sent.size(), 1);
        QCOMPARE(r.released, 1);
        r.accept = false;
        r.start();
        QVERIFY(!r.discovery.busy());
        QCOMPARE(r.sent.size(), 2);
        QCOMPARE(r.released, 2);
    }
    void boundedPacingAndAddressMove()
    {
        Rig r;
        r.start();
        r.rx(claim(1));
        r.discovery.advance(249);
        QCOMPARE(r.sent.size(), 1);
        r.discovery.advance(250);
        QCOMPARE(r.sent.last().Destination, 1);
        r.rx(claim(2));
        r.discovery.advance(499);
        QCOMPARE(r.sent.size(), 2);
        r.discovery.advance(500);
        QCOMPARE(r.sent.last().Destination, 2);
        r.rx(claim(3));
        r.discovery.advance(750);
        QCOMPARE(r.sent.size(), 3); // Max two attempts per NAME.
        r.discovery.cancel();
    }
    void conflictingAddressCancels()
    {
        Rig r;
        r.start();
        r.rx(claim(1, 1));
        r.rx(claim(1, 2));
        QVERIFY(!r.discovery.busy());
        QVERIFY(r.discovery.status().contains("Conflicting NAME"));
        QCOMPARE(r.released, 1);
    }
    void reconnectAndReadinessLoss()
    {
        Rig r;
        r.start();
        r.registry.setSession("new connection", "fixture");
        r.discovery.advance(250);
        QVERIFY(!r.discovery.busy());
        QVERIFY(r.discovery.status().contains("connection changed"));
        r.start();
        r.available = false;
        r.discovery.advance(250);
        QVERIFY(!r.discovery.busy());
        QVERIFY(r.discovery.status().contains("lost readiness"));
    }
    void negativeAcknowledgement()
    {
        Rig r;
        r.start();
        r.rx(claim(1));
        r.discovery.advance(250);
        tN2kMsg ack;
        SetN2kPGN59392(ack, 1, 255, 126996);
        ack.Source = 1;
        ack.Destination = 44;
        r.rx(ack);
        r.discovery.advance(500);
        QCOMPARE(r.sent.size(), 2);
        QVERIFY(r.discovery.status().contains("1 rejected"));
    }
    void capacityIsBounded()
    {
        Rig r;
        r.start();
        for (int i = 1; i <= 64; ++i)
            r.rx(claim(i, i));
        for (int i = 1; i < 48; ++i)
            r.discovery.advance(i * 250);
        QVERIFY(r.sent.size() <= 33);
        r.discovery.advance(12000);
        QVERIFY(!r.discovery.busy());
        QVERIFY(r.discovery.status().contains("limit: 32"));
    }
    void socketIntegrationAndRouteExclusion()
    {
        Backend backend;
        Nmea2000Handler h(nullptr, nullptr, &backend);
        h.SetInterfaceType("SocketCan");
        h.setMode(Viewer);
        h.InitializeSocketCan(SocketCanConfig{"fixture", 123});
        QTRY_VERIFY(h.IsCanInterfaceOpen());
        const auto generation = h.diagnostics()->generation();
        auto *d = h.deviceDiscovery();
        d->start();
        QVERIFY(d->busy());
        QVERIFY(h.routeServiceActive());
        QVERIFY(!h.beginRouteService());
        d->advance(0);
        for (int i = 1; i <= 4; ++i)
            d->advance(i * 250);
        d->advance(12000);
        QCOMPARE(backend.sent.size(), 5);
        QVERIFY(!h.routeServiceActive());
        QCOMPARE(h.diagnostics()->generation(), generation);
        QCOMPARE(h.diagnostics()->sourceObservation(1).model, QString("Controlled model"));
        QVERIFY(h.beginRouteService());
        d->start();
        QVERIFY(!d->busy());
        QVERIFY(h.routeServiceActive());
        h.endRouteService(false);
        QVERIFY(!h.routeServiceActive());
    }
    void unsupportedHandlers_data()
    {
        QTest::addColumn<QString>("transport");
        for (const QString s : {"Actisense", "CANable SLCAN", "Waveshare USB-CAN-A", "SignalK",
                                "Network NMEA2000", "SocketCan"})
            QTest::newRow(qPrintable(s)) << s;
    }
    void unsupportedHandlers()
    {
        QFETCH(QString, transport);
        Nmea2000Handler h;
        h.SetInterfaceType(transport);
        h.deviceDiscovery()->start();
        QVERIFY(!h.deviceDiscovery()->busy());
        QVERIFY(!h.routeServiceActive());
        QVERIFY(!h.deviceDiscovery()->status().isEmpty());
    }
    void refreshAndDiscoveryUi()
    {
        Rig r;
        Nmea2000DiagnosticsDialog view(&r.registry, nullptr, &r.discovery);
        view.show();
        connect(&view, &Nmea2000DiagnosticsDialog::discoveryRequested, &r.discovery,
                &NmeaDeviceDiscovery::start);
        view.findChild<QPushButton *>("diagnosticRefresh")->click();
        QCOMPARE(r.opened, 0);
        view.findChild<QPushButton *>("diagnosticDiscover")->click();
        QCOMPARE(r.opened, 1);
        QVERIFY(r.discovery.busy());
        QVERIFY(!view.findChild<QPushButton *>("diagnosticDiscover")->isEnabled());
        auto *pause = view.findChild<QCheckBox *>("diagnosticPause");
        pause->setChecked(true);
        r.registry.observe(claim(1));
        view.refresh(false);
        QCOMPARE(view.findChild<QTableWidget *>("observedDevices")->rowCount(), 0);
        view.findChild<QPushButton *>("diagnosticCancelDiscovery")->click();
        QVERIFY(!r.discovery.busy());
        QCOMPARE(view.findChild<QTableWidget *>("observedDevices")->rowCount(), 1);
        QVERIFY(pause->isChecked());
        view.findChild<QPushButton *>("diagnosticDiscover")->click();
        view.close();
        QVERIFY(!r.discovery.busy());
    }
    void toolbarAndF12Safeguards()
    {
        Backend backend;
        MainWindow window(nullptr, nullptr, nullptr, &backend);
        auto *h = window.findChild<Nmea2000Handler *>();
        auto *worker = window.findChild<NmeaSenderWorker *>();
        h->SetInterfaceType("SocketCan");
        h->setMode(Viewer);
        h->InitializeSocketCan(SocketCanConfig{"fixture", 123});
        QTRY_VERIFY(h->IsCanInterfaceOpen());
        auto *action = window.findChild<QAction *>("actionNmea2000Diagnostics");
        action->trigger();
        auto *view = window.findChild<Nmea2000DiagnosticsDialog *>();
        QVERIFY(view);
        view->findChild<QPushButton *>("diagnosticDiscover")->click();
        QVERIFY(h->deviceDiscovery()->busy());
        QVERIFY(!worker->isSending());
        QVERIFY(!window.findChild<QAction *>("actionStart_Sending")->isEnabled());
        window.findChild<QAction *>("actionStop_Sending")->trigger();
        QVERIFY(!h->deviceDiscovery()->busy());
        QVERIFY(!worker->isSending());
        h->deviceDiscovery()->start();
        QVERIFY(h->deviceDiscovery()->busy());
        QTest::keyClick(view, Qt::Key_F12);
        QVERIFY(!h->deviceDiscovery()->busy());
        QVERIFY(!worker->isSending());
        QVERIFY(window.findChild<MainController *>()->layoutWidgets("Generate").isEmpty());
    }
    void networkEndToEnd()
    {
        const int port = qEnvironmentVariableIntValue("RAY_NETWORK_TEST_PORT");
        if (!port)
            QSKIP("Existing localhost virtual bridge fixture supplied separately");
        QFile key("build/m239b8/bridge-key-private.txt");
        QVERIFY(key.open(QIODevice::ReadOnly));
        QTemporaryDir logs(QDir::currentPath() + "/build/m243c1/traffic-XXXXXX");
        QVERIFY(logs.isValid());
        QVERIFY(ApplicationLog::initialize(logs.path()));
        const auto logCleanup = qScopeGuard([] { ApplicationLog::shutdown(); });
        NetworkCanConfig config;
        config.host = "127.0.0.1";
        config.port = port;
        config.interfaceName = "lab";
        config.token = QString::fromUtf8(key.readAll()).trimmed();
        config.allowTx = true;
        std::vector<std::unique_ptr<NetworkCanSession>> nodes;
        int number = 0;
        for (int address : {1, 10, 150, 204}) {
            auto node = std::make_unique<NetworkCanSession>();
            node->setRawReceptionLogging(false); // Synthetic producers are not the application RX recorder.
            auto c = config;
            c.uniqueNumber = 5001 + number++;
            c.preferredSource = address;
            node->configure(c, "tx");
            QTRY_VERIFY_WITH_TIMEOUT(node->transmitting(), 6000);
            nodes.push_back(std::move(node));
        }
        Nmea2000Handler h;
        h.SetInterfaceType("Network NMEA2000");
        h.setMode(Viewer);
        config.uniqueNumber = 9001;
        config.preferredSource = 45;
        h.networkCan()->configure(config);
        QTRY_VERIFY(h.networkCan()->connected());
        const auto epoch = h.diagnostics()->generation();
        for (auto &node : nodes) {
            tN2kMsg m;
            SetN2kXTE(m, 1, N2kxtem_Autonomous, false, 0);
            QVERIFY(node->send(m));
        }
        QTRY_VERIFY(!h.diagnostics()->sourceObservation(204).key.isEmpty());
        auto *discovery = h.deviceDiscovery();
        discovery->start();
        QVERIFY(discovery->busy());
        QTRY_VERIFY_WITH_TIMEOUT(discovery->status().contains("identities"), 6000);
        QVERIFY(!ApplicationLog::recording()); // Discovery did not enable Traffic Recording.
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        tN2kMsg marker;
        SetN2kXTE(marker, 42, N2kxtem_Autonomous, false, 17.23);
        QSignalSpy received(&h, &Nmea2000Handler::RcvdPgn);
        QVERIFY(nodes.front()->send(marker));
        QTRY_VERIFY(!received.isEmpty());
        QTest::qWait(100);
        QVERIFY(ApplicationLog::flush());
        QFile traffic(ApplicationLog::filePath(true));
        QVERIFY(traffic.open(QIODevice::ReadOnly));
        int copies = 0;
        const auto payload = QByteArray(reinterpret_cast<const char *>(marker.Data), marker.DataLen).toHex();
        for (const auto &line : traffic.readAll().split('\n')) {
            const auto row = QJsonDocument::fromJson(line).object();
            if (row["kind"] == "can-frame" && row["payload_hex"].toString().toLatin1() == payload &&
                (row["id"].toString().toUInt(nullptr, 16) & 255) == 1)
                ++copies;
        }
        QCOMPARE(copies, 1);
        traffic.close();
        ApplicationLog::stopTraffic();
        QTRY_VERIFY_WITH_TIMEOUT(!discovery->busy(), 20000);
        QVERIFY2(discovery->status().contains("4 models"), qPrintable(discovery->status()));
        QVERIFY(h.networkCan()->connected());
        QVERIFY(!h.networkCan()->transmitting());
        QCOMPARE(h.diagnostics()->generation(), epoch);
        for (int address : {1, 10, 150, 204}) {
            auto observed = h.diagnostics()->sourceObservation(address);
            QVERIFY(observed.hasName);
            QCOMPARE(observed.manufacturer, 2046);
            QCOMPARE(observed.model, QString(APP_NAME));
        }
        // RX requires no TX proof: an incorrect management key must fail only the
        // temporary discovery connection, preserving the original RX connection.
        config.token = "incorrect-fixture-key";
        h.networkCan()->configure(config);
        QTRY_VERIFY(h.networkCan()->connected());
        const auto retryEpoch = h.diagnostics()->generation();
        discovery->start();
        QTRY_VERIFY_WITH_TIMEOUT(!discovery->busy(), 6000);
        QVERIFY(discovery->status().contains("failed", Qt::CaseInsensitive));
        QVERIFY(h.networkCan()->connected());
        QCOMPARE(h.diagnostics()->generation(), retryEpoch);
        QVERIFY(!h.routeServiceActive());
    }
};
int main(int argc, char **argv)
{
    QDir().mkpath(QDir::currentPath() + "/build/m243c1");
    QTemporaryDir profile(QDir::currentPath() + "/build/m243c1/profile-XXXXXX");
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    DeviceDiscoveryTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "device_discovery_tests.moc"
