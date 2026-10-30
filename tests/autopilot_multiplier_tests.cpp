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
#include <functional>
#include <limits>
#include "convert.h"
#include "datastructs.h"
#include "environmentalmotion.h"
#include "N2kMessages.h"
#define private public
#include "autopilotsimulator.h"
#undef private

namespace {
void near(double a,double b,double tolerance=.001) { QVERIFY2(std::abs(a-b)<tolerance,qPrintable(QString("%1 != %2").arg(a,0,'g',15).arg(b,0,'g',15))); }
void route(AutoPilotSimulator &a, const QVector<double> &distances={0,1000}) {
    const QGeoCoordinate start(42,-70);
    for(int i=0;i<distances.size();++i)a.route.append({start.atDistanceAndAzimuth(distances[i],0),QString::number(i)});
    a.elapsedNow=[] {return 0;};a.utcNow=[] {return QDateTime(QDate(2026,10,18),QTime(12,0),QTimeZone::UTC);};
    a.setSpeed(10);
}
}
class AutopilotMultiplierTests : public QObject {
    Q_OBJECT
private slots:
    void factors_data() {QTest::addColumn<int>("factor");for(int i:{1,2,5,10})QTest::newRow(qPrintable(QString::number(i)))<<i;}
    void factors() {
        QFETCH(int,factor);AutoPilotSimulator a;route(a);QVERIFY(a.setTraversalMultiplier(factor));a.start();a.advance(2);
        near(a.totalDistanceTraveled,20*factor);near(a.position.distanceTo(a.route[0].coordinate),20*factor);
        near(a.vesselWpBearing.GroundSpeed>=0?a.vesselWpBearing.GroundSpeed:a.vesselWpBearing.WaypointClosingVelocity,10*factor);near(a.vesselWpBearing.WaypointClosingVelocity,10*factor);
        near(a.vesselWpBearing.RemainingTimeS,(1000-20.0*factor)/(10*factor));near(a.vesselWpBearing.TimeTraveledS,2);
        near(a.getSpeed(),10);near(a.vesselWpBearing.Xte,0);near(a.vesselWpBearing.CourseRadians,0);
    }
    void runtimeChangeAndEta() {
        AutoPilotSimulator a;route(a);qint64 clock=0;a.elapsedNow=[&]{return clock;};a.start();
        clock=1000;a.calculateNextCoordinate();near(a.totalDistanceTraveled,10);
        const auto p=a.position;const auto index=a.currentIndex;
        QVERIFY(a.setTraversalMultiplier(2));QCOMPARE(a.position,p);QCOMPARE(a.currentIndex,index);
        near(a.vesselWpBearing.RemainingTimeS,49.5);near(a.vesselWpBearing.EtaTimeS,12*3600+49.5);
        clock=2000;a.calculateNextCoordinate();near(a.totalDistanceTraveled,30);
        QVERIFY(a.setTraversalMultiplier(1));near(a.vesselWpBearing.RemainingTimeS,97);
        clock=3000;a.calculateNextCoordinate();near(a.totalDistanceTraveled,40);near(a.vesselWpBearing.TimeTraveledS,3);
        near(a.getSpeed(),10);QCOMPARE(a.currentIndex,0);
    }
    void delayedTicksAndPartialFactorChange() {
        AutoPilotSimulator a;route(a);qint64 clock=0;a.elapsedNow=[&]{return clock;};a.start();
        clock=500;QVERIFY(a.setTraversalMultiplier(2));near(a.totalDistanceTraveled,5); // old rate for elapsed half second
        clock=1750;a.calculateNextCoordinate();near(a.totalDistanceTraveled,30);near(a.vesselWpBearing.TimeTraveledS,1.75);
        clock=4250;a.calculateNextCoordinate();near(a.totalDistanceTraveled,80);near(a.vesselWpBearing.TimeTraveledS,4.25);
        a.calculateNextCoordinate();near(a.totalDistanceTraveled,80); // duplicate callback has zero elapsed time
    }
    void cornersSeveralWaypointsAndCompletion() {
        AutoPilotSimulator a;const QGeoCoordinate p(42,-70),q=p.atDistanceAndAzimuth(5,0),r=q.atDistanceAndAzimuth(5,90),s=r.atDistanceAndAzimuth(10,180);
        a.route={{p,"A"},{q,"B"},{r,"C"},{s,"D"}};a.elapsedNow=[] {return 0;};a.setSpeed(2);a.setTraversalMultiplier(10);a.start();
        QSignalSpy done(&a,&AutoPilotSimulator::routeCompleted);a.advance(.75);
        QCOMPARE(a.currentIndex,2);near(a.totalDistanceTraveled,15);near(a.position.distanceTo(r),5);
        a.advance(1);QVERIFY(!a.running);near(a.totalDistanceTraveled,20);near(a.position.distanceTo(s),0);near(a.vesselWpBearing.TimeTraveledS,1);
        QCOMPARE(done.count(),1);near(a.vesselWpBearing.GroundSpeed,0);near(a.vesselWpBearing.RemainingTimeS,0);
        a.advance(100);QCOMPARE(done.count(),1);near(a.vesselWpBearing.TimeTraveledS,1);
    }
    void reverseRouteFileSpeedsAndRestart() {
        AutoPilotSimulator a;route(a,{0,10,20});a.route[0].speedMs=2;a.route[1].speedMs=4;a.route[2].speedMs=5;
        a.setSpeedSource(AutopilotSpeedSource::RouteFile);a.setTraversalMultiplier(2);a.setReverseRoute(true);a.start();a.advance(5);
        QCOMPARE(a.currentIndex,1);QVERIFY(!a.forwardDirection);near(a.totalDistanceTraveled,32); // 2.5 s + 1.25 s + 1 s + 0.25 s
        // At 5 s the return leg has crossed waypoint 1 and continues toward 0.
    }
    void reverseExactBoundaries() {
        AutoPilotSimulator a;route(a,{0,10,20});a.setSpeed(5);a.setTraversalMultiplier(2);a.setReverseRoute(true);a.start();a.advance(3);
        near(a.totalDistanceTraveled,30);near(a.position.distanceTo(a.route[1].coordinate),0);QCOMPARE(a.currentIndex,1);QVERIFY(!a.forwardDirection);
        a.advance(1);near(a.totalDistanceTraveled,40);near(a.position.distanceTo(a.route[0].coordinate),0);QVERIFY(a.forwardDirection);
        a.stop();const auto p=a.position;QVERIFY(a.setTraversalMultiplier(10));a.advance(1);QCOMPARE(a.position,p);QVERIFY(!a.running);
        a.start();near(a.totalDistanceTraveled,0);near(a.vesselWpBearing.TimeTraveledS,0);QCOMPARE(a.traversalMultiplier(),10);QCOMPARE(a.currentIndex,0);
    }
    void invalidAndIdle() {
        AutoPilotSimulator a;route(a);QVERIFY(!a.setTraversalMultiplier(0));QVERIFY(!a.setTraversalMultiplier(-1));QVERIFY(!a.setTraversalMultiplier(11));QVERIFY(!a.setTraversalMultiplier(INT_MAX));QCOMPARE(a.traversalMultiplier(),1);
        a.setTraversalMultiplier(10);QVERIFY(!a.running);a.setSpeed(0);a.start();a.advance(5);near(a.totalDistanceTraveled,0);near(a.vesselWpBearing.TimeTraveledS,5);QCOMPARE(a.vesselWpBearing.RemainingTimeS,N2kDoubleNA);
        a.setSpeed(10);a.advance(1);near(a.totalDistanceTraveled,100);a.stop();auto elapsed=a.vesselWpBearing.TimeTraveledS;a.advance(1);near(a.vesselWpBearing.TimeTraveledS,elapsed);
    }
    void environmentalVectorAndRealWaveClock() {
        AutoPilotSimulator a,b;route(a,{0,10000});route(b,{0,10000});
        auto source=[] {EnvironmentalConditions c;c.windValid=c.wavesValid=true;c.windSpeed=8;c.windFrom=M_PI/2;c.waveHeight=2;c.wavePeriod=8;c.waveFrom=0;return c;};
        a.environment.enabled=b.environment.enabled=true;a.environment.source=b.environment.source=source;
        b.setTraversalMultiplier(2);a.start();b.start();a.advance(1);b.advance(1);
        near(b.totalDistanceTraveled,2*a.totalDistanceTraveled,.1);near(b.vesselWpBearing.GroundSpeed,std::hypot(20,a.environmentalResult.east),.01);
        near(b.vesselWpBearing.TimeTraveledS,1);near(b.vesselWpBearing.WavePitch,a.vesselWpBearing.WavePitch,.001);near(b.vesselWpBearing.WaveRoll,a.vesselWpBearing.WaveRoll,.001);
        const auto p=b.position;const auto base=b.getSpeed();b.updateTimer.stop();b.setTraversalMultiplier(5);QCOMPARE(b.position,p);near(b.getSpeed(),base);
        near(b.vesselWpBearing.GroundSpeed,b.environmentalResult.speed);near(b.environmentalPropulsionSpeed,50);
    }
    void actualRouteEquivalentSpeeds_data() {
        QTest::addColumn<int>("weather");QTest::addColumn<int>("factor");QTest::addColumn<bool>("reverse");
        for(int weather:{0,1,2})for(int factor:{1,2,4})for(bool reverse:{false,true})
            QTest::newRow(qPrintable(QString("weather%1-factor%2-reverse%3").arg(weather).arg(factor).arg(reverse)))<<weather<<factor<<reverse;
    }
    void actualRouteEquivalentSpeeds() {
        QFETCH(int,weather);QFETCH(int,factor);QFETCH(bool,reverse);
        AutoPilotSimulator reference,candidate;
        const QString file=QDir::currentPath()+"/routes/kml/mr_haverhill.kml";
        auto source=[weather] {EnvironmentalConditions c;c.windValid=weather==1;c.wavesValid=weather==2;
            c.windSpeed=8;c.windFrom=M_PI/2;c.waveHeight=2;c.wavePeriod=8;c.waveFrom=.7;return c;};
        for(auto *a:{&reference,&candidate}) {
            QVERIFY(a->loadRoute(file));QCOMPARE(a->route.size(),62);a->elapsedNow=[] {return 0;};
            a->environment.enabled=weather!=0;a->environment.source=source;a->setReverseRoute(reverse);
        }
        constexpr double effective=44.704; // 100 MPH in canonical m/s
        reference.setSpeed(effective);candidate.setSpeed(effective/factor);candidate.setTraversalMultiplier(factor);
        reference.start();candidate.start();QSignalSpy done(&candidate,&AutoPilotSimulator::routeCompleted);
        bool returned=false;
        for(int tick=1;tick<=4000 && candidate.isRunning();++tick) {
            const auto previous=candidate.position;const auto distance=candidate.totalDistanceTraveled;
            reference.advance(.5);candidate.advance(.5);
            QVERIFY(candidate.position.isValid());QVERIFY(std::isfinite(candidate.totalDistanceTraveled));
            const double increment=candidate.totalDistanceTraveled-distance;
            QVERIFY2(increment>=0 && increment<=(effective+1.5)*.5+.001,"distance exceeds effective ground-motion budget");
            QVERIFY(previous.distanceTo(candidate.position)<=increment+.001);
            near(candidate.totalDistanceTraveled,reference.totalDistanceTraveled,1e-6);
            near(candidate.position.distanceTo(reference.position),0,1e-6);
            QCOMPARE(candidate.currentIndex,reference.currentIndex);QCOMPARE(candidate.forwardDirection,reference.forwardDirection);
            near(candidate.vesselWpBearing.DistanceToWaypointM,candidate.position.distanceTo(candidate.route[candidate.destinationIndex()].coordinate),1e-6);
            near(candidate.vesselWpBearing.Xte,reference.vesselWpBearing.Xte,1e-6);
            QVERIFY(std::abs(candidate.vesselWpBearing.Xte)<30);
            if(candidate.running) {
                const auto &v=candidate.vesselWpBearing;
                if(v.WaypointClosingVelocity>0)near(v.RemainingTimeS,v.DistanceToWaypointM/v.WaypointClosingVelocity,1e-6);
            }
            if(tick==34 || tick==286)qInfo().nospace()<<"actual-route seconds="<<tick*.5<<" weather="<<weather<<" factor="<<factor
                <<" base="<<candidate.getSpeed()<<" position="<<candidate.position<<" segment="<<candidate.currentIndex
                <<" increment="<<increment<<" total="<<candidate.totalDistanceTraveled<<" XTE="<<candidate.vesselWpBearing.Xte;
            returned=returned || !candidate.forwardDirection;
            if(reverse && returned && candidate.forwardDirection)break;
        }
        near(candidate.getSpeed(),effective/factor);
        if(reverse){QVERIFY(returned);QVERIFY(candidate.forwardDirection);QCOMPARE(done.count(),0);candidate.stop();}
        else {QVERIFY(candidate.completed);QCOMPARE(done.count(),1);}
        const auto p=candidate.position;const double total=candidate.totalDistanceTraveled;
        candidate.advance(20);QCOMPARE(candidate.position,p);near(candidate.totalDistanceTraveled,total);
        candidate.start();QCOMPARE(candidate.currentIndex,0);near(candidate.totalDistanceTraveled,0);
        candidate.advance(.5);QVERIFY(candidate.totalDistanceTraveled>0);
    }
    void environmentalShortLegsAndRuntimeChanges() {
        AutoPilotSimulator a;route(a,{0,2,4,6,8,10});a.setSpeed(10);a.setTraversalMultiplier(4);
        a.environment.enabled=true;a.environment.source=[] {EnvironmentalConditions c;c.wavesValid=true;c.waveHeight=1;c.wavePeriod=8;return c;};
        qint64 clock=0;a.elapsedNow=[&]{return clock;};a.start();clock=100;a.calculateNextCoordinate();near(a.totalDistanceTraveled,4,.01);
        const auto p=a.position;const int index=a.currentIndex;a.setTraversalMultiplier(2);near(a.position.distanceTo(p),0);QCOMPARE(a.currentIndex,index);
        clock=200;a.calculateNextCoordinate();near(a.totalDistanceTraveled,6,.01);a.setTraversalMultiplier(4);
        clock=1000;a.calculateNextCoordinate();QVERIFY(a.completed);near(a.totalDistanceTraveled,10,.01);near(a.vesselWpBearing.TimeTraveledS,.3,.01);
        a.start();clock+=50;a.calculateNextCoordinate();a.stop();auto stopped=a.position;clock+=100000;a.calculateNextCoordinate();QCOMPARE(a.position,stopped);
    }
    void invariantFailureIsDiagnostic() {
        AutoPilotSimulator a;route(a,{0,100000});a.environment.enabled=true;
        a.environment.source=[] {EnvironmentalConditions c;c.windValid=true;c.windSpeed=1;return c;};
        a.start();a.totalDistanceTraveled=std::numeric_limits<double>::infinity();
        QSignalSpy errors(&a,&AutoPilotSimulator::statusMessage);
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression("Autopilot navigation invariant failed: ground distance/position invariant.*"));
        a.advance(1);QVERIFY(!a.running);QCOMPARE(errors.count(),1);QCOMPARE(a.position,a.route[0].coordinate);
    }
    void extremeArithmeticStopsSafely() {
        for(bool environmental:{false,true}) {
            AutoPilotSimulator a;route(a);a.setSpeed(std::numeric_limits<double>::max());a.setTraversalMultiplier(10);
            a.environment.enabled=environmental;a.environment.source=[] {EnvironmentalConditions c;c.windValid=true;c.windSpeed=1;return c;};
            a.start();QSignalSpy errors(&a,&AutoPilotSimulator::statusMessage);a.advance(1);
            QVERIFY(!a.running);QVERIFY(a.position.isValid());QCOMPARE(errors.count(),1);QVERIFY(std::isfinite(a.vesselWpBearing.GroundSpeed));
        }
    }
    void inMemoryNavigationWire() {
        AutoPilotSimulator a;route(a);a.setTraversalMultiplier(2);a.start();a.advance(2);const auto v=a.vesselWpBearing;
        tN2kMsg speed,navigation;SetN2kPGN129026(speed,1,N2khr_true,v.CourseRadians,v.GroundSpeed);
        unsigned char sid;tN2kHeadingReference ref;double course,sog;QVERIFY(ParseN2kPGN129026(speed,sid,ref,course,sog));near(sog,20,.011);
        SetN2kPGN129284(navigation,1,v.DistanceToWaypointM,N2khr_true,v.PerpendicularCrossed,v.ArrivalCircleEntered,N2kdct_GreatCircle,v.EtaTimeS,v.EtaDate,v.BearingOriginToDestinationWaypoint,v.BearingPositionToDestinationWaypoint,v.OriginWaypointNumber,v.DestinationWaypointNumber,v.DestinationLatitude,v.DestinationLongitude,v.WaypointClosingVelocity);
        auto raw=[&](int offset,int count){quint64 value=0;for(int i=0;i<count;++i)value|=quint64(navigation.Data[offset+i])<<(8*i);return value;};
        QCOMPARE(raw(1,4),quint64(96000));QCOMPARE(raw(32,2),quint64(2000));QCOMPARE(raw(6,4),quint64((12*3600+48)*10000));
        near(v.TotalDistanceTraveledM,40);near(v.TimeTraveledS,2);near(a.getSpeed(),10);
    }
};
QTEST_GUILESS_MAIN(AutopilotMultiplierTests)
#include "autopilot_multiplier_tests.moc"
