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

#ifndef TRANSPORTCAPABILITIES_H
#define TRANSPORTCAPABILITIES_H

#include <QString>
#include <QtGlobal>

namespace TransportCapabilities {
// Qt targets set QT_NO_DEBUG for Release and derive QT_DEBUG for Debug.
// Keep the build distinction here; settings cannot override release policy.
#ifdef QT_DEBUG
inline constexpr bool experimentalActisenseTx = true;
#else
inline constexpr bool experimentalActisenseTx = false;
#endif

inline bool canTransmit(const QString &transport)
{
    if (transport == "Actisense") return experimentalActisenseTx;
    return transport == "Network NMEA2000" || transport == "SocketCan" || transport == "SignalK"
        || transport == "CANable SLCAN" || transport == "Waveshare USB-CAN-A";
}

inline QString actisenseNotice()
{
    return experimentalActisenseTx
        ? QStringLiteral("Actisense NGT-1 transmit support is experimental. Use only on an isolated test network.")
        : QStringLiteral("Actisense NGT-1 operates in receive-only mode. Transmitting is disabled in this build.");
}
}
#endif
