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

// Standalone production-codec safety corpus; no Qt or operating-system I/O.
// MSVC: cl /std:c++17 /EHsc /fsanitize=address actisense_codec_stress.cpp
// GCC/Clang: c++ -std=c++17 -fsanitize=address,undefined actisense_codec_stress.cpp
#include "../src/actisensecodec.h"
#include <cassert>
#include <cstdio>

int main()
{
    using namespace ActisenseCodec;
    Parser parser;
    Message output;
    auto feed = [&](std::uint8_t byte) {
        if (parser.feed(byte, output)) {
            assert(output.length <= MaxPayload);
            assert(output.priority <= 7 && output.pgn <= 0x3ffff);
        }
        assert(parser.bufferedSize() <= MaxBody);
    };
    // Reproduce the historical short-body OOB input.
    for (auto byte : {0x10, 0x02, 0x93, 0x00, 0x6d, 0x10, 0x03})
        assert(!parser.feed(byte, output));
    Message message;
    message.priority = 6;
    message.pgn = 129025;
    message.destination = 255;
    message.timestamp = 0x10101010;
    for (std::size_t length = 0; length <= MaxPayload; ++length) {
        message.length = length;
        for (std::size_t i = 0; i < length; ++i) message.data[i] = std::uint8_t(i);
        const auto frame = encode(message, 0x93);
        parser.reset();
        int emitted = 0;
        for (std::size_t i = 0; i < frame.size; ++i) emitted += parser.feed(frame.bytes[i], output);
        assert(emitted == 1 && output.length == length && output.data == message.data);
        // Every truncation, then each possible replacement of each encoded byte.
        for (std::size_t cut = 0; cut < frame.size; ++cut) {
            parser.reset();
            for (std::size_t i = 0; i < cut; ++i) feed(frame.bytes[i]);
        }
        for (std::size_t at = 0; at < frame.size; ++at) {
            for (unsigned replacement = 0; replacement < 256; ++replacement) {
                parser.reset();
                for (std::size_t i = 0; i < frame.size; ++i)
                    feed(i == at ? std::uint8_t(replacement) : frame.bytes[i]);
            }
        }
    }
    std::uint32_t random = 0x92d48f31;
    for (unsigned i = 0; i < 10000000; ++i) {
        random = random * 1664525 + 1013904223;
        feed(std::uint8_t(random >> 24));
        if (i % 997 == 0) parser.reset();
    }
    std::puts("PASS Actisense codec safety corpus: lengths 0..223, all single-byte mutations, truncations, 10M noise bytes");
}
