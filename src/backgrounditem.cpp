/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
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

#include "backgrounditem.h"

BackgroundItem::BackgroundItem(int radius)
    : m_radius(radius) {}

void BackgroundItem::clearColors() {
    m_colors.clear();
}

void BackgroundItem::addColor(qreal position, const QColor &color) {
    m_colors.append(qMakePair(position, color));
}

int BackgroundItem::radius() const {
    return m_radius;
}

const QVector<QPair<qreal, QColor>>& BackgroundItem::colors() const {
    return m_colors;
}
