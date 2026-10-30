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

#ifndef OPENWEATHERMAP_H
#define OPENWEATHERMAP_H
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include "weatherobservation.h"
class OpenWeatherMap : public QObject {
    Q_OBJECT
  public:
    explicit OpenWeatherMap(QObject *parent = nullptr,
                            QUrl endpoint = QUrl("https://api.openweathermap.org/data/2.5/weather"), QNetworkAccessManager *manager = nullptr);
    static bool parse(const QByteArray &, WeatherObservation &, QString &);
  public slots:
    void setApiKey(const QString &);
    void fetchWeather(double latitude, double longitude);
    void cancel();
  signals:
    void observationFetched(const WeatherObservation &);
    void weatherFetched(const QString &, const QString &, double, double, double, double, double);
    void errorOccurred(const QString &);

  private:
    QNetworkAccessManager *networkManager;
    QPointer<QNetworkReply> active;
    QString apiKey;
    QUrl endpoint;
};
#endif
