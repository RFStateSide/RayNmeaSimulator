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

#ifndef ENGINESIMULATOR_H
#define ENGINESIMULATOR_H

#include "qobject.h"
class EngineSimulator : public QObject
{
    Q_OBJECT
public:
    explicit EngineSimulator(QObject *parent = nullptr);
    ~EngineSimulator();

    QStringList getEngineList(const QString &filePath);
    QVector<QPair<int, double>> getEngineRpmGphData(const QString &filePath, const QString &engineName);
    int getWOT(const std::vector<std::pair<int, double>> &rpmGphData);

signals:

private:
    std::vector<std::pair<int, double>> activeRpmGphData
        = {{600, 1.3}, {1000, 2.5}, {1500, 3.5}, {2000, 6.1}, {2500, 9.9}, {3000, 12.4}, {3500, 17.4}, {4000, 22.0}, {4600, 28.5}};
};

#endif // ENGINESIMULATOR_H
