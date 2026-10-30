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

#ifndef GNSSPOSITIONDATA129029_H
#define GNSSPOSITIONDATA129029_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class GnssPositionData129029;
}

class GnssPositionData129029 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit GnssPositionData129029(int id, const QString &label, QWidget *parent);
    ~GnssPositionData129029();

    /*
 * // GNSS Position Data
void SetN2kPGN129029(tN2kMsg &N2kMsg, unsigned char SID, uint16_t DaysSince1970, double SecondsSinceMidnight,
                     double Latitude, double Longitude, double Altitude,
                     tN2kGNSStype GNSStype, tN2kGNSSmethod GNSSmethod,
                     unsigned char nSatellites, double HDOP, double PDOP, double GeoidalSeparation,
                     unsigned char nReferenceStations, tN2kGNSStype ReferenceStationType, uint16_t ReferenceSationID,
                     double AgeOfCorrection
                     ) {
 */
    Q_PROPERTY(uchar SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(uint16_t DaysSince1970 READ DaysSince1970 WRITE setDaysSince1970 NOTIFY DaysSince1970Changed FINAL)
    Q_PROPERTY(double SecondsSinceMidnight READ SecondsSinceMidnight WRITE setSecondsSinceMidnight NOTIFY SecondsSinceMidnightChanged FINAL)
    Q_PROPERTY(double Latitude READ Latitude WRITE setLatitude NOTIFY LatitudeChanged FINAL)
    Q_PROPERTY(double Longitude READ Longitude WRITE setLongitude NOTIFY LongitudeChanged FINAL)
    Q_PROPERTY(double Altitude READ Altitude WRITE setAltitude NOTIFY AltitudeChanged FINAL)
    Q_PROPERTY(tN2kGNSStype GNSStype READ GNSStype WRITE setGNSStype NOTIFY GNSStypeChanged FINAL)
    Q_PROPERTY(tN2kGNSSmethod GNSSmethod READ GNSSmethod WRITE setGNSSmethod NOTIFY GNSSmethodChanged FINAL)
    Q_PROPERTY(uchar nSatellites READ nSatellites WRITE setnSatellites NOTIFY nSatellitesChanged FINAL)
    Q_PROPERTY(double HDOP READ HDOP WRITE setHDOP NOTIFY HDOPChanged FINAL)
    Q_PROPERTY(double PDOP READ PDOP WRITE setPDOP NOTIFY PDOPChanged FINAL)
    Q_PROPERTY(double GeoidalSeparation READ GeoidalSeparation WRITE setGeoidalSeparation NOTIFY GeoidalSeparationChanged FINAL)
    Q_PROPERTY(uchar nReferenceStations READ nReferenceStations WRITE setnReferenceStations NOTIFY nReferenceStationsChanged FINAL)
    Q_PROPERTY(tN2kGNSStype ReferenceStationType READ ReferenceStationType WRITE setReferenceStationType NOTIFY ReferenceStationTypeChanged FINAL)
    Q_PROPERTY(uint16_t ReferenceSationID READ ReferenceSationID WRITE setReferenceSationID NOTIFY ReferenceSationIDChanged FINAL)
    Q_PROPERTY(double AgeOfCorrection READ AgeOfCorrection WRITE setAgeOfCorrection NOTIFY AgeOfCorrectionChanged FINAL)

    uchar SID() const;
    void setSID(uchar newSID);

    uint16_t DaysSince1970() const;
    void setDaysSince1970(uint16_t newDaysSince1970);

    double SecondsSinceMidnight() const;
    void setSecondsSinceMidnight(double newSecondsSinceMidnight);

    double Latitude() const;
    void setLatitude(double newLatitude);

    double Longitude() const;
    void setLongitude(double newLongitude);

    double Altitude() const;
    void setAltitude(double newAltitude);

    tN2kGNSStype GNSStype() const;
    void setGNSStype(tN2kGNSStype newGNSStype);

    tN2kGNSSmethod GNSSmethod() const;
    void setGNSSmethod(tN2kGNSSmethod newGNSSmethod);

    uchar nSatellites() const;
    void setnSatellites(uchar newNSatellites);

    double HDOP() const;
    void setHDOP(double newHDOP);

    double PDOP() const;
    void setPDOP(double newPDOP);

    double GeoidalSeparation() const;
    void setGeoidalSeparation(double newGeoidalSeparation);

    uchar nReferenceStations() const;
    void setnReferenceStations(uchar newNReferenceStations);

    tN2kGNSStype ReferenceStationType() const;
    void setReferenceStationType(tN2kGNSStype newReferenceStationType);

    uint16_t ReferenceSationID() const;
    void setReferenceSationID(uint16_t newReferenceSationID);

    double AgeOfCorrection() const;
    void setAgeOfCorrection(double newAgeOfCorrection);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void DaysSince1970Changed();
    void SecondsSinceMidnightChanged();
    void LatitudeChanged();
    void LongitudeChanged();
    void AltitudeChanged();
    void GNSStypeChanged();
    void GNSSmethodChanged();
    void nSatellitesChanged();
    void HDOPChanged();
    void PDOPChanged();
    void GeoidalSeparationChanged();
    void nReferenceStationsChanged();
    void ReferenceStationTypeChanged();
    void ReferenceSationIDChanged();
    void AgeOfCorrectionChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

private slots:
    void on_lineEditLatitude_textEdited(const QString &arg1);
    void on_lineEditLongitude_textEdited(const QString &arg1);
    void on_lineEditnSatellites_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

public slots:
    void updateSID(unsigned char newSID);
    void updateDaysSince1970(uint16_t newDaysSince1970);
    void updateSecondsSinceMidnight(double newSecondsSinceMidnight);

protected slots:
    void onTimerTimeout() override;

private:
    Ui::GnssPositionData129029 *ui;
    uchar m_SID{};
    uint16_t m_DaysSince1970{};
    double m_SecondsSinceMidnight{};
    double m_Latitude{};
    double m_Longitude{};
    double m_Altitude{};
    tN2kGNSStype m_GNSStype{};
    tN2kGNSSmethod m_GNSSmethod{};
    uchar m_nSatellites{};
    double m_HDOP{};
    double m_PDOP{};
    double m_GeoidalSeparation{};
    uchar m_nReferenceStations{};
    tN2kGNSStype m_ReferenceStationType{};
    uint16_t m_ReferenceSationID{};
    double m_AgeOfCorrection{};
};

#endif // GNSSPOSITIONDATA129029_H
