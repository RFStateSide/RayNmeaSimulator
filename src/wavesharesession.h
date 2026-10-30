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
#include <QObject>
#include <QSerialPort>
#include <QPointer>
#include <QTimer>
#include <QSettings>
#include <QQueue>
#include <QElapsedTimer>
#include <functional>
#include <memory>
#include "N2kMsg.h"

struct WaveshareConfig {
    quint32 uniqueNumber = 0;
    int preferredSource = 44;
    static WaveshareConfig load(QSettings &settings, const QString &prefix = QStringLiteral("Waveshare")); // Nmea2000 group
    bool valid() const { return uniqueNumber > 0 && uniqueNumber <= 2097151 && preferredSource >= 0 && preferredSource <= 251; }
};
class RawCanProtocol;
using WaveshareCore = RawCanProtocol;
class WaveshareSession final : public QObject {
    Q_OBJECT
public:
    using PortFactory = std::function<QSerialPort *(QObject *)>;
    enum State { Closed, Configuring, AwaitingTraffic, Receiving, Failed, Claiming, Transmitting };
    Q_ENUM(State)
    explicit WaveshareSession(QObject *parent = nullptr, PortFactory factory = {});
    ~WaveshareSession();
    // Shared serial/core lifecycle; the legacy class name preserves existing API.
    enum Protocol { Waveshare, CanableSlcan };
    void setProtocol(Protocol value);
    Protocol protocol() const { return wireProtocol; }
    bool start(const QString &portName);
    bool startTransmitting(const QString &portName, const WaveshareConfig &identity);
    bool send(const tN2kMsg &message);
    bool isTransmitting() const;
    bool startService(const QString &name,const WaveshareConfig &identity);
    bool sendService(const tN2kMsg &message);
    bool serviceReady() const;
    int sourceAddress() const;
    void stop(const QString &reason = QStringLiteral("Waveshare stopped; reconnect RX or explicitly Enable Sending."));
    State state() const { return current; }
    QString status() const { return message; }
    quint64 blockedCanWrites() const;
signals:
    void statusChanged(const QString &status);
    void received(const tN2kMsg &message);
    void failed(const QString &message);
private:
    bool open(const QString &portName, bool transmit, const WaveshareConfig &identity);
    bool enqueue(quint32 id, quint8 length, const unsigned char *data);
    void pumpWrites();
    void configurationComplete();
    void report(State state, const QString &text);
    void read(quint64 generation);
    void service();
    void fail(const QString &text);
    Protocol wireProtocol = Waveshare;
    QString context(QString text) const;
    class ReceiveState;
    std::unique_ptr<ReceiveState> rx;
    // Local strong references keep a core alive through reentrant STOP callbacks.
    std::shared_ptr<WaveshareCore> core;
    PortFactory factory;
    QPointer<QSerialPort> port;
    QTimer timer, configurationDeadline;
    State current = Closed;
    QString message = QStringLiteral("Waveshare RX stopped.");
    quint64 generation = 0;
    qint64 configurationRemaining = 0;
    QByteArray pendingWrite;
    QQueue<QByteArray> output;
    qint64 inFlight = 0;
    QElapsedTimer progress;
    QElapsedTimer waveshareFrameDrained;
    QElapsedTimer claimQuiet;
    bool claimOutstanding = false;
    bool serviceLease = false;
    bool txAuthorized = false, pumping = false;
    bool servicing = false, warned = false;
};
