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

#ifndef SIGNALKSETUPCLIENT_H
#define SIGNALKSETUPCLIENT_H
#include "signalkpublisher.h"

// Setup-only protocol client: deliberately has no measurement publishing API.
class SignalKSetupClient : public QObject {
    Q_OBJECT
public:
    explicit SignalKSetupClient(QObject *parent = nullptr);
    ~SignalKSetupClient() override;
    void requestAccess(const SignalKConfig &config);
    void testConnection(const SignalKConfig &config);
    void cancel();
    bool isBusy() const { return operation != Idle; }
    void setTimeouts(int requestMs, int approvalMs, int pollMs); // Also used by protocol fixtures.
signals:
    void statusChanged(const QString &status);
    void busyChanged(bool busy);
    void tokenApproved(const QString &token);
    void connectionReady();
private:
    enum Operation { Idle, Access, Test } operation = Idle;
    SignalKConfig config;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> reply;
    QWebSocket socket;
    QTimer poll, deadline;
    QString pendingOrigin, pendingId, clientId;
    bool uncertainRequest = false;
    quint64 generation = 0;
    int requestTimeout = 8000, approvalTimeout = 600000;
    bool validate(const SignalKConfig &value);
    void finish(const QString &status);
    void pollAccess();
    void send(const QString &path, const QByteArray &body = {});
    void handle(const QJsonObject &data);
};
#endif
