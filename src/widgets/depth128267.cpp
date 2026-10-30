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

#include "depth128267.h"
#include "ui_depth128267.h"

#include "n2khelper.h"
#include "instrumentdisplay.h"

Depth128267::Depth128267(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::Depth128267)
{
    ui->setupUi(this);
    setId(id);
    setPgn(128267);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (128267)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());
    connect(ui->checkBoxEnabled, &QCheckBox::clicked, this, &PgnWidgetBase::setEnabled);

    initializeUnitsLimits();
    setInitialGuiValues();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

Depth128267::~Depth128267()
{
    delete ui;
}

void Depth128267::initializeUnitsLimits()
{
    ui->lineEditDepth->setValidator(m_DepthBelowTransducerValidator);
    //"Meters", "Feet", "Fathoms"
    switch (UserPrefs().depthUnitIndex) {
    case 0: // Meter
        ui->labelUnitsDepth->setText("m");
        break;
    case 1: // Feet
        ui->labelUnitsDepth->setText("ft");
        break;
    case 2: // Fathoms
        ui->labelUnitsDepth->setText("fathoms");
        break;
    }
    m_DepthBelowTransducerValidator->setRange(0, N2kHelper::convertN2kDepthToUserUnits(1000, UserPrefs()), 1);
}

void Depth128267::initializeClickableLabels()
{
    connect(this, &Depth128267::UserPrefsChanged, this, &Depth128267::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsDepth->setMenuItems(unitsListDepth);
    connect(ui->labelUnitsDepth, &ClickableLabel::actionTriggered, this, &Depth128267::ChangeDepthUnits_triggered);
}

void Depth128267::onTimerTimeout()
{
    if (m_SignalToEmit == "DepthBelowTransducer") {
        m_DepthBelowTransducer = N2kHelper::convertUserDepthToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs()); // Library API and model use meters.
        emit DepthBelowTransducerChanged();
    }

    m_SignalToEmit.clear();
}

unsigned char Depth128267::SID() const
{
    return m_SID;
}

void Depth128267::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double Depth128267::DepthBelowTransducer() const
{
    return m_DepthBelowTransducer;
}

void Depth128267::setDepthBelowTransducer(double newDepthBelowTransducer)
{
    if (qFuzzyCompare(m_DepthBelowTransducer, newDepthBelowTransducer))
        return;
    m_DepthBelowTransducer = newDepthBelowTransducer;
    ui->lineEditDepth->setText(
        InstrumentDisplay::number(N2kHelper::convertN2kDepthToUserUnits(m_DepthBelowTransducer, UserPrefs()), m_DepthBelowTransducer));
    emit DepthBelowTransducerChanged();
}

double Depth128267::Offset() const
{
    return m_Offset;
}

void Depth128267::setOffset(double newOffset)
{
    if (qFuzzyCompare(m_Offset, newOffset))
        return;
    m_Offset = newOffset;
    emit OffsetChanged();
}

double Depth128267::Range() const
{
    return m_Range;
}

void Depth128267::setRange(double newRange)
{
    if (qFuzzyCompare(m_Range, newRange))
        return;
    m_Range = newRange;
    emit RangeChanged();
}

void Depth128267::setInitialGuiValues()
{
    ui->lineEditDepth->setText(InstrumentDisplay::number(N2kHelper::convertN2kDepthToUserUnits(m_DepthBelowTransducer, UserPrefs()), m_DepthBelowTransducer));
}

void Depth128267::on_lineEditDepth_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditDepth, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "DepthBelowTransducer";
    onTimerTimeout();
}

