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

#ifndef DATETIME129033_H
#define DATETIME129033_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class DateTime129033;
}

class DateTime129033 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit DateTime129033(int id, QString label, QWidget *parent = nullptr);
    ~DateTime129033();

    Q_PROPERTY(uint16_t DaysSince1970 READ DaysSince1970 WRITE setDaysSince1970 NOTIFY DaysSince1970Changed FINAL)
    Q_PROPERTY(double SecondsSinceMidnight READ SecondsSinceMidnight WRITE setSecondsSinceMidnight NOTIFY SecondsSinceMidnightChanged FINAL)
    Q_PROPERTY(int16_t LocalOffset READ LocalOffset WRITE setLocalOffset NOTIFY LocalOffsetChanged FINAL)

    void setInitialGuiValues() override;
    uint16_t DaysSince1970() const;
    void setDaysSince1970(uint16_t newDaysSince1970);

    double SecondsSinceMidnight() const;
    void setSecondsSinceMidnight(double newSecondsSinceMidnight);

    int16_t LocalOffset() const;
    void setLocalOffset(int16_t newLocalOffset);

public slots:
    void updateDaysSince1970(uint16_t newDaysSince1970);
    void updateSecondsSinceMidnight(double newSecondsSinceMidnight);
    void updateLocalOffset(int16_t newLocalOffset);

signals:
    void DaysSince1970Changed();
    void SecondsSinceMidnightChanged();
    void LocalOffsetChanged();

protected:
    void initializeUnitsLimits() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::DateTime129033 *ui;

    uint16_t m_DaysSince1970 = 0;
    double m_SecondsSinceMidnight = 0.0;
    int16_t m_LocalOffset = 0;
};

#endif // DATETIME129033_H
