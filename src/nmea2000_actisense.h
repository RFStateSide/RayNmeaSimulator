/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
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

#ifndef NMEA2000_ACTISENSE_H
#define NMEA2000_ACTISENSE_H

#include <QSerialPort>
#include <QFile>
#include <QElapsedTimer>
#include <QJsonObject>
#include "actisensecodec.h"
#include "NMEA2000.h"
#include "N2kMsg.h"
#include "N2kMessages.h"

class NMEA2000_Actisense : public QObject, public tNMEA2000
{
    Q_OBJECT

public:
    explicit NMEA2000_Actisense(QSerialPort &serialPort, QObject *parent = nullptr);
    ~NMEA2000_Actisense();

    // CAN functions
    bool CANOpen();
    bool CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent);
    bool CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf);
    bool SendMessage(const tN2kMsg &N2kMsg);

    // Product information methods
    unsigned short GetN2kVersion() const;
    unsigned short GetProductCode() const;
    const char* GetModelID() const;
    const char* GetSwCode() const;
    const char* GetModelVersion() const;
    const char* GetModelSerialCode() const;
    unsigned short GetCertificationLevel() const;
    unsigned short GetLoadEquivalency() const;

    void setPreamble(const QString &preamble);

    // NMEA2000 message handling
    void HandleMsg(const tN2kMsg &N2kMsg);

    // New methods from NMEA2000.cpp
    void HandleCommandedAddress(uint64_t CommandedName, unsigned char NewAddress, int iDev);
    void HandleCommandedAddress(const tN2kMsg &N2kMsg);

    // ISO Request methods
    static void SetN2kPGN59904(tN2kMsg &N2kMsg, uint8_t Destination, unsigned long RequestedPGN);
    static bool ParseN2kPGN59904(const tN2kMsg &N2kMsg, unsigned long &RequestedPGN);

    // PGN List methods
    static void SetN2kPGN126464(tN2kMsg &N2kMsg, uint8_t Destination, tN2kPGNList tr, const unsigned long *PGNs);

#if !defined(N2K_NO_HEARTBEAT_SUPPORT)
    // Heartbeat method
    static void SetN2kPGN126993(tN2kMsg &N2kMsg, uint32_t timeInterval_ms, uint8_t sequenceCounter);
#endif

private slots:
    void onSerialDataAvailable();

signals:
    void nmea2000MessageReceived(tN2kMsg &N2kMsg);

private:
    QSerialPort &serialPort;
    ActisenseCodec::Parser parser;
    QFile txTrace;
    QElapsedTimer traceClock;
    qint64 traceBytes = 0, traceSequence = 0, acceptedBytes = 0, completedBytes = 0;
    void trace(const char *event, QJsonObject fields = {});
    bool AddressChanged = false;
    bool DeviceInformationChanged = false;

    tProductInformation productInformation;  // Use this for product information

    QByteArray encodeMessage(const tN2kMsg &N2kMsg);

    QString preamble = "0x94";
};

#endif // NMEA2000_ACTISENSE_H
