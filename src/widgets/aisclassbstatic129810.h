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

class AisClassBStatic129810 final : public PgnWidgetBase {
    Q_OBJECT
    Q_PROPERTY(int MessageID READ MessageID WRITE setMessageID)
    Q_PROPERTY(int Repeat READ Repeat WRITE setRepeat)
    Q_PROPERTY(quint32 UserID READ UserID WRITE setUserID)
    Q_PROPERTY(int VesselType READ VesselType WRITE setVesselType)
    Q_PROPERTY(QString Vendor READ Vendor WRITE setVendor)
    Q_PROPERTY(QString Callsign READ Callsign WRITE setCallsign)
    Q_PROPERTY(double Length READ Length WRITE setLength)
    Q_PROPERTY(double Beam READ Beam WRITE setBeam)
    Q_PROPERTY(double PosRefStbd READ PosRefStbd WRITE setPosRefStbd)
    Q_PROPERTY(double PosRefBow READ PosRefBow WRITE setPosRefBow)
    Q_PROPERTY(quint32 MothershipID READ MothershipID WRITE setMothershipID)
    Q_PROPERTY(int AISInfo READ AISInfo WRITE setAISInfo)
    Q_PROPERTY(int SID READ SID WRITE setSID)
public:
    AisClassBStatic129810(int id, const QString &label, QWidget *parent = nullptr);
    static QList<QList<QString>> parameters();
    static QString fieldDescription(const QString &field);
    static bool dimension(const QString &field);
    static bool validField(const QString &field, const QString &text);
    static bool editableField(const QString &field, const QString &text);
    static QList<QPair<int, QString>> vesselTypes();
    static bool encode(const QMap<QString, QString> &fields, tN2kMsg &message);
    static bool decode(const tN2kMsg &message, QStringList &parameters);
    QString value(const QString &field) const { return values.value(field); }
    void setValue(const QString &field, const QString &text);
    void setAssociationText(const QString &text);
    int MessageID() const { return value("MessageID").toInt(); }
    void setMessageID(int v) { setValue("MessageID", QString::number(v)); }
    int Repeat() const { return value("Repeat").toInt(); }
    void setRepeat(int v) { setValue("Repeat", QString::number(v)); }
    quint32 UserID() const { return value("UserID").toUInt(); }
    void setUserID(quint32 v) { setValue("UserID", QString::number(v)); }
    int VesselType() const { return value("VesselType").toInt(); }
    void setVesselType(int v) { setValue("VesselType", QString::number(v)); }
    QString Vendor() const { return value("Vendor"); }
    void setVendor(const QString &v) { setValue("Vendor", v); }
    QString Callsign() const { return value("Callsign"); }
    void setCallsign(const QString &v) { setValue("Callsign", v); }
    double Length() const { return value("Length").toDouble(); }
    void setLength(double v) { setValue("Length", QString::number(v, 'g', 17)); }
    double Beam() const { return value("Beam").toDouble(); }
    void setBeam(double v) { setValue("Beam", QString::number(v, 'g', 17)); }
    double PosRefStbd() const { return value("PosRefStbd").toDouble(); }
    void setPosRefStbd(double v) { setValue("PosRefStbd", QString::number(v, 'g', 17)); }
    double PosRefBow() const { return value("PosRefBow").toDouble(); }
    void setPosRefBow(double v) { setValue("PosRefBow", QString::number(v, 'g', 17)); }
    quint32 MothershipID() const { return value("MothershipID").toUInt(); }
    void setMothershipID(quint32 v) { setValue("MothershipID", QString::number(v)); }
    int AISInfo() const { return value("AISInfo").toInt(); }
    void setAISInfo(int v) { setValue("AISInfo", QString::number(v)); }
    int SID() const { return value("SID").toInt(); }
    void setSID(int v) { setValue("SID", QString::number(v)); }
    void setInitialGuiValues() override;
signals:
    void parameterValueChanged(const QString &field, const QString &value);
protected:
    void initializeUnitsLimits() override;
private:
    QMap<QString, QString> values;
    QMap<QString, QLineEdit *> editors;
    QMap<QString, QLabel *> labels;
    QLabel *association = nullptr;
    void refresh();
};
