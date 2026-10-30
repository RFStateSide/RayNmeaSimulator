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

#ifndef DATETIMEZDA_H
#define DATETIMEZDA_H

#include <QDoubleValidator>
#include <QDateTime>
#include <tuple>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class DateTimeZDA;
}

class DateTimeZDA : public PgnWidgetBase
{
    Q_OBJECT

    //double &GPSTime, int &GPSDay, int &GPSMonth, int &GPSYear, int &LZD, int &LZMD
    Q_PROPERTY(double GPSTime READ GPSTime WRITE setGPSTime NOTIFY GPSTimeChanged FINAL)
    Q_PROPERTY(int GPSDay READ GPSDay WRITE setGPSDay NOTIFY GPSDayChanged FINAL)
    Q_PROPERTY(int GPSMonth READ GPSMonth WRITE setGPSMonth NOTIFY GPSMonthChanged FINAL)
    Q_PROPERTY(int GPSYear READ GPSYear WRITE setGPSYear NOTIFY GPSYearChanged FINAL)
    Q_PROPERTY(int LZD READ LZD WRITE setLZD NOTIFY LZDChanged FINAL)
    Q_PROPERTY(int LZMD READ LZMD WRITE setLZMD NOTIFY LZMDChanged FINAL)

    Q_PROPERTY(time_t UnixTime READ UnixTime WRITE setUnixTime NOTIFY UnixTimeChanged FINAL)

public:
    explicit DateTimeZDA(int id, QString label, QWidget *parent = nullptr);
    ~DateTimeZDA();

    void setInitialGuiValues() override;

    double GPSTime() const;
    void setGPSTime(double newGPSTime);

    int GPSDay() const;
    void setGPSDay(int newGPSDay);

    int GPSMonth() const;
    void setGPSMonth(int newGPSMonth);

    int GPSYear() const;
    void setGPSYear(int newGPSYear);

    int LZD() const;
    void setLZD(int newLZD);

    int LZMD() const;
    void setLZMD(int newLZMD);

    time_t UnixTime() const;
    void setUnixTime(time_t newUnixTime);

signals:
    void GPSTimeChanged();
    void GPSDayChanged();
    void GPSMonthChanged();
    void GPSYearChanged();
    void LZDChanged();
    void LZMDChanged();
    void UnixTimeChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_checkBoxEnabled_clicked(bool checked);
    void on_lineEditDaysSince1970_textEdited(const QString &arg1);
    void on_lineEditSecondsSinceMidnight_textEdited(const QString &arg1);

private:
    Ui::DateTimeZDA *ui;
    double m_GPSTime = 0.0;
    int m_GPSDay = 0;
    int m_GPSMonth = 0;
    int m_GPSYear = 0;
    int m_LZD = 0;
    int m_LZMD = 0;
    time_t m_UnixTime = 0;

    QString FormateDate(int day, int month, int year);
    std::tuple<int, int, int> extractGPSDateFromTimeT(time_t timestamp);
};

#endif // DATETIMEZDA_H
