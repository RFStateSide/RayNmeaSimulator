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
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <algorithm>

// A receive-only, bounded cache. No generator, transmitter, file or navigation ownership.
class ReceivedRoutes
{
  public:
    struct Route
    {
        QString source, id, name, status;
        QVector<Waypoint> waypoints;
        bool complete = false;
        QMap<QString, QString> attributes;
    };
    // Promotion requires two identical geometry passes plus an identical final
    // attribute read, performed by the bounded client. Never used by passive RX.
    static Route validatedServiceSnapshot(const QString &source, const QString &name,
        const QMap<QString,QString> &attributes, const QJsonArray &points)
    {
        ReceivedRoutes staging;
        staging.observeCanIdentity(source,name);
        staging.serviceReadback=true;
        staging.receive("130066",attributes,source,1);
        QMap<QString,QString> geometry{{"DatabaseID",attributes.value("DatabaseID")},{"RouteID",attributes.value("RouteID")},
            {"GeometryPGN","130067"},{"StartRPS","0"},{"TotalWaypoints",QString::number(points.size())},
            {"Waypoints",QString::fromUtf8(QJsonDocument(points).toJson(QJsonDocument::Compact))}};
        staging.receive("130067",geometry,source,1);
        const auto routes=staging.collection(1);
        Route result=routes.isEmpty()?Route{}:routes.first();result.attributes=attributes;result.attributes["SourceNAME"]=name;
        QJsonArray ids;for(const auto &point:points)ids.append(point.toObject().value("ID"));
        result.attributes["WaypointIDs"]=QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact));
        result.attributes["ServiceReadback"]="Repeated geometry and attributes matched";
        if(attributes.value("Datestamp").toUInt()>=65533 || attributes.value("Timestamp").toULongLong()>864010000ULL)result.attributes["VersionStatus"]="Unavailable; bounded repeated readback";
        return result;
    }
    void observeCanIdentity(const QString &source, const QString &name)
    {
        if (name.size() != 16 || (canNames.size() >= 254 && !canNames.contains(source)))
            return;
        if (canNames.value(source) == name)
            return;
        for (auto it = nativeRoutes.begin(); it != nativeRoutes.end();)
            if (it->source == source)
                it = nativeRoutes.erase(it);
            else
                ++it;
        canNames[source] = name;
    }
    void receive(const QString &code, const QMap<QString, QString> &fields, const QString &source,
                 qint64 now = QDateTime::currentMSecsSinceEpoch())
    {
        expire(now);
        if (source.isEmpty() || source.size() > 128)
            return;
        if (code == "129285" || code == "130066" || code == "130067")
        {
            const auto database = fields.value("DatabaseID"), routeID = fields.value("RouteID");
            const QString key = source + QChar(0x1f) + database + QChar(0x1f) + routeID;
            if ((nativeRoutes.size() + routes.size() >= 32) && !nativeRoutes.contains(key))
                return;
            auto &entry = nativeRoutes[key];
            entry.source = source;
            if (code == "130066")
            {
                const auto day = fields.value("Datestamp").toULongLong(),
                           ticks = fields.value("Timestamp").toULongLong();
                const bool known = day < 65533 && ticks <= 864010000ULL;
                const quint64 version = known ? ((day << 32) | ticks) : 0;
                if (known && entry.versionKnown && version < entry.version)
                    return;
                if (known && (!entry.versionKnown || version > entry.version))
                    entry.conflicted = false;
                if (known && entry.versionKnown && version == entry.version && entry.attributes != fields)
                    entry.conflicted = true;
                if (!entry.attributes.isEmpty() && entry.attributes != fields)
                    entry.positions.clear();
                entry.updated = now;
                entry.attributesReceived = now;
                entry.attributes = fields;
                entry.version = version;
                entry.versionKnown = known;
            }
            else
            {
                const int start = fields.value("StartRPS").toInt();
                const auto array = QJsonDocument::fromJson(fields.value("Waypoints").toUtf8()).array();
                if (start < 0 || start + array.size() > 128)
                    return;
                if (!entry.routeFields.isEmpty() &&
                    (entry.routeFields.value("RouteName") != fields.value("RouteName") ||
                     entry.routeFields.value("NavigationDirection") != fields.value("NavigationDirection") ||
                     entry.routeFields.value("SupplementaryData") != fields.value("SupplementaryData")))
                    entry.positions.clear();
                entry.updated = now;
                entry.routeFields = fields;
                for (int i = 0; i < array.size(); ++i)
                {
                    const auto point = array[i].toObject();
                    const QString serialized = QString::fromUtf8(QJsonDocument(point).toJson(QJsonDocument::Compact));
                    if (entry.positions.contains(start + i) && entry.positions[start + i] != serialized)
                    {
                        entry.conflicted = true;
                        entry.positions.clear();
                    }
                    entry.positions[start + i] = serialized;
                    entry.positionTimes[start + i] = now;
                }
            }
            return;
        }
        // A database waypoint list is not an ordered route and cannot authorize navigation.
        if (code == "130074")
            return;
        if (code == "WPL")
        {
            const auto name = fields.value("WaypointName");
            if (!points.contains(source) && points.size() >= 32)
                return;
            bool latOk = false, lonOk = false;
            fields.value("Latitude").toDouble(&latOk);
            fields.value("Longitude").toDouble(&lonOk);
            if (!latOk || !lonOk)
                return;
            QGeoCoordinate coordinate(fields.value("Latitude").toDouble(), fields.value("Longitude").toDouble());
            if (name.isEmpty() || name.size() > 16 || !coordinate.isValid())
                return;
            auto &catalog = points[source];
            if (catalog.size() >= 512 && !catalog.contains(name))
                return;
            const bool changed = catalog.contains(name) && catalog[name].point.coordinate != coordinate;
            catalog[name] = {Waypoint{coordinate, name, 6.7032053}, now};
            // Unversioned WPL changes cannot silently revise a complete cached route.
            if (changed)
                for (auto &route : routes)
                    if (route.source == source)
                    {
                        route.pages.clear();
                        route.updated = now;
                    }
            return;
        }
        if (code != "RTE")
            return;
        const QString name = fields.value("RouteName"), key = source + QChar(0x1f) + name;
        const int total = fields.value("TotalMessages").toInt(), page = fields.value("MessageNumber").toInt();
        if (total < 1 || total > 16 || page < 1 || page > total || fields.value("RouteType") != "c")
            return;
        QStringList ids;
        const auto document = QJsonDocument::fromJson(fields.value("WaypointIDs").toUtf8());
        if (!document.isArray() || name.isEmpty() || name.size() > 16)
            return;
        const auto array = document.array();
        for (const auto id : array)
        {
            if (!id.isString() || id.toString().isEmpty() || id.toString().size() > 16)
                return;
            ids.append(id.toString());
        }
        if (ids.size() > 12 || (routes.size() + nativeRoutes.size() >= 32 && !routes.contains(key)))
            return;
        auto &route = routes[key];
        if (route.total != total || (route.pages.contains(page) && route.pages[page] != ids))
            route.pages.clear();
        route.source = source;
        route.name = name;
        route.total = total;
        route.updated = now;
        route.pages[page] = ids;
        route.pageTimes[page] = now;
    }
    QVector<Route> collection(qint64 now = QDateTime::currentMSecsSinceEpoch()) const
    {
        QVector<Route> result;
        for (auto it = nativeRoutes.begin(); it != nativeRoutes.end(); ++it)
        {
            const auto &entry = it.value();
            Route route;
            route.source = entry.source;
            route.id = it.key() + QChar(0x1f) + canNames.value(entry.source);
            route.attributes = entry.attributes;
            route.attributes["SourceNAME"] = canNames.value(entry.source);
            route.name = entry.attributes.value("RouteName", entry.routeFields.value("RouteName"));
            if (route.name.isEmpty())
                route.name = "Route " + entry.attributes.value("RouteID");
            route.status = "Incomplete route or unavailable required metadata/geometry";
            const int count = entry.attributes.value("WaypointCount").toInt();
            bool valid =
                !entry.conflicted && !entry.attributes.isEmpty() && !entry.routeFields.isEmpty() &&
                now >= entry.updated && now - entry.updated <= 30000 && now >= entry.attributesReceived &&
                now - entry.attributesReceived <= 30000 && entry.attributes.value("DatabaseID").toUInt() <= 65532 &&
                entry.attributes.value("RouteID").toUInt() <= 65532 && count >= 2 && count <= 128 &&
                entry.positions.size() == count && entry.attributes.value("CriticalParameters") == "0" &&
                entry.attributes.value("NavigationMethod") == "0" &&
                (entry.attributes.value("IdentificationMethod") == "1" ||
                 (serviceReadback && entry.routeFields.value("GeometryPGN") == "130067" && entry.attributes.value("IdentificationMethod") == "0")) &&
                (entry.attributes.value("RouteStatus") == "0" || entry.attributes.value("RouteStatus") == "1") &&
                (entry.routeFields.value("GeometryPGN") == "130067" ||
                 entry.routeFields.value("SupplementaryData") == "0") &&
                (entry.routeFields.value("GeometryPGN") == "130067" ||
                 entry.routeFields.value("NavigationDirection") == "0" ||
                 entry.routeFields.value("NavigationDirection") == "1") &&
                ((entry.attributes.value("Datestamp").toUInt() < 65533 ||
                  (serviceReadback && entry.attributes.value("Datestamp").toUInt() == 65535)) &&
                 (entry.attributes.value("Timestamp").toULongLong() <= 864010000ULL ||
                  (serviceReadback && entry.attributes.value("Timestamp").toULongLong() == 0xffffffffULL)));
            if (entry.routeFields.value("GeometryPGN") == "130067" &&
                entry.routeFields.value("TotalWaypoints").toInt() != count)
                valid = false;
            if (!entry.attributes.value("RouteName").isEmpty() && !entry.routeFields.value("RouteName").isEmpty() &&
                entry.attributes.value("RouteName") != entry.routeFields.value("RouteName"))
                valid = false;
            QMap<int, Waypoint> identifiers;
            for (int index = 0; valid && index < count; ++index)
            {
                if (!entry.positions.contains(index) || now - entry.positionTimes.value(index) > 30000 ||
                    now < entry.positionTimes.value(index))
                {
                    valid = false;
                    break;
                }
                const auto point = QJsonDocument::fromJson(entry.positions[index].toUtf8()).object();
                const int id = point["ID"].toInt();
                Waypoint waypoint{QGeoCoordinate(point["Latitude"].toDouble(), point["Longitude"].toDouble()),
                                  point["Name"].toString(), 6.7032053};
                if (id > 65532 || !waypoint.coordinate.isValid() ||
                    (identifiers.contains(id) && (identifiers[id].coordinate != waypoint.coordinate ||
                                                  (!identifiers[id].name.isEmpty() && !waypoint.name.isEmpty() &&
                                                   identifiers[id].name != waypoint.name))) ||
                    (!route.waypoints.isEmpty() &&
                     route.waypoints.last().coordinate.distanceTo(waypoint.coordinate) < 0.001))
                {
                    valid = false;
                    break;
                }
                identifiers[id] = waypoint;
                route.waypoints.append(waypoint);
            }
            if (valid && entry.routeFields.value("NavigationDirection") == "1")
                std::reverse(route.waypoints.begin(), route.waypoints.end());
            route.complete = valid;
            if (valid)
                route.status = "Complete great-circle route, no required supplementary geometry";
            else
                route.waypoints.clear();
            result.append(route);
        }
        for (auto it = routes.begin(); it != routes.end(); ++it)
        {
            const auto &entry = it.value();
            Route route;
            route.source = entry.source;
            route.id = it.key();
            route.name = entry.name;
            route.status = "Incomplete or expired route";
            if (now - entry.updated > 30000 || now < entry.updated || entry.pages.size() != entry.total)
            {
                result.append(route);
                continue;
            }
            bool valid = true;
            QStringList identifiers;
            for (int page = 1; page <= entry.total; ++page)
            {
                if (!entry.pages.contains(page) || now - entry.pageTimes.value(page) > 30000 ||
                    now < entry.pageTimes.value(page))
                {
                    valid = false;
                    break;
                }
                identifiers += entry.pages[page];
            }
            if (identifiers.size() < 2 || identifiers.size() > 128)
                valid = false;
            for (const auto &id : identifiers)
            {
                const auto bySource = points.constFind(entry.source);
                if (bySource == points.end() || !bySource->contains(id))
                {
                    valid = false;
                    break;
                }
                const auto &point = bySource->value(id);
                if (now - point.updated > 30000 || now < point.updated)
                {
                    valid = false;
                    break;
                }
                if (!route.waypoints.isEmpty() &&
                    route.waypoints.last().coordinate.distanceTo(point.point.coordinate) < 0.001)
                {
                    valid = false;
                    break;
                }
                route.waypoints.append(point.point);
            }
            route.complete = valid;
            route.status = valid ? "Complete legacy RTE/WPL route" : "Missing/invalid waypoint data";
            if (!valid)
                route.waypoints.clear();
            result.append(route);
        }
        return result;
    }

  private:
    bool serviceReadback=false;
    struct Point
    {
        Waypoint point;
        qint64 updated = 0;
    };
    struct Entry
    {
        QString source, name;
        int total = 0;
        qint64 updated = 0;
        QMap<int, QStringList> pages;
        QMap<int, qint64> pageTimes;
    };
    QMap<QString, QMap<QString, Point>> points;
    QMap<QString, Entry> routes;
    struct NativeEntry
    {
        QString source;
        qint64 updated = 0;
        quint64 version = 0;
        bool versionKnown = false;
        bool conflicted = false;
        qint64 attributesReceived = 0;
        QMap<int, qint64> positionTimes;
        QMap<QString, QString> attributes, routeFields;
        QMap<int, QString> positions;
    };
    QMap<QString, NativeEntry> nativeRoutes;
    QMap<QString, QString> canNames;
    void expire(qint64 now)
    {
        for (auto it = nativeRoutes.begin(); it != nativeRoutes.end();)
            if (now - it->updated > 30000 || now < it->updated)
                it = nativeRoutes.erase(it);
            else
                ++it;
        for (auto it = routes.begin(); it != routes.end();)
            if (now - it->updated > 30000 || now < it->updated)
                it = routes.erase(it);
            else
                ++it;
        for (auto source = points.begin(); source != points.end();)
        {
            for (auto it = source->begin(); it != source->end();)
                if (now - it->updated > 30000 || now < it->updated)
                    it = source->erase(it);
                else
                    ++it;
            if (source->isEmpty())
                source = points.erase(source);
            else
                ++source;
        }
        // Bound source identities too; reject further sources rather than evict accepted data.
    }
};
