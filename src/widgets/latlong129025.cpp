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

#include "coordinateeditor.h"
#include "latlong129025.h"
#include "QFocusEvent"
#include "convert.h"
#include "n2khelper.h"
#include "ui_latlong129025.h"

// TODO: Refactor inherite PgnWidgetBase class to avoid code duplication
LatLong129025::LatLong129025(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::LatLong129025)
{
    ui->setupUi(this);
    setId(id);
    setPgn(129025);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (129025)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, &LatLong129025::onTimerTimeout);

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

LatLong129025::~LatLong129025()
{
    delete ui;
}

void LatLong129025::onTimerTimeout()
{
    double converted = 0.0;
    switch (UserPrefs().latLongUnitIndex) {
    case 0:
        converted = m_ValueToEmit.toDouble();
        break;
    case 1:
        converted = Convert::DMSToDecimal(m_ValueToEmit, m_SignalToEmit == "LatitudeChanged");
        break;
    case 2:
        converted = Convert::DMMToDecimal(m_ValueToEmit, m_SignalToEmit == "LatitudeChanged");
        break;
    }

    if (m_SignalToEmit == "LatitudeChanged") {
        m_Latitude = converted;
        emit LatitudeChanged();
    } else if (m_SignalToEmit == "LongitudeChanged") {
        m_Longitude = converted;
        emit LongitudeChanged();
    }

    m_ValueToEmit.clear();
}

void LatLong129025::initializeUnitsLimits()
{
    setCoordinateValidator(ui->lineEditLatitude, UserPrefs().latLongUnitIndex, true);
    setCoordinateValidator(ui->lineEditLongitude, UserPrefs().latLongUnitIndex, false);
    // "DD.DDDD", "DD°MM’SS", "DD°MM.MMM’"
    switch (UserPrefs().latLongUnitIndex) {
    case 0:
        ui->labelUnitLatitude->setText("DD.DDDDDD°");
        ui->labelUnitLongitude->setText("DD.DDDDDD°");
        break;
    case 1:
        ui->labelUnitLatitude->setText("DD°MM’SS”");
        ui->labelUnitLongitude->setText("DD°MM’SS”");
        break;
    case 2:
        ui->labelUnitLatitude->setText("DD°MM.MMM’");
        ui->labelUnitLongitude->setText("DD°MM.MMM’");
        break;
    }
}

void LatLong129025::initializeClickableLabels()
{
    connect(this, &LatLong129025::UserPrefsChanged, this, &LatLong129025::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitLatitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLatitude, &ClickableLabel::actionTriggered, this, &LatLong129025::ChangeLatLongUnits_triggered);
    ui->labelUnitLongitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLongitude, &ClickableLabel::actionTriggered, this, &LatLong129025::ChangeLatLongUnits_triggered);
}

double LatLong129025::Latitude() const
{
    return m_Latitude;
}

void LatLong129025::setLatitude(double newLatitude)
{
    if (m_Latitude == newLatitude)
        return;
    m_Latitude = newLatitude;

    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    emit LatitudeChanged();
}

double LatLong129025::Longitude() const
{
    return m_Longitude;
}

void LatLong129025::setLongitude(double newLongitude)
{
    if (m_Longitude == newLongitude)
        return;
    m_Longitude = newLongitude;

    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    emit LongitudeChanged();
}

void LatLong129025::setInitialGuiValues()
{
    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
}

void LatLong129025::on_lineEditLatitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLatitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LatitudeChanged";
    onTimerTimeout();
}

void LatLong129025::on_lineEditLongitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLongitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LongitudeChanged";
    onTimerTimeout();
}

void LatLong129025::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}
