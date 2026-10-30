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

#pragma once
#include <QSet>
#include "N2kMsg.h"
namespace RouteServicePolicy {
inline bool allowed(const tN2kMsg &m) {
    if (m.Destination > 251 && m.Destination != 255)
        return false;
    if (m.PGN == 59904) {
        if (m.DataLen != 3)
            return false;
        const unsigned p = m.Data[0] | (m.Data[1] << 8) | (m.Data[2] << 16);
        return p == 60928 || p == 126996;
    }
    if (m.PGN != 126208 || m.DataLen < 11 || m.Data[0] != 0 ||
        m.Destination > 251)
        return false;
    for (int i = 4; i < 10; ++i)
        if (m.Data[i] != 255)
            return false;
    const int p = m.Data[1] | (m.Data[2] << 8) | (m.Data[3] << 16),
              count = m.Data[10];
    if (m.DataLen != 11 + 3 * count || count > 4)
        return false;
    QSet<int> fields;
    for (int i = 11; i < m.DataLen; i += 3) {
        const int f = m.Data[i], v = m.Data[i + 1] | (m.Data[i + 2] << 8);
        if (fields.contains(f) || v > 65532)
            return false;
        fields.insert(f);
        if (p == 130064) {
            if (f != 1 && f != 2)
                return false;
        } else if (p == 130065) {
            if (f != 1 && f != 2 && f != 4)
                return false;
        } else if (p == 130066) {
            if (f != 1 && f != 2)
                return false;
        } else if (p == 130067) {
            if (f != 1 && f != 2 && f != 4 && f != 5)
                return false;
        } else
            return false;
        if (p != 130066 && f == 2 && (v < 1 || v > 4))
            return false;
    }
    if (p == 130064)
        return true;
    if (p == 130065)
        return fields.contains(1) && fields.contains(4);
    if (p == 130066)
        return fields.contains(1) && fields.contains(2);
    return p == 130067 && fields.contains(1) && fields.contains(2) &&
           fields.contains(4) && fields.contains(5);
}
} // namespace RouteServicePolicy
