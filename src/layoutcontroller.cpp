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
#include "applicationlog.h"
#include "windlassstatus.h"
#include "aisclassbposition129039.h"
#include "aisclassbstatic129810.h"
#include "aisclassbstatic129809.h"
#include "vesselelectricalsystem.h"
#include "maincontroller.h"
#include "n2kconst.h"
#include "pgnwidgetbase.h"
#include "spacer000000.h"
#include "fluidlevel127505.h"
#include "generatorweather.h"
#include "convert.h"
#include <QCheckBox>
#include <QApplication>
#include <QFrame>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>

bool MainController::selectLayoutWidget(QWidget* widget, const QString& field)
{
    if (!widget || m_restoringLayout) return false;
    bool registered = false;
    for (const auto& group : groupWidgetsMap) if (group.contains(widget)) { registered = true; break; }
    if (!registered) return false;
    auto* base = qobject_cast<PgnWidgetBase*>(widget);
    if (!base || !pgnTableController->getPgnTableModel()->findPgn(base->Id())) return false;
    currentIdX = base->Id();
    pgnTableController->getPgnTableModel()->setIdxHasFocus(currentIdX);
    if (!field.isEmpty()) pgnTableController->selectParameter(field);
    emit layoutWidgetSelected(widget);
    return true;
}

bool MainController::validateLayout(const QJsonObject& root, QString *error) const
{
    struct Definition {
        QString pgn, fields;
    };
    static const QMap<QString, Definition> definitions = {
        {"Datum129044", {"129044", "LocalDatum,DeltaLatitude,DeltaLongitude,DeltaAltitude,ReferenceDatum"}},
        {"GnssNoise129542", {"129542", "SID,RMSPositionUncertainty,MajorAxisDeviation,MinorAxisDeviation,MajorAxisOrientation,LatitudeDeviation,LongitudeDeviation,AltitudeDeviation"}},
        {"GnssError129547", {"129547", "SID,RMSRangeInputDeviation,MajorErrorEllipseDeviation,MinorErrorEllipseDeviation,ErrorEllipseOrientation,LatitudeDeviation,LongitudeDeviation,AltitudeDeviation"}},
        {"PilotHeading65359", {"65359", "ManufacturerCode,HeaderReserved,IndustryCode,SID,TrueHeading,MagneticHeading,ReservedByte"}},
 {"RouteInfo129285",{"129285","StartRPS,DatabaseID,RouteID,NavigationDirection,SupplementaryData,RouteName,Waypoints"}},
 {"WaypointList130074",{"130074","StartWaypointID,TotalWaypoints,DatabaseID,Waypoints"}},
 {"AutopilotAPB",{"APB","Status,CycleLockStatus,XTE,SteerDirection,ArrivalAlarm,PerpendicularPassed,OriginBearing,OriginReference,DestinationID,PositionBearing,PositionReference,HeadingToSteer,HeadingReference"}},
 {"NavigationRMB",{"RMB","Status,XTE,SteerDirection,OriginID,DestinationID,Latitude,Longitude,DistanceToWaypoint,BearingToWaypoint,ClosingVelocity,ArrivalAlarm"}},

 {"RouteRTE",{"RTE","TotalMessages,MessageNumber,RouteType,RouteName,WaypointIDs"}},
 {"WaypointWPL",{"WPL","Latitude,Longitude,WaypointName"}},
 {"GnssActiveGSA", {"GSA","SelectionMode,FixType,Satellite1,Satellite2,Satellite3,Satellite4,Satellite5,Satellite6,Satellite7,Satellite8,Satellite9,Satellite10,Satellite11,Satellite12,PDOP,HDOP,VDOP"}},
        { "EngineStatic127498", { "127498", "EngineInstance,RatedEngineSpeed,VIN,SoftwareID" } },
        { "InverterStatus127509", { "127509", "Instance,ACInstance,DCInstance,OperatingState,InverterEnable" } },
        { "VesselTrip127496", { "127496", "TimeToEmpty,DistanceToEmpty,EstimatedFuelRemaining,TripRunTime" } },
        { "EnvironmentalLegacy130311", { "130311", "SID,TemperatureSource,Temperature,HumiditySource,Humidity,AtmosphericPressure" } },
        { "GnssDops129539", { "129539", "SID,DesiredMode,ActualMode,HDOP,VDOP,TDOP" } },
        { "GnssSatellites129540", { "129540", "SID,Mode,Satellites" } },
        { "DistanceLog128275", { "128275", "DaysSince1970,SecondsSinceMidnight,Log,TripLog" } },
        { "Pressure130314", { "130314", "SID,PressureInstance,PressureSource,ActualPressure" } },
        { "EngineTrip127497", { "127497", "EngineInstance,TripFuelUsed,FuelRateAverage,FuelRateEconomy,InstantaneousFuelEconomy" } },
        { "DepthDPT", { "DPT", "DepthBelowTransducer,Offset,Range" } },
        { "DepthDBT", { "DBT", "DepthBelowTransducer" } },
        { "WindMWV", { "MWV", "WindAngle,Reference,WindSpeed,Status" } },
        { "WaterTemperatureMTW", { "MTW", "WaterTemperature" } },
        { "WaterSpeedHeadingVHW", { "VHW", "TrueHeading,MagneticHeading,WaterSpeed" } },
        { "SatellitesGSV", { "GSV", "TotalMessages,MessageNumber,SatelliteCount,Satellites" } },

        { "TrimTab130576", { "130576", "PortTrimTab,StbdTrimTab" } },
        { "Attitude127257", { "127257", "SID,Yaw,Pitch,Roll" } },
        { "BatteryStatus127508", { "127508", "BatteryInstance,BatteryVoltage,BatteryCurrent,BatteryTemperature,SID" } },
        { "ChargerStatus127507",
            { "127507", "Instance,BatteryInstance,ChargeState,ChargerMode,Enabled,EqualizationPending,EqualizationTimeRemaining" } },
        { "CogSog129026", { "129026", "SID,HeadingReference,COG,SOG" } },
        { "CogSogVTG", { "VTG", "TrueCOG,MagneticCOG,SOG" } },
        { "CrossTrack129283", { "129283", "SID,XTEMode,NavigationTerminated,XTE" } },
        { "DateTime129033", { "129033", "DaysSince1970,SecondsSinceMidnight,LocalOffset" } },
        { "DateTimeZDA", { "ZDA", "GPSTime,GPSDay,GPSMonth,GPSYear,LZD,LZMD" } },
        { "DcStatus127506", { "127506", "SID,DCInstance,DCType,StateOfCharge,StateOfHealth,TimeRemaining,RippleVoltage,Capacity" } },
        { "Depth128267", { "128267", "SID,DepthBelowTransducer,Offset,Range" } },
        { "EngineDynamic127489",
            { "127489",
                "EngineInstance,EngineOilPress,EngineOilTemp,EngineCoolantTemp,AltenatorVoltage,FuelRate,EngineHours,EngineCoolantPress,"
                "EngineFuelPress,EngineLoad,EngineTorque,Status1,Status2" } },
        { "EngineRapid127488", { "127488", "EngineInstance,EngineSpeed,EngineBoostPressure,EngineTiltTrim" } },
        { "FluidLevel127505", { "127505", "Instance,FluidType,Level,Capacity" } },
        { "GeneratorDateTime", { "-129033", "SID,DaysSince1970,SecondsSinceMidnight,LocalOffset" } },
        { "GeneratorEngine",
            { "-127489",
                "EngineInstance,EngineSpeed,EngineBoostPressure,EngineTiltTrim,EngineOilPress,EngineOilTemp,EngineCoolantTemp,"
                "AltenatorVoltage,FuelRate,EngineHours,EngineCoolantPress,EngineFuelPress,EngineLoad,EngineTorque,Status1,Status2" } },
        { "GeneratorTransLowerUnit", { "-127493", "EngineInstance,TransmissionGear,OilPressure,OilTemperature,DiscreteStatus1" } },
        { "GeneratorVesselMovement",
            { "-129025",
                "SID,Latitude,Longitude,Altitude,HeadingReference,Heading,Deviation,Variation,COG,SOG,RateOfTurn,Yaw,Pitch,Roll,Heave,"
                "RudderPosition" } },
        { "GeneratorWeather",
            { "-130311",
                "SID,AirTemperature,SeaTemperature,Humidity,AtmosphericPressure,WindSpeed,WindAngle,TemperatureSource,HumiditySource,"
                "Latitude,Longitude" } },
        { "GeographicPositionGLL", { "GLL", "GPSTime,Latitude,Longitude" } },
        { "GlobalPositioningGGA",
            { "GGA",
                "GPSTime,Latitude,Longitude,GPSQualityIndicator,SatelliteCount,HDOP,Altitude,GeoidalSeparation,DGPSAge,"
                "DGPSReferenceStationID" } },
        { "GnssPositionData129029",
            { "129029",
                "SID,DaysSince1970,SecondsSinceMidnight,Latitude,Longitude,Altitude,GNSStype,GNSSmethod,nSatellites,HDOP,PDOP,"
                "GeoidalSeparation,nReferenceStations,ReferenceStationType,ReferenceSationID,AgeOfCorrection" } },
        { "HeadingHDG", { "HDG", "Heading,Deviation,Variation" } },
        { "Heave127252", { "127252", "SID,Heave,Delay,DelaySource" } },
        { "LatLong129025", { "129025", "Latitude,Longitude" } },
        { "MagneticVariation127258", { "127258", "SID,Source,DaysSince1970,Variation" } },
        { "Navigation129284",
            { "129284",
                "SID,DistanceToWaypoint,BearingReference,PerpendicularCrossed,ArrivalCircleEntered,CalculationType,ETATime,ETADate,"
                "BearingOriginToDestinationWaypoint,BearingPositionToDestinationWaypoint,OriginWaypointNumber,DestinationWaypointNumber,"
                "DestinationLatitude,DestinationLongitude,WaypointClosingVelocity" } },
        { "WindlassOperating128777", { "128777", "SID,WindlassIdentifier,RodeCounterValue,WindlassLineSpeed,WindlassMotionStatus,RodeTypeStatus,AnchorDockingStatus,OperatingEvents" } },
        { "WindlassMonitoring128778", { "128778", "SID,WindlassIdentifier,TotalMotorTime,ControllerVoltage,MotorCurrent,MonitoringEvents" } },
        { "WindData130306", { "130306", "SID,WindSpeed,WindAngle,WindReference" } },
        { "AisClassBPosition129039", { "129039", "MessageID,Repeat,UserID,Latitude,Longitude,Accuracy,RAIM,Seconds,COG,SOG,AISInfo,Heading,Unit,Display,DSC,Band,Msg22,Mode,State,SID" } },
        { "AisClassBStatic129810", { "129810", "MessageID,Repeat,UserID,VesselType,Vendor,Callsign,Length,Beam,PosRefStbd,PosRefBow,MothershipID,AISInfo,SID" } },
        { "AisClassBStatic129809", { "129809", "MessageID,Repeat,UserID,Name,AISInfo,SID" } },
        { "Humidity130313", { "130313", "SID,HumidityInstance,HumiditySource,ActualHumidity,SetHumidity" } },
        { "OutsideEnvironmental130310", { "130310", "SID,OutsideAmbientAirTemperature,WaterTemperature,AtmosphericPressure" } },
        { "RateOfTurn127251", { "127251", "SID,RateOfTurn" } },
        { "RecommendedMinGnssRMC", { "RMC", "GPSTime,Status,Latitude,Longitude,TrueCOG,SOG,DaysSince1970,Variation,DateTime" } },
        { "Rudder127245", { "127245", "RudderPosition,Instance,RudderDirectionOrder,AngleOrder" } },
        { "Spacer000000", { "0", "Rows,ShowTitleRow" } },
        { "Speed128259", { "128259", "SID,WaterReferenced,GroundReferenced,SWRT" } },
        { "SystemTime126992", { "126992", "SID,SystemDate,SystemTime,TimeSource" } },
        { "TemperatureExtended130316", { "130316", "SID,TempInstance,TempSource,ActualTemperature,SetTemperature" } },
        { "TemperatureWidget130312", { "130312", "TemperatureInstance,SID,TemperatureSource,ActualTemperature,SetTemperature" } },
        { "TransmissionDynamic127493", { "127493", "EngineInstance,TransmissionGear,OilPressure,OilTemperature,DiscreteStatus1" } },
        { "TrueHeadingHDT", { "HDT", "Heading" } },
        { "VesselHeading127250", { "127250", "SID,Heading,Deviation,Variation,HeadingReference" } },
    };
    auto integer = [](const QJsonValue& value, int low, int high) {
        return value.isDouble() && std::isfinite(value.toDouble()) && std::floor(value.toDouble()) == value.toDouble()
            && value.toDouble() >= low && value.toDouble() <= high;
    };
    if (error) error->clear();
    QString context = "Layout";
    // Report structure and field names, never parameter contents, routes or credentials.
    auto fail = [&](const QString &reason) {
        if (error) *error = context + ": " + reason;
        return false;
    };
    auto displayName = [](QString text) {
        text.replace('\n', ' '); text.replace('\r', ' ');
        return text.left(100);
    };
    if (!integer(root["formatVersion"], 2, 2))
        return fail("This development Layout uses an unsupported format. Recreate it with this version (format 2).");
    if (root["documentType"] != "layout") return fail("Expected a concrete Layout document.");
    if (root.contains("setupPolicies") || root.contains("template")) return fail("Unresolved Template metadata in Layout.");
    if (root.contains("numberOfWidgetColumns") && !integer(root["numberOfWidgetColumns"], 1, 4))
        return fail("numberOfWidgetColumns must be 1–4");
    if (!root["layouts"].isArray()) return fail("layouts must be an array");
    const QStringList groups { "Generate", "Navigation", "Propulsion", "Environment", "Tanks", "Electrical", "Other", "Communication" };
    QSet<int> ids;
    QSet<QString> groupNames;
    QMap<QString, QString> generators;
    int count = 0;
    for (const auto& groupValue : root["layouts"].toArray()) {
        context = "Layout";
        if (!groupValue.isObject()) return fail("invalid group object");
        const auto group = groupValue.toObject();
        const auto name = group["groupName"].toString();
        context = "Group " + displayName(name);
        if (!groups.contains(name)) return fail("unknown groupName");
        if (groupNames.contains(name)) return fail("duplicate groupName");
        if (!group["widgets"].isArray()) return fail("widgets must be an array");
        groupNames.insert(name);
        for (const auto& widgetValue : group["widgets"].toArray()) {
            context = "Group " + displayName(name);
            if (++count > 512) return fail("more than 512 widgets");
            if (!widgetValue.isObject()) return fail("invalid widget object");
            const auto widget = widgetValue.toObject();
            const auto type = widget["type"].toString();
            context += QString(" / %1 [%2, id %3]").arg(displayName(widget["label"].toString()), displayName(type))
                           .arg(widget["idX"].toInt(-1));
            if (!definitions.contains(type)) return fail("unsupported widget type");
            if (definitions[type].pgn != widget["pgn"].toString()) return fail("pgn does not match widget type");
            if (!integer(widget["idX"], 0, 1000000)) return fail("invalid idX");
            if (ids.contains(widget["idX"].toInt())) return fail("duplicate idX");
            if (!widget["label"].isString()) return fail("label must be text");
            if (!integer(widget["interval"], 0, 3600000)) return fail("interval must be 0–3600000 ms");
            if (!integer(widget["rowSpan"], 1, 64)) return fail("rowSpan must be 1–64");
            if (!integer(widget["columnSpan"], 1, 12)) return fail("columnSpan must be 1–12");
            if (!widget["parameters"].isArray()) return fail("parameters must be an array");
            if (!widget["ranges"].isArray()) return fail("ranges must be an array");
            if ((name == "Generate") != widget["pgn"].toString().startsWith('-'))
                return fail("generator/group mismatch");
            if (type.startsWith("Generator") ? !widget["dispatch"].isBool() : widget.contains("dispatch"))
                return fail("generator dispatch must be explicitly boolean");
            if (!widget["enabled"].isBool()) return fail("enabled must be explicitly boolean");
            if (type=="GeneratorEngine" && !widget["engineProfile"].isString())
                return fail("engineProfile must be explicitly saved (empty means no profile)");
            if (widget.contains("electricalSimulation") && (!widget["electricalSimulation"].isBool()
                || (type != "BatteryStatus127508" && type != "ChargerStatus127507" && type != "DcStatus127506")))
                return fail("invalid electricalSimulation option");
            if (widget.contains("engineProfile") && !widget["engineProfile"].isString()) return fail("engineProfile must be text");
            ids.insert(widget["idX"].toInt());
            QMap<QString, QString> parameters;
            for (const auto& rowValue : widget["parameters"].toArray()) {
                const auto row = rowValue.toArray();
                if (row.size() != 4) return fail("parameter row must contain four fields");
                for (const auto& v : row) if (!v.isString()) return fail("parameter row fields must be text");
                auto field = row[0].toString();
                if (widget["pgn"].toString().endsWith("127493")) {
                    if (field == "TransmissionInstance") field = "EngineInstance";
                    if (field == "TransmissionOilPressure") field = "OilPressure";
                    if (field == "TransmissionOilTemperature") field = "OilTemperature";
                }
                if (field.isEmpty()) return fail("empty parameter name");
                if (parameters.contains(field)) return fail("duplicate parameter " + displayName(field));
                parameters.insert(field, row[1].toString());
                if (type == "GeographicPositionGLL" && field == "Status"
                    && row[1].toString() != "A" && row[1].toString() != "V") return fail("invalid GLL validity");
                if(Expansion::definition(widget["pgn"].toString()) && !Expansion::validField(widget["pgn"].toString(),field,row[1].toString()))return fail("invalid instrument parameter "+displayName(field));
                if (WindlassStatus::supported(widget["pgn"].toString().toInt()) && !WindlassStatus::validField(widget["pgn"].toString().toInt(),field,row[1].toString()))return fail("invalid windlass parameter " + displayName(field));
                if (type=="AisClassBPosition129039" && !AisClassBPosition129039::validField(field,row[1].toString()))return fail("invalid AIS Position parameter " + displayName(field));
                if (type=="AisClassBStatic129810" && !AisClassBStatic129810::validField(field,row[1].toString()))
                    return fail("invalid AIS Part B parameter " + displayName(field));
                if (type == "AisClassBStatic129809" && !AisClassBStatic129809::validField(field, row[1].toString()))
                    return fail("invalid AIS parameter " + displayName(field));
                if (!( !Expansion::choices(row[3].toString()).isEmpty() && Expansion::choices(row[3].toString()).first().first.metaType().id()==QMetaType::QString) && row[3].toString() != "char" && row[3].toString() != "bool" && row[3].toString() != "QString" && row[3].toString() != "RTE route type" && row[3].toString() != "GSA selection" && row[3].toString() != "tNMEA0183WindReference" && row[3].toString() != "char (A/V)") {
                    bool ok;
                    const double number = row[1].toString().toDouble(&ok);
                    if (!ok || !std::isfinite(number))
                        return fail("parameter " + displayName(field) + " must contain a finite numeric value");
                }
            }
            for (const auto& field : definitions[type].fields.split(',', Qt::SkipEmptyParts))
                if (!parameters.contains(field)) return fail("missing parameter " + field);
            int rangeIndex = 0;
            for (const auto& rangeValue : widget["ranges"].toArray()) {
                const auto range = rangeValue.toArray();
                if (range.size() != 2 || !range[0].isDouble() || !range[1].isDouble() || !std::isfinite(range[0].toDouble())
                    || !std::isfinite(range[1].toDouble()) || range[0].toDouble() > range[1].toDouble())
                    return fail(QString("invalid range at index %1").arg(rangeIndex));
                ++rangeIndex;
            }
            if (type.startsWith("Generator")) {
                QString key = type;
                QString identity = type;
                if (type == "GeneratorEngine" || type == "GeneratorTransLowerUnit") {
                    bool ok;
                    const int instance = parameters.value("EngineInstance").toInt(&ok);
                    if (!ok || instance < 0 || instance > 7) return fail("EngineInstance must be 0–7");
                    key += QString::number(instance);
                    identity += QString(" EngineInstance=%1").arg(instance);
                }
                if (generators.contains(key)) return fail("duplicate generator " + identity + "; conflicts with " + generators.value(key));
                generators.insert(key, context);
            }
            if (widget.contains("configuration")) {
                if (!widget["configuration"].isObject()) return fail("configuration must be an object");
                const auto c = widget["configuration"].toObject();
                if (type == "GeneratorWeather") {
                    if (!GeneratorWeather::validLayoutConfiguration(c)) return fail("invalid Weather configuration (refresh/observation metadata)");
                } else if (type == "GeneratorVesselMovement") {
                    if (!c["route"].isString() || c["route"].toString().size() > 4096) return fail("invalid configuration.route");
                    if (!c["reverse"].isBool()) return fail("configuration.reverse must be boolean");
                    if (!integer(c["speedSource"], 0, 2)) return fail("configuration.speedSource must be 0–2");
                    if (c.contains("autopilotMultiplier") && !integer(c["autopilotMultiplier"], 1, 10))
                        return fail("configuration.autopilotMultiplier must be an integer from 1 to 10");
                    if (!c["speed"].isDouble() || !std::isfinite(c["speed"].toDouble())
                        || c["speed"].toDouble() < 0 || c["speed"].toDouble() > 1000) return fail("invalid configuration.speed");
                } else return fail("configuration is not supported for this widget type");
            }
        }
    }
    return true;
}

QList<QWidget*> MainController::orderedWidgets(const QString& group) const
{
    auto widgets = groupWidgetsMap.value(group);
    const auto grid = subFormLayoutMap.value(group).gridLayout;
    auto position = [grid](QWidget* widget) {
        int r = 0, c = 0, rs = 0, cs = 0;
        if (grid)
            grid->getItemPosition(grid->indexOf(widget->parentWidget()), &r, &c, &rs, &cs);
        return qMakePair(r, c);
    };
    std::stable_sort(widgets.begin(), widgets.end(), [&](auto* a, auto* b) { return position(a) < position(b); });
    return widgets;
}
void MainController::placeWidgetFrame(QWidget* widget, QWidget* frame, SubFormLayoutState& state)
{
    const int width = qMax(1, widget->property("ColumnSpan").toInt());
    const int columns = qMax(width, numberOfWidgetWide * numberColumnsWidgetSpans);
    const int height = qMax(1, widget->property("RowSpan").toInt());
    for (;;) {
        if (state.lastColumn + width > columns) {
            state.lastColumn = 0;
            ++state.lastRow;
        }
        bool free = true;
        for (int r = state.lastRow; r < state.lastRow + height; ++r)
            for (int c = state.lastColumn; c < state.lastColumn + width; ++c)
                if (state.occupiedCells.contains({ r, c }))
                    free = false;
        if (free)
            break;
        ++state.lastColumn;
    }
    state.gridLayout->addWidget(frame, state.lastRow, state.lastColumn, height, width);
    for (int r = state.lastRow; r < state.lastRow + height; ++r)
        for (int c = state.lastColumn; c < state.lastColumn + width; ++c)
            state.occupiedCells.insert({ r, c });
    state.lastColumn += width;
}

void MainController::reflowLayouts()
{
    for (const auto& group : subFormLayoutMap.keys()) {
        const auto widgets = orderedWidgets(group);
        auto& state = subFormLayoutMap[group];
        for (auto* widget : widgets)
            state.gridLayout->removeWidget(widget->parentWidget());
        state.lastRow = 1;
        state.lastColumn = 0;
        state.occupiedCells.clear();
        for (auto* widget : widgets)
            placeWidgetFrame(widget, widget->parentWidget(), state);
        updateGridPresentation(state);
    }
    emit layoutObjectsChanged();
}
void MainController::updateGridPresentation(SubFormLayoutState &state)
{
    auto *grid=state.gridLayout;
    int end=0;
    for(int i=0;i<grid->count();++i) {
        int row,column,rows,columns;grid->getItemPosition(i,&row,&column,&rows,&columns);
        end=qMax(end,column+columns);
    }
    // Stretch logical span units equally. Empty tail columns must not reserve
    // width; explicit holes inside a saved placement retain their identity.
    for(int i=0;i<grid->columnCount();++i) {
        grid->setColumnMinimumWidth(i,0);
        grid->setColumnStretch(i,i<end?1:0);
    }
    grid->invalidate();
}
void MainController::updateWidgetSpan(QWidget *widget)
{
    if(m_restoringLayout)return;
    for(const auto &group:subFormLayoutMap.keys()) {
        auto &state=subFormLayoutMap[group];auto *grid=state.gridLayout;
        if(grid->indexOf(widget->parentWidget())<0)continue;
        int oldRow,oldColumn,oldRows,oldColumns;
        grid->getItemPosition(grid->indexOf(widget->parentWidget()),&oldRow,&oldColumn,&oldRows,&oldColumns);
        if(oldRows==widget->property("RowSpan").toInt() && oldColumns==widget->property("ColumnSpan").toInt())return;
        const auto widgets=orderedWidgets(group);
        struct Position { QWidget *widget;int row,column,rows,columns; };
        QList<Position> positions;
        for(auto *item:widgets) {
            int r,c,rs,cs;grid->getItemPosition(grid->indexOf(item->parentWidget()),&r,&c,&rs,&cs);
            positions.append({item,r,c,rs,cs});
        }
        // An explicit span edit reserves its anchor first. Only objects whose
        // old cells now conflict need automatic placement; resizing never calls this.
        std::stable_sort(positions.begin(),positions.end(),[widget](const auto&a,const auto&b){return a.widget==widget && b.widget!=widget;});
        state.occupiedCells.clear();state.lastRow=1;state.lastColumn=0;
        QList<QWidget*> fallback;
        for(auto p:positions) {
            grid->removeWidget(p.widget->parentWidget());
            if(p.widget==widget){p.rows=qMax(1,widget->property("RowSpan").toInt());p.columns=qMax(1,widget->property("ColumnSpan").toInt());}
            bool free=true;
            for(int r=p.row;r<p.row+p.rows;++r)for(int c=p.column;c<p.column+p.columns;++c)
                if(state.occupiedCells.contains({r,c}))free=false;
            if(!free){fallback.append(p.widget);continue;}
            grid->addWidget(p.widget->parentWidget(),p.row,p.column,p.rows,p.columns);
            for(int r=p.row;r<p.row+p.rows;++r)for(int c=p.column;c<p.column+p.columns;++c)state.occupiedCells.insert({r,c});
        }
        for(auto *item:fallback)placeWidgetFrame(item,item->parentWidget(),state);
        updateGridPresentation(state);
        emit layoutObjectsChanged();return;
    }
}
namespace {
QJsonObject loggingWidget(QObject *object) {
    QJsonObject value{{"type",object?object->metaObject()->className():"destroyed"}};
    if(auto *widget=qobject_cast<PgnWidgetBase*>(object)) {
        value["widget_id"]=widget->Id();value["pgn"]=widget->property("Pgn").toString();
        for(const auto *key:{"EngineInstance","Instance","SystemInstance"})if(widget->property(key).isValid())value[key]=QJsonValue::fromVariant(widget->property(key));
    }
    return value;
}
}
void MainController::trackGeneratorConnection(const QMetaObject::Connection &connection,QObject *source,QObject *destination,const QString &signal,const QString &slot) {
    m_generatorConnections.append(connection);
    QJsonObject fields{{"event",connection?"connected":"connection-failed"},{"connection_id",QString::number(++m_connectionNumber)},{"binding_epoch",QString::number(m_bindingEpoch)},{"source",loggingWidget(source)},{"destination",loggingWidget(destination)},{"signal",signal},{"slot_or_property",slot}};
    m_bindingDiagnostics.append({fields,source,destination,false});ApplicationLog::binding(fields);
}
void MainController::bindingSnapshot(){
    for(const auto &record:m_bindingDiagnostics)if(!record.ended){auto fields=record.fields;fields["event"]="existing-binding-snapshot";ApplicationLog::binding(fields);}
}
void MainController::bindingObjectDestroyed(QObject *object){
    for(auto &record:m_bindingDiagnostics)if(!record.ended && (record.source==object||record.destination==object)) {
        auto fields=record.fields;fields["event"]="auto-disconnected";fields["reason"]="QObject destruction";ApplicationLog::binding(fields);record.ended=true;
    }
}
MainController::~MainController(){invalidateGeneratorBindings("controller shutdown");}
void MainController::invalidateGeneratorBindings(const QString &reason)
{
    ApplicationLog::binding({{"event","binding-invalidation"},{"reason",reason},{"binding_epoch",QString::number(m_bindingEpoch)},{"connections",m_generatorConnections.size()}});
    ++m_bindingEpoch;
    for (int i=0;i<m_generatorConnections.size();++i) {
        const bool removed=QObject::disconnect(m_generatorConnections[i]);
        if(i<m_bindingDiagnostics.size()&&!m_bindingDiagnostics[i].ended){auto fields=m_bindingDiagnostics[i].fields;fields["event"]=removed?"disconnected":"already-ended";fields["reason"]=reason;ApplicationLog::binding(fields);}
    }
    m_generatorConnections.clear();m_bindingDiagnostics.clear();
}
void MainController::prepareWidget(QWidget* widget)
{
    auto* base = qobject_cast<PgnWidgetBase*>(widget);
    if (!base)
        return;
    base->initializeGeneratorControls();
    auto sync = [this, base] {
        pgnTableController->setPgnEnabled(base->Id(), base->PgnWidgetBase::isEnabled());
        if (auto* check = base->findChild<QCheckBox*>("checkBoxEnabled")) {
            QSignalBlocker block(check);
            check->setChecked(base->PgnWidgetBase::isEnabled());
        }
    };
    sync();
    if (base->property("LayoutPrepared").toBool())
        return;
    base->setProperty("LayoutPrepared", true);
    connect(base,&QObject::destroyed,this,[this,base]{bindingObjectDestroyed(base);});
    noteLayoutStructureChange();
    auto spanChanged=[this,base]{updateWidgetSpan(base);};
    connect(base,&PgnWidgetBase::ColumnSpanChanged,this,spanChanged);
    connect(base,&PgnWidgetBase::RowSpanChanged,this,spanChanged);
    updateWidgetSpan(base);
    // Selection is explicit and shared with Object Tree navigation. Do not queue
    // focus ownership: an older notification must not override a newer click.
    connect(base, &PgnWidgetBase::WidgetHasFocus, this, [this, base] {
        auto* focus = QApplication::focusWidget();
        if (focus && (focus == base || base->isAncestorOf(focus))) selectLayoutWidget(base);
    });
    m_electrical->observe(base);
    if (auto* engine = qobject_cast<GeneratorEngine*>(base))
        connect(engine, &GeneratorEngine::fuelConsumed, this, &MainController::consumeTankFuel);
    connect(base, &PgnWidgetBase::enabledChanged, base, sync);
    connect(base, &PgnWidgetBase::dispatchChanged, this, [this] { noteLayoutStructureChange(); });
    if(auto *movement=qobject_cast<GeneratorVesselMovement*>(base)) {
        auto release=[this,movement] {
            if(movement->deliveryAllowed())return; // Re-enable waits for a normal sample.
            for(const auto &group:groupWidgetsMap)for(auto *widget:group)
                if(auto *position=qobject_cast<AisClassBPosition129039*>(widget))position->setNavigationControlled(false);
        };
        connect(base,&PgnWidgetBase::enabledChanged,this,release);
        connect(base,&PgnWidgetBase::dispatchChanged,this,release);
    }
    auto changed = [this] {
        if (!m_restoringLayout && !m_rebindingOnly)
            rebuildGeneratorBindings();
    };
    if (auto* w = qobject_cast<GeneratorEngine*>(base))
        connect(w, &GeneratorEngine::EngineInstanceChanged, this, changed);
    if (auto* w = qobject_cast<GeneratorTransLowerUnit*>(base))
        connect(w, &GeneratorTransLowerUnit::EngineInstanceChanged, this, changed);
    if (auto* w = qobject_cast<EngineRapid127488*>(base))
        connect(w, &EngineRapid127488::EngineInstanceChanged, this, changed);
    if (auto* w = qobject_cast<EngineDynamic127489*>(base))
        connect(w, &EngineDynamic127489::EngineInstanceChanged, this, changed);
    if (auto* w = qobject_cast<TransmissionDynamic127493*>(base))
        connect(w, &TransmissionDynamic127493::EngineInstanceChanged, this, changed);
    const int registeredId = base->Id();
    connect(base, &QObject::destroyed, this, [this, base, registeredId] {
        m_suspendedTimers.remove(base);
        for (auto& group : groupWidgetsMap) group.removeAll(base);
        pgnTableController->getPgnTableModel()->removePgn(registeredId);
        noteLayoutStructureChange();
        emit layoutObjectsChanged();
    });
}

void MainController::consumeTankFuel(int engineInstance, double liters)
{
    if (m_mode != Sender || m_restoringLayout || (qobject_cast<PgnWidgetBase*>(sender()) && !qobject_cast<PgnWidgetBase*>(sender())->deliveryAllowed()) || engineInstance < 0 || engineInstance > 1
        || !std::isfinite(liters) || liters <= 0) return;
    // Reuse the established engine-instance association if duplicate generators
    // are configured; duplicate measurement cards must not multiply consumption.
    if (auto* engine = qobject_cast<GeneratorEngine*>(sender());
        engine && generatorEngines.value(engineInstance).first != engine) return;
    auto* model = pgnTableController->getPgnTableModel();
    for (const auto& group : groupWidgetsMap) for (auto* widget : group) {
        auto* tank = qobject_cast<FluidLevel127505*>(widget);
        if (!tank) continue;
        const auto* data = model->findPgn(tank->Id());
        if (!data) continue;
        QMap<QString, double> fields;
        for (const auto& row : data->paramDetails) {
            bool valid = false; const double value = row[1].toDouble(&valid);
            if (valid && std::isfinite(value) && value != N2kDoubleNA) fields.insert(row[0], value);
        }
        if (fields.value("Instance", -1) != engineInstance
            || (fields.value("FluidType", -1) != N2kft_Fuel && fields.value("FluidType", -1) != N2kft_FuelGasoline)
            || !fields.contains("Level") || !fields.contains("Capacity") || fields["Capacity"] <= 0) continue;
        const double capacity = fields["Capacity"];
        const double quantity = capacity * qBound(0.0, fields["Level"], 100.0) / 100.0;
        const double level = qMax(0.0, quantity - liters) / capacity * 100.0;
        // The existing model remains the only tank state, including manual edits.
        model->changeParam(tank->Id(), "Level", QString::number(level, 'g', 17));
        const QSignalBlocker display(tank);
        tank->setLevel(level);
    }
}
void MainController::rebuildGeneratorBindings()
{
    if (m_restoringLayout || m_rebindingOnly)
        return;
    ApplicationLog::binding({{"event","binding-rebuild-begin"}});
    invalidateGeneratorBindings("binding rebuild");
    m_rebindingOnly = true;
    const auto done = qScopeGuard([this] { m_rebindingOnly = false; ApplicationLog::binding({{"event","binding-rebuild-end"},{"connections",m_generatorConnections.size()},{"binding_epoch",QString::number(m_bindingEpoch)}}); });
    generatorEngines.clear();
    generatorTransLowerUnits.clear();
    engineRapids.clear();
    engineDynamics.clear();
    transmissionDynamics.clear();
    for (const auto& group : groupWidgetsMap)
        for (auto* widget : group) {
            if (auto* w = qobject_cast<GeneratorEngine*>(widget))
                generatorEngines.insert(w->EngineInstance(), { w, false });
            if (auto* w = qobject_cast<GeneratorTransLowerUnit*>(widget))
                generatorTransLowerUnits.insert(w->EngineInstance(), { w, false });
        }
    QPointer<GeneratorWeather> weather;
    for(const auto &group:groupWidgetsMap) for(auto *widget:group)
        if(auto *candidate=qobject_cast<GeneratorWeather*>(widget);candidate && (!weather || candidate->Id()<weather->Id())) weather=candidate;
    auto *model=pgnTableController->getPgnTableModel();
    for(const auto &group:groupWidgetsMap) for(auto *widget:group) if(auto *movement=qobject_cast<GeneratorVesselMovement*>(widget)) {
        movement->setEnvironmentalSource([this,weather,model] {
            EnvironmentalConditions c;
            if(m_mode!=Sender || m_restoringLayout || !weather || !weather->deliveryAllowed())return c;
            const auto *data=model->findPgn(weather->Id());if(!data)return c;
            auto measurement=[&](const QString &field,double minimum,double maximum,double &out) {
                const auto origin=weather->fieldOrigin(field);
                if(!origin.provider.isEmpty() && !origin.fresh())return false;
                for(const auto &row:data->paramDetails) if(row[0]==field) {
                    bool ok=false;out=row[1].toDouble(&ok);
                    return ok && std::isfinite(out) && out!=N2kDoubleNA && out>=minimum && out<=maximum;
                }
                return false;
            };
            c.windValid=measurement("WindSpeed",0,150,c.windSpeed) && measurement("WindAngle",0,2*M_PI,c.windFrom);
            c.wavesValid=measurement("WaveHeight",0,100,c.waveHeight) && measurement("MeanWaveDirection",0,2*M_PI,c.waveFrom);
            if(!measurement("DominantWavePeriod",0.1,120,c.wavePeriod))
                c.wavesValid=measurement("AverageWavePeriod",0.1,120,c.wavePeriod) && c.wavesValid;
            return c;
        });
        // These are inspection updates only; simulation reads the current model on its own existing tick.
        trackGeneratorConnection(connect(model,&PgnTableModel::valuesChanged,movement,[movement,weather](int id) {
            if(weather && id==weather->Id())movement->refreshEnvironmentalStatus();
        }),model,movement,"valuesChanged","refreshEnvironmentalStatus");
        if(weather) {
            trackGeneratorConnection(connect(weather,&PgnWidgetBase::dispatchChanged,movement,&GeneratorVesselMovement::refreshEnvironmentalStatus),weather,movement,"dispatchChanged","refreshEnvironmentalStatus");
            trackGeneratorConnection(connect(weather,&GeneratorWeather::ObservationMetadataChanged,movement,&GeneratorVesselMovement::refreshEnvironmentalStatus),weather,movement,"ObservationMetadataChanged","refreshEnvironmentalStatus");
            trackGeneratorConnection(connect(weather,&QObject::destroyed,movement,&GeneratorVesselMovement::refreshEnvironmentalStatus),weather,movement,"destroyed","refreshEnvironmentalStatus");
        }
        movement->refreshEnvironmentalStatus();
    }
    bindAisVesselMovement();
    if (m_mode != Sender)
        return;
    static const QMap<QString, int> sentences { { "GGA", SNT_GGA }, { "RMC", SNT_RMC }, { "HDG", SNT_HDG }, { "HDT", SNT_HDT },
        { "GLL", SNT_GLL }, { "VTG", SNT_VTG }, { "ZDA", SNT_ZDA } };
    for (const auto& group : groupWidgetsMap)
        for (auto* widget : group) {
            const auto data = pgnTableController->getPgnTableModel()->getPgnDataToSend(widget->property("Id").toInt());
            bindWidgetToModel(widget, Expansion::definition(data.pgn)?Expansion::definition(data.pgn)->id:sentences.value(data.pgn, data.pgn.toInt()), pgnTableController->getPgnTableModel(), data.idX);
            int instance = -1;
            if (auto* w = qobject_cast<EngineRapid127488*>(widget)) {
                instance = w->EngineInstance();
                engineRapids.insert(instance, { w, false });
            }
            if (auto* w = qobject_cast<EngineDynamic127489*>(widget)) {
                instance = w->EngineInstance();
                engineDynamics.insert(instance, { w, false });
            }
            if (auto* w = qobject_cast<TransmissionDynamic127493*>(widget)) {
                instance = w->EngineInstance();
                transmissionDynamics.insert(instance, { w, false });
            }
            if (instance >= 0 && (generatorEngines.contains(instance) || generatorTransLowerUnits.contains(instance)))
                bindWidgetsBasedOnEngineInstanceToGenerator(instance);
        }
    // Engine/transmission generators must also work before measurement cards exist.
    auto instances = generatorEngines.keys();
    // Lower-unit sources also drive instruments independently of engine sources.
    for (int instance : generatorTransLowerUnits.keys())
        if (!instances.contains(instance)) instances.append(instance);
    for (int instance : instances)
        bindWidgetsBasedOnEngineInstanceToGenerator(instance);
    if (generatorVesselMovementWidget) {
        for (int instance : instances)
            bindGeneratorVesselMovementToGeneratorEngine(generatorVesselMovementWidget, instance);
    }
    m_electrical->synchronize();
}
void MainController::bindAisVesselMovement()
{
    bool hasPosition=false;
    for(const auto &group:groupWidgetsMap)for(auto *widget:group)
        if(qobject_cast<AisClassBPosition129039*>(widget))hasPosition=true;
    if(!hasPosition)return; // Existing non-AIS Layouts need no AIS signal work.
    // Reuse the existing epoch/lifetime guards; no AIS timer or motion state.
    for(const auto &group:groupWidgetsMap)for(auto *widget:group) {
        if(auto *movement=qobject_cast<GeneratorVesselMovement*>(widget)) {
            auto bindField=[this,movement](auto signal,const QStringList &fields) {
                // Observe on the generator's GUI-thread signal, so a later clock
                // tick cannot retimestamp a delayed copy of an earlier position.
                // Sender scheduling shares this thread and never dispatches here.
                connectWhileSenderAlive(movement,signal,this,[this,fields]{updateAisVesselMovement(fields);}, Qt::AutoConnection, "updateAisVesselMovement");
            };
            bindField(&GeneratorVesselMovement::LatitudeChanged,{"Latitude"});
            bindField(&GeneratorVesselMovement::LongitudeChanged,{"Longitude"});
            bindField(&GeneratorVesselMovement::COGChanged,{"COG"});
            bindField(&GeneratorVesselMovement::SOGChanged,{"SOG"});
            bindField(&GeneratorVesselMovement::HeadingChanged,{"Heading"});
            bindField(&GeneratorVesselMovement::HeadingReferenceChanged,{"COG","Heading"});
            bindField(&GeneratorVesselMovement::VariationChanged,{"COG","Heading"});
            bindField(&GeneratorVesselMovement::DeviationChanged,{"Heading"});
            connectWhileSenderAlive(movement,&PgnWidgetBase::parametersSampled,this,
                [this](const QStringList &fields){updateAisVesselMovement(fields);}, Qt::AutoConnection, "updateAisVesselMovement");
            connectWhileSenderAlive(movement,&GeneratorVesselMovement::navigationControlChanged,this,[this]{updateAisVesselMovement();}, Qt::AutoConnection, "updateAisVesselMovement");
            connectWhileSenderAlive(movement,&PgnWidgetBase::enabledChanged,this,[this]{updateAisVesselMovement();}, Qt::AutoConnection, "updateAisVesselMovement");
        }
        if(auto *position=qobject_cast<AisClassBPosition129039*>(widget))
            connectWhileSenderAlive(position,&AisClassBPosition129039::parameterValueChanged,this,
                [this](const QString &field,const QString &){if(field=="UserID")updateAisVesselMovement();}, Qt::AutoConnection, "updateAisVesselMovement");
    }
    auto *model=pgnTableController->getPgnTableModel();
    connectWhileSenderAlive(model,&PgnTableModel::parameterChanged,this,
        [this,model](int id,const QString &field,const QVariant &){
            const auto *pgn=model->findPgn(id);
            if(field=="UserID" && pgn && pgn->pgn=="129039")updateAisVesselMovement();
        }, Qt::AutoConnection, "updateAisVesselMovement");
    updateAisVesselMovement();
}

void MainController::updateAisVesselMovement(const QStringList &fields)
{
    if(m_restoringLayout)return;
    // Layout validation permits one Vessel Movement generator. Lowest id is a
    // deterministic fallback for transient programmatic creation before validation.
    GeneratorVesselMovement *movement=nullptr;
    GeneratorDateTime *clock=nullptr;
    for(const auto &group:groupWidgetsMap)for(auto *widget:group)
        if(auto *candidate=qobject_cast<GeneratorVesselMovement*>(widget);candidate && (!movement||candidate->Id()<movement->Id()))movement=candidate;
    for(const auto &group:groupWidgetsMap)for(auto *widget:group)
        if(auto *candidate=qobject_cast<GeneratorDateTime*>(widget);candidate && (!clock||candidate->Id()<clock->Id()))clock=candidate;
    auto *model=pgnTableController->getPgnTableModel();
    const bool active=m_mode==Sender && movement && movement->deliveryAllowed() && movement->navigationControlActive()
        && myVessel.MMSI>=100000000 && myVessel.MMSI<=999999999;
    for(const auto &group:groupWidgetsMap)for(auto *widget:group) {
        auto *position=qobject_cast<AisClassBPosition129039*>(widget);if(!position)continue;
        const auto *data=model->findPgn(position->Id());
        const bool owns=active && data && data->source.isEmpty() && position->UserID()==myVessel.MMSI;
        const bool newlyControlled=owns && !position->navigationControlled();
        position->setNavigationControlled(owns);
        if(!owns)continue;
        const auto selected=newlyControlled ? QStringList{"Latitude","Longitude","COG","SOG","Heading"} : fields;
        QStringList sample;
        QDateTime observation;
        for(const auto &field:selected) {
            if(!AisClassBPosition129039::measurement(field))continue;
            double value=field=="Latitude"?movement->Latitude():field=="Longitude"?movement->Longitude():
                field=="SOG"?movement->SOG():field=="COG"?movement->COG():movement->Heading();
            if((field=="COG"||field=="Heading") && value!=N2kDoubleNA) {
                if(movement->HeadingReference()==N2khr_magnetic) {
                    const double correction=movement->Variation()+(field=="Heading"?movement->Deviation():0.0);
                    if(!std::isfinite(correction)||movement->Variation()==N2kDoubleNA || (field=="Heading"&&movement->Deviation()==N2kDoubleNA))value=N2kDoubleNA;
                    else value=Convert::DegreesToRadians(Convert::NormalizeHeading(Convert::MagneticToTrue(Convert::RadiansToDegrees(value),Convert::RadiansToDegrees(correction))));
                } else if(movement->HeadingReference()!=N2khr_true)value=N2kDoubleNA;
            }
            QString canonical=QString::number(value,'g',17);
            if(!AisClassBPosition129039::validField(field,canonical))canonical=QString::number(N2kDoubleNA,'g',17);
            sample<<field<<canonical;
        }
        if(selected.contains("Latitude") || selected.contains("Longitude")) {
            // Date/Time is the existing UTC simulation clock, not a transmission timestamp.
            // Restored/initialized clocks have unknown update metadata until a real sample.
            const auto updated=clock ? model->parameterUpdatedAt(clock->Id(),"SecondsSinceMidnight") : QDateTime();
            const auto age=updated.isValid() ? updated.msecsTo(QDateTime::currentDateTimeUtc()) : -1;
            const double seconds=clock ? clock->SecondsSinceMidnight() : N2kDoubleNA;
            const bool validClock=clock && clock->deliveryAllowed() && clock->RefreshRate()>0 && clock->DaysSince1970()<=65533
                && std::isfinite(seconds) && seconds>=0 && seconds<86400 && age>=0
                && age<=qMax(qint64(2000),qint64(clock->RefreshRate())*2);
            position->setObservationTimeControlled(validClock);
            if(validClock) {
                sample<<"Seconds"<<QString::number(int(std::floor(seconds))%60);
                observation=QDateTime::fromMSecsSinceEpoch(qint64(clock->DaysSince1970())*86400000+qRound64(seconds*1000),Qt::UTC);
            }
            else if(position->Seconds()<60)sample<<"Seconds"<<"60"; // Do not reuse an old numeric observation time.
            // Existing unavailable/manual/dead-reckoning/inoperative codes remain explicit.
        }
        // One authoritative model; normal bindings refresh widget/tree/editor.
        // Sending snapshots and interval scheduling remain entirely unchanged.
        if(!sample.isEmpty()) {
            model->changeParams(position->Id(),sample);
            if(observation.isValid())
                for(const auto &field:{QString("Latitude"),QString("Longitude")})
                    if(selected.contains(field))model->setParameterObservationTime(position->Id(),field,observation);
        }
    }
}

void MainController::suspendGenerators(bool viewer)
{
    m_electrical->setGenerate(!viewer);
    for (const auto& group : groupWidgetsMap)
        for (auto* widget : group) {
            if (!QString(widget->metaObject()->className()).startsWith("Generator"))
                continue;
            widget->setProperty("GeneratorModeActive", !viewer);
            if (viewer) {
                if (auto* w = qobject_cast<GeneratorVesselMovement*>(widget))
                    w->stopOperationalActivity();
                if (auto* w = qobject_cast<GeneratorEngine*>(widget))
                    w->setEngineRunning(false);
                if (auto* w = qobject_cast<GeneratorTransLowerUnit*>(widget))
                    w->setEngineRunning(false);
                if (!m_suspendedTimers.contains(widget)) {
                    QList<QPair<QPointer<QTimer>, int>> timers;
                    for (auto* timer : widget->findChildren<QTimer*>())
                        if (timer->thread() == thread() && timer->isActive()) {
                            timers.append({ timer, timer->interval() });
                            timer->stop();
                        }
                    m_suspendedTimers.insert(widget, timers);
                }
            } else {
                const bool resumeClock = qobject_cast<GeneratorDateTime*>(widget) || qobject_cast<GeneratorVesselMovement*>(widget);
                for (const auto& saved : m_suspendedTimers.take(widget))
                    if (resumeClock && saved.first)
                        saved.first->start(saved.second);
            }
            if(auto *movement=qobject_cast<GeneratorVesselMovement*>(widget))movement->refreshEnvironmentalStatus();
            widget->QWidget::setEnabled(!viewer); // UI activity is separate from the saved sending Enable property.
        }
}
