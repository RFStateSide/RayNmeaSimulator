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

#include "slcancodec.h"
#include <algorithm>

namespace SlcanCodec {
QByteArray configuration() { return "C\rS5\rA0\rM0\rO\r"; }
QByteArray encode(const CanFrame &f) {
    if (f.id > 0x1fffffff || f.length > 8) return {};
    QByteArray out = "T" + QByteArray::number(f.id, 16).rightJustified(8, '0').toUpper();
    out += char('0' + f.length);
    out += QByteArray(reinterpret_cast<const char *>(f.data.data()), f.length).toHex().toUpper();
    return out + '\r';
}
QList<CanFrame> Parser::feed(const QByteArray &bytes) {
    QList<CanFrame> result;
    for (char c : bytes) {
        if (c == '\a') { error = true; ++rejected; continue; }
        if (c != '\r') {
            if (overflow) { ++rejected; continue; }
            if (buffer.size() >= 26) {
                rejected += buffer.size() + 1; buffer.clear(); overflow = true;
            } else buffer += c;
            continue;
        }
        if (overflow) { overflow = false; continue; }
        const auto line = buffer; buffer.clear();
        if (line.isEmpty()) continue; // Empty ACKs do not prove controller readiness.
        bool valid = line.size() >= 10 && line[0] == 'T' && line[9] >= '0' && line[9] <= '8';
        const int length = valid ? line[9] - '0' : 0;
        valid = valid && line.size() == 10 + 2 * length;
        for (int i = 1; valid && i < line.size(); ++i) {
            if (i == 9) continue;
            const char h = line[i];
            valid = (h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F');
        }
        bool ok = false;
        const auto id = valid ? line.mid(1, 8).toUInt(&ok, 16) : 0;
        if (!valid || !ok || id > 0x1fffffff) { rejected += line.size(); continue; }
        CanFrame f; f.id = id; f.length = length;
        const auto data = QByteArray::fromHex(line.mid(10));
        std::copy(data.begin(), data.end(), f.data.begin());
        result += f;
    }
    return result;
}
}
