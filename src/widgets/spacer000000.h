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

#ifndef SPACER000000_H
#define SPACER000000_H

#include "pgnwidgetbase.h"

namespace Ui {
class spacer000000;
}

class Spacer000000 : public PgnWidgetBase
{
    Q_OBJECT

    Q_PROPERTY(int Rows READ Rows WRITE setRows NOTIFY RowsChanged FINAL)
    Q_PROPERTY(bool showTitleRow READ showTitleRow WRITE setShowTitleRow NOTIFY showTitleRowChanged FINAL)

public:
    explicit Spacer000000(int id, const QString &label, QWidget *parent);
    ~Spacer000000();

    int Rows() const;
    void setRows(int newRows);

    bool showTitleRow() const;
    void setShowTitleRow(bool newShowTitleRow);

    void setInitialGuiValues() override;

signals:
    void RowsChanged();

    void showTitleRowChanged();

private:
    Ui::spacer000000 *ui;
    int m_Rows = 2;
    bool m_showTitleRow = true;
};

#endif // SPACER000000_H
