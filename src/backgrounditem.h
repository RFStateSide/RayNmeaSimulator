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

#ifndef BACKGROUNDITEM_H
#define BACKGROUNDITEM_H

#include <QColor>
#include <QVector>
#include <QPair>

class BackgroundItem {
public:
    explicit BackgroundItem(int radius);
    void clearColors();
    void addColor(qreal position, const QColor &color);
    int radius() const;
    const QVector<QPair<qreal, QColor>>& colors() const;

private:
    int m_radius;
    QVector<QPair<qreal, QColor>> m_colors;
};

#endif // BACKGROUNDITEM_H
