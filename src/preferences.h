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

#ifndef PREFERENCES_H
#define PREFERENCES_H
#include <QSettings>
#include <cmath>
#include <limits>
#include "datastructs.h"
// Call with the Preference settings group active. Validate before narrowing to uint8_t.
inline UserUnitPrefs readUnitPreferences(QSettings& settings)
{
    UserUnitPrefs prefs;
    auto index = [&settings](const char* key, int fallback, int count) {
        bool ok = false;
        const int value = settings.value(key, fallback).toInt(&ok);
        return ok && value >= 0 && value < count ? value : fallback;
    };
    prefs.tempUnitIndex = index("UnitsTemp", prefs.tempUnitIndex, 3);
    prefs.pressureUnitIndex = index("UnitsPressure", prefs.pressureUnitIndex, 4);
    prefs.flowRateUnitIndex = index("UnitsFlowRate", prefs.flowRateUnitIndex, 2);
    prefs.volumeUnitIndex = index("UnitsVolume", prefs.volumeUnitIndex, 2);
    prefs.speedUnitIndex = index("UnitsSpeed", prefs.speedUnitIndex, 4);
    prefs.depthUnitIndex = index("UnitsDepth", prefs.depthUnitIndex, 3);
    prefs.latLongUnitIndex = index("UnitsLatLong", prefs.latLongUnitIndex, 3);
    prefs.distanceUnitIndex = index("UnitsDistance", prefs.distanceUnitIndex, 3);
    prefs.fuelEconomyUnitIndex = index("UnitsFuelEconomy", prefs.fuelEconomyUnitIndex, 6);
    prefs.currencyUnitIndex = index("UnitsCurrency", prefs.currencyUnitIndex, 1);
    prefs.rateOfTurnUnitIndex = index("UnitsRateOfTurn", prefs.rateOfTurnUnitIndex, 2);
    prefs.timeUnitIndex = index("UnitsTime", prefs.timeUnitIndex, 4);
    prefs.dateUnitIndex = index("UnitsDate", prefs.dateUnitIndex, 3);
    prefs.headingReferenceIndex = index("UnitsHeadingReference", prefs.headingReferenceIndex, 2);
    prefs.atmosphericPressureUnitIndex = index("UnitsAtmosphericPressure", prefs.atmosphericPressureUnitIndex, 5);
    return prefs;
}

// Call with the Vessel group active. This is the shared canonical settings schema.
inline MyVessel readVesselSettings(QSettings& settings)
{
    MyVessel vessel;
    auto number = [&settings](const char* key, double fallback, double minimum = 0.0, double maximum = std::numeric_limits<double>::max(),
                      const char* legacy = nullptr) {
        bool ok = false;
        const double value = settings.value(key, legacy ? settings.value(legacy, fallback) : QVariant(fallback)).toDouble(&ok);
        return ok && std::isfinite(value) && value >= minimum && value <= maximum ? value : fallback;
    };
    vessel.EnvironmentalInfluence = settings.value("EnvironmentalInfluence", false).toBool();
    vessel.Name = settings.value("Name", "Vessel").toString();
    const auto rawMmsi=settings.value("MMSI",0).toString();
    bool mmsiOk=false;const auto mmsi=rawMmsi.toULongLong(&mmsiOk);
    vessel.MMSI=mmsiOk && rawMmsi==QString::number(mmsi) && (mmsi==0 || mmsi==0xffffffffULL || (mmsi>=100000000 && mmsi<=999999999))?quint32(mmsi):0;
    bool typeOk = false;
    const int type = settings.value("VesselType", 0).toInt(&typeOk);
    vessel.VesselType = typeOk && type >= 0 && type <= 255 ? type : 0;
    vessel.RadioCallsign = settings.value("RadioCallsign").toString();
    vessel.Length = number("Length", vessel.Length, 0, 1e9, "Lenght");
    vessel.Beam = number("Beam", vessel.Beam);
    vessel.Height = number("Height", vessel.Height);
    vessel.Draft = number("Draft", vessel.Draft);
    vessel.EngineProfile = settings.value("EngineProfile", vessel.EngineProfile).toString();
    const int engines = settings.value("NumEngines", vessel.NumEngines).toInt();
    vessel.NumEngines = engines >= 1 && engines <= 8 ? engines : vessel.NumEngines;
    vessel.WotSpeed = number("WotSpeed", vessel.WotSpeed);
    vessel.CruisingSpeed = number("CruisingSpeed", vessel.CruisingSpeed, 0, 1e9, "CrusingSpeed");
    vessel.PortFuelTankSize = number("PortFuelTankSize", vessel.PortFuelTankSize);
    vessel.StbdFuelTankSize = number("StarboardFuelTankSize", vessel.StbdFuelTankSize, 0, 1e9, "StbdFuelTankSize");
    vessel.PortFuelTankLevel = number("PortFuelLevel", vessel.PortFuelTankSize * 0.9, 0, 1e9, "PortFuelTankLevel");
    vessel.StbdFuelTankLevel = number("StarboardFuelLevel", vessel.StbdFuelTankSize * 0.9, 0, 1e9, "StbdFuelTankLevel");
    auto &e = vessel.Electrical;
    e.nominalVoltage = int(number("ElectricalNominalVoltage", 12, 12, 24));
    if (e.nominalVoltage != 24) e.nominalVoltage = 12;
    e.batteryCount = int(number("ElectricalBatteryCount", 2, 1, 8));
    e.batteryType = int(number("ElectricalBatteryType", 2, 0, 2));
    e.capacityC = number("ElectricalCapacityC", 360000, 0, 36000000);
    e.initialSoc = qRound(number("ElectricalInitialSoc", 80, 0, 100));
    e.initialVoltage = number("ElectricalInitialVoltage", e.nominalVoltage * 1.05, e.nominalVoltage * .5, e.nominalVoltage * 1.3);
    e.temperatureK = number("ElectricalTemperatureK", 293.15, 213.15, 523.15);
    e.simulateNew = settings.value("ElectricalSimulateNew", true).toBool();
    for (int i = 0; i < 8; ++i) {
        bool ok = false; int engine = settings.value(QString("ElectricalEngine%1").arg(i), e.engineForBattery[i]).toInt(&ok);
        e.engineForBattery[i] = ok && engine >= -1 && engine < 8 ? engine : -1;
    }
    vessel.Latitude = number("Latitude", 42.813903, -90, 90);
    vessel.Longitude = number("Longitude", -70.867977, -180, 180);
    return vessel;
}

inline void writeVesselElectricalSettings(QSettings& settings, const VesselElectricalDefaults& e)
{
    settings.setValue("ElectricalNominalVoltage", e.nominalVoltage);
    settings.setValue("ElectricalBatteryCount", e.batteryCount);
    settings.setValue("ElectricalBatteryType", e.batteryType);
    settings.setValue("ElectricalCapacityC", e.capacityC);
    settings.setValue("ElectricalInitialSoc", e.initialSoc);
    settings.setValue("ElectricalInitialVoltage", e.initialVoltage);
    settings.setValue("ElectricalTemperatureK", e.temperatureK);
    settings.setValue("ElectricalSimulateNew", e.simulateNew);
    for (int i = 0; i < 8; ++i) settings.setValue(QString("ElectricalEngine%1").arg(i), e.engineForBattery.value(i, -1));
}

inline UserOptionPrefs readOptionPreferences(QSettings& settings)
{
    UserOptionPrefs prefs;
    prefs.newWidgetsEnabled = settings.value("EnableNewWidgets", true).toBool();
    prefs.promptSaveLayoutOnClose = settings.value("PromptSaveLayoutOnClose", true).toBool();
    const int columns = settings.value("WidgetColumns", prefs.widgetColumns).toInt();
    bool borderOk = false;
    const int border = settings.value("WidgetBorderWidth", prefs.borderWidth).toInt(&borderOk);
    prefs.widgetColumns = columns >= 1 && columns <= 4 ? columns : prefs.widgetColumns;
    prefs.borderWidth = borderOk && border >= 0 && border <= 3 ? border : prefs.borderWidth;
    return prefs;
}
#endif
