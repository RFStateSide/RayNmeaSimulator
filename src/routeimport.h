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

#ifndef ROUTEIMPORT_H
#define ROUTEIMPORT_H

#include <QGeoCoordinate>
#include <QString>
#include <QVector>

struct Waypoint
{
    QGeoCoordinate coordinate;
    QString name = "un-named";
    double speedMs = 6.7032053; // Preserve the established route-file default.
};

namespace RouteImport {
struct Result {
    QVector<Waypoint> waypoints;
    QString format;
    QString selection;
    QString name; // Optional display metadata; never used for navigation or serialization.
    QString error;
    bool pointSequence = false; // Legacy compatibility: requires explicit route-intent confirmation.
    explicit operator bool() const { return error.isEmpty(); }
};
// Validates an isolated candidate; never changes simulator or application state.
Result read(const QString &path);
QString fileFilter();
}
#endif
