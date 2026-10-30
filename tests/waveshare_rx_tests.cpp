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
#include <QPointer>
#include <QTreeView>
#include <QtEndian>
#include "wavesharecodec.h"
#include "wavesharesession.h"
#include "nmea2000handler.h"
#include "mainwindow.h"
#include "appsettings.h"
#include "pgntablemodel.h"
#include "pgntreecontroller.h"
#include "themes.h"
#include <QDir>
#include <QLabel>
#include "aisclassbstatic129809.h"
#include "aisclassbstatic129810.h"
#include "aisclassbposition129039.h"
#include "N2kMessages.h"

class MemoryWavePort : public QSerialPort {
public:
    QByteArray input, output;
    bool failWrite = false, acknowledge = true;
    bool open(OpenMode mode) override { return QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { input.clear(); QIODevice::close(); }
    qint64 bytesAvailable() const override { return input.size(); }
    void feed(const QByteArray &b) { input += b; emit readyRead(); }
protected:
    qint64 readData(char *b, qint64 n) override {
        n = qMin(n, qint64(input.size())); memcpy(b, input.constData(), n); input.remove(0, n); return n;
    }
    qint64 writeData(const char *b, qint64 n) override {
        if (failWrite) return -1;
        output.append(b, n);
        if (acknowledge) QTimer::singleShot(0, this, [this, n] { emit bytesWritten(n); });
        return n;
    }
};

class WaveshareTests : public QObject {
    Q_OBJECT
    QTemporaryDir profile{QDir::currentPath()+"/build/m231f-profile-XXXXXX"};
    static QByteArray wire(quint32 id, QByteArray data) {
        QByteArray b; b.append(char(0xaa)); b.append(char(0xe0 | data.size()));
        for (int i = 0; i < 4; ++i) b.append(char(id >> (8*i)));
        return b + data + char(0x55);
    }
    static QByteArray packets(const tN2kMsg &msg) {
        const quint32 id = quint32(msg.Priority) << 26 | quint32(msg.PGN) << 8 | msg.Source;
        QByteArray data(reinterpret_cast<const char *>(msg.Data), msg.DataLen);
        if (data.size() <= 8) return wire(id, data);
        QByteArray result; int offset = 0, index = 0;
        while (offset < data.size()) {
            QByteArray part(1, char(0x20 | index));
            if (index == 0) part.append(char(data.size()));
            const int n = qMin(8-part.size(), data.size()-offset);
            part += data.mid(offset, n); offset += n;
            while (part.size() < 8) part.append(char(0xff));
            result += wire(id, part); ++index;
        }
        return result;
    }
    void settings() {
        AppSettings s(APP_COMPANY, APP_NAME); s.clear();
        s.setValue("Nmea2000/Nmea2000Enabled", true);
        s.setValue("Nmea2000/InterfaceType", "Waveshare USB-CAN-A");
        s.setValue("Nmea2000/WavesharePort", "memory");
    }
private slots:
    void initTestCase() {
        QVERIFY(profile.isValid()); qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
        qputenv("RAYNMEA_TEST_CREDENTIALS", "1");
    }
    void exactIndependentFixtures() {
        QCOMPARE(WaveshareCodec::receiveConfiguration().toHex(), QByteArray("aa5512050200000000000000000201000000001c"));
        WaveshareCodec::Parser p;
        const auto f = p.feed(QByteArray::fromHex("aae82b09f709ff00f000001e000155"));
        QCOMPARE(f.size(), 1); QCOMPARE(f[0].id, 0x09f7092bu); QCOMPARE(f[0].length, 8);
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(f[0].data.data()),8).toHex(), QByteArray("ff00f000001e0001"));
    }
    void allLengthsAndFragments() {
        for (int n = 0; n <= 8; ++n) for (quint32 id : {0u, 0x1fffffffu}) {
            const auto b = wire(id, QByteArray::fromHex("aa55aa55aa55aa55").left(n));
            for (int split = 0; split <= b.size(); ++split) {
                WaveshareCodec::Parser p; auto frames = p.feed(b.left(split)); frames += p.feed(b.mid(split));
                QCOMPARE(frames.size(),1); QCOMPARE(frames[0].id,id); QCOMPARE(frames[0].length,n); QCOMPARE(p.bufferedBytes(),0);
            }
        }
    }
    void malformedAndBoundedRecovery() {
        WaveshareCodec::Parser p;
        auto good = QByteArray::fromHex("aae82b09f709ff00f000001e000155");
        QByteArray invalid = QByteArray(100000, 'x') + QByteArray::fromHex("aae90000000055aac800000000000000000055aaf00000000055aae00000002055");
        p.feed(invalid); QVERIFY(p.bufferedBytes() < 15); QVERIFY(p.rejectedBytes() > 99999);
        p.reset(); QCOMPARE(p.feed(good + good).size(),2);
        for (int i=0;i<1000;++i) { auto bad=good; bad[bad.size()-1]=0; QCOMPARE(p.feed(bad+good).size(),1); }
    }
    void portOpenIsNotReadyAndNoCanTx() {
        MemoryWavePort *port=nullptr; WaveshareSession s(nullptr,[&](QObject *o){port=new MemoryWavePort;port->setParent(o);return port;});
        QVERIFY(s.start("memory")); QCOMPARE(s.state(),WaveshareSession::Configuring);
        QTRY_COMPARE(s.state(),WaveshareSession::AwaitingTraffic);
        QTest::qWait(300); QCOMPARE(port->output,WaveshareCodec::receiveConfiguration()); QCOMPARE(s.blockedCanWrites(),0u);
        port->feed(QByteArray::fromHex("aae82b09f709ff00f000001e000155"));
        QTRY_COMPARE(s.state(),WaveshareSession::Receiving);
        port->feed(wire(0x18eaff2b,QByteArray::fromHex("14f001"))+wire(0x18eeff2b,QByteArray(8,char(0xff))));
        QTest::qWait(220);QCOMPARE(port->output.size(),20);QCOMPARE(s.blockedCanWrites(),0u);
    }
    void singleFrameThroughHandler() {
        MemoryWavePort *port=nullptr;
        Nmea2000Handler h(nullptr,nullptr,nullptr,[&](QObject *o){port=new MemoryWavePort;port->setParent(o);return port;});
        QSignalSpy spy(&h,&Nmea2000Handler::RcvdPgn);
        h.SetInterfaceType("Waveshare USB-CAN-A");h.setMode(Viewer);h.ConfigureWaveshare("memory");QTest::qWait(20);
        port->feed(QByteArray::fromHex("aae82b09f709ff00f000001e000155aae82b0af709ff000000000000ff55"));
        QTRY_COMPARE(spy.size(),2); QCOMPARE(spy[0][0].toString(),QString("128777")); QCOMPARE(spy[1][0].toString(),QString("128778"));
        QCOMPARE(spy[0][2].toString(),QString("43")); QCOMPARE(port->output.size(),20);
    }
    void fastPacketsMultipleSources() {
        MemoryWavePort *port=nullptr;Nmea2000Handler h(nullptr,nullptr,nullptr,[&](QObject *o){port=new MemoryWavePort;port->setParent(o);return port;});
        QSignalSpy spy(&h,&Nmea2000Handler::RcvdPgn);h.SetInterfaceType("Waveshare USB-CAN-A");h.setMode(Viewer);h.ConfigureWaveshare("memory");QTest::qWait(20);
        tN2kMsg a,b,c; SetN2kPGN129809(a,24,N2kaisr_Initial,234567890,"SIMULATED ONE",N2kaischannel_B_VDL_reception,255);
        SetN2kPGN129810(b,24,N2kaisr_Initial,234567891,37,"VENDOR","AB123",10.668,3.6576,N2kDoubleNA,N2kDoubleNA,0,N2kaischannel_B_VDL_reception,255);
        SetN2kPGN129039(c,18,N2kaisr_Initial,234567890,43.1,-70.2,false,false,10,1.2,3.,N2kaischannel_B_VDL_reception,1.2,N2kaisunit_ClassB_CS,false,false,false,true,N2kaismode_Autonomous,false);
        a.Source=43;b.Source=44;c.Source=43;
        port->feed(packets(a)+packets(b)+packets(c));QTRY_COMPARE(spy.size(),3);
        QCOMPARE(spy[0][0].toString(),QString("129809"));QCOMPARE(spy[1][2].toString(),QString("44"));QCOMPARE(spy[2][0].toString(),QString("129039"));
        QCOMPARE(port->output.size(),20);
    }
    void reconnectClearsIncompleteAndStaleCallbacks() {
        MemoryWavePort *port=nullptr;WaveshareSession s(nullptr,[&](QObject *o){port=new MemoryWavePort;port->setParent(o);return port;});
        QSignalSpy spy(&s,&WaveshareSession::received);s.start("first");QTest::qWait(20);auto old=QPointer<MemoryWavePort>(port);
        port->feed(QByteArray::fromHex("aae82b09f709ff00"));s.stop();s.start("second");QTest::qWait(20);
        if(old) old->feed(QByteArray::fromHex("f000001e000155"));
        port->feed(QByteArray::fromHex("aae82b09f709ff00f000001e000155"));QTRY_COMPARE(spy.size(),1);
        QCOMPARE(port->output.size(),20);
    }
    void serialFailureStopsWithoutRetry() {
        MemoryWavePort *port=nullptr;int created=0;WaveshareSession s(nullptr,[&](QObject *o){++created;port=new MemoryWavePort;port->setParent(o);return port;});
        s.start("memory");QTest::qWait(20);emit port->errorOccurred(QSerialPort::ResourceError);
        QCOMPARE(s.state(),WaveshareSession::Failed);QTest::qWait(50);QCOMPARE(created,1);
        QVERIFY(s.start("memory"));QCOMPARE(created,2);
    }
    void configurationFailureAndDeadline() {
        int created=0;
        WaveshareSession bad(nullptr,[&](QObject *o){++created;auto *p=new MemoryWavePort;p->setParent(o);p->failWrite=true;return p;});
        QVERIFY(!bad.start("None"));QCOMPARE(created,0);QVERIFY(!bad.start("memory"));QCOMPARE(bad.state(),WaveshareSession::Failed);
        WaveshareSession stalled(nullptr,[](QObject *o){auto *p=new MemoryWavePort;p->setParent(o);p->acknowledge=false;return p;});
        QVERIFY(stalled.start("memory"));QTRY_COMPARE_WITH_TIMEOUT(stalled.state(),WaveshareSession::Failed,3000);
        QVERIFY(stalled.status().contains("did not complete"));
    }
    void queueOverflowFailsClosed() {
        MemoryWavePort *p=nullptr;WaveshareSession s(nullptr,[&](QObject *o){p=new MemoryWavePort;p->setParent(o);return p;});
        s.start("memory");QTest::qWait(20);
        const auto frame=QByteArray::fromHex("aae82b09f709ff00f000001e000155");
        for(int n=0;n<6 && s.state()!=WaveshareSession::Failed;++n)p->feed(frame.repeated(200));
        QCOMPARE(s.state(),WaveshareSession::Failed);QVERIFY(!p->isOpen());QVERIFY(s.status().contains("overflow"));
    }
    void fastPacketReconnectAndReentrantStop() {
        MemoryWavePort *p=nullptr;WaveshareSession s(nullptr,[&](QObject *o){p=new MemoryWavePort;p->setParent(o);return p;});
        QSignalSpy spy(&s,&WaveshareSession::received);tN2kMsg a;SetN2kPGN129809(a,24,N2kaisr_Initial,234567890,"SIMULATED",N2kaischannel_B_VDL_reception,255);a.Source=43;
        const auto bytes=packets(a);s.start("one");QTest::qWait(220);p->feed(bytes.left(15));QTest::qWait(30);
        s.start("two");QTest::qWait(220);p->feed(bytes.mid(15));QTest::qWait(30);QCOMPARE(spy.size(),0);
        connect(&s,&WaveshareSession::received,&s,[&]{s.stop();});p->feed(bytes+bytes);QTRY_COMPARE(spy.size(),1);QCOMPARE(s.state(),WaveshareSession::Closed);
    }
    void viewerModelsTreeAndIdenticalSamples() {
        settings();MemoryWavePort *p=nullptr;MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){p=new MemoryWavePort;p->setParent(o);return p;});
        auto *m=w.findChild<PgnTableController*>()->getPgnTableModel();auto *tree=w.findChild<PgnTreeController*>()->getTreeModel();
        w.findChild<QAction*>("actionViewer_Rx")->setChecked(true);QTest::qWait(20);
        tN2kMsg a,b,engine,nav;SetN2kPGN129809(a,24,N2kaisr_Initial,234567890,"SIMULATED A",N2kaischannel_B_VDL_reception,255);a.Source=43;
        SetN2kPGN129809(b,24,N2kaisr_Initial,234567891,"SIMULATED B",N2kaischannel_B_VDL_reception,255);b.Source=43;
        SetN2kEngineParamRapid(engine,1,1200,N2kDoubleNA,N2kInt8NA);engine.Source=44;
        SetN2kLatLonRapid(nav,43.1,-70.2);nav.Source=44;
        p->feed(packets(a)+packets(b)+packets(engine)+packets(nav)+QByteArray::fromHex("aae82b0af709ff000000000000ff55"));
        QTRY_COMPARE(m->getPgns("129809").size(),2);QTRY_COMPARE(m->getPgns("127488").size(),1);QTRY_COMPARE(m->getPgns("129025").size(),1);QTRY_COMPARE(m->getPgns("128778").size(),1);
        auto rows=m->getPgns("129809");QCOMPARE(rows[0].source,QString("43"));QCOMPARE(rows[1].source,QString("43"));
        const auto stamp=m->parameterUpdatedAt(rows[0].idX,"Name");QVERIFY(stamp.isValid());
        auto widgets=w.findChildren<AisClassBStatic129809*>();QCOMPARE(widgets.size(),2);QTRY_VERIFY(tree->indexFor(widgets[0],"Name").isValid());
        QTest::qWait(10);p->feed(packets(a));QTRY_VERIFY(m->parameterUpdatedAt(rows[0].idX,"Name")>stamp);
        QCOMPARE(p->output,WaveshareCodec::receiveConfiguration());
    }
    void interfaceChangeClosesAndNoTransmission() {
        MemoryWavePort *p=nullptr;Nmea2000Handler h(nullptr,nullptr,nullptr,[&](QObject *o){p=new MemoryWavePort;p->setParent(o);return p;});
        h.SetInterfaceType("Waveshare USB-CAN-A");h.setMode(Viewer);h.ConfigureWaveshare("memory");QTest::qWait(20);
        QVERIFY(!h.sendPgn(129809,{}));QCOMPARE(p->output.size(),20);auto old=QPointer<MemoryWavePort>(p);
        h.SetInterfaceType("None");QVERIFY(!old||!old->isOpen());h.ReconnectWaveshare();QVERIFY(!old||!old->isOpen());
    }
    void savedConfigurationDisableAndLayoutStop() {
        settings();int created=0;MemoryWavePort *p=nullptr;MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){++created;p=new MemoryWavePort;p->setParent(o);return p;});
        auto *setup=w.findChild<DialogSetup*>();w.findChild<QAction*>("actionViewer_Rx")->setChecked(true);QTRY_COMPARE(created,1);
        // A settings reload creates one fresh session, not an interim old-device session.
        emit setup->ReloadSettings();QCOMPARE(created,2);QTest::qWait(20);
        auto *controller=w.findChild<MainController*>();emit controller->layoutAboutToReplace();QVERIFY(!p->isOpen());
        emit setup->ConnectSerialCanDevice();QCOMPARE(created,3);
        {AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Nmea2000/Nmea2000Enabled",false);s.sync();}
        emit setup->ReloadSettings();QVERIFY(!p->isOpen());w.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);w.findChild<QAction*>("actionViewer_Rx")->setChecked(true);
        emit setup->ConnectSerialCanDevice();QCOMPARE(created,3);QVERIFY(!p->isOpen());
    }
    void setupPortSaveCancelReload() {
        settings();DialogSetup setup;setup.show();QTest::qWait(20);auto *port=setup.findChild<QComboBox*>("wavesharePort");QVERIFY(port);QCOMPARE(port->currentText(),QString("memory"));
        port->setCurrentText("draft");setup.reject();setup.show();QTest::qWait(20);QCOMPARE(port->currentText(),QString("memory"));
        port->setCurrentText("new-device");QVERIFY(QMetaObject::invokeMethod(&setup,"on_pushButtonSave_clicked",Qt::DirectConnection));
        {AppSettings s(APP_COMPANY,APP_NAME);QCOMPARE(s.value("Nmea2000/WavesharePort").toString(),QString("new-device"));}
        port->setCurrentText("discard");QVERIFY(QMetaObject::invokeMethod(&setup,"on_pushButtonLoad_clicked",Qt::DirectConnection));QCOMPARE(port->currentText(),QString("new-device"));setup.reject();
    }
    void nativeSetupPresentation() {
        const auto evidence=qEnvironmentVariable("M231F_EVIDENCE");
        for(const auto &theme:QStringList{"Dark","Light"})for(int points:{9,12,16}) {
            settings();{AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.setValue("Preference/FontSize",points);}
            MainWindow w(nullptr,nullptr,nullptr,nullptr,[](QObject *o){auto *p=new MemoryWavePort;p->setParent(o);return p;});
            w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();auto *setup=w.findChild<DialogSetup*>();QVERIFY(setup);auto *port=setup->findChild<QComboBox*>("wavesharePort");QVERIFY(port);QCOMPARE(port->currentText(),QString("memory"));
            auto *tabs=setup->findChild<QTabWidget*>("tabWidget");QVERIFY(tabs);tabs->setCurrentIndex(1);setup->resize(1100,900);QTest::qWait(30);
            QVERIFY(port->isVisible());QVERIFY(port->isEnabled());QCOMPARE(setup->font().pointSize(),points);
            if(!evidence.isEmpty()){QDir().mkpath(evidence);QVERIFY(setup->grab().save(evidence+"/waveshare-"+theme+QString::number(points)+".png"));}
            setup->reject();
        }
    }
    void generateStopAndExplicitReconnect() {
        settings();MemoryWavePort *port=nullptr;int created=0;
        MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){++created;port=new MemoryWavePort;port->setParent(o);return port;});
        QCOMPARE(created,0);w.findChild<QAction*>("actionViewer_Rx")->setChecked(true);QTRY_COMPARE(created,1);QTest::qWait(20);
        QVERIFY(w.findChild<Nmea2000Handler*>()->IsWaveshareActive());
        QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isEnabled());
        QVERIFY(!w.findChild<QAction*>("actionStop_Sending")->isEnabled());
        w.show();QTest::qWait(20);auto old=QPointer<MemoryWavePort>(port);QTest::keyClick(&w,Qt::Key_F12);QVERIFY(!old || !old->isOpen());
        auto *setup=w.findChild<DialogSetup*>();emit setup->ConnectSerialCanDevice();QTRY_COMPARE(created,2);
        w.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);QVERIFY(!port->isOpen());QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
};
QTEST_MAIN(WaveshareTests)
#include "waveshare_rx_tests.moc"
