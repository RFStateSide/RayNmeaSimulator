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

#include "signalkpublisher.h"
#include "N2kMessages.h"
#include <QDateTime>
#include <QJsonObject>
#include <cmath>

namespace {
bool available(double n) { return std::isfinite(n) && !N2kIsNA(n); }
QString instance(const QString &root, unsigned char id) { return root + "." + QString::number(id) + "."; }
QString temperaturePath(tN2kTempSource source, unsigned char id) {
    // Per-probe identifiers retain independent environmental sensor identities.
    const QString probe = "sensor" + QString::number(id);
    switch (int(source)) {
    case 0: return "environment.water.temperature";
    case 1: return "environment.outside.temperature";
    case 2: return "environment.inside." + probe + ".temperature";
    case 3: return "environment.inside.engineRoom" + QString::number(id) + ".temperature";
    case 4: return "environment.inside.mainCabin" + QString::number(id) + ".temperature";
    case 5: return "tanks.liveWell." + QString::number(id) + ".temperature";
    case 6: return {}; // No bait-well temperature path in the supported schema.
    case 7: return "environment.inside.refrigerator" + QString::number(id) + ".temperature";
    case 8: return "environment.inside.heating" + QString::number(id) + ".temperature";
    case 9: return "environment.outside.dewPointTemperature";
    case 10: return "environment.outside.apparentWindChillTemperature";
    case 11: return "environment.outside.theoreticalWindChillTemperature";
    case 12: return "environment.outside.heatIndexTemperature";
    case 13: return "environment.inside.freezer" + QString::number(id) + ".temperature";
    case 14: return "propulsion." + QString::number(id) + ".exhaustTemperature";
    default: return {};
    }
}
}

QJsonArray SignalKPublisher::values(const tN2kMsg &N2kMsg)
{
    QJsonArray result;
    auto add = [&](const QString &path, const QJsonValue &value) { if (!path.isEmpty()) result.append(QJsonObject{{"path", path}, {"value", value}}); };
    auto number = [&](const QString &path, double value, double scale = 1.0) { if (available(value)) add(path, value * scale); };
    auto environmental = [&](const QString &path, double v, double scale = 1.) {
        add(path, available(v) ? QJsonValue(v * scale) : QJsonValue(QJsonValue::Null));
    };
    auto position = [&](const QString &path, double latitude, double longitude) {
        if (available(latitude) && available(longitude) && qAbs(latitude) <= 90 && qAbs(longitude) <= 180)
            add(path, QJsonObject{{"latitude", latitude}, {"longitude", longitude}});
    };
    auto date = [&](uint16_t days, double seconds) {
        if (days != N2kUInt16NA && available(seconds) && seconds >= 0 && seconds < 86400)
            add("navigation.datetime", QDateTime::fromMSecsSinceEpoch(qint64(days)*86400000 + qRound64(seconds*1000), Qt::UTC).toString(Qt::ISODateWithMs));
    };
    switch (N2kMsg.PGN) {
    case 126992: {
        unsigned char SID;
        uint16_t SystemDate;
        double SystemTime;
        tN2kTimeSource TimeSource;
        if (!ParseN2kPGN126992(N2kMsg, SID, SystemDate, SystemTime, TimeSource)) break;
        date(SystemDate, SystemTime);
        break;
    }
    case 127245: {
        double RudderPosition;
        unsigned char Instance;
        tN2kRudderDirectionOrder RudderDirectionOrder;
        double AngleOrder;
        if (!ParseN2kPGN127245(N2kMsg, RudderPosition, Instance, RudderDirectionOrder, AngleOrder)) break;
        if (Instance == 0) number("steering.rudderAngle", RudderPosition);
        break;
    }
    case 127250: {
        unsigned char SID;
        double Heading;
        double Deviation;
        double Variation;
        tN2kHeadingReference HeadingReference;
        if (!ParseN2kPGN127250(N2kMsg, SID, Heading, Deviation, Variation, HeadingReference)) break;
        if (HeadingReference == N2khr_true) number("navigation.headingTrue", Heading);
        else if (HeadingReference == N2khr_magnetic) number("navigation.headingMagnetic", Heading);
        number("navigation.magneticDeviation", Deviation); number("navigation.magneticVariation", Variation);
        break;
    }
    case 127251: {
        unsigned char SID;
        double RateOfTurn;
        if (!ParseN2kPGN127251(N2kMsg, SID, RateOfTurn)) break;
        number("navigation.rateOfTurn", RateOfTurn);
        break;
    }
    case 127252: {
        unsigned char SID;
        double Heave;
        double Delay;
        tN2kDelaySource DelaySource;
        if (!ParseN2kPGN127252(N2kMsg, SID, Heave, Delay, DelaySource)) break;
        number("environment.heave", Heave);
        break;
    }
    case 127257: {
        unsigned char SID;
        double Yaw;
        double Pitch;
        double Roll;
        if (!ParseN2kPGN127257(N2kMsg, SID, Yaw, Pitch, Roll)) break;
        QJsonObject attitude;
        if (available(Yaw)) attitude.insert("yaw", Yaw);
        if (available(Pitch)) attitude.insert("pitch", Pitch);
        if (available(Roll)) attitude.insert("roll", Roll);
        if (!attitude.isEmpty()) add("navigation.attitude", attitude);
        break;
    }
    case 127258: {
        unsigned char SID;
        tN2kMagneticVariation Source;
        uint16_t DaysSince1970;
        double Variation;
        if (!ParseN2kPGN127258(N2kMsg, SID, Source, DaysSince1970, Variation)) break;
        number("navigation.magneticVariation", Variation);
        break;
    }
    case 127488: {
        unsigned char EngineInstance;
        double EngineSpeed;
        double EngineBoostPressure;
        int8_t EngineTiltTrim;
        if (!ParseN2kPGN127488(N2kMsg, EngineInstance, EngineSpeed, EngineBoostPressure, EngineTiltTrim)) break;
        if (EngineInstance == 255) break;
        const auto p = instance("propulsion", EngineInstance);
        number(p+"revolutions", EngineSpeed, 1.0/60); number(p+"boostPressure", EngineBoostPressure);
        if (EngineTiltTrim >= 0 && EngineTiltTrim <= 100) number(p+"drive.trimState", EngineTiltTrim, .01);
        break;
    }
    case 127489: {
        unsigned char EngineInstance;
        double EngineOilPress;
        double EngineOilTemp;
        double EngineCoolantTemp;
        double AltenatorVoltage;
        double FuelRate;
        double EngineHours;
        double EngineCoolantPress;
        double EngineFuelPress;
        int8_t EngineLoad;
        int8_t EngineTorque;
        tN2kEngineDiscreteStatus1 Status1;
        tN2kEngineDiscreteStatus2 Status2;
        if (!ParseN2kPGN127489(N2kMsg,
                          EngineInstance,
                          EngineOilPress,
                          EngineOilTemp,
                          EngineCoolantTemp,
                          AltenatorVoltage,
                          FuelRate,
                          EngineHours,
                          EngineCoolantPress,
                          EngineFuelPress,
                          EngineLoad,
                          EngineTorque,
                          Status1,
                          Status2)) break;
        if (EngineInstance == 255) break;
        const auto p = instance("propulsion", EngineInstance);
        number(p+"oilPressure", EngineOilPress); number(p+"oilTemperature", EngineOilTemp);
        number(p+"coolantTemperature", EngineCoolantTemp); number(p+"alternatorVoltage", AltenatorVoltage);
        number(p+"fuel.rate", FuelRate, 1.0/3600000); number(p+"runTime", EngineHours);
        number(p+"coolantPressure", EngineCoolantPress); number(p+"fuel.pressure", EngineFuelPress);
        if (EngineLoad >= 0 && EngineLoad <= 100) number(p+"engineLoad", EngineLoad, .01);
        if (EngineTorque >= 0 && EngineTorque <= 100) number(p+"engineTorque", EngineTorque, .01);
        break;
    }
    case 127493: {
        unsigned char EngineInstance;
        tN2kTransmissionGear TransmissionGear;
        double OilPressure;
        double OilTemperature;
        unsigned char DiscreteStatus1;
        if (!ParseN2kPGN127493(N2kMsg, EngineInstance, TransmissionGear, OilPressure, OilTemperature, DiscreteStatus1)) break;
        if (EngineInstance == 255) break;
        const auto p = instance("propulsion", EngineInstance)+"transmission.";
        if (TransmissionGear == N2kTG_Forward) add(p+"gear", "Forward");
        else if (TransmissionGear == N2kTG_Reverse) add(p+"gear", "Reverse");
        else if (TransmissionGear == N2kTG_Neutral) add(p+"gear", "Neutral");
        number(p+"oilPressure", OilPressure); number(p+"oilTemperature", OilTemperature);
        break;
    }
    case 127505: {
        unsigned char Instance;
        tN2kFluidType FluidType;
        double Level;
        double Capacity;
        if (!ParseN2kPGN127505(N2kMsg, Instance, FluidType, Level, Capacity)) break;
        const QStringList fluids{"fuel", "freshWater", "wasteWater", "liveWell", "lubrication", "blackWater", "fuel"};
        if (Instance == 255 || int(FluidType) >= fluids.size()) break;
        // Generic fuel and gasoline are distinct N2k fluid types even when a
        // layout reuses the same numeric instance. Keep both identities.
        const auto p = int(FluidType) == 6
            ? "tanks.fuel.gasoline" + QString::number(Instance) + "."
            : instance("tanks."+fluids[int(FluidType)], Instance);
        if (int(FluidType) == 6) add(p+"type", "petrol");
        if (available(Level) && Level >= 0 && Level <= 100) number(p+"currentLevel", Level, .01);
        number(p+"capacity", Capacity, .001);
        break;
    }
    case 127506: {
        unsigned char SID;
        unsigned char DCInstance;
        tN2kDCType DCType;
        uint8_t StateOfCharge;
        uint8_t StateOfHealth;
        double TimeRemaining;
        double RippleVoltage;
        double Capacity;
        if (!ParseN2kPGN127506(N2kMsg, SID, DCInstance, DCType, StateOfCharge, StateOfHealth, TimeRemaining, RippleVoltage, Capacity)) break;
        // Charge is supplied by PGN 127506 in coulombs, not energy (joules).
        // Do not fabricate energy from a nominal/unknown voltage.
        if (DCInstance == 255 || int(DCType) != 0) break;
        const auto p = instance("electrical.batteries", DCInstance);
        if (StateOfCharge <= 100) number(p+"capacity.stateOfCharge", StateOfCharge, .01);
        if (StateOfHealth <= 100) number(p+"capacity.stateOfHealth", StateOfHealth, .01);
        number(p+"capacity.timeRemaining", TimeRemaining); number(p+"voltage.ripple", RippleVoltage);
        break;
    }
    case 127507: {
        unsigned char Instance;
        unsigned char BatteryInstance;
        tN2kChargeState ChargeState;
        tN2kChargerMode ChargerMode;
        tN2kOnOff Enabled;
        tN2kOnOff EqualizationPending;
        double EqualizationTimeRemaining;
        if (!ParseN2kPGN127507(N2kMsg, Instance, BatteryInstance, ChargeState, ChargerMode, Enabled, EqualizationPending, EqualizationTimeRemaining)) break;
        if (Instance == 255 || BatteryInstance == 255) break;
        const auto p = "electrical.chargers.charger" + QString::number(Instance) + "battery" + QString::number(BatteryInstance) + ".";
        const QStringList states{"other", "bulk", "acceptance", "overcharge", "equalize", "float", "other", "other", "other", "other"};
        if (int(ChargeState) < states.size()) add(p+"chargingMode", states[int(ChargeState)]);
        break;
    }
    case 127508: {
        unsigned char BatteryInstance;
        double BatteryVoltage;
        double BatteryCurrent;
        double BatteryTemperature;
        unsigned char SID;
        if (!ParseN2kPGN127508(N2kMsg, BatteryInstance, BatteryVoltage, BatteryCurrent, BatteryTemperature, SID)) break;
        if (BatteryInstance == 255) break;
        const auto p = instance("electrical.batteries", BatteryInstance);
        number(p+"voltage", BatteryVoltage); number(p+"current", BatteryCurrent); number(p+"temperature", BatteryTemperature);
        break;
    }
    case 128259: {
        unsigned char SID;
        double WaterReferenced;
        double GroundReferenced;
        tN2kSpeedWaterReferenceType SWRT;
        if (!ParseN2kPGN128259(N2kMsg, SID, WaterReferenced, GroundReferenced, SWRT)) break;
        number("navigation.speedThroughWater", WaterReferenced);
        break;
    }
    case 128267: {
        unsigned char SID;
        double DepthBelowTransducer;
        double Offset;
        double Range;
        if (!ParseN2kPGN128267(N2kMsg, SID, DepthBelowTransducer, Offset, Range)) break;
        number("environment.depth.belowTransducer", DepthBelowTransducer);
        if (available(Offset) && available(DepthBelowTransducer)) {
            if (Offset >= 0) number("environment.depth.belowSurface", DepthBelowTransducer+Offset);
            else number("environment.depth.belowKeel", DepthBelowTransducer+Offset);
        }
        break;
    }
    case 129025: {
        double Latitude;
        double Longitude;
        if (!ParseN2kPGN129025(N2kMsg, Latitude, Longitude)) break;
        position("navigation.position", Latitude, Longitude);
        break;
    }
    case 129026: {
        unsigned char SID;
        tN2kHeadingReference ref;
        double COG;
        double SOG;
        if (!ParseN2kPGN129026(N2kMsg, SID, ref, COG, SOG)) break;
        if (ref == N2khr_true) number("navigation.courseOverGroundTrue", COG);
        else if (ref == N2khr_magnetic) number("navigation.courseOverGroundMagnetic", COG);
        number("navigation.speedOverGround", SOG);
        break;
    }
    case 129029: {
        unsigned char SID;
        uint16_t DaysSince1970;
        double SecondsSinceMidnight;
        double Latitude;
        double Longitude;
        double Altitude;
        tN2kGNSStype GNSStype;
        tN2kGNSSmethod GNSSmethod;
        unsigned char nSatellites;
        double HDOP;
        double PDOP;
        double GeoidalSeparation;
        unsigned char nReferenceStations;
        tN2kGNSStype ReferenceStationType;
        uint16_t ReferenceSationID;
        double AgeOfCorrection;
        if (!ParseN2kPGN129029(N2kMsg,
                          SID,
                          DaysSince1970,
                          SecondsSinceMidnight,
                          Latitude,
                          Longitude,
                          Altitude,
                          GNSStype,
                          GNSSmethod,
                          nSatellites,
                          HDOP,
                          PDOP,
                          GeoidalSeparation,
                          nReferenceStations,
                          ReferenceStationType,
                          ReferenceSationID,
                          AgeOfCorrection)) break;
        position("navigation.position", Latitude, Longitude); date(DaysSince1970, SecondsSinceMidnight);
        const QStringList types{"GPS", "GLONASS", "Combined GPS/GLONASS", "GPS", "Combined GPS/GLONASS", "Chayka", "Integrated navigation system", "Surveyed", "Galileo"};
        if (int(GNSStype) < types.size()) add("navigation.gnss.type", types[int(GNSStype)]);
        const QStringList methods{"no GPS", "GNSS Fix", "DGNSS fix", "Precise GNSS", "RTK fixed integer", "RTK float", "Estimated (DR) mode", "Manual input", "Simulator mode"};
        if (int(GNSSmethod) < methods.size()) add("navigation.gnss.methodQuality", methods[int(GNSSmethod)]);
        else if (int(GNSSmethod) == 14) add("navigation.gnss.methodQuality", "Error");
        number("navigation.gnss.antennaAltitude", Altitude);
        if (nSatellites != 255) number("navigation.gnss.satellites", nSatellites);
        number("navigation.gnss.horizontalDilution", HDOP); number("navigation.gnss.positionDilution", PDOP);
        number("navigation.gnss.geoidalSeparation", GeoidalSeparation);
        if (nReferenceStations > 0 && nReferenceStations != 255) {
            number("navigation.gnss.differentialAge", AgeOfCorrection);
            if (ReferenceSationID != N2kUInt16NA) add("navigation.gnss.differentialReference", QString::number(ReferenceSationID));
        }
        break;
    }
    case 129033: {
        uint16_t DaysSince1970;
        double SecondsSinceMidnight;
        int16_t LocalOffset;
        if (!ParseN2kPGN129033(N2kMsg, DaysSince1970, SecondsSinceMidnight, LocalOffset)) break;
        date(DaysSince1970, SecondsSinceMidnight);
        break;
    }
    case 130306: {
        if (N2kMsg.DataLen != 8) break;
        unsigned char sid; double speed, angle; tN2kWindReference reference;
        if (!ParseN2kPGN130306(N2kMsg, sid, speed, angle, reference)) break;
        const auto relative = available(angle) && angle > M_PI ? angle - 2*M_PI : angle;
        switch (reference) {
        case N2kWind_True_North: environmental("environment.wind.directionTrue", angle); environmental("environment.wind.speedOverGround", speed); break;
        case N2kWind_Magnetic: environmental("environment.wind.directionMagnetic", angle); environmental("environment.wind.speedOverGround", speed); break;
        case N2kWind_Apparent: environmental("environment.wind.angleApparent", relative); environmental("environment.wind.speedApparent", speed); break;
        case N2kWind_True_boat: environmental("environment.wind.angleTrueGround", relative); environmental("environment.wind.speedOverGround", speed); break;
        case N2kWind_True_water: environmental("environment.wind.angleTrueWater", relative); environmental("environment.wind.speedTrue", speed); break;
        default: break; // No transformation or fabricated reference.
        }
        break;
    }
    case 130313: {
        if (N2kMsg.DataLen != 8) break;
        unsigned char sid, id; tN2kHumiditySource source; double actual, set;
        if (!ParseN2kPGN130313(N2kMsg, sid, id, source, actual, set) || id == 255) break;
        if (source == N2khs_OutsideHumidity) environmental("environment.outside.relativeHumidity", actual, .01);
        else if (source == N2khs_InsideHumidity) environmental("environment.inside.sensor" + QString::number(id) + ".relativeHumidity", actual, .01);
        break;
    }
    case 130310: {
        unsigned char SID;
        double WaterTemperature;
        double OutsideAmbientAirTemperature;
        double AtmosphericPressure;
        if (!ParseN2kPGN130310(N2kMsg, SID, WaterTemperature, OutsideAmbientAirTemperature, AtmosphericPressure)) break;
        environmental("environment.water.temperature", WaterTemperature); environmental("environment.outside.temperature", OutsideAmbientAirTemperature);
        environmental("environment.outside.pressure", AtmosphericPressure);
        break;
    }
    case 130311: {
        unsigned char SID;
        tN2kTempSource TempSource;
        double Temperature;
        tN2kHumiditySource HumiditySource;
        double Humidity;
        double AtmosphericPressure;
        if (!ParseN2kPGN130311(N2kMsg, SID, TempSource, Temperature, HumiditySource, Humidity, AtmosphericPressure)) break;
        environmental(temperaturePath(TempSource, 0), Temperature);
        if (int(HumiditySource) == 0) environmental("environment.inside.relativeHumidity", Humidity, .01);
        else if (int(HumiditySource) == 1) environmental("environment.outside.relativeHumidity", Humidity, .01);
        environmental("environment.outside.pressure", AtmosphericPressure);
        break;
    }
    case 130312: {
        unsigned char SID;
        unsigned char TemperatureInstance;
        tN2kTempSource TemperatureSource;
        double ActualTemperature;
        double SetTemperature;
        if (!ParseN2kPGN130312(N2kMsg, SID, TemperatureInstance, TemperatureSource, ActualTemperature, SetTemperature)) break;
        if (TemperatureInstance != 255) environmental(temperaturePath(TemperatureSource, TemperatureInstance), ActualTemperature);
        break;
    }
    case 130316: {
        unsigned char SID;
        unsigned char TempInstance;
        tN2kTempSource TempSource;
        double ActualTemperature;
        double SetTemperature;
        if (!ParseN2kPGN130316(N2kMsg, SID, TempInstance, TempSource, ActualTemperature, SetTemperature)) break;
        if (TempInstance != 255) environmental(temperaturePath(TempSource, TempInstance), ActualTemperature);
        break;
    }
    default: break; // No invented paths for unsupported management/status fields.
    }
    return result;
}

QJsonObject SignalKPublisher::aisDelta(int pgn, const QMap<QString,QString> &fields,
                                      const QHash<QString,QDateTime> &sampleTimes, const QString &source)
{
    if (pgn!=129809 && pgn!=129810 && pgn!=129039) return {};
    const auto mmsi=fields.value("UserID");
    bool ok=false;const auto id=mmsi.toUInt(&ok);
    if (!ok || id<200000000 || id>799999999 || mmsi!=QString::number(id)) return {};
    QJsonArray updates;
    // One timestamp per accepted source sample, never regenerated by the send timer.
    // Root/static configuration may have unknown observation time: omit timestamp;
    // the receiver's ingestion time is not asserted to be a measured observation time.
    auto add=[&](const QString &path,const QJsonValue &value,const QDateTime &time=QDateTime()) {
        QJsonObject update{{"$source",source},{"values",QJsonArray{QJsonObject{{"path",path},{"value",value}}}}};
        if(time.isValid())update.insert("timestamp",time.toUTC().toString(Qt::ISODateWithMs));
        updates.append(update);
    };
    auto numeric=[&](const QString &field,double low,double high) {
        bool valid=false;const double n=fields.value(field).toDouble(&valid);
        return valid && available(n) && n>=low && n<=high;
    };
    auto time=[&](const QString &field){return sampleTimes.value(field);};
    add("",QJsonObject{{"mmsi",mmsi}});
    add("sensors.ais.class","B");
    if(pgn==129809) {
        if(!fields.value("Name").trimmed().isEmpty())add("",QJsonObject{{"name",fields.value("Name")}},time("Name"));
    } else if(pgn==129810) {
        if(!fields.value("Callsign").trimmed().isEmpty())
            add("",QJsonObject{{"communication",QJsonObject{{"callsignVhf",fields.value("Callsign")}}}},time("Callsign"));
        bool motherOk=false;const auto mother=fields.value("MothershipID").toUInt(&motherOk);
        if(motherOk && mother>=200000000 && mother<=799999999)
            add("",QJsonObject{{"mothershipMmsi",QString::number(mother)}},time("MothershipID"));
        // Standard schema enumeration facts, not a copied third-party conversion implementation.
        static const QMap<int,QString> types{
        {20,"Wing In Ground"},
        {21,"Wing In Ground hazard cat A"},
        {22,"Wing In Ground hazard cat B"},
        {23,"Wing In Ground hazard cat C"},
        {24,"Wing In Ground hazard cat D"},
        {25,"Wing In Ground"},
        {26,"Wing In Ground"},
        {27,"Wing In Ground"},
        {28,"Wing In Ground"},
        {29,"Wing In Ground (no other information)"},
        {30,"Fishing"},
        {31,"Towing"},
        {32,"Towing exceeds 200m or wider than 25m"},
        {33,"Engaged in dredging or underwater operations"},
        {34,"Engaged in diving operations"},
        {35,"Engaged in military operations"},
        {36,"Sailing"},
        {37,"Pleasure"},
        {40,"High speed craft"},
        {41,"High speed craft carrying dangerous goods"},
        {42,"High speed craft hazard cat B"},
        {43,"High speed craft hazard cat C"},
        {44,"High speed craft hazard cat D"},
        {45,"High speed craft"},
        {46,"High speed craft"},
        {47,"High speed craft"},
        {48,"High speed craft"},
        {49,"High speed craft (no additional information)"},
        {50,"Pilot vessel"},
        {51,"SAR"},
        {52,"Tug"},
        {53,"Port tender"},
        {54,"Anti-pollution"},
        {55,"Law enforcement"},
        {56,"Spare"},
        {57,"Spare #2"},
        {58,"Medical"},
        {59,"RR Resolution No.18"},
        {60,"Passenger ship"},
        {61,"Passenger ship hazard cat A"},
        {62,"Passenger ship hazard cat B"},
        {63,"Passenger ship hazard cat C"},
        {64,"Passenger ship hazard cat D"},
        {65,"Passenger ship"},
        {66,"Passenger ship"},
        {67,"Passenger ship"},
        {68,"Passenger ship"},
        {69,"Passenger ship (no additional information)"},
        {70,"Cargo ship"},
        {71,"Cargo ship carrying dangerous goods"},
        {72,"Cargo ship hazard cat B"},
        {73,"Cargo ship hazard cat C"},
        {74,"Cargo ship hazard cat D"},
        {75,"Cargo ship"},
        {76,"Cargo ship"},
        {77,"Cargo ship"},
        {78,"Cargo ship"},
        {79,"Cargo ship (no additional information)"},
        {80,"Tanker"},
        {81,"Tanker carrying dangerous goods"},
        {82,"Tanker hazard cat B"},
        {83,"Tanker hazard cat C"},
        {84,"Tanker hazard cat D"},
        {85,"Tanker"},
        {86,"Tanker"},
        {87,"Tanker"},
        {88,"Tanker"},
        {89,"Tanker (no additional information)"},
        {90,"Other"},
        {91,"Other carrying dangerous goods"},
        {92,"Other hazard cat B"},
        {93,"Other hazard cat C"},
        {94,"Other hazard cat D"},
        {95,"Other"},
        {96,"Other"},
        {97,"Other"},
        {98,"Other"},
        {99,"Other (no additional information)"}};
        const int type=fields.value("VesselType").toInt();
        if(types.contains(type))add("design.aisShipType",QJsonObject{{"id",type},{"name",types.value(type)}},time("VesselType"));
        if(numeric("Length",0,6553.3))add("design.length",QJsonObject{{"overall",fields.value("Length").toDouble()}},time("Length"));
        if(numeric("Beam",0,6553.3))add("design.beam",fields.value("Beam").toDouble(),time("Beam"));
        if(numeric("PosRefBow",0,6553.3) && (!numeric("Length",0,6553.3) || fields.value("PosRefBow").toDouble()<=fields.value("Length").toDouble()))
            add("sensors.ais.fromBow",fields.value("PosRefBow").toDouble(),time("PosRefBow"));
        if(numeric("PosRefStbd",0,6553.3) && numeric("Beam",0,6553.3)
            && fields.value("PosRefStbd").toDouble()<=fields.value("Beam").toDouble()) {
            // Positive to starboard: the PGN offset is measured inward from starboard.
            const auto stamp=time("PosRefStbd")>time("Beam")?time("PosRefStbd"):time("Beam");
            add("sensors.ais.fromCenter",fields.value("Beam").toDouble()/2-fields.value("PosRefStbd").toDouble(),stamp);
        }
    } else {
        // Require recorded source time. The scheduler supplies qualified observation
        // time, local edit/sample time, or the fixed configuration-load snapshot time.
        // A caller with no provenance must not have old data timestamped by the server.
        if(numeric("Latitude",-90,90) && numeric("Longitude",-180,180)
            && time("Latitude").isValid() && time("Longitude").isValid()) {
            // A coordinate pair is no newer than its older component.
            const auto stamp=time("Latitude")<time("Longitude")?time("Latitude"):time("Longitude");
            add("navigation.position",QJsonObject{{"latitude",fields.value("Latitude").toDouble()},
                {"longitude",fields.value("Longitude").toDouble()}},stamp);
        }
        for(const auto &pair:QList<QPair<QString,QString>>{{"SOG","navigation.speedOverGround"},
            {"COG","navigation.courseOverGroundTrue"},{"Heading","navigation.headingTrue"}}) {
            const double limit=pair.first=="SOG"?655.33:6.2832;
            if(numeric(pair.first,0,limit) && time(pair.first).isValid())
                add(pair.second,fields.value(pair.first).toDouble(),time(pair.first));
        }
    }
    return {{"context","vessels.urn:mrn:imo:mmsi:"+mmsi},{"updates",updates}};
}
