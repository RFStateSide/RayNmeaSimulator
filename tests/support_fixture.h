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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <cstring>
class SupportReply final : public QNetworkReply {
public:
    QByteArray payload;
    qint64 offset = 0;
    bool aborted = false;
    SupportReply(const QNetworkRequest &request, QObject *parent, int status, QByteArray body,
                 NetworkError failure, int delay, bool redirected, qint64 advertised)
        : QNetworkReply(parent), payload(std::move(body)) {
        setRequest(request); setUrl(request.url()); setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (redirected) setAttribute(QNetworkRequest::RedirectionTargetAttribute, QUrl("https://other.invalid/"));
        if (advertised >= 0) setHeader(QNetworkRequest::ContentLengthHeader, advertised);
        open(QIODevice::ReadOnly);
        if (delay >= 0) QTimer::singleShot(delay, this, [this, failure] {
            if (aborted) return;
            emit metaDataChanged();
            if (aborted) return;
            if (failure != NoError) setError(failure, "controlled failure");
            emit readyRead();
            if (aborted) return;
            setFinished(true); emit finished();
        });
    }
    void abort() override { if (!aborted) { aborted=true; setError(OperationCanceledError,"cancelled"); setFinished(true); emit finished(); } }
    qint64 bytesAvailable() const override { return payload.size()-offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 max) override {
        const auto n=qMin(max,qint64(payload.size()-offset));
        if (!n) return -1;
        memcpy(data,payload.constData()+offset,size_t(n)); offset+=n; return n;
    }
};
class SupportTransport final : public QNetworkAccessManager {
public:
    int status=200, delay=1, calls=0;
    qint64 advertised=-1;
    QByteArray body="rfss-checkout-v1";
    QNetworkReply::NetworkError failure=QNetworkReply::NoError;
    bool redirected=false;
    QNetworkRequest seen;
    Operation operation=UnknownOperation;
    QPointer<SupportReply> last;
protected:
    QNetworkReply *createRequest(Operation op,const QNetworkRequest &request,QIODevice *out) override {
        Q_ASSERT(!out); ++calls; seen=request; operation=op;
        return last=new SupportReply(request,this,status,body,failure,delay,redirected,advertised);
    }
};
