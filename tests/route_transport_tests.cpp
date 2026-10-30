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
#include <QSerialPort>
#include "nmea2000handler.h"
#include "chartplotterrouteclient.h"
#include "chartplotterroutedialog.h"
#include "routeservicepolicy.h"
#include "slcancodec.h"
#include "wavesharecodec.h"
#include "appsettings.h"
#include "route_service_fixture.h"
using namespace RouteFixture;
namespace {
class Port final : public QSerialPort {
public:
    QByteArray input,output;
    bool open(OpenMode mode) override {return QIODevice::open(mode|QIODevice::Unbuffered);}
    void close() override {input.clear();QIODevice::close();}
    qint64 bytesAvailable() const override {return input.size();}
    void feed(const QByteArray &data){input+=data;emit readyRead();}
protected:
    qint64 readData(char *data,qint64 n) override {n=qMin(n,qint64(input.size()));memcpy(data,input.constData(),n);input.remove(0,n);return n;}
    qint64 writeData(const char *data,qint64 n) override {
        output.append(data,n);QTimer::singleShot(0,this,[this,n]{emit bytesWritten(n);});return n;
    }
};
bool waitFor(const std::function<bool()> &predicate,int timeout=7000) {
    QElapsedTimer t;t.start();while(!predicate()&&t.elapsed()<timeout)QTest::qWait(5);return predicate();
}
struct SerialRig {
    QPointer<Port> port;Nmea2000Handler handler;ChartplotterRouteClient client;
    bool slcan;int offset=0,consumed=0,hold=0;quint8 sequence=0;
    RequestDecoder decoder;SlcanCodec::Parser slcanParser;WaveshareCodec::Parser binaryParser;
    QVector<unsigned> transmitted;QTimer pump;
    SerialRig(bool s,Mode mode):handler(nullptr,nullptr,nullptr,[this](QObject *o){
        auto *p=new Port;p->setParent(o);port=p;offset=0;return p;
    }),slcan(s) {
        handler.SetInterfaceType(s?"CANable SLCAN":"Waveshare USB-CAN-A");handler.setMode(mode);
        handler.ConfigureWaveshare("synthetic",{123456,44});
        client.ready=[this]{return handler.routeServiceReady();};
        client.localAddress=[this]{return handler.routeServiceSource();};
        client.send=[this](const tN2kMsg &m){return handler.sendRouteService(m);};
        QObject::connect(&handler,&Nmea2000Handler::RouteServiceMessage,&client,&ChartplotterRouteClient::ingest);
        QObject::connect(&handler,&Nmea2000Handler::RouteServiceRevoked,&client,[this]{client.cancel();});
        QObject::connect(&pump,&QTimer::timeout,&client,[this]{poll();});pump.start(2);
    }
    ~SerialRig(){pump.stop();handler.endRouteService(true);}
    void feed(const tN2kMsg &m) {
        if(!port||!port->isOpen())return;
        for(const auto &f:frames(m,sequence++&7)) {
            WaveshareCodec::CanFrame raw;raw.id=f.id;raw.length=f.data.size();
            memcpy(raw.data.data(),f.data.constData(),f.data.size());
            auto bytes=slcan?SlcanCodec::encode(raw):WaveshareCodec::encode(raw);
            // Deliberately split every adapter record; both parsers must assemble it.
            port->feed(bytes.left(3));if(port)port->feed(bytes.mid(3));
        }
    }
    void poll() {
        if(!port)return;
        const int config=(slcan?SlcanCodec::configuration():WaveshareCodec::transmitConfiguration()).size();
        if(port->output.size()<config)return;
        if(!offset)offset=config;
        const auto bytes=port->output.mid(offset);offset=port->output.size();
        auto raw=slcan?slcanParser.feed(bytes):binaryParser.feed(bytes);
        for(const auto &f:raw){transmitted.append(pgn(f.id));decoder.receive(f.id,QByteArray(reinterpret_cast<const char*>(f.data.data()),f.length));}
        while(consumed<decoder.messages.size()) {
            const auto m=decoder.messages[consumed++];
            if(requested(m)==hold)continue;
            const auto reply=response(m);if(reply.PGN)feed(reply);
        }
    }
    bool browse() {
        if(!handler.beginRouteService()||!waitFor([this]{return handler.routeServiceReady();}))return false;
        client.ingest(claim());client.browse({1,name(),"Synthetic chartplotter"});
        return waitFor([this]{return client.state()==ChartplotterRouteClient::Idle&&client.entries().size()==1;});
    }
    bool complete() {
        if(!browse())return false;
        client.retrieve(client.entries().first());
        return waitFor([this]{return client.state()==ChartplotterRouteClient::Complete;});
    }
};
}
class RouteTransportTests:public QObject {
    Q_OBJECT
private slots:
    void serialTransactions_data() {
        QTest::addColumn<bool>("slcan");QTest::addColumn<bool>("viewer");
        for(bool s:{false,true})for(bool v:{false,true})QTest::newRow(qPrintable(QString("%1-%2").arg(s?"SLCAN":"Waveshare",v?"Viewer":"Generate")))<<s<<v;
    }
    void serialTransactions() {
        QFETCH(bool,slcan);QFETCH(bool,viewer);SerialRig r(slcan,viewer?Viewer:Sender);
        QVERIFY(r.handler.beginRouteService());r.client.discover();
        QVERIFY(waitFor([&]{return !r.client.devices().isEmpty()&&!r.client.devices().first().model.isEmpty();}));
        QCOMPARE(r.client.devices().first().name,name());QCOMPARE(r.client.devices().first().model,QString("Synthetic chartplotter"));
        r.client.advance(7000);QVERIFY(!r.client.busy());QVERIFY(r.complete());
        auto route=r.client.candidate();QVERIFY(route.complete);QCOMPARE(route.waypoints.size(),3);
        QCOMPARE(route.waypoints[2].coordinate,QGeoCoordinate(10.02,20.02));QCOMPARE(route.waypoints[2].name,QString("Point 2"));
        QCOMPARE(route.attributes.value("RouteID"),QString("7"));
        auto *session=r.handler.findChild<WaveshareSession*>();QVERIFY(!session->isTransmitting());
        double lat=1,lon=2;QVERIFY(!r.handler.SendPgn129025(lat,lon));
        for(const auto &m:r.decoder.messages){QVERIFY(RouteServicePolicy::allowed(m));QCOMPARE(int(m.Source),44);}
        for(auto p:r.transmitted)QVERIFY(p==59904||p==126208||p==60928||p==126993||p==126996||p==126998||p==126464||p==59392);
        const int before=r.decoder.messages.size();r.client.cancel();r.handler.endRouteService(true);QTest::qWait(80);
        QVERIFY(!r.handler.routeServiceActive());QVERIFY(!r.handler.sendRouteService(msg(59904,QByteArray::fromHex("00ee00"))));
        QCOMPARE(r.decoder.messages.size(),before);QVERIFY(!r.client.candidate().complete);
        // Explicit reconnect is RX only. No queued request or automatic restart.
        r.handler.ConfigureWaveshare("synthetic",{123456,44});QTest::qWait(100);
        QVERIFY(!r.handler.routeServiceActive());QVERIFY(!r.client.busy());
    }

    void serialFaultRevokesCandidate_data() {
        QTest::addColumn<bool>("slcan");QTest::addColumn<int>("stage");
        for(bool s:{false,true})for(int stage=0;stage<5;++stage)
            QTest::newRow(qPrintable(QString("%1-stage%2").arg(s?"SLCAN":"Waveshare").arg(stage)))<<s<<stage;
    }
    void serialFaultRevokesCandidate() {
        QFETCH(bool,slcan);QFETCH(int,stage);SerialRig r(slcan,Viewer);
        if(stage==0){QVERIFY(r.handler.beginRouteService());r.client.discover();}
        else if(stage==4)QVERIFY(r.complete());
        else {
            r.hold=stage==1?60928:stage==2?130065:130067;
            if(stage<3){QVERIFY(r.handler.beginRouteService());r.client.ingest(claim());r.client.browse({1,name(),"Fixture"});}
            else {QVERIFY(r.browse());r.client.retrieve(r.client.entries().first());}
            QVERIFY(waitFor([&]{return !r.decoder.messages.isEmpty()&&requested(r.decoder.messages.last())==r.hold;}));
        }
        QSignalSpy revoked(&r.handler,&Nmea2000Handler::RouteServiceRevoked);
        QVERIFY(r.port);r.port->errorOccurred(QSerialPort::ResourceError);
        QVERIFY(!r.handler.routeServiceActive());QCOMPARE(revoked.size(),1);QVERIFY(!r.client.candidate().complete);
        QTest::qWait(80);QVERIFY(!r.client.busy());
    }
    void serialFramedCorrelationAndFastPacketDamage_data(){QTest::addColumn<bool>("slcan");QTest::newRow("Waveshare")<<false;QTest::newRow("SLCAN")<<true;}
    void serialFramedCorrelationAndFastPacketDamage() {
        QFETCH(bool,slcan);SerialRig r(slcan,Viewer);QVERIFY(r.browse());r.hold=130066;r.client.retrieve(r.client.entries().first());
        QVERIFY(waitFor([&]{return requested(r.decoder.messages.last())==130066;}));
        auto wrong=attrs();wrong.Source=2;r.feed(wrong);QTest::qWait(30);QCOMPARE(r.client.state(),ChartplotterRouteClient::Attributes);
        wrong=attrs();wrong.Data[0]=1;r.feed(wrong);QTest::qWait(30);QCOMPARE(r.client.state(),ChartplotterRouteClient::Attributes);
        wrong=attrs();wrong.Data[2]=8;r.feed(wrong);QTest::qWait(30);QCOMPARE(r.client.state(),ChartplotterRouteClient::Attributes);
        // Missing, duplicated and reordered continuation frames must not
        // manufacture a message or advance the transaction.
        for(int damage=0;damage<3;++damage) {
            auto damaged=frames(attrs(),r.sequence++&7);QVERIFY(damaged.size()>2);
            if(damage==0)damaged.swapItemsAt(1,2);
            if(damage==1)damaged.removeAt(1);
            if(damage==2)damaged.insert(2,damaged[1]);
            for(const auto &f:damaged) {
                WaveshareCodec::CanFrame raw;raw.id=f.id;raw.length=f.data.size();memcpy(raw.data.data(),f.data.constData(),f.data.size());
                r.port->feed(slcan?SlcanCodec::encode(raw):WaveshareCodec::encode(raw));
            }
            QTest::qWait(40);QCOMPARE(r.client.state(),ChartplotterRouteClient::Attributes);QVERIFY(!r.client.candidate().complete);
        }
        QByteArray bytes;u(bytes,2,1);u(bytes,130066,3);u(bytes,0,1);u(bytes,2,1);u(bytes,0x10,1);
        auto ack=msg(126208,bytes);ack.Destination=45;r.feed(ack);QTest::qWait(30);QCOMPARE(r.client.state(),ChartplotterRouteClient::Attributes);
        ack.Destination=r.handler.routeServiceSource();r.feed(ack);QVERIFY(waitFor([&]{return r.client.state()==ChartplotterRouteClient::Failed;}));
        QVERIFY(!r.client.candidate().complete);r.handler.endRouteService(true);
    }
    void actisenseNeverAuthorizesRouteService() {
        Nmea2000Handler h;h.SetInterfaceType("Actisense");QVERIFY(!h.beginRouteService());QVERIFY(!h.routeServiceActive());
    }
};
QTEST_MAIN(RouteTransportTests)
#include "route_transport_tests.moc"
