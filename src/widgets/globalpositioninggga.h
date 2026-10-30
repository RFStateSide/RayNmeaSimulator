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

#ifndef GLOBALPOSITIONINGGGA_H
#define GLOBALPOSITIONINGGGA_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class GlobalPositioningGGA;
}

class GlobalPositioningGGA : public PgnWidgetBase
{
    Q_OBJECT

    Q_PROPERTY(double GPSTime READ GPSTime WRITE setGPSTime NOTIFY GPSTimeChanged FINAL)
    Q_PROPERTY(double Latitude READ Latitude WRITE setLatitude NOTIFY LatitudeChanged FINAL)
    Q_PROPERTY(double Longitude READ Longitude WRITE setLongitude NOTIFY LongitudeChanged FINAL)
    Q_PROPERTY(int GPSQualityIndicator READ GPSQualityIndicator WRITE setGPSQualityIndicator NOTIFY GPSQualityIndicatorChanged FINAL)
    Q_PROPERTY(int SatelliteCount READ SatelliteCount WRITE setSatelliteCount NOTIFY SatelliteCountChanged FINAL)
    Q_PROPERTY(double HDOP READ HDOP WRITE setHDOP NOTIFY HDOPChanged FINAL)
    Q_PROPERTY(double Altitude READ Altitude WRITE setAltitude NOTIFY AltitudeChanged FINAL)
    Q_PROPERTY(double GeoidalSeparation READ GeoidalSeparation WRITE setGeoidalSeparation NOTIFY GeoidalSeparationChanged FINAL)
    Q_PROPERTY(double DGPSAge READ DGPSAge WRITE setDGPSAge NOTIFY DGPSAgeChanged FINAL)
    Q_PROPERTY(int DGPSReferenceStationID READ DGPSReferenceStationID WRITE setDGPSReferenceStationID NOTIFY DGPSReferenceStationIDChanged FINAL)

public:
    explicit GlobalPositioningGGA(int id, const QString &label, QWidget *parent);
    ~GlobalPositioningGGA();

    void setInitialGuiValues() override;

    double GPSTime() const;
    void setGPSTime(double newGPSTime);

    double Latitude() const;
    void setLatitude(double newLatitude);

    double Longitude() const;
    void setLongitude(double newLongitude);

    int GPSQualityIndicator() const;
    void setGPSQualityIndicator(int newGPSQualityIndicator);

    int SatelliteCount() const;
    void setSatelliteCount(int newSatelliteCount);

    double HDOP() const;
    void setHDOP(double newHDOP);

    double Altitude() const;
    void setAltitude(double newAltitude);

    double GeoidalSeparation() const;
    void setGeoidalSeparation(double newGeoidalSeparation);

    double DGPSAge() const;
    void setDGPSAge(double newDGPSAge);

    int DGPSReferenceStationID() const;
    void setDGPSReferenceStationID(int newDGPSReferenceStationID);

signals:
    void GPSTimeChanged();
    void LatitudeChanged();
    void LongitudeChanged();
    void GPSQualityIndicatorChanged();
    void SatelliteCountChanged();
    void HDOPChanged();
    void AltitudeChanged();
    void GeoidalSeparationChanged();
    void DGPSAgeChanged();
    void DGPSReferenceStationIDChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditLatitude_textEdited(const QString &arg1);
    void on_lineEditLongitude_textEdited(const QString &arg1);
    void on_lineEditnSatellites_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

    void on_lineEditTime_textEdited(const QString &arg1);

private:
    Ui::GlobalPositioningGGA *ui;
    double m_GPSTime{};
    double m_Latitude{};
    double m_Longitude{};
    int m_GPSQualityIndicator{};
    int m_SatelliteCount{};
    double m_HDOP{};
    double m_Altitude{};
    double m_GeoidalSeparation{};
    double m_DGPSAge{};
    int m_DGPSReferenceStationID{};
};

#endif // GLOBALPOSITIONINGGGA_H
