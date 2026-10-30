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
#include "geographicpositiongll.h"
#include "convert.h"
#include "n2khelper.h"
#include "ui_geographicpositiongll.h"

GeographicPositionGLL::GeographicPositionGLL(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::GeographicPositionGLL)
{
    ui->setupUi(this);

    setId(id);
    setPgn(SNT_GLL);
    setColumnSpan(3);
    setRowSpan(3);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (GLL)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

GeographicPositionGLL::~GeographicPositionGLL()
{
    delete ui;
}

void GeographicPositionGLL::setInitialGuiValues()
{
    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
}

void GeographicPositionGLL::initializeUnitsLimits()
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

void GeographicPositionGLL::initializeClickableLabels()
{
    connect(this, &GeographicPositionGLL::UserPrefsChanged, this, &GeographicPositionGLL::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitLatitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLatitude, &ClickableLabel::actionTriggered, this, &GeographicPositionGLL::ChangeLatLongUnits_triggered);
    ui->labelUnitLongitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLongitude, &ClickableLabel::actionTriggered, this, &GeographicPositionGLL::ChangeLatLongUnits_triggered);
}

void GeographicPositionGLL::onTimerTimeout()
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
}

void GeographicPositionGLL::setStatus(char status)
{
    if ((status != 'A' && status != 'V') || m_Status == status) return;
    m_Status = status;
    emit StatusChanged();
}

double GeographicPositionGLL::GPSTime() const
{
    return m_GPSTime;
}

void GeographicPositionGLL::setGPSTime(double newGPSTime)
{
    if (qFuzzyCompare(m_GPSTime, newGPSTime))
        return;
    m_GPSTime = newGPSTime;
    emit GPSTimeChanged();
}

double GeographicPositionGLL::Latitude() const
{
    return m_Latitude;
}

void GeographicPositionGLL::setLatitude(double newLatitude)
{
    if (m_Latitude == newLatitude)
        return;
    m_Latitude = newLatitude;
    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    emit LatitudeChanged();
}

double GeographicPositionGLL::Longitude() const
{
    return m_Longitude;
}

void GeographicPositionGLL::setLongitude(double newLongitude)
{
    if (m_Longitude == newLongitude)
        return;
    m_Longitude = newLongitude;
    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    emit LongitudeChanged();
}

void GeographicPositionGLL::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void GeographicPositionGLL::on_lineEditLatitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLatitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LatitudeChanged";
    onTimerTimeout();
}

void GeographicPositionGLL::on_lineEditLongitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLongitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LongitudeChanged";
    onTimerTimeout();
}

