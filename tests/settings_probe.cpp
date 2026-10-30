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

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include "appsettings.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    AppSettings settings("RFStateSide", "RayNmeaSimulator");
#ifdef RAYNMEA_TESTING
    if (app.arguments().size() > 1) {
        settings.setValue("IsolationProbe/value", app.arguments().at(1));
        settings.sync();
        if (settings.status() != QSettings::NoError) return 2;
    }
    const QString value = settings.value("IsolationProbe/value", "missing").toString();
#else
    // Inspect only metadata: never read or write owner values in this probe.
    const QString value = "not-read";
    QSettings historical("RFStateSide", "RayNmeaSimulator");
    if (settings.fileName() != historical.fileName()
        || settings.format() != historical.format()
        || settings.fallbacksEnabled() != historical.fallbacksEnabled()) return 3;
#endif
    QTextStream(stdout) << QJsonDocument(QJsonObject{
        {"file", settings.fileName()}, {"ini", settings.format() == QSettings::IniFormat},
        {"fallbacks", settings.fallbacksEnabled()}, {"value", value}
    }).toJson(QJsonDocument::Compact) << Qt::endl;
    return 0;
}
