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

#include "supportcheckout.h"
#include "aboutlinks.h"
#include <QApplication>
#include <QEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWidget>
namespace {
constexpr qint64 maxBody = 64;
#ifdef RAYNMEA_TESTING
QPointer<SupportCheckout> testInstance;
#endif
}
QUrl SupportCheckout::availabilityUrl() { return QUrl("https://rfstateside.com/support/checkout/raynmeasimulator/availability"); }
QUrl SupportCheckout::checkoutUrl() { return QUrl("https://rfstateside.com/support/checkout/raynmeasimulator"); }
SupportCheckout::SupportCheckout(QObject *parent, QNetworkAccessManager *transport, int timeoutMs, Browser opener)
    : QObject(parent), network(transport ? transport : new QNetworkAccessManager(this)),
      timeout(timeoutMs), browser(opener ? std::move(opener) : Browser([](QWidget *parent, const QUrl &url) { AboutLinks::open(parent, url); })) {
    deadline.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, this, [this] { finish(false); });
    connect(qApp, &QCoreApplication::aboutToQuit, this, &SupportCheckout::cancel);
}
SupportCheckout::~SupportCheckout() { cancel(); }
SupportCheckout *SupportCheckout::instance() {
#ifdef RAYNMEA_TESTING
    if (testInstance) return testInstance;
#endif
    static QPointer<SupportCheckout> shared;
    if (!shared) shared = new SupportCheckout(qApp);
    return shared;
}
#ifdef RAYNMEA_TESTING
void SupportCheckout::setTestInstance(SupportCheckout *controller) { testInstance = controller; }
#endif
void SupportCheckout::cancel() {
    deadline.stop();
    if (owner) owner->removeEventFilter(this);
    disconnect(ownerDestroyed);
    owner.clear();
    if (reply) {
        auto *p = reply.data(); reply.clear();
        p->disconnect(this); p->abort(); p->deleteLater();
    }
    body.clear();
}
bool SupportCheckout::eventFilter(QObject *watched, QEvent *event) {
    if (watched == owner && (event->type() == QEvent::Close || event->type() == QEvent::Hide)) cancel();
    return QObject::eventFilter(watched, event);
}
void SupportCheckout::start(QWidget *origin) {
    // Coalesce overlap and double-clicks, including reentrancy during browser handoff.
    if (!origin || busy() || (handoff.isValid() && handoff.elapsed() < 1000)) return;
    owner = origin->window();
    owner->installEventFilter(this);
    ownerDestroyed = connect(owner, &QObject::destroyed, this, &SupportCheckout::cancel);
    body.clear();
    QNetworkRequest request(availabilityUrl());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Accept", "text/plain");
    request.setRawHeader("Accept-Encoding", "identity");
    request.setTransferTimeout(timeout);
    reply = network->get(request);
    reply->setReadBufferSize(maxBody + 1);
    auto *p = reply.data();
    connect(p, &QIODevice::readyRead, this, [this, p] {
        if (reply == p && !consume()) finish(false);
    });
    connect(p, &QNetworkReply::metaDataChanged, this, [this, p] {
        if (reply == p && p->header(QNetworkRequest::ContentLengthHeader).toLongLong() > maxBody) finish(false);
    });
    connect(p, &QNetworkReply::finished, this, [this, p] {
        if (reply != p) return;
        const bool bounded = consume();
        finish(bounded && p->error() == QNetworkReply::NoError
               && p->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
               && !p->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()
               && body == "rfss-checkout-v1");
    });
    deadline.start(timeout);
}
bool SupportCheckout::consume() {
    // Read at most one byte beyond the cap, even if a backend ignores its read buffer.
    body += reply->read(maxBody + 1 - body.size());
    return body.size() <= maxBody && reply->bytesAvailable() == 0;
}
void SupportCheckout::finish(bool available) {
    if (!reply) return;
    QPointer<QWidget> parent = owner;
    handoff.start();
    cancel(); // Disconnect/abort before external code can reenter or destroy the origin.
    if (parent) browser(parent, available ? checkoutUrl() : QUrl(QString::fromLatin1(AboutLinks::SupportUrl)));
    // A failed browser handoff never launches a second checkout.
}
