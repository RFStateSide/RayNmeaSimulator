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
class QLineEdit; class QLabel;
class AisClassBPosition129039 final : public PgnWidgetBase {
    Q_OBJECT
    Q_PROPERTY(int MessageID READ MessageID WRITE setMessageID)
    Q_PROPERTY(int Repeat READ Repeat WRITE setRepeat)
    Q_PROPERTY(quint32 UserID READ UserID WRITE setUserID)
    Q_PROPERTY(double Latitude READ Latitude WRITE setLatitude)
    Q_PROPERTY(double Longitude READ Longitude WRITE setLongitude)
    Q_PROPERTY(bool Accuracy READ Accuracy WRITE setAccuracy)
    Q_PROPERTY(bool RAIM READ RAIM WRITE setRAIM)
    Q_PROPERTY(int Seconds READ Seconds WRITE setSeconds)
    Q_PROPERTY(double COG READ COG WRITE setCOG)
    Q_PROPERTY(double SOG READ SOG WRITE setSOG)
    Q_PROPERTY(int AISInfo READ AISInfo WRITE setAISInfo)
    Q_PROPERTY(double Heading READ Heading WRITE setHeading)
    Q_PROPERTY(int Unit READ Unit WRITE setUnit)
    Q_PROPERTY(bool Display READ Display WRITE setDisplay)
    Q_PROPERTY(bool DSC READ DSC WRITE setDSC)
    Q_PROPERTY(bool Band READ Band WRITE setBand)
    Q_PROPERTY(bool Msg22 READ Msg22 WRITE setMsg22)
    Q_PROPERTY(int Mode READ Mode WRITE setMode)
    Q_PROPERTY(bool State READ State WRITE setState)
    Q_PROPERTY(int SID READ SID WRITE setSID)
public:
    AisClassBPosition129039(int id, const QString &label, QWidget *parent = nullptr);
    static QList<QList<QString>> parameters();
    static bool validField(const QString &field, const QString &text);
    static QString fieldDescription(const QString &field);
    static QList<QPair<int,QString>> choices(const QString &field);
    static bool measurement(const QString &field);
    static QString displayValue(const QString &field, const QString &value, UserUnitPrefs prefs);
    static bool encode(const QMap<QString,QString> &fields, tN2kMsg &message);
    static bool decode(const tN2kMsg &message, QStringList &parameters);
    QString value(const QString &field) const { return values.value(field); }
    void setValue(const QString &field, const QString &text);
    int MessageID() const { return value("MessageID").toInt(); }
    void setMessageID(int v) { setValue("MessageID", QString::number(v)); }
    int Repeat() const { return value("Repeat").toInt(); }
    void setRepeat(int v) { setValue("Repeat", QString::number(v)); }
    quint32 UserID() const { return value("UserID").toUInt(); }
    void setUserID(quint32 v) { setValue("UserID", QString::number(v)); }
    double Latitude() const { return value("Latitude").toDouble(); }
    void setLatitude(double v) { setValue("Latitude", QString::number(v, 'g', 17)); }
    double Longitude() const { return value("Longitude").toDouble(); }
    void setLongitude(double v) { setValue("Longitude", QString::number(v, 'g', 17)); }
    bool Accuracy() const { return value("Accuracy")=="true" || value("Accuracy")=="1"; }
    void setAccuracy(bool v) { setValue("Accuracy", v ? "true" : "false"); }
    bool RAIM() const { return value("RAIM")=="true" || value("RAIM")=="1"; }
    void setRAIM(bool v) { setValue("RAIM", v ? "true" : "false"); }
    int Seconds() const { return value("Seconds").toInt(); }
    void setSeconds(int v) { setValue("Seconds", QString::number(v)); }
    double COG() const { return value("COG").toDouble(); }
    void setCOG(double v) { setValue("COG", QString::number(v, 'g', 17)); }
    double SOG() const { return value("SOG").toDouble(); }
    void setSOG(double v) { setValue("SOG", QString::number(v, 'g', 17)); }
    int AISInfo() const { return value("AISInfo").toInt(); }
    void setAISInfo(int v) { setValue("AISInfo", QString::number(v)); }
    double Heading() const { return value("Heading").toDouble(); }
    void setHeading(double v) { setValue("Heading", QString::number(v, 'g', 17)); }
    int Unit() const { return value("Unit").toInt(); }
    void setUnit(int v) { setValue("Unit", QString::number(v)); }
    bool Display() const { return value("Display")=="true" || value("Display")=="1"; }
    void setDisplay(bool v) { setValue("Display", v ? "true" : "false"); }
    bool DSC() const { return value("DSC")=="true" || value("DSC")=="1"; }
    void setDSC(bool v) { setValue("DSC", v ? "true" : "false"); }
    bool Band() const { return value("Band")=="true" || value("Band")=="1"; }
    void setBand(bool v) { setValue("Band", v ? "true" : "false"); }
    bool Msg22() const { return value("Msg22")=="true" || value("Msg22")=="1"; }
    void setMsg22(bool v) { setValue("Msg22", v ? "true" : "false"); }
    int Mode() const { return value("Mode").toInt(); }
    void setMode(int v) { setValue("Mode", QString::number(v)); }
    bool State() const { return value("State")=="true" || value("State")=="1"; }
    void setState(bool v) { setValue("State", v ? "true" : "false"); }
    int SID() const { return value("SID").toInt(); }
    void setSID(int v) { setValue("SID", QString::number(v)); }
    void setInitialGuiValues() override;
    bool navigationControlled() const { return m_navigationControlled; }
    void setNavigationControlled(bool controlled);
    bool observationTimeControlled() const { return m_observationTimeControlled; }
    void setObservationTimeControlled(bool controlled);
signals:
    void navigationControlChanged();
    void parameterValueChanged(const QString &field, const QString &value);
protected:
    void initializeUnitsLimits() override;
private:
    bool m_navigationControlled = false; // Runtime ownership only; never serialized.
    bool m_observationTimeControlled = false;
    QMap<QString,QString> values;
    QMap<QString,QLineEdit*> editors;
    QMap<QString,QLabel*> captions;
    void refresh();
};
