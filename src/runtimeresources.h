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

#ifndef RUNTIMERESOURCES_H
#define RUNTIMERESOURCES_H
#include <QCoreApplication>
#include <QFile>
#include "themes.h"
#include "apppaths.h"
namespace RuntimeResources {
// Optional application-local overrides; compiled defaults work in any shadow build.
inline QString path(const QString &relative) {
    const QString user = AppPaths::resourcesDirectory();
    if (!user.isEmpty() && QFile::exists(user + "/" + relative)) return user + "/" + relative;
    const QString external = QCoreApplication::applicationDirPath() + "/" + relative;
    return QFile::exists(external) ? external : ":/" + relative;
}
inline QString engineProfiles() {
#ifdef RAYNMEA_TESTING
    if (qEnvironmentVariableIsSet("RAYNMEA_TEST_ENGINE_PROFILES"))
        return qEnvironmentVariable("RAYNMEA_TEST_ENGINE_PROFILES");
#endif
    const QString relative = "Data/engine_profiles.json";
    const QString local = AppPaths::resourcesDirectory() + "/" + relative;
    if (QFileInfo::exists(local) || QFileInfo(local).isSymLink()) return local;
    // Do not seed a bundled copy ahead of an existing external override, even
    // when that override is invalid: leave it in place and diagnose in the loader.
    for (const auto &legacy : {QCoreApplication::applicationDirPath() + "/" + relative,
             AppPaths::legacyDirectory() + "/resources/" + relative,
             AppPaths::legacyDirectory() + "/" + relative})
        if (QFileInfo::exists(legacy) || QFileInfo(legacy).isSymLink()) return legacy;
    return AppPaths::copyResourceIfMissing(":/" + relative, local) ? local : ":/" + relative;
}
inline QString stylePath(const QString &theme) { return Themes::source(theme); }
inline QString styleSheet(const QString &theme) { return Themes::styleSheet(theme); }
}
#endif
