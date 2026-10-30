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
#include "navigationstatistics.h"
#include <QQueue>
#include <functional>
#include "wavesharecodec.h"
#include "NMEA2000.h"
#include "N2kGroupFunctionDefaultHandlers.h"
#include <cstdlib>
// The pinned core has no releasing destructor. Own its receive/send/device
// storage and default group handlers explicitly, and release its lazily
// allocated counters. Every open gets a fresh instance, including address
// claim/heartbeat/TP state.
class RawCanProtocol final : public tNMEA2000 {
    class Handler final : public tMsgHandler {
      public:
        RawCanProtocol &owner;
        explicit Handler(RawCanProtocol &o) : tMsgHandler(0, &o), owner(o) {}
        void HandleMsg(const tN2kMsg &m) override {
            if (owner.deliver)
                owner.deliver(m);
        }
    } handler;
    tN2kCANMsg messages[16];
    tInternalDevice device[1];
    tCANSendFrame sending[50];
    tProductInformation product;
#if !defined(N2K_NO_GROUP_FUNCTION_SUPPORT)
    tN2kGroupFunctionHandlerForPGN60928 nameHandler{this};
    tN2kGroupFunctionHandlerForPGN126464 listHandler{this};
#if !defined(N2K_NO_HEARTBEAT_SUPPORT)
    tN2kGroupFunctionHandlerForPGN126993 heartbeatHandler{this};
#endif
    tN2kGroupFunctionHandlerForPGN126996 productHandler{this};
    tN2kGroupFunctionHandlerForPGN126998 configurationHandler{this};
    tN2kGroupFunctionHandler defaultHandler{this, 0};
#endif
  public:
    QQueue<WaveshareCodec::CanFrame> frames;
    std::function<void(const tN2kMsg &)> deliver;
    std::function<bool(quint32, quint8, const unsigned char *)> write;
    quint64 blocked = 0;
    RawCanProtocol(bool tx, quint32 uniqueNumber, int preferredSource)
        : handler(*this) {
        Devices = device;
        DeviceCount = 1;
        N2kCANMsgBuf = messages;
        MaxN2kCANMsgs = 16;
        for (auto &message : messages)
            message.FreeMessage();
        CANSendFrameBuf = sending;
        MaxCANSendFrames = 50;
        CANSendFrameBufferRead = CANSendFrameBufferWrite = 0;
        SetMode(tx ? N2km_ListenAndNode : N2km_ListenOnly, preferredSource);
        EnableForward(false);
        if (tx) {
            const auto serial = QByteArray::number(uniqueNumber);
            product.Set(serial.constData(), 105, APP_NAME, APP_VERSION,
                        APP_NAME, 0, 2101, 0);
            SetProductInformation(&product);
            SetDeviceInformation(uniqueNumber, 210, 80, 2046);
            ExtendTransmitMessages(NavigationStatistics::QualifiedPgns);
            ExtendReceiveMessages(NavigationStatistics::QualifiedPgns);
            SetHeartbeatInterval(30000);
#if !defined(N2K_NO_GROUP_FUNCTION_SUPPORT)
            AddGroupFunctionHandler(&nameHandler);
            AddGroupFunctionHandler(&listHandler);
#if !defined(N2K_NO_HEARTBEAT_SUPPORT)
            AddGroupFunctionHandler(&heartbeatHandler);
#endif
            AddGroupFunctionHandler(&productHandler);
            AddGroupFunctionHandler(&configurationHandler);
            AddGroupFunctionHandler(&defaultHandler);
#endif
        }
        OpenScheduler.FromNow(0);
    }
    ~RawCanProtocol() {
        delete[] device[0].PGNSequenceCounters;
        delete device[0].LocalProductInformation;
        free(LocalConfigurationInformationData);
    }
    bool ready() {
        return IsOpen() && !IsAddressClaimStarted(0) && GetN2kSource() <= 251;
    }
    bool CANOpen() override {
        return true;
    } // Serial ownership is outside the core.
    bool CANGetFrame(unsigned long &id, unsigned char &len,
                     unsigned char *data) override {
        // Preserve already validated serial traffic through the core's startup
        // drain.
        if (OpenState != os_Open || frames.isEmpty())
            return false;
        auto frame = frames.dequeue();
        id = frame.id;
        len = frame.length;
        std::copy(frame.data.begin(), frame.data.end(), data);
        return true;
    }
    bool CANSendFrame(unsigned long id, unsigned char len,
                      const unsigned char *data, bool) override {
        if (write)
            return write(id, len, data);
        ++blocked;
        return false;
    }
};
