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

#ifndef RAYNMEA_MACOS_MILLIS_H
#define RAYNMEA_MACOS_MILLIS_H
#include <cstdint>

namespace MacosTiming {
// Explicit modulo 2^32 conversion, shared with deterministic boundary tests.
constexpr uint32_t millisecondsFromNanoseconds(uint64_t elapsed)
{
    return static_cast<uint32_t>(elapsed / 1000000ULL);
}
}
extern "C" uint32_t millis();
#endif
