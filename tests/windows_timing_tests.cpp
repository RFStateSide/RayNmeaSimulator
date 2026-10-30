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

#include "N2kTimer.h"
#include <chrono>
#include <thread>
#include <iostream>
#ifdef TEST_ROLLOVER
static uint32_t tick = 0xfffffffeU;
extern "C" uint32_t millis() { return tick; }
#endif
int main() {
#ifdef TEST_ROLLOVER
    if (N2kMillis64() != 0xfffffffeULL) return 1;
    tick = 0xffffffffU;
    if (N2kMillis64() != 0xffffffffULL) return 2;
    tick = 0;
    if (N2kMillis64() != 0x100000000ULL || N2kMillis() != 0) return 3;
    tick = 7;
    if (N2kMillis64() != 0x100000007ULL) return 4;
    std::cout << "PASS deterministic upstream 32-bit rollover extension\n";
#else
    const auto first = N2kMillis64();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto last = N2kMillis64();
    if (last < first + 20 || static_cast<uint32_t>(last) > millis()) return 1;
    if (N2kMillis() < static_cast<uint32_t>(last)) return 2;
    std::cout << "PASS Windows upstream timer and application millis hook\n";
#endif
}
