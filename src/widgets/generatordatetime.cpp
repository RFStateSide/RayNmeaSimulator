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

#include "generatordatetime.h"
#include "ui_generatordatetime.h"

#include <QDateTime>
#include "n2khelper.h"

GeneratorDateTime::GeneratorDateTime(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::GeneratorDateTime)
{
    ui->setupUi(this);

    setId(id);
    setPgn(-129033);
    ui->labelTitle->setText("Generate " + label);
    setColumnSpan(3);
    setRowSpan(4);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    // Start the poll timer for simulating the system time
    pollTimer = new QTimer(this);
    pollTimer->start(RefreshRate());
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(PollTimerElapsed()));

    initializeUnitsLimits();
    connect(this, &GeneratorDateTime::UserPrefsChanged, this, &GeneratorDateTime::initializeUnitsLimits);
    setInitialGuiValues();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    //ui->pushButtonRemove->setIcon(iconExit);
}

GeneratorDateTime::~GeneratorDateTime()
{
    pollTimer->stop();
    delete ui;
}

void GeneratorDateTime::initializeUnitsLimits()
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

uchar GeneratorDateTime::SID() const
{
    return m_SID;
}

void GeneratorDateTime::setSID(uchar newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

uint16_t GeneratorDateTime::DaysSince1970() const
{
    return m_DaysSince1970;
}

void GeneratorDateTime::setDaysSince1970(uint16_t newDaysSince1970)
{
    //if (m_DaysSince1970 == newDaysSince1970)
    //    return;
    m_DaysSince1970 = newDaysSince1970;
    setInitialGuiValues();
    emit DaysSince1970Changed();
}

double GeneratorDateTime::SecondsSinceMidnight() const
{
    return m_SecondsSinceMidnight;
}

void GeneratorDateTime::setSecondsSinceMidnight(double newSecondsSinceMidnight)
{
    if (qFuzzyCompare(m_SecondsSinceMidnight, newSecondsSinceMidnight))
        return;
    m_SecondsSinceMidnight = newSecondsSinceMidnight;
    setInitialGuiValues();
    emit SecondsSinceMidnightChanged();
}

int16_t GeneratorDateTime::LocalOffset() const
{
    return m_LocalOffset;
}

void GeneratorDateTime::setLocalOffset(int16_t newLocalOffset)
{
    // Emit the signal always to update newly added widgets
    //if (m_LocalOffset == newLocalOffset)
    //    return;
    m_LocalOffset = newLocalOffset;
    setInitialGuiValues();
    emit LocalOffsetChanged();
}

int GeneratorDateTime::RefreshRate() const
{
    return m_RefreshRate;
}

void GeneratorDateTime::setRefreshRate(int newRefreshRate)
{
    if (m_RefreshRate == newRefreshRate)
        return;
    m_RefreshRate = newRefreshRate;
    emit RefreshRateChanged();
}

void GeneratorDateTime::PollTimerElapsed()
{
    m_PollCounter++;
    if (m_PollCounter > 255)
        m_PollCounter = 0;
    setSID(m_PollCounter);

    // Get the current date and time in UTC
    QDateTime dateTime = QDateTime::currentDateTimeUtc();
    QDate date = dateTime.date();
    QTime time = dateTime.time();

    // Calculate DaysSince1970
    QDate epochStart(1970, 1, 1);
    uint16_t daysSince1970 = static_cast<uint16_t>(epochStart.daysTo(date));

    // Calculate SecondsSinceMidnight in 0.0001 s resolution
    double secondsSinceMidnight = (time.hour() * 3600 + time.minute() * 60 + time.second() + time.msec() / 1000.0);
    //double millisecondsSinceMidnight = (time.hour() * 3600 + time.minute() * 60 + time.second() + time.msec() / 1000.0) * 1000;

    // Clock values are UTC; LocalOffset is separately configured metadata.
    // Do not overwrite a restored or Property Editor offset with the host timezone.
    setDaysSince1970(daysSince1970);
    setSecondsSinceMidnight(secondsSinceMidnight);
    emit LocalOffsetChanged(); // Deliver the configured offset to newly added instruments.

    // Calculate UnixTime
    time_t unixTime = static_cast<time_t>(dateTime.toSecsSinceEpoch());
    setUnixTime(unixTime);
    emit parametersSampled({"SID","DaysSince1970","SecondsSinceMidnight","LocalOffset"});
}

time_t GeneratorDateTime::UnixTime() const
{
    return m_UnixTime;
}

void GeneratorDateTime::setUnixTime(time_t newUnixTime)
{
    if (m_UnixTime == newUnixTime)
        return;
    m_UnixTime = newUnixTime;
    emit UnixTimeChanged();
}

void GeneratorDateTime::setInitialGuiValues()
{
    // The generator shows UTC; PGN 129033 instruments show the configured local time.
    ui->lineEditDaysSince1970->setText(N2kHelper::convertN2kDateToUserFormat(DaysSince1970(), UserPrefs()));
    ui->lineEditSecondsSinceMidnight->setText(N2kHelper::convertN2kTimeToUserFormat(SecondsSinceMidnight(), UserPrefs()));
    ui->lineEditLocalOffset->setText(QString::number(LocalOffset() / 60.0));
}
