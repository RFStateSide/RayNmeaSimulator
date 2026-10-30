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

#ifndef VESSELHEADING127250_H
#define VESSELHEADING127250_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class VesselHeading127250;
}

class VesselHeading127250 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit VesselHeading127250(int id, const QString &label, QWidget *parent);
    ~VesselHeading127250();

    Q_PROPERTY(uchar SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double Heading READ Heading WRITE setHeading NOTIFY HeadingChanged FINAL)
    Q_PROPERTY(double Deviation READ Deviation WRITE setDeviation NOTIFY DeviationChanged FINAL)
    Q_PROPERTY(double Variation READ Variation WRITE setVariation NOTIFY VariationChanged FINAL)
    Q_PROPERTY(tN2kHeadingReference HeadingReference READ HeadingReference WRITE setHeadingReference NOTIFY HeadingReferenceChanged FINAL)

    uchar SID() const;
    void setSID(uchar newSID);

    double Heading() const;
    void setHeading(double newHeading);

    double Deviation() const;
    void setDeviation(double newDeviation);

    double Variation() const;
    void setVariation(double newVariation);

    tN2kHeadingReference HeadingReference() const;
    void setHeadingReference(const tN2kHeadingReference &newHeadingReference);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void HeadingChanged();
    void DeviationChanged();
    void VariationChanged();
    void HeadingReferenceChanged();

protected:
    void initializeUnitsLimits() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditHeading_textEdited(const QString &arg1);
    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::VesselHeading127250 *ui;
    uchar m_SID;
    double m_Heading;
    double m_Deviation;
    double m_Variation;
    tN2kHeadingReference m_HeadingReference;
    QDoubleValidator *headingValidator = new QDoubleValidator(this);
    QString m_SignalToEmit = "";
    const int DELAY = 300;

};

#endif // VESSELHEADING127250_H
