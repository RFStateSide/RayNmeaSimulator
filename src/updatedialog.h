/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2026 RF StateSide LLC
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

#pragma once
#include <QDialog>
#include "updatechecker.h"
class QLabel;
class QPushButton;
class QPlainTextEdit;
class UpdateDialog final:public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget *parent=nullptr,UpdateChecker *checker=nullptr);
    void startCheck();
private:
    UpdateChecker *checker;
    QLabel *latest,*status,*package;
    QPlainTextEdit *notes;
    QPushButton *check,*open;
    QUrl releaseUrl;
    QString releaseTag;
};
