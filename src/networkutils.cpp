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

#include "networkutils.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>

NetworkUtils::NetworkUtils(QObject *parent)
    : QObject{parent}
{}

// Method to check for an internet connection
bool NetworkUtils::isInternetAvailable()
{
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("https://www.google.com"));

    // Send a HEAD request
    QNetworkReply *reply = manager.head(request);

    // Wait for the reply using an event loop
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    bool isConnected = (reply->error() == QNetworkReply::NoError);

    reply->deleteLater(); // Clean up the reply object
    return isConnected;
}
