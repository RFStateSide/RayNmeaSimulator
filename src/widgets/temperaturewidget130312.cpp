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
#include "temperaturewidget130312.h"
#include "QFocusEvent"
#include "n2khelper.h"
#include "ui_temperaturewidget130312.h"

#include "N2kMsg.h"

TemperatureWidget130312::TemperatureWidget130312(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::TemperatureWidget130312)
{
    ui->setupUi(this);

    setId(id);
    setPgn(130312);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelSource->setText(label + " (130312)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();
    setInitialGuiValues();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

TemperatureWidget130312::~TemperatureWidget130312()
{
    delete ui;
}

uchar TemperatureWidget130312::TemperatureInstance() const
{
    return m_TemperatureInstance;
}

void TemperatureWidget130312::setTemperatureInstance(uchar newTemperatureInstance)
{
    if (m_TemperatureInstance == newTemperatureInstance)
        return;
    m_TemperatureInstance = newTemperatureInstance;
    emit TemperatureInstanceChanged();
}

uchar TemperatureWidget130312::SID() const
{
    return m_SID;
}

void TemperatureWidget130312::setSID(uchar newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

tN2kTempSource TemperatureWidget130312::TemperatureSource() const
{
    return m_TemperatureSource;
}

void TemperatureWidget130312::setTemperatureSource(tN2kTempSource newTemperatureSource)
{
    if (m_TemperatureSource == newTemperatureSource)
        return;
    m_TemperatureSource = newTemperatureSource;

    if (m_TemperatureSource == N2kts_SeaTemperature)
        ui->labelSource->setText("Sea Temp");
    else if (m_TemperatureSource == N2kts_OutsideTemperature)
        ui->labelSource->setText("Outside Temp");
    else if (m_TemperatureSource == N2kts_InsideTemperature)
        ui->labelSource->setText("Inside Temp");
    else if (m_TemperatureSource == N2kts_EngineRoomTemperature)
        ui->labelSource->setText("Engine Room Temp");
    else if (m_TemperatureSource == N2kts_MainCabinTemperature)
        ui->labelSource->setText("Main Cabin Temp");
    else if (m_TemperatureSource == N2kts_LiveWellTemperature)
        ui->labelSource->setText("Live Well Temp");
    else if (m_TemperatureSource == N2kts_BaitWellTemperature)
        ui->labelSource->setText("Bait Well Temp");
    else if (m_TemperatureSource == N2kts_RefridgerationTemperature)
        ui->labelSource->setText("Refridgeration Temp");
    else if (m_TemperatureSource == N2kts_HeatingSystemTemperature)
        ui->labelSource->setText("Heating System Temp");
    else if (m_TemperatureSource == N2kts_DewPointTemperature)
        ui->labelSource->setText("Dew Point Temp");
    else if (m_TemperatureSource == N2kts_ApparentWindChillTemperature)
        ui->labelSource->setText("Apparent Wind Chill Temp");
    else if (m_TemperatureSource == N2kts_TheoreticalWindChillTemperature)
        ui->labelSource->setText("Theoretical Wind Chill Temp");
    else if (m_TemperatureSource == N2kts_HeatIndexTemperature)
        ui->labelSource->setText("Heat Index Temp");
    else if (m_TemperatureSource == N2kts_FreezerTemperature)
        ui->labelSource->setText("Freezer Temp");
    else if (m_TemperatureSource == N2kts_ExhaustGasTemperature)
        ui->labelSource->setText("Exhaust Gas Temp");
    else if (m_TemperatureSource == N2kts_ShaftSealTemperature)
        ui->labelSource->setText("Shaft Seal Temp");

    emit TemperatureSourceChanged();
}

double TemperatureWidget130312::ActualTemperature() const
{
    return m_ActualTemperature;
}

void TemperatureWidget130312::setActualTemperature(double newActualTemperature)
{
    if (qFuzzyCompare(m_ActualTemperature, newActualTemperature))
        return;
    m_ActualTemperature = newActualTemperature;
    setInitialGuiValues();
    emit ActualTemperatureChanged();
}

double TemperatureWidget130312::SetTemperature() const
{
    return m_SetTemperature;
}

void TemperatureWidget130312::setSetTemperature(double newSetTemperature)
{
    if (qFuzzyCompare(m_SetTemperature, newSetTemperature))
        return;
    m_SetTemperature = newSetTemperature;
    emit SetTemperatureChanged();
}

void TemperatureWidget130312::setInitialGuiValues()
{
    ui->lineEdit->setText(m_ActualTemperature == N2kDoubleNA ? QString("Unavailable") : QString::number(N2kHelper::convertN2kTemperatureToUserUnits(m_ActualTemperature, UserPrefs()), 'f', validatorTemperature->decimals()));
}

void TemperatureWidget130312::initializeUnitsLimits()
{
    // One physical envelope retains the historical Celsius range. Precision
    // represents both endpoints exactly; validators affect manual entry only.
    switch (UserPrefs().tempUnitIndex) {
    case 0:
        ui->labelUnits->setText("K");
        validatorTemperature->setRange(213.05, 523.25, 2);
        break;
    case 1:
        ui->labelUnits->setText("°C");
        validatorTemperature->setRange(-60.1, 250.1, 1);
        break;
    case 2:
        ui->labelUnits->setText("°F");
        validatorTemperature->setRange(-76.18, 482.18, 2);
        break;
    }

    ui->lineEdit->setValidator(validatorTemperature);
    ui->lineEdit->setToolTip(tr("Manual input: %1 to %2 %3. Received and saved values are not clamped to this range.")
        .arg(validatorTemperature->bottom(), 0, 'f', validatorTemperature->decimals())
        .arg(validatorTemperature->top(), 0, 'f', validatorTemperature->decimals()).arg(ui->labelUnits->text()));
}

void TemperatureWidget130312::initializeClickableLabels()
{
    connect(this, &TemperatureWidget130312::UserPrefsChanged, this, &TemperatureWidget130312::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnits->setMenuItems(unitsListTemp);
    connect(ui->labelUnits, &ClickableLabel::actionTriggered, this, &TemperatureWidget130312::ChangeTempUnits_triggered);
}

void TemperatureWidget130312::onTimerTimeout()
{
    if (m_SignalToEmit == "ActualTemperatureChanged") {
        m_ActualTemperature = N2kHelper::convertUserTemperatureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit ActualTemperatureChanged();
    }
}

void TemperatureWidget130312::on_lineEdit_textEdited(const QString &arg1)
{
    // QLineEdit permits Intermediate text while typing (including an out-of-
    // range number or a lone minus). It is not yet a valid measurement.
    QString input = arg1;
    int position = 0;
    if (validatorTemperature->validate(input, position) != QValidator::Acceptable) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "ActualTemperatureChanged";
    onTimerTimeout();
}

void TemperatureWidget130312::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

