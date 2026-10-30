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

#ifndef TRUEHEADINGHDT_H
#define TRUEHEADINGHDT_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class TrueHeadingHDT;
}

class TrueHeadingHDT : public PgnWidgetBase
{
    Q_OBJECT

    Q_PROPERTY(double Heading READ Heading WRITE setHeading NOTIFY HeadingChanged FINAL)

public:
    explicit TrueHeadingHDT(int id, const QString &label, QWidget *parent);
    ~TrueHeadingHDT();

    void setInitialGuiValues() override;

    double Heading() const;
    void setHeading(double newHeading);

signals:
    void HeadingChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_checkBoxEnabled_clicked(bool checked);

    void on_lineEditHeading_textEdited(const QString &arg1);

private:
    Ui::TrueHeadingHDT *ui;
    double m_Heading;

    QDoubleValidator *headingValidator = new QDoubleValidator(this);
};

#endif // TRUEHEADINGHDT_H
