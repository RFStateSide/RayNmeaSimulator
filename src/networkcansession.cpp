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
#include "networkcansession.h"
#include "rawcanprotocol.h"
#include "wavesharesession.h"
#include "routeservicepolicy.h"
#include "credentialstore.h"
#include <QDateTime>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>
NetworkCanConfig NetworkCanConfig::load(QSettings &s) {
    NetworkCanConfig c;
    c.host = s.value("NetworkCanHost", "127.0.0.1").toString();
    c.port = s.value("NetworkCanPort", 29501).toInt();
    c.interfaceName = s.value("NetworkCanInterface", "can0").toString();
    c.allowTx = s.value("NetworkCanAllowTx", false).toBool();
    auto id = WaveshareConfig::load(s, "NetworkCan");
    c.uniqueNumber = id.uniqueNumber;
    c.preferredSource = id.preferredSource;
    c.token = CredentialStore::read("NetworkNmea2000Token").value;
    return c;
}
NetworkCanSession::NetworkCanSession(QObject *p) : QObject(p) {
    socket.setProxy(QNetworkProxy::NoProxy);
    timer.setInterval(2);
    connect(&timer, &QTimer::timeout, this, &NetworkCanSession::tick);
    connect(&socket, &QTcpSocket::readyRead, this, &NetworkCanSession::read);
    connect(&socket, &QTcpSocket::errorOccurred, this, [this] {
        if (!stopping)
            fail(tr("Network CAN connection failed (%1): %2; TX revoked")
                     .arg(int(socket.error()))
                     .arg(socket.errorString()));
    });
    connect(&socket, &QTcpSocket::disconnected, this, [this] {
        if (!stopping)
            fail(tr("Network CAN disconnected; TX revoked"));
    });
}
NetworkCanSession::~NetworkCanSession() { stop(); }
void NetworkCanSession::configure(const NetworkCanConfig &c, const QString &role) {
    stop();
    config = c;
    ApplicationLog::registerSecret(config.token);
    start(role);
}
void NetworkCanSession::report(const QString &s) {
    if (text == s)
        return;
    text = s;
    ApplicationLog::event("raynmea.transport.network",objectName().isEmpty()?s:objectName()+": "+s);
    emit statusChanged(s);
}
void NetworkCanSession::fail(const QString &s) {
    stop();
    report(s);
    emit failed(s);
}
void NetworkCanSession::stop(bool again) {
    stopping = true;
    ++generation;
    timer.stop();
    online = false;
    core.reset();
    socket.abort();
    input.clear();
    client.clear();
    nonce.clear();
    sequence = 0;
    role = "rx";
    stopping = false;
    report(tr("Network CAN stopped; TX revoked"));
    if (again)
        start();
}
bool NetworkCanSession::start(const QString &r) {
    stop();
    if (config.host.trimmed().isEmpty() || config.port < 1 ||
        config.port > 65535 ||
        !QRegularExpression("^[A-Za-z0-9_-]{1,15}$")
             .match(config.interfaceName)
             .hasMatch() ||
        config.uniqueNumber < 1 || config.uniqueNumber > 2097151 ||
        config.preferredSource < 0 || config.preferredSource > 251 ||
        !QStringList{"rx", "tx", "service"}.contains(r)) {
        report(tr("Invalid Network CAN configuration"));
        return false;
    }
    if (r != "rx" && (!config.allowTx || config.token.isEmpty())) {
        report(tr("Network CAN is RX-only or has no secure access key"));
        return false;
    }
    role = r;
    deadline = QDateTime::currentMSecsSinceEpoch() + 5000;
    lastPing = 0;
    socket.connectToHost(config.host, config.port);
    timer.start();
    report(tr("Network CAN connecting to %1:%2 / %3 (%4); instrument Sending "
              "disabled until ready")
               .arg(config.host)
               .arg(config.port)
               .arg(config.interfaceName, r));
    return true;
}
bool NetworkCanSession::envelope(const QJsonObject &j) {
    if (socket.state() != QAbstractSocket::ConnectedState ||
        socket.bytesToWrite() > 8192)
        return false;
    const auto b = QJsonDocument(j).toJson(QJsonDocument::Compact) + '\n';
    return socket.write(b) == b.size();
}
bool NetworkCanSession::ready() const {
    return online && core && (role == "rx" || core->ready());
}
int NetworkCanSession::sourceAddress() const {
    return core ? core->GetN2kSource() : -1;
}
bool NetworkCanSession::send(const tN2kMsg &m, bool service) {
    if (!ready() || (service ? role != "service" : role != "tx") ||
        (service && !RouteServicePolicy::allowed(m)))
        return false;
    auto active = core;
    return active->SendMsg(m);
}
void NetworkCanSession::tick() {
    const qint64 n = QDateTime::currentMSecsSinceEpoch();
    if (!online && n >= deadline) {
        fail(tr("Network CAN handshake timed out"));
        return;
    }
    if (socket.state() == QAbstractSocket::ConnectedState &&
        n - lastPing >= 1000) {
        if (!envelope({{"kind", "ping"}})) {
            fail(tr("Network CAN backpressure; TX revoked"));
            return;
        }
        lastPing = n;
    }
    if (online && n >= deadline) {
        fail(tr("Network CAN heartbeat timed out; TX revoked"));
        return;
    }
    auto active = core;
    const auto g = generation;
    if (active)
        active->ParseMessages();
    if (g != generation)
        return;
    if (online && ready())
        report(
            role == "tx" ? tr("Network CAN Sending ready; source %1")
                               .arg(sourceAddress())
            : role == "service"
                ? tr("Network CAN route service ready; instrument Sending "
                     "disabled")
                : tr("Network CAN RX-only connected; no CAN application TX"));
}
void NetworkCanSession::read() {
    input += socket.readAll();
    if (input.size() > 65536) {
        fail(tr("Network CAN input overflow"));
        return;
    }
    while (input.contains('\n')) {
        const int at = input.indexOf('\n');
        const auto line = input.left(at);
        input.remove(0, at + 1);
        if (line.size() > 1024) {
            fail(tr("Network CAN frame envelope too large"));
            return;
        }
        QJsonParseError e;
        auto doc = QJsonDocument::fromJson(line, &e);
        if (e.error != QJsonParseError::NoError || !doc.isObject()) {
            fail(tr("Malformed Network CAN envelope"));
            return;
        }
        auto j = doc.object();
        auto k = j["kind"].toString();
        if (k == "hello") {
            if (online || j["version"].toInt() != 1 ||
                j["interface"].toString() != config.interfaceName ||
                j["nonce"].toString().size() != 64) {
                fail(tr("Network CAN protocol/interface mismatch"));
                return;
            }
            client = j["client"].toString();
            nonce = j["nonce"].toString();
            auto material =
                (nonce + '|' + client + '|' + role + '|' + config.interfaceName)
                    .toUtf8();
            auto proof =
                QMessageAuthenticationCode::hash(
                    material, config.token.toUtf8(), QCryptographicHash::Sha256)
                    .toHex();
            if (!envelope({{"kind", "auth"},
                           {"interface", config.interfaceName},
                           {"mode", role},
                           {"proof", QString::fromLatin1(proof)}})) {
                fail(tr("Network CAN authorization failed"));
                return;
            }
        } else if (k == "ready") {
            if (online || j["mode"].toString() != role) {
                fail(tr("Unexpected Network CAN authorization"));
                return;
            }
            online = true;
            deadline = QDateTime::currentMSecsSinceEpoch() + 4000;
            core = std::make_shared<RawCanProtocol>(
                role != "rx", config.uniqueNumber, config.preferredSource);
            const auto g = generation;
            core->deliver = [this, g](const tN2kMsg &m) {
                if (g != generation)
                    return;
                const auto active = core;
                if (role != "rx" && active && m.PGN == 60928 &&
                    m.DataLen == 8) {
                    int at = 0;
                    if (m.GetUInt64(at) ==
                        active->GetDeviceInformation().GetName()) {
                        fail(tr("Duplicate NMEA2000 NAME detected. Choose a "
                                "distinct unique device number; TX revoked."));
                        return;
                    }
                }
                emit received(m);
            };
            core->write = [this, g](quint32 id, quint8 len,
                                    const unsigned char *data) {
                if (g != generation || !online || role == "rx" ||
                    id > 0x1fffffff || len > 8)
                    return false;
                bool ok = envelope(
                    {{"kind", "frame"},
                     {"id", double(id | 0x80000000U)},
                     {"dlc", len},
                     {"data",
                      QString::fromLatin1(
                          QByteArray(reinterpret_cast<const char *>(data), len)
                              .toHex())}});
                if (!ok)
                    fail(tr("Network CAN output stalled; TX revoked"));
                return ok;
            };
            // Publish the RX connection epoch before another explicit management
            // action can observe connected()==true with stale registry context.
            report(role=="rx" ? tr("Network CAN RX-only connected; no CAN application TX")
                              : tr("Network CAN authorized; claiming source address"));
        } else if (k == "pong") {
            if (online)
                deadline = QDateTime::currentMSecsSinceEpoch() + 4000;
        } else if (k == "error") {
            fail(tr("Network CAN bridge rejected the session or detected a CAN "
                    "fault"));
            return;
        } else if (k == "frame") {
            if (!online)
                continue;
            const auto seq = j["seq"].toVariant().toULongLong();
            const double raw = j["id"].toDouble(-1);
            const int len = j["dlc"].toInt(-1);
            const auto hex = j["data"].toString();
            if (seq <= sequence || raw < 0 || raw > 4294967295.0 ||
                raw != double(quint32(raw)) || len < 0 || len > 8 ||
                hex.size() != 2 * len ||
                !QRegularExpression("^[0-9a-fA-F]*$").match(hex).hasMatch()) {
                fail(tr("Invalid Network CAN frame/order"));
                return;
            }
            sequence = seq;
            const quint32 id = quint32(raw);
            if (id & 0x20000000U) {
                fail(tr("CAN error received; TX revoked"));
                return;
            }
            if (!(id & 0x80000000U) || (id & 0x40000000U) ||
                j["origin"].toString() == client)
                continue;
            if (core->frames.size() >= 1024) {
                fail(tr("Network CAN receive queue overflow"));
                return;
            }
            WaveshareCodec::CanFrame f;
            f.id = id & 0x1fffffff;
            f.length = len;
            auto data = QByteArray::fromHex(hex.toLatin1());
            std::copy(data.begin(), data.end(), f.data.begin());
            if(logRawReception) ApplicationLog::canFrame("RX","Network NMEA2000",f.id,f.length,f.data.data());
            core->frames.enqueue(f);
        } else {
            fail(tr("Unknown Network CAN response"));
            return;
        }
    }
}
