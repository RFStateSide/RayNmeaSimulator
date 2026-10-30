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
#include "expansioninstruments.h"
class QComboBox;

// Narrow application codecs for the four M2.43D instruments. No transport or
// generator ownership lives here. Error/reserved remain distinct from NA/zero.
namespace NavigationStatistics {
constexpr double Error = -1000000001.0;
constexpr double Reserved = -1000000002.0;
const QList<Expansion::Definition> &definitions();
bool supported(const QString &code);
bool encode(const QString &code, const QMap<QString, QString> &fields, tN2kMsg &message);
bool decode(const tN2kMsg &message, QStringList &fields);
QString status(const Expansion::Field &field, const QString &value);
QString editValue(const Expansion::Field &field, const QString &text);
bool datumField(const QString &field);
QComboBox *datumEditor(QWidget *parent, const QString &value = {});
void setDatumEditor(QComboBox *editor, const QString &value);
QString datumValue(const QComboBox *editor);
// Static lifetime required by the pinned core's PGN-list APIs. Only the
// qualified 1851/4 variant is implemented for the proprietary PGN entry.
inline const unsigned long QualifiedPgns[] = {65359UL, 129044UL, 129542UL, 129547UL, 0};
}
