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

#ifndef ENGINERAPID127488_H
#define ENGINERAPID127488_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class EngineRapid127488;
}

class EngineRapid127488 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit EngineRapid127488(int id, QString label, QWidget *parent = nullptr);
    ~EngineRapid127488();

    Q_PROPERTY(int EngineInstance READ EngineInstance WRITE setEngineInstance NOTIFY EngineInstanceChanged FINAL)
    Q_PROPERTY(double EngineSpeed READ EngineSpeed WRITE setEngineSpeed NOTIFY EngineSpeedChanged FINAL)
    Q_PROPERTY(double EngineBoostPressure READ EngineBoostPressure WRITE setEngineBoostPressure NOTIFY EngineBoostPressureChanged FINAL)
    Q_PROPERTY(int EngineTiltTrim READ EngineTiltTrim WRITE setEngineTiltTrim NOTIFY EngineTiltTrimChanged FINAL)

    int EngineInstance() const;
    void setEngineInstance(int newEngineInstance);
    double EngineSpeed() const;
    void setEngineSpeed(const double &newEngineSpeed);
    double EngineBoostPressure() const;
    void setEngineBoostPressure(double newEngineBoostPressure);
    int EngineTiltTrim() const;
    void setEngineTiltTrim(int newEngineTiltTrim);
    void setInitialGuiValues() override;

signals:
    void EngineInstanceChanged();
    void EngineSpeedChanged();
    void EngineBoostPressureChanged();
    void EngineTiltTrimChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

private slots:
    void on_lineEditEngineSpeed_textEdited(const QString &arg1);
    void on_lineEditBoostPressure_textEdited(const QString &arg1);
    void on_lineEditEngineTiltTrim_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

protected slots:
    void onTimerTimeout() override;

private:
    Ui::EngineRapid127488 *ui;
    QDoubleValidator *validatorRpm = new QDoubleValidator(this);
    QDoubleValidator *validatorPressure = new QDoubleValidator(this);
    QIntValidator *validatorTiltTrim = new QIntValidator(this);
    int m_EngineInstance = -1;
    double m_EngineSpeed = 0.0;
    double m_EngineBoostPressure = 0.0;
    int m_EngineTiltTrim = 0;
};

#endif // ENGINERAPID127488_H
