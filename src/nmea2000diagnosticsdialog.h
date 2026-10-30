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
#include <QPointer>
class Nmea2000Diagnostics;
class NmeaDeviceDiscovery;
class QPushButton;
class QTableWidget;
class QTreeWidget;
class QPlainTextEdit;
class QLabel;
class QCheckBox;
class QLineEdit;
class Nmea2000DiagnosticsDialog final : public QDialog
{
    Q_OBJECT
  public:
    explicit Nmea2000DiagnosticsDialog(Nmea2000Diagnostics *, QWidget *parent = nullptr,
                                      NmeaDeviceDiscovery *discovery = nullptr);
    void discoveryFeedback(const QString &);
    void refresh(bool explicitRequest = true);
  signals:
    void openApplicationLog();
    void discoveryRequested();

  private:
    void showDevice();
    void showMessage();
    void addField(const QString &, const QString &, const QString & = {});
    QPointer<Nmea2000Diagnostics> model;
    QTableWidget *devices, *messages;
    QTreeWidget *details;
    QPlainTextEdit *raw;
    QLabel *status;
    QCheckBox *pause;
    QLineEdit *filter;
    quint64 displayedRevision = quint64(-1);
    QString displayedDeviceStates;
    QLabel *discoveryStatus;
};
