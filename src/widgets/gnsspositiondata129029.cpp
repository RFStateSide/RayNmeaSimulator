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
#include "gnsspositiondata129029.h"
#include "ui_gnsspositiondata129029.h"

#include "convert.h"
#include "n2khelper.h"

GnssPositionData129029::GnssPositionData129029(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::GnssPositionData129029)
{
    ui->setupUi(this);
    setId(id);
    setPgn(129029);
    setColumnSpan(3);
    setRowSpan(4);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (129029)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

GnssPositionData129029::~GnssPositionData129029()
{
    delete ui;
}

void GnssPositionData129029::initializeUnitsLimits()
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

    ui->lineEditnSatellites->setValidator(new QIntValidator(0, 255, this));
}

void GnssPositionData129029::initializeClickableLabels()
{
    connect(this, &GnssPositionData129029::UserPrefsChanged, this, &GnssPositionData129029::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitLatitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLatitude, &ClickableLabel::actionTriggered, this, &GnssPositionData129029::ChangeLatLongUnits_triggered);
    ui->labelUnitLongitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLongitude, &ClickableLabel::actionTriggered, this, &GnssPositionData129029::ChangeLatLongUnits_triggered);
}

uchar GnssPositionData129029::SID() const
{
    return m_SID;
}

void GnssPositionData129029::setSID(uchar newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

uint16_t GnssPositionData129029::DaysSince1970() const
{
    return m_DaysSince1970;
}

void GnssPositionData129029::setDaysSince1970(uint16_t newDaysSince1970)
{
    if (m_DaysSince1970 == newDaysSince1970)
        return;
    m_DaysSince1970 = newDaysSince1970;
    emit DaysSince1970Changed();
}

double GnssPositionData129029::SecondsSinceMidnight() const
{
    return m_SecondsSinceMidnight;
}

void GnssPositionData129029::setSecondsSinceMidnight(double newSecondsSinceMidnight)
{
    if (qFuzzyCompare(m_SecondsSinceMidnight, newSecondsSinceMidnight))
        return;
    m_SecondsSinceMidnight = newSecondsSinceMidnight;
    emit SecondsSinceMidnightChanged();
}

double GnssPositionData129029::Latitude() const
{
    return m_Latitude;
}

void GnssPositionData129029::setLatitude(double newLatitude)
{
    if (m_Latitude == newLatitude)
        return;
    m_Latitude = newLatitude;

    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    emit LatitudeChanged();
}

double GnssPositionData129029::Longitude() const
{
    return m_Longitude;
}

void GnssPositionData129029::setLongitude(double newLongitude)
{
    if (m_Longitude == newLongitude)
        return;
    m_Longitude = newLongitude;

    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    emit LongitudeChanged();
}

double GnssPositionData129029::Altitude() const
{
    return m_Altitude;
}

void GnssPositionData129029::setAltitude(double newAltitude)
{
    if (qFuzzyCompare(m_Altitude, newAltitude))
        return;
    m_Altitude = newAltitude;
    emit AltitudeChanged();
}

tN2kGNSStype GnssPositionData129029::GNSStype() const
{
    return m_GNSStype;
}

void GnssPositionData129029::setGNSStype(tN2kGNSStype newGNSStype)
{
    if (m_GNSStype == newGNSStype)
        return;
    m_GNSStype = newGNSStype;
    emit GNSStypeChanged();
}

tN2kGNSSmethod GnssPositionData129029::GNSSmethod() const
{
    return m_GNSSmethod;
}

void GnssPositionData129029::setGNSSmethod(tN2kGNSSmethod newGNSSmethod)
{
    if (m_GNSSmethod == newGNSSmethod)
        return;
    m_GNSSmethod = newGNSSmethod;
    emit GNSSmethodChanged();
}

uchar GnssPositionData129029::nSatellites() const
{
    return m_nSatellites;
}

void GnssPositionData129029::setnSatellites(uchar newNSatellites)
{
    if (m_nSatellites == newNSatellites)
        return;
    m_nSatellites = newNSatellites;
    ui->lineEditnSatellites->setText(QString::number(m_nSatellites));
    emit nSatellitesChanged();
}

double GnssPositionData129029::HDOP() const
{
    return m_HDOP;
}

void GnssPositionData129029::setHDOP(double newHDOP)
{
    if (qFuzzyCompare(m_HDOP, newHDOP))
        return;
    m_HDOP = newHDOP;
    emit HDOPChanged();
}

double GnssPositionData129029::PDOP() const
{
    return m_PDOP;
}

void GnssPositionData129029::setPDOP(double newPDOP)
{
    if (qFuzzyCompare(m_PDOP, newPDOP))
        return;
    m_PDOP = newPDOP;
    emit PDOPChanged();
}

double GnssPositionData129029::GeoidalSeparation() const
{
    return m_GeoidalSeparation;
}

void GnssPositionData129029::setGeoidalSeparation(double newGeoidalSeparation)
{
    if (qFuzzyCompare(m_GeoidalSeparation, newGeoidalSeparation))
        return;
    m_GeoidalSeparation = newGeoidalSeparation;
    emit GeoidalSeparationChanged();
}

uchar GnssPositionData129029::nReferenceStations() const
{
    return m_nReferenceStations;
}

void GnssPositionData129029::setnReferenceStations(uchar newNReferenceStations)
{
    if (m_nReferenceStations == newNReferenceStations)
        return;
    m_nReferenceStations = newNReferenceStations;
    emit nReferenceStationsChanged();
}

tN2kGNSStype GnssPositionData129029::ReferenceStationType() const
{
    return m_ReferenceStationType;
}

void GnssPositionData129029::setReferenceStationType(tN2kGNSStype newReferenceStationType)
{
    if (m_ReferenceStationType == newReferenceStationType)
        return;
    m_ReferenceStationType = newReferenceStationType;
    emit ReferenceStationTypeChanged();
}

uint16_t GnssPositionData129029::ReferenceSationID() const
{
    return m_ReferenceSationID;
}

void GnssPositionData129029::setReferenceSationID(uint16_t newReferenceSationID)
{
    if (m_ReferenceSationID == newReferenceSationID)
        return;
    m_ReferenceSationID = newReferenceSationID;
    emit ReferenceSationIDChanged();
}

double GnssPositionData129029::AgeOfCorrection() const
{
    return m_AgeOfCorrection;
}

void GnssPositionData129029::setAgeOfCorrection(double newAgeOfCorrection)
{
    if (qFuzzyCompare(m_AgeOfCorrection, newAgeOfCorrection))
        return;
    m_AgeOfCorrection = newAgeOfCorrection;
    emit AgeOfCorrectionChanged();
}

void GnssPositionData129029::on_lineEditLatitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLatitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LatitudeChanged";
    onTimerTimeout();
}

void GnssPositionData129029::on_lineEditLongitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLongitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LongitudeChanged";
    onTimerTimeout();
}

void GnssPositionData129029::on_lineEditnSatellites_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditnSatellites, arg1)) return;
    //ui->lineEditnSatellites->setText(arg1);
    //m_nSatellites = arg1.toUShort();
    m_ValueToEmit = arg1;
    m_SignalToEmit = "nSatellitesChanged";
    onTimerTimeout();
}

void GnssPositionData129029::updateSID(unsigned char newSID)
{
    setSID(newSID);
}

void GnssPositionData129029::updateDaysSince1970(uint16_t newDaysSince1970)
{
    setDaysSince1970(newDaysSince1970);
}

void GnssPositionData129029::updateSecondsSinceMidnight(double newSecondsSinceMidnight)
{
    setSecondsSinceMidnight(newSecondsSinceMidnight);
}

void GnssPositionData129029::onTimerTimeout()
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
        m_nSatellites = m_ValueToEmit.toUShort();
        emit nSatellitesChanged();
    }

    m_SignalToEmit.clear();
}

void GnssPositionData129029::setInitialGuiValues()
{
    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    ui->lineEditnSatellites->setText(QString::number(m_nSatellites));
}

void GnssPositionData129029::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

