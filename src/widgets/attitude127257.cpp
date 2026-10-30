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

#include "attitude127257.h"
#include "qstyle.h"
#include "ui_attitude127257.h"

#include "n2khelper.h"

Attitude127257::Attitude127257(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::Attitude127257)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127257);
    setColumnSpan(3);
    setRowSpan(4);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127257)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

Attitude127257::~Attitude127257()
{
    delete ui;
}

void Attitude127257::initializeUnitsLimits()
{
    degValidator = new QDoubleValidator(-90, 90, 1, this);
    ui->lineEditYaw->setValidator(degValidator);
    ui->lineEditPitch->setValidator(degValidator);
    ui->lineEditRoll->setValidator(degValidator);
}

void Attitude127257::onTimerTimeout()
{
    if (m_SignalToEmit == "YawChanged") {
        m_Yaw = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        emit YawChanged();
    } else if (m_SignalToEmit == "PitchChanged") {
        m_Pitch = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        emit PitchChanged();
    } else if (m_SignalToEmit == "RollChanged") {
        m_Roll = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        emit RollChanged();
    }

    m_SignalToEmit.clear();
}

void Attitude127257::setInitialGuiValues()
{
    ui->lineEditYaw->setText(QString::number(Convert::RadiansToDegrees(m_Yaw), 'f', 1));
    ui->lineEditPitch->setText(QString::number(Convert::RadiansToDegrees(m_Pitch), 'f', 1));
    ui->lineEditRoll->setText(QString::number(Convert::RadiansToDegrees(m_Roll), 'f', 1));
}

unsigned char Attitude127257::SID() const
{
    return m_SID;
}

void Attitude127257::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double Attitude127257::Yaw() const
{
    return m_Yaw;
}

void Attitude127257::setYaw(double newYaw)
{
    if (qFuzzyCompare(m_Yaw, newYaw))
        return;
    m_Yaw = newYaw;
    ui->lineEditYaw->setText(QString::number(Convert::RadiansToDegrees(newYaw), 'f', 1));
    emit YawChanged();
}

double Attitude127257::Pitch() const
{
    return m_Pitch;
}

void Attitude127257::setPitch(double newPitch)
{
    if (qFuzzyCompare(m_Pitch, newPitch))
        return;
    m_Pitch = newPitch;
    ui->lineEditPitch->setText(QString::number(Convert::RadiansToDegrees(newPitch), 'f', 1));
    emit PitchChanged();
}

double Attitude127257::Roll() const
{
    return m_Roll;
}

void Attitude127257::setRoll(double newRoll)
{
    if (qFuzzyCompare(m_Roll, newRoll))
        return;
    m_Roll = newRoll;
    ui->lineEditRoll->setText(QString::number(Convert::RadiansToDegrees(newRoll), 'f', 1));
    emit RollChanged();
}

void Attitude127257::on_lineEditYaw_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditYaw, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "YawChanged";
    onTimerTimeout();
}

void Attitude127257::on_lineEditPitch_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditPitch, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "PitchChanged";
    onTimerTimeout();
}

void Attitude127257::on_lineEditRoll_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditRoll, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "RollChanged";
    onTimerTimeout();
}

void Attitude127257::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

