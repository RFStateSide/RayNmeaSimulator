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

#include "chargerstatus127507.h"
#include "ui_chargerstatus127507.h"
#include <QSignalBlocker>

ChargerStatus127507::ChargerStatus127507(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::ChargerStatus127507)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127507);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127507)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeChargeStateComboBox();
    initializeUnitsLimits();
    setInitialGuiValues();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

ChargerStatus127507::~ChargerStatus127507()
{
    delete ui;
}

void ChargerStatus127507::setInitialGuiValues()
{
    const QSignalBlocker battery(ui->spinBoxBatteryInstance), state(ui->comboBoxChargeState);
    ui->spinBoxBatteryInstance->setValue(m_BatteryInstance);
    int index = ui->comboBoxChargeState->findData(int(m_ChargeState));
    if (index < 0) {
        ui->comboBoxChargeState->addItem(QString("Reserved (%1)").arg(int(m_ChargeState)), int(m_ChargeState));
        index = ui->comboBoxChargeState->count() - 1;
    }
    ui->comboBoxChargeState->setCurrentIndex(index);
}

void ChargerStatus127507::initializeUnitsLimits() {}

void ChargerStatus127507::initializeClickableLabels() {}

void ChargerStatus127507::onTimerTimeout() {}

unsigned char ChargerStatus127507::Instance() const
{
    return m_Instance;
}

void ChargerStatus127507::setInstance(unsigned char newInstance)
{
    if (m_Instance == newInstance)
        return;
    m_Instance = newInstance;
    emit InstanceChanged();
}

unsigned char ChargerStatus127507::BatteryInstance() const
{
    return m_BatteryInstance;
}

void ChargerStatus127507::setBatteryInstance(unsigned char newBatteryInstance)
{
    if (m_BatteryInstance == newBatteryInstance)
        return;
    m_BatteryInstance = newBatteryInstance;

    const QSignalBlocker blocker(ui->spinBoxBatteryInstance);
    ui->spinBoxBatteryInstance->setValue(m_BatteryInstance);

    emit BatteryInstanceChanged();
}

tN2kChargeState ChargerStatus127507::ChargeState() const
{
    return m_ChargeState;
}

void ChargerStatus127507::setChargeState(tN2kChargeState newChargeState)
{
    if (m_ChargeState == newChargeState)
        return;
    m_ChargeState = newChargeState;

    setInitialGuiValues();

    emit ChargeStateChanged();
}

tN2kChargerMode ChargerStatus127507::ChargerMode() const
{
    return m_ChargerMode;
}

void ChargerStatus127507::setChargerMode(tN2kChargerMode newChargerMode)
{
    if (m_ChargerMode == newChargerMode)
        return;
    m_ChargerMode = newChargerMode;
    emit ChargerModeChanged();
}

tN2kOnOff ChargerStatus127507::Enabled() const
{
    return m_Enabled;
}

void ChargerStatus127507::setEnabled(tN2kOnOff newEnabled)
{
    if (m_Enabled == newEnabled)
        return;
    m_Enabled = newEnabled;
    emit EnabledChanged();
}

tN2kOnOff ChargerStatus127507::EqualizationPending() const
{
    return m_EqualizationPending;
}

void ChargerStatus127507::setEqualizationPending(tN2kOnOff newEqualizationPending)
{
    if (m_EqualizationPending == newEqualizationPending)
        return;
    m_EqualizationPending = newEqualizationPending;
    emit EqualizationPendingChanged();
}

double ChargerStatus127507::EqualizationTimeRemaining() const
{
    return m_EqualizationTimeRemaining;
}

void ChargerStatus127507::setEqualizationTimeRemaining(double newEqualizationTimeRemaining)
{
    if (qFuzzyCompare(m_EqualizationTimeRemaining, newEqualizationTimeRemaining))
        return;
    m_EqualizationTimeRemaining = newEqualizationTimeRemaining;
    emit EqualizationTimeRemainingChanged();
}

void ChargerStatus127507::on_spinBoxBatteryInstance_valueChanged(int arg1)
{
    setBatteryInstance(arg1);
}

void ChargerStatus127507::on_comboBoxChargeState_currentTextChanged(const QString &arg1)
{
    Q_UNUSED(arg1);
    if (ui->comboBoxChargeState->currentIndex() >= 0)
        setChargeState(static_cast<tN2kChargeState>(ui->comboBoxChargeState->currentData().toInt()));
}

void ChargerStatus127507::on_checkBoxEnabled_clicked(bool checked)
{
    PgnWidgetBase::setEnabled(checked);
}

void ChargerStatus127507::initializeChargeStateComboBox()
{
    const QSignalBlocker blocker(ui->comboBoxChargeState);
    ui->comboBoxChargeState->clear();
    for (int i = 0; i < chargeStates.size(); ++i)
        ui->comboBoxChargeState->addItem(chargeStates[i], i < 10 ? i : i + 4);
}

