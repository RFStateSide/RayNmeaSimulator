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

#include "routeimport.h"

#include <QDebug>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <cmath>
#include <functional>

namespace {
constexpr qint64 maximumBytes = 4 * 1024 * 1024;
constexpr int maximumPoints = 25000;
const QString gxNamespace = QStringLiteral("http://www.google.com/kml/ext/2.2");

// Quote bounded values, escaping newlines/control characters in diagnostic context.
QString quoted(const QString &text, int limit = 160) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{text.left(limit)}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
}
QString tag(const QDomElement &e) { return e.localName().isEmpty() ? e.tagName() : e.localName(); }
bool named(const QDomElement &e, const QString &name, const QString &ns) {
    return tag(e) == name && e.namespaceURI() == ns;
}
QDomElement child(const QDomElement &e, const QString &name) {
    for (auto n = e.firstChildElement(); !n.isNull(); n = n.nextSiblingElement())
        if (named(n, name, e.namespaceURI())) return n;
    return {};
}
QString numberError(const QString &text, const QString &field, double minimum, double maximum, double &value) {
    bool ok = false;
    value = text.toDouble(&ok);
    if (!ok || !std::isfinite(value) || value < minimum || value > maximum)
        return QString("Invalid %1 %2; valid range is %3 to %4.").arg(field, quoted(text)).arg(minimum).arg(maximum);
    return {};
}
QString coordinate(const QString &lat, const QString &lon, Waypoint &point) {
    double latitude, longitude;
    auto error = numberError(lat, "latitude", -90, 90, latitude);
    if (error.isEmpty()) error = numberError(lon, "longitude", -180, 180, longitude);
    if (error.isEmpty()) point.coordinate = QGeoCoordinate(latitude, longitude);
    return error;
}
struct Parser {
    RouteImport::Result result;
    QString ns;
    QString fail(const QDomElement &element, const QString &context, const QString &reason) {
        result.error = QString("%1 (element %2, line %3, column %4): %5")
                           .arg(context, tag(element)).arg(element.lineNumber()).arg(element.columnNumber()).arg(reason);
        return result.error;
    }
    bool append(QVector<Waypoint> &points, const Waypoint &point, const QDomElement &element, const QString &context) {
        if (points.size() >= maximumPoints) {
            fail(element, context, "Route exceeds the 25000-point limit."); return false;
        }
        points.append(point); return true;
    }
    bool kmlMetadata(const QDomElement &placemark, Waypoint &point, const QString &context) {
        const auto name = child(placemark, "name");
        if (!name.isNull()) point.name = name.text();
        const auto ext = child(placemark, "ExtendedData");
        for (auto d = ext.firstChildElement(); !d.isNull(); d = d.nextSiblingElement()) {
            if (named(d, "Data", ns) && d.attribute("name") == "speed") {
                const auto text = child(d, "value").text();
                bool ok; const double knots = text.toDouble(&ok);
                if (!ok || !std::isfinite(knots) || knots < 0) { fail(d, context, "Invalid speed " + quoted(text) + "; expected nonnegative knots."); return false; }
                point.speedMs = knots * 0.514444;
                if (!std::isfinite(point.speedMs)) { fail(d, context, "Speed exceeds the supported numeric range."); return false; }
            }
        }
        return true;
    }
    void gpx(const QDomElement &root) {
        QVector<QDomElement> routes, segments;
        QString segmentSelection;
        int trackIndex = 0;
        for (auto e = root.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
            if (named(e, "rte", ns)) routes.append(e);
            if (named(e, "trk", ns)) {
                ++trackIndex;
                int segmentIndex = 0;
                for (auto s = e.firstChildElement(); !s.isNull(); s = s.nextSiblingElement()) {
                    if (!named(s, "trkseg", ns)) continue;
                    ++segmentIndex;
                    if (child(s, "trkpt").isNull()) continue;
                    segments.append(s);
                    segmentSelection = QString("track %1, segment %2").arg(trackIndex).arg(segmentIndex);
                }
            }
        }
        if (routes.size() > 1 || (routes.isEmpty() && segments.size() > 1)) {
            result.error = "Multiple routes or track segments are unsupported; export one continuous route or segment."; return;
        }
        if (routes.isEmpty() && segments.isEmpty()) {
            result.error = "No usable route or track points were found; standalone waypoints are not an ordered route."; return;
        }
        const bool route = !routes.isEmpty();
        const auto container = route ? routes.first() : segments.first();
        result.selection = route ? "route 1" : segmentSelection;
        const auto name = child(route ? container : container.parentNode().toElement(), "name").text();
        result.name = name.trimmed();
        if (!name.isEmpty()) result.selection += " " + quoted(name);
        int index = 0;
        for (auto e = container.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
            if (!named(e, route ? "rtept" : "trkpt", ns)) continue;
            const QString context = result.selection + QString(", waypoint %1").arg(++index);
            Waypoint point;
            auto error = coordinate(e.attribute("lat"), e.attribute("lon"), point);
            if (!error.isEmpty()) { fail(e, context, error); return; }
            const auto name = child(e, "name");
            if (!name.isNull()) point.name = name.text();
            if (!append(result.waypoints, point, e, context)) return;
        }
    }
    void kml(const QDomElement &root) {
        struct Geometry { QDomElement element, placemark; int placemarkIndex; };
        QVector<Geometry> paths, points;
        int placemarkIndex = 0;
        bool unsupported = false;
        std::function<void(QDomElement,QDomElement,int)> visit = [&](QDomElement e, QDomElement placemark, int index) {
            const auto name = tag(e);
            if (named(e, "Placemark", ns)) { placemark = e; index = ++placemarkIndex; }
            if (named(e, "LineString", ns) || named(e, "Track", gxNamespace) || named(e, "Track", ns)) {
                paths.append({e, placemark, index}); return;
            }
            if (named(e, "Point", ns)) { points.append({e, placemark, index}); return; }
            if (named(e, "Polygon", ns) || named(e, "Model", ns) || named(e, "LinearRing", ns)) { unsupported = true; return; }
            const bool container = e == root || named(e,"Document",ns) || named(e,"Folder",ns)
                || named(e,"Placemark",ns) || named(e,"MultiGeometry",ns) || named(e,"MultiTrack",gxNamespace) || named(e,"MultiTrack",ns);
            if (container)
                for (auto n = e.firstChildElement(); !n.isNull(); n = n.nextSiblingElement()) visit(n, placemark, index);
        };
        visit(root, {}, 0);
        if (paths.size() > 1) {
            result.error = "Multiple KML paths are unsupported; export one LineString or gx:Track (paths are never joined)."; return;
        }
        const bool legacy = paths.isEmpty();
        if (legacy && (points.isEmpty() || unsupported)) {
            result.error = "No usable route was found; use one LineString, gx:Track, or a point-only waypoint sequence. Polygon/Model geometry is unsupported."; return;
        }
        if (legacy) {
            // Preserve original OpenCPN point-only routes, without joining separate folders.
            const auto parent = points.first().placemark.parentNode();
            for (const auto &p : points)
                if (p.placemark.isNull() || p.placemark.parentNode() != parent) {
                    result.error = "Point waypoints in separate KML containers are ambiguous; export one ordered path."; return;
                }
        }
        result.selection = legacy ? "legacy Point waypoint sequence" : tag(paths.first().element) + " 1";
        result.pointSequence = legacy;
        if (!legacy) result.name = child(paths.first().placemark, "name").text().trimmed();
        const auto selected = legacy ? points : paths;
        for (const auto &geometry : selected) {
            Waypoint base;
            const QString context = QString("Placemark %1").arg(geometry.placemarkIndex);
            if (!kmlMetadata(geometry.placemark, base, context)) return;
            QStringList tuples;
            const bool track = tag(geometry.element) == "Track";
            if (track) {
                for (auto e = geometry.element.firstChildElement(); !e.isNull(); e = e.nextSiblingElement())
                    if (named(e, "coord", geometry.element.namespaceURI())) tuples.append(e.text().trimmed());
            } else {
                tuples = child(geometry.element, "coordinates").text().trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            }
            if (tuples.isEmpty() || (legacy && tuples.size() != 1)) { fail(geometry.element, context, "Missing coordinates or invalid Point tuple count."); return; }
            int index = 0;
            for (const auto &tuple : tuples) {
                const auto parts = track ? tuple.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts) : tuple.split(',');
                const auto where = context + QString(", coordinate %1").arg(++index);
                if (parts.size() < 2 || parts.size() > 3 || (track && parts.size() != 3)) { fail(geometry.element, where, "Invalid coordinate tuple " + quoted(tuple) + "; expected " + (track ? "longitude latitude altitude." : "longitude,latitude[,altitude].")); return; }
                Waypoint point = base;
                auto error = coordinate(parts[1], parts[0], point);
                if (error.isEmpty() && parts.size() == 3) {
                    bool ok; const double altitude = parts[2].toDouble(&ok);
                    if (!ok || !std::isfinite(altitude)) error = "Invalid altitude " + quoted(parts[2]) + ".";
                    // The existing route follower is two-dimensional; altitude is validated only.
                }
                if (!error.isEmpty()) { fail(geometry.element, where, error); return; }
                if (!append(result.waypoints, point, geometry.element, where)) return;
            }
        }
        // Original OpenCPN samples contain both a path and matching waypoint markers.
        // Geometry is authoritative. Reuse names/per-waypoint speeds only for an exact
        // document-order correspondence, never by joining or reordering annotations.
        if (!legacy && points.size() == result.waypoints.size()) {
            bool matches = true;
            for (int i = 0; i < points.size(); ++i) {
                const auto tuple = child(points[i].element, "coordinates").text().trimmed().split(',');
                Waypoint point;
                if (tuple.size() < 2 || tuple.size() > 3 || !coordinate(tuple[1], tuple[0], point).isEmpty()
                    || point.coordinate.distanceTo(result.waypoints[i].coordinate) >= 0.001) { matches = false; break; }
            }
            if (matches) {
                for (int i = 0; i < points.size(); ++i)
                    if (!kmlMetadata(points[i].placemark, result.waypoints[i], QString("Placemark %1, waypoint %2").arg(points[i].placemarkIndex).arg(i+1))) return;
                result.selection += " (matching waypoint metadata)";
            }
        }
    }
};
}

RouteImport::Result RouteImport::read(const QString &path) {
    Parser parser;
    auto &r = parser.result;
    r.format = QFileInfo(path).suffix().toUpper();
    const auto finish = [&]() {
        if (!r.error.isEmpty()) {
            r.waypoints.clear();
            qWarning().noquote() << r.format << "import failed:" << quoted(path, 32768) << "\n" << r.error;
        } else {
            qDebug().noquote() << r.format << "validated:" << quoted(path, 32768) << ";" << r.selection << "; loaded" << r.waypoints.size() << "waypoints";
        }
        return r;
    };
    if (r.format != "GPX" && r.format != "KML") { r.error = "Unsupported format; select a GPX or KML file."; return finish(); }
    QFile file(path);
    if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly)) {
        r.error = "Cannot read route file: " + (QFileInfo(path).isFile() ? file.errorString() : QString("file is missing or is not a regular file.")); return finish();
    }
    const auto bytes = file.read(maximumBytes + 1);
    if (file.error() != QFileDevice::NoError) { r.error = "Route file read failed: " + file.errorString(); return finish(); }
    if (bytes.size() > maximumBytes) { r.error = "Route exceeds the 4 MiB file limit."; return finish(); }
    // Preflight before constructing a DOM: bound nesting/element counts and reject DTDs.
    // No external resource resolver is installed; route files need no entities or DTDs.
    QXmlStreamReader xml(bytes);
    int depth = 0, elements = 0;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::EntityReference) { r.error = "DTD/entity declarations are unsupported in route files."; break; }
        if (token == QXmlStreamReader::StartElement && (++depth > 64 || ++elements > 100000)) { r.error = "XML exceeds the 64-level / 100000-element limit."; break; }
        if (token == QXmlStreamReader::EndElement) --depth;
    }
    if (r.error.isEmpty() && xml.hasError()) r.error = "Invalid XML: " + xml.errorString();
    if (!r.error.isEmpty()) { r.error = QString("Line %1, column %2: %3").arg(xml.lineNumber()).arg(xml.columnNumber()).arg(r.error); return finish(); }
    QDomDocument document;
    const auto parsed = document.setContent(bytes, QDomDocument::ParseOption::UseNamespaceProcessing);
    if (!parsed) { r.error = QString("Line %1, column %2: Invalid XML: %3").arg(parsed.errorLine).arg(parsed.errorColumn).arg(parsed.errorMessage); return finish(); }
    const auto root = document.documentElement();
    parser.ns = root.namespaceURI();
    const QStringList namespaces = r.format == "GPX"
        ? QStringList{"", "http://www.topografix.com/GPX/1/0", "http://www.topografix.com/GPX/1/1"}
        : QStringList{"", "http://www.opengis.net/kml/2.2", "http://earth.google.com/kml/2.0", "http://earth.google.com/kml/2.1"};
    if (tag(root) != r.format.toLower() || !namespaces.contains(parser.ns)) {
        r.error = "Unexpected document root or namespace; expected " + r.format + "."; return finish();
    }
    if (r.format == "GPX") parser.gpx(root); else parser.kml(root);
    if (r.error.isEmpty()) {
        // Keep nonconsecutive revisits, but collapse consecutive coincident points as before.
        for (int i = r.waypoints.size() - 1; i > 0; --i)
            if (r.waypoints[i-1].coordinate.distanceTo(r.waypoints[i].coordinate) < 0.001) r.waypoints.removeAt(i);
        if (r.waypoints.size() < 2) r.error = "No usable route: at least two distinct consecutive waypoints are required.";
    }
    return finish();
}

QString RouteImport::fileFilter() {
    QStringList patterns;
    for (const auto &extension : {QString("gpx"), QString("kml")})
        for (int mask = 0; mask < 8; ++mask) {
            QString variant = extension;
            for (int i = 0; i < 3; ++i) if (mask & (1 << i)) variant[i] = variant[i].toUpper();
            patterns.append("*." + variant);
        }
    return "Route Files (" + patterns.join(' ') + ");;All Files (*)";
}
