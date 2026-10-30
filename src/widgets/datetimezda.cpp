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

#include "datetimezda.h"
#include "ui_datetimezda.h"

#include "n2khelper.h"

DateTimeZDA::DateTimeZDA(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::DateTimeZDA)
{
    ui->setupUi(this);

    setId(id);
    setPgn(SNT_ZDA);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (ZDA)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

DateTimeZDA::~DateTimeZDA()
{
    delete ui;
}

void DateTimeZDA::setInitialGuiValues()
{
    ui->lineEditDaysSince1970->setText(FormateDate(m_GPSDay, m_GPSMonth, m_GPSYear));
    ui->lineEditSecondsSinceMidnight->setText(N2kHelper::convertN2kTimeToUserFormat(m_GPSTime, UserPrefs()));
}

void DateTimeZDA::initializeUnitsLimits() {}

void DateTimeZDA::initializeClickableLabels() {}

void DateTimeZDA::onTimerTimeout()
{
    if (m_SignalToEmit == "DaysSince1970Changed") {
        switch (UserPrefs().dateUnitIndex) {
        case 0: { // MM/DD/YYYY
            QStringList dateParts = m_ValueToEmit.split("/");
            setGPSMonth(dateParts[0].toInt());
            emit GPSMonthChanged();
            setGPSDay(dateParts[1].toInt());
            emit GPSDayChanged();
            setGPSYear(dateParts[2].toInt());
            emit GPSYearChanged();
            break;
        }
        case 1: { // DD/MM/YYYY
            QStringList dateParts = m_ValueToEmit.split("/");
            setGPSDay(dateParts[0].toInt());
            emit GPSDayChanged();
            setGPSMonth(dateParts[1].toInt());
            emit GPSMonthChanged();
            setGPSYear(dateParts[2].toInt());
            emit GPSYearChanged();
            break;
        }
        case 2: { // YYYY/MM/DD
            QStringList dateParts = m_ValueToEmit.split("/");
            setGPSYear(dateParts[0].toInt());
            emit GPSYearChanged();
            setGPSMonth(dateParts[1].toInt());
            emit GPSMonthChanged();
            setGPSDay(dateParts[2].toInt());
            emit GPSDayChanged();
            break;
        }
        }
    } else if (m_SignalToEmit == "SecondsSinceMidnightChanged") {
        const double secondsSinceMidnight = N2kHelper::convertUserFormattedTimeToN2k(m_ValueToEmit, UserPrefs());
        if (secondsSinceMidnight >= 0) setGPSTime(secondsSinceMidnight);
    }
}

double DateTimeZDA::GPSTime() const
{
    return m_GPSTime;
}

void DateTimeZDA::setGPSTime(double newGPSTime)
{
    if (qFuzzyCompare(m_GPSTime, newGPSTime))
        return;
    m_GPSTime = newGPSTime;
    ui->lineEditSecondsSinceMidnight->setText(N2kHelper::convertN2kTimeToUserFormat(newGPSTime, UserPrefs()));
    emit GPSTimeChanged();
}

int DateTimeZDA::GPSDay() const
{
    return m_GPSDay;
}

void DateTimeZDA::setGPSDay(int newGPSDay)
{
    if (m_GPSDay == newGPSDay)
        return;
    m_GPSDay = newGPSDay;
    ui->lineEditDaysSince1970->setText(FormateDate(newGPSDay, m_GPSMonth, m_GPSYear));
    emit GPSDayChanged();
}

int DateTimeZDA::GPSMonth() const
{
    return m_GPSMonth;
}

void DateTimeZDA::setGPSMonth(int newGPSMonth)
{
    if (m_GPSMonth == newGPSMonth)
        return;
    m_GPSMonth = newGPSMonth;
    ui->lineEditDaysSince1970->setText(FormateDate(m_GPSDay, newGPSMonth, m_GPSYear));
    emit GPSMonthChanged();
}

int DateTimeZDA::GPSYear() const
{
    return m_GPSYear;
}

void DateTimeZDA::setGPSYear(int newGPSYear)
{
    if (m_GPSYear == newGPSYear)
        return;
    m_GPSYear = newGPSYear;
    ui->lineEditDaysSince1970->setText(FormateDate(m_GPSDay, m_GPSMonth, newGPSYear));
    emit GPSYearChanged();
}

int DateTimeZDA::LZD() const
{
    return m_LZD;
}

void DateTimeZDA::setLZD(int newLZD)
{
    if (m_LZD == newLZD)
        return;
    m_LZD = newLZD;
    emit LZDChanged();
}

int DateTimeZDA::LZMD() const
{
    return m_LZMD;
}

void DateTimeZDA::setLZMD(int newLZMD)
{
    if (m_LZMD == newLZMD)
        return;
    m_LZMD = newLZMD;
    emit LZMDChanged();
}

time_t DateTimeZDA::UnixTime() const
{
    return m_UnixTime;
}

void DateTimeZDA::setUnixTime(time_t newUnixTime)
{
    if (m_UnixTime == newUnixTime)
        return;
    m_UnixTime = newUnixTime;
    const QDateTime utc = QDateTime::fromSecsSinceEpoch(newUnixTime, QTimeZone::utc());
    const double secondsSinceMidnight = QTime(0, 0).secsTo(utc.time());
    auto [gpsDay, gpsMonth, gpsYear] = extractGPSDateFromTimeT(newUnixTime);
    setGPSTime(secondsSinceMidnight);
    setGPSDay(gpsDay);
    setGPSMonth(gpsMonth);
    setGPSYear(gpsYear);
    emit UnixTimeChanged();
}

void DateTimeZDA::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void DateTimeZDA::on_lineEditDaysSince1970_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditDaysSince1970, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "DaysSince1970Changed";
    onTimerTimeout();
}

void DateTimeZDA::on_lineEditSecondsSinceMidnight_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditSecondsSinceMidnight, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "SecondsSinceMidnightChanged";
    onTimerTimeout();
}

QString DateTimeZDA::FormateDate(int day, int month, int year)
{
    // ZDA supplies separate calendar fields, rather than an N2k epoch-day count.
    if (year < 1 || year > 9999 || !QDate(year, month, day).isValid())
        return QStringLiteral("Unavailable");
    switch (UserPrefs().dateUnitIndex) {
    case 0: // MM/DD/YYYY
        return QString("%1/%2/%3").arg(month, 2, 10, QChar('0')).arg(day, 2, 10, QChar('0')).arg(year);
    case 1: // DD/MM/YYYY
        return QString("%1/%2/%3").arg(day, 2, 10, QChar('0')).arg(month, 2, 10, QChar('0')).arg(year);
    case 2: // YYYY/MM/DD
        return QString("%1/%2/%3").arg(year).arg(month, 2, 10, QChar('0')).arg(day, 2, 10, QChar('0'));
    default:
        return QString("%1/%2/%3").arg(day, 2, 10, QChar('0')).arg(month, 2, 10, QChar('0')).arg(year);
    }
}

std::tuple<int, int, int> DateTimeZDA::extractGPSDateFromTimeT(time_t timestamp)
{
    // Convert time_t to QDateTime
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC);

    // Extract Day, Month, and Year
    int GPSDay = dateTime.date().day();
    int GPSMonth = dateTime.date().month();
    int GPSYear = dateTime.date().year();

    return std::make_tuple(GPSDay, GPSMonth, GPSYear);
}

