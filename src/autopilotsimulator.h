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

#ifndef AUTOPILOTSIMULATOR_H
#define AUTOPILOTSIMULATOR_H

#include <QElapsedTimer>
#include <QDateTime>
#include <functional>
#include "N2kMsg.h"
#include "N2kTypes.h"
#include <QGeoCoordinate>
#include <QObject>
#include <QShowEvent>
#include <QString>
#include <QTimer>
#include <QVector>
#include "convert.h"
#include "datastructs.h"
#include "environmentalmotion.h"
#include "routeimport.h"

class AutoPilotSimulator : public QObject
{
    Q_OBJECT

public:
    explicit AutoPilotSimulator(QObject *parent = nullptr);
    ~AutoPilotSimulator();

    EnvironmentalMotion environment;
    bool loadRoute(const QString &filePath, bool confirmedPointSequence = false);
    bool loadRoute(const QVector<Waypoint> &candidate);
    QString lastRouteError() const { return routeError; }
    bool isRunning() const { return running; }
    void start();
    void stop();
    double getSpeed() const;
    int traversalMultiplier() const { return multiplier; }
    bool setTraversalMultiplier(int value);
    void setReverseRoute(bool reverse);

signals:
    void coordinateUpdated(const QGeoCoordinate &coordinate);
    void LatLongChanged(double latitude, double longitude);
    void nextWaypoint(const QString &name);
    void routeCompleted();
    void routeLoaded(bool loaded);
    void headingUpdated(double headingRadians);
    void statusMessage(const QString &message);
    void distanceToNextWaypoint(double distanceMeters); // New signal for distance in meters
    void timeToNextWaypoint(double timeSeconds);
    void RunningStateChanged(bool state);
    //void distanceTraveledNM(double distanceNauticalMiles);
    //void timeTraveledSeconds(double totalTimeSeconds);
    void XTEChanged(double XTE);
    // TODO Refactor to use VesselWpBearing struct and removed redundant signals
    void VesselWpBearingChanged(VesselWpBearing vesselWpBearing);

public slots:
    void setSpeed(double speed);
    void setSpeedSource(AutopilotSpeedSource speedSource = AutopilotSpeedSource::Manual);

private:
    QVector<Waypoint> route;
    int currentIndex = 0; // origin of the active leg
    double speedMs = 0.0;
    int multiplier = 1;
    double effectiveSpeed() const;
    double realElapsedSeconds = 0;
    qint64 lastTickMs = 0;
    QTimer updateTimer;
    QElapsedTimer travelTimer;
    std::function<qint64()> elapsedNow = [this] { return travelTimer.isValid() ? travelTimer.elapsed() : 0; };
    bool reverseRoute = false;
    bool forwardDirection = true;
    bool running = false;
    bool completed = false;
    bool environmentalTrack = false;
    EnvironmentalMotion::Result environmentalResult;
    double helmHeading = 0;
    double environmentalPropulsionSpeed = 0;
    void navigationFailure(const char *reason, double seconds, const QGeoCoordinate &previous,
                           const QGeoCoordinate &candidate, double increment);
    void advanceEnvironmental(double seconds);
    QGeoCoordinate position;
    VesselWpBearing vesselWpBearing;
    AutopilotSpeedSource speedSource = AutopilotSpeedSource::Manual;
    std::function<QDateTime()> utcNow = [] { return QDateTime::currentDateTimeUtc(); };
    QString routeError;
    void calculateNextCoordinate();
    void advance(double seconds);
    void publishMetadata();
    void updateLatLong(double latitude, double longitude);
    double accumulatedDistance = 0.0;
    double totalDistanceTraveled = 0.0;
    int destinationIndex() const;
    static double crossTrack(const QGeoCoordinate &origin, const QGeoCoordinate &destination,
                             const QGeoCoordinate &position);
    static bool passedPerpendicular(const QGeoCoordinate &origin, const QGeoCoordinate &destination,
                                    const QGeoCoordinate &position);

};

#endif // AUTOPILOTSIMULATOR_H
