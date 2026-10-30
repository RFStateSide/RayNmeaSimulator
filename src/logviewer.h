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

// Adapted from RF StateSide BrickSuite; owner-attested internal reuse (2026-10-28).
// BrickSuite retains its existing license; this adaptation is AGPL-3.0-or-later.
#pragma once
#include "applicationlog.h"
#include <QDialog>
#include <QWidget>
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class LoggingPreferences final : public QWidget
{
  public:
    explicit LoggingPreferences(QWidget *parent = nullptr);
    void load();
    ApplicationLog::Options value() const;

  private:
    QComboBox *level;
    QCheckBox *bindings, *rx, *tx, *n2k, *n183, *raw;
    QSpinBox *applicationSize, *trafficSize;
    QLineEdit *excluded;
};
class LogViewer final : public QDialog
{
    Q_OBJECT
  public:
    explicit LogViewer(QWidget *parent = nullptr);
    void refresh();

  private:
    void refreshDisplay();
    bool refreshPending = false;
    QComboBox *channel, *severity;
    QLineEdit *category, *search;
    QCheckBox *pause, *follow, *rawJson;
    QLabel *location, *status;
    QPlainTextEdit *text;
    QPushButton *record;
};
