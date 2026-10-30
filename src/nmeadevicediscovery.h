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
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>
#include <QMap>
#include <QSet>
#include <functional>
class tN2kMsg;
class Nmea2000Diagnostics;

// Bounded request orchestration; identity lives only in the passive registry.
// Existing transport cores retain all address claiming / protocol scheduling.
class NmeaDeviceDiscovery final : public QObject
{
    Q_OBJECT
  public:
    explicit NmeaDeviceDiscovery(Nmea2000Diagnostics *, QObject *parent = nullptr);
    std::function<QString()> open; // Empty means an authorized transport is opening.
    std::function<bool()> ready;
    std::function<bool(const tN2kMsg &)> send;
    std::function<void()> release;
    std::function<int()> source;
    bool busy() const
    {
        return running;
    }
    QString status() const
    {
        return text;
    }
    void start();
    void cancel(const QString &reason = QStringLiteral("Discovery cancelled; observations retained."));
    void fail(const QString &reason);
    void ingest(const tN2kMsg &);
    void advance(qint64 now); // Deterministic tests; production uses elapsed time.
  signals:
    void changed();

  private:
    void finish(const QString &);
    void progress();
    QPointer<Nmea2000Diagnostics> registry;
    QTimer timer;
    QElapsedTimer clock;
    bool running = false, requesting = false, finishing = false;
    quint64 generation = 0;
    qint64 deadline = 0, lastRequest = -250;
    int submissions = 0;
    QString text = "Discover Devices sends bounded identity requests only after an explicit click.";
    QSet<int> expected;
    QSet<QString> claims, products, rejected;
    QMap<int, QString> addressClaims;
    struct Attempt {
        int address = -1, count = 0;
    };
    QMap<QString, Attempt> attempts;
};
