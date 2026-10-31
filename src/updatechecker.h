// Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
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
    bool metadata=false;
    QByteArray buffer;
    UpdateResult result;
};
