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

#include "transmissiondynamic127493.h"
#include "ui_transmissiondynamic127493.h"

#include "n2khelper.h"

TransmissionDynamic127493::TransmissionDynamic127493(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::TransmissionDynamic127493)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127493);
    setColumnSpan(3);
    setRowSpan(7);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127493)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

TransmissionDynamic127493::~TransmissionDynamic127493()
{
    delete ui;
}

void TransmissionDynamic127493::initializeUnitsLimits()
{
    // "Kelvin (°K)", "Celsius (°C)", "Fahrenheit (°F)"
    switch (UserPrefs().tempUnitIndex) {
    case 0:
        ui->labelUnitsOilTemperature->setText("°K");
        validatorTemperature->setRange(222.039, 394.261, 2);
        break;
    case 1:
        ui->labelUnitsOilTemperature->setText("°C");
        validatorTemperature->setRange(-50, 120, 2);
        break;
    case 2:
        ui->labelUnitsOilTemperature->setText("°F");
        validatorTemperature->setRange(-58, 248, 2);
        break;
    default:
        qWarning() << "Invalid temperature unit index:" << UserPrefs().tempUnitIndex;
    }

    // "Pascal (kPa)", "Bar", "PSI"
    switch (UserPrefs().pressureUnitIndex) {
    case 0:
        ui->labelUnitsOilPressure->setText("Pa");
        validatorPressure->setRange(0.0, 1034000, 2);
        break;
    case 1:
        ui->labelUnitsOilPressure->setText("kPa");
        validatorPressure->setRange(0.0, 1034, 2);
        break;
    case 2:
        ui->labelUnitsOilPressure->setText("Bar");
        validatorPressure->setRange(0.0, 10.34, 2);
        break;
    case 3:
        ui->labelUnitsOilPressure->setText("PSI");
        validatorPressure->setRange(0.0, 150, 2);
        break;
    }

    ui->lineEditOilTemperature->setValidator(validatorTemperature);
    ui->lineEditOilPressure->setValidator(validatorPressure);
}

void TransmissionDynamic127493::initializeClickableLabels()
{
    connect(this, &TransmissionDynamic127493::UserPrefsChanged, this, &TransmissionDynamic127493::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsOilTemperature->setMenuItems(unitsListTemp);
    connect(ui->labelUnitsOilTemperature, &ClickableLabel::actionTriggered, this, &TransmissionDynamic127493::ChangeTempUnits_triggered);
    ui->labelUnitsOilPressure->setMenuItems(unitsListPressure);
    connect(ui->labelUnitsOilPressure, &ClickableLabel::actionTriggered, this, &TransmissionDynamic127493::ChangePressureUnits_triggered);
}

void TransmissionDynamic127493::onTimerTimeout()
{
    if (m_SignalToEmit == "oilPressureChanged") {
        double converted = N2kHelper::convertUserPressureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        m_OilPressure = converted;
        emit OilPressureChanged();
    } else if (m_SignalToEmit == "oilTemperatureChanged") {
        double converted = N2kHelper::convertUserTemperatureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        m_OilTemperature = converted;
        emit OilTemperatureChanged();
    }
}

unsigned char TransmissionDynamic127493::EngineInstance() const
{
    return m_EngineInstance;
}

void TransmissionDynamic127493::setEngineInstance(unsigned char newEngineInstance)
{
    if (m_EngineInstance == newEngineInstance)
        return;
    m_EngineInstance = newEngineInstance;
    ui->labelTitle->setText("Engine #" + QString::number(m_EngineInstance + 1) + ", Transmission (127493)");
    emit EngineInstanceChanged();
}

tN2kTransmissionGear TransmissionDynamic127493::TransmissionGear() const
{
    return m_TransmissionGear;
}

void TransmissionDynamic127493::setTransmissionGear(tN2kTransmissionGear newTransmissionGear)
{
    if (m_TransmissionGear == newTransmissionGear)
        return;

    m_TransmissionGear = newTransmissionGear;
    switch (static_cast<int>(m_TransmissionGear)) {
    case N2kTG_Forward:
        ui->radioButtonForward->setChecked(true);
        break;
    case N2kTG_Neutral:
        ui->radioButtonNeutral->setChecked(true);
        break;
    case N2kTG_Reverse:
        ui->radioButtonReverse->setChecked(true);
        break;
    case N2kTG_Unknown:
        ui->radioButtonNeutral->setChecked(true);
        break;
    }

    emit TransmissionGearChanged();
}

double TransmissionDynamic127493::OilPressure() const
{
    return m_OilPressure;
}

void TransmissionDynamic127493::setOilPressure(double newOilPressure)
{
    if (m_OilPressure == newOilPressure)
        return;

    m_OilPressure = newOilPressure;
    ui->lineEditOilPressure->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(m_OilPressure, UserPrefs()), 'f', 1));
    emit OilPressureChanged();
}

double TransmissionDynamic127493::OilTemperature() const
{
    return m_OilTemperature;
}

void TransmissionDynamic127493::setOilTemperature(double newOilTemperature)
{
    if (m_OilTemperature == newOilTemperature)
        return;

    m_OilTemperature = newOilTemperature;
    ui->lineEditOilTemperature->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_OilTemperature, UserPrefs()), 'f', 2));
    emit OilTemperatureChanged();
}

unsigned char TransmissionDynamic127493::DiscreteStatus1() const
{
    return m_DiscreteStatus1;
}

void TransmissionDynamic127493::setDiscreteStatus1(unsigned char newDiscreteStatus1)
{
    if (m_DiscreteStatus1 == newDiscreteStatus1)
        return;

    m_DiscreteStatus1 = newDiscreteStatus1;

    ui->checkBoxOverTemperature->setChecked(m_DiscreteStatus1 & (1 << 1));
    ui->checkBoxLowOilPressure->setChecked(m_DiscreteStatus1 & (1 << 2));
    ui->checkBoxCheckTransmission->setChecked(m_DiscreteStatus1 & (1 << 0));
    ui->checkBoxLowOilLevel->setChecked(m_DiscreteStatus1 & (1 << 3));
    ui->checkBoxSailDrive->setChecked(m_DiscreteStatus1 & (1 << 4));

    emit DiscreteStatus1Changed();
}

// INFO: DiscreteStatus2 not implemented in NMEA2000 library
unsigned char TransmissionDynamic127493::DiscreteStatus2() const
{
    return m_DiscreteStatus2;
}

void TransmissionDynamic127493::setDiscreteStatus2(unsigned char newDiscreteStatus2)
{
    if (m_DiscreteStatus2 == newDiscreteStatus2)
        return;
    m_DiscreteStatus2 = newDiscreteStatus2;

    ui->checkBoxSlipConditionDetected->setChecked(m_DiscreteStatus2 & (1 << 0));
    ui->checkBoxClutchOverload->setChecked(m_DiscreteStatus2 & (1 << 1));
    ui->checkBoxPowerTakeOffEngaged->setChecked(m_DiscreteStatus2 & (1 << 2));

    emit DiscreteStatus2Changed();
}

void TransmissionDynamic127493::setInitialGuiValues()
{
    ui->lineEditOilPressure->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(m_OilPressure, UserPrefs()), 'f', 1));
    ui->lineEditOilTemperature->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_OilTemperature, UserPrefs()), 'f', 2));
    setTransmissionGear(TransmissionGear());
}

void TransmissionDynamic127493::on_radioButtonReverse_clicked()
{
    setTransmissionGear(N2kTG_Reverse);
}

void TransmissionDynamic127493::on_radioButtonNeutral_clicked()
{
    setTransmissionGear(N2kTG_Neutral);
}

void TransmissionDynamic127493::on_radioButtonForward_clicked()
{
    setTransmissionGear(N2kTG_Forward);
}

void TransmissionDynamic127493::on_lineEditOilPressure_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditOilPressure, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "oilPressureChanged";
    onTimerTimeout();
}

void TransmissionDynamic127493::on_lineEditOilTemperature_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditOilTemperature, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "oilTemperatureChanged";
    onTimerTimeout();
}

void TransmissionDynamic127493::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void TransmissionDynamic127493::on_checkBoxOverTemperature_clicked(bool checked)
{
    const unsigned char mask = 1u << 1;
    setDiscreteStatus1(checked ? m_DiscreteStatus1 | mask : m_DiscreteStatus1 & ~mask);
}

void TransmissionDynamic127493::on_checkBoxLowOilPressure_clicked(bool checked)
{
    const unsigned char mask = 1u << 2;
    setDiscreteStatus1(checked ? m_DiscreteStatus1 | mask : m_DiscreteStatus1 & ~mask);
}

void TransmissionDynamic127493::on_checkBoxCheckTransmission_clicked(bool checked)
{
    const unsigned char mask = 1u << 0;
    setDiscreteStatus1(checked ? m_DiscreteStatus1 | mask : m_DiscreteStatus1 & ~mask);
}

void TransmissionDynamic127493::on_checkBoxLowOilLevel_clicked(bool checked)
{
    const unsigned char mask = 1u << 3;
    setDiscreteStatus1(checked ? m_DiscreteStatus1 | mask : m_DiscreteStatus1 & ~mask);
}

void TransmissionDynamic127493::on_checkBoxSailDrive_clicked(bool checked)
{
    const unsigned char mask = 1u << 4;
    setDiscreteStatus1(checked ? m_DiscreteStatus1 | mask : m_DiscreteStatus1 & ~mask);
}

void TransmissionDynamic127493::on_checkBoxSlipConditionDetected_clicked(bool checked)
{
    if (checked) {
        m_DiscreteStatus2 |= (1 << 0); // Set bit 0
    } else {
        m_DiscreteStatus2 &= ~(1 << 0); // Clear bit 0
    }
    emit DiscreteStatus2Changed();
}

void TransmissionDynamic127493::on_checkBoxPowerTakeOffEngaged_clicked(bool checked)
{
    if (checked) {
        m_DiscreteStatus2 |= (1 << 2); // Set bit 2
    } else {
        m_DiscreteStatus2 &= ~(1 << 2); // Clear bit 2
    }
    emit DiscreteStatus2Changed();
}

void TransmissionDynamic127493::on_checkBoxClutchOverload_clicked(bool checked)
{
    if (checked) {
        m_DiscreteStatus2 |= (1 << 1); // Set bit 1
    } else {
        m_DiscreteStatus2 &= ~(1 << 1); // Clear bit 1
    }
    emit DiscreteStatus2Changed();
}

