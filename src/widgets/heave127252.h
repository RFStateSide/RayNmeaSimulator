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

#ifndef HEAVE127252_H
#define HEAVE127252_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class Heave127252;
}

class Heave127252 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit Heave127252(int id, const QString &label, QWidget *parent);
    ~Heave127252();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double Heave READ Heave WRITE setHeave NOTIFY HeaveChanged FINAL)
    Q_PROPERTY(double Delay READ Delay WRITE setDelay NOTIFY DelayChanged FINAL)
    Q_PROPERTY(tN2kDelaySource DelaySource READ DelaySource WRITE setDelaySource NOTIFY DelaySourceChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    double Heave() const;
    void setHeave(double newHeave);

    double Delay() const;
    void setDelay(double newDelay);

    tN2kDelaySource DelaySource() const;
    void setDelaySource(tN2kDelaySource newDelaySource);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void HeaveChanged();
    void DelayChanged();
    void DelaySourceChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditHeave_textEdited(const QString &arg1);

    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::Heave127252 *ui;

    QDoubleValidator *validatorHeave = new QDoubleValidator(this);
    unsigned char m_SID = 0;
    double m_Heave = 0;
    double m_Delay = 0;
    tN2kDelaySource m_DelaySource = N2kDD374_UserDefined;
};

#endif // HEAVE127252_H
