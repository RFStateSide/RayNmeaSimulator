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

#ifndef GENERATORDATETIME_H
#define GENERATORDATETIME_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class GeneratorDateTime;
}

class GeneratorDateTime : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit GeneratorDateTime(int id, const QString &label, QWidget *parent);
    ~GeneratorDateTime();

    Q_PROPERTY(uchar SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(uint16_t DaysSince1970 READ DaysSince1970 WRITE setDaysSince1970 NOTIFY DaysSince1970Changed FINAL)
    Q_PROPERTY(double SecondsSinceMidnight READ SecondsSinceMidnight WRITE setSecondsSinceMidnight NOTIFY SecondsSinceMidnightChanged FINAL)
    Q_PROPERTY(int16_t LocalOffset READ LocalOffset WRITE setLocalOffset NOTIFY LocalOffsetChanged FINAL)
    Q_PROPERTY(int RefreshRate READ RefreshRate WRITE setRefreshRate NOTIFY RefreshRateChanged FINAL)

    Q_PROPERTY(time_t UnixTime READ UnixTime WRITE setUnixTime NOTIFY UnixTimeChanged FINAL)

    void setInitialGuiValues() override;
    uchar SID() const;
    void setSID(uchar newSID);

    uint16_t DaysSince1970() const;
    void setDaysSince1970(uint16_t newDaysSince1970);

    double SecondsSinceMidnight() const;
    void setSecondsSinceMidnight(double newSecondsSinceMidnight);

    int16_t LocalOffset() const;
    void setLocalOffset(int16_t newLocalOffset);

    int RefreshRate() const;
    void setRefreshRate(int newRefreshRate);

    time_t UnixTime() const;
    void setUnixTime(time_t newUnixTime);

private slots:
    void PollTimerElapsed();

signals:
    void SIDChanged();
    void DaysSince1970Changed();
    void SecondsSinceMidnightChanged();
    void LocalOffsetChanged();
    void RefreshRateChanged();

    void UnixTimeChanged();

protected:
    void initializeUnitsLimits() override;

private:
    Ui::GeneratorDateTime *ui;
    QTimer *pollTimer;
    uchar m_SID = 0;
    uint16_t m_DaysSince1970 = 0;
    double m_SecondsSinceMidnight = 0.0;
    int16_t m_LocalOffset = 0;
    int m_RefreshRate = 1000;
    int m_PollCounter = 0;
    time_t m_UnixTime = 0;
};

#endif // GENERATORDATETIME_H
