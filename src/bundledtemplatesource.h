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

#ifndef BUNDLEDTEMPLATESOURCE_H
#define BUNDLEDTEMPLATESOURCE_H
#include <QtGlobal>
#ifdef QT_DEBUG
#include <QJsonObject>
namespace BundledTemplateSource {
QString project();
QString directory(const QString &project, QString &error);
QStringList files(const QString &project, QString &error);
bool contains(const QString &project, const QString &path);
QString savePath(const QString &project, const QString &name, QString &error);
bool write(const QString &project, const QString &path, const QJsonObject &document, QString &error);
bool rename(const QString &project, const QString &path, const QString &name, QString &error);
bool remove(const QString &project, const QString &path, QString &error);
}
#endif
#endif
