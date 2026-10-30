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

#include "magneticvariation127258.h"
#include "ui_magneticvariation127258.h"

#include "QFocusEvent"
#include "convert.h"

MagneticVariation127258::MagneticVariation127258(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::MagneticVariation127258)
{
    ui->setupUi(this);
    setId(id);
    setPgn(127258);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (127258)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

MagneticVariation127258::~MagneticVariation127258()
{
    delete ui;
}

void MagneticVariation127258::initializeUnitsLimits()
{
    variationValidator->setRange(-360, 360, 3);
    ui->lineEditMagneticVariation->setValidator(variationValidator);
}

void MagneticVariation127258::initializeClickableLabels()
{
    // No clickable labels for this widget
}

void MagneticVariation127258::onTimerTimeout()
{
    if (m_SignalToEmit == "VariationChanged") {
        //ui->lineEditMagneticVariation->setText(QString::number(Convert::DegreesToRadians(m_ValueToEmit.toDouble()), 'f', 3));
        m_Variation = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        emit VariationChanged();
    }

    m_SignalToEmit.clear();
}

unsigned char MagneticVariation127258::SID() const
{
    return m_SID;
}

void MagneticVariation127258::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

tN2kMagneticVariation MagneticVariation127258::Source() const
{
    return m_Source;
}

void MagneticVariation127258::setSource(tN2kMagneticVariation newSource)
{
    if (m_Source == newSource)
        return;
    m_Source = newSource;
    emit SourceChanged();
}

uint16_t MagneticVariation127258::DaysSince1970() const
{
    return m_DaysSince1970;
}

void MagneticVariation127258::setDaysSince1970(uint16_t newDaysSince1970)
{
    if (m_DaysSince1970 == newDaysSince1970)
        return;
    m_DaysSince1970 = newDaysSince1970;
    emit DaysSince1970Changed();
}

double MagneticVariation127258::Variation() const
{
    return m_Variation;
}

void MagneticVariation127258::setVariation(double newVariation)
{
    if (qFuzzyCompare(m_Variation, newVariation))
        return;
    m_Variation = newVariation;
    ui->lineEditMagneticVariation->setText(QString::number(Convert::RadiansToDegrees(newVariation), 'f', 3));
    emit VariationChanged();
}

void MagneticVariation127258::setInitialGuiValues()
{
    ui->lineEditMagneticVariation->setText(QString::number(Convert::RadiansToDegrees(m_Variation), 'f', 3));
}

void MagneticVariation127258::on_lineEditMagneticVariation_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditMagneticVariation, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "VariationChanged";
    onTimerTimeout();
}

void MagneticVariation127258::on_pushButtonRemove_clicked(bool checked)
{
    setEnabled(checked);
}

