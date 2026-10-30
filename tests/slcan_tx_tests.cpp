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
#include <QSpinBox>
#include <QLabel>
#include "slcancodec.h"
#include "wavesharesession.h"
#include "nmea2000handler.h"
#include "mainwindow.h"
#include "appsettings.h"
#include "N2kMessages.h"
#include "aisclassbstatic129809.h"
#include "aisclassbstatic129810.h"
#include "aisclassbposition129039.h"
#include "windlassstatus.h"

class TxPort : public QSerialPort {
public:
    QByteArray output, input;
    qint64 chunk = 10000;
    bool acknowledge = true, failWrite = false;
    bool open(OpenMode mode) override { return QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { input.clear(); QIODevice::close(); }
    qint64 bytesAvailable() const override { return input.size(); }
    void feed(const QByteArray &bytes) { input += bytes; emit readyRead(); }
protected:
    qint64 readData(char *data, qint64 n) override {
        n = qMin(n, qint64(input.size())); memcpy(data, input.constData(), n); input.remove(0, n); return n;
    }
    qint64 writeData(const char *data, qint64 n) override {
        if (failWrite) return -1;
        n = qMin(n, chunk); output.append(data, n);
        if (n && acknowledge) QTimer::singleShot(0, this, [this, n] { emit bytesWritten(n); });
        return n;
    }
};
namespace {
WaveshareConfig identity() { return {123456, 44}; }
quint32 pgn(const SlcanCodec::CanFrame &f) {
    auto value = (f.id >> 8) & 0x3ffff;
    return (value & 0xff00) < 0xf000 ? value & 0x3ff00 : value;
}
QList<SlcanCodec::CanFrame> frames(TxPort *p) {
    SlcanCodec::Parser parser; return parser.feed(p->output.mid(SlcanCodec::configuration().size()));
}
QList<SlcanCodec::CanFrame> messages(TxPort *p, quint32 value) {
    QList<SlcanCodec::CanFrame> result;
    for (const auto &f : frames(p)) if (pgn(f) == value) result += f;
    return result;
}
tN2kMsg navigation() { tN2kMsg m; SetN2kLatLonRapid(m, 43.1, -70.2); m.Source=99; return m; }
QAction *start(MainWindow &w) { return w.findChild<QAction*>("actionStart_Sending"); }
QAction *stop(MainWindow &w) { return w.findChild<QAction*>("actionStop_Sending"); }
}
class SlcanTxTests : public QObject {
    Q_OBJECT
    QTemporaryDir profile{QDir::currentPath()+"/build/m231i-profile-XXXXXX"};
    void settings() {
        AppSettings s(APP_COMPANY, APP_NAME); s.clear();
        s.setValue("Nmea2000/Nmea2000Enabled",true);
        s.setValue("Nmea2000/InterfaceType","CANable SLCAN");
        s.setValue("Nmea2000/CanablePort","memory");
        s.setValue("Nmea2000/CanableUniqueNumber",123456);
        s.setValue("Nmea2000/CanablePreferredSource",44); s.sync();
    }
private slots:
    void initTestCase() {
        QApplication::setStyle("Fusion"); // Match the normal application entry point.
        QVERIFY(profile.isValid()); qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
        qputenv("RAYNMEA_TEST_CREDENTIALS","1");
    }
    void independentWireFixtures() {
        QCOMPARE(SlcanCodec::configuration().toHex(),QByteArray("430d53350d41300d4d300d4f0d"));
        SlcanCodec::CanFrame f;f.id=0x09f7092b;f.length=8;f.data={255,0,240,0,0,30,0,1};
        QCOMPARE(SlcanCodec::encode(f).toHex(),QByteArray("54303946373039324238464630304630303030303145303030310d"));
        for(int n=0;n<=8;++n) for(quint32 id:{0u,0x1fffffffu}) {
            f.id=id;f.length=n;SlcanCodec::Parser parser;auto result=parser.feed(SlcanCodec::encode(f));
            QCOMPARE(result.size(),1);QCOMPARE(result[0].id,id);QCOMPARE(result[0].length,n);
        }
        f.id=0x20000000;QVERIFY(SlcanCodec::encode(f).isEmpty());
        f.id=1;f.length=9;QVERIFY(SlcanCodec::encode(f).isEmpty());
    }
    void fragmentedMalformedAndAdapterErrors() {
        SlcanCodec::CanFrame f;f.id=0x09f7092b;f.length=8;f.data={255,0,240,0,0,30,0,1};
        auto bytes=SlcanCodec::encode(f);
        for(int split=0;split<=bytes.size();++split) {
            SlcanCodec::Parser p;auto frames=p.feed(bytes.left(split));frames+=p.feed(bytes.mid(split));
            QCOMPARE(frames.size(),1);QCOMPARE(frames[0].id,f.id);
        }
        SlcanCodec::Parser p;
        for(const auto &bad:QList<QByteArray>{"T200000000\r","T00000001900\r","T000000018AABB\r","T000000011GG\r","t123100\r","R000001230\r","D000001230\r"})
            QVERIFY(p.feed(bad).isEmpty());
        QVERIFY(p.feed(QByteArray(100000,'x')+"\r").isEmpty());QVERIFY(p.bufferedBytes()<=26);
        QCOMPARE(p.feed(bytes+bytes).size(),2);QVERIFY(p.rejectedBytes()>100000);
        p.feed("\a");QVERIFY(p.adapterError());p.reset();QVERIFY(!p.adapterError());
    }
    void viewerRxAndBellStops() {
        TxPort *p=nullptr;WaveshareSession s(nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});s.setProtocol(WaveshareSession::CanableSlcan);
        QSignalSpy received(&s,&WaveshareSession::received);QVERIFY(s.start("memory"));QTRY_COMPARE(s.state(),WaveshareSession::AwaitingTraffic);
        SlcanCodec::CanFrame f;f.id=0x09f7092b;f.length=8;f.data={255,0,240,0,0,30,0,1};
        p->feed(SlcanCodec::encode(f));QTRY_COMPARE(received.size(),1);QCOMPARE(s.state(),WaveshareSession::Receiving);
        QCOMPARE(p->output,SlcanCodec::configuration());QVERIFY(!s.send(navigation()));
        p->feed("\a");QCOMPARE(s.state(),WaveshareSession::Failed);QVERIFY(!p->isOpen());
        QVERIFY(s.startTransmitting("memory",identity()));QTRY_VERIFY(s.isTransmitting());
        QVERIFY(s.status().contains("CANable SLCAN TX"));QVERIFY(s.status().contains("locally claimed"));QVERIFY(s.status().contains("Sending active"));s.stop();QVERIFY(p->output.endsWith("C\r"));
    }
    void separateTransportIdentityAndProtocolSwitch() {
        AppSettings settings(APP_COMPANY,APP_NAME);settings.clear();settings.beginGroup("Nmea2000");
        settings.setValue("WaveshareUniqueNumber",654321);settings.setValue("CanableUniqueNumber",123456);
        QCOMPARE(WaveshareConfig::load(settings).uniqueNumber,654321u);
        QCOMPARE(WaveshareConfig::load(settings,"Canable").uniqueNumber,123456u);
        settings.setValue("CanableUniqueNumber",0);QVERIFY(!WaveshareConfig::load(settings,"Canable").valid());
        QVERIFY(WaveshareConfig::load(settings).valid());
        TxPort *p=nullptr;Nmea2000Handler h(nullptr,nullptr,nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});
        h.SetInterfaceType("CANable SLCAN");h.setMode(Sender);h.ConfigureWaveshare("memory",identity());h.StartWaveshareSending();
        QTRY_VERIFY(h.IsWaveshareReady());auto old=QPointer<TxPort>(p);
        h.SetInterfaceType("Waveshare USB-CAN-A");QVERIFY(!old || !old->isOpen());QVERIFY(!h.IsWaveshareConfigured());
        h.ConfigureWaveshare("memory",identity());h.StartWaveshareSending();QTRY_VERIFY(h.IsWaveshareReady());
        QCOMPARE(p->output.left(20),WaveshareCodec::transmitConfiguration());
        QCOMPARE(h.findChild<WaveshareSession*>()->protocol(),WaveshareSession::Waveshare);
    }
    void configurationDeadlineHasOneDeterministicCause() {
        for(auto protocol:{WaveshareSession::Waveshare,WaveshareSession::CanableSlcan}) {
            TxPort *p=nullptr;int opened=0;
            WaveshareSession s(nullptr,[&](QObject *o){++opened;p=new TxPort;p->setParent(o);p->acknowledge=false;return p;});
            s.setProtocol(protocol);QSignalSpy failure(&s,&WaveshareSession::failed);
            QVERIFY(s.startTransmitting("memory",identity()));QPointer<TxPort> guarded=p;
            QTRY_COMPARE_WITH_TIMEOUT(s.state(),WaveshareSession::Failed,3000);
            QCOMPARE(failure.size(),1);QVERIFY(s.status().contains("did not complete"));
            QVERIFY(!guarded || !guarded->isOpen());QTest::qWait(40);QCOMPARE(opened,1);
        }
    }
    void setupTransportVisibility() {
        settings();DialogSetup setup;setup.show();QTest::qWait(20);
        QVERIFY(setup.findChild<QWidget*>("socketCanSettings")->isHidden());
        QVERIFY(!setup.findChild<QComboBox*>("canablePort")->parentWidget()->isHidden());
        QVERIFY(setup.findChild<QComboBox*>("wavesharePort")->parentWidget()->isHidden());setup.reject();
    }
    void identityPersistenceAndValidation() {
        AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.beginGroup("Nmea2000");
        const auto first=WaveshareConfig::load(s,"Canable");QVERIFY(first.valid());QCOMPARE(first.preferredSource,44);
        QCOMPARE(WaveshareConfig::load(s,"Canable").uniqueNumber,first.uniqueNumber);
        s.setValue("CanableUniqueNumber",0);QVERIFY(!WaveshareConfig::load(s,"Canable").valid());
        s.setValue("CanableUniqueNumber",2097152);QVERIFY(!WaveshareConfig::load(s,"Canable").valid());
        s.setValue("CanableUniqueNumber",1234);s.setValue("CanablePreferredSource",252);QVERIFY(!WaveshareConfig::load(s,"Canable").valid());
        s.setValue("CanablePreferredSource","invalid");QVERIFY(!WaveshareConfig::load(s,"Canable").valid());
        WaveshareSession session;QVERIFY(!session.startTransmitting("memory",{})); // no port access
    }
    void claimBeforeInstrumentOutput() {
        TxPort *p=nullptr;WaveshareSession s(nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});s.setProtocol(WaveshareSession::CanableSlcan);
        QVERIFY(!s.send(navigation()));QVERIFY(s.startTransmitting("memory",identity()));
        QVERIFY(!s.send(navigation()));QCOMPARE(p->output,SlcanCodec::configuration());
        QTRY_VERIFY(s.isTransmitting());QCOMPARE(messages(p,60928).size(),1);
        const auto claim=messages(p,60928).first();QCOMPARE(claim.id&255,44u);QCOMPARE(claim.length,8);
        quint64 name=0;for(int i=0;i<8;++i)name|=quint64(claim.data[i])<<(8*i);
        QCOMPARE(name&0x1fffff,quint64(123456));QCOMPARE((name>>21)&2047,quint64(2046));
        QVERIFY(s.send(navigation()));QTRY_COMPARE(messages(p,129025).size(),1);
        QCOMPARE(messages(p,129025).first().id&255,44u); // message Source 99 is not CAN device identity
    }
    void lowerNameConflictMovesAddress() {
        TxPort *p=nullptr;WaveshareSession s(nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});s.setProtocol(WaveshareSession::CanableSlcan);
        s.startTransmitting("memory",identity());QTRY_VERIFY(s.isTransmitting());
        SlcanCodec::CanFrame claim;claim.id=0x18eeff2c;claim.length=8; // lower NAME wins
        p->feed(SlcanCodec::encode(claim));
        QTRY_VERIFY(messages(p,60928).size()>=2);QTRY_VERIFY(s.isTransmitting());
        QVERIFY(s.send(navigation()));QTRY_COMPARE(messages(p,129025).size(),1);
        QVERIFY((messages(p,129025).first().id&255)!=44);QVERIFY((messages(p,129025).first().id&255)!=43);
        QVERIFY(s.status().contains("source 45"));
    }
    void higherNameConflictKeepsAddress() {
        TxPort *p=nullptr;WaveshareSession s(nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});s.setProtocol(WaveshareSession::CanableSlcan);
        s.startTransmitting("memory",identity());QTRY_VERIFY(s.isTransmitting());
        SlcanCodec::CanFrame claim;claim.id=0x18eeff2c;claim.length=8;claim.data.fill(255);
        p->feed(SlcanCodec::encode(claim));QTRY_VERIFY(messages(p,60928).size()>=2);QTRY_VERIFY(s.isTransmitting());
        QVERIFY(s.send(navigation()));QTRY_COMPARE(messages(p,129025).size(),1);QCOMPARE(messages(p,129025).first().id&255,44u);
    }
    void allQualificationPgnsThroughPinnedCore() {
        TxPort *p=nullptr;Nmea2000Handler h(nullptr,nullptr,nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});
        h.SetInterfaceType("CANable SLCAN");h.setMode(Sender);h.ConfigureWaveshare("memory",identity());
        QVERIFY(!p);h.StartWaveshareSending();QTRY_VERIFY(h.IsWaveshareReady());
        QList<tN2kMsg> expected;
        tN2kMsg m;SetN2kPGN129809(m,24,N2kaisr_Initial,234567890,"SIMULATED A",N2kaisown_information_not_broadcast,255);expected+=m;
        SetN2kPGN129810(m,24,N2kaisr_Initial,234567890,37,"VENDOR","ABC123",10.668,3.6576,N2kDoubleNA,N2kDoubleNA,0,N2kaisown_information_not_broadcast,255);expected+=m;
        // Reuse accepted position codec fixture bytes, including unavailable dimensions/flags.
        m.SetPGN(129039);m.Priority=4;m.DataLen=27;memset(m.Data,0xff,27);expected+=m;
        m.SetPGN(128777);m.Priority=2;m.DataLen=8;memcpy(m.Data,QByteArray::fromHex("ff00f000001e0001").constData(),8);expected+=m;
        m.SetPGN(128778);m.Priority=2;m.DataLen=8;memcpy(m.Data,QByteArray::fromHex("ff000000000000ff").constData(),8);expected+=m;
        expected+=navigation();SetN2kEngineParamRapid(m,1,1200,N2kDoubleNA,N2kInt8NA);expected+=m;
        // Direct session uses the same core as handler dispatch below.
        auto *session=h.findChild<WaveshareSession*>();QVERIFY(session);
        for(const auto &original:expected) QVERIFY(session->send(original));
        QTRY_VERIFY(messages(p,127488).size()==1);
        for(const auto &original:expected) {
            auto sent=messages(p,original.PGN);QVERIFY(!sent.isEmpty());QByteArray payload;
            if(original.DataLen<=8) payload=QByteArray(reinterpret_cast<const char*>(sent[0].data.data()),sent[0].length);
            else {
                QCOMPARE(sent[0].data[1],original.DataLen);
                for(int i=0;i<sent.size();++i) {QCOMPARE(sent[i].data[0]&31,i);for(int j=i?1:2;j<8;++j)payload+=char(sent[i].data[j]);}
                payload.truncate(original.DataLen);
            }
            QCOMPARE(payload,QByteArray(reinterpret_cast<const char*>(original.Data),original.DataLen));
            for(const auto &f:sent){QCOMPARE(f.id&255,44u);QCOMPARE(f.id>>26,quint32(original.Priority));}
        }
        QVERIFY(h.sendPgn(129025,{"Latitude:42","Longitude:-70"}));QTRY_COMPARE(messages(p,129025).size(),2);
        QVERIFY(h.sendPgn(128777,{"SID:255","WindlassIdentifier:0","RodeCounterValue:15","WindlassLineSpeed:2","WindlassMotionStatus:0","RodeTypeStatus:0","AnchorDockingStatus:1","OperatingEvents:0"}));
        QTRY_COMPARE(messages(p,128777).size(),2);
    }
    void qualificationPgnsViaNormalDispatch() {
        TxPort *p=nullptr;Nmea2000Handler h(nullptr,nullptr,nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});
        h.SetInterfaceType("CANable SLCAN");h.ConfigureWaveshare("memory",identity());h.setMode(Sender);
        h.StartWaveshareSending();QTRY_VERIFY(h.IsWaveshareReady());
        for(int value:{128777,128778,129809,129810,129039}) {
            QList<QList<QString>> rows = value==129809 ? AisClassBStatic129809::parameters()
                : value==129810 ? AisClassBStatic129810::parameters()
                : value==129039 ? AisClassBPosition129039::parameters() : WindlassStatus::parameters(value);
            QStringList fields;for(const auto &r:rows)fields << r[0]+":"+(r[0]=="UserID" ? "211000001" : r[1]);
            QVERIFY2(h.sendPgn(value,fields),qPrintable(QString::number(value)));
            QTRY_VERIFY(!messages(p,value).isEmpty());
            for(const auto &f:messages(p,value))QCOMPARE(f.id&255,44u);
        }
    }
    void reentrantStopAndViewerOpenDuringClaim() {
        TxPort *p=nullptr;WaveshareSession s(nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});s.setProtocol(WaveshareSession::CanableSlcan);
        bool cancelled=false;
        connect(&s,&WaveshareSession::statusChanged,&s,[&](const QString &text){
            if(!cancelled && text.contains("source claim pending")) {cancelled=true;s.stop();s.start("memory-rx");}
        });
        QVERIFY(s.startTransmitting("memory",identity()));QTRY_VERIFY(cancelled);
        QTRY_COMPARE(s.state(),WaveshareSession::AwaitingTraffic);QTest::qWait(300);
        QCOMPARE(p->output,SlcanCodec::configuration());QVERIFY(!s.send(navigation()));
    }
    void partialAndTemporarilyBlockedWrites() {
        TxPort *p=nullptr;WaveshareSession s(nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);p->chunk=2;return p;});s.setProtocol(WaveshareSession::CanableSlcan);
        s.startTransmitting("memory",identity());QTRY_VERIFY(s.isTransmitting());
        QCOMPARE(p->output.left(SlcanCodec::configuration().size()),SlcanCodec::configuration());QCOMPARE(messages(p,60928).size(),1);
        p->chunk=0;QVERIFY(s.send(navigation()));QTest::qWait(30);QCOMPARE(messages(p,129025).size(),0);
        p->chunk=1;QTRY_COMPARE(messages(p,129025).size(),1);QCOMPARE(s.state(),WaveshareSession::Transmitting);
    }
    void stalledAndOverflowStopWithoutReplay() {
        TxPort *p=nullptr;WaveshareSession s(nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});s.setProtocol(WaveshareSession::CanableSlcan);
        s.startTransmitting("memory",identity());QTRY_VERIFY(s.isTransmitting());p->chunk=0;
        QVERIFY(s.send(navigation()));auto stalled=QPointer<TxPort>(p);QTRY_COMPARE_WITH_TIMEOUT(s.state(),WaveshareSession::Failed,3000);
        QVERIFY(!stalled || !stalled->isOpen());QVERIFY(!s.send(navigation()));
        s.startTransmitting("memory",identity());QTRY_VERIFY(s.isTransmitting());p->chunk=0;
        for(int i=0;i<300 && s.state()!=WaveshareSession::Failed;++i)s.send(navigation());
        QCOMPARE(s.state(),WaveshareSession::Failed);QVERIFY(s.status().contains("overflow"));
        s.startTransmitting("memory",identity());QTRY_VERIFY(s.isTransmitting());QCOMPARE(messages(p,129025).size(),0);
    }
    void errorAndConfigurationCancellation() {
        TxPort *p=nullptr;int opened=0;WaveshareSession s(nullptr,[&](QObject *o){++opened;p=new TxPort;p->setParent(o);return p;});s.setProtocol(WaveshareSession::CanableSlcan);
        s.startTransmitting("memory",identity());auto cancelled=QPointer<TxPort>(p);s.stop();QTest::qWait(30);QVERIFY(!cancelled || !cancelled->isOpen());QCOMPARE(s.state(),WaveshareSession::Closed);
        s.startTransmitting("memory",identity());QTRY_VERIFY(s.isTransmitting());
        emit p->errorOccurred(QSerialPort::ResourceError);QCOMPARE(s.state(),WaveshareSession::Failed);QTest::qWait(50);QCOMPARE(opened,2);
        s.startTransmitting("memory",identity());QTRY_VERIFY(s.isTransmitting());p->failWrite=true;QVERIFY(!s.send(navigation()));QCOMPARE(s.state(),WaveshareSession::Failed);
    }
    void repeatedStartStopAndModeIsolation() {
        TxPort *p=nullptr;Nmea2000Handler h(nullptr,nullptr,nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});
        h.SetInterfaceType("CANable SLCAN");h.ConfigureWaveshare("memory",identity());h.setMode(Sender);
        for(int i=0;i<5;++i) {
            h.StartWaveshareSending();QTRY_VERIFY(h.IsWaveshareReady());QCOMPARE(messages(p,60928).size(),1);
            QVERIFY(h.sendPgn(129025,{"Latitude:42","Longitude:-70"}));h.StopWaveshare();auto old=QPointer<TxPort>(p);QTest::qWait(20);
            QVERIFY(!old || !old->isOpen());QVERIFY(!h.IsWaveshareReady());
        }
        h.setMode(Viewer);QTest::qWait(30);QVERIFY(p->isOpen());QCOMPARE(p->output,SlcanCodec::configuration());
        h.StartWaveshareSending();QCOMPARE(p->output.size(),SlcanCodec::configuration().size());h.setMode(Sender);QVERIFY(!p->isOpen());
        h.ReconnectWaveshare();QVERIFY(!p->isOpen());
    }
    void applicationAuthorizationEnableIntervalsAndF12() {
        settings();TxPort *p=nullptr;int opened=0;
        MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){++opened;p=new TxPort;p->setParent(o);return p;});w.show();
        QCOMPARE(opened,0);auto *model=w.findChild<PgnTableModel*>();QVERIFY(model);model->clearModel();
        model->addPgn(99001,"fixture","129025",false,{{"Latitude","42","deg","double"},{"Longitude","-70","deg","double"}}, {},1000);
        QVERIFY(start(w)->isEnabled());start(w)->trigger();QTRY_VERIFY(start(w)->isChecked());QTest::qWait(300);
        QCOMPARE(messages(p,129025).size(),0);model->setPgnEnabled(99001,true);
        QTRY_COMPARE(messages(p,129025).size(),1);QTest::qWait(300);QCOMPARE(messages(p,129025).size(),1);
        model->changeParam(99001,"Latitude","43");QTRY_VERIFY_WITH_TIMEOUT(messages(p,129025).size()>=2,1800);
        const auto sent=messages(p,129025);QVERIFY(sent.first().data!=sent.last().data);
        p->chunk=0;QTest::keyClick(&w,Qt::Key_F12);QVERIFY(!p->isOpen());QVERIFY(!start(w)->isChecked());
        start(w)->trigger();QTRY_VERIFY(start(w)->isChecked());QVERIFY(messages(p,129025).isEmpty());stop(w)->trigger();QVERIFY(!p->isOpen());
    }
    void layoutReplacementStopsAndDoesNotResume() {
        settings();TxPort *p=nullptr;int opened=0;MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){++opened;p=new TxPort;p->setParent(o);return p;});
        start(w)->trigger();QTRY_VERIFY(start(w)->isChecked());emit w.findChild<MainController*>()->layoutAboutToReplace();
        QVERIFY(!p->isOpen());QTest::qWait(350);QCOMPARE(opened,1);QVERIFY(!start(w)->isChecked());
        start(w)->trigger();QTRY_VERIFY(start(w)->isChecked());w.findChild<QAction*>("actionViewer_Rx")->setChecked(true);
        QTest::qWait(30);QCOMPARE(p->output,SlcanCodec::configuration());QVERIFY(!start(w)->isEnabled());
    }
    void nativeTransmitPresentation() {
        const auto directory=qEnvironmentVariable("M231I_EVIDENCE");
        for(const auto &theme:QStringList{"Dark","Light"}) {
            settings();{AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.setValue("Preference/FontSize",12);}
            bool acknowledge=false;TxPort *p=nullptr;
            MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);p->acknowledge=acknowledge;return p;});
            w.resize(1400,950);w.show();start(w)->trigger();QTest::qWait(30);
            QVERIFY(!start(w)->isChecked());QVERIFY(stop(w)->isEnabled());
            QVERIFY(w.findChild<QLabel*>("sendingStatus")->text().contains("configuring"));
            if(!directory.isEmpty()){QDir().mkpath(directory);QVERIFY(w.grab().save(directory+"/tx-configuring-"+theme+".png"));}
            stop(w)->trigger();acknowledge=true;start(w)->trigger();QTRY_VERIFY(start(w)->isChecked());
            QVERIFY(w.findChild<QLabel*>("sendingStatus")->toolTip().contains("source 44"));
            if(!directory.isEmpty())QVERIFY(w.grab().save(directory+"/tx-claimed-"+theme+".png"));
            stop(w)->trigger();w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();
            auto *setup=w.findChild<DialogSetup*>();QVERIFY(setup);setup->findChild<QTabWidget*>("tabWidget")->setCurrentIndex(1);
            setup->resize(1100,900);QTest::qWait(30);
            if(!directory.isEmpty())QVERIFY(setup->grab().save(directory+"/tx-setup-"+theme+".png"));setup->reject();
        }
    }
    void setupIdentitySaveCancelAndPresentation() {
        settings();DialogSetup setup;setup.show();QTest::qWait(20);
        auto *number=setup.findChild<QSpinBox*>("canableIdentity"),*source=setup.findChild<QSpinBox*>("canableSource");QVERIFY(number&&source);
        QCOMPARE(number->value(),123456);QCOMPARE(source->value(),44);number->setValue(98765);source->setValue(45);setup.reject();
        setup.show();QTest::qWait(20);QCOMPARE(number->value(),123456);QCOMPARE(source->value(),44);
        number->setValue(98765);source->setValue(45);QVERIFY(QMetaObject::invokeMethod(&setup,"on_pushButtonSave_clicked",Qt::DirectConnection));
        AppSettings s(APP_COMPANY,APP_NAME);QCOMPARE(s.value("Nmea2000/CanableUniqueNumber").toUInt(),98765u);QCOMPARE(s.value("Nmea2000/CanablePreferredSource").toInt(),45);
        setup.reject();
    }
};
QTEST_MAIN(SlcanTxTests)
#include "slcan_tx_tests.moc"
