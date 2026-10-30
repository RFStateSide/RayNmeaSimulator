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

#include <QInputDialog>
#include "coordinateeditor.h"
#include "generatorvesselmovement.h"
#include "apppaths.h"
#include "ui_generatorvesselmovement.h"

#include "n2khelper.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QScopeGuard>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPointer>
#include <helper.h>

GeneratorVesselMovement::GeneratorVesselMovement(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::GeneratorVesselMovement)
    , gController(new GameController(this))
{
    ui->setupUi(this);
    auto *receivedStatus=new QLabel(this);
    receivedStatus->setObjectName("receivedRouteStatus");receivedStatus->setWordWrap(true);receivedStatus->hide();
    ui->verticalLayout_7->insertWidget(1,receivedStatus);
    auto *manualHint = new QLabel(tr("Manual movement is active. Stop Manual movement on the Manual tab to enable Autopilot."), this);
    manualHint->setObjectName("autopilotManualMovementHint");
    manualHint->setTextFormat(Qt::PlainText);
    manualHint->setWordWrap(true);
    manualHint->setVisible(ui->checkBoxManualMovementEnabled->isChecked());
    ui->verticalLayout_7->insertWidget(1, manualHint);
    connect(ui->checkBoxManualMovementEnabled, &QCheckBox::toggled,
            manualHint, &QWidget::setVisible);

    // Retain the existing file-path storage and Layout contract; show the selected
    // route's name in the same Autopilot row for either route source.
    auto *routeName = new QLineEdit(this);
    routeName->setObjectName("selectedRouteName");
    routeName->setReadOnly(true);
    routeName->setPlaceholderText(tr("No route selected"));
    routeName->setAccessibleName(tr("Selected route"));
    ui->horizontalLayout_10->insertWidget(0, routeName);
    ui->lineEditFilePath->hide();
    connect(ui->lineEditFilePath, &QLineEdit::textChanged, this,
            [this] { updateRouteName(); });
    auto *exportButton = new QPushButton(tr("Export Route…"), this);
    exportButton->setObjectName("exportRouteButton");
    exportButton->setToolTip(tr("Export the selected route to GPX or KML. Names and ordered positions only; no speed, elevation or device identifiers. Does not change movement or Sending."));
    ui->verticalLayout_7->insertWidget(1, exportButton, 0, Qt::AlignRight);
    connect(exportButton, &QPushButton::clicked, this, &GeneratorVesselMovement::exportSelectedRoute);
    setId(id);
    setPgn(-129025);
    setColumnSpan(3);
    setRowSpan(17);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText("Generate " + label);
    ui->checkBoxEnabled->setToolTip(tr("Enable signals from this generator. Outbound transport output still requires explicit Enable Sending."));
    ui->checkBoxManualMovementEnabled->setToolTip(tr("Run local vessel movement in Manual mode using configured engines and transmissions. Does not enable Sending."));
    ui->checkBoxAutoPilotEnabled->setToolTip(tr("Follow the selected route with the existing Autopilot. Stop Manual movement first. Required engine profiles must be configured. Does not enable Sending."));
    ui->pushButtonSelectRouteFile->setToolTip(tr("Choose a GPX/KML file or a chartplotter route. Active retrieval needs separate request authorization. Selection alone does not start Autopilot, movement or Sending."));
    ui->checkBoxReverseRoute->setToolTip(tr("Reverse direction at each route end and keep following. Autopilot starts at the first waypoint; this option does not reverse the initial order."));
    ui->comboBoxSpeedSource->setToolTip(tr("Choose Manual, Throttle Position or Route File as the Autopilot speed source. This does not enable Sending."));
    ui->spinBoxMultSpeedByX->setToolTip(tr("Multiply Autopilot travel speed (1–10), including SOG and closing velocity. Elapsed time stays in real seconds; base speed and Sending are unchanged."));
    ui->checkBoxSyncThrottle->setToolTip(tr("Link port and starboard throttle controls. Disable for independent throttle adjustment."));
    ui->checkBoxGamePadEnabled->setToolTip(tr("Explicitly enable the selected gamepad for local control. Disconnect stops movement and clears throttle, rudder and gears; reconnection does not resume automatically."));
    ui->pushButtonRefreshGameList->setToolTip(tr("Refresh the connected controller list before selecting and explicitly enabling a gamepad."));

    ui->horizontalSliderHeading->setToolTip(tr("Set the Manual helm heading in degrees. Course over ground can differ in reverse motion or environmental drift."));
    ui->horizontalSliderRudder->setToolTip(tr("Set the rudder angle in degrees. Center for no commanded turn."));
    ui->verticalSliderThrottle->setToolTip(tr("Port throttle in percent. Sync links starboard; this does not enable Sending."));
    ui->verticalSliderThrottleStbd->setToolTip(tr("Starboard throttle in percent. Disable Sync for independent adjustment."));

    manualSpeedMps = m_MyVessel.CruisingSpeed;
    initializeUnitsLimits();
    connect(this, &PgnWidgetBase::UserPrefsChanged, this, &GeneratorVesselMovement::initializeUnitsLimits);

    gController->initialize();
    connect(gController, &GameController::disconnected, this, [this] {
        ui->checkBoxGamePadEnabled->setChecked(false);
        on_checkBoxGamePadEnabled_clicked(false);
        emit ShowStatusBarMessage("Gamepad disconnected: vessel movement stopped. Refresh and enable to reconnect.", 5000);
    });

    initializeComboBoxes();
    initializeCompass();

    ui->pushButtonRefreshGameList->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    //ui->pushButtonRemove->setIcon(iconExit);

    // Start the poll timer for simulating the system time
    pollTimer = new QTimer(this);
    pollTimer->start(500);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(PollTimerElapsed()));

    initializeMoveVesselSimulator();
    initializeAutoPilot();
    environmentalStatus=new QLabel(this);environmentalStatus->setObjectName("environmentalMotionStatus");
    environmentalStatus->setWordWrap(true);
    environmentalStatus->setToolTip("Representative wind drift and wave attitude; Heading remains helm-controlled. Current is not simulated. Configure in Setup → Vessel.");
    qobject_cast<QVBoxLayout*>(layout())->insertWidget(1,environmentalStatus);
    auto source=[this]{return environmentalConditions();};
    moveVesselSimulator->environment.source=source;autoPilotSimulator->environment.source=source;
    refreshEnvironmentalStatus();

    ui->groupBoxGamePadController->setEnabled(false);


}

GeneratorVesselMovement::~GeneratorVesselMovement()
{
    pollTimer->stop();
    gController->close();
    connectHandleGameControllerEvents(false);
    delete ui;
}

void GeneratorVesselMovement::setMyVessel(const MyVessel &newMyVessel)
{
    if (m_MyVessel == newMyVessel)
        return;
    m_MyVessel = newMyVessel;
    moveVesselSimulator->environment.vessel=newMyVessel;autoPilotSimulator->environment.vessel=newMyVessel;
    setEnvironmentalInfluence(newMyVessel.EnvironmentalInfluence);
    manualSpeedMps = newMyVessel.CruisingSpeed;
    const QSignalBlocker speedBlocker(ui->doubleSpinBoxSpeed);
    ui->doubleSpinBoxSpeed->setValue(N2kHelper::convertN2kSpeedToUserUnits(manualSpeedMps, UserPrefs()));
}

void GeneratorVesselMovement::setEnvironmentalSource(std::function<EnvironmentalConditions()> source)
{
    environmentalSource=std::move(source);refreshEnvironmentalStatus();
}
void GeneratorVesselMovement::setEnvironmentalInfluence(bool enabled)
{
    m_MyVessel.EnvironmentalInfluence=enabled;
    moveVesselSimulator->environment.enabled=enabled;autoPilotSimulator->environment.enabled=enabled;
    refreshEnvironmentalStatus();
}
EnvironmentalConditions GeneratorVesselMovement::environmentalConditions()
{
    return environmentalSource ? environmentalSource() : EnvironmentalConditions{};
}
void GeneratorVesselMovement::refreshEnvironmentalStatus()
{
    if(!environmentalStatus)return;
    const auto c=environmentalConditions();
    const bool viewer=property("GeneratorModeActive").isValid() && !property("GeneratorModeActive").toBool();
    environmentalStatus->setText(viewer ? tr("Environmental effects suspended — Viewer") : !m_MyVessel.EnvironmentalInfluence ? tr("Environmental effects OFF") :
        c.windValid || c.wavesValid ? tr("Environmental effects enabled — %1").arg(c.windValid && c.wavesValid ? tr("wind and waves") : c.windValid ? tr("wind") : tr("waves")) :
        tr("Environmental effects enabled — no applicable valid conditions"));
}

void GeneratorVesselMovement::initializeUnitsLimits()
{
    setCoordinateValidator(ui->lineEditLatitude, UserPrefs().latLongUnitIndex, true);
    setCoordinateValidator(ui->lineEditLongitude, UserPrefs().latLongUnitIndex, false);
    // "Knots", "km/h", "MPH range"
    switch (UserPrefs().speedUnitIndex) {
    case 0:
        ui->labelUnitsSog->setText("m/s");
        validatorSpeed->setRange(0.0, 90.0, 1);
        break;
    case 3:
        ui->labelUnitsSog->setText("kn");
        validatorSpeed->setRange(0.0, 173.7, 1);
        break;
    case 1:
        ui->labelUnitsSog->setText("km/h");
        validatorSpeed->setRange(0.0, 322.0, 1);
        break;
    case 2:
        ui->labelUnitsSog->setText("MPH");
        validatorSpeed->setRange(0.0, 201.0, 1);
        break;
    }
    ui->lineEditSog->setValidator(validatorSpeed);
    ui->labelUnitsSpeed->setText(ui->labelUnitsSog->text());
    ui->labelUnitsXte->setText(UserPrefs().distanceUnitIndex == 0 ? "m" : "ft");

    // Rate of Turn rad/s and deg/s
    switch (UserPrefs().rateOfTurnUnitIndex) {
    case 0:
        validatorRateOfTurn->setRange(-0.6981, 0.6981, 4);
        ui->labelUnitsRateOfTurn->setText("rad/s");
        break;
    case 1:
        validatorRateOfTurn->setRange(-40.0, 40.0, 1);
        ui->labelUnitsRateOfTurn->setText("deg/s");
        break;
    }
    ui->lineEditRateOfTurn->setValidator(validatorRateOfTurn);

    // Heave distance
    switch (UserPrefs().distanceUnitIndex) {
    case 0:
        ui->labelUnitsHeave->setText("m");
        validatorDistance->setRange(0.0, 1.5, 2);
        break;
    case 1:
    case 2:
        ui->labelUnitsHeave->setText("ft");
        validatorDistance->setRange(0.0, 5.0, 2);
        break;
    }
    ui->lineEditHeave->setValidator(validatorDistance);

    // Pitch, Roll
    validatorPitchRoll->setRange(-45.0, 45.0, 1);
    ui->lineEditPitch->setValidator(validatorPitchRoll);
    ui->lineEditRoll->setValidator(validatorPitchRoll);
}

void GeneratorVesselMovement::keyPressEvent(QKeyEvent *event)
{
    double throttlePosition = m_ThrottlePosition;
    double rudderPosition = Convert::RadiansToDegrees(m_RudderPosition);
    qDebug() << "Rutter position: " << rudderPosition;
    switch (event->key()) {
    case Qt::Key_Up:
        throttlePosition += 1;
        if (throttlePosition > 100) {
            throttlePosition = 100;
        }
        ui->verticalSliderThrottle->setValue(throttlePosition);
        on_verticalSliderThrottle_valueChanged(throttlePosition);
        break;
    case Qt::Key_Down:
        throttlePosition -= 1;
        if (throttlePosition < 0) {
            throttlePosition = 0;
        }
        ui->verticalSliderThrottle->setValue(throttlePosition);
        on_verticalSliderThrottle_valueChanged(throttlePosition);
        break;
    case Qt::Key_Left:
        rudderPosition -= 1;
        if (rudderPosition < -45) {
            rudderPosition = -45;
        }
        ui->horizontalSliderRudder->setValue(rudderPosition * 10);
        on_horizontalSliderRudder_valueChanged(rudderPosition * 10);
        break;
    case Qt::Key_Right:
        rudderPosition += 1;
        if (rudderPosition > 45) {
            rudderPosition = 45;
        }
        ui->horizontalSliderRudder->setValue(rudderPosition * 10);
        on_horizontalSliderRudder_valueChanged(rudderPosition * 10);
        break;
    default:
        QWidget::keyPressEvent(event); // pass on other keys
        break;
    }
}

void GeneratorVesselMovement::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        break;
    case Qt::Key_Down:
        break;
    case Qt::Key_Left:
        break;
    case Qt::Key_Right:
        break;
    default:
        QWidget::keyReleaseEvent(event); // pass on other keys
        break;
    }
}

void GeneratorVesselMovement::onTimerTimeout()
{
    if (m_SignalToEmit == "HeadingChanged") {
        const double requestedDegrees=m_ValueToEmit.toDouble();
        setHeading(Convert::DegreesToRadians(requestedDegrees));
        if (isManualMovementEnabled) {
            double trueDegrees=requestedDegrees;
            if (HeadingReference()==N2khr_magnetic)
                trueDegrees=Convert::MagneticToTrue(trueDegrees,Convert::RadiansToDegrees(Variation()+Deviation()));
            moveVesselSimulator->setHeading(Convert::NormalizeHeading(trueDegrees));
        }
        setCOG(Convert::DegreesToRadians(Convert::NormalizeHeading(requestedDegrees +
            (isManualMovementEnabled && m_CurrentVesselStatus.sog<0 ? 180.0 : 0.0))));
    } else if (m_SignalToEmit == "SOGChanged") {
        m_SOG = N2kHelper::convertUserSpeedToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit SOGChanged();
    } else if (m_SignalToEmit == "LatitudeChanged") {
        m_Latitude = N2kHelper::convertUserLatitudeToN2kUnits(m_ValueToEmit, UserPrefs());
        emit LatitudeChanged();
    } else if (m_SignalToEmit == "LongitudeChanged") {
        m_Longitude = N2kHelper::convertUserLongitudeToN2kUnits(m_ValueToEmit, UserPrefs());
        emit LongitudeChanged();
    } else if (m_SignalToEmit == "RateOfTurnChanged") {
        m_RateOfTurn = N2kHelper::convertUserRateOfTurnToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit RateOfTurnChanged();
    } else if (m_SignalToEmit == "RudderPositionChanged") {
        m_RudderPosition = m_ValueToEmit.toDouble();
        emit RudderPositionChanged();
    } else if (m_SignalToEmit == "PitchChanged") {
        m_Pitch = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        emit PitchChanged();
    } else if (m_SignalToEmit == "RollChanged") {
        m_Roll = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        emit RollChanged();
    } else if (m_SignalToEmit == "HeaveChanged") {
        m_Heave = N2kHelper::convertUserDistanceNearToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit HeaveChanged();
    }

    m_ValueToEmit.clear();
}

void GeneratorVesselMovement::VesselWpBearingChanged(VesselWpBearing vesselWpBearing)
{
    setDistanceToWaypoint(vesselWpBearing.DistanceToWaypointM);
    setBearingReference(static_cast<tN2kHeadingReference>(vesselWpBearing.Reference));
    setETATime(vesselWpBearing.EtaTimeS);
    setETADate(vesselWpBearing.EtaDate);
    setBearingOriginToDestinationWaypoint(vesselWpBearing.BearingOriginToDestinationWaypoint);
    setBearingPositionToDestinationWaypoint(vesselWpBearing.BearingPositionToDestinationWaypoint);
    setOriginWaypointNumber(vesselWpBearing.OriginWaypointNumber);
    setDestinationWaypointNumber(vesselWpBearing.DestinationWaypointNumber);
    setDestinationLatitude(vesselWpBearing.DestinationLatitude);
    setDestinationLongitude(vesselWpBearing.DestinationLongitude);
    setWaypointClosingVelocity(vesselWpBearing.WaypointClosingVelocity);
    setPerpendicularCrossed(vesselWpBearing.PerpendicularCrossed);
    // This callback is a produced navigation sample, not construction/restore.
    // Existing XTE bindings must also receive a new sample equal to the runtime
    // value (notably the first on-track zero or a repeated unavailable sample).
    const bool unchangedXte = qFuzzyCompare(XTE(), vesselWpBearing.Xte);
    setXTE(vesselWpBearing.Xte);
    if (unchangedXte) emit XTEChanged(); // Exactly one notification per sample.
    setArrivalCircleEntered(vesselWpBearing.ArrivalCircleEntered);

    // True for Heading
    setHeading(vesselWpBearing.HeadingRadians>=0 ? vesselWpBearing.HeadingRadians : vesselWpBearing.CourseRadians);

    // Route geometry is geographic: publish true heading and course.
    setHeadingReference(N2khr_true);
    setCOG(vesselWpBearing.CourseRadians);

    setSOG(vesselWpBearing.GroundSpeed>=0 ? vesselWpBearing.GroundSpeed : vesselWpBearing.WaypointClosingVelocity);
    if(vesselWpBearing.GroundSpeed>=0 && vesselWpBearing.WaveHeave!=N2kDoubleNA) {
        setPitch(Convert::DegreesToRadians(vesselWpBearing.WavePitch));
        setRoll(Convert::DegreesToRadians(vesselWpBearing.WaveRoll));setHeave(vesselWpBearing.WaveHeave);
        emit parametersSampled({"Pitch","Roll","Heave"});
    }
    refreshEnvironmentalStatus();

    emit parametersSampled({"Heading","COG","SOG","DistanceToWaypoint","ETATime","ETADate","XTE"});
    lastWaypointBearing = vesselWpBearing;
    refreshRouteDisplay();
}

void GeneratorVesselMovement::refreshRouteDisplay()
{
    const auto &vesselWpBearing = lastWaypointBearing;
    // Set UI elements
    ui->lineEditNextWaypoint->setText(vesselWpBearing.WaypointName);
    ui->lineEditWpLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(vesselWpBearing.DestinationLatitude, UserPrefs()));
    ui->lineEditWpLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(vesselWpBearing.DestinationLongitude, UserPrefs()));
    ui->lineEditHeadingToWp->setText(vesselWpBearing.BearingPositionToDestinationWaypoint == N2kDoubleNA ? tr("Unavailable")
        : QString::number(Convert::RadiansToDegrees(vesselWpBearing.BearingPositionToDestinationWaypoint), 'f', 1));
    ui->lineEditDistanceToWp->setText(
        Convert::formatDistanceNearFar(vesselWpBearing.DistanceToWaypointM, UserPrefs().distanceUnitIndex, ui->labelUnitsDistance));
    ui->lineEditXteToWp->setText(QString::number(N2kHelper::convertN2kDistanceNearToUserUnits(vesselWpBearing.Xte, UserPrefs()), 'f', 0));
    ui->lineEditTimeToWp->setText(vesselWpBearing.RemainingTimeS == N2kDoubleNA ? tr("Unavailable")
        : Convert::SecondsToFormattedTime_dayhhmmss(vesselWpBearing.RemainingTimeS));
    ui->lineEditDistanceTraveled->setText(
        Convert::formatDistanceNearFar(vesselWpBearing.TotalDistanceTraveledM, UserPrefs().distanceUnitIndex, ui->labelUnitsDistanceTraveled));
    ui->lineEditTimeTraveled->setText(Convert::SecondsToFormattedTime_dayhhmmss(vesselWpBearing.TimeTraveledS));
}

void GeneratorVesselMovement::LatLongChanged(double latitude, double longitude)
{
    // Signal from AutoPilotSimulator
    setLatitude(latitude);
    setLongitude(longitude);
    emit parametersSampled({"Latitude","Longitude"});
}

void GeneratorVesselMovement::initializeCompass()
{
    // Heads Up Compass
    m_compass = new Compass(this);
    m_compass->setGeometry(10, 10, 125, 125);
    ui->horizontalLayoutCompass->addWidget(m_compass);

    // Customize compass background
    m_compass->addBackground(49);
    BackgroundItem *bkgCompass1 = m_compass->addBackground(49); //49
    bkgCompass1->clearColors();
    bkgCompass1->addColor(0.0, QColor(50, 50, 50));    // Dark gray
    bkgCompass1->addColor(0.5, QColor(200, 200, 200)); // Light gray
    bkgCompass1->addColor(1.0, QColor(50, 50, 50));    // Dark gray

    BackgroundItem *bkgCompass2 = m_compass->addBackground(46); //44
    bkgCompass2->clearColors();
    bkgCompass2->addColor(0.0, QColor(200, 200, 200)); // Light gray
    bkgCompass2->addColor(0.5, QColor(50, 50, 50));    // Dark gray
    bkgCompass2->addColor(1.0, QColor(200, 200, 200)); // Light gray

    BackgroundItem *bkgCompass3 = m_compass->addBackground(42); //40
    bkgCompass3->clearColors();
    //bkgCompass3->addColor(0.0, QColor(200, 200, 200));  // Light gray
    bkgCompass3->addColor(0.0, Qt::gray); // gray

    // Set percision
    m_compass->setPrecision(1);

    // Set text colors
    m_compass->setDegreeTextColor(Qt::black);
    m_compass->setValueTextColor(Qt::red);
}

void GeneratorVesselMovement::initializeAutoPilot()
{
    autoPilotSimulator = new AutoPilotSimulator(this);

    // SOGChanged is a measurement notification (including periodic telemetry),
    // not an operator speed command. Only the selected speed-source controls
    // may change Autopilot propulsion speed.
    connect(ui->spinBoxMultSpeedByX,qOverload<int>(&QSpinBox::valueChanged),autoPilotSimulator,
            [this](int value){autoPilotSimulator->setTraversalMultiplier(value);});

    connect(autoPilotSimulator, &AutoPilotSimulator::VesselWpBearingChanged, this, &GeneratorVesselMovement::VesselWpBearingChanged);
    connect(autoPilotSimulator, &AutoPilotSimulator::LatLongChanged, this, &GeneratorVesselMovement::LatLongChanged);
    connect(autoPilotSimulator, &AutoPilotSimulator::statusMessage, this,
            [this](const QString &message){
                if(!autoPilotSimulator->isRunning() && ui->checkBoxAutoPilotEnabled->isChecked())
                    ui->checkBoxAutoPilotEnabled->click();
                emit ShowStatusBarMessage(message,15000);
            });
    connect(autoPilotSimulator, &AutoPilotSimulator::routeCompleted, this, [this] {
        if (ui->checkBoxAutoPilotEnabled->isChecked())
            ui->checkBoxAutoPilotEnabled->click();
    });
}

void GeneratorVesselMovement::clearAutoPilot()
{
    
}

void GeneratorVesselMovement::initializeMoveVesselSimulator()
{
    moveVesselSimulator = new MoveVesselSimulator(this);

    // Connect the MoveVesselSimulator signals to the GeneratorVesselMovement slots
    connect(moveVesselSimulator, &MoveVesselSimulator::vesselStatusUpdated, this, &GeneratorVesselMovement::setCurrentVesselStatus);
}

void GeneratorVesselMovement::initializeOuthers()
{
    // Set default speed to vessel cruising speed
    ui->doubleSpinBoxSpeed->setValue(m_MyVessel.CruisingSpeed);
    ui->comboBoxSpeedSource->setCurrentIndex(AutopilotSpeedSource::Manual);
}

double GeneratorVesselMovement::normalizeAxisToRudderAngle(int16_t value, bool sticky, double lastRudderPosition)
{
    constexpr int deadZone = 4096;
    if (qAbs(int(value)) <= deadZone) return sticky ? lastRudderPosition : 0.0;
    const double magnitude = double(qAbs(int(value)) - deadZone) / ((value < 0 ? 32768 : 32767) - deadZone);
    const double angle = (value < 0 ? -1.0 : 1.0) * magnitude * 45.0;
    return qBound(-45.0, sticky ? lastRudderPosition + angle : angle, 45.0);
}

double GeneratorVesselMovement::normalizeAxisToThrottle(int16_t value, bool sticky, double lastThrottlePosition)
{
    constexpr int deadZone = 4096;
    if (qAbs(int(value)) <= deadZone) return sticky ? lastThrottlePosition : 0.0;
    const double magnitude = double(qAbs(int(value)) - deadZone) / ((value < 0 ? 32768 : 32767) - deadZone) * 100.0;
    // Preserve the existing latch: push up to raise, pull down to reduce.
    return qBound(0.0, sticky ? (value < 0 ? qMax(lastThrottlePosition, magnitude) : lastThrottlePosition - magnitude)
                            : (value < 0 ? magnitude : 0.0), 100.0);
}

void GeneratorVesselMovement::connectHandleGameControllerEvents(bool connect)
{
    if (connect) {
        qDebug() << "Game Controller Initialized";
        QObject::connect(gController, &GameController::buttonPressed, this, [this](int button) {
            //qDebug() << "Button pressed:" << button;
            ui->lineEditButtonPressed->setText(QString::number(button));

            switch (button) {
            case 0:
                setTransmissionGearStbd(N2kTG_Reverse);
                break;
            case 1:
                setTransmissionGearStbd(N2kTG_Forward);
                break;
            case 2:
                setTransmissionGear(N2kTG_Reverse);
                break;
            case 3:
                setTransmissionGear(N2kTG_Forward);
                break;
            case 9:
                setTransmissionGear(N2kTG_Neutral);
                setTransmissionGearStbd(N2kTG_Neutral);
                break;
            }
        });

        QObject::connect(gController, &GameController::buttonReleased, this, [this](int button) {
            //qDebug() << "Button released:" << button;
            Q_UNUSED(button);
            ui->lineEditButtonPressed->setText("");
        });

        QObject::connect(gController, &GameController::axisMoved, this, [this](int axis, int value) {
            ui->lineEditAxis->setText(QString::number(axis));
            ui->lineEditAxisValue->setText(QString::number(value));

            double throttle = 0.0;
            if (axis == 0) {
                m_rudderPortDeg = normalizeAxisToRudderAngle(value /*, true, m_rudderPortDeg*/);
            } else if (axis == 2) {
                m_rudderStbdDeg = normalizeAxisToRudderAngle(value /*, true, m_rudderStbdDeg*/);
            } else if (axis == 1) {
                throttle = normalizeAxisToThrottle(value, true, m_ThrottlePosition);
                setThrottlePosition(throttle);
                ui->verticalSliderThrottle->setValue(throttle);
            } else if (axis == 3) {
                if (!ui->checkBoxSyncThrottle->isChecked()) {
                    throttle = normalizeAxisToThrottle(value, true, m_ThrottlePositionStbd);
                    setThrottlePositionStbd(throttle);
                    ui->verticalSliderThrottleStbd->setValue(throttle);
                }
            }

            if (axis == 0 || axis == 2) {
                double singleRudderDeg = 0.0;
                // Independent engines retain the two-rudder average. With a shared
                // helm, a centered stick must not cancel the other held stick.
                if (m_MyVessel.NumEngines > 1 && !ui->checkBoxSyncThrottle->isChecked())
                    singleRudderDeg = (m_rudderPortDeg + m_rudderStbdDeg) / 2;
                else if (m_rudderPortDeg == 0.0)
                    singleRudderDeg = m_rudderStbdDeg;
                else if (m_rudderStbdDeg == 0.0)
                    singleRudderDeg = m_rudderPortDeg;
                else
                    singleRudderDeg = (m_rudderPortDeg + m_rudderStbdDeg) / 2;
                setRudderPosition(Convert::DegreesToRadians(singleRudderDeg));
            }
        });
        QObject::connect(gController, &GameController::hatPositionChanged, this, [](int hat, uint8_t value) {
            qDebug() << "Hat" << hat << "moved to position" << value;
        });

    } else {
        QObject::disconnect(gController, &GameController::buttonPressed, nullptr, nullptr);
        QObject::disconnect(gController, &GameController::buttonReleased, nullptr, nullptr);
        QObject::disconnect(gController, &GameController::axisMoved, nullptr, nullptr);
        QObject::disconnect(gController, &GameController::hatPositionChanged, nullptr, nullptr);
    }
}

void GeneratorVesselMovement::resetVesselControls()
{
    setSOG(0.0);
    setTransmissionGear(N2kTG_Neutral);
    setTransmissionGearStbd(N2kTG_Neutral);
    setThrottlePosition(0.0);
    ui->verticalSliderThrottle->setValue(0);
    setThrottlePositionStbd(0.0);
    ui->verticalSliderThrottleStbd->setValue(0);
}

unsigned char GeneratorVesselMovement::SID() const
{
    return m_SID;
}

void GeneratorVesselMovement::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double GeneratorVesselMovement::Latitude() const
{
    return m_Latitude;
}

void GeneratorVesselMovement::setLatitude(double newLatitude)
{
    if (m_Latitude == newLatitude)
        return;
    m_Latitude = newLatitude;
    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    emit LatitudeChanged();
}

double GeneratorVesselMovement::Longitude() const
{
    return m_Longitude;
}

void GeneratorVesselMovement::setLongitude(double newLongitude)
{
    if (m_Longitude == newLongitude)
        return;
    m_Longitude = newLongitude;
    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    emit LongitudeChanged();
}

double GeneratorVesselMovement::Altitude() const
{
    return m_Altitude;
}

void GeneratorVesselMovement::setAltitude(double newAltitude)
{
    if (qFuzzyCompare(m_Altitude, newAltitude))
        return;
    m_Altitude = newAltitude;
    emit AltitudeChanged();
}

tN2kHeadingReference GeneratorVesselMovement::HeadingReference() const
{
    return m_HeadingReference;
}

void GeneratorVesselMovement::setHeadingReference(tN2kHeadingReference newHeadingReference)
{
    if (m_HeadingReference == newHeadingReference)
        return;
    m_HeadingReference = newHeadingReference;

    switch (static_cast<int>(m_HeadingReference)) {
    case 0:
        m_compass->setValueSuffix("˚T");
        break;
    case 1:
        m_compass->setValueSuffix("˚M");
        break;
    case 2:
    case 3:
        m_compass->setValueSuffix("˚");
        break;
    }

    emit HeadingReferenceChanged();
}

double GeneratorVesselMovement::Heading() const
{
    return m_Heading;
}

// TODO: Determine valid implamation for heading that works with COG that is typically magnetic heading
void GeneratorVesselMovement::setHeading(double newHeading)
{
    if (qFuzzyCompare(m_Heading, newHeading))
        return;
    m_Heading = newHeading;
    m_compass->setHeading(Convert::RadiansToDegrees(newHeading));
    const QSignalBlocker headingSliderBlocker(ui->horizontalSliderHeading);
    ui->horizontalSliderHeading->setValue(qRound(Convert::RadiansToDegrees(newHeading)*10));
    emit HeadingChanged();
}

double GeneratorVesselMovement::Deviation() const
{
    return m_Deviation;
}

void GeneratorVesselMovement::setDeviation(double newDeviation)
{
    if (qFuzzyCompare(m_Deviation, newDeviation))
        return;
    m_Deviation = newDeviation;
    emit DeviationChanged();
}

double GeneratorVesselMovement::Variation() const
{
    return m_Variation;
}

void GeneratorVesselMovement::setVariation(double newVariation)
{
    if (qFuzzyCompare(m_Variation, newVariation))
        return;
    m_Variation = newVariation;
    emit VariationChanged();
}

double GeneratorVesselMovement::COG() const
{
    return m_COG;
}

void GeneratorVesselMovement::setCOG(double newCOG)
{
    if (qFuzzyCompare(m_COG, newCOG))
        return;
    m_COG = newCOG;
    emit COGChanged();
}

double GeneratorVesselMovement::SOG() const
{
    return m_SOG;
}

void GeneratorVesselMovement::setSOG(double newSOG)
{
    //if (qFuzzyCompare(m_SOG, newSOG))
    //    return;
    m_SOG = newSOG;
    ui->lineEditSog->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_SOG, UserPrefs()), 'f', 1));
    emit SOGChanged();
}

double GeneratorVesselMovement::RateOfTurn() const
{
    return m_RateOfTurn;
}

void GeneratorVesselMovement::setRateOfTurn(double newRateOfTurn)
{
    if (qFuzzyCompare(m_RateOfTurn, newRateOfTurn))
        return;
    m_RateOfTurn = newRateOfTurn;
    int percision = 1;
    switch (UserPrefs().rateOfTurnUnitIndex) {
    case 0:
        percision = 4;
        break;
    case 1:
        percision = 1;
        break;
    }
    ui->lineEditRateOfTurn->setText(QString::number(N2kHelper::convertN2kRateOfTurnToUserUnits(m_RateOfTurn, UserPrefs()), 'f', percision));
    emit RateOfTurnChanged();
}

double GeneratorVesselMovement::Yaw() const
{
    return m_Yaw;
}

void GeneratorVesselMovement::setYaw(double newYaw)
{
    if (qFuzzyCompare(m_Yaw, newYaw))
        return;
    m_Yaw = newYaw;
    emit YawChanged();
}

double GeneratorVesselMovement::Pitch() const
{
    return m_Pitch;
}

void GeneratorVesselMovement::setPitch(double newPitch)
{
    if (qFuzzyCompare(m_Pitch, newPitch))
        return;
    m_Pitch = newPitch;
    ui->lineEditPitch->setText(QString::number(Convert::RadiansToDegrees(newPitch), 'f', 1));
    emit PitchChanged();
}

double GeneratorVesselMovement::Roll() const
{
    return m_Roll;
}

void GeneratorVesselMovement::setRoll(double newRoll)
{
    if (qFuzzyCompare(m_Roll, newRoll))
        return;
    m_Roll = newRoll;
    ui->lineEditRoll->setText(QString::number(Convert::RadiansToDegrees(newRoll), 'f', 1));
    emit RollChanged();
}

double GeneratorVesselMovement::RudderPosition() const
{
    return m_RudderPosition;
}

void GeneratorVesselMovement::setRudderPosition(double newRudderPosition)
{
    //if (qFuzzyCompare(m_RudderPosition, newRudderPosition))
    //    return;
    m_RudderPosition = newRudderPosition;
    double rudderDeg = Convert::RadiansToDegrees(newRudderPosition);
    ui->horizontalSliderRudder->setValue(rudderDeg * 10);

    emit RudderPositionChanged();
}

void GeneratorVesselMovement::on_horizontalSliderRudder_valueChanged(int value)
{
    double rudder = value * .1;
    //qDebug() << "Rudder Slider: " << rudder;
    ui->horizontalSliderRudder->setToolTip(tr("Rudder: %1°. Steer local vessel movement; center for no commanded turn.").arg(rudder, 0, 'f', 1));
    m_ValueToEmit = QString::number(Convert::DegreesToRadians(rudder));
    m_SignalToEmit = "RudderPositionChanged";
    onTimerTimeout();

    if (isManualMovementEnabled) {
        moveVesselSimulator->setRudderPositionPort(rudder);
        moveVesselSimulator->setRudderPositionStarboard(rudder);
    }
}

void GeneratorVesselMovement::on_horizontalSliderHeading_valueChanged(int value)
{
    if (ui->checkBoxAutoPilotEnabled->isChecked()) return;
    double heading = value * .1;
    ui->horizontalSliderHeading->setToolTip(tr("Heading: %1°. Set the Manual helm heading; course over ground can differ during reverse motion or environmental drift.").arg(heading, 0, 'f', 1));
    m_compass->setHeading(heading);
    m_ValueToEmit = QString::number(heading);
    m_SignalToEmit = "HeadingChanged";
    onTimerTimeout();
}

void GeneratorVesselMovement::on_lineEditLatitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLatitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LatitudeChanged";
    onTimerTimeout();
}

void GeneratorVesselMovement::on_lineEditLongitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLongitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LongitudeChanged";
    onTimerTimeout();
}

void GeneratorVesselMovement::on_lineEditSog_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditSog, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "SOGChanged";
    onTimerTimeout();
    if (autoPilotSimulator->isRunning() && ui->comboBoxSpeedSource->currentIndex() == AutopilotSpeedSource::Manual)
        autoPilotSimulator->setSpeed(m_SOG);
}

void GeneratorVesselMovement::on_lineEditRateOfTurn_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditRateOfTurn, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "RateOfTurnChanged";
    onTimerTimeout();
}

void GeneratorVesselMovement::on_lineEditPitch_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditPitch, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "PitchChanged";
    onTimerTimeout();
}

void GeneratorVesselMovement::on_lineEditRoll_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditRoll, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "RollChanged";
    onTimerTimeout();
}
// 500ms poll timer elapsed event
void GeneratorVesselMovement::PollTimerElapsed()
{
    m_PollCounter++;
    if (m_PollCounter > 255)
        m_PollCounter = 0;
    setSID(m_PollCounter);

    // Every 500ms intervals
    emit SIDChanged();
    emit LatitudeChanged();
    emit LongitudeChanged();
    emit COGChanged();
    emit SOGChanged();

    // Every 1 second intervals
    if (m_PollCounter % 2 == 0) {
        emit AltitudeChanged();
        emit HeadingChanged();
        emit RateOfTurnChanged();
        emit PitchChanged();
        emit RollChanged();
    }
    // Every 2 second intervals
    if (m_PollCounter % 4 == 0) {
        emit HeadingReferenceChanged();
        emit RudderPositionChanged();
        emit DeviationChanged();
        emit VariationChanged();
        emit YawChanged();
        emit TransmissionGearChanged();
    }
    // Every 5 second intervals
    if (m_PollCounter % 10 == 0) {
    }
    // Every 10 second intervals
    if (m_PollCounter % 20 == 0) {
    }
}

double GeneratorVesselMovement::DistanceToWaypoint() const
{
    return m_DistanceToWaypoint;
}

void GeneratorVesselMovement::setDistanceToWaypoint(double newDistanceToWaypoint)
{
    if (qFuzzyCompare(m_DistanceToWaypoint, newDistanceToWaypoint))
        return;
    m_DistanceToWaypoint = newDistanceToWaypoint;
    emit DistanceToWaypointChanged();
}

tN2kHeadingReference GeneratorVesselMovement::BearingReference() const
{
    return m_BearingReference;
}

void GeneratorVesselMovement::setBearingReference(tN2kHeadingReference newBearingReference)
{
    if (m_BearingReference == newBearingReference)
        return;
    m_BearingReference = newBearingReference;
    emit BearingReferenceChanged();
}

bool GeneratorVesselMovement::PerpendicularCrossed() const
{
    return m_PerpendicularCrossed;
}

void GeneratorVesselMovement::setPerpendicularCrossed(bool newPerpendicularCrossed)
{
    if (m_PerpendicularCrossed == newPerpendicularCrossed)
        return;
    m_PerpendicularCrossed = newPerpendicularCrossed;
    emit PerpendicularCrossedChanged();
}

bool GeneratorVesselMovement::ArrivalCircleEntered() const
{
    return m_ArrivalCircleEntered;
}

void GeneratorVesselMovement::setArrivalCircleEntered(bool newArrivalCircleEntered)
{
    if (m_ArrivalCircleEntered == newArrivalCircleEntered)
        return;
    m_ArrivalCircleEntered = newArrivalCircleEntered;
    emit ArrivalCircleEnteredChanged();
}

tN2kDistanceCalculationType GeneratorVesselMovement::CalculationType() const
{
    return m_CalculationType;
}

void GeneratorVesselMovement::setCalculationType(tN2kDistanceCalculationType newCalculationType)
{
    if (m_CalculationType == newCalculationType)
        return;
    m_CalculationType = newCalculationType;
    emit CalculationTypeChanged();
}

double GeneratorVesselMovement::ETATime() const
{
    return m_ETATime;
}

void GeneratorVesselMovement::setETATime(double newETATime)
{
    if (qFuzzyCompare(m_ETATime, newETATime))
        return;
    m_ETATime = newETATime;
    emit ETATimeChanged();
}

int16_t GeneratorVesselMovement::ETADate() const
{
    return m_ETADate;
}

void GeneratorVesselMovement::setETADate(int16_t newETADate)
{
    if (m_ETADate == newETADate)
        return;
    m_ETADate = newETADate;
    emit ETADateChanged();
}

double GeneratorVesselMovement::BearingOriginToDestinationWaypoint() const
{
    return m_BearingOriginToDestinationWaypoint;
}

void GeneratorVesselMovement::setBearingOriginToDestinationWaypoint(double newBearingOriginToDestinationWaypoint)
{
    if (qFuzzyCompare(m_BearingOriginToDestinationWaypoint, newBearingOriginToDestinationWaypoint))
        return;
    m_BearingOriginToDestinationWaypoint = newBearingOriginToDestinationWaypoint;
    emit BearingOriginToDestinationWaypointChanged();
}

double GeneratorVesselMovement::BearingPositionToDestinationWaypoint() const
{
    return m_BearingPositionToDestinationWaypoint;
}

void GeneratorVesselMovement::setBearingPositionToDestinationWaypoint(double newBearingPositionToDestinationWaypoint)
{
    if (qFuzzyCompare(m_BearingPositionToDestinationWaypoint, newBearingPositionToDestinationWaypoint))
        return;
    m_BearingPositionToDestinationWaypoint = newBearingPositionToDestinationWaypoint;
    emit BearingPositionToDestinationWaypointChanged();
}

uint32_t GeneratorVesselMovement::OriginWaypointNumber() const
{
    return m_OriginWaypointNumber;
}

void GeneratorVesselMovement::setOriginWaypointNumber(uint32_t newOriginWaypointNumber)
{
    if (m_OriginWaypointNumber == newOriginWaypointNumber)
        return;
    m_OriginWaypointNumber = newOriginWaypointNumber;
    emit OriginWaypointNumberChanged();
}

uint32_t GeneratorVesselMovement::DestinationWaypointNumber() const
{
    return m_DestinationWaypointNumber;
}

void GeneratorVesselMovement::setDestinationWaypointNumber(uint32_t newDestinationWaypointNumber)
{
    if (m_DestinationWaypointNumber == newDestinationWaypointNumber)
        return;
    m_DestinationWaypointNumber = newDestinationWaypointNumber;
    emit DestinationWaypointNumberChanged();
}

double GeneratorVesselMovement::DestinationLatitude() const
{
    return m_DestinationLatitude;
}

void GeneratorVesselMovement::setDestinationLatitude(double newDestinationLatitude)
{
    if (m_DestinationLatitude == newDestinationLatitude)
        return;
    m_DestinationLatitude = newDestinationLatitude;
    emit DestinationLatitudeChanged();
}

double GeneratorVesselMovement::DestinationLongitude() const
{
    return m_DestinationLongitude;
}

void GeneratorVesselMovement::setDestinationLongitude(double newDestinationLongitude)
{
    if (m_DestinationLongitude == newDestinationLongitude)
        return;
    m_DestinationLongitude = newDestinationLongitude;
    emit DestinationLongitudeChanged();
}

double GeneratorVesselMovement::WaypointClosingVelocity() const
{
    return m_WaypointClosingVelocity;
}

void GeneratorVesselMovement::setWaypointClosingVelocity(double newWaypointClosingVelocity)
{
    if (qFuzzyCompare(m_WaypointClosingVelocity, newWaypointClosingVelocity))
        return;
    m_WaypointClosingVelocity = newWaypointClosingVelocity;
    emit WaypointClosingVelocityChanged();
}

void GeneratorVesselMovement::updateRouteName()
{
    const auto path = ui->lineEditFilePath->text();
    const auto route = path.isEmpty() ? RouteImport::Result{} : RouteImport::read(path);
    auto *name = findChild<QLineEdit*>("selectedRouteName");
    name->setText(route && !route.name.isEmpty() ? route.name : QFileInfo(path).completeBaseName());
    name->setToolTip(path);
}

void GeneratorVesselMovement::on_pushButtonSelectRouteFile_clicked()
{
    const auto received=receivedRouteProvider ? receivedRouteProvider() : QVector<ReceivedRoutes::Route>{};
    QVector<ReceivedRoutes::Route> available;
    for(const auto &route:received)if(route.complete)available.append(route);
    if(!available.isEmpty() || activeRouteChooser) {
        bool accepted=false;
        const auto operation=QInputDialog::getItem(this,tr("Select Route"),tr("Route source"),
            {tr("Select Route File"),tr("Select Chartplotter Route")},0,false,&accepted);
        if(!accepted)return;
        if(operation==tr("Select Chartplotter Route")) {
            if(activeRouteChooser){
                const auto selected=activeRouteChooser(this);if(!selected.complete)return;
                selectedReceivedRoute=selected.waypoints;receivedRouteSelected=true;
                selectedReceivedRouteDescription=tr("Chartplotter source %1 — route %2").arg(selected.source,selected.name);
                auto *name=findChild<QLineEdit*>("selectedRouteName");name->setText(selected.name);name->setToolTip(tr("Chartplotter route: %1").arg(selected.name));
                auto *status=findChild<QLabel*>("receivedRouteStatus");
                status->setText(tr("Selected chartplotter route: %1 - %2 waypoints. Apply on the next explicit Autopilot activation.").arg(selected.name).arg(selected.waypoints.size()));status->show();
                emit ShowStatusBarMessage(status->text(),15000);return;
            }
            QStringList labels;
            for(const auto &route:available) {
                const QString identity=route.attributes.contains("DatabaseID") ? tr(" — database %1 / route %2").arg(route.attributes.value("DatabaseID"),route.attributes.value("RouteID")) : QString();
                labels.append(tr("%1 — source %2%3 — %4 waypoints").arg(route.name,route.source,identity).arg(route.waypoints.size()));
            }
            const auto label=QInputDialog::getItem(this,tr("Select Chartplotter Route"),tr("Complete received routes"),labels,0,false,&accepted);
            if(!accepted)return;
            const int index=labels.indexOf(label);if(index<0)return;
            bool stillValid=false;
            const auto current=receivedRouteProvider ? receivedRouteProvider() : QVector<ReceivedRoutes::Route>{};
            for(const auto &route:current)if(route.complete && route.id==available[index].id && route.attributes==available[index].attributes && route.waypoints.size()==available[index].waypoints.size()) {
                bool same=true;for(int i=0;i<route.waypoints.size();++i)if(route.waypoints[i].coordinate!=available[index].waypoints[i].coordinate || route.waypoints[i].name!=available[index].waypoints[i].name || route.waypoints[i].speedMs!=available[index].waypoints[i].speedMs)same=false;
                if(same)stillValid=true;
            }
            if(!stillValid) {QMessageBox::warning(this,tr("Route changed"),tr("The received route changed or expired. Open Select Route again. Your existing selection is unchanged."));return;}
            selectedReceivedRoute=available[index].waypoints;receivedRouteSelected=true;
            selectedReceivedRouteDescription=tr("Chartplotter snapshot: %1").arg(label);
            auto *name=findChild<QLineEdit*>("selectedRouteName");name->setText(available[index].name);name->setToolTip(label);
            auto *status=findChild<QLabel*>("receivedRouteStatus");
            status->setText(tr("Selected chartplotter route: %1. This snapshot is not saved in Layouts.").arg(label));status->show();
            emit ShowStatusBarMessage(tr("Chartplotter route selected: %1. Current navigation is unchanged; apply on the next explicit Autopilot activation.").arg(label),15000);
            return;
        }
    }
    QString error;
    // Explicit browse prepares a writable user location, never an installed/source directory.
    if (!AppPaths::ensureDirectory(AppPaths::routesDirectory(), &error))
        emit ShowStatusBarMessage(error, 15000);
    QFileDialog dialog(this, tr("Select Route File"), AppPaths::routeDialogDirectory(ui->lineEditFilePath->text()),
                       RouteImport::fileFilter());
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::HideNameFilterDetails);
    const auto inheritedSidebar = dialog.sidebarUrls();
    // Keep this chooser deterministic without rewriting the desktop's shared
    // Qt bookmarks when QFileDialog persists its state on destruction.
    const auto restoreSidebar = qScopeGuard([&] { dialog.setSidebarUrls(inheritedSidebar); });
    dialog.setSidebarUrls(AppPaths::routeSidebarUrls());
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
        const auto path = dialog.selectedFiles().first();
        const auto candidate = RouteImport::read(path);
        if (!candidate) {
            emit ShowStatusBarMessage(tr("Route import failed: %1").arg(candidate.error), 15000);
            QMessageBox::warning(this, tr("Route Import"), candidate.error);
            return;
        }
        selectedReceivedRoute.clear();
        receivedRouteSelected=false;selectedReceivedRouteDescription.clear();
        findChild<QLabel*>("receivedRouteStatus")->hide();
        ui->lineEditFilePath->setText(path);
        updateRouteName();
        emit ShowStatusBarMessage(tr("Route selected: %1 waypoints (%2). Enable Autopilot to start.")
                                     .arg(candidate.waypoints.size()).arg(candidate.selection), 10000);
    }

}

RouteExport::Snapshot GeneratorVesselMovement::selectedRouteForExport(QString *error) const
{
    RouteExport::Snapshot snapshot;
    if (receivedRouteSelected) {
        snapshot.name=findChild<QLineEdit*>("selectedRouteName")->text();
        snapshot.waypoints=selectedReceivedRoute;
        snapshot.sourceDescription=selectedReceivedRouteDescription;
    } else {
        snapshot.sourceFile=ui->lineEditFilePath->text();
        if (snapshot.sourceFile.isEmpty()) { *error=tr("Select a route before exporting.");return {}; }
        const auto route=RouteImport::read(snapshot.sourceFile);
        if (!route) { *error=route.error;return {}; }
        snapshot.pointSequence=route.pointSequence;
        snapshot.name=route.name.isEmpty()?QFileInfo(snapshot.sourceFile).completeBaseName():route.name;
        snapshot.waypoints=route.waypoints;
        snapshot.sourceDescription=tr("Route file: %1").arg(snapshot.sourceFile);
    }
    *error=RouteExport::validate(snapshot);
    return snapshot;
}

void GeneratorVesselMovement::exportSelectedRoute()
{
    QString error;
    auto snapshot=selectedRouteForExport(&error);
    const auto notify=[this](const QString &message) {
        emit ShowStatusBarMessage(tr("Route export: %1").arg(message),15000);
        QMessageBox box(QMessageBox::Warning,tr("Export Route"),message,QMessageBox::Ok,window());
        box.setTextFormat(Qt::PlainText);box.exec();
    };
    if (!error.isEmpty()) { notify(error);return; }
    QPointer<GeneratorVesselMovement> guard(this);
    if (snapshot.pointSequence) {
        QMessageBox consent(QMessageBox::Question,tr("Export waypoint sequence as route?"),
            tr("This KML contains map pins without an ordered path. Export these points as a route in document order?"),
            QMessageBox::Yes|QMessageBox::No,window());
        consent.setDefaultButton(QMessageBox::No);
        if (consent.exec()!=QMessageBox::Yes || !guard) return;
    }
    const QString gpxFilter=tr("GPX route (*.gpx)"),kmlFilter=tr("KML route (*.kml)");
    QPointer<QFileDialog> dialog=new QFileDialog(window(),tr("Export Route — %1").arg(snapshot.sourceDescription),
        AppPaths::routesDirectory()+"/route.gpx");
    dialog->setObjectName("exportRouteFileDialog");
    dialog->setAcceptMode(QFileDialog::AcceptSave);dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setOption(QFileDialog::DontUseNativeDialog);
    dialog->setOption(QFileDialog::DontConfirmOverwrite);
    dialog->setNameFilters({gpxFilter,kmlFilter});
    connect(this,&QObject::destroyed,dialog,&QDialog::reject);
    int accepted;
    QStringList files;
    QString filter;
    {
        // Match Select Route File: use curated shortcuts for this dialog only,
        // preserving Qt's shared bookmarks when the dialog saves its state.
        const auto inheritedSidebar=dialog->sidebarUrls();
        const auto restoreSidebar=qScopeGuard([&]{if(dialog)dialog->setSidebarUrls(inheritedSidebar);});
        dialog->setSidebarUrls(AppPaths::routeSidebarUrls());
        accepted=dialog->exec();
        if (!dialog) return;
        files=dialog->selectedFiles();filter=dialog->selectedNameFilter();
    }
    delete dialog;
    if (!guard || accepted!=QDialog::Accepted || files.isEmpty()) return;
    const auto format=filter==kmlFilter?RouteExport::Format::Kml:RouteExport::Format::Gpx;
    QString path=files.first();const auto extension=RouteExport::suffix(format);
    if (QFileInfo(path).suffix().isEmpty()) path+="."+extension;
    else if (QFileInfo(path).suffix().compare(extension,Qt::CaseInsensitive)!=0) {
        notify(tr("The filename suffix must match the selected %1 format. No file was written.").arg(extension.toUpper()));return;
    }
    const auto result=RouteExport::serialize(snapshot,format);
    if (!result) { notify(result.error);return; }
    const auto destination=RouteExport::inspectDestination(path,snapshot.sourceFile);
    if (!destination) { notify(destination.error);return; }
    if (destination.exists) {
        QMessageBox box(QMessageBox::Question,tr("Replace export file?"),
            tr("Source: %1\nReplace this destination?\n%2").arg(snapshot.sourceDescription,destination.path),
            QMessageBox::Yes|QMessageBox::No,window());
        box.setTextFormat(Qt::PlainText);box.setDefaultButton(QMessageBox::No);
        if (box.exec()!=QMessageBox::Yes || !guard) return;
    }
    error=RouteExport::writeAtomic(destination,result.bytes,snapshot.sourceFile);
    if (!error.isEmpty()) { notify(error);return; }
    emit ShowStatusBarMessage(tr("Exported %1 waypoints to %2. Route selection, movement and Sending are unchanged.")
        .arg(snapshot.waypoints.size()).arg(destination.path),15000);
}

void GeneratorVesselMovement::on_checkBoxAutoPilotEnabled_clicked(bool checked)
{
    const auto notifyControl=qScopeGuard([this]{emit navigationControlChanged();});
    const QString readinessMessage = checked && engineReadinessCheck ? engineReadinessCheck() : QString();
    if (!readinessMessage.isEmpty()) {
        ui->checkBoxAutoPilotEnabled->setChecked(false);
        emit ShowStatusBarMessage(readinessMessage, 5000);
        return;
    }
    if (checked) {
        if(!selectedReceivedRoute.isEmpty()) {
            if(!autoPilotSimulator->loadRoute(selectedReceivedRoute)) {
                ui->checkBoxAutoPilotEnabled->setChecked(autoPilotSimulator->isRunning());
                emit ShowStatusBarMessage(autoPilotSimulator->lastRouteError(),15000);return;
            }
        } else {
        const auto candidate = RouteImport::read(ui->lineEditFilePath->text());
        if (!candidate) {
            ui->checkBoxAutoPilotEnabled->setChecked(autoPilotSimulator->isRunning());
            emit ShowStatusBarMessage(tr("Route import failed: %1").arg(candidate.error), 15000);
            return;
        }
        if (candidate.pointSequence && QMessageBox::question(this, tr("Confirm Point Waypoint Route"),
                tr("This KML contains %1 Point markers, not an ordered path. Use these markers in document order as a route? "
                   "Choose No for unrelated map annotations.").arg(candidate.waypoints.size()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            ui->checkBoxAutoPilotEnabled->setChecked(autoPilotSimulator->isRunning());
            return;
        }
        if (!autoPilotSimulator->loadRoute(ui->lineEditFilePath->text(), candidate.pointSequence)) {
            ui->checkBoxAutoPilotEnabled->setChecked(autoPilotSimulator->isRunning());
            emit ShowStatusBarMessage(tr("Route import failed: %1").arg(autoPilotSimulator->lastRouteError()), 15000);
            return;
        }
        }
        autoPilotSimulator->setReverseRoute(ui->checkBoxReverseRoute->isChecked());
        if (ui->comboBoxSpeedSource->currentIndex() == AutopilotSpeedSource::Manual) {
            autoPilotSimulator->setSpeedSource(AutopilotSpeedSource::Manual);
            autoPilotSimulator->setSpeed(manualSpeedMps);
        } else if (ui->comboBoxSpeedSource->currentIndex() == AutopilotSpeedSource::ThrottlePosition) {
            autoPilotSimulator->setSpeedSource(AutopilotSpeedSource::ThrottlePosition);
            autoPilotSimulator->setSpeed(Convert::ThrottleDegreesToSpeedMs(ThrottlePosition(), m_MyVessel.WotSpeed));
        } else if (ui->comboBoxSpeedSource->currentIndex() == AutopilotSpeedSource::RouteFile) {
            autoPilotSimulator->setSpeedSource(AutopilotSpeedSource::RouteFile);
        }
        if (ui->checkBoxGamePadEnabled->isChecked()) {
            ui->checkBoxGamePadEnabled->setChecked(false);
            on_checkBoxGamePadEnabled_clicked(false);
        }
        ui->checkBoxGamePadEnabled->setEnabled(false);
        setTransmissionGear(N2kTG_Forward);
        //emit InTripStateChanged(true);
        //qDebug() << "Autopilot InTripStateChanged true emitted";
        emit ShowStatusBarMessage("Starting Autopilot Simulator...", 5000);

        // Emit to other widgets that the generator vessel is in use
        setStartEngineEnabled(true);
        emit UseGenVessel(true);

        // Use Sync Throttle Position
        m_SyncThrottleLastState = ui->checkBoxSyncThrottle->isChecked();
        ui->checkBoxSyncThrottle->setChecked(true);
        on_checkBoxSyncThrottle_clicked(true);

        // Disable manual UI elements
        ui->horizontalSliderHeading->setEnabled(false);
        ui->horizontalSliderRudder->setEnabled(false);
        ui->checkBoxSyncThrottle->setEnabled(false);

        autoPilotSimulator->start();
    } else {
        autoPilotSimulator->stop();
        ui->checkBoxGamePadEnabled->setEnabled(ui->comboBoxGamePadDevice->currentText() != "None Found");
        //emit InTripStateChanged(false);
        //qDebug() << "Autopilot InTripStateChanged false emitted";
        emit ShowStatusBarMessage("Autopilot Simulator stopped.", 5000);
        setStartEngineEnabled(true);

        resetVesselControls();
        // Re-set Sync Throttle Position
        ui->checkBoxSyncThrottle->setChecked(m_SyncThrottleLastState);
        on_checkBoxSyncThrottle_clicked(m_SyncThrottleLastState);

        // Re-enable manual UI elements
        ui->horizontalSliderHeading->setEnabled(true);
        ui->horizontalSliderRudder->setEnabled(true);
        ui->checkBoxSyncThrottle->setEnabled(true);
    }

    setNavigationTerminated(!checked);

    // Enable/Disable manual movement enabled checkbox
    ui->checkBoxManualMovementEnabled->setEnabled(!checked);
}

void GeneratorVesselMovement::on_doubleSpinBoxSpeed_valueChanged(double arg1)
{
    manualSpeedMps = N2kHelper::convertUserSpeedToN2kUnits(arg1, UserPrefs());
    m_ValueToEmit = QString::number(arg1);
    m_SignalToEmit = "SOGChanged";
    onTimerTimeout();

    // Autopiliot is running and speedSource is Manual set AutoPilotSimulator speed
    if (ui->checkBoxAutoPilotEnabled->isChecked() && ui->comboBoxSpeedSource->currentIndex() == AutopilotSpeedSource::Manual) {
        autoPilotSimulator->setSpeed(N2kHelper::convertUserSpeedToN2kUnits(arg1, UserPrefs()));
    }
}

void GeneratorVesselMovement::on_checkBoxReverseRoute_clicked(bool checked)
{
    autoPilotSimulator->setReverseRoute(checked);
}

bool GeneratorVesselMovement::NavigationTerminated() const
{
    return m_NavigationTerminated;
}

void GeneratorVesselMovement::setNavigationTerminated(bool newNavigationTerminated)
{
    if (m_NavigationTerminated == newNavigationTerminated)
        return;
    m_NavigationTerminated = newNavigationTerminated;
    emit NavigationTerminatedChanged();
}

double GeneratorVesselMovement::XTE() const
{
    return m_XTE;
}

void GeneratorVesselMovement::setXTE(double newXTE)
{
    if (qFuzzyCompare(m_XTE, newXTE))
        return;
    m_XTE = newXTE;
    emit XTEChanged();
}

tN2kTransmissionGear GeneratorVesselMovement::TransmissionGear() const
{
    return m_TransmissionGear;
}

void GeneratorVesselMovement::setTransmissionGear(tN2kTransmissionGear newTransmissionGear)
{
    // Single engine transmission gear or port engine transmission gear for dual engine
    //    if (m_TransmissionGear == newTransmissionGear)
    //        return;
    m_TransmissionGear = newTransmissionGear;
    switch (static_cast<int>(m_TransmissionGear)) {
    case N2kTG_Forward:
        ui->radioButtonForwardPort->setChecked(true);
        ui->radioButtonNeutralPort->setChecked(false);
        ui->radioButtonReversePort->setChecked(false);
        break;
    case N2kTG_Neutral:
        ui->radioButtonNeutralPort->setChecked(true);
        ui->radioButtonForwardPort->setChecked(false);
        ui->radioButtonReversePort->setChecked(false);
        break;
    case N2kTG_Reverse:
        ui->radioButtonReversePort->setChecked(true);
        ui->radioButtonForwardPort->setChecked(false);
        ui->radioButtonNeutralPort->setChecked(false);
        break;
    }

    if (isManualMovementEnabled) {
        moveVesselSimulator->setGearPort(newTransmissionGear);
    }

    emit TransmissionGearChanged();
}

tN2kTransmissionGear GeneratorVesselMovement::TransmissionGearStbd() const
{
    return m_TransmissionGearStbd;
}

void GeneratorVesselMovement::setTransmissionGearStbd(tN2kTransmissionGear newTransmissionGearStbd)
{
    //    if (m_TransmissionGearStbd == newTransmissionGearStbd)
    //        return;
    m_TransmissionGearStbd = newTransmissionGearStbd;
    switch (static_cast<int>(m_TransmissionGearStbd)) {
    case N2kTG_Forward:
        ui->radioButtonForwardStbd->setChecked(true);
        ui->radioButtonNeutralStbd->setChecked(false);
        ui->radioButtonReverseStbd->setChecked(false);
        break;
    case N2kTG_Neutral:
        ui->radioButtonNeutralStbd->setChecked(true);
        ui->radioButtonForwardStbd->setChecked(false);
        ui->radioButtonReverseStbd->setChecked(false);
        break;
    case N2kTG_Reverse:
        ui->radioButtonReverseStbd->setChecked(true);
        ui->radioButtonForwardStbd->setChecked(false);
        ui->radioButtonNeutralStbd->setChecked(false);
    }

    if (isManualMovementEnabled) {
        moveVesselSimulator->setGearStarboard(newTransmissionGearStbd);
    }

    emit TransmissionGearStbdChanged();
}

void GeneratorVesselMovement::on_radioButtonForwardPort_clicked()
{
    setTransmissionGear(N2kTG_Forward);
}

void GeneratorVesselMovement::on_radioButtonNeutralPort_clicked()
{
    setTransmissionGear(N2kTG_Neutral);
}

void GeneratorVesselMovement::on_radioButtonReversePort_clicked()
{
    setTransmissionGear(N2kTG_Reverse);
}

void GeneratorVesselMovement::on_radioButtonForwardStbd_clicked()
{
    setTransmissionGearStbd(N2kTG_Forward);
}

void GeneratorVesselMovement::on_radioButtonNeutralStbd_clicked()
{
    setTransmissionGearStbd(N2kTG_Neutral);
}

void GeneratorVesselMovement::on_radioButtonReverseStbd_clicked()
{
    setTransmissionGearStbd(N2kTG_Reverse);
}

VesselStatus GeneratorVesselMovement::CurrentVesselStatus() const
{
    return m_CurrentVesselStatus;
}

void GeneratorVesselMovement::setCurrentVesselStatus(const VesselStatus &newCurrentVesselStatus)
{
    const QStringList sampled={"Latitude","Longitude","Heading","COG","SOG","Pitch","Roll","Heave","RudderPosition","RateOfTurn"};
    if (m_CurrentVesselStatus == newCurrentVesselStatus) { emit parametersSampled(sampled); return; }
    m_CurrentVesselStatus = newCurrentVesselStatus;

    //qDebug() << "Latitude: " << newCurrentVesselStatus.latitude << " Longitude: " << newCurrentVesselStatus.longitude;
    setLatitude(newCurrentVesselStatus.latitude);
    setLongitude(newCurrentVesselStatus.longitude);
    // MoveVesselSimulator returns true heading in degrees, NMEA 2000 base unit is radians
    const double headingDeg = newCurrentVesselStatus.heading;
    setHeadingReference(N2khr_true);
    setHeading(Convert::DegreesToRadians(headingDeg));
    const double courseDeg = Convert::NormalizeHeading(headingDeg + (newCurrentVesselStatus.sog < 0 ? 180.0 : 0.0));
    setCOG(newCurrentVesselStatus.groundSpeed>=0 ? (newCurrentVesselStatus.groundCourse>=0 ? Convert::DegreesToRadians(newCurrentVesselStatus.groundCourse) : N2kDoubleNA) : Convert::DegreesToRadians(courseDeg));
    setSOG(newCurrentVesselStatus.groundSpeed>=0 ? newCurrentVesselStatus.groundSpeed : std::abs(newCurrentVesselStatus.sog));
    refreshEnvironmentalStatus();
    setPitch(Convert::DegreesToRadians(newCurrentVesselStatus.pitch));
    setRoll(Convert::DegreesToRadians(newCurrentVesselStatus.roll));
    setHeave(newCurrentVesselStatus.heave);
    setRudderPosition(Convert::DegreesToRadians(newCurrentVesselStatus.rudderPosition));
    setRateOfTurn(Convert::DegreesPerSecondToRadiansPerSecond(newCurrentVesselStatus.rateOfTurn));

    emit parametersSampled(sampled);
    emit CurrentVesselStatusChanged();
}

void GeneratorVesselMovement::on_checkBoxManualMovementEnabled_clicked(bool checked)
{
    const auto notifyControl=qScopeGuard([this]{emit navigationControlChanged();});
    const QString readinessMessage = checked && engineReadinessCheck ? engineReadinessCheck() : QString();
    if (!readinessMessage.isEmpty()) {
        ui->checkBoxManualMovementEnabled->setChecked(false);
        emit ShowStatusBarMessage(readinessMessage, 5000);
        return;
    }
    if (checked) {
        emit ShowStatusBarMessage("Starting Vessel Movement Simulator...", 5000);
        // MoveVesselSimulator uses true heading in degrees, NMEA 2000 base unit is radians
        double headingDeg = Convert::RadiansToDegrees(Heading());
        if (HeadingReference() == N2khr_magnetic)
            headingDeg = Convert::MagneticToTrue(headingDeg, Convert::RadiansToDegrees(Variation() + Deviation()));
        headingDeg = Convert::NormalizeHeading(headingDeg);
        moveVesselSimulator->setMyVessel(m_MyVessel);
        moveVesselSimulator->setHeading(headingDeg); // Compass heading is magnetic
        moveVesselSimulator->setLastLatLong(Latitude(), Longitude());
        moveVesselSimulator->setGearPort(TransmissionGear());
        moveVesselSimulator->setGearStarboard(TransmissionGearStbd());
        moveVesselSimulator->setThrottlePositionPort(ThrottlePosition());
        moveVesselSimulator->setThrottlePositionStarboard(ThrottlePositionStbd());
        double rudderDeg = Convert::RadiansToDegrees(RudderPosition());
        moveVesselSimulator->setRudderPositionPort(rudderDeg);
        moveVesselSimulator->setRudderPositionStarboard(rudderDeg);
        moveVesselSimulator->setEnabled(true);
        isManualMovementEnabled = true;
        setStartEngineEnabled(true);
        emit UseGenVessel(true);
    } else {
        emit ShowStatusBarMessage("Vessel Movement Simulator stopped.", 5000);
        moveVesselSimulator->setEnabled(false);
        isManualMovementEnabled = false;
        if (ui->checkBoxGamePadEnabled->isChecked()) {
            ui->checkBoxGamePadEnabled->setChecked(false);
            on_checkBoxGamePadEnabled_clicked(false);
        }
        setStartEngineEnabled(false);
        resetVesselControls();
        emit UseGenVessel(false);
    }

    // Enable/Disable auto pilot enabled checkbox
    ui->checkBoxAutoPilotEnabled->setEnabled(!checked);
}

double GeneratorVesselMovement::ThrottlePosition() const
{
    return m_ThrottlePosition;
}

void GeneratorVesselMovement::setThrottlePosition(double newThrottlePosition)
{
    if (qFuzzyCompare(m_ThrottlePosition, newThrottlePosition))
        return;
    m_ThrottlePosition = newThrottlePosition;

    if (isManualMovementEnabled) {
        moveVesselSimulator->setThrottlePositionPort(newThrottlePosition);
    }

    // Autopiliot is running and speedSource is ThrottlePosition set AutoPilotSimulator speed
    if (ui->checkBoxAutoPilotEnabled->isChecked() && ui->comboBoxSpeedSource->currentIndex() == AutopilotSpeedSource::ThrottlePosition) {
        autoPilotSimulator->setSpeed(Convert::ThrottleDegreesToSpeedMs(newThrottlePosition, m_MyVessel.WotSpeed));
    }

    emit ThrottlePositionChanged();
}

double GeneratorVesselMovement::ThrottlePositionStbd() const
{
    return m_ThrottlePositionStbd;
}

void GeneratorVesselMovement::setThrottlePositionStbd(double newThrottlePositionStbd)
{
    if (qFuzzyCompare(m_ThrottlePositionStbd, newThrottlePositionStbd))
        return;
    m_ThrottlePositionStbd = newThrottlePositionStbd;

    if (isManualMovementEnabled) {
        moveVesselSimulator->setThrottlePositionStarboard(newThrottlePositionStbd);
    }

    emit ThrottlePositionStbdChanged();
}

void GeneratorVesselMovement::on_verticalSliderThrottle_valueChanged(int value)
{
    ui->verticalSliderThrottle->setToolTip(tr("Port throttle: %1%. Controls local engine generation; Sync links starboard.").arg(value));
    setThrottlePosition(value);

    // Sync throttle positions
    if (ui->checkBoxSyncThrottle->isChecked()) {
        ui->verticalSliderThrottleStbd->setValue(value);
        on_verticalSliderThrottleStbd_valueChanged(value);
    }
}

void GeneratorVesselMovement::on_verticalSliderThrottleStbd_valueChanged(int value)
{
    ui->verticalSliderThrottleStbd->setToolTip(tr("Starboard throttle: %1%. Disable Sync for independent adjustment.").arg(value));
    setThrottlePositionStbd(value);
}

bool GeneratorVesselMovement::StartEngineEnabled() const
{
    return m_StartEngineEnabled;
}

void GeneratorVesselMovement::setStartEngineEnabled(bool newStartEngineEnabled)
{
    if (m_StartEngineEnabled == newStartEngineEnabled)
        return;
    m_StartEngineEnabled = newStartEngineEnabled;
    emit StartEngineEnabledChanged();
}

void GeneratorVesselMovement::setInitialGuiValues()
{
    refreshRouteDisplay();
    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    ui->lineEditRateOfTurn->setText(QString::number(N2kHelper::convertN2kRateOfTurnToUserUnits(m_RateOfTurn, UserPrefs()), 'f', 1));
    ui->lineEditSog->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_SOG, UserPrefs()), 'f', 1));
    ui->lineEditRateOfTurn->setText(QString::number(N2kHelper::convertN2kRateOfTurnToUserUnits(m_RateOfTurn, UserPrefs()), 'f', 1));
    ui->lineEditPitch->setText(QString::number(Convert::RadiansToDegrees(m_Pitch), 'f', 1));
    ui->lineEditRoll->setText(QString::number(Convert::RadiansToDegrees(m_Roll), 'f', 1));
    const QSignalBlocker speedBlocker(ui->doubleSpinBoxSpeed);
    ui->doubleSpinBoxSpeed->setValue(N2kHelper::convertN2kSpeedToUserUnits(manualSpeedMps, UserPrefs()));

    // Set single or Dual engine UI elements
    if (m_MyVessel.NumEngines == 1) {
        ui->radioButtonForwardStbd->setVisible(false);
        ui->radioButtonNeutralStbd->setVisible(false);
        ui->radioButtonReverseStbd->setVisible(false);
        ui->labelGearStbd->setVisible(false);
        ui->checkBoxSyncThrottle->setVisible(false);
        ui->verticalSliderThrottleStbd->setVisible(false);
        ui->labelGearPort->setText("Gear");
        ui->labelGearPort->setAlignment(Qt::AlignLeft);
        ui->labelThrottleStbd->setVisible(false);

    } else { // TODO: Below are defalt widget values, probably don't need to set them. Remove if not needed.
        ui->radioButtonForwardStbd->setVisible(true);
        ui->radioButtonNeutralStbd->setVisible(true);
        ui->radioButtonReverseStbd->setVisible(true);
        ui->labelGearStbd->setVisible(true);
        ui->checkBoxSyncThrottle->setVisible(true);
        ui->verticalSliderThrottleStbd->setVisible(true);
        ui->labelGearPort->setText("Port");
        ui->labelGearPort->setAlignment(Qt::AlignHCenter);
        ui->labelThrottleStbd->setVisible(true);
    }
}

void GeneratorVesselMovement::on_checkBoxSyncThrottle_clicked(bool checked)
{
    ui->verticalSliderThrottleStbd->setEnabled(!checked);

    // Sync throttle positions
    if (checked) {
        ui->verticalSliderThrottleStbd->setValue(ui->verticalSliderThrottle->value());
        setThrottlePositionStbd(ui->verticalSliderThrottle->value());
    }
}

void GeneratorVesselMovement::initializeComboBoxes()
{
    if (ui->comboBoxSpeedSource->count() == 0) ui->comboBoxSpeedSource->addItems(m_SpeedSources);

    // Populate gamepad devices and enable/disable checkbox
    QStringList gameDevices = gController->availableControllers();
    if (gameDevices.isEmpty()) {
        ui->comboBoxGamePadDevice->addItem("None Found");
        ui->checkBoxGamePadEnabled->setEnabled(false);
    } else {
        ui->comboBoxGamePadDevice->addItems(gameDevices);
        ui->checkBoxGamePadEnabled->setEnabled(!ui->checkBoxAutoPilotEnabled->isChecked());
    }
}

void GeneratorVesselMovement::on_checkBoxGamePadEnabled_clicked(bool checked)
{
    if (checked && ui->checkBoxAutoPilotEnabled->isChecked()) {
        ui->checkBoxGamePadEnabled->setChecked(false);
        emit ShowStatusBarMessage("Stop Autopilot before enabling the gamepad.", 5000);
        return;
    }
    if (checked && ui->comboBoxGamePadDevice->currentText() != "None Found"
        && !ui->checkBoxAutoPilotEnabled->isChecked()) {
        if (gController->open(ui->comboBoxGamePadDevice->currentIndex())) {
            resetVesselControls();
            m_rudderPortDeg = m_rudderStbdDeg = 0.0;
            setRudderPosition(0.0);
            connectHandleGameControllerEvents(false);
            connectHandleGameControllerEvents(true);
            emit ShowStatusBarMessage("Gamepad enabled: center both sticks and release buttons before driving.", 5000);
            ui->groupBoxGamePadController->setEnabled(true);
        } else {
            ui->checkBoxGamePadEnabled->setChecked(false);
            on_checkBoxGamePadEnabled_clicked(false);
            emit ShowStatusBarMessage("Gamepad failed to open: refresh the device list and try again.", 5000);
        }
    } else {
        gController->close();
        connectHandleGameControllerEvents(false);
        ui->checkBoxGamePadEnabled->setChecked(false);
        if (isManualMovementEnabled) ui->checkBoxManualMovementEnabled->click();
        resetVesselControls();
        m_rudderPortDeg = m_rudderStbdDeg = 0.0;
        setRudderPosition(0.0);
        ui->lineEditButtonPressed->clear();
        ui->lineEditAxis->clear();
        ui->lineEditAxisValue->clear();
        emit ShowStatusBarMessage("GamePad disabled.", 5000);
        ui->groupBoxGamePadController->setEnabled(false);
    }
}

void GeneratorVesselMovement::on_pushButtonRefreshGameList_clicked()
{
    if (ui->checkBoxGamePadEnabled->isChecked()) {
        ui->checkBoxGamePadEnabled->setChecked(false);
        on_checkBoxGamePadEnabled_clicked(false);
    }
    ui->comboBoxGamePadDevice->clear();
    initializeComboBoxes();
}

double GeneratorVesselMovement::Heave() const
{
    return m_Heave;
}

void GeneratorVesselMovement::setHeave(double newHeave)
{
    if (qFuzzyCompare(m_Heave, newHeave))
        return;
    m_Heave = newHeave;
    ui->lineEditHeave->setText(QString::number(N2kHelper::convertN2kDistanceNearToUserUnits(newHeave, UserPrefs()), 'f', 2));
    emit HeaveChanged();
}

void GeneratorVesselMovement::on_lineEditHeave_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditHeave, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "HeaveChanged";
    onTimerTimeout();
}

void GeneratorVesselMovement::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

QJsonObject GeneratorVesselMovement::layoutConfiguration() const
{
    return {{"route",ui->lineEditFilePath->text()}, {"reverse",ui->checkBoxReverseRoute->isChecked()},
            {"speedSource",ui->comboBoxSpeedSource->currentIndex()},
            {"speed",manualSpeedMps}, {"autopilotMultiplier",ui->spinBoxMultSpeedByX->value()}};
}
void GeneratorVesselMovement::restoreLayoutConfiguration(const QJsonObject &configuration)
{
    selectedReceivedRoute.clear();receivedRouteSelected=false;selectedReceivedRouteDescription.clear();findChild<QLabel*>("receivedRouteStatus")->hide();
    ui->lineEditFilePath->setText(configuration["route"].toString());
    updateRouteName();
    ui->checkBoxReverseRoute->setChecked(configuration["reverse"].toBool());
    ui->comboBoxSpeedSource->setCurrentIndex(configuration["speedSource"].toInt());
    ui->doubleSpinBoxSpeed->setValue(N2kHelper::convertN2kSpeedToUserUnits(configuration["speed"].toDouble(),UserPrefs()));
    manualSpeedMps = configuration["speed"].toDouble();
    const int multiplier=configuration["autopilotMultiplier"].toInt(1);
    ui->spinBoxMultSpeedByX->setValue(multiplier>=1 && multiplier<=10 ? multiplier : 1);
}
void GeneratorVesselMovement::stopOperationalActivity()
{
    const auto notifyControl=qScopeGuard([this]{emit navigationControlChanged();});
    if(ui->checkBoxAutoPilotEnabled->isChecked())ui->checkBoxAutoPilotEnabled->click();
    if(ui->checkBoxManualMovementEnabled->isChecked())ui->checkBoxManualMovementEnabled->click();
    if(ui->checkBoxGamePadEnabled->isChecked())ui->checkBoxGamePadEnabled->click();
    autoPilotSimulator->stop(); moveVesselSimulator->setEnabled(false); isManualMovementEnabled=false;
    setStartEngineEnabled(false); resetVesselControls();
}

bool GeneratorVesselMovement::navigationControlActive() const
{
    return isEnabled() && (!property("GeneratorModeActive").isValid() || property("GeneratorModeActive").toBool())
        && (isManualMovementEnabled || autoPilotSimulator->isRunning());
}
