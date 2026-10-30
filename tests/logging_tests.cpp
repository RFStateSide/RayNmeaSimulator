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

// Qt headers precede the pinned Arduino compatibility F macro.
#include "N2kMessages.h"
#include "applicationlog.h"
#include "appsettings.h"
#include "layoutdocument.h"
#include "logviewer.h"
#include "maincontroller.h"
#include "mainwindow.h"
#include "nmeareceiver.h"
#include "wavesharecodec.h"
#include "wavesharesession.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLockFile>
#include <QNetworkProxy>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QPushButton>
#include <thread>
#include <algorithm>
#include <QSysInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QScopeGuard>
#include "networkcansession.h"
#ifdef Q_OS_LINUX
#include <linux/can.h>
#endif
namespace
{
QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
QList<QJsonObject> records(bool traffic)
{
    QList<QJsonObject> result;
    for (const auto &line : readFile(ApplicationLog::filePath(traffic)).split('\n')) {
        const auto o = QJsonDocument::fromJson(line).object();
        if (!o.isEmpty())
            result << o;
    }
    return result;
}
bool hasEvent(const QList<QJsonObject> &list, const QString &event)
{
    for (const auto &o : list)
        if (o["event"] == event)
            return true;
    return false;
}
QByteArray sentence(const QByteArray &body)
{
    unsigned char check = 0;
    for (auto c : body)
        check ^= static_cast<unsigned char>(c);
    return "$" + body + "*" + QByteArray::number(check, 16).rightJustified(2, '0').toUpper() + "\r\n";
}
class MemoryPort : public QSerialPort
{
  public:
    QByteArray input, output;
    bool partial = false;
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
        return input.size();
    }
    qint64 bytesToWrite() const override
    {
        return 0;
    }
    void feed(const QByteArray &b)
    {
        input += b;
        emit readyRead();
    }

  protected:
    qint64 readData(char *data, qint64 n) override
    {
        n = qMin(n, qint64(input.size()));
        memcpy(data, input.constData(), n);
        input.remove(0, n);
        return n;
    }
    qint64 writeData(const char *data, qint64 n) override
    {
        if (partial)
            n = qMin<qint64>(3, n);
        output.append(data, n);
        QTimer::singleShot(0, this, [this, n] { emit bytesWritten(n); });
        return n;
    }
};
struct Workspace {
    QMdiArea area;
    QTableView table;
    QTreeView tree;
    PgnTableController tables{&table};
    PgnTreeController trees{&tree};
    MainController controller{&tables, &trees, &area, nullptr};
    Workspace()
    {
        controller.setMyVessel(MyVessel{});
        controller.setUserPrefs(UserUnitPrefs{});
        controller.setUserOptionPrefs(UserOptionPrefs{});
        for (const QString group : {"Generate", "Navigation", "Propulsion", "Environment", "Tanks",
                                    "Electrical", "Other", "Communication"})
            controller.addSubWindowForGroup(group, group);
    }
};
QJsonObject clockLayout()
{
    QJsonObject root;
    QString error;
    LayoutDocument::read(":/defaults/defaultLayout.json", root, error);
    QJsonArray groups;
    for (const auto &entry : root["layouts"].toArray()) {
        auto group = entry.toObject();
        QJsonArray widgets;
        for (const auto &w : group["widgets"].toArray())
            if (w.toObject()["type"] == "GeneratorDateTime" || w.toObject()["type"] == "DateTime129033")
                widgets << w;
        if (!widgets.isEmpty()) {
            group["widgets"] = widgets;
            groups << group;
        }
    }
    root["layouts"] = groups;
    return root;
}
} // namespace
class LoggingTests : public QObject
{
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> directory;
  private slots:
    void init()
    {
        if (QByteArray(QTest::currentTestFunction()) == "concurrentSustainedTraffic")
            qputenv("RAYNMEA_LOG_TEST_QUEUE_LIMIT", "4096");
        else
            qunsetenv("RAYNMEA_LOG_TEST_QUEUE_LIMIT");
        AppSettings s("RFStateSide", "RayNmeaSimulator");
        s.clear();
        directory = std::make_unique<QTemporaryDir>(QDir::currentPath() + "/build/m242b/case-XXXXXX");
        QVERIFY(directory->isValid());
        QVERIFY(ApplicationLog::initialize(directory->path() + "/logs"));
        QVERIFY(ApplicationLog::flush());
    }
    void cleanup()
    {
        ApplicationLog::shutdown();
        directory.reset();
    }
    void defaultsAndQtHandler()
    {
        QCOMPARE(ApplicationLog::options().level, 1);
        QVERIFY(!ApplicationLog::bindingEnabled());
        QVERIFY(!ApplicationLog::recording());
        QVERIFY(!QFileInfo::exists(ApplicationLog::filePath(true)));
        qInfo("Qt-handler-marker");
        qDebug("hidden-debug-marker");
        QVERIFY(ApplicationLog::flush());
        auto bytes = readFile(ApplicationLog::filePath(false));
        QVERIFY(bytes.contains("Qt-handler-marker"));
        QVERIFY(!bytes.contains("hidden-debug-marker"));
        QVERIFY(ApplicationLog::initialize());
    }
    void severityCategoriesAndIndependentBindings()
    {
        auto o = ApplicationLog::options();
        o.level = 2;
        o.disabledCategories = {"quiet"};
        ApplicationLog::configure(o);
        ApplicationLog::event("test", "hidden-info");
        ApplicationLog::event("test", "visible-warning", QtWarningMsg);
        ApplicationLog::event("quiet.test", "hidden-category", QtWarningMsg);
        ApplicationLog::binding({{"event", "hidden-binding"}});
        o.bindings = true;
        o.level = 3;
        ApplicationLog::configure(o);
        ApplicationLog::binding({{"event", "visible-binding"}});
        ApplicationLog::event("test", "visible-critical", QtCriticalMsg);
        QVERIFY(ApplicationLog::flush());
        const auto bytes = readFile(ApplicationLog::filePath(false));
        QVERIFY(!bytes.contains("hidden-"));
        QVERIFY(bytes.contains("visible-warning"));
        QVERIFY(bytes.contains("visible-critical"));
        QVERIFY(bytes.contains("visible-binding"));
    }
    void interruptedTailRecovery()
    {
        const auto path = ApplicationLog::filePath(false);
        ApplicationLog::shutdown();
        QFile damaged(path);
        QVERIFY(damaged.open(QIODevice::WriteOnly | QIODevice::Append));
        damaged.write("{\"interrupted\"");
        damaged.close();
        QVERIFY(ApplicationLog::initialize(QFileInfo(path).absolutePath()));
        ApplicationLog::event("test", "record-after-interrupted-tail");
        QVERIFY(ApplicationLog::flush());
        bool found = false;
        for (const auto &record : records(false))
            if (record["message"] == "record-after-interrupted-tail")
                found = true;
        QVERIFY(found);
    }
    void privacyAndRecordBoundaries()
    {
        ApplicationLog::registerSecret("fixture-secret-value");
        ApplicationLog::event(
            "test",
            "key=fixture-secret-value https://fixture.invalid/path?appid=fixture-secret-value\nforged line");
        qWarning() << "Invalid NMEA0183 message:"
                   << "$GPGGA,private-position*00";
        ApplicationLog::event("test", "Authorization: Bearer unregistered-secret");
        ApplicationLog::event("test", "{\"token\": \"json-secret\", \"password\": \"other-secret\"}");
        QVERIFY(ApplicationLog::flush());
        const auto bytes = readFile(ApplicationLog::filePath(false));
        QVERIFY(!bytes.contains("fixture-secret-value"));
        QVERIFY(!bytes.contains("private-position"));
        QVERIFY(!bytes.contains("appid="));
        QVERIFY(!bytes.contains("unregistered-secret"));
        QVERIFY(!bytes.contains("json-secret"));
        QVERIFY(!bytes.contains("other-secret"));
        for (auto line : bytes.split('\n'))
            if (!line.isEmpty())
                QVERIFY(QJsonDocument::fromJson(line).isObject());
    }
    void rotationRetentionAndClear()
    {
        auto o = ApplicationLog::options();
        o.applicationMiB = 1;
        ApplicationLog::configure(o);
        LogViewer concurrentViewer;
        concurrentViewer.show();
        for (int batch = 0; batch < 80; ++batch) {
            for (int i = 0; i < 20; ++i)
                ApplicationLog::event("rotation", QString::number(batch * 20 + i) + QString(4000, 'x'));
            concurrentViewer.refresh();
            QVERIFY(ApplicationLog::flush());
            QVERIFY(!ApplicationLog::health(false).startsWith("Failed"));
        }
        const auto path = ApplicationLog::filePath(false);
        const auto names =
            QDir(QFileInfo(path).absolutePath()).entryList({"application.jsonl*"}, QDir::Files);
        int files = 0;
        for (const auto &name : names)
            if (!name.endsWith(".lock")) {
                ++files;
                QVERIFY(QFileInfo(QFileInfo(path).absolutePath() + "/" + name).size() <= 1024 * 1024);
            }
        QCOMPARE(files, 5);
        QVERIFY(ApplicationLog::written(false) >= 1600);
        ApplicationLog::clear(false);
        QVERIFY(ApplicationLog::flush());
        QCOMPARE(readFile(path).size(), 0);
        QVERIFY(QFileInfo::exists(path + ".1"));
        ApplicationLog::event("test", "export-marker");
        QVERIFY(ApplicationLog::flush());
        const auto dest = directory->path() + "/export.jsonl";
        ApplicationLog::exportCurrent(false, dest);
        QVERIFY(ApplicationLog::flush());
        QVERIFY(readFile(dest).contains("export-marker"));
    }
    void unavailableStorageAndLockRecovery()
    {
        QLockFile locked(ApplicationLog::filePath(true) + ".lock");
        QVERIFY(locked.tryLock());
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY_WITH_TIMEOUT(ApplicationLog::health(true).startsWith("Failed"), 3000);
        QVERIFY(!ApplicationLog::recording());
        ApplicationLog::event("test", "app-remains-usable");
        QVERIFY(ApplicationLog::flush());
        QVERIFY(readFile(ApplicationLog::filePath(false)).contains("app-remains-usable"));
        locked.unlock();
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        ApplicationLog::stopTraffic();
        QVERIFY(ApplicationLog::flush());
        QVERIFY(!ApplicationLog::recording());
    }
    void missingStorageRecoveryAndRotationFailure()
    {
        ApplicationLog::shutdown();
        const auto obstacle = directory->path() + "/obstacle";
        QFile blocked(obstacle);
        QVERIFY(blocked.open(QIODevice::WriteOnly));
        blocked.write("fixture");
        blocked.close();
        QVERIFY(ApplicationLog::initialize(obstacle + "/logs"));
        QTRY_VERIFY(ApplicationLog::health(false).startsWith("Failed"));
        ApplicationLog::clear(false);
        QTRY_VERIFY(!ApplicationLog::clearing(false));
        QVERIFY(ApplicationLog::health(false).startsWith("Failed"));
        QVERIFY(QFile::remove(obstacle));
        QVERIFY(QDir().mkpath(obstacle));
        ApplicationLog::retryApplication();
        QTRY_COMPARE(ApplicationLog::health(false), QString("Recording"));
        auto o = ApplicationLog::options();
        o.applicationMiB = 1;
        ApplicationLog::configure(o);
        const auto path = ApplicationLog::filePath(false);
        QVERIFY(QDir().mkpath(path + ".4"));
        for (int batch = 0; batch < 30 && !ApplicationLog::health(false).startsWith("Failed"); ++batch) {
            for (int i = 0; i < 20; ++i)
                ApplicationLog::event("rotation-failure",
                                      QString::number(batch * 20 + i) + QString(4000, 'x'));
            QVERIFY(ApplicationLog::flush());
        }
        QVERIFY(ApplicationLog::health(false).startsWith("Failed"));
        QVERIFY(QFileInfo(path).size() <= 1024 * 1024);
        QVERIFY(QDir().rmdir(path + ".4"));
        ApplicationLog::retryApplication();
        QTRY_COMPARE(ApplicationLog::health(false), QString("Recording"));
        ApplicationLog::event("test", "recovered-after-rotation-failure");
        QVERIFY(ApplicationLog::flush());
        QVERIFY(readFile(path).contains("recovered-after-rotation-failure"));
    }
    void logicalRxTxAndSentenceFraming()
    {
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        Nmea2000Handler handler;
        handler.setMode(Viewer);
        tN2kMsg m;
        SetN2kLatLonRapid(m, 1.25, -2.5);
        m.Source = 23;
        handler.nmea2000MessageReceived(m);
        ApplicationLog::n2k("TX", "fixture", m, "software-accepted");
        auto *port = new MemoryPort;
        Nmea0183Handler serial(nullptr, port);
        QVERIFY(serial.Initialize("memory"));
        QVERIFY(serial.OpenDevice());
        serial.setMode(Viewer);
        const auto frame = sentence("GPZDA,120000,24,10,2026,00,00");
        QSignalSpy decoded(&serial, &Nmea0183Handler::RcvdSnt);
        port->feed(frame.left(8));
        port->feed(frame.mid(8));
        QVERIFY(decoded.count() > 0);
        QVERIFY(ApplicationLog::flush());
        const auto before = decoded.count();
        port->feed(frame.left(8));
        ApplicationLog::stopTraffic();
        QVERIFY(ApplicationLog::flush());
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        port->feed(frame.mid(8));
        QCOMPARE(decoded.count(), before + 1);
        serial.setMode(Sender);
        port->feed(frame);
        QVERIFY(serial.SendNmea0183Message(QString::fromLatin1(frame)));
        port->partial = true;
        QVERIFY(!serial.SendNmea0183Message(QString::fromLatin1(frame)));
        QVERIFY(ApplicationLog::flush());
        int rx = 0, tx = 0, sentences = 0;
        for (const auto &r : records(true)) {
            if (r["kind"] == "nmea2000-message") {
                if (r["direction"] == "RX") {
                    ++rx;
                    QCOMPARE(r["source"].toInt(), 23);
                    QCOMPARE(r["pgn"].toInt(), 129025);
                } else
                    ++tx;
            }
            if (r["kind"] == "nmea0183-sentence")
                ++sentences;
        }
        QCOMPARE(rx, 1);
        QCOMPARE(tx, 1);
        QCOMPARE(sentences, 4);
        bool partial = false;
        for (const auto &record : records(true))
            if (record["status"] == "partially-accepted")
                partial = true;
        QVERIFY(partial);
    }
    void rawCanRxThroughSerialAdapter()
    {
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        MemoryPort *port = nullptr;
        WaveshareSession session(nullptr, [&](QObject *parent) {
            port = new MemoryPort;
            port->setParent(parent);
            return port;
        });
        session.start("memory");
        QTRY_VERIFY(session.state() != WaveshareSession::Configuring);
        QVERIFY(port);
        QByteArray data = QByteArray::fromHex("0102030405060708");
        QByteArray frame;
        frame += char(0xaa);
        frame += char(0xe8);
        quint32 id = 0x09f80117;
        for (int i = 0; i < 4; ++i)
            frame += char(id >> (8 * i));
        frame += data;
        frame += char(0x55);
        port->feed(frame);
        QVERIFY(ApplicationLog::flush());
        bool found = false;
        for (const auto &r : records(true))
            if (r["kind"] == "can-frame") {
                found = true;
                QCOMPARE(r["id"].toString(), QString("09f80117"));
                QCOMPARE(r["payload_hex"].toString(), QString("0102030405060708"));
                QCOMPARE(r["status"].toString(), QString("transport-received"));
            }
        QVERIFY(found);
    }
    void actualLogicalNmea2000Submission()
    {
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        MemoryPort *port = nullptr;
        Nmea2000Handler handler(nullptr, nullptr, nullptr, [&](QObject *parent) {
            port = new MemoryPort;
            port->setParent(parent);
            return port;
        });
        handler.SetInterfaceType("Waveshare USB-CAN-A");
        handler.setMode(Sender);
        WaveshareConfig identity;
        identity.uniqueNumber = 12345;
        identity.preferredSource = 44;
        handler.ConfigureWaveshare("memory", identity);
        handler.StartWaveshareSending();
        QTRY_VERIFY_WITH_TIMEOUT(handler.IsWaveshareReady(), 5000);
        QVERIFY(port);
        QVERIFY(handler.sendPgn(129025, {"Latitude:1.25", "Longitude:-2.5"}));
        QVERIFY(ApplicationLog::flush());
        bool found = false;
        for (const auto &record : records(true))
            if (record["kind"] == "nmea2000-message" && record["direction"] == "TX") {
                found = true;
                QCOMPARE(record["pgn"].toInt(), 129025);
                QCOMPARE(record["source"].toInt(), 44);
                QCOMPARE(record["source_basis"].toString(), QString("active-session"));
                QCOMPARE(record["status"].toString(), QString("software-accepted"));
            }
        QVERIFY(found);
    }
    void networkRawRxDoesNotRecordAuthentication()
    {
        const auto proxy = QNetworkProxy::applicationProxy();
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        const auto restore = qScopeGuard([proxy] { QNetworkProxy::setApplicationProxy(proxy); });
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *peer = server.nextPendingConnection();
            auto send = [peer](const QJsonObject &o) {
                peer->write(QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n');
            };
            send({{"kind", "hello"},
                  {"version", 1},
                  {"interface", "fixture"},
                  {"nonce", QString(64, 'a')},
                  {"client", "test-client"}});
            connect(peer, &QTcpSocket::readyRead, peer, [peer, send, input = QByteArray()]() mutable {
                input += peer->readAll();
                while (input.contains('\n')) {
                    const int at = input.indexOf('\n');
                    const auto record = QJsonDocument::fromJson(input.left(at)).object();
                    input.remove(0, at + 1);
                    if (record["kind"] == "auth") {
                        send({{"kind", "ready"}, {"mode", "rx"}});
                        send({{"kind", "frame"},
                              {"seq", 1},
                              {"id", double(0x89f80117u)},
                              {"dlc", 8},
                              {"data", "0102030405060708"},
                              {"origin", "other-client"}});
                    }
                }
            });
        });
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        NetworkCanSession session;
        NetworkCanConfig config;
        config.host = "127.0.0.1";
        config.port = server.serverPort();
        config.interfaceName = "fixture";
        config.uniqueNumber = 12345;
        config.allowTx = false;
        config.token = "network-fixture-secret";
        session.configure(config);
        QTRY_VERIFY_WITH_TIMEOUT(session.ready(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(ApplicationLog::written(true) >= 2, 5000);
        QVERIFY(ApplicationLog::flush());
        bool found = false;
        for (const auto &r : records(true))
            if (r["kind"] == "can-frame") {
                found = true;
                QCOMPARE(r["transport"].toString(), QString("Network NMEA2000"));
                QCOMPARE(r["id"].toString(), QString("09f80117"));
            }
        QVERIFY(found);
        const auto app = readFile(ApplicationLog::filePath(false)),
                   traffic = readFile(ApplicationLog::filePath(true));
        QVERIFY(!app.contains("network-fixture-secret"));
        QVERIFY(!traffic.contains("network-fixture-secret"));
        QVERIFY(!traffic.contains("proof"));
        QVERIFY(!traffic.contains("nonce"));
    }
#ifdef Q_OS_LINUX
    void socketCanNativeIdentifierFlags()
    {
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        unsigned char data[8] = {};
        ApplicationLog::socketCanFrame(CAN_EFF_FLAG | 0x09f80117, 8, data);
        ApplicationLog::socketCanFrame(0x123, 8, data); // Standard frame, not NMEA2000.
        ApplicationLog::socketCanFrame(CAN_EFF_FLAG | CAN_RTR_FLAG | 0x09f80117, 0, data);
        ApplicationLog::socketCanFrame(CAN_ERR_FLAG | 1, 8, data);
        QVERIFY(ApplicationLog::flush());
        int count = 0;
        for (const auto &record : records(true))
            if (record["kind"] == "can-frame") {
                ++count;
                QCOMPARE(record["id"].toString(), QString("09f80117"));
                QCOMPARE(record["transport"].toString(), QString("SocketCAN"));
                QVERIFY(record["extended"].toBool());
            }
        QCOMPARE(count, 1);
    }
#endif
    void offFenceAndRestart()
    {
        QVERIFY(ApplicationLog::startTraffic());
        ApplicationLog::stopTraffic();
        QVERIFY(!ApplicationLog::recording());
        QVERIFY(ApplicationLog::flush());
        QCOMPARE(ApplicationLog::trafficState(), QString("Off"));
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        unsigned char data[8] = {};
        for (int i = 0; i < 2000; ++i)
            ApplicationLog::canFrame("RX", "fixture", 0x9f80117, 8, data);
        ApplicationLog::stopTraffic();
        QVERIFY(ApplicationLog::flush());
        QCOMPARE(ApplicationLog::trafficState(), QString("Off"));
        const auto bytes = readFile(ApplicationLog::filePath(true));
        for (int i = 0; i < 1000; ++i)
            ApplicationLog::canFrame("RX", "fixture", 0x9f80117, 8, data);
        QVERIFY(ApplicationLog::flush());
        QCOMPARE(readFile(ApplicationLog::filePath(true)), bytes);
        auto path = ApplicationLog::filePath(true);
        AppSettings persisted("RFStateSide", "RayNmeaSimulator");
        persisted.setValue("Logging/TrafficRecording", true);
        persisted.sync();
        ApplicationLog::shutdown();
        QVERIFY(ApplicationLog::initialize(QFileInfo(path).absolutePath()));
        QVERIFY(!ApplicationLog::recording());
        QCOMPARE(ApplicationLog::trafficState(), QString("Off"));
    }
    void concurrentSustainedTraffic()
    {
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        std::atomic<int> done{0};
        QList<std::shared_ptr<std::thread>> threads;
        for (int n = 0; n < 3; ++n)
            threads << std::make_shared<std::thread>([&, n] {
                unsigned char data[8] = {};
                for (int i = 0; i < 12000; ++i)
                    ApplicationLog::canFrame("RX", QString("fixture%1").arg(n), 0x9f80117, 8, data);
                ++done;
            });
        int ticks = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, this, [&] { ++ticks; });
        timer.start(5);
        QTRY_COMPARE_WITH_TIMEOUT(done.load(), 3, 20000);
        for (auto &thread : threads)
            thread->join();
        QVERIFY(ticks > 0);
        ApplicationLog::event("test", "app-during-traffic");
        QVERIFY(ApplicationLog::flush(10000));
        QVERIFY(readFile(ApplicationLog::filePath(false)).contains("app-during-traffic"));
        QVERIFY(ApplicationLog::written(true) > 0);
        QVERIFY(ApplicationLog::dropped(true) > 0);
    }
    void normalTrafficLatencyAndLosslessRecording()
    {
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        std::vector<qint64> samples;
        samples.reserve(5000);
        unsigned char data[8] = {};
        QElapsedTimer total;
        total.start();
        for (int i = 0; i < 5000; ++i) {
            QElapsedTimer call;
            call.start();
            ApplicationLog::canFrame("RX", "latency-fixture", 0x09f80117, 8, data);
            samples.push_back(call.nsecsElapsed());
            if (i % 100 == 0)
                QCoreApplication::processEvents();
        }
        const auto elapsed = total.nsecsElapsed();
        QVERIFY(ApplicationLog::flush(10000));
        QCOMPARE(ApplicationLog::dropped(true), quint64(0));
        QVERIFY(ApplicationLog::written(true) >= 5001);
        std::sort(samples.begin(), samples.end());
        QJsonObject metrics{
            {"purpose", "Synthetic logger callback benchmark, not physical CAN qualification"},
            {"samples", 5000},
            {"elapsed_ms", double(elapsed) / 1e6},
            {"callback_p50_us", double(samples[2500]) / 1000},
            {"callback_p99_us", double(samples[4950]) / 1000},
            {"callback_max_us", double(samples.back()) / 1000},
            {"dropped", double(ApplicationLog::dropped(true))}};
#ifdef QT_DEBUG
        const auto mode = QString("debug");
#else
        const auto mode = QString("release");
#endif
        QFile output(QDir::currentPath() + "/build/m242b/latency-" + QSysInfo::productType() + "-" + mode +
                     ".json");
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(QJsonDocument(metrics).toJson());
    }
    void bindingLifecycleAndDispatch()
    {
        auto o = ApplicationLog::options();
        o.bindings = true;
        o.level = 3;
        ApplicationLog::configure(o);
        {
            Workspace w;
            QVERIFY(w.controller.loadLayoutDocument(clockLayout(), {}, [](const QString &) { return true; }));
            auto *clock = w.area.findChild<GeneratorDateTime *>();
            QVERIFY(clock);
            QVERIFY(ApplicationLog::flush());
            auto before = records(false);
            QVERIFY(hasEvent(before, "connected"));
            clock->setDispatch(false);
            QVERIFY(ApplicationLog::flush());
            auto after = records(false);
            int disconnects = 0;
            for (const auto &r : after)
                if (r["event"] == "disconnected")
                    ++disconnects;
            int previous = 0;
            for (const auto &r : before)
                if (r["event"] == "disconnected")
                    ++previous;
            QCOMPARE(disconnects, previous);
            w.controller.setMode(Viewer);
            QVERIFY(ApplicationLog::flush());
            QVERIFY(hasEvent(records(false), "disconnected"));
        }
        QVERIFY(ApplicationLog::flush());
        QVERIFY(hasEvent(records(false), "binding-rebuild-end"));
    }
    void clearAndRefreshWhilePaused_data()
    {
        QTest::addColumn<bool>("traffic");
        QTest::addColumn<bool>("paused");
        QTest::newRow("application-following") << false << false;
        QTest::newRow("application-paused") << false << true;
        QTest::newRow("traffic-following") << true << false;
        QTest::newRow("traffic-paused") << true << true;
    }
    void clearAndRefreshWhilePaused()
    {
        QFETCH(bool, traffic);
        QFETCH(bool, paused);
        auto config = ApplicationLog::options();
        config.disabledCategories = {"qt."}; // Exclude offscreen platform chatter from exact-file assertions.
        ApplicationLog::configure(config);
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        unsigned char data[8] = {};
        auto write = [&](const QString &marker) {
            ApplicationLog::event("clear-fixture", marker);
            ApplicationLog::canFrame("RX", marker, 0x09f80117, 8, data);
            QVERIFY(ApplicationLog::flush());
        };
        write("before-clear");
        const auto selectedPath = ApplicationLog::filePath(traffic);
        const auto otherPath = ApplicationLog::filePath(!traffic);
        for (bool channel : {false, true}) {
            QFile archive(ApplicationLog::filePath(channel) + ".1");
            QVERIFY(archive.open(QIODevice::WriteOnly));
            QCOMPARE(archive.write("retained-archive"), qint64(16));
        }
        LogViewer viewer;
        viewer.show();
        auto *channel = viewer.findChild<QComboBox *>("logChannel");
        auto *pause = viewer.findChild<QCheckBox *>("pauseDisplay");
        auto *refresh = viewer.findChild<QPushButton *>("refreshLog");
        auto *clear = viewer.findChild<QPushButton *>("clearLog");
        auto *text = viewer.findChild<QPlainTextEdit *>("logText");
        QVERIFY(channel && pause && refresh && clear && text);
        channel->setCurrentIndex(traffic ? 1 : 0);
        viewer.refresh();
        QTRY_VERIFY(text->toPlainText().contains("before-clear"));
        pause->setChecked(paused);
        const auto initial = text->toPlainText();
        write("automatic-new-record");
        QTest::qWait(1200);
        if (paused)
            QCOMPARE(text->toPlainText(), initial);
        else
            QTRY_VERIFY(text->toPlainText().contains("automatic-new-record"));
        refresh->click();
        QTRY_VERIFY(text->toPlainText().contains("automatic-new-record"));
        QCOMPARE(pause->isChecked(), paused);

        bool yes = false;
        QTimer answers;
        answers.setInterval(10);
        connect(&answers, &QTimer::timeout, this, [&] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                box->button(yes ? QMessageBox::Yes : QMessageBox::No)->click();
        });
        answers.start();
        const auto beforeCancel = text->toPlainText();
        const auto selectedBytes = readFile(selectedPath);
        const auto otherBytes = readFile(otherPath);
        clear->click(); // No must change neither the file nor the display.
        QCOMPARE(text->toPlainText(), beforeCancel);
        QCOMPARE(readFile(selectedPath), selectedBytes);
        QCOMPARE(readFile(otherPath), otherBytes);
        yes = true;
        clear->click();
        QVERIFY(text->toPlainText().isEmpty()); // No stale display while clear is queued.
        QTRY_VERIFY(!ApplicationLog::clearing(traffic));
        QVERIFY(ApplicationLog::flush());
        QTRY_VERIFY(text->toPlainText().isEmpty());
        QVERIFY(readFile(selectedPath).isEmpty());
        QCOMPARE(readFile(otherPath), otherBytes);
        QCOMPARE(pause->isChecked(), paused);
        QVERIFY(ApplicationLog::recording());
        for (bool channel : {false, true})
            QCOMPARE(readFile(ApplicationLog::filePath(channel) + ".1"), QByteArray("retained-archive"));
        write("after-clear");
        QTest::qWait(1200);
        if (paused)
            QVERIFY(text->toPlainText().isEmpty());
        else
            QTRY_VERIFY(text->toPlainText().contains("after-clear"));
        refresh->click();
        QTRY_VERIFY(text->toPlainText().contains("after-clear"));
        QVERIFY(!text->toPlainText().contains("before-clear"));
        QCOMPARE(pause->isChecked(), paused);
        QVERIFY(ApplicationLog::recording());
    }
    void viewerAndSettingsDoNotSend()
    {
        MainWindow window;
        QVERIFY(!window.findChild<NmeaSenderWorker *>()->isSending());
        LogViewer viewer(&window);
        viewer.show();
        auto *button = viewer.findChild<QPushButton *>("trafficRecording");
        QVERIFY(button);
        QVERIFY(!ApplicationLog::recording());
        QCOMPARE(viewer.findChild<QComboBox *>("logChannel")->count(), 2);
        LoggingPreferences prefs;
        auto *binding = prefs.findChild<QCheckBox *>("loggingBindings");
        QVERIFY(binding);
        binding->setChecked(true);
        QVERIFY(!ApplicationLog::bindingEnabled());
        viewer.refresh();
        QVERIFY(viewer.findChild<QPlainTextEdit *>("logText")->maximumBlockCount() == 2000);
        viewer.close();
        QVERIFY(!ApplicationLog::recording());
        QVERIFY(!window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void realViewerRecordingAndSettingsLifecycle()
    {
        MainWindow window;
        LogViewer viewer(&window);
        viewer.show();
        auto *record = viewer.findChild<QPushButton *>("trafficRecording");
        QVERIFY(record);
        bool yes = false;
        QTimer answers;
        answers.setInterval(10);
        connect(&answers, &QTimer::timeout, this, [&] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                box->button(yes ? QMessageBox::Yes : QMessageBox::No)->click();
        });
        answers.start();
        record->click();
        QVERIFY(!ApplicationLog::recording());
        yes = true;
        record->click();
        QTRY_VERIFY(ApplicationLog::recording());
        viewer.close();
        QVERIFY(ApplicationLog::recording());
        LogViewer second(&window);
        second.show();
        ApplicationLog::exportCurrent(true, directory->path() + "/nonexistent/export.jsonl");
        QTRY_VERIFY(ApplicationLog::health(true).startsWith("Export failed"));
        QVERIFY(ApplicationLog::recording());
        second.refresh();
        QCOMPARE(second.findChild<QPushButton *>("trafficRecording")->text(),
                 QString("Stop Traffic Recording"));
        QPushButton *settings = nullptr;
        for (auto *button : second.findChildren<QPushButton *>())
            if (button->text() == "Logging Settings…")
                settings = button;
        QVERIFY(settings);
        bool save = false, visited = false;
        QTimer editor;
        editor.setInterval(10);
        connect(&editor, &QTimer::timeout, this, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog || dialog->windowTitle() != "Logging Settings" || visited)
                return;
            visited = true;
            dialog->findChild<QCheckBox *>("loggingBindings")->setChecked(true);
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QTimer::singleShot(0, buttons->button(save ? QDialogButtonBox::Save : QDialogButtonBox::Cancel),
                               &QPushButton::click);
        });
        editor.start();
        settings->click();
        QVERIFY(visited);
        QVERIFY(!ApplicationLog::bindingEnabled());
        visited = false;
        save = true;
        settings->click();
        QVERIFY(visited);
        QVERIFY(ApplicationLog::bindingEnabled());
        QVERIFY(!window.findChild<NmeaSenderWorker *>()->isSending());
        second.refresh();
        QCoreApplication::processEvents();
        second.grab().save(QDir::currentPath() + "/build/m242b/log-viewer.png");
        second.findChild<QPushButton *>("trafficRecording")->click();
        QTRY_VERIFY(!ApplicationLog::recording());
        QVERIFY(!window.findChild<NmeaSenderWorker *>()->isSending());
    }
};
int main(int argc, char **argv)
{
    QDir().mkpath(QDir::currentPath() + "/build/m242b");
    QTemporaryDir profile(QDir::currentPath() + "/build/m242b/profile-XXXXXX");
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    LoggingTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "logging_tests.moc"
