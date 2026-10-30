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
#include <QTimer>
#include <QElapsedTimer>
#include <QSettings>
#include <functional>
#include "NMEA2000.h"
struct SocketCanConfig {
    QString interfaceName = "can0";
    quint32 uniqueNumber = 0;
    bool operator==(const SocketCanConfig &o) const { return interfaceName==o.interfaceName && uniqueNumber==o.uniqueNumber; }
};
// The backend/core is process-lifetime owned. Session never deletes it.
class SocketCanBackend {
public:
    virtual ~SocketCanBackend() = default;
    virtual void configure(const SocketCanConfig&, std::function<void(const tN2kMsg&)>) = 0;
    virtual void service() = 0;
    virtual bool ready() const = 0;
    virtual bool serviceReady() const { return ready(); }
    virtual int sourceAddress() const { return -1; }
    virtual int openingError() const = 0;
    virtual bool send(const tN2kMsg&) = 0;
    virtual bool close() = 0;
};
class SocketCanSession final : public QObject {
    Q_OBJECT
public:
    enum State { Closed, Initializing, Ready, Failed, ShuttingDown };
    Q_ENUM(State)
    using LinkValidator = std::function<QString(const QString&)>;
    explicit SocketCanSession(QObject *parent=nullptr, SocketCanBackend *backend=nullptr, LinkValidator validator={});
    ~SocketCanSession();
    bool start(const SocketCanConfig&);
    void setViewer(bool);
    bool send(const tN2kMsg&);
    bool sendService(const tN2kMsg&);
    void authorizeService(bool value) { serviceAuthorized=value; }
    int sourceAddress() const { return backend ? backend->sourceAddress() : -1; }
    bool routeServiceReady() const { return !terminal && current==Ready && backend && backend->serviceReady(); }
    void stopSending();
    void shutdown(const QString &reason = "SocketCAN stopped. Restart required.");
    void service();
    State state() const { return current; }
    bool restartRequired() const { return terminal; }
    bool serviceActive() const { return timer.isActive(); }
    QString status() const { return message; }
    quint64 serviceCount() const { return services; }
    qint64 maximumServiceGapNs() const { return maxGap; }
    static bool validName(const QString&);
    static QString validateLink(const QString&, const QByteArray&);
    static SocketCanConfig loadConfig(QSettings&);
signals:
    void statusChanged(const QString&);
    void received(const tN2kMsg&);
private:
    void report(State,const QString&);
    void finishClose();
    QString closeReason;
    SocketCanBackend *backend;
    LinkValidator validator;
    SocketCanConfig config;
    QTimer timer;
    QElapsedTimer elapsed;
    State current=Closed;
    bool serviceAuthorized=false,serviceAttempted=false;
    bool configured=false, terminal=false, viewer=false, attempted=false, servicing=false;
    QString message="SocketCAN closed";
    quint64 services=0;
    qint64 last=0,maxGap=0;
};
