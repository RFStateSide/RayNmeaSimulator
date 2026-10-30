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
#include <QByteArray>
#include <QList>
#include <array>

namespace WaveshareCodec {
struct CanFrame {
    quint32 id = 0;
    quint8 length = 0;
    std::array<unsigned char, 8> data{};
};
// Model A variable-length classical extended DATA only.
QByteArray receiveConfiguration();
QByteArray transmitConfiguration();
// Empty on invalid identifier/DLC; never clamps or truncates a frame.
QByteArray encode(const CanFrame &frame);
class Parser {
public:
    QList<CanFrame> feed(const QByteArray &bytes);
    void reset() { buffer.clear(); rejected = 0; }
    quint64 rejectedBytes() const { return rejected; }
    int bufferedBytes() const { return buffer.size(); }
private:
    QByteArray buffer; // At most one incomplete 15-byte packet.
    quint64 rejected = 0;
};
}
