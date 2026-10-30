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

#ifndef ATTITUDE127257_H
#define ATTITUDE127257_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class Attitude127257;
}

class Attitude127257 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit Attitude127257(int id, const QString &label, QWidget *parent);
    ~Attitude127257();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double Yaw READ Yaw WRITE setYaw NOTIFY YawChanged FINAL)
    Q_PROPERTY(double Pitch READ Pitch WRITE setPitch NOTIFY PitchChanged FINAL)
    Q_PROPERTY(double Roll READ Roll WRITE setRoll NOTIFY RollChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    double Yaw() const;
    void setYaw(double newYaw);

    double Pitch() const;
    void setPitch(double newPitch);

    double Roll() const;
    void setRoll(double newRoll);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void YawChanged();
    void PitchChanged();
    void RollChanged();

protected:
    void initializeUnitsLimits() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditYaw_textEdited(const QString &arg1);
    void on_lineEditPitch_textEdited(const QString &arg1);
    void on_lineEditRoll_textEdited(const QString &arg1);
    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::Attitude127257 *ui;
    unsigned char m_SID;
    double m_Yaw;
    double m_Pitch;
    double m_Roll;

    QDoubleValidator *degValidator;
};

#endif // ATTITUDE127257_H
