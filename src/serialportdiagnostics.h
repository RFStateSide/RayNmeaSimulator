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

#ifndef SERIALPORTDIAGNOSTICS_H
#define SERIALPORTDIAGNOSTICS_H
#include <QSerialPort>
#include <QCoreApplication>

inline QString serialPortDiagnostic(const QString &path, QSerialPort::SerialPortError error,
                                    const QString &detail)
{
    QString advice;
    switch (error) {
    case QSerialPort::DeviceNotFoundError:
        advice = QCoreApplication::translate("SerialPort", "Device missing or disconnected. Check the cable, Refresh ports and select the attached device."); break;
    case QSerialPort::PermissionError:
        advice = QCoreApplication::translate("SerialPort", "Access denied or device busy. Check serial-port permissions and whether another application has the port open."); break;
    case QSerialPort::OpenError:
        advice = QCoreApplication::translate("SerialPort", "Cannot open device. Check whether another application has the port open and verify access permissions."); break;
    case QSerialPort::ResourceError:
        advice = QCoreApplication::translate("SerialPort", "Device disconnected or unavailable. Check the cable and Refresh ports before reconnecting."); break;
    default: break;
    }
    return QString("%1: %2 %3").arg(path, detail, advice).trimmed();
}
#endif
