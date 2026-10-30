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

#include "navigation129284.h"
#include "ui_navigation129284.h"

#include "n2khelper.h"
#include <N2kMsg.h>
#include <cmath>
#include <QRegularExpression>

namespace {
// ETA is seconds since midnight (including the TIME leap-second values).
// Preserve the existing ss / mm:ss / hh:mm:ss and formatted day spellings;
// validate before parsing so malformed components never silently become zero.
class EtaTimeValidator final : public QValidator {
public:
    using QValidator::QValidator;
    static bool parse(QString text, double &value) {
        qulonglong days = 0;
        static const QRegularExpression dayPattern(QStringLiteral("^(\\d+)\\s*day\\s+(.+)$"));
        const auto day = dayPattern.match(text);
        if (day.hasMatch()) { bool ok=false; days=day.captured(1).toULongLong(&ok); if (!ok || days>1) return false; text=day.captured(2); }
        const auto parts=text.split(QLatin1Char(':'));
        if (parts.isEmpty() || parts.size()>3) return false;
        qulonglong total=days*86400;
        for (int i=0;i<parts.size();++i) {
            if (parts[i].isEmpty()) return false;
            for (const auto ch:parts[i]) if (ch<QLatin1Char('0') || ch>QLatin1Char('9')) return false;
            bool ok=false;const auto component=parts[i].toULongLong(&ok);if (!ok || component>86401) return false;
            const int remaining=parts.size()-1-i;
            if (parts.size()==3 && remaining==1 && component>59) return false;
            if (parts.size()>1 && remaining==0 && component>59) {
                const bool leap = component<=61 && days==0
                    && ((parts.size()==3 && parts[0].toUInt()==23 && parts[1].toUInt()==59)
                        || (parts.size()==2 && parts[0].toUInt()==1439));
                if (!leap) return false;
            }
            total += component*(remaining==2?3600:remaining==1?60:1);
        }
        if (total>86401) return false; value=double(total);return true;
    }
    State validate(QString &text,int &) const override {double value=0;return parse(text,value)?Acceptable:Intermediate;}
};
}


Navigation129284::Navigation129284(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::Navigation129284)
    , m_DistanceToWaypoint(N2kDoubleNA)
{
    ui->setupUi(this);
    setId(id);
    setPgn(129284);
    setColumnSpan(3);
    setRowSpan(4);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (129284)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    ui->lineEditBearingPositionToDestinationWaypoint->setValidator(new QDoubleValidator(0.0, 360.0, 2, this));
    ui->lineEditETATime->setValidator(new EtaTimeValidator(this));
    initializeUnitsLimits();
    initializeClickableLabels();
    updateDistancePresentation();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

Navigation129284::~Navigation129284()
{
    delete ui;
}

void Navigation129284::initializeUnitsLimits()
{
    // km & m, NM & ft, mi & ft
    switch (UserPrefs().distanceUnitIndex) {
    case 0:
        ui->lineEditDistanceToWaypoint->setValidator(new QDoubleValidator(0.0, 65535.0, 2, this));
        ui->labelUnitsDistanceToWaypoint->setText("km");
        break;
    case 1:
        ui->lineEditDistanceToWaypoint->setValidator(new QDoubleValidator(0.0, 65535.0, 2, this));
        ui->labelUnitsDistanceToWaypoint->setText("NM");
        break;
    case 2:
        ui->lineEditDistanceToWaypoint->setValidator(new QDoubleValidator(0.0, 65535.0, 2, this));
        ui->labelUnitsDistanceToWaypoint->setText("mi");
        break;
    }
}

void Navigation129284::initializeClickableLabels()
{
    connect(this, &Navigation129284::UserPrefsChanged, this, &Navigation129284::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsDistanceToWaypoint->setMenuItems(unitsListDistance);
    connect(ui->labelUnitsDistanceToWaypoint, &ClickableLabel::actionTriggered, this, &Navigation129284::ChangeDistanceUnits_triggered);
    ui->labelUnitsETATime->setMenuItems(unitsListTime);
    connect(ui->labelUnitsETATime, &ClickableLabel::actionTriggered, this, &Navigation129284::ChangeTimeUnits_triggered);
}

void Navigation129284::onTimerTimeout()
{
    if (m_SignalToEmit == "DistanceToWaypoint") {
        double converted = N2kHelper::convertUserDistanceFarToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        m_DistanceToWaypoint = converted;
        emit DistanceToWaypointChanged();
    } else if (m_SignalToEmit == "ETATime") {
        double converted = 0;
        if (!EtaTimeValidator::parse(m_ValueToEmit, converted)) return;
        m_ETATime = converted;
        emit ETATimeChanged();
    } else if (m_SignalToEmit == "BearingPositionToDestinationWaypoint") {
        double converted = Convert::DegreesToRadians(m_ValueToEmit.toDouble());
        m_BearingPositionToDestinationWaypoint = converted;
        emit BearingPositionToDestinationWaypointChanged();
    }

    m_SignalToEmit.clear();
}

uchar Navigation129284::SID() const
{
    return m_SID;
}

void Navigation129284::setSID(uchar newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double Navigation129284::DistanceToWaypoint() const
{
    return m_DistanceToWaypoint;
}

void Navigation129284::setDistanceToWaypoint(double newDistanceToWaypoint)
{
    if (qFuzzyCompare(m_DistanceToWaypoint, newDistanceToWaypoint))
        return;
    m_DistanceToWaypoint = newDistanceToWaypoint;
    updateDistancePresentation();
    emit DistanceToWaypointChanged();
}

tN2kHeadingReference Navigation129284::BearingReference() const
{
    return m_BearingReference;
}

void Navigation129284::setBearingReference(const tN2kHeadingReference &newBearingReference)
{
    if (m_BearingReference == newBearingReference)
        return;
    m_BearingReference = newBearingReference;
    switch (static_cast<int>(newBearingReference)) {
    case N2khr_true:
        ui->labelUnitsBearingPositionToDestinationWaypoint->setText("°T");
        break;
    case N2khr_magnetic:
        ui->labelUnitsBearingPositionToDestinationWaypoint->setText("°M");
        break;
    default:
        ui->labelUnitsBearingPositionToDestinationWaypoint->setText("°");
    }

    emit BearingReferenceChanged();
}

bool Navigation129284::PerpendicularCrossed() const
{
    return m_PerpendicularCrossed;
}

void Navigation129284::setPerpendicularCrossed(bool newPerpendicularCrossed)
{
    if (m_PerpendicularCrossed == newPerpendicularCrossed)
        return;
    m_PerpendicularCrossed = newPerpendicularCrossed;
    emit PerpendicularCrossedChanged();
}

bool Navigation129284::ArrivalCircleEntered() const
{
    return m_ArrivalCircleEntered;
}

void Navigation129284::setArrivalCircleEntered(bool newArrivalCircleEntered)
{
    if (m_ArrivalCircleEntered == newArrivalCircleEntered)
        return;
    m_ArrivalCircleEntered = newArrivalCircleEntered;
    emit ArrivalCircleEnteredChanged();
}

tN2kDistanceCalculationType Navigation129284::CalculationType() const
{
    return m_CalculationType;
}

void Navigation129284::setCalculationType(const tN2kDistanceCalculationType &newCalculationType)
{
    if (m_CalculationType == newCalculationType)
        return;
    m_CalculationType = newCalculationType;
    emit CalculationTypeChanged();
}

double Navigation129284::ETATime() const
{
    return m_ETATime;
}

void Navigation129284::setETATime(double newETATime)
{
    if (qFuzzyCompare(m_ETATime, newETATime))
        return;
    m_ETATime = newETATime;
    ui->lineEditETATime->setText(newETATime == N2kDoubleNA ? tr("Unavailable") : N2kHelper::convertN2kTimeToUserUnits(newETATime));
    emit ETATimeChanged();
}

int16_t Navigation129284::ETADate() const
{
    return m_ETADate;
}

void Navigation129284::setETADate(int16_t newETADate)
{
    if (m_ETADate == newETADate)
        return;
    m_ETADate = newETADate;
    emit ETADateChanged();
}

double Navigation129284::BearingOriginToDestinationWaypoint() const
{
    return m_BearingOriginToDestinationWaypoint;
}

void Navigation129284::setBearingOriginToDestinationWaypoint(double newBearingOriginToDestinationWaypoint)
{
    if (qFuzzyCompare(m_BearingOriginToDestinationWaypoint, newBearingOriginToDestinationWaypoint))
        return;
    m_BearingOriginToDestinationWaypoint = newBearingOriginToDestinationWaypoint;
    emit BearingOriginToDestinationWaypointChanged();
}

double Navigation129284::BearingPositionToDestinationWaypoint() const
{
    return m_BearingPositionToDestinationWaypoint;
}

void Navigation129284::setBearingPositionToDestinationWaypoint(double newBearingPositionToDestinationWaypoint)
{
    if (qFuzzyCompare(m_BearingPositionToDestinationWaypoint, newBearingPositionToDestinationWaypoint))
        return;
    m_BearingPositionToDestinationWaypoint = newBearingPositionToDestinationWaypoint;
    ui->lineEditBearingPositionToDestinationWaypoint->setText(newBearingPositionToDestinationWaypoint == N2kDoubleNA ? tr("Unavailable") : QString::number(Convert::RadiansToDegrees(newBearingPositionToDestinationWaypoint), 'f', 2));
    emit BearingPositionToDestinationWaypointChanged();
}

uint32_t Navigation129284::OriginWaypointNumber() const
{
    return m_OriginWaypointNumber;
}

void Navigation129284::setOriginWaypointNumber(uint32_t newOriginWaypointNumber)
{
    if (m_OriginWaypointNumber == newOriginWaypointNumber)
        return;
    m_OriginWaypointNumber = newOriginWaypointNumber;
    emit OriginWaypointNumberChanged();
}

uint32_t Navigation129284::DestinationWaypointNumber() const
{
    return m_DestinationWaypointNumber;
}

void Navigation129284::setDestinationWaypointNumber(uint32_t newDestinationWaypointNumber)
{
    if (m_DestinationWaypointNumber == newDestinationWaypointNumber)
        return;
    m_DestinationWaypointNumber = newDestinationWaypointNumber;
    emit DestinationWaypointNumberChanged();
}

double Navigation129284::DestinationLatitude() const
{
    return m_DestinationLatitude;
}

void Navigation129284::setDestinationLatitude(double newDestinationLatitude)
{
    if (m_DestinationLatitude == newDestinationLatitude)
        return;
    m_DestinationLatitude = newDestinationLatitude;
    emit DestinationLatitudeChanged();
}

double Navigation129284::DestinationLongitude() const
{
    return m_DestinationLongitude;
}

void Navigation129284::setDestinationLongitude(double newDestinationLongitude)
{
    if (m_DestinationLongitude == newDestinationLongitude)
        return;
    m_DestinationLongitude = newDestinationLongitude;
    emit DestinationLongitudeChanged();
}

double Navigation129284::WaypointClosingVelocity() const
{
    return m_WaypointClosingVelocity;
}

void Navigation129284::setWaypointClosingVelocity(double newWaypointClosingVelocity)
{
    if (qFuzzyCompare(m_WaypointClosingVelocity, newWaypointClosingVelocity))
        return;
    m_WaypointClosingVelocity = newWaypointClosingVelocity;
    emit WaypointClosingVelocityChanged();
}

void Navigation129284::setInitialGuiValues()
{
    updateDistancePresentation();
    ui->lineEditETATime->setText(m_ETATime == N2kDoubleNA ? tr("Unavailable") : N2kHelper::convertN2kTimeToUserUnits(m_ETATime));
    ui->lineEditBearingPositionToDestinationWaypoint->setText(m_BearingPositionToDestinationWaypoint == N2kDoubleNA ? tr("Unavailable") : QString::number(Convert::RadiansToDegrees(m_BearingPositionToDestinationWaypoint), 'f', 2));
}

void Navigation129284::updateDistancePresentation()
{
    const bool valid=std::isfinite(m_DistanceToWaypoint)
        && !N2kIsNA(m_DistanceToWaypoint) && m_DistanceToWaypoint>=0;
    ui->lineEditDistanceToWaypoint->setText(valid
        ? QString::number(N2kHelper::convertN2kDistanceFarToUserUnits(m_DistanceToWaypoint, UserPrefs()), 'f', 2)
        : tr("Unavailable"));
}

void Navigation129284::on_lineEditDistanceToWaypoint_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditDistanceToWaypoint, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "DistanceToWaypoint";
    onTimerTimeout();
}

void Navigation129284::on_lineEditETATime_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditETATime, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "ETATime";
    onTimerTimeout();
}

void Navigation129284::on_lineEditBearingPositionToDestinationWaypoint_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditBearingPositionToDestinationWaypoint, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "BearingPositionToDestinationWaypoint";
    onTimerTimeout();
}

void Navigation129284::on_pushButtonRemove_clicked(bool checked)
{
    setEnabled(checked);
}

