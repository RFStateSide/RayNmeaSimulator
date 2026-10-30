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
#include "recommendedmingnssrmc.h"
#include "convert.h"
#include "n2khelper.h"
#include "ui_recommendedmingnssrmc.h"

RecommendedMinGnssRMC::RecommendedMinGnssRMC(int id, QString label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::RecommendedMinGnssRMC)
{
    ui->setupUi(this);
    setId(id);
    setPgn(SNT_RMC);
    setColumnSpan(3);
    setRowSpan(7);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (RMC)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

RecommendedMinGnssRMC::~RecommendedMinGnssRMC()
{
    delete ui;
}

void RecommendedMinGnssRMC::setInitialGuiValues()
{
    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    ui->lineEditCog->setText(QString::number(Convert::RadiansToDegrees(m_TrueCOG)));
    ui->lineEditSog->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_SOG, UserPrefs())));
}

void RecommendedMinGnssRMC::initializeUnitsLimits()
{
    setCoordinateValidator(ui->lineEditLatitude, UserPrefs().latLongUnitIndex, true);
    setCoordinateValidator(ui->lineEditLongitude, UserPrefs().latLongUnitIndex, false);
    // "Knots", "km/h", "MPH range"
    switch (UserPrefs().speedUnitIndex) {
    case 0:
        validatorSog->setRange(0, 90.0, 1);
        ui->labelUnitsSog->setText("m/s");
        break;
    case 3:
        validatorSog->setRange(0, 173.7, 1);
        ui->labelUnitsSog->setText("knots");
        break;
    case 1:
        validatorSog->setRange(0, 321.7, 1);
        ui->labelUnitsSog->setText("km/h");
        break;
    case 2:
        validatorSog->setRange(0, 199.9, 1);
        ui->labelUnitsSog->setText("MPH");
        break;
    }

    ui->lineEditSog->setValidator(validatorSog);
    validatorCog->setRange(0, 360, 1);
    ui->lineEditCog->setValidator(validatorCog);

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

void RecommendedMinGnssRMC::initializeClickableLabels()
{
    connect(this, &RecommendedMinGnssRMC::UserPrefsChanged, this, &RecommendedMinGnssRMC::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitLatitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLatitude, &ClickableLabel::actionTriggered, this, &RecommendedMinGnssRMC::ChangeLatLongUnits_triggered);
    ui->labelUnitLongitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLongitude, &ClickableLabel::actionTriggered, this, &RecommendedMinGnssRMC::ChangeLatLongUnits_triggered);
    ui->labelUnitsSog->setMenuItems(unitsListSpeed);
    connect(ui->labelUnitsSog, &ClickableLabel::actionTriggered, this, &RecommendedMinGnssRMC::ChangeSpeedUnits_triggered);
    ui->labelUnitsCog->setMenuItems(unitsListHeading);
    connect(ui->labelUnitsCog, &ClickableLabel::actionTriggered, this, &RecommendedMinGnssRMC::ChangeHeadingUnits_triggered);
}

void RecommendedMinGnssRMC::onTimerTimeout()
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
    } else if (m_SignalToEmit == "TrueCOG") {
        double convertedValue = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        m_TrueCOG = convertedValue;
        ui->lineEditCog->setText(m_ValueToEmit);
        emit TrueCOGChanged();
    } else if (m_SignalToEmit == "SOG") {
        double convertedValue = N2kHelper::convertUserSpeedToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        m_SOG = convertedValue;
        ui->lineEditSog->setText(m_ValueToEmit);
        emit SOGChanged();
    }
}

double RecommendedMinGnssRMC::GPSTime() const
{
    return m_GPSTime;
}

void RecommendedMinGnssRMC::setGPSTime(double newGPSTime)
{
    if (qFuzzyCompare(m_GPSTime, newGPSTime))
        return;
    m_GPSTime = newGPSTime;
    ui->lineEditTime->setText(N2kHelper::convertN2kTimeToUserFormat(m_GPSTime, UserPrefs()));
    emit GPSTimeChanged();
}

char RecommendedMinGnssRMC::Status() const
{
    return m_Status;
}

void RecommendedMinGnssRMC::setStatus(char newStatus)
{
    if (m_Status == newStatus)
        return;
    m_Status = newStatus;
    emit StatusChanged();
}

double RecommendedMinGnssRMC::Latitude() const
{
    return m_Latitude;
}

void RecommendedMinGnssRMC::setLatitude(double newLatitude)
{
    if (m_Latitude == newLatitude)
        return;
    m_Latitude = newLatitude;
    ui->lineEditLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(m_Latitude, UserPrefs()));
    emit LatitudeChanged();
}

double RecommendedMinGnssRMC::Longitude() const
{
    return m_Longitude;
}

void RecommendedMinGnssRMC::setLongitude(double newLongitude)
{
    if (m_Longitude == newLongitude)
        return;
    m_Longitude = newLongitude;
    ui->lineEditLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(m_Longitude, UserPrefs()));
    emit LongitudeChanged();
}

double RecommendedMinGnssRMC::TrueCOG() const
{
    return m_TrueCOG;
}

void RecommendedMinGnssRMC::setTrueCOG(double newTrueCOG)
{
    if (qFuzzyCompare(m_TrueCOG, newTrueCOG))
        return;
    m_TrueCOG = newTrueCOG;
    ui->lineEditCog->setText(QString::number(Convert::RadiansToDegrees(m_TrueCOG)));
    emit TrueCOGChanged();
}

double RecommendedMinGnssRMC::SOG() const
{
    return m_SOG;
}

void RecommendedMinGnssRMC::setSOG(double newSOG)
{
    if (qFuzzyCompare(m_SOG, newSOG))
        return;
    m_SOG = newSOG;
    ui->lineEditSog->setText(QString::number(N2kHelper::convertN2kSpeedToUserUnits(m_SOG, UserPrefs())));
    emit SOGChanged();
}

ulong RecommendedMinGnssRMC::DaysSince1970() const
{
    return m_DaysSince1970;
}

void RecommendedMinGnssRMC::setDaysSince1970(ulong newDaysSince1970)
{
    if (m_DaysSince1970 == newDaysSince1970)
        return;
    m_DaysSince1970 = newDaysSince1970;
    ui->lineEditDate->setText(N2kHelper::convertN2kDateToUserFormat(newDaysSince1970, UserPrefs()));
    emit DaysSince1970Changed();
}

double RecommendedMinGnssRMC::Variation() const
{
    return m_Variation;
}

void RecommendedMinGnssRMC::setVariation(double newVariation)
{
    if (qFuzzyCompare(m_Variation, newVariation))
        return;
    m_Variation = newVariation;
    emit VariationChanged();
}

time_t RecommendedMinGnssRMC::DateTime() const
{
    return m_DateTime;
}

void RecommendedMinGnssRMC::setDateTime(time_t newDateTime)
{
    if (m_DateTime == newDateTime)
        return;
    m_DateTime = newDateTime;
    emit DateTimeChanged();
}

void RecommendedMinGnssRMC::on_lineEditLatitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLatitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LatitudeChanged";
    onTimerTimeout();
}

void RecommendedMinGnssRMC::on_lineEditLongitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLongitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LongitudeChanged";
    onTimerTimeout();
}

void RecommendedMinGnssRMC::on_lineEditCog_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditCog, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "TrueCOGChanged";
    onTimerTimeout();
}

void RecommendedMinGnssRMC::on_lineEditSog_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditSog, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "SOGChanged";
    onTimerTimeout();
}

void RecommendedMinGnssRMC::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}

