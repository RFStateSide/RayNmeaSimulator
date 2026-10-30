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

#ifndef WEATHEROBSERVATION_H
#define WEATHEROBSERVATION_H
#include <QDateTime>
#include <QStringList>
#include <QMetaType>
#include <cmath>
#include "N2kMsg.h"
// Provenance contains metadata only; live values remain in the Weather widget/model.
struct WeatherFieldOrigin {
    QString provider, station, location;
    QDateTime observed, retrieved;
    double latitude = N2kDoubleNA, longitude = N2kDoubleNA;
    int maximumAgeSeconds = 7200;
    bool retained = false, stale = false;
    bool fresh(const QDateTime &now = QDateTime::currentDateTimeUtc()) const {
        return !stale && observed.isValid() && observed.msecsTo(now) >= -600000 &&
               observed.msecsTo(now) <= qint64(maximumAgeSeconds)*1000;
    }
};
// Canonical observations; unavailable is the existing protocol sentinel, never zero.
struct WeatherObservation {
    QString provider, location, station, description;
    QDateTime observed, retrieved;
    double latitude = N2kDoubleNA, longitude = N2kDoubleNA;
    double air = N2kDoubleNA, sea = N2kDoubleNA, pressure = N2kDoubleNA;
    double humidity = N2kDoubleNA, windSpeed = N2kDoubleNA, windAngle = N2kDoubleNA, waveHeight = N2kDoubleNA;
    double dominantWavePeriod = N2kDoubleNA, averageWavePeriod = N2kDoubleNA, meanWaveDirection = N2kDoubleNA;
    double windGust = N2kDoubleNA, dewPoint = N2kDoubleNA, visibility = N2kDoubleNA;
    double pressureTendency = N2kDoubleNA, tide = N2kDoubleNA;
    QStringList missing, invalid;
    int maximumAgeSeconds = 7200;
    bool fresh(const QDateTime &now = QDateTime::currentDateTimeUtc()) const {
        return observed.isValid() && observed.secsTo(now) >= -600 &&
               observed.secsTo(now) <= maximumAgeSeconds;
    }
    static bool valid(double v) { return std::isfinite(v) && v != N2kDoubleNA; }
    bool any() const {
        for (double v : {air, sea, pressure, humidity, windSpeed, windAngle, waveHeight,
                         dominantWavePeriod, averageWavePeriod, meanWaveDirection, windGust,
                         dewPoint, visibility, pressureTendency, tide})
            if (valid(v))
                return true;
        return false;
    }
};
Q_DECLARE_METATYPE(WeatherObservation)
#endif
