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

#ifndef SPEED128259_H
#define SPEED128259_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class Speed128259;
}

class Speed128259 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit Speed128259(int id, const QString &label, QWidget *parent);
    ~Speed128259();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double WaterReferenced READ WaterReferenced WRITE setWaterReferenced NOTIFY WaterReferencedChanged FINAL)
    Q_PROPERTY(double GroundReferenced READ GroundReferenced WRITE setGroundReferenced NOTIFY GroundReferencedChanged FINAL)
    Q_PROPERTY(tN2kSpeedWaterReferenceType SWRT READ SWRT WRITE setSWRT NOTIFY SWRTChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    double WaterReferenced() const;
    void setWaterReferenced(double newWaterReferenced);

    double GroundReferenced() const;
    void setGroundReferenced(double newGroundReferenced);

    tN2kSpeedWaterReferenceType SWRT() const;
    void setSWRT(const tN2kSpeedWaterReferenceType &newSWRT);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void WaterReferencedChanged();
    void GroundReferencedChanged();
    void SWRTChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

private slots:
    void on_lineEditWaterReferenced_textEdited(const QString &arg1);
    void on_lineEditGroundReferenced_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

protected slots:
    void onTimerTimeout() override;

private:
    Ui::Speed128259 *ui;
    unsigned char m_SID;
    double m_WaterReferenced;
    double m_GroundReferenced;
    tN2kSpeedWaterReferenceType m_SWRT;

    QDoubleValidator *waterReferencedValidator = new QDoubleValidator(this);
    QDoubleValidator *groundReferencedValidator = new QDoubleValidator(this);
};

#endif // SPEED128259_H
