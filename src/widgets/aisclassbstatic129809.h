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
#include <QVariant>
class QLineEdit;

// Manual/static AIS report. CAN source remains PgnWidgetBase::Source, not UserID.
class AisClassBStatic129809 final : public PgnWidgetBase {
    Q_OBJECT
    Q_PROPERTY(int MessageID READ MessageID WRITE setMessageID)
    Q_PROPERTY(int Repeat READ Repeat WRITE setRepeat)
    Q_PROPERTY(quint32 UserID READ UserID WRITE setUserID)
    Q_PROPERTY(QString Name READ Name WRITE setName)
    Q_PROPERTY(int AISInfo READ AISInfo WRITE setAISInfo)
    Q_PROPERTY(int SID READ SID WRITE setSID)
public:
    AisClassBStatic129809(int id, const QString &label, QWidget *parent = nullptr);
    static QList<QList<QString>> parameters();
    static QString fieldDescription(const QString &field);
    static bool validField(const QString &field, const QString &value);
    static bool encode(const QMap<QString, QString> &fields, tN2kMsg &message);
    static bool decode(const tN2kMsg &message, QStringList &parameters);
    QString value(const QString &field) const { return values.value(field); }
    void setValue(const QString &field, const QString &value);
    int MessageID() const { return value("MessageID").toInt(); }
    void setMessageID(int v) { setValue("MessageID", QString::number(v)); }
    int Repeat() const { return value("Repeat").toInt(); }
    void setRepeat(int v) { setValue("Repeat", QString::number(v)); }
    quint32 UserID() const { return value("UserID").toUInt(); }
    void setUserID(quint32 v) { setValue("UserID", QString::number(v)); }
    QString Name() const { return value("Name"); }
    void setName(const QString &v) { setValue("Name", v); }
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
    void refresh();
};
