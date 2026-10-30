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

#ifndef SIGNALKPUBLISHER_H
#define SIGNALKPUBLISHER_H

#include <QObject>
#include <QJsonArray>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QWebSocket>
#include "N2kMsg.h"

struct SignalKConfig {
    QString server = "http://127.0.0.1:3000";
    QString token;
    QString source = "raynmeasimulator";
};

// Self measurements and explicitly identified AIS target deltas only. No subscriptions, received-data forwarding or PUTs.
class SignalKPublisher : public QObject {
    Q_OBJECT
public:
    explicit SignalKPublisher(QObject *parent = nullptr);
    ~SignalKPublisher() override { stop(); }
    void configure(const SignalKConfig &config);
    void start();
    void stop();
    bool publish(const tN2kMsg &message);
    bool publishAis(int pgn, const QMap<QString,QString> &fields, const QHash<QString,QDateTime> &sampleTimes);
    static QJsonObject aisDelta(int pgn, const QMap<QString,QString> &fields,
                               const QHash<QString,QDateTime> &sampleTimes, const QString &source);
    bool isReady() const { return ready; }
    bool isActive() const { return active; }
    static QJsonArray values(const tN2kMsg &message);
signals:
    void statusChanged(const QString &status);
private:
    bool sendDelta(const QJsonObject &delta);
    QString serverSelf;
    QSet<QString> aisWarnings;
    void connectToServer();
    void retry();
    SignalKConfig config;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> authorization;
    QWebSocket socket;
    QTimer reconnect, deadline, heartbeat, flushDeadline;
    bool active = false, ready = false;
    bool awaitingPong = false;
    quint64 generation = 0;
};
#endif
