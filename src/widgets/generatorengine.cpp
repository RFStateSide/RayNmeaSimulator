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

#include "runtimeresources.h"
#include "generatorengine.h"
#include "N2kMsg.h"
#include <QFile>
#include <QSignalBlocker>
#include <cmath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QStringList>
#include <QVector>
#include "helper.h"
#include "n2khelper.h"
#include "ui_generatorengine.h"

GeneratorEngine::GeneratorEngine(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::GeneratorEngine)
{
    ui->setupUi(this);
    statusBar = new QStatusBar(this);
    ui->verticalLayout_3->addWidget(statusBar);
    statusBar->showMessage("Press Start engine...");

    setId(id);
    Q_UNUSED(label);
    setPgn(-127489);
    setColumnSpan(3);
    setRowSpan(9);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->checkBoxEnabled->setToolTip(tr("Enable signals from this generator. Outbound transport output still requires explicit Enable Sending."));
    ui->comboBoxVesselEngine->setToolTip(tr("Select an engine profile before starting. Stop the simulated engine before changing its profile."));
    ui->checkBoxStartEngine->setToolTip(tr("Start or stop this simulated engine. Local generation does not require Sending; Engine Stop does not stop other outbound instruments."));
    ui->verticalSliderThrottle->setToolTip(tr("Set this engine throttle. The selected profile determines RPM and fuel rate; this does not enable Sending."));
    ui->checkBoxEng1Rand->setToolTip(tr("Add simulated RPM variation while this engine is running."));

    initilizeComboBoxes();
    initializeUnitsLimits();
    connect(this, &PgnWidgetBase::UserPrefsChanged, this, &GeneratorEngine::initializeUnitsLimits);
    initializeOthers();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    //ui->pushButtonRemove->setIcon(iconExit);

    // Initilize the poll timer for simulating the system time
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(PollTimerElapsed()));

    // Initilize / start the cooldown timer for simulating temp cooldown
    coolDownTimer = new QTimer(this);
    connect(this, &PgnWidgetBase::enabledChanged, this, &GeneratorEngine::accountFuelUsage);
    m_fuelEnabled = isEnabled();
    connect(coolDownTimer, SIGNAL(timeout()), this, SLOT(CoolDownTimerElapsed()));
    //coolDownTimer->start(1000);
}

GeneratorEngine::~GeneratorEngine()
{
    delete ui;
}

void GeneratorEngine::initializeUnitsLimits()
{
    // Flow Rate (LPH, GPH)
    switch (UserPrefs().flowRateUnitIndex) {
    case 0: // LPH
        ui->labelUnitsFuelRate->setText("LPH");
        break;
    case 1: // GPH
        ui->labelUnitsFuelRate->setText("GPH");
        break;
    }

    // Temperature (K °C, °F)
    switch (UserPrefs().tempUnitIndex) {
    case 0: // K
        ui->labelUnitsEngineCoolantTemp->setText("K");
        break;
    case 1: // °C
        ui->labelUnitsEngineCoolantTemp->setText("°C");
        break;
    case 2: // °F
        ui->labelUnitsEngineCoolantTemp->setText("°F");
        break;
    }
}

void GeneratorEngine::onTimerTimeout()
{
    if (m_SignalToEmit == "EngineSpeedChanged") {
        emit EngineSpeedChanged();
    } else if (m_SignalToEmit == "EngineHoursChanged") {
        // Commit canonical seconds without reformatting the active editor on
        // every keystroke. Public/programmatic updates still use setEngineHours.
        m_EngineHours = m_ValueToEmit.toDouble() * 3600.0;
        emit EngineHoursChanged();
    }

    m_SignalToEmit.clear();
}

void GeneratorEngine::PollTimerElapsed()
{
    accountFuelUsage();
    if (!profileReady() || !EngineRunning()) return;
    m_pollCounter++;
    double rpmSpeed = ui->verticalSliderThrottle->value();
    // .25 seconds has elapsed

    // .5 second has elapsed
    if (m_pollCounter % 2 == 0) {
        //if (!m_UsingGenVessel) {
        // Update engine speed based on throttle position
        if (ui->checkBoxEng1Rand->isChecked()) {
            rpmSpeed += generateRandNum();
        }
        setEngineSpeed(rpmSpeed);
        emit parametersSampled({"EngineSpeed"});
        //}
    }

    // 1 second has elapsed
    if (m_pollCounter % 4 == 0) {
        if (m_UsingGenVessel) {
            statusBar->showMessage("Generator vessel controlling RPM...");
        } else {
            statusBar->showMessage("Generator engine controlling RPM...");
        }

        // 1 second has elapsed
        setEngineHours(m_EngineHours + 1);

        // Update fuel rate based on engine speed
        double gph = calculateFlowRateGph(EngineSpeed());
        setFuelRate(Convert::GphToLph(gph));

        // Update throttle position based on engine speed
        double throttlePosition = calculateThrottlePositionPercentageFromRpm(rpmSpeed, m_idleAndWot);
        setEngineTorque(throttlePosition);
        setEngineLoad(throttlePosition);

        // Update coolant temperature based on time and throttle position
        double coolantTemp = calculateCoolantTemperature(m_pollCounter / 4, throttlePosition, m_EngineCoolantTemp);
        setEngineCoolantTemp(coolantTemp);

        // Update coolant pressure based on throttle position
        double coolantPressure = calculatePressure(throttlePosition, {68947, 137895}); // 10-20 psi range (69-138 kPa)
        setEngineCoolantPress(coolantPressure);

        // Update oil temperature based on time and throttle position and engine performance type
        double oilTemp = calculateOilTemperature(m_pollCounter / 4, throttlePosition, m_EngineOilTemp, m_engineProperties.forcedInduction);
        setEngineOilTemp(oilTemp);

        // Update oil pressure based on throttle position
        double oilPressure = calculatePressure(throttlePosition, {172368, 379211}); // 25-55 psi range (172.37-379.21 kPa)
        setEngineOilPress(oilPressure);

        // Update fuel pressure based on throttle position and fuel injection status
        double fuelPressure = calculateFuelPressure(throttlePosition, m_engineProperties.fuelInjected);
        setEngineFuelPress(fuelPressure);

        // Update boost pressure based on throttle position and fuel type
        if (m_engineProperties.forcedInduction) {
            double boostPressure = calculateBoostPressure(throttlePosition, m_engineProperties.fuelType);
            setEngineBoostPressure(boostPressure);
        }

        // Update alternator voltage based on throttle position and alternator type
        double alternatorVoltage = calculateVoltage(throttlePosition, m_engineProperties.alternatorVoltage);
        setAltenatorVoltage(alternatorVoltage);
        emit parametersSampled({"EngineHours","FuelRate","EngineTorque","EngineLoad","EngineCoolantTemp","EngineCoolantPress","EngineOilTemp","EngineOilPress","EngineFuelPress","AltenatorVoltage"});
        if (m_engineProperties.forcedInduction) emit parametersSampled({"EngineBoostPressure"});
    }

    // 5 seconds has elapsed
    if (m_pollCounter % 20 == 0) {
        // 5 seconds has elapsed
    }

}

void GeneratorEngine::CoolDownTimerElapsed()
{
    m_coolDownCounter--;

    if (m_EngineCoolantTemp > m_engineRmTempK) {
        double coolantTemp = calculateCoolantTemperature(m_coolDownCounter, 1.0, m_EngineCoolantTemp);
        setEngineCoolantTemp(coolantTemp);
    }

    if (m_EngineOilTemp > m_engineRmTempK) {
        double oilTemp = calculateOilTemperature(m_coolDownCounter, 1.0, m_EngineOilTemp, m_engineProperties.forcedInduction);
        setEngineOilTemp(oilTemp);
    }

    // Once the coolant and oil temps reach engine room temp, stop the cooldown timer
    if (m_EngineCoolantTemp <= m_engineRmTempK && m_EngineOilTemp <= m_engineRmTempK) {
        coolDownTimer->stop();
        statusBar->showMessage("Engine cooled down...", 5000);
    }

    // Update the status bar with the current cooldown time
    statusBar->showMessage("Engine stopped, cooling down...");
}

void GeneratorEngine::initilizeComboBoxes()
{
    QStringList engineList = helper::getEngineList(RuntimeResources::engineProfiles());
    ui->comboBoxVesselEngine->addItem("None");
    ui->comboBoxVesselEngine->addItems(engineList);
}

bool GeneratorEngine::reloadEngineProfiles()
{
    if (EngineRunning()) return false;
    const QString selected = engineProfile();
    {
        QSignalBlocker blocker(ui->comboBoxVesselEngine);
        ui->comboBoxVesselEngine->clear();
        initilizeComboBoxes();
        if (ui->comboBoxVesselEngine->findText(selected, Qt::MatchExactly | Qt::MatchCaseSensitive) < 0)
            ui->comboBoxVesselEngine->addItem(selected);
        ui->comboBoxVesselEngine->setCurrentText(selected);
    }
    setEngineProfile(selected); // Reload the cached curve even if the name did not change.
    return true;
}

void GeneratorEngine::initializeOthers()
{
    // Seed the random number generator
    srand((uint) time.msec());

    m_idleAndWot = profileReady() ? getIdleAndWOT(m_activeRpmGphData) : std::make_pair(0, 0);
    ui->verticalSliderThrottle->setRange(m_idleAndWot.first, m_idleAndWot.second);
    ui->verticalSliderThrottle->setValue(m_idleAndWot.first);
    setEngineSpeed(0.0); // Set the initial engine speed to 0 RPM, not idle RPM that slider sets
}

std::vector<std::pair<int, double>> GeneratorEngine::getEngineRpmGphData(const QString &filePath, const QString &engineName)
{
    if (engineName == "None") {
        return {}; // Return empty vector if no engine is selected
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open engine profiles file.";
        return {};
    }

    // Read all content as a QString
    QTextStream stream(&file);
    QString fileContent = stream.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileContent.toUtf8());

    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Failed to parse JSON file or incorrect format.";
        return {};
    }

    std::vector<std::pair<int, double>> rpmGphPairs;
    QJsonObject jsonObj = doc.object();
    QJsonArray enginesArray = jsonObj["engines"].toArray();

    for (const QJsonValue &engine : enginesArray) {
        QJsonObject engineObj = engine.toObject();
        QString engineObjName = engineObj["name"].toString();

        if (engineObjName == engineName) {
            QJsonArray rpmGphArray;

            // Check for both keys: rpm_gph_pairs or rpm_gph_data
            if (engineObj.contains("rpm_gph_pairs")) {
                rpmGphArray = engineObj["rpm_gph_pairs"].toArray();
            } else if (engineObj.contains("rpm_gph_data")) {
                rpmGphArray = engineObj["rpm_gph_data"].toArray();
            } else {
                qWarning() << "No RPM-GPH data found for engine:" << engineName;
                return {};
            }

            // Parse RPM and GPH data
            for (const QJsonValue &pair : rpmGphArray) {
                QJsonObject pairObj = pair.toObject();
                const double rpmValue = pairObj["rpm"].toDouble(-1);
                const double gph = pairObj["gph"].toDouble(-1);
                if (!pairObj["rpm"].isDouble() || !pairObj["gph"].isDouble()
                    || !std::isfinite(rpmValue) || !std::isfinite(gph)
                    || rpmValue <= 0 || rpmValue > 100000 || std::floor(rpmValue) != rpmValue || gph < 0
                    || (!rpmGphPairs.empty() && rpmValue <= rpmGphPairs.back().first)) return {};
                const int rpm = int(rpmValue);
                rpmGphPairs.emplace_back(rpm, gph);
            }

            // Break the loop after finding the matching engine
            break;
        }
    }

    if (rpmGphPairs.size() < 2) {
        qWarning() << "No valid fuel curve for" << engineName;
        return {};
    }

    return rpmGphPairs;
}

/*
QVector<QPair<int, double>> GeneratorEngine::getEngineRpmGphData(const QString &filePath, const QString &engineName)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open engine profiles file.";
        return {};
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QVector<QPair<int, double>> rpmGphPairs;
    if (doc.isObject()) {
        QJsonObject jsonObj = doc.object();
        QJsonArray enginesArray = jsonObj["engines"].toArray();
        for (const QJsonValue &engine : enginesArray) {
            QJsonObject engineObj = engine.toObject();
            if (engineObj["name"].toString() == engineName) {
                QJsonArray rpmGphArray = engineObj["rpm_gph_pairs"].toArray();
                for (const QJsonValue &pair : rpmGphArray) {
                    QJsonObject pairObj = pair.toObject();
                    int rpm = pairObj["rpm"].toInt();
                    double gph = pairObj["gph"].toDouble();
                    rpmGphPairs.append(qMakePair(rpm, gph));
                }
                break;
            }
        }
    }

    return rpmGphPairs;
}
*/

std::pair<int, int> GeneratorEngine::getIdleAndWOT(const std::vector<std::pair<int, double>> &rpmGphData)
{
    if (rpmGphData.empty()) {
        return std::make_pair(-1, -1); // Return -1 for both if the vector is empty
    }

    // Initialize min and max RPMs
    int minRpm = rpmGphData.front().first;
    int maxRpm = rpmGphData.front().first;

    // Iterate through the vector to find min and max RPM
    for (const auto &entry : rpmGphData) {
        if (entry.first < minRpm) {
            minRpm = entry.first;
        }
        if (entry.first > maxRpm) {
            maxRpm = entry.first;
        }
    }

    // Return the minimum RPM (Idle) and maximum RPM (WOT) as a pair
    return std::make_pair(minRpm, maxRpm);
}
/*
bool GeneratorEngine::isEngineFuelInjected(const QString &filePath, const QString &engineName)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open engine profiles file.";
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isObject()) {
        QJsonObject jsonObj = doc.object();
        QJsonArray enginesArray = jsonObj["engines"].toArray();
        for (const QJsonValue &engineValue : enginesArray) {
            QJsonObject engineObj = engineValue.toObject();
            if (engineObj["name"].toString() == engineName) {
                if (engineObj.contains("FuelInjected")) {
                    return engineObj["FuelInjected"].toBool();
                } else {
                    qWarning() << "FuelInjected parameter not found for engine:" << engineName;
                    return false; // Default return if the parameter doesn't exist
                }
            }
        }
    }

    qWarning() << "Engine not found:" << engineName;
    return false;
}

bool GeneratorEngine::isEngineForceInjected(const QString &filePath, const QString &engineName)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open engine profiles file.";
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isObject()) {
        QJsonObject jsonObj = doc.object();
        QJsonArray enginesArray = jsonObj["engines"].toArray();
        for (const QJsonValue &engine : enginesArray) {
            QJsonObject engineObj = engine.toObject();
            if (engineObj["name"].toString() == engineName) {
                return engineObj["forcedInduction"].toBool(false); // Default to false if not found
            }
        }
    }

    return false; // Return false if engine not found
}
*/
EngineProperties GeneratorEngine::getEngineProperties(const QString &filePath, const QString &engineName)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open engine profiles file.";
        return {};
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format.";
        return {};
    }

    QJsonObject jsonObj = doc.object();
    QJsonArray enginesArray = jsonObj["engines"].toArray();

    for (const QJsonValue &engine : enginesArray) {
        QJsonObject engineObj = engine.toObject();
        if (engineObj["name"].toString() == engineName) {
            EngineProperties engineProperties;

            engineProperties.name = engineObj["name"].toString();
            engineProperties.horsepower = engineObj["horsepower"].toInt();
            engineProperties.fuelType = engineObj["fuelType"].toString(); // Assuming "fuelType" is added to the JSON
            engineProperties.fuelInjected = engineObj["fuelInjected"].toBool();
            engineProperties.forcedInduction = engineObj["forcedInduction"].toBool();
            engineProperties.alternatorVoltage = engineObj["alternatorVoltage"].toInt();

            return engineProperties;
        }
    }

    qWarning() << "Engine not found in the JSON, using default properties.";
    return EngineProperties();
}

double GeneratorEngine::calculateThrottlePositionPercentageFromRpm(int rpm, std::pair<int, int> idleAndWot)
{
    if (idleAndWot.second <= idleAndWot.first) return 0.0;
    // Define the known points
    double rpm1 = idleAndWot.first;
    double percent1 = 1.0;

    double rpm2 = idleAndWot.second;
    double percent2 = 100.0;

    // Calculate the slope (m) for the linear relationship
    double m = (percent2 - percent1) / (rpm2 - rpm1);

    // Calculate the intercept (c) using one of the points
    double c = percent1 - m * rpm1;

    // Calculate the throttle position in percentage for the given RPM
    double throttlePosition = m * rpm + c;

    // Ensure the percentage is within valid bounds (1% to 100%)
    if (throttlePosition < 1.0) {
        throttlePosition = 1.0;
    } else if (throttlePosition > 100.0) {
        throttlePosition = 100.0;
    }

    return throttlePosition;
}

double GeneratorEngine::calculateRpmFromThrottlePosition(double throttlePositionPercentage, std::pair<int, int> idleAndWot)
{
    // Define the known points
    double percent1 = 1.0;
    double rpm1 = idleAndWot.first;

    double percent2 = 100.0;
    double rpm2 = idleAndWot.second;

    // Calculate the slope (m) for the linear relationship
    double m = (rpm2 - rpm1) / (percent2 - percent1);

    // Calculate the intercept (c) using one of the points
    double c = rpm1 - m * percent1;

    // Calculate the RPM for the given throttle position percentage
    double rpm = m * throttlePositionPercentage + c;

    // Ensure the RPM is within the valid bounds (idle to WOT RPM)
    if (rpm < rpm1) {
        rpm = rpm1;
    } else if (rpm > rpm2) {
        rpm = rpm2;
    }

    return rpm;
}

double GeneratorEngine::calculateFlowRateGph(int rpm)
{
    if (!profileReady()) return 0.0;
    // Handle edge cases: RPM below the minimum
    if (rpm <= m_activeRpmGphData.front().first) {
        return m_activeRpmGphData.front().second;
    }

    // Handle edge cases: RPM above the maximum
    if (rpm >= m_activeRpmGphData.back().first) {
        return m_activeRpmGphData.back().second;
    }

    // Iterate over the data points to find the correct interval
    for (size_t i = 1; i < m_activeRpmGphData.size(); ++i) {
        int rpm1 = m_activeRpmGphData[i - 1].first;
        int rpm2 = m_activeRpmGphData[i].first;
        double gph1 = m_activeRpmGphData[i - 1].second;
        double gph2 = m_activeRpmGphData[i].second;

        if (rpm >= rpm1 && rpm <= rpm2) {
            // Perform linear interpolation between rpm1 and rpm2
            double slope = (gph2 - gph1) / (rpm2 - rpm1);
            return gph1 + slope * (rpm - rpm1);
        }
    }

    // Default return (should not reach here)
    return 0.0;
}

double GeneratorEngine::calculateCoolantTemperature(int timeInSeconds, double throttlePositionPercentage, double lastCoolantTemperatureK, double engineRmTempK)
{
    // Convert throttle position percentage to a multiplier (1% to 100% becomes 0.01 to 1.00)
    double throttleMultiplier = throttlePositionPercentage / 100.0;

    // Starting and target temperatures in Celsius
    double targetTemperatureC = (165.0 - 32) * 5.0 / 9.0; // Convert 165°F to Celsius
    //double engineRoomTemperatureC = (72.0 - 32) * 5.0 / 9.0; // Convert 72°F to Celsius (engine room temp)
    double engineRoomTemperatureC = Convert::KelvinToCelsius(engineRmTempK);

    // Convert lastCoolantTemperature from Kelvin to Celsius
    double lastCoolantTemperatureC = Convert::KelvinToCelsius(lastCoolantTemperatureK);

    // Maximum time assumed for the engine to reach normal operating temperature at full throttle
    int maxTimeToWarmUp = 360; // 5 minutes (seconds)

    // Adjust the warm-up time based on throttle position
    double effectiveWarmUpTime = maxTimeToWarmUp / throttleMultiplier;

    // If timeInSeconds is negative, calculate cooldown
    if (timeInSeconds < 0) {
        timeInSeconds = -timeInSeconds; // Convert to positive for cooldown calculation

        // Use a cooldown time factor (typically longer than warm-up time)
        double cooldownFactor = 1.5; // Cooldown is slower, so adjust factor (e.g., 50% longer)

        // Calculate cooldown temperature decrease factor
        double cooldownTime = cooldownFactor * effectiveWarmUpTime;
        double temperatureDecreaseFactor = timeInSeconds / cooldownTime;

        // Start the cooldown from the last known coolant temperature (in Celsius)
        double temperatureC = lastCoolantTemperatureC - (lastCoolantTemperatureC - engineRoomTemperatureC) * temperatureDecreaseFactor;

        // Clamp the temperature to not drop below engine room temperature
        if (temperatureC < engineRoomTemperatureC) {
            temperatureC = engineRoomTemperatureC;
        }

        // Convert back to Kelvin for return
        return Convert::CelsiusToKelvin(temperatureC);
    }

    // If timeInSeconds is positive, calculate warm-up
    double temperatureIncreaseFactor = timeInSeconds / effectiveWarmUpTime;

    // Calculate the current temperature based on the increase factor, starting from lastCoolantTemperatureC
    double temperatureC = lastCoolantTemperatureC + (targetTemperatureC - lastCoolantTemperatureC) * temperatureIncreaseFactor;

    // Clamp the temperature to not exceed the target temperature
    if (temperatureC > targetTemperatureC) {
        temperatureC = targetTemperatureC;
    }

    // Convert back to Kelvin for return
    return Convert::CelsiusToKelvin(temperatureC);
}

double GeneratorEngine::calculateOilTemperature(
    int timeInSeconds, double throttlePositionPercentage, double lastOilTemperatureK, bool highPerformance, double engineRmTempK)
{
    // Convert throttle position percentage to a multiplier (1% to 100% becomes 0.01 to 1.00)
    double throttleMultiplier = throttlePositionPercentage / 100.0;

    // Starting and target oil temperatures in Celsius
    double targetTemperatureC = highPerformance ? 130.0 : 110.0; // Target oil temperature for high-performance engines
    //double engineRoomTemperatureC = (72.0 - 32) * 5.0 / 9.0;     // Convert 72°F to Celsius (engine room temp)
    double engineRoomTemperatureC = Convert::KelvinToCelsius(engineRmTempK);

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

double GeneratorEngine::calculatePressure(double throttlePositionPercentage, std::pair<int, int> pressureRange)
{
    // Return 0.0 if throttle position is less than 1%
    if (throttlePositionPercentage < 1.0) {
        return 0.0;
    }

    // Convert throttle position percentage to a multiplier (1% to 100% becomes 0.01 to 1.00)
    double throttleMultiplier = throttlePositionPercentage / 100.0;

    // Calculate the pressure based on the throttle multiplier and the given pressure range
    double pressure = pressureRange.first + (pressureRange.second - pressureRange.first) * throttleMultiplier;

    return pressure; // Return in Pa
}

double GeneratorEngine::calculateFuelPressure(double throttlePositionPercentage,
                                              bool fuelInjected,
                                              std::pair<int, int> pressureRangeCarbureted,
                                              std::pair<int, int> pressureRangeFuelInjected)
{
    // If throttle position is less than 1%, return 0.0
    if (throttlePositionPercentage < 1.0) {
        return 0.0;
    }

    // Determine the pressure range based on whether the engine is fuel injected or carbureted
    std::pair<int, int> pressureRange = fuelInjected ? pressureRangeFuelInjected : pressureRangeCarbureted;

    // Calculate the pressure based on throttle position
    double pressure = pressureRange.first + ((pressureRange.second - pressureRange.first) * (throttlePositionPercentage / 100.0));

    return pressure; // Return in Pa
}

double GeneratorEngine::calculateBoostPressure(double throttlePositionPercentage,
                                               QString fuelType,
                                               std::pair<int, int> pressureGasolineRange,
                                               std::pair<int, int> pressureDieselRange)
{
    // If throttle position is less than 1%, return 0.0 (no boost)
    if (throttlePositionPercentage < 1.0) {
        return 0.0;
    }

    // Determine the pressure range based on fuel type
    std::pair<int, int> pressureRange;
    if (fuelType.toLower() == "diesel") {
        pressureRange = pressureDieselRange;
    } else {
        pressureRange = pressureGasolineRange;
    }

    // Calculate boost pressure based on throttle position (in percentage)
    double throttleMultiplier = throttlePositionPercentage / 100.0;

    // Linearly interpolate between the minimum and maximum pressures
    double boostPressure = pressureRange.first + (pressureRange.second - pressureRange.first) * throttleMultiplier;

    return boostPressure; // Return in Pa
}

double GeneratorEngine::calculateVoltage(double throttlePositionPercentage,
                                         int alternatorVoltageType,
                                         std::pair<int, int> voltage12Range,
                                         std::pair<int, int> voltage24Range)
{
    // Convert throttle position percentage to a multiplier (0 to 1.0)
    double throttleMultiplier = throttlePositionPercentage / 100.0;

    // Determine the voltage range based on the alternator voltage type
    double minVoltage, maxVoltage;
    if (alternatorVoltageType == 24) {
        minVoltage = voltage24Range.first;
        maxVoltage = voltage24Range.second;
    } else {
        // Default to 12V if not 24V
        minVoltage = voltage12Range.first;
        maxVoltage = voltage12Range.second;
    }

    // Calculate the voltage based on throttle position
    double voltage = minVoltage + (maxVoltage - minVoltage) * throttleMultiplier;

    // Return the calculated voltage in volts
    return voltage;
}

int GeneratorEngine::generateRandNum(int low, int high)
{
    return rand() % ((high + 1) - low) + low;
}

void GeneratorEngine::setMyVessel(const struct MyVessel &newMyVessel)
{
    m_MyVessel = newMyVessel;

    const int index = ui->comboBoxVesselEngine->findText(m_MyVessel.EngineProfile);
    setEngineProfile(index >= 0 ? m_MyVessel.EngineProfile : QStringLiteral("None"));
    if (!profileReady() && engineProfile() != "None")
        setEngineProfile(QStringLiteral("None"));

    setEngineHours(m_MyVessel.EngineHours);
}

QString GeneratorEngine::engineProfile() const
{
    return ui->comboBoxVesselEngine->currentText();
}

void GeneratorEngine::setEngineProfile(const QString &name)
{
    // Profile JSON uses the exact name as its key. Keep an unavailable saved key
    // in the combo so saving again does not discard the user's intended profile.
    auto *combo = ui->comboBoxVesselEngine;
    int index = combo->findText(name, Qt::MatchExactly | Qt::MatchCaseSensitive);
    if (index < 0) {
        combo->addItem(name);
        index = combo->count() - 1;
    }
    if (combo->currentIndex() == index)
        on_comboBoxVesselEngine_currentTextChanged(name);
    else
        combo->setCurrentIndex(index);
}

int GeneratorEngine::EngineInstance() const
{
    return m_EngineInstance;
}

void GeneratorEngine::setEngineInstance(int newEngineInstance)
{
    if (m_EngineInstance == newEngineInstance)
        return;
    accountFuelUsage(); // Settle the previous instance before rebinding it.
    m_EngineInstance = newEngineInstance;
    //Generate Engine / Outboard
    ui->labelTitle->setText("Generate Engine #" + QString::number(newEngineInstance + 1) + " / Outboard");
    emit EngineInstanceChanged();
}

double GeneratorEngine::EngineSpeed() const
{
    return m_EngineSpeed;
}

void GeneratorEngine::setEngineSpeed(double newEngineSpeed)
{
    if (qFuzzyCompare(m_EngineSpeed, newEngineSpeed))
        return;
    m_EngineSpeed = newEngineSpeed;
    ui->lineEditEngineSpeed->setText(QString::number(newEngineSpeed));
    emit EngineSpeedChanged();
}

double GeneratorEngine::EngineBoostPressure() const
{
    return m_EngineBoostPressure;
}

void GeneratorEngine::setEngineBoostPressure(double newEngineBoostPressure)
{
    if (qFuzzyCompare(m_EngineBoostPressure, newEngineBoostPressure))
        return;
    m_EngineBoostPressure = newEngineBoostPressure;
    emit EngineBoostPressureChanged();
}

int GeneratorEngine::EngineTiltTrim() const
{
    return m_EngineTiltTrim;
}

void GeneratorEngine::setEngineTiltTrim(int newEngineTiltTrim)
{
    if (m_EngineTiltTrim == newEngineTiltTrim)
        return;
    m_EngineTiltTrim = newEngineTiltTrim;
    emit EngineTiltTrimChanged();
}

double GeneratorEngine::EngineOilPress() const
{
    return m_EngineOilPress;
}

void GeneratorEngine::setEngineOilPress(double newEngineOilPress)
{
    if (qFuzzyCompare(m_EngineOilPress, newEngineOilPress))
        return;
    m_EngineOilPress = newEngineOilPress;
    emit EngineOilPressChanged();
}

double GeneratorEngine::EngineOilTemp() const
{
    return m_EngineOilTemp;
}

void GeneratorEngine::setEngineOilTemp(double newEngineOilTemp)
{
    if (qFuzzyCompare(m_EngineOilTemp, newEngineOilTemp))
        return;
    m_EngineOilTemp = newEngineOilTemp;
    emit EngineOilTempChanged();
}

double GeneratorEngine::EngineCoolantTemp() const
{
    return m_EngineCoolantTemp;
}

void GeneratorEngine::setEngineCoolantTemp(double newEngineCoolantTemp)
{
    if (qFuzzyCompare(m_EngineCoolantTemp, newEngineCoolantTemp))
        return;
    m_EngineCoolantTemp = newEngineCoolantTemp;
    ui->lineEditCoolantTemp->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(newEngineCoolantTemp, UserPrefs()), 'f', 2));
    emit EngineCoolantTempChanged();
}

double GeneratorEngine::AltenatorVoltage() const
{
    return m_AltenatorVoltage;
}

void GeneratorEngine::setAltenatorVoltage(double newAltenatorVoltage)
{
    if (qFuzzyCompare(m_AltenatorVoltage, newAltenatorVoltage))
        return;
    m_AltenatorVoltage = newAltenatorVoltage;
    emit AltenatorVoltageChanged();
}

double GeneratorEngine::FuelRate() const
{
    return m_FuelRate;
}

void GeneratorEngine::setFuelRate(double newFuelRate)
{
    if (qFuzzyCompare(m_FuelRate, newFuelRate))
        return;
    accountFuelUsage(); // Elapsed time belongs to the previous rate.
    m_FuelRate = newFuelRate;
    ui->lineEditFuelRate->setText(QString::number(N2kHelper::convertN2kFlowRateToUserUnits(newFuelRate, UserPrefs()), 'f', 2));
    emit FuelRateChanged();
}

double GeneratorEngine::EngineHours() const
{
    return m_EngineHours;
}

void GeneratorEngine::setEngineHours(double newEngineHours)
{
    if (qFuzzyCompare(m_EngineHours, newEngineHours))
        return;
    m_EngineHours = newEngineHours;
    //ui->lineEditEngineHours->setText(Convert::SecondsToFormattedTime_hhmmss(m_EngineHours));
    ui->lineEditEngineHours->setText(QString::number(m_EngineHours / 3600, 'f', 2));
    emit EngineHoursChanged();
}

double GeneratorEngine::EngineCoolantPress() const
{
    return m_EngineCoolantPress;
}

void GeneratorEngine::setEngineCoolantPress(double newEngineCoolantPress)
{
    if (qFuzzyCompare(m_EngineCoolantPress, newEngineCoolantPress))
        return;
    m_EngineCoolantPress = newEngineCoolantPress;
    emit EngineCoolantPressChanged();
}

double GeneratorEngine::EngineFuelPress() const
{
    return m_EngineFuelPress;
}

void GeneratorEngine::setEngineFuelPress(double newEngineFuelPress)
{
    if (qFuzzyCompare(m_EngineFuelPress, newEngineFuelPress))
        return;
    m_EngineFuelPress = newEngineFuelPress;
    emit EngineFuelPressChanged();
}

double GeneratorEngine::EngineLoad() const
{
    return m_EngineLoad;
}

void GeneratorEngine::setEngineLoad(double newEngineLoad)
{
    if (qFuzzyCompare(m_EngineLoad, newEngineLoad))
        return;
    m_EngineLoad = newEngineLoad;
    emit EngineLoadChanged();
}

double GeneratorEngine::EngineTorque() const
{
    return m_EngineTorque;
}

void GeneratorEngine::setEngineTorque(double newEngineTorque)
{
    if (qFuzzyCompare(m_EngineTorque, newEngineTorque))
        return;
    m_EngineTorque = newEngineTorque;
    emit EngineTorqueChanged();
}

tN2kEngineDiscreteStatus1 GeneratorEngine::EngineDiscreteStatus1() const
{
    return m_Status1;
}

void GeneratorEngine::setEngineDiscreteStatus1(const tN2kEngineDiscreteStatus1 &newStatus1)
{
    m_Status1 = newStatus1;
    emit EngineDiscreteStatus1Changed();
}

tN2kEngineDiscreteStatus2 GeneratorEngine::EngineDiscreteStatus2() const
{
    return m_Status2;
}

void GeneratorEngine::setEngineDiscreteStatus2(const tN2kEngineDiscreteStatus2 &newStatus2)
{
    m_Status2 = newStatus2;
    emit EngineDiscreteStatus2Changed();
}

void GeneratorEngine::on_comboBoxVesselEngine_currentTextChanged(const QString &arg1)
{
    QString pathToData = RuntimeResources::engineProfiles();
    if (m_EngineRunning) setEngineRunning(false);
    m_activeRpmGphData = getEngineRpmGphData(pathToData, arg1);
    ui->checkBoxStartEngine->setEnabled(profileReady());
    ui->verticalSliderThrottle->setEnabled(profileReady());
    if (!profileReady()) {
        m_idleAndWot = {0, 0};
        ui->verticalSliderThrottle->setRange(0, 0);
        setEngineSpeed(0); setFuelRate(0);
        statusBar->showMessage(QString("Engine profile '%1' unavailable: select a valid engine profile.").arg(arg1));
        return;
    }
    statusBar->showMessage("Press Start engine...");
    m_idleAndWot = getIdleAndWOT(m_activeRpmGphData);
    //m_engineProperties.fuelInjected = isEngineFuelInjected(pathToData, arg1);
    //m_engineProperties.forcedInduction = isEngineForceInjected(pathToData, arg1);
    m_engineProperties = getEngineProperties(pathToData, arg1);
    ui->verticalSliderThrottle->setRange(m_idleAndWot.first, m_idleAndWot.second);
    ui->verticalSliderThrottle->setValue(m_idleAndWot.first);
    setEngineSpeed(0.0); // Set the initial engine speed to 0 RPM, not idle RPM that slider sets
}

void GeneratorEngine::on_checkBoxStartEngine_clicked(bool checked)
{
    setEngineRunning(checked);
}

void GeneratorEngine::on_lineEditEngineHours_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditEngineHours, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineHoursChanged";
    onTimerTimeout();
}

void GeneratorEngine::on_verticalSliderThrottle_valueChanged(int value)
{
    if (EngineRunning()) {
        //setEngineSpeed(value);
        m_ValueToEmit = QString::number(value);
    } else {
        ui->verticalSliderThrottle->setValue(m_idleAndWot.first);
        //setEngineSpeed(0);
        m_ValueToEmit = "0";
    }
    m_SignalToEmit = "EngineSpeedChanged";
    onTimerTimeout();

    m_ThrottlePosition = calculateThrottlePositionPercentageFromRpm(value, m_idleAndWot);
    emit ThrottlePositionChanged();
}

bool GeneratorEngine::EngineRunning() const
{
    return m_EngineRunning;
}

void GeneratorEngine::setEngineRunning(bool newEngineRunning)
{
    if (newEngineRunning && !profileReady()) {
        ui->checkBoxStartEngine->setChecked(false);
        statusBar->showMessage(QString("Engine profile '%1' unavailable: select a valid engine profile.").arg(engineProfile()));
        return;
    }
    if (m_EngineRunning == newEngineRunning)
        return;
    accountFuelUsage();
    m_EngineRunning = newEngineRunning;
    if (newEngineRunning) { m_fuelClock.start(); m_fuelLastNs = 0; }
    else m_fuelClock.invalidate();
    ui->checkBoxStartEngine->setChecked(newEngineRunning);

    if (newEngineRunning) {
        pollTimer->start(RATE_250_MS);
        coolDownTimer->stop();
        m_coolDownCounter = 0;
        ui->comboBoxVesselEngine->setEnabled(false); // Don't allow changing the engine profile while the engine is running
    } else {
        pollTimer->stop();
        m_pollCounter = 0;
        coolDownTimer->start(RATE_1_SEC);
        ui->comboBoxVesselEngine->setEnabled(true);
        ui->verticalSliderThrottle->setValue(m_idleAndWot.first);
        setEngineSpeed(0.0);
        setFuelRate(0.0);
        setEngineLoad(0.0);
        setEngineTorque(0.0);
        setEngineCoolantPress(0.0);
        setEngineFuelPress(0.0);
        setEngineOilPress(0.0);
        setEngineBoostPressure(0.0);
        setAltenatorVoltage(0.0);
    }

    emit EngineRunningChanged();
}

void GeneratorEngine::accountFuelUsage()
{
    if (m_fuelClock.isValid()) {
        const auto now = m_fuelClock.nsecsElapsed();
        const double seconds = (now - m_fuelLastNs) / 1e9;
        m_fuelLastNs = now;
        consumeElapsedFuel(seconds);
    }
    m_fuelEnabled = isEnabled();
}

void GeneratorEngine::consumeElapsedFuel(double seconds)
{
    // The normal engine poll timer drives this accounting even with Sending OFF.
    if (!m_EngineRunning || !m_fuelEnabled || !std::isfinite(seconds) || seconds <= 0
        || !std::isfinite(m_FuelRate) || m_FuelRate <= 0 || m_FuelRate == N2kDoubleNA) return;
    const double liters = m_FuelRate * (seconds / 3600.0);
    if (std::isfinite(liters) && liters > 0) emit fuelConsumed(m_EngineInstance, liters);
}

double GeneratorEngine::ThrottlePosition() const
{
    return m_ThrottlePosition;
}

void GeneratorEngine::setThrottlePosition(double newThrottlePosition)
{
    if (qFuzzyCompare(m_ThrottlePosition, newThrottlePosition))
        return;
    m_ThrottlePosition = newThrottlePosition;
    emit ThrottlePositionChanged();
}

void GeneratorEngine::setRpmFromThrottlePosition(double throttlePositionPercentage)
{
    if (profileReady() && EngineRunning() && m_UsingGenVessel) {
        double rpmSpeed = calculateRpmFromThrottlePosition(throttlePositionPercentage, m_idleAndWot);
        if (ui->checkBoxEng1Rand->isChecked()) {
            rpmSpeed += generateRandNum();
        }
        setEngineSpeed(rpmSpeed);
        ui->verticalSliderThrottle->setValue(rpmSpeed);
    }
}

void GeneratorEngine::setUsingGenVessel(bool usingGenVessel)
{
    m_UsingGenVessel = usingGenVessel;
}

void GeneratorEngine::setInitialGuiValues()
{
    ui->lineEditEngineHours->setText(QString::number(m_EngineHours / 3600, 'f', 2));
    ui->lineEditCoolantTemp->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_EngineCoolantTemp, UserPrefs()), 'f', 2));
    ui->lineEditFuelRate->setText(QString::number(N2kHelper::convertN2kFlowRateToUserUnits(m_FuelRate, UserPrefs()), 'f', 2));
    ui->lineEditEngineSpeed->setText(QString::number(m_EngineSpeed));
}

void GeneratorEngine::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}
