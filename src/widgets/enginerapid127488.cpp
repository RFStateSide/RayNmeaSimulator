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

#include "enginerapid127488.h"
#include "QFocusEvent"
#include "n2khelper.h"
#include "ui_enginerapid127488.h"

// TODO: Refactor inherite PgnWidgetBase class to avoid code duplication
EngineRapid127488::EngineRapid127488(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::EngineRapid127488)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127488);
    Q_UNUSED(label);
    setColumnSpan(3);
    setRowSpan(4);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

EngineRapid127488::~EngineRapid127488()
{
    timer->stop();
    delete ui;
}

int EngineRapid127488::EngineInstance() const
{
    return m_EngineInstance;
}

void EngineRapid127488::setEngineInstance(int newEngineInstance)
{
    if (m_EngineInstance == newEngineInstance)
        return;
    m_EngineInstance = newEngineInstance;
    ui->labelTitle->setText("Engine #" + QString::number(newEngineInstance + 1) + ", Rapid Update (127488)");
    emit EngineInstanceChanged();
}

double EngineRapid127488::EngineSpeed() const
{
    return m_EngineSpeed;
}

void EngineRapid127488::setEngineSpeed(const double &newEngineSpeed)
{
    if (m_EngineSpeed == newEngineSpeed)
        return;
    m_EngineSpeed = newEngineSpeed;
    ui->lineEditEngineSpeed->setText(QString::number(newEngineSpeed, 'f', 2));
    emit EngineSpeedChanged();
}

double EngineRapid127488::EngineBoostPressure() const
{
    return m_EngineBoostPressure;
}

void EngineRapid127488::setEngineBoostPressure(double newEngineBoostPressure)
{
    if (qFuzzyCompare(m_EngineBoostPressure, newEngineBoostPressure))
        return;
    m_EngineBoostPressure = newEngineBoostPressure;
    double converted = N2kHelper::convertN2kPressureToUserUnits(newEngineBoostPressure, UserPrefs());
    ui->lineEditBoostPressure->setText(QString::number(converted, 'f', 2));
    emit EngineBoostPressureChanged();
}

int EngineRapid127488::EngineTiltTrim() const
{
    return m_EngineTiltTrim;
}

void EngineRapid127488::setEngineTiltTrim(int newEngineTiltTrim)
{
    if (m_EngineTiltTrim == newEngineTiltTrim)
        return;
    m_EngineTiltTrim = newEngineTiltTrim;
    ui->lineEditEngineTiltTrim->setText(QString::number(newEngineTiltTrim));
    emit EngineTiltTrimChanged();
}

void EngineRapid127488::initializeUnitsLimits()
{
    // Pa, kPa, Bar, PSI: equivalent existing boost-pressure limits.
    switch (UserPrefs().pressureUnitIndex) {
    case 0: validatorPressure->setRange(0.0, 1034000.0, 2); break;
    case 1: validatorPressure->setRange(0.0, 1034.0, 2); break;
    case 2: validatorPressure->setRange(0.0, 10.34, 2); break;
    case 3: validatorPressure->setRange(0.0, 150.0, 2); break;
    }

    ui->labelUnitsEngineBoostPressure->setText(unitsListPressure.value(UserPrefs().pressureUnitIndex));

    // TODO: Support user prefs for limits
    validatorRpm->setRange(0.0, 10000.0, 2);
    validatorTiltTrim->setRange(0, 100);
    ui->lineEditEngineSpeed->setValidator(validatorRpm);
    ui->lineEditBoostPressure->setValidator(validatorPressure);
    ui->lineEditEngineTiltTrim->setValidator(validatorTiltTrim);

    // Set the default values
    //setEngineSpeed(0.0);
    //setEngineBoostPressure(0.0);
    //setEngineTiltTrim(0);
}

void EngineRapid127488::initializeClickableLabels()
{
    connect(this, &EngineRapid127488::UserPrefsChanged, this, &EngineRapid127488::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsEngineBoostPressure->setMenuItems(unitsListPressure);
    connect(ui->labelUnitsEngineBoostPressure, &ClickableLabel::actionTriggered, this, &EngineRapid127488::ChangePressureUnits_triggered);
}

// Slow down the signal emission to prevent too many signals being emitted
void EngineRapid127488::onTimerTimeout()
{
    if (m_SignalToEmit == "EngineSpeed") {
        double value = m_ValueToEmit.toDouble();
        setEngineSpeed(value);
    } else if (m_SignalToEmit == "EngineBoostPressure") {
        double value = m_ValueToEmit.toDouble();
        double converted = N2kHelper::convertUserPressureToN2kUnits(value, UserPrefs());
        setEngineBoostPressure(converted);
    } else if (m_SignalToEmit == "EngineTiltTrim") {
        int value = m_ValueToEmit.toInt();
        setEngineTiltTrim(value);
    }

    m_SignalToEmit.clear();
}

void EngineRapid127488::setInitialGuiValues()
{
    ui->lineEditEngineSpeed->setText(QString::number(m_EngineSpeed, 'f', 2));
    ui->lineEditBoostPressure->setText(QString::number(N2kHelper::convertN2kPressureToUserUnits(m_EngineBoostPressure, UserPrefs()), 'f', 2));
    ui->lineEditEngineTiltTrim->setText(QString::number(m_EngineTiltTrim, 'f', 1));
}

void EngineRapid127488::on_lineEditEngineSpeed_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditEngineSpeed, arg1)) return;
    //double value = arg1.toDouble();
    //m_EngineSpeed = value;
    // Set the timer to trigger the WaterTemperatureChanged signal
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineSpeed";
    onTimerTimeout();
}

void EngineRapid127488::on_lineEditBoostPressure_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditBoostPressure, arg1)) return;
    //double value = arg1.toDouble();
    //double converted = N2kHelper::convertUserPressureToN2kUnits(value, UserPrefs());
    //m_EngineBoostPressure = converted;
    // Set the timer to trigger the AtmosphericPressureChanged signal
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineBoostPressure";
    onTimerTimeout();
}

void EngineRapid127488::on_lineEditEngineTiltTrim_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditEngineTiltTrim, arg1)) return;
    //int value = arg1.toInt();
    //m_EngineTiltTrim = value;
    // Set the timer to trigger the AtmosphericPressureChanged signal
    m_ValueToEmit = arg1;
    m_SignalToEmit = "EngineTiltTrim";
    onTimerTimeout();
}

void EngineRapid127488::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

