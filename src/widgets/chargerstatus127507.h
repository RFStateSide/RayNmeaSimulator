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

#ifndef CHARGERSTATUS127507_H
#define CHARGERSTATUS127507_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class ChargerStatus127507;
}

class ChargerStatus127507 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit ChargerStatus127507(int id, const QString &label, QWidget *parent);
    ~ChargerStatus127507();

    /*
 * unsigned char Instance, unsigned char BatteryInstance,
                     tN2kChargeState ChargeState, tN2kChargerMode ChargerMode,
                     tN2kOnOff Enabled, tN2kOnOff EqualizationPending, double EqualizationTimeRemaining
 */
    Q_PROPERTY(unsigned char Instance READ Instance WRITE setInstance NOTIFY InstanceChanged FINAL)
    Q_PROPERTY(unsigned char BatteryInstance READ BatteryInstance WRITE setBatteryInstance NOTIFY BatteryInstanceChanged FINAL)
    Q_PROPERTY(tN2kChargeState ChargeState READ ChargeState WRITE setChargeState NOTIFY ChargeStateChanged FINAL)
    Q_PROPERTY(tN2kChargerMode ChargerMode READ ChargerMode WRITE setChargerMode NOTIFY ChargerModeChanged FINAL)
    Q_PROPERTY(tN2kOnOff Enabled READ Enabled WRITE setEnabled NOTIFY EnabledChanged FINAL)
    Q_PROPERTY(tN2kOnOff EqualizationPending READ EqualizationPending WRITE setEqualizationPending NOTIFY EqualizationPendingChanged FINAL)
    Q_PROPERTY(double EqualizationTimeRemaining READ EqualizationTimeRemaining WRITE setEqualizationTimeRemaining NOTIFY EqualizationTimeRemainingChanged FINAL)

    void setInitialGuiValues() override;

    unsigned char Instance() const;
    void setInstance(unsigned char newInstance);

    unsigned char BatteryInstance() const;
    void setBatteryInstance(unsigned char newBatteryInstance);

    tN2kChargeState ChargeState() const;
    void setChargeState(tN2kChargeState newChargeState);

    tN2kChargerMode ChargerMode() const;
    void setChargerMode(tN2kChargerMode newChargerMode);

    tN2kOnOff Enabled() const;
    void setEnabled(tN2kOnOff newEnabled);

    tN2kOnOff EqualizationPending() const;
    void setEqualizationPending(tN2kOnOff newEqualizationPending);

    double EqualizationTimeRemaining() const;
    void setEqualizationTimeRemaining(double newEqualizationTimeRemaining);

signals:
    void InstanceChanged();
    void BatteryInstanceChanged();
    void ChargeStateChanged();
    void ChargerModeChanged();
    void EnabledChanged();
    void EqualizationPendingChanged();
    void EqualizationTimeRemainingChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_spinBoxBatteryInstance_valueChanged(int arg1);
    void on_comboBoxChargeState_currentTextChanged(const QString &arg1);
    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::ChargerStatus127507 *ui;
    unsigned char m_Instance = 0;
    unsigned char m_BatteryInstance = 0;
    tN2kChargeState m_ChargeState = N2kCS_Not_Charging;
    tN2kChargerMode m_ChargerMode = N2kCM_Standalone;
    tN2kOnOff m_Enabled = N2kOnOff_Off;
    tN2kOnOff m_EqualizationPending = N2kOnOff_Off;
    double m_EqualizationTimeRemaining = 0.0;

    QStringList chargeStates
        = {"Not Charging", "Bulk", "Absorption", "Overcharge", "Equalise", "Float", "No Float", "Constant VI", "Disabled", "Fault", "Error", "Unavailable"};
    void initializeChargeStateComboBox();
};

#endif // CHARGERSTATUS127507_H
