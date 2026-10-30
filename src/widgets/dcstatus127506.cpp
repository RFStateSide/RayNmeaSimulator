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

#include <QValidator>
#include "dcstatus127506.h"
#include "ui_dcstatus127506.h"

DcStatus127506::DcStatus127506(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::DcStatus127506)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127506);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127506)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();
    setInitialGuiValues();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

DcStatus127506::~DcStatus127506()
{
    delete ui;
}

void DcStatus127506::initializeUnitsLimits()
{
    m_doubleValidator->setRange(0, 100, 0);
    ui->lineEdit->setValidator(m_doubleValidator);
    ui->lineEdit->setToolTip("State of charge (%). Other DC details are available in the Property Editor.");
}

void DcStatus127506::initializeClickableLabels() {}

void DcStatus127506::onTimerTimeout()
{
    if (m_SignalToEmit == "StateOfChargeChanged") {
        m_StateOfCharge = m_ValueToEmit.toUInt();
        emit StateOfChargeChanged();
    } else if (m_SignalToEmit == "TimeRemainingChanged") {
        m_TimeRemaining = m_ValueToEmit.toDouble();
        emit TimeRemainingChanged();
    } else if (m_SignalToEmit == "RippleVoltageChanged") {
        m_RippleVoltage = m_ValueToEmit.toDouble();
        emit RippleVoltageChanged();
    } else if (m_SignalToEmit == "CapacityChanged") {
        m_Capacity = m_ValueToEmit.toDouble();
        emit CapacityChanged();
    } else if (m_SignalToEmit == "StateOfHealthChanged") {
        m_StateOfHealth = m_ValueToEmit.toUInt();
        emit StateOfHealthChanged();
    }
}

unsigned char DcStatus127506::SID() const
{
    return m_SID;
}

void DcStatus127506::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

unsigned char DcStatus127506::DCInstance() const
{
    return m_DCInstance;
}

void DcStatus127506::setDCInstance(unsigned char newDCInstance)
{
    if (m_DCInstance == newDCInstance)
        return;
    m_DCInstance = newDCInstance;
    emit DCInstanceChanged();
}

tN2kDCType DcStatus127506::DCType() const
{
    return m_DCType;
}

void DcStatus127506::setDCType(const tN2kDCType &newDCType)
{
    const bool changed = m_DCType != newDCType;
    m_DCType = newDCType;

    if (m_DCType == N2kDCt_Battery)
        ui->labelSource->setText("Battery");
    else if (m_DCType == N2kDCt_Alternator)
        ui->labelSource->setText("Alternator");
    else if (m_DCType == N2kDCt_Converter)
        ui->labelSource->setText("Converter");
    else if (m_DCType == N2kDCt_SolarCell)
        ui->labelSource->setText("Solar Cell");
    else if (m_DCType == N2kDCt_WindGenerator)
        ui->labelSource->setText("Wind Generator");
    else
        ui->labelSource->setText("Unknown");

    if (changed) emit DCTypeChanged();
}

uint8_t DcStatus127506::StateOfCharge() const
{
    return m_StateOfCharge;
}

void DcStatus127506::setStateOfCharge(uint8_t newStateOfCharge)
{
    if (m_StateOfCharge == newStateOfCharge)
        return;
    m_StateOfCharge = newStateOfCharge;

    ui->lineEdit->setText(m_StateOfCharge <= 100 ? QString::number(m_StateOfCharge) : "Unavailable");
    emit StateOfChargeChanged();
}

double DcStatus127506::TimeRemaining() const
{
    return m_TimeRemaining;
}

void DcStatus127506::setTimeRemaining(double newTimeRemaining)
{
    if (qFuzzyCompare(m_TimeRemaining, newTimeRemaining))
        return;
    m_TimeRemaining = newTimeRemaining;
    emit TimeRemainingChanged();
}

double DcStatus127506::RippleVoltage() const
{
    return m_RippleVoltage;
}

void DcStatus127506::setRippleVoltage(double newRippleVoltage)
{
    if (qFuzzyCompare(m_RippleVoltage, newRippleVoltage))
        return;
    m_RippleVoltage = newRippleVoltage;
    emit RippleVoltageChanged();
}

double DcStatus127506::Capacity() const
{
    return m_Capacity;
}

void DcStatus127506::setCapacity(double newCapacity)
{
    if (qFuzzyCompare(m_Capacity, newCapacity))
        return;
    m_Capacity = newCapacity;
    emit CapacityChanged();
}

uint8_t DcStatus127506::StateOfHealth() const
{
    return m_StateOfHealth;
}

void DcStatus127506::setStateOfHealth(uint8_t newStateOfHealth)
{
    if (m_StateOfHealth == newStateOfHealth)
        return;
    m_StateOfHealth = newStateOfHealth;
    emit StateOfHealthChanged();
}

void DcStatus127506::on_lineEdit_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEdit, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "StateOfChargeChanged";
    onTimerTimeout();
}

void DcStatus127506::on_checkBoxEnabled_clicked(bool checked)
{
    this->setEnabled(checked);
}


void DcStatus127506::setInitialGuiValues()
{
    setDCType(m_DCType); // Refresh even when the initial enum equals the default.
    ui->lineEdit->setText(m_StateOfCharge <= 100 ? QString::number(m_StateOfCharge) : "Unavailable");
}
