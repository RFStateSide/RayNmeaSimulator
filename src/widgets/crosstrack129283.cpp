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

#include "crosstrack129283.h"
#include "ui_crosstrack129283.h"

#include "n2khelper.h"
#include "N2kMsg.h"
#include <cmath>
CrossTrack129283::CrossTrack129283(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent)
    , ui(new Ui::CrossTrack129283)
{
    ui->setupUi(this);
    setId(id);
    setPgn(129283);
    setColumnSpan(3);
    setRowSpan(2);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText(label + " (129283)");
    ui->checkBoxEnabled->setToolTip("Enable " + label);
    ui->pushButtonRemove->setToolTip("Remove " + label);
    ui->checkBoxEnabled->setChecked(isEnabled());

    initializeUnitsLimits();
    initializeClickableLabels();
    setInitialGuiValues();

    QIcon iconExit = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    ui->pushButtonRemove->setIcon(iconExit);
}

CrossTrack129283::~CrossTrack129283()
{
    delete ui;
}

void CrossTrack129283::initializeUnitsLimits()
{
    // Signed 32-bit, 0.01 m; the three upper codes are reserved/error/NA.
    if (!XTEValidator) XTEValidator = new QDoubleValidator(this);
    XTEValidator->setRange(N2kHelper::convertN2kDistanceNearToUserUnits(-21474836.48, UserPrefs()),
                           N2kHelper::convertN2kDistanceNearToUserUnits(21474836.44, UserPrefs()), 1);
    XTEValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->labelUnitsXTE->setText(UserPrefs().distanceUnitIndex == 0 ? "m" : "ft");
    ui->lineEditXTE->setValidator(XTEValidator);
}

void CrossTrack129283::setInitialGuiValues()
{
    const bool valid = std::isfinite(m_XTE) && !N2kIsNA(m_XTE)
        && m_XTE >= -21474836.48 && m_XTE <= 21474836.44;
    ui->lineEditXTE->setText(valid
        ? QString::number(N2kHelper::convertN2kDistanceNearToUserUnits(m_XTE, UserPrefs()), 'f', 1)
        : tr("Unavailable"));
}

void CrossTrack129283::initializeClickableLabels()
{
    connect(this, &CrossTrack129283::UserPrefsChanged, this, &CrossTrack129283::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsXTE->setMenuItems(unitsListDistance);
    connect(ui->labelUnitsXTE, &ClickableLabel::actionTriggered, this, &CrossTrack129283::ChangeDistanceUnits_triggered);
}

void CrossTrack129283::onTimerTimeout()
{
    if (m_SignalToEmit == "XTEChanged") {
        m_XTE = N2kHelper::convertUserDistanceNearToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs());
        emit XTEChanged();
    }
}

unsigned char CrossTrack129283::SID() const
{
    return m_SID;
}

void CrossTrack129283::setSID(unsigned char newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

tN2kXTEMode CrossTrack129283::XTEMode() const
{
    return m_XTEMode;
}

void CrossTrack129283::setXTEMode(tN2kXTEMode newXTEMode)
{
    if (m_XTEMode == newXTEMode)
        return;
    m_XTEMode = newXTEMode;
    emit XTEModeChanged();
}

bool CrossTrack129283::NavigationTerminated() const
{
    return m_NavigationTerminated;
}

void CrossTrack129283::setNavigationTerminated(bool newNavigationTerminated)
{
    if (m_NavigationTerminated == newNavigationTerminated)
        return;
    m_NavigationTerminated = newNavigationTerminated;
    emit NavigationTerminatedChanged();
}

double CrossTrack129283::XTE() const
{
    return m_XTE;
}

void CrossTrack129283::setXTE(double newXTE)
{
    if (qFuzzyCompare(m_XTE, newXTE))
        return;
    m_XTE = newXTE;
    setInitialGuiValues();
    emit XTEChanged();
}

void CrossTrack129283::on_lineEditXTE_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditXTE, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "XTEChanged";
    onTimerTimeout();
}

void CrossTrack129283::on_checkBoxEnabled_clicked(bool checked)
{
    setEnabled(checked);
}
