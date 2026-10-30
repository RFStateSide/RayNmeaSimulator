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

#ifndef MOVEVESSELSIMULATOR_H
#define MOVEVESSELSIMULATOR_H

#include <QDebug>
#include <QObject>
#include <QTimer>
#include "N2kTypes.h"
#include "datastructs.h"
#include <cmath>
#include "environmentalmotion.h"

class MoveVesselSimulator : public QObject
{
    Q_OBJECT

public:
    explicit MoveVesselSimulator(QObject *parent = nullptr);

    EnvironmentalMotion environment;
    void setEnabled(bool enabled);
    void setHeading(double heading);
    void setGearPort(tN2kTransmissionGear transmissionGear);
    void setGearStarboard(tN2kTransmissionGear transmissionGear);
    void setWotSpeed(double wotSpeedMs = 17.8816);
    void setThrottlePositionPort(double throttlePosition);
    void setThrottlePositionStarboard(double throttlePosition);
    void setLastLatLong(double latitude, double longitude);
    void setRudderPositionPort(double rudderPosition);
    void setRudderPositionStarboard(double rudderPosition);
    void setMyVessel(MyVessel vessel);

signals:
    void vesselStatusUpdated(const VesselStatus &status);

private slots:
    void updateVesselPosition();

private:
    QTimer updateTimer;
    bool isEnabled = false;
    tN2kTransmissionGear gearPort = N2kTG_Neutral;
    tN2kTransmissionGear gearStarboard = N2kTG_Neutral;
    double throttlePositionPort = 0.0;
    double throttlePositionStarboard = 0.0;
    double latitude = 0.0;
    double longitude = 0.0;
    double rudderPositionPort = 0.0;
    double rudderPositionStarboard = 0.0;
    double heading = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
    double heave = 0.0;
    MyVessel m_vessel;

    double calculateSpeed();
    double calculateRudderPosition();
    double calculateRateOfTurn();
};

#endif // MOVEVESSELSIMULATOR_H
