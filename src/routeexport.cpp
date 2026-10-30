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

#include "routeexport.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QXmlStreamWriter>
#include <cmath>
#include <limits>

namespace {
constexpr qsizetype maximumBytes = 4 * 1024 * 1024;
bool xmlText(const QString &text) {
    for (qsizetype i = 0; i < text.size(); ++i) {
        const auto c = text[i].unicode();
        if (QChar::isHighSurrogate(c)) {
            if (++i >= text.size() || !text[i].isLowSurrogate()) return false;
        } else if (QChar::isLowSurrogate(c) ||
                   !(c == 9 || c == 10 || c == 13 || (c >= 32 && c <= 0xd7ff) ||
                     (c >= 0xe000 && c <= 0xfffd))) return false;
    }
    return true;
}
// max_digits10 preserves the binary value; GPX xsd:decimal forbids exponents.
QString decimal(double value) {
    if (value == 0) return "0";
    QString text = QString::number(value, 'g', std::numeric_limits<double>::max_digits10);
    const int e = text.indexOf('e', 0, Qt::CaseInsensitive);
    if (e < 0) return text;
    int exponent = text.mid(e + 1).toInt();
    text.truncate(e);
    const bool negative = text.startsWith('-');
    if (negative) text.remove(0, 1);
    int point = text.indexOf('.');
    if (point < 0) point = text.size();
    else text.remove(point, 1);
    point += exponent;
    if (point <= 0) text = "0." + QString(-point, '0') + text;
    else if (point >= text.size()) text += QString(point - text.size(), '0');
    else text.insert(point, '.');
    return negative ? "-" + text : text;
}
QString longitude(double value) { return decimal(value == 180 ? -180 : value); }
bool samePath(const QString &a, const QString &b) {
#ifdef Q_OS_WIN
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}
bool unchanged(const RouteExport::Destination &a, const RouteExport::Destination &b) {
    return b && a.path == b.path && a.parent == b.parent && a.exists == b.exists &&
           a.digest == b.digest && a.modified == b.modified && a.size == b.size;
}
}
namespace RouteExport {
QString suffix(Format format) { return format == Format::Gpx ? "gpx" : "kml"; }
QString validate(const Snapshot &s) {
    if (s.waypoints.size() < 2 || s.waypoints.size() > 25000)
        return QStringLiteral("Export needs one complete route with 2 to 25000 waypoints.");
    qsizetype textSize = s.name.size();
    if (!xmlText(s.name)) return QStringLiteral("Route name contains an invalid XML character.");
    for (qsizetype i = 0; i < s.waypoints.size(); ++i) {
        const auto &p = s.waypoints[i];
        if (!p.coordinate.isValid() || !std::isfinite(p.coordinate.latitude()) ||
            !std::isfinite(p.coordinate.longitude()))
            return QStringLiteral("Waypoint %1 has invalid latitude/longitude.").arg(i + 1);
        if (!xmlText(p.name)) return QStringLiteral("Waypoint %1 name contains an invalid XML character.").arg(i + 1);
        textSize += p.name.size();
        if (textSize > maximumBytes) return QStringLiteral("Route text exceeds the 4 MiB export limit.");
        if (i && s.waypoints[i - 1].coordinate.distanceTo(p.coordinate) < 0.001)
            return QStringLiteral("Consecutive waypoints %1 and %2 are coincident (less than 1 mm apart).").arg(i).arg(i + 1);
    }
    return {};
}
Result serialize(const Snapshot &s, Format format) {
    Result result;
    result.error = validate(s);
    if (!result) return result;
    QXmlStreamWriter xml(&result.bytes);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    if (format == Format::Gpx) {
        xml.writeStartElement("gpx");
        xml.writeDefaultNamespace("http://www.topografix.com/GPX/1/1");
        xml.writeAttribute("version", "1.1");
        xml.writeAttribute("creator", "RayNmeaSimulator");
        xml.writeStartElement("rte");xml.writeTextElement("name", s.name);
        for (const auto &p : s.waypoints) {
            xml.writeStartElement("rtept");
            xml.writeAttribute("lat", decimal(p.coordinate.latitude()));
            xml.writeAttribute("lon", longitude(p.coordinate.longitude()));
            xml.writeTextElement("name", p.name);xml.writeEndElement();
            if (result.bytes.size() > maximumBytes) break;
        }
        xml.writeEndElement();
    } else {
        xml.writeStartElement("kml");
        xml.writeDefaultNamespace("http://www.opengis.net/kml/2.2");
        xml.writeStartElement("Document");xml.writeTextElement("name", s.name);
        xml.writeStartElement("Placemark");xml.writeTextElement("name", s.name);
        xml.writeStartElement("LineString");xml.writeTextElement("altitudeMode", "clampToGround");
        xml.writeStartElement("coordinates");
        for (const auto &p : s.waypoints) {
            xml.writeCharacters(longitude(p.coordinate.longitude()) + "," + decimal(p.coordinate.latitude()) + " ");
            if (result.bytes.size() > maximumBytes) break;
        }
        xml.writeEndElement();xml.writeEndElement();xml.writeEndElement();
        for (const auto &p : s.waypoints) {
            xml.writeStartElement("Placemark");xml.writeTextElement("name", p.name);
            xml.writeStartElement("Point");
            xml.writeTextElement("coordinates", longitude(p.coordinate.longitude()) + "," + decimal(p.coordinate.latitude()));
            xml.writeEndElement();xml.writeEndElement();
            if (result.bytes.size() > maximumBytes) break;
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();xml.writeEndDocument();
    if (xml.hasError() || result.bytes.size() > maximumBytes) {
        result.error = "Cannot serialize route within the 4 MiB XML limit.";
        result.bytes.clear();
    }
    return result;
}
Destination inspectDestination(const QString &path, const QString &source) {
    Destination d;
    const QFileInfo info(path);
    d.path = info.absoluteFilePath();
    d.parent = info.absoluteDir().canonicalPath();
    if (path.trimmed().isEmpty() || path.startsWith(':') || d.parent.isEmpty() ||
        info.isSymLink() || (info.exists() && !info.isFile())) {
        d.error = "Choose a regular file in an existing directory; symbolic-link destinations are not supported.";
        return d;
    }
    if (!source.isEmpty()) {
        const auto canonicalSource = QFileInfo(source).canonicalFilePath();
        if (samePath(d.path, QFileInfo(source).absoluteFilePath()) ||
            (!canonicalSource.isEmpty() && samePath(QDir::cleanPath(QDir(d.parent).filePath(info.fileName())), canonicalSource))) {
            d.error = "Export cannot overwrite the selected source route. Choose a different file.";
            return d;
        }
    }
    d.exists = info.exists();
    if (d.exists) {
        d.size = info.size();d.modified = info.lastModified();
        QFile file(d.path);
        if (d.size > maximumBytes || !file.open(QIODevice::ReadOnly)) {
            d.error = "Cannot inspect destination safely (unreadable or larger than 4 MiB). Choose a different file.";
            return d;
        }
        const auto bytes = file.read(maximumBytes + 1);
        if (file.error() != QFileDevice::NoError || bytes.size() != d.size) d.error = "Destination changed or could not be read.";
        d.digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    }
    return d;
}
QString writeAtomic(const Destination &approved, const QByteArray &bytes, const QString &source) {
    if (!approved) return approved.error;
    if (bytes.isEmpty() || bytes.size() > maximumBytes) return "Invalid or oversized export data.";
    if (!unchanged(approved, inspectDestination(approved.path, source)))
        return "Destination changed since selection. Select and confirm the destination again.";
    QSaveFile file(approved.path);file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
        return "Export write failed: " + file.errorString();
    if (!unchanged(approved, inspectDestination(approved.path, source))) {
        file.cancelWriting();return "Destination changed before replacement; export cancelled.";
    }
    if (!file.commit()) return "Cannot atomically replace export file: " + file.errorString();
    return {};
}
}
