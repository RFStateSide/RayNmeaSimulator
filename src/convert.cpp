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

#include "convert.h"
#include <QRegularExpression>
//#include "helper.h" // Removed 8/29/2024 v0.5.3

Convert::Convert()
{

}

double Convert::FahrenheitToKelvin(double tempF)
{
    return (tempF - 32) * 5.0 / 9.0 + 273.15;
}

double Convert::KelvinToFahrenheit(double tempK)
{
    return (tempK - 273.15) * 9.0 / 5.0 + 32;
}

double Convert::CelsiusToKelvin(double tempC)
{
    return tempC + 273.15;
}

double Convert::KelvinToCelsius(double tempK)
{
    return tempK - 273.15;
}

double Convert::CelsiusToFahrenheit(double tempC)
{
    return tempC * 9 / 5 + 32;
}

double Convert::FahrenheitToCelsius(double tempF)
{
    return (tempF - 32) * 5 / 9;
}

double Convert::PsiToPascal(double psi)
{
    return psi * 6894.75729;
}

double Convert::PascalToPsi(double pascal)
{
    return pascal / 6894.75729;
}

//Gallon per hour(US) to Liter per hour
double Convert::GphToLph(double gph)
{
    return gph * 3.785411784;
}

double Convert::KPaToBar(double kPa)
{
    return kPa / 100;
}

double Convert::BarToKPa(double bar)
{
    return bar * 100;
}

double Convert::KPaToPsi(double bar)
{
    return bar * 0.14503773773;
}

double Convert::PsiToKPa(double psi)
{
    return psi / 0.14503773773;
}

double Convert::PaToBar(double Pa)
{
    return Pa / 100000.0;
}

double Convert::BarToPa(double bar)
{
    return bar * 100000.0;
}

double Convert::PaToPsi(double pa)
{
    return pa * 0.0001450377;
}

double Convert::PsiToPa(double psi)
{
    return psi / 0.0001450377;
}

double Convert::LphToGph(double lph)
{
    return lph / 3.785411784;
}

//Gallon per hour(US) to Cubic Meters per hour
double Convert::GphToCmph(double gph)
{
    return gph * .003785411784;
}

double Convert::CmphToGph(double cmph)
{
    return cmph / 0.003785411784;
}

double Convert::HoursToSeconds(double hours)
{
    return hours * 3600;
}

double Convert::SecondsToHours(double seconds)
{
    return seconds / 3600;
}

double Convert::MinutesToHours(double minutes)
{
    return minutes / 60;
}

double Convert::HoursToMinutes(double hours)
{
    return hours * 60;
}

QString Convert::SecondsToFormattedTime_hhmmss(double seconds)
{
    // Calculate hours, minutes, and seconds
    int totalSeconds = static_cast<int>(seconds);
    int hrs = totalSeconds / 3600;
    int mins = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;
    int msecs = static_cast<int>((seconds - totalSeconds) * 1000);

    //    QString t = QString::number(hrs) + ":" + QString::number(mins) + ":" + QString::number(seconds);
    //    return t;

    // Create a QTime object
    QTime time(hrs % 24, mins, secs, msecs); // % 24 to handle hours overflow

    // Format the QTime object as a string in the desired format (HH:mm:ss.zzz)
    return time.toString("HH:mm:ss");
}

QString Convert::SecondsToFormattedTime_mmss(const QString &secondsString)
{
    bool ok;
    int totalSeconds = secondsString.toInt(&ok);
    if (!ok) {
        return "00:00"; // Handle invalid input
    }

    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    // Format the string as mm:ss
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

QString Convert::SecondsToFormattedTime_dayhhmm(double timeInSeconds)
{
    int totalSeconds = static_cast<int>(timeInSeconds);
    int days = totalSeconds / 86400;
    int hours = (totalSeconds % 86400) / 3600;
    int minutes = (totalSeconds % 3600) / 60;

    return QString("%1 day %2:%3")
        .arg(days)
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'));
}

QString Convert::SecondsToFormattedTime_dayhhmmss(double timeInSeconds)
{
    int totalSeconds = static_cast<int>(timeInSeconds);
    int days = totalSeconds / 86400;
    int hours = (totalSeconds % 86400) / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    if (days == 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1 day %2:%3:%4")
            .arg(days)
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}

double Convert::FormattedTime_dayhhmmssToSeconds(const QString &dayhhmmss)
{
    int days = 0, hours = 0, minutes = 0, seconds = 0;

    // Split the time into parts based on spaces
    QStringList parts = dayhhmmss.split(' ');

    // If there's a day part, it should be at the start
    if (parts.size() > 1 && parts[0].endsWith("day")) {
        bool ok;
        days = parts[0].split("day")[0].toInt(&ok);
        if (!ok) {
            qWarning() << "Invalid day format.";
            return -1;  // Return an invalid value to indicate error
        }
        parts.removeFirst();  // Remove day part to handle the rest
    }

    // Process the time part
    if (!parts.isEmpty()) {
        QStringList timeParts = parts[0].split(':');

        // Depending on how many parts there are, parse accordingly
        switch (timeParts.size()) {
        case 3:  // hh:mm:ss
            hours = timeParts[0].toInt();
            minutes = timeParts[1].toInt();
            seconds = timeParts[2].toInt();
            break;
        case 2:  // mm:ss
            minutes = timeParts[0].toInt();
            seconds = timeParts[1].toInt();
            break;
        case 1:  // ss
            seconds = timeParts[0].toInt();
            break;
        default:
            qWarning() << "Invalid time format. Expected 'hh:mm:ss', 'mm:ss', or 'ss'.";
            return -1;  // Return an invalid value to indicate error
        }
    }

    // Calculate total seconds
    double totalSeconds = days * 86400 + hours * 3600 + minutes * 60 + seconds;

    return totalSeconds;
}

QString Convert::MinutesToFormattedTime_hhmm(double minutes)
{
    // Calculate hours and remaining minutes
    int totalMinutes = static_cast<int>(minutes);
    int hours = totalMinutes / 60;
    int mins = totalMinutes % 60;
    //int secs = static_cast<int>((minutes - totalMinutes) * 60);

    QString t = QString::number(hours) + ":" + QString::number(mins);
    return t;

    // Create a QTime object
    //QTime time(hours, mins, secs);

    // Format the QTime object as a string in the desired format (HH:mm:ss)
    //return time.toString("HH:mm:ss");
}

QString Convert::FormattedTime_mmssToSeconds(const QString &minutesSecondsString)
{
    QStringList parts = minutesSecondsString.split(':');
    if (parts.size() != 2) {
        return "0"; // Handle invalid input
    }

    bool ok;
    int minutes = parts[0].toInt(&ok);
    if (!ok) {
        return "0"; // Handle invalid minutes
    }

    int seconds = parts[1].toInt(&ok);
    if (!ok) {
        return "0"; // Handle invalid seconds
    }

    int totalSeconds = minutes * 60 + seconds;
    return QString::number(totalSeconds);
}

QString Convert::HoursToFormattedTime_hhmmss(double hours)
{
    // Calculate hours, minutes, and seconds
    int totalMinutes = static_cast<int>(hours * 60);
    int hrs = totalMinutes / 60;
    int mins = totalMinutes % 60;
    int secs = static_cast<int>((hours * 3600) - (hrs * 3600) - (mins * 60));

    // Create a QTime object
    QTime time(hrs % 24, mins, secs); // % 24 to handle hours overflow

    // Format the QTime object as a string in the desired format (HH:mm:ss)
    return time.toString("HH:mm:ss");
}

QDateTime Convert::N2kDateTime(uint16_t days, double seconds, int16_t offsetMinutes)
{
    return QDateTime(QDate(1970, 1, 1).addDays(days), QTime(0, 0), QTimeZone::utc())
        .addMSecs(static_cast<qint64>(seconds * 1000)).toOffsetFromUtc(offsetMinutes * 60);
}

QString Convert::ToLocalDateTime(uint16_t days, double seconds, int16_t offsetMinutes)
{
    return N2kDateTime(days, seconds, offsetMinutes).toString("M/d/yyyy H:mm:ss");
}

QString Convert::ToLocalDateTime(qint64 timeUTC, int localOffset)
{
    // Create a QDateTime from the given UTC time
    QDateTime utcDateTime = QDateTime::fromMSecsSinceEpoch(timeUTC, QTimeZone::utc());

    // Apply the local time offset (in minutes)
    QDateTime localDateTime = utcDateTime.addSecs(localOffset * 60);

    // Return formatted local date and time
    return localDateTime.toString("M/d/yyyy H:mm:ss");
}

/* Refactored removing the need to use helper class in the Convert class
QString Convert::Nmea0183TimeToLocalTimeFormatted(const QString &timeUTC, int localOffset)
{
    // NMEA 0183 UTC time is typically in "HHMMSS" or "HHMMSS.sss" format.
    if (timeUTC.length() < 6) {
        return QString("Invalid Time Format");
    }

    // Extract hours, minutes, and seconds from the string
    int hours = timeUTC.mid(0, 2).toInt();
    int minutes = timeUTC.mid(2, 2).toInt();
    int seconds = timeUTC.mid(4, 2).toInt();
    int milliseconds = 0;

    // Check for fractional seconds if provided
    if (timeUTC.length() > 6 && timeUTC[6] == '.') {
        milliseconds = timeUTC.mid(7).toInt();
    }

    // Create a QTime object for the UTC time
    QTime utcTime(hours, minutes, seconds, milliseconds);

    if (!utcTime.isValid()) {
        return QString("Invalid Time");
    }

    // Get the current date
    QDate currentDate = QDate::currentDate();

    // Combine date and time into a QDateTime object
    QDateTime utcDateTime(currentDate, utcTime, Qt::UTC);

    // Convert to local time using the offset
//    QDateTime localDateTime = utcDateTime.addSecs(localOffset * 3600);
    QDateTime localDateTime = utcDateTime.addSecs(localOffset * 60);

    // Check if Date is correct and if not fix
    QDateTime correctedDateTime = helper::correctDateTime(localDateTime);

    // Format the local date-time as a string
    //return localDateTime.toString("M/d/yyyy H:mm:ss");
    return correctedDateTime.toString("M/d/yyyy H:mm:ss");
}*/

// Added 8/29/2024 v0.5.3 to replace above method
QDateTime Convert::Nmea0183TimeToLocalDateTime(const QString &timeUTC, int localOffset)
{
    // Extract hours, minutes, and seconds from the string
    int hours = timeUTC.mid(0, 2).toInt();
    int minutes = timeUTC.mid(2, 2).toInt();
    int seconds = timeUTC.mid(4, 2).toInt();
    int milliseconds = 0;

    // Check for fractional seconds if provided
    if (timeUTC.length() > 6 && timeUTC[6] == '.') {
        milliseconds = timeUTC.mid(7).toInt();
    }

    // Create a QTime object for the UTC time
    QTime utcTime(hours, minutes, seconds, milliseconds);

    // Get the current date
    QDate currentDate = QDate::currentDate();

    // Combine date and time into a QDateTime object
    QDateTime utcDateTime(currentDate, utcTime, QTimeZone::utc());

    // Convert to local time using the offset
    return utcDateTime.addSecs(localOffset * 60);

}

QString Convert::SkTimeToLocalTimeFormatted(QString skTime, int localOffset)
{
    // Define the format that matches the input Signal K time format
    QString inputFormat = "ddd MMM d HH:mm:ss yyyy 'GMT'";

    // Parse the input time string into a QDateTime object
    QDateTime gmtDateTime = QDateTime::fromString(skTime, inputFormat);
    gmtDateTime.setTimeZone(QTimeZone::utc()); // Explicitly set it as UTC

    if (!gmtDateTime.isValid()) {
        return QString(); // Return an empty string if parsing failed
    }

    // Apply the local offset (in seconds) to the GMT time
    QDateTime localDateTime = gmtDateTime.addSecs(localOffset);

    // Define the desired output format
    QString outputFormat = "M/d/yyyy H:mm:ss";

    // Return the formatted local time string
    return localDateTime.toString(outputFormat);
}

double Convert::UnixTimeToSecondsSinceMidnight(double unixTime)
{
    // Convert the unixTime to QDateTime object (using UTC for consistency)
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(unixTime), Qt::UTC);

    // Extract the time of day from QDateTime
    QTime time = dateTime.time();

    // Calculate seconds since midnight
    double secondsSinceMidnight = time.hour() * 3600 + time.minute() * 60 + time.second();

    return secondsSinceMidnight;
}

uint16_t Convert::UnixTimeToDaysSince1970(double unixTime)
{
    // Convert the unixTime to QDateTime object (using UTC for consistency)
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(unixTime), Qt::UTC);

    // Get the start date (epoch date: 1970-01-01)
    QDate epochDate(1970, 1, 1);

    // Calculate the number of days between the epoch date and the date from the unixTime
    int daysSince1970 = epochDate.daysTo(dateTime.date());

    // Cast to uint16_t (ensure the value is in the valid range)
    return static_cast<uint16_t>(daysSince1970);
}

uint16_t Convert::Nmea0183ZDAToDaysSince1970(int &GPSDay, int &GPSMonth, int &GPSYear, int &LZD, int &LZMD)
{
    // Construct a QDate object using the provided day, month, and year
    QDate date(GPSYear, GPSMonth, GPSDay);

    // Check if the date is valid
    if (!date.isValid()) {
        return 0; // Return 0 as an error indicator if the date is invalid
    }

    // ZDA's calendar fields are UTC. Offset fields are independent metadata.
    Q_UNUSED(LZD);
    Q_UNUSED(LZMD);
    return static_cast<uint16_t>(QDate(1970, 1, 1).daysTo(date));
}

time_t Convert::SecondsSinceMidnightToUnixTime(double secondsSinceMidnight, const QDate &date)
{
    // Ensure the provided date is valid
    if (!date.isValid()) {
        return static_cast<time_t>(-1); // Return -1 to indicate an error if the date is invalid
    }

    // Create a QTime object representing the time since midnight
    QTime time = QTime::fromMSecsSinceStartOfDay(static_cast<int>(secondsSinceMidnight * 1000));
    if (!time.isValid()) {
        return static_cast<time_t>(-1); // Return -1 to indicate an error if the time is invalid
    }

    // Combine the date and time into a QDateTime object
    QDateTime dateTime(date, time, Qt::UTC); // Use UTC for consistency

    // Convert to Unix time (seconds since epoch)
    return static_cast<time_t>(dateTime.toSecsSinceEpoch());
}

double Convert::LitersPerHourToLitersPerNauticalMile(double litersPerHour, double speedInKnots)
{
    if (speedInKnots == 0) {
        return 0;
    }
    return litersPerHour / speedInKnots;
}

double Convert::LitersPerHourToUSGallonsPerNauticalMile(double litersPerHour, double speedInKnots)
{
    double gallonsPerHour = litersPerHour / 3.78541;  // Convert LPH to GPH
    return gallonsPerHour / speedInKnots;  // GPH / Knots = Gallons per Nautical Mile
}

double Convert::LitersPerHourToMilesPerUSGallon(double litersPerHour, double speedInKnots)
{
    double gallonsPerHour = litersPerHour / 3.78541; // 3.78541 liters per US gallon
    double milesPerHour = speedInKnots * 1.15078; // 1.50779 knots to MPH
    return milesPerHour / gallonsPerHour;
}

double Convert::LitersPerHourToKilometersPerLiter(double litersPerHour, double speedInKnots)
{
    double kilometersPerHour = speedInKnots * KM_TO_NM;
    return kilometersPerHour / litersPerHour;
}

double Convert::LphToKmh(double fuelRateLh, double fuelEfficiencyKmL)
{
    if (fuelRateLh == 0.0) {
        return 0.0; // Avoid division by zero
    }

    return fuelRateLh / fuelEfficiencyKmL;
}

double Convert::LitersPerNauticalMileToGallonsPerNauticalMile(double litersPerNauticalMile)
{
    return litersPerNauticalMile / LITERS_PER_US_GALLON;
}

double Convert::LitersPerNauticalMileToMilesPerUSGallon(double litersPerNauticalMile)
{
     return (1.15078 / litersPerNauticalMile) * 3.78541;
}

double Convert::LitersPerNauticalMileToNauticalMilesPerLiter(double litersPerNauticalMile)
{
    return 1.0 / litersPerNauticalMile;
}

double Convert::USGallonsPerNauticalMileToNauticalMilesPerLiter(double galPerNm)
{
    if (galPerNm <= 0) {
        qWarning() << "Invalid Gal/NM value: must be greater than zero.";
        return 0.0;
    }
    // Convert Gal/NM to L/NM, then to NM/L
    double litersPerNm = galPerNm * 3.78541;  // 3.78541 liters per US gallon
    return 1.0 / litersPerNm;
}

double Convert::MilesPerUSGallonToNauticalMilesPerLiter(double mpg)
{
    if (mpg <= 0) {
        qWarning() << "Invalid MPG value: must be greater than zero.";
        return 0.0;
    }
    // Convert MPG to nautical miles per gallon, then to nautical miles per liter
    double nmPerGal = mpg * 1.15078;
    return nmPerGal / 3.78541;
}

double Convert::NauticalMilesPerUSGallonToNauticalMilesPerLiter(double nmPerGal)
{
    if (nmPerGal <= 0) {
        qWarning() << "Invalid NM/Gal value: must be greater than zero.";
        return 0.0;
    }
    // Convert NM/Gal to NM/L
    return nmPerGal / 3.78541;
}

double Convert::LitersPerNauticalMileToNauticalMilesPerGallon(double litersPerNauticalMile)
{
    return (1.0 / litersPerNauticalMile) * LITERS_PER_US_GALLON;
}

double Convert::GallonsPerNauticalMileToLitersPerNauticalMile(double gallonsPerNauticalMile)
{
    return gallonsPerNauticalMile / LITERS_TO_GALLONS;
}

double Convert::LitersPerNauticalMileToKilometersPerLiter(double litersPerNauticalMile)
{
    return (NAUTICAL_MILES_PER_MILE * KILOMETERS_PER_MILE) / litersPerNauticalMile;
}

double Convert::USGallonsPerNauticalMileToKilometersPerLiter(double gallonsPerNauticalMile)
{
    if (gallonsPerNauticalMile <= 0) {
        qWarning() << "Invalid gallons/NM value: must be greater than zero.";
        return 0.0;
    }
    return (1.15078 * 1.60934) / (gallonsPerNauticalMile * 3.78541);
}

double Convert::MilesPerUSGallonToLitersPerNauticalMile(double milesPerGallon)
{
    return (1.15078 / milesPerGallon) * 3.78541;
}

double Convert::NauticalMilesPerLiterToLitersPerNauticalMile(double nmPerLiter)
{
    if (nmPerLiter <= 0) {
        qWarning() << "NauticalMilesPerLiterToLitersPerNauticalMile Invalid NM/L value: must be greater than zero.";
        return 0.0;
    }
    return 1.0 / nmPerLiter;
}

double Convert::NauticalMilesPerLiterToUSGallonsPerNauticalMile(double nmPerLiter)
{
    if (nmPerLiter <= 0) {
        qWarning() << "NauticalMilesPerLiterToGallonsPerNauticalMile Invalid NM/L value: must be greater than zero.";
        return 0.0;
    }
    double litersPerNauticalMile = 1.0 / nmPerLiter;
    return litersPerNauticalMile / 3.78541;
}

double Convert::NauticalMilesPerLiterToMilesPerUSGallon(double nmPerLiter)
{
    if (nmPerLiter <= 0) {
        qWarning() << "NauticalMilesPerLiterToMPG Invalid NM/L value: must be greater than zero.";
        return 0.0;
    }
    double nauticalMilesPerGallon = nmPerLiter * 3.78541;
    return nauticalMilesPerGallon * 1.15078;
}

double Convert::NauticalMilesPerLiterToNauticalMilesPerGallon(double nmPerLiter)
{
    if (nmPerLiter <= 0) {
        qWarning() << "NauticalMilesPerLiterToNauticalMilesPerGallon Invalid NM/L value: must be greater than zero.";
        return 0.0;
    }
    return nmPerLiter * LITERS_PER_US_GALLON;
}

double Convert::NauticalMilesPerUSGallonToLitersPerNauticalMile(double nauticalMilesPerGallon)
{
    return (1.0 / nauticalMilesPerGallon) * 3.78541;
}

double Convert::MilesPerUSGallonToKilometersPerLiter(double milesPerGallon)
{
    if (milesPerGallon <= 0) {
        qWarning() << "Invalid MPG value: must be greater than zero.";
        return 0.0;
    }
    return 1.60934 / milesPerGallon;
}

double Convert::KilometersPerLiterToLitersPerNauticalMile(double kilometersPerLiter)
{
    if (kilometersPerLiter <= 0) {
        qWarning() << "Invalid KM/L value: must be greater than zero.";
        return 0.0;
    }
    return (1.15078 * 1.60934) / kilometersPerLiter;
}

double Convert::KilometersPerLiterToMilesPerUSGallon(double kilometersPerLiter)
{
    if (kilometersPerLiter <= 0) {
        qWarning() << "Invalid KM/L value: must be greater than zero.";
        return 0.0;
    }
    return kilometersPerLiter * 0.621371 / 3.78541;
}

double Convert::CostPerLiterToCostPerGallon(double costPerLiter)
{
    return costPerLiter / LITERS_TO_GALLONS;
}

double Convert::CostPerGallonToCostPerLiter(double costPerGallon)
{
    return costPerGallon / LITERS_PER_US_GALLON;
}

double Convert::LkmToLnm(double value)
{
    return value * 1.852;
}

double Convert::LkmToUSGalNm(double value)
{
    return value * 1.852 / 3.785411784;
}

double Convert::LkmToMPG(double value)
{
    if (value == 0.0) return 0.0;
    return 3.785411784 / (value * 1.609344);
}

double Convert::LkmToNmL(double value)
{
    if (value == 0.0) return 0.0;
    return 1.0 / (value * 1.852);
}

double Convert::LkmToNmUSGal(double value)
{
    if (value == 0.0) return 0.0;
    return 1.0 / (value * 1.852 / 3.785411784);
}

double Convert::LnmToLkm(double value)
{
    return value / 1.852;
}

double Convert::USGalNmToLkm(double value)
{
    return value / (1.852 / 3.785411784);
}

double Convert::MPGToLkm(double value)
{
    if (value == 0.0) return 0.0;
    return 3.785411784 / (value * 1.609344);
}

double Convert::NmLToLkm(double value)
{
    if (value == 0.0) return 0.0;
    return 1.0 / (value * 1.852);
}

double Convert::NmGalToLkm(double value)
{
    if (value == 0.0) return 0.0;
    return 1.0 / (value * 1.852 / 3.785411784);
}

double Convert::m3sToLph(double m3s)
{
     return m3s * 1000 * 3600;
}

double Convert::lphToM3s(double lph)
{
    return lph / (1000.0 * 3600.0);
}

double Convert::M3sToLkm(double m3s, double velocityMs)
{
    if (velocityMs == 0.0) return 0.0;
    return (m3s * 1000000.0) / velocityMs;
}

double Convert::LkmToM3s(double lkm, double velocityMs)
{
    if (velocityMs == 0.0) return 0.0;
    return (lkm * velocityMs) / 1000000.0;
}

double Convert::GallonToCubicMeter(double gal)
{
    return gal * 0.0037854117840007;
}

double Convert::CubicMeterToGal(double cubicMeter)
{
    return cubicMeter * 264.1720523581;
}

QDateTime Convert::UnixTimestampToQDateTime(double timestamp)
{
    QDateTime origin = QDateTime::fromSecsSinceEpoch(0, QTimeZone::utc());
    return origin.addSecs(static_cast<qint64>(timestamp));
}

double Convert::QDateTimeToUnixTimestamp(const QDateTime &date)
{
    QDateTime origin = QDateTime::fromSecsSinceEpoch(0, QTimeZone::utc());
    qint64 diff = origin.secsTo(date.toUTC());
    return std::floor(static_cast<double>(diff));
}

double Convert::RadiansToDegrees(double radians)
{
    double degrees = (180 / 3.141592653589793) * radians;
    return degrees;
}

double Convert::DegreesToRadians(double degrees)
{
    double radians = degrees * (3.141592653589793 / 180);
    return radians;
}

double Convert::MetersToNauticalMiles(double meters)
{
    return meters / 1852.0;
}

double Convert::NauticalMilesToMeters(double nm)
{
    double miles = nm * METERS_PER_NAUTICAL_MILE;
    return miles;
}

double Convert::NauticalMilesToMiles(double nm)
{
    double miles = nm * 1.15078;
    return miles;
}

double Convert::NauticalMilesToKilometers(double nm)
{
    double km = nm * 1.852;
    return km;
}

double Convert::KilometersToNauticalMiles(double kilometers)
{
    return kilometers / 1.852;
}

double Convert::KmhToMetersPerSecond(double kmh)
{
    return kmh / 3.6;
}

double Convert::MilesToNauticalMiles(double miles)
{
    return miles / 1.15078;
}

double Convert::MilesToMeters(double miles)
{
    double meters = miles * 1609.344;
    return meters;
}

double Convert::MetersToMiles(double meters)
{
    double miles = meters / 1609.344;
    return miles;
}

double Convert::FeetToDecimeters(double feet)
{
    return feet * 3.048;
}

double Convert::MagneticToTrue(double degrees, double variation)
{
    double magentic = degrees + variation;
    // Normalize the heading to be within 0-360 degrees
    if (magentic > 360) {
        magentic -= 360;
    } else if (magentic < 0) {
        magentic += magentic;
    }
    return magentic;

    /*    double magnetic = degrees + variation;
    // Normalize to be within 0-360 degrees
    while (magnetic >= 360) {
        magnetic -= 360;
    }
    while (magnetic < 0) {
        magnetic += 360;
    }
    return magnetic;
*/
}

double Convert::MagneticToTrueHeading(double magneticHeading, double magneticVariation, bool isVariationEast)
{
    double trueHeading;

    if (isVariationEast) {
        trueHeading = magneticHeading - magneticVariation;
    } else {
        trueHeading = magneticHeading + magneticVariation;
    }

    // Normalize the heading to be within 0-360 degrees
    if (trueHeading < 0) {
        trueHeading += 360.0;
    } else if (trueHeading >= 360.0) {
        trueHeading -= 360.0;
    }

    return trueHeading;
}

double Convert::TrueToMagentic(double degrees, double variation)
{
    double tr = degrees - variation;
    // Normalize the heading to be within 0-360 degrees
    if (tr > 360) {
        tr -= 360;
    } else if (tr < 0) {
        tr += degrees;
    }
    return tr;
    /*
    double trueHeading = degrees - variation;
    // Normalize to be within 0-360 degrees
    while (trueHeading >= 360) {
        trueHeading -= 360;
    }
    while (trueHeading < 0) {
        trueHeading += 360;
    }
    return trueHeading;
*/
}

double Convert::TrueToMagneticHeading(double trueHeading, double magneticVariation, bool isVariationEast)
{
    double magneticHeading;

    if (isVariationEast) {
        magneticHeading = trueHeading - magneticVariation;
    } else {
        magneticHeading = trueHeading + magneticVariation;
    }

    // Normalize the heading to be within 0-360 degrees
    if (magneticHeading < 0) {
        magneticHeading += 360.0;
    } else if (magneticHeading >= 360) {
        magneticHeading -= 360.0;
    }

    return magneticHeading;
}

double Convert::PercentageToLiters(double percentage, double tankCapacityLiters)
{
    return (percentage / 100.0) * tankCapacityLiters;
}

double Convert::LitersToUSGallons(double liters)
{
    return liters / 3.785411784;
}

double Convert::USGallonsToLiters(double usGallons)
{
    return usGallons * 3.785411784;
}

unsigned char Convert::QStringHexToUInt(QString str)
{
    // Remove "0x" prefix if present
    if (str.startsWith("0x")) {
        str = str.mid(2);
    }

    bool ok;
    unsigned int intValue = str.toUInt(&ok, 16);

    // Check if the conversion was successful
    if (ok) {
        // Cast to unsigned char
        unsigned char ucharValue = static_cast<unsigned char>(intValue);
        return ucharValue;
    } else {
        qWarning() << "Conversion QStringToUInt failed!";
        return 0;
    }
}

double Convert::MetersPerSecondToKnots(double ms)
{
    return ms * 3600.0 / 1852.0;
}

double Convert::KnotsToMetersPerSecond(double knots)
{
    return knots * 1852.0 / 3600.0;
}

double Convert::MetersPerSecondToKmh(double ms)
{
    return ms *= 3.6;
}

double Convert::MetersPerSecondToMph(double ms)
{
    return ms / 0.44704;
}

double Convert::MphToMetersPerSecond(double mph)
{
    return mph * 0.44704;
}

double Convert::MetersToFeet(double meters)
{
    return meters / 0.3048;
}

double Convert::MetersToFathoms(double meters)
{
    return meters / 1.8288;
}

double Convert::FeetToMeters(double feet)
{
    return feet * 0.3048;
}

double Convert::FathomsToMeters(double fathoms)
{
    return fathoms * 1.8288;
}

double Convert::FeetToNauticalFeet(double feet)
{
    constexpr double conversionFactor = 1.15078;
    return feet / conversionFactor;
}

double Convert::NauticalFeetToFeet(double nauticalFeet)
{
    constexpr double conversionFactor = 1.15078;
    return nauticalFeet * conversionFactor;
}

double Convert::MetersToNauticalFeet(double meters)
{
    constexpr double metersToFeet = 3.28084;
    constexpr double conversionFactor = 1.15078; // Nautical feet factor derived from nautical and US mile difference
    return (meters * metersToFeet) / conversionFactor;
}

double Convert::NauticalFeetToMeters(double nauticalFeet)
{
    constexpr double metersToFeet = 3.28084;
    constexpr double conversionFactor = 1.15078; // Same factor for consistency
    return (nauticalFeet * conversionFactor) / metersToFeet;
}

// Outdrive trim percentage to degrees where 0% = -5 deg and 100% = 45 deg
double Convert::TrimPercentageToDegrees(double percentage)
{
    return -5 + 0.5 * percentage;
}

// Outdrive trim percentage to degrees where -5 deg = 0% and 45 deg = 100%
double Convert::DegreesToTrimPercentage(double degrees)
{
    return 2 * (degrees + 5);
}

double Convert::ThrottleDegreesToSpeedMs(double degrees, double maxSpeedMs)
{
    // Ensure the throttle position is within the range of 0 to 100
    if (degrees < 0.0)
        degrees = 0.0;
    if (degrees > 100.0)
        degrees = 100.0;

    // Calculate the speed as a proportion of maxSpeedMs
    double speedMs = (degrees / 100.0) * maxSpeedMs;
    return speedMs;
}

double Convert::NormalizeHeading(double heading)
{
    if (heading < 0) {
        heading += 360;
    } else if (heading >= 360) {
        heading -= 360;
    }

    return heading;
}

// Convert decimal degrees to DMM with direction
QString Convert::DecimalToDMM(double decimalDegrees, bool isLatitude)
{
    // Round before splitting so 59.9999 minutes carries into the next degree.
    const qint64 thousandths = qRound64(fabs(decimalDegrees) * 60000.0);
    int degrees = thousandths / 60000;
    double minutes = (thousandths % 60000) / 1000.0;
    char direction = isLatitude ? (decimalDegrees >= 0 ? 'N' : 'S') : (decimalDegrees >= 0 ? 'E' : 'W');

    return QString("%1° %2' %3").arg(degrees).arg(minutes, 0, 'f', 3).arg(direction);
}

// Convert decimal degrees to DMS with direction
QString Convert::DecimalToDMS(double decimalDegrees, bool isLatitude)
{
    const qint64 hundredths = qRound64(fabs(decimalDegrees) * 360000.0);
    int degrees = hundredths / 360000;
    int minutes = (hundredths % 360000) / 6000;
    double seconds = (hundredths % 6000) / 100.0;
    char direction = isLatitude ? (decimalDegrees >= 0 ? 'N' : 'S') : (decimalDegrees >= 0 ? 'E' : 'W');

    return QString("%1° %2' %3\" %4").arg(degrees).arg(minutes).arg(seconds, 0, 'f', 2).arg(direction);
}

// Convert DMM to decimal degrees
double Convert::DmmToDecimal(int degrees, double minutes, char direction)
{
    double decimalDegrees = degrees + (minutes / 60.0);
    if (direction == 'S' || direction == 'W') {
        decimalDegrees = -decimalDegrees;
    }
    return decimalDegrees;
}

double Convert::DMMToDecimal(const QString &dmmString, bool isLatitude)
{
    QRegularExpression regex(R"((\d+)°\s+([\d.]+)'\s+([NSEW]))");
    QRegularExpressionMatch match = regex.match(dmmString);

    if (!match.hasMatch()) {
        qWarning() << "Invalid DMM format: " << dmmString;
        return 0.0; // Return 0 or handle error as needed
    }

    // Extract degrees, minutes, and direction from the string
    int degrees = match.captured(1).toInt();
    double minutes = match.captured(2).toDouble();
    char direction = match.captured(3).toUpper().toLatin1().at(0);

    // Convert DMM to decimal degrees
    double decimalDegrees = degrees + (minutes / 60.0);

    // Determine the sign based on direction
    if (isLatitude && (direction == 'S')) {
        decimalDegrees = -decimalDegrees;
    } else if (!isLatitude && (direction == 'W')) {
        decimalDegrees = -decimalDegrees;
    }

    return decimalDegrees;
}

// Convert DMS to decimal degrees
double Convert::DmsToDecimal(int degrees, int minutes, double seconds, char direction)
{
    double decimalDegrees = degrees + (minutes / 60.0) + (seconds / 3600.0);
    if (direction == 'S' || direction == 'W') {
        decimalDegrees = -decimalDegrees;
    }
    return decimalDegrees;
}

double Convert::DMSToDecimal(const QString &dmsString, bool isLatitude)
{
    QRegularExpression regex(R"((\d+)°\s+(\d+)'(?:\s+([\d.]+)\")?\s+([NSEW]))");
    QRegularExpressionMatch match = regex.match(dmsString);

    if (!match.hasMatch()) {
        qWarning() << "Invalid DMS format: " << dmsString;
        return 0.0; // Return 0 or handle error as needed
    }

    // Extract degrees, minutes, seconds, and direction from the string
    int degrees = match.captured(1).toInt();
    int minutes = match.captured(2).toInt();
    double seconds = match.captured(3).isEmpty() ? 0.0 : match.captured(3).toDouble();
    char direction = match.captured(4).toUpper().toLatin1().at(0);

    // Convert DMS to decimal degrees
    double decimalDegrees = degrees + (minutes / 60.0) + (seconds / 3600.0);

    // Determine the sign based on direction
    if (isLatitude && (direction == 'S')) {
        decimalDegrees = -decimalDegrees;
    } else if (!isLatitude && (direction == 'W')) {
        decimalDegrees = -decimalDegrees;
    }

    return decimalDegrees;
}

double Convert::Convert0183DegreesToDecimal(const QString &coordinate, const QString &direction)
{
    bool ok;
    int degLength = (direction == "N" || direction == "S") ? 2 : 3;
    double degrees = coordinate.left(degLength).toDouble(&ok);
    if (!ok) return 0;

    double minutes = coordinate.mid(degLength).toDouble(&ok);
    if (!ok) return 0;

    double decimalDegrees = degrees + (minutes / 60.0);

    if (direction == "S" || direction == "W") {
        decimalDegrees = -decimalDegrees;
    }

    return decimalDegrees;

}

double Convert::Convert0183AltitudeToMeters(double altitude, const QString &unit)
{
    if (unit == "M") {
        return altitude;
    } else if (unit == "F") {
        return altitude * 0.3048;
    }
    return altitude; // Default to meters if the unit is unrecognized
}

double Convert::RadiansPerSecondToDegreesPerSecond(double radiansPerSecond)
{
    return radiansPerSecond * 57.2958;
}

double Convert::DegreesPerSecondToRadiansPerSecond(double degreesPerSecond)
{
    return degreesPerSecond / 57.2958;
}

double Convert::Convert0183GeoidHeightToMeters(double height, const QString &unit)
{
    if (unit == "M") {
        return height;
    } else if (unit == "F") {
        return height * 0.3048;
    }
    return height; // Default to meters if the unit is unrecognized
}

/**
 * @brief DialogAutoPilotSimulator::formatDistanceNearFar
 * @param distance in meters
 * @param distanceUnitIndex 0=km & m, 1=NM & ft, 2=mi & ft
 * @return formatted distance string
 * @details Formats the distance to the next waypoint based on the selected distance unit index.
 *         The distance is formatted in kilometers and meters, nautical miles and feet, or miles and feet.
 *         The method also updates the label for the distance units.
 */
QString Convert::formatDistanceNearFar(double distance, uint8_t distanceUnitIndex, QLabel *labelUnitsDistance)
{
    QString formattedDistance;

    switch (distanceUnitIndex) {
    case 0: // "km & m"
        if (distance >= 1000.0) {
            formattedDistance = QString::number(distance * .001, 'f', 2);
            labelUnitsDistance->setText("km");
        } else {
            double meters = distance;
            formattedDistance = QString::number(meters, 'f', 0);
            labelUnitsDistance->setText("m");
        }
        break;

    case 1: // "NM & ft"
        if (distance >= 185.2) {
            formattedDistance = QString::number(Convert::MetersToNauticalMiles(distance), 'f', 2);
            labelUnitsDistance->setText("NM");
        } else {
            //double feet = distance * 6076.12;  // 1 NM = 6076.12 ft
            formattedDistance = QString::number(Convert::MetersToFeet(distance), 'f', 0);
            labelUnitsDistance->setText("ft");
        }
        break;

    case 2: // "mi & ft"
        if (distance >= 528.0) {
            formattedDistance = QString::number(Convert::MetersToMiles(distance), 'f', 2);
            labelUnitsDistance->setText("mi");
        } else {
            //double feet = distance * 5280.0;  // 1 mi = 5280 ft
            formattedDistance = QString::number(Convert::MetersToFeet(distance), 'f', 0);
            labelUnitsDistance->setText("ft");
        }
        break;

    default:
        formattedDistance = "Invalid unit index";
        break;
    }

    return formattedDistance;
}

double Convert::Convert0183TimeUTCToSeconds(const QString &timeUTC)
{
    if (timeUTC.length() < 6) {
        return 0;
    }

    int hours = timeUTC.mid(0, 2).toInt();
    int minutes = timeUTC.mid(2, 2).toInt();
    double seconds = timeUTC.mid(4).toDouble();

    return hours * 3600 + minutes * 60 + seconds;
}
