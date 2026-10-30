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

#ifndef GENERATORENGINE_H
#define GENERATORENGINE_H

#include <QDoubleValidator>
#include <QElapsedTimer>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"
#include "qdatetime.h"
#include "qstatusbar.h"

namespace Ui {
class GeneratorEngine;
}

class GeneratorEngine : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit GeneratorEngine(int id, QString label, QWidget *parent = nullptr);
    ~GeneratorEngine();

    Q_PROPERTY(int EngineInstance READ EngineInstance WRITE setEngineInstance NOTIFY EngineInstanceChanged FINAL)
    Q_PROPERTY(double EngineSpeed READ EngineSpeed WRITE setEngineSpeed NOTIFY EngineSpeedChanged FINAL)
    Q_PROPERTY(double EngineBoostPressure READ EngineBoostPressure WRITE setEngineBoostPressure NOTIFY EngineBoostPressureChanged FINAL)
    Q_PROPERTY(int EngineTiltTrim READ EngineTiltTrim WRITE setEngineTiltTrim NOTIFY EngineTiltTrimChanged FINAL)
    Q_PROPERTY(double EngineOilPress READ EngineOilPress WRITE setEngineOilPress NOTIFY EngineOilPressChanged FINAL)
    Q_PROPERTY(double EngineOilTemp READ EngineOilTemp WRITE setEngineOilTemp NOTIFY EngineOilTempChanged FINAL)
    Q_PROPERTY(double EngineCoolantTemp READ EngineCoolantTemp WRITE setEngineCoolantTemp NOTIFY EngineCoolantTempChanged FINAL)
    Q_PROPERTY(double AltenatorVoltage READ AltenatorVoltage WRITE setAltenatorVoltage NOTIFY AltenatorVoltageChanged FINAL)
    Q_PROPERTY(double FuelRate READ FuelRate WRITE setFuelRate NOTIFY FuelRateChanged FINAL)
    Q_PROPERTY(double EngineHours READ EngineHours WRITE setEngineHours NOTIFY EngineHoursChanged FINAL)
    Q_PROPERTY(double EngineCoolantPress READ EngineCoolantPress WRITE setEngineCoolantPress NOTIFY EngineCoolantPressChanged FINAL)
    Q_PROPERTY(double EngineFuelPress READ EngineFuelPress WRITE setEngineFuelPress NOTIFY EngineFuelPressChanged FINAL)
    Q_PROPERTY(double EngineLoad READ EngineLoad WRITE setEngineLoad NOTIFY EngineLoadChanged FINAL)
    Q_PROPERTY(double EngineTorque READ EngineTorque WRITE setEngineTorque NOTIFY EngineTorqueChanged FINAL)
    Q_PROPERTY(tN2kEngineDiscreteStatus1 Status1 READ EngineDiscreteStatus1 WRITE setEngineDiscreteStatus1 NOTIFY EngineDiscreteStatus1Changed FINAL)
    Q_PROPERTY(tN2kEngineDiscreteStatus2 Status2 READ EngineDiscreteStatus2 WRITE setEngineDiscreteStatus2 NOTIFY EngineDiscreteStatus2Changed FINAL)
    Q_PROPERTY(bool EngineRunning READ EngineRunning WRITE setEngineRunning NOTIFY EngineRunningChanged FINAL)
    Q_PROPERTY(double ThrottlePosition READ ThrottlePosition WRITE setThrottlePosition NOTIFY ThrottlePositionChanged FINAL)

    bool reloadEngineProfiles();
    bool profileReady() const { return m_activeRpmGphData.size() >= 2; }
    QString engineProfile() const;
    void setEngineProfile(const QString &name);
    void setMyVessel(const struct MyVessel &newMyVessel);

    int EngineInstance() const;
    void setEngineInstance(int newEngineInstance);

    double EngineSpeed() const;
    void setEngineSpeed(double newEngineSpeed);

    double EngineBoostPressure() const;
    void setEngineBoostPressure(double newEngineBoostPressure);

    int EngineTiltTrim() const;
    void setEngineTiltTrim(int newEngineTiltTrim);

    double EngineOilPress() const;
    void setEngineOilPress(double newEngineOilPress);

    double EngineOilTemp() const;
    void setEngineOilTemp(double newEngineOilTemp);

    double EngineCoolantTemp() const;
    void setEngineCoolantTemp(double newEngineCoolantTemp);

    double AltenatorVoltage() const;
    void setAltenatorVoltage(double newAltenatorVoltage);

    double FuelRate() const;
    void setFuelRate(double newFuelRate);

    double EngineHours() const;
    void setEngineHours(double newEngineHours);

    double EngineCoolantPress() const;
    void setEngineCoolantPress(double newEngineCoolantPress);

    double EngineFuelPress() const;
    void setEngineFuelPress(double newEngineFuelPress);

    double EngineLoad() const;
    void setEngineLoad(double newEngineLoad);

    double EngineTorque() const;
    void setEngineTorque(double newEngineTorque);

    tN2kEngineDiscreteStatus1 EngineDiscreteStatus1() const;
    void setEngineDiscreteStatus1(const tN2kEngineDiscreteStatus1 &newStatus1);

    tN2kEngineDiscreteStatus2 EngineDiscreteStatus2() const;
    void setEngineDiscreteStatus2(const tN2kEngineDiscreteStatus2 &newStatus2);

    bool EngineRunning() const;
    void setEngineRunning(bool newEngineRunning);

    double ThrottlePosition() const;
    void setThrottlePosition(double newThrottlePosition);
    void setRpmFromThrottlePosition(double throttlePositionPercentage);
    void setUsingGenVessel(bool usingGenVessel);

    void setInitialGuiValues() override;

signals:
    void EngineInstanceChanged();
    void EngineSpeedChanged();
    void EngineBoostPressureChanged();
    void EngineTiltTrimChanged();
    void EngineOilPressChanged();
    void EngineOilTempChanged();
    void EngineCoolantTempChanged();
    void AltenatorVoltageChanged();
    void FuelRateChanged();
    void EngineHoursChanged();
    void EngineCoolantPressChanged();
    void EngineFuelPressChanged();
    void EngineLoadChanged();
    void EngineTorqueChanged();
    void EngineDiscreteStatus1Changed();
    void EngineDiscreteStatus2Changed();
    void EngineRunningChanged();
    void fuelConsumed(int engineInstance, double liters);

    void ThrottlePositionChanged();

protected:
    void initializeUnitsLimits() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void accountFuelUsage();
    void consumeElapsedFuel(double seconds);
    void PollTimerElapsed();
    void CoolDownTimerElapsed();
    void on_comboBoxVesselEngine_currentTextChanged(const QString &arg1);
    void on_checkBoxStartEngine_clicked(bool checked);
    void on_lineEditEngineHours_textEdited(const QString &arg1);
    void on_verticalSliderThrottle_valueChanged(int value);

    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::GeneratorEngine *ui;
    QStatusBar *statusBar;
    QTimer *pollTimer = nullptr;
    QTimer *coolDownTimer = nullptr;
    QTime time = QTime::currentTime();
    struct MyVessel m_MyVessel;
    int m_EngineInstance = -1;
    double m_EngineSpeed = 0.0;
    double m_EngineBoostPressure = 0.0;
    int m_EngineTiltTrim = 0;
    double m_EngineOilPress = 0.0;
    double m_EngineOilTemp = 0.0;
    double m_EngineCoolantTemp = 0.0;
    double m_AltenatorVoltage = 0.0;
    double m_FuelRate = 0.0;
    double m_EngineHours = 0.0;
    double m_EngineCoolantPress = 0.0;
    double m_EngineFuelPress = 0.0;
    double m_EngineLoad = 0.0;
    double m_EngineTorque = 0.0;
    bool m_EngineRunning = false;
    QElapsedTimer m_fuelClock;
    qint64 m_fuelLastNs = 0;
    bool m_fuelEnabled = true;
    tN2kEngineDiscreteStatus1 m_Status1 = 0;
    tN2kEngineDiscreteStatus2 m_Status2 = 0;

    const int RATE_250_MS = 250;
    const int RATE_1_SEC = 1000;
    int m_pollCounter = 0;
    int m_coolDownCounter = 0;
    double m_engineRoomTempF = 72.0;

    std::vector<std::pair<int, double>> m_activeRpmGphData
        = {{600, 1.3}, {1000, 2.5}, {1500, 3.5}, {2000, 6.1}, {2500, 9.9}, {3000, 12.4}, {3500, 17.4}, {4000, 22.0}, {4600, 28.5}};
    std::pair<int, int> m_idleAndWot = {600, 4600};
    //bool m_fuelInjected = true;
    //bool m_forceInjected = false; // Tubrocharged or Supercharged
    EngineProperties m_engineProperties;
    double m_engineRmTempK = 295.372; // 72°F

    // Methods
    void initilizeComboBoxes();
    void initializeOthers();
    //QVector<QPair<int, double>> getEngineRpmGphData(const QString &filePath, const QString &engineName);
    std::vector<std::pair<int, double>> getEngineRpmGphData(const QString &filePath, const QString &engineName);
    int getWOT(const std::vector<std::pair<int, double>> &rpmGphData);
    std::pair<int, int> getIdleAndWOT(const std::vector<std::pair<int, double>> &rpmGphData);
    //bool isEngineFuelInjected(const QString &filePath, const QString &engineName);
    //bool isEngineForceInjected(const QString &filePath, const QString &engineName);
    EngineProperties getEngineProperties(const QString &filePath, const QString &engineName);

    double calculateThrottlePositionPercentageFromRpm(int rpm, std::pair<int, int> idleAndWot = {600, 4600});
    double calculateRpmFromThrottlePosition(double throttlePositionPercentage, std::pair<int, int> idleAndWot = {600, 4600});
    double calculateFlowRateGph(int rpm);
    double calculateCoolantTemperature(int timeInSeconds, double throttlePositionPercentage, double lastCoolantTemperatureK, double engineRmTempK = 295.372);
    double calculateOilTemperature(
        int timeInSeconds, double throttlePositionPercentage, double lastOilTemperatureK, bool highPerformance = false, double engineRmTempK = 295.372);
    double calculatePressure(double throttlePositionPercentage, std::pair<int, int> pressureRange = {68947, 137895}); // 10-20 psi
    double calculateFuelPressure(double throttlePositionPercentage,
                                 bool fuelInjected = true,
                                 std::pair<int, int> pressureRangeCarbureted = {27579, 68947},      // 4-10 psi
                                 std::pair<int, int> pressureRangeFuelInjected = {275790, 413685}); // 40-60 psi
    double calculateBoostPressure(double throttlePositionPercentage,
                                  QString fuelType = "Gasoline",
                                  std::pair<int, int> pressureGasolineRange = {68947, 179263}, // 10-26 psi
                                  std::pair<int, int> pressureDieselRange = {103421, 248211}); // 15-36 psi
    double calculateVoltage(double throttlePositionPercentage,
                            int alternatorVoltageType = 12,
                            std::pair<int, int> voltage12Range = {13.8, 14.4},
                            std::pair<int, int> voltage24Range = {27.6, 28.8});
    double calculateOilTemperature(int timeInSeconds, double throttlePositionPercentage);
    double calculateOilPressure(double throttlePositionPercentage);
    int generateRandNum(int low = -25, int high = 25);
    double m_ThrottlePosition = 0.0;
    bool m_UsingGenVessel = false; // Using the generator vessel data to set the engine data
};

#endif // GENERATORENGINE_H
