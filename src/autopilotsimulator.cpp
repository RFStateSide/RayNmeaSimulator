/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
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

#include "autopilotsimulator.h"
#include <cmath>
#include <algorithm>
#include <QDebug>
#include <limits>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(autopilotNavigation, "raynmea.autopilot.navigation", QtWarningMsg)

namespace {
constexpr double earthRadius=6371007.2; // QGeoCoordinate spherical distance model
constexpr double arrivalRadius=10.0; // existing simulator arrival circle, metres
}

AutoPilotSimulator::AutoPilotSimulator(QObject *parent) : QObject(parent) {
    connect(&updateTimer,&QTimer::timeout,this,&AutoPilotSimulator::calculateNextCoordinate);
}
AutoPilotSimulator::~AutoPilotSimulator() { updateTimer.stop(); }
bool AutoPilotSimulator::loadRoute(const QString &path, bool confirmedPointSequence) {
    auto candidate = RouteImport::read(path);
    if (candidate && candidate.pointSequence && !confirmedPointSequence) {
        candidate.error = "Point-only KML is not an ordered path. Confirm document-order waypoint use in Autopilot, or export a LineString.";
        qWarning() << "KML import not activated:" << path << candidate.error;
    }
    routeError = candidate.error;
    if (!candidate) {
        emit statusMessage(routeError);
        return false; // Keep the active route, timer, progression and metadata untouched.
    }
    return loadRoute(candidate.waypoints);
}
bool AutoPilotSimulator::loadRoute(const QVector<Waypoint> &candidate) {
    if(candidate.size()<2) {routeError="Route needs at least two waypoints";return false;}
    for(int i=0;i<candidate.size();++i) {
        if(!candidate[i].coordinate.isValid() || !std::isfinite(candidate[i].speedMs) || candidate[i].speedMs<0 ||
            (i && candidate[i-1].coordinate.distanceTo(candidate[i].coordinate)<0.001)) {
            routeError="Invalid route waypoint or consecutive duplicate";return false;
        }
    }
    routeError.clear();
    stop();
    route = candidate;
    currentIndex = 0; forwardDirection = true; completed = false;
    accumulatedDistance = totalDistanceTraveled = 0;
    realElapsedSeconds = 0;
    position = QGeoCoordinate(); travelTimer.invalidate();
    environment.reset(); environmentalTrack = false; environmentalResult = {};
    publishMetadata(); emit routeLoaded(true);
    return true; // Explicit start remains the caller's responsibility.
}
void AutoPilotSimulator::start() {
    if(route.size()<2)return;
    updateTimer.stop();currentIndex=0;forwardDirection=true;completed=false;running=true;
    accumulatedDistance=totalDistanceTraveled=0;position=route.first().coordinate;
    environment.reset();environmentalTrack=false;environmentalResult={};environmentalPropulsionSpeed=0;
    vesselWpBearing=VesselWpBearing();travelTimer.start();
    lastTickMs=elapsedNow();realElapsedSeconds=0;
    if(speedSource==AutopilotSpeedSource::RouteFile)speedMs=route.first().speedMs;
    publishMetadata();updateTimer.start(1000);emit RunningStateChanged(true);
}
void AutoPilotSimulator::stop() {
    updateTimer.stop();running=false;
    publishMetadata();emit RunningStateChanged(false);
}
double AutoPilotSimulator::getSpeed() const { return speedMs; }
void AutoPilotSimulator::setSpeed(double value) {
    speedMs=std::isfinite(value) && value>=0 && value!=N2kDoubleNA ? value : 0;
    if(running)publishMetadata();
}
double AutoPilotSimulator::effectiveSpeed() const {
    return speedMs <= std::numeric_limits<double>::max()/multiplier ? speedMs*multiplier : 0;
}
bool AutoPilotSimulator::setTraversalMultiplier(int value) {
    if(value<1 || value>10)return false;
    if(value==multiplier)return true;
    // Settle real time at the old factor before changing it. No route reset or
    // global clock acceleration; metadata immediately reflects the new factor.
    if(running && updateTimer.isActive())calculateNextCoordinate();
    multiplier=value;
    if(running)publishMetadata();
    return true;
}
void AutoPilotSimulator::setSpeedSource(AutopilotSpeedSource source) { speedSource=source; }
void AutoPilotSimulator::setReverseRoute(bool reverse) { reverseRoute=reverse; }
int AutoPilotSimulator::destinationIndex() const { return currentIndex+(forwardDirection?1:-1); }
void AutoPilotSimulator::calculateNextCoordinate() {
    if(!running)return;
    const qint64 now=elapsedNow();
    if(now<lastTickMs) {
        navigationFailure("monotonic elapsed time moved backwards",(now-lastTickMs)/1000.0,position,position,0);
        publishMetadata();emit RunningStateChanged(false);return;
    }
    const double seconds=(now-lastTickMs)/1000.0;
    lastTickMs=now;
    advance(seconds);
}
void AutoPilotSimulator::advance(double seconds) {
    if(!running || route.size()<2)return;
    if(!std::isfinite(seconds) || seconds<0) {
        navigationFailure("invalid elapsed interval",seconds,position,position,0);
        publishMetadata();emit RunningStateChanged(false);return;
    }
    if(environment.active() || environmentalTrack) { advanceEnvironmental(seconds);return; }
    const auto previous=position;
    const double previousDistance=totalDistanceTraveled;
    double remaining=seconds;
    // Bound pathological speed/route inputs without risking an event-loop hang.
    int transitions=0;
    while(remaining>1e-9 && transitions<10000) {
        const int next=destinationIndex();
        if(next<0 || next>=route.size()){stop();return;}
        const auto &origin=route[currentIndex].coordinate;const auto &dest=route[next].coordinate;
        const double length=origin.distanceTo(dest);
        if(speedSource==AutopilotSpeedSource::RouteFile)speedMs=route[currentIndex].speedMs;
        const double speed=effectiveSpeed();
        if(speedMs>0 && speed==0) {
            navigationFailure("speed exceeds numerical limits",seconds,position,position,0);break;
        }
        if(speed<=0) { remaining=0;break; }
        const double left=std::max(0.0,length-accumulatedDistance);
        const double distance=std::min(speed*remaining,left);
        if(!std::isfinite(distance) || !std::isfinite(totalDistanceTraveled+distance)) {
            navigationFailure("route distance invariant",seconds,position,position,distance);break;
        }
        accumulatedDistance+=distance;totalDistanceTraveled+=distance;remaining-=distance/speed;
        if(accumulatedDistance+1e-7<length) {
            position=origin.atDistanceAndAzimuth(accumulatedDistance,origin.azimuthTo(dest));break;
        }
        position=dest;
        const bool endpoint=forwardDirection ? next==route.size()-1 : next==0;
        if(endpoint && !reverseRoute) {
            completed=true;running=false;updateTimer.stop();break;
        }
        currentIndex=next;accumulatedDistance=0;
        if(endpoint)forwardDirection=!forwardDirection;
        ++transitions;
    }
    if(transitions==10000){running=false;updateTimer.stop();emit statusMessage("Route traversal limit reached; simulation stopped.");}
    realElapsedSeconds+=seconds-remaining;
    qCDebug(autopilotNavigation).nospace() << "advance dt=" << seconds
        << " base=" << speedMs << " factor=" << multiplier << " effective=" << effectiveSpeed()
        << " previous=" << previous << " position=" << position << " segment=" << currentIndex
        << " increment=" << totalDistanceTraveled-previousDistance << " total=" << totalDistanceTraveled
        << " environment=false";
    if(running && speedSource==AutopilotSpeedSource::RouteFile)speedMs=route[currentIndex].speedMs;
    publishMetadata();
    if(!running){emit RunningStateChanged(false);if(completed)emit routeCompleted();}
}
void AutoPilotSimulator::advanceEnvironmental(double seconds) {
    if(seconds<=0)return;
    environmentalTrack=true;
    const auto initialPosition=position;
    const double initialDistance=totalDistanceTraveled;
    double left=seconds;
    // Same existing route target/steering authority; bounded integration substeps.
    for(int step=0;left>1e-9 && running && step<10000;++step) {
        // Effective propulsion controls navigation; weather response and wave
        // phase retain real time and are independent of the speed multiplier.
        const int next=destinationIndex();if(next<0 || next>=route.size()){stop();return;}
        const auto dest=route[next].coordinate;
        if(speedSource==AutopilotSpeedSource::RouteFile)speedMs=route[currentIndex].speedMs;
        double dt=std::min(0.5,left);
        const double distanceToTarget=position.distanceTo(dest);
        if(effectiveSpeed()>0 && distanceToTarget>1e-6)
            dt=std::min(dt,distanceToTarget/effectiveSpeed());
        left-=dt;
        helmHeading=position.azimuthTo(dest);
        environmentalPropulsionSpeed=effectiveSpeed();
        environmentalResult=environment.advance(dt,helmHeading,environmentalPropulsionSpeed);
        if(!environmentalResult.applied) {
            environmentalResult={true,effectiveSpeed()*std::cos(qDegreesToRadians(helmHeading)),effectiveSpeed()*std::sin(qDegreesToRadians(helmHeading)),effectiveSpeed(),effectiveSpeed()>1e-6?helmHeading:-1,0,0,0};
        }
        if(!std::isfinite(environmentalResult.speed) || (speedMs>0 && effectiveSpeed()==0)) {
            navigationFailure("speed exceeds numerical limits",seconds,position,position,0);break;
        }
        const auto previous=position;
        auto ground=environmentalResult;
        const auto candidate=EnvironmentalMotion::integrate(previous,ground,dt);
        const double increment=previous.distanceTo(candidate);
        const double expected=ground.speed*dt;
        if(!candidate.isValid() || !std::isfinite(increment) || !std::isfinite(expected)
           || std::abs(increment-expected)>std::max(0.001,expected*1e-6)
           || !std::isfinite(totalDistanceTraveled+increment)) {
            navigationFailure("ground distance/position invariant",dt,previous,candidate,increment);
            break;
        }
        position=candidate;
        totalDistanceTraveled+=increment;
        const double arrival=std::max(arrivalRadius,ground.speed*dt);
        if(position.distanceTo(dest)<=arrival || passedPerpendicular(route[currentIndex].coordinate,dest,position)) {
            const bool endpoint=forwardDirection ? next==route.size()-1 : next==0;
            if(endpoint && !reverseRoute) {completed=true;running=false;updateTimer.stop();}
            else {currentIndex=next;accumulatedDistance=0;if(endpoint)forwardDirection=!forwardDirection;}
        }
    }
    realElapsedSeconds+=seconds-left;
    qCDebug(autopilotNavigation).nospace() << "advance dt=" << seconds
        << " base=" << speedMs << " factor=" << multiplier << " effective=" << effectiveSpeed()
        << " previous=" << initialPosition << " position=" << position << " segment=" << currentIndex
        << " increment=" << totalDistanceTraveled-initialDistance << " total=" << totalDistanceTraveled
        << " ground=" << environmentalResult.speed
        << " driftNorth=" << environmentalResult.north-environmentalPropulsionSpeed*std::cos(qDegreesToRadians(helmHeading))
        << " driftEast=" << environmentalResult.east-environmentalPropulsionSpeed*std::sin(qDegreesToRadians(helmHeading));
    if(left>1e-9 && running) {running=false;updateTimer.stop();emit statusMessage("Route traversal limit reached; simulation stopped.");}
    publishMetadata();
    if(!running){emit RunningStateChanged(false);if(completed)emit routeCompleted();}
}

void AutoPilotSimulator::navigationFailure(const char *reason,double seconds,const QGeoCoordinate &previous,
                                          const QGeoCoordinate &candidate,double increment) {
    qWarning().nospace() << "Autopilot navigation invariant failed: " << reason
        << "; dt=" << seconds << "; base=" << speedMs << "; factor=" << multiplier
        << "; effective=" << effectiveSpeed() << "; previous=" << previous
        << "; candidate=" << candidate << "; segment=" << currentIndex
        << "; increment=" << increment << "; total=" << totalDistanceTraveled;
    running=false;updateTimer.stop();
    emit statusMessage("Autopilot navigation invariant failed; stopped. See Application Output for details.");
}

double AutoPilotSimulator::crossTrack(const QGeoCoordinate &origin,const QGeoCoordinate &dest,const QGeoCoordinate &p) {
    const double delta=origin.distanceTo(p)/earthRadius;
    const double angle=qDegreesToRadians(origin.azimuthTo(p)-origin.azimuthTo(dest));
    // Positive is to starboard/right of the directed leg, as in PGN129283.
    return earthRadius*std::asin(std::clamp(std::sin(delta)*std::sin(angle),-1.0,1.0));
}
bool AutoPilotSimulator::passedPerpendicular(const QGeoCoordinate &origin,const QGeoCoordinate &dest,const QGeoCoordinate &p) {
    const double delta=origin.distanceTo(p)/earthRadius;
    const double angle=qDegreesToRadians(origin.azimuthTo(p)-origin.azimuthTo(dest));
    const double along=earthRadius*std::atan2(std::sin(delta)*std::cos(angle),std::cos(delta));
    return along+0.001>=origin.distanceTo(dest);
}
void AutoPilotSimulator::publishMetadata() {
    auto &v=vesselWpBearing;
    v.EtaTimeS=N2kDoubleNA;v.EtaDate=static_cast<int16_t>(N2kUInt16NA);v.RemainingTimeS=N2kDoubleNA;
    v.WaypointClosingVelocity=0;v.TotalDistanceTraveledM=totalDistanceTraveled;
    v.TimeTraveledS=realElapsedSeconds;
    // Keep the established optional-ground-speed sentinel at ordinary 1x.
    // The widget uses closing velocity as SOG in that case, exactly as before.
    v.GroundSpeed=multiplier==1 && !environmentalTrack ? -1 : (running ? effectiveSpeed() : 0);
    if(!environmentalTrack)v.WaveHeave=N2kDoubleNA;
    const int next=destinationIndex();
    if(route.size()<2 || next<0 || next>=route.size() || !position.isValid()) {
        v.WaypointName.clear();v.DistanceToWaypointM=N2kDoubleNA;v.Xte=N2kDoubleNA;
        v.BearingOriginToDestinationWaypoint=v.BearingPositionToDestinationWaypoint=N2kDoubleNA;
        v.DestinationLatitude=v.DestinationLongitude=N2kDoubleNA;
        v.OriginWaypointNumber=v.DestinationWaypointNumber=N2kUInt32NA;
        v.ArrivalCircleEntered=v.PerpendicularCrossed=false;
        emit VesselWpBearingChanged(v);return;
    }
    const auto &origin=route[currentIndex].coordinate;const auto &dest=route[next].coordinate;
    v.Reference=N2khr_true;v.WaypointName=route[next].name;
    v.OriginWaypointNumber=currentIndex;v.DestinationWaypointNumber=next;
    v.DestinationLatitude=dest.latitude();v.DestinationLongitude=dest.longitude();
    v.DistanceToWaypointM=position.distanceTo(dest);
    if(v.DistanceToWaypointM<0.001)v.DistanceToWaypointM=0;
    v.BearingOriginToDestinationWaypoint=qDegreesToRadians(origin.azimuthTo(dest));
    v.BearingPositionToDestinationWaypoint=v.DistanceToWaypointM>0 ? qDegreesToRadians(position.azimuthTo(dest)) : N2kDoubleNA;
    v.CourseRadians=v.DistanceToWaypointM>0 ? v.BearingPositionToDestinationWaypoint : v.BearingOriginToDestinationWaypoint;
    v.Xte=crossTrack(origin,dest,position);if(std::abs(v.Xte)<1e-6)v.Xte=0;
    v.PerpendicularCrossed=passedPerpendicular(origin,dest,position);
    v.ArrivalCircleEntered=v.DistanceToWaypointM<=arrivalRadius;
    v.WaypointClosingVelocity=running ? effectiveSpeed() : 0;
    if((running && effectiveSpeed()>0) || completed) {
        v.RemainingTimeS=completed?0:v.DistanceToWaypointM/effectiveSpeed();
        const auto now=utcNow().toUTC();const double milliseconds=v.RemainingTimeS*1000;
        if(std::isfinite(milliseconds) && milliseconds>=0 && milliseconds<1e12) {
            const auto eta=now.addMSecs(qRound64(milliseconds));
            const auto days=QDate(1970,1,1).daysTo(eta.date());
            if(eta.isValid() && days>=0 && days<65533) {
                v.EtaTimeS=eta.time().msecsSinceStartOfDay()/1000.0;v.EtaDate=static_cast<int16_t>(days);
            }
        }
    }
    if(environmentalTrack) {
        // Re-publish a changed operator speed without advancing weather time or
        // feeding the previously published ground speed back into propulsion.
        const double delta=effectiveSpeed()-environmentalPropulsionSpeed;
        environmentalResult.north+=delta*std::cos(qDegreesToRadians(helmHeading));
        environmentalResult.east+=delta*std::sin(qDegreesToRadians(helmHeading));
        environmentalPropulsionSpeed=effectiveSpeed();
        environmentalResult.speed=std::hypot(environmentalResult.north,environmentalResult.east);
        environmentalResult.course=environmentalResult.speed>1e-6
            ? std::fmod(qRadiansToDegrees(std::atan2(environmentalResult.east,environmentalResult.north))+360.0,360.0) : -1;
        v.HeadingRadians=qDegreesToRadians(helmHeading);
        v.CourseRadians=environmentalResult.course>=0?qDegreesToRadians(environmentalResult.course):N2kDoubleNA;
        v.GroundSpeed=running?environmentalResult.speed:0;
        v.WavePitch=environmentalResult.pitch;v.WaveRoll=environmentalResult.roll;v.WaveHeave=environmentalResult.wavesApplied?environmentalResult.heave:N2kDoubleNA;
        // Closing velocity is the ground vector projected toward the unchanged target.
        const double bearing=position.azimuthTo(dest);
        v.WaypointClosingVelocity=running?std::max(0.0,environmentalResult.north*std::cos(qDegreesToRadians(bearing))+environmentalResult.east*std::sin(qDegreesToRadians(bearing))):0;
        if(running) {
            v.RemainingTimeS=v.EtaTimeS=N2kDoubleNA;v.EtaDate=static_cast<int16_t>(N2kUInt16NA);
            if(v.WaypointClosingVelocity>1e-6) {
                v.RemainingTimeS=v.DistanceToWaypointM/v.WaypointClosingVelocity;
                if(v.RemainingTimeS*1000<1e12) {
                    const auto eta=utcNow().toUTC().addMSecs(qRound64(v.RemainingTimeS*1000));
                    const auto days=QDate(1970,1,1).daysTo(eta.date());
                    if(eta.isValid() && days>=0 && days<65533) {v.EtaTimeS=eta.time().msecsSinceStartOfDay()/1000.0;v.EtaDate=static_cast<int16_t>(days);}
                }
            }
        }
    }
    updateLatLong(position.latitude(),position.longitude());
    emit headingUpdated(v.HeadingRadians>=0 ? v.HeadingRadians : v.CourseRadians);emit XTEChanged(v.Xte);
    emit distanceToNextWaypoint(v.DistanceToWaypointM);emit timeToNextWaypoint(v.RemainingTimeS);
    emit VesselWpBearingChanged(v);
}
void AutoPilotSimulator::updateLatLong(double latitude,double longitude) { emit LatLongChanged(latitude,longitude); }
