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

#include "heave127252.h"
#include "ui_heave127252.h"

#include "n2khelper.h"

Heave127252::Heave127252(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::Heave127252)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127252);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127252)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

Heave127252::~Heave127252()
{
    delete ui;
}

void Heave127252::initializeUnitsLimits()
{
    ui->lineEditHeave->setValidator(validatorHeave);
    switch (UserPrefs().distanceUnitIndex) {
    case 0: // meter
        validatorHeave->setRange(-100.0, 100.0, 2);
        ui->labelUnitsHeave->setText("m");
        break;
    case 1: // nautical ft
        validatorHeave->setRange(-337.96, 337.96, 2);
        ui->labelUnitsHeave->setText("naut ft");
        break;
    case 2: // ft
        validatorHeave->setRange(-328.08, 328.08, 2);
        ui->labelUnitsHeave->setText("ft");
    }
}

void Heave127252::initializeClickableLabels()
{
    connect(this, &Heave127252::UserPrefsChanged, this, &Heave127252::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsHeave->setMenuItems(unitsListDistance);
    connect(ui->labelUnitsHeave, &ClickableLabel::actionTriggered, this, &Heave127252::ChangeDistanceUnits_triggered);
}

void Heave127252::onTimerTimeout()
{
    if (m_SignalToEmit == "Heave") {
        double converted = N2kHelper::convertUserDistanceNearToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        m_Heave = converted;
        emit HeaveChanged();
    }
}

unsigned char Heave127252::SID() const
{
    return m_SID;
}

void Heave127252::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double Heave127252::Heave() const
{
    return m_Heave;
}

void Heave127252::setHeave(double newHeave)
{
    if (qFuzzyCompare(m_Heave, newHeave))
        return;
    m_Heave = newHeave;
    ui->lineEditHeave->setText(QString::number(N2kHelper::convertN2kDistanceNearToUserUnits(newHeave, UserPrefs()), 'f', 2));
    emit HeaveChanged();
}

double Heave127252::Delay() const
{
    return m_Delay;
}

void Heave127252::setDelay(double newDelay)
{
    if (qFuzzyCompare(m_Delay, newDelay))
        return;
    m_Delay = newDelay;
    emit DelayChanged();
}

tN2kDelaySource Heave127252::DelaySource() const
{
    return m_DelaySource;
}

void Heave127252::setDelaySource(tN2kDelaySource newDelaySource)
{
    if (m_DelaySource == static_cast<tN2kDelaySource>(newDelaySource))
        return;
    m_DelaySource = newDelaySource;
    emit DelaySourceChanged();
}

void Heave127252::setInitialGuiValues()
{
    ui->lineEditHeave->setText(QString::number(N2kHelper::convertN2kDistanceNearToUserUnits(m_Heave, UserPrefs()), 'f', 2));
}

void Heave127252::on_lineEditHeave_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditHeave, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "Heave";
    onTimerTimeout();
}

void Heave127252::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

