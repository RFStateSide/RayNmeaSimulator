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
#include <QCheckBox>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QNetworkProxy>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLineEdit>
#include <QAction>
#include <QFile>
#include <QFontDatabase>
#include <thread>
#include <atomic>
#include "applicationlog.h"
#include "mainwindow.h"
#include "nmea2000diagnostics.h"
#include "nmea2000diagnosticsdialog.h"
#include "nmeareceiver.h"
#include "NMEA2000.h"
#include "N2kMessages.h"

namespace
{
tN2kMsg message(quint32 pgn, const QByteArray &bytes, int source = 1)
{
    tN2kMsg m;
    m.SetPGN(pgn);
    m.Source = source;
    m.Destination = 255;
    m.Priority = 6;
    for (auto b : bytes)
        m.AddByte(quint8(b));
    return m;
}
tN2kMsg claim(int source, quint64 unique = 42, int manufacturer = 1851)
{
    tNMEA2000::tDeviceInformation d;
    d.SetName(0);
    d.SetUniqueNumber(unique);
    d.SetManufacturerCode(manufacturer);
    d.SetDeviceClass(60);
    d.SetDeviceFunction(140);
    d.SetIndustryGroup(4);
    d.SetDeviceInstance(0x21);
    QByteArray b;
    auto n = d.GetName();
    for (int i = 0; i < 8; ++i)
        b += char(n >> (8 * i));
    return message(60928, b, source);
}
tN2kMsg product(int source = 1)
{
    tN2kMsg m;
    SetN2kPGN126996(m, 2100, 105, "Test model", "v1", "hardware1", "fixture-only", 0, 1);
    m.Source = source;
    return m;
}
QString field(const Nmea2000Diagnostics::Message &m, const QString &name)
{
    for (const auto &f : m.fields)
        if (f.name == name)
            return f.value;
    return {};
}
QByteArray read(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
} // namespace
class DeviceDiagnosticsTests : public QObject
{
    Q_OBJECT
    QTemporaryDir logs{QDir::currentPath() + "/build/m243c/logs-XXXXXX"};
  private slots:
    void init()
    {
        QVERIFY(logs.isValid());
        ApplicationLog::shutdown();
        QLoggingCategory::setFilterRules({});
        QVERIFY(ApplicationLog::initialize(logs.path() + "/" + QTest::currentTestFunction()));
    }
    void cleanup()
    {
        ApplicationLog::shutdown();
        QLoggingCategory::setFilterRules({});
    }
    void lookupCoverage()
    {
        QVERIFY(NmeaLookup::manufacturer(1851).contains("Raymarine"));
        QVERIFY(NmeaLookup::manufacturer(419).contains("Fusion"));
        QVERIFY(NmeaLookup::manufacturer(591).contains("Coast"));
        QVERIFY(NmeaLookup::manufacturer(999999).isEmpty());
        QVERIFY(!NmeaLookup::deviceClass(60).isEmpty());
        QVERIFY(!NmeaLookup::deviceFunction(60, 140).isEmpty());
        QVERIFY(NmeaLookup::deviceFunction(1, 140).isEmpty());
        QVERIFY(!NmeaLookup::industry(4).isEmpty());
        QVERIFY(!NmeaLookup::provenance().isEmpty());
    }
    void identityAndProduct()
    {
        Nmea2000Diagnostics d;
        auto c = claim(1);
        auto k = d.observe(c);
        auto row = d.message(k);
        auto device = d.device(row.deviceKey);
        QVERIFY(device.hasName);
        QCOMPARE(device.manufacturer, 1851);
        QCOMPARE(device.uniqueNumber, quint32(42));
        QVERIFY(device.arbitraryAddressCapable);
        QCOMPARE(device.function, 140);
        QCOMPARE(device.cls, 60);
        QCOMPARE(device.industry, 4);
        QCOMPARE(device.lowerInstance, 1);
        QCOMPARE(device.upperInstance, 4);
        QCOMPARE(device.wireName,
                 QString::fromLatin1(QByteArray(reinterpret_cast<char *>(c.Data), 8).toHex()));
        d.observe(product());
        device = d.device(row.deviceKey);
        QCOMPARE(device.model, QString("Test model"));
        QCOMPARE(device.software, QString("v1"));
        QCOMPARE(device.productCode, 105);
        QVERIFY(!device.provisionalProduct);
        d.observe(claim(150, 43, 419));
        QCOMPARE(d.devices().size(), 2);
        auto unknown = d.message(d.observe(message(130916, QByteArray::fromHex("3b9f00000000000000"), 17)));
        QCOMPARE(unknown.receiptIdentity["source_manufacturer"].toInt(), -1);
    }
    void productBeforeClaim()
    {
        Nmea2000Diagnostics d;
        auto k = d.observe(product());
        QVERIFY(d.device(d.message(k).deviceKey).provisionalProduct);
        k = d.observe(claim(1));
        const auto v = d.device(d.message(k).deviceKey);
        QCOMPARE(v.model, QString("Test model"));
        QVERIFY(v.provisionalProduct);
        QCOMPARE(d.devices().size(), 1);
        d.observe(product());
        QVERIFY(!d.device(v.key).provisionalProduct);
    }
    void addressMovesAndReuse()
    {
        Nmea2000Diagnostics d;
        auto k = d.observe(claim(1), 0, 10);
        const auto nameKey = d.message(k).deviceKey;
        d.observe(product(), 0, 11);
        k = d.observe(claim(2), 0, 12);
        QCOMPARE(d.message(k).deviceKey, nameKey);
        QCOMPARE(d.device(nameKey).source, 2);
        QCOMPARE(d.device(nameKey).model, QString("Test model"));
        QCOMPARE(d.devices().size(), 1);
        d.observe(message(126993, QByteArray::fromHex("60ea01ffffffffffff"), 1), 0, 13);
        QVERIFY(d.device(nameKey).conflict);
        k = d.observe(claim(1, 55), 0, 14);
        QVERIFY(!d.device(d.message(k).deviceKey).conflict);
        QVERIFY(d.device(d.message(k).deviceKey).model.isEmpty());
        auto oldKey = d.message(k).deviceKey;
        d.observe(claim(1, 99), 0, 15);
        QVERIFY(!d.device(oldKey).active);
    }
    void moveOntoOccupiedAddress()
    {
        Nmea2000Diagnostics d;
        auto old = d.message(d.observe(claim(2, 99))).deviceKey;
        auto moved = d.message(d.observe(claim(1, 42))).deviceKey;
        d.observe(claim(2, 42));
        QVERIFY(!d.device(old).active);
        QCOMPARE(d.device(moved).source, 2);
    }
    void invalidClaims_data()
    {
        QTest::addColumn<int>("source");
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("zero") << 1 << QByteArray(8, 0);
        QTest::newRow("unavailable") << 1 << QByteArray(8, char(255));
        QTest::newRow("reserved-source") << 254 << QByteArray::fromHex("0102030405060708");
        QTest::newRow("truncated") << 1 << QByteArray(7, 0);
    }
    void retiredNameMovesAfterAddressReuse()
    {
        Nmea2000Diagnostics d;
        const auto first = d.message(d.observe(claim(1, 42))).deviceKey;
        const auto second = d.message(d.observe(claim(1, 99))).deviceKey;
        QVERIFY(!d.device(first).active);
        d.observe(claim(2, 42));
        QCOMPARE(d.sourceObservation(1).key, second);
        QCOMPARE(d.sourceObservation(2).key, first);
        d.observe(message(130916, QByteArray::fromHex("3b9f00000000000000"), 1));
        QVERIFY(!d.device(first).conflict);
        QVERIFY(!d.device(second).conflict);
        const auto third = d.message(d.observe(claim(2, 77))).deviceKey;
        d.observe(claim(3, 42));
        QCOMPARE(d.sourceObservation(1).key, second);
        QCOMPARE(d.sourceObservation(2).key, third);
        QCOMPARE(d.sourceObservation(3).key, first);
    }
    void invalidClaims()
    {
        QFETCH(int, source);
        QFETCH(QByteArray, bytes);
        Nmea2000Diagnostics d;
        auto r = d.message(d.observe(message(60928, bytes, source)));
        QVERIFY(!d.device(r.deviceKey).hasName);
        QCOMPARE(r.payload, bytes);
        QCOMPARE(r.malformed, bytes.size() != 8);
    }
    void reconnectEpochAndSnapshots()
    {
        Nmea2000Diagnostics d;
        d.setSession("transport1", "test");
        auto epoch = d.generation();
        auto row = d.message(d.observe(claim(1)));
        d.setSession("transport2", "test2");
        QVERIFY(!d.device(row.deviceKey).active);
        QVERIFY(d.observe(claim(1), epoch).isEmpty());
        auto next = d.message(d.observe(message(129044, QByteArray(20, 0))));
        QCOMPARE(next.receiptIdentity["source_manufacturer"].toInt(), -1);
        QCOMPARE(d.message(row.key).receiptIdentity["source_manufacturer"].toInt(), 1851);
        QVERIFY(row.session != next.session);
    }
    void boundedCaches()
    {
        Nmea2000Diagnostics d;
        for (int i = 0; i < 700; ++i)
            d.observe(claim(1, i + 1));
        QVERIFY(d.devices().size() <= d.DeviceLimit);
        QVERIFY(d.evictions() > 0);
        // Re-observing an evicted NAME reconstructs it instead of following a dangling index.
        auto row = d.message(d.observe(claim(2, 1)));
        QVERIFY(d.device(row.deviceKey).hasName);
        for (int i = 0; i < 2200; ++i)
            d.observe(message(100000 + i, QByteArray(8, 0)));
        QCOMPARE(d.messages().size(), d.MessageLimit);
    }
    void heartbeat_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<QString>("interval");
        QTest::newRow("normal") << QByteArray::fromHex("60ea07cfffffffff") << QString("60 s");
        QTest::newRow("zero") << QByteArray(8, 0) << QString("0 s");
        QTest::newRow("NA") << QByteArray(8, char(255)) << QString("Unavailable");
        QTest::newRow("error") << QByteArray::fromHex("feff010200000000") << QString("Error");
        QTest::newRow("reserved") << QByteArray::fromHex("fdff010000000000") << QString("Reserved");
    }
    void heartbeat()
    {
        QFETCH(QByteArray, bytes);
        QFETCH(QString, interval);
        Nmea2000Diagnostics d;
        auto r = d.message(d.observe(message(126993, bytes)));
        QVERIFY(!r.malformed);
        QVERIFY(field(r, "Current transmission interval").startsWith(interval));
        if (bytes == QByteArray(8, char(255)))
            QVERIFY(field(r, "Controller 1").contains("Unavailable"));
        QCOMPARE(r.payload, bytes);
        QVERIFY(!field(r, "Reserved bits").isEmpty());
    }
    void groupFunction_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<bool>("malformed");
        QTest::newRow("request") << QByteArray::fromHex("0000ef01ffffffffffff00") << false;
        QTest::newRow("command") << QByteArray::fromHex("0100ef01f300") << false;
        QTest::newRow("ack-negative") << QByteArray::fromHex("0200ef01210342f1") << false;
        QTest::newRow("short-ack") << QByteArray::fromHex("0200ef01210342") << true;
        QTest::newRow("missing-request-index") << QByteArray::fromHex("0000ef01ffffffffffff01") << true;
        QTest::newRow("missing-command-index") << QByteArray::fromHex("0100ef01f301") << true;
        for (int f = 3; f <= 6; ++f) {
            QTest::newRow(qPrintable(QString("standard-%1").arg(f)))
                << (QByteArray(1, char(f)) + QByteArray::fromHex("010101070000")) << false;
            QTest::newRow(qPrintable(QString("proprietary-%1").arg(f)))
                << (QByteArray(1, char(f)) + QByteArray::fromHex("00ef013b9f070000")) << false;
        }
        QTest::newRow("unknown") << QByteArray::fromHex("ff00ef0101234567") << false;
        QTest::newRow("short-header") << QByteArray::fromHex("0000ef") << true;
        QTest::newRow("invalid-target") << QByteArray::fromHex("00ffffff00000000000000") << true;
    }
    void groupFunction()
    {
        QFETCH(QByteArray, bytes);
        QFETCH(bool, malformed);
        Nmea2000Diagnostics d;
        auto r = d.message(d.observe(message(126208, bytes)));
        QCOMPARE(r.malformed, malformed);
        QCOMPARE(r.payload, bytes);
        if (quint8(bytes[0]) == 2 && !malformed) {
            QCOMPARE(field(r, "PGN error code"), QString("0x1"));
            QCOMPARE(field(r, "Parameter error 3"), QString("0x1"));
        }
        if (!malformed && bytes.size() == 9)
            QCOMPARE(r.payloadManufacturer, 1851);
    }
    void opaquePayloads_data()
    {
        QTest::addColumn<quint32>("pgn");
        QTest::addColumn<int>("size");
        QTest::newRow("long131056") << quint32(131056) << 222;
        QTest::newRow("short131056") << quint32(131056) << 210;
        QTest::newRow("130916") << quint32(130916) << 9;
        QTest::newRow("65535") << quint32(65535) << 8;
    }
    void opaquePayloads()
    {
        QFETCH(quint32, pgn);
        QFETCH(int, size);
        Nmea2000Diagnostics d;
        d.observe(claim(1));
        QByteArray bytes;
        for (int i = 0; i < size; ++i)
            bytes += char(i);
        bytes[0] = char(0x4f);
        bytes[1] = char(0x9a); // 591 / industry 4.
        auto r = d.message(d.observe(message(pgn, bytes)));
        QVERIFY(!r.malformed);
        QCOMPARE(r.payload, bytes);
        QCOMPARE(r.payloadManufacturer, 591);
        QCOMPARE(r.receiptIdentity["source_manufacturer"].toInt(), 1851);
        QCOMPARE(r.fields.size(), size + 1);
        QVERIFY(r.status.contains("Opaque"));
    }
    void futureVariantsRemainQualified()
    {
        Nmea2000Diagnostics d;
        auto b = QByteArray::fromHex("3b9f000102030405");
        auto r = d.message(d.observe(message(65311, b)));
        QVERIFY(r.function.contains("Raymarine"));
        QVERIFY(!r.function.contains("BEP"));
        QVERIFY(r.status.contains("undefined"));
        for (quint32 p : {129044u, 129542u, 129547u, 65359u, 65379u, 126720u, 130820u}) {
            r = d.message(d.observe(message(p, b)));
            QCOMPARE(r.payload, b);
            QVERIFY(!r.status.contains("complete", Qt::CaseInsensitive));
        }
    }
    void debugEligibilityAndOnce()
    {
        auto o = ApplicationLog::options();
        QVERIFY(!ApplicationLog::reportUndecodedPgn(131056));
        o.level = 0;
        ApplicationLog::configure(o);
        QVERIFY(ApplicationLog::reportUndecodedPgn(131056));
        QVERIFY(!ApplicationLog::reportUndecodedPgn(131056));
        o.disabledCategories = {"raynmea.protocol"};
        ApplicationLog::configure(o);
        QVERIFY(!ApplicationLog::reportUndecodedPgn(130916));
        o.disabledCategories.clear();
        ApplicationLog::configure(o);
        QLoggingCategory::setFilterRules("raynmea.protocol.nmea2000.debug=false");
        QVERIFY(!ApplicationLog::reportUndecodedPgn(130916));
        QLoggingCategory::setFilterRules("raynmea.protocol.nmea2000.debug=true");
        QVERIFY(ApplicationLog::reportUndecodedPgn(130916));
        QVERIFY(!ApplicationLog::reportUndecodedPgn(0xffffffff));
        QVERIFY(ApplicationLog::flush());
        const auto bytes = read(ApplicationLog::filePath(false));
        QCOMPARE(bytes.count("application decoder unavailable"), 2);
        QVERIFY(bytes.contains("Debug"));
        QVERIFY(!bytes.contains("Warning"));
        ApplicationLog::clear(false);
        QVERIFY(ApplicationLog::flush());
        QVERIFY(!ApplicationLog::reportUndecodedPgn(131056));
    }
    void concurrentOnce()
    {
        auto o = ApplicationLog::options();
        o.level = 0;
        ApplicationLog::configure(o);
        std::atomic<int> count{0};
        std::vector<std::thread> threads;
        for (int i = 0; i < 12; ++i)
            threads.emplace_back([&] {
                if (ApplicationLog::reportUndecodedPgn(130916))
                    ++count;
            });
        for (auto &t : threads)
            t.join();
        QCOMPARE(count.load(), 1);
        QVERIFY(ApplicationLog::flush());
        QCOMPARE(read(ApplicationLog::filePath(false)).count("application decoder unavailable"), 1);
    }
    void productionReceiveAndTrafficIndependence()
    {
        Nmea2000Handler h;
        h.setMode(Viewer);
        QSignalSpy route(&h, &Nmea2000Handler::RouteServiceMessage);
        QSignalSpy instruments(&h, &Nmea2000Handler::RcvdPgn);
        auto o = ApplicationLog::options();
        o.level = 0;
        ApplicationLog::configure(o);
        QVERIFY(ApplicationLog::startTraffic());
        QTRY_VERIFY(ApplicationLog::recording());
        auto m = message(131056, QByteArray::fromHex("3b9f000102030405"));
        h.nmea2000MessageReceived(m);
        h.nmea2000MessageReceived(m);
        QCOMPARE(route.size(), 2);
        QCOMPARE(instruments.size(), 0);
        h.setMode(Sender);
        h.nmea2000MessageReceived(m);
        h.setMode(Viewer);
        tN2kMsg x;
        SetN2kXTE(x, 1, N2kxtem_Autonomous, false, -12.5);
        x.Source = 1;
        h.nmea2000MessageReceived(x);
        QCOMPARE(instruments.size(), 1);
        QCOMPARE(route.size(), 4);
        auto last = h.diagnostics()->messages();
        bool decoded = false;
        for (const auto &r : last)
            if (r.pgn == 129283) {
                decoded = r.status.contains("Existing application");
                QCOMPARE(field(r, "Decoded XTE"), QString("-12.5"));
            }
        QVERIFY(decoded);
        QVERIFY(ApplicationLog::flush());
        const auto traffic = read(ApplicationLog::filePath(true));
        QCOMPARE(traffic.count("transport-received"), 4);
        QCOMPARE(read(ApplicationLog::filePath(false)).count("application decoder unavailable"), 1);
        ApplicationLog::stopTraffic();
        QTRY_VERIFY(!ApplicationLog::recording());
        const auto before = ApplicationLog::written(true);
        h.nmea2000MessageReceived(m);
        QVERIFY(ApplicationLog::flush());
        QCOMPARE(ApplicationLog::written(true), before);
    }
    void diagnosticUi()
    {
        Nmea2000Diagnostics model;
        model.observe(claim(1));
        model.observe(product());
        auto m = message(131056, QByteArray::fromHex("3b9f000102030405"));
        model.observe(m);
        Nmea2000DiagnosticsDialog view(&model);
        view.show();
        QVERIFY(!view.isModal());
        auto *table = view.findChild<QTableWidget *>("observedPgns");
        auto *raw = view.findChild<QPlainTextEdit *>("diagnosticRaw");
        auto *pause = view.findChild<QCheckBox *>("diagnosticPause");
        auto *filter = view.findChild<QLineEdit *>("diagnosticFilter");
        QVERIFY(table && raw && pause && filter);
        filter->setText("131056");
        QCOMPARE(table->rowCount(), 1);
        table->setCurrentCell(0, 0);
        QCOMPARE(raw->toPlainText(), QString("3b 9f 00 01 02 03 04 05"));
        QVERIFY(raw->isReadOnly());
        pause->setChecked(true);
        model.observe(message(130916, QByteArray::fromHex("3b9f00000000000000")));
        view.refresh(false);
        QCOMPARE(table->rowCount(), 1);
        filter->clear();
        QCOMPARE(table->rowCount(), 4);
        model.observe(message(65535, QByteArray::fromHex("4f9a000000000000")));
        view.refresh(false);
        QCOMPARE(table->rowCount(), 4);
        view.findChild<QPushButton *>("diagnosticRefresh")->click();
        QCOMPARE(table->rowCount(), 5);
        QVERIFY(pause->isChecked());
        pause->setChecked(false);
        view.refresh(false);
        QCOMPARE(table->rowCount(), 5);
        if (!qEnvironmentVariableIsEmpty("RAYNMEA_DIAGNOSTIC_SCREENSHOT"))
            view.grab().save(qEnvironmentVariable("RAYNMEA_DIAGNOSTIC_SCREENSHOT"));
        QVERIFY(!ApplicationLog::recording());
        view.close();
        QVERIFY(!ApplicationLog::recording());
    }
    void malformedWarningPreserved()
    {
        Nmea2000Handler h;
        h.setMode(Viewer);
        h.nmea2000MessageReceived(message(126993, QByteArray(7, 0)));
        QVERIFY(ApplicationLog::flush());
        const auto records = read(ApplicationLog::filePath(false));
        QVERIFY(records.contains("Malformed received NMEA2000 PGN"));
        QVERIFY(records.contains("Warning"));
        QVERIFY(!records.contains("application decoder unavailable"));
    }
    void liveWidgetIdentityOwnership()
    {
        QMdiArea area;
        QTableView table;
        QTreeView tree;
        PgnTableController tables(&table);
        PgnTreeController trees(&tree);
        MainController controller(&tables, &trees, &area, nullptr);
        Nmea2000Handler handler;
        Nmea0183Handler sentences;
        NmeaReceiver receiver(&controller, &tables, &handler, &sentences);
        controller.setMode(Viewer);
        handler.setMode(Viewer);
        controller.setMyVessel(MyVessel{});
        controller.setUserPrefs(UserUnitPrefs{});
        controller.setUserOptionPrefs(UserOptionPrefs{});
        controller.addSubWindowForGroup("Propulsion", "Propulsion");
        auto *model = tables.getPgnTableModel();
        auto engine = [&](int source, double rpm) {
            tN2kMsg m;
            SetN2kEngineParamRapid(m, 1, rpm, N2kDoubleNA, N2kInt8NA);
            m.Source = source;
            handler.nmea2000MessageReceived(m);
        };
        auto rpm = [&](int id) {
            for (const auto &r : model->getPgnParams(id))
                if (r[0] == "EngineSpeed")
                    return r[1].toDouble();
            return -1.;
        };
        engine(1, 1200);
        QCOMPARE(model->getPgns("127488").size(), 1);
        const int first = model->getPgns("127488").first().idX;
        handler.nmea2000MessageReceived(claim(1));
        engine(1, 1500);
        QCOMPARE(model->getPgns("127488").size(), 1);
        QCOMPARE(rpm(first), 1500.);
        handler.nmea2000MessageReceived(claim(2));
        engine(2, 2300);
        QCOMPARE(model->getPgns("127488").size(), 1);
        QCOMPARE(rpm(first), 2300.);
        QCOMPARE(model->findPgn(first)->source, QString("2"));
        handler.nmea2000MessageReceived(claim(2, 99));
        engine(2, 3100);
        QCOMPARE(model->getPgns("127488").size(), 2);
        QCOMPARE(rpm(first), 2300.);
        handler.SetInterfaceType("Actisense");
        handler.SetInterfaceType("");
        handler.nmea2000MessageReceived(claim(2, 99));
        engine(2, 4200);
        QCOMPARE(model->getPgns("127488").size(), 3);
        QCOMPARE(rpm(first), 2300.);
        QTemporaryDir layout(logs.path() + "/layout-XXXXXX");
        const auto path = layout.filePath("snapshot.json");
        QVERIFY(controller.saveLayoutToJson(path));
        QVERIFY(!controller.layoutNeedsSave());
        handler.nmea2000MessageReceived(product(2));
        QVERIFY(!controller.layoutNeedsSave());
        const auto bytes = read(path);
        QVERIFY(!bytes.contains("name_at_receipt"));
        QVERIFY(!bytes.contains("promotedFrom"));
        controller.removeAllWidgets();
        engine(2, 5100);
        QCOMPARE(model->getPgns("127488").size(), 1);
        QVERIFY(controller.loadLayoutFromJson(path, [](const QString &) { return true; }));
        engine(2, 6200);
        QCOMPARE(model->getPgns("127488").size(),
                 4); // Loaded widgets are local snapshots, never subscriptions.
        QVERIFY(controller.layoutWidgets("Generate").isEmpty());
    }
    void toolsActionDoesNotAuthorizeSending()
    {
        MainWindow window;
        auto *action = window.findChild<QAction *>("actionNmea2000Diagnostics");
        QVERIFY(action);
        auto *sender = window.findChild<NmeaSenderWorker *>();
        QVERIFY(sender);
        QVERIFY(!sender->isSending());
        action->trigger();
        QCOMPARE(window.findChildren<Nmea2000DiagnosticsDialog *>().size(), 1);
        action->trigger();
        QCOMPARE(window.findChildren<Nmea2000DiagnosticsDialog *>().size(), 1);
        QVERIFY(!sender->isSending());
        QVERIFY(!ApplicationLog::recording());
        window.findChild<Nmea2000DiagnosticsDialog *>()->close();
        QVERIFY(!sender->isSending());
    }
    void archivedReplay()
    {
        const auto path = qEnvironmentVariable("RAYNMEA_DIAGNOSTIC_REPLAY");
        if (path.isEmpty())
            QSKIP("Private archived traffic supplied separately");
        const auto data = QJsonDocument::fromJson(read(path)).array();
        QVERIFY(!data.isEmpty());
        Nmea2000Diagnostics d;
        QSet<quint32> observed;
        int count = 0;
        const QSet<quint32> targets = {126208, 126993, 129044, 129542, 129547, 65359, 65379,
                                       126720, 130820, 65311,  131056, 130916, 65535};
        for (const auto &v : data) {
            const auto o = v.toObject();
            auto p = quint32(o["pgn"].toInt());
            if (!targets.contains(p) && p != 60928 && p != 126996)
                continue;
            auto bytes = QByteArray::fromHex(o["payload_hex"].toString().toLatin1());
            auto row = d.message(d.observe(message(p, bytes, o["source"].toInt())));
            QCOMPARE(row.payload, bytes);
            QVERIFY2(!row.malformed, qPrintable(QString("PGN %1: %2").arg(p).arg(row.status)));
            observed.insert(p);
            ++count;
        }
        for (auto p : targets)
            QVERIFY2(observed.contains(p), qPrintable(QString::number(p)));
        QVERIFY(count > 1000);
        QVERIFY(d.messages().size() <= d.MessageLimit);
    }
};
int main(int argc, char **argv)
{
    QDir().mkpath(QDir::currentPath() + "/build/m243c");
    QTemporaryDir profile(QDir::currentPath() + "/build/m243c/profile-XXXXXX");
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    const auto fontPath = qEnvironmentVariable("RAYNMEA_DIAGNOSTIC_FONT");
    if (!fontPath.isEmpty()) {
        const int id = QFontDatabase::addApplicationFont(fontPath);
        const auto families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty())
            app.setFont(QFont(families.first(), 9));
    }
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    DeviceDiagnosticsTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "device_diagnostics_tests.moc"
