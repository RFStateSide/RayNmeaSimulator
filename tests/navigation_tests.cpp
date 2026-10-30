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
#include <QGeoCoordinate>
#include <QShowEvent>
#include "convert.h"
#include "datastructs.h"
#include "N2kMessages.h"
// Deterministic simulation ticks without waiting for wall-clock timers.
#define private public
#include "autopilotsimulator.h"
#include "movevesselsimulator.h"
#undef private
class NavigationTests : public QObject {
 Q_OBJECT
private slots:
 void metadataTransitionsAndArrival() {
  AutoPilotSimulator a;QGeoCoordinate p(42,-70),q=p.atDistanceAndAzimuth(10,0),r=q.atDistanceAndAzimuth(20,90);
  a.route={{p,"A"},{q,"B"},{r,"C"}};a.setSpeed(15);a.start();a.advance(1);
  auto v=a.vesselWpBearing;QCOMPARE(v.OriginWaypointNumber,1u);QCOMPARE(v.DestinationWaypointNumber,2u);
  QVERIFY(qAbs(v.DistanceToWaypointM-15)<.01);QVERIFY(qAbs(v.BearingOriginToDestinationWaypoint-M_PI/2)<1e-6);
  QVERIFY(qAbs(v.BearingPositionToDestinationWaypoint-M_PI/2)<1e-5);QVERIFY(qAbs(v.Xte)<.001);QVERIFY(!v.PerpendicularCrossed);
  a.advance(2);v=a.vesselWpBearing;QCOMPARE(v.DistanceToWaypointM,0.0);QVERIFY(v.ArrivalCircleEntered);QVERIFY(v.PerpendicularCrossed);
  QCOMPARE(v.BearingPositionToDestinationWaypoint,N2kDoubleNA);QCOMPARE(v.WaypointClosingVelocity,0.0);QCOMPARE(v.RemainingTimeS,0.0);
  QVERIFY(qAbs(v.TotalDistanceTraveledM-30)<.01);QVERIFY(a.position.distanceTo(r)<.001);
 }
 void etaUtcRolloverAndUnavailable() {
  AutoPilotSimulator a;QGeoCoordinate p(42,-70);a.route={{p,"A"},{p.atDistanceAndAzimuth(20,0),"B"}};
  a.utcNow=[] {return QDateTime(QDate(2026,10,14),QTime(23,59,55),QTimeZone::UTC);};
  a.setSpeed(2);a.start();auto v=a.vesselWpBearing;
  QVERIFY(qAbs(v.RemainingTimeS-10)<.001);QVERIFY(qAbs(v.EtaTimeS-5)<.001);
  QCOMPARE(v.EtaDate,int16_t(QDate(1970,1,1).daysTo(QDate(2026,10,15))));
  a.setSpeed(0);a.advance(1);v=a.vesselWpBearing;QCOMPARE(v.EtaTimeS,N2kDoubleNA);QCOMPARE(uint16_t(v.EtaDate),N2kUInt16NA);QCOMPARE(v.RemainingTimeS,N2kDoubleNA);
  a.setSpeed(2);a.advance(1);a.stop();QCOMPARE(a.vesselWpBearing.EtaTimeS,N2kDoubleNA);QVERIFY(!a.running);
  a.start();QVERIFY(a.position.distanceTo(p)<.001);QCOMPARE(a.vesselWpBearing.OriginWaypointNumber,0u);
 }
 void crossTrackAndPerpendicularAreIndependent() {
  QGeoCoordinate o(0,0),d=o.atDistanceAndAzimuth(100,0),right=o.atDistanceAndAzimuth(50,0).atDistanceAndAzimuth(10,90);
  QVERIFY(qAbs(AutoPilotSimulator::crossTrack(o,d,right)-10)<.01);QVERIFY(!AutoPilotSimulator::passedPerpendicular(o,d,right));
  auto left=d.atDistanceAndAzimuth(20,0).atDistanceAndAzimuth(10,270);
  QVERIFY(AutoPilotSimulator::crossTrack(o,d,left)<0);QVERIFY(AutoPilotSimulator::passedPerpendicular(o,d,left));
  QVERIFY(AutoPilotSimulator::crossTrack(d,o,right)<0);
 }
 void navigationWireFields() {
  AutoPilotSimulator a;QGeoCoordinate p(0,0);a.route={{p,"A"},{p.atDistanceAndAzimuth(100,90),"B"}};
  a.utcNow=[] {return QDateTime(QDate(2026,10,14),QTime(23,59,55),QTimeZone::UTC);};a.setSpeed(10);a.start();
  auto encode=[](const VesselWpBearing &v){tN2kMsg m;SetN2kPGN129284(m,7,v.DistanceToWaypointM,N2khr_true,v.PerpendicularCrossed,v.ArrivalCircleEntered,N2kdct_GreatCircle,v.EtaTimeS,v.EtaDate,v.BearingOriginToDestinationWaypoint,v.BearingPositionToDestinationWaypoint,v.OriginWaypointNumber,v.DestinationWaypointNumber,v.DestinationLatitude,v.DestinationLongitude,v.WaypointClosingVelocity);return m;};
  auto m=encode(a.vesselWpBearing);QCOMPARE(m.DataLen,34);
  auto u=[&](int o,int n){quint64 v=0;for(int i=0;i<n;++i)v|=quint64(m.Data[o+i])<<(8*i);return v;};
  QCOMPARE(u(1,4),quint64(10000));QCOMPARE(u(6,4),quint64(50000));QCOMPARE(u(10,2),quint64(QDate(1970,1,1).daysTo(QDate(2026,10,15))));
  QCOMPARE(u(12,2),quint64(15708));QCOMPARE(u(16,4),quint64(0));QCOMPARE(u(20,4),quint64(1));QCOMPARE(u(32,2),quint64(1000));
  a.setSpeed(0);a.advance(1);m=encode(a.vesselWpBearing);QCOMPARE(u(6,4),quint64(0xffffffff));QCOMPARE(u(10,2),quint64(0xffff));
  a.setSpeed(100);a.advance(2);m=encode(a.vesselWpBearing);QCOMPARE(u(1,4),quint64(0));QCOMPARE(m.Data[5]&0x3c,0x14);QCOMPARE(u(14,2),quint64(0xffff));
  tN2kMsg x;SetN2kPGN129283(x,7,N2kxtem_Autonomous,true,-12.34);QCOMPARE(x.Data[1]&0x40,0x40);
  qint32 raw=qint32(quint32(x.Data[2])|quint32(x.Data[3])<<8|quint32(x.Data[4])<<16|quint32(x.Data[5])<<24);QCOMPARE(raw,-1234);
 }
 void gpxImports_data() {
  QTest::addColumn<QString>("xml");QTest::addColumn<bool>("accepted");
  const QString pts="<rtept lat='42' lon='-70'><name>A</name><extensions><opencpn:guid>test</opencpn:guid></extensions></rtept><rtept lat='42.01' lon='-70'><name>B</name></rtept>";
  const QString root="<gpx xmlns='http://www.topografix.com/GPX/1/1' xmlns:opencpn='http://www.opencpn.org' version='1.1'>";
  QTest::newRow("opencpn-route")<<root+"<wpt lat='0' lon='0'/><rte><name>Export</name>"+pts+"</rte></gpx>"<<true;
  QTest::newRow("track")<<QString("<gpx><trk><trkseg><trkpt lat='42' lon='-70'/><trkpt lat='42.01' lon='-70'/></trkseg></trk></gpx>")<<true;
  QTest::newRow("standalone-points")<<QString("<gpx><wpt lat='42' lon='-70'/><wpt lat='42.01' lon='-70'/></gpx>")<<false;
  QTest::newRow("multiple-routes")<<root+"<rte>"+pts+"</rte><rte>"+pts+"</rte></gpx>"<<false;
  QTest::newRow("empty")<<QString("<gpx/>")<<false;
  QTest::newRow("missing-coordinate")<<QString("<gpx><rte><rtept lat='42'/><rtept lat='43' lon='-70'/></rte></gpx>")<<false;
  QTest::newRow("invalid-coordinate")<<QString("<gpx><rte><rtept lat='NaN' lon='-70'/><rtept lat='43' lon='-70'/></rte></gpx>")<<false;
  QTest::newRow("truncated")<<root+"<rte>"+pts<<false;
 }
 void gpxImports() {
  QFETCH(QString,xml);QFETCH(bool,accepted);QTemporaryDir dir;QFile f(dir.filePath("route.gpx"));QVERIFY(f.open(QIODevice::WriteOnly));f.write(xml.toUtf8());f.close();
  AutoPilotSimulator a;QCOMPARE(a.loadRoute(f.fileName()),accepted);
  if(accepted){QCOMPARE(a.route.size(),2);a.setSpeed(2);a.start();a.advance(1);QVERIFY(a.position.latitude()>42);}
  else {a.start();QVERIFY(!a.running);QVERIFY(a.route.isEmpty());}
 }
 void malformedRouteFailsPromptly() {
  QTemporaryDir dir;QFile f(dir.filePath("bad.kml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("<kml><Placemark><name>broken");f.close();
  AutoPilotSimulator a;QVERIFY(!a.loadRoute(f.fileName()));a.start();QVERIFY(!a.updateTimer.isActive());
 }
 void singletonCannotStartReverse() {
  AutoPilotSimulator a;a.route={{QGeoCoordinate(42,-70),"A"}};a.setReverseRoute(true);a.start();QVERIFY(!a.updateTimer.isActive());
 }
 void routeCompletesAndStops() {
  AutoPilotSimulator a;QGeoCoordinate p(42,-70);a.route={{p,"A"},{p.atDistanceAndAzimuth(5,90),"B"}};a.setSpeed(10);
  QSignalSpy done(&a,&AutoPilotSimulator::routeCompleted);a.start();a.advance(1.0);a.advance(1.0);QCOMPARE(done.count(),1);QVERIFY(!a.updateTimer.isActive());
 }
 void restartResetsDistance() {
  AutoPilotSimulator a; a.route={{QGeoCoordinate(42,-70),"A"},{QGeoCoordinate(42.01,-70),"B"}};
  a.setSpeed(10); a.start(); a.updateTimer.stop(); a.advance(1.0);
  QGeoCoordinate first; connect(&a,&AutoPilotSimulator::LatLongChanged,this,[&](double x,double y){first={x,y};});
  a.stop();a.start();a.updateTimer.stop();a.advance(1.0);
  QVERIFY(qAbs(a.route[0].coordinate.distanceTo(first)-10)<0.01);
 }
 void instancesHaveIndependentDistance() {
  AutoPilotSimulator a,b; a.route=b.route={{QGeoCoordinate(42,-70),"A"},{QGeoCoordinate(42.01,-70),"B"}};
  a.setSpeed(10);b.setSpeed(10);a.start();b.start();a.updateTimer.stop();b.updateTimer.stop();
  QGeoCoordinate pos;connect(&b,&AutoPilotSimulator::LatLongChanged,this,[&](double x,double y){pos={x,y};});
  a.advance(1.0);b.advance(1.0);QVERIFY(qAbs(b.route[0].coordinate.distanceTo(pos)-10)<0.01);
 }
 void reverseDoesNotSkipLeg() {
  AutoPilotSimulator a;QGeoCoordinate p(42,-70),q=p.atDistanceAndAzimuth(10,0),r=q.atDistanceAndAzimuth(10,90);
  a.route={{p,"A"},{q,"B"},{r,"C"}};a.setSpeed(10.001);a.setReverseRoute(true);a.start();a.updateTimer.stop();
  QGeoCoordinate pos;connect(&a,&AutoPilotSimulator::LatLongChanged,this,[&](double x,double y){pos={x,y};});
  for(int i=0;i<3;++i)a.advance(1.0);
  QVERIFY(pos.distanceTo(q)<0.01);QCOMPARE(a.vesselWpBearing.DestinationWaypointNumber,uint32_t(0));
 }
 void neutralPublishesZeroSpeed() {
  MoveVesselSimulator a; VesselStatus s;int count=0;connect(&a,&MoveVesselSimulator::vesselStatusUpdated,this,[&](const VesselStatus &v){s=v;++count;});
  a.setEnabled(true);a.updateTimer.stop();a.setGearPort(N2kTG_Forward);a.setGearStarboard(N2kTG_Forward);a.setThrottlePositionPort(50);a.setThrottlePositionStarboard(50);a.updateVesselPosition();QVERIFY(s.sog>0);
  a.setGearPort(N2kTG_Neutral);a.setGearStarboard(N2kTG_Neutral);a.updateVesselPosition();QCOMPARE(count,2);QCOMPARE(s.sog,0.0);
 }
 void singleEngineReverseMovesAstern() {
  MoveVesselSimulator a;VesselStatus s;connect(&a,&MoveVesselSimulator::vesselStatusUpdated,this,[&](const VesselStatus &v){s=v;});
  a.setLastLatLong(42,-70);a.setEnabled(true);a.updateTimer.stop();a.setGearPort(N2kTG_Reverse);a.setThrottlePositionPort(50);a.updateVesselPosition();QVERIFY(s.latitude<42);
 }
};
QTEST_GUILESS_MAIN(NavigationTests)
#include "navigation_tests.moc"
