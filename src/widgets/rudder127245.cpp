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

#include "rudder127245.h"
#include "ui_rudder127245.h"

#include "n2khelper.h"

Rudder127245::Rudder127245(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::Rudder127245)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127245);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127245)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

Rudder127245::~Rudder127245()
{
    delete ui;
}

void Rudder127245::setInitialGuiValues()
{
    ui->lineEditRudderPosition->setText(QString::number(Convert::RadiansToDegrees(m_RudderPosition), 'f', 1));
}

void Rudder127245::initializeUnitsLimits()
{
    ui->lineEditRudderPosition->setValidator(new QDoubleValidator(-45.0, 45.0, 1, this));
}

void Rudder127245::initializeClickableLabels() {}

void Rudder127245::onTimerTimeout()
{
    if (m_SignalToEmit == "RudderPositionChanged") {
        m_RudderPosition = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        emit RudderPositionChanged();
    }

    m_SignalToEmit.clear();
}

double Rudder127245::RudderPosition() const
{
    return m_RudderPosition;
}

void Rudder127245::setRudderPosition(double newRudderPosition)
{
    if (qFuzzyCompare(m_RudderPosition, newRudderPosition))
        return;
    m_RudderPosition = newRudderPosition;
    ui->lineEditRudderPosition->setText(QString::number(Convert::RadiansToDegrees(m_RudderPosition), 'f', 1));
    emit RudderPositionChanged();
}

unsigned char Rudder127245::Instance() const
{
    return m_Instance;
}

void Rudder127245::setInstance(unsigned char newInstance)
{
    if (m_Instance == newInstance)
        return;
    m_Instance = newInstance;
    emit InstanceChanged();
}

tN2kRudderDirectionOrder Rudder127245::RudderDirectionOrder() const
{
    return m_RudderDirectionOrder;
}

void Rudder127245::setRudderDirectionOrder(tN2kRudderDirectionOrder newRudderDirectionOrder)
{
    if (m_RudderDirectionOrder == newRudderDirectionOrder)
        return;
    m_RudderDirectionOrder = newRudderDirectionOrder;
    emit RudderDirectionOrderChanged();
}

double Rudder127245::AngleOrder() const
{
    return m_AngleOrder;
}

void Rudder127245::setAngleOrder(double newAngleOrder)
{
    if (qFuzzyCompare(m_AngleOrder, newAngleOrder))
        return;
    m_AngleOrder = newAngleOrder;
    emit AngleOrderChanged();
}

void Rudder127245::on_lineEditRudderPosition_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditRudderPosition, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "RudderPositionChanged";
    onTimerTimeout();
}

