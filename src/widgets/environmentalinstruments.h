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
#include "pgnwidgetbase.h"
#include "N2kMsg.h"
#include <QMap>
class QFormLayout;
class QLineEdit;

// A view of the ordinary PGN parameter rows, not a second observation model.
class EnvironmentalInstrument : public PgnWidgetBase {
    Q_OBJECT
public:
    EnvironmentalInstrument(int id, const QString &label, int pgn, QWidget *parent);
    static QList<QList<QString>> parameters(int pgn);
    double value(const QString &field) const;
    void setValue(const QString &field, double value);
    void setInitialGuiValues() override;
signals:
    void parameterValueChanged(const QString &field, double value);
protected:
    void initializeUnitsLimits() override;
private:
    QFormLayout *form = nullptr;
    QMap<QString, double> values;
    QMap<QString, QLineEdit*> editors;
    void refresh();
};

class WindData130306 final : public EnvironmentalInstrument {
    Q_OBJECT
    Q_PROPERTY(int SID READ SID WRITE setSID)
    Q_PROPERTY(double WindSpeed READ WindSpeed WRITE setWindSpeed)
    Q_PROPERTY(double WindAngle READ WindAngle WRITE setWindAngle)
    Q_PROPERTY(int WindReference READ WindReference WRITE setWindReference)
public:
    WindData130306(int id, const QString &label, QWidget *parent = nullptr)
        : EnvironmentalInstrument(id, label, 130306, parent) {}
    int SID() const { return value("SID"); }
    void setSID(int v) { setValue("SID", v); }
    double WindSpeed() const { return value("WindSpeed"); }
    void setWindSpeed(double v) { setValue("WindSpeed", v); }
    double WindAngle() const { return value("WindAngle"); }
    void setWindAngle(double v) { setValue("WindAngle", v); }
    int WindReference() const { return value("WindReference"); }
    void setWindReference(int v) { setValue("WindReference", v); }
};

class Humidity130313 final : public EnvironmentalInstrument {
    Q_OBJECT
    Q_PROPERTY(int SID READ SID WRITE setSID)
    Q_PROPERTY(int HumidityInstance READ HumidityInstance WRITE setHumidityInstance)
    Q_PROPERTY(int HumiditySource READ HumiditySource WRITE setHumiditySource)
    Q_PROPERTY(double ActualHumidity READ ActualHumidity WRITE setActualHumidity)
    Q_PROPERTY(double SetHumidity READ SetHumidity WRITE setSetHumidity)
public:
    Humidity130313(int id, const QString &label, QWidget *parent = nullptr)
        : EnvironmentalInstrument(id, label, 130313, parent) {}
    int SID() const { return value("SID"); }
    void setSID(int v) { setValue("SID", v); }
    int HumidityInstance() const { return value("HumidityInstance"); }
    void setHumidityInstance(int v) { setValue("HumidityInstance", v); }
    int HumiditySource() const { return value("HumiditySource"); }
    void setHumiditySource(int v) { setValue("HumiditySource", v); }
    double ActualHumidity() const { return value("ActualHumidity"); }
    void setActualHumidity(double v) { setValue("ActualHumidity", v); }
    double SetHumidity() const { return value("SetHumidity"); }
    void setSetHumidity(double v) { setValue("SetHumidity", v); }
};
