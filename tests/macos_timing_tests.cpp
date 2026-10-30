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

#include "macos_millis.h"
#include "N2kTimer.h"
#include <chrono>
#include <thread>
#include <iostream>
#ifdef TEST_ROLLOVER
static uint64_t elapsed = 0xfffffffeULL * 1000000ULL;
extern "C" uint32_t millis() { return MacosTiming::millisecondsFromNanoseconds(elapsed); }
#endif
int main()
{
    using MacosTiming::millisecondsFromNanoseconds;
    static_assert(millisecondsFromNanoseconds(999999) == 0);
    static_assert(millisecondsFromNanoseconds(1000000) == 1);
    static_assert(millisecondsFromNanoseconds(0xffffffffULL * 1000000ULL) == 0xffffffffU);
    static_assert(millisecondsFromNanoseconds(0x100000000ULL * 1000000ULL) == 0);
#ifdef TEST_ROLLOVER
    if (N2kMillis64() != 0xfffffffeULL) return 1;
    elapsed += 1000000;
    if (N2kMillis64() != 0xffffffffULL) return 2;
    elapsed += 1000000;
    if (N2kMillis64() != 0x100000000ULL || N2kMillis() != 0) return 3;
    elapsed += 7000000;
    if (N2kMillis64() != 0x100000007ULL || N2kMillis() != 7) return 4;
    std::cout << "PASS macOS conversion and upstream rollover extension\n";
#else
    const auto before = N2kMillis64();
    const auto wallBefore = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto after = N2kMillis64();
    const auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - wallBefore).count();
    if (after < before + 20 || after - before > static_cast<uint64_t>(interval + 10)) return 1;
    auto previous = after;
    for (int i = 0; i < 10000; ++i) {
        const auto next = N2kMillis64();
        if (next < previous || N2kMillis() > millis()) return 2;
        previous = next;
    }
    std::cout << "PASS macOS monotonic hook and millisecond units\n";
#endif
}
