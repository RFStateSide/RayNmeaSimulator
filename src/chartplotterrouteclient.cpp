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

#include "chartplotterrouteclient.h"
#include "expansioninstruments.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>
#include <stdexcept>
#include "NMEA2000.h"

namespace {
struct Reader {
    const tN2kMsg &m;
    int at = 0;
    quint64 u(int n) {
        if (at + n > m.DataLen)
            throw std::runtime_error("Truncated route service reply");
        quint64 v = 0;
        for (int i = 0; i < n; ++i)
            v |= quint64(m.Data[at++]) << (8 * i);
        return v;
    }
    QString str() {
        int n = int(u(1)), enc = int(u(1));
        if (n < 2 || n > 252 || at + n - 2 > m.DataLen)
            throw std::runtime_error("Invalid route name length");
        QString s;
        if (enc == 1)
            s = QString::fromLatin1(reinterpret_cast<const char *>(m.Data + at),
                                    n - 2);
        else if (enc == 0 && !(n % 2)) {
            for (int i = 0; i < n - 2; i += 2)
                s += QChar(qFromLittleEndian<quint16>(m.Data + at + i));
        } else
            throw std::runtime_error("Unsupported route name encoding");
        at += n - 2;
        if (s.size() > 30)
            throw std::runtime_error("Route name exceeds protocol limit");
        for (auto c : s)
            if (!c.isPrint())
                throw std::runtime_error("Invalid route name");
        return s;
    }
    void end() {
        if (at != m.DataLen)
            throw std::runtime_error("Unexpected route service trailing bytes");
    }
};
QMap<QString, QString> map(const QStringList &f) {
    QMap<QString, QString> r;
    for (int i = 0; i + 1 < f.size(); i += 2)
        r[f[i]] = f[i + 1];
    return r;
}
QByteArray bytes(const tN2kMsg &m) {
    return QByteArray(reinterpret_cast<const char *>(m.Data), m.DataLen);
}
} // namespace
ChartplotterRouteClient::ChartplotterRouteClient(QObject *p) : QObject(p) {
    clock.start();
    timer.setInterval(25);
    connect(&timer, &QTimer::timeout, this, [this] { advance(now()); });
}
void ChartplotterRouteClient::set(State s, const QString &t) {
    current = s;
    text = t;
    emit changed();
}
void ChartplotterRouteClient::fail(const QString &t) {
    timer.stop();
    completed = {};
    set(Failed, t);
    emit failed(t);
}
void ChartplotterRouteClient::cancel(const QString &t) {
    timer.stop();
    pending.Clear();
    completed = {};
    set(Idle, t);
}
void ChartplotterRouteClient::begin(State after) {
    pending.Clear();
    pendingSent = false;
    completed = {};
    requests = 0;
    retries = 0;
    operationDeadline = now() + 60000;
    deadline = now() + 5000;
    afterOpen = after;
    set(Opening, tr("Opening authorized route-service session"));
    timer.start();
}
void ChartplotterRouteClient::resetSelection() {
    target={};listed.clear();
    cancel(tr("Refresh Routes for the selected chartplotter"));
}
void ChartplotterRouteClient::discover() {
    target = {};
    productRequested.clear();
    begin(Discovering);
}
void ChartplotterRouteClient::browse(const Device &d) {
    if (d.source < 0 || d.source > 251 || d.name.size() != 16) {
        fail(tr("Select a device with a valid NAME"));
        return;
    }
    target = d;
    listed.clear();
    databases.clear();
    databaseIndex = 0;
    databaseTotal = -1;
    begin(Identity);
}
void ChartplotterRouteClient::retrieve(const Entry &e) {
    if (!known.contains(target.source) ||
        known[target.source].name != target.name) {
        fail(tr("Chartplotter identity changed"));
        return;
    }
    bool listedEntry = false;
    for (const auto &entry : listed)
        if (entry.database == e.database && entry.id == e.id &&
            entry.name == e.name)
            listedEntry = true;
    if (!listedEntry) {
        fail(tr("Refresh the route list before retrieval"));
        return;
    }
    selected = e;
    positions.clear();
    verification.clear();
    attributes.clear();
    attributeBytes.clear();
    begin(Attributes);
}
void ChartplotterRouteClient::iso(int dst, unsigned long pgn) {
    tN2kMsg m;
    m.SetPGN(59904);
    m.Priority = 6;
    m.Destination = dst;
    m.Add3ByteInt(pgn);
    if (++requests > 64 || !send || !send(m))
        fail(tr("Route-service transport rejected a request"));
    lastSent = now();
}
void ChartplotterRouteClient::request(unsigned long pgn,
                                      const QList<QPair<int, int>> &pairs,
                                      State s) {
    if (requests >= 64) {
        fail(tr("Route-service request budget exhausted"));
        return;
    }
    pending.Clear();
    pending.SetPGN(126208);
    pending.Priority = 3;
    pending.Destination = target.source;
    pending.AddByte(0);
    pending.Add3ByteInt(pgn);
    pending.Add4ByteUInt(0xffffffff);
    pending.Add2ByteUInt(0xffff);
    pending.AddByte(pairs.size());
    for (auto pair : pairs) {
        if (pair.second < 0 || pair.second > 65532) {
            fail(tr("Invalid observed service identifier"));
            return;
        }
        pending.AddByte(pair.first);
        pending.Add2ByteUInt(pair.second);
    }
    pendingSent = false;
    retries = 0;
    deadline = now() + 5000;
    set(s, tr("Waiting for PGN %1 (source %2, NAME %3)")
               .arg(pgn)
               .arg(target.source)
               .arg(target.name));
    // One outstanding request, >=250ms between logical service submissions.
    if (now() - lastSent >= 250) {
        if (++requests > 64 || !send || !send(pending)) {
            fail(tr("Route-service send failed"));
            return;
        }
        lastSent = now();
        pendingSent = true;
    } else
        deadline = now() + 5000; // advance() dispatches the unsent message.
}
void ChartplotterRouteClient::advance(qint64 n) {
    if (!busy())
        return;
    if (n >= operationDeadline) {
        fail(tr("Route-service operation timed out"));
        return;
    }
    if (current == Opening) {
        if (ready && ready()) {
            if (afterOpen == Discovering) {
                set(Discovering, tr("Discovering device identities"));
                iso(255, 60928);
                deadline = n + 6000;
            } else {
                set(Identity, tr("Revalidating chartplotter NAME"));
                iso(target.source, 60928);
                deadline = n + 3000;
            }
        } else if (n >= deadline)
            fail(tr("Route-service transport not ready"));
        return;
    }
    if (ready && !ready()) {
        fail(tr("Route-service connection lost"));
        return;
    }
    if (current == Discovering) {
        if (n - lastSent >= 250) {
            for (const auto &device : known)
                if (device.model.isEmpty() &&
                    !productRequested.contains(device.source) &&
                    productRequested.size() < 16) {
                    productRequested.insert(device.source);
                    iso(device.source, 126996);
                    break;
                }
        }
        if (n >= deadline) {
            timer.stop();
            set(Idle, tr("Select a chartplotter and Refresh Routes"));
        }
        return;
    }
    if (pending.PGN == 126208 && !pendingSent && n - lastSent >= 250) {
        if (++requests > 64 || !send || !send(pending)) {
            fail(tr("Route-service send failed"));
            return;
        }
        lastSent = n;
        pendingSent = true;
        deadline = n + 5000;
    }
    if (n >= deadline) {
        if (current == Identity) {
            fail(tr("Chartplotter identity did not respond"));
            return;
        }
        if (retries++ == 0 && pending.PGN == 126208 && requests < 64) {
            ++requests;
            if (!send || !send(pending)) {
                fail(tr("Retry rejected"));
                return;
            }
            lastSent = n;
            deadline = n + 5000;
            text = tr("Retrying the same bounded request once");
            emit changed();
        } else
            fail(tr("Route-service response timeout; previous selection "
                    "preserved"));
    }
}
void ChartplotterRouteClient::ingest(const tN2kMsg &m) {
    if (m.DataLen < 0 || m.DataLen > 223)
        return;
    if (m.PGN == 60928 && m.DataLen == 8 && m.Source <= 251) {
        if (!known.contains(m.Source) && known.size() >= 64)
            return;
        QString name = QString::fromLatin1(bytes(m).toHex());
        auto old = known.value(m.Source);
        bool moved = false;
        for (auto it = known.begin(); it != known.end();)
            if (it.key() != m.Source && it->name == name) {
                it = known.erase(it);
                moved = true;
            } else
                ++it;
        if (old.name != name) old = {};
        old.source = m.Source;
        old.name = name;
        known[m.Source] = old;
        if ((busy() || current == Complete) && target.source >= 0 &&
            ((m.Source == target.source && name != target.name) ||
             (moved && name == target.name))) {
            fail(tr("Chartplotter NAME/address changed; refresh required"));
            return;
        }
        if (current == Identity && m.Source == target.source &&
            name == target.name) {
            pending.Clear();
            if (afterOpen == Attributes)
                request(130066, {{1, selected.database}, {2, selected.id}},
                        Attributes);
            else
                request(130064, {}, Databases);
        }
        emit changed();
        return;
    }
    if (m.PGN == 126996 && m.DataLen == 134 && known.contains(m.Source)) {
        unsigned short version, product;
        unsigned char certification, load;
        char model[33]{}, software[33]{}, modelVersion[33]{}, serial[33]{};
        if (!ParseN2kPGN126996(m, version, product, sizeof(model), model,
                              sizeof(software), software, sizeof(modelVersion), modelVersion,
                              sizeof(serial), serial, certification, load)) return;
        auto &device = known[m.Source];
        device.model = QString::fromLatin1(model).trimmed();
        device.software = QString::fromLatin1(software).trimmed();
        device.modelVersion = QString::fromLatin1(modelVersion).trimmed();
        device.productCode = product;
        emit changed();
        return;
    }
    if (!busy() || m.Source != target.source)
        return;
    try {
        // Addressed acknowledgments must refer to our outstanding request.
        // Broadcast route data remains source/NAME/DB/route/position checked
        // below.
        const int local = localAddress ? localAddress() : -1;
        if (m.PGN == 59392) {
            if (current == Identity && local >= 0 && m.Destination == local &&
                m.DataLen == 8 &&
                (m.Data[5] | (m.Data[6] << 8) | (m.Data[7] << 16)) == 60928 &&
                m.Data[0] != 0)
                fail(tr("Chartplotter rejected the identity request"));
            return;
        }
        if (!pendingSent || pending.PGN != 126208)
            return;
        if (m.PGN == 126208 && m.DataLen >= 6 && m.Data[0] == 2) {
            const int p = m.Data[1] | (m.Data[2] << 8) | (m.Data[3] << 16);
            const int wanted = pending.Data[1] | (pending.Data[2] << 8) |
                               (pending.Data[3] << 16);
            if (local >= 0 && m.Destination == local && p == wanted) {
                // PGN and interval error nibbles; parameter results are packed
                // four bits each after the parameter count (Appendix B.1).
                bool error = m.Data[4] != 0;
                const int count = m.Data[5];
                if (m.DataLen < 6 + (count + 1) / 2)
                    throw std::runtime_error(
                        "Truncated Group Function acknowledgment");
                for (int i = 0; i < count; ++i)
                    if (((m.Data[6 + i / 2] >> (4 * (i % 2))) & 15) != 0)
                        error = true;
                if (error)
                    fail(tr("Chartplotter rejected a route-service request or "
                            "selector"));
            }
            return;
        }
        Reader r{m};
        if (current == Databases && m.PGN == 130064) {
            int start = r.u(2), count = r.u(2), total = r.u(2);
            Q_UNUSED(start);
            if (databaseTotal >= 0 && databaseTotal != total)
                throw std::runtime_error("Database total changed during listing");
            databaseTotal = total;
            if (total > 16 || count > total)
                throw std::runtime_error("Database list bound exceeded");
            int last = databases.isEmpty() ? -1 : databases.last();
            for (int i = 0; i < count; ++i) {
                int id = r.u(2);
                r.str();
                r.u(4);
                r.u(2);
                r.u(1);
                r.u(2);
                r.u(4);
                r.u(4);
                if (id > 65532 || id <= last || databases.contains(id))
                    throw std::runtime_error("Conflicting database page");
                databases.append(id);
                last = id;
            }
            r.end();
            if (databases.size() < total) {
                if (!count)
                    throw std::runtime_error("No progress in database page");
                request(130064, {{1, last + 1}, {2, 4}}, Databases);
            } else if (databases.isEmpty()) {
                timer.stop();
                set(Idle, tr("No exposed route databases"));
            } else {
                databaseIndex = 0;
                next = 0;
                routeTotal = -1;
                request(130065, {{1, 0}, {2, 1}, {4, databases[0]}}, Routes);
            }
            return;
        }
        if (current == Routes && m.PGN == 130065) {
            r.u(2);
            int count = r.u(2), total = r.u(2), db = r.u(2);
            if (db != databases.value(databaseIndex))
                return;
            if (total > 128 || count > total || listed.size() + count > 128)
                throw std::runtime_error("Route list bound exceeded");
            int last = -1;
            for (int i = 0; i < count; ++i) {
                int id = r.u(2);
                QString name = r.str();
                r.u(1);
                if (id > 65532 || id < next || id <= last)
                    throw std::runtime_error("Invalid route page ordering");
                listed.append({db, id, -1, name});
                last = id;
            }
            r.end();
            if (routeTotal >= 0 && routeTotal != total)
                throw std::runtime_error("Route total changed during listing");
            routeTotal = total;
            int have = 0;
            for (auto e : listed)
                if (e.database == db)
                    ++have;
            if (have < total) {
                if (!count)
                    throw std::runtime_error("No progress in route list");
                next = last + 1;
                request(130065, {{1, next}, {2, 1}, {4, db}}, Routes);
            } else if (++databaseIndex < databases.size()) {
                next = 0;
                routeTotal = -1;
                request(130065, {{1, 0}, {2, 1}, {4, databases[databaseIndex]}},
                        Routes);
            } else {
                timer.stop();
                set(Idle,
                    tr("Route list complete; select a route to retrieve"));
            }
            return;
        }
        if ((current == Attributes || current == VerifyAttributes) &&
            m.PGN == 130066) {
            QStringList fields;
            if (!Expansion::decodeRouteAttributes(m, fields))
                throw std::runtime_error("Unsupported route attributes");
            auto f = map(fields);
            if (f["DatabaseID"].toInt() != selected.database ||
                f["RouteID"].toInt() != selected.id)
                return;
            if (current == VerifyAttributes) {
                if (bytes(m) != attributeBytes)
                    throw std::runtime_error("Route changed during retrieval");
                finish();
                return;
            }
            expected = f["WaypointCount"].toInt();
            if (expected < 2 || expected > 128 ||
                f["CriticalParameters"] != "0" ||
                f["NavigationMethod"] != "0" ||
                !QStringList{"0", "1"}.contains(f["IdentificationMethod"]) ||
                !QStringList{"0", "1"}.contains(f["RouteStatus"]))
                throw std::runtime_error(
                    "Unsupported route geometry or supplementary parameters");
            if (f["RouteName"] != selected.name)
                throw std::runtime_error("Route name changed");
            attributes = f;
            attributeBytes = bytes(m);
            next = 0;
            request(130067,
                    {{1, 0},
                     {2, qMin(4, expected)},
                     {4, selected.database},
                     {5, selected.id}},
                    Geometry);
            return;
        }
        if ((current == Geometry || current == VerifyGeometry) &&
            m.PGN == 130067) {
            QStringList fields;
            if (!Expansion::decodeRouteGeometry(m, fields))
                throw std::runtime_error("Invalid route geometry payload");
            auto f = map(fields);
            if (f["DatabaseID"].toInt() != selected.database ||
                f["RouteID"].toInt() != selected.id)
                return;
            if (f["TotalWaypoints"].toInt() != expected)
                throw std::runtime_error("Route waypoint total changed");
            int start = f["StartRPS"].toInt();
            auto array =
                QJsonDocument::fromJson(f["Waypoints"].toUtf8()).array();
            if (start != next || array.isEmpty() ||
                start + array.size() > expected)
                throw std::runtime_error("Missing/reordered route positions");
            auto &dest = current == Geometry ? positions : verification;
            for (int i = 0; i < array.size(); ++i)
                dest[start + i] = array[i].toObject();
            next += array.size();
            if (next < expected) {
                request(130067,
                        {{1, next},
                         {2, qMin(4, expected - next)},
                         {4, selected.database},
                         {5, selected.id}},
                        current);
                return;
            }
            if (current == Geometry) {
                next = 0;
                request(130067,
                        {{1, 0},
                         {2, qMin(4, expected)},
                         {4, selected.database},
                         {5, selected.id}},
                        VerifyGeometry);
            } else {
                if (positions != verification)
                    throw std::runtime_error(
                        "Route geometry changed during retrieval");
                request(130066, {{1, selected.database}, {2, selected.id}},
                        VerifyAttributes);
            }
            return;
        }
    } catch (const std::exception &e) {
        fail(QString::fromUtf8(e.what()));
    }
}
void ChartplotterRouteClient::finish() {
    QJsonArray points;
    for (int i = 0; i < expected; ++i) {
        if (!positions.contains(i)) {
            fail(tr("Route positions incomplete"));
            return;
        }
        points.append(positions[i]);
    }
    completed = ReceivedRoutes::validatedServiceSnapshot(
        QString::number(target.source), target.name, attributes, points);
    if (!completed.complete) {
        fail(completed.status);
        return;
    }
    timer.stop();
    pending.Clear();
    set(Complete, tr("Complete route: %1 — %2 ordered waypoints. Explicit "
                     "selection required.")
                      .arg(completed.name)
                      .arg(completed.waypoints.size()));
}
