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

#ifndef OUTSIDEENVIRONMENTAL130310_H
#define OUTSIDEENVIRONMENTAL130310_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class OutsideEnvironmental130130;
}

class OutsideEnvironmental130310 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit OutsideEnvironmental130310(int id, QString label, QWidget *parent = nullptr);
    ~OutsideEnvironmental130310();

    Q_PROPERTY(uchar SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double WaterTemperature READ WaterTemperature WRITE setWaterTemperature NOTIFY WaterTemperatureChanged FINAL)
    Q_PROPERTY(double OutsideAmbientAirTemperature READ OutsideAmbientAirTemperature WRITE setOutsideAmbientAirTemperature NOTIFY
                   OutsideAmbientAirTemperatureChanged FINAL)
    Q_PROPERTY(double AtmosphericPressure READ AtmosphericPressure WRITE setAtmosphericPressure NOTIFY AtmosphericPressureChanged FINAL)

    uchar SID() const;
    void setSID(uchar newSID);
    double WaterTemperature() const;
    void setWaterTemperature(double newSetWaterTemperature);
    double OutsideAmbientAirTemperature() const;
    void setOutsideAmbientAirTemperature(double newSetOutsideAmbientAirTemperature);
    double AtmosphericPressure() const;
    void setAtmosphericPressure(double newSetAtmosphericPressure);
    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void WaterTemperatureChanged();
    void OutsideAmbientAirTemperatureChanged();
    void AtmosphericPressureChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

private slots:
    void on_lineEditWaterTemperature_textEdited(const QString &arg1);
    void on_lineEditAmbientAirTemperature_textEdited(const QString &arg1);
    void on_lineEditAtmosphericPressure_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::OutsideEnvironmental130130 *ui;

    QDoubleValidator *validatorTemperature = new QDoubleValidator(this);
    QDoubleValidator *validatorPressure = new QDoubleValidator(this);
    double m_WaterTemperature = 0.0;
    double m_OutsideAmbientAirTemperature = 0.0;
    double m_AtmosphericPressure = 0.0;

    uchar m_SID = 0;
};

#endif // OUTSIDEENVIRONMENTAL130310_H
