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
// Qt PermissionError can also mean a device lock; do not guess from translated OS text.
inline QString canableSerialDiagnostic(QString path, QSerialPort::SerialPortError error,
                                       QString detail, bool linuxHost =
#ifdef Q_OS_LINUX
                                       true
#else
                                       false
#endif
                                       )
{
    if (linuxHost && !path.startsWith('/') && (path.startsWith("ttyACM") || path.startsWith("ttyUSB")))
        path.prepend("/dev/");
    QString reason;
    switch (error) {
    case QSerialPort::PermissionError:
        reason = QCoreApplication::translate("SerialPort", "permission denied or device busy.");
        if (linuxHost)
            reason += QCoreApplication::translate("SerialPort", " On Linux, check whether your user has access to the serial device (typically through the dialout group), or whether another application is using the port. Group membership changes require logging out and back in. See Help > Troubleshooting > Linux Serial Port Permissions.");
        else
            reason += QCoreApplication::translate("SerialPort", " Check serial-port permissions and whether another application is using the port.");
        break;
    case QSerialPort::DeviceNotFoundError:
        reason = QCoreApplication::translate("SerialPort", "device missing or disconnected. Check the cable, Refresh ports and select the attached device."); break;
    case QSerialPort::OpenError:
        reason = QCoreApplication::translate("SerialPort", "device already open or unavailable. Check whether another application is using the port and verify access permissions."); break;
    case QSerialPort::ResourceError:
        reason = QCoreApplication::translate("SerialPort", "device disconnected or unavailable. Check the cable and Refresh ports."); break;
    default:
        detail = detail.trimmed();
        while (detail.endsWith('.') || detail.endsWith('!')) detail.chop(1);
        reason = detail.isEmpty() ? QCoreApplication::translate("SerialPort", "serial device error.") : detail + '.';
    }
    return QCoreApplication::translate("SerialPort", "CANable SLCAN: Cannot open %1 — %2 Reconnect explicitly after resolving the issue.").arg(path, reason);
}
#endif
