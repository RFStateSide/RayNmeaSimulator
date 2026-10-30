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

#ifndef FLUIDLEVEL127505_H
#define FLUIDLEVEL127505_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class FluidLevel127505;
}

class FluidLevel127505 : public PgnWidgetBase
{
    Q_OBJECT

    Q_PROPERTY(unsigned char Instance READ Instance WRITE setInstance NOTIFY InstanceChanged FINAL)
    Q_PROPERTY(tN2kFluidType FluidType READ FluidType WRITE setFluidType NOTIFY FluidTypeChanged FINAL)
    Q_PROPERTY(double Level READ Level WRITE setLevel NOTIFY LevelChanged FINAL)
    Q_PROPERTY(double Capacity READ Capacity WRITE setCapacity NOTIFY CapacityChanged FINAL)

public:
    explicit FluidLevel127505(int id, const QString &label, QWidget *parent);
    ~FluidLevel127505();

    unsigned char Instance() const;
    void setInstance(unsigned char newInstance);

    tN2kFluidType FluidType() const;
    void setFluidType(tN2kFluidType newFluidType);

    double Level() const;
    void setLevel(double newLevel);

    double Capacity() const;
    void setCapacity(double newCapacity);

    void setInitialGuiValues() override;

signals:
    void InstanceChanged();
    void FluidTypeChanged();
    void LevelChanged();
    void CapacityChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_checkBoxEnabled_clicked(bool checked);
    void on_lineEditLevel_textEdited(const QString &arg1);
    void on_lineEditCapacity_textEdited(const QString &arg1);

private:
    Ui::FluidLevel127505 *ui;
    unsigned char m_Instance;
    tN2kFluidType m_FluidType;
    double m_Level;
    double m_Capacity;
};

#endif // FLUIDLEVEL127505_H
