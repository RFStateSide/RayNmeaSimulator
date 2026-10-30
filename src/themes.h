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

#ifndef THEMES_H
#define THEMES_H
#include <QPalette>
#include <QStringList>

namespace Themes {
void initializeEditableResources();
QStringList externalDirectories();
// Approved built-ins are identical in every build; external custom themes follow.
QStringList builtInThemes();
QStringList available();
// Resolve display/Save choice without rewriting a stored preference on load.
QString selectionName(const QString& name);
bool isSystem(const QString& name);
void initializeSystemStyle();
void apply(const QString& name);
QString source(const QString& name);
QString styleSheet(const QString& name);
QPalette palette(const QString& name);
}
#endif
