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

#include "nmea2000diagnostics.h"
#include "proprietarydiagnostics.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <algorithm>
#include "NMEA2000.h"

namespace
{
const QJsonObject &catalog()
{
    static const QJsonObject data = [] {
        QFile file(":/nmea2000/lookups.json");
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object()
                                              : QJsonObject{};
    }();
    return data;
}
QString lookup(const char *table, const QString &key)
{
    return catalog()["lookups"].toObject()[table].toObject()[key].toString();
}
quint64 number(const QByteArray &b, int at, int bytes)
{
    quint64 result = 0;
    for (int i = 0; i < bytes; ++i)
        result |= quint64(quint8(b.at(at + i))) << (i * 8);
    return result;
}
QString hex(quint64 n, int digits = 0)
{
    return "0x" + QString::number(n, 16).rightJustified(digits, '0');
}
bool proprietary(quint32 p)
{
    return p == 61184 || p == 126720 || (p >= 65280 && p <= 65535) || (p >= 130816 && p <= 131071);
}
QString codeValue(int code, const QString &name)
{
    return QString::number(code) + (name.isEmpty() ? " (unlisted)" : " - " + name);
}
QString state(int value, const QStringList &names)
{
    return QString::number(value) + " - " +
           (value == 3 ? QString("Unavailable") : names.value(value, "Undefined"));
}
QString quantity(quint64 value, quint64 unavailable, double resolution, const QString &unit)
{
    if (value == unavailable)
        return "Unavailable (" + hex(value) + ")";
    if (value == unavailable - 1)
        return "Error (" + hex(value) + ")";
    if (value == unavailable - 2)
        return "Reserved (" + hex(value) + ")";
    return QString::number(value * resolution, 'g', 12) + unit + " (raw " + QString::number(value) + ")";
}
QString matchedVariant(quint32 pgn, const QByteArray &b, bool &shortVariant)
{
    QString best;
    int bestCount = -1;
    for (const auto &value : catalog()["variants"].toArray()) {
        const auto v = value.toObject();
        if (v["pgn"].toInt() != int(pgn))
            continue;
        const auto matches = v["matches"].toArray();
        if (matches.isEmpty())
            continue;
        bool valid = true;
        for (const auto &m : matches) {
            const auto f = m.toObject();
            const int offset = f["offset"].toInt(), bits = f["bits"].toInt();
            if (bits < 1 || bits > 32 || offset < 0 || offset + bits > b.size() * 8) {
                valid = false;
                break;
            }
            quint64 actual = 0;
            for (int i = 0; i < bits; ++i)
                actual |= quint64((quint8(b[(offset + i) / 8]) >> ((offset + i) % 8)) & 1) << i;
            if (actual != quint64(f["value"].toDouble())) {
                valid = false;
                break;
            }
        }
        if (valid && matches.size() > bestCount) {
            bestCount = matches.size();
            best = v["name"].toString();
            shortVariant = v["length"].toInt() > b.size();
        }
    }
    return best;
}
} // namespace
namespace NmeaLookup
{
QString manufacturer(int c)
{
    return lookup("MANUFACTURER_CODE", QString::number(c));
}
QString deviceClass(int c)
{
    return lookup("DEVICE_CLASS", QString::number(c));
}
QString deviceFunction(int cls, int c)
{
    return lookup("DEVICE_FUNCTION", QString::number(cls) + ":" + QString::number(c));
}
QString industry(int c)
{
    return lookup("INDUSTRY_CODE", QString::number(c));
}
QString enumeration(const QString &table, int code)
{
    return catalog()["lookups"].toObject()[table].toObject()[QString::number(code)].toString();
}
QString pgnName(quint32 p)
{
    return catalog()["names"].toObject()[QString::number(p)].toString("Undefined PGN function");
}
QJsonObject provenance()
{
    return catalog()["provenance"].toObject();
}
} // namespace NmeaLookup

Nmea2000Diagnostics::Nmea2000Diagnostics(QObject *parent) : QObject(parent)
{
    clock.start();
    setSession("initial", "No connection observed");
}
void Nmea2000Diagnostics::setSession(const QString &key, const QString &description)
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (context == key)
        return;
    context = key;
    label = description;
    ++epoch;
    ++version;
    for (auto &d : deviceRows)
        d.active = false;
    byAddress.clear();
    byName.clear();
    retiredAddress.clear();
    addressVersion.clear();
    latestRow.clear();
}
Nmea2000Diagnostics::Device &Nmea2000Diagnostics::sourceDevice(int source, qint64 now)
{
    if (!byAddress.contains(source)) {
        const auto key = QString("%1:address:%2:%3").arg(epoch).arg(source).arg(addressVersion.value(source));
        Device d;
        d.key = key;
        d.session = epoch;
        d.source = source;
        d.lastMs = now;
        d.confidence = source <= 251 ? "Identity unavailable (no observed claim)"
                                     : "Reserved/null source; identity unavailable";
        deviceRows[key] = d;
        byAddress[source] = key;
    }
    return deviceRows[byAddress[source]];
}
void Nmea2000Diagnostics::claim(const tN2kMsg &m, const QByteArray &b, qint64 now)
{
    if (m.Source > 251 || b.size() != 8)
        return;
    const auto n = number(b, 0, 8);
    if (n == 0 || n == quint64(-1))
        return;
    const QString name = QString::number(n, 16).rightJustified(16, '0');
    auto old = sourceDevice(m.Source, now);
    if (old.hasName && old.nameHex == name)
        return;
    // A fresh, different claim resolves provisional traffic at a retired address.
    retiredAddress.remove(m.Source);
    const auto key = QString("%1:name:%2").arg(epoch).arg(name);
    Device d;
    if (byName.contains(name)) {
        d = deviceRows.value(byName[name]);
        if (old.key != d.key) {
            if (old.hasName)
                deviceRows[old.key].active = false;
            else
                deviceRows.remove(old.key);
        }
        if (d.source != m.Source) {
            // Its old address may already belong to another observed NAME.
            // Moving a historical owner must not evict that newer owner.
            if (byAddress.value(d.source) == d.key) {
                retiredAddress[d.source] = d.key;
                byAddress.remove(d.source);
                ++addressVersion[d.source];
            }
            d.confidence = "Observed NAME moved; old address retired";
        }
    } else if (!old.hasName) {
        // An ambiguous provisional address must not donate product metadata or
        // conflict flags to a newly established, different identity.
        if (!old.conflict)
            d = old;
        if (!old.conflict)
            d.promotedFrom = old.key;
        deviceRows.remove(old.key);
        d.confidence = d.provisionalProduct ? "Observed NAME; product was observed before claim"
                                            : "Observed Address Claim";
    } else {
        deviceRows[old.key].active = false;
        d.confidence = "Observed new NAME at reused address; prior owner retired";
        ++addressVersion[m.Source];
    }
    d.key = key;
    d.source = m.Source;
    d.session = epoch;
    d.active = true;
    d.hasName = true;
    d.nameHex = name;
    d.wireName = QString::fromLatin1(b.toHex());
    tNMEA2000::tDeviceInformation info;
    info.SetName(n);
    d.uniqueNumber = info.GetUniqueNumber();
    d.arbitraryAddressCapable = (n >> 63) != 0;
    d.manufacturer = info.GetManufacturerCode();
    d.function = info.GetDeviceFunction();
    d.cls = info.GetDeviceClass();
    d.industry = info.GetIndustryGroup();
    d.lowerInstance = info.GetDeviceInstanceLower();
    d.upperInstance = info.GetDeviceInstanceUpper();
    d.systemInstance = info.GetSystemInstance();
    deviceRows[key] = d;
    byAddress[m.Source] = key;
    byName[name] = key;
}
void Nmea2000Diagnostics::product(Device &d, const tN2kMsg &m)
{
    if (m.DataLen != 134 || d.conflict)
        return;
    unsigned short version, code;
    unsigned char certification, load;
    char model[33]{}, software[33]{}, modelVersion[33]{}, serial[33]{};
    if (!ParseN2kPGN126996(m, version, code, sizeof(model), model, sizeof(software), software,
                           sizeof(modelVersion), modelVersion, sizeof(serial), serial, certification, load))
        return;
    auto clean = [](const char *s) {
        QString r = QString::fromLatin1(s).trimmed();
        for (auto &c : r)
            if (!c.isPrint())
                c = '?';
        return r;
    };
    d.model = clean(model);
    d.software = clean(software);
    d.modelVersion = clean(modelVersion);
    d.serial = clean(serial);
    d.productCode = code;
    d.nmeaVersion = version;
    d.provisionalProduct = !d.hasName;
    d.confidence =
        d.hasName ? "Observed claim and Product Information" : "Provisional product; NAME unavailable";
}
QString Nmea2000Diagnostics::describe(const Device &d) const
{
    QString maker = d.manufacturer < 0 ? "Manufacturer unavailable"
                                       : codeValue(d.manufacturer, NmeaLookup::manufacturer(d.manufacturer));
    return maker + " / " + (d.model.isEmpty() ? QString("model unavailable") : d.model) +
           QString(" / SA%1").arg(d.source);
}
QString Nmea2000Diagnostics::deviceState(const Device &d) const
{
    if (d.session != epoch)
        return "Historical connection";
    if (!d.active)
        return "Retired address owner";
    if (d.conflict)
        return "Conflicting/ambiguous NAME observations";
    if (d.heartbeatInterval > 0 && d.heartbeatMs >= 0 &&
        clock.elapsed() - d.heartbeatMs > qMax(5000, 3 * d.heartbeatInterval))
        return "Heartbeat overdue (passive observation only)";
    return d.confidence;
}
QString Nmea2000Diagnostics::observe(const tN2kMsg &m, quint64 expected, qint64 now)
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (expected && expected != epoch)
        return {};
    if (now < 0)
        now = clock.elapsed();
    const auto utc = QDateTime::currentDateTimeUtc();
    const QByteArray payload(reinterpret_cast<const char *>(m.Data),
                             qBound(0, m.DataLen, int(tN2kMsg::MaxDataLen)));
    if (m.PGN == 60928 && m.DataLen == 8)
        claim(m, payload, now);
    auto &d = sourceDevice(m.Source, now);
    if (retiredAddress.contains(m.Source)) {
        auto &prior = deviceRows[retiredAddress[m.Source]];
        if (prior.source != m.Source && now - prior.lastMs < 2000) {
            prior.conflict = true;
            d.conflict = true;
            d.confidence = "Traffic at retired address; identity ambiguous";
        }
    }
    d.lastMs = now;
    if (!d.firstUtc.isValid())
        d.firstUtc = utc;
    d.lastUtc = utc;
    ++d.messages;
    if (m.PGN == 126996)
        product(d, m);
    Message row;
    row.pgn = m.PGN;
    row.source = m.Source;
    row.destination = m.Destination;
    row.priority = m.Priority;
    row.payload = payload;
    row.session = epoch;
    row.transport = label;
    row.lastMs = now;
    row.lastUtc = utc;
    row.deviceKey = d.key;
    row.sourceDescription = describe(d);
    row.receiptIdentity = {{"source", m.Source},
                           {"source_manufacturer", d.manufacturer},
                           {"source_model", d.model},
                           {"name_at_receipt", d.nameHex},
                           {"confidence", deviceState(d)}};
    interpret(row);
    if (m.PGN == 65311 && row.payloadManufacturer == 1851 && payload.size() == 8 &&
        (number(payload, 0, 2) >> 13) == 4) {
        // Corroboration at this receipt only; reuse existing bounded observations.
        // Never turn a matching candidate into a standard instrument update.
        const auto reference = rows.value(latestRow.value(QString::number(m.Source) + ":127258"));
        const bool recent = reference.pgn == 127258 && !reference.malformed && reference.deviceKey == d.key &&
            reference.session == epoch && now >= reference.lastMs && now - reference.lastMs <= 5000 &&
            reference.payload.size() >= 6;
        QString comparison = "No recent same-owner standard sample";
        QString explanation = "Candidate stays unverified; no automatic requests or measurements";
        if (recent) {
            const auto candidate = number(payload, 3, 2), standard = number(reference.payload, 4, 2);
            comparison = candidate == standard ? "Same raw field bytes" : "Different raw field bytes";
            explanation = QString("Candidate bytes 3..4 versus PGN127258 bytes 4..5, same observed source/owner/session, standard sample age %1 ms; equality is not semantic qualification").arg(now - reference.lastMs);
            if (candidate >= 0x7ffd && candidate <= 0x7fff)
                explanation += "; candidate special-value convention remains unverified";
        }
        row.fields.append({"PGN127258 comparison at receipt", comparison, explanation});
    }
    if (m.DataLen < 0 || m.DataLen > 223 || m.PGN > 0x3ffff) {
        row.malformed = true;
        row.qualified = false;
        row.status = "Malformed logical envelope";
    }
    if (m.PGN == 126993 && !row.malformed) {
        d.heartbeatMs = now;
        const auto interval = number(payload, 0, 2);
        d.heartbeatInterval = interval < 65533 ? int(interval) : -1;
    }
    row.key = QString("%1:%2:%3:%4").arg(d.key).arg(m.Source).arg(m.PGN).arg(row.variant);
    const auto old = rows.value(row.key);
    row.count = old.count + 1;
    row.firstUtc = old.count ? old.firstUtc : utc;
    rows[row.key] = row;
    latestRow[QString::number(m.Source) + ":" + QString::number(m.PGN)] = row.key;
    ++version;
    prune();
    return row.key;
}
void Nmea2000Diagnostics::markApplicationDecoded(const QString &pgn, const QString &source,
                                                 const QStringList &values)
{
    const auto key = latestRow.value(source + ":" + pgn);
    if (!key.isEmpty() && rows.contains(key) && !rows[key].malformed) {
        rows[key].status = "Existing application instrument decoder";
        auto &fields = rows[key].fields;
        fields.erase(std::remove_if(fields.begin(), fields.end(),
                                    [](const Field &field) { return field.name.startsWith("Decoded "); }),
                     fields.end());
        for (int i = 0; i + 1 < values.size(); i += 2)
            rows[key].fields.append(
                {"Decoded " + values[i], values[i + 1],
                 "Existing decoder canonical value; units/validity follow instrument property definition"});
        ++version;
    }
}
void Nmea2000Diagnostics::prune()
{
    while (rows.size() > MessageLimit) {
        auto oldest = rows.begin();
        for (auto i = rows.begin(); i != rows.end(); ++i)
            if (i->lastUtc < oldest->lastUtc)
                oldest = i;
        const auto key = oldest.key();
        rows.erase(oldest);
        for (auto i = latestRow.begin(); i != latestRow.end();)
            if (i.value() == key)
                i = latestRow.erase(i);
            else
                ++i;
        ++evicted;
    }
    while (deviceRows.size() > DeviceLimit) {
        auto oldest = deviceRows.end();
        for (auto i = deviceRows.begin(); i != deviceRows.end(); ++i)
            if (i->session != epoch || !i->active)
                if (oldest == deviceRows.end() || i->lastUtc < oldest->lastUtc)
                    oldest = i;
        if (oldest == deviceRows.end())
            break;
        const auto key = oldest.key();
        deviceRows.erase(oldest);
        // Historical messages retain their immutable source snapshot.
        for (auto i = retiredAddress.begin(); i != retiredAddress.end();)
            if (i.value() == key)
                i = retiredAddress.erase(i);
            else
                ++i;
        for (auto i = byName.begin(); i != byName.end();)
            if (i.value() == key)
                i = byName.erase(i);
            else
                ++i;
        ++evicted;
    }
}
QList<Nmea2000Diagnostics::Device> Nmea2000Diagnostics::devices() const
{
    auto list = deviceRows.values();
    std::sort(list.begin(), list.end(), [](const Device &a, const Device &b) {
        return a.session != b.session ? a.session > b.session : a.source < b.source;
    });
    return list;
}
QList<Nmea2000Diagnostics::Message> Nmea2000Diagnostics::messages() const
{
    auto list = rows.values();
    std::sort(list.begin(), list.end(), [](const Message &a, const Message &b) {
        return a.session != b.session ? a.session > b.session
               : a.source != b.source ? a.source < b.source
                                      : a.pgn < b.pgn;
    });
    return list;
}

void Nmea2000Diagnostics::interpret(Message &r)
{
    const auto &b = r.payload;
    auto field = [&](const QString &n, const QString &v, const QString &meaning = "Observed") {
        r.fields.append({n, v, meaning});
    };
    auto malformed = [&](const QString &reason) {
        r.malformed = true;
        r.status = "Malformed: " + reason;
    };
    r.function = NmeaLookup::pgnName(r.pgn);
    r.status = "Raw; application decoder unavailable";
    if (proprietary(r.pgn)) {
        r.function = "Manufacturer-specific PGN; undefined variant";
        if (b.size() < 2) {
            malformed("Truncated proprietary header");
            return;
        }
        const auto h = number(b, 0, 2);
        r.payloadManufacturer = h & 2047;
        r.manufacturerRole = "Payload manufacturer";
        field("Payload manufacturer",
              codeValue(r.payloadManufacturer, NmeaLookup::manufacturer(r.payloadManufacturer)),
              "11-bit proprietary header; independent of source NAME");
        field("Header reserved bits", hex((h >> 11) & 3), "Reserved; preserved raw");
        field("Payload industry", codeValue(h >> 13, NmeaLookup::industry(h >> 13)));
        bool shortVariant = false;
        const auto match = matchedVariant(r.pgn, b, shortVariant);
        if (!match.isEmpty()) {
            r.function = match;
            r.status = "Header interpreted; semantic fields not yet qualified";
            field("Definition prefix", match,
                  shortVariant ? "Prefix candidate only; shorter than reference layout"
                               : "Matching prefix is not full semantic qualification");
        }
        if (ProprietaryDiagnostics::interpret(r)) return;
        if (r.pgn == 131056 || r.pgn == 130916 || r.pgn == 65535)
            r.status = "Opaque proprietary body; header interpreted";
        for (int i = 2; i < b.size(); ++i)
            field(QString("Undefined payload byte %1").arg(i), hex(quint8(b[i]), 2),
                  "Unknown semantics; raw value preserved");
        return;
    }
    if (r.pgn == 60928) {
        r.function = "ISO Address Claim";
        if (b.size() != 8) {
            malformed("Address Claim must contain 8 bytes");
            return;
        }
        field("NAME (wire byte order)", QString::fromLatin1(b.toHex()));
        field("NAME (numeric hex)", hex(number(b, 0, 8), 16));
        r.status = r.source > 251 || number(b, 0, 8) == 0 || number(b, 0, 8) == quint64(-1)
                       ? "Unusable/unknown NAME or source; raw claim retained"
                       : "Passive identity observation";
        return;
    }
    if (r.pgn == 126996) {
        r.function = "Product Information";
        if (b.size() != 134) {
            malformed("Product Information must contain 134 bytes");
            return;
        }
        r.status = "Passive product observation";
        return;
    }
    if (r.pgn == 126993) {
        r.function = "Heartbeat";
        if (b.size() != 8) {
            malformed("Heartbeat must contain 8 bytes");
            return;
        }
        r.status = "Protocol diagnostics; existing stack TX unchanged";
        field("Current transmission interval", quantity(number(b, 0, 2), 65535, 0.001, " s"),
              "Interval semantics; CANboat field-label discrepancy documented");
        field("Sequence counter", quantity(quint8(b[2]), 255, 1, {}));
        field("Controller 1", state(quint8(b[3]) & 3, {"Error Active", "Error Passive", "Bus Off"}));
        field("Controller 2", state((quint8(b[3]) >> 2) & 3, {"Error Active", "Error Passive", "Bus Off"}));
        field("Equipment status", state((quint8(b[3]) >> 4) & 3, {"Operational", "Fault", "Undefined"}));
        field("Reserved bits", hex(number(b, 3, 5) >> 6), "34 bits, preserved raw");
        return;
    }
    if (r.pgn == 126208) {
        r.function = "NMEA Group Function";
        if (b.size() < 4) {
            malformed("Truncated Group Function header");
            return;
        }
        const int f = quint8(b[0]);
        const auto target = quint32(number(b, 1, 3));
        r.variant = QString::number(f) + ":" + QString::number(target);
        field("Function", QString::number(f) + " - " +
                              QStringList{"Request", "Command", "Acknowledge", "Read Fields",
                                          "Read Fields Reply", "Write Fields", "Write Fields Reply"}
                                  .value(f, "Undefined"));
        field("Target PGN", QString::number(target));
        if (target > 0x3ffff) {
            malformed("Invalid target PGN");
            return;
        }
        int at = 4;
        if (f == 0) {
            if (b.size() < 11) {
                malformed("Truncated request parameters");
                return;
            }
            const auto interval = number(b, 4, 4), offset = number(b, 8, 2);
            field("Transmission interval", interval == 0xffffffff
                                               ? hex(interval) + " (protocol special value)"
                                               : QString::number(interval) + " ms");
            field("Transmission offset", offset == 0xffff ? hex(offset) + " (protocol special value)"
                                                          : QString::number(offset * 10) + " ms");
            const int count = quint8(b[10]);
            field("Parameter pair count", QString::number(count));
            at = 11;
            if (b.size() - at < count) {
                malformed("Missing request parameter indices");
                return;
            }
        } else if (f == 1 || f == 2) {
            if (b.size() < 6) {
                malformed("Truncated command/acknowledgment");
                return;
            }
            const int count = quint8(b[5]);
            field("Parameter count", QString::number(count));
            if (f == 1) {
                field("Priority setting", QString::number(quint8(b[4]) & 15));
                field("Reserved priority bits", hex(quint8(b[4]) >> 4));
                if (b.size() - 6 < count) {
                    malformed("Missing command parameter indices");
                    return;
                }
            } else {
                field("PGN error code", hex(quint8(b[4]) & 15));
                field("Interval/priority error code", hex(quint8(b[4]) >> 4));
                if (b.size() < 6 + (count + 1) / 2) {
                    malformed("Truncated acknowledgment parameter errors");
                    return;
                }
                for (int i = 0; i < count; ++i)
                    field(QString("Parameter error %1").arg(i + 1),
                          hex((quint8(b[6 + i / 2]) >> (4 * (i % 2))) & 15));
            }
            at = 6;
        } else if (f >= 3 && f <= 6) {
            const bool prop = proprietary(target);
            const int minimum = prop ? 9 : 7;
            if (b.size() < minimum) {
                malformed("Truncated read/write header");
                return;
            }
            if (prop) {
                const auto h = number(b, at, 2);
                r.payloadManufacturer = h & 2047;
                r.manufacturerRole = "Target manufacturer";
                field("Target manufacturer", codeValue(h & 2047, NmeaLookup::manufacturer(h & 2047)));
                field("Target industry", codeValue(h >> 13, NmeaLookup::industry(h >> 13)));
                field("Target reserved bits", hex((h >> 11) & 3));
                at += 2;
            }
            field("Unique request ID", QString::number(quint8(b[at++])));
            const int selections = quint8(b[at++]), parameters = quint8(b[at++]);
            field("Selection pair count", QString::number(selections));
            field("Parameter count", QString::number(parameters));
            if (b.size() - at < selections + parameters) {
                malformed("Missing read/write field indices");
                return;
            }
        }
        if (at < b.size())
            field("Remaining parameter/extension bytes", QString::fromLatin1(b.mid(at).toHex()),
                  "Undefined target-dependent widths; never guessed or transmitted");
        r.status = f <= 6 ? "Partial interpretation: protocol header; target-dependent body raw"
                          : "Undefined Group Function; raw body retained";
        return;
    }
    field("Raw payload", QString::fromLatin1(b.toHex()), "No qualified diagnostic field map");
}
