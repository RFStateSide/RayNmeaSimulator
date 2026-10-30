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

#include <QJsonObject>
#ifndef GENERATORVESSELMOVEMENT_H
#define GENERATORVESSELMOVEMENT_H
#include <functional>
#include "receivedroutes.h"
#include "routeexport.h"

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "autopilotsimulator.h"
#include "compass.h"
#include "datastructs.h"
#include "gamecontroller.h"
#include "movevesselsimulator.h"
#include "pgnwidgetbase.h"

namespace Ui {
class GeneratorVesselMovement;
}

class GeneratorVesselMovement : public PgnWidgetBase
{
    Q_OBJECT
    double manualSpeedMps = 0.0;
    VesselWpBearing lastWaypointBearing;
    void refreshRouteDisplay();
    std::function<QVector<ReceivedRoutes::Route>()> receivedRouteProvider;
    QVector<Waypoint> selectedReceivedRoute;
    bool receivedRouteSelected = false;
    QString selectedReceivedRouteDescription;
    void updateRouteName();
    std::function<ReceivedRoutes::Route(QWidget*)> activeRouteChooser;


public:
    RouteExport::Snapshot selectedRouteForExport(QString *error) const;
    void setReceivedRouteProvider(std::function<QVector<ReceivedRoutes::Route>()> provider) {receivedRouteProvider=std::move(provider);}
    void setActiveRouteChooser(std::function<ReceivedRoutes::Route(QWidget*)> chooser){activeRouteChooser=std::move(chooser);}
    QJsonObject layoutConfiguration() const;
    void restoreLayoutConfiguration(const QJsonObject &configuration);
    void stopOperationalActivity();
    void setEnvironmentalSource(std::function<EnvironmentalConditions()> source);
    void setEnvironmentalInfluence(bool enabled);
    void refreshEnvironmentalStatus();
    // An empty message means all required engines and transmissions are ready.
    void setEngineReadinessCheck(std::function<QString()> check) { engineReadinessCheck = std::move(check); }
    explicit GeneratorVesselMovement(int id, const QString &label, QWidget *parent);
    ~GeneratorVesselMovement();

    // Properties exposed in tableView
    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double Latitude READ Latitude WRITE setLatitude NOTIFY LatitudeChanged FINAL)
    Q_PROPERTY(double Longitude READ Longitude WRITE setLongitude NOTIFY LongitudeChanged FINAL)
    Q_PROPERTY(double Altitude READ Altitude WRITE setAltitude NOTIFY AltitudeChanged FINAL)
    Q_PROPERTY(tN2kHeadingReference HeadingReference READ HeadingReference WRITE setHeadingReference NOTIFY HeadingReferenceChanged FINAL)
    Q_PROPERTY(double Heading READ Heading WRITE setHeading NOTIFY HeadingChanged FINAL)
    Q_PROPERTY(double Deviation READ Deviation WRITE setDeviation NOTIFY DeviationChanged FINAL)
    Q_PROPERTY(double Variation READ Variation WRITE setVariation NOTIFY VariationChanged FINAL)
    Q_PROPERTY(double COG READ COG WRITE setCOG NOTIFY COGChanged FINAL)
    Q_PROPERTY(double SOG READ SOG WRITE setSOG NOTIFY SOGChanged FINAL)
    Q_PROPERTY(double RateOfTurn READ RateOfTurn WRITE setRateOfTurn NOTIFY RateOfTurnChanged FINAL)
    Q_PROPERTY(double Yaw READ Yaw WRITE setYaw NOTIFY YawChanged FINAL)
    Q_PROPERTY(double Pitch READ Pitch WRITE setPitch NOTIFY PitchChanged FINAL)
    Q_PROPERTY(double Roll READ Roll WRITE setRoll NOTIFY RollChanged FINAL)
    Q_PROPERTY(double Heave READ Heave WRITE setHeave NOTIFY HeaveChanged FINAL)
    Q_PROPERTY(double RudderPosition READ RudderPosition WRITE setRudderPosition NOTIFY RudderPositionChanged FINAL)

    // Navigational properties not exposed in genertator tableView, for AutoPilot and Waypoint navigation PGNs
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

    // Cross track navigation properties not exposed in generator tableView, for AutoPilot and Waypoint navigation PGNs
    Q_PROPERTY(bool NavigationTerminated READ NavigationTerminated WRITE setNavigationTerminated NOTIFY NavigationTerminatedChanged FINAL)
    Q_PROPERTY(double XTE READ XTE WRITE setXTE NOTIFY XTEChanged FINAL)

    // Transmission properties not exposed in generator tableView, for AutoPilot, EngineSimulator PGNs
    Q_PROPERTY(tN2kTransmissionGear TransmissionGear READ TransmissionGear WRITE setTransmissionGear NOTIFY TransmissionGearChanged FINAL)
    Q_PROPERTY(tN2kTransmissionGear TransmissionGearStbd READ TransmissionGearStbd WRITE setTransmissionGearStbd NOTIFY TransmissionGearStbdChanged FINAL)

    Q_PROPERTY(double ThrottlePosition READ ThrottlePosition WRITE setThrottlePosition NOTIFY ThrottlePositionChanged FINAL)
    Q_PROPERTY(double ThrottlePositionStbd READ ThrottlePositionStbd WRITE setThrottlePositionStbd NOTIFY ThrottlePositionStbdChanged FINAL)

    // Vessel status properties not exposed in generator tableView, for MoveVesselSimulator
    Q_PROPERTY(VesselStatus CurrentVesselStatus READ CurrentVesselStatus WRITE setCurrentVesselStatus NOTIFY CurrentVesselStatusChanged FINAL)
    Q_PROPERTY(bool StartEngineEnabled READ StartEngineEnabled WRITE setStartEngineEnabled NOTIFY StartEngineEnabledChanged FINAL)

    void setMyVessel(const MyVessel &newMyVessel);
    // Operational movement is distinct from the saved generator output Enable.
    bool navigationControlActive() const;

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    double Latitude() const;
    void setLatitude(double newLatitude);

    double Longitude() const;
    void setLongitude(double newLongitude);

    double Altitude() const;
    void setAltitude(double newAltitude);

    tN2kHeadingReference HeadingReference() const;
    void setHeadingReference(tN2kHeadingReference newHeadingReference);

    double Heading() const;
    void setHeading(double newHeading);

    double Deviation() const;
    void setDeviation(double newDeviation);

    double Variation() const;
    void setVariation(double newVariation);

    double COG() const;
    void setCOG(double newCOG);

    double SOG() const;
    void setSOG(double newSOG);

    double RateOfTurn() const;
    void setRateOfTurn(double newRateOfTurn);

    double Yaw() const;
    void setYaw(double newYaw);

    double Pitch() const;
    void setPitch(double newPitch);

    double Roll() const;
    void setRoll(double newRoll);

    double RudderPosition() const;
    void setRudderPosition(double newRudderPosition);

    double DistanceToWaypoint() const;
    void setDistanceToWaypoint(double newDistanceToWaypoint);

    tN2kHeadingReference BearingReference() const;
    void setBearingReference(tN2kHeadingReference newBearingReference);

    bool PerpendicularCrossed() const;
    void setPerpendicularCrossed(bool newPerpendicularCrossed);

    bool ArrivalCircleEntered() const;
    void setArrivalCircleEntered(bool newArrivalCircleEntered);

    tN2kDistanceCalculationType CalculationType() const;
    void setCalculationType(tN2kDistanceCalculationType newCalculationType);

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

    bool NavigationTerminated() const;
    void setNavigationTerminated(bool newNavigationTerminated);

    double XTE() const;
    void setXTE(double newXTE);

    tN2kTransmissionGear TransmissionGear() const;
    void setTransmissionGear(tN2kTransmissionGear newTransmissionGear);

    tN2kTransmissionGear TransmissionGearStbd() const;
    void setTransmissionGearStbd(tN2kTransmissionGear newTransmissionGearStbd);

    VesselStatus CurrentVesselStatus() const;
    void setCurrentVesselStatus(const VesselStatus &newCurrentVesselStatus);

    double ThrottlePosition() const;
    void setThrottlePosition(double newThrottlePosition);

    double ThrottlePositionStbd() const;
    void setThrottlePositionStbd(double newThrottlePositionStbd);

    bool StartEngineEnabled() const;
    void setStartEngineEnabled(bool newStartEngineEnabled);

    void setInitialGuiValues() override;

    double Heave() const;
    void setHeave(double newHeave);

signals:
    void navigationControlChanged();
    void SIDChanged();
    void LatitudeChanged();
    void LongitudeChanged();
    void AltitudeChanged();
    void HeadingReferenceChanged();
    void HeadingChanged();
    void DeviationChanged();
    void VariationChanged();
    void COGChanged();
    void SOGChanged();
    void RateOfTurnChanged();
    void YawChanged();
    void PitchChanged();
    void RollChanged();
    void RudderPositionChanged();
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
    void ShowStatusBarMessage(QString message, int timeout);
    void NavigationTerminatedChanged();
    void XTEChanged();
    void TransmissionGearChanged();
    void TransmissionGearStbdChanged();
    void CurrentVesselStatusChanged();
    void ThrottlePositionChanged();
    void ThrottlePositionStbdChanged();

    void StartEngineEnabledChanged();
    void UseGenVessel(bool usingGenVessel);

    void HeaveChanged();

protected:
    void initializeUnitsLimits() override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

protected slots:
    void onTimerTimeout() override;

public slots:
    void VesselWpBearingChanged(VesselWpBearing vesselWpBearing);
    void LatLongChanged(double latitude, double longitude);

private slots:
    void PollTimerElapsed();
    void on_horizontalSliderRudder_valueChanged(int value);
    void on_horizontalSliderHeading_valueChanged(int value);
    void on_lineEditLatitude_textEdited(const QString &arg1);
    void on_lineEditLongitude_textEdited(const QString &arg1);
    void on_lineEditSog_textEdited(const QString &arg1);
    void on_lineEditRateOfTurn_textEdited(const QString &arg1);
    void on_lineEditPitch_textEdited(const QString &arg1);
    void on_lineEditRoll_textEdited(const QString &arg1);
    void on_pushButtonSelectRouteFile_clicked();
    void exportSelectedRoute();
    void on_checkBoxAutoPilotEnabled_clicked(bool checked);
    void on_doubleSpinBoxSpeed_valueChanged(double arg1);
    void on_checkBoxReverseRoute_clicked(bool checked);
    void on_radioButtonForwardPort_clicked();
    void on_radioButtonNeutralPort_clicked();
    void on_radioButtonReversePort_clicked();
    void on_radioButtonForwardStbd_clicked();
    void on_radioButtonNeutralStbd_clicked();
    void on_radioButtonReverseStbd_clicked();
    void on_checkBoxManualMovementEnabled_clicked(bool checked);
    void on_verticalSliderThrottle_valueChanged(int value);
    void on_verticalSliderThrottleStbd_valueChanged(int value);
    void on_checkBoxSyncThrottle_clicked(bool checked);
    void on_checkBoxGamePadEnabled_clicked(bool checked);
    void on_pushButtonRefreshGameList_clicked();

    void on_lineEditHeave_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

private:
    std::function<QString()> engineReadinessCheck;
    Ui::GeneratorVesselMovement *ui;
    QTimer *pollTimer;
    AutoPilotSimulator *autoPilotSimulator;
    MoveVesselSimulator *moveVesselSimulator;
    GameController *gController;
    void initializeComboBoxes();
    void initializeCompass();
    void initializeAutoPilot();
    void clearAutoPilot();
    void initializeMoveVesselSimulator();
    void initializeOuthers();
    double normalizeAxisToRudderAngle(int16_t value, bool sticky = false, double lastRudderPosition = 0.0);
    double normalizeAxisToThrottle(int16_t value, bool sticky = false, double lastThrottlePosition = 0.0);
    void connectHandleGameControllerEvents(bool connect);
    void resetVesselControls();

    int m_PollCounter = 0;
    Compass *m_compass;
    unsigned char m_SID = 0;
    double m_Latitude = 0.0;
    double m_Longitude = 0.0;
    double m_Altitude = 0.0;
    tN2kHeadingReference m_HeadingReference = N2khr_magnetic;
    double m_Heading = 0.0;
    double m_Deviation = 0.0;
    double m_Variation = 0.0;
    double m_COG = 0.0;
    double m_SOG = 0.0;
    double m_RateOfTurn = 0.0;
    double m_Yaw = 0.0;
    double m_Pitch = 0.0;
    double m_Roll = 0.0;
    double m_RudderPosition = 0.0;

    QDoubleValidator *validatorHeading = new QDoubleValidator(this);
    QDoubleValidator *validatorSpeed = new QDoubleValidator(this);
    QDoubleValidator *validatorRateOfTurn = new QDoubleValidator(this);
    QDoubleValidator *validatorPitchRoll = new QDoubleValidator(this);
    QDoubleValidator *validatorDistance = new QDoubleValidator(this);
    double m_DistanceToWaypoint = 0.0;
    tN2kHeadingReference m_BearingReference = N2khr_true;
    bool m_PerpendicularCrossed = false;
    bool m_ArrivalCircleEntered = false;
    tN2kDistanceCalculationType m_CalculationType = N2kdct_GreatCircle;
    double m_ETATime = 0.0;
    int16_t m_ETADate = 0;
    double m_BearingOriginToDestinationWaypoint = 0.0;
    double m_BearingPositionToDestinationWaypoint = 0.0;
    uint32_t m_OriginWaypointNumber = 0;
    uint32_t m_DestinationWaypointNumber = 0;
    double m_DestinationLatitude = 0.0;
    double m_DestinationLongitude = 0.0;
    double m_WaypointClosingVelocity = 0.0;
    bool m_NavigationTerminated = true;
    double m_XTE = 0.0;
    tN2kTransmissionGear m_TransmissionGear = N2kTG_Neutral;
    tN2kTransmissionGear m_TransmissionGearStbd = N2kTG_Neutral;
    VesselStatus m_CurrentVesselStatus;
    MyVessel m_MyVessel = MyVessel();
    std::function<EnvironmentalConditions()> environmentalSource;
    QLabel *environmentalStatus = nullptr;
    EnvironmentalConditions environmentalConditions();
    double m_ThrottlePosition = 0.0;
    double m_ThrottlePositionStbd = 0.0;
    bool isManualMovementEnabled = false; // Manual movement utilizes VesselMovementSimulator
    bool m_StartEngineEnabled = false;
    bool m_UseGenVessel = false;
    QList<QString> m_SpeedSources = {"Manual", "Throttle Position", "Route File"};
    bool m_SyncThrottleLastState = false; // Used to reset throttle position when AutoPilot is disabled
    double m_rudderPortDeg = 0.0;
    double m_rudderStbdDeg = 0.0;
    double m_lastRudderPort = 0.0;
    double m_lastRudderStbd = 0.0;
    double m_Heave;
};

#endif // GENERATORVESSELMOVEMENT_H
