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

#ifndef CONVERT_H
#define CONVERT_H

#include <QDateTime>
#include <QTime>
#include <QTimeZone>
#include "qlabel.h"

class Convert
{
public:
    Convert();

    // Conversion methods
    // Temperature
    static double FahrenheitToKelvin(double tempF);
    static double KelvinToFahrenheit(double tempK);
    static double CelsiusToKelvin(double tempC);
    static double KelvinToCelsius(double tempK);
    static double CelsiusToFahrenheit(double tempC);
    static double FahrenheitToCelsius(double tempF);
    // Pressure
    static double PsiToPascal(double psi);
    static double PascalToPsi(double pascal);
    static double GphToLph(double gph);
    static double KPaToBar(double kPa);
    static double BarToKPa(double bar);
    static double KPaToPsi(double bar);
    static double PsiToKPa(double psi);
    static double PaToBar(double Pa);
    static double BarToPa(double bar);
    static double PaToPsi(double pa);
    static double PsiToPa(double psi);
    // Volume
    static double LphToGph(double lph);
    static double GphToCmph(double gph);
    static double CmphToGph(double cmph);
    static double GallonToCubicMeter(double gal);
    static double CubicMeterToGal(double cubicMeter);
    static double PercentageToLiters(double percentage, double tankCapacityLiters);
    static double LitersToUSGallons(double liters);
    static double USGallonsToLiters(double usGallons);
    static unsigned char QStringHexToUInt(QString str);
    // Time and Formatting of Time
    static double HoursToSeconds(double hours);
    static double SecondsToHours(double seconds);
    static double MinutesToHours(double minutes);
    static double HoursToMinutes(double hours);
    static QString SecondsToFormattedTime_hhmmss(double seconds);
    static QString SecondsToFormattedTime_mmss(const QString &secondsString);
    static QString SecondsToFormattedTime_dayhhmm(double timeInSeconds);
    static QString SecondsToFormattedTime_dayhhmmss(double timeInSeconds);
    static double FormattedTime_dayhhmmssToSeconds(const QString &dayhhmmss);
    static QString MinutesToFormattedTime_hhmm(double minutes);
    static QString HoursToFormattedTime_hhmmss(double hours);
    static QString HoursToFormattedTime(double hours);
    static QString FormattedTime_mmssToSeconds(const QString &minutesSecondsString);
    static QDateTime N2kDateTime(uint16_t days, double seconds, int16_t offsetMinutes = 0);
    static QString ToLocalDateTime(uint16_t DaysSince1970, double SecondsSinceMidnight, int16_t LocalOffset);
    static QString ToLocalDateTime(qint64 timeUTC, int localOffset);
    static QDateTime UnixTimestampToQDateTime(double timestamp);
    static double QDateTimeToUnixTimestamp(const QDateTime &date);
    static double Convert0183TimeUTCToSeconds(const QString &timeUTC);
    static QString Nmea0183TimeToLocalTimeFormatted(const QString &timeUTC, int localOffset);
    static QDateTime Nmea0183TimeToLocalDateTime(const QString &timeUTC, int localOffset);
    static QString SkTimeToLocalTimeFormatted(QString skTime, int localOffset);
    static double UnixTimeToSecondsSinceMidnight(double unixTime);
    static uint16_t UnixTimeToDaysSince1970(double unixTime);
    static uint16_t Nmea0183ZDAToDaysSince1970(int &GPSDay, int &GPSMonth, int &GPSYear, int &LZD, int &LZMD);
    static time_t SecondsSinceMidnightToUnixTime(double secondsSinceMidnight, const QDate &date);
    // Navigation
    static double RadiansToDegrees(double radians);
    static double DegreesToRadians(double degrees);
    static double MagneticToTrue(double degrees, double variation);
    static double MagneticToTrueHeading(double magneticHeading, double magneticVariation, bool isVariationEast);
    static double TrueToMagentic(double degrees, double variation);
    static double TrueToMagneticHeading(double trueHeading, double magneticVariation, bool isVariationEast);
    static QString DecimalToDMM(double decimalDegrees, bool isLatitude);
    static QString DecimalToDMS(double decimalDegrees, bool isLatitude);
    static double DmmToDecimal(int degrees, double minutes, char direction);
    static double DMMToDecimal(const QString &dmmString, bool isLatitude);
    static double DmsToDecimal(int degrees, int minutes, double seconds, char direction);
    static double DMSToDecimal(const QString &dmsString, bool isLatitude);
    static double Convert0183DegreesToDecimal(const QString &coordinate, const QString &direction);
    static double Convert0183AltitudeToMeters(double altitude, const QString &unit);
    static double RadiansPerSecondToDegreesPerSecond(double radiansPerSecond);
    static double DegreesPerSecondToRadiansPerSecond(double degreesPerSecond);
    // Distance
    static double MetersToNauticalMiles(double meters);
    static double NauticalMilesToMeters(double nm);
    static double NauticalMilesToMiles(double nm);
    static double NauticalMilesToKilometers(double nm);
    static double KilometersToNauticalMiles(double kilometers);
    static double KmhToMetersPerSecond(double kmh);
    static double MilesToNauticalMiles(double miles);
    static double MilesToMeters(double miles);
    static double MetersToMiles(double meters);
    static double FeetToDecimeters(double feet);
    static double MetersPerSecondToKnots(double ms);
    static double KnotsToMetersPerSecond(double knots);
    static double MetersPerSecondToKmh(double ms);
    static double MetersPerSecondToMph(double ms);
    static double MphToMetersPerSecond(double mph);
    static double MetersToFeet(double meters);
    static double MetersToFathoms(double meters);
    static double FeetToMeters(double feet);
    static double FathomsToMeters(double fathoms);
    static double FeetToNauticalFeet(double feet);
    static double NauticalFeetToFeet(double nauticalFeet);
    static double MetersToNauticalFeet(double meters);
    static double NauticalFeetToMeters(double nauticalFeet);
    static double Convert0183GeoidHeightToMeters(double height, const QString &unit);
    static QString formatDistanceNearFar(double distance, uint8_t distanceUnitIndex, QLabel *labelUnitsDistance);
    // Nautical Economy Units (L/nm, gal/nm, MPG, nm/L, nm/gal)
    static double LitersPerHourToLitersPerNauticalMile(double litersPerHour, double speedInKnots);
    static double LitersPerHourToUSGallonsPerNauticalMile(double litersPerHour, double speedInKnots);
    static double LitersPerHourToMilesPerUSGallon(double litersPerHour, double speedInKnots);
    static double LitersPerHourToKilometersPerLiter(double litersPerHour, double speedInKnots);
    static double LphToKmh(double fuelRateLh, double fuelEfficiencyKmL);
    static double LitersPerNauticalMileToGallonsPerNauticalMile(double litersPerNauticalMile);
    static double LitersPerNauticalMileToMilesPerUSGallon(double litersPerNauticalMile);
    static double LitersPerNauticalMileToNauticalMilesPerLiter(double litersPerNauticalMile);
    static double USGallonsPerNauticalMileToNauticalMilesPerLiter(double galPerNm);
    static double MilesPerUSGallonToNauticalMilesPerLiter(double mpg);
    static double NauticalMilesPerUSGallonToNauticalMilesPerLiter(double nmPerGal);
    static double LitersPerNauticalMileToNauticalMilesPerGallon(double litersPerNauticalMile);
    static double GallonsPerNauticalMileToLitersPerNauticalMile(double gallonsPerNauticalMile);
    static double MilesPerUSGallonToLitersPerNauticalMile(double milesPerGallon);
    static double NauticalMilesPerLiterToLitersPerNauticalMile(double nmPerLiter);
    static double NauticalMilesPerLiterToUSGallonsPerNauticalMile(double nmPerLiter);
    static double NauticalMilesPerLiterToMilesPerUSGallon(double nmPerLiter);
    static double NauticalMilesPerLiterToNauticalMilesPerGallon(double nmPerLiter);
    static double NauticalMilesPerUSGallonToLitersPerNauticalMile(double nauticalMilesPerGallon);
    static double LitersPerNauticalMileToKilometersPerLiter(double litersPerNauticalMile);
    static double GallonsPerNauticalMileToMilesPerGallon(double gallonsPerNauticalMile);
    static double USGallonsPerNauticalMileToKilometersPerLiter(double gallonsPerNauticalMile);
    static double MilesPerUSGallonToKilometersPerLiter(double milesPerGallon);
    static double KilometersPerLiterToLitersPerNauticalMile(double kilometersPerLiter);
    static double KilometersPerLiterToMilesPerUSGallon(double kilometersPerLiter);
    static double CostPerLiterToCostPerGallon(double costPerLiter);
    static double CostPerGallonToCostPerLiter(double costPerGallon);
    // Convert from L/km to other units
    static double LkmToLnm(double value);
    static double LkmToUSGalNm(double value);
    static double LkmToMPG(double value);
    static double LkmToNmL(double value);
    static double LkmToNmUSGal(double value);
    // Convert back to L/km from other units
    static double LnmToLkm(double value);
    static double USGalNmToLkm(double value);
    static double MPGToLkm(double value);
    static double NmLToLkm(double value);
    static double NmGalToLkm(double value);
    static double m3sToLph(double m3s);
    static double lphToM3s(double lph);
    static double M3sToLkm(double m3s, double velocityMs);
    static double LkmToM3s(double lkm, double velocityMs);
    // Other conversions
    static double TrimPercentageToDegrees(double percentage);
    static double DegreesToTrimPercentage(double degrees);
    static double ThrottleDegreesToSpeedMs(double degrees, double maxSpeedMs);
    static double NormalizeHeading(double heading);
    // Pressure conversions
    // hPa to other units
    static double hPaToInHg(double hPa) { return hPa * 0.0295299830714; }
    static double hPaToMmHg(double hPa) { return hPa * 0.750061561303; }
    static double hPaToPa(double hPa) { return hPa * 100.0; }
    static double hPaToKPa(double hPa) { return hPa * 0.1; }
    static double hPaToAtm(double hPa) { return hPa / 1013.25; }
    // Other units to hPa
    static double inHgToHpa(double inHg) { return inHg / 0.0295299830714; }
    static double mmHgToHpa(double mmHg) { return mmHg / 0.750061561303; }
    static double paToHpa(double pa) { return pa / 100.0; }
    static double kPaToHpa(double kPa) { return kPa * 10.0; }
    static double atmToHpa(double atm) { return atm * 1013.25; }

private:
    // Conversion constants
    static constexpr double NAUTICAL_MILES_PER_MILE = 0.868976;
    static constexpr double MILES_PER_NAUTICAL_MILE = 1.15078;
    static constexpr double METERS_PER_NAUTICAL_MILE = 1852.0;
    static constexpr double LITERS_PER_US_GALLON = 3.78541;
    static constexpr double LITERS_PER_IMPERIAL_GALLON = 4.54609;
    static constexpr double KILOMETERS_PER_MILE = 1.60934;
    //static constexpr double LITER_TO_US_GALLON = 0.264172;
    static constexpr double LITER_TO_IMPERIAL_GALLON = 0.219969;
    static constexpr double KM_TO_NM = 1.852;
    static constexpr double LITERS_TO_GALLONS = 0.264172;
    static constexpr double MILES_TO_KM = 1.60934;
    static constexpr double MILES_PER_KILOMETER = 0.621371;
    static constexpr double GALLONS_PER_LITER = 0.264172;
};

#endif // CONVERT_H
