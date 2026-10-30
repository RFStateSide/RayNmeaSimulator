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

#pragma once
#include "routeimport.h"
#include <QByteArray>
#include <QDateTime>

namespace RouteExport {
enum class Format { Gpx, Kml };
// A value copy of the operator's selection, never the active pilot's progression.
struct Snapshot {
    QString name;
    QVector<Waypoint> waypoints;
    QString sourceFile; // Protection only; never serialized.
    QString sourceDescription;
    bool pointSequence = false; // Imported map pins need explicit route-intent consent.
};
struct Result {
    QByteArray bytes;
    QString error;
    explicit operator bool() const { return error.isEmpty(); }
};
struct Destination {
    QString path, parent, error;
    QByteArray digest;
    QDateTime modified;
    qint64 size = 0;
    bool exists = false;
    explicit operator bool() const { return error.isEmpty(); }
};
QString validate(const Snapshot &snapshot);
Result serialize(const Snapshot &snapshot, Format format);
// Observe BEFORE overwrite confirmation, then compare again before replacement.
Destination inspectDestination(const QString &path, const QString &sourceFile = {});
QString writeAtomic(const Destination &approved, const QByteArray &bytes,
                    const QString &sourceFile = {});
QString suffix(Format format);
}
