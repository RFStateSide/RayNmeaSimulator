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

#include "vesselheading127250.h"
#include "QFocusEvent"
#include "n2khelper.h"
#include "ui_vesselheading127250.h"

// TODO: Refactor inherite PgnWidgetBase class to avoid code duplication
VesselHeading127250::VesselHeading127250(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::VesselHeading127250)
{
    ui->setupUi(this);

    setId(id);
    setPgn(127250);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127250)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

VesselHeading127250::~VesselHeading127250()
{
    timer->stop();
    delete ui;
}

void VesselHeading127250::initializeUnitsLimits()
{
    headingValidator->setRange(0, 360, 1);
    ui->lineEditHeading->setValidator(headingValidator);
}

void VesselHeading127250::onTimerTimeout()
{
    if (m_SignalToEmit == "Heading") {
        double converted = m_ValueToEmit.toDouble();
        setHeading(Convert::DegreesToRadians(converted));
    }

    m_ValueToEmit.clear();
}

uchar VesselHeading127250::SID() const
{
    return m_SID;
}

void VesselHeading127250::setSID(uchar newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double VesselHeading127250::Heading() const
{
    return m_Heading;
}

void VesselHeading127250::setHeading(double newHeading)
{
    if (qFuzzyCompare(m_Heading, newHeading))
        return;
    m_Heading = newHeading;
    ui->lineEditHeading->setText(QString::number(Convert::RadiansToDegrees(newHeading), 'f', 1));
    emit HeadingChanged();
}

double VesselHeading127250::Deviation() const
{
    return m_Deviation;
}

void VesselHeading127250::setDeviation(double newDeviation)
{
    if (qFuzzyCompare(m_Deviation, newDeviation))
        return;
    m_Deviation = newDeviation;
    emit DeviationChanged();
}

double VesselHeading127250::Variation() const
{
    return m_Variation;
}

void VesselHeading127250::setVariation(double newVariation)
{
    if (qFuzzyCompare(m_Variation, newVariation))
        return;
    m_Variation = newVariation;
    emit VariationChanged();
}

tN2kHeadingReference VesselHeading127250::HeadingReference() const
{
    return m_HeadingReference;
}

void VesselHeading127250::setHeadingReference(const tN2kHeadingReference &newHeadingReference)
{
    if (m_HeadingReference == newHeadingReference)
        return;
    m_HeadingReference = newHeadingReference;
    switch (static_cast<int>(m_HeadingReference)) {
    case 0:
        ui->labelUnitHeading->setText("˚T");
        break;
    case 1:
        ui->labelUnitHeading->setText("˚M");
        break;
    case 2:
    case 3:
        ui->labelUnitHeading->setText("˚");
        break;
    }

    emit HeadingReferenceChanged();
}

void VesselHeading127250::setInitialGuiValues()
{
    ui->lineEditHeading->setText(QString::number(Convert::RadiansToDegrees(m_Heading), 'f', 1));
}

void VesselHeading127250::on_lineEditHeading_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditHeading, arg1)) return;
    //double heading = arg1.toDouble();
    //double converted = Convert::DegreesToRadians(heading);
    //setHeading(converted);
    m_ValueToEmit = arg1;
    m_SignalToEmit = "Heading";
    onTimerTimeout();
}

void VesselHeading127250::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}
