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

#include "batterystatus127508.h"
#include "ui_batterystatus127508.h"

#include <n2khelper.h>
#include "instrumentdisplay.h"

BatteryStatus127508::BatteryStatus127508(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::BatteryStatus127508)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127508);
    setColumnSpan(3);
    setRowSpan(4);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127508)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();
    setInitialGuiValues();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

BatteryStatus127508::~BatteryStatus127508()
{
    delete ui;
}

void BatteryStatus127508::setInitialGuiValues()
{
    ui->lineEditBatteryVoltage->setText(InstrumentDisplay::number(m_BatteryVoltage, m_BatteryVoltage));
    ui->lineEditBatteryCurrent->setText(InstrumentDisplay::number(m_BatteryCurrent, m_BatteryCurrent));
    ui->lineEditBatteryTemperature->setText(InstrumentDisplay::number(N2kHelper::convertN2kTemperatureToUserUnits(m_BatteryTemperature, UserPrefs()), m_BatteryTemperature));
}

void BatteryStatus127508::initializeUnitsLimits()
{
    ui->lineEditBatteryVoltage->setValidator(validatorVoltage);
    ui->lineEditBatteryCurrent->setValidator(validatorCurrent);
    ui->lineEditBatteryTemperature->setValidator(validatorTemperature);
    // "Kelvin (K)", "Celsius (°C)", "Fahrenheit (°F)"
    switch (UserPrefs().tempUnitIndex) {
    case 0:
        ui->labelUnitsBatteryTemperature->setText("K");
        validatorTemperature->setRange(222.039, 394.261, 1);
        break;
    case 1:
        ui->labelUnitsBatteryTemperature->setText("°C");
        validatorTemperature->setRange(-51.111, 121.11, 1);
        break;
    case 2:
        ui->labelUnitsBatteryTemperature->setText("°F");
        validatorTemperature->setRange(-60.0, 250.0, 1);
    }

    validatorVoltage->setRange(0.0, 96.0, 1);
    validatorCurrent->setRange(-200.0, 200.0, 1);
    ui->labelUnitsBatteryVoltage->setText("Vdc");
    ui->labelUnitsBatteryCurrent->setText("A");
}

void BatteryStatus127508::initializeClickableLabels()
{
    connect(this, &BatteryStatus127508::UserPrefsChanged, this, &BatteryStatus127508::initializeUnitsLimits);

    ui->labelUnitsBatteryTemperature->setMenuItems(unitsListTemp);
    connect(ui->labelUnitsBatteryTemperature, &ClickableLabel::actionTriggered, this, &BatteryStatus127508::ChangeTempUnits_triggered);
}

void BatteryStatus127508::onTimerTimeout()
{
    if (m_SignalToEmit == "BatteryVoltageChanged") {
        m_BatteryVoltage = m_ValueToEmit.toDouble();
        emit BatteryVoltageChanged();
    } else if (m_SignalToEmit == "BatteryCurrentChanged") {
        m_BatteryCurrent = m_ValueToEmit.toDouble();
        emit BatteryCurrentChanged();
    } else if (m_SignalToEmit == "BatteryTemperatureChanged") {
        double convertedValue = N2kHelper::convertUserTemperatureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        m_BatteryTemperature = convertedValue;
        emit BatteryTemperatureChanged();
    }
}

void BatteryStatus127508::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

unsigned char BatteryStatus127508::BatteryInstance() const
{
    return m_BatteryInstance;
}

void BatteryStatus127508::setBatteryInstance(unsigned char newBatteryInstance)
{
    if (m_BatteryInstance == newBatteryInstance)
        return;
    m_BatteryInstance = newBatteryInstance;
    emit BatteryInstanceChanged();
}

double BatteryStatus127508::BatteryVoltage() const
{
    return m_BatteryVoltage;
}

void BatteryStatus127508::setBatteryVoltage(double newBatteryVoltage)
{
    if (qFuzzyCompare(m_BatteryVoltage, newBatteryVoltage))
        return;
    m_BatteryVoltage = newBatteryVoltage;
    ui->lineEditBatteryVoltage->setText(InstrumentDisplay::number(m_BatteryVoltage, m_BatteryVoltage));
    emit BatteryVoltageChanged();
}

double BatteryStatus127508::BatteryCurrent() const
{
    return m_BatteryCurrent;
}

void BatteryStatus127508::setBatteryCurrent(double newBatteryCurrent)
{
    if (qFuzzyCompare(m_BatteryCurrent, newBatteryCurrent))
        return;
    m_BatteryCurrent = newBatteryCurrent;
    ui->lineEditBatteryCurrent->setText(InstrumentDisplay::number(m_BatteryCurrent, m_BatteryCurrent));
    emit BatteryCurrentChanged();
}

double BatteryStatus127508::BatteryTemperature() const
{
    return m_BatteryTemperature;
}

void BatteryStatus127508::setBatteryTemperature(double newBatteryTemperature)
{
    if (qFuzzyCompare(m_BatteryTemperature, newBatteryTemperature))
        return;
    m_BatteryTemperature = newBatteryTemperature;
    double convertedValue = N2kHelper::convertN2kTemperatureToUserUnits(m_BatteryTemperature, UserPrefs());
    ui->lineEditBatteryTemperature->setText(InstrumentDisplay::number(convertedValue, m_BatteryTemperature));
    emit BatteryTemperatureChanged();
}

unsigned char BatteryStatus127508::SID() const
{
    return m_SID;
}

void BatteryStatus127508::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

void BatteryStatus127508::on_lineEditBatteryVoltage_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditBatteryVoltage, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "BatteryVoltageChanged";
    onTimerTimeout();
}

void BatteryStatus127508::on_lineEditBatteryCurrent_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditBatteryCurrent, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "BatteryCurrentChanged";
    onTimerTimeout();
}

void BatteryStatus127508::on_lineEditBatteryTemperature_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditBatteryTemperature, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "BatteryTemperatureChanged";
    onTimerTimeout();
}

