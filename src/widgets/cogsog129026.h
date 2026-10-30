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

#ifndef COGSOG129026_H
#define COGSOG129026_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class CogSog129026;
}

class CogSog129026 : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit CogSog129026(int id, QString label, QWidget *parent = nullptr);
    ~CogSog129026();

    Q_PROPERTY(unsigned char SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(tN2kHeadingReference HeadingReference READ HeadingReference WRITE setHeadingReference NOTIFY HeadingReferenceChanged FINAL)
    Q_PROPERTY(double COG READ COG WRITE setCOG NOTIFY COGChanged FINAL)
    Q_PROPERTY(double SOG READ SOG WRITE setSOG NOTIFY SOGChanged FINAL)

    unsigned char SID() const;
    void setSID(unsigned char newSID);

    double COG() const;
    void setCOG(double newCOG);

    double SOG() const;
    void setSOG(double newSOG);

    tN2kHeadingReference HeadingReference() const;
    void setHeadingReference(tN2kHeadingReference newHeadingReference);

    void setInitialGuiValues() override;

signals:
    void SIDChanged();
    void COGChanged();
    void SOGChanged();
    void HeadingReferenceChanged();

protected:
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_lineEditCog_textEdited(const QString &arg1);
    void on_lineEditSog_textEdited(const QString &arg1);
    void on_checkBoxEnabled_clicked(bool checked);

private:
    Ui::CogSog129026 *ui;
    unsigned char m_SID;
    double m_COG;
    double m_SOG;
    tN2kHeadingReference m_HeadingReference;

    QDoubleValidator *validatorCog = new QDoubleValidator(this);
    QDoubleValidator *validatorSog = new QDoubleValidator(this);
};

#endif // COGSOG129026_H
