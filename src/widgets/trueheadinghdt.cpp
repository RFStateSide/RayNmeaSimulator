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

#include "trueheadinghdt.h"
#include "convert.h"
#include "ui_trueheadinghdt.h"

TrueHeadingHDT::TrueHeadingHDT(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::TrueHeadingHDT)
{
    ui->setupUi(this);

    setId(id);
    setPgn(SNT_HDT);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (HDT)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

TrueHeadingHDT::~TrueHeadingHDT()
{
    delete ui;
}

void TrueHeadingHDT::setInitialGuiValues()
{
    ui->lineEditHeading->setText(QString::number(Convert::RadiansToDegrees(m_Heading), 'f', 1));
}

void TrueHeadingHDT::initializeUnitsLimits()
{
    headingValidator->setRange(0, 360, 1);
    ui->lineEditHeading->setValidator(headingValidator);
}

void TrueHeadingHDT::initializeClickableLabels()
{
    connect(this, &TrueHeadingHDT::UserPrefsChanged, this, &TrueHeadingHDT::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitHeading->setMenuItems(unitsListHeading);
    connect(ui->labelUnitHeading, &ClickableLabel::actionTriggered, this, &TrueHeadingHDT::ChangeHeadingUnits_triggered);
}

void TrueHeadingHDT::onTimerTimeout()
{
    if (m_SignalToEmit == "Heading") {
        double converted = m_ValueToEmit.toDouble();
        setHeading(Convert::DegreesToRadians(converted));
    }

    m_ValueToEmit.clear();
}

double TrueHeadingHDT::Heading() const
{
    return m_Heading;
}

void TrueHeadingHDT::setHeading(double newHeading)
{
    if (qFuzzyCompare(m_Heading, newHeading))
        return;
    m_Heading = newHeading;
    ui->lineEditHeading->setText(QString::number(Convert::RadiansToDegrees(newHeading), 'f', 1));
    emit HeadingChanged();
}

void TrueHeadingHDT::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void TrueHeadingHDT::on_lineEditHeading_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditHeading, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "Heading";
    onTimerTimeout();
}

