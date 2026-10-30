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

#ifndef COGSOGVTG_H
#define COGSOGVTG_H

#include "N2kTypes.h"
#include "pgnwidgetbase.h"
#include "qvalidator.h"

namespace Ui {
class CogSogVTG;
}

class CogSogVTG : public PgnWidgetBase
{
    Q_OBJECT

    //double TrueCOG, double MagneticCOG, double SOG
    Q_PROPERTY(double TrueCOG READ TrueCOG WRITE setTrueCOG NOTIFY TrueCOGChanged FINAL)
    Q_PROPERTY(double MagneticCOG READ MagneticCOG WRITE setMagneticCOG NOTIFY MagneticCOGChanged FINAL)
    Q_PROPERTY(double SOG READ SOG WRITE setSOG NOTIFY SOGChanged FINAL)

public:
    explicit CogSogVTG(int id, QString label, QWidget *parent = nullptr);
    ~CogSogVTG();

    void setInitialGuiValues() override;

    double TrueCOG() const;
    void setTrueCOG(double newTrueCOG);

    double MagneticCOG() const;
    void setMagneticCOG(double newMagneticCOG);

    double SOG() const;
    void setSOG(double newSOG);

signals:
    void TrueCOGChanged();

    void MagneticCOGChanged();

    void SOGChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_checkBoxEnabled_clicked(bool checked);

    void on_lineEditTrueCog_textEdited(const QString &arg1);

    void on_lineEditMagneticCog_textEdited(const QString &arg1);

    void on_lineEditSog_textEdited(const QString &arg1);

private:
    Ui::CogSogVTG *ui;
    double m_TrueCOG;
    double m_MagneticCOG;
    double m_SOG;

    QDoubleValidator *validatorCog = new QDoubleValidator(this);
    QDoubleValidator *validatorSog = new QDoubleValidator(this);
};

#endif // COGSOGVTG_H
