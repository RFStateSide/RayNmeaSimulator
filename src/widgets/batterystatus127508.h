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

#ifndef BATTERYSTATUS127508_H
#define BATTERYSTATUS127508_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "N2kMsg.h"
#include "pgnwidgetbase.h"

namespace Ui {
class BatteryStatus127508;
}

class BatteryStatus127508 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit BatteryStatus127508(int id, const QString &label, QWidget *parent);
    ~BatteryStatus127508();

    Q_PROPERTY(unsigned char BatteryInstance READ BatteryInstance WRITE setBatteryInstance NOTIFY BatteryInstanceChanged FINAL)
    Q_PROPERTY(double BatteryVoltage READ BatteryVoltage WRITE setBatteryVoltage NOTIFY BatteryVoltageChanged FINAL)
    Q_PROPERTY(double BatteryCurrent READ BatteryCurrent WRITE setBatteryCurrent NOTIFY BatteryCurrentChanged FINAL)
    Q_PROPERTY(double BatteryTemperature READ BatteryTemperature WRITE setBatteryTemperature NOTIFY BatteryTemperatureChanged FINAL)
    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)

    void setInitialGuiValues() override;

    unsigned char BatteryInstance() const;
    void setBatteryInstance(unsigned char newBatteryInstance);

    double BatteryVoltage() const;
    void setBatteryVoltage(double newBatteryVoltage);

    double BatteryCurrent() const;
    void setBatteryCurrent(double newBatteryCurrent);

    double BatteryTemperature() const;
    void setBatteryTemperature(double newBatteryTemperature);

    unsigned char SID() const;
    void setSID(unsigned char newSID);

signals:
    void BatteryInstanceChanged();
    void BatteryVoltageChanged();
    void BatteryCurrentChanged();
    void BatteryTemperatureChanged();
    void SIDChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;
private slots:
    void on_checkBoxEnabled_clicked(bool checked);

    void on_lineEditBatteryVoltage_textEdited(const QString &arg1);

    void on_lineEditBatteryCurrent_textEdited(const QString &arg1);

    void on_lineEditBatteryTemperature_textEdited(const QString &arg1);

private:
    Ui::BatteryStatus127508 *ui;
    QDoubleValidator *validatorTemperature = new QDoubleValidator(this);
    QDoubleValidator *validatorCurrent = new QDoubleValidator(this);
    QDoubleValidator *validatorVoltage = new QDoubleValidator(this);
    unsigned char m_BatteryInstance = 0;
    double m_BatteryVoltage = 0;
    double m_BatteryCurrent = 0;
    double m_BatteryTemperature = N2kDoubleNA;
    unsigned char m_SID = 0;
};

#endif // BATTERYSTATUS127508_H
