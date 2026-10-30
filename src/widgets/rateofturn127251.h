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

#ifndef RATEOFTURN127251_H
#define RATEOFTURN127251_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class RateOfTurn127251;
}

class RateOfTurn127251 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit RateOfTurn127251(int id, const QString &label, QWidget *parent);
    ~RateOfTurn127251();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double RateOfTurn READ RateOfTurn WRITE setRateOfTurn NOTIFY RateOfTurnChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    double RateOfTurn() const;
    void setRateOfTurn(double newRateOfTurn);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();

    void RateOfTurnChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditRateOfTurn_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::RateOfTurn127251 *ui;
    unsigned char m_SID;
    double m_RateOfTurn;

    QDoubleValidator *validatorRateOfTurn = new QDoubleValidator(this);
};

#endif // RATEOFTURN127251_H
