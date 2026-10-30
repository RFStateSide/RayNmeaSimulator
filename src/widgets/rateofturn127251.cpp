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

#include "rateofturn127251.h"
#include "ui_rateofturn127251.h"

#include "n2khelper.h"
#include "N2kMsg.h"
#include <cmath>

RateOfTurn127251::RateOfTurn127251(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::RateOfTurn127251)
    , m_RateOfTurn(N2kDoubleNA)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127251);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127251)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();
    setInitialGuiValues();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

RateOfTurn127251::~RateOfTurn127251()
{
    delete ui;
}

void RateOfTurn127251::initializeUnitsLimits()
{
    // "rad/s", "deg/s"
    switch (UserPrefs().rateOfTurnUnitIndex) {
    case 0:
        validatorRateOfTurn->setRange(-3.14159, 3.14159, 5);
        ui->labelUnitsRateOfTurn->setText("rad/s");
        break;
    case 1:
        validatorRateOfTurn->setRange(-180.0, 180.0, 2);
        ui->labelUnitsRateOfTurn->setText("deg/s");
        break;
    }

    ui->lineEditRateOfTurn->setValidator(validatorRateOfTurn);
}

void RateOfTurn127251::initializeClickableLabels()
{
    connect(this, &RateOfTurn127251::UserPrefsChanged, this, &RateOfTurn127251::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsRateOfTurn->setMenuItems(unitsListRateOfTurn);
    connect(ui->labelUnitsRateOfTurn, &ClickableLabel::actionTriggered, this, &RateOfTurn127251::ChangeRateOfTurnUnits_triggered);
}

void RateOfTurn127251::onTimerTimeout()
{
    if (m_SignalToEmit == "RateOfTurn") {
        double convertedValue = N2kHelper::convertUserRateOfTurnToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        m_RateOfTurn = convertedValue;
        //ui->lineEditRateOfTurn->setText(m_ValueToEmit);
        emit RateOfTurnChanged();
    }
}

void RateOfTurn127251::on_lineEditRateOfTurn_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditRateOfTurn, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "RateOfTurn";
    onTimerTimeout();
}

unsigned char RateOfTurn127251::SID() const
{
    return m_SID;
}

void RateOfTurn127251::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double RateOfTurn127251::RateOfTurn() const
{
    return m_RateOfTurn;
}

void RateOfTurn127251::setRateOfTurn(double newRateOfTurn)
{
    if (qFuzzyCompare(m_RateOfTurn, newRateOfTurn))
        return;
    m_RateOfTurn = newRateOfTurn;
    setInitialGuiValues();

    emit RateOfTurnChanged();
}

void RateOfTurn127251::setInitialGuiValues()
{
    if (!std::isfinite(m_RateOfTurn) || N2kIsNA(m_RateOfTurn)) {
        ui->lineEditRateOfTurn->setText(tr("Unavailable"));
        return;
    }
    int percision = 2;
    switch (UserPrefs().rateOfTurnUnitIndex) {
    case 0:
        percision = 5;
        break;
    case 1:
        percision = 2;
        break;
    }
    ui->lineEditRateOfTurn->setText(QString::number(N2kHelper::convertN2kRateOfTurnToUserUnits(m_RateOfTurn, UserPrefs()), 'f', percision));
}

void RateOfTurn127251::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}
