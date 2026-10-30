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

#include "generatortranslowerunit.h"
#include "qmessagebox.h"
#include "ui_generatortranslowerunit.h"

#include <convert.h>
#include <n2khelper.h>

GeneratorTransLowerUnit::GeneratorTransLowerUnit(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::GeneratorTransLowerUnit)
{
    ui->setupUi(this);
    setId(id);
    Q_UNUSED(label);
    setPgn(-127493);
    setColumnSpan(3);
    setRowSpan(6);
    setMinimumWidth(ColumnSpan() * 100);

    ui->checkBoxEnabled->setToolTip(tr("Enable signals from this generator. Outbound transport output still requires explicit Enable Sending."));
    ui->radioButtonForward->setToolTip(tr("Set this transmission instance to forward gear; vessel motion also depends on the associated engine."));
    ui->radioButtonNeutral->setToolTip(tr("Set this transmission instance to neutral. This does not stop Sending."));
    ui->radioButtonReverse->setToolTip(tr("Set this transmission instance to reverse gear; this is local simulation."));
    ui->horizontalSliderLoad->setToolTip(tr("Set this transmission load indication. This does not authorize Sending."));

    initializeUnitsLimits();
    connect(this, &GeneratorTransLowerUnit::UserPrefsChanged, this, &GeneratorTransLowerUnit::initializeUnitsLimits);

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    //ui->pushButtonRemove->setIcon(iconExit);

    // Initilize the poll timer for simulating the system time
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(PollTimerElapsed()));

    // Initilize / start the cooldown timer for simulating temp cooldown
    coolDownTimer = new QTimer(this);
    connect(coolDownTimer, SIGNAL(timeout()), this, SLOT(CoolDownTimerElapsed()));

    // Bit order follows the pinned library's SetN2kTransmissionParameters flags.
    const QList<QCheckBox *> statusControls{ui->checkBoxCheckTransmission, ui->checkBoxOverTemperature,
        ui->checkBoxLowOilPressure, ui->checkBoxLowOilLevel, ui->checkBoxSailDrive};
    for (int bit = 0; bit < statusControls.size(); ++bit) {
        connect(statusControls[bit], &QCheckBox::clicked, this, [this, bit](bool checked) {
            const unsigned char mask = 1u << bit;
            setDiscreteStatus1(checked ? m_DiscreteStatus1 | mask : m_DiscreteStatus1 & ~mask);
        });
    }

    // Set initial values
    setOilTemperature(m_MyVessel.WaterTempK);
}

GeneratorTransLowerUnit::~GeneratorTransLowerUnit()
{
    delete ui;
}

void GeneratorTransLowerUnit::setMyVessel(const MyVessel &newMyVessel)
{
    m_MyVessel = newMyVessel;
}

int GeneratorTransLowerUnit::EngineInstance() const
{
    return m_EngineInstance;
}

void GeneratorTransLowerUnit::setEngineInstance(int newEngineInstance)
{
    if (m_EngineInstance == newEngineInstance)
        return;
    m_EngineInstance = newEngineInstance;
    ui->labelTitle->setText("Generate Transmission #" + QString::number(m_EngineInstance + 1) + " / Lower Unit");
    emit EngineInstanceChanged();
}

tN2kTransmissionGear GeneratorTransLowerUnit::TransmissionGear() const
{
    return m_TransmissionGear;
}

void GeneratorTransLowerUnit::setTransmissionGear(tN2kTransmissionGear newTransmissionGear)
{
    //if (m_TransmissionGear == newTransmissionGear)
    //    return;
    m_TransmissionGear = newTransmissionGear;
    switch (static_cast<int>(m_TransmissionGear)) {
    case N2kTG_Forward:
        ui->radioButtonForward->setChecked(true);
        ui->radioButtonNeutral->setChecked(false);
        ui->radioButtonReverse->setChecked(false);
        break;
    case N2kTG_Neutral:
        ui->radioButtonNeutral->setChecked(true);
        ui->radioButtonForward->setChecked(false);
        ui->radioButtonReverse->setChecked(false);
        break;
    case N2kTG_Reverse:
        ui->radioButtonReverse->setChecked(true);
        ui->radioButtonForward->setChecked(false);
        ui->radioButtonNeutral->setChecked(false);
        break;
    }

    emit TransmissionGearChanged();
}

double GeneratorTransLowerUnit::OilPressure() const
{
    return m_OilPressure;
}

void GeneratorTransLowerUnit::setOilPressure(double newOilPressure)
{
    if (qFuzzyCompare(m_OilPressure, newOilPressure))
        return;
    m_OilPressure = newOilPressure;
    ui->lineEditOilPressure->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(newOilPressure, UserPrefs()), 'f', 1));
    emit OilPressureChanged();
}

double GeneratorTransLowerUnit::OilTemperature() const
{
    return m_OilTemperature;
}

void GeneratorTransLowerUnit::setOilTemperature(double newOilTemperature)
{
    if (qFuzzyCompare(m_OilTemperature, newOilTemperature))
        return;
    m_OilTemperature = newOilTemperature;
    ui->lineEditOilTemperature->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(newOilTemperature, UserPrefs()), 'f', 1));
    emit OilTemperatureChanged();
}

unsigned char GeneratorTransLowerUnit::DiscreteStatus1() const
{
    return m_DiscreteStatus1;
}

void GeneratorTransLowerUnit::setDiscreteStatus1(unsigned char newDiscreteStatus1)
{
    ui->checkBoxCheckTransmission->setChecked(newDiscreteStatus1 & (1u << 0));
    ui->checkBoxOverTemperature->setChecked(newDiscreteStatus1 & (1u << 1));
    ui->checkBoxLowOilPressure->setChecked(newDiscreteStatus1 & (1u << 2));
    ui->checkBoxLowOilLevel->setChecked(newDiscreteStatus1 & (1u << 3));
    ui->checkBoxSailDrive->setChecked(newDiscreteStatus1 & (1u << 4));
    if (m_DiscreteStatus1 == newDiscreteStatus1)
        return;
    m_DiscreteStatus1 = newDiscreteStatus1;
    emit DiscreteStatus1Changed();
}

void GeneratorTransLowerUnit::initializeUnitsLimits()
{
    // Temperature (K °C, °F)
    switch (UserPrefs().tempUnitIndex) {
    case 0: // K
        ui->labelUnitsOilTemperature->setText("K");
        break;
    case 1: // °C
        ui->labelUnitsOilTemperature->setText("°C");
        break;
    case 2: // °F
        ui->labelUnitsOilTemperature->setText("°F");
        break;
    }

    // Pressure (Pa, kPa, Bar, PSI)
    switch (UserPrefs().pressureUnitIndex) {
    case 0: // Pa
        ui->labelUnitsOilPressure->setText("Pa");
        break;
    case 1: // kPa
        ui->labelUnitsOilPressure->setText("kPa");
        break;
    case 2: // Bar
        ui->labelUnitsOilPressure->setText("Bar");
        break;
    case 3: // PSI
        ui->labelUnitsOilPressure->setText("PSI");
        break;
    }
}

void GeneratorTransLowerUnit::onTimerTimeout() {}

void GeneratorTransLowerUnit::PollTimerElapsed()
{
    if (TransmissionGear() == N2kTG_Neutral) {
        return;
    }

    m_pollCounter++;

    // Update the Oil pressure based on the engine load
    double oilPressure = calculateOilPressure(m_EngineLoad);
    setOilPressure(oilPressure);

    // Update the Oil temperature based on the engine load
    double oilTemperature = calculateOilTemperature(m_pollCounter, m_EngineLoad, m_OilTemperature);
    setOilTemperature(oilTemperature);
    emit parametersSampled({"OilPressure","OilTemperature"});
}

void GeneratorTransLowerUnit::CoolDownTimerElapsed()
{
    m_coolDownCounter--;

    // Update the Oil temperature based on cooldown
    if (m_OilTemperature > m_waterTempK) {
        double oilTemperature = calculateOilTemperature(m_coolDownCounter, 10, m_OilTemperature);
        setOilTemperature(oilTemperature);
    }
}

void GeneratorTransLowerUnit::on_radioButtonForward_clicked()
{
    setTransmissionGear(N2kTG_Forward);
}

void GeneratorTransLowerUnit::on_radioButtonNeutral_clicked()
{
    setTransmissionGear(N2kTG_Neutral);
}

void GeneratorTransLowerUnit::on_radioButtonReverse_clicked()
{
    setTransmissionGear(N2kTG_Reverse);
}

void GeneratorTransLowerUnit::on_lineEditOilPressure_textEdited(const QString &arg1) {}

void GeneratorTransLowerUnit::on_lineEditOilTemperature_textEdited(const QString &arg1) {}

double GeneratorTransLowerUnit::calculateOilTemperature(
    int timeInSeconds, double engineLoadPercentage, double lastOilTemperatureK, bool highPerformance, double waterTempK)
{
    // Convert throttle position percentage to a multiplier (1% to 100% becomes 0.01 to 1.00)
    double throttleMultiplier = engineLoadPercentage / 100.0;

    // Starting and target oil temperatures in Celsius
    double targetTemperatureC = highPerformance ? 130.0 : 110.0; // Target oil temperature for high-performance engines
    //double engineRoomTemperatureC = (72.0 - 32) * 5.0 / 9.0;     // Convert 72°F to Celsius (engine room temp)
    double engineRoomTemperatureC = Convert::KelvinToCelsius(waterTempK);

    // Convert last oil temperature from Kelvin to Celsius
    double lastOilTemperatureC = lastOilTemperatureK - 273.15;

    // Maximum time assumed for the engine to reach normal oil temperature at full throttle
    int maxTimeToWarmUp = 480; // 8 minutes for oil to heat up at full throttle (seconds)

    // Adjust the warm-up time based on throttle position
    double effectiveWarmUpTime = maxTimeToWarmUp / throttleMultiplier;

    // If timeInSeconds is negative, calculate cooldown
    if (timeInSeconds < 0) {
        timeInSeconds = -timeInSeconds; // Convert to positive for cooldown calculation

        // Use a cooldown time factor (typically longer than warm-up time)
        double cooldownFactor = 2.0; // Cooldown is slower, so adjust factor (e.g., twice as long as warm-up)

        // Calculate cooldown temperature decrease factor
        double cooldownTime = cooldownFactor * effectiveWarmUpTime;
        double temperatureDecreaseFactor = timeInSeconds / cooldownTime;

        // Calculate the current temperature during cooldown, going toward engine room temperature
        double temperatureC = lastOilTemperatureC - (lastOilTemperatureC - engineRoomTemperatureC) * temperatureDecreaseFactor;

        // Clamp the temperature to not drop below engine room temperature
        if (temperatureC < engineRoomTemperatureC) {
            temperatureC = engineRoomTemperatureC;
        }

        // Return temperature in Kelvin
        return temperatureC + 273.15;
    }

    // If timeInSeconds is positive, calculate warm-up
    double temperatureIncreaseFactor = timeInSeconds / effectiveWarmUpTime;

    // Calculate the current temperature based on the increase factor
    double temperatureC = lastOilTemperatureC + (targetTemperatureC - lastOilTemperatureC) * temperatureIncreaseFactor;

    // Clamp the temperature to not exceed the target temperature
    if (temperatureC > targetTemperatureC) {
        temperatureC = targetTemperatureC;
    }

    // Return temperature in Kelvin
    return temperatureC + 273.15;
}

double GeneratorTransLowerUnit::calculateOilPressure(double engineLoadPercentage, std::pair<int, int> pressureRange)
{
    // Return 0.0 if throttle position is less than 1%
    if (engineLoadPercentage < 1.0) {
        return 0.0;
    }

    // Convert throttle position percentage to a multiplier (1% to 100% becomes 0.01 to 1.00)
    double throttleMultiplier = engineLoadPercentage / 100.0;

    // Calculate the pressure based on the throttle multiplier and the given pressure range
    double pressure = pressureRange.first + (pressureRange.second - pressureRange.first) * throttleMultiplier;

    return pressure; // Pa
}

bool GeneratorTransLowerUnit::EngineRunning() const
{
    return m_EngineRunning;
}

void GeneratorTransLowerUnit::setEngineRunning(bool newEngineRunning)
{
    if (m_EngineRunning == newEngineRunning)
        return;
    m_EngineRunning = newEngineRunning;

    if (m_EngineRunning) {
        pollTimer->start(RATE_1_SEC);
        coolDownTimer->stop();
        m_coolDownCounter = 0;
    } else {
        pollTimer->stop();
        m_pollCounter = 0;
        coolDownTimer->start(RATE_1_SEC);
        setOilPressure(0.0);
    }
    emit EngineRunningChanged();
}

double GeneratorTransLowerUnit::EngineLoad() const
{
    return m_EngineLoad;
}

void GeneratorTransLowerUnit::setEngineLoad(double newEngineLoad)
{
    if (qFuzzyCompare(m_EngineLoad, newEngineLoad))
        return;
    m_EngineLoad = newEngineLoad;
    ui->horizontalSliderLoad->setValue(static_cast<int>(m_EngineLoad));
    emit EngineLoadChanged();
}

void GeneratorTransLowerUnit::setInitialGuiValues()
{
    setDiscreteStatus1(m_DiscreteStatus1);
    ui->lineEditOilPressure->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(m_OilPressure, UserPrefs()), 'f', 1));
    ui->lineEditOilTemperature->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_OilTemperature, UserPrefs()), 'f', 1));
    ui->horizontalSliderLoad->setValue(static_cast<int>(m_EngineLoad));
    setTransmissionGear(TransmissionGear());
}

void GeneratorTransLowerUnit::on_horizontalSliderLoad_sliderPressed()
{
    // Show message box to inform user that the engine controls the load indicator
    QMessageBox msgBox;
    msgBox.setText("Engine controls the load indicator. \rResetting the slider to the current engine load.");
    msgBox.exec();

    // Reset the slider value to the current engine load
    ui->horizontalSliderLoad->setValue(static_cast<int>(m_EngineLoad));
}

void GeneratorTransLowerUnit::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}
