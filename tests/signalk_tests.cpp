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

#include "signalk_testserver.h"

class SignalKTests : public QObject {
    Q_OBJECT
    static QJsonObject mapped(const tN2kMsg &message) {
        QJsonObject out;
        for (const auto &entry : SignalKPublisher::values(message)) {
            const auto o = entry.toObject(); out.insert(o["path"].toString(), o["value"]);
        }
        return out;
    }
private slots:
    void engineInstancesAndSI() {
        for (unsigned char id : {0,1}) {
            const auto p = "propulsion."+QString::number(id)+".";
            tN2kMsg m; SetN2kPGN127488(m,id,id?3600:1800,120000,25);
            auto v=mapped(m); QCOMPARE(v[p+"revolutions"].toDouble(),id?60.:30.);
            QCOMPARE(v[p+"boostPressure"].toDouble(),120000.); QCOMPARE(v[p+"drive.trimState"].toDouble(),.25);
            SetN2kPGN127489(m,id,200000,350,360,14.2,36,7200,150000,300000,int8_t(50),int8_t(75),tN2kEngineDiscreteStatus1(0),tN2kEngineDiscreteStatus2(0));
            v=mapped(m); QCOMPARE(v[p+"fuel.rate"].toDouble(),.00001);
            QCOMPARE(v[p+"runTime"].toDouble(),7200.); QCOMPARE(v[p+"engineLoad"].toDouble(),.5);
            QCOMPARE(v[p+"oilPressure"].toDouble(),200000.); QCOMPARE(v[p+"coolantTemperature"].toDouble(),360.);
            QVERIFY(!v.contains("propulsion."+QString::number(1-id)+".revolutions"));
            for (auto gear : {N2kTG_Forward,N2kTG_Reverse,N2kTG_Neutral}) {
                SetN2kPGN127493(m,id,gear,100000,330);
                QCOMPARE(mapped(m)[p+"transmission.gear"].toString(),gear==N2kTG_Forward?"Forward":gear==N2kTG_Reverse?"Reverse":"Neutral");
            }
        }
    }
    void batteriesChargersTanks() {
        tN2kMsg m;
        for (unsigned char id : {0,1}) {
            auto p="electrical.batteries."+QString::number(id)+".";
            SetN2kPGN127508(m,id,12.5+id,-4,295,1);
            auto v=mapped(m); QCOMPARE(v[p+"voltage"].toDouble(),12.5+id); QCOMPARE(v[p+"current"].toDouble(),-4.);
            SetN2kPGN127506(m,1,id,N2kDCt_Battery,75,90,600,.12,360000);
            v=mapped(m); QCOMPARE(v[p+"capacity.stateOfCharge"].toDouble(),.75); QVERIFY(!v.contains(p+"capacity.actual"));
            SetN2kPGN127505(m,id,N2kft_Fuel,25,400);
            v=mapped(m); QCOMPARE(v["tanks.fuel."+QString::number(id)+".currentLevel"].toDouble(),.25);
            QCOMPARE(v["tanks.fuel."+QString::number(id)+".capacity"].toDouble(),.4);
            SetN2kPGN127507(m,0,id,N2kCS_Float,N2kCM_Standalone,N2kOnOff_On,N2kOnOff_Off,0);
            QCOMPARE(mapped(m)["electrical.chargers.charger0battery"+QString::number(id)+".chargingMode"].toString(),"float");
        }
    }
    void navigationAndUnavailable() {
        tN2kMsg m; SetN2kPGN129025(m,42.5,-70.25);
        auto v=mapped(m); QCOMPARE(v["navigation.position"].toObject()["latitude"].toDouble(),42.5);
        SetN2kPGN129026(m,1,N2khr_true,1.2,4.5); v=mapped(m);
        QVERIFY(qAbs(v["navigation.courseOverGroundTrue"].toDouble()-1.2)<.0001); QCOMPARE(v["navigation.speedOverGround"].toDouble(),4.5);
        SetN2kPGN127250(m,1,2.5,N2kDoubleNA,N2kDoubleNA,N2khr_magnetic);v=mapped(m);
        QVERIFY(v.contains("navigation.headingMagnetic")); QVERIFY(!v.contains("navigation.magneticVariation"));
        SetN2kPGN129033(m,0,43200,0); QCOMPARE(mapped(m)["navigation.datetime"].toString(),"1970-01-01T12:00:00.000Z");
        SetN2kPGN129025(m,N2kDoubleNA,N2kDoubleNA); QVERIFY(mapped(m).isEmpty());
        SetN2kPGN130310(m,1,290,300,101300);v=mapped(m); QCOMPARE(v["environment.outside.temperature"].toDouble(),300.);
        SetN2kPGN130311(m,1,N2kts_OutsideTemperature,300,N2khs_OutsideHumidity,60,101300);
        QCOMPARE(mapped(m)["environment.outside.relativeHumidity"].toDouble(),.6);
        m.Clear(); m.SetPGN(99999); QVERIFY(mapped(m).isEmpty());
    }
    void gnssAndEnvironmentalIdentity() {
        tN2kMsg m;
        SetN2kPGN129029(m,1,0,43200,42.5,-70.25,12,N2kGNSSt_GPSGLONASS,N2kGNSSm_DGNSS,14,1.2,1.8,30,1,N2kGNSSt_GPS,23,2);
        auto v=mapped(m);QCOMPARE(v["navigation.gnss.type"].toString(),"Combined GPS/GLONASS");
        QCOMPARE(v["navigation.gnss.methodQuality"].toString(),"DGNSS fix");
        QCOMPARE(v["navigation.gnss.satellites"].toDouble(),14.);
        QCOMPARE(v["navigation.gnss.antennaAltitude"].toDouble(),12.);
        QCOMPARE(v["navigation.gnss.differentialReference"].toString(),"23");
        SetN2kPGN130316(m,1,2,N2kts_EngineRoomTemperature,310,N2kDoubleNA);
        v=mapped(m);QVERIFY(v.contains("environment.inside.engineRoom2.temperature"));QVERIFY(!v.contains("propulsion.2.temperature"));
        SetN2kPGN127505(m,2,N2kft_FuelGasoline,50,200);
        v=mapped(m);QVERIFY(v.contains("tanks.fuel.gasoline2.capacity"));QVERIFY(!v.contains("tanks.fuel.2.capacity"));QVERIFY(!v.contains("tanks.gas.2.capacity"));
        SetN2kPGN127252(m,1,1.5);QCOMPARE(mapped(m)["environment.heave"].toDouble(),1.5);
    }
    void publishStopReconnectAndFreshness() {
        TestServer server; SignalKPublisher publisher; publisher.configure(server.config());
        tN2kMsg m; SetN2kPGN129025(m,42,-70);
        QVERIFY(!publisher.publish(m)); publisher.start();
        QTRY_VERIFY_WITH_TIMEOUT(publisher.isReady(),3000);
        QVERIFY(server.headerSeen); QVERIFY(server.subscribedNone);
        for(int i=0;i<30;++i) QVERIFY(publisher.publish(m));
        QTRY_COMPARE(server.deltas.size(),30);
        const auto delta=server.deltas.last(); QCOMPARE(delta["context"].toString(),"vessels.self");
        const auto update=delta["updates"].toArray().first().toObject();
        QVERIFY(!update.contains("source"));
        QCOMPARE(update["$source"].toString(),"test-publisher");
        QVERIFY(QDateTime::fromString(update["timestamp"].toString(),Qt::ISODateWithMs).isValid());
        server.peer->abort(); QTRY_VERIFY(!publisher.isReady());
        QVERIFY(!publisher.publish(m)); SetN2kPGN129025(m,43,-71);
        QTRY_VERIFY_WITH_TIMEOUT(publisher.isReady(),4000);
        QCOMPARE(server.deltas.size(),30); QVERIFY(publisher.publish(m)); QTRY_COMPARE(server.deltas.size(),31);
        const auto pos=server.deltas.last()["updates"].toArray().first().toObject()["values"].toArray().first().toObject()["value"].toObject();
        QCOMPARE(pos["latitude"].toDouble(),43.);
        publisher.stop(); QVERIFY(!publisher.isReady()); QVERIFY(!publisher.publish(m));
        QTest::qWait(2200); QCOMPARE(server.deltas.size(),31);
    }
    void readOnlyRefusedAndStopDuringAuthorization() {
        TestServer server; server.writer=false; SignalKPublisher publisher;publisher.configure(server.config());publisher.start();
        QTest::qWait(200); QVERIFY(!publisher.isReady()); QVERIFY(!server.peer);
        publisher.stop(); server.writer=true; publisher.start(); publisher.stop();
        QTest::qWait(2200); QVERIFY(!publisher.isReady()); QVERIFY(server.deltas.isEmpty());
    }
    void outboundBurstIsBounded() {
        TestServer server;SignalKPublisher publisher;publisher.configure(server.config());publisher.start();
        QTRY_VERIFY_WITH_TIMEOUT(publisher.isReady(),3000);
        tN2kMsg m;SetN2kPGN129025(m,42,-70);
        // Deliberately withhold event-loop socket draining; pressure must fail
        // closed instead of growing an unbounded pending measurement queue.
        int accepted=0;
        while(accepted<10000 && publisher.publish(m)) ++accepted;
        QVERIFY(accepted<10000);QVERIFY(!publisher.isReady());publisher.stop();
        QTest::qWait(100);QVERIFY(server.deltas.size()<=accepted);
    }
    void invalidConfiguration() {
        SignalKPublisher publisher; QSignalSpy status(&publisher,&SignalKPublisher::statusChanged);
        for(const QString url:{"ws://localhost:3000", "http://user:secret@localhost", "http://localhost/?token=secret", "http://localhost/path"}) {
            publisher.configure({url,"","valid"});publisher.start(); QVERIFY(!publisher.isReady());
            QVERIFY(!status.last().first().toString().contains("secret"));
        }
    }
};
QTEST_GUILESS_MAIN(SignalKTests)
#include "signalk_tests.moc"
