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

#ifndef ENGINEDYNAMIC127489_H
#define ENGINEDYNAMIC127489_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

#include "../NMEA2000/src/N2kTypes.h"
#include "datastructs.h"

namespace Ui {
class EngineDynamic127489;
}

class EngineDynamic127489 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit EngineDynamic127489(int id, QString label, QWidget *parent = nullptr);
    ~EngineDynamic127489();

    Q_PROPERTY(int EngineInstance READ EngineInstance WRITE setEngineInstance NOTIFY EngineInstanceChanged FINAL)
    Q_PROPERTY(double EngineOilPress READ EngineOilPress WRITE setEngineOilPress NOTIFY EngineOilPressChanged FINAL)
    Q_PROPERTY(double EngineOilTemp READ EngineOilTemp WRITE setEngineOilTemp NOTIFY EngineOilTempChanged FINAL)
    Q_PROPERTY(double EngineCoolantTemp READ EngineCoolantTemp WRITE setEngineCoolantTemp NOTIFY EngineCoolantTempChanged FINAL)
    Q_PROPERTY(double AltenatorVoltage READ AltenatorVoltage WRITE setAltenatorVoltage NOTIFY AltenatorVoltageChanged FINAL)
    Q_PROPERTY(double FuelRate READ FuelRate WRITE setFuelRate NOTIFY FuelRateChanged FINAL)
    Q_PROPERTY(double EngineHours READ EngineHours WRITE setEngineHours NOTIFY EngineHoursChanged FINAL)
    Q_PROPERTY(double EngineCoolantPress READ EngineCoolantPress WRITE setEngineCoolantPress NOTIFY EngineCoolantPressChanged FINAL)
    Q_PROPERTY(double EngineFuelPress READ EngineFuelPress WRITE setEngineFuelPress NOTIFY EngineFuelPressChanged FINAL)
    Q_PROPERTY(double EngineLoad READ EngineLoad WRITE setEngineLoad NOTIFY EngineLoadChanged FINAL)
    Q_PROPERTY(double EngineTorque READ EngineTorque WRITE setEngineTorque NOTIFY EngineTorqueChanged FINAL)
    Q_PROPERTY(tN2kEngineDiscreteStatus1 Status1 READ EngineDiscreteStatus1 WRITE setEngineDiscreteStatus1 NOTIFY EngineDiscreteStatus1Changed FINAL)
    Q_PROPERTY(tN2kEngineDiscreteStatus2 Status2 READ EngineDiscreteStatus2 WRITE setEngineDiscreteStatus2 NOTIFY EngineDiscreteStatus2Changed FINAL)

    int EngineInstance() const;
    void setEngineInstance(int newEngineInstance);

    double EngineOilPress() const;
    void setEngineOilPress(double newEngineOilPress);

    double EngineOilTemp() const;
    void setEngineOilTemp(double newEngineOilTemp);

    double EngineCoolantTemp() const;
    void setEngineCoolantTemp(double newEngineCoolantTemp);

    double AltenatorVoltage() const;
    void setAltenatorVoltage(double newAltenatorVoltage);

    double FuelRate() const;
    void setFuelRate(double newFuelRate);

    double EngineHours() const;
    void setEngineHours(double newEngineHours);

    double EngineCoolantPress() const;
    void setEngineCoolantPress(double newEngineCoolantPress);

    double EngineFuelPress() const;
    void setEngineFuelPress(double newEngineFuelPress);

    double EngineLoad() const;
    void setEngineLoad(double newEngineLoad);

    double EngineTorque() const;
    void setEngineTorque(double newEngineTorque);

    tN2kEngineDiscreteStatus1 EngineDiscreteStatus1() const;
    void setEngineDiscreteStatus1(const tN2kEngineDiscreteStatus1 &newStatus1);

    tN2kEngineDiscreteStatus2 EngineDiscreteStatus2() const;
    void setEngineDiscreteStatus2(const tN2kEngineDiscreteStatus2 &newStatus2);

    void setInitialGuiValues() override;

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

signals:
    void EngineInstanceChanged();
    void EngineOilPressChanged();
    void EngineOilTempChanged();
    void EngineCoolantTempChanged();
    void AltenatorVoltageChanged();
    void FuelRateChanged();
    void EngineHoursChanged();
    void EngineCoolantPressChanged();
    void EngineFuelPressChanged();
    void EngineLoadChanged();
    void EngineTorqueChanged();
    void EngineDiscreteStatus1Changed();
    void EngineDiscreteStatus2Changed();

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditEngineOilPress_textEdited(const QString &arg1);
    void on_lineEditEngineOilTemp_textEdited(const QString &arg1);
    void on_lineEditCoolantTemp_textEdited(const QString &arg1);
    void on_lineEditAltenatorVoltage_textEdited(const QString &arg1);
    void on_lineEditFuelRate_textEdited(const QString &arg1);
    void on_lineEditEngineHours_textEdited(const QString &arg1);
    void on_lineEditCoolantPress_textEdited(const QString &arg1);
    void on_lineEditFuelPress_textEdited(const QString &arg1);
    void on_lineEditEngineLoad_textEdited(const QString &arg1);
    void on_lineEditEngineTorque_textEdited(const QString &arg1);
    void on_checkBoxEnabled_clicked(bool checked);
    void on_checkBoxLowCoolantLevel_clicked(bool checked);
    void on_checkBoxWaterFlow_clicked(bool checked);
    void on_checkBoxWaterInFuel_clicked(bool checked);
    void on_checkBoxChargeIndicator_clicked(bool checked);
    void on_checkBoxPreheatIndicator_clicked(bool checked);
    void on_checkBoxHighBoostPressure_clicked(bool checked);
    void on_checkBoxRevLimitExceeded_clicked(bool checked);
    void on_checkBoxEGRSystem_clicked(bool checked);
    void on_checkBoxThrottlePositionSensor_clicked(bool checked);
    void on_checkBoxCheckEngine_clicked(bool checked);
    void on_checkBoxOverTemperature_clicked(bool checked);
    void on_checkBoxLowOilPressure_clicked(bool checked);
    void on_checkBoxLowOilLevel_clicked(bool checked);
    void on_checkBoxLowFuelPressure_clicked(bool checked);
    void on_checkBoxLowSystemVoltage_clicked(bool checked);
    void on_checkBoxWarningLevel1_clicked(bool checked);
    void on_checkBoxWarningLevel2_clicked(bool checked);
    void on_checkBoxLowOilPowerReduction_clicked(bool checked);
    void on_checkBoxMaintenanceNeeded_clicked(bool checked);
    void on_checkBoxEngineCommError_clicked(bool checked);
    void on_checkBoxSubOrSecondaryThrottle_clicked(bool checked);
    void on_checkBoxNeutralStartProtect_clicked(bool checked);
    void on_checkBoxEngineShuttingDown_clicked(bool checked);

private:
    Ui::EngineDynamic127489 *ui;
    QDoubleValidator *validatorTemperature = new QDoubleValidator(this);
    QDoubleValidator *validatorPressure = new QDoubleValidator(this);
    QDoubleValidator *validatorVoltage = new QDoubleValidator(this);
    QDoubleValidator *validatorFuelRate = new QDoubleValidator(this);
    QDoubleValidator *validatorHours = new QDoubleValidator(this);
    QIntValidator *validatorPercentage = new QIntValidator(this);
    QIntValidator *validatorTorque = new QIntValidator(this);

    int m_EngineInstance = 0;
    double m_EngineOilPress = 0.0;
    double m_EngineOilTemp = 0.0;
    double m_EngineCoolantTemp = 0.0;
    double m_AltenatorVoltage = 0.0;
    double m_FuelRate = 0.0;
    double m_EngineHours = 0.0;
    double m_EngineCoolantPress = 0.0;
    double m_EngineFuelPress = 0.0;
    double m_EngineLoad = 0.0;
    double m_EngineTorque = 0.0;
    tN2kEngineDiscreteStatus1 m_Status1 = static_cast<tN2kEngineDiscreteStatus1>(0);
    tN2kEngineDiscreteStatus2 m_Status2 = static_cast<tN2kEngineDiscreteStatus2>(0);

};

#endif // ENGINEDYNAMIC127489_H
