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

#include "speed128259.h"
#include "ui_speed128259.h"

#include "n2khelper.h"

Speed128259::Speed128259(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::Speed128259)
{
    ui->setupUi(this);
    setId(id);
    setPgn(128259);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (128259)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

Speed128259::~Speed128259()
{
    delete ui;
}

void Speed128259::initializeUnitsLimits()
{
    // "Knots", "km/h", "MPH range"
    switch (UserPrefs().speedUnitIndex) {
    case 0:
        groundReferencedValidator->setRange(0, 90.0, 1);
        waterReferencedValidator->setRange(0, 90.0, 1);
        ui->labelUnitsGroundReferenced->setText("m/s");
        ui->labelUnitsWaterReferenced->setText("m/s");
        break;
    case 3:
        groundReferencedValidator->setRange(0, 173.7, 1);
        waterReferencedValidator->setRange(0, 173.7, 1);
        ui->labelUnitsGroundReferenced->setText("knots");
        ui->labelUnitsWaterReferenced->setText("knots");
        break;
    case 1:
        groundReferencedValidator->setRange(0, 321.7, 1);
        waterReferencedValidator->setRange(0, 321.7, 1);
        ui->labelUnitsGroundReferenced->setText("km/h");
        ui->labelUnitsWaterReferenced->setText("km/h");
        break;
    case 2:
        groundReferencedValidator->setRange(0, 199.9, 1);
        waterReferencedValidator->setRange(0, 199.9, 1);
        ui->labelUnitsGroundReferenced->setText("MPH");
        ui->labelUnitsWaterReferenced->setText("MPH");
        break;
    }

    ui->lineEditGroundReferenced->setValidator(groundReferencedValidator);
    ui->lineEditWaterReferenced->setValidator(waterReferencedValidator);
}

void Speed128259::initializeClickableLabels()
{
    connect(this, &Speed128259::UserPrefsChanged, this, &Speed128259::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsGroundReferenced->setMenuItems(unitsListSpeed);
    connect(ui->labelUnitsGroundReferenced, &ClickableLabel::actionTriggered, this, &Speed128259::ChangeSpeedUnits_triggered);
    ui->labelUnitsWaterReferenced->setMenuItems(unitsListSpeed);
    connect(ui->labelUnitsWaterReferenced, &ClickableLabel::actionTriggered, this, &Speed128259::ChangeSpeedUnits_triggered);
}

void Speed128259::onTimerTimeout()
{
    double convertedValue = N2kHelper::convertUserSpeedToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());

    if (m_SignalToEmit == "WaterReferenced") {
        m_WaterReferenced = convertedValue;
        ui->lineEditWaterReferenced->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_WaterReferenced, UserPrefs()), 'f', 1));
        emit WaterReferencedChanged();
    } else if (m_SignalToEmit == "GroundReferenced") {
        m_GroundReferenced = convertedValue;
        ui->lineEditGroundReferenced->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_GroundReferenced, UserPrefs()), 'f', 1));
        emit GroundReferencedChanged();
    }
    m_SignalToEmit.clear();
}

unsigned char Speed128259::SID() const
{
    return m_SID;
}

void Speed128259::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double Speed128259::WaterReferenced() const
{
    return m_WaterReferenced;
}

void Speed128259::setWaterReferenced(double newWaterReferenced)
{
    if (qFuzzyCompare(m_WaterReferenced, newWaterReferenced))
        return;
    m_WaterReferenced = newWaterReferenced;
    ui->lineEditWaterReferenced->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(newWaterReferenced, UserPrefs()), 'f', 1));
    emit WaterReferencedChanged();
}

double Speed128259::GroundReferenced() const
{
    return m_GroundReferenced;
}

void Speed128259::setGroundReferenced(double newGroundReferenced)
{
    if (qFuzzyCompare(m_GroundReferenced, newGroundReferenced))
        return;
    m_GroundReferenced = newGroundReferenced;
    ui->lineEditGroundReferenced->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(newGroundReferenced, UserPrefs()), 'f', 1));
    emit GroundReferencedChanged();
}

tN2kSpeedWaterReferenceType Speed128259::SWRT() const
{
    return m_SWRT;
}

void Speed128259::setSWRT(const tN2kSpeedWaterReferenceType &newSWRT)
{
    if (m_SWRT == newSWRT)
        return;
    m_SWRT = newSWRT;
    emit SWRTChanged();
}

void Speed128259::setInitialGuiValues()
{
    ui->lineEditWaterReferenced->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_WaterReferenced, UserPrefs()), 'f', 1));
    ui->lineEditGroundReferenced->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_GroundReferenced, UserPrefs()), 'f', 1));
}

void Speed128259::on_lineEditWaterReferenced_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditWaterReferenced, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "WaterReferenced";
    onTimerTimeout();
}

void Speed128259::on_lineEditGroundReferenced_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditGroundReferenced, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "GroundReferenced";
    onTimerTimeout();
}

void Speed128259::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

