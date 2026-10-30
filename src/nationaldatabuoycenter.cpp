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

#include "nationaldatabuoycenter.h"
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QTimer>
#include <QtMath>
#include <algorithm>
#include <atomic>
#include <QElapsedTimer>
#include <QMetaEnum>
#include <QSslError>
Q_LOGGING_CATEGORY(ndbcLog, "raynmea.weather.ndbc")
namespace {
std::atomic<quint64> nextRequestId{0};
qint64 monotonicMilliseconds() { QElapsedTimer t; t.start(); return t.msecsSinceReference(); }
QString safeStation(const QString &station) {
    return station.isEmpty() ? "-" : QRegularExpression("^[A-Za-z0-9]{3,10}$").match(station).hasMatch() ? station : "<invalid>";
}
QString context(quint64 id, const QString &stage, const QString &station) {
    return QString("NOAA/NDBC request=%1 stage=%2 station=%3").arg(id).arg(stage, safeStation(station));
}
QString replyContext(QNetworkReply *reply) {
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const auto error = reply->error();
    const auto *name = QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(error);
    return context(reply->property("ndbcId").toULongLong(), reply->property("ndbcStage").toString(), reply->property("ndbcStation").toString())
        + QString(" elapsed_ms=%1 http=%2 network=%3(%4)")
            .arg(monotonicMilliseconds()-reply->property("ndbcStarted").toLongLong())
            .arg(status.isValid() ? status.toString() : "none", name ? QString::fromLatin1(name) : "UnknownNetworkError")
            .arg(int(error))
        + (reply->property("ndbcTlsCodes").isValid() ? " tls_codes=" + reply->property("ndbcTlsCodes").toString() : QString());
}
QString failureCause(QNetworkReply *reply) {
    if (reply->property("weatherTimeout").toBool() || reply->error() == QNetworkReply::TimeoutError) return "timeout";
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status && (status < 200 || status >= 300)) return "http-error";
    switch (reply->error()) {
    case QNetworkReply::HostNotFoundError: return "dns-error";
    case QNetworkReply::ConnectionRefusedError: return "connection-refused";
    case QNetworkReply::RemoteHostClosedError: return "connection-closed";
    case QNetworkReply::SslHandshakeFailedError: return "tls-error";
    case QNetworkReply::OperationCanceledError: return "request-aborted";
    default: return "network-error";
    }
}
QString cancellationName(NationalDataBuoyCenter::Cancellation reason) {
    using C = NationalDataBuoyCenter::Cancellation;
    switch (reason) {
    case C::Superseded: return "superseded";
    case C::CoordinatesChanged: return "coordinates-changed";
    case C::SourceChanged: return "source-changed";
    case C::ModeChanged: return "mode-changed";
    case C::Destroyed: return "destroyed";
    default: return "explicit";
    }
}
}
NationalDataBuoyCenter::NationalDataBuoyCenter(QObject *p, QUrl stations, QUrl data, QNetworkAccessManager *manager)
    : QObject(p), networkManager(manager ? manager : new QNetworkAccessManager(this)), stationsUrl(stations), dataUrl(data) {
    qRegisterMetaType<WeatherObservation>();
}
QString NationalDataBuoyCenter::diagnosticContext(const char *stage, const QString &station) const {
    return context(requestId, QString::fromLatin1(stage), station.isEmpty() ? lastStation : station);
}
void NationalDataBuoyCenter::cancel(Cancellation reason) {
    auto reply = active;
    active.clear();
    if (reply) {
        qCInfo(ndbcLog).noquote() << replyContext(reply) << "result=cancelled reason=" + cancellationName(reason);
        reply->abort();
    }
}
QNetworkReply *NationalDataBuoyCenter::request(QUrl url, const char *stage, const QString &station) {
    cancel(Cancellation::Superseded);
    QNetworkRequest r(url);
    r.setTransferTimeout(15000);
    r.setRawHeader("User-Agent", "RayNmeaSimulator Weather");
    auto *reply = networkManager->get(r);
    active = reply;
    requestId = ++nextRequestId; lastStation = station;
    reply->setProperty("ndbcId", QVariant::fromValue(requestId));
    reply->setProperty("ndbcStage", QString::fromLatin1(stage));
    reply->setProperty("ndbcStation", station);
    reply->setProperty("ndbcStarted", monotonicMilliseconds());
    qCDebug(ndbcLog).noquote() << diagnosticContext(stage, station) << "result=started";
    connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError> &errors) {
        QStringList codes; for (const auto &error : errors) codes << QString::number(int(error.error()));
        reply->setProperty("ndbcTlsCodes", codes.join(',')); // Never log certificates or error strings.
    });
    QTimer::singleShot(15000, reply, [reply] {
        if (!reply->isFinished()) {
            reply->setProperty("weatherTimeout", true);
            reply->abort();
        }
    });
    return reply;
}

double NationalDataBuoyCenter::distance(double a, double b, double c, double d) {
    double dl = qDegreesToRadians(c - a), dn = qDegreesToRadians(d - b);
    double h = std::pow(std::sin(dl / 2), 2) + std::cos(qDegreesToRadians(a)) *
                                                   std::cos(qDegreesToRadians(c)) *
                                                   std::pow(std::sin(dn / 2), 2);
    return 6371 * 2 * std::asin(std::sqrt(qBound(0., h, 1.)));
}
bool NationalDataBuoyCenter::parseStationsXml(const QByteArray &bytes, QList<Station> &stations,
                                              QString &error) {
    stations.clear();
    QXmlStreamReader xml(bytes);
    bool root = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (!root) {
                if (xml.name() != QLatin1String("stations")) {
                    error = "Unexpected NOAA station XML root.";
                    return false;
                }
                root = true;
            }
            if (xml.name() != QLatin1String("station"))
                continue;
            auto a = xml.attributes();
            bool latOk = false, lonOk = false;
            Station s;
            const auto met = a.value("met");
            s.standardMet = met == QLatin1String("y") ? 1 : met == QLatin1String("n") ? 0 : -1;
            s.id = a.value("id").toString();
            s.name = a.value("name").toString();
            s.latitude = a.value("lat").toDouble(&latOk);
            s.longitude = a.value("lon").toDouble(&lonOk);
            if (!latOk || !lonOk || !std::isfinite(s.latitude) || !std::isfinite(s.longitude) ||
                qAbs(s.latitude) > 90 || qAbs(s.longitude) > 180 ||
                !QRegularExpression("^[A-Za-z0-9]{3,10}$").match(s.id).hasMatch())
                continue;
            if (s.name.isEmpty())
                s.name = s.id;
            stations << s;
        }
    }
    if (xml.hasError() || !root) {
        stations.clear();
        error = "Malformed NOAA station XML.";
        return false;
    }
    return true;
}
bool NationalDataBuoyCenter::parse(const QByteArray &bytes, WeatherObservation &o, QString &error) {
    o = WeatherObservation{};
    o.provider = "NOAA NDBC";
    o.maximumAgeSeconds = 10800;
    o.retrieved = QDateTime::currentDateTimeUtc();
    const QStringList expected =
        QString("#YY MM DD hh mm WDIR WSPD GST WVHT DPD APD MWD PRES ATMP WTMP DEWP VIS PTDY TIDE")
            .split(' ');
    bool header = false;
    QStringList f;
    for (auto line : QString::fromUtf8(bytes).split('\n')) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        auto tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (line.startsWith("#YY")) {
            header = tokens.size() >= 18 && tokens.size() <= 19;
            for (int i = 0; header && i < tokens.size(); ++i)
                header = tokens[i] == expected[i];
            continue;
        }
        if (line.startsWith('#'))
            continue;
        f = tokens;
        break;
    }
    if (!header || f.size() < 18 || f.size() > 19) {
        error = "NOAA standard meteorological header/field count is invalid.";
        return false;
    }
    QList<int> date;
    for (int i = 0; i < 5; ++i) {
        bool ok = false;
        int value = f[i].toInt(&ok);
        if (!ok) {
            error = "Invalid NOAA observation time.";
            return false;
        }
        date << value;
    }
    o.observed = QDateTime(QDate(date[0], date[1], date[2]), QTime(date[3], date[4]), Qt::UTC);
    if (!o.observed.isValid()) {
        error = "Invalid NOAA observation date/time.";
        return false;
    }
    auto read = [&](int i, double low, double high) {
        QString key = expected[i];
        if (i >= f.size() || f[i] == "MM") {
            o.missing << key;
            return N2kDoubleNA;
        }
        bool ok = false;
        double v = f[i].toDouble(&ok);
        if (!ok || !std::isfinite(v) || v < low || v > high) {
            o.invalid << key;
            return N2kDoubleNA;
        }
        return v;
    };
    o.windAngle = read(5, 0, 360);
    if (WeatherObservation::valid(o.windAngle))
        o.windAngle = qDegreesToRadians(o.windAngle);
    o.windSpeed = read(6, 0, 200);
    o.windGust = read(7, 0, 200);
    o.waveHeight = read(8, 0, 100);
    o.dominantWavePeriod = read(9, 0, 1000);
    o.averageWavePeriod = read(10, 0, 1000);
    o.meanWaveDirection = read(11, 0, 360);
    if (WeatherObservation::valid(o.meanWaveDirection))
        o.meanWaveDirection = qDegreesToRadians(o.meanWaveDirection);
    o.visibility = read(16, 0, 10000);
    if (WeatherObservation::valid(o.visibility)) o.visibility *= 1852.; // nautical miles -> m
    o.pressureTendency = read(17, -200, 200);
    if (WeatherObservation::valid(o.pressureTendency)) o.pressureTendency *= 100.; // hPa/3 h -> Pa/3 h
    o.tide = read(18, -100, 100);
    if (WeatherObservation::valid(o.tide)) o.tide *= 0.3048; // feet above MLLW -> m

    o.pressure = read(12, 1, 2000);
    if (WeatherObservation::valid(o.pressure))
        o.pressure *= 100;
    double air = read(13, -100, 100), sea = read(14, -100, 100), dew = read(15, -100, 100);
    if (WeatherObservation::valid(air))
        o.air = air + 273.15;
    if (WeatherObservation::valid(dew)) o.dewPoint = dew + 273.15;
    if (WeatherObservation::valid(sea))
        o.sea = sea + 273.15;
    if (WeatherObservation::valid(air) && WeatherObservation::valid(dew) && dew <= air) {
        o.humidity =
            qBound(0., 100 * std::exp(17.625 * dew / (243.04 + dew) - 17.625 * air / (243.04 + air)), 100.);
    } else
        o.missing << "relative humidity (derived)";
    QStringList lines;
    for (int i = 5; i < f.size(); ++i)
        lines << expected[i] + ": " + (f[i] == "MM" ? "Unavailable" : f[i]);
    o.description = "NOAA observation UTC: " + o.observed.toString(Qt::ISODate) +
                    "\nUnits: wind m/s, direction deg true, waves m, periods s, pressure hPa, temperatures "
                    "C, visibility nmi, tide ft\n" +
                    lines.join('\n');
    if (!o.any()) {
        error = "NOAA returned no usable measurements.";
        return false;
    }
    return true;
}
void NationalDataBuoyCenter::getNearestStations(double latitude, double longitude, double radius) {
    cancel(Cancellation::Superseded);
    if (!std::isfinite(latitude) || !std::isfinite(longitude) || qAbs(latitude) > 90 ||
        qAbs(longitude) > 180 || !std::isfinite(radius) || radius <= 0) {
        qCWarning(ndbcLog).noquote() << context(0, "discovery", {}) << "result=invalid-search-input";
        emit errorOccurred("Invalid NOAA station search coordinates/radius.");
        return;
    }
    auto *reply = request(stationsUrl, "discovery");
    connect(reply, &QNetworkReply::finished, this, [this, reply, latitude, longitude, radius] {
        reply->deleteLater();
        if (active != reply) {
            qCDebug(ndbcLog).noquote() << replyContext(reply) << "result=ignored-inactive";
            return;
        }
        active.clear();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(ndbcLog).noquote() << replyContext(reply) << "result=" + failureCause(reply);
            emit errorOccurred("NOAA station lookup failed or timed out; retry Fetch Weather.");
            return;
        }
        QList<Station> all, nearby;
        QString error;
        if (!parseStationsXml(reply->readAll(), all, error)) {
            qCWarning(ndbcLog).noquote() << replyContext(reply) << "result=parse-error detail=invalid-station-xml";
            emit errorOccurred(error);
            return;
        }
        for (auto s : all)
            if (distance(latitude, longitude, s.latitude, s.longitude) <= radius)
                nearby << s;
        std::sort(nearby.begin(), nearby.end(), [=](const Station &a, const Station &b) {
            return distance(latitude, longitude, a.latitude, a.longitude) <
                   distance(latitude, longitude, b.latitude, b.longitude);
        });
        qCDebug(ndbcLog).noquote() << replyContext(reply) << "result=completed stations=" + QString::number(all.size())
                                  << "nearby=" + QString::number(nearby.size());
        emit nearestStationsFound(nearby);
    });
}
void NationalDataBuoyCenter::fetchStationData(const QString &station) {
    feedUnavailable = false;
    cancel(Cancellation::Superseded);
    if (!QRegularExpression("^[A-Za-z0-9]{3,10}$").match(station).hasMatch()) {
        qCWarning(ndbcLog).noquote() << context(0, "selection", station) << "result=invalid-station-id";
        emit errorOccurred("Invalid NOAA station identifier.");
        return;
    }
    auto *reply = request(dataUrl.resolved(QUrl(station.toLower() + ".txt")), "observation", station);
    connect(reply, &QNetworkReply::finished, this, [this, reply, station] {
        reply->deleteLater();
        if (active != reply) {
            qCDebug(ndbcLog).noquote() << replyContext(reply) << "result=ignored-inactive";
            return;
        }
        active.clear();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(ndbcLog).noquote() << replyContext(reply) << "result=" + failureCause(reply);
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            feedUnavailable = status == 404 || status == 410 || status >= 500;
            emit errorOccurred("NOAA observation request failed or timed out; retry Fetch Weather.");
            return;
        }
        WeatherObservation o;
        QString error;
        if (!parse(reply->readAll(), o, error)) {
            feedUnavailable = true;
            const QString cause = error == "NOAA returned no usable measurements." ? "no-usable-observations" : "parse-error";
            qCWarning(ndbcLog).noquote() << replyContext(reply) << "result=" + cause
                << (cause == "parse-error" ? "detail=invalid-standard-met" : "detail=no-valid-measurements");
            emit errorOccurred(error);
            return;
        }
        if (!o.invalid.isEmpty()) qCWarning(ndbcLog).noquote() << replyContext(reply)
            << "result=partial-invalid invalid_fields=" + QString::number(o.invalid.size());
        const auto age = o.observed.secsTo(QDateTime::currentDateTimeUtc());
        if (!o.fresh()) qCWarning(ndbcLog).noquote() << replyContext(reply) << "result=stale-observation age_s=" + QString::number(age);
        else qCDebug(ndbcLog).noquote() << replyContext(reply) << "result=completed age_s=" + QString::number(age)
            << "missing_fields=" + QString::number(o.missing.size()) << "invalid_fields=" + QString::number(o.invalid.size());
        o.station = station;
        o.location = station;
        emit observationFetched(o);
        emit dataFetched(o.description, o.air, o.sea, o.waveHeight, o.windSpeed, o.windAngle,
                         WeatherObservation::valid(o.pressure) ? o.pressure / 100 : N2kDoubleNA, o.humidity);
    });
}
