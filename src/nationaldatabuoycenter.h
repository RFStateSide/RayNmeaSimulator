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

#ifndef NATIONALDATABUOYCENTER_H
#define NATIONALDATABUOYCENTER_H
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(ndbcLog)
#include "datastructs.h"
#include "weatherobservation.h"
class NationalDataBuoyCenter : public QObject {
    Q_OBJECT
  public:
    enum class Cancellation { Explicit, Superseded, CoordinatesChanged, SourceChanged, ModeChanged, Destroyed };
    Q_ENUM(Cancellation)
    explicit NationalDataBuoyCenter(QObject *parent = nullptr,
                                    QUrl stations = QUrl("https://www.ndbc.noaa.gov/activestations.xml"),
                                    QUrl data = QUrl("https://www.ndbc.noaa.gov/data/realtime2/"),
                                    QNetworkAccessManager *manager = nullptr); // Optional non-owned fixture manager.
    QString diagnosticContext(const char *stage, const QString &station = {}) const;
    bool stationFeedUnavailable() const { return feedUnavailable; }
    bool hasActiveRequest() const { return !active.isNull(); }
    static bool parse(const QByteArray &, WeatherObservation &, QString &);
    static bool parseStationsXml(const QByteArray &, QList<Station> &, QString &);
  public slots:
    void fetchStationData(const QString &);
    void getNearestStations(double latitude, double longitude, double radius);
    void cancel(Cancellation reason = Cancellation::Explicit);
  signals:
    void observationFetched(const WeatherObservation &);
    void dataFetched(const QString &, double, double, double, double, double, double, double);
    void nearestStationsFound(const QList<Station> &);
    void errorOccurred(const QString &);

  private:
    QNetworkAccessManager *networkManager;
    QPointer<QNetworkReply> active;
    QUrl stationsUrl, dataUrl;
    bool feedUnavailable = false;
    quint64 requestId = 0;
    QString lastStation;
    QNetworkReply *request(QUrl, const char *stage, const QString &station = {});
    static double distance(double, double, double, double);
};
#endif
