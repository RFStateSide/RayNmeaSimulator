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

#ifndef UIHELPER_H
#define UIHELPER_H

#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>
#include "N2kTypes.h"
#include "datastructs.h"

class UiHelper
{
public:
    UiHelper();

    static int ShowSpinBoxDialog(const QString &title, const QString &labelText, int minValue, int maxValue, int defaultValue);
    static int ShowTemperatureSourcePicker(const QString &title, const QString &labelText, int defaultIndex = 0);
    static int ShowFluidLevelSourcePicker(const QString &title, const QString &labelText, int defaultIndex = 0);
    static QString GetLayoutNameInput(QWidget *parent, const QString &title, const QString &message, const QString &defaultFileName);
    static QString ShowLayoutPicker(const QString &title, const QString &labelText);
    static QString ShowGroupPicker(const QString &title, const QString &labelText, const QStringList &groupList, const QString &initialGroup = QString());
    static QString ShowStationPicker(const QString &title, const QString &labelText, const QList<Station> &stations, QWidget* owner = nullptr);
};

#endif // UIHELPER_H
