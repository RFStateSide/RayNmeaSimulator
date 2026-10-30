/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
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

#include "nmeasenderworker.h"
#include <QScopedValueRollback>
#include <QThread>
#include <cmath>

// The scheduler, model and transport handlers share the GUI thread. No queued
// transport dispatch survives stop; QObject ownership cancels pending timer calls.
NmeaSenderWorker::NmeaSenderWorker(QObject *parent) : QObject(parent), timer(new QTimer(this))
{
    timer->setInterval(250);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, &NmeaSenderWorker::sendNmeaMessage);
}

NmeaSenderWorker::~NmeaSenderWorker() { stopSending(); }

void NmeaSenderWorker::setPgnTableController(PgnTableController *controller)
{
    Q_ASSERT(QThread::currentThread() == thread());
    stopSending();
    if (model) disconnect(model, nullptr, this, nullptr);
    model = controller ? controller->getPgnTableModel() : nullptr;
    Q_ASSERT(!model || model->thread() == thread());
    if (model) {
        connect(model, &QAbstractItemModel::modelReset, this, [this] {
            ++modelRevision;
            if (model->getPgnList().isEmpty()) lastTimeSentMap.clear();
        });
        connect(model, &QObject::destroyed, this, &NmeaSenderWorker::stopSending);
    }
}

void NmeaSenderWorker::setPgnData(const PgnData &data) { Q_UNUSED(data); }

void NmeaSenderWorker::setMode(Mode value)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (value != Sender) stopSending();
    mode = value;
}

void NmeaSenderWorker::startSending()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (keepSending || mode != Sender || !model) return;
    ++generation;
    lastTimeSentMap.clear();
    messageTimer.restart();
    keepSending = true;
    timer->start();
}

void NmeaSenderWorker::stopSending()
{
    Q_ASSERT(QThread::currentThread() == thread());
    timer->stop();
    if (!keepSending) return;
    keepSending = false;
    ++generation;
    lastTimeSentMap.clear();
    emit finished();
}

void NmeaSenderWorker::sendNmeaMessage()
{
    sendNmeaMessageAt(messageTimer.isValid() ? messageTimer.elapsed() : 0);
}

void NmeaSenderWorker::sendNmeaMessageAt(qint64 now)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!keepSending || mode != Sender || !model || dispatching) return;
    QScopedValueRollback<bool> dispatchGuard(dispatching, true);
    const auto run = generation;
    const auto revision = modelRevision;
    // Copy identities; never retain references into the mutable model across a signal.
    const QList<PgnData> snapshot = model->getPgnList();
    for (auto it = lastTimeSentMap.begin(); it != lastTimeSentMap.end();) {
        if (model->getPgnDataToSend(it.key()).idX < 0) it = lastTimeSentMap.erase(it);
        else ++it;
    }
    for (const auto &entry : snapshot) {
        if (!keepSending || mode != Sender || !model || generation != run || modelRevision != revision) return;
        const auto data = model->getPgnDataToSend(entry.idX);
        if (data.idX < 0 || !data.enabled || data.pgn.toInt() < 0) continue;
        // Anchor new entries to the scheduler tick, not its delivery delay.
        // Otherwise a late first callback can put every deadline just after
        // the following timer ticks and repeatedly lose whole polling periods.
        double scheduled = now - now % timer->interval();
        if (lastTimeSentMap.contains(data.idX)) {
            const double previous = lastTimeSentMap.value(data.idX).time;
            if (now < previous + data.sendInterval) continue;
            // Keep the interval phase despite event-loop jitter. Missed periods
            // are skipped, never replayed as a burst after a stalled event loop.
            if (data.sendInterval > 0)
                scheduled = previous + std::floor((now - previous) / data.sendInterval) * data.sendInterval;
            else
                scheduled = now;
        }
        lastTimeSentMap.insert(data.idX, {data.idX, scheduled});
        QStringList params;
        for (const auto &param : data.paramDetails) params << param[0] + ":" + param[1];
        if (data.pgn.size() == 3) emit sendSnt(data.pgn, params);
        else if (data.pgn.toInt() != 0) {
            QHash<QString,QDateTime> times;
            if (data.pgn == "129039" || data.pgn == "129809" || data.pgn == "129810") {
                // Received/source-qualified streams are never reflected as simulated AIS targets.
                if (!data.source.isEmpty()) continue;
                for (const auto &param : data.paramDetails)
                    times.insert(param[0], model->parameterPublicationTime(data.idX, param[0]));
            }
            emit sendPgn(data.pgn.toInt(), params, times);
        }
    }
}
