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
#include <QApplication>
#include <QTemporaryDir>
#include <QNetworkProxy>
#include <QFontDatabase>
#include <QLoggingCategory>
#include <QTableWidget>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>
#include "appsettings.h"
#include "applicationlog.h"
#include "nmea2000diagnostics.h"
#include "nmea2000diagnosticsdialog.h"
#include "nmea2000handler.h"
#include "mainwindow.h"
#include "maincontroller.h"
#include "nmeasenderworker.h"
#include "generatorvesselmovement.h"
#include "N2kMessages.h"

namespace {
tN2kMsg message(int pgn, const QByteArray &bytes, int source = 204, int destination = 255)
{
    tN2kMsg m; m.SetPGN(pgn); m.Source = source; m.Destination = destination;
    for (char b : bytes) m.AddByte(quint8(b));
    return m;
}
Nmea2000Diagnostics::Message decode(int pgn, const QByteArray &bytes)
{
    Nmea2000Diagnostics d; return d.message(d.observe(message(pgn, bytes)));
}
QString field(const Nmea2000Diagnostics::Message &m, const QString &name)
{
    for (const auto &f : m.fields) if (f.name == name) return f.value;
    return {};
}
tN2kMsg claim(int source, int unique, int manufacturer = 1851)
{
    tNMEA2000::tDeviceInformation info; info.SetName(0); info.SetUniqueNumber(unique);
    info.SetManufacturerCode(manufacturer); info.SetIndustryGroup(4);
    QByteArray b; for (int i = 0; i < 8; ++i) b += char(info.GetName() >> (i * 8));
    return message(60928, b, source);
}
tN2kMsg candidate(quint16 value, int source = 1)
{
    QByteArray b = QByteArray::fromHex("3b9faa0000112233"); b[3] = char(value); b[4] = char(value >> 8);
    return message(65311, b, source);
}
tN2kMsg standard(double radians, int source = 1)
{
    tN2kMsg m; SetN2kPGN127258(m, 1, tN2kMagneticVariation(0), 20000, radians); m.Source = source; return m;
}
QByteArray read(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }
}
class ProprietaryDiagnosticsTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> logs;
private slots:
    void init() {
        AppSettings s(APP_COMPANY, APP_NAME); s.clear(); s.setValue("Nmea2000/Nmea2000Enabled", false); s.setValue("Nmea0183/Nmea0183Enabled", false); s.sync();
        ApplicationLog::shutdown(); QLoggingCategory::setFilterRules({}); logs = std::make_unique<QTemporaryDir>();
        QVERIFY(logs->isValid()); QVERIFY(ApplicationLog::initialize(logs->path()));
    }
    void cleanup() { ApplicationLog::shutdown(); QLoggingCategory::setFilterRules({}); }
    void mode16_data() {
        QTest::addColumn<int>("value"); QTest::addColumn<QString>("name");
        for (const auto &p : QList<QPair<int,QString>>{{0,"Standby"},{0x40,"compass"},{0x100,"Wind"},{0x180,"Track"},{0x181,"COG"},{17,"Undefined"},{65535,"Unavailable"},{65534,"Error"},{65533,"Reserved"}})
            QTest::newRow(qPrintable(QString::number(p.first))) << p.first << p.second;
    }
    void mode16() {
        QFETCH(int,value); QFETCH(QString,name);
        QByteArray b = QByteArray::fromHex("3b9f0000ffff03ff"); b[2] = char(value); b[3] = char(value >> 8);
        const auto r = decode(65379, b); QVERIFY(r.qualified); QVERIFY(!r.malformed); QCOMPARE(r.payload, b);
        QVERIFY(field(r,"External pilot mode").contains(name)); QCOMPARE(field(r,"Submode raw"),"ff ff");
        QCOMPARE(field(r,"Mode data raw"),"03"); QCOMPARE(field(r,"Reserved byte 7"),"ff");
        QVERIFY(r.status.contains("TX/control unqualified"));
        auto wrong = b; wrong[0] ^= 1; QVERIFY(!decode(65379,wrong).qualified);
        wrong = b; wrong[1] ^= 0x20; QVERIFY(!decode(65379,wrong).qualified);
    }
    void mode8_data() {
        QTest::addColumn<int>("value"); QTest::addColumn<QString>("name");
        for (const auto &p : QList<QPair<int,QString>>{{0x40,"Standby"},{0x42,"Auto"},{0x46,"Wind"},{0x4a,"Track"},{0,"Undefined"},{255,"Unavailable"},{254,"Error"},{253,"Reserved"}})
            QTest::newRow(qPrintable(QString::number(p.first))) << p.first << p.second;
    }
    void mode8() {
        QFETCH(int,value); QFETCH(QString,name);
        QByteArray b = QByteArray::fromHex("3b9ff081840102030000aabbcc"); b[8] = char(value);
        auto r = decode(126720,b); QVERIFY(r.qualified); QVERIFY(r.status.contains("observed 13-byte"));
        QVERIFY(field(r,"External pilot mode").contains(name)); QCOMPARE(field(r,"Unknown bytes 5..7"),"01 02 03");
        QCOMPARE(field(r,"Mode data raw"),"aa"); QCOMPARE(field(r,"Unknown bytes 11..12"),"bb cc");
        QCOMPARE(r.payload,b);
        auto full = b + QByteArray(8,char(0xab)); r = decode(126720,full); QVERIFY(r.qualified); QVERIFY(r.status.contains("reference 21-byte"));
        QCOMPARE(r.payload,full); QVERIFY(!decode(126720,b.left(12)).qualified); QVERIFY(!decode(126720,b + char(0)).qualified);
        b[4] = char(0x86); r = decode(126720,b); QVERIFY(!r.qualified); QVERIFY(field(r,"External pilot mode").isEmpty());
    }
    void identificationAndSelectors() {
        auto r = decode(126720,QByteArray::fromHex("3b9ff081901005")); QVERIFY(r.qualified);
        QVERIFY(field(r,"SeaTalk device code").contains("Course Computer")); QCOMPARE(field(r,"Reserved byte 5"),"10");
        r = decode(126720,QByteArray::fromHex("3b9ff08190ff03")); QVERIFY(field(r,"SeaTalk device code").contains("S100"));
        for (const auto &prefix : {"6c0f50", "f0819c", "f00601", "f081ae", "f081a4"}) {
            auto b = QByteArray::fromHex(QByteArray("3b9f") + prefix + "0102030405060708"); r = decode(126720,b);
            QVERIFY(!r.qualified); QCOMPARE(r.payload,b); QVERIFY(field(r,"External pilot mode").isEmpty());
            QVERIFY(!field(r,"Proprietary identifier").isEmpty()); QVERIFY(!field(r,"Raw Data (entire payload)").isEmpty());
        }
    }
    void power_data() {
        QTest::addColumn<int>("value"); QTest::addColumn<QString>("name");
        for (const auto &p : QList<QPair<int,QString>>{{1,"On"},{2,"Off"},{0,"Undefined"},{7,"Undefined"},{255,"Unavailable"},{254,"Error"},{253,"Reserved"}})
            QTest::newRow(qPrintable(QString::number(p.first))) << p.first << p.second;
    }
    void power() {
        QFETCH(int,value); QFETCH(QString,name);
        auto b = QByteArray::fromHex("a399208000"); b[4] = char(value);
        auto r = decode(130820,b); QVERIFY(r.qualified); QVERIFY(field(r,"Power state").contains(name)); QCOMPARE(r.payload,b);
        QVERIFY(!decode(130820,b + char(0)).qualified);
        b[0] ^= 1; QVERIFY(!decode(130820,b).qualified);
        b[0] ^= 1; b[1] ^= 0x20; QVERIFY(!decode(130820,b).qualified);
    }
    void additionalFusionReferenceFields() {
        auto r = decode(130820,QByteArray::fromHex("a399178001")); QVERIFY(r.qualified); QVERIFY(field(r,"Mute value").contains("Mute On"));
        r = decode(130820,QByteArray::fromHex("a399178002")); QVERIFY(field(r,"Mute value").contains("Mute Off"));
        r = decode(130820,QByteArray::fromHex("a3991d8000fffefd")); QVERIFY(r.qualified);
        QVERIFY(field(r,"Zone 1 volume value").startsWith("0 (")); QVERIFY(field(r,"Zone 2 volume value").contains("Unavailable"));
        QVERIFY(field(r,"Zone 3 volume value").contains("Error")); QVERIFY(field(r,"Zone 4 volume value").contains("Reserved"));
        r = decode(130820,QByteArray::fromHex("a399098003e20400")); QVERIFY(r.qualified);
        QCOMPARE(field(r,"Track progress"),"1.25 s (raw 1250)"); QVERIFY(r.variant.endsWith("/source3"));
        r = decode(130820,QByteArray::fromHex("a399098003000000")); QCOMPARE(field(r,"Track progress"),"0 s (raw 0)");
        for (const auto &special : {"ffffff","feffff","fdffff"}) {
            r = decode(130820,QByteArray::fromHex(QByteArray("a399098003") + special));
            QVERIFY(r.qualified); QVERIFY(!field(r,"Track progress").contains(" s (raw"));
        }
    }
    void fusionUnknownAndCollisionIsolation() {
        Nmea2000Diagnostics d;
        const auto power = message(130820,QByteArray::fromHex("a399208002"),10);
        const auto key = d.observe(power); QVERIFY(d.message(key).qualified);
        auto other = power; other.Data[0] ^= 1; const auto otherKey = d.observe(other); QVERIFY(otherKey != key); QVERIFY(!d.message(otherKey).qualified);
        other = power; other.Data[1] ^= 0x20; const auto industryKey = d.observe(other); QVERIFY(industryKey != key);
        auto progress = message(130820,QByteArray::fromHex("a399098001000000"),10); const auto source1 = d.observe(progress);
        progress.Data[4] = 2; QVERIFY(d.observe(progress) != source1);
        for (const auto &b : {QByteArray::fromHex("a3990940aabbcc"), QByteArray::fromHex("a3991040") + QByteArray(15,char(0x12))}) {
            auto r = decode(130820,b); QVERIFY(!r.qualified); QVERIFY(r.status.contains("semantics undefined")); QCOMPARE(r.payload,b);
            QVERIFY(field(r,"Track progress").isEmpty()); QVERIFY(field(r,"Power state").isEmpty());
        }
        auto r = decode(130820,QByteArray::fromHex("a399218000")); QVERIFY(!r.qualified); QVERIFY(r.function.contains("Device",Qt::CaseInsensitive));
    }
    void candidate_data() {
        QTest::addColumn<int>("raw"); QTest::addColumn<QString>("shown");
        for (const auto &p : QList<QPair<int,QString>>{{0,"0 rad"},{1000,"0.1 rad"},{64536,"-0.1 rad"},{65535,"-0.0001 rad"},{32768,"-3.2768 rad"},{32767,"Possible Unavailable"},{32766,"Possible Error"},{32765,"Possible Reserved"}})
            QTest::newRow(qPrintable(QString::number(p.first))) << p.first << p.second;
    }
    void candidate() {
        QFETCH(int,raw); QFETCH(QString,shown);
        const auto m = ::candidate(quint16(raw)); Nmea2000Diagnostics d; const auto r = d.message(d.observe(m));
        QVERIFY(!r.qualified); QVERIFY(r.status.contains("Candidate only")); QVERIFY(r.function.contains("Raymarine")); QVERIFY(!r.function.contains("BEP"));
        QVERIFY(field(r,"Candidate variation hypothesis").contains(shown)); QCOMPARE(r.payload,QByteArray(reinterpret_cast<const char *>(m.Data),8));
        QCOMPARE(field(r,"Unknown bytes 2..2"),"aa"); QCOMPARE(field(r,"Unknown bytes 5..7"),"11 22 33");
        QVERIFY(field(r,"PGN127258 comparison at receipt").contains("No recent"));
        auto bep = m; bep.Data[0] = 0x27; bep.Data[1] = 0x99;
        const auto other = d.message(d.observe(bep)); QVERIFY(!other.qualified); QVERIFY(other.function.contains("BEP")); QVERIFY(field(other,"Candidate variation hypothesis").isEmpty());
    }
    void candidateReferenceOwnershipAndFreshness() {
        Nmea2000Diagnostics d; d.observe(claim(1,42)); d.observe(standard(-.1),0,100);
        auto r = d.message(d.observe(::candidate(64536),0,200)); QVERIFY(field(r,"PGN127258 comparison at receipt").startsWith("Same raw"));
        r = d.message(d.observe(::candidate(1000),0,300)); QVERIFY(field(r,"PGN127258 comparison at receipt").startsWith("Different raw"));
        r = d.message(d.observe(::candidate(64536),0,6000)); QVERIFY(field(r,"PGN127258 comparison at receipt").startsWith("No recent"));
        d.observe(claim(2,42)); r = d.message(d.observe(::candidate(64536,2),0,6100)); QVERIFY(field(r,"PGN127258 comparison at receipt").startsWith("No recent"));
        d.observe(standard(-.1,2),0,6200); r = d.message(d.observe(::candidate(64536,2),0,6300)); QVERIFY(field(r,"PGN127258 comparison at receipt").startsWith("Same raw"));
        d.observe(claim(2,99)); r = d.message(d.observe(::candidate(64536,2),0,6400)); QVERIFY(field(r,"PGN127258 comparison at receipt").startsWith("No recent"));
        d.setSession("replacement","Fixture reconnect"); r = d.message(d.observe(::candidate(64536,2),0,6500)); QVERIFY(field(r,"PGN127258 comparison at receipt").startsWith("No recent"));
        auto invalid = standard(-.1,2);
        for (int i = 0; i < int(tN2kMsg::MaxDataLen); ++i) invalid.Data[i] = 0;
        invalid.DataLen = 224; d.observe(invalid,0,6600);
        r = d.message(d.observe(::candidate(64536,2),0,6700)); QVERIFY(field(r,"PGN127258 comparison at receipt").startsWith("No recent"));
    }
    void identityAndDestinationPreservation() {
        Nmea2000Diagnostics d; d.observe(claim(204,42,2046));
        tN2kMsg product; SetN2kPGN126996(product,2100,1,"Fixture model","v1","hw","fixture-only",0,1); product.Source = 204; d.observe(product);
        auto m = message(126720,QByteArray::fromHex("3b9ff081901005"),204,1);
        const auto key1 = d.observe(m); const auto r = d.message(key1); QVERIFY(r.qualified);
        QCOMPARE(r.receiptIdentity["source_manufacturer"].toInt(),2046); QCOMPARE(r.payloadManufacturer,1851);
        QCOMPARE(r.receiptIdentity["source_model"].toString(),"Fixture model");
        m.Destination = 2; QVERIFY(d.observe(m) != key1);
        d.observe(claim(205,42,2046)); m.Source = 205; const auto moved = d.message(d.observe(m)); QCOMPARE(moved.deviceKey,r.deviceKey);
        d.observe(claim(205,99,2046)); const auto replacement = d.message(d.observe(m)); QVERIFY(replacement.deviceKey != r.deviceKey);
        QCOMPARE(d.message(key1).receiptIdentity,r.receiptIdentity);
    }
    void qualifiedLoggingAndFallback() {
        auto o = ApplicationLog::options(); o.level = 0; ApplicationLog::configure(o);
        Nmea2000Handler h; h.setMode(Viewer); QSignalSpy instruments(&h,&Nmea2000Handler::RcvdPgn);
        h.nmea2000MessageReceived(message(65379,QByteArray::fromHex("3b9f0000000003ff")));
        h.nmea2000MessageReceived(message(126720,QByteArray::fromHex("3b9ff081901005")));
        h.nmea2000MessageReceived(message(130820,QByteArray::fromHex("a399208002"),10));
        QVERIFY(ApplicationLog::flush()); QVERIFY(!read(ApplicationLog::filePath(false)).contains("application decoder unavailable"));
        for (int i = 0; i < 3; ++i) {
            h.nmea2000MessageReceived(message(126720,QByteArray::fromHex("3b9ff0819c010200")));
            h.nmea2000MessageReceived(message(130820,QByteArray::fromHex("a3990940aabbcc"),10));
        }
        QVERIFY(ApplicationLog::flush()); QCOMPARE(read(ApplicationLog::filePath(false)).count("application decoder unavailable"),2);
        QCOMPARE(instruments.size(),0); QVERIFY(!ApplicationLog::recording());
    }
    void noLocalControlOrWidgetSideEffects() {
        MainWindow w; auto *c = w.findChild<MainController *>(); QVERIFY(c); c->removeAllWidgets();
        auto *h = w.findChild<Nmea2000Handler *>(); QVERIFY(h); const auto before = c->captureLayout();
        QSignalSpy routes(h,&Nmea2000Handler::RouteServiceMessage), instruments(h,&Nmea2000Handler::RcvdPgn);
        const QList<tN2kMsg> received{message(65379,QByteArray::fromHex("3b9f4000000003ff")),
            message(126720,QByteArray::fromHex("3b9ff081840102034200000000")),
            message(130820,QByteArray::fromHex("a399208001"),10), ::candidate(1000)};
        for (Mode mode : {Sender,Viewer}) { h->setMode(mode); for (const auto &m : received) h->nmea2000MessageReceived(m); }
        QCOMPARE(routes.size(),8); QCOMPARE(instruments.size(),0); QCOMPARE(c->captureLayout(),before);
        QVERIFY(w.findChildren<GeneratorVesselMovement *>().isEmpty()); QVERIFY(!w.findChild<NmeaSenderWorker *>()->isSending());
        QVERIFY(!ApplicationLog::recording());
    }
    void lengthsAndOpaqueFamilies() {
        for (int pgn : {65379,126720,130820,65311}) for (int length = 0; length <= 32; ++length) {
            auto b = QByteArray(length,char(0xff));
            if (length >= 2) { b[0] = pgn == 130820 ? char(0xa3) : char(0x3b); b[1] = pgn == 130820 ? char(0x99) : char(0x9f); }
            auto r = decode(pgn,b); QCOMPARE(r.payload,b); if (length < 2) QVERIFY(r.malformed);
            if (pgn == 65379 && length != 8) QVERIFY(!r.qualified);
        }
        for (int pgn : {131056,130916,65535}) {
            auto b = QByteArray::fromHex("3b9f010203040506"); auto r = decode(pgn,b);
            QVERIFY(r.status.contains("Opaque")); QCOMPARE(r.payload,b); QVERIFY(!r.qualified);
        }
    }
    void diagnosticsUi() {
        Nmea2000Diagnostics d; d.observe(claim(10,42,419));
        auto m = message(130820,QByteArray::fromHex("a399208002"),10); d.observe(m);
        Nmea2000DiagnosticsDialog dialog(&d); dialog.resize(1100,800); dialog.show(); dialog.refresh();
        auto *table = dialog.findChild<QTableWidget *>("observedPgns"); QVERIFY(table);
        auto *filter = dialog.findChild<QLineEdit *>("diagnosticFilter"); QVERIFY(filter); filter->setText("130820");
        QCOMPARE(table->rowCount(),1); table->setCurrentCell(0,0);
        auto *details = dialog.findChild<QTreeWidget *>(); QVERIFY(details);
        auto *raw = dialog.findChild<QPlainTextEdit *>("diagnosticRaw"); QVERIFY(raw); QCOMPARE(raw->toPlainText(),"a3 99 20 80 02");
        bool power = false; for (int i = 0; i < details->topLevelItemCount(); ++i)
            if (details->topLevelItem(i)->text(0)=="Power state") power = details->topLevelItem(i)->text(1).contains("Off");
        QVERIFY(power);
        if (!qEnvironmentVariableIsEmpty("RAYNMEA_PROPRIETARY_PREVIEW")) QVERIFY(dialog.grab().save(qEnvironmentVariable("RAYNMEA_PROPRIETARY_PREVIEW")));
    }
    void archivedReplay() {
        const auto path = qEnvironmentVariable("RAYNMEA_DIAGNOSTIC_REPLAY"); if (path.isEmpty()) QSKIP("Private archived vessel replay not provided");
        QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly)); const auto records = QJsonDocument::fromJson(f.readAll()).array();
        Nmea2000Diagnostics d; QMap<int,int> counts, qualified;
        for (const auto &record : records) {
            const auto o = record.toObject(); const int pgn = o["pgn"].toInt();
            if (!QSet<int>{65379,126720,130820,65311,127258}.contains(pgn)) continue;
            auto b = QByteArray::fromHex(o["payload_hex"].toString().toLatin1());
            auto row = d.message(d.observe(message(pgn,b,o["source"].toInt(),o["destination"].toInt(255))));
            QCOMPARE(row.payload,b); ++counts[pgn]; if (row.qualified) ++qualified[pgn];
            if (pgn == 65379) QVERIFY(field(row,"External pilot mode").contains("Standby"));
            if (pgn == 130820 && row.qualified) QVERIFY(field(row,"Power state").contains("Off"));
            if (pgn == 65311) { QVERIFY(!row.qualified); QVERIFY(field(row,"Candidate variation hypothesis").contains("unverified")); }
        }
        QCOMPARE(counts[65379],94); QCOMPARE(counts[126720],895); QCOMPARE(counts[130820],139); QCOMPARE(counts[65311],9);
        QCOMPARE(qualified[65379],94); QCOMPARE(qualified[126720],51); QCOMPARE(qualified[130820],94); QCOMPARE(qualified[65311],0);
    }
};
int main(int argc, char **argv)
{
    QTemporaryDir profile; qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QApplication app(argc,argv); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    const auto font = qEnvironmentVariable("RAYNMEA_DIAGNOSTIC_FONT");
    if (!font.isEmpty()) { const int id = QFontDatabase::addApplicationFont(font); const auto names = QFontDatabase::applicationFontFamilies(id); if (!names.isEmpty()) app.setFont(QFont(names.first(),9)); }
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    ProprietaryDiagnosticsTests t; return QTest::qExec(&t,argc,argv);
}
#include "proprietary_diagnostics_tests.moc"
