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

#ifndef APPPATHS_H
#define APPPATHS_H
#include <QFileInfo>
#include <QStringList>
#include <QUrl>

// Path getters are side-effect free. Resource provisioning copies only missing files.
// Legacy files stay in place; new saves use the platform application-data directory.
namespace AppPaths {
QString dataDirectory();
QString layoutsDirectory();
QString routesDirectory();
QString themesDirectory();
QString logsDirectory();
QString resourcesDirectory();
QString cacheDirectory();
QString legacyDirectory();
QStringList layoutDirectories();
QFileInfoList layoutFiles();
QString defaultLayout(QString *error = nullptr);
QString layoutSavePath(const QString &name, QString *error = nullptr);
QStringList themeDirectories();
QStringList routeDirectories();
QList<QUrl> routeSidebarUrls();
QString routeDialogDirectory(const QString &selectedFile = {});
bool ensureDirectory(const QString &directory, QString *error = nullptr);
bool copyResourceIfMissing(const QString &source, const QString &target);
void initializeRouteExamples();
}
#endif
