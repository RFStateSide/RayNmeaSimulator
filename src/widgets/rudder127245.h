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

#ifndef RUDDER127245_H
#define RUDDER127245_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class Rudder127245;
}

class Rudder127245 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit Rudder127245(int id, QString label, QWidget *parent = nullptr);
    ~Rudder127245();

    Q_PROPERTY(double RudderPosition READ RudderPosition WRITE setRudderPosition NOTIFY RudderPositionChanged FINAL)
    Q_PROPERTY(unsigned char Instance READ Instance WRITE setInstance NOTIFY InstanceChanged FINAL)
    Q_PROPERTY(tN2kRudderDirectionOrder RudderDirectionOrder READ RudderDirectionOrder WRITE setRudderDirectionOrder NOTIFY RudderDirectionOrderChanged FINAL)
    Q_PROPERTY(double AngleOrder READ AngleOrder WRITE setAngleOrder NOTIFY AngleOrderChanged FINAL)

    double RudderPosition() const;
    void setRudderPosition(double newRudderPosition);

    unsigned char Instance() const;
    void setInstance(unsigned char newInstance);

    tN2kRudderDirectionOrder RudderDirectionOrder() const;
    void setRudderDirectionOrder(tN2kRudderDirectionOrder newRudderDirectionOrder);

    double AngleOrder() const;
    void setAngleOrder(double newAngleOrder);

    void setInitialGuiValues() override;

signals:
    void RudderPositionChanged();
    void InstanceChanged();
    void RudderDirectionOrderChanged();
    void AngleOrderChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditRudderPosition_textEdited(const QString &arg1);

private:
    Ui::Rudder127245 *ui;
    double m_RudderPosition;
    unsigned char m_Instance;
    tN2kRudderDirectionOrder m_RudderDirectionOrder;
    double m_AngleOrder;
};

#endif // RUDDER127245_H
