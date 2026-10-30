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

#ifndef TEMPERATUREWIDGET130312_H
#define TEMPERATUREWIDGET130312_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "N2kMsg.h"
#include "pgnwidgetbase.h"

namespace Ui {
class TemperatureWidget130312;
}

class TemperatureWidget130312 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit TemperatureWidget130312(int id, const QString &label, QWidget *parent);
    ~TemperatureWidget130312();

    Q_PROPERTY(uchar TemperatureInstance READ TemperatureInstance WRITE setTemperatureInstance NOTIFY TemperatureInstanceChanged FINAL)
    Q_PROPERTY(uchar SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(tN2kTempSource TemperatureSource READ TemperatureSource WRITE setTemperatureSource NOTIFY TemperatureSourceChanged FINAL)
    Q_PROPERTY(double ActualTemperature READ ActualTemperature WRITE setActualTemperature NOTIFY ActualTemperatureChanged FINAL)
    Q_PROPERTY(double SetTemperature READ SetTemperature WRITE setSetTemperature NOTIFY SetTemperatureChanged FINAL)

    uchar TemperatureInstance() const;
    void setTemperatureInstance(uchar newTemperatureInstance);
    uchar SID() const;
    void setSID(uchar newSID);
    tN2kTempSource TemperatureSource() const;
    void setTemperatureSource(tN2kTempSource newTemperatureSource);
    double ActualTemperature() const;
    void setActualTemperature(double newActualTemperature);
    double SetTemperature() const;
    void setSetTemperature(double newSetTemperature);

    void setInitialGuiValues() override;

signals:
    void TemperatureInstanceChanged();
    void SIDChanged();
    void TemperatureSourceChanged();
    void ActualTemperatureChanged();
    void SetTemperatureChanged();

private slots:
    void on_lineEdit_textEdited(const QString &arg1);
    void on_checkBoxEnabled_clicked(bool checked);

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private:
    Ui::TemperatureWidget130312 *ui;
    QDoubleValidator *validatorTemperature = new QDoubleValidator(this);

    uchar m_TemperatureInstance;
    uchar m_SID;
    tN2kTempSource m_TemperatureSource;
    double m_ActualTemperature = N2kDoubleNA;
    double m_SetTemperature;
};

#endif // TEMPERATUREWIDGET130312_H
