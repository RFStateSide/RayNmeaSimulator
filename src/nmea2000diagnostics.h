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
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>
class tN2kMsg;

namespace NmeaLookup
{
QString manufacturer(int code);
QString deviceClass(int code);
QString deviceFunction(int cls, int code);
QString industry(int code);
QString enumeration(const QString &table, int code);
QString pgnName(quint32 pgn);
QJsonObject provenance();
} // namespace NmeaLookup

// GUI-thread, receive-only observations. No transport, sender, settings or route
// client ownership. Consumers can use generation + immutable receipt identity.
class Nmea2000Diagnostics final : public QObject
{
    Q_OBJECT
  public:
    struct Field {
        QString name, value, interpretation;
    };
    struct Device {
        QString key, nameHex, wireName, confidence, model, software, modelVersion;
        QString promotedFrom; // Unambiguous provisional address key; no retrospective identity assertion.
        int source = -1, manufacturer = -1, function = -1, cls = -1, industry = -1;
        int lowerInstance = -1, upperInstance = -1, systemInstance = -1;
        int productCode = -1, nmeaVersion = -1;
        quint32 uniqueNumber = 0;
        bool arbitraryAddressCapable = false;
        QString serial;
        quint64 session = 0, messages = 0;
        bool hasName = false, active = true, conflict = false, provisionalProduct = false;
        QDateTime firstUtc, lastUtc;
        qint64 lastMs = 0, heartbeatMs = -1;
        int heartbeatInterval = -1;
    };
    struct Message {
        QString key, deviceKey, function, variant, status, sourceDescription;
        QString transport;
        QJsonObject receiptIdentity;
        QVector<Field> fields;
        QByteArray payload;
        quint32 pgn = 0;
        int source = 255, destination = 255, priority = 0, payloadManufacturer = -1;
        QString manufacturerRole;
        quint64 session = 0, count = 0;
        qint64 lastMs = 0;
        QDateTime firstUtc, lastUtc;
        bool malformed = false;
        bool qualified = false; // Qualified diagnostic fields, not a TX/control capability.
    };
    explicit Nmea2000Diagnostics(QObject *parent = nullptr);
    void setSession(const QString &key, const QString &label);
    quint64 generation() const
    {
        return epoch;
    }
    quint64 revision() const
    {
        return version;
    }
    QString sessionLabel() const
    {
        return label;
    }
    quint64 evictions() const
    {
        return evicted;
    }
    QString observe(const tN2kMsg &, quint64 expectedGeneration = 0, qint64 nowMs = -1);
    void markApplicationDecoded(const QString &pgn, const QString &source, const QStringList &values = {});
    QList<Device> devices() const;
    QList<Message> messages() const;
    Device device(const QString &key) const
    {
        return deviceRows.value(key);
    }
    Device sourceObservation(int source) const
    {
        return deviceRows.value(byAddress.value(source));
    }
    Message message(const QString &key) const
    {
        return rows.value(key);
    }
    QString describe(const Device &) const;
    QString deviceState(const Device &) const;
    static constexpr int DeviceLimit = 512, MessageLimit = 2048;

  private:
    Device &sourceDevice(int source, qint64 now);
    void claim(const tN2kMsg &, const QByteArray &, qint64 now);
    void product(Device &, const tN2kMsg &);
    void interpret(Message &);
    void prune();
    QElapsedTimer clock;
    QString context, label;
    quint64 epoch = 0, version = 0, evicted = 0;
    QMap<QString, Device> deviceRows;
    QMap<QString, Message> rows;
    QMap<int, QString> byAddress, retiredAddress;
    QMap<QString, QString> byName, latestRow;
    QMap<int, quint64> addressVersion;
};
