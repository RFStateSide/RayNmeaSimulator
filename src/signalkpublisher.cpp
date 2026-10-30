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

#include "applicationlog.h"
#include "signalkpublisher.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrlQuery>
#include <cmath>

SignalKPublisher::SignalKPublisher(QObject *parent) : QObject(parent)
{
    reconnect.setSingleShot(true);
    reconnect.setInterval(2000);
    deadline.setSingleShot(true);
    deadline.setInterval(8000);
    heartbeat.setInterval(5000);
    flushDeadline.setSingleShot(true);
    flushDeadline.setInterval(1000);
    connect(this,&SignalKPublisher::statusChanged,this,[](const QString &text){ApplicationLog::event("raynmea.transport.signalk",text);});
    connect(&flushDeadline, &QTimer::timeout, this, [this] {
        if (socket.bytesToWrite() > 0) { socket.abort(); retry(); }
    });
    connect(&socket, &QWebSocket::bytesWritten, this, [this](qint64) {
        if (socket.bytesToWrite() == 0) flushDeadline.stop();
    });
    socket.setMaxAllowedIncomingMessageSize(65536);
    socket.setMaxAllowedIncomingFrameSize(65536);
    connect(&reconnect, &QTimer::timeout, this, &SignalKPublisher::connectToServer);
    connect(&deadline, &QTimer::timeout, this, [this] {
        if (authorization) authorization->abort();
        socket.abort();
        retry();
    });
    connect(&socket, &QWebSocket::connected, this, [this] {
        if (!active) { socket.abort(); return; }
        // Wait for the server hello before accepting samples.
        emit statusChanged("Signal K: connected; waiting for server hello");
    });
    connect(&socket, &QWebSocket::textMessageReceived, this, [this](const QString &text) {
        const auto object = QJsonDocument::fromJson(text.toUtf8()).object();
        if (active && !ready && object.contains("self") && object.contains("version")) {
            serverSelf=object.value("self").toString();
            if (!serverSelf.startsWith("vessels.")) serverSelf.prepend("vessels.");
            ready = true;
            deadline.stop();
            awaitingPong = false;
            heartbeat.start();
            emit statusChanged("Signal K: publishing as " + config.source);
        }
        // Never reflect incoming measurements or control requests.
    });
    connect(&socket, &QWebSocket::disconnected, this, &SignalKPublisher::retry);
    connect(&socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        // Do not log request URLs, headers, server responses or token-bearing errors.
        retry();
    });
    connect(&socket, &QWebSocket::pong, this, [this](quint64, const QByteArray &) { awaitingPong = false; });
    connect(&heartbeat, &QTimer::timeout, this, [this] {
        if (awaitingPong) { socket.abort(); retry(); return; }
        awaitingPong = true;
        socket.ping();
    });
}

void SignalKPublisher::configure(const SignalKConfig &value)
{
    stop();
    config = value;
}

void SignalKPublisher::start()
{
    if (active) return;
    const QUrl url(config.server);
    const auto label = config.source;
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")
        || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
        || (url.path() != "" && url.path() != "/")
        || label.isEmpty() || label.size() > 64
        || label.contains(QRegularExpression("[^A-Za-z0-9_-]"))
        || config.token.contains('\r') || config.token.contains('\n')) {
        emit statusChanged("Signal K: use an http(s) server origin and a source label containing letters, digits, _ or -");
        return;
    }
    active = true;
    connectToServer();
}

void SignalKPublisher::stop()
{
    const bool wasActive = active;
    active = ready = false;
    serverSelf.clear();
    aisWarnings.clear();
    ++generation;
    reconnect.stop(); deadline.stop(); heartbeat.stop(); flushDeadline.stop();
    if (authorization) { authorization->abort(); authorization->deleteLater(); authorization = nullptr; }
    network.clearConnectionCache();
    socket.abort(); // Discard all buffered samples; Stop must not drain old output.
    if (wasActive) emit statusChanged("Signal K: stopped");
}

void SignalKPublisher::retry()
{
    ready = false;
    heartbeat.stop(); deadline.stop(); flushDeadline.stop();
    if (active && !reconnect.isActive()) {
        emit statusChanged("Signal K: disconnected or unauthorized; retrying (check server and write token)");
        reconnect.start();
    }
}

void SignalKPublisher::connectToServer()
{
    if (!active) return;
    socket.abort();
    reconnect.stop();
    const auto run = ++generation;
    QUrl url(config.server);
    url.setPath("/skServer/loginStatus");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (!config.token.isEmpty()) request.setRawHeader("Authorization", "Bearer " + config.token.toUtf8());
    authorization = network.get(request);
    const auto reply = authorization;
    deadline.start();
    emit statusChanged("Signal K: checking write authorization");
    connect(reply, &QNetworkReply::finished, this, [this, reply, run] {
        reply->deleteLater();
        if (!active || run != generation) return;
        authorization = nullptr;
        const auto data = QJsonDocument::fromJson(reply->readAll()).object();
        const auto level = data.value("userLevel").toString();
        if (reply->error() != QNetworkReply::NoError
            || !data.contains("authenticationRequired")
            || (data.value("authenticationRequired").toBool() && level != "readwrite" && level != "admin")) {
            retry();
            return;
        }
        QUrl endpoint(config.server);
        endpoint.setScheme(endpoint.scheme() == "https" ? "wss" : "ws");
        endpoint.setPath("/signalk/v1/stream");
        endpoint.setQuery("subscribe=none");
        QNetworkRequest upgrade(endpoint);
        if (!config.token.isEmpty()) upgrade.setRawHeader("Authorization", "Bearer " + config.token.toUtf8());
        socket.open(upgrade);
    });
}

bool SignalKPublisher::publish(const tN2kMsg &message)
{
    if (!active || !ready) return false;
    const auto samples = values(message);
    if (samples.isEmpty()) return false;
    // Bound Qt's outbound buffer, including bursts of widgets on one scheduler
    // tick. There is no application/offline queue; abort discards pending data.
    // Native Signal K shorthand. Sending an NMEA-style source descriptor makes
    // server 2.11 normalize it to the device provider plus a fictitious talker.
    QString source = config.source;
    if (message.PGN == 130313 && message.DataLen == 8)
        source += "-humidity" + QString::number(message.Data[1]);
    const QJsonObject update{{"$source", source},
                             {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                             {"values", samples}};
    return sendDelta(QJsonObject{{"context", "vessels.self"}, {"updates", QJsonArray{update}}});
}

bool SignalKPublisher::publishAis(int pgn, const QMap<QString,QString> &fields,
                                 const QHash<QString,QDateTime> &sampleTimes)
{
    if (!active || !ready) return false;
    const auto reject=[this](const QString &reason) {
        if (!aisWarnings.contains(reason)) {
            aisWarnings.insert(reason);
            qWarning().noquote()<<reason;
            emit statusChanged(reason);
        }
        return false;
    };
    const auto delta=aisDelta(pgn,fields,sampleTimes,config.source);
    if (delta.isEmpty()) return reject("Signal K: AIS publication rejected; configure a nine-digit vessel MMSI beginning 2-7 (Signal K schema). NMEA2000 values are unchanged.");
    // Servers normalize their own MMSI context to self. Refuse that identity collision.
    if (delta.value("context").toString()==serverSelf) {
        return reject("Signal K: AIS target MMSI matches destination self; target not published");
    }
    return sendDelta(delta);
}

bool SignalKPublisher::sendDelta(const QJsonObject &delta)
{
    if (!active || !ready) return false;
    const auto payload=QJsonDocument(delta).toJson(QJsonDocument::Compact);
    if (socket.bytesToWrite() + payload.size() > 65536) { socket.abort(); retry(); return false; }
    const bool accepted = socket.sendTextMessage(QString::fromUtf8(payload)) > 0;
    if (!accepted) { socket.abort(); retry(); return false; }
    if (socket.bytesToWrite() > 0 && !flushDeadline.isActive()) flushDeadline.start();
    return accepted;
}
