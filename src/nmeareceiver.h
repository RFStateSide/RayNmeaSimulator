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

#ifndef NMEARECEIVER_H
#define NMEARECEIVER_H

#include <QObject>
#include "maincontroller.h"
#include "nmea0183handler.h"
#include "nmea2000handler.h"

class NmeaReceiver : public QObject
{
    Q_OBJECT
public:
    explicit NmeaReceiver(MainController *mainController,
                          PgnTableController *pgnTableController,
                          Nmea2000Handler *nmea2000Handler,
                          Nmea0183Handler *nmea0183Handler,
                          QObject *parent = nullptr);

public slots:
    void processNmeaPgnSnt(QString pgn, const QStringList &paramList, const QString &source);

signals:
    void addRcvdPgnSnt(const QString &pgn, const QStringList &paramList, const QString &source);
    void updateRcvdPgnSnt(int id, const QStringList &paramList);

private:
    MainController *m_mainController;
    PgnTableController *m_pgnTableController;
    Nmea2000Handler *m_nmea2000Handler;
    Nmea0183Handler *m_nmea0183Handler;

    // Runtime live-Viewer ownership only. Never serialized as a vessel subscription.
    struct ReceiveOwner {
        QString key, promotedFrom;
        quint64 generation = 0;
    };
    ReceiveOwner m_incomingOwner;
    QHash<int, ReceiveOwner> m_liveOwners;
    QList<PgnData> receiveCandidates(const QString &pgn, const QString &source);

    bool isParamValueMatch(QString paramName, const QStringList &params1, const QList<QList<QString>> &params2);
};

#endif // NMEARECEIVER_H
