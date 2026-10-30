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
#include "globalpositioninggga.h"
#include "ui_globalpositioninggga.h"

#include "convert.h"
#include "n2khelper.h"

GlobalPositioningGGA::GlobalPositioningGGA(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::GlobalPositioningGGA)
{
    ui->setupUi(this);
    setId(id);
    setPgn(SNT_GGA);
    setColumnSpan(3);
    setRowSpan(5);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (GGA)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

GlobalPositioningGGA::~GlobalPositioningGGA()
{
    delete ui;
}

void GlobalPositioningGGA::setInitialGuiValues()
{
    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    ui->lineEditnSatellites->setText(QString::number(m_SatelliteCount));
    ui->lineEditTime->setText(N2kHelper::convertN2kTimeToUserFormat(m_GPSTime, UserPrefs()));
}

void GlobalPositioningGGA::initializeUnitsLimits()
{
    setCoordinateValidator(ui->lineEditLatitude, UserPrefs().latLongUnitIndex, true);
    setCoordinateValidator(ui->lineEditLongitude, UserPrefs().latLongUnitIndex, false);
    // "DD.DDDD", "DD°MM’SS", "DD°MM.MMM’"
    switch (UserPrefs().latLongUnitIndex) {
    case 0:
        ui->labelUnitLatitude->setText("DD.DDDD°");
        ui->labelUnitLongitude->setText("DD.DDDD°");
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

    switch (UserPrefs().timeUnitIndex) {
    case 0:
        ui->labelUnitsTime->setText("hh:mm");
        break;
    case 1:
        ui->labelUnitsTime->setText("hh:mm AP");
        break;
    case 2:
        ui->labelUnitsTime->setText("hh:mm:ss");
        break;
    case 3:
        ui->labelUnitsTime->setText("hh:mm:ss AP");
        break;
    }

    ui->lineEditnSatellites->setValidator(new QIntValidator(0, 99, this));
}

void GlobalPositioningGGA::initializeClickableLabels()
{
    connect(this, &GlobalPositioningGGA::UserPrefsChanged, this, &GlobalPositioningGGA::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitLatitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLatitude, &ClickableLabel::actionTriggered, this, &GlobalPositioningGGA::ChangeLatLongUnits_triggered);
    ui->labelUnitLongitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLongitude, &ClickableLabel::actionTriggered, this, &GlobalPositioningGGA::ChangeLatLongUnits_triggered);
}

void GlobalPositioningGGA::onTimerTimeout()
{
    if (m_SignalToEmit == "LatitudeChanged") {
        double converted = 0.0;
        switch (UserPrefs().latLongUnitIndex) {
        case 0:
            converted = m_ValueToEmit.toDouble();
            break;
        case 1:
            converted = Convert::DMSToDecimal(m_ValueToEmit, true);
            break;
        case 2:
            converted = Convert::DMMToDecimal(m_ValueToEmit, true);
            break;
        }
        m_Latitude = converted;
        emit LatitudeChanged();
    } else if (m_SignalToEmit == "LongitudeChanged") {
        double converted = 0.0;
        switch (UserPrefs().latLongUnitIndex) {
        case 0:
            converted = m_ValueToEmit.toDouble();
            break;
        case 1:
            converted = Convert::DMSToDecimal(m_ValueToEmit, false);
            break;
        case 2:
            converted = Convert::DMMToDecimal(m_ValueToEmit, false);
            break;
        }
        m_Longitude = converted;
        emit LongitudeChanged();
    } else if (m_SignalToEmit == "nSatellitesChanged") {
        m_SatelliteCount = m_ValueToEmit.toUShort();
        emit SatelliteCountChanged();
    } else if (m_SignalToEmit == "GPSTimeChanged") {
        m_GPSTime = N2kHelper::convertUserFormattedTimeToN2k(m_ValueToEmit, UserPrefs());
        emit GPSTimeChanged();
    }

    m_SignalToEmit.clear();
}

double GlobalPositioningGGA::GPSTime() const
{
    return m_GPSTime;
}

void GlobalPositioningGGA::setGPSTime(double newGPSTime)
{
    if (qFuzzyCompare(m_GPSTime, newGPSTime))
        return;
    m_GPSTime = newGPSTime;
    ui->lineEditTime->setText(N2kHelper::convertN2kTimeToUserFormat(m_GPSTime, UserPrefs()));
    emit GPSTimeChanged();
}

double GlobalPositioningGGA::Latitude() const
{
    return m_Latitude;
}

void GlobalPositioningGGA::setLatitude(double newLatitude)
{
    if (m_Latitude == newLatitude)
        return;
    m_Latitude = newLatitude;

    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    emit LatitudeChanged();
}

double GlobalPositioningGGA::Longitude() const
{
    return m_Longitude;
}

void GlobalPositioningGGA::setLongitude(double newLongitude)
{
    if (m_Longitude == newLongitude)
        return;
    m_Longitude = newLongitude;

    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    emit LongitudeChanged();
}

int GlobalPositioningGGA::GPSQualityIndicator() const
{
    return m_GPSQualityIndicator;
}

void GlobalPositioningGGA::setGPSQualityIndicator(int newGPSQualityIndicator)
{
    if (m_GPSQualityIndicator == newGPSQualityIndicator)
        return;
    m_GPSQualityIndicator = newGPSQualityIndicator;
    emit GPSQualityIndicatorChanged();
}

int GlobalPositioningGGA::SatelliteCount() const
{
    return m_SatelliteCount;
}

void GlobalPositioningGGA::setSatelliteCount(int newSatelliteCount)
{
    if (m_SatelliteCount == newSatelliteCount)
        return;
    m_SatelliteCount = newSatelliteCount;
    emit SatelliteCountChanged();
}

double GlobalPositioningGGA::HDOP() const
{
    return m_HDOP;
}

void GlobalPositioningGGA::setHDOP(double newHDOP)
{
    if (qFuzzyCompare(m_HDOP, newHDOP))
        return;
    m_HDOP = newHDOP;
    emit HDOPChanged();
}

double GlobalPositioningGGA::Altitude() const
{
    return m_Altitude;
}

void GlobalPositioningGGA::setAltitude(double newAltitude)
{
    if (qFuzzyCompare(m_Altitude, newAltitude))
        return;
    m_Altitude = newAltitude;
    emit AltitudeChanged();
}

double GlobalPositioningGGA::GeoidalSeparation() const
{
    return m_GeoidalSeparation;
}

void GlobalPositioningGGA::setGeoidalSeparation(double newGeoidalSeparation)
{
    if (qFuzzyCompare(m_GeoidalSeparation, newGeoidalSeparation))
        return;
    m_GeoidalSeparation = newGeoidalSeparation;
    emit GeoidalSeparationChanged();
}

double GlobalPositioningGGA::DGPSAge() const
{
    return m_DGPSAge;
}

void GlobalPositioningGGA::setDGPSAge(double newDGPSAge)
{
    if (qFuzzyCompare(m_DGPSAge, newDGPSAge))
        return;
    m_DGPSAge = newDGPSAge;
    emit DGPSAgeChanged();
}

int GlobalPositioningGGA::DGPSReferenceStationID() const
{
    return m_DGPSReferenceStationID;
}

void GlobalPositioningGGA::setDGPSReferenceStationID(int newDGPSReferenceStationID)
{
    if (m_DGPSReferenceStationID == newDGPSReferenceStationID)
        return;
    m_DGPSReferenceStationID = newDGPSReferenceStationID;
    emit DGPSReferenceStationIDChanged();
}

void GlobalPositioningGGA::on_lineEditLatitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLatitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LatitudeChanged";
    onTimerTimeout();
}

void GlobalPositioningGGA::on_lineEditLongitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLongitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LongitudeChanged";
    onTimerTimeout();
}

void GlobalPositioningGGA::on_lineEditnSatellites_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditnSatellites, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "nSatellitesChanged";
    onTimerTimeout();
}

void GlobalPositioningGGA::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

void GlobalPositioningGGA::on_lineEditTime_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditTime, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "GPSTimeChanged";
    onTimerTimeout();
}

