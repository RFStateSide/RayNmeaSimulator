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

#include "openweathermap.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QTimer>
#include <QtMath>
OpenWeatherMap::OpenWeatherMap(QObject *p, QUrl url, QNetworkAccessManager *manager)
    : QObject(p), networkManager(manager ? manager : new QNetworkAccessManager(this)), endpoint(url) {
    qRegisterMetaType<WeatherObservation>();
}
void OpenWeatherMap::cancel() {
    auto reply = active;
    active.clear();
    if (reply)
        reply->abort();
}
void OpenWeatherMap::setApiKey(const QString &key) {
    if (apiKey == key)
        return;
    cancel();
    apiKey = key;
}
bool OpenWeatherMap::parse(const QByteArray &bytes, WeatherObservation &o, QString &error) {
    o = WeatherObservation{};
    o.provider = "OpenWeatherMap";
    o.retrieved = QDateTime::currentDateTimeUtc();
    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        error = "Malformed OpenWeatherMap JSON.";
        return false;
    }
    auto root = doc.object();
    if (!root["main"].isObject() || (root.contains("cod") && root["cod"].toVariant().toString() != "200")) {
        error = "OpenWeatherMap response has no successful measurement object.";
        return false;
    }
    auto read = [&](QJsonObject object, QString key, double low, double high) {
        auto v = object[key];
        if (v.isUndefined() || v.isNull()) {
            o.missing << key;
            return N2kDoubleNA;
        }
        if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() < low || v.toDouble() > high) {
            o.invalid << key;
            return N2kDoubleNA;
        }
        return v.toDouble();
    };
    auto main = root["main"].toObject();
    auto wind = root["wind"].toObject();
    auto coord = root["coord"].toObject();
    o.air = read(main, "temp", 1, 500);
    o.humidity = read(main, "humidity", 0, 100);
    o.pressure = read(main, "pressure", 1, 2000);
    if (WeatherObservation::valid(o.pressure))
        o.pressure *= 100;
    o.windSpeed = read(wind, "speed", 0, 200);
    o.windAngle = read(wind, "deg", 0, 360);
    if (WeatherObservation::valid(o.windAngle))
        o.windAngle = qDegreesToRadians(o.windAngle);
    o.windGust = read(wind, "gust", 0, 200);
    o.visibility = read(root, "visibility", 0, 18520000); // documented meters
    o.missing << "sea temperature" << "WVHT" << "DPD" << "APD" << "MWD" << "DEWP" << "PTDY" << "TIDE";
    o.latitude = read(coord, "lat", -90, 90);
    o.longitude = read(coord, "lon", -180, 180);
    o.location = root["name"].toString();
    const auto dt = root["dt"];
    if (dt.isDouble() && std::isfinite(dt.toDouble()) && dt.toDouble() > 0 && dt.toDouble() < 4102444800. &&
        std::floor(dt.toDouble()) == dt.toDouble())
        o.observed = QDateTime::fromSecsSinceEpoch(qint64(dt.toDouble()), Qt::UTC);
    if (root["weather"].isArray() && !root["weather"].toArray().isEmpty())
        o.description = root["weather"].toArray().first().toObject()["description"].toString();
    if (!o.any()) {
        error = "OpenWeatherMap returned no usable measurements.";
        return false;
    }
    return true;
}
void OpenWeatherMap::fetchWeather(double latitude, double longitude) {
    cancel();
    if (apiKey.isEmpty()) {
        emit errorOccurred("Configure an OpenWeatherMap API key in Setup.");
        return;
    }
    if (!std::isfinite(latitude) || !std::isfinite(longitude) || qAbs(latitude) > 90 ||
        qAbs(longitude) > 180) {
        emit errorOccurred("Invalid weather coordinates.");
        return;
    }
    QUrl url = endpoint;
    QUrlQuery query;
    query.addQueryItem("lat", QString::number(latitude, 'g', 15));
    query.addQueryItem("lon", QString::number(longitude, 'g', 15));
    query.addQueryItem("units", "standard");
    query.addQueryItem("appid", apiKey);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    auto *reply = networkManager->get(request);
    active = reply;
    QTimer::singleShot(15000, reply, [reply] {
        if (!reply->isFinished()) {
            reply->setProperty("weatherTimeout", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, latitude, longitude] {
        reply->deleteLater();
        if (active != reply)
            return;
        active.clear();
        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            emit errorOccurred(reply->property("weatherTimeout").toBool()
                                   ? "OpenWeatherMap request timed out; retry Fetch Weather."
                                   : QString("OpenWeatherMap request failed (HTTP %1, network %2). Check "
                                             "credentials, subscription and connectivity.")
                                         .arg(status)
                                         .arg(int(reply->error())));
            return;
        }
        WeatherObservation o;
        QString error;
        if (!parse(reply->readAll(), o, error)) {
            emit errorOccurred(error);
            return;
        }
        if (!WeatherObservation::valid(o.latitude))
            o.latitude = latitude;
        if (!WeatherObservation::valid(o.longitude))
            o.longitude = longitude;
        emit observationFetched(o);
        emit weatherFetched(o.location, o.description, o.air, o.humidity,
                            WeatherObservation::valid(o.pressure) ? o.pressure / 100 : N2kDoubleNA,
                            o.windSpeed, o.windAngle);
    });
}
