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

#include "nmea2000_actisense.h"
#include "transportcapabilities.h"
#include <QDateTime>
#include <QDebug>
#include <QThread>
#include <QJsonDocument>

NMEA2000_Actisense::NMEA2000_Actisense(QSerialPort &serialPort, QObject *parent)
    : QObject(parent)
    , tNMEA2000()
    , serialPort(serialPort)
{
    // Opt-in, new-file-only, bounded evidence. Never enable through saved settings.
    const auto tracePath = qEnvironmentVariable("RAYNMEA_ACTISENSE_TX_TRACE");
    if (!tracePath.isEmpty()) {
        txTrace.setFileName(tracePath);
        if (txTrace.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            traceClock.start();
            trace("session", {{"qt", qVersion()}, {"utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
        } else qWarning() << "Actisense TX trace unavailable (requires a new writable file)";
    }
    connect(&serialPort, &QSerialPort::bytesWritten, this, [this](qint64 count) {
        completedBytes += count;
        trace("progress", {{"count", count}, {"completed_total", completedBytes}});
    });
    serialPort.setReadBufferSize(4096);
    connect(&serialPort, &QSerialPort::readyRead, this, &NMEA2000_Actisense::onSerialDataAvailable);

    connect(&serialPort, &QIODevice::aboutToClose, this, [this] {
        trace("closing"); parser.reset();
    });
    connect(&serialPort, &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError error) {
        if (error != QSerialPort::NoError) parser.reset();
        if (error != QSerialPort::NoError) trace("error", {{"code", int(error)}});
    });

    // Initialize product information, example values
    productInformation.N2kVersion = 1300;  // Example value
    productInformation.ProductCode = 12345;  // Example value
    strcpy(productInformation.N2kModelID, "NGT-1 Model");
    strcpy(productInformation.N2kSwCode, "1.0.0");
    strcpy(productInformation.N2kModelVersion, "1.0");
    strcpy(productInformation.N2kModelSerialCode, "123456789");
    productInformation.CertificationLevel = 1;  // Example value
    productInformation.LoadEquivalency = 1;  // Example value
}

NMEA2000_Actisense::~NMEA2000_Actisense()
{
    if (serialPort.isOpen()) {
        serialPort.close();
        QThread::msleep(100); // Wait for the port to close
    }
    trace("end");
}

void NMEA2000_Actisense::trace(const char *event, QJsonObject fields)
{
    if (!txTrace.isOpen()) return;
    fields["event"] = QString::fromLatin1(event);
    fields["elapsed_ns"] = traceClock.nsecsElapsed();
    fields["queued"] = serialPort.bytesToWrite();
    fields["accepted_total"] = acceptedBytes;
    auto line = QJsonDocument(fields).toJson(QJsonDocument::Compact) + '\n';
    if (traceBytes + line.size() > 8 * 1024 * 1024) {
        txTrace.close();
        qWarning() << "Actisense TX trace reached 8 MiB limit; tracing stopped";
        return;
    }
    if (txTrace.write(line) != line.size()) {
        txTrace.close();
        qWarning() << "Actisense TX trace write failed; tracing stopped";
        return;
    }
    traceBytes += line.size();
}

// Implement the required CAN functions
bool NMEA2000_Actisense::CANOpen() {
    parser.reset();
    if (serialPort.open(QIODevice::ReadWrite)) {
        qDebug() << "Success opening Actisense serial port";
        return true;
    } else {
        qDebug() << "Failed to open Actisense serial port";
        return false;
    }
}

bool NMEA2000_Actisense::CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool wait_sent) {
    // An NGT accepts BST-94 messages, not native-memory CAN frame structs.
    // Do not emit an invalid serial frame through the inherited raw-CAN hook.
    Q_UNUSED(id); Q_UNUSED(len); Q_UNUSED(buf); Q_UNUSED(wait_sent);
    return false;
}

bool NMEA2000_Actisense::SendMessage(const tN2kMsg &N2kMsg)
{
    if (!TransportCapabilities::canTransmit("Actisense")) return false;
    QByteArray message = encodeMessage(N2kMsg);
    if (message.isEmpty() || !serialPort.isWritable()) return false;
    const auto sequence = ++traceSequence;
    if (txTrace.isOpen())
        trace("submit", {{"sequence", sequence}, {"pgn", qint64(N2kMsg.PGN)},
                         {"payload_hex", QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(N2kMsg.Data), N2kMsg.DataLen).toHex())},
                         {"frame_hex", QString::fromLatin1(message.toHex())}, {"length", message.size()}});
    qint64 bytesWritten = serialPort.write(message);
    if (bytesWritten > 0) acceptedBytes += bytesWritten;
    trace("accepted", {{"sequence", sequence}, {"count", bytesWritten}, {"error", int(serialPort.error())}});
    if (bytesWritten != message.size()) {
        qDebug() << "Failed to write the complete message to the serial port.";
        return false;
    }
    const bool flushed = serialPort.flush();
    trace("flush", {{"sequence", sequence}, {"result", flushed}, {"error", int(serialPort.error())}});
    return true;
}

bool NMEA2000_Actisense::CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf) {
    // BST-93 already carries reassembled messages. Only the bounded streaming
    // parser consumes serial input; raw-CAN reads would race it and are unsupported.
    Q_UNUSED(id); Q_UNUSED(len); Q_UNUSED(buf);
    return false;
}

// Implement required product information methods using productInformation
unsigned short NMEA2000_Actisense::GetN2kVersion() const {
    return productInformation.N2kVersion;
}

unsigned short NMEA2000_Actisense::GetProductCode() const {
    return productInformation.ProductCode;
}

const char* NMEA2000_Actisense::GetModelID() const {
    return productInformation.N2kModelID;
}

const char* NMEA2000_Actisense::GetSwCode() const {
    return productInformation.N2kSwCode;
}

const char* NMEA2000_Actisense::GetModelVersion() const {
    return productInformation.N2kModelVersion;
}

const char* NMEA2000_Actisense::GetModelSerialCode() const {
    return productInformation.N2kModelSerialCode;
}

unsigned short NMEA2000_Actisense::GetCertificationLevel() const {
    return productInformation.CertificationLevel;
}

unsigned short NMEA2000_Actisense::GetLoadEquivalency() const {
    return productInformation.LoadEquivalency;
}

void NMEA2000_Actisense::setPreamble(const QString &preamble)
{
    this->preamble = preamble;
}

// Implement message handling
void NMEA2000_Actisense::HandleMsg(const tN2kMsg &N2kMsg) {
    qDebug() << "Received N2kMsg with PGN:" << N2kMsg.PGN;
}

// Implement Commanded Address methods
void NMEA2000_Actisense::HandleCommandedAddress(uint64_t CommandedName, unsigned char NewAddress, int iDev) {
    if (Devices[iDev].DeviceInformation.GetName() == CommandedName && Devices[iDev].N2kSource != NewAddress) {
        Devices[iDev].N2kSource = NewAddress;
        Devices[iDev].UpdateAddressClaimEndSource();
        StartAddressClaim(iDev);
        AddressChanged = true;
    }
}

void NMEA2000_Actisense::HandleCommandedAddress(const tN2kMsg &N2kMsg) {
    if (N2kMsg.PGN != 65240L || !N2kMsg.IsTPMessage() || N2kMsg.DataLen != 9) return;

    int iDev = FindSourceDeviceIndex(N2kMsg.Destination);
    if (!tNMEA2000::IsBroadcast(N2kMsg.Destination) && iDev == -1) return;

    int Index = 0;
    uint64_t CommandedName = N2kMsg.GetUInt64(Index);
    unsigned char NewAddress = N2kMsg.GetByte(Index);
    if (NewAddress >= 252) return;

    HandleCommandedAddress(CommandedName, NewAddress, iDev);
}

// Implement ISO Request methods
void NMEA2000_Actisense::SetN2kPGN59904(tN2kMsg &N2kMsg, uint8_t Destination, unsigned long RequestedPGN) {
    N2kMsg.SetPGN(59904L);
    N2kMsg.Destination = Destination;
    N2kMsg.Priority = 6;
    N2kMsg.Add3ByteInt(RequestedPGN);
}

bool NMEA2000_Actisense::ParseN2kPGN59904(const tN2kMsg &N2kMsg, unsigned long &RequestedPGN) {
    if (N2kMsg.DataLen >= 3 && N2kMsg.DataLen <= 8) {
        int Index = 0;
        RequestedPGN = N2kMsg.Get3ByteUInt(Index);
        return true;
    }
    return false;
}

// Implement PGN List methods
void NMEA2000_Actisense::SetN2kPGN126464(tN2kMsg &N2kMsg, uint8_t Destination, tN2kPGNList tr, const unsigned long *PGNs) {
    N2kMsg.SetPGN(126464L);
    N2kMsg.Destination = Destination;
    N2kMsg.Priority = 6;
    N2kMsg.AddByte(tr);

    for (int i = 0; (pgm_read_dword(&PGNs[i])) != 0; i++) {
        N2kMsg.Add3ByteInt(pgm_read_dword(&PGNs[i]));
    }
}

#if !defined(N2K_NO_HEARTBEAT_SUPPORT)
static const uint32_t MaxHeartbeatInterval = 60000;  // Define max heartbeat interval as 60 seconds
void NMEA2000_Actisense::SetN2kPGN126993(tN2kMsg &N2kMsg, uint32_t timeInterval_ms, uint8_t sequenceCounter) {
    N2kMsg.SetPGN(126993L);
    N2kMsg.Priority = 7;
    if (timeInterval_ms > MaxHeartbeatInterval) {
        N2kMsg.Add2ByteUInt(0xfffe);  // Error
    } else {
        N2kMsg.Add2ByteUInt((uint16_t)(timeInterval_ms));
    }
    N2kMsg.AddByte(sequenceCounter);
    N2kMsg.AddByte(0xff);  // Reserved
    N2kMsg.Add4ByteUInt(0xffffffff);  // Reserved
}

#endif // N2K_NO_HEARTBEAT_SUPPORT

void NMEA2000_Actisense::onSerialDataAvailable()
{
    // Bounded reads avoid an additional unbounded readAll allocation. The parser
    // itself retains at most one maximum-size unescaped BST-93 frame.
    char chunk[256];
    while (serialPort.bytesAvailable() > 0) {
        const qint64 count = serialPort.read(chunk, sizeof(chunk));
        if (count < 0) { parser.reset(); return; }
        if (count == 0) return;
        for (qint64 i = 0; i < count; ++i) {
            ActisenseCodec::Message message;
            if (!parser.feed(static_cast<unsigned char>(chunk[i]), message)) continue;
            tN2kMsg decoded;
            decoded.Priority = message.priority;
            decoded.PGN = message.pgn;
            decoded.Destination = message.destination;
            decoded.Source = message.source;
            decoded.MsgTime = message.timestamp;
            decoded.DataLen = static_cast<int>(message.length);
            for (std::size_t j = 0; j < message.length; ++j) decoded.Data[j] = message.data[j];
            emit nmea2000MessageReceived(decoded);
        }
    }
}

QByteArray NMEA2000_Actisense::encodeMessage(const tN2kMsg &N2kMsg)
{
    static_assert(tN2kMsg::MaxDataLen == ActisenseCodec::MaxPayload,
                  "Actisense payload capacity must match tN2kMsg");
    if (N2kMsg.DataLen < 0 || N2kMsg.DataLen > tN2kMsg::MaxDataLen
        || N2kMsg.PGN > 0x3ffffUL) return {};
    ActisenseCodec::Message message;
    message.priority = N2kMsg.Priority;
    message.pgn = static_cast<std::uint32_t>(N2kMsg.PGN);
    message.destination = N2kMsg.Destination;
    message.timestamp = static_cast<std::uint32_t>(QTime::currentTime().msecsSinceStartOfDay());
    message.length = static_cast<std::size_t>(N2kMsg.DataLen);
    for (std::size_t i = 0; i < message.length; ++i) message.data[i] = N2kMsg.Data[i];
    const auto frame = ActisenseCodec::encode(message, preamble == "0x93" ? 0x93 : 0x94);
    return QByteArray(reinterpret_cast<const char *>(frame.bytes.data()), frame.size);
}
