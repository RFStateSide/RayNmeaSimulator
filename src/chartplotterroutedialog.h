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
#include "chartplotterrouteclient.h"
#include <QDialog>
class Nmea2000Handler;
class QComboBox;
class QListWidget;
class QLabel;
class QPushButton;
class ChartplotterRouteDialog : public QDialog {
    Q_OBJECT
  public:
    ChartplotterRouteDialog(Nmea2000Handler *handler,
                            const QVector<ReceivedRoutes::Route> &passive,
                            QWidget *parent = nullptr);
    ~ChartplotterRouteDialog();
    ReceivedRoutes::Route selection() const { return selected; }
    std::function<bool(const ReceivedRoutes::Route &)> passiveStillValid;

  private:
    bool authorize();
    void update();
    void choose();
    Nmea2000Handler *handler;
    ChartplotterRouteClient client;
    QComboBox *devices;
    QListWidget *routes;
    QLabel *status;
    QPushButton *discover, *refresh, *retrieve, *select, *cancelOperation;
    QVector<ReceivedRoutes::Route> passive;
    ReceivedRoutes::Route selected;
};
