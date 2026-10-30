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

#include "spacer000000.h"
#include "ui_spacer000000.h"

Spacer000000::Spacer000000(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::spacer000000)
{
    ui->setupUi(this);
    setId(id);
    setPgn(0);
    setColumnSpan(3);
    setRowSpan(2);

    ui->labelTitle->setText(label + "Rows " + QString::number(Rows()));
    ui->pushButtonRemove->setToolTip("Remove " + label);

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

Spacer000000::~Spacer000000()
{
    delete ui;
}

int Spacer000000::Rows() const
{
    return m_Rows;
}

void Spacer000000::setRows(int newRows)
{
    if (m_Rows == newRows)
        return;
    m_Rows = newRows;
    setRowSpan(newRows);
    this->setMinimumHeight(RowSpan() * 30);
    emit RowsChanged();
}

bool Spacer000000::showTitleRow() const
{
    return m_showTitleRow;
}

void Spacer000000::setShowTitleRow(bool newShowTitleRow)
{
    if (m_showTitleRow == newShowTitleRow)
        return;
    m_showTitleRow = newShowTitleRow;

    if (m_showTitleRow) {
        ui->labelTitle->show();
        ui->pushButtonRemove->show();
    } else {
        ui->labelTitle->hide();
        ui->pushButtonRemove->hide();
    }

    emit showTitleRowChanged();
}

void Spacer000000::setInitialGuiValues()
{
    ui->labelTitle->setText(Label() + "Rows " + QString::number(Rows()));
}
