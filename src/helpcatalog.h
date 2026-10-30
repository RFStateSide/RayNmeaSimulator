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
#include <QList>
#include <QStringList>
#include <QUrl>

// RF StateSide internal adaptation of BrickSuite HelpManager's local topic catalog.
struct HelpTopicInfo {
    QString id, title, group, resource;
};
namespace HelpCatalog {
const QList<HelpTopicInfo> &topics();
const HelpTopicInfo *topic(const QString &id);
QString article(const QString &id);
QStringList search(const QString &query);
QUrl topicUrl(const QString &id);
QString topicForUrl(const QUrl &url);
bool isImage(const QUrl &url);
QUrl resolve(const QUrl &url, const QUrl &base = QUrl("qrc:/help/introduction.html"));
}
