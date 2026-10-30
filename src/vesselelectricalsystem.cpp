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

#include "vesselelectricalsystem.h"
#include "batterystatus127508.h"
#include "chargerstatus127507.h"
#include "dcstatus127506.h"
#include "generatorengine.h"
#include "pgntablemodel.h"
#include "pgnwidgetbase.h"
#include <QCheckBox>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
namespace {
double field(const PgnData *data, const QString &name) {
    if (data)
        for (const auto &row : data->paramDetails)
            if (row[0] == name) {
                bool ok = false;
                const double v = row[1].toDouble(&ok);
                return ok && std::isfinite(v) && v != N2kDoubleNA ? v : qQNaN();
            }
    return qQNaN();
}
} // namespace
VesselElectricalSystem::VesselElectricalSystem(PgnTableModel *m, std::function<GeneratorEngine *(int)> s,
                                               QObject *p)
    : QObject(p), model(m), source(std::move(s)) {
    timer.setParent(this);
    timer.setObjectName("electricalPollTimer");
    timer.setInterval(250);
    connect(&timer, &QTimer::timeout, this, &VesselElectricalSystem::tick);
    connect(model, &PgnTableModel::parameterChanged, this, [this](int id, const QString &, const QVariant &) {
        if (!writing && generate)
            for (auto card : cards)
                if (card && card->Id() == id) {
                    release(card);
                    break;
                }
    });
}
void VesselElectricalSystem::configure(const VesselElectricalDefaults &d) {
    if (defaults == d)
        return;
    timer.stop();
    clock.invalidate();
    cycles.clear();
    defaults = d;
}
void VesselElectricalSystem::setGenerate(bool value) {
    generate = value;
    timer.stop();
    clock.invalidate();
    cycles.clear();
}
int VesselElectricalSystem::battery(PgnWidgetBase *card) const {
    const auto *data = model->findPgn(card->Id());
    if (card->Pgn() == 127506 && field(data, "DCType") != N2kDCt_Battery)
        return -1;
    const auto value = field(data, card->Pgn() == 127506 ? "DCInstance" : "BatteryInstance");
    return std::isfinite(value) && value >= 0 && value < 8 ? int(value) : -1;
}
bool VesselElectricalSystem::owned(PgnWidgetBase *card) const {
    return card && card->property("ElectricalSimulation").toBool();
}
void VesselElectricalSystem::release(PgnWidgetBase *card) {
    if (writing || !generate)
        return;
    const int id = battery(card);
    for (auto c : cards)
        if (c && (c == card || battery(c) == id)) {
            c->setProperty("ElectricalSimulation", false);
            if (auto *check = c->findChild<QCheckBox *>("electricalSimulationToggle")) {
                QSignalBlocker block(check);
                check->setChecked(false);
            }
        }
    cycles.remove(id);
    schedule();
}
void VesselElectricalSystem::observe(PgnWidgetBase *card) {
    if (auto *engine = qobject_cast<GeneratorEngine *>(card)) {
        engines.append(engine);
        connect(engine, &GeneratorEngine::EngineRunningChanged, this, [this, engine] {
            tick();
            synchronize(engine);
        });
        connect(engine, &PgnWidgetBase::enabledChanged, this, [this, engine] {
            tick();
            synchronize(engine);
        });
        connect(engine, &PgnWidgetBase::dispatchChanged, this, [this] { synchronize(); });
        connect(engine, &GeneratorEngine::EngineInstanceChanged, this, [this] {
            tick();
            synchronize();
        });
        return;
    }
    if (card->Pgn() != 127508 && card->Pgn() != 127506 && card->Pgn() != 127507)
        return;
    cards.append(card);
    auto *check = new QCheckBox("Engine electrical simulation", card);
    check->setObjectName("electricalSimulationToggle");
    check->setToolTip("Uses the associated engine from Vessel Setup. Starts with the engine; manual edits "
                      "pause simulation for this battery. Re-enable participating cards explicitly. "
                      "Instrument Enable controls publication separately.");
    check->setChecked(owned(card));
    if (card->layout())
        card->layout()->addWidget(check);
    connect(check, &QCheckBox::toggled, this, [this, card](bool on) {
        tick();
        card->setProperty("ElectricalSimulation", on);
        synchronize();
    });
    auto manual = [this, card] { release(card); };
    if (auto *b = qobject_cast<BatteryStatus127508 *>(card)) {
        connect(b, &BatteryStatus127508::BatteryVoltageChanged, this, manual);
        connect(b, &BatteryStatus127508::BatteryCurrentChanged, this, manual);
        connect(b, &BatteryStatus127508::BatteryTemperatureChanged, this, manual);
        connect(b, &BatteryStatus127508::BatteryInstanceChanged, this, manual);
    } else if (auto *d = qobject_cast<DcStatus127506 *>(card)) {
        connect(d, &DcStatus127506::StateOfChargeChanged, this, manual);
        connect(d, &DcStatus127506::CapacityChanged, this, manual);
        connect(d, &DcStatus127506::DCTypeChanged, this, manual);
        connect(d, &DcStatus127506::DCInstanceChanged, this, manual);
        connect(d, &DcStatus127506::StateOfHealthChanged, this, manual);
        connect(d, &DcStatus127506::TimeRemainingChanged, this, manual);
        connect(d, &DcStatus127506::RippleVoltageChanged, this, manual);
    } else if (auto *c = qobject_cast<ChargerStatus127507 *>(card)) {
        connect(c, &ChargerStatus127507::ChargeStateChanged, this, manual);
        connect(c, &ChargerStatus127507::BatteryInstanceChanged, this, manual);
        connect(c, &ChargerStatus127507::EnabledChanged, this, manual);
        connect(c, &ChargerStatus127507::ChargerModeChanged, this, manual);
        connect(c, &ChargerStatus127507::EqualizationPendingChanged, this, manual);
        connect(c, &ChargerStatus127507::EqualizationTimeRemainingChanged, this, manual);
    }
    synchronize();
}
void VesselElectricalSystem::synchronize(GeneratorEngine *event) {
    if (!generate)
        return;
    for (int id = 0; id < defaults.batteryCount; ++id) {
        bool any = false;
        for (auto card : cards)
            if (owned(card) && battery(card) == id)
                any = true;
        if (!any) {
            cycles.remove(id);
            continue;
        }
        auto *engine = source(defaults.engineForBattery.value(id, -1));
        if (!std::any_of(engines.begin(), engines.end(),
                         [engine](const auto &live) { return live && live == engine; }))
            engine = nullptr;
        auto &cycle = cycles[id];
        if(engine && !engine->deliveryAllowed())engine=nullptr;
        const bool running = engine && engine->EngineRunning();
        if (event && event != engine && cycle.engine != event)
            continue;
        if (running && (!cycle.running || cycle.engine != engine)) {
            cycle.active = true;
            cycle.starter = event == engine ? 1.0 : 0.0;
            cycle.recovery = 0;
        } else if (!running && cycle.running) {
            cycle.active = true;
            cycle.starter = 0;
            cycle.recovery = 5;
        }
        cycle.engine = engine;
        cycle.running = running;
        if (cycle.active)
            apply(id, cycle, 0, cycle.starter > 0);
    }
    schedule();
}
void VesselElectricalSystem::schedule() {
    bool active = false;
    for (auto it = cycles.begin(); it != cycles.end();) {
        bool any = false;
        for (auto card : cards)
            if (owned(card) && battery(card) == it.key())
                any = true;
        if (!any) {
            it = cycles.erase(it);
            continue;
        }
        active |= it->active;
        ++it;
    }
    if (active && generate) {
        if (!timer.isActive()) {
            clock.start();
            lastNs = 0;
            timer.start();
        }
    } else {
        timer.stop();
        clock.invalidate();
    }
}
void VesselElectricalSystem::tick() {
    if (!generate || !clock.isValid())
        return;
    const auto now = clock.nsecsElapsed();
    const double seconds = (now - lastNs) / 1e9;
    lastNs = now;
    advance(seconds);
}
void VesselElectricalSystem::advance(double seconds) {
    if (!generate || !std::isfinite(seconds) || seconds < 0)
        return;
    for (auto it = cycles.begin(); it != cycles.end(); ++it) {
        auto &c = it.value();
        if (!c.active)
            continue;
        double remaining = seconds;
        if (c.starter > 0) {
            const double part = qMin(remaining, c.starter);
            apply(it.key(), c, part, true);
            c.starter -= part;
            remaining -= part;
        }
        if (c.starter <= 0) {
            if (c.running) {
                apply(it.key(), c, remaining, false);
            } else {
                const double part = qMin(remaining, c.recovery);
                c.recovery -= part;
                apply(it.key(), c, part, false);
                if (c.recovery <= 0)
                    c.active = false;
            }
        }
    }
    schedule();
}
void VesselElectricalSystem::apply(int id, Cycle &cycle, double seconds, bool starter) {
    QList<PgnWidgetBase *> participating;
    for (auto card : cards)
        if (owned(card) && battery(card) == id)
            participating.append(card);
    std::sort(participating.begin(), participating.end(), [](auto *a, auto *b) { return a->Id() < b->Id(); });
    const PgnData *dc = nullptr;
    const PgnData *batteryData = nullptr;
    for (auto *card : participating) {
        if (!dc && card->Pgn() == 127506)
            dc = model->findPgn(card->Id());
        if (!batteryData && card->Pgn() == 127508)
            batteryData = model->findPgn(card->Id());
    }
    const double capacity = field(dc, "Capacity"), storedSoc = field(dc, "StateOfCharge");
    const bool integrates = std::isfinite(capacity) && capacity > 0 && std::isfinite(storedSoc) &&
                            storedSoc >= 0 && storedSoc <= 100;
    double soc = integrates ? qBound(0., storedSoc + cycle.remainder, 100.) : defaults.initialSoc;
    const double resting =
        defaults.initialVoltage + defaults.nominalVoltage * .001 * (soc - defaults.initialSoc);
    double alternator = cycle.engine ? cycle.engine->AltenatorVoltage() : 0;
    if (!std::isfinite(alternator) || alternator == N2kDoubleNA || alternator <= 0)
        alternator = 0;
    // A profile on a different voltage system cannot charge this bank. Do not
    // invent a voltage converter or disagree with the engine PGN measurement.
    if (alternator < defaults.nominalVoltage * .8 || alternator > defaults.nominalVoltage * 1.3)
        alternator = 0;
    const bool charging = !starter && cycle.running && alternator > resting;
    double current = starter                 ? -180. * 12 / defaults.nominalVoltage
                     : charging && soc < 100 ? qBound(0., (alternator - defaults.initialVoltage) * 20., 40.)
                                             : 0;
    if (integrates) {
        soc = qBound(0., soc + current * seconds / capacity * 100., 100.);
        cycle.remainder = soc - qRound(soc);
    }
    double voltage = field(batteryData, "BatteryVoltage");
    if (!std::isfinite(voltage) || voltage <= 0)
        voltage = resting;
    const double target = starter ? defaults.initialVoltage * .76 : charging ? alternator : resting;
    voltage = starter ? target : target + (voltage - target) * std::exp(-seconds / 2.0);
    if (!cycle.running && cycle.recovery <= 0)
        voltage = resting;
    const double time = integrates && current < 0 ? soc * .01 * capacity / -current : N2kDoubleNA;
    QScopedValueRollback<bool> guard(writing, true);
    for (auto *card : participating) {
        QStringList changes;
        auto put = [&](const QString &name, double v) {
            if (std::isfinite(v))
                changes << name << QString::number(v, 'g', 17);
        };
        if (card->Pgn() == 127508) {
            put("BatteryVoltage", voltage);
            put("BatteryCurrent", current);
        }
        if (card->Pgn() == 127506) {
            if (integrates)
                put("StateOfCharge", qRound(soc));
            put("TimeRemaining", time);
        }
        if (card->Pgn() == 127507) {
            put("ChargeState", charging ? (soc >= 95 ? N2kCS_Float : N2kCS_Bulk) : N2kCS_Not_Charging);
            put("Enabled", charging ? N2kOnOff_On : N2kOnOff_Off);
        }
        model->changeParams(card->Id(), changes);
    }
}
