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

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryFile>
#include <QCoreApplication>
#include <cstdlib>

namespace TestProfile {
// Shared by settings and layout paths. An invalid test profile must never select
// the production home or native settings store.
inline QString root()
{
    const QString path = QDir::cleanPath(qEnvironmentVariable("RAYNMEA_TEST_HOME"));
    auto fail = [] { qCritical("Invalid or unavailable RAYNMEA_TEST_HOME"); std::exit(EXIT_FAILURE); };
    if (!QDir::isAbsolutePath(path) || path == QDir::rootPath()
        || QFileInfo(path).absoluteFilePath() == QFileInfo(QDir::homePath()).absoluteFilePath()) fail();
    for (QString part = path;; part = QFileInfo(part).absolutePath()) {
        if (QFileInfo(part).isSymLink()) fail();
        const QString parent = QFileInfo(part).absolutePath();
        if (parent == part) break;
    }
    if (!QDir().mkpath(path) || !QFileInfo(path).isDir()) fail();
    QTemporaryFile probe(QDir(path).filePath(".write-check-XXXXXX"));
    if (!probe.open()) fail();
    return path;
}

inline QString settingsFile()
{
    const QString directory = root() + "/settings/RFStateSide";
    if (QFileInfo(QFileInfo(directory).absolutePath()).isSymLink()
        || QFileInfo(directory).isSymLink() || !QDir().mkpath(directory)) {
        qCritical("Unavailable isolated settings directory"); std::exit(EXIT_FAILURE);
    }
    const QString file = directory + "/RayNmeaSimulator.ini";
    if (QFileInfo(file).isSymLink() || (QFileInfo::exists(file) && !QFileInfo(file).isFile())) {
        qCritical("Invalid isolated settings file"); std::exit(EXIT_FAILURE);
    }
    QFile probe(file);
    if (!probe.open(QIODevice::ReadWrite)) {
        qCritical("Unavailable isolated settings file"); std::exit(EXIT_FAILURE);
    }
    return file;
}
}

namespace DevelopmentProfile {
inline bool active() { return QCoreApplication::arguments().contains("--isolated-profile"); }
inline void initialize()
{
    if (!active()) return;
#if !defined(RAYNMEA_TESTING)
    const auto arguments = QCoreApplication::arguments();
    const auto index = arguments.indexOf("--isolated-profile");
    if (!arguments.contains("--no-nmea") || index + 1 >= arguments.size()) {
        qCritical("--isolated-profile requires an absolute disposable directory and --no-nmea");
        std::exit(EXIT_FAILURE);
    }
    qputenv("RAYNMEA_TEST_HOME", arguments.at(index + 1).toUtf8());
    const auto root = TestProfile::root();
    TestProfile::settingsFile(); // Reject redirected settings subdirectories/files too.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root + "/settings");
#else
    qCritical("--isolated-profile is reserved for production qualification runs");
    std::exit(EXIT_FAILURE);
#endif
}
}

class AppSettings final : public QSettings
{
public:
    AppSettings(const QString &organization, const QString &application)
#ifdef RAYNMEA_TESTING
        : QSettings(TestProfile::settingsFile(), QSettings::IniFormat)
    {
        Q_UNUSED(organization);
        Q_UNUSED(application);
        setFallbacksEnabled(false);
    }
#else
        : QSettings(DevelopmentProfile::active() ? QSettings::IniFormat : QSettings::NativeFormat,
                    QSettings::UserScope, organization, application)
    {
        if (DevelopmentProfile::active()) {
            setFallbacksEnabled(false);
            if (format() != QSettings::IniFormat
                || QDir::cleanPath(fileName()) != TestProfile::root() + "/settings/" + organization + "/" + application + ".ini")
                qFatal("Development settings did not resolve inside the disposable profile");
        }
    }
#endif
};

#endif
