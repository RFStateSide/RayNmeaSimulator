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

#include <QValidator>
#include "temperatureextended130316.h"
#include "ui_temperatureextended130316.h"

#include "n2khelper.h"

#include "N2kMsg.h"

TemperatureExtended130316::TemperatureExtended130316(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::TemperatureExtended130316)
{
    ui->setupUi(this);
    setId(id);
    setPgn(130316);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (130316)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

TemperatureExtended130316::~TemperatureExtended130316()
{
    delete ui;
}

void TemperatureExtended130316::initializeUnitsLimits()
{
    // kelvin, celsius, fahrenheit (-60 to 250)
    switch (UserPrefs().tempUnitIndex) {
    case 0: // kelvin
        ui->labelUnits->setText("K");
        ActualTemperatureValidator->setRange(213.15, 523.15, 2);
        break;
    case 1: // celsius
        ui->labelUnits->setText("°C");
        ActualTemperatureValidator->setRange(-60, 250, 1);
        break;
    case 2: // fahrenheit
        ui->labelUnits->setText("°F");
        ActualTemperatureValidator->setRange(-76, 482, 1);
        break;
    }

    ui->lineEdit->setValidator(ActualTemperatureValidator);
}

void TemperatureExtended130316::initializeClickableLabels()
{
    connect(this, &TemperatureExtended130316::UserPrefsChanged, this, &TemperatureExtended130316::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnits->setMenuItems(unitsListTemp);
    connect(ui->labelUnits, &ClickableLabel::actionTriggered, this, &TemperatureExtended130316::ChangeTempUnits_triggered);
}

void TemperatureExtended130316::onTimerTimeout()
{
    if (m_SignalToEmit == "ActualTemperatureChanged") {
        m_ActualTemperature = N2kHelper::convertUserTemperatureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit ActualTemperatureChanged();
    }
}

unsigned char TemperatureExtended130316::SID() const
{
    return m_SID;
}

void TemperatureExtended130316::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

unsigned char TemperatureExtended130316::TempInstance() const
{
    return m_TempInstance;
}

void TemperatureExtended130316::setTempInstance(unsigned char newTempInstance)
{
    if (m_TempInstance == newTempInstance)
        return;
    m_TempInstance = newTempInstance;
    emit TempInstanceChanged();
}

tN2kTempSource TemperatureExtended130316::TempSource() const
{
    return m_TempSource;
}

void TemperatureExtended130316::setTempSource(tN2kTempSource newTempSource)
{
    if (m_TempSource == newTempSource)
        return;
    m_TempSource = newTempSource;

    if (m_TempSource == N2kts_SeaTemperature)
        ui->labelSource->setText("Sea Temp");
    else if (m_TempSource == N2kts_OutsideTemperature)
        ui->labelSource->setText("Outside Temp");
    else if (m_TempSource == N2kts_InsideTemperature)
        ui->labelSource->setText("Inside Temp");
    else if (m_TempSource == N2kts_EngineRoomTemperature)
        ui->labelSource->setText("Engine Room Temp");
    else if (m_TempSource == N2kts_MainCabinTemperature)
        ui->labelSource->setText("Main Cabin Temp");
    else if (m_TempSource == N2kts_LiveWellTemperature)
        ui->labelSource->setText("Live Well Temp");
    else if (m_TempSource == N2kts_BaitWellTemperature)
        ui->labelSource->setText("Bait Well Temp");
    else if (m_TempSource == N2kts_RefridgerationTemperature)
        ui->labelSource->setText("Refridgeration Temp");
    else if (m_TempSource == N2kts_HeatingSystemTemperature)
        ui->labelSource->setText("Heating System Temp");
    else if (m_TempSource == N2kts_DewPointTemperature)
        ui->labelSource->setText("Dew Point Temp");
    else if (m_TempSource == N2kts_ApparentWindChillTemperature)
        ui->labelSource->setText("Apparent Wind Chill Temp");
    else if (m_TempSource == N2kts_TheoreticalWindChillTemperature)
        ui->labelSource->setText("Theoretical Wind Chill Temp");
    else if (m_TempSource == N2kts_HeatIndexTemperature)
        ui->labelSource->setText("Heat Index Temp");
    else if (m_TempSource == N2kts_FreezerTemperature)
        ui->labelSource->setText("Freezer Temp");
    else if (m_TempSource == N2kts_ExhaustGasTemperature)
        ui->labelSource->setText("Exhaust Gas Temp");
    else if (m_TempSource == N2kts_ShaftSealTemperature)
        ui->labelSource->setText("Shaft Seal Temp");

    emit TempSourceChanged();
}

double TemperatureExtended130316::ActualTemperature() const
{
    return m_ActualTemperature;
}

void TemperatureExtended130316::setActualTemperature(double newActualTemperature)
{
    if (qFuzzyCompare(m_ActualTemperature, newActualTemperature))
        return;
    m_ActualTemperature = newActualTemperature;
    ui->lineEdit->setText(QString::number(N2kHelper::convertN2kTemperatureToUserUnits(newActualTemperature, UserPrefs()), 'f', 1));
    emit ActualTemperatureChanged();
}

double TemperatureExtended130316::SetTemperature() const
{
    return m_SetTemperature;
}

void TemperatureExtended130316::setSetTemperature(double newSetTemperature)
{
    if (qFuzzyCompare(m_SetTemperature, newSetTemperature))
        return;
    m_SetTemperature = newSetTemperature;
    emit SetTemperatureChanged();
}

void TemperatureExtended130316::setInitialGuiValues()
{
    ui->lineEdit->setText(m_ActualTemperature == N2kDoubleNA ? QString("Unavailable") : QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_ActualTemperature, UserPrefs()), 'f', 1));
}

void TemperatureExtended130316::on_lineEdit_textEdited(const QString &arg1)
{
    m_ValueToEmit = arg1;
    m_SignalToEmit = "ActualTemperatureChanged";
    onTimerTimeout();
}

void TemperatureExtended130316::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}
