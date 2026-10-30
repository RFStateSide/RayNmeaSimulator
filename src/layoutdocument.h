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

#ifndef LAYOUTDOCUMENT_H
#define LAYOUTDOCUMENT_H
#include <QJsonObject>
#include <QSettings>

// Document policies are resolved once. They never become live model bindings.
namespace LayoutDocument {
QString mapping(const QString &pgn, const QString &field);
bool read(const QString &path, QJsonObject &root, QString &error);
bool write(const QString &path, const QJsonObject &root, QString &error);
bool resolve(const QJsonObject &source, QSettings &settings, QJsonObject &layout, QString &error);
QString directory();
QString savePath(const QString &name, QString &error);
QString savePathInDirectory(const QString &name, const QString &targetDirectory, QString &error);
QStringList templates();
QString summary(const QJsonObject &root);
}
#endif
