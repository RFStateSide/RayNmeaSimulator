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

#include "helper.h"
#include "appsettings.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QSerialPortInfo>
#include <QStringList>
#include <QVector>
#include "QObject"

helper::helper() {}

QString helper::HomePath()
{
#ifdef RAYNMEA_TESTING
    return TestProfile::root();
#else
    if (DevelopmentProfile::active()) return TestProfile::root();
    return QDir::homePath();
#endif
}

bool helper::QStringToBool(const QString &string)
{
    QString lower = string.trimmed().toLower();
    return (lower == "true" || lower == "1" || lower == "yes");
}

QList<QString> helper::GetCommPorts()
{
    const auto serialPortInfos = QSerialPortInfo::availablePorts();
    QList<QString> ports;

    for (const QSerialPortInfo& portInfo : serialPortInfos)
    {
#ifdef Q_OS_WIN
        if (portInfo.portName().startsWith("COM")) {
            ports.append(portInfo.portName());
        }
#else
        ports.append(portInfo.portName());
#endif
    }
    ports.sort();
    ports.insert(0, "None");

    return ports;

}

QList<QString> helper::GetBaudRates()
{
    QList<QString> listBaudRates;
    QList<qint32> baudRates = QSerialPortInfo::standardBaudRates();
    for (qint32 baudRate : baudRates) {
        listBaudRates.append(QString::number(baudRate));
    }
    return listBaudRates;
}

QSerialPort::BaudRate helper::GetBaudRateFromString(const QString &baudRateStr)
{
    bool ok;
    int baudRate = baudRateStr.toInt(&ok);

    if (!ok) {
        return QSerialPort::Baud115200;
    }

    switch (baudRate) {
    case 1200:
        return QSerialPort::Baud1200;
    case 2400:
        return QSerialPort::Baud2400;
    case 4800:
        return QSerialPort::Baud4800;
    case 9600:
        return QSerialPort::Baud9600;
    case 19200:
        return QSerialPort::Baud19200;
    case 38400:
        return QSerialPort::Baud38400;
    case 57600:
        return QSerialPort::Baud57600;
    case 115200:
        return QSerialPort::Baud115200;
    //case 230400:  return QSerialPort::Baud230400;
    //case 460800:  return QSerialPort::Baud460800;
    //case 921600:  return QSerialPort::Baud921600;
    default:
        return QSerialPort::Baud115200;
    }
}

int helper::CalculateCheckSum(const QByteArray &data) {
    quint8 sum = 0;
    for (quint8 b : data) {
        sum += b;
    }
    return 256 - sum;
}

bool helper::VerifyCheckSum(const QByteArray &data) {
    if (data.size() < 2) {
        return false;  // Not enough data to contain a checksum
    }

    quint8 sum = 0;
    for (int i = 0; i < data.size() - 1; ++i) {
        sum += static_cast<quint8>(data[i]);
    }

    if (static_cast<quint8>(data[data.size() - 1]) == 256 - sum) {
        return true;
    } else {
        return false;
    }
}

void helper::Edit2ByteInt(QByteArray &data, int offset, int val)
{
    data[offset + 1] = static_cast<char>(val);
    data[offset] = static_cast<char>(val >> 8);
}

void helper::Edit2ByteDouble(QByteArray &data, int offset, double val, double scale)
{
    int ival = static_cast<int>(std::round(val / scale));
    data[offset + 1] = static_cast<char>(ival);
    data[offset] = static_cast<char>(ival >> 8);
}

bool helper::FileExists(const QString &filePath)
{
    QFileInfo checkFile(filePath);
    return checkFile.exists() && checkFile.isFile();
}

bool helper::IsDoubleValid(double value)
{
    return !qIsNaN(value) && !qIsInf(value) && value != 0.0;
}

bool helper::IsDaysSince1970Valid(int daysSince1970)
{
    // Get the current date
    QDate yesterdayDate = QDate::currentDate().addDays(-1);

    // Calculate the future date, 20 years from now
    QDate futureDate = yesterdayDate.addYears(20);

    // Calculate the date from DaysSince1970
    QDate dateFromDays = QDate::fromJulianDay(QDate(1970, 1, 1).toJulianDay() + daysSince1970);

    // Check if the date is valid and within the range
    return dateFromDays.isValid() && dateFromDays >= yesterdayDate && dateFromDays <= futureDate;
}

QString helper::GetTodaysDate()
{
    QDate currentDate = QDate::currentDate();
    return currentDate.toString("MM/dd/yyyy");
}

QList<QString> helper::GetListTripLogs(const QDir &folderPath)
{
    QList<QString> tripLogs;

    // Create a copy of folderPath to modify it
    QDir dirCopy = folderPath;

    // Set the name filters to look for .csv files
    QStringList filters;
    filters << "*trip*.csv";
    dirCopy.setNameFilters(filters);

    // Get the list of files in the directory
    QFileInfoList fileList = dirCopy.entryInfoList(QDir::Files);

    // Iterate through the files and add the ones that match the criteria to the list
    for (const QFileInfo &fileInfo : fileList) {
        if (fileInfo.fileName().contains("trip", Qt::CaseInsensitive) && fileInfo.suffix() == "csv")
        {
            tripLogs.append(fileInfo.fileName());
        }
    }

    return tripLogs;
}

QList<QString> helper::GetListFuelLogs(const QDir &folderPath)
{
    QList<QString> fuelLogs;

    // Create a copy of folderPath to modify it
    QDir dirCopy = folderPath;

    // Set the name filters to look for .csv files
    QStringList filters;
    filters << "*fuel*.csv";
    dirCopy.setNameFilters(filters);

    // Get the list of files in the directory
    QFileInfoList fileList = dirCopy.entryInfoList(QDir::Files);

    // Iterate through the files and add the ones that match the criteria to the list
    for (const QFileInfo &fileInfo : fileList) {
        if (fileInfo.fileName().contains("fuel", Qt::CaseInsensitive) && fileInfo.suffix() == "csv") {
            fuelLogs.append(fileInfo.fileName());
        }
    }

    return fuelLogs;
}


bool helper::createEmptyFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not create file:" << filePath;
        return false;
    }

    file.close();
    return true;
}

QString helper::GetApplicationPath()
{
    return QCoreApplication::applicationDirPath();
}

int helper::GetLocalOffsetFromSystemTime()
{
    // Get the current local time
    QDateTime localDateTime = QDateTime::currentDateTime();

    // Get the current UTC time
    QDateTime utcDateTime = localDateTime.toUTC();

    // Calculate the offset in seconds
    int offsetInSeconds = localDateTime.offsetFromUtc();

    return offsetInSeconds;

/*    // Optionally, convert the offset to hours and minutes
    int offsetHours = offsetInSeconds / 3600;
    return offsetHours;
*/
}

double helper::GetRpiCpuTemperature()
{
    // Path to the file that contains the CPU temperature
    QFile file("/sys/class/thermal/thermal_zone0/temp");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open the CPU temperature file";
        return -1;  // Return an error value
    }

    QTextStream in(&file);
    QString tempString = in.readLine();
    file.close();

    bool ok;
    double temperature = tempString.toDouble(&ok) / 1000.0; // Convert from millidegrees to degrees
    if (!ok) {
        qWarning() << "Failed to convert temperature string to double";
        return -1;  // Return an error value
    }

    return temperature;
}

/**
 * @brief Corrects the date of a QDateTime object to the current date while keeping the time.
 * @param sensorDateTime The QDateTime object to correct.
 * @return The corrected QDateTime object.
 */
QDateTime helper::correctDateTime(const QDateTime &sensorDateTime)
{
    // Get the current system date
    QDate currentDate = QDate::currentDate();

    // Extract the time from the sensor's QDateTime
    QTime sensorTime = sensorDateTime.time();

    // Check if the date from the sensor matches the current date
    if (sensorDateTime.date() != currentDate) {
        // If the date doesn't match, correct it to the current date while keeping the time
        QDateTime correctedDateTime(currentDate, sensorTime);
        return correctedDateTime;
    }

    // If the date matches, return the original QDateTime
    return sensorDateTime;
}

QString helper::GetPathToData()
{
    QString appPath = GetApplicationPath();
    return appPath + "/Data";
}

QStringList helper::getEngineList(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning().noquote() << "Cannot read engine catalog:" << filePath << file.errorString()
                            << "Check permissions or repair the catalog; it has not been replaced.";
        return QStringList();
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    QStringList engineNames;
    if (error.error != QJsonParseError::NoError || !doc.isObject()
        || !doc.object().value("engines").isArray()) {
        qWarning().noquote() << "Invalid engine catalog:" << filePath
                            << "JSON offset" << error.offset << error.errorString()
                            << "Expected an object with an engines array. Repair the file; it has not been replaced.";
        return {};
    }
    if (doc.isObject()) {
        QJsonObject jsonObj = doc.object();
        QJsonArray enginesArray = jsonObj["engines"].toArray();
        for (const QJsonValue &engine : enginesArray) {
            const auto name = engine.toObject()["name"].toString();
            if (name.trimmed().isEmpty() || engineNames.contains(name)) {
                qWarning().noquote() << "Invalid engine catalog:" << filePath
                                    << "Engine names must be nonempty and unique. Repair the file; it has not been replaced.";
                return {};
            }
            engineNames << name;
        }
    }

    return engineNames;
}

QStringList helper::getListWidgetLayouts(const QString &filePath)
{
    QStringList layouts;

    QDir dirCopy = filePath;

    // Set the name filters to look for .json files
    QStringList filters;
    filters << "*layout*.json";
    dirCopy.setNameFilters(filters);

    // Get the list of files in the directory
    QFileInfoList fileList = dirCopy.entryInfoList(QDir::Files);

    // Iterate through the files and add the ones that match the criteria to the list
    for (const QFileInfo &fileInfo : fileList) {
        if (fileInfo.fileName().contains("layout", Qt::CaseInsensitive) && fileInfo.suffix() == "json") {
            layouts.append(fileInfo.fileName());
        }
    }

    return layouts;
}
