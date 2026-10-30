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

#ifndef HELPER_H
#define HELPER_H

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSerialPort>
#include <QTime>
#include <cmath> // For std::round
#include <qstring.h>

class helper
{
public:
    helper();
    static QString HomePath();
    static bool QStringToBool(const QString &string);
    static QList<QString> GetCommPorts();
    static QList<QString> GetBaudRates();
    static QSerialPort::BaudRate GetBaudRateFromString(const QString &baudRateStr);
    static int CalculateCheckSum(const QByteArray &data);
    static bool VerifyCheckSum(const QByteArray &data);
    static void Edit2ByteInt(QByteArray &data, int offset, int val);
    static void Edit2ByteDouble(QByteArray &data, int offset, double val, double scale = 1.0);

    static bool FileExists(const QString &filePath);
    static bool IsDoubleValid(double value);
    static bool IsDaysSince1970Valid(int daysSince1970);
    static QString GetTodaysDate();
    static QList<QString> GetListTripLogs(const QDir &folderPath);
    static QList<QString> GetListFuelLogs(const QDir &folderPath);
    static bool createEmptyFile(const QString &filePath);
    static QString GetApplicationPath();
    static int GetLocalOffsetFromSystemTime();
    static double GetRpiCpuTemperature();
    static QDateTime correctDateTime(const QDateTime& sensorDateTime);
    static QString GetPathToData();
    static QStringList getEngineList(const QString &filePath);
    static QStringList getListWidgetLayouts(const QString &filePath);
};

#endif // HELPER_H
