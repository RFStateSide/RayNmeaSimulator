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
class QLineEdit;
class QLabel;

// Presentation adapter for the ordinary authoritative PGN parameter rows.
// No timer, generator or control/actuation path belongs to these status views.
class WindlassStatus : public PgnWidgetBase {
    Q_OBJECT
    Q_PROPERTY(int SID READ SID WRITE setSID)
    Q_PROPERTY(int WindlassIdentifier READ WindlassIdentifier WRITE setWindlassIdentifier)
    Q_PROPERTY(double RodeCounterValue READ RodeCounterValue WRITE setRodeCounterValue)
    Q_PROPERTY(double WindlassLineSpeed READ WindlassLineSpeed WRITE setWindlassLineSpeed)
    Q_PROPERTY(int WindlassMotionStatus READ WindlassMotionStatus WRITE setWindlassMotionStatus)
    Q_PROPERTY(int RodeTypeStatus READ RodeTypeStatus WRITE setRodeTypeStatus)
    Q_PROPERTY(int AnchorDockingStatus READ AnchorDockingStatus WRITE setAnchorDockingStatus)
    Q_PROPERTY(int OperatingEvents READ OperatingEvents WRITE setOperatingEvents)
    Q_PROPERTY(double TotalMotorTime READ TotalMotorTime WRITE setTotalMotorTime)
    Q_PROPERTY(double ControllerVoltage READ ControllerVoltage WRITE setControllerVoltage)
    Q_PROPERTY(double MotorCurrent READ MotorCurrent WRITE setMotorCurrent)
    Q_PROPERTY(int MonitoringEvents READ MonitoringEvents WRITE setMonitoringEvents)
public:
    WindlassStatus(int id, const QString &label, int pgn, QWidget *parent = nullptr);
    static bool supported(int pgn) { return pgn==128777 || pgn==128778; }
    static QList<QList<QString>> parameters(int pgn);
    static bool measurement(const QString &field);
    static bool validField(int pgn, const QString &field, const QString &text);
    static QList<QPair<int,QString>> choices(const QString &field);
    static QString description(const QString &field);
    static QString displayUnit(const QString &field, const UserUnitPrefs &prefs);
    static QString displayValue(const QString &field, const QString &text, const UserUnitPrefs &prefs);
    static bool encode(int pgn, const QMap<QString,QString> &fields, tN2kMsg &message);
    static bool decode(const tN2kMsg &message, QStringList &fields);
    static bool validParameters(int pgn, const QStringList &fields);
    QString value(const QString &field) const { return values.value(field); }
    void setValue(const QString &field, const QString &text);
    void showInvalidEdit(const QString &field);
    void setInitialGuiValues() override;
    int SID() const { return value("SID").toInt(); }
    void setSID(int v) { setValue("SID", QString::number(v)); }
    int WindlassIdentifier() const { return value("WindlassIdentifier").toInt(); }
    void setWindlassIdentifier(int v) { setValue("WindlassIdentifier", QString::number(v)); }
    double RodeCounterValue() const { return value("RodeCounterValue").toDouble(); }
    void setRodeCounterValue(double v) { setValue("RodeCounterValue", QString::number(v, 'g', 17)); }
    double WindlassLineSpeed() const { return value("WindlassLineSpeed").toDouble(); }
    void setWindlassLineSpeed(double v) { setValue("WindlassLineSpeed", QString::number(v, 'g', 17)); }
    int WindlassMotionStatus() const { return value("WindlassMotionStatus").toInt(); }
    void setWindlassMotionStatus(int v) { setValue("WindlassMotionStatus", QString::number(v)); }
    int RodeTypeStatus() const { return value("RodeTypeStatus").toInt(); }
    void setRodeTypeStatus(int v) { setValue("RodeTypeStatus", QString::number(v)); }
    int AnchorDockingStatus() const { return value("AnchorDockingStatus").toInt(); }
    void setAnchorDockingStatus(int v) { setValue("AnchorDockingStatus", QString::number(v)); }
    int OperatingEvents() const { return value("OperatingEvents").toInt(); }
    void setOperatingEvents(int v) { setValue("OperatingEvents", QString::number(v)); }
    double TotalMotorTime() const { return value("TotalMotorTime").toDouble(); }
    void setTotalMotorTime(double v) { setValue("TotalMotorTime", QString::number(v, 'g', 17)); }
    double ControllerVoltage() const { return value("ControllerVoltage").toDouble(); }
    void setControllerVoltage(double v) { setValue("ControllerVoltage", QString::number(v, 'g', 17)); }
    double MotorCurrent() const { return value("MotorCurrent").toDouble(); }
    void setMotorCurrent(double v) { setValue("MotorCurrent", QString::number(v, 'g', 17)); }
    int MonitoringEvents() const { return value("MonitoringEvents").toInt(); }
    void setMonitoringEvents(int v) { setValue("MonitoringEvents", QString::number(v)); }
signals:
    void parameterValueChanged(const QString &field, const QString &value);
protected:
    void initializeUnitsLimits() override;
private:
    QMap<QString,QString> values;
    QMap<QString,QLineEdit*> editors;
    void refresh();
};
class WindlassOperating128777 final : public WindlassStatus {
    Q_OBJECT
public:
    WindlassOperating128777(int id,const QString &label,QWidget *parent=nullptr)
        : WindlassStatus(id,label,128777,parent) {}
};
class WindlassMonitoring128778 final : public WindlassStatus {
    Q_OBJECT
public:
    WindlassMonitoring128778(int id,const QString &label,QWidget *parent=nullptr)
        : WindlassStatus(id,label,128778,parent) {}
};
