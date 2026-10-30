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

#ifndef GENERATORTRANSLOWERUNIT_H
#define GENERATORTRANSLOWERUNIT_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class GeneratorTransLowerUnit;
}

class GeneratorTransLowerUnit : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit GeneratorTransLowerUnit(int id, QString label, QWidget *parent = nullptr);
    ~GeneratorTransLowerUnit();

    Q_PROPERTY(int EngineInstance READ EngineInstance WRITE setEngineInstance NOTIFY EngineInstanceChanged FINAL)
    Q_PROPERTY(tN2kTransmissionGear TransmissionGear READ TransmissionGear WRITE setTransmissionGear NOTIFY TransmissionGearChanged FINAL)
    Q_PROPERTY(double OilPressure READ OilPressure WRITE setOilPressure NOTIFY OilPressureChanged FINAL)
    Q_PROPERTY(double OilTemperature READ OilTemperature WRITE setOilTemperature NOTIFY OilTemperatureChanged FINAL)
    Q_PROPERTY(unsigned char DiscreteStatus1 READ DiscreteStatus1 WRITE setDiscreteStatus1 NOTIFY DiscreteStatus1Changed FINAL)

    // Engine properties not exposed in tableView, for controlling tramission / lower unit state
    Q_PROPERTY(bool EngineRunning READ EngineRunning WRITE setEngineRunning NOTIFY EngineRunningChanged FINAL)
    Q_PROPERTY(double EngineLoad READ EngineLoad WRITE setEngineLoad NOTIFY EngineLoadChanged FINAL)

    void setMyVessel(const struct MyVessel &newMyVessel);

    int EngineInstance() const;
    void setEngineInstance(int newEngineInstance);

    tN2kTransmissionGear TransmissionGear() const;
    void setTransmissionGear(tN2kTransmissionGear newTransmissionGear);

    double OilPressure() const;
    void setOilPressure(double newOilPressure);

    double OilTemperature() const;
    void setOilTemperature(double newOilTemperature);

    unsigned char DiscreteStatus1() const;
    void setDiscreteStatus1(unsigned char newDiscreteStatus1);

    bool EngineRunning() const;
    void setEngineRunning(bool newEngineRunning);

    double EngineLoad() const;
    void setEngineLoad(double newEngineLoad);

    void setInitialGuiValues() override;

signals:
    void EngineInstanceChanged();
    void TransmissionGearChanged();
    void OilPressureChanged();
    void OilTemperatureChanged();
    void DiscreteStatus1Changed();
    void EngineRunningChanged();
    void EngineLoadChanged();

protected:
    void initializeUnitsLimits() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void PollTimerElapsed();
    void CoolDownTimerElapsed();
    void on_radioButtonForward_clicked();
    void on_radioButtonNeutral_clicked();
    void on_radioButtonReverse_clicked();
    void on_lineEditOilPressure_textEdited(const QString &arg1);
    void on_lineEditOilTemperature_textEdited(const QString &arg1);
    void on_horizontalSliderLoad_sliderPressed();

    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::GeneratorTransLowerUnit *ui;
    QTimer *pollTimer;
    QTimer *coolDownTimer;
    struct MyVessel m_MyVessel;
    int m_EngineInstance = -1;
    tN2kTransmissionGear m_TransmissionGear = N2kTG_Neutral;
    double m_OilPressure = 0.0;
    double m_OilTemperature = 0.0;
    unsigned char m_DiscreteStatus1 = 0;
    bool m_EngineRunning = false;
    const int RATE_1_SEC = 1000;
    int m_pollCounter = 0;
    int m_coolDownCounter = 0;
    double m_waterTempK = 295.372;

    double calculateOilTemperature(
        int timeInSeconds, double engineLoadPercentage, double lastOilTemperatureK, bool highPerformance = false, double waterTempK = 295.372);
    double calculateOilPressure(double engineLoadPercentage, std::pair<int, int> pressureRange = {34473, 137895}); // 5-20 psi
    double m_EngineLoad = 0.0;
};

#endif // GENERATORTRANSLOWERUNIT_H
