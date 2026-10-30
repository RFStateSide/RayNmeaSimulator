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

#ifndef NMEASENDERWORKER_H
#define NMEASENDERWORKER_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include "pgntablecontroller.h"
#include "pgntablemodel.h"

struct LastTimeSent
{
    int idX;
    double time; // Scheduled time, not the potentially late callback time.
};

class NmeaSenderWorker : public QObject
{
    Q_OBJECT

public:
    explicit NmeaSenderWorker(QObject *parent = nullptr);
    ~NmeaSenderWorker();

    void setPgnTableController(PgnTableController *pgnTableCtrl); // Set the PGN table controller

    void setPgnData(const PgnData &pgnData); // Retained compatibility hook
    bool isSending() const { return keepSending; }
    void setMode(Mode mode);

public slots:
    void startSending();    // Slot to start sending NMEA messages
    void stopSending();     // Slot to stop sending
    void sendNmeaMessage(); // Slot to send NMEA messages


signals:
    void finished();                                     // Signal emitted when finished
    void sendPgn(int pgn, const QStringList &paramList, const QHash<QString,QDateTime> &sampleTimes = {}); // Signal to send a PGN
    void sendSnt(const QString &snt, const QStringList &paramList); // Signal to send a SNT

private:
    friend class SenderLifecycleTests;
    void sendNmeaMessageAt(qint64 now);
    QPointer<PgnTableModel> model;
    QElapsedTimer messageTimer;
    bool keepSending = false;
    bool dispatching = false;
    Mode mode = Sender;
    quint64 generation = 0;
    quint64 modelRevision = 0;
    QTimer *timer;
    QMap<int, LastTimeSent> lastTimeSentMap;
};

#endif // NMEASENDERWORKER_H
