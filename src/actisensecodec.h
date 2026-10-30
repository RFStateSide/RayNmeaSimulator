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

#ifndef ACTISENSECODEC_H
#define ACTISENSECODEC_H

#include <array>
#include <cstddef>
#include <cstdint>

// Actisense SDK: docs/DataFormats/Binary/bst-detail/BST-{93,94}-NGT.md
// and docs/DataProtocols/bdtp-protocol.md. Length/checksum exclude DLE stuffing.
// This host receives BST-93 only; BST-94 is a gateway transmit request.
namespace ActisenseCodec {
constexpr std::size_t MaxPayload = 223;
constexpr std::size_t MaxBody = 2 + 11 + MaxPayload + 1; // command, length, data, checksum
struct Message {
    std::uint8_t priority = 0, destination = 0, source = 0;
    std::uint32_t pgn = 0, timestamp = 0;
    std::size_t length = 0;
    std::array<std::uint8_t, MaxPayload> data{};
};
struct Frame {
    std::array<std::uint8_t, 2 * MaxBody + 4> bytes{};
    std::size_t size = 0;
};

inline Frame encode(const Message &message, std::uint8_t command)
{
    Frame frame;
    if ((command != 0x93 && command != 0x94) || message.length > MaxPayload
        || message.priority > 7 || message.pgn > 0x3ffff) return frame;
    frame.bytes[frame.size++] = 0x10;
    frame.bytes[frame.size++] = 0x02;
    std::uint8_t sum = 0;
    auto escaped = [&](std::uint8_t byte) {
        frame.bytes[frame.size++] = byte;
        if (byte == 0x10) frame.bytes[frame.size++] = byte;
    };
    auto body = [&](std::uint8_t byte) { sum += byte; escaped(byte); };
    body(command);
    body(static_cast<std::uint8_t>(message.length + (command == 0x93 ? 11 : 6)));
    body(message.priority);
    for (int shift = 0; shift < 24; shift += 8) body(message.pgn >> shift);
    body(message.destination);
    if (command == 0x93) {
        body(0); // Preserve the historical virtual-COM self-test source identity.
        for (int shift = 0; shift < 32; shift += 8) body(message.timestamp >> shift);
    }
    body(static_cast<std::uint8_t>(message.length));
    for (std::size_t i = 0; i < message.length; ++i) body(message.data[i]);
    escaped(static_cast<std::uint8_t>(0 - sum));
    frame.bytes[frame.size++] = 0x10;
    frame.bytes[frame.size++] = 0x03;
    return frame;
}

class Parser {
public:
    void reset() { state = State::Seeking; used = 0; }
    std::size_t bufferedSize() const { return used; }

    // Emits at most one fully validated message per byte. Incomplete frames retain
    // bounded state until the next byte, explicit reset, or a fresh DLE/STX.
    bool feed(std::uint8_t byte, Message &output)
    {
        switch (state) {
        case State::Seeking:
            if (byte == 0x10) state = State::Start;
            break;
        case State::Start:
            if (byte == 0x02) { used = 0; state = State::Body; }
            else if (byte != 0x10) reset();
            break;
        case State::Body:
            if (byte == 0x10) state = State::Escape;
            else append(byte);
            break;
        case State::Escape:
            if (byte == 0x10) { state = State::Body; append(byte); }
            else if (byte == 0x02) { used = 0; state = State::Body; }
            else if (byte == 0x03) {
                const bool valid = decode(output);
                reset();
                return valid;
            } else discard(); // Unknown DLE control: discard this frame.
            break;
        case State::Discard:
            if (byte == 0x10) state = State::DiscardEscape;
            break;
        case State::DiscardEscape:
            if (byte == 0x02) { used = 0; state = State::Body; }
            else if (byte == 0x03) reset();
            else state = State::Discard; // Includes escaped DLE in rejected data.
            break;
        }
        return false;
    }
private:
    enum class State { Seeking, Start, Body, Escape, Discard, DiscardEscape };
    State state = State::Seeking;
    std::array<std::uint8_t, MaxBody> bytes{};
    std::size_t used = 0;

    void discard() { used = 0; state = State::Discard; }

    void append(std::uint8_t byte)
    {
        if (used == bytes.size()) { discard(); return; }
        bytes[used++] = byte;
        if ((used == 1 && bytes[0] != 0x93)
            || (used == 2 && (bytes[1] < 11 || bytes[1] > 11 + MaxPayload))
            || (used >= 2 && used > std::size_t(bytes[1]) + 3)) discard();
    }
    bool decode(Message &output) const
    {
        if (used < 14 || bytes[0] != 0x93 || used != std::size_t(bytes[1]) + 3)
            return false;
        const std::size_t payload = bytes[12];
        if (payload > MaxPayload || bytes[1] != payload + 11 || used != payload + 14
            || bytes[2] > 7 || bytes[5] > 3) return false;
        std::uint8_t sum = 0;
        for (std::size_t i = 0; i < used; ++i) sum += bytes[i];
        if (sum != 0) return false;
        Message message;
        message.priority = bytes[2];
        message.pgn = std::uint32_t(bytes[3]) | (std::uint32_t(bytes[4]) << 8)
            | (std::uint32_t(bytes[5]) << 16);
        message.destination = bytes[6];
        message.source = bytes[7];
        for (int i = 0; i < 4; ++i) message.timestamp |= std::uint32_t(bytes[8 + i]) << (8 * i);
        message.length = payload;
        for (std::size_t i = 0; i < payload; ++i) message.data[i] = bytes[13 + i];
        output = message;
        return true;
    }
};
}
#endif
