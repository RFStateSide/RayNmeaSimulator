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

#include "cogsog129026.h"
#include "ui_cogsog129026.h"

#include "n2khelper.h"

// TODO: Refactor inherite PgnWidgetBase class to avoid code duplication
CogSog129026::CogSog129026(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::CogSog129026)
{
    ui->setupUi(this);
    setId(id);
    setPgn(129026);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (129026)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

CogSog129026::~CogSog129026()
{
    delete ui;
}

void CogSog129026::onTimerTimeout()
{
    if (m_SignalToEmit == "COG") {
        double convertedValue = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        m_COG = convertedValue;
        emit COGChanged();
    } else if (m_SignalToEmit == "SOG") {
        double convertedValue = N2kHelper::convertUserSpeedToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        m_SOG = convertedValue;
        emit SOGChanged();
    }
    m_SignalToEmit.clear();
}

void CogSog129026::setInitialGuiValues()
{
    ui->lineEditCog->setText(QString::number(Convert::RadiansToDegrees(m_COG), 'f', 1));
    ui->lineEditSog->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_SOG, UserPrefs()), 'f', 1));
}

unsigned char CogSog129026::SID() const
{
    return m_SID;
}

void CogSog129026::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double CogSog129026::COG() const
{
    return m_COG;
}

void CogSog129026::setCOG(double newCOG)
{
    if (qFuzzyCompare(m_COG, newCOG))
        return;
    m_COG = newCOG;
    double converted = Convert::RadiansToDegrees(newCOG);
    ui->lineEditCog->setText(QString::number(converted, 'f', 1));
    emit COGChanged();
}

double CogSog129026::SOG() const
{
    return m_SOG;
}

void CogSog129026::setSOG(double newSOG)
{
    if (qFuzzyCompare(m_SOG, newSOG))
        return;
    m_SOG = newSOG;
    double converted = N2kHelper::convertN2kSpeedToUserUnits(newSOG, UserPrefs());
    ui->lineEditSog->setText(QString::number(converted, 'f', 1));
    emit SOGChanged();
}

void CogSog129026::on_lineEditCog_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditCog, arg1)) return;
    //double cog = arg1.toDouble();
    //double converted = Convert::DegreesToRadians(cog);
    //m_COG = converted;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "COG";
    onTimerTimeout();
}

void CogSog129026::on_lineEditSog_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditSog, arg1)) return;
    //double sog = arg1.toDouble();
    //double converted = N2kHelper::convertUserSpeedToN2kUnits(sog, UserPrefs());
    //m_SOG = converted;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "SOG";
    onTimerTimeout();
}

void CogSog129026::initializeUnitsLimits()
{
    // "Knots", "km/h", "MPH range"
    switch (UserPrefs().speedUnitIndex) {
    case 0:
        validatorSog->setRange(0, 90.0, 1);
        ui->labelUnitsSog->setText("m/s");
        break;
    case 3:
        validatorSog->setRange(0, 173.7, 1);
        ui->labelUnitsSog->setText("knots");
        break;
    case 1:
        validatorSog->setRange(0, 321.7, 1);
        ui->labelUnitsSog->setText("km/h");
        break;
    case 2:
        validatorSog->setRange(0, 199.9, 1);
        ui->labelUnitsSog->setText("MPH");
        break;
    }

    ui->lineEditSog->setValidator(validatorSog);
    validatorCog->setRange(0, 360, 1);
    ui->lineEditCog->setValidator(validatorCog);
}

void CogSog129026::initializeClickableLabels()
{
    connect(this, &CogSog129026::UserPrefsChanged, this, &CogSog129026::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsCog->setMenuItems(unitsListHeading);
    connect(ui->labelUnitsCog, &ClickableLabel::actionTriggered, this, &CogSog129026::ChangeHeadingUnits_triggered);
    ui->labelUnitsSog->setMenuItems(unitsListSpeed);
    connect(ui->labelUnitsSog, &ClickableLabel::actionTriggered, this, &CogSog129026::ChangeSpeedUnits_triggered);
}

tN2kHeadingReference CogSog129026::HeadingReference() const
{
    return m_HeadingReference;
}

void CogSog129026::setHeadingReference(tN2kHeadingReference newHeadingReference)
{
    if (m_HeadingReference == newHeadingReference)
        return;
    m_HeadingReference = newHeadingReference;
    switch (static_cast<int>(m_HeadingReference)) {
    case 0:
        ui->labelUnitsCog->setText("˚T");
        break;
    case 1:
        ui->labelUnitsCog->setText("˚M");
        break;
    case 2:
    case 3:
        ui->labelUnitsCog->setText("˚");
        break;
    }

    emit HeadingReferenceChanged();
}

void CogSog129026::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

