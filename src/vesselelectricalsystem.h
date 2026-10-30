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

#ifndef VESSELELECTRICALSYSTEM_H
#define VESSELELECTRICALSYSTEM_H
#include "datastructs.h"
#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <functional>
class PgnWidgetBase;
class GeneratorEngine;
class PgnTableModel;
// Coordinates existing PGN models. Only transient phase/time and quantization
// remainder live here; voltage/current/capacity/SOC remain in those models.
class VesselElectricalSystem : public QObject {
    Q_OBJECT
  public:
    VesselElectricalSystem(PgnTableModel *, std::function<GeneratorEngine *(int)>, QObject *parent);
    void configure(const VesselElectricalDefaults &);
    void setGenerate(bool);
    void observe(PgnWidgetBase *);
    void synchronize(GeneratorEngine *event = nullptr);
    Q_INVOKABLE void advance(double seconds);

  private:
    struct Cycle {
        QPointer<GeneratorEngine> engine;
        bool running = false, active = false;
        double starter = 0, recovery = 0, remainder = 0;
    };
    PgnTableModel *model;
    std::function<GeneratorEngine *(int)> source;
    VesselElectricalDefaults defaults;
    QList<QPointer<PgnWidgetBase>> cards;
    QList<QPointer<GeneratorEngine>> engines;
    QMap<int, Cycle> cycles;
    QTimer timer;
    QElapsedTimer clock;
    qint64 lastNs = 0;
    bool generate = true, writing = false;
    int battery(PgnWidgetBase *) const;
    bool owned(PgnWidgetBase *) const;
    void release(PgnWidgetBase *);
    void tick();
    void schedule();
    void apply(int battery, Cycle &, double seconds, bool starter);
};
#endif
