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
#include "wavesharecodec.h"

// CANable2 classical extended DATA subset. No FD, RTR or implicit retries.
namespace SlcanCodec {
using CanFrame = WaveshareCodec::CanFrame;
QByteArray configuration();
QByteArray encode(const CanFrame &frame);
class Parser {
public:
    QList<CanFrame> feed(const QByteArray &bytes);
    void reset() { buffer.clear(); rejected = 0; overflow = error = false; }
    quint64 rejectedBytes() const { return rejected; }
    int bufferedBytes() const { return buffer.size(); }
    bool adapterError() const { return error; }
private:
    QByteArray buffer;
    quint64 rejected = 0;
    bool overflow = false, error = false;
};
}
