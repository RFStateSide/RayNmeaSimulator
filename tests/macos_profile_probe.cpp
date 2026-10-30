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

#include "appsettings.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    DevelopmentProfile::initialize();
    if (!DevelopmentProfile::active()) return 2; // Never inspect the normal store.
    AppSettings settings("RFStateSide", "RayNmeaSimulator");
    if (app.arguments().contains("--write")) {
        settings.setValue("qualification", "private");
        settings.sync();
    }
    const QJsonObject record{{"file", settings.fileName()},
                             {"ini", settings.format() == QSettings::IniFormat},
                             {"fallbacks", settings.fallbacksEnabled()},
                             {"value", settings.value("qualification", "missing").toString()}};
    std::puts(QJsonDocument(record).toJson(QJsonDocument::Compact).constData());
    return settings.status() == QSettings::NoError ? 0 : 1;
}
