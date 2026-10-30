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

#ifndef TEMPERATUREEXTENDED130316_H
#define TEMPERATUREEXTENDED130316_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class TemperatureExtended130316;
}

class TemperatureExtended130316 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit TemperatureExtended130316(int id, const QString &label, QWidget *parent);
    ~TemperatureExtended130316();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(unsigned char TempInstance READ TempInstance WRITE setTempInstance NOTIFY TempInstanceChanged FINAL)
    Q_PROPERTY(tN2kTempSource TempSource READ TempSource WRITE setTempSource NOTIFY TempSourceChanged FINAL)
    Q_PROPERTY(double ActualTemperature READ ActualTemperature WRITE setActualTemperature NOTIFY ActualTemperatureChanged FINAL)
    Q_PROPERTY(double SetTemperature READ SetTemperature WRITE setSetTemperature NOTIFY SetTemperatureChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    unsigned char TempInstance() const;
    void setTempInstance(unsigned char newTempInstance);

    tN2kTempSource TempSource() const;
    void setTempSource(tN2kTempSource newTempSource);

    double ActualTemperature() const;
    void setActualTemperature(double newActualTemperature);

    double SetTemperature() const;
    void setSetTemperature(double newSetTemperature);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void TempInstanceChanged();
    void TempSourceChanged();
    void ActualTemperatureChanged();
    void SetTemperatureChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEdit_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::TemperatureExtended130316 *ui;
    unsigned char m_SID;
    unsigned char m_TempInstance;
    tN2kTempSource m_TempSource;
    double m_ActualTemperature;
    double m_SetTemperature;

    QDoubleValidator *ActualTemperatureValidator = new QDoubleValidator(this);
};

#endif // TEMPERATUREEXTENDED130316_H
