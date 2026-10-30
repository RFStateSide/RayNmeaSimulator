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

#ifndef NAVIGATION129284_H
#define NAVIGATION129284_H

#include <QDoubleValidator>
#include <QWidget>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class Navigation129284;
}

class Navigation129284 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit Navigation129284(int id, const QString &label, QWidget *parent);
    ~Navigation129284();

    Q_PROPERTY(uchar SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double DistanceToWaypoint READ DistanceToWaypoint WRITE setDistanceToWaypoint NOTIFY DistanceToWaypointChanged FINAL)
    Q_PROPERTY(tN2kHeadingReference BearingReference READ BearingReference WRITE setBearingReference NOTIFY BearingReferenceChanged FINAL)
    Q_PROPERTY(bool PerpendicularCrossed READ PerpendicularCrossed WRITE setPerpendicularCrossed NOTIFY PerpendicularCrossedChanged FINAL)
    Q_PROPERTY(bool ArrivalCircleEntered READ ArrivalCircleEntered WRITE setArrivalCircleEntered NOTIFY ArrivalCircleEnteredChanged FINAL)
    Q_PROPERTY(tN2kDistanceCalculationType CalculationType READ CalculationType WRITE setCalculationType NOTIFY CalculationTypeChanged FINAL)
    Q_PROPERTY(double ETATime READ ETATime WRITE setETATime NOTIFY ETATimeChanged FINAL)
    Q_PROPERTY(int16_t ETADate READ ETADate WRITE setETADate NOTIFY ETADateChanged FINAL)
    Q_PROPERTY(double BearingOriginToDestinationWaypoint READ BearingOriginToDestinationWaypoint WRITE setBearingOriginToDestinationWaypoint NOTIFY
                   BearingOriginToDestinationWaypointChanged FINAL)
    Q_PROPERTY(double BearingPositionToDestinationWaypoint READ BearingPositionToDestinationWaypoint WRITE setBearingPositionToDestinationWaypoint NOTIFY
                   BearingPositionToDestinationWaypointChanged FINAL)
    Q_PROPERTY(uint32_t OriginWaypointNumber READ OriginWaypointNumber WRITE setOriginWaypointNumber NOTIFY OriginWaypointNumberChanged FINAL)
    Q_PROPERTY(uint32_t DestinationWaypointNumber READ DestinationWaypointNumber WRITE setDestinationWaypointNumber NOTIFY DestinationWaypointNumberChanged FINAL)
    Q_PROPERTY(double DestinationLatitude READ DestinationLatitude WRITE setDestinationLatitude NOTIFY DestinationLatitudeChanged FINAL)
    Q_PROPERTY(double DestinationLongitude READ DestinationLongitude WRITE setDestinationLongitude NOTIFY DestinationLongitudeChanged FINAL)
    Q_PROPERTY(double WaypointClosingVelocity READ WaypointClosingVelocity WRITE setWaypointClosingVelocity NOTIFY WaypointClosingVelocityChanged FINAL)

    uchar SID() const;
    void setSID(uchar newSID);

    double DistanceToWaypoint() const;
    void setDistanceToWaypoint(double newDistanceToWaypoint);

    tN2kHeadingReference BearingReference() const;
    void setBearingReference(const tN2kHeadingReference &newBearingReference);

    bool PerpendicularCrossed() const;
    void setPerpendicularCrossed(bool newPerpendicularCrossed);

    bool ArrivalCircleEntered() const;
    void setArrivalCircleEntered(bool newArrivalCircleEntered);

    tN2kDistanceCalculationType CalculationType() const;
    void setCalculationType(const tN2kDistanceCalculationType &newCalculationType);

    double ETATime() const;
    void setETATime(double newETATime);

    int16_t ETADate() const;
    void setETADate(int16_t newETADate);

    double BearingOriginToDestinationWaypoint() const;
    void setBearingOriginToDestinationWaypoint(double newBearingOriginToDestinationWaypoint);

    double BearingPositionToDestinationWaypoint() const;
    void setBearingPositionToDestinationWaypoint(double newBearingPositionToDestinationWaypoint);

    uint32_t OriginWaypointNumber() const;
    void setOriginWaypointNumber(uint32_t newOriginWaypointNumber);

    uint32_t DestinationWaypointNumber() const;
    void setDestinationWaypointNumber(uint32_t newDestinationWaypointNumber);

    double DestinationLatitude() const;
    void setDestinationLatitude(double newDestinationLatitude);

    double DestinationLongitude() const;
    void setDestinationLongitude(double newDestinationLongitude);

    double WaypointClosingVelocity() const;
    void setWaypointClosingVelocity(double newWaypointClosingVelocity);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void DistanceToWaypointChanged();
    void BearingReferenceChanged();
    void PerpendicularCrossedChanged();
    void ArrivalCircleEnteredChanged();
    void CalculationTypeChanged();
    void ETATimeChanged();
    void ETADateChanged();
    void BearingOriginToDestinationWaypointChanged();
    void BearingPositionToDestinationWaypointChanged();
    void OriginWaypointNumberChanged();
    void DestinationWaypointNumberChanged();
    void DestinationLatitudeChanged();
    void DestinationLongitudeChanged();
    void WaypointClosingVelocityChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

private slots:
    void on_lineEditDistanceToWaypoint_textEdited(const QString &arg1);
    void on_lineEditETATime_textEdited(const QString &arg1);
    void on_lineEditBearingPositionToDestinationWaypoint_textEdited(const QString &arg1);
    void on_pushButtonRemove_clicked(bool checked);

protected slots:
    void onTimerTimeout() override;

private:
    void updateDistancePresentation();
    Ui::Navigation129284 *ui;
    uchar m_SID;
    double m_DistanceToWaypoint;
    tN2kHeadingReference m_BearingReference;
    bool m_PerpendicularCrossed;
    bool m_ArrivalCircleEntered;
    tN2kDistanceCalculationType m_CalculationType;
    double m_ETATime;
    int16_t m_ETADate;
    double m_BearingOriginToDestinationWaypoint;
    double m_BearingPositionToDestinationWaypoint;
    uint32_t m_OriginWaypointNumber;
    uint32_t m_DestinationWaypointNumber;
    double m_DestinationLatitude;
    double m_DestinationLongitude;
    double m_WaypointClosingVelocity;
};

#endif // NAVIGATION129284_H
