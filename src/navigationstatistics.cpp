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

#include "navigationstatistics.h"
#include <cmath>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalBlocker>

namespace NavigationStatistics {
bool datumField(const QString &field) { return field == "LocalDatum" || field == "ReferenceDatum"; }
QComboBox *datumEditor(QWidget *parent, const QString &value)
{
    auto *combo = new QComboBox(parent);
    combo->setProperty("datumEditor", true);
    combo->setEditable(true); combo->setInsertPolicy(QComboBox::NoInsert);
    // CANboat/NMEA reference specifies IHO S-60 codes, not a closed enum.
    // Common named codes corroborated by Trimble's published DTM reference.
    for (const auto &item : QList<QPair<QString, QString>>{{"W84", "WGS 84"}, {"W72", "WGS 72"},
             {"S85", "SGS 85"}, {"P90", "PE 90"}, {"999", "User defined"}, {"", "Unavailable"}})
        combo->addItem(item.second + (item.first.isEmpty() ? QString() : " (" + item.first + ")"), item.first);
    combo->setToolTip("Choose a named datum or type an exact 3/4-character code, including an unlisted IHO code or subdivision. Empty means unavailable. Canonical codes and offsets are preserved; no coordinate transformation.");
    combo->lineEdit()->setToolTip(combo->toolTip());
    combo->setMinimumContentsLength(8); combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setDatumEditor(combo, value);
    return combo;
}
void setDatumEditor(QComboBox *combo, const QString &value)
{
    const QSignalBlocker block(combo);
    const QSignalBlocker textBlock(combo->lineEdit());
    int index = combo->findData(value);
    if (index < 0) {
        // Keep one unlisted-code slot; live datum changes are not a history.
        const auto custom = combo->property("datumCustomIndex");
        if (custom.isValid()) {
            index = custom.toInt(); combo->setItemText(index, value); combo->setItemData(index, value);
        } else {
            combo->addItem(value, value); index = combo->count() - 1; combo->setProperty("datumCustomIndex", index);
        }
    }
    combo->setCurrentIndex(index); combo->lineEdit()->setModified(false);
}
QString datumValue(const QComboBox *combo)
{
    if (combo->currentIndex() >= 0 && combo->currentText() == combo->itemText(combo->currentIndex()))
        return combo->currentData().toString();
    return combo->currentText().compare("Unavailable", Qt::CaseInsensitive) == 0 ? QString() : combo->currentText();
}
const QList<Expansion::Definition> &definitions()
{
    static const QList<Expansion::Definition> rows = [] {
        const auto na = QString::number(N2kDoubleNA, 'g', 17);
        QList<Expansion::Definition> result{
            {"129044", "Datum129044", "Datum", "Navigation",
             {{"LocalDatum", "", "QString", "W84", 0, 0},
              {"DeltaLatitude", "deg", "double", "0", -90, 90},
              {"DeltaLongitude", "deg", "double", "0", -180, 180},
              {"DeltaAltitude", "m", "double", "0", -21474836.48, 21474836.44},
              {"ReferenceDatum", "", "QString", "W84", 0, 0}}, {}, 129044}};
        // 6.2832 is the nearest 0.0001-rad wire value to a complete turn.
        for (const auto &code : {QString("129542"), QString("129547")}) {
            const bool noise = code == "129542";
            result.append({code, noise ? "GnssNoise129542" : "GnssError129547",
                           noise ? "GNSS Pseudorange Noise Statistics" : "GNSS Pseudorange Error Statistics",
                           "Navigation",
                           {{"SID", "", "unsigned char", "255", 0, 255},
                            {noise ? "RMSPositionUncertainty" : "RMSRangeInputDeviation", "m", "double", na, 0, 655.32},
                            {noise ? "MajorAxisDeviation" : "MajorErrorEllipseDeviation", "m", "double", na, 0, 655.32},
                            {noise ? "MinorAxisDeviation" : "MinorErrorEllipseDeviation", "m", "double", na, 0, 655.32},
                            {noise ? "MajorAxisOrientation" : "ErrorEllipseOrientation", "rad", "double", na, 0, 6.2832},
                            {"LatitudeDeviation", "m", "double", na, 0, 655.32},
                            {"LongitudeDeviation", "m", "double", na, 0, 655.32},
                            {"AltitudeDeviation", "m", "double", na, 0, 655.32}}, {}, code.toInt()});
        }
        result.append({"65359", "PilotHeading65359", "Raymarine SeaTalk Pilot Heading", "Navigation",
                       {{"ManufacturerCode", "", "uint16_t", "1851", 1851, 1851},
                        {"HeaderReserved", "", "unsigned char", "3", 0, 3},
                        {"IndustryCode", "", "unsigned char", "4", 4, 4},
                        {"SID", "", "unsigned char", "255", 0, 255},
                        {"TrueHeading", "rad", "double", na, 0, 6.2832},
                        {"MagneticHeading", "rad", "double", na, 0, 6.2832},
                        {"ReservedByte", "", "unsigned char", "255", 0, 255}}, {}, 65359});
        return result;
    }();
    return rows;
}
bool supported(const QString &code)
{
    return code == "129044" || code == "129542" || code == "129547" || code == "65359";
}
QString status(const Expansion::Field &f, const QString &value)
{
    const double v = value.toDouble();
    if (f.type == "double") {
        if (v == N2kDoubleNA) return "Unavailable";
        if (v == Error) return "Error";
        if (v == Reserved) return "Reserved";
    }
    if (f.name == "SID") {
        if (v == 255) return "Unavailable";
        if (v == 254) return "Error";
        if (v == 253) return "Reserved";
    }
    return {};
}
QString editValue(const Expansion::Field &f, const QString &text)
{
    if ((f.name == "LocalDatum" || f.name == "ReferenceDatum") && text.compare("Unavailable", Qt::CaseInsensitive) == 0) return {};
    if (f.type != "double" && f.name != "SID") return text;
    if (text.compare("Unavailable", Qt::CaseInsensitive) == 0)
        return f.name == "SID" ? "255" : QString::number(N2kDoubleNA, 'g', 17);
    if (text.compare("Error", Qt::CaseInsensitive) == 0)
        return f.name == "SID" ? "254" : QString::number(Error, 'g', 17);
    if (text.compare("Reserved", Qt::CaseInsensitive) == 0)
        return f.name == "SID" ? "253" : QString::number(Reserved, 'g', 17);
    return text;
}
namespace {
QString number(double v) { return QString::number(v, 'g', 17); }
quint32 raw(double v, double scale, quint32 na)
{
    if (v == N2kDoubleNA) return na;
    if (v == Error) return na - 1;
    if (v == Reserved) return na - 2;
    return quint32(qint64(std::llround(v / scale)));
}
double measurement(quint32 v, double scale, quint32 na, bool sign = false)
{
    if (v == na) return N2kDoubleNA;
    if (v == na - 1) return Error;
    if (v == na - 2) return Reserved;
    return (sign ? double(qint32(v)) : double(v)) * scale;
}
bool datum(const tN2kMsg &m, int at, QString &text)
{
    QByteArray bytes(reinterpret_cast<const char *>(m.Data + at), 4);
    // A fixed datum is three IHO characters plus optional subdivision; preserve
    // its fourth character. Empty/all-FF is explicitly unavailable.
    if (bytes == QByteArray(4, char(0xff))) { text.clear(); return true; }
    const int end = bytes.indexOf('\0');
    if (end >= 0) {
        for (int i = end; i < 4; ++i) if (bytes[i] != '\0') return false;
        bytes.truncate(end);
    }
    text = QString::fromLatin1(bytes);
    return Expansion::validField("129044", "LocalDatum", text);
}
}
bool encode(const QString &code, const QMap<QString, QString> &f, tN2kMsg &m)
{
    if (!supported(code)) return false;
    const auto *d = Expansion::definition(code);
    m.SetPGN(code.toULong()); m.Priority = code == "65359" ? 7 : 6;
    if (code == "129044") {
        auto put = [&](const QString &text) {
            QByteArray b = text.toLatin1();
            if (b.isEmpty()) b = QByteArray(4, char(0xff));
            else b = b.leftJustified(4, '\0');
            for (char c : b) m.AddByte(quint8(c));
        };
        put(f["LocalDatum"]);
        for (int i = 1; i <= 3; ++i)
            m.Add4ByteUInt(raw(f[d->fields[i].name].toDouble(), i == 3 ? .01 : 1e-7, 0x7fffffff));
        put(f["ReferenceDatum"]);
    } else if (code == "65359") {
        m.Add2ByteUInt(1851 | (f["HeaderReserved"].toUInt() << 11) | (4 << 13));
        m.AddByte(f["SID"].toUInt());
        for (const auto &key : {"TrueHeading", "MagneticHeading"})
            m.Add2ByteUInt(raw(f[key].toDouble(), .0001, 0xffff));
        m.AddByte(f["ReservedByte"].toUInt());
    } else {
        m.AddByte(f["SID"].toUInt());
        for (int i = 1; i < d->fields.size(); ++i)
            m.Add2ByteUInt(raw(f[d->fields[i].name].toDouble(), i == 4 ? .0001 : .01, 0xffff));
    }
    return true;
}
bool decode(const tN2kMsg &m, QStringList &fields)
{
    const QString code = QString::number(m.PGN);
    if (!supported(code) || m.DataLen != (code == "129044" ? 20 : code == "65359" ? 8 : 15)) return false;
    const auto *d = Expansion::definition(code);
    QMap<QString, QString> f;
    int index = 0;
    if (code == "129044") {
        if (!datum(m, 0, f["LocalDatum"]) || !datum(m, 16, f["ReferenceDatum"])) return false;
        index = 4;
        for (int i = 1; i <= 3; ++i)
            f[d->fields[i].name] = number(measurement(m.Get4ByteUInt(index), i == 3 ? .01 : 1e-7, 0x7fffffff, true));
    } else if (code == "65359") {
        const auto header = m.Get2ByteUInt(index);
        if ((header & 2047) != 1851 || (header >> 13) != 4) return false;
        f["ManufacturerCode"] = "1851";
        f["HeaderReserved"] = QString::number((header >> 11) & 3);
        f["IndustryCode"] = "4";
        f["SID"] = QString::number(m.GetByte(index));
        for (const auto &key : {"TrueHeading", "MagneticHeading"})
            f[key] = number(measurement(m.Get2ByteUInt(index), .0001, 0xffff));
        f["ReservedByte"] = QString::number(m.GetByte(index));
    } else {
        f["SID"] = QString::number(m.GetByte(index));
        for (int i = 1; i < d->fields.size(); ++i)
            f[d->fields[i].name] = number(measurement(m.Get2ByteUInt(index), i == 4 ? .0001 : .01, 0xffff));
    }
    // Published fields are unsigned angles / bounded signed coordinates. Some
    // RS150 samples exceed the angle domain; do not guess signed semantics.
    // Preserve full original bytes in Diagnostics and expose Error only for the
    // affected measurement, retaining the other usable statistics in this sample.
    for (const auto &field : d->fields) {
        if (field.type != "double") continue;
        const double v = f[field.name].toDouble();
        if (v != N2kDoubleNA && v != Error && v != Reserved && (v < field.minimum || v > field.maximum))
            f[field.name] = number(Error);
    }
    QStringList result;
    for (auto it = f.cbegin(); it != f.cend(); ++it) result << it.key() << it.value();
    if (!Expansion::validParameters(code, result)) return false;
    fields = result;
    return true;
}
}
