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

#include "trimtab130576.h"
#include "ui_trimtab130576.h"

TrimTab130576::TrimTab130576(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::TrimTab130576)
{
    ui->setupUi(this);
    setId(id);
    setPgn(130576);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (130576)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

TrimTab130576::~TrimTab130576()
{
    delete ui;
}

void TrimTab130576::setInitialGuiValues()
{
    ui->lineEditPortTrimTab->setText(QString::number(m_PortTrimTab));
    ui->lineEditStbdTrimTab->setText(QString::number(m_StbdTrimTab));
}

void TrimTab130576::initializeUnitsLimits()
{
    ui->labelUnitsPortTrimTab->setText("%");
    ui->labelUnitsStbdTrimTab->setText("%");
    ui->lineEditPortTrimTab->setValidator(trimTabValidator);
    ui->lineEditStbdTrimTab->setValidator(trimTabValidator);
}

void TrimTab130576::onTimerTimeout()
{
    if (m_SignalToEmit == "PortTrimTabChanged") {
        setPortTrimTab(m_ValueToEmit.toInt());
    } else if (m_SignalToEmit == "StbdTrimTabChanged") {
        setStbdTrimTab(m_ValueToEmit.toInt());
    }

    m_SignalToEmit.clear();
}

void TrimTab130576::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void TrimTab130576::on_lineEditPortTrimTab_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditPortTrimTab, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "PortTrimTabChanged";
    onTimerTimeout();
}

void TrimTab130576::on_lineEditStbdTrimTab_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditStbdTrimTab, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "StbdTrimTabChanged";
    onTimerTimeout();
}

int8_t TrimTab130576::PortTrimTab() const
{
    return m_PortTrimTab;
}

void TrimTab130576::setPortTrimTab(int8_t newPortTrimTab)
{
    if (m_PortTrimTab == newPortTrimTab)
        return;
    m_PortTrimTab = newPortTrimTab;
    ui->lineEditPortTrimTab->setText(QString::number(m_PortTrimTab));
    emit PortTrimTabChanged();
}

int8_t TrimTab130576::StbdTrimTab() const
{
    return m_StbdTrimTab;
}

void TrimTab130576::setStbdTrimTab(int8_t newStbdTrimTab)
{
    if (m_StbdTrimTab == newStbdTrimTab)
        return;
    m_StbdTrimTab = newStbdTrimTab;
    ui->lineEditStbdTrimTab->setText(QString::number(m_StbdTrimTab));
    emit StbdTrimTabChanged();
}
