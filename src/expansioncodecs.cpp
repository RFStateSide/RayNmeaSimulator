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

#include "expansioninstruments.h"
#include "navigationstatistics.h"
#include "N2kMessages.h"
#include "NMEA0183Messages.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
namespace Expansion
{
const QList<Definition> &definitions()
{
    static const QList<Definition> rows = [] {
    QList<Definition> rows = {
        {"129285",
         "RouteInfo129285",
         "Navigation Route/WP Information",
         "Navigation",
         {{"StartRPS", "", "uint16_t", "0", 0, 65535},
          {"DatabaseID", "", "uint16_t", "0", 0, 65535},
          {"RouteID", "", "uint16_t", "0", 0, 65535},
          {"NavigationDirection", "", "tN2kNavigationDirection", "0", 0, 7},
          {"SupplementaryData", "", "tN2kGenericStatusPair", "0", 0, 3},
          {"RouteName", "", "QString", "route", 0, 0},
          {"Waypoints", "", "QString", "[]", 0, 0}},
         {"DatabaseID", "RouteID", "StartRPS"},
         129285},
        {"130074",
         "WaypointList130074",
         "Waypoint List, Name & Position",
         "Navigation",
         {{"StartWaypointID", "", "uint16_t", "0", 0, 65535},
          {"TotalWaypoints", "", "uint16_t", "0", 0, 65535},
          {"DatabaseID", "", "uint16_t", "0", 0, 65535},
          {"Waypoints", "", "QString", "[]", 0, 0}},
         {"DatabaseID", "StartWaypointID"},
         130074},
        {"APB",
         "AutopilotAPB",
         "Autopilot Sentence B",
         "Navigation",
         {{"Status", "", "char (A/V)", "V", 0, 0},
          {"CycleLockStatus", "", "char (A/V)", "V", 0, 0},
          {"XTE", "m", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 185200},
          {"SteerDirection", "", "Steer direction", "L", 0, 0},
          {"ArrivalAlarm", "", "Arrival indication", "V", 0, 0},
          {"PerpendicularPassed", "", "Perpendicular indication", "V", 0, 0},
          {"OriginBearing", "rad", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 6.283185307179586},
          {"OriginReference", "", "Bearing reference", "T", 0, 0},
          {"DestinationID", "", "QString", "waypoint", 0, 0},
          {"PositionBearing", "rad", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 6.283185307179586},
          {"PositionReference", "", "Bearing reference", "T", 0, 0},
          {"HeadingToSteer", "rad", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 6.283185307179586},
          {"HeadingReference", "", "Bearing reference", "T", 0, 0}},
         {},
         SNT_APB},
        {"RMB",
         "NavigationRMB",
         "Recommended Minimum Navigation",
         "Navigation",
         {{"Status", "", "char (A/V)", "V", 0, 0},
          {"XTE", "m", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 185200},
          {"SteerDirection", "", "Steer direction", "L", 0, 0},
          {"OriginID", "", "QString", "waypoint", 0, 0},
          {"DestinationID", "", "QString", "waypoint", 0, 0},
          {"Latitude", "deg", "double", QString::number(N2kDoubleNA, 'g', 17), -90, 90},
          {"Longitude", "deg", "double", QString::number(N2kDoubleNA, 'g', 17), -180, 180},
          {"DistanceToWaypoint", "m", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 185200000},
          {"BearingToWaypoint", "rad", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 6.283185307179586},
          {"ClosingVelocity", "m/s", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 1000},
          {"ArrivalAlarm", "", "Arrival indication", "V", 0, 0}},
         {},
         SNT_RMB},

        {"RTE",
         "RouteRTE",
         "Route (page)",
         "Navigation",
         {{"TotalMessages", "", "unsigned char", "1", 1, 16},
          {"MessageNumber", "", "unsigned char", "1", 1, 16},
          {"RouteType", "", "RTE route type", "c", 0, 0},
          {"RouteName", "", "QString", "route", 0, 0},
          {"WaypointIDs", "", "QString", "[]", 0, 0}},
         {"RouteName", "MessageNumber"},
         100030},
        {"WPL",
         "WaypointWPL",
         "Waypoint Location",
         "Navigation",
         {{"Latitude", "deg", "double", QString::number(N2kDoubleNA, 'g', 17), -90, 90},
          {"Longitude", "deg", "double", QString::number(N2kDoubleNA, 'g', 17), -180, 180},
          {"WaypointName", "", "QString", "waypoint", 0, 0}},
         {"WaypointName"},
         SNT_WPL},
        {"GSA",
         "GnssActiveGSA",
         "GNSS DOP and Active Satellites",
         "Navigation",
         {{"SelectionMode", "", "GSA selection", "A", 0, 0},
          {"FixType", "", "GSA fix", "1", 1, 3},
          {"Satellite1", "", "unsigned char", "255", 0, 255},
          {"Satellite2", "", "unsigned char", "255", 0, 255},
          {"Satellite3", "", "unsigned char", "255", 0, 255},
          {"Satellite4", "", "unsigned char", "255", 0, 255},
          {"Satellite5", "", "unsigned char", "255", 0, 255},
          {"Satellite6", "", "unsigned char", "255", 0, 255},
          {"Satellite7", "", "unsigned char", "255", 0, 255},
          {"Satellite8", "", "unsigned char", "255", 0, 255},
          {"Satellite9", "", "unsigned char", "255", 0, 255},
          {"Satellite10", "", "unsigned char", "255", 0, 255},
          {"Satellite11", "", "unsigned char", "255", 0, 255},
          {"Satellite12", "", "unsigned char", "255", 0, 255},
          {"PDOP", "", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 99.9},
          {"HDOP", "", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 99.9},
          {"VDOP", "", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 99.9}},
         {},
         100029},
        {"127498",
         "EngineStatic127498",
         "Engine Parameters, Static",
         "Propulsion",
         {{"EngineInstance", "", "unsigned char", "0", 0, 255},
          {"RatedEngineSpeed", "rpm", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 16383},
          {"VIN", "", "QString", "", 0, 0},
          {"SoftwareID", "", "QString", "", 0, 0}},
         {"EngineInstance"},
         127498},
        {"127509",
         "InverterStatus127509",
         "Inverter Status",
         "Electrical",
         {{"Instance", "", "unsigned char", "0", 0, 255},
          {"ACInstance", "", "unsigned char", "0", 0, 255},
          {"DCInstance", "", "unsigned char", "0", 0, 255},
          {"OperatingState", "", "InverterState (4 bit)", "15", 0, 15},
          {"InverterEnable", "", "OffOn (2 bit)", "3", 0, 3}},
         {"Instance", "ACInstance", "DCInstance"},
         127509},
        {"127496",
         "VesselTrip127496",
         "Vessel Trip Parameters",
         "Navigation",
         {{"TimeToEmpty", "s", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 4294967.292},
          {"DistanceToEmpty", "m", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 42949672.92},
          {"EstimatedFuelRemaining", "L", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 65532},
          {"TripRunTime", "s", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 4294967.292}},
         {},
         127496},
        {"130311",
         "EnvironmentalLegacy130311",
         "Legacy Environmental Parameters",
         "Environment",
         {{"SID", "", "unsigned char", "0", 0, 255},
          {"TemperatureSource", "", "tN2kTempSource (6 bit)", "1", 0, 63},
          {"Temperature", "K", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 655.32},
          {"HumiditySource", "", "tN2kHumiditySource (2 bit)", "1", 0, 3},
          {"Humidity", "%", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 100},
          {"AtmosphericPressure", "Pa", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 6553200}},
         {"TemperatureSource", "HumiditySource"},
         130311},
        {"129539",
         "GnssDops129539",
         "GNSS DOPs",
         "Navigation",
         {{"SID", "", "unsigned char", "0", 0, 255},
          {"DesiredMode", "", "tN2kGNSSDOPmode", "0", 0, 7},
          {"ActualMode", "", "tN2kGNSSDOPmode", "0", 0, 7},
          {"HDOP", "", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 327.65},
          {"VDOP", "", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 327.65},
          {"TDOP", "", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 327.65}},
         {},
         129539},
        {"129540",
         "GnssSatellites129540",
         "GNSS Satellites in View",
         "Navigation",
         {{"SID", "", "unsigned char", "0", 0, 255},
          {"Mode", "", "tN2kRangeResidualMode", "0", 0, 3},
          {"Satellites", "", "QString", "[]", 0, 0}},
         {},
         129540},
        {"128275",
         "DistanceLog128275",
         "Distance Log",
         "Navigation",
         {{"DaysSince1970", "", "uint16_t", "65535", 0, 65535},
          {"SecondsSinceMidnight", "s", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 86399.9999},
          {"Log", "m", "uint32_t", "4294967295", 0, 4294967295},
          {"TripLog", "m", "uint32_t", "4294967295", 0, 4294967295}},
         {},
         128275},
        {"130314",
         "Pressure130314",
         "Actual Pressure",
         "Environment",
         {{"SID", "", "unsigned char", "0", 0, 255},
          {"PressureInstance", "", "unsigned char", "0", 0, 255},
          {"PressureSource", "", "tN2kPressureSource", "0", 0, 255},
          {"ActualPressure", "Pa", "double", QString::number(N2kDoubleNA, 'g', 17), -214748364.8, 214748364.5}},
         {"PressureInstance", "PressureSource"},
         130314},
        {"127497",
         "EngineTrip127497",
         "Engine Trip Parameters",
         "Propulsion",
         {{"EngineInstance", "", "unsigned char", "0", 0, 255},
          {"TripFuelUsed", "L", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 65532},
          {"FuelRateAverage", "L/h", "double", QString::number(N2kDoubleNA, 'g', 17), -3276.8, 3276.5},
          {"FuelRateEconomy", "L/h", "double", QString::number(N2kDoubleNA, 'g', 17), -3276.8, 3276.5},
          {"InstantaneousFuelEconomy", "L/h", "double", QString::number(N2kDoubleNA, 'g', 17), -3276.8, 3276.5}},
         {"EngineInstance"},
         127497},
        {"DPT",
         "DepthDPT",
         "Depth",
         "Navigation",
         {{"DepthBelowTransducer", "m", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 100000},
          {"Offset", "m", "double", QString::number(N2kDoubleNA, 'g', 17), -10000, 10000},
          {"Range", "m", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 100000}},
         {},
         100026},
        {"DBT",
         "DepthDBT",
         "Depth Below Transducer",
         "Navigation",
         {{"DepthBelowTransducer", "m", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 100000}},
         {},
         100027},
        {"MWV",
         "WindMWV",
         "Wind Speed and Angle",
         "Environment",
         {{"WindAngle", "rad", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 6.283185307179586},
          {"Reference", "", "tNMEA0183WindReference", "T", 0, 0},
          {"WindSpeed", "m/s", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 1000},
          {"Status", "", "char (A/V)", "A", 0, 0}},
         {"Reference"},
         100028},
        {"MTW",
         "WaterTemperatureMTW",
         "Water Temperature",
         "Environment",
         {{"WaterTemperature", "K", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 1000}},
         {},
         SNT_MTW},
        {"VHW",
         "WaterSpeedHeadingVHW",
         "Water Speed and Heading",
         "Navigation",
         {{"TrueHeading", "rad", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 6.283185307179586},
          {"MagneticHeading", "rad", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 6.283185307179586},
          {"WaterSpeed", "m/s", "double", QString::number(N2kDoubleNA, 'g', 17), 0, 1000}},
         {},
         SNT_VHW},
        {"GSV",
         "SatellitesGSV",
         "Satellites in View (page)",
         "Navigation",
         {{"TotalMessages", "", "uint32_t", "1", 1, 9},
          {"MessageNumber", "", "uint32_t", "1", 1, 9},
          {"SatelliteCount", "", "uint32_t", "0", 0, 99},
          {"Satellites", "", "QString", "[]", 0, 0}},
         {"MessageNumber"},
         SNT_GSV},
    };
    rows.append(NavigationStatistics::definitions());
    return rows;
    }();
    return rows;
}
const Definition *definition(const QString &code)
{
    for (const auto &d : definitions())
        if (d.code == code)
            return &d;
    return nullptr;
}
const Definition *definition(int id)
{
    for (const auto &d : definitions())
        if (d.id == id)
            return &d;
    return nullptr;
}
QList<QList<QString>> parameters(const QString &code)
{
    QList<QList<QString>> rows;
    if (auto *d = definition(code))
        for (const auto &f : d->fields)
            rows.append({f.name, f.initial, f.unit, f.type});
    return rows;
}

namespace
{
QString number(double v)
{
    return QString::number(v, 'g', 17);
}
bool satellites(const QString &code, const QString &text, QJsonArray *result = nullptr)
{
    if (text.size() > 12000)
        return false;
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return false;
    const auto array = doc.array();
    const bool can = code == "129540";
    if (array.size() > (can ? 18 : 4))
        return false;
    const QStringList keys = can ? QStringList{"PRN", "Elevation", "Azimuth", "SNR", "RangeResiduals", "UsageStatus"}
                                 : QStringList{"PRN", "Elevation", "Azimuth", "SNR"};
    for (const auto &value : array)
    {
        if (!value.isObject())
            return false;
        const auto object = value.toObject();
        if (object.size() != keys.size())
            return false;
        for (const auto &key : keys)
        {
            if (!object[key].isDouble())
                return false;
            const double v = object[key].toDouble();
            if (!std::isfinite(v))
                return false;
            if (key == "PRN" || key == "UsageStatus")
            {
                if (std::floor(v) != v || v < 0 || v > (key == "PRN" ? (can ? 255 : 99) : 15))
                    return false;
            }
            else if (v != N2kDoubleNA)
            {
                const double lo = key == "Elevation"        ? (can ? -1.5708 : 0)
                                  : key == "SNR"            ? (can ? -327.68 : 0)
                                  : key == "RangeResiduals" ? -21474.83648
                                                            : 0;
                const double hi = key == "Elevation" ? (can ? 1.5708 : 90)
                                  : key == "Azimuth" ? (can ? 6.2832 : 359)
                                  : key == "SNR"     ? (can ? 327.65 : 99)
                                                     : 21474.83645;
                if (v < lo || v > hi || (!can && std::floor(v) != v))
                    return false;
            }
        }
    }
    if (result)
        *result = array;
    return true;
}
bool validMap(const QString &code, const QMap<QString, QString> &f)
{
    const auto *d = definition(code);
    if (!d || f.size() != d->fields.size())
        return false;
    for (const auto &field : d->fields)
        if (!f.contains(field.name) || !validField(code, field.name, f[field.name]))
            return false;
    if (code == "130074" && f["TotalWaypoints"].toDouble() < 65533 &&
        f["TotalWaypoints"].toDouble() < QJsonDocument::fromJson(f["Waypoints"].toUtf8()).array().size())
        return false;
    if (code == "RTE" && int(f["MessageNumber"].toDouble()) > int(f["TotalMessages"].toDouble()))
        return false;
    if (code == "GSV")
    {
        QJsonArray a;
        satellites(code, f["Satellites"], &a);
        const int total = f["TotalMessages"].toInt(), page = f["MessageNumber"].toInt(),
                  count = f["SatelliteCount"].toInt();
        if (total < 1 || page < 1 || page > total || total != qMax(1, (count + 3) / 4) ||
            a.size() != qBound(0, count - (page - 1) * 4, 4))
            return false;
    }
    return true;
}
} // namespace
bool validField(const QString &code, const QString &field, const QString &text)
{
    const auto *d = definition(code);
    if (!d)
        return false;
    for (const auto &f : d->fields)
        if (f.name == field)
        {
            if (!choices(f.type).isEmpty())
            {
                for (const auto &choice : choices(f.type))
                {
                    if (choice.first.metaType().id() == QMetaType::QString)
                    {
                        if (choice.first.toString() == text)
                            return true;
                    }
                    else
                    {
                        bool ok = false;
                        const double value = text.toDouble(&ok);
                        if (ok && std::isfinite(value) && value == choice.first.toDouble())
                            return true;
                    }
                }
                return false;
            }
            if (code == "129044" && f.type == "QString") {
                if (text.isEmpty()) return true; // Unavailable fixed-width code.
                if (text.size() < 3 || text.size() > 4) return false;
                for (const auto c : text) if (c.unicode() < 32 || c.unicode() > 126) return false;
                return true;
            }
            if (NavigationStatistics::supported(code) && f.type == "double" &&
                (text.toDouble() == NavigationStatistics::Error || text.toDouble() == NavigationStatistics::Reserved)) return true;
            if (f.type == "QString")
            {
                auto identifier = [](const QString &name) {
                    if (name.isEmpty() || name.size() > 16)
                        return false;
                    for (const auto c : name)
                        if (c.unicode() < 32 || c.unicode() > 126 || QString(",*$!\\").contains(c))
                            return false;
                    return true;
                };
                const auto nativeName = [](const QString &name) {
                    if (name.size() > 16)
                        return false;
                    for (const auto c : name)
                        if (c.unicode() < 32 || c.unicode() > 126)
                            return false;
                    return true;
                };
                if ((code == "129285" || code == "130074") && field == "Waypoints")
                {
                    if (text.size() > 8192)
                        return false;
                    QJsonParseError error;
                    const auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
                    if (error.error != QJsonParseError::NoError || !document.isArray() || document.array().size() > 18)
                        return false;
                    QMap<int, QJsonObject> ids;
                    for (const auto item : document.array())
                    {
                        if (!item.isObject())
                            return false;
                        const auto point = item.toObject();
                        if (point.size() != 4 || !point["ID"].isDouble() || !point["Name"].isString() ||
                            !point["Latitude"].isDouble() || !point["Longitude"].isDouble())
                            return false;
                        const double id = point["ID"].toDouble();
                        if (!std::isfinite(id) || id < 0 || id > 65535 || std::floor(id) != id ||
                            (code == "130074" && id <= 65532 && ids.contains(int(id))) ||
                            !nativeName(point["Name"].toString()))
                            return false;
                        if (id <= 65532)
                        {
                            if (ids.contains(int(id)))
                            {
                                auto prior = ids.value(int(id));
                                if (!prior["Name"].toString().isEmpty() && !point["Name"].toString().isEmpty() &&
                                    prior["Name"] != point["Name"])
                                    return false;
                                for (const auto &key : QStringList{"Latitude", "Longitude"})
                                {
                                    const auto oldValue = prior[key].toDouble(), newValue = point[key].toDouble();
                                    if (oldValue != N2kDoubleNA && newValue != N2kDoubleNA && oldValue != newValue)
                                        return false;
                                    if (oldValue == N2kDoubleNA)
                                        prior[key] = point[key];
                                }
                                if (prior["Name"].toString().isEmpty())
                                    prior["Name"] = point["Name"];
                                ids[int(id)] = prior;
                            }
                            else
                                ids[int(id)] = point;
                        }
                        const double lat = point["Latitude"].toDouble(), lon = point["Longitude"].toDouble();
                        if (!std::isfinite(lat) || !std::isfinite(lon) ||
                            (lat != N2kDoubleNA && (lat < -90 || lat > 90)) ||
                            (lon != N2kDoubleNA && (lon < -180 || lon > 180)))
                            return false;
                    }
                    return true;
                }
                if (code == "129285" && field == "RouteName")
                    return nativeName(text);
                if (((code == "APB" || code == "RMB") && (field == "OriginID" || field == "DestinationID")) ||
                    (code == "RTE" && field == "RouteName") || (code == "WPL" && field == "WaypointName"))
                    return identifier(text);
                if (code == "RTE" && field == "WaypointIDs")
                {
                    QJsonParseError error;
                    const auto doc = QJsonDocument::fromJson(text.toUtf8(), &error);
                    if (error.error != QJsonParseError::NoError || !doc.isArray() || doc.array().size() > 12)
                        return false;
                    for (const auto item : doc.array())
                        if (!item.isString() || !identifier(item.toString()))
                            return false;
                    return true;
                }
                if (code == "127498" && (field == "VIN" || field == "SoftwareID"))
                {
                    if (text.size() > 32)
                        return false;
                    for (const auto c : text)
                        if (c.unicode() < 32 || c.unicode() > 126)
                            return false;
                    return true;
                }
                if (field == "Satellites")
                    return satellites(code, text);
                if (field == "Reference")
                    return text == "T" || text == "R";
                if (field == "Status")
                    return text == "A" || text == "V";
                return false;
            }
            bool ok = false;
            const double v = text.toDouble(&ok);
            if (!ok || !std::isfinite(v))
                return false;
            if (f.type == "double" && v == N2kDoubleNA)
                return true;
            if (code == "GSA" && field.startsWith("Satellite") && v != 255 && (v < 1 || v > 99))
                return false;
            if (v < f.minimum || v > f.maximum || (f.type != "double" && std::floor(v) != v))
                return false;
            if (field == "TemperatureSource" && v > 15 && v != 62 && v != 63)
                return false;
            if (field == "PressureSource" && v > 8 && v < 253)
                return false;
            return true;
        }
    return false;
}
bool validParameters(const QString &code, const QStringList &fields)
{
    if (fields.size() % 2)
        return false;
    QMap<QString, QString> f;
    for (int i = 0; i < fields.size(); i += 2)
    {
        if (f.contains(fields[i]))
            return false;
        f[fields[i]] = fields[i + 1];
    }
    return validMap(code, f);
}
bool encode(const QString &code, const QMap<QString, QString> &f, tN2kMsg &m)
{
    if (!validMap(code, f))
        return false;
    if (NavigationStatistics::supported(code)) return NavigationStatistics::encode(code, f, m);
    auto v = [&](const char *s) { return f[s].toDouble(); };
    if (code == "129285" || code == "130074")
    {
        if (code == "129285")
            SetN2kPGN129285(m, v("StartRPS"), v("DatabaseID"), v("RouteID"),
                            tN2kNavigationDirection(int(v("NavigationDirection"))),
                            f["RouteName"].toLatin1().constData(), tN2kGenericStatusPair(int(v("SupplementaryData"))));
        else
            SetN2kPGN130074(m, v("StartWaypointID"), v("TotalWaypoints"), v("DatabaseID"));
        for (const auto item : QJsonDocument::fromJson(f["Waypoints"].toUtf8()).array())
        {
            const auto point = item.toObject();
            auto name = point["Name"].toString().toLatin1();
            const bool ok = code == "129285"
                                ? AppendN2kPGN129285(m, point["ID"].toInt(), name.constData(),
                                                     point["Latitude"].toDouble(), point["Longitude"].toDouble())
                                : AppendN2kPGN130074(m, point["ID"].toInt(), name.data(), point["Latitude"].toDouble(),
                                                     point["Longitude"].toDouble());
            if (!ok)
                return false;
        }
    }
    else if (code == "127498")
    {
        m.SetPGN(127498);
        m.Priority = 5;
        m.AddByte(v("EngineInstance"));
        m.Add2ByteUDouble(v("RatedEngineSpeed"), 0.25);
        m.AddVarStr(f["VIN"].toLatin1().constData());
        m.AddVarStr(f["SoftwareID"].toLatin1().constData());
    }
    else if (code == "127509")
    {
        m.SetPGN(127509);
        m.Priority = 6;
        m.AddByte(v("Instance"));
        m.AddByte(v("ACInstance"));
        m.AddByte(v("DCInstance"));
        m.AddByte(0xc0 | (int(v("InverterEnable")) << 4) | int(v("OperatingState")));
    }
    else if (code == "127496")
    {
        // CANboat PGN 127496: unsigned 32/32/16/32, seconds/metres/litres/seconds.
        m.SetPGN(127496);
        m.Priority = 5;
        m.Add4ByteUDouble(v("TimeToEmpty"), 0.001);
        m.Add4ByteUDouble(v("DistanceToEmpty"), 0.01);
        m.Add2ByteUDouble(v("EstimatedFuelRemaining"), 1);
        m.Add4ByteUDouble(v("TripRunTime"), 0.001);
    }
    else if (code == "130311")
        SetN2kPGN130311(m, v("SID"), tN2kTempSource(int(v("TemperatureSource"))), v("Temperature"),
                        tN2kHumiditySource(int(v("HumiditySource"))), v("Humidity"), v("AtmosphericPressure"));
    else if (code == "130314")
        SetN2kPGN130314(m, v("SID"), v("PressureInstance"), tN2kPressureSource(int(v("PressureSource"))),
                        v("ActualPressure"));
    else if (code == "129539")
        SetN2kPGN129539(m, v("SID"), tN2kGNSSDOPmode(int(v("DesiredMode"))), tN2kGNSSDOPmode(int(v("ActualMode"))),
                        v("HDOP"), v("VDOP"), v("TDOP"));
    else if (code == "128275")
        SetN2kPGN128275(m, v("DaysSince1970"), v("SecondsSinceMidnight"), static_cast<uint32_t>(f["Log"].toDouble()),
                        static_cast<uint32_t>(f["TripLog"].toDouble()));
    else if (code == "127497")
        SetN2kPGN127497(m, v("EngineInstance"), v("TripFuelUsed"), v("FuelRateAverage"), v("FuelRateEconomy"),
                        v("InstantaneousFuelEconomy"));
    else if (code == "129540")
    {
        SetN2kPGN129540(m, v("SID"), tN2kRangeResidualMode(int(v("Mode"))));
        QJsonArray array;
        satellites(code, f["Satellites"], &array);
        for (const auto &item : array)
        {
            const auto o = item.toObject();
            tSatelliteInfo sat;
            sat.PRN = o["PRN"].toInt();
            sat.Elevation = o["Elevation"].toDouble();
            sat.Azimuth = o["Azimuth"].toDouble();
            sat.SNR = o["SNR"].toDouble();
            sat.RangeResiduals = o["RangeResiduals"].toDouble();
            sat.UsageStatus = tN2kPRNUsageStatus(o["UsageStatus"].toInt());
            if (!AppendN2kPGN129540(m, sat))
                return false;
        }
    }
    else
        return false;
    return true;
}
bool decode(const tN2kMsg &m, QStringList &fields)
{
    const QString code = QString::number(m.PGN);
    if (!definition(code))
        return false;
    if (NavigationStatistics::supported(code)) return NavigationStatistics::decode(m, fields);
    QMap<QString, QString> f;
    auto add = [&](const char *key, double v) { f[key] = number(v); };
    unsigned char sid, instance;
    double a, b, c;
    if (code == "129285" || code == "130074")
    {
        if (m.DataLen < 10)
            return false;
        int index = 0;
        add(code == "129285" ? "StartRPS" : "StartWaypointID", m.Get2ByteUInt(index));
        const auto count = m.Get2ByteUInt(index);
        if (count > 18)
            return false;
        const auto readName = [&](QString &name) {
            if (index + 2 > m.DataLen)
                return false;
            const int length = m.Data[index], encoding = m.Data[index + 1];
            if (length < 2 || length > 18 || encoding != 1 || index + length > m.DataLen)
                return false;
            name = QString::fromLatin1(reinterpret_cast<const char *>(m.Data + index + 2), length - 2);
            index += length;
            return true;
        };
        if (code == "129285")
        {
            add("DatabaseID", m.Get2ByteUInt(index));
            add("RouteID", m.Get2ByteUInt(index));
            const auto flags = m.GetByte(index);
            add("NavigationDirection", flags & 7);
            add("SupplementaryData", (flags >> 3) & 3);
            QString name;
            if (!readName(name) || index >= m.DataLen)
                return false;
            f["RouteName"] = name;
            m.GetByte(index);
        }
        else
        {
            add("TotalWaypoints", m.Get2ByteUInt(index));
            add("DatabaseID", m.Get2ByteUInt(index));
            m.GetByte(index);
            m.GetByte(index);
        }
        QJsonArray array;
        for (int i = 0; i < count; ++i)
        {
            if (index + 2 > m.DataLen)
                return false;
            const int id = m.Get2ByteUInt(index);
            QString name;
            if (!readName(name) || index + 8 > m.DataLen)
                return false;
            const double lat = m.Get4ByteDouble(1e-7, index), lon = m.Get4ByteDouble(1e-7, index);
            array.append(QJsonObject{{"ID", id}, {"Name", name}, {"Latitude", lat}, {"Longitude", lon}});
        }
        if (index != m.DataLen)
            return false;
        f["Waypoints"] = QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
    }
    else if (code == "127498")
    {
        if (m.DataLen < 7)
            return false;
        int index = 0;
        add("EngineInstance", m.GetByte(index));
        add("RatedEngineSpeed", m.Get2ByteUDouble(0.25, index));
        for (const QString key : {QString("VIN"), QString("SoftwareID")})
        {
            if (index + 2 > m.DataLen)
                return false;
            const int length = m.Data[index], encoding = m.Data[index + 1];
            if (length < 2 || length > 34 || encoding != 1 || index + length > m.DataLen)
                return false;
            f[key] = QString::fromLatin1(reinterpret_cast<const char *>(m.Data + index + 2), length - 2);
            index += length;
        }
        if (index != m.DataLen)
            return false;
    }
    else if (code == "127509")
    {
        if (m.DataLen != 4)
            return false;
        add("Instance", m.Data[0]);
        add("ACInstance", m.Data[1]);
        add("DCInstance", m.Data[2]);
        add("OperatingState", m.Data[3] & 15);
        add("InverterEnable", (m.Data[3] >> 4) & 3);
    }
    else if (code == "127496")
    {
        if (m.DataLen != 14)
            return false;
        int index = 0;
        add("TimeToEmpty", m.Get4ByteUDouble(0.001, index));
        add("DistanceToEmpty", m.Get4ByteUDouble(0.01, index));
        add("EstimatedFuelRemaining", m.Get2ByteUDouble(1, index));
        add("TripRunTime", m.Get4ByteUDouble(0.001, index));
    }
    else if (code == "130311")
    {
        if (m.DataLen != 8)
            return false;
        tN2kTempSource t;
        tN2kHumiditySource h;
        if (!ParseN2kPGN130311(m, sid, t, a, h, b, c))
            return false;
        add("SID", sid);
        add("TemperatureSource", t);
        add("Temperature", a);
        add("HumiditySource", h);
        add("Humidity", b);
        add("AtmosphericPressure", c);
    }
    else if (code == "130314")
    {
        if (m.DataLen != 8)
            return false;
        tN2kPressureSource p;
        if (!ParseN2kPGN130314(m, sid, instance, p, a))
            return false;
        add("SID", sid);
        add("PressureInstance", instance);
        add("PressureSource", p);
        add("ActualPressure", a);
    }
    else if (code == "129539")
    {
        if (m.DataLen != 8)
            return false;
        tN2kGNSSDOPmode desired, actual;
        if (!ParseN2kPgn129539(m, sid, desired, actual, a, b, c))
            return false;
        add("SID", sid);
        add("DesiredMode", desired);
        add("ActualMode", actual);
        add("HDOP", a);
        add("VDOP", b);
        add("TDOP", c);
    }
    else if (code == "128275")
    {
        if (m.DataLen != 14)
            return false;
        uint16_t days;
        uint32_t log, trip;
        if (!ParseN2kPGN128275(m, days, a, log, trip))
            return false;
        // The pinned parser reads the integer logs through Get4ByteUDouble and
        // loses their uint32 NA sentinel. Preserve their actual integer fields.
        int index = 6;
        log = m.Get4ByteUInt(index);
        trip = m.Get4ByteUInt(index);
        add("DaysSince1970", days);
        add("SecondsSinceMidnight", a);
        add("Log", log);
        add("TripLog", trip);
    }
    else if (code == "127497")
    {
        if (m.DataLen != 9)
            return false;
        double d;
        if (!ParseN2kPGN127497(m, instance, a, b, c, d))
            return false;
        add("EngineInstance", instance);
        add("TripFuelUsed", a);
        add("FuelRateAverage", b);
        add("FuelRateEconomy", c);
        add("InstantaneousFuelEconomy", d);
    }
    else if (code == "129540")
    {
        if (m.DataLen < 3 || m.Data[2] > 18 || m.DataLen != 3 + m.Data[2] * 12)
            return false;
        uint8_t count;
        tN2kRangeResidualMode mode;
        if (!ParseN2kPGN129540(m, sid, mode, count))
            return false;
        add("SID", sid);
        add("Mode", mode);
        QJsonArray array;
        for (int i = 0; i < count; ++i)
        {
            tSatelliteInfo s;
            if (!ParseN2kPGN129540(m, i, s))
                return false;
            array.append(QJsonObject{{"PRN", s.PRN},
                                     {"Elevation", s.Elevation},
                                     {"Azimuth", s.Azimuth},
                                     {"SNR", s.SNR},
                                     {"RangeResiduals", s.RangeResiduals},
                                     {"UsageStatus", int(s.UsageStatus)}});
        }
        f["Satellites"] = QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
    }
    else
        return false;
    if (!validMap(code, f))
        return false;
    QStringList candidate;
    for (const auto &field : definition(code)->fields)
        candidate << field.name << f[field.name];
    fields = candidate;
    return true;
}

bool encodeSentence(const QString &code, const QMap<QString, QString> &f, tNMEA0183Msg &m)
{
    if (!validMap(code, f))
        return false;
    auto v = [&](const char *s) { return f[s].toDouble(); };
    auto degrees = [](double n) { return n == N2kDoubleNA ? NMEA0183DoubleNA : n * 180 / M_PI; };
    if (code == "APB" || code == "RMB")
    {
        if (!m.Init(code.toLatin1().constData(), "GP"))
            return false;
        const auto str = [&](const char *name) { return m.AddStrField(f[name].toLatin1().constData()); };
        const auto angle = [&](const char *name) { return m.AddDoubleField(v(name), 180 / M_PI, "%.3f"); };
        const auto distance = [&](const char *name) { return m.AddDoubleField(v(name), 1.0 / 1852, "%.3f"); };
        if (!str("Status"))
            return false;
        if (code == "APB")
        {
            return str("CycleLockStatus") && distance("XTE") && str("SteerDirection") && m.AddStrField("N") &&
                   str("ArrivalAlarm") && str("PerpendicularPassed") && angle("OriginBearing") &&
                   str("OriginReference") && str("DestinationID") && angle("PositionBearing") &&
                   str("PositionReference") && angle("HeadingToSteer") && str("HeadingReference");
        }
        return distance("XTE") && str("SteerDirection") && str("OriginID") && str("DestinationID") &&
               m.AddLatitudeField(v("Latitude"), "%012.7f") && m.AddLongitudeField(v("Longitude"), "%013.7f") &&
               distance("DistanceToWaypoint") && angle("BearingToWaypoint") &&
               m.AddDoubleField(v("ClosingVelocity"), 3600.0 / 1852, "%.3f") && str("ArrivalAlarm");
    }
    if (code == "WPL")
    {
        if (!m.Init("WPL", "GP"))
            return false;
        return m.AddLatitudeField(v("Latitude"), "%012.7f") && m.AddLongitudeField(v("Longitude"), "%013.7f") &&
               m.AddStrField(f["WaypointName"].toLatin1().constData());
    }
    if (code == "RTE")
    {
        if (!m.Init("RTE", "GP") || !m.AddUInt32Field(v("TotalMessages")) || !m.AddUInt32Field(v("MessageNumber")) ||
            !m.AddStrField(f["RouteType"].toLatin1().constData()) ||
            !m.AddStrField(f["RouteName"].toLatin1().constData()))
            return false;
        const auto ids = QJsonDocument::fromJson(f["WaypointIDs"].toUtf8()).array();
        for (const auto id : ids)
            if (!m.AddStrField(id.toString().toLatin1().constData()))
                return false;
        return true;
    }
    if (code == "GSA")
    {
        if (!m.Init("GSA", "GP") || !m.AddStrField(f["SelectionMode"].toLatin1().constData()) ||
            !m.AddUInt32Field(v("FixType")))
            return false;
        for (int i = 1; i <= 12; ++i)
        {
            const auto n = f["Satellite" + QString::number(i)].toUInt();
            if (n == 255)
            {
                if (!m.AddStrField(""))
                    return false;
            }
            else if (!m.AddUInt32Field(n))
                return false;
        }
        for (const auto &key : QStringList{"PDOP", "HDOP", "VDOP"})
            if (!m.AddDoubleField(f[key].toDouble(), 1, "%.2f"))
                return false;
        return true;
    }
    if (code == "DPT")
        return NMEA0183SetDPT(m, v("DepthBelowTransducer"), v("Offset"), v("Range"));
    if (code == "DBT")
        return NMEA0183SetDBT(m, v("DepthBelowTransducer"));
    if (code == "MTW")
        return NMEA0183SetMTW(m,
                              v("WaterTemperature") == N2kDoubleNA ? NMEA0183DoubleNA : v("WaterTemperature") - 273.15);
    if (code == "VHW")
        return NMEA0183SetVHW(m, v("TrueHeading"), v("MagneticHeading"), v("WaterSpeed"));
    if (code == "MWV")
    {
        if (!NMEA0183SetMWV(m, degrees(v("WindAngle")),
                            f["Reference"] == "T" ? NMEA0183Wind_True : NMEA0183Wind_Apparent, v("WindSpeed")))
            return false;
        // The pinned constructor fixes status A. Preserve the configured status
        // using the same message builder, without altering the library.
        if (f["Status"] == "V")
        {
            if (!m.Init("MWV", "II") || !m.AddDoubleField(degrees(v("WindAngle"))) ||
                !m.AddStrField(f["Reference"].toLatin1().constData()) || !m.AddDoubleField(v("WindSpeed")) ||
                !m.AddStrField("M") || !m.AddStrField("V"))
                return false;
        }
        return true;
    }
    if (code == "GSV")
    {
        QJsonArray a;
        satellites(code, f["Satellites"], &a);
        // The upstream helper always writes four slots, including absent slots.
        // Build the bounded final page with only its actual records.
        if (!m.Init("GSV", "GP") || !m.AddUInt32Field(v("TotalMessages")) || !m.AddUInt32Field(v("MessageNumber")) ||
            !m.AddUInt32Field(v("SatelliteCount")))
            return false;
        for (const auto &value : a)
            for (const auto &key : QStringList{"PRN", "Elevation", "Azimuth", "SNR"})
            {
                const double n = value.toObject()[key].toDouble();
                if (n == N2kDoubleNA)
                {
                    if (!m.AddStrField(""))
                        return false;
                }
                else if (!m.AddUInt32Field(n))
                    return false;
            }
        return true;
    }
    return false;
}
bool decodeSentence(const tNMEA0183Msg &m, QStringList &fields)
{
    const QString code = QString::fromLatin1(m.MessageCode());
    if (!definition(code) || code.size() != 3)
        return false;
    QStringList wire;
    for (int i = 0; i < m.FieldCount(); ++i)
        wire << QString::fromLatin1(m.Field(i));
    auto numeric = [&](int i, double lo, double hi, bool integer = false) {
        if (i >= wire.size())
            return false;
        if (wire[i].isEmpty())
            return true;
        static const QRegularExpression decimal("^[+-]?[0-9]+(?:\\.[0-9]+)?$");
        bool ok;
        const double n = wire[i].toDouble(&ok);
        return decimal.match(wire[i]).hasMatch() && ok && std::isfinite(n) && n >= lo && n <= hi &&
               (!integer || std::floor(n) == n);
    };
    auto n = [&](int i) { return wire[i].isEmpty() ? N2kDoubleNA : wire[i].toDouble(); };
    auto scaled = [](double v, double scale, double offset = 0.) { return v == N2kDoubleNA ? v : v * scale + offset; };
    QMap<QString, QString> f;
    auto add = [&](const char *key, double value) { f[key] = number(value); };
    if (code == "APB" || code == "RMB")
    {
        if (wire.size() != (code == "APB" ? 14 : 13) || !numeric(code == "APB" ? 2 : 1, 0, 100))
            return false;
        const auto charField = [&](const char *name, int index, const QStringList &allowed) {
            if (!allowed.contains(wire[index]))
                return false;
            f[name] = wire[index];
            return true;
        };
        const QStringList av{"A", "V"}, lr{"L", "R"}, refs{"T", "M"};
        if (!charField("Status", 0, av))
            return false;
        if (code == "APB")
        {
            if (!charField("CycleLockStatus", 1, av) || !charField("SteerDirection", 3, lr) || wire[4] != "N" ||
                !charField("ArrivalAlarm", 5, av) || !charField("PerpendicularPassed", 6, av) || !numeric(7, 0, 360) ||
                !charField("OriginReference", 8, refs) || !numeric(10, 0, 360) ||
                !charField("PositionReference", 11, refs) || !numeric(12, 0, 360) ||
                !charField("HeadingReference", 13, refs))
                return false;
            tAPB parsed;
            if (!NMEA0183ParseAPB_nc(m, parsed))
                return false;
            add("XTE", scaled(n(2), 1852));
            add("OriginBearing", scaled(n(7), M_PI / 180));
            add("PositionBearing", scaled(n(10), M_PI / 180));
            add("HeadingToSteer", scaled(n(12), M_PI / 180));
            f["DestinationID"] = wire[9];
        }
        else
        {
            if (!charField("SteerDirection", 2, lr) || !numeric(9, 0, 100000) || !numeric(10, 0, 360) ||
                !numeric(11, 0, 1943.85) || !charField("ArrivalAlarm", 12, av))
                return false;
            for (int offset : {5, 7})
            {
                if (wire[offset].isEmpty() && wire[offset + 1].isEmpty())
                    continue;
                const bool lat = offset == 5;
                if (!numeric(offset, 0, lat ? 9000 : 18000) || wire[offset].isEmpty() ||
                    !(lat ? QStringList{"N", "S"} : QStringList{"E", "W"}).contains(wire[offset + 1]))
                    return false;
                const double position = wire[offset].toDouble();
                const int degrees = int(position / 100);
                if (position - degrees * 100 >= 60 || (degrees == (lat ? 90 : 180) && position - degrees * 100 != 0))
                    return false;
            }
            tRMB parsed;
            if (!NMEA0183ParseRMB_nc(m, parsed))
                return false;
            add("XTE", scaled(n(1), 1852));
            f["OriginID"] = wire[3];
            f["DestinationID"] = wire[4];
            add("Latitude", wire[5].isEmpty() ? N2kDoubleNA : parsed.latitude);
            add("Longitude", wire[7].isEmpty() ? N2kDoubleNA : parsed.longitude);
            add("DistanceToWaypoint", scaled(n(9), 1852));
            add("BearingToWaypoint", scaled(n(10), M_PI / 180));
            add("ClosingVelocity", scaled(n(11), 1852.0 / 3600));
        }
    }
    else if (code == "RTE")
    {
        if (wire.size() < 4 || wire.size() > 16 || !numeric(0, 1, 16, true) || !numeric(1, 1, 16, true) ||
            wire[0].isEmpty() || wire[1].isEmpty())
            return false;
        f["TotalMessages"] = wire[0];
        f["MessageNumber"] = wire[1];
        f["RouteType"] = wire[2];
        f["RouteName"] = wire[3];
        QJsonArray ids;
        for (int i = 4; i < wire.size(); ++i)
            ids.append(wire[i]);
        f["WaypointIDs"] = QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact));
    }
    else if (code == "WPL")
    {
        if (wire.size() != 5)
            return false;
        for (int offset : {0, 2})
        {
            const bool latitude = offset == 0;
            if (wire[offset].isEmpty() && wire[offset + 1].isEmpty())
                continue;
            if (!numeric(offset, 0, latitude ? 9000 : 18000) || wire[offset].isEmpty() ||
                !(latitude ? QStringList{"N", "S"} : QStringList{"E", "W"}).contains(wire[offset + 1]))
                return false;
            const double position = wire[offset].toDouble();
            const int degrees = int(position / 100);
            if (position - degrees * 100 >= 60 || (degrees == (latitude ? 90 : 180) && position - degrees * 100 != 0))
                return false;
        }
        tWPL waypoint;
        if (!NMEA0183ParseWPL_nc(m, waypoint))
            return false;
        add("Latitude", wire[0].isEmpty() ? N2kDoubleNA : waypoint.latitude);
        add("Longitude", wire[2].isEmpty() ? N2kDoubleNA : waypoint.longitude);
        f["WaypointName"] = wire[4];
    }
    else if (code == "GSA")
    {
        if (wire.size() != 17 || (wire[0] != "A" && wire[0] != "M") || !numeric(1, 1, 3, true) || wire[1].isEmpty())
            return false;
        f["SelectionMode"] = wire[0];
        add("FixType", wire[1].toDouble());
        for (int i = 0; i < 12; ++i)
        {
            if (!numeric(i + 2, 1, 99, true))
                return false;
            add(qPrintable("Satellite" + QString::number(i + 1)), wire[i + 2].isEmpty() ? 255 : wire[i + 2].toDouble());
        }
        for (int i = 0; i < 3; ++i)
        {
            if (!numeric(i + 14, 0, 99.9))
                return false;
            add(qPrintable((QStringList{"PDOP", "HDOP", "VDOP"}[i])), n(i + 14));
        }
    }
    else if (code == "DPT")
    {
        if ((wire.size() != 2 && wire.size() != 3) || !numeric(0, 0, 100000) || !numeric(1, -10000, 10000) ||
            (wire.size() == 3 && !numeric(2, 0, 100000)))
            return false;
        double depth, offset, range = NMEA0183DoubleNA;
        if (wire.size() == 3)
        {
            if (!NMEA0183ParseDPT_nc(m, depth, offset, range))
                return false;
        }
        else if (!NMEA0183ParseDPT_nc(m, depth, offset))
            return false;
        add("DepthBelowTransducer", depth);
        add("Offset", offset);
        add("Range", range);
    }
    else if (code == "DBT")
    {
        if (wire.size() != 6 || wire[1] != "f" || wire[3] != "M" || wire[5] != "F" || !numeric(0, 0, 400000) ||
            !numeric(2, 0, 100000) || !numeric(4, 0, 60000))
            return false;
        // No DBT parser exists upstream. Prefer metres, with conventional
        // feet/fathoms fallbacks when the metric field is unavailable.
        double depth = n(2);
        if (depth == N2kDoubleNA)
            depth = scaled(n(0), 0.3048);
        if (depth == N2kDoubleNA)
            depth = scaled(n(4), 1.8288);
        add("DepthBelowTransducer", depth);
    }
    else if (code == "MTW")
    {
        if (wire.size() != 2 || wire[1] != "C" || !numeric(0, -273.15, 726.85))
            return false;
        double temp;
        if (!NMEA0183ParseMTW_nc(m, temp))
            return false;
        add("WaterTemperature", scaled(n(0), 1, 273.15)); // upstream atof maps blank to zero
    }
    else if (code == "VHW")
    {
        if (wire.size() != 8 || wire[1] != "T" || wire[3] != "M" || wire[5] != "N" || wire[7] != "K" ||
            !numeric(0, 0, 360) || !numeric(2, 0, 360) || !numeric(4, 0, 1943.85) || !numeric(6, 0, 3600))
            return false;
        double t, h, s;
        if (!NMEA0183ParseVHW_nc(m, t, h, s))
            return false;
        add("TrueHeading", t);
        add("MagneticHeading", h);
        add("WaterSpeed", s);
    }
    else if (code == "MWV")
    {
        if (wire.size() != 5 || !numeric(0, 0, 360) || !numeric(2, 0, 3600) || (wire[1] != "T" && wire[1] != "R") ||
            !QStringList{"M", "N", "K"}.contains(wire[3]) || (wire[4] != "A" && wire[4] != "V"))
            return false;
        double angle, speed;
        tNMEA0183WindReference ref;
        if (!NMEA0183ParseMWV_nc(m, angle, ref, speed))
            return false;
        add("WindAngle", scaled(n(0), M_PI / 180));
        add("WindSpeed", scaled(n(2), wire[3] == "N" ? 0.5144444444444444 : wire[3] == "K" ? 1. / 3.6 : 1.));
        f["Reference"] = wire[1];
        f["Status"] = wire[4];
    }
    else if (code == "GSV")
    {
        // A page is a bounded instrument identified by talker and message number.
        // No unbounded multipart session or implied route/GNSS generator exists.
        if (wire.size() < 3 || wire.size() > 19 || (wire.size() - 3) % 4 || !numeric(0, 1, 9, true) ||
            !numeric(1, 1, 9, true) || !numeric(2, 0, 99, true) || wire[0].isEmpty() || wire[1].isEmpty() ||
            wire[2].isEmpty())
            return false;
        add("TotalMessages", n(0));
        add("MessageNumber", n(1));
        add("SatelliteCount", n(2));
        QJsonArray a;
        for (int i = 3; i < wire.size(); i += 4)
        {
            if (wire[i].isEmpty() || !numeric(i, 0, 99, true) || !numeric(i + 1, -90, 90, true) ||
                !numeric(i + 2, 0, 359, true) || !numeric(i + 3, 0, 99, true))
                return false;
            a.append(QJsonObject{{"PRN", n(i)}, {"Elevation", n(i + 1)}, {"Azimuth", n(i + 2)}, {"SNR", n(i + 3)}});
        }
        f["Satellites"] = QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
    }
    else
        return false;
    if (!validMap(code, f))
        return false;
    QStringList candidate;
    for (const auto &field : definition(code)->fields)
        candidate << field.name << f[field.name];
    fields = candidate;
    return true;
}

// Receive-only route metadata. This is not a periodic instrument or a request client.
bool decodeRouteAttributes(const tN2kMsg &m, QStringList &fields)
{
    if (m.PGN != 130066 || m.DataLen < 19)
        return false;
    int index = 0;
    QMap<QString, QString> f;
    const auto add = [&](const char *key, quint64 value) { f[key] = QString::number(value); };
    add("DatabaseID", m.Get2ByteUInt(index));
    add("RouteID", m.Get2ByteUInt(index));
    const int length = m.Data[index], encoding = m.Data[index + 1];
    if (length < 2 || length > 18 || encoding != 1 || index + length + 13 > m.DataLen)
        return false;
    f["RouteName"] = QString::fromLatin1(reinterpret_cast<const char *>(m.Data + index + 2), length - 2);
    index += length;
    for (const auto c : f["RouteName"])
        if (c.unicode() < 32 || c.unicode() > 126)
            return false;
    add("Timestamp", m.Get4ByteUInt(index));
    add("Datestamp", m.Get2ByteUInt(index));
    add("ChangeFlags", m.GetByte(index));
    add("WaypointCount", m.Get2ByteUInt(index));
    add("CriticalParameters", m.GetByte(index));
    const auto flags = m.GetByte(index);
    add("NavigationMethod", flags & 3);
    add("IdentificationMethod", (flags >> 2) & 3);
    add("RouteStatus", flags >> 4);
    if (index + 2 != m.DataLen)
        return false;
    f["XTELimit"] = QString::number(m.Get2ByteDouble(1, index), 'g', 17);
    QStringList candidate;
    for (auto it = f.begin(); it != f.end(); ++it)
        candidate << it.key() << it.value();
    fields = candidate;
    return true;
}

bool decodeRouteGeometry(const tN2kMsg &m, QStringList &fields)
{
    if (m.PGN != 130067 || m.DataLen < 10)
        return false;
    int index = 0;
    QMap<QString, QString> f;
    const auto add = [&](const char *key, int n) { f[key] = QString::number(n); };
    add("StartRPS", m.Get2ByteUInt(index));
    const int count = m.Get2ByteUInt(index);
    if (count > 18)
        return false;
    add("TotalWaypoints", m.Get2ByteUInt(index));
    add("DatabaseID", m.Get2ByteUInt(index));
    add("RouteID", m.Get2ByteUInt(index));
    QJsonArray points;
    for (int i = 0; i < count; ++i)
    {
        if (index + 4 > m.DataLen)
            return false;
        const int id = m.Get2ByteUInt(index), length = m.Data[index], encoding = m.Data[index + 1];
        if (length < 2 || length > 18 || encoding != 1 || index + length + 8 > m.DataLen)
            return false;
        const auto name = QString::fromLatin1(reinterpret_cast<const char *>(m.Data + index + 2), length - 2);
        index += length;
        const double lat = m.Get4ByteDouble(1e-7, index), lon = m.Get4ByteDouble(1e-7, index);
        points.append(QJsonObject{{"ID", id}, {"Name", name}, {"Latitude", lat}, {"Longitude", lon}});
    }
    if (index != m.DataLen)
        return false;
    const auto text = QString::fromUtf8(QJsonDocument(points).toJson(QJsonDocument::Compact));
    if (!validField("129285", "Waypoints", text))
        return false;
    f["Waypoints"] = text;
    f["GeometryPGN"] = "130067";
    QStringList candidate;
    for (auto it = f.begin(); it != f.end(); ++it)
        candidate << it.key() << it.value();
    fields = candidate;
    return true;
}
} // namespace Expansion
