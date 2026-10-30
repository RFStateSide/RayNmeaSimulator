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

#include "signalksetupclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUuid>

SignalKSetupClient::SignalKSetupClient(QObject *parent) : QObject(parent)
{
    poll.setSingleShot(true); poll.setInterval(2000);
    deadline.setSingleShot(true);
    connect(&poll, &QTimer::timeout, this, &SignalKSetupClient::pollAccess);
    connect(&deadline, &QTimer::timeout, this, [this] {
        finish(operation == Access
            ? "Access request expired locally. Retry to check the same request; approval may still be pending on the server."
            : "Connection test timed out. Check server connectivity and WebSocket support.");
    });
    socket.setMaxAllowedIncomingMessageSize(65536);
    socket.setMaxAllowedIncomingFrameSize(65536);
    connect(&socket, &QWebSocket::textMessageReceived, this, [this](const QString &text) {
        if (operation != Test) return;
        const auto hello = QJsonDocument::fromJson(text.toUtf8()).object();
        if (hello.value("self").toString().isEmpty() || hello.value("version").toString().isEmpty()) {
            finish("Connection failed: invalid Signal K WebSocket hello. Check the server endpoint."); return;
        }
        finish("Connection ready: authenticated read/write access and Signal K WebSocket hello verified. No measurements sent; Sending remains unchanged.");
        emit connectionReady();
    });
    connect(&socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (operation == Test) finish("WebSocket connection failed. Check the token, TLS certificate, server and proxy WebSocket configuration.");
    });
    connect(&socket, &QWebSocket::disconnected, this, [this] {
        if (operation == Test) finish("WebSocket closed before the Signal K hello. Check server logs and token permissions.");
    });
}
SignalKSetupClient::~SignalKSetupClient() { cancel(); }
void SignalKSetupClient::setTimeouts(int requestMs, int approvalMs, int pollMs)
{
    requestTimeout = requestMs; approvalTimeout = approvalMs; poll.setInterval(pollMs);
}
bool SignalKSetupClient::validate(const SignalKConfig &value)
{
    const QUrl url(value.server);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https") || url.host().isEmpty()
        || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
        || (!url.path().isEmpty() && url.path() != "/")) {
        emit statusChanged("Enter a server origin such as http://hostname:3000 or https://hostname, without credentials, paths or query parameters."); return false;
    }
    if (value.source.isEmpty() || value.source.size() > 64 || value.source.contains(QRegularExpression("[^A-Za-z0-9_-]"))) {
        emit statusChanged("Enter a publisher source label using 1-64 letters, digits, _ or -."); return false;
    }
    if (value.token.contains('\r') || value.token.contains('\n')) {
        emit statusChanged("Invalid token format. Paste only the device token, without line breaks."); return false;
    }
    return true;
}
void SignalKSetupClient::cancel()
{
    const bool busy = isBusy();
    operation = Idle; ++generation; poll.stop(); deadline.stop();
    if (reply) { auto old = reply; reply = nullptr; old->abort(); old->deleteLater(); }
    socket.abort(); network.clearConnectionCache();
    if (busy) {
        emit busyChanged(false);
        emit statusChanged("Cancelled locally. A pending administrator request can be resumed with Request Access; cancellation does not revoke server access.");
    }
}
void SignalKSetupClient::finish(const QString &status)
{
    cancel(); emit statusChanged(status);
}
void SignalKSetupClient::requestAccess(const SignalKConfig &value)
{
    if (isBusy() || !validate(value)) return;
    config = value;
    QUrl origin(config.server); origin.setPath("");
    const QString server = origin.toString();
    if (pendingOrigin != server) {
        pendingId.clear(); clientId.clear(); uncertainRequest = false; pendingOrigin = server;
    }
    if (uncertainRequest && pendingId.isEmpty()) {
        emit statusChanged("The previous request outcome is unknown. Check Signal K Admin for this device and deny/remove any pending request, then restart the application before retrying. Manual token entry is also available."); return;
    }
    operation = Access; emit busyChanged(true); deadline.start(approvalTimeout);
    if (!pendingId.isEmpty()) { pollAccess(); return; }
    clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    emit statusChanged("Requesting read/write measurement access. Administrator approval is required.");
    uncertainRequest = true;
    send("/signalk/v1/access/requests", QJsonDocument(QJsonObject{
        {"clientId", clientId}, {"description", "RayNmeaSimulator (" + config.source + ")"},
        {"permissions", "readwrite"}}).toJson(QJsonDocument::Compact));
}
void SignalKSetupClient::pollAccess()
{
    if (operation != Access) return;
    send("/signalk/v1/requests/" + QString::fromLatin1(QUrl::toPercentEncoding(pendingId)));
}
void SignalKSetupClient::testConnection(const SignalKConfig &value)
{
    if (isBusy() || !validate(value)) return;
    config = value; operation = Test; emit busyChanged(true);
    deadline.start(requestTimeout);
    emit statusChanged("Testing server availability and read/write authorization; no measurements will be sent.");
    send("/skServer/loginStatus");
}
void SignalKSetupClient::send(const QString &path, const QByteArray &body)
{
    QUrl url(config.server); url.setPath(path, QUrl::TolerantMode);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(requestTimeout);
    if (operation == Test && !config.token.isEmpty()) request.setRawHeader("Authorization", "Bearer " + config.token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    reply = body.isEmpty() ? network.get(request) : network.post(request, body);
    const auto current = reply; const auto run = generation;
    connect(current, &QNetworkReply::readyRead, this, [current] {
        if (current->bytesAvailable() > 65536) current->abort();
    });
    connect(current, &QNetworkReply::finished, this, [this, current, run] {
        current->deleteLater();
        if (run != generation || operation == Idle) return;
        reply = nullptr;
        const int code = current->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 401 || code == 403) {
            if (operation == Access && pendingId.isEmpty()) uncertainRequest = false;
            finish(operation == Test ? "Authorization failed: token is missing, invalid, expired or lacks access. Request a read/write device token."
                                    : "Access request refused. Ask the administrator to enable device access requests."); return;
        }
        if (code >= 300 && code < 400) { finish("Server redirected the request. Enter its final http(s) origin directly."); return; }
        if (code == 0 && current->error() != QNetworkReply::NoError) {
            QString failure;
            switch (current->error()) {
            case QNetworkReply::HostNotFoundError:
                if (operation == Access && pendingId.isEmpty()) uncertainRequest = false;
                failure = "Server hostname was not found. Check the server address and DNS."; break;
            case QNetworkReply::ConnectionRefusedError:
                if (operation == Access && pendingId.isEmpty()) uncertainRequest = false;
                failure = "Server refused the connection. Check the port and that Signal K is running."; break;
            case QNetworkReply::SslHandshakeFailedError:
                if (operation == Access && pendingId.isEmpty()) uncertainRequest = false;
                failure = "TLS verification failed. Use a trusted certificate matching the server hostname."; break;
            case QNetworkReply::TimeoutError:
                failure = "Server request timed out. Check connectivity and retry. An access request may still be pending in Signal K Admin."; break;
            default:
                failure = "Network request failed. Check connectivity and retry; an access request may still be pending in Signal K Admin.";
            }
            finish(failure); return;
        }
        if (current->error() != QNetworkReply::NoError || code < 200 || code >= 300) {
            finish(operation == Access && !pendingId.isEmpty() && (code == 404 || code == 500)
                ? "Access request no longer available (server restart or expiry). Ask the administrator to check pending requests, then restart the application to submit a new request."
                : "Request failed. Check server address, connectivity, TLS certificate and device access support; retry when corrected."); return;
        }
        const auto data = QJsonDocument::fromJson(current->readAll()).object();
        if (data.isEmpty()) { finish("Invalid server response. Check the Signal K server address and API compatibility."); return; }
        handle(data);
    });
}
void SignalKSetupClient::handle(const QJsonObject &data)
{
    if (operation == Access) {
        const auto id = data.value("requestId").toString();
        if (id.isEmpty() || id.size() > 256) { finish("Access request failed: server did not return a valid request identifier."); return; }
        pendingId = id; uncertainRequest = false;
        const auto state = data.value("state").toString();
        if (state == "PENDING") {
            emit statusChanged("Pending administrator approval: approve RayNmeaSimulator in Signal K Admin > Security > Access Requests. Request Access resumes after cancellation.");
            poll.start(); return;
        }
        const auto result = data.value("accessRequest").toObject();
        if (state == "COMPLETED") {
            pendingId.clear();
            if (result.value("permission").toString() == "DENIED") { finish("Access denied by administrator. Request Access to retry, or enter an existing read/write token."); return; }
            const auto token = result.value("token").toString();
            if (result.value("permission").toString() == "APPROVED" && !token.isEmpty() && !token.contains('\r') && !token.contains('\n')) {
                finish("Access approved. Token retrieved into the masked token field. Use Test Connection, then Save to persist it.");
                emit tokenApproved(token); return;
            }
            finish("Access request failed or expired. Check administrator settings and retry."); return;
        }
        pendingId.clear();
        finish("Access request expired or returned an unsupported state. Check administrator settings and retry."); return;
    }
    if (!data.value("authenticationRequired").isBool()) { finish("Server did not provide Signal K authorization status. Check the server origin."); return; }
    const auto level = data.value("userLevel").toString();
    if (level != "readwrite" && level != "admin") {
        finish(config.token.isEmpty() ? "No authenticated device token. Request Access or enter an approved read/write token."
                                     : "Token is invalid, expired or read-only. Request an approved read/write token."); return;
    }
    QUrl endpoint(config.server); endpoint.setScheme(endpoint.scheme() == "https" ? "wss" : "ws");
    endpoint.setPath("/signalk/v1/stream"); endpoint.setQuery("subscribe=none");
    QNetworkRequest request(endpoint);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Authorization", "Bearer " + config.token.toUtf8());
    emit statusChanged("Read/write authorization verified; waiting for Signal K WebSocket hello.");
    socket.open(request);
}
