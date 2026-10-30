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
#include <QScopeGuard>
#include <QAction>
#include <QNetworkProxy>
#include <QPointer>
#include <QTableView>
#include <QCheckBox>
#include <QJsonDocument>
#include "generatorvesselmovement.h"
#include "generatorengine.h"
#include "mainwindow.h"
#include "appsettings.h"
#include "nmeasenderworker.h"
#include "nmea2000handler.h"
#include "nmea0183handler.h"
#include "N2kMessages.h"
#include "actisensecodec.h"

class MemoryPort : public QSerialPort {
public:
    QByteArray input, output;
    bool failOpen = false;
    int opens = 0;
    bool open(OpenMode mode) override { ++opens; return !failOpen && QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { QIODevice::close(); input.clear(); }
    qint64 bytesAvailable() const override { return input.size(); }
    qint64 bytesToWrite() const override { return 0; }
    void feed(const QByteArray &bytes) { input += bytes; emit readyRead(); }
protected:
    qint64 readData(char *data, qint64 size) override {
        const auto n=qMin(size,qint64(input.size()));memcpy(data,input.constData(),n);input.remove(0,n);return n;
    }
    qint64 writeData(const char *data, qint64 size) override { output.append(data,size);return size; }
};

class DeferredCanStartup : public NMEA2000_Actisense {
public:
    explicit DeferredCanStartup(QSerialPort &port) : NMEA2000_Actisense(port) {
        SetMode(tNMEA2000::N2km_ListenAndNode,44);
        OpenScheduler.Disable();
    }
};

// Models asynchronous acceptance, fragmented completion and a temporary stall.
// It deliberately does not claim to exercise Qt's native Windows/USB backend.
class BufferedPort : public MemoryPort {
public:
    QByteArray pending, delivered;
    qint64 highWater = 0, limit = -1;
    qint64 bytesToWrite() const override { return pending.size(); }
    void drain(int count) {
        count=qMin(count,int(pending.size())); if(!count)return;
        delivered+=pending.left(count);pending.remove(0,count);emit bytesWritten(count);
    }
    void close() override { MemoryPort::close(); pending.clear(); }
protected:
    qint64 writeData(const char *data,qint64 size) override {
        const auto count=limit<0?size:qMin(size,limit);
        output.append(data,count);pending.append(data,count);
        highWater=qMax(highWater,qint64(pending.size()));return count;
    }
};

class SenderLifecycleTests : public QObject {
    Q_OBJECT
    static void add(PgnTableModel *model,int id,const QString &pgn="HDT",bool enabled=true,double interval=1000000) {
        model->addPgn(id,"Test",pgn,enabled,{{"Heading","1","rad","double"}},{},interval);
    }
private slots:
    void movingDefaultBufferedTransport_data() {
        QTest::addColumn<QString>("preamble");
        QTest::newRow("bst94")<<QString("0x94");
        QTest::newRow("bst93")<<QString("0x93");
    }
    void movingDefaultBufferedTransport() {
        QFETCH(QString,preamble);
        AppSettings settings(APP_COMPANY,APP_NAME);
        settings.setValue("Vessel/NumEngines",2);settings.sync();
        MainWindow window;
        auto *controller=window.findChild<MainController*>();
        auto *sender=window.findChild<NmeaSenderWorker*>();
        QVERIFY(controller && sender);
        controller->loadLayoutFromJson(":/defaults/defaultLayout.json");
        auto *movement=window.findChild<GeneratorVesselMovement*>();QVERIFY(movement);
        const auto engines=window.findChildren<GeneratorEngine*>();QCOMPARE(engines.size(),2);
        for(auto *engine:engines) {QCOMPARE(engine->engineProfile(),QString("MerCruiser 7.4 MPI"));QVERIFY(engine->profileReady());}
        auto *enable=movement->findChild<QCheckBox*>("checkBoxManualMovementEnabled");QVERIFY(enable);enable->click();
        for(auto *engine:engines)QTRY_VERIFY(engine->EngineRunning());
        // Startup bindings are queued; establish running state before throttles.
        QTest::qWait(50);
        movement->setTransmissionGear(N2kTG_Forward);movement->setTransmissionGearStbd(N2kTG_Forward);
        movement->setThrottlePosition(25);movement->setThrottlePositionStbd(40);
        auto *port=new BufferedPort;Nmea2000Handler handler(nullptr,port);
        handler.SetInterfaceType("Actisense");QVERIFY(handler.InitializeActisense("MEMORY",QSerialPort::Baud115200,preamble));
        QList<int> intended, intendedInstances;
        connect(sender,&NmeaSenderWorker::sendPgn,&handler,[&](int pgn,const QStringList &params){
            intended.append(pgn);int instance=-1;
            for(const auto &param:params)if(param.startsWith("EngineInstance:"))instance=param.mid(15).toInt();
            intendedInstances.append(instance);QVERIFY(handler.sendPgn(pgn,params));
        });
        QTimer drain;drain.setInterval(10);drain.setTimerType(Qt::PreciseTimer);
        connect(&drain,&QTimer::timeout,port,[port]{port->drain(115);});drain.start();
        sender->startSending();QTest::qWait(2100);
        drain.stop();QTest::qWait(1000); // Driver cannot drain, while simulation continues.
        QVERIFY(port->bytesToWrite()>0);
        movement->setThrottlePosition(35);drain.start();QTest::qWait(3100);
        sender->stopSending();const auto submitted=port->output;
        QTest::qWait(1000);QCOMPARE(port->bytesToWrite(),qint64(0));QCOMPARE(port->output,submitted);
        QCOMPARE(port->delivered,submitted);QVERIFY(port->highWater<4096);
        const auto capture=qEnvironmentVariable("RAYNMEA_TEST_TX_CAPTURE");
        if(!capture.isEmpty()) {
            QFile file(capture+"-"+preamble+".bin");
            QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::NewOnly));
            QCOMPARE(file.write(submitted),qint64(submitted.size()));
        }
        for(auto *engine:engines)QVERIFY(engine->EngineSpeed()>1000);
        QVERIFY(movement->Latitude()!=0 || movement->Longitude()!=0);
        int pos=0,frame=0;QSet<int> pgns,instances;QSet<QByteArray> positions;
        QMap<QString,int> engineCounts;
        while(pos<submitted.size()) {
            QCOMPARE(submitted.mid(pos,2),QByteArray::fromHex("1002"));pos+=2;
            QByteArray body;bool ended=false;
            while(pos<submitted.size()) {
                const auto b=quint8(submitted[pos++]);
                if(b!=16){body+=char(b);continue;}
                QVERIFY(pos<submitted.size());auto c=quint8(submitted[pos++]);
                if(c==3){ended=true;break;}QCOMPARE(c,quint8(16));body+=char(16);
            }
            QVERIFY(ended);QVERIFY(body.size()>=9);
            const bool physical=preamble=="0x94";const int offset=physical?8:13;
            QCOMPARE(quint8(body[0]),quint8(physical?0x94:0x93));
            QCOMPARE(body.size(),int(quint8(body[1]))+3);
            QCOMPARE(int(quint8(body[1])),int(quint8(body[offset-1]))+(physical?6:11));
            unsigned sum=0;for(auto b:body)sum+=quint8(b);QCOMPARE(sum&255u,0u);
            int pgn=quint8(body[3])+(quint8(body[4])<<8)+(quint8(body[5])<<16);
            QVERIFY(frame<intended.size());QCOMPARE(pgn,intended[frame]);pgns.insert(pgn);
            if(pgn==127488 || pgn==127489 || pgn==127493) {
                QCOMPARE(int(quint8(body[offset])),intendedInstances[frame]);
                ++engineCounts[QString::number(pgn)+"/"+QString::number(quint8(body[offset]))];
            }
            ++frame;
            if(pgn==127488)instances.insert(quint8(body[offset]));
            if(pgn==129025)positions.insert(body.mid(offset,8));
        }
        QCOMPARE(frame,intended.size());QCOMPARE(pgns.size(),19);QCOMPARE(instances,QSet<int>({0,1}));QVERIFY(positions.size()>2);
        for(int pgn:{127488,127489,127493}) {
            QVERIFY(engineCounts[QString::number(pgn)+"/0"]>0);
            QCOMPARE(engineCounts[QString::number(pgn)+"/0"],engineCounts[QString::number(pgn)+"/1"]);
        }
        qInfo()<<preamble<<"moving default frames"<<frame<<"bytes"<<submitted.size()<<"queue high water"<<port->highWater;
        qInfo()<<"Scheduled, encoded and delivered engine counts"<<engineCounts;
        handler.setMode(Viewer);double lat=1,lon=2;QVERIFY(!handler.SendPgn129025(lat,lon));
        QCOMPARE(port->output,submitted);handler.ShutdownActisense();QVERIFY(!port->isOpen());
    }
    void outboundDiagnostics() {
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto path=dir.filePath("tx.jsonl");
        qputenv("RAYNMEA_ACTISENSE_TX_TRACE",path.toUtf8());
        QByteArray expected;
        {
            BufferedPort port;NMEA2000_Actisense adapter(port);QVERIFY(adapter.CANOpen());
            tN2kMsg msg;SetN2kPGN127488(msg,1,2500,0,0);
            QVERIFY(adapter.SendMessage(msg));expected=port.output;port.drain(3);port.drain(1000);
            // Deliberately characterize rejected partial acceptance without claiming
            // this is the physical failure: the caller receives false, no retry.
            port.limit=3;QVERIFY(!adapter.SendMessage(msg));
            emit port.errorOccurred(QSerialPort::WriteError);port.close();
        }
        qunsetenv("RAYNMEA_ACTISENSE_TX_TRACE");
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto original=file.readAll();file.close();
        int submits=0,progress=0,errors=0;bool partial=false,closed=false;
        for(const auto &line:original.split('\n')) {
            if(line.isEmpty())continue;
            const auto o=QJsonDocument::fromJson(line).object();QVERIFY(!o.isEmpty());
            if(o["event"]=="submit"){++submits;QCOMPARE(QByteArray::fromHex(o["frame_hex"].toString().toLatin1()),expected);}
            if(o["event"]=="progress")progress+=o["count"].toInt();
            if(o["event"]=="error")++errors;
            if(o["event"]=="accepted" && o["count"].toInt()==3)partial=true;
            if(o["event"]=="closing")closed=true;
        }
        QCOMPARE(submits,2);QCOMPARE(progress,expected.size());QVERIFY(errors>=1);QVERIFY(partial && closed);
        qputenv("RAYNMEA_ACTISENSE_TX_TRACE",path.toUtf8());
        {MemoryPort port;NMEA2000_Actisense adapter(port);}
        qunsetenv("RAYNMEA_ACTISENSE_TX_TRACE");
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void defaultLayoutMemoryTransport() {
        MainWindow window;
        auto *controller=window.findChild<MainController*>();
        auto *sender=window.findChild<NmeaSenderWorker*>();
        auto *model=window.findChild<PgnTableModel*>();
        QVERIFY(controller && sender && model);
        controller->loadLayoutFromJson(":/defaults/defaultLayout.json");
        QSet<int> expected;
        for(const auto &entry:model->getPgnList())
            if(entry.enabled && entry.pgn.toInt()>0) expected.insert(entry.pgn.toInt());
        QVERIFY(expected.contains(127488)); QVERIFY(expected.contains(127489));
        QVERIFY(expected.contains(127493)); QVERIFY(expected.contains(129029));
        auto *port=new MemoryPort; Nmea2000Handler handler(nullptr,port);
        handler.SetInterfaceType("Actisense");
        QVERIFY(handler.InitializeActisense("MEMORY",QSerialPort::Baud115200,"0x94"));
        QMap<int,int> counts;
        connect(sender,&NmeaSenderWorker::sendPgn,&handler,[&](int pgn,const QStringList &params) {
            QVERIFY(expected.contains(pgn)); QVERIFY(handler.sendPgn(pgn,params)); ++counts[pgn];
        });
        sender->startSending(); QTest::qWait(2200); sender->stopSending();
        const auto stream=port->output;
        QTest::qWait(300); QCOMPARE(port->output,stream);
        QCOMPARE(QSet<int>(counts.keyBegin(),counts.keyEnd()),expected);
        // Independently validate every concatenated BST-94 frame, including fast packets.
        int pos=0,frames=0;
        while(pos<stream.size()) {
            QCOMPARE(stream.mid(pos,2),QByteArray::fromHex("1002")); pos+=2;
            QByteArray body; bool ended=false;
            while(pos<stream.size()) {
                const auto byte=quint8(stream[pos++]);
                if(byte!=0x10) {body.append(char(byte));continue;}
                QVERIFY(pos<stream.size());const auto control=quint8(stream[pos++]);
                if(control==3) {ended=true;break;}
                QCOMPARE(control,quint8(0x10));body.append(char(0x10));
            }
            QVERIFY(ended); QVERIFY(body.size()>=9);
            QCOMPARE(quint8(body[0]),quint8(0x94));
            QCOMPARE(body.size(),int(quint8(body[1]))+3);
            QCOMPARE(int(quint8(body[1])),int(quint8(body[7]))+6);
            unsigned sum=0; for(auto byte:body) sum+=quint8(byte); QCOMPARE(sum&255u,0u);
            const int pgn=quint8(body[3])+(quint8(body[4])<<8)+(quint8(body[5])<<16);
            QVERIFY(expected.contains(pgn));++frames;
        }
        int total=0; for(auto count:counts)total+=count;
        QCOMPARE(frames,total); QVERIFY(frames<200);
        QVERIFY(counts[127488]>=14 && counts[127488]<=18);
        qInfo()<<"Default layout memory frames"<<frames<<"counts"<<counts;
    }
    void deadlineBoundariesAndStall() {
        QTableView table; PgnTableController controller(&table); NmeaSenderWorker sender;
        sender.setPgnTableController(&controller); auto *model=controller.getPgnTableModel();
        add(model,1,"127488",true,250); add(model,2,"127488",true,500);
        QSignalSpy sends(&sender,&NmeaSenderWorker::sendPgn);
        sender.startSending(); sender.findChild<QTimer*>()->stop();
        QCOMPARE(sender.findChild<QTimer*>()->timerType(),Qt::PreciseTimer);
        sender.sendNmeaMessageAt(259); QCOMPARE(sends.count(),2); // First callback also late.
        sender.sendNmeaMessageAt(499); QCOMPARE(sends.count(),2);
        sender.sendNmeaMessageAt(500); QCOMPARE(sends.count(),3); // Exact deadline.
        sender.sendNmeaMessageAt(760); QCOMPARE(sends.count(),5); // Late by 10ms.
        sender.sendNmeaMessageAt(1000); QCOMPARE(sends.count(),6); // No drift to 1010.
        sender.sendNmeaMessageAt(1760); QCOMPARE(sends.count(),8); // One each, no backlog burst.
        sender.sendNmeaMessageAt(1760); QCOMPARE(sends.count(),8);
        sender.sendNmeaMessageAt(2000); QCOMPARE(sends.count(),9);
        sender.stopSending(); sender.sendNmeaMessageAt(3000); QCOMPARE(sends.count(),9);
        sender.startSending(); sender.findChild<QTimer*>()->stop();
        sender.sendNmeaMessageAt(250); QCOMPARE(sends.count(),11); // Fresh run.
        sender.setMode(Viewer); sender.sendNmeaMessageAt(500); QCOMPARE(sends.count(),11);
    }
    void twoEngineCadence_data() {
        QTest::addColumn<QString>("preamble");
        QTest::newRow("physical-format-memory-only") << QString("0x94");
        QTest::newRow("virtual-format-memory-only") << QString("0x93");
    }
    void twoEngineCadence() {
        QFETCH(QString, preamble);
        QTableView table; PgnTableController controller(&table); NmeaSenderWorker sender;
        sender.setPgnTableController(&controller);
        auto *model = controller.getPgnTableModel();
        for (int instance = 0; instance < 2; ++instance)
            model->addPgn(instance+1,"Engine","127488",true,
                {{"EngineInstance",QString::number(instance),"","unsigned char"},
                 {"EngineSpeed",QString::number(instance ? 2500 : 1250),"RPM","double"},
                 {"EngineBoostPressure","0","Pa","double"},
                 {"EngineTiltTrim","0","deg","char"}}, {},250);
        auto *port = new MemoryPort; Nmea2000Handler handler(nullptr,port);
        handler.SetInterfaceType("Actisense");
        QVERIFY(handler.InitializeActisense("MEMORY",QSerialPort::Baud115200,preamble));
        QList<qint64> times[2]; QElapsedTimer clock; clock.start();
        int expected[2] = {1250,2500}; QSet<int> observed[2];
        connect(&sender,&NmeaSenderWorker::sendPgn,&handler,[&](int pgn,const QStringList &params) {
            QCOMPARE(pgn,127488); port->output.clear(); QVERIFY(handler.sendPgn(pgn,params));
            const auto wire = port->output;
            QCOMPARE(wire.left(2),QByteArray::fromHex("1002"));
            QCOMPARE(wire.right(2),QByteArray::fromHex("1003"));
            QByteArray body;
            for(int i=2;i<wire.size()-2;++i) {
                body += wire[i];
                if(quint8(wire[i])==0x10) { ++i; QCOMPARE(quint8(wire[i]),quint8(0x10)); }
            }
            unsigned sum=0; for(auto byte:body) sum+=quint8(byte);
            QCOMPARE(sum & 255u,0u);
            QCOMPARE(quint8(body[0]),quint8(preamble=="0x94"?0x94:0x93));
            QCOMPARE(body.size(),int(quint8(body[1]))+3);
            QCOMPARE(body.mid(3,3),QByteArray::fromHex("00f201"));
            const int offset=preamble=="0x94"?8:13;
            QCOMPARE(quint8(body[offset-1]),quint8(8));
            const int instance=quint8(body[offset]); QVERIFY(instance<2);
            const int raw=quint8(body[offset+1])+(quint8(body[offset+2])<<8);
            QCOMPARE(raw,expected[instance]*4);
            observed[instance].insert(raw/4);
            times[instance].append(clock.elapsed());
        });
        sender.startSending(); QTest::qWait(2100);
        expected[0]=1500; model->changeParam(1,"EngineSpeed","1500");
        QTest::qWait(2100);
        expected[1]=2750; model->changeParam(2,"EngineSpeed","2750");
        QTest::qWait(2100); sender.stopSending();
        const auto bytes=port->output; const auto count=times[0].size();
        QTest::qWait(550); sender.sendNmeaMessage();
        QCOMPARE(port->output,bytes); QCOMPARE(times[0].size(),count);
        QCOMPARE(times[0].size(),times[1].size());
        QCOMPARE(observed[0],QSet<int>({1250,1500}));
        QCOMPARE(observed[1],QSet<int>({2500,2750}));
        for(int instance=0;instance<2;++instance) {
            QList<qint64> gaps;
            for(int i=1;i<times[instance].size();++i) gaps.append(times[instance][i]-times[instance][i-1]);
            QVERIFY(!gaps.isEmpty()); std::sort(gaps.begin(),gaps.end());
            qInfo() << preamble << "instance" << instance << "frames" << times[instance].size()
                    << "gap ms min/median/max" << gaps.first() << gaps[gaps.size()/2] << gaps.last();
            QVERIFY2(times[instance].size()>=22,"250ms cadence lost scheduler cycles (6.3s window)");
            QVERIFY(gaps[gaps.size()/2]>=200 && gaps[gaps.size()/2]<=300);
        }
    }
    void serialOpenIndependentOfCanScheduler() {
        MemoryPort port;DeferredCanStartup adapter(port);
        // Deterministically reproduce a startup deadline that is not due on either OS.
        QVERIFY(!adapter.Open());QCOMPARE(port.opens,0);
        QVERIFY(adapter.CANOpen());QCOMPARE(port.opens,1);QVERIFY(port.isOpen());
        QCOMPARE(adapter.GetN2kSource(),44);
        tN2kMsg msg;SetN2kPGN129025(msg,42.5,-70.25);
        QVERIFY(adapter.SendMessage(msg));QCOMPARE(quint8(port.output.at(2)),quint8(0x94));
        port.close();
    }
    void serialOpenFailureAndRecovery() {
        auto *port=new MemoryPort;Nmea2000Handler handler(nullptr,port);
        handler.setMode(Viewer);port->failOpen=true;
        QVERIFY(!handler.InitializeActisense("MEMORY",QSerialPort::Baud38400));
        QCOMPARE(port->opens,1);QVERIFY(!handler.IsActisensePortOpen());
        QCOMPARE(handler.findChildren<NMEA2000_Actisense*>().size(),0);
        double lat=42.5,lon=-70.25;
        handler.setMode(Sender);QVERIFY(!handler.SendPgn129025(lat,lon));QVERIFY(port->output.isEmpty());
        port->failOpen=false;handler.setMode(Viewer);
        QVERIFY(handler.InitializeActisense("MEMORY",QSerialPort::Baud38400));
        QCOMPARE(port->opens,2);QCOMPARE(port->portName(),QString("MEMORY"));
        QCOMPARE(port->baudRate(),qint32(QSerialPort::Baud38400));
        QCOMPARE(port->dataBits(),QSerialPort::Data8);QCOMPARE(port->parity(),QSerialPort::NoParity);
        QCOMPARE(port->stopBits(),QSerialPort::OneStop);QCOMPARE(port->flowControl(),QSerialPort::NoFlowControl);
        auto *adapter=handler.findChild<NMEA2000_Actisense*>();QVERIFY(adapter);
        QCOMPARE(adapter->GetN2kSource(),44);QVERIFY(port->output.isEmpty());
        QVERIFY(!handler.SendPgn129025(lat,lon));QVERIFY(port->output.isEmpty());
        handler.setMode(Sender);QVERIFY(handler.SendPgn129025(lat,lon));
        QCOMPARE(quint8(port->output.at(2)),quint8(0x94));
        QPointer<NMEA2000_Actisense> old=adapter;
        QVERIFY(handler.InitializeActisense("MEMORY-2",QSerialPort::Baud4800,"0x93"));
        QVERIFY(old.isNull());QCOMPARE(port->opens,3);
        QCOMPARE(handler.findChildren<NMEA2000_Actisense*>().size(),1);
        QCOMPARE(port->portName(),QString("MEMORY-2"));QCOMPARE(port->baudRate(),qint32(4800));
        port->output.clear();QVERIFY(handler.SendPgn129025(lat,lon));
        QCOMPARE(quint8(port->output.at(2)),quint8(0x93));
        QVERIFY(!handler.InitializeActisense(""));QVERIFY(!handler.IsActisensePortOpen());
        const auto bytes=port->output;QVERIFY(!handler.SendPgn129025(lat,lon));QCOMPARE(port->output,bytes);
    }
    void initialAndRepeatedCycles() {
        QTableView table;PgnTableController controller(&table);NmeaSenderWorker sender;
        sender.setPgnTableController(&controller);auto *model=controller.getPgnTableModel();
        add(model,1);QSignalSpy sends(&sender,&NmeaSenderWorker::sendSnt),stops(&sender,&NmeaSenderWorker::finished);
        QVERIFY(!sender.isSending());sender.sendNmeaMessage();QCOMPARE(sends.count(),0);
        QCOMPARE(sender.thread(),model->thread());QCOMPARE(sender.findChild<QTimer*>()->interval(),250);
        for(int cycle=0;cycle<5;++cycle) {
            sender.startSending();sender.startSending();QVERIFY(sender.isSending());
            sender.sendNmeaMessage();QCOMPARE(sends.count(),cycle+1);
            sender.sendNmeaMessage();QCOMPARE(sends.count(),cycle+1);
            sender.stopSending();sender.stopSending();QCOMPARE(stops.count(),cycle+1);
            QMetaObject::invokeMethod(&sender,"sendNmeaMessage",Qt::QueuedConnection);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
            QCOMPARE(sends.count(),cycle+1);QVERIFY(!sender.findChild<QTimer*>()->isActive());
        }
    }
    void intervalsAndEnablement() {
        QTableView table;PgnTableController controller(&table);NmeaSenderWorker sender;
        sender.setPgnTableController(&controller);auto *model=controller.getPgnTableModel();
        add(model,1);add(model,2,"129025");add(model,3,"HDT",false);add(model,4,"-129025");
        QSignalSpy sentences(&sender,&NmeaSenderWorker::sendSnt),pgns(&sender,&NmeaSenderWorker::sendPgn);
        sender.startSending();sender.sendNmeaMessage();QCOMPARE(sentences.count(),1);QCOMPARE(pgns.count(),1);
        sender.sendNmeaMessage();QCOMPARE(sentences.count(),1);QCOMPARE(pgns.count(),1);
        model->setSendInterval(1,-1);model->changeParam(1,"Heading","2");
        sender.sendNmeaMessage();QCOMPARE(sentences.count(),2);
        QCOMPARE(sentences.last()[1].toStringList(),QStringList({"Heading:2"}));
        model->setPgnEnabled(1,false);sender.sendNmeaMessage();QCOMPARE(sentences.count(),2);
        add(model,5,"HDT");sender.sendNmeaMessage();QCOMPARE(sentences.count(),3);
        model->removePgn(5);sender.sendNmeaMessage();QCOMPARE(sentences.count(),3);
        sender.setMode(Viewer);sender.startSending();sender.sendNmeaMessage();QVERIFY(!sender.isSending());
        sender.setMode(Sender);QVERIFY(!sender.isSending());sender.startSending();QVERIFY(sender.isSending());
    }
    void mutationAndReentrancy() {
        QTableView table;PgnTableController controller(&table);NmeaSenderWorker sender;
        sender.setPgnTableController(&controller);auto *model=controller.getPgnTableModel();
        add(model,1);add(model,2);QSignalSpy sends(&sender,&NmeaSenderWorker::sendSnt);
        auto connection=connect(&sender,&NmeaSenderWorker::sendSnt,&sender,[&] {
            model->clearModel();add(model,1);sender.sendNmeaMessage();
        });
        sender.startSending();sender.sendNmeaMessage();QCOMPARE(sends.count(),1);
        disconnect(connection);sender.sendNmeaMessage();QCOMPARE(sends.count(),2);
        sender.stopSending();model->clearModel();add(model,1);add(model,2);
        connection=connect(&sender,&NmeaSenderWorker::sendSnt,&sender,[&] {sender.stopSending();sender.startSending();});
        sender.startSending();sender.sendNmeaMessage();QCOMPARE(sends.count(),3);
        disconnect(connection);sender.stopSending();
    }
    void windowModesAndReload() {
        AppSettings settings(APP_COMPANY,APP_NAME);
        settings.setValue("Nmea0183/Nmea0183Enabled",true);settings.setValue("Nmea0183/Nmea0183Port","MEMORY");settings.sync();
        auto reset=qScopeGuard([&]{ settings.setValue("Nmea0183/Nmea0183Enabled",false);settings.sync(); });
        MainWindow window(nullptr,nullptr,new MemoryPort);auto *sender=window.findChild<NmeaSenderWorker*>();QVERIFY(sender);
        auto *start=window.findChild<QAction*>("actionStart_Sending");QVERIFY(start);
        auto *viewer=window.findChild<QAction*>("actionViewer_Rx");QVERIFY(viewer);
        auto *generate=window.findChild<QAction*>("actionSimulator_Tx");QVERIFY(generate);
        QVERIFY(!sender->isSending());QVERIFY(!start->isChecked());
        for(int cycle=0;cycle<4;++cycle) {
            start->setChecked(true);QVERIFY(sender->isSending());
            viewer->setChecked(true);QVERIFY(!sender->isSending());QVERIFY(!start->isChecked());QVERIFY(!start->isEnabled());
            sender->startSending();QVERIFY(!sender->isSending());
            window.HotReloadSettings();QVERIFY(!sender->isSending());QVERIFY(viewer->isChecked());
            // M2.33 mode actions are exclusive and idempotent: unchecking
            // Viewer cannot implicitly select Generate or authorize Sending.
            viewer->setChecked(false);QVERIFY(viewer->isChecked());QVERIFY(!sender->isSending());
            generate->setChecked(true);QVERIFY(!sender->isSending());QVERIFY(start->isEnabled());
            start->setChecked(true);QVERIFY(sender->isSending());
            window.HotReloadSettings();QVERIFY(!sender->isSending());QVERIFY(!start->isChecked());
            window.HotReloadSettings();QVERIFY(!sender->isSending());
        }
    }
    void layoutDuringSending() {
        MainWindow window;auto *sender=window.findChild<NmeaSenderWorker*>();
        auto *controller=window.findChild<MainController*>();auto *model=window.findChild<PgnTableModel*>();
        QVERIFY(sender);QVERIFY(controller);QVERIFY(model);
        controller->setMode(Viewer); // Create receive instruments without generator prompts.
        controller->addRcvdPgn("129025",{"Latitude","42.5","Longitude","-70.25"},"123");
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto layout=dir.filePath("layout.json");controller->saveLayoutToJson(layout);
        QSignalSpy sends(sender,&NmeaSenderWorker::sendPgn);
        for(int cycle=0;cycle<3;++cycle) {
            sender->startSending();sender->sendNmeaMessage();
            QMetaObject::invokeMethod(sender,"sendNmeaMessage",Qt::QueuedConnection);
            controller->removeAllWidgets();QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
            QCOMPARE(model->getPgnList().size(),0);
            controller->loadLayoutFromJson(layout);sender->sendNmeaMessage();sender->stopSending();
        }
        QVERIFY(sends.count()>=3);controller->removeAllWidgets();
    }
    void destroyWithPendingWork() {
        AppSettings settings(APP_COMPANY,APP_NAME);
        settings.setValue("Nmea0183/Nmea0183Enabled",true);settings.setValue("Nmea0183/Nmea0183Port","MEMORY");settings.sync();
        auto reset=qScopeGuard([&]{ settings.setValue("Nmea0183/Nmea0183Enabled",false);settings.sync(); });
        for(int cycle=0;cycle<4;++cycle) {
            auto *window=new MainWindow(nullptr,nullptr,new MemoryPort);QPointer<NmeaSenderWorker> sender=window->findChild<NmeaSenderWorker*>();QVERIFY(sender);
            auto *start=window->findChild<QAction*>("actionStart_Sending");start->setChecked(true);
            QMetaObject::invokeMethod(sender,"sendNmeaMessage",Qt::QueuedConnection);
            if(cycle%2) window->findChild<QAction*>("actionStop_Sending")->trigger();
            window->close();QVERIFY(!sender->isSending());
            delete window;QVERIFY(sender.isNull());
            QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
        }
    }
    void transportPolicy() {
        auto *port=new MemoryPort;Nmea0183Handler sentences(nullptr,port);
        QVERIFY(sentences.Initialize("MEMORY"));QVERIFY(sentences.OpenDevice());
        QVERIFY(sentences.SendNmea0183Message("$GPHDT,1.0,T*ZZ\r\n")==false); // Invalid checksum stays rejected.
        const QStringList params={"Heading:1"};
        QVERIFY(sentences.sendSnt("HDT",params));const auto bytes=port->output;
        QMetaObject::invokeMethod(&sentences,[&] { sentences.sendSnt("HDT",params); },Qt::QueuedConnection);
        sentences.setMode(Viewer);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
        QCOMPARE(port->output,bytes);
        QVERIFY(!sentences.sendSnt("HDT",params));QCOMPARE(port->output,bytes);
        sentences.setMode(Sender);QVERIFY(sentences.sendSnt("HDT",params));
        auto *canPort=new MemoryPort;Nmea2000Handler handler(nullptr,canPort);
        handler.SetInterfaceType("Actisense");handler.InitializeActisense("MEMORY",QSerialPort::Baud115200,"0x93");
        QVERIFY(handler.IsActisensePortOpen());double lat=42.5,lon=-70.25;
        QVERIFY(handler.SendPgn129025(lat,lon));const auto canBytes=canPort->output;
        handler.setMode(Viewer);QVERIFY(!handler.SendPgn129025(lat,lon));QVERIFY(!handler.sendPgn(129025,{"Latitude:42.5","Longitude:-70.25"}));QCOMPARE(canPort->output,canBytes);
        handler.setMode(Sender);QVERIFY(handler.SendPgn129025(lat,lon));
    }
    void repeatedAdapterReconfiguration() {
        auto *port=new MemoryPort;Nmea2000Handler handler(nullptr,port);
        handler.setMode(Viewer);QSignalSpy received(&handler,&Nmea2000Handler::RcvdPgn);
        for(int cycle=0;cycle<5;++cycle) {
            handler.InitializeActisense("MEMORY",QSerialPort::Baud115200,cycle%2?"0x94":"0x93");
            const auto adapters=handler.findChildren<NMEA2000_Actisense*>();QCOMPARE(adapters.size(),1);
            QPointer<NMEA2000_Actisense> old=adapters.first();
            handler.setMode(Viewer);handler.setMode(Viewer);
            tN2kMsg msg;SetN2kPGN129025(msg,42.5,-70.25);msg.Source=123;
            ActisenseCodec::Message message;
            message.pgn=msg.PGN;message.priority=msg.Priority;message.destination=msg.Destination;
            message.length=msg.DataLen;std::copy_n(msg.Data,msg.DataLen,message.data.begin());
            const auto frame=ActisenseCodec::encode(message,0x93);
            port->feed(QByteArray(reinterpret_cast<const char*>(frame.bytes.data()),frame.size));
            QCOMPARE(received.count(),cycle+1);
            handler.setMode(Sender);port->output.clear();
            double lat=42.5,lon=-70.25;QVERIFY(handler.SendPgn129025(lat,lon));
            QCOMPARE(quint8(port->output.at(2)),quint8(cycle%2?0x94:0x93));
            handler.setMode(Viewer);
            handler.InitializeActisense("",QSerialPort::Baud115200);QVERIFY(old.isNull());QVERIFY(!handler.IsActisensePortOpen());
            QCOMPARE(handler.findChildren<NMEA2000_Actisense*>().size(),0);
        }
        port->failOpen=true;handler.InitializeActisense("MEMORY");QVERIFY(!handler.IsActisensePortOpen());
        QCOMPARE(handler.findChildren<NMEA2000_Actisense*>().size(),0);
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);QTemporaryDir profile;if(!profile.isValid())return 1;
    qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    // Exercise real sending actions, but never initialize an OS transport.
    AppSettings settings(APP_COMPANY,APP_NAME);
    settings.setValue("Nmea2000/Nmea2000Enabled",false);settings.setValue("Nmea2000/InterfaceType","None");
    settings.setValue("Nmea0183/Nmea0183Enabled",false);settings.setValue("Nmea0183/Nmea0183Port","None");settings.sync();
    SenderLifecycleTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "sender_lifecycle_tests.moc"
