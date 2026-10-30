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

#ifndef SYSTEMTIME126992_H
#define SYSTEMTIME126992_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class SystemTime126992;
}

class SystemTime126992 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit SystemTime126992(int id, const QString &label, QWidget *parent);
    ~SystemTime126992();
    void setInitialGuiValues() override;

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(uint16_t SystemDate READ SystemDate WRITE setSystemDate NOTIFY SystemDateChanged FINAL)
    Q_PROPERTY(double SystemTime READ SystemTime WRITE setSystemTime NOTIFY SystemTimeChanged FINAL)
    Q_PROPERTY(tN2kTimeSource TimeSource READ TimeSource WRITE setTimeSource NOTIFY TimeSourceChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    uint16_t SystemDate() const;
    void setSystemDate(uint16_t newSystemDate);

    double SystemTime() const;
    void setSystemTime(double newSystemTime);

    tN2kTimeSource TimeSource() const;
    void setTimeSource(const tN2kTimeSource &newTimeSource);

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

public slots:
    void updateSID(unsigned char newSID);
    void updateDaysSince1970(uint16_t newDaysSince1970);
    void updateSecondsSinceMidnight(double newSecondsSinceMidnight);

signals:
    void SIDChanged();
    void SystemDateChanged();
    void SystemTimeChanged();
    void TimeSourceChanged();

private slots:
    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::SystemTime126992 *ui;
    QTimer *pollTimer;
    QTimer *timer;

    unsigned char m_SID = 0;
    uint16_t m_SystemDate = 0;
    double m_SystemTime = 0;
    tN2kTimeSource m_TimeSource = N2ktimes_GPS;

};

#endif // SYSTEMTIME126992_H
