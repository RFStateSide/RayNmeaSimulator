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

#include "enginedynamic127489.h"
#include "ui_enginedynamic127489.h"

#include "n2khelper.h"

EngineDynamic127489::EngineDynamic127489(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::EngineDynamic127489)
{
    ui->setupUi(this);
    setId(id);
    Q_UNUSED(label);
    setPgn(127489);
    setColumnSpan(3);
    setRowSpan(12);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

EngineDynamic127489::~EngineDynamic127489()
{
    delete ui;
}

void EngineDynamic127489::onTimerTimeout()
{
    if (m_SignalToEmit == "EngineOilPressChanged") {
        m_EngineOilPress = N2kHelper::convertUserPressureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit EngineOilPressChanged();
    } else if (m_SignalToEmit == "EngineOilTempChanged") {
        m_EngineOilTemp = N2kHelper::convertUserTemperatureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit EngineOilTempChanged();
    } else if (m_SignalToEmit == "EngineCoolantTempChanged") {
        m_EngineCoolantTemp = N2kHelper::convertUserTemperatureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit EngineCoolantTempChanged();
    } else if (m_SignalToEmit == "AltenatorVoltageChanged") {
        m_AltenatorVoltage = m_ValueToEmit.toDouble();
        emit AltenatorVoltageChanged();
    } else if (m_SignalToEmit == "FuelRateChanged") {
        m_FuelRate = N2kHelper::convertUserFlowRateToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit FuelRateChanged();
    } else if (m_SignalToEmit == "EngineHoursChanged") {
        m_EngineHours = m_ValueToEmit.toDouble() * 3600.0; // Compact editor uses hours; canonical value is seconds.
        emit EngineHoursChanged();
    } else if (m_SignalToEmit == "EngineCoolantPressChanged") {
        m_EngineCoolantPress = N2kHelper::convertUserPressureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit EngineCoolantPressChanged();
    } else if (m_SignalToEmit == "EngineFuelPressChanged") {
        m_EngineFuelPress = N2kHelper::convertUserPressureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit EngineFuelPressChanged();
    } else if (m_SignalToEmit == "EngineLoadChanged") {
        m_EngineLoad = m_ValueToEmit.toInt();
        emit EngineLoadChanged();
    } else if (m_SignalToEmit == "EngineTorqueChanged") {
        m_EngineTorque = m_ValueToEmit.toInt();
        emit EngineTorqueChanged();
    }

    m_SignalToEmit.clear();
}

void EngineDynamic127489::setInitialGuiValues()
{
    ui->lineEditEngineOilPress->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(m_EngineOilPress, UserPrefs()), 'f', 1));
    ui->lineEditEngineOilTemp->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_EngineOilTemp, UserPrefs()), 'f', 1));
    ui->lineEditCoolantTemp->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_EngineCoolantTemp, UserPrefs()), 'f', 1));
    ui->lineEditAltenatorVoltage->setText(QString::number(AltenatorVoltage(), 'f', 1));
    ui->lineEditFuelRate->setText(QString::number(N2kHelper::convertN2kFlowRateToUserUnits(m_FuelRate, UserPrefs()), 'f', 1));
    ui->lineEditEngineHours->setText(QString::number(m_EngineHours / 3600, 'f', 2));
    ui->lineEditCoolantPress->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(m_EngineCoolantPress, UserPrefs()), 'f', 1));
    ui->lineEditFuelPress->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(m_EngineFuelPress, UserPrefs()), 'f', 1));
    ui->lineEditEngineLoad->setText(QString::number(m_EngineLoad, 'f', 1));
    ui->lineEditEngineTorque->setText(QString::number(m_EngineTorque, 'f', 1));
}

int EngineDynamic127489::EngineInstance() const
{
    return m_EngineInstance;
}

void EngineDynamic127489::setEngineInstance(int newEngineInstance)
{
    if (m_EngineInstance == newEngineInstance)
        return;
    m_EngineInstance = newEngineInstance;
    ui->labelTitle->setText("Engine #" + QString::number(newEngineInstance + 1) + ", Dynamic Update (127489)");
    emit EngineInstanceChanged();
}

double EngineDynamic127489::EngineOilPress() const
{
    return m_EngineOilPress;
}

void EngineDynamic127489::setEngineOilPress(double newEngineOilPress)
{
    if (qFuzzyCompare(m_EngineOilPress, newEngineOilPress))
        //if (qFuzzyCompare(m_EngineOilPress, N2kHelper::convertN2kPressureToUserUnits(newEngineOilPress, m_UserPrefs)))
        return;
    m_EngineOilPress = newEngineOilPress;
    ui->lineEditEngineOilPress->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(newEngineOilPress, UserPrefs()), 'f', 1));
    emit EngineOilPressChanged();
}

double EngineDynamic127489::EngineOilTemp() const
{
    return m_EngineOilTemp;
    //return N2kHelper::convertN2kTemperatureToUserUnits(m_EngineOilTemp, UserPrefs());
}

void EngineDynamic127489::setEngineOilTemp(double newEngineOilTemp)
{
    if (qFuzzyCompare(m_EngineOilTemp, newEngineOilTemp))
        //if (qFuzzyCompare(m_EngineOilTemp, N2kHelper::convertN2kTemperatureToUserUnits(newEngineOilTemp, m_UserPrefs)))
        return;
    m_EngineOilTemp = newEngineOilTemp;
    ui->lineEditEngineOilTemp->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(newEngineOilTemp, UserPrefs()), 'f', 1));
    emit EngineOilTempChanged();
}

double EngineDynamic127489::EngineCoolantTemp() const
{
    return m_EngineCoolantTemp;
    //return N2kHelper::convertN2kTemperatureToUserUnits(m_EngineCoolantTemp, UserPrefs());
}

void EngineDynamic127489::setEngineCoolantTemp(double newEngineCoolantTemp)
{
    if (qFuzzyCompare(m_EngineCoolantTemp, newEngineCoolantTemp))
        //if (qFuzzyCompare(m_EngineCoolantTemp, N2kHelper::convertN2kTemperatureToUserUnits(newEngineCoolantTemp, m_UserPrefs)))
        return;
    m_EngineCoolantTemp = newEngineCoolantTemp;
    ui->lineEditCoolantTemp->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(newEngineCoolantTemp, UserPrefs()), 'f', 1));
    emit EngineCoolantTempChanged();
}

void EngineDynamic127489::setAltenatorVoltage(double newAltenatorVoltage)
{
    if (qFuzzyCompare(m_AltenatorVoltage, newAltenatorVoltage))
        return;
    m_AltenatorVoltage = newAltenatorVoltage;
    ui->lineEditAltenatorVoltage->setText(QString::number(newAltenatorVoltage, 'f', 1));
    emit AltenatorVoltageChanged();
}

double EngineDynamic127489::AltenatorVoltage() const
{
    return m_AltenatorVoltage;
}

void EngineDynamic127489::setFuelRate(double newFuelRate)
{
    if (qFuzzyCompare(m_FuelRate, newFuelRate))
        //if (qFuzzyCompare(m_FuelRate, N2kHelper::convertN2kFlowRateToUserUnits(newFuelRate, m_UserPrefs)))
        return;
    m_FuelRate = newFuelRate;
    ui->lineEditFuelRate->setText(QString::number(N2kHelper::convertN2kFlowRateToUserUnits(newFuelRate, UserPrefs()), 'f', 1));
    emit FuelRateChanged();
}

double EngineDynamic127489::FuelRate() const
{
    return m_FuelRate;
    //return N2kHelper::convertN2kFlowRateToUserUnits(m_FuelRate, UserPrefs());
}

void EngineDynamic127489::setEngineHours(double newEngineHours)
{
    if (qFuzzyCompare(m_EngineHours, newEngineHours))
        return;
    m_EngineHours = newEngineHours;
    ui->lineEditEngineHours->setText(QString::number(newEngineHours / 3600, 'f', 2));
    emit EngineHoursChanged();
}

double EngineDynamic127489::EngineHours() const
{
    return m_EngineHours;
}

void EngineDynamic127489::setEngineCoolantPress(double newEngineCoolantPress)
{
    if (qFuzzyCompare(m_EngineCoolantPress, newEngineCoolantPress))
        //if (qFuzzyCompare(m_EngineCoolantPress, N2kHelper::convertN2kPressureToUserUnits(newEngineCoolantPress, m_UserPrefs)))
        return;
    m_EngineCoolantPress = newEngineCoolantPress;
    ui->lineEditCoolantPress->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(newEngineCoolantPress, UserPrefs()), 'f', 1));
    emit EngineCoolantPressChanged();
}

double EngineDynamic127489::EngineCoolantPress() const
{
    return m_EngineCoolantPress;
    //return N2kHelper::convertN2kPressureToUserUnits(m_EngineCoolantPress, UserPrefs());
}

void EngineDynamic127489::setEngineFuelPress(double newEngineFuelPress)
{
    if (qFuzzyCompare(m_EngineFuelPress, newEngineFuelPress))
        //if (qFuzzyCompare(m_EngineFuelPress, N2kHelper::convertN2kPressureToUserUnits(newEngineFuelPress, m_UserPrefs)))
        return;
    m_EngineFuelPress = newEngineFuelPress;
    ui->lineEditFuelPress->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(newEngineFuelPress, UserPrefs()), 'f', 1));
    emit EngineFuelPressChanged();
}

double EngineDynamic127489::EngineFuelPress() const
{
    return m_EngineFuelPress;
    //return N2kHelper::convertN2kPressureToUserUnits(m_EngineFuelPress, UserPrefs());
}

void EngineDynamic127489::setEngineLoad(double newEngineLoad)
{
    if (qFuzzyCompare(m_EngineLoad, newEngineLoad))
        return;
    m_EngineLoad = newEngineLoad;
    ui->lineEditEngineLoad->setText(QString::number(newEngineLoad, 'f', 1));
    emit EngineLoadChanged();
}

double EngineDynamic127489::EngineLoad() const
{
    return m_EngineLoad;
}

void EngineDynamic127489::setEngineTorque(double newEngineTorque)
{
    if (qFuzzyCompare(m_EngineTorque, newEngineTorque))
        return;
    m_EngineTorque = newEngineTorque;
    ui->lineEditEngineTorque->setText(QString::number(newEngineTorque, 'f', 1));
    emit EngineTorqueChanged();
}

double EngineDynamic127489::EngineTorque() const
{
    return m_EngineTorque;
}

tN2kEngineDiscreteStatus1 EngineDynamic127489::EngineDiscreteStatus1() const
{
    return m_Status1;
}

void EngineDynamic127489::setEngineDiscreteStatus1(const tN2kEngineDiscreteStatus1 &newStatus1)
{
    if (m_Status1.Status == newStatus1.Status)
        return;
    m_Status1 = newStatus1;

    ui->checkBoxCheckEngine->setChecked(m_Status1.Bits.CheckEngine);
    ui->checkBoxOverTemperature->setChecked(m_Status1.Bits.OverTemperature);
    ui->checkBoxLowOilPressure->setChecked(m_Status1.Bits.LowOilPressure);
    ui->checkBoxLowOilLevel->setChecked(m_Status1.Bits.LowOilLevel);
    ui->checkBoxLowFuelPressure->setChecked(m_Status1.Bits.LowFuelPressure);
    ui->checkBoxLowSystemVoltage->setChecked(m_Status1.Bits.LowSystemVoltage);
    ui->checkBoxLowCoolantLevel->setChecked(m_Status1.Bits.LowCoolantLevel);
    ui->checkBoxWaterFlow->setChecked(m_Status1.Bits.WaterFlow);
    ui->checkBoxWaterInFuel->setChecked(m_Status1.Bits.WaterInFuel);
    ui->checkBoxChargeIndicator->setChecked(m_Status1.Bits.ChargeIndicator);
    ui->checkBoxPreheatIndicator->setChecked(m_Status1.Bits.PreheatIndicator);
    ui->checkBoxHighBoostPressure->setChecked(m_Status1.Bits.HighBoostPressure);

    emit EngineDiscreteStatus1Changed();
}

tN2kEngineDiscreteStatus2 EngineDynamic127489::EngineDiscreteStatus2() const
{
    return m_Status2;
}

void EngineDynamic127489::setEngineDiscreteStatus2(const tN2kEngineDiscreteStatus2 &newStatus2)
{
    if (m_Status2.Status == newStatus2.Status)
        return;
    m_Status2 = newStatus2;

    ui->checkBoxWarningLevel1->setChecked(m_Status2.Bits.WarningLevel1);
    ui->checkBoxWarningLevel2->setChecked(m_Status2.Bits.WarningLevel2);
    ui->checkBoxLowOilPowerReduction->setChecked(m_Status2.Bits.LowOiPowerReduction);
    ui->checkBoxMaintenanceNeeded->setChecked(m_Status2.Bits.MaintenanceNeeded);
    ui->checkBoxEngineCommError->setChecked(m_Status2.Bits.EngineCommError);
    ui->checkBoxSubOrSecondaryThrottle->setChecked(m_Status2.Bits.SubOrSecondaryThrottle);
    ui->checkBoxNeutralStartProtect->setChecked(m_Status2.Bits.NeutralStartProtect);
    ui->checkBoxEngineShuttingDown->setChecked(m_Status2.Bits.EngineShuttingDown);

    emit EngineDiscreteStatus2Changed();
}

void EngineDynamic127489::initializeUnitsLimits()
{
    // "Kelvin (°K)", "Celsius (°C)", "Fahrenheit (°F)"
    switch (UserPrefs().tempUnitIndex) {
    case 0:
        ui->labelUnitsEngineCoolantTemp->setText("°K");
        ui->labelUnitsEngineOilTemp->setText("°K");
        validatorTemperature->setRange(222.039, 394.261, 2);
        break;
    case 1:
        ui->labelUnitsEngineCoolantTemp->setText("°C");
        ui->labelUnitsEngineOilTemp->setText("°C");
        validatorTemperature->setRange(-51.111, 121.111, 2);
        break;
    case 2:
        ui->labelUnitsEngineCoolantTemp->setText("°F");
        ui->labelUnitsEngineOilTemp->setText("°F");
        validatorTemperature->setRange(-60.0, 250.0, 2);
        break;
    default:
        qWarning() << "Invalid temperature unit index:" << UserPrefs().tempUnitIndex;
        break;
    }

    // Pa, kPa, Bar, PSI
    switch (UserPrefs().pressureUnitIndex) {
    case 0:
        ui->labelUnitsEngineCoolantPress->setText("Pa");
        ui->labelUnitsEngineFuelPress->setText("Pa");
        ui->labelUnitsEngineOilPress->setText("Pa");
        validatorPressure->setRange(0.0, 1034000.0, 1);
        break;
    case 1:
        ui->labelUnitsEngineCoolantPress->setText("kPa");
        ui->labelUnitsEngineFuelPress->setText("kPa");
        ui->labelUnitsEngineOilPress->setText("kPa");
        validatorPressure->setRange(0.0, 1034.0, 1);
        break;
    case 2:
        ui->labelUnitsEngineCoolantPress->setText("Bar");
        ui->labelUnitsEngineFuelPress->setText("Bar");
        ui->labelUnitsEngineOilPress->setText("Bar");
        validatorPressure->setRange(0.0, 10.34, 2);
        break;
    case 3:
        ui->labelUnitsEngineCoolantPress->setText("PSI");
        ui->labelUnitsEngineFuelPress->setText("PSI");
        ui->labelUnitsEngineOilPress->setText("PSI");
        validatorPressure->setRange(0.0, 150.0, 1);
        break;
    default:
        qWarning() << "Invalid pressure unit index";
        break;
    }

    // "LPH", "GPH"
    switch (UserPrefs().flowRateUnitIndex) {
    case 0:
        ui->labelUnitsFuelRate->setText("LPH");
        validatorFuelRate->setRange(0.0, 1000.0, 2);
        break;
    case 1:
        ui->labelUnitsFuelRate->setText("GPH");
        validatorFuelRate->setRange(0.0, 264.172, 2);
        break;
    default:
        qWarning() << "Invalid flow rate unit index";
        break;
    }

    // TODO: Support user prefs for limits
    validatorHours->setRange(0.0, 10000.0, 2);
    validatorVoltage->setRange(0.0, 1000.0, 2);
    validatorPercentage->setRange(0, 100);
    validatorTorque->setRange(-100, 100);
    ui->lineEditEngineHours->setValidator(validatorHours);
    ui->lineEditEngineOilPress->setValidator(validatorPressure);
    ui->lineEditCoolantPress->setValidator(validatorPressure);
    ui->lineEditFuelPress->setValidator(validatorPressure);
    ui->lineEditEngineLoad->setValidator(validatorPercentage);
    ui->lineEditEngineTorque->setValidator(validatorTorque);
    ui->lineEditAltenatorVoltage->setValidator(validatorVoltage);
    ui->lineEditFuelRate->setValidator(validatorFuelRate);
    ui->lineEditEngineOilTemp->setValidator(validatorTemperature);
    ui->lineEditCoolantTemp->setValidator(validatorTemperature);
}

void EngineDynamic127489::initializeClickableLabels()
{
    connect(this, &EngineDynamic127489::UserPrefsChanged, this, &EngineDynamic127489::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsEngineCoolantTemp->setMenuItems(unitsListTemp);
    connect(ui->labelUnitsEngineCoolantTemp, &ClickableLabel::actionTriggered, this, &EngineDynamic127489::ChangeTempUnits_triggered);
    ui->labelUnitsEngineOilTemp->setMenuItems(unitsListTemp);
    connect(ui->labelUnitsEngineOilTemp, &ClickableLabel::actionTriggered, this, &EngineDynamic127489::ChangeTempUnits_triggered);
    ui->labelUnitsEngineCoolantPress->setMenuItems(unitsListPressure);
    connect(ui->labelUnitsEngineCoolantPress, &ClickableLabel::actionTriggered, this, &EngineDynamic127489::ChangePressureUnits_triggered);
    ui->labelUnitsEngineFuelPress->setMenuItems(unitsListPressure);
    connect(ui->labelUnitsEngineFuelPress, &ClickableLabel::actionTriggered, this, &EngineDynamic127489::ChangePressureUnits_triggered);
    ui->labelUnitsEngineOilPress->setMenuItems(unitsListPressure);
    connect(ui->labelUnitsEngineOilPress, &ClickableLabel::actionTriggered, this, &EngineDynamic127489::ChangePressureUnits_triggered);
    ui->labelUnitsFuelRate->setMenuItems(unitsListFlowRate);
    connect(ui->labelUnitsFuelRate, &ClickableLabel::actionTriggered, this, &::EngineDynamic127489::ChangeFuelRateUnits_triggered);
}

void EngineDynamic127489::on_lineEditEngineOilPress_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditEngineOilPress, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineOilPressChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_lineEditEngineOilTemp_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditEngineOilTemp, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineOilTempChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_lineEditCoolantTemp_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditCoolantTemp, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineCoolantTempChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_lineEditAltenatorVoltage_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditAltenatorVoltage, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "AltenatorVoltageChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_lineEditFuelRate_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditFuelRate, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "FuelRateChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_lineEditEngineHours_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditEngineHours, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineHoursChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_lineEditCoolantPress_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditCoolantPress, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineCoolantPressChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_lineEditFuelPress_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditFuelPress, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineFuelPressChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_lineEditEngineLoad_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditEngineLoad, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineLoadChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_lineEditEngineTorque_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditEngineTorque, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineTorqueChanged";
    onTimerTimeout();
}

void EngineDynamic127489::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void EngineDynamic127489::on_checkBoxCheckEngine_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.CheckEngine = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxOverTemperature_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.OverTemperature = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxLowOilPressure_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.LowOilPressure = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxLowOilLevel_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.LowOilLevel = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxLowFuelPressure_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.LowFuelPressure = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxLowSystemVoltage_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.LowSystemVoltage = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxLowCoolantLevel_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.LowCoolantLevel = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxWaterFlow_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.WaterFlow = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxWaterInFuel_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.WaterInFuel = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxChargeIndicator_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.ChargeIndicator = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxPreheatIndicator_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.PreheatIndicator = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxHighBoostPressure_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.HighBoostPressure = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxRevLimitExceeded_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.RevLimitExceeded = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxEGRSystem_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.EGRSystem = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxThrottlePositionSensor_clicked(bool checked)
{
    tN2kEngineDiscreteStatus1 status1 = EngineDiscreteStatus1();
    status1.Bits.ThrottlePositionSensor = checked;
    setEngineDiscreteStatus1(status1);
}

void EngineDynamic127489::on_checkBoxWarningLevel1_clicked(bool checked)
{
    tN2kEngineDiscreteStatus2 status2 = EngineDiscreteStatus2();
    status2.Bits.WarningLevel1 = checked;
    setEngineDiscreteStatus2(status2);
}

void EngineDynamic127489::on_checkBoxWarningLevel2_clicked(bool checked)
{
    tN2kEngineDiscreteStatus2 status2 = EngineDiscreteStatus2();
    status2.Bits.WarningLevel2 = checked;
    setEngineDiscreteStatus2(status2);
}

void EngineDynamic127489::on_checkBoxLowOilPowerReduction_clicked(bool checked)
{
    tN2kEngineDiscreteStatus2 status2 = EngineDiscreteStatus2();
    status2.Bits.LowOiPowerReduction = checked;
    setEngineDiscreteStatus2(status2);
}

void EngineDynamic127489::on_checkBoxMaintenanceNeeded_clicked(bool checked)
{
    tN2kEngineDiscreteStatus2 status2 = EngineDiscreteStatus2();
    status2.Bits.MaintenanceNeeded = checked;
    setEngineDiscreteStatus2(status2);
}

void EngineDynamic127489::on_checkBoxEngineCommError_clicked(bool checked)
{
    tN2kEngineDiscreteStatus2 status2 = EngineDiscreteStatus2();
    status2.Bits.EngineCommError = checked;
    setEngineDiscreteStatus2(status2);
}

void EngineDynamic127489::on_checkBoxSubOrSecondaryThrottle_clicked(bool checked)
{
    tN2kEngineDiscreteStatus2 status2 = EngineDiscreteStatus2();
    status2.Bits.SubOrSecondaryThrottle = checked;
    setEngineDiscreteStatus2(status2);
}

void EngineDynamic127489::on_checkBoxNeutralStartProtect_clicked(bool checked)
{
    tN2kEngineDiscreteStatus2 status2 = EngineDiscreteStatus2();
    status2.Bits.NeutralStartProtect = checked;
    setEngineDiscreteStatus2(status2);
}

void EngineDynamic127489::on_checkBoxEngineShuttingDown_clicked(bool checked)
{
    tN2kEngineDiscreteStatus2 status2 = EngineDiscreteStatus2();
    status2.Bits.EngineShuttingDown = checked;
    setEngineDiscreteStatus2(status2);
}

