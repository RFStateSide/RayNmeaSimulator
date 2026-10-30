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

#include "fluidlevel127505.h"
#include "ui_fluidlevel127505.h"

#include <n2khelper.h>

FluidLevel127505::FluidLevel127505(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::FluidLevel127505)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127505);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127505)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    // Signed 0.004 percent / unsigned 0.1 L fields in pinned PGN127505.
    ui->lineEditLevel->setValidator(new QDoubleValidator(-131.072, 131.056, 1, this));
    ui->lineEditCapacity->setValidator(new QDoubleValidator(this));
    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

FluidLevel127505::~FluidLevel127505()
{
    delete ui;
}

void FluidLevel127505::initializeUnitsLimits()
{
    auto *capacity = const_cast<QDoubleValidator*>(qobject_cast<const QDoubleValidator*>(ui->lineEditCapacity->validator()));
    capacity->setRange(0.0, N2kHelper::convertN2kVolumeToUserUnits(429496729.2, UserPrefs()), 1);
    switch (UserPrefs().volumeUnitIndex) {
    case 0:
        ui->labelUnitsCapacity->setText("L");
        break;
    case 1:
        ui->labelUnitsCapacity->setText("gal");
    }
}

void FluidLevel127505::initializeClickableLabels()
{
    connect(this, &FluidLevel127505::UserPrefsChanged, this, &FluidLevel127505::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsCapacity->setMenuItems(unitsListVolume);
    connect(ui->labelUnitsCapacity, &ClickableLabel::actionTriggered, this, &FluidLevel127505::ChangeVolumeUnits_triggered);
}

void FluidLevel127505::onTimerTimeout()
{
    if (m_SignalToEmit == "LevelChanged") {
        m_Level = m_ValueToEmit.toDouble();
        emit LevelChanged();
    } else if (m_SignalToEmit == "CapacityChanged") {
        m_Capacity = N2kHelper::convertUserVolumeToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit CapacityChanged();
    }
}

unsigned char FluidLevel127505::Instance() const
{
    return m_Instance;
}

void FluidLevel127505::setInstance(unsigned char newInstance)
{
    if (m_Instance == newInstance)
        return;
    m_Instance = newInstance;
    emit InstanceChanged();
}

tN2kFluidType FluidLevel127505::FluidType() const
{
    return m_FluidType;
}

void FluidLevel127505::setFluidType(tN2kFluidType newFluidType)
{
    if (m_FluidType == newFluidType)
        return;
    m_FluidType = newFluidType;

    if (m_FluidType == N2kft_Fuel)
        ui->labelLevel->setText("Fuel Level");
    else if (m_FluidType == N2kft_Water)
        ui->labelLevel->setText("Water Level");
    else if (m_FluidType == N2kft_GrayWater)
        ui->labelLevel->setText("Gray Water Level");
    else if (m_FluidType == N2kft_LiveWell)
        ui->labelLevel->setText("Live Well Level");
    else if (m_FluidType == N2kft_Oil)
        ui->labelLevel->setText("Oil Level");
    else if (m_FluidType == N2kft_BlackWater)
        ui->labelLevel->setText("Black Water Level");
    else if (m_FluidType == N2kft_FuelGasoline)
        ui->labelLevel->setText("Gasoline Fuel Level");
    else if (m_FluidType == N2kft_Error)
        ui->labelLevel->setText("Error");
    else if (m_FluidType == N2kft_Unavailable)
        ui->labelLevel->setText("Unavailable");
    else
        ui->labelLevel->setText("Unknown");
    emit FluidTypeChanged();
}

double FluidLevel127505::Level() const
{
    return m_Level;
}

void FluidLevel127505::setLevel(double newLevel)
{
    if (qFuzzyCompare(m_Level, newLevel))
        return;
    m_Level = newLevel;
    ui->lineEditLevel->setText(QString::number(newLevel, 'f', 1));
    emit LevelChanged();
}

double FluidLevel127505::Capacity() const
{
    return m_Capacity;
}

void FluidLevel127505::setCapacity(double newCapacity)
{
    if (qFuzzyCompare(m_Capacity, newCapacity))
        return;
    m_Capacity = newCapacity;
    ui->lineEditCapacity->setText(QString::number(N2kHelper::convertN2kVolumeToUserUnits(newCapacity, UserPrefs()), 'f', 1));
    emit CapacityChanged();
}

void FluidLevel127505::setInitialGuiValues()
{
    ui->lineEditLevel->setText(QString::number(m_Level, 'f', 1));
    ui->lineEditCapacity->setText(QString::number(N2kHelper::convertN2kVolumeToUserUnits(m_Capacity, UserPrefs()), 'f', 1));
}

void FluidLevel127505::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void FluidLevel127505::on_lineEditLevel_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLevel, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LevelChanged";
    onTimerTimeout();
}

void FluidLevel127505::on_lineEditCapacity_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditCapacity, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "CapacityChanged";
    onTimerTimeout();
}

