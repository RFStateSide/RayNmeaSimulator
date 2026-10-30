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

#ifndef TRANSMISSIONDYNAMIC127493_H
#define TRANSMISSIONDYNAMIC127493_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class TransmissionDynamic127493;
}

class TransmissionDynamic127493 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit TransmissionDynamic127493(int id, const QString &label, QWidget *parent);
    ~TransmissionDynamic127493();

    Q_PROPERTY(unsigned char EngineInstance READ EngineInstance WRITE setEngineInstance NOTIFY EngineInstanceChanged FINAL)
    Q_PROPERTY(tN2kTransmissionGear TransmissionGear READ TransmissionGear WRITE setTransmissionGear NOTIFY TransmissionGearChanged FINAL)
    Q_PROPERTY(double OilPressure READ OilPressure WRITE setOilPressure NOTIFY OilPressureChanged FINAL)
    Q_PROPERTY(double OilTemperature READ OilTemperature WRITE setOilTemperature NOTIFY OilTemperatureChanged FINAL)
    Q_PROPERTY(unsigned char DiscreteStatus1 READ DiscreteStatus1 WRITE setDiscreteStatus1 NOTIFY DiscreteStatus1Changed FINAL)
    // INFO: DiscreteStatus2 not implemented in NMEA2000 library - coded in widget only for future implementation
    Q_PROPERTY(unsigned char DiscreteStatus2 READ DiscreteStatus2 WRITE setDiscreteStatus2 NOTIFY DiscreteStatus2Changed FINAL)

    unsigned char EngineInstance() const;
    void setEngineInstance(unsigned char newEngineInstance);

    tN2kTransmissionGear TransmissionGear() const;
    void setTransmissionGear(tN2kTransmissionGear newTransmissionGear);

    double OilPressure() const;
    void setOilPressure(double newOilPressure);

    double OilTemperature() const;
    void setOilTemperature(double newOilTemperature);

    unsigned char DiscreteStatus1() const;
    void setDiscreteStatus1(unsigned char newDiscreteStatus1);

    void setInitialGuiValues() override;

    unsigned char DiscreteStatus2() const;
    void setDiscreteStatus2(unsigned char newDiscreteStatus2);

signals:
    void EngineInstanceChanged();
    void TransmissionGearChanged();
    void OilPressureChanged();
    void OilTemperatureChanged();
    void DiscreteStatus1Changed();

    void DiscreteStatus2Changed();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_radioButtonReverse_clicked();
    void on_radioButtonNeutral_clicked();
    void on_radioButtonForward_clicked();
    void on_lineEditOilPressure_textEdited(const QString &arg1);
    void on_lineEditOilTemperature_textEdited(const QString &arg1);
    void on_checkBoxEnabled_clicked(bool checked);
    void on_checkBoxOverTemperature_clicked(bool checked);
    void on_checkBoxLowOilPressure_clicked(bool checked);
    void on_checkBoxCheckTransmission_clicked(bool checked);
    void on_checkBoxLowOilLevel_clicked(bool checked);
    void on_checkBoxSailDrive_clicked(bool checked);
    void on_checkBoxSlipConditionDetected_clicked(bool checked);
    void on_checkBoxPowerTakeOffEngaged_clicked(bool checked);
    void on_checkBoxClutchOverload_clicked(bool checked);

private:
    Ui::TransmissionDynamic127493 *ui;
    unsigned char m_EngineInstance{};
    tN2kTransmissionGear m_TransmissionGear{};
    double m_OilPressure{};
    double m_OilTemperature{};
    unsigned char m_DiscreteStatus1{};

    QDoubleValidator *validatorTemperature = new QDoubleValidator(this);
    QDoubleValidator *validatorPressure = new QDoubleValidator(this);
    unsigned char m_DiscreteStatus2{};
};

#endif // TRANSMISSIONDYNAMIC127493_H
