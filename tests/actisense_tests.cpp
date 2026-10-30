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
#include <QNetworkProxy>
#include "appsettings.h"
#include "transportcapabilities.h"
#include "actisensecodec.h"
#include "nmea2000_actisense.h"
#include "nmea2000handler.h"
#include "mainwindow.h"
#include "maincontroller.h"
#include "latlong129025.h"
#include "dialogsetup.h"
#include <QComboBox>
#include <QShowEvent>
#include <QLineEdit>
#include <QPointer>
#include "pgntablecontroller.h"
#include "nmeareceiver.h"
#include "fluidlevel127505.h"

class FakeActisensePort : public QSerialPort {
public:
    QByteArray input, output;
    FakeActisensePort *peer = nullptr;
    bool failRead = false;
    qint64 writeResult = -2;
    bool open(OpenMode mode) override { return QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { QIODevice::close(); input.clear(); }
    qint64 bytesAvailable() const override { return input.size(); }
    void feed(const QByteArray &bytes) { input += bytes; emit readyRead(); }
protected:
    qint64 readData(char *data, qint64 size) override {
        if (failRead) { input.clear(); return -1; }
        const auto n = qMin(size, qint64(input.size()));
        memcpy(data, input.constData(), n); input.remove(0, n); return n;
    }
    qint64 writeData(const char *data, qint64 size) override {
        const auto n = writeResult == -2 ? size : qMin(size, writeResult);
        if (n > 0) { output.append(data, n); if (peer) peer->feed(QByteArray(data, n)); }
        return n;
    }
};

class ActisenseTests : public QObject {
    Q_OBJECT
    static QByteArray wire(const ActisenseCodec::Frame &f) {
        return QByteArray(reinterpret_cast<const char *>(f.bytes.data()), f.size);
    }
    // Independent fixture wrapper; deliberately also permits malformed bodies.
    static QByteArray envelope(QByteArray body) {
        int sum = 0; for (unsigned char b : body) sum += b;
        body.append(char(-sum)); body.replace(QByteArray(1, char(16)), QByteArray(2, char(16)));
        return QByteArray::fromHex("1002") + body + QByteArray::fromHex("1003");
    }
    static QByteArray good() { return QByteArray::fromHex("1002930e0601f801ff00040302010310102030f31003"); }
    static int consume(ActisenseCodec::Parser &p, const QByteArray &bytes,
                       ActisenseCodec::Message &m) {
        int count = 0;
        for (unsigned char b : bytes) { if (p.feed(b, m)) ++count; }
        return count;
    }
private slots:
    void exactEncoding() {
        ActisenseCodec::Message m;
        m.priority=6; m.pgn=129025; m.destination=255; m.source=123;
        m.timestamp=0x01020304; m.length=3; m.data[0]=16; m.data[1]=32; m.data[2]=48;
        QCOMPARE(wire(ActisenseCodec::encode(m,0x93)), good());
        QCOMPARE(wire(ActisenseCodec::encode(m,0x94)), QByteArray::fromHex("100294090601f801ff0310102030011003"));
        m.length=0;
        QCOMPARE(wire(ActisenseCodec::encode(m,0x93)), QByteArray::fromHex("1002930b0601f801ff000403020100591003"));
        QCOMPARE(wire(ActisenseCodec::encode(m,0x94)), QByteArray::fromHex("100294060601f801ff00671003"));
        // Fixed fixture chosen so checksum itself is DLE.
        m.length=1; m.data[0]=0x55;
        QCOMPARE(wire(ActisenseCodec::encode(m,0x94)), QByteArray::fromHex("100294070601f801ff015510101003"));
        m.length=224; QVERIFY(wire(ActisenseCodec::encode(m,0x93)).isEmpty());
        m.length=0; m.priority=8; QVERIFY(wire(ActisenseCodec::encode(m,0x94)).isEmpty());
        m.priority=6; m.pgn=0x40000; QVERIFY(wire(ActisenseCodec::encode(m,0x94)).isEmpty());
        m.pgn=129025; QVERIFY(wire(ActisenseCodec::encode(m,0x95)).isEmpty());
    }
    void fragmentationAndValues() {
        for(int split=0;split<=good().size();++split) {
            ActisenseCodec::Parser p; ActisenseCodec::Message m;
            const int count=consume(p,good().left(split),m)+consume(p,good().mid(split),m);
            QCOMPARE(count,1); QCOMPARE(m.pgn,std::uint32_t(129025)); QCOMPARE(m.source,std::uint8_t(0));
            QCOMPARE(m.timestamp,std::uint32_t(0x01020304)); QCOMPARE(m.priority,std::uint8_t(6));
            QCOMPARE(m.destination,std::uint8_t(255)); QCOMPARE(m.length,std::size_t(3));
            QCOMPARE(m.data[0],std::uint8_t(16)); QCOMPARE(m.data[1],std::uint8_t(32)); QCOMPARE(m.data[2],std::uint8_t(48));
        }
        ActisenseCodec::Parser p;ActisenseCodec::Message m;
        QCOMPARE(consume(p,"garbage"+good()+good()+good(),m),3);
        auto body=QByteArray::fromHex("930e0601f801ff2a0403020103102030");
        QCOMPARE(consume(p,envelope(body),m),1);QCOMPARE(m.source,std::uint8_t(42));
    }
    void allBytesAndMaximum() {
        for(int value=0;value<256;++value) {
            ActisenseCodec::Message m; m.priority=7;m.pgn=0x31010;m.destination=value;
            m.timestamp=0x10101010;m.length=223;m.data.fill(value);
            for(auto command:{0x93,0x94}) {
                const auto bytes=wire(ActisenseCodec::encode(m,command));
                // Independently construct the expected raw layout, then BDTP-wrap.
                QByteArray body;body.append(char(command));body.append(char(223+(command==0x93?11:6)));
                body.append(QByteArray::fromHex("07101003"));body.append(char(value));
                if(command==0x93)body.append(QByteArray::fromHex("0010101010"));
                body.append(char(223));body.append(QByteArray(223,char(value)));
                QCOMPARE(bytes,envelope(body));QVERIFY(bytes.size()<=int(2*ActisenseCodec::MaxBody+4));
                ActisenseCodec::Parser p;ActisenseCodec::Message decoded;
                QCOMPARE(consume(p,bytes,decoded),command==0x93?1:0);
                if(command==0x93) {QCOMPARE(decoded.length,m.length);QCOMPARE(decoded.data,m.data);}
            }
        }
        // L and DL fields equal to DLE must also be escaped.
        for(auto length:{5,10,16}) {
            ActisenseCodec::Message m;m.length=length;m.pgn=129025;
            for (auto command : {0x93, 0x94}) {
                QByteArray body;
                body.append(char(command));body.append(char(length+(command==0x93?11:6)));
                body.append(QByteArray::fromHex("0001f80100"));
                if (command==0x93) body.append(QByteArray(5, '\0'));
                body.append(char(length));body.append(QByteArray(length, '\0'));
                const auto bytes=wire(ActisenseCodec::encode(m,command));
                QCOMPARE(bytes,envelope(body));
                ActisenseCodec::Parser p;ActisenseCodec::Message decoded;
                QCOMPARE(consume(p,bytes,decoded),command==0x93?1:0);
                if (command==0x93) QCOMPARE(decoded.length,std::size_t(length));
            }
        }
    }
    void malformedAndRecovery() {
        QList<QByteArray> invalid={QByteArray::fromHex("100293006d1003"),
            envelope(QByteArray::fromHex("94060601f801ff00")), // valid outbound command, rejected inbound
            envelope(QByteArray::fromHex("9500")), QByteArray::fromHex("10021003"),
            QByteArray::fromHex("1002930b1011"), // unexpected control
            QByteArray::fromHex("1002930b0601f801ff000403020100691003")}; // legacy checksum +16
        auto body=QByteArray::fromHex("930e0601f801ff000403020103102030");
        // Prefix sums to zero; abandoning escape state on command rejection
        // would misread the stuffed DLE/STX and emit the embedded 93 body.
        invalid << envelope(QByteArray::fromHex("945a1002") + body);
        for(int i=0;i<body.size();++i)invalid<<envelope(body.left(i));
        auto bad=body;bad[1]=15;invalid<<envelope(bad);
        bad=body;bad[12]=2;invalid<<envelope(bad);
        bad=body;bad[12]=4;invalid<<envelope(bad);
        bad=body;bad[2]=8;invalid<<envelope(bad);
        bad=body;bad[5]=4;invalid<<envelope(bad);
        bad=body.left(13);bad[1]=char(235);bad[12]=char(224);bad+=QByteArray(224,'x');invalid<<envelope(bad);
        invalid<<envelope(body+"x");
        auto badChecksum=good();badChecksum[badChecksum.size()-3]^=1;invalid<<badChecksum;
        for(const auto &bytes:invalid) {
            ActisenseCodec::Parser p;ActisenseCodec::Message m;m.pgn=123;
            QCOMPARE(consume(p,bytes,m),0);QCOMPARE(m.pgn,std::uint32_t(123));
            QCOMPARE(consume(p,good(),m),1);
        }
        for(int n=0;n<good().size();++n) {
            ActisenseCodec::Parser p;ActisenseCodec::Message m;
            QCOMPARE(consume(p,good().left(n),m),0);
            // A dangling DLE followed by the next frame's DLE is a legal escaped
            // data byte. Without a disconnect/reset, this ambiguous prefix can
            // consume one frame; the following complete frame must recover.
            const int first = consume(p,good(),m);
            QVERIFY(first == 0 || first == 1);
            QCOMPARE(consume(p,good(),m),1);
            p.reset();QCOMPARE(p.bufferedSize(),std::size_t(0));
            QCOMPARE(consume(p,good(),m),1);
        }
    }
    void boundedNoiseAndReset() {
        ActisenseCodec::Parser p;ActisenseCodec::Message m;
        consume(p,QByteArray::fromHex("100293ea"),m);
        for(int i=0;i<1000000;++i) {p.feed(0x41,m);QVERIFY(p.bufferedSize()<=ActisenseCodec::MaxBody);}
        std::uint32_t random=0x12345678;
        for(int i=0;i<250000;++i) {
            random=random*1664525+1013904223;p.feed(random>>24,m);
            QVERIFY(p.bufferedSize()<=ActisenseCodec::MaxBody);
            if(i%997==0)p.reset();
        }
        p.reset();QCOMPARE(consume(p,good(),m),1);
        for(int i=0;i<100;++i) {consume(p,good().left(7),m);p.reset();QCOMPARE(consume(p,good(),m),1);}
    }
    void serialErrorsAndWrites() {
        FakeActisensePort port;NMEA2000_Actisense adapter(port);QVERIFY(port.open(QIODevice::ReadWrite));
        int count=0;connect(&adapter,&NMEA2000_Actisense::nmea2000MessageReceived,[&](tN2kMsg &){++count;});
        for(auto error:{QSerialPort::ReadError,QSerialPort::ResourceError,QSerialPort::DeviceNotFoundError}) {
            port.feed(good().left(8));emit port.errorOccurred(error);port.feed(good().mid(8));QCOMPARE(count,0);
        }
        port.feed(good().left(8));port.failRead=true;port.feed("x");port.failRead=false;
        port.feed(good().mid(8));QCOMPARE(count,0);
        port.feed(good().left(8));port.close();QVERIFY(port.open(QIODevice::ReadWrite));
        port.feed(good().mid(8));QCOMPARE(count,0);port.feed(good());QCOMPARE(count,1);
        tN2kMsg msg;msg.SetPGN(129025);msg.Priority=6;msg.Destination=255;msg.AddByte(0x55);
        QCOMPARE(adapter.SendMessage(msg), TransportCapabilities::experimentalActisenseTx);
        QCOMPARE(port.output, TransportCapabilities::experimentalActisenseTx
            ? QByteArray::fromHex("100294070601f801ff015510101003") : QByteArray());
        for(qint64 n:{qint64(-1),qint64(0),qint64(3)}) {port.writeResult=n;QVERIFY(!adapter.SendMessage(msg));}
        port.writeResult=-2;msg.DataLen=224;QVERIFY(!adapter.SendMessage(msg));msg.DataLen=-1;QVERIFY(!adapter.SendMessage(msg));
        unsigned long id=0;unsigned char len=0,buf[8]{};
        QVERIFY(!adapter.CANGetFrame(id,len,buf));QVERIFY(!adapter.CANSendFrame(id,8,buf,false));
        port.close();msg.DataLen=1;QVERIFY(!adapter.SendMessage(msg));
    }
    void fakeSerialFragmentation() {
        FakeActisensePort port;
        NMEA2000_Actisense adapter(port);
        QVERIFY(adapter.CANOpen()); // Fake override; never opens an OS port.
        int received = 0;
        connect(&adapter, &NMEA2000_Actisense::nmea2000MessageReceived,
                [&](const tN2kMsg &message) {
            ++received;
            QCOMPARE(message.PGN, 129025UL);
            QCOMPARE(message.Source, 0);
            QCOMPARE(message.MsgTime, 0x01020304UL);
            QCOMPARE(message.DataLen, 3);
            QCOMPARE(message.Data[0], 16);
            QCOMPARE(message.Data[1], 32);
            QCOMPARE(message.Data[2], 48);
        });
        for (int split = 0; split <= good().size(); ++split) {
            const int before = received;
            port.feed(good().left(split));
            port.feed(good().mid(split));
            QCOMPARE(received, before + 1);
        }
        const int before = received;
        QByteArray batch = QByteArray::fromHex("100293006d1003");
        for (int i = 0; i < 100; ++i) batch += good();
        port.feed(batch); // Crosses multiple bounded read chunks in one readyRead.
        QCOMPARE(received, before + 100);
        port.close();
    }
    void pairedViewerIntegration_data() {
        QTest::addColumn<int>("selection");
        QTest::newRow("selected") << 0;
        QTest::newRow("no-focus") << 1;
        QTest::newRow("other-selected") << 2;
    }
    void pairedViewerIntegration() {
        QFETCH(int,selection);
        MainWindow window;
        auto *handler=window.findChild<Nmea2000Handler*>();QVERIFY(handler);
        auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        controller->removeAllWidgets();
        handler->setMode(Viewer);controller->setMode(Viewer);
        FakeActisensePort tx,rx;NMEA2000_Actisense sender(tx),receiver(rx);
        QVERIFY(tx.open(QIODevice::ReadWrite));QVERIFY(rx.open(QIODevice::ReadWrite));tx.peer=&rx;
        connect(&receiver,&NMEA2000_Actisense::nmea2000MessageReceived,handler,&Nmea2000Handler::nmea2000MessageReceived);
        sender.setPreamble("0x93");
        auto *model=window.findChild<PgnTableModel*>();QVERIFY(model);
        LatLong129025 *other=nullptr;
        if(selection==2) {
            controller->addRcvdPgn("129025",{"Latitude","10","Longitude","20"},"124");
            other=window.findChild<LatLong129025*>();QVERIFY(other);
            QVERIFY(controller->selectLayoutWidget(other));
            QTRY_COMPARE(model->IdxHasFocus(),other->Id());
        }
        int target=-1;
        for(double lat:{42.5,42.5001,42.5002}) {
            tN2kMsg msg;SetN2kPGN129025(msg,lat,-70.25);msg.Source=123;
            if (TransportCapabilities::experimentalActisenseTx) QVERIFY(sender.SendMessage(msg));
            else {
                // Inject an external RX fixture: Release must not use application TX
                // merely to simulate a peer's BST-93 receive traffic.
                ActisenseCodec::Message external; external.pgn=msg.PGN;
                external.priority=msg.Priority; external.destination=msg.Destination;
                external.length=msg.DataLen;
                std::copy_n(msg.Data,msg.DataLen,external.data.begin());
                rx.feed(wire(ActisenseCodec::encode(external,0x93)));
            }
            QTRY_VERIFY(window.findChild<LatLong129025*>());
            LatLong129025 *widget=nullptr;
            for(auto *candidate:window.findChildren<LatLong129025*>())
                if(candidate!=other) widget=candidate;
            QVERIFY(widget);target=widget->Id();
            auto prefs=widget->UserPrefs();prefs.latLongUnitIndex=0;widget->setUserPrefs(prefs);
            QTRY_COMPARE(widget->Latitude(),lat);
            QTRY_COMPARE(model->getPgnDataToSend(target).paramDetails[0][1].toDouble(),lat);
            QVERIFY(qAbs(widget->Longitude()+70.25)<1e-7);
            if(selection==0) {
                QVERIFY(controller->selectLayoutWidget(widget));
                QTRY_COMPARE(model->IdxHasFocus(),widget->Id());
            } else if(selection==1) {
                QCOMPARE(model->IdxHasFocus(),-1);
            } else {
                QCOMPARE(model->IdxHasFocus(),other->Id());
                QCOMPARE(other->Latitude(),10.0);
                QCOMPARE(model->getPgnDataToSend(other->Id()).paramDetails[0][1].toDouble(),10.0);
            }
            QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
            QCOMPARE(widget->Latitude(),lat);
            QCOMPARE(model->getPgnDataToSend(target).paramDetails[0][1].toDouble(),lat);
            QCOMPARE(widget->findChild<QLineEdit*>("lineEditLatitude")->text(),QString::number(lat,'f',6));
        }
        QCOMPARE(window.findChildren<LatLong129025*>().size(),selection==2?2:1);
        window.findChild<MainController*>()->removeAllWidgets();window.close();
    }
    void receiveOrderingAndLifecycle() {
        MainWindow window;
        auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        auto *model=window.findChild<PgnTableModel*>();QVERIFY(model);
        controller->removeAllWidgets();controller->setMode(Viewer);
        for(int cycle=0;cycle<3;++cycle) {
            controller->addRcvdPgn("129025",{"Latitude","42.5","Longitude","-70.25"},"123");
            QPointer<LatLong129025> widget=window.findChild<LatLong129025*>();QVERIFY(widget);
            const int id=widget->Id();
            QSignalSpy edits(widget,&LatLong129025::LatitudeChanged);
            // Receive before draining initialization/focus events, with sub-display precision.
            controller->updateRcvdPgn(id,{"Latitude","42.5001234"});
            QCOMPARE(edits.count(),0);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
            QCOMPARE(model->getPgnParams(id)[0][1],QString("42.5001234"));
            QCOMPARE(widget->Latitude(),42.5001234);
            // A real widget edit is synchronous; a later receive stays authoritative.
            auto prefs=widget->UserPrefs();prefs.latLongUnitIndex=0;widget->setUserPrefs(prefs);
            auto *editor=widget->findChild<QLineEdit*>("lineEditLatitude");QVERIFY(editor);
            editor->setText("41.0");
            emit editor->textEdited("41.0");
            QTRY_COMPARE(model->getPgnParams(id)[0][1].toDouble(),41.0);
            controller->updateRcvdPgn(id,{"Latitude","42.5002"});
            QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
            QCOMPARE(model->getPgnParams(id)[0][1],QString("42.5002"));
            QCOMPARE(edits.count(),1);
            model->SetMode(Sender);model->setCurrentPgn(id);
            QVERIFY(model->setData(model->index(0,1),43.0));
            QCOMPARE(widget->Latitude(),43.0);QCOMPARE(edits.count(),1);
            model->SetMode(Viewer);
            QTemporaryDir layout;QVERIFY(layout.isValid());
            const QString path=layout.filePath("receive.json");
            controller->saveLayoutToJson(path);
            emit widget->WidgetHasFocus(id); // Pending, context-bound callback.
            controller->removeAllWidgets();
            controller->loadLayoutFromJson(path);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            QVERIFY(widget.isNull());
            auto *restored=window.findChild<LatLong129025*>();QVERIFY(restored);
            controller->updateRcvdPgn(restored->Id(),{"Latitude","44.0001"});
            emit restored->WidgetHasFocus(restored->Id());
            controller->removeAllWidgets();
            QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            QCOMPARE(model->getPgnList().size(),0);
        }
    }
    void multipleReceiveMatches() {
        MainWindow window;
        auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        auto *model=window.findChild<PgnTableModel*>();QVERIFY(model);
        auto *receiver=window.findChild<NmeaReceiver*>();QVERIFY(receiver);
        controller->removeAllWidgets();controller->setMode(Viewer);
        const QStringList initial={"Latitude","42.5","Longitude","-70.25"};
        controller->addRcvdPgn("129025",initial,"123");
        controller->addRcvdPgn("129025",initial,"123");
        controller->addRcvdPgn("129025",initial,"124");
        const auto positions=window.findChildren<LatLong129025*>();QCOMPARE(positions.size(),3);
        QVERIFY(controller->selectLayoutWidget(positions.last()));
        QTRY_COMPARE(model->IdxHasFocus(),positions.last()->Id());
        for(const QString lat:{QString("42.5001"),QString("42.5002")}) {
            receiver->processNmeaPgnSnt("129025",{"Latitude",lat,"Longitude","-70.25"},"123");
            QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
            for(auto *widget:positions) {
                const double expected=model->getSource(widget->Id())=="123"?lat.toDouble():42.5;
                QCOMPARE(widget->Latitude(),expected);
                QCOMPARE(model->getPgnParams(widget->Id())[0][1].toDouble(),expected);
            }
        }
        // Same source, different tank instances retain separate values.
        for(const QString instance:{QString("0"),QString("1")})
            controller->addRcvdPgn("127505",{"Instance",instance,"FluidType","0","Level","25","Capacity","100"},"123");
        receiver->processNmeaPgnSnt("127505",{"Instance","1","FluidType","0","Level","75","Capacity","100"},"123");
        QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
        for(const auto &tank:model->getPgns("127505")) {
            const auto params=model->getPgnParams(tank.idX);
            QCOMPARE(params[2][1].toDouble(),params[0][1]=="1"?75.0:25.0);
        }
        controller->removeAllWidgets();
    }
    void preambleSettings() {
        AppSettings settings(APP_COMPANY,APP_NAME);settings.remove("Nmea2000/ActisensePreamble");settings.sync();
        {DialogSetup setup;QShowEvent shown;QCoreApplication::sendEvent(&setup,&shown);
         QCOMPARE(setup.findChild<QComboBox*>("comboBoxNmea2000ActisensePreamble")->currentText(),QString("0x94"));}
        for(const QString value:{QString("0x93"),QString("0x94")}) {
            {DialogSetup setup;QShowEvent shown;QCoreApplication::sendEvent(&setup,&shown);
             setup.findChild<QComboBox*>("comboBoxNmea2000ActisensePreamble")->setCurrentText(value);
             QVERIFY(QMetaObject::invokeMethod(&setup,"on_pushButtonSave_clicked",Qt::DirectConnection));}
            DialogSetup restored;QShowEvent shown;QCoreApplication::sendEvent(&restored,&shown);
            QCOMPARE(restored.findChild<QComboBox*>("comboBoxNmea2000ActisensePreamble")->currentText(),value);
        }
    }
};
int main(int argc,char **argv) {
    QList<QByteArray> storage;for(int i=0;i<argc;++i)storage.append(argv[i]);storage.append("--no-nmea");
    QList<char*> args;for(auto &s:storage)args.append(s.data());int count=args.size();args.append(nullptr);
    QApplication app(count,args.data());QTemporaryDir profile;if(!profile.isValid())return 1;
    qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    ActisenseTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "actisense_tests.moc"
