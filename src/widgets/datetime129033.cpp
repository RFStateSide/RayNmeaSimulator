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

#include "datetime129033.h"
#include "ui_datetime129033.h"

#include "n2khelper.h"
#include "N2kMsg.h"
#include <QTimeZone>
#include <cmath>

// TODO: Refactor inherite PgnWidgetBase class to avoid code duplication
DateTime129033::DateTime129033(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::DateTime129033)
{
    ui->setupUi(this);

    setId(id);
    setPgn(129033);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (129033)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    connect(this, &DateTime129033::UserPrefsChanged, this, &DateTime129033::initializeUnitsLimits);
    setInitialGuiValues();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

DateTime129033::~DateTime129033()
{
    delete ui;
}


void DateTime129033::updateDaysSince1970(uint16_t newDaysSince1970)
{
    setDaysSince1970(newDaysSince1970);
}

void DateTime129033::updateSecondsSinceMidnight(double newSecondsSinceMidnight)
{
    setSecondsSinceMidnight(newSecondsSinceMidnight);
}

void DateTime129033::updateLocalOffset(int16_t newLocalOffset)
{
    setLocalOffset(newLocalOffset);
}


void DateTime129033::initializeUnitsLimits()
{
    switch (UserPrefs().dateUnitIndex) {
    case 0:
        ui->labelUnitsDaysSince1970->setText("mm/dd/yyyy");
        break;
    case 1:
        ui->labelUnitsDaysSince1970->setText("dd/mm/yyyy");
        break;
    case 2:
        ui->labelUnitsDaysSince1970->setText("yyyy/mm/dd");
        break;
    }

    switch (UserPrefs().timeUnitIndex) {
    case 0:
        ui->labelUnitsSecondsSinceMidnight->setText("hh:mm");
        break;
    case 1:
        ui->labelUnitsSecondsSinceMidnight->setText("hh:mm AP");
        break;
    case 2:
        ui->labelUnitsSecondsSinceMidnight->setText("hh:mm:ss");
        break;
    case 3:
        ui->labelUnitsSecondsSinceMidnight->setText("hh:mm:ss AP");
        break;
    }
}

void DateTime129033::onTimerTimeout()
{
    // No user input, read only, no need to emit signals
}

uint16_t DateTime129033::DaysSince1970() const
{
    return m_DaysSince1970;
}

void DateTime129033::setDaysSince1970(uint16_t newDaysSince1970)
{
    if (m_DaysSince1970 == newDaysSince1970)
        return;
    m_DaysSince1970 = newDaysSince1970;
    setInitialGuiValues();
    emit DaysSince1970Changed();
}

double DateTime129033::SecondsSinceMidnight() const
{
    return m_SecondsSinceMidnight;
}

void DateTime129033::setSecondsSinceMidnight(double newSecondsSinceMidnight)
{
    if (qFuzzyCompare(m_SecondsSinceMidnight, newSecondsSinceMidnight))
        return;
    m_SecondsSinceMidnight = newSecondsSinceMidnight;
    setInitialGuiValues();
    emit SecondsSinceMidnightChanged();
}

int16_t DateTime129033::LocalOffset() const
{
    return m_LocalOffset;
}

void DateTime129033::setLocalOffset(int16_t newLocalOffset)
{
    if (m_LocalOffset == newLocalOffset)
        return;
    m_LocalOffset = newLocalOffset;
    setInitialGuiValues();
    emit LocalOffsetChanged();
}

void DateTime129033::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}


void DateTime129033::setInitialGuiValues()
{
    // The pinned parser preserves integer NA/error/reserved codes, and maps
    // unavailable floating fields to N2kDoubleNA. Validate each field independently.
    const bool dateValid = DaysSince1970() < N2kUInt16NA - 2;
    const bool timeValid = std::isfinite(SecondsSinceMidnight())
        && SecondsSinceMidnight() >= 0 && SecondsSinceMidnight() < 86401;
    const bool offsetValid = LocalOffset() < N2kInt16NA - 2;
    const bool leapSecond = timeValid && SecondsSinceMidnight() >= 86400;
    const QDate utcDate = QDate(1970, 1, 1).addDays(dateValid ? DaysSince1970() : 0);
    // QTime cannot represent :60. Use the preceding second for calendar/offset
    // arithmetic, then format the leap-second suffix explicitly below.
    const double displaySeconds = timeValid ? (leapSecond ? 86399 : SecondsSinceMidnight()) : 0;
    const auto local = QDateTime(utcDate, QTime(0, 0), QTimeZone::utc())
        .addMSecs(static_cast<qint64>(displaySeconds * 1000))
        .addSecs(offsetValid ? int(LocalOffset()) * 60 : 0);
    const auto dateFormat = QStringList{"MM/dd/yyyy", "dd/MM/yyyy", "yyyy/MM/dd"}
        .value(UserPrefs().dateUnitIndex, "MM/dd/yyyy");
    auto timeFormat = QStringList{"HH:mm", "hh:mm AP", "HH:mm:ss", "hh:mm:ss AP"}
        .value(UserPrefs().timeUnitIndex, "HH:mm:ss");
    if (leapSecond) timeFormat.replace("ss", "'60'");

    // With no usable offset display the transmitted UTC value, explicitly marked.
    // With no time, a valid UTC date is still useful; do not invent local rollover.
    const bool dateUtc = !offsetValid || !timeValid;
    initializeUnitsLimits();
    if (dateUtc) ui->labelUnitsDaysSince1970->setText(ui->labelUnitsDaysSince1970->text() + " (UTC)");
    if (!offsetValid) ui->labelUnitsSecondsSinceMidnight->setText(ui->labelUnitsSecondsSinceMidnight->text() + " (UTC)");
    ui->lineEditDaysSince1970->setText(!dateValid ? tr("Unavailable")
        : (dateUtc ? utcDate : local.date()).toString(dateFormat));
    ui->lineEditSecondsSinceMidnight->setText(timeValid ? local.time().toString(timeFormat) : tr("Unavailable"));
    ui->lineEditDaysSince1970->setToolTip(dateUtc
        ? tr("UTC date. Local rollover cannot be determined without valid time and offset.")
        : tr("Local date using the transmitted UTC offset (%1 min).").arg(LocalOffset()));
    ui->lineEditSecondsSinceMidnight->setToolTip(!offsetValid
        ? tr("UTC time; the transmitted local offset is unavailable or invalid.")
        : tr("Local time using the transmitted UTC offset (%1 min).").arg(LocalOffset()));
}
