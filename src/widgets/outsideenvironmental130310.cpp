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

#include "outsideenvironmental130310.h"
#include "QFocusEvent"
#include "n2khelper.h"
#include "ui_outsideenvironmental130130.h"

// TODO: Refactor inherite PgnWidgetBase class to avoid code duplication
#include "N2kMsg.h"

OutsideEnvironmental130310::OutsideEnvironmental130310(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::OutsideEnvironmental130130)
{
    ui->setupUi(this);
    setId(id);
    setPgn(130310);
    setColumnSpan(3);
    setRowSpan(4);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (130310)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    ui->lineEditWaterTemperature->setValidator(validatorTemperature);
    ui->lineEditAmbientAirTemperature->setValidator(validatorTemperature);
    ui->lineEditAtmosphericPressure->setValidator(validatorPressure);
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);


}

OutsideEnvironmental130310::~OutsideEnvironmental130310()
{
    delete ui;
}

void OutsideEnvironmental130310::initializeUnitsLimits()
{
    switch (UserPrefs().tempUnitIndex) {
    case 0:
        ui->labelUnitsWaterTemperature->setText("°K");
        ui->labelUnitsAmbientAirTemperature->setText("°K");
        validatorTemperature->setRange(222.039, 394.261, 2);
        break;
    case 1:
        ui->labelUnitsWaterTemperature->setText("°C");
        ui->labelUnitsAmbientAirTemperature->setText("°C");
        validatorTemperature->setRange(-51.111, 121.111, 2);
        break;
    case 2:
        ui->labelUnitsWaterTemperature->setText("°F");
        ui->labelUnitsAmbientAirTemperature->setText("°F");
        validatorTemperature->setRange(-60.0, 250.0, 2);
        break;
    }

    switch (UserPrefs().pressureUnitIndex) {
    case 0:
        ui->labelUnitsAtmosphericPressure->setText("Pa");
        validatorPressure->setRange(0.0, 1034000, 2);
        break;
    case 1:
        ui->labelUnitsAtmosphericPressure->setText("kPa");
        validatorPressure->setRange(0.0, 1034, 2);
        break;
    case 2:
        ui->labelUnitsAtmosphericPressure->setText("Bar");
        validatorPressure->setRange(0.0, 10.34, 2);
        break;
    case 3:
        ui->labelUnitsAtmosphericPressure->setText("PSI");
        validatorPressure->setRange(0.0, 150, 2);
        break;
    }
}

void OutsideEnvironmental130310::initializeClickableLabels()
{
    connect(this, &OutsideEnvironmental130310::UserPrefsChanged, this, &OutsideEnvironmental130310::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsWaterTemperature->setMenuItems(unitsListTemp);
    connect(ui->labelUnitsWaterTemperature, &ClickableLabel::actionTriggered, this, &OutsideEnvironmental130310::ChangeTempUnits_triggered);
    ui->labelUnitsAmbientAirTemperature->setMenuItems(unitsListTemp);
    connect(ui->labelUnitsAmbientAirTemperature, &ClickableLabel::actionTriggered, this, &OutsideEnvironmental130310::ChangeTempUnits_triggered);
    ui->labelUnitsAtmosphericPressure->setMenuItems(unitsListPressure);
    connect(ui->labelUnitsAtmosphericPressure, &ClickableLabel::actionTriggered, this, &OutsideEnvironmental130310::ChangePressureUnits_triggered);
}

double OutsideEnvironmental130310::WaterTemperature() const
{
    return m_WaterTemperature;
}

void OutsideEnvironmental130310::setWaterTemperature(double newSetWaterTemperature)
{
    if (qFuzzyCompare(m_WaterTemperature, newSetWaterTemperature))
        return;
    m_WaterTemperature = newSetWaterTemperature;
    double converted = N2kHelper::convertN2kTemperatureToUserUnits(newSetWaterTemperature, UserPrefs());
    ui->lineEditWaterTemperature->setText(newSetWaterTemperature == N2kDoubleNA ? QString("Unavailable") : QString::number(converted, 'f', 2));
    emit WaterTemperatureChanged();
}

double OutsideEnvironmental130310::OutsideAmbientAirTemperature() const
{
    return m_OutsideAmbientAirTemperature;
}

void OutsideEnvironmental130310::setOutsideAmbientAirTemperature(double newSetOutsideAmbientAirTemperature)
{
    if (qFuzzyCompare(m_OutsideAmbientAirTemperature, newSetOutsideAmbientAirTemperature))
        return;
    m_OutsideAmbientAirTemperature = newSetOutsideAmbientAirTemperature;
    double converted = N2kHelper::convertN2kTemperatureToUserUnits(newSetOutsideAmbientAirTemperature, UserPrefs());
    ui->lineEditAmbientAirTemperature->setText(newSetOutsideAmbientAirTemperature == N2kDoubleNA ? QString("Unavailable") : QString::number(converted, 'f', 2));
    emit OutsideAmbientAirTemperatureChanged();
}
double OutsideEnvironmental130310::AtmosphericPressure() const
{
    return m_AtmosphericPressure;
}

void OutsideEnvironmental130310::setAtmosphericPressure(double newSetAtmosphericPressure)
{
    if (qFuzzyCompare(m_AtmosphericPressure, newSetAtmosphericPressure))
        return;
    m_AtmosphericPressure = newSetAtmosphericPressure;
    double converted = N2kHelper::convertN2kPressureToUserUnits(newSetAtmosphericPressure, UserPrefs());
    ui->lineEditAtmosphericPressure->setText(newSetAtmosphericPressure == N2kDoubleNA ? QString("Unavailable") : QString::number(converted, 'f', 2));
    emit AtmosphericPressureChanged();
}

void OutsideEnvironmental130310::setInitialGuiValues()
{
    ui->lineEditWaterTemperature->setText(m_WaterTemperature == N2kDoubleNA ? QString("Unavailable") : QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_WaterTemperature, UserPrefs()), 'f', 2));
    ui->lineEditAmbientAirTemperature->setText(m_OutsideAmbientAirTemperature == N2kDoubleNA ? QString("Unavailable") :
        QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_OutsideAmbientAirTemperature, UserPrefs()), 'f', 2));
    ui->lineEditAtmosphericPressure->setText(m_AtmosphericPressure == N2kDoubleNA ? QString("Unavailable") : QString::number(N2kHelper::convertN2kPressureToUserUnits(m_AtmosphericPressure, UserPrefs()), 'f', 2));
}

void OutsideEnvironmental130310::on_lineEditWaterTemperature_textEdited(const QString &arg1)
{
    if (acceptsEdit(ui->lineEditWaterTemperature, arg1))
        setWaterTemperature(N2kHelper::convertUserTemperatureToN2kUnits(arg1.toDouble(), UserPrefs()));
}

void OutsideEnvironmental130310::on_lineEditAmbientAirTemperature_textEdited(const QString &arg1)
{
    if (acceptsEdit(ui->lineEditAmbientAirTemperature, arg1))
        setOutsideAmbientAirTemperature(N2kHelper::convertUserTemperatureToN2kUnits(arg1.toDouble(), UserPrefs()));
}

void OutsideEnvironmental130310::on_lineEditAtmosphericPressure_textEdited(const QString &arg1)
{
    if (acceptsEdit(ui->lineEditAtmosphericPressure, arg1))
        setAtmosphericPressure(N2kHelper::convertUserPressureToN2kUnits(arg1.toDouble(), UserPrefs()));
}

uchar OutsideEnvironmental130310::SID() const
{
    return m_SID;
}

void OutsideEnvironmental130310::setSID(uchar newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

void OutsideEnvironmental130310::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

