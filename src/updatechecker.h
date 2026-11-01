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
#include <QObject>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <optional>
class QNetworkAccessManager;
class QNetworkReply;
struct UpdateHost {
    QString os, architecture, distribution, distributionVersion;
    QJsonObject abi;
    static UpdateHost current();
    QString platformKey() const;
};
struct UpdateResult {
    enum State { Unavailable, Current, Newer, Older, Incompatible, Error } state=Error;
    QString installed, latest, message, notes, tag, packageName, checksum;
    QUrl releaseUrl;
    QJsonObject release;
};
Q_DECLARE_METATYPE(UpdateResult)
class UpdateChecker final: public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent=nullptr, QNetworkAccessManager *network=nullptr, int timeoutMs=15000);
    ~UpdateChecker() override;
    void check();
    void cancel();
    bool busy() const { return !reply.isNull(); }
    static QUrl endpoint();
    static QUrl releaseEndpoint();
    static UpdateResult parseManifest(const QByteArray &,const QString &installed);
    static std::optional<int> compareVersions(const QString &a,const QString &b);
    static UpdateResult parseRelease(const QByteArray &,const QString &installed);
    static UpdateResult selectPackage(UpdateResult,const QByteArray &,const UpdateHost &);
    static bool officialPage(const QUrl &,const QString &tag);
    static bool officialAsset(const QUrl &,const QString &tag,const QString &name);
signals:
    void completed(const UpdateResult &result);
private:
    void request(const QUrl &url);
    void finish(UpdateResult result);
    QNetworkAccessManager *network;
    QPointer<QNetworkReply> reply;
    QTimer deadline;
    int timeoutMs;
    int redirects=0;
    bool fetchingRelease=false;
    QByteArray manifestData;
    QByteArray buffer;
    UpdateResult result;
};
