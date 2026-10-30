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

#ifndef N2KHELPER_H
#define N2KHELPER_H

#include <QDateTime>
#include <QString>
#include <QtGlobal>
#include "N2kTypes.h"
#include "convert.h"
#include "datastructs.h"
#include <cstdint>
#include <tuple>

class N2kHelper
{
public:
    N2kHelper();

    static tN2kTempSource getTempSourceFromLabel(const QString &label);
    static double convertN2kTemperatureToUserUnits(double temperature, UserUnitPrefs prefs);
    static double convertUserTemperatureToN2kUnits(double temperature, UserUnitPrefs prefs);
    static double convertN2kPressureToUserUnits(double pressure, UserUnitPrefs prefs);
    static double convertUserPressureToN2kUnits(double pressure, UserUnitPrefs prefs);
    static double convertN2kAtmosphericPressureToUserUnits(double pressure, UserUnitPrefs prefs);
    static double convertUserAtmosphericPressureToN2kUnits(double pressure, UserUnitPrefs prefs);
    static double convertN2kFlowRateToUserUnits(double flowRate, UserUnitPrefs prefs);
    static double convertUserFlowRateToN2kUnits(double flowRate, UserUnitPrefs prefs);
    static double convertN2kVolumeToUserUnits(double volume, UserUnitPrefs prefs);
    static double convertUserVolumeToN2kUnits(double volume, UserUnitPrefs prefs);
    static double convertN2kDistanceFarToUserUnits(double distance, UserUnitPrefs prefs);
    static double convertUserDistanceFarToN2kUnits(double distance, UserUnitPrefs prefs);
    static double convertN2kDistanceNearToUserUnits(double distance, UserUnitPrefs prefs);
    static double convertUserDistanceNearToN2kUnits(double distance, UserUnitPrefs prefs);
    static double convertN2kSpeedToUserUnits(double speed, UserUnitPrefs prefs);
    static double convertUserSpeedToN2kUnits(double speed, UserUnitPrefs prefs);
    static double convertN2kDepthToUserUnits(double depth, UserUnitPrefs prefs);
    static double convertUserDepthToN2kUnits(double depth, UserUnitPrefs prefs);
    static QString convertN2kLatitudeToUserUnits(double latitude, UserUnitPrefs prefs);
    static double convertUserLatitudeToN2kUnits(QString latitude, UserUnitPrefs prefs);
    static QString convertN2kLongitudeToUserUnits(double longitude, UserUnitPrefs prefs);
    static double convertUserLongitudeToN2kUnits(QString longitude, UserUnitPrefs prefs);
    static double convertN2kFuelEconomyToUserUnits(double fuelEconomy, UserUnitPrefs prefs);
    static double convertUserFuelEconomyToN2kUnits(double fuelEconomy, UserUnitPrefs prefs);
    static double convertN2kCurrencyToUserUnits(double currency, UserUnitPrefs prefs);
    static double convertUserCurrencyToN2kUnits(double currency, UserUnitPrefs prefs);
    static double convertN2kRateOfTurnToUserUnits(double rateOfTurn, UserUnitPrefs prefs);
    static double convertUserRateOfTurnToN2kUnits(double rateOfTurn, UserUnitPrefs prefs);
    static QString convertN2kTimeToUserUnits(double time);
    static double convertUserTimeToN2kUnits(QString time);
    static QString convertN2kDateToUserFormat(uint16_t DaysSince1970, const UserUnitPrefs &prefs);
    static QString convertN2kDateToUserFormat(uint16_t DaysSince1970, const UserUnitPrefs &prefs, int16_t localOffset, double secondsSinceMidnight = 0);
    static uint16_t convertUserFormattedDateToN2k(const QString &userFormattedDate, const UserUnitPrefs &prefs);
    static QString convertN2kTimeToUserFormat(double SecondsSinceMidnight, const UserUnitPrefs &prefs);
    static QString convertN2kTimeToUserFormat(double SecondsSinceMidnight, const UserUnitPrefs &prefs, int16_t localOffset);
    static double convertUserFormattedTimeToN2k(const QString &userFormattedTime, const UserUnitPrefs &prefs);
    static QString convertN2kHoursToUserFormat(double hoursInMinutes, const UserUnitPrefs &prefs);
    static double convertUserFormattedHoursToN2k(const QString &userFormattedHours, const UserUnitPrefs &prefs);

    static std::pair<uint16_t, double> getSystemDateAndTime(const QDateTime &dateTime);
    static std::tuple<uint16_t, double, int16_t> getLocalSystemDateTime(const QDateTime &localDateTime);

    static tN2kEngineDiscreteStatus1 getEngineDiscreteStatus1(uint16_t status1);
    static uint16_t setEngineDiscreteStatus1(tN2kEngineDiscreteStatus1 status1);
    static tN2kEngineDiscreteStatus1 getEngineDiscreteStatus2(uint16_t status2);
    static uint16_t setEngineDiscreteStatus2(tN2kEngineDiscreteStatus1 status2);
};

#endif // N2KHELPER_H
