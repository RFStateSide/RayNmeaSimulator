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

// Adapted from RF StateSide BrickSuite; owner-attested internal reuse (2026-10-28).
// BrickSuite retains its existing license; this adaptation is AGPL-3.0-or-later.
#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QtGlobal>
#include <functional>
class QObject;
class QSettings;
class tN2kMsg;

// Qt-native logging adapted from BrickSuite's Logger lifecycle and viewer model.
// This API only observes data: it owns no transport or simulation objects.
namespace ApplicationLog
{
struct Options {
    int level = 1; // Debug, Information, Warning, Critical; explicit severity ranks.
    bool bindings = false, rx = true, tx = true, nmea2000 = true, nmea0183 = true, rawCan = true;
    int applicationMiB = 2, trafficMiB = 8; // Current plus four archives per channel.
    QStringList disabledCategories;
    static Options load(QSettings &);
    void save(QSettings &) const;
};
bool initialize(const QString &directory = {});
void shutdown();
void configure(const Options &);
Options options();
void registerSecret(const QString &);
QString sanitized(QString);
void event(const QString &category, const QString &message, QtMsgType level = QtInfoMsg);
bool reportUndecodedPgn(quint32 pgn); // At most one accepted Debug record per PGN per logger/app run.
void binding(const QJsonObject &);
bool bindingEnabled();
void onBindingsEnabled(QObject *context, std::function<void()> snapshot);
bool startTraffic(); // Asynchronous: inspect health/recording for file-open outcome.
void stopTraffic();
bool recording();
quint64 trafficGeneration();
QString trafficState();
QString health(bool traffic);
QString filePath(bool traffic);
// 1: snapshot read, 0: writer busy (retain displayed text), -1: unavailable.
int readTail(bool traffic, QByteArray &output, int maximumBytes = 256 * 1024);
quint64 dropped(bool traffic);
quint64 written(bool traffic);
void clear(bool traffic);    // Current file only; archives deliberately retained.
bool clearing(bool traffic); // True until the asynchronous current-file clear has finished.
void exportCurrent(bool traffic, const QString &destination);
bool flush(int timeoutMs = 3000); // For orderly shutdown/tests, never used in traffic callbacks.
void retryApplication();
void n2k(const char *direction, const QString &transport, const tN2kMsg &, const QString &status,
         int sessionSource = -1);
void sentence(const char *direction, const QString &transport, const QByteArray &, const QString &status);
void canFrame(const char *direction, const QString &transport, quint32 id, quint8 length,
              const unsigned char *data);
#ifdef Q_OS_LINUX
// Native SocketCAN IDs include flags; preserve only extended data frames.
void socketCanFrame(quint32 nativeId, quint8 length, const unsigned char *data);
#endif
} // namespace ApplicationLog
