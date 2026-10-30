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

#include "proprietarydiagnostics.h"
#include "convert.h"

namespace {
QString hex(quint64 n, int width = 0)
{
    return "0x" + QString::number(n, 16).rightJustified(width, '0');
}
struct Decoder {
    Nmea2000Diagnostics::Message &r;
    const QByteArray &b;
    QVector<bool> covered;
    explicit Decoder(Nmea2000Diagnostics::Message &row) : r(row), b(row.payload), covered(b.size(), false)
    {
        covered[0] = covered[1] = true; // Common manufacturer/industry fields already exposed.
    }
    quint64 number(int at, int width) const
    {
        Q_ASSERT(at >= 0 && at + width <= b.size() && width <= 8);
        quint64 value = 0;
        for (int i = 0; i < width; ++i) value |= quint64(quint8(b[at + i])) << (i * 8);
        return value;
    }
    void field(const QString &name, const QString &value, const QString &meaning)
    {
        r.fields.append({name, value, meaning});
    }
    QString span(int at, int width) const
    {
        return QString("Byte offset %1, width %2 bits").arg(at).arg(width * 8);
    }
    void mark(int at, int width) { for (int i = at; i < at + width; ++i) covered[i] = true; }
    void raw(int at, int width, const QString &name, const QString &meaning = "Undefined; no units or value semantics assigned")
    {
        field(name, QString::fromLatin1(b.mid(at, width).toHex(' ')), span(at, width) + "; " + meaning);
        mark(at, width);
    }
    QString scalar(int at, int width, const QString &table = {}) const
    {
        const auto value = number(at, width), na = (quint64(1) << (width * 8)) - 1;
        QString name = value == na ? "Unavailable" : value == na - 1 ? "Error" : value == na - 2 ? "Reserved"
                                : table.isEmpty() ? QString() : NmeaLookup::enumeration(table, int(value));
        if (!table.isEmpty() && name.isEmpty()) name = "Undefined";
        return QString::number(value) + " (" + hex(value, width * 2) + ")" + (name.isEmpty() ? QString() : " - " + name);
    }
    void value(int at, int width, const QString &name, const QString &table = {})
    {
        field(name, scalar(at, width, table), span(at, width) + "; published field; observed external state only");
        mark(at, width);
    }
    void qualified(const QString &function, const QString &status)
    {
        r.function = function; r.status = status; r.qualified = true;
    }
    void finish()
    {
        for (int at = 2; at < b.size();) {
            if (covered[at]) { ++at; continue; }
            const int start = at;
            while (at < b.size() && !covered[at]) ++at;
            raw(start, at - start, QString("Unknown bytes %1..%2").arg(start).arg(at - 1));
        }
        field("Raw Data (entire payload)", QString::fromLatin1(b.toHex(' ')),
              "Original logical-payload wire order, including header/reserved/undefined bytes; never transmitted here");
    }
};
}
namespace ProprietaryDiagnostics {
bool interpret(Nmea2000Diagnostics::Message &r)
{
    if (r.pgn != 65379 && r.pgn != 126720 && r.pgn != 130820 && r.pgn != 65311) return false;
    if (r.payload.size() < 2) return false; // Common interpreter handles malformed headers.
    Decoder d(r);
    const int manufacturer = int(d.number(0, 2) & 2047), industry = int(d.number(0, 2) >> 13);
    const bool raymarine = manufacturer == 1851 && industry == 4;
    const bool fusion = manufacturer == 419 && industry == 4;
    r.variant = QString("m%1/i%2/len%3").arg(manufacturer).arg(industry).arg(d.b.size());
    r.status = "Unqualified proprietary variant; undefined body retained raw";
    if (r.pgn == 65379 && raymarine) {
        r.function = "Raymarine SeaTalk Pilot Mode (external observation)";
        if (d.b.size() == 8) {
            d.qualified(r.function, "Qualified diagnostic subset; pilot status observation, TX/control unqualified");
            d.value(2, 2, "External pilot mode", "SEATALK_PILOT_MODE_16");
            d.raw(4, 2, "Submode raw");
            d.raw(6, 1, "Mode data raw");
            d.raw(7, 1, "Reserved byte 7", "Reserved; original value preserved");
            d.field("Capability boundary", "Receive diagnostics only", "Does not operate local VesselMovement Autopilot or infer a remote command");
        } else r.status = "Unverified SeaTalk Pilot Mode length; raw only (reference length 8)";
    } else if (r.pgn == 126720) {
        r.variant += "/dst" + QString::number(r.destination);
        r.variant += "/prefix" + QString::fromLatin1(d.b.mid(2, 3).toHex());
        if (raymarine && d.b.size() >= 3) {
            d.raw(2, 1, "Proprietary identifier", "Raymarine selector byte; raw numeric identifier");
            if (d.b.size() >= 4) d.raw(3, 1, "Command selector", "Raymarine selector, not an authorization to issue commands");
            if (d.b.size() >= 5 && d.number(2, 1) == 0xf0 && d.number(3, 1) == 0x81) {
                const int sub = int(d.number(4, 1));
                d.raw(4, 1, "SeaTalk1 subcommand selector", "Documented selector; does not establish remote-control semantics");
                if (sub == 0x84) {
                    r.function = "Raymarine SeaTalk1 Pilot Mode (external observation)";
                    if (d.b.size() == 13 || d.b.size() == 21) {
                        const bool shortLayout = d.b.size() == 13;
                        d.qualified(r.function, shortLayout
                            ? "Qualified observed 13-byte prefix subset; reference is 21 bytes, tail undefined; RX diagnostics only"
                            : "Qualified reference 21-byte field subset; trailing body undefined; RX diagnostics only");
                        d.raw(5, 3, "Unknown bytes 5..7");
                        // This is the SeaTalk1 8-bit mode table, NOT 65379's 16-bit table.
                        d.value(8, 1, "External pilot mode", "SEATALK_PILOT_MODE");
                        d.value(9, 1, "Submode value");
                        d.raw(10, 1, "Mode data raw");
                    } else r.status = "Unverified SeaTalk1 pilot-mode layout length; raw only (observed 13 / reference 21)";
                } else if (sub == 0x90) {
                    r.function = "Raymarine SeaTalk1 Device Identification";
                    if (d.b.size() == 7) {
                        d.qualified(r.function, "Qualified diagnostic field subset; observed 7-byte layout; RX only");
                        d.raw(5, 1, "Reserved byte 5", "Reserved; preserved raw");
                        d.value(6, 1, "SeaTalk device code", "SEATALK_DEVICE_ID");
                    } else r.status = "Unverified SeaTalk1 identification length; raw only (reference length 7)";
                }
            }
        }
    } else if (r.pgn == 130820) {
        if (d.b.size() >= 4) {
            const int id = int(d.number(2, 2));
            r.variant += (fusion ? "/id" : "/raw-prefix") + hex(id, 4);
            if (fusion) {
                d.raw(2, 2, "Fusion message ID", "Little-endian 16-bit selector " + hex(id, 4));
                int length = id == 0x8020 || id == 0x8017 ? 5 : id == 0x801d || id == 0x8009 ? 8 : 0;
                if (id == 0x8020) r.function = "Fusion Power State";
                if (id == 0x8017) r.function = "Fusion Mute Status";
                if (id == 0x801d) r.function = "Fusion Zone Volumes";
                if (id == 0x8009) r.function = "Fusion Track Progress";
                if (length && d.b.size() == length) {
                    d.qualified(r.function, id == 0x8020
                        ? "Qualified observed power-state diagnostic field; RX only, TX/control unqualified"
                        : "Qualified reference-defined diagnostic fields; fixture-tested, not vessel-observed; RX only");
                    if (id == 0x8020) d.value(4, 1, "Power state", "FUSION_POWER_STATE");
                    if (id == 0x8017) d.value(4, 1, "Mute value", "FUSION_MUTE_COMMAND");
                    if (id == 0x801d) for (int zone = 0; zone < 4; ++zone)
                        d.value(4 + zone, 1, QString("Zone %1 volume value").arg(zone + 1)); // No published percent unit.
                    if (id == 0x8009) {
                        d.value(4, 1, "Audio source ID");
                        r.variant += "/source" + QString::number(d.number(4, 1));
                        const auto value = d.number(5, 3);
                        d.field("Track progress", value >= 0xfffffd ? d.scalar(5, 3)
                            : QString::number(value * .001, 'g', 12) + " s (raw " + QString::number(value) + ")",
                            d.span(5, 3) + "; published 0.001-second duration"); d.mark(5, 3);
                    }
                } else if (length) r.status = "Unverified Fusion layout length; raw only (reference " + QString::number(length) + ")";
                else {
                    if (id == 0x4009 || id == 0x4010 || r.function.contains("undefined variant"))
                        r.function = "Fusion undefined message " + hex(id, 4);
                    r.status = id == 0x4009 || id == 0x4010
                        ? "Observed Fusion selector; semantics undefined; not an 0x8000-family mapping"
                        : "Unqualified Fusion selector; undefined data retained raw";
                }
            }
        } else r.status = "Incomplete proprietary message-ID prefix; raw only";
    } else if (r.pgn == 65311 && raymarine) {
        r.function = "Raymarine Magnetic Variation candidate";
        r.status = "Candidate only: purpose corroborated, full map/scaling/sentinels undefined; no TX";
        if (d.b.size() == 8) {
            const auto value = quint16(d.number(3, 2));
            const int signedValue = value < 0x8000 ? int(value) : int(value) - 65536;
            d.raw(3, 2, "Candidate variation raw bytes", "Candidate signed16 field location; interpretation is unverified");
            d.field("Candidate signed16 value", QString::number(signedValue), d.span(3, 2) + "; two's-complement hypothesis only");
            QString shown;
            if (value >= 0x7ffd && value <= 0x7fff)
                shown = "Possible " + QString(value == 0x7fff ? "Unavailable" : value == 0x7ffe ? "Error" : "Reserved") + " under standard signed16 convention; NOT verified";
            else {
                const double radians = signedValue * .0001;
                shown = QString::number(radians, 'g', 12) + " rad / " + QString::number(Convert::RadiansToDegrees(radians), 'g', 12) + " deg (unverified hypothesis)";
            }
            d.field("Candidate variation hypothesis", shown,
                    "Assumed 0.0001 rad, not a validated measurement; one repeated captured value cannot qualify signedness/scaling/sentinels");
        } else r.status = "Raymarine variation candidate with unverified length; raw only (observed length 8)";
    }
    d.finish();
    return true;
}
}
