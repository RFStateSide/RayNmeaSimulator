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

#ifndef DCSTATUS127506_H
#define DCSTATUS127506_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class DcStatus127506;
}

class DcStatus127506 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit DcStatus127506(int id, const QString &label, QWidget *parent);
    ~DcStatus127506();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(unsigned char DCInstance READ DCInstance WRITE setDCInstance NOTIFY DCInstanceChanged FINAL)
    Q_PROPERTY(tN2kDCType DCType READ DCType WRITE setDCType NOTIFY DCTypeChanged FINAL)
    Q_PROPERTY(uint8_t StateOfCharge READ StateOfCharge WRITE setStateOfCharge NOTIFY StateOfChargeChanged FINAL)
    Q_PROPERTY(uint8_t StateOfHealth READ StateOfHealth WRITE setStateOfHealth NOTIFY StateOfHealthChanged FINAL)
    Q_PROPERTY(double TimeRemaining READ TimeRemaining WRITE setTimeRemaining NOTIFY TimeRemainingChanged FINAL)
    Q_PROPERTY(double RippleVoltage READ RippleVoltage WRITE setRippleVoltage NOTIFY RippleVoltageChanged FINAL)
    Q_PROPERTY(double Capacity READ Capacity WRITE setCapacity NOTIFY CapacityChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    unsigned char DCInstance() const;
    void setDCInstance(unsigned char newDCInstance);

    tN2kDCType DCType() const;
    void setDCType(const tN2kDCType &newDCType);

    uint8_t StateOfCharge() const;
    void setStateOfCharge(uint8_t newStateOfCharge);

    double TimeRemaining() const;
    void setTimeRemaining(double newTimeRemaining);

    double RippleVoltage() const;
    void setRippleVoltage(double newRippleVoltage);

    double Capacity() const;
    void setCapacity(double newCapacity);

    uint8_t StateOfHealth() const;
    void setStateOfHealth(uint8_t newStateOfHealth);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();

    void DCInstanceChanged();

    void DCTypeChanged();

    void StateOfChargeChanged();

    void TimeRemainingChanged();

    void RippleVoltageChanged();

    void CapacityChanged();

    void StateOfHealthChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEdit_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::DcStatus127506 *ui;
    unsigned char m_SID = 0;
    unsigned char m_DCInstance = 0;
    tN2kDCType m_DCType = N2kDCt_Battery;
    uint8_t m_StateOfCharge = 0;
    double m_TimeRemaining = 0;
    double m_RippleVoltage = 0;
    double m_Capacity = 0;
    uint8_t m_StateOfHealth = 0;

    QDoubleValidator *m_doubleValidator = new QDoubleValidator(this);
};

#endif // DCSTATUS127506_H
