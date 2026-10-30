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

#ifndef TRIMTAB130576_H
#define TRIMTAB130576_H

#include <QDoubleValidator>
#include "N2kTypes.h"
#include "pgnwidgetbase.h"

namespace Ui {
class TrimTab130576;
}

class TrimTab130576 : public PgnWidgetBase
{
    Q_OBJECT

    // int8_t PortTrimTab, int8_t StbdTrimTab
    Q_PROPERTY(int8_t PortTrimTab READ PortTrimTab WRITE setPortTrimTab NOTIFY PortTrimTabChanged FINAL)
    Q_PROPERTY(int8_t StbdTrimTab READ StbdTrimTab WRITE setStbdTrimTab NOTIFY StbdTrimTabChanged FINAL)

public:
    explicit TrimTab130576(int id, QString label, QWidget *parent = nullptr);
    ~TrimTab130576();

    void setInitialGuiValues() override;

    int8_t PortTrimTab() const;
    void setPortTrimTab(int8_t newPortTrimTab);

    int8_t StbdTrimTab() const;
    void setStbdTrimTab(int8_t newStbdTrimTab);

signals:
    void PortTrimTabChanged();
    void StbdTrimTabChanged();

protected:
    void initializeUnitsLimits() override;

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_checkBoxEnabled_clicked(bool checked);
    void on_lineEditPortTrimTab_textEdited(const QString &arg1);
    void on_lineEditStbdTrimTab_textEdited(const QString &arg1);

private:
    Ui::TrimTab130576 *ui;
    int8_t m_PortTrimTab;
    int8_t m_StbdTrimTab;

    QIntValidator *trimTabValidator = new QIntValidator(0, 100, this);
};

#endif // TRIMTAB130576_H
