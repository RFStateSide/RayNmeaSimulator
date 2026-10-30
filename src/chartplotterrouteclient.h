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

#pragma once
#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <functional>
#include "N2kMsg.h"
#include "receivedroutes.h"

// GUI-thread, bounded read-only route service. Transport authorization is owned
// separately by the caller; this class never enables instrument Sending.
class ChartplotterRouteClient : public QObject {
    Q_OBJECT
  public:
    struct Device {
        int source = -1;
        QString name, model;
        QString software, modelVersion;
        int productCode = -1;
    };
    struct Entry {
        int database = 0, id = 0, count = -1;
        QString name;
    };
    enum State {
        Idle,
        Opening,
        Discovering,
        Identity,
        Databases,
        Routes,
        Attributes,
        Geometry,
        VerifyGeometry,
        VerifyAttributes,
        Complete,
        Failed
    };
    Q_ENUM(State)
    explicit ChartplotterRouteClient(QObject *parent = nullptr);
    std::function<bool(const tN2kMsg &)> send;
    std::function<bool()> ready;
    std::function<int()> localAddress;
    QList<Device> devices() const { return known.values(); }
    QVector<Entry> entries() const { return listed; }
    State state() const { return current; }
    QString status() const { return text; }
    ReceivedRoutes::Route candidate() const { return completed; }
    bool busy() const { return current > Idle && current < Complete; }
    void discover();
    void resetSelection();
    void browse(const Device &device);
    void retrieve(const Entry &entry);
    void
    cancel(const QString &reason = QStringLiteral("Route service cancelled"));
    void ingest(const tN2kMsg &message);
    void advance(qint64 now); // deterministic deadlines for focused tests
  signals:
    void changed();
    void failed(const QString &reason);

  private:
    void set(State state, const QString &message);
    void fail(const QString &message);
    void begin(State after);
    void request(unsigned long pgn, const QList<QPair<int, int>> &selectors,
                 State state);
    void iso(int destination, unsigned long pgn);
    void finish();
    qint64 now() const { return clock.elapsed(); }
    QTimer timer;
    QElapsedTimer clock;
    State current = Idle, afterOpen = Discovering;
    QMap<int, Device> known;
    Device target;
    Entry selected;
    QVector<Entry> listed;
    QVector<int> databases;
    QMap<int, QJsonObject> positions, verification;
    QMap<QString, QString> attributes;
    QByteArray attributeBytes;
    ReceivedRoutes::Route completed;
    QString text;
    tN2kMsg pending;
    qint64 deadline = 0, operationDeadline = 0, lastSent = -1000;
    bool pendingSent = false;
    QSet<int> productRequested;
    int requests = 0, retries = 0, databaseIndex = 0, next = 0, expected = -1,
        routeTotal = -1, databaseTotal = -1;
};
