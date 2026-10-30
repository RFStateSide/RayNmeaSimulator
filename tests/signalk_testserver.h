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

#ifndef SIGNALK_TESTSERVER_H
#define SIGNALK_TESTSERVER_H
#include <QtTest>
#include <functional>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocketServer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include "signalkpublisher.h"
#include "N2kMessages.h"

class TestServer : public QObject {
public:
    QTcpServer http;
    QWebSocketServer ws{"test", QWebSocketServer::NonSecureMode};
    QList<QJsonObject> deltas;
    QPointer<QWebSocket> peer;
    bool writer = true, headerSeen = false, subscribedNone = false, sendHello = true;
    QString self = "vessels.urn:mrn:signalk:uuid:test";
    QString accessState = "PENDING", permission = "APPROVED";
    int posts = 0, polls = 0, loginCode = 200, accessCode = 0;
    std::function<void()> afterPoll;
    QJsonObject accessBody;
    bool malformedAccess = false, rejectWebSocket = false;
    TestServer() {
        if (!http.listen(QHostAddress::LocalHost)) qFatal("test listen failed");
        connect(&http, &QTcpServer::newConnection, this, [this] {
            auto *client = http.nextPendingConnection();
            connect(client, &QTcpSocket::disconnected, client, &QObject::deleteLater);
            connect(client, &QTcpSocket::readyRead, this, [this, client] {
                const auto request = client->peek(16384);
                if (!request.contains("\r\n\r\n")) return;
                const auto respond = [client](int code, const QByteArray &body) {
                    client->readAll();
                    client->write("HTTP/1.1 " + QByteArray::number(code) + " Status\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    client->disconnectFromHost();
                };
                if (request.startsWith("POST /signalk/v1/access/requests ") || request.startsWith("GET /signalk/v1/requests/fixture-id ")) {
                    const bool post = request.startsWith("POST");
                    if (post) {
                        int length = 0;
                        for (const auto &line : request.left(request.indexOf("\r\n\r\n")).split('\n'))
                            if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                        if (request.size() < request.indexOf("\r\n\r\n") + 4 + length) return;
                        ++posts; accessBody = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4, length)).object();
                    } else ++polls;
                    const QJsonObject response{{"requestId", "fixture-id"}, {"state", accessState}, {"statusCode", accessState == "PENDING" ? 202 : 200},
                        {"accessRequest", QJsonObject{{"permission", permission}, {"token", "test-token"}}}};
                    respond(accessCode ? accessCode : (post ? 202 : 200), malformedAccess ? QByteArray("{}") : QJsonDocument(response).toJson(QJsonDocument::Compact));
                    if (!post && afterPoll) afterPoll();
                } else if (request.startsWith("GET /skServer/loginStatus ")) {
                    client->readAll();
                    headerSeen = request.contains("Authorization: Bearer test-token");
                    const QByteArray body = writer && headerSeen
                        ? "{\"authenticationRequired\":true,\"userLevel\":\"readwrite\"}"
                        : "{\"authenticationRequired\":true,\"userLevel\":\"readonly\"}";
                    respond(loginCode, body);
                } else {
                    if (rejectWebSocket) { respond(403, "{}"); return; }
                    subscribedNone = request.startsWith("GET /signalk/v1/stream?subscribe=none ");
                    client->disconnect(this);
                    ws.handleConnection(client);
                    QMetaObject::invokeMethod(client, "readyRead", Qt::QueuedConnection);
                }
            });
        });
        connect(&ws, &QWebSocketServer::newConnection, this, [this] {
            peer = ws.nextPendingConnection();
            connect(peer, &QWebSocket::disconnected, peer, &QObject::deleteLater);
            connect(peer, &QWebSocket::textMessageReceived, this, [this](const QString &text) {
                deltas.append(QJsonDocument::fromJson(text.toUtf8()).object());
                const auto evidence=qEnvironmentVariable("RAYNMEA_AIS_DELTA_EVIDENCE");
                if(!evidence.isEmpty()) { QFile file(evidence); if(file.open(QIODevice::WriteOnly|QIODevice::Append))file.write(text.toUtf8()+"\n"); }
            });
            if (sendHello) peer->sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"self",self},{"version","2.11.0"}}).toJson(QJsonDocument::Compact)));
        });
    }
    SignalKConfig config() const { return {"http://127.0.0.1:"+QString::number(http.serverPort()), "test-token", "test-publisher"}; }
};

#endif
