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

#include "movevesselsimulator.h"

MoveVesselSimulator::MoveVesselSimulator(QObject *parent)
    : QObject(parent)
    , isEnabled(false)
    , gearPort(N2kTG_Neutral)
    , gearStarboard(N2kTG_Neutral)
    , throttlePositionPort(0.0)
    , throttlePositionStarboard(0.0)
    , latitude(0.0)
    , longitude(0.0)
    , rudderPositionPort(0.0)
    , rudderPositionStarboard(0.0)
    , heading(0.0)
    , pitch(0.0)
    , roll(0.0)
{
    connect(&updateTimer, &QTimer::timeout, this, &MoveVesselSimulator::updateVesselPosition);
    updateTimer.setInterval(500);
}

void MoveVesselSimulator::setEnabled(bool enabled)
{
    isEnabled = enabled;
    if (isEnabled) {
        environment.reset();
        updateTimer.start();
        /*        qDebug() << "Vessel simulator enabled throttlePort:" << throttlePositionPort << "throttleStarboard:" << throttlePositionStarbard
                 << "gearPort:" << gearPort << "gearStarboard:" << gearStarbard << "wotSpeed:" << wotSpeedMs << "rudderPort:" << rudderPositionPort
                 << "rudderStarboard:" << rudderPositionStarbard << "heading:" << heading << "latitude:" << latitude << "longitude:" << longitude;
*/
    } else {
        updateTimer.stop();
        environment.reset();
    }
}

void MoveVesselSimulator::setHeading(double heading)
{
    this->heading = heading;
    //qDebug() << "Vessel simulator heading set to:" << this->heading;
}

void MoveVesselSimulator::setGearPort(tN2kTransmissionGear transmissionGear)
{
    gearPort = transmissionGear;
}

void MoveVesselSimulator::setGearStarboard(tN2kTransmissionGear transmissionGear)
{
    gearStarboard = transmissionGear;
}

void MoveVesselSimulator::setThrottlePositionPort(double throttlePosition)
{
    throttlePositionPort = throttlePosition;
}

void MoveVesselSimulator::setThrottlePositionStarboard(double throttlePosition)
{
    throttlePositionStarboard = throttlePosition;
}

void MoveVesselSimulator::setLastLatLong(double lat, double lon)
{
    latitude = lat;
    longitude = lon;
}

void MoveVesselSimulator::setRudderPositionPort(double rudderPosition)
{
    rudderPositionPort = rudderPosition;
    //qDebug() << "Vessel simulator rudder position set to:" << rudderPosition;
}

void MoveVesselSimulator::setRudderPositionStarboard(double rudderPosition)
{
    rudderPositionStarboard = rudderPosition;
}

void MoveVesselSimulator::setMyVessel(MyVessel vessel)
{
    m_vessel = vessel;
    environment.vessel = vessel;
}

double MoveVesselSimulator::calculateSpeed()
{
    /*    // Calculate speed based on throttle positions and WOT speed
    double portSpeed = (gearPort == N2kTG_Forward ? throttlePositionPort : -throttlePositionPort) * wotSpeedMs / 100.0;
    double starboardSpeed = (gearStarbard == N2kTG_Forward ? throttlePositionStarbard : -throttlePositionStarbard) * wotSpeedMs / 100.0;

    return (portSpeed + starboardSpeed) / 2.0;
*/
    double portSpeed = 0.0;
    double starboardSpeed = 0.0;

    if (gearPort == N2kTG_Forward) {
        portSpeed = throttlePositionPort * m_vessel.WotSpeed / 100.0;
    } else if (gearPort == N2kTG_Reverse) {
        portSpeed = -throttlePositionPort * m_vessel.WotSpeed / 100.0;
    }

    if (gearStarboard == N2kTG_Forward) {
        starboardSpeed = throttlePositionStarboard * m_vessel.WotSpeed / 100.0;
    } else if (gearStarboard == N2kTG_Reverse) {
        starboardSpeed = -throttlePositionStarboard * m_vessel.WotSpeed / 100.0;
    }

    // Average speed for straight movement; if rotating, use the net direction
    return (gearPort == gearStarboard || gearPort == N2kTG_Neutral || gearStarboard == N2kTG_Neutral) ? (portSpeed + starboardSpeed) / 2.0
                                                                                                      : 0.0; // For counteracting gears, return 0 speed
}

double MoveVesselSimulator::calculateRudderPosition()
{
    // Average the rudder positions for port and starboard
    return (rudderPositionPort + rudderPositionStarboard) / 2.0;
}

double MoveVesselSimulator::calculateRateOfTurn()
{
    // Normalize rudder position to a range [-1, 1]
    double normalizedRudder = calculateRudderPosition() / 45.0; // Assuming 45° max rudder angle
    if (normalizedRudder > 1.0)
        normalizedRudder = 1.0;
    if (normalizedRudder < -1.0)
        normalizedRudder = -1.0;

    double speed = calculateSpeed();
    double baseROT = normalizedRudder * speed;

    // If engines are in opposing gears (rotating in place)
    if (gearPort != gearStarboard && gearPort != N2kTG_Neutral && gearStarboard != N2kTG_Neutral) {
        baseROT = (gearPort == N2kTG_Reverse ? -1 : 1) * 20.0 * (throttlePositionPort + throttlePositionStarboard) / 200.0;
    }

    double maxROT = 25.0; // Max ROT in degrees/sec
    if (baseROT > maxROT)
        baseROT = maxROT;
    if (baseROT < -maxROT)
        baseROT = -maxROT;

    //qDebug() << "Vessel simulator ROT:" << baseROT;
    return baseROT;
}

void MoveVesselSimulator::updateVesselPosition()
{
    if (!isEnabled) {
        return;
    }

    double speed = calculateSpeed();
    double rateOfTurn = calculateRateOfTurn();

    // Adjust heading and speed based on individual gear positions, including reverse scenarios
    if (gearPort == N2kTG_Forward && gearStarboard == N2kTG_Neutral) {
        rateOfTurn += throttlePositionPort * 0.1;
    } else if (gearStarboard == N2kTG_Forward && gearPort == N2kTG_Neutral) {
        rateOfTurn -= throttlePositionStarboard * 0.1;
    } else if (gearPort == N2kTG_Reverse && gearStarboard == N2kTG_Neutral) {
        rateOfTurn -= throttlePositionPort * 0.1;
    } else if (gearStarboard == N2kTG_Reverse && gearPort == N2kTG_Neutral) {
        rateOfTurn += throttlePositionStarboard * 0.1;
    }

    // Update heading and normalize within 0-360 degrees
    heading += rateOfTurn * 0.5;
    if (heading >= 360.0)
        heading -= 360.0;
    if (heading < 0.0)
        heading += 360.0;

    double distance = speed * 0.5;

    if (gearPort != gearStarboard && gearPort != N2kTG_Neutral && gearStarboard != N2kTG_Neutral) {
        distance = 0;
    }

    // Keep the accepted OFF path exactly; optional ground vector uses spherical integration.
    const auto effects=environment.advance(0.5,heading,speed);
    if(effects.applied) {
        const auto p=EnvironmentalMotion::integrate(QGeoCoordinate(latitude,longitude),effects,0.5);
        latitude=p.latitude();longitude=p.longitude();
    } else {
    // Update latitude and longitude based on heading and speed
    latitude += distance * std::cos(heading * M_PI / 180.0) / 111139.0;
    longitude += distance * std::sin(heading * M_PI / 180.0) / (111139.0 * std::cos(latitude * M_PI / 180.0));

    }

    // Calculate roll based on Rate of Turn and vessel beam
    roll = rateOfTurn * (m_vessel.Length / m_vessel.Beam) * 0.15; // Changed from (0.1) to (0.15) to increase the effect
    if (roll > 20.0)
        roll = 20.0; // Limit roll to 20 degrees max
    if (roll < -20.0)
        roll = -20.0;

    // Calculate pitch based on sudden throttle changes
    static double lastThrottlePositionPort = throttlePositionPort;
    static double lastThrottlePositionStarboard = throttlePositionStarboard;
    double throttleChange = ((throttlePositionPort - lastThrottlePositionPort) + (throttlePositionStarboard - lastThrottlePositionStarboard)) / 2.0;
    pitch += throttleChange * 0.15; // Adjust pitch based on throttle change. Changed (0.05) (0.15) to increase the effect
    if (pitch > 15.0)
        pitch = 15.0; // Limit pitch to 15 degrees max
    if (pitch < -5.0)
        pitch = -5.0; // Limit pitch when decelerating to a slight downward angle

    // Decay pitch to simulate leveling off
    pitch *= 0.95;

    // Update last throttle positions for the next calculation
    lastThrottlePositionPort = throttlePositionPort;
    lastThrottlePositionStarboard = throttlePositionStarboard;

    // Heave Calculation
    double rollEffect = std::abs(roll) * 0.1;       // Increase the roll effect on heave Changed from (0.1) to (0.2) to increase the effect
    double rotEffect = std::abs(rateOfTurn) * 0.02; // Increase ROT effect on heave

    heave = (rollEffect + rotEffect) * (m_vessel.Beam / m_vessel.Displacement) * 500.0;
    // Use the vessel's draft for limiting the heave effect
    double heaveLimit = std::max(m_vessel.Draft / 2, 1.0);
    if (heave > heaveLimit)
        heave = heaveLimit; // Increased heave limit to allow more noticeable effect

    /*    // Heave Calculation #2 for verifying the effect using angle and beam
    double heave2 = m_vessel.Beam / 2.0 * std::sin(roll * M_PI / 180.0);
    //qDebug() << "Heave" << heave << "Heave2" << heave2;
*/
    //qDebug() << "Heave" << heave << "RateOfTurn" << rateOfTurn << "Roll" << roll << "Pitch" << pitch;
    //qDebug() << "Latitude" << latitude << "Longitude" << longitude;
    VesselStatus status = {latitude, longitude, heading, speed, pitch, roll, heave, calculateRudderPosition(), rateOfTurn};

    if(effects.applied) {
        status.groundSpeed=effects.speed;status.groundCourse=effects.course;
        status.pitch=std::clamp(pitch+effects.pitch,-20.0,20.0);
        status.roll=std::clamp(roll+effects.roll,-35.0,35.0);
        // Bound the environmental contribution, retaining the existing maneuver heave.
        status.heave=heave+effects.heave;
    }
    emit vesselStatusUpdated(status);
}
