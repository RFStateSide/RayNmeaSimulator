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

#include "wavesharecodec.h"
#include <QtEndian>

QByteArray WaveshareCodec::receiveConfiguration()
{
    // M2.31E exact bench fixture: variable protocol, 250 kbps, extended,
    // zero filter/mask, mode 02, retransmission disabled, additive checksum.
    return QByteArray::fromHex("aa5512050200000000000000000201000000001c");
}

QByteArray WaveshareCodec::transmitConfiguration()
{
    // Documented Model A normal mode 00; physical TX qualification is separate.
    auto bytes = receiveConfiguration();
    bytes[13] = 0;
    quint8 sum = 0;
    for (int i = 2; i < 19; ++i) sum += quint8(bytes[i]);
    bytes[19] = char(sum);
    return bytes;
}

QByteArray WaveshareCodec::encode(const CanFrame &frame)
{
    if (frame.id > 0x1fffffff || frame.length > 8) return {};
    QByteArray bytes(6, char(0));
    bytes[0] = char(0xaa); bytes[1] = char(0xe0 | frame.length);
    qToLittleEndian(frame.id, bytes.data() + 2);
    bytes.append(reinterpret_cast<const char *>(frame.data.data()), frame.length);
    bytes.append(char(0x55));
    return bytes;
}

QList<WaveshareCodec::CanFrame> WaveshareCodec::Parser::feed(const QByteArray &bytes)
{
    QList<CanFrame> frames;
    for (char byte : bytes) {
        buffer.append(byte);
        while (!buffer.isEmpty()) {
            if (quint8(buffer[0]) != 0xaa) {
                buffer.remove(0, 1); ++rejected; continue;
            }
            if (buffer.size() < 2) break;
            const auto flags = quint8(buffer[1]);
            // Standard, RTR, FD/control and DLC 9..15 are not NMEA2000 data.
            if ((flags & 0xf0) != 0xe0 || (flags & 15) > 8) {
                buffer.remove(0, 1); ++rejected; continue;
            }
            const int length = flags & 15, size = length + 7;
            if (buffer.size() < size) break;
            const auto id = qFromLittleEndian<quint32>(buffer.constData() + 2);
            if (quint8(buffer[size - 1]) != 0x55 || id > 0x1fffffff) {
                buffer.remove(0, 1); ++rejected; continue;
            }
            CanFrame frame; frame.id = id; frame.length = length;
            for (int i = 0; i < length; ++i) frame.data[i] = quint8(buffer[6 + i]);
            frames.append(frame); buffer.remove(0, size);
        }
    }
    return frames;
}
