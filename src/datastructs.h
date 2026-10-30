/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
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

#ifndef DATASTRUCTS_H
#define DATASTRUCTS_H
#include <QList>

#include <QString>
#include <cstdint>

class DataStructs
{
public:
    DataStructs();
};

// Enums
enum Mode { Viewer, Sender };
enum Nmea2000InterfaceType { None, SerialPort, SocketCan, SignalK };
enum AutopilotSpeedSource { Manual, ThrottlePosition, RouteFile };

// for leveraging existing codebase where uses PGN numbers
enum NMEA0183Sentence {
    SNT_GGA = 100001,
    SNT_GLL = 100002,
    SNT_RMB = 100003,
    SNT_RMC = 100004,
    SNT_WPL = 100005,
    SNT_BOD = 100006,
    SNT_ZDA = 100007,
    SNT_GSV = 100008,
    SNT_APB = 100009,
    SNT_VTG = 100010,
    SNT_HDT = 100011,
    SNT_HDM = 100012,
    SNT_HDG = 100013,
    SNT_MTW = 100014,
    SNT_VLW = 100015,
    SNT_VHW = 100016,
    SNT_VWR = 100017
};

// Struts
struct UserUnitPrefs
{
    uint8_t tempUnitIndex = 2;        // Index in unitsTemp (Default is Fahrenheit)
    uint8_t pressureUnitIndex = 3;    // Index in unitsPressure (Default is PSI)
    uint8_t flowRateUnitIndex = 1;    // Index in unitsFlowRate (Default is GPH)
    uint8_t volumeUnitIndex = 1;      // Index in unitsVolume (Default is gal)
    uint8_t speedUnitIndex = 2;       // Index in unitsSpeed (Default is MPH)
    uint8_t depthUnitIndex = 1;       // Index in unitsDepth (Default is ft)
    uint8_t latLongUnitIndex = 2;     // Index in UnitsLatLong (Default is DD°MM.MMM’)
    uint8_t distanceUnitIndex = 2;    // Index in UnitsDistance (Default is mi & ft)
    uint8_t fuelEconomyUnitIndex = 3; // Index in UnitsEconomy (Default is MPG)
    uint8_t currencyUnitIndex = 0;    // Index in UnitsCurrency (Default is $)
    uint8_t rateOfTurnUnitIndex = 1;  // Index in UnitsRateOfTurn (Default is deg/second)
    uint8_t timeUnitIndex = 2;        // Index in UnitsTime (Default is HH:MM:SS)
    uint8_t dateUnitIndex = 0;        // Index in UnitsDate (Default is MM/DD/YYYY)
    uint8_t headingReferenceIndex = 0; // Index in UnitsHeadingReference (Default is True)
    uint8_t atmosphericPressureUnitIndex = 0; // Index in UnitsAtmosphericPressure (Default is hPa or mbar)
    // Define operator== to compare two UserUnitPrefs
    bool operator==(const UserUnitPrefs& other) const
    {
        return tempUnitIndex == other.tempUnitIndex && pressureUnitIndex == other.pressureUnitIndex && flowRateUnitIndex == other.flowRateUnitIndex
               && volumeUnitIndex == other.volumeUnitIndex && speedUnitIndex == other.speedUnitIndex && depthUnitIndex == other.depthUnitIndex
               && latLongUnitIndex == other.latLongUnitIndex && distanceUnitIndex == other.distanceUnitIndex
               && fuelEconomyUnitIndex == other.fuelEconomyUnitIndex && currencyUnitIndex == other.currencyUnitIndex
               && rateOfTurnUnitIndex == other.rateOfTurnUnitIndex && timeUnitIndex == other.timeUnitIndex && dateUnitIndex == other.dateUnitIndex
               && headingReferenceIndex == other.headingReferenceIndex && atmosphericPressureUnitIndex == other.atmosphericPressureUnitIndex;
    }

    // Define operator!= to compare two UserUnitPrefs
    bool operator!=(const UserUnitPrefs& other) const { return !(*this == other); }
};

struct UserOptionPrefs
{
    bool newWidgetsEnabled = true;
    bool promptSaveLayoutOnClose = true;
    int widgetColumns = 2;
    int borderWidth = 1;
};

struct VesselWpBearing
{
    QString WaypointName = "";
    double DistanceToWaypointM = 0.0;
    uint8_t Reference = 0;
    double EtaTimeS = 0.0; // wire UTC seconds since midnight
    double RemainingTimeS = 0.0; // UI duration; N2kDoubleNA when unavailable
    bool PerpendicularCrossed = false;
    bool ArrivalCircleEntered = false;
    double CourseRadians = 0.0;
    // Optional ground motion, populated only by environmental route integration.
    double HeadingRadians = -1, GroundSpeed = -1;
    double WavePitch = 0, WaveRoll = 0, WaveHeave = 0;
    int16_t EtaDate = 0;
    double BearingOriginToDestinationWaypoint = 0.0;
    double BearingPositionToDestinationWaypoint = 0.0;
    uint32_t OriginWaypointNumber = 0;
    uint32_t DestinationWaypointNumber = 0;
    double DestinationLatitude = 0.0;
    double DestinationLongitude = 0.0;
    double WaypointClosingVelocity = 0.0;
    double Xte = 0.0;
    double TotalDistanceTraveledM = 0.0;
    double TimeTraveledS = 0.0;

    // Define operator== to compare two VesselWpBearing
    bool operator==(const VesselWpBearing& other) const
    {
        return WaypointName == other.WaypointName && DistanceToWaypointM == other.DistanceToWaypointM && Reference == other.Reference
               && EtaTimeS == other.EtaTimeS && RemainingTimeS == other.RemainingTimeS
               && PerpendicularCrossed == other.PerpendicularCrossed && ArrivalCircleEntered == other.ArrivalCircleEntered
               && HeadingRadians == other.HeadingRadians && GroundSpeed == other.GroundSpeed
               && WavePitch == other.WavePitch && WaveRoll == other.WaveRoll && WaveHeave == other.WaveHeave
               && CourseRadians == other.CourseRadians && EtaDate == other.EtaDate && BearingOriginToDestinationWaypoint == other.BearingOriginToDestinationWaypoint
               && BearingPositionToDestinationWaypoint == other.BearingPositionToDestinationWaypoint && OriginWaypointNumber == other.OriginWaypointNumber
               && DestinationWaypointNumber == other.DestinationWaypointNumber && DestinationLatitude == other.DestinationLatitude
               && DestinationLongitude == other.DestinationLongitude && WaypointClosingVelocity == other.WaypointClosingVelocity && Xte == other.Xte
               && TotalDistanceTraveledM == other.TotalDistanceTraveledM && TimeTraveledS == other.TimeTraveledS;
    }

    // Define operator!= to compare two VesselWpBearing
    bool operator!=(const VesselWpBearing& other) const { return !(*this == other); }
};

// Defaults only; live electrical measurements remain in the PGN model.
struct VesselElectricalDefaults {
    int nominalVoltage = 12, batteryCount = 2, batteryType = 2; // AGM (tN2kBatType)
    double capacityC = 360000, initialSoc = 80, initialVoltage = 12.6, temperatureK = 293.15;
    bool simulateNew = true;
    QList<int> engineForBattery {0, 1, -1, -1, -1, -1, -1, -1};
    bool operator==(const VesselElectricalDefaults& b) const {
        return nominalVoltage == b.nominalVoltage && batteryCount == b.batteryCount && batteryType == b.batteryType
            && capacityC == b.capacityC && initialSoc == b.initialSoc && initialVoltage == b.initialVoltage
            && temperatureK == b.temperatureK && simulateNew == b.simulateNew && engineForBattery == b.engineForBattery;
    }
    bool operator!=(const VesselElectricalDefaults& b) const { return !(*this == b); }
};

struct MyVessel
{
    VesselElectricalDefaults Electrical;
    bool EnvironmentalInfluence = false;
    QString Name = "None";
    quint32 MMSI = 0; // Simulated AIS identity, never the CAN source.
    int VesselType = 0; // AIS ship type; 0 unspecified, 255 unavailable. Not an NMEA library enum.
    QString RadioCallsign;
    double Length = 10.668; // 35 ft
    double Beam = 3.6576;   // 12 ft
    double Height = 2.4384; // 8 ft
    double Draft = 0.9144;  // 3 ft
    double Displacement = 8164.663; // 18,000 lbs (in kg)
    QString EngineProfile = "MerCruiser 7.4 MPI";
    int NumEngines = 2;
    int EngineHours = 150 * 3600;                     // 150 hours in seconds
    double WotSpeed = 17.8816;                        // 40 mph
    double CruisingSpeed = 12.51712;                  // 28 mph
    double PortFuelTankSize = 378.5411784;            // 100 US gal, in liters
    double StbdFuelTankSize = 378.5411784;            // 100 US gal, in liters
    double PortFuelTankLevel = PortFuelTankSize * .9; // 90 gal
    double StbdFuelTankLevel = StbdFuelTankSize * .9; // 90 gal
    double EngineRoomTempK = 275.372;                 // 72°F
    double WaterTempK = 295.372;                      // 72°F
    double Latitude = 42.4979167;                     // Merrimac River Entrance
    double Longitude = -70.4960333;                   // Merrimac River Entrance

    // Define operator== to compare two MyVessel
    bool operator==(const MyVessel& other) const
    {
        return EnvironmentalInfluence == other.EnvironmentalInfluence && Electrical == other.Electrical && Name == other.Name && MMSI == other.MMSI && VesselType == other.VesselType && RadioCallsign == other.RadioCallsign && Length == other.Length && Beam == other.Beam && Height == other.Height && Draft == other.Draft
               && Displacement == other.Displacement && EngineProfile == other.EngineProfile && NumEngines == other.NumEngines && WotSpeed == other.WotSpeed
               && CruisingSpeed == other.CruisingSpeed && PortFuelTankSize == other.PortFuelTankSize && StbdFuelTankSize == other.StbdFuelTankSize
               && PortFuelTankLevel == other.PortFuelTankLevel && StbdFuelTankLevel == other.StbdFuelTankLevel && EngineHours == other.EngineHours
               && EngineRoomTempK == other.EngineRoomTempK && WaterTempK == other.WaterTempK && Latitude == other.Latitude && Longitude == other.Longitude;
    }

    // Define operator!= to compare two MyVessel
    bool operator!=(const MyVessel& other) const { return !(*this == other); }
};

struct EngineProperties
{
    QString name = "MerCruiser 7.4 MPI";
    int horsepower = 310;
    QString fuelType = "Gasoline"; // e.g., Diesel, Gasoline
    bool fuelInjected = true;
    bool forcedInduction = false; // true for turbocharged/supercharged engines
    int alternatorVoltage = 12;   // in volts (e.g., 12, 24)
};

struct VesselStatus
{
    double latitude = 42.813903;
    double longitude = -70.867977;
    double heading = 0.0;
    double sog = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
    double heave = 0.0;
    double rudderPosition = 0.0;
    double rateOfTurn = 0.0;

    // Signed propulsion speed above remains independent of optional ground motion.
    double groundCourse = -1, groundSpeed = -1;

    // Define operator== to compare two VesselStatus
    bool operator==(const VesselStatus& other) const
    {
        return groundCourse == other.groundCourse && groundSpeed == other.groundSpeed && latitude == other.latitude && longitude == other.longitude && heading == other.heading && sog == other.sog && pitch == other.pitch
               && heave == other.heave && roll == other.roll && rudderPosition == other.rudderPosition && rateOfTurn == other.rateOfTurn;
    }

    // Define operator!= to compare two VesselStatus
    bool operator!=(const VesselStatus& other) const { return !(*this == other); }
};

struct PgnAsCombobox
{
    QString pgn = "";
    QString label = "";
    QString group = "";
};

struct Station
{
    QString id;
    QString name;
    double latitude;
    double longitude;
    // NOAA active-station standard-met availability hint: unknown / yes / no.
    int standardMet = -1;
};

#endif // DATASTRUCTS_H
