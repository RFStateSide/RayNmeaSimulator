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

#ifndef MAGNETICVARIATION127258_H
#define MAGNETICVARIATION127258_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class MagneticVariation127258;
}

class MagneticVariation127258 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit MagneticVariation127258(int id, const QString &label, QWidget *parent);
    ~MagneticVariation127258();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(tN2kMagneticVariation Source READ Source WRITE setSource NOTIFY SourceChanged FINAL)
    Q_PROPERTY(uint16_t DaysSince1970 READ DaysSince1970 WRITE setDaysSince1970 NOTIFY DaysSince1970Changed FINAL)
    Q_PROPERTY(double Variation READ Variation WRITE setVariation NOTIFY VariationChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    tN2kMagneticVariation Source() const;
    void setSource(tN2kMagneticVariation newSource);

    uint16_t DaysSince1970() const;
    void setDaysSince1970(uint16_t newDaysSince1970);

    double Variation() const;
    void setVariation(double newVariation);

    void setInitialGuiValues() override;

signals:
    void IdChanged();
    void PgnChanged();
    void ColumnSpanChanged();
    void RowSpanChanged();
    void UserPrefsChanged();
    void SIDChanged();
    void SourceChanged();
    void DaysSince1970Changed();
    void VariationChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditMagneticVariation_textEdited(const QString &arg1);

    void on_pushButtonRemove_clicked(bool checked);

private:
    Ui::MagneticVariation127258 *ui;
    unsigned char m_SID;
    tN2kMagneticVariation m_Source;
    uint16_t m_DaysSince1970;
    double m_Variation;

    QDoubleValidator *variationValidator = new QDoubleValidator(this);

};

#endif // MAGNETICVARIATION127258_H
