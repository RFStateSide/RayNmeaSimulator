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

#ifndef ENVIRONMENTALMOTION_H
#define ENVIRONMENTALMOTION_H
#include <QGeoCoordinate>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>
#include "datastructs.h"

// Transient canonical input snapshot, never a second authoritative measurement store.
struct EnvironmentalConditions {
    bool windValid = false, wavesValid = false;
    double windSpeed = 0, windFrom = 0; // m/s, radians true FROM
    double waveHeight = 0, wavePeriod = 0, waveFrom = 0; // m, s, radians true FROM
};

// Shared calculation only: each existing movement loop advances its own state.
// Representative coefficients and caps, not a hydrodynamic/training model.
class EnvironmentalMotion {
public:
    bool enabled = false;
    MyVessel vessel;
    std::function<EnvironmentalConditions()> source;
    struct Result { bool applied=false; double north=0,east=0,speed=0,course=-1,pitch=0,roll=0,heave=0; bool wavesApplied=false; };
    void reset() { north=east=pitchAmplitude=rollAmplitude=heaveAmplitude=phase=0;period=8; }
    bool active() const { const auto c=enabled && source?source():EnvironmentalConditions{}; return (enabled && (c.windValid || c.wavesValid)) || std::abs(north)+std::abs(east)+std::abs(pitchAmplitude)+std::abs(rollAmplitude)+heaveAmplitude>1e-8; }
    Result advance(double seconds, double headingDegrees, double propulsionSpeed) {
        Result r;
        if (!active() || !std::isfinite(seconds) || seconds<=0) return r;
        const auto c=enabled && source ? source() : EnvironmentalConditions{};
        const double heading=qDegreesToRadians(headingDegrees);
        const double blend=-std::expm1(-seconds/5.0); // continuous five-second response
        double targetNorth=0,targetEast=0,pa=0,ra=0,ha=0;
        if(c.windValid && std::isfinite(c.windSpeed) && c.windSpeed>=0 && std::isfinite(c.windFrom)) {
            const double size=std::clamp(std::sqrt(std::max(1.0,vessel.Length*vessel.Beam)/39.0),0.5,1.5);
            const double drift=std::min(1.5,c.windSpeed*0.025*size);
            targetNorth=-drift*std::cos(c.windFrom);targetEast=-drift*std::sin(c.windFrom);
        }
        if(c.wavesValid && std::isfinite(c.waveHeight) && c.waveHeight>=0 && std::isfinite(c.wavePeriod)
           && c.wavePeriod>0 && std::isfinite(c.waveFrom)) {
            const double height=std::min(c.waveHeight,12.0),relative=c.waveFrom-heading;
            pa=std::min(10.0,qRadiansToDegrees(std::atan2(height*0.25,std::max(1.0,vessel.Length))))*std::cos(relative);
            ra=std::min(15.0,qRadiansToDegrees(std::atan2(height*0.25,std::max(1.0,vessel.Beam))))*std::sin(relative);
            ha=std::min(1.5,height*0.15);
            period+=blend*(std::clamp(c.wavePeriod,2.0,30.0)-period);
        }
        north+=blend*(targetNorth-north);east+=blend*(targetEast-east);
        pitchAmplitude+=blend*(pa-pitchAmplitude);rollAmplitude+=blend*(ra-rollAmplitude);heaveAmplitude+=blend*(ha-heaveAmplitude);
        auto settle=[](double&v,double target){if(target==0 && std::abs(v)<1e-8)v=0;};
        settle(north,targetNorth);settle(east,targetEast);settle(pitchAmplitude,pa);settle(rollAmplitude,ra);settle(heaveAmplitude,ha);
        phase=std::fmod(phase+2*M_PI*seconds/period,2*M_PI);
        r.applied=true;r.north=propulsionSpeed*std::cos(heading)+north;r.east=propulsionSpeed*std::sin(heading)+east;
        r.speed=std::hypot(r.north,r.east);
        r.course=r.speed>1e-6 ? std::fmod(qRadiansToDegrees(std::atan2(r.east,r.north))+360.0,360.0) : -1;
        r.wavesApplied=c.wavesValid || std::abs(pitchAmplitude)+std::abs(rollAmplitude)+heaveAmplitude>1e-8;
        r.pitch=pitchAmplitude*std::sin(phase);r.roll=rollAmplitude*std::sin(phase);r.heave=heaveAmplitude*std::cos(phase);
        return r;
    }
    static QGeoCoordinate integrate(const QGeoCoordinate &p,const Result&r,double seconds) {
        if(!p.isValid() || !r.applied || r.speed<=1e-6) return p;
        return p.atDistanceAndAzimuth(r.speed*seconds,r.course);
    }
private:
    double north=0,east=0,pitchAmplitude=0,rollAmplitude=0,heaveAmplitude=0,phase=0,period=8;
};
#endif
