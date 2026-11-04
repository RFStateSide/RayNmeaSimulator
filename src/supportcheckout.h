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
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QUrl>
#include <functional>
class QNetworkAccessManager;
class QNetworkReply;
class QWidget;

// One shared controller coordinates Help and About, without sharing updater state.
class SupportCheckout final : public QObject {
    Q_OBJECT
public:
    using Browser = std::function<void(QWidget *, const QUrl &)>;
    explicit SupportCheckout(QObject *parent = nullptr, QNetworkAccessManager *transport = nullptr,
                             int timeoutMs = 2500, Browser browser = {});
    ~SupportCheckout() override;
    static SupportCheckout *instance();
#ifdef RAYNMEA_TESTING
    static void setTestInstance(SupportCheckout *controller);
#endif
    void start(QWidget *origin);
    void cancel();
    bool busy() const { return !reply.isNull(); }
    static QUrl availabilityUrl();
    static QUrl checkoutUrl();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void finish(bool available);
    bool consume();
    QNetworkAccessManager *network;
    QPointer<QNetworkReply> reply;
    QPointer<QWidget> owner;
    QMetaObject::Connection ownerDestroyed;
    QTimer deadline;
    QElapsedTimer handoff;
    QByteArray body;
    int timeout;
    Browser browser;
};
