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

#ifndef CROSSTRACK129283_H
#define CROSSTRACK129283_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class CrossTrack129283;
}

class CrossTrack129283 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit CrossTrack129283(int id, const QString &label, QWidget *parent);
    ~CrossTrack129283();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(tN2kXTEMode XTEMode READ XTEMode WRITE setXTEMode NOTIFY XTEModeChanged FINAL)
    Q_PROPERTY(bool NavigationTerminated READ NavigationTerminated WRITE setNavigationTerminated NOTIFY NavigationTerminatedChanged FINAL)
    Q_PROPERTY(double XTE READ XTE WRITE setXTE NOTIFY XTEChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    tN2kXTEMode XTEMode() const;
    void setXTEMode(tN2kXTEMode newXTEMode);

    bool NavigationTerminated() const;
    void setNavigationTerminated(bool newNavigationTerminated);

    double XTE() const;
    void setXTE(double newXTE);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void XTEModeChanged();
    void NavigationTerminatedChanged();
    void XTEChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditXTE_textEdited(const QString &arg1);
    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::CrossTrack129283 *ui;

    QDoubleValidator *XTEValidator = nullptr;
    unsigned char m_SID = 0;
    tN2kXTEMode m_XTEMode = N2kxtem_Autonomous;
    bool m_NavigationTerminated = false;
    double m_XTE = 0;
};

#endif // CROSSTRACK129283_H
