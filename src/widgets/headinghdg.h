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

#ifndef HEADINGHDG_H
#define HEADINGHDG_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class HeadingHDG;
}

class HeadingHDG : public PgnWidgetBase
{
    Q_OBJECT

    Q_PROPERTY(double Heading READ Heading WRITE setHeading NOTIFY HeadingChanged FINAL)
    Q_PROPERTY(double Deviation READ Deviation WRITE setDeviation NOTIFY DeviationChanged FINAL)
    Q_PROPERTY(double Variation READ Variation WRITE setVariation NOTIFY VariationChanged FINAL)

public:
    explicit HeadingHDG(int id, const QString &label, QWidget *parent);
    ~HeadingHDG();

    double Heading() const;
    void setHeading(double newHeading);

    double Deviation() const;
    void setDeviation(double newDeviation);

    double Variation() const;
    void setVariation(double newVariation);

    void setInitialGuiValues() override;

signals:
    void HeadingChanged();
    void DeviationChanged();
    void VariationChanged();

private slots:
    void on_checkBoxEnabled_clicked(bool checked);
    void on_lineEditHeading_textEdited(const QString &arg1);

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private:
    Ui::HeadingHDG *ui;
    double m_Heading;
    double m_Deviation;
    double m_Variation;

    QDoubleValidator *headingValidator = new QDoubleValidator(this);
};

#endif // HEADINGHDG_H
