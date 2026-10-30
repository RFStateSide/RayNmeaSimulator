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

#ifndef RECOMMENDEDMINGNSSRMC_H
#define RECOMMENDEDMINGNSSRMC_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class RecommendedMinGnssRMC;
}

class RecommendedMinGnssRMC : public PgnWidgetBase
{
    Q_OBJECT

    /*
 *double &GPSTime, char &Status, double &Latitude, double &Longitude,
                      double &TrueCOG, double &SOG, unsigned long &DaysSince1970, double &Variation, time_t *DateTime 
 */
    Q_PROPERTY(double GPSTime READ GPSTime WRITE setGPSTime NOTIFY GPSTimeChanged FINAL)
    Q_PROPERTY(char Status READ Status WRITE setStatus NOTIFY StatusChanged FINAL)
    Q_PROPERTY(double Latitude READ Latitude WRITE setLatitude NOTIFY LatitudeChanged FINAL)
    Q_PROPERTY(double Longitude READ Longitude WRITE setLongitude NOTIFY LongitudeChanged FINAL)
    Q_PROPERTY(double TrueCOG READ TrueCOG WRITE setTrueCOG NOTIFY TrueCOGChanged FINAL)
    Q_PROPERTY(double SOG READ SOG WRITE setSOG NOTIFY SOGChanged FINAL)
    Q_PROPERTY(ulong DaysSince1970 READ DaysSince1970 WRITE setDaysSince1970 NOTIFY DaysSince1970Changed FINAL)
    Q_PROPERTY(double Variation READ Variation WRITE setVariation NOTIFY VariationChanged FINAL)
    Q_PROPERTY(time_t DateTime READ DateTime WRITE setDateTime NOTIFY DateTimeChanged FINAL)

public:
    explicit RecommendedMinGnssRMC(int id, QString label, QWidget *parent = nullptr);
    ~RecommendedMinGnssRMC();

    void setInitialGuiValues() override;

    double GPSTime() const;
    void setGPSTime(double newGPSTime);

    char Status() const;
    void setStatus(char newStatus);

    double Latitude() const;
    void setLatitude(double newLatitude);

    double Longitude() const;
    void setLongitude(double newLongitude);

    double TrueCOG() const;
    void setTrueCOG(double newTrueCOG);

    double SOG() const;
    void setSOG(double newSOG);

    ulong DaysSince1970() const;
    void setDaysSince1970(ulong newDaysSince1970);

    double Variation() const;
    void setVariation(double newVariation);

    time_t DateTime() const;
    void setDateTime(time_t newDateTime);

signals:
    void GPSTimeChanged();
    void StatusChanged();
    void LatitudeChanged();
    void LongitudeChanged();
    void TrueCOGChanged();
    void SOGChanged();
    void DaysSince1970Changed();
    void VariationChanged();
    void DateTimeChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditLatitude_textEdited(const QString &arg1);
    void on_lineEditLongitude_textEdited(const QString &arg1);
    void on_lineEditCog_textEdited(const QString &arg1);
    void on_lineEditSog_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::RecommendedMinGnssRMC *ui;
    double m_GPSTime;
    char m_Status;
    double m_Latitude;
    double m_Longitude;
    double m_TrueCOG;
    double m_SOG;
    ulong m_DaysSince1970;
    double m_Variation;
    time_t m_DateTime;

    QDoubleValidator *validatorCog = new QDoubleValidator(this);
    QDoubleValidator *validatorSog = new QDoubleValidator(this);
};

#endif // RECOMMENDEDMINGNSSRMC_H
