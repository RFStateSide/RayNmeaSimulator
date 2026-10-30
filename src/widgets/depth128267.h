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

#ifndef DEPTH128267_H
#define DEPTH128267_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class Depth128267;
}

class Depth128267 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit Depth128267(int id, const QString &label, QWidget *parent);
    ~Depth128267();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double DepthBelowTransducer READ DepthBelowTransducer WRITE setDepthBelowTransducer NOTIFY DepthBelowTransducerChanged FINAL)
    Q_PROPERTY(double Offset READ Offset WRITE setOffset NOTIFY OffsetChanged FINAL)
    Q_PROPERTY(double Range READ Range WRITE setRange NOTIFY RangeChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    double DepthBelowTransducer() const;
    void setDepthBelowTransducer(double newDepthBelowTransducer);

    double Offset() const;
    void setOffset(double newOffset);

    double Range() const;
    void setRange(double newRange);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void DepthBelowTransducerChanged();
    void OffsetChanged();
    void RangeChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditDepth_textEdited(const QString &arg1);

private:
    Ui::Depth128267 *ui;
    unsigned char m_SID = 0;
    double m_DepthBelowTransducer = 0;
    double m_Offset = 0;
    double m_Range = 0;

    QDoubleValidator *m_DepthBelowTransducerValidator = new QDoubleValidator(this);
};

#endif // DEPTH128267_H
