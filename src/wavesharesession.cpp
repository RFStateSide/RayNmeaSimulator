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
#include "serialportdiagnostics.h"
#include "wavesharesession.h"
#include "rawcanprotocol.h"
#include "routeservicepolicy.h"
#include "wavesharecodec.h"
#include "slcancodec.h"
#include "NMEA2000.h"
#include "N2kGroupFunctionDefaultHandlers.h"
#include <QScopedValueRollback>
#include <QRandomGenerator>
#include <QDebug>
#include <cstdlib>

WaveshareConfig WaveshareConfig::load(QSettings &s, const QString &prefix) {
    WaveshareConfig c;
    if (!s.contains(prefix + "UniqueNumber")) {
        s.setValue(prefix + "UniqueNumber", QRandomGenerator::global()->bounded(1u, 2097152u));
        s.sync();
    }
    bool numberOk = false, sourceOk = false;
    c.uniqueNumber = s.value(prefix + "UniqueNumber").toUInt(&numberOk);
    c.preferredSource = s.value(prefix + "PreferredSource", 44).toInt(&sourceOk);
    if (!numberOk || !sourceOk || s.status() != QSettings::NoError) c.uniqueNumber = 0;
    return c;
}

class WaveshareSession::ReceiveState { public: WaveshareCodec::Parser parser; SlcanCodec::Parser slcan; };

WaveshareSession::WaveshareSession(QObject *parent, PortFactory f)
    : QObject(parent), rx(new ReceiveState), factory(std::move(f))
{
    timer.setInterval(5);
    connect(&timer, &QTimer::timeout, this, &WaveshareSession::service);
    configurationDeadline.setSingleShot(true); configurationDeadline.setInterval(2000);
    connect(&configurationDeadline, &QTimer::timeout, this, [this] {
        fail(tr("Waveshare configuration write did not complete; reconnect explicitly."));
    });
}
WaveshareSession::~WaveshareSession() { stop(); }
quint64 WaveshareSession::blockedCanWrites() const { return core ? core->blocked : 0; }
bool WaveshareSession::isTransmitting() const {
    return !serviceLease && serviceReady();
}
int WaveshareSession::sourceAddress() const { return core ? core->GetN2kSource() : -1; }
bool WaveshareSession::serviceReady() const {
    return txAuthorized && current == Transmitting && port && !claimOutstanding
        && claimQuiet.isValid() && claimQuiet.elapsed() >= 250;
}
QString WaveshareSession::context(QString text) const {
    if (wireProtocol == CanableSlcan) text.replace("Waveshare", "CANable SLCAN");
    return text;
}
void WaveshareSession::setProtocol(Protocol value) {
    if (wireProtocol == value) return;
    stop(); wireProtocol = value; rx->slcan.reset();
    // Teardown used the old framing. Refresh identity only after switching;
    // this neither reopens the device nor changes transmission authorization.
    report(Closed, QStringLiteral("Waveshare stopped; reconnect RX or explicitly Enable Sending."));
}
void WaveshareSession::report(State state, const QString &original) {
    const auto text = context(original);
    if (state == current && message == text) return;
    current = state; message = text; ApplicationLog::event("raynmea.transport.serial",text); emit statusChanged(text);
}
void WaveshareSession::stop(const QString &reason) {
    // Revoke before closing or emitting any callback. No old core can enqueue again.
    ++generation; serviceLease=false;txAuthorized = false; timer.stop(); configurationDeadline.stop();
    if (port) {
        auto *old = port.data(); port = nullptr;
        disconnect(old, nullptr, this, nullptr);
        if (wireProtocol == CanableSlcan) {
            // Revoke host authorization first. C is best effort, not a queue-flush ACK.
            const bool boundary = pendingWrite.isEmpty() && inFlight == 0;
            if (!factory) old->clear(QSerialPort::AllDirections);
            if (boundary) {
                const auto written = old->write("C\r", 2);
                if (!factory && written == 2 && !old->waitForBytesWritten(25) && old->bytesToWrite())
                    qWarning() << "CANable SLCAN close command not drained; controller state unconfirmed.";
                if (written != 2) qWarning() << "CANable SLCAN close command incomplete; controller state unconfirmed.";
            } else qWarning() << "CANable SLCAN stopped during serial output; adapter close/queued frames unconfirmed.";
        } else if (!factory) old->clear(QSerialPort::AllDirections);
        old->close(); old->deleteLater();
    }
    core.reset(); rx->parser.reset(); rx->slcan.reset(); output.clear(); pendingWrite.clear();
    inFlight = configurationRemaining = 0; warned = false;
    claimOutstanding = false; claimQuiet.invalidate();
    waveshareFrameDrained.invalidate();
    report(Closed, reason);
}
void WaveshareSession::fail(const QString &text) {
    const auto detail = context(text);
    stop(detail); qWarning().noquote() << detail; report(Failed, detail); emit failed(detail);
}
bool WaveshareSession::start(const QString &name) { return open(name, false, {}); }
bool WaveshareSession::startTransmitting(const QString &name, const WaveshareConfig &identity) {
    return open(name, true, identity);
}
bool WaveshareSession::startService(const QString &name,const WaveshareConfig &id){bool ok=open(name,true,id);if(ok)serviceLease=true;return ok;}
bool WaveshareSession::sendService(const tN2kMsg &m){const auto active=core;if(!serviceLease||!serviceReady()||!active||!active->ready()||!RouteServicePolicy::allowed(m))return false;return active->SendMsg(m);}
bool WaveshareSession::open(const QString &name, bool transmit, const WaveshareConfig &identity) {
    stop();
    if (name.trimmed().isEmpty() || name == "None" || (transmit && !identity.valid())) {
        fail(tr("Waveshare: select a serial device and valid unique device number/source in Setup.")); return false;
    }
    const auto epoch = generation;
    txAuthorized = transmit;
    core = std::make_shared<WaveshareCore>(transmit, identity.uniqueNumber, identity.preferredSource);
    core->deliver = [this, epoch](const tN2kMsg &m) {
        if (epoch == generation && ((current == Receiving && !txAuthorized)||serviceLease) && port) emit received(m);
    };
    if (transmit) core->write = [this, epoch](quint32 id, quint8 n, const unsigned char *d) {
        return epoch == generation && enqueue(id, n, d);
    };
    port = factory ? factory(this) : new QSerialPort(this);
    if (!port) { fail(tr("Waveshare: cannot create serial session.")); return false; }
    auto *p = port.data(); p->setPortName(name); p->setReadBufferSize(8192);
    if (!p->setBaudRate(wireProtocol == CanableSlcan ? 115200 : 2000000) || !p->setDataBits(QSerialPort::Data8)
        || !p->setParity(QSerialPort::NoParity) || !p->setStopBits(QSerialPort::OneStop)
        || !p->setFlowControl(QSerialPort::NoFlowControl)) {
        fail(wireProtocol == CanableSlcan ? tr("CANable SLCAN: cannot configure 115200 8N1.") : tr("Waveshare: cannot configure 2 Mbaud 8N1.")); return false;
    }
    connect(p, &QSerialPort::errorOccurred, this, [this, epoch](QSerialPort::SerialPortError error) {
        if (epoch == generation && port && error != QSerialPort::NoError)
            fail(tr("Waveshare serial error: %1. Reconnect explicitly.").arg(serialPortDiagnostic(port->portName(), error, port->errorString())));
    });
    if (!p->open(QIODevice::ReadWrite)) {
        if (epoch == generation) fail(tr("Waveshare open failed: %1").arg(serialPortDiagnostic(p->portName(), p->error(), p->errorString())));
        return false;
    }
    if (epoch != generation || !port) return false;
    if (!factory) p->clear(QSerialPort::AllDirections);
    if (epoch != generation || !port) return false;
    connect(p, &QSerialPort::readyRead, this, [this, epoch] { read(epoch); });
    connect(p, &QSerialPort::bytesWritten, this, [this, epoch](qint64 n) {
        if (epoch != generation || n <= 0) return;
        inFlight -= n; progress.restart();
        if (inFlight < 0) { fail(tr("Waveshare unexpected serial write accounting; session stopped.")); return; }
        if (current == Configuring) {
            configurationRemaining -= n;
            if (configurationRemaining == 0) configurationComplete();
        } else if (wireProtocol == Waveshare && txAuthorized && !inFlight && pendingWrite.isEmpty()) {
            // Serial completion is much faster than CAN delivery. This Model A
            // drops burst frames without reporting backpressure. Leave the bench-
            // qualified 5 ms between complete CAN packets, never within a packet.
            waveshareFrameDrained.start();
        }
        if (epoch == generation) pumpWrites();
    });
    pendingWrite = wireProtocol == CanableSlcan ? SlcanCodec::configuration()
        : transmit ? WaveshareCodec::transmitConfiguration() : WaveshareCodec::receiveConfiguration();
    configurationRemaining = pendingWrite.size();
    report(Configuring, wireProtocol == CanableSlcan
        ? tr("CANable SLCAN: configuring 115200 baud / 250 kbps extended CAN.")
        : transmit
        ? tr("Waveshare TX: requesting 250 kbps / extended / mode 00.")
        : tr("Waveshare RX: configuring 250 kbps / extended / mode 02."));
    if (epoch != generation || !port) return false;
    progress.start(); configurationDeadline.start(); timer.start(); pumpWrites();
    return epoch == generation;
}
void WaveshareSession::configurationComplete() {
    configurationDeadline.stop();
    report(txAuthorized ? Claiming : AwaitingTraffic, wireProtocol == CanableSlcan
        ? (txAuthorized ? tr("CANable SLCAN: normal mode requested; claiming source address.")
                        : tr("CANable SLCAN: active normal mode requested; awaiting valid CAN data."))
        : txAuthorized
        ? tr("Waveshare TX: mode 00 requested (unacknowledged); claiming source address.")
        : tr("Waveshare RX: mode 02 requested; awaiting valid CAN data (configuration unacknowledged)."));
}
void WaveshareSession::pumpWrites() {
    if (pumping || !port || inFlight) return;
    QScopedValueRollback<bool> guard(pumping, true);
    if (pendingWrite.isEmpty() && !output.isEmpty()) {
        // Nonblocking, Waveshare-only rate limit; STOP still revokes and clears
        // queued traffic immediately. Configuration and SLCAN are not paced.
        if (wireProtocol == Waveshare && waveshareFrameDrained.isValid()
            && waveshareFrameDrained.elapsed() < 5) return;
        pendingWrite = output.dequeue();
    }
    if (pendingWrite.isEmpty()) return;
    const auto epoch = generation;
    const qint64 n = port->write(pendingWrite);
    if (epoch != generation) return;
    if (n < 0 || n > pendingWrite.size()) { fail(tr("Waveshare serial write failed; explicitly restart the session.")); return; }
    if (n) { pendingWrite.remove(0, n); inFlight += n; progress.restart(); }
}
bool WaveshareSession::enqueue(quint32 id, quint8 n, const unsigned char *data) {
    if (!txAuthorized || !port || (current != Claiming && current != Transmitting)) return false;
    const auto epoch = generation;
    if (n > 8 || id > 0x1fffffff) { fail(tr("Waveshare invalid CAN frame; session stopped.")); return false; }
    if (output.size() >= 256) { fail(tr("Waveshare TX queue overflow; session stopped. Explicitly Enable Sending to retry.")); return false; }
    WaveshareCodec::CanFrame f; f.id = id; f.length = n;
    if (((id >> 8) & 0x3ff00) == 60928) {
        claimOutstanding = true; claimQuiet.invalidate();
        report(Claiming, tr("Waveshare TX: source claim pending serial completion; instrument output paused."));
        if (epoch != generation || !txAuthorized || !port) return false;
    }
    std::copy(data, data + n, f.data.begin()); output.enqueue(wireProtocol == CanableSlcan ? SlcanCodec::encode(f) : WaveshareCodec::encode(f));
    pumpWrites(); return epoch == generation && txAuthorized && port;
}
bool WaveshareSession::send(const tN2kMsg &m) {
    const auto active = core;
    if (!isTransmitting() || !active || !active->ready()) return false;
    const auto epoch = generation;
    if (!active->SendMsg(m) && epoch == generation) fail(tr("Waveshare NMEA2000 send failed; session stopped."));
    return epoch == generation && txAuthorized;
}
void WaveshareSession::read(quint64 epoch) {
    if (epoch != generation || !port) return;
    const auto bytes = port->read(4096);
    if (bytes.isEmpty() || current == Configuring || !core) return;
    const auto frames = wireProtocol == CanableSlcan ? rx->slcan.feed(bytes) : rx->parser.feed(bytes);
    if (wireProtocol == CanableSlcan && rx->slcan.adapterError()) {
        fail(tr("CANable SLCAN adapter reported a command/frame error (BEL); Sending stopped. Reconnect explicitly.")); return;
    }
    if (!warned && (wireProtocol == CanableSlcan ? rx->slcan.rejectedBytes() : rx->parser.rejectedBytes())) {
        warned = true; qWarning().noquote() << context("Waveshare RX discarded malformed or unsupported serial bytes (further warnings coalesced).");
    }
    if (core->frames.size() + frames.size() > 1024) {
        fail(tr("Waveshare RX queue overflow; session stopped. Reconnect explicitly.")); return;
    }
    for (const auto &frame : frames) {
        ApplicationLog::canFrame("RX",wireProtocol==CanableSlcan?"CANable SLCAN":"Waveshare USB-CAN-A",frame.id,frame.length,frame.data.data());
        core->frames.enqueue(frame);
    }
    if (!frames.isEmpty() && !txAuthorized) report(Receiving, wireProtocol == CanableSlcan
        ? tr("CANable SLCAN: receiving validated extended CAN data.")
        : tr("Waveshare RX: receiving validated CAN data; mode 02 requested."));
    if (epoch == generation && port && port->bytesAvailable())
        QTimer::singleShot(0, this, [this, epoch] { read(epoch); });
}
void WaveshareSession::service() {
    if (servicing || !port) return;
    QScopedValueRollback<bool> guard(servicing, true);
    if (current != Configuring && (inFlight || !pendingWrite.isEmpty() || !output.isEmpty()) && progress.elapsed() >= 2000) {
        fail(tr("Waveshare serial output stalled; session stopped. Explicitly restart.")); return;
    }
    pumpWrites();
    const auto active = core; const auto epoch = generation;
    if (!active || current == Configuring) return;
    active->ParseMessages();
    if (epoch != generation || !txAuthorized) return;
    // A core timer alone is insufficient when a serial claim is backpressured.
    // Wait for local serial completion, then a claim interval. Neither is a CAN ACK.
    if (claimOutstanding && !inFlight && pendingWrite.isEmpty() && output.isEmpty()) {
        claimOutstanding = false; claimQuiet.start();
    }
    const bool ready = active->ready() && !claimOutstanding && claimQuiet.isValid() && claimQuiet.elapsed() >= 250;
    report(ready ? Transmitting : Claiming, ready
        ? (serviceLease ? tr("Route service: source %1 locally claimed; instrument Sending remains disabled.") : tr("Waveshare TX: source %1 locally claimed; Sending active.")).arg(active->GetN2kSource())
        : tr("Waveshare TX: claiming source %1; instrument output paused.").arg(active->GetN2kSource()));
}
