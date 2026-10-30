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

#include "n2khelper.h"
#include "N2kMsg.h"
#include <cmath>

N2kHelper::N2kHelper() {}

tN2kTempSource N2kHelper::getTempSourceFromLabel(const QString &label)
{
    if (label == "Sea Temperature") {
        return N2kts_SeaTemperature;
    } else if (label == "Outside Temp") {
        return N2kts_OutsideTemperature;
    } else if (label == "Engine Room Temp") {
        return N2kts_EngineRoomTemperature;
    } else if (label == "Main Cabin Temp") {
        return N2kts_MainCabinTemperature;
    } else if (label == "Live Well Temp") {
        return N2kts_LiveWellTemperature;
    } else if (label == "Bait Well Temp") {
        return N2kts_BaitWellTemperature;
    } else if (label == "Refrigeration Temp") {
        return N2kts_RefridgerationTemperature;
    } else if (label == "Heating System Temp") {
        return N2kts_HeatingSystemTemperature;
    } else if (label == "Dew Point Temp") {
        return N2kts_DewPointTemperature;
    } else if (label == "Apparent Wind Chill Temp") {
        return N2kts_ApparentWindChillTemperature;
    } else if (label == "Theoretical Wind Chill Temp") {
        return N2kts_TheoreticalWindChillTemperature;
    } else if (label == "Heat Index Temp") {
        return N2kts_HeatIndexTemperature;
    } else if (label == "Freezer Temp") {
        return N2kts_FreezerTemperature;
    } else if (label == "Exhaust Gas Temp") {
        return N2kts_ExhaustGasTemperature;
    } else if (label == "Shaft Seal Temp") {
        return N2kts_ShaftSealTemperature;
    } else {
        return N2kts_SeaTemperature;
    }
}

double N2kHelper::convertN2kTemperatureToUserUnits(double temperature, UserUnitPrefs prefs)
{
    switch (prefs.tempUnitIndex) {
    case 0: // Kelvin
        return temperature;
    case 1: // Celsius
        return Convert::KelvinToCelsius(temperature);
    case 2: // Fahrenheit
        return Convert::KelvinToFahrenheit(temperature);
    default:
        return temperature;
    }
}

double N2kHelper::convertUserTemperatureToN2kUnits(double temperature, UserUnitPrefs prefs)
{
    switch (prefs.tempUnitIndex) {
    case 0: // Kelvin
        return temperature;
    case 1: // Celsius
        return Convert::CelsiusToKelvin(temperature);
    case 2: // Fahrenheit
        return Convert::FahrenheitToKelvin(temperature);
    default:
        return temperature;
    }
}

double N2kHelper::convertN2kPressureToUserUnits(double pressure, UserUnitPrefs prefs)
{
    // Convert PA to KPa
    switch (prefs.pressureUnitIndex) {
    case 0: // Pa
        return pressure;
    case 1: // kPa
        return pressure / 1000.0;
    case 2: // Bar
        return Convert::PaToBar(pressure);
    case 3: // PSI
        return Convert::PaToPsi(pressure);
    default:
        return pressure;
    }
}

double N2kHelper::convertUserPressureToN2kUnits(double pressure, UserUnitPrefs prefs)
{
    // Convert KPa to PA
    switch (prefs.pressureUnitIndex) {
    case 0: // Pa
        return pressure;
    case 1: // kPa
        return pressure * 1000.0;
    case 2: // Bar
        return Convert::BarToPa(pressure);
    case 3: // PSI
        return Convert::PsiToPa(pressure);
    default:
        return pressure;
    }
}

double N2kHelper::convertN2kAtmosphericPressureToUserUnits(double pressure, UserUnitPrefs prefs)
{
    switch (prefs.atmosphericPressureUnitIndex) {
    case 0: // hPa
        return pressure / 100.0;
    case 1: // inHg
        return Convert::hPaToInHg(pressure / 100.0);
    case 2: // mmHg
        return Convert::hPaToMmHg(pressure / 100.0);
    case 3: // Pa
        return pressure;
    case 4: // atm
        return Convert::hPaToAtm(pressure / 100.0);

    default:
        return pressure;
    }
}

double N2kHelper::convertUserAtmosphericPressureToN2kUnits(double pressure, UserUnitPrefs prefs)
{
    switch (prefs.atmosphericPressureUnitIndex) {
    case 0: // hPa
        return pressure * 100.0;
    case 1: // inHg
        return Convert::inHgToHpa(pressure) * 100.0;
    case 2: // mmHg
        return Convert::mmHgToHpa(pressure) * 100.0;
    case 3: // Pa
        return pressure;
    case 4: // atm
        return Convert::atmToHpa(pressure) * 100.0;

    default:
        return pressure;
    }
}

double N2kHelper::convertN2kFlowRateToUserUnits(double flowRate, UserUnitPrefs prefs)
{
    switch (prefs.flowRateUnitIndex) {
    case 0: // LPH
        return flowRate;
    case 1: // GPH
        return Convert::LphToGph(flowRate);
    default:
        return flowRate;
    }
}

double N2kHelper::convertUserFlowRateToN2kUnits(double flowRate, UserUnitPrefs prefs)
{
    switch (prefs.flowRateUnitIndex) {
    case 0: // LPH
        return flowRate;
    case 1: // GPH
        return Convert::GphToLph(flowRate);
    default:
        return flowRate;
    }
}

double N2kHelper::convertN2kVolumeToUserUnits(double volume, UserUnitPrefs prefs)
{
    switch (prefs.volumeUnitIndex) {
    case 0: // Liter
        return volume;
    case 1: // US Gallon
        //return Convert::CubicMeterToGal(volume);
        return Convert::LitersToUSGallons(volume);
    default:
        return volume;
    }
}

double N2kHelper::convertUserVolumeToN2kUnits(double volume, UserUnitPrefs prefs)
{
    switch (prefs.volumeUnitIndex) {
    case 0: // Liter
        return volume;
    case 1: // US Gallon
        return Convert::USGallonsToLiters(volume);
    default:
        return volume;
    }
}

double N2kHelper::convertN2kDistanceFarToUserUnits(double distance, UserUnitPrefs prefs)
{
    switch (prefs.distanceUnitIndex) {
    case 0: // Kilometer
        return distance / 1000.0;
    case 1: // Nautical Mile
        return distance / 1852.0;
    case 2: // Mile
        return distance / 1609.344;
    default:
        return distance;
    }
}

double N2kHelper::convertUserDistanceFarToN2kUnits(double distance, UserUnitPrefs prefs)
{
    switch (prefs.distanceUnitIndex) {
    case 0: // Kilometer
        return distance * 1000.0;
    case 1: // Nautical Mile
        return distance * 1852.0;
    case 2: // Mile
        return distance * 1609.344;
    default:
        return distance;
    }
}

double N2kHelper::convertN2kDistanceNearToUserUnits(double distance, UserUnitPrefs prefs)
{
    switch (prefs.distanceUnitIndex) {
    case 0: // Meter
        return distance;
        break;
    case 1: // Feet (NM & ft)
        return distance / 0.3048;
        break;
    default: // Foot
        return distance / 0.3048;
        break;
    }
}

double N2kHelper::convertUserDistanceNearToN2kUnits(double distance, UserUnitPrefs prefs)
{
    switch (prefs.distanceUnitIndex) {
    case 0: // Meter
        return distance;
        break;
    case 1: // Feet (NM & ft)
        return distance * 0.3048;
        break;
    default: // Foot
        return distance * 0.3048;
        break;
    }
}

double N2kHelper::convertN2kSpeedToUserUnits(double speed, UserUnitPrefs prefs)
{
    switch (prefs.speedUnitIndex) {
    case 0: // Meters per second
        return speed;
    case 1: // Kilometers per hour
        return Convert::MetersPerSecondToKmh(speed);
    case 2: // Miles per hour
        return Convert::MetersPerSecondToMph(speed);
    case 3: // Knots
        return Convert::MetersPerSecondToKnots(speed);
    default:
        return speed;
    }
}

double N2kHelper::convertUserSpeedToN2kUnits(double speed, UserUnitPrefs prefs)
{
    switch (prefs.speedUnitIndex) {
    case 0: // Meters per second
        return speed;
    case 1: // Kilometers per hour
        return Convert::KmhToMetersPerSecond(speed);
    case 2: // Miles per hour
        return Convert::MphToMetersPerSecond(speed);
    case 3: // Knots
        return Convert::KnotsToMetersPerSecond(speed);
    default:
        return speed;
    }
}

double N2kHelper::convertN2kDepthToUserUnits(double depth, UserUnitPrefs prefs)
{
    switch (prefs.depthUnitIndex) {
    case 0: // Meter
        return depth;
    case 1: // Feet
        return Convert::MetersToFeet(depth);
    case 2: // Fathoms
        return Convert::MetersToFathoms(depth);
    default:
        return depth;
    }
}

double N2kHelper::convertUserDepthToN2kUnits(double depth, UserUnitPrefs prefs)
{
    switch (prefs.depthUnitIndex) {
    case 0: // Meter
        return depth;
    case 1: // Feet
        return Convert::FeetToMeters(depth);
    case 2: // Fathoms
        return Convert::FathomsToMeters(depth);
    default:
        return depth;
    }
}

QString N2kHelper::convertN2kLatitudeToUserUnits(double latitude, UserUnitPrefs prefs)
{
    // "DD.DDDD", "DD°MM’SS", "DD°MM.MMM’"
    switch (prefs.latLongUnitIndex) {
    case 0: // DD.DDDDDD
        return QString::number(latitude, 'f', 6);
    case 1: // DD°MM’SS
        return Convert::DecimalToDMS(latitude, true);
    case 2: // DD°MM.MMM’
        return Convert::DecimalToDMM(latitude, true);
    default:
        return QString::number(latitude, 'f', 6);
    }
}

double N2kHelper::convertUserLatitudeToN2kUnits(QString latitude, UserUnitPrefs prefs)
{
    // "DD.DDDD", "DD°MM’SS", "DD°MM.MMM’"
    switch (prefs.latLongUnitIndex) {
    case 0: // DD.DDDD
        return latitude.toDouble();
    case 1: // DD°MM’SS
        return Convert::DMSToDecimal(latitude, true);
    case 2: // DD°MM.MMM’
        return Convert::DMMToDecimal(latitude, true);
    default:
        return latitude.toDouble();
    }
}

QString N2kHelper::convertN2kLongitudeToUserUnits(double longitude, UserUnitPrefs prefs)
{
    // "DD.DDDD", "DD°MM’SS", "DD°MM.MMM’"
    switch (prefs.latLongUnitIndex) {
    case 0: // DD.DDDDDD
        return QString::number(longitude, 'f', 6);
    case 1: // DD°MM’SS
        return Convert::DecimalToDMS(longitude, false);
    case 2: // DD°MM.MMM’
        return Convert::DecimalToDMM(longitude, false);
    default:
        return QString::number(longitude, 'f', 6);
    }
}

double N2kHelper::convertUserLongitudeToN2kUnits(QString longitude, UserUnitPrefs prefs)
{
    // "DD.DDDD", "DD°MM’SS", "DD°MM.MMM’"
    switch (prefs.latLongUnitIndex) {
    case 0: // DD.DDDDDD
        return longitude.toDouble();
    case 1: // DD°MM’SS
        return Convert::DMSToDecimal(longitude, false);
    case 2: // DD°MM.MMM’
        return Convert::DMMToDecimal(longitude, false);
    default:
        return longitude.toDouble();
    }
}

double N2kHelper::convertN2kFuelEconomyToUserUnits(double fuelEconomy, UserUnitPrefs prefs)
{
    switch (prefs.fuelEconomyUnitIndex) {
    case 0: // L/km
        return fuelEconomy;
    case 1: // L/nm
        return Convert::LkmToLnm(fuelEconomy);
    case 2: // gal/nm
        return Convert::LkmToUSGalNm(fuelEconomy);
    case 3: // MPG
        return Convert::LkmToMPG(fuelEconomy);
    case 4: // nm/L
        return Convert::LkmToNmL(fuelEconomy);
    case 5: // nm/gal
        return Convert::LkmToNmUSGal(fuelEconomy);
    default:
        return fuelEconomy;
    }
}

double N2kHelper::convertUserFuelEconomyToN2kUnits(double fuelEconomy, UserUnitPrefs prefs)
{
    switch (prefs.fuelEconomyUnitIndex) {
    case 0: // L/km
        return fuelEconomy;
    case 1: // L/nm
        return Convert::LnmToLkm(fuelEconomy);
    case 2: // gal/nm
        return Convert::USGalNmToLkm(fuelEconomy);
    case 3: // MPG
        return Convert::MPGToLkm(fuelEconomy);
    case 4: // nm/L
        return Convert::NmLToLkm(fuelEconomy);
    case 5: // nm/gal
        return Convert::NmGalToLkm(fuelEconomy);
    default:
        return fuelEconomy;
    }
}

double N2kHelper::convertN2kCurrencyToUserUnits(double currency, UserUnitPrefs prefs)
{
    // TODO: Implement currency conversion, assuming USD for now
    return currency;
    Q_UNUSED(prefs);
}

double N2kHelper::convertUserCurrencyToN2kUnits(double currency, UserUnitPrefs prefs)
{
    // TODO: Implement currency conversion, assuming USD for now
    return currency;
    Q_UNUSED(prefs);
}

double N2kHelper::convertN2kRateOfTurnToUserUnits(double rateOfTurn, UserUnitPrefs prefs)
{
    switch (prefs.rateOfTurnUnitIndex) {
    case 0: // rad/s
        return rateOfTurn;
    case 1: // deg/s
        return Convert::RadiansToDegrees(rateOfTurn);
    default:
        return rateOfTurn;
    }
}

double N2kHelper::convertUserRateOfTurnToN2kUnits(double rateOfTurn, UserUnitPrefs prefs)
{
    switch (prefs.rateOfTurnUnitIndex) {
    case 0: // rad/s
        return rateOfTurn;
    case 1: // deg/s
        return Convert::DegreesToRadians(rateOfTurn);
    default:
        return rateOfTurn;
    }
}

QString N2kHelper::convertN2kTimeToUserUnits(double time)
{
    return Convert::SecondsToFormattedTime_dayhhmmss(time);
}

double N2kHelper::convertUserTimeToN2kUnits(QString time)
{
    return Convert::FormattedTime_dayhhmmssToSeconds(time);
}

QString N2kHelper::convertN2kDateToUserFormat(uint16_t DaysSince1970, const UserUnitPrefs &prefs)
{
    if (DaysSince1970 >= N2kUInt16NA - 2) return QStringLiteral("Unavailable");
    // Base UTC date (Unix epoch start)
    QDate baseDate(1970, 1, 1);

    // Add days since 1970 to the base date
    QDate utcDate = baseDate.addDays(DaysSince1970);

    // Extract user's date preferences
    QString dateFormat = "MM/dd/yyyy";
    switch (prefs.dateUnitIndex) {
    case 0:
        dateFormat = "MM/dd/yyyy";
        break;
    case 1:
        dateFormat = "dd/MM/yyyy";
        break;
    case 2:
        dateFormat = "yyyy/MM/dd";
        break;
    }

    //qDebug() << "Date Format:" << dateFormat;
    // Create a QLocale to ensure formatting isn't influenced by the system's locale
    QLocale locale(QLocale::C); // Using "C" locale to ensure a neutral, standard format

    // Apply the date format using the specified locale
    return locale.toString(utcDate, dateFormat);
    /*
    // Base UTC date (Unix epoch start)
    QDate baseDate(1970, 1, 1);

    // Add days since 1970 to the base date
    QDate utcDate = baseDate.addDays(DaysSince1970);

    // Extract user's date preferences
    QString formattedDate;

    switch (prefs.dateUnitIndex) {
    case 0:
        // Format as dd/MM/yyyy
        formattedDate = QString("%1/%2/%3").arg(utcDate.day(), 2, 10, QChar('0')).arg(utcDate.month(), 2, 10, QChar('0')).arg(utcDate.year());
        break;
    case 1:
        // Format as MM/dd/yyyy
        formattedDate = QString("%1/%2/%3").arg(utcDate.month(), 2, 10, QChar('0')).arg(utcDate.day(), 2, 10, QChar('0')).arg(utcDate.year());
        break;
    case 2:
        // Format as yyyy/MM/dd
        formattedDate = QString("%1/%2/%3").arg(utcDate.year()).arg(utcDate.month(), 2, 10, QChar('0')).arg(utcDate.day(), 2, 10, QChar('0'));
        break;
    default:
        formattedDate = utcDate.toString("dd/MM/yyyy"); // Fallback format
        break;
    }

    qDebug() << "prefs.dateUnitIndex" << prefs.dateUnitIndex << "Formatted Date:" << formattedDate;
    return formattedDate;
*/
}

QString N2kHelper::convertN2kDateToUserFormat(uint16_t DaysSince1970, const UserUnitPrefs &prefs, int16_t localOffset, double secondsSinceMidnight)
{
    // A local calendar date requires all three fields. PGN 129033's widget
    // separately handles partial UTC data and labels that fallback explicitly.
    if (DaysSince1970 >= N2kUInt16NA - 2 || localOffset >= N2kInt16NA - 2
        || !std::isfinite(secondsSinceMidnight) || secondsSinceMidnight < 0 || secondsSinceMidnight >= 86401)
        return QStringLiteral("Unavailable");
    const double calendarSeconds = secondsSinceMidnight >= 86400 ? 86399 : secondsSinceMidnight;
    QDateTime utcDateTime = Convert::N2kDateTime(DaysSince1970, calendarSeconds, 0).addSecs(int(localOffset) * 60);

    // Extract user's date preferences and map it to the correct QDate format
    QString dateFormat;
    switch (prefs.dateUnitIndex) {
    case 0:
        dateFormat = "MM/dd/yyyy";
        break; // MM/DD/YYYY
    case 1:
        dateFormat = "dd/MM/yyyy";
        break; // DD/MM/YYYY
    case 2:
        dateFormat = "yyyy/MM/dd";
        break; // YYYY/MM/DD
    default:
        dateFormat = "dd/MM/yyyy";
        break;
    }

    // Return the formatted local date string
    return utcDateTime.date().toString(dateFormat);
}

uint16_t N2kHelper::convertUserFormattedDateToN2k(const QString &userFormattedDate, const UserUnitPrefs &prefs)
{
    // Extract user's date preferences
    QString dateFormat = "MM/dd/yyyy";
    switch (prefs.dateUnitIndex) {
    case 0:
        dateFormat = "MM/dd/yyyy";
        break;
    case 1:
        dateFormat = "dd/MM/yyyy";
        break;
    case 2:
        dateFormat = "yyyy/MM/dd";
        break;
    }

    // Parse the date based on the user's date format
    QDate date = QDate::fromString(userFormattedDate, dateFormat);

    // Calculate DaysSince1970
    QDate baseDate(1970, 1, 1);
    uint16_t DaysSince1970 = baseDate.daysTo(date);

    return DaysSince1970;
}

QString N2kHelper::convertN2kTimeToUserFormat(double SecondsSinceMidnight, const UserUnitPrefs &prefs)
{
    return convertN2kTimeToUserFormat(SecondsSinceMidnight, prefs, 0);
}

QString N2kHelper::convertN2kTimeToUserFormat(double SecondsSinceMidnight, const UserUnitPrefs &prefs, int16_t localOffset)
{
    if (!std::isfinite(SecondsSinceMidnight) || SecondsSinceMidnight < 0
        || SecondsSinceMidnight >= 86401 || localOffset >= N2kInt16NA - 2)
        return QStringLiteral("Unavailable");
    const bool leapSecond = SecondsSinceMidnight >= 86400;
    const int totalSeconds = leapSecond ? 86399 : static_cast<int>(SecondsSinceMidnight);
    const QTime time = QTime(0, 0).addSecs(totalSeconds + int(localOffset) * 60);

    // Extract user's time preferences and map it to the correct QTime format
    QString timeFormat;
    switch (prefs.timeUnitIndex) {
    case 0:
        timeFormat = "HH:mm";
        break; // HH:MM (24-hour)
    case 1:
        timeFormat = "hh:mm AP";
        break; // HH:MM AM/PM (12-hour)
    case 2:
        timeFormat = "HH:mm:ss";
        break; // HH:MM:SS (24-hour)
    case 3:
        timeFormat = "hh:mm:ss AP";
        break; // HH:MM:SS AM/PM (12-hour)
    default:
        timeFormat = "HH:mm:ss";
        break; // Default to HH:MM:SS
    }

    // Return the formatted local time string
    if (leapSecond) timeFormat.replace("ss", "'60'");
    return time.toString(timeFormat);
}

double N2kHelper::convertUserFormattedTimeToN2k(const QString &userFormattedTime, const UserUnitPrefs &prefs)
{
    // Extract user's time preferences
    QString timeFormat = "HH:mm:ss";
    switch (prefs.timeUnitIndex) {
    case 0:
        timeFormat = "HH:mm";
        break;
    case 1:
        timeFormat = "hh:mm AP";
        break;
    case 2:
        timeFormat = "HH:mm:ss";
        break;
    case 3:
        timeFormat = "hh:mm:ss AP";
        break;
    }

    // Parse the time based on the user's time format
    QTime time = QTime::fromString(userFormattedTime, timeFormat);

    // Calculate SecondsSinceMidnight
    double SecondsSinceMidnight = time.hour() * 3600 + time.minute() * 60 + time.second();

    return SecondsSinceMidnight;
}

QString N2kHelper::convertN2kHoursToUserFormat(double hoursInMinutes, const UserUnitPrefs &prefs)
{
    // Convert hours in minutes to total seconds
    double totalSeconds = hoursInMinutes * 60.0;

    // Use the existing N2kHelper method to format time based on user preferences
    return Convert::SecondsToFormattedTime_hhmmss(totalSeconds);
}

double N2kHelper::convertUserFormattedHoursToN2k(const QString &userFormattedHours, const UserUnitPrefs &prefs)
{
    // Extract user's time preferences
    QString timeFormat;
    switch (prefs.timeUnitIndex) {
    case 0:
        timeFormat = "HH:mm";
        break;
    case 1:
        timeFormat = "hh:mm AP";
        break;
    case 2:
        timeFormat = "HH:mm:ss";
        break;
    case 3:
        timeFormat = "hh:mm:ss AP";
        break;
    default:
        timeFormat = "HH:mm:ss";
        break;
    }

    // Parse the user input based on the time format
    QTime time = QTime::fromString(userFormattedHours, timeFormat);

    // Calculate hours in minutes (NMEA2000 format)
    double hoursInMinutes = time.hour() * 60.0 + time.minute() + time.second() / 60.0;

    return hoursInMinutes;
}

std::pair<uint16_t, double> N2kHelper::getSystemDateAndTime(const QDateTime &dateTime)
{
    // Get the date as days since January 1, 1970
    QDate date = dateTime.date();
    QDate epochStart(1970, 1, 1);
    uint16_t systemDate = static_cast<uint16_t>(epochStart.daysTo(date));

    // Calculate SystemTime as milliseconds since midnight
    QTime time = dateTime.time();
    quint32 systemTime = static_cast<quint32>(time.msecsSinceStartOfDay());

    return {systemDate, systemTime};
}

std::tuple<uint16_t, double, int16_t> N2kHelper::getLocalSystemDateTime(const QDateTime &localDateTime)
{
    // Calculate SystemDate as days since January 1, 1970
    QDate date = localDateTime.date();
    QDate epochStart(1970, 1, 1);
    uint16_t systemDate = static_cast<uint16_t>(epochStart.daysTo(date));

    // Calculate SystemTime as milliseconds since midnight
    QTime time = localDateTime.time();
    quint32 systemTime = static_cast<quint32>(time.msecsSinceStartOfDay());

    // Calculate local time offset in minutes from UTC
    //int16_t localOffset = static_cast<int16_t>(localDateTime.offsetFromUtc() / 60);
    int16_t localOffset = static_cast<int16_t>(0);

    qDebug() << "System Date:" << systemDate << "System Time:" << systemTime << "Local Offset:" << localOffset;
    return {systemDate, systemTime, localOffset};
}

tN2kEngineDiscreteStatus1 N2kHelper::getEngineDiscreteStatus1(uint16_t status1)
{
    return tN2kEngineDiscreteStatus1(status1); // Initialize union with status1 value
}

uint16_t N2kHelper::setEngineDiscreteStatus1(tN2kEngineDiscreteStatus1 status1)
{
    return status1.Status; // Return the uint16_t status from the union
}
