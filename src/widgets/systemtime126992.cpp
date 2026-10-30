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

#include "systemtime126992.h"
#include "ui_systemtime126992.h"

#include "QFocusEvent"
#include "n2khelper.h"

// TODO: Refactor inherite PgnWidgetBase class to avoid code duplication
SystemTime126992::SystemTime126992(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::SystemTime126992)
{
    ui->setupUi(this);

    setId(id);
    setPgn(126992);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (126992)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();
    setInitialGuiValues();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

void SystemTime126992::initializeUnitsLimits()
{
    switch (UserPrefs().dateUnitIndex) {
    case 0:
        ui->labelUnitsDate->setText("mm/dd/yyyy");
        break;
    case 1:
        ui->labelUnitsDate->setText("dd/mm/yyyy");
        break;
    case 2:
        ui->labelUnitsDate->setText("yyyy/mm/dd");
        break;
    }

    switch (UserPrefs().timeUnitIndex) {
    case 0:
        ui->labelUnitsTime->setText("hh:mm");
        break;
    case 1:
        ui->labelUnitsTime->setText("hh:mm AP");
        break;
    case 2:
        ui->labelUnitsTime->setText("hh:mm:ss");
        break;
    case 3:
        ui->labelUnitsTime->setText("hh:mm:ss AP");
        break;
    }
}

void SystemTime126992::initializeClickableLabels()
{
    connect(this, &SystemTime126992::UserPrefsChanged, this, &SystemTime126992::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsDate->setMenuItems(unitsListDate);
    connect(ui->labelUnitsDate, &ClickableLabel::actionTriggered, this, &SystemTime126992::ChangeDateUnits_triggered);
    ui->labelUnitsTime->setMenuItems(unitsListTime);
    connect(ui->labelUnitsTime, &ClickableLabel::actionTriggered, this, &SystemTime126992::ChangeTimeUnits_triggered);
}

void SystemTime126992::onTimerTimeout() {}

SystemTime126992::~SystemTime126992()
{
    //pollTimer->stop();
    delete ui;
}

void SystemTime126992::updateSID(unsigned char newSID)
{
    setSID(newSID);
}

void SystemTime126992::updateDaysSince1970(uint16_t newDaysSince1970)
{
    setSystemDate(newDaysSince1970);
}

void SystemTime126992::updateSecondsSinceMidnight(double newSecondsSinceMidnight)
{
    setSystemTime(newSecondsSinceMidnight);
}

unsigned char SystemTime126992::SID() const
{
    return m_SID;
}

void SystemTime126992::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

uint16_t SystemTime126992::SystemDate() const
{
    return m_SystemDate;
}

void SystemTime126992::setSystemDate(uint16_t newSystemDate)
{
    if (m_SystemDate == newSystemDate)
        return;
    m_SystemDate = newSystemDate;
    setInitialGuiValues();
    emit SystemDateChanged();
}

double SystemTime126992::SystemTime() const
{
    return m_SystemTime;
}

void SystemTime126992::setSystemTime(double newSystemTime)
{
    if (qFuzzyCompare(m_SystemTime, newSystemTime))
        return;
    m_SystemTime = newSystemTime;
    setInitialGuiValues();
    emit SystemTimeChanged();
}

tN2kTimeSource SystemTime126992::TimeSource() const
{
    return m_TimeSource;
}

void SystemTime126992::setTimeSource(const tN2kTimeSource &newTimeSource)
{
    if (m_TimeSource == newTimeSource)
        return;
    m_TimeSource = newTimeSource;
    emit TimeSourceChanged();
}

void SystemTime126992::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void SystemTime126992::setInitialGuiValues()
{
    ui->lineEditDate->setText(N2kHelper::convertN2kDateToUserFormat(SystemDate(), UserPrefs()));
    ui->lineEditTime->setText(N2kHelper::convertN2kTimeToUserFormat(SystemTime(), UserPrefs()));
}
