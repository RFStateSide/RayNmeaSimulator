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

#include "cogsogvtg.h"
#include "n2khelper.h"
#include "ui_cogsogvtg.h"

CogSogVTG::CogSogVTG(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::CogSogVTG)
{
    ui->setupUi(this);
    setId(id);
    setPgn(SNT_VTG);
    setColumnSpan(3);
    setRowSpan(4);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (VTG)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

CogSogVTG::~CogSogVTG()
{
    delete ui;
}

void CogSogVTG::setInitialGuiValues()
{
    ui->lineEditTrueCog->setText(QString::number(Convert::RadiansToDegrees(m_TrueCOG), 'f', 1));
    ui->lineEditMagneticCog->setText(QString::number(Convert::RadiansToDegrees(m_MagneticCOG), 'f', 1));
    ui->lineEditSog->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_SOG, UserPrefs()), 'f', 1));
}

void CogSogVTG::initializeUnitsLimits()
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
    ui->lineEditTrueCog->setValidator(validatorCog);
    ui->lineEditMagneticCog->setValidator(validatorCog);
}

void CogSogVTG::initializeClickableLabels()
{
    connect(this, &CogSogVTG::UserPrefsChanged, this, &CogSogVTG::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsSog->setMenuItems(unitsListSpeed);
    connect(ui->labelUnitsSog, &ClickableLabel::actionTriggered, this, &CogSogVTG::ChangeSpeedUnits_triggered);
    ui->labelUnitsTrueCog->setMenuItems(unitsListHeading);
    connect(ui->labelUnitsTrueCog, &ClickableLabel::actionTriggered, this, &CogSogVTG::ChangeHeadingUnits_triggered);
    ui->labelUnitsMagneticCog->setMenuItems(unitsListHeading);
    connect(ui->labelUnitsMagneticCog, &ClickableLabel::actionTriggered, this, &CogSogVTG::ChangeHeadingUnits_triggered);
}

void CogSogVTG::onTimerTimeout()
{
    if (m_SignalToEmit == "TrueCOG") {
        m_TrueCOG = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        emit TrueCOGChanged();
    } else if (m_SignalToEmit == "MagneticCOG") {
        m_MagneticCOG = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        emit MagneticCOGChanged();
    } else if (m_SignalToEmit == "SOG") {
        m_SOG = N2kHelper::convertUserSpeedToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit SOGChanged();
    }

    m_SignalToEmit.clear();
}

double CogSogVTG::TrueCOG() const
{
    return m_TrueCOG;
}

void CogSogVTG::setTrueCOG(double newTrueCOG)
{
    if (qFuzzyCompare(m_TrueCOG, newTrueCOG))
        return;
    m_TrueCOG = newTrueCOG;
    ui->lineEditTrueCog->setText(QString::number(Convert::RadiansToDegrees(newTrueCOG), 'f', 1));
    emit TrueCOGChanged();
}

double CogSogVTG::MagneticCOG() const
{
    return m_MagneticCOG;
}

void CogSogVTG::setMagneticCOG(double newMagneticCOG)
{
    if (qFuzzyCompare(m_MagneticCOG, newMagneticCOG))
        return;
    m_MagneticCOG = newMagneticCOG;
    ui->lineEditMagneticCog->setText(QString::number(Convert::RadiansToDegrees(newMagneticCOG), 'f', 1));
    emit MagneticCOGChanged();
}

double CogSogVTG::SOG() const
{
    return m_SOG;
}

void CogSogVTG::setSOG(double newSOG)
{
    if (qFuzzyCompare(m_SOG, newSOG))
        return;
    m_SOG = newSOG;
    ui->lineEditSog->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(newSOG, UserPrefs()), 'f', 1));
    emit SOGChanged();
}

void CogSogVTG::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void CogSogVTG::on_lineEditTrueCog_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditTrueCog, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "TrueCOG";
    onTimerTimeout();
}

void CogSogVTG::on_lineEditMagneticCog_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditMagneticCog, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "MagneticCOG";
    onTimerTimeout();
}

void CogSogVTG::on_lineEditSog_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditSog, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "SOG";
    onTimerTimeout();
}

