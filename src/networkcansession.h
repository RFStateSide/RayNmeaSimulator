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
#include <QTcpSocket>
#include <QTimer>
#include <QSettings>
#include <memory>
#include "N2kMsg.h"
class RawCanProtocol;
struct NetworkCanConfig {
    QString host = "127.0.0.1", interfaceName = "can0", token;
    int port = 29501, preferredSource = 45;
    quint32 uniqueNumber = 0;
    bool allowTx = false;
    static NetworkCanConfig load(QSettings &);
};
class NetworkCanSession : public QObject {
    Q_OBJECT
  public:
    explicit NetworkCanSession(QObject *parent = nullptr);
    ~NetworkCanSession();
    void configure(const NetworkCanConfig &, const QString &role = "rx");
    NetworkCanConfig configuration() const { return config; } // Internal transport reuse; never displayed/logged.
    void setRawReceptionLogging(bool enabled) { logRawReception = enabled; }
    bool start(const QString &role = "rx");
    void stop(bool receiveAgain = false);
    void revoke() {
        if (role != "rx")
            stop(true);
    }
    bool ready() const;
    bool openingOrActiveTx() const { return role == "tx" && timer.isActive(); }
    bool connected() const { return online; }
    bool transmitting() const { return role == "tx" && ready(); }
    bool send(const tN2kMsg &, bool service = false);
    int sourceAddress() const;
    QString status() const { return text; }
  signals:
    void received(const tN2kMsg &);
    void statusChanged(const QString &);
    void failed(const QString &);

  private:
    bool envelope(const QJsonObject &);
    void read();
    void tick();
    void fail(const QString &);
    void report(const QString &);
    NetworkCanConfig config;
    QTcpSocket socket;
    QTimer timer;
    QByteArray input;
    std::shared_ptr<RawCanProtocol> core;
    QString role = "rx", client, nonce, text = "Network CAN disconnected";
    bool online = false, stopping = false;
    bool logRawReception = true;
    qint64 deadline = 0, lastPing = 0, epoch = 0;
    quint64 sequence = 0, generation = 0;
};
