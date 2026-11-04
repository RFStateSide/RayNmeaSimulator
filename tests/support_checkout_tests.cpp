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

#include <QtTest>
#include <QApplication>
#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include "supportcheckout.h"
#include "aboutlinks.h"
#include "support_fixture.h"
class SupportCheckoutTests : public QObject {
    Q_OBJECT
private slots:
    void selection_data() {
        QTest::addColumn<int>("status"); QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("error"); QTest::addColumn<bool>("valid");
        QTest::newRow("valid")<<200<<QByteArray("rfss-checkout-v1")<<0<<true;
        for (int status : {204,301,302,307,404,429,500})
            QTest::newRow(qPrintable(QString::number(status)))<<status<<QByteArray("rfss-checkout-v1")<<0<<false;
        for (auto body : {QByteArray(),QByteArray("wrong"),QByteArray("rfss-checkout-v1\n"),QByteArray(" rfss-checkout-v1"),QByteArray("rfss-checkout-v1\0",17),QByteArray(4096,'x')})
            QTest::newRow(qPrintable("body-"+QString::number(body.size())+QString::fromLatin1(body.left(2))))<<200<<body<<0<<false;
        for (int error : {int(QNetworkReply::HostNotFoundError),int(QNetworkReply::SslHandshakeFailedError),int(QNetworkReply::ConnectionRefusedError),int(QNetworkReply::RemoteHostClosedError)})
            QTest::newRow(qPrintable("error-"+QString::number(error)))<<200<<QByteArray("rfss-checkout-v1")<<error<<false;
    }
    void selection() {
        QFETCH(int,status); QFETCH(QByteArray,body); QFETCH(int,error); QFETCH(bool,valid);
        SupportTransport net; net.status=status; net.body=body; net.failure=QNetworkReply::NetworkError(error);
        QWidget owner; QList<QUrl> opened;
        SupportCheckout controller(nullptr,&net,100,[&](QWidget *,const QUrl &url){opened<<url;});
        controller.start(&owner); controller.start(&owner);
        QTRY_COMPARE(opened.size(),1);
        QCOMPARE(opened[0],valid?SupportCheckout::checkoutUrl():QUrl(AboutLinks::SupportUrl));
        QVERIFY(!controller.busy()); QCOMPARE(net.calls,1);
        controller.start(&owner); QTest::qWait(110); QCOMPARE(opened.size(),1);
        QCOMPARE(net.operation,QNetworkAccessManager::GetOperation);
        QCOMPARE(net.seen.url(),SupportCheckout::availabilityUrl());
        QVERIFY(net.seen.url().query().isEmpty()); QVERIFY(net.seen.url().userInfo().isEmpty());
        QCOMPARE(net.seen.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),int(QNetworkRequest::ManualRedirectPolicy));
        QCOMPARE(net.seen.attribute(QNetworkRequest::CookieLoadControlAttribute).toInt(),int(QNetworkRequest::Manual));
        QCOMPARE(net.seen.attribute(QNetworkRequest::CookieSaveControlAttribute).toInt(),int(QNetworkRequest::Manual));
        QCOMPARE(net.seen.attribute(QNetworkRequest::AuthenticationReuseAttribute).toInt(),int(QNetworkRequest::Manual));
        QVERIFY(!net.seen.hasRawHeader("Authorization")); QVERIFY(!net.seen.hasRawHeader("Cookie"));
    }
    void timeoutRedirectAndDeclaredOversize() {
        for (int mode=0;mode<3;++mode) {
            SupportTransport net; if(mode==0)net.delay=-1; if(mode==1)net.redirected=true; if(mode==2)net.advertised=1024*1024;
            QWidget owner; QList<QUrl> opened;
            SupportCheckout controller(nullptr,&net,20,[&](QWidget *,const QUrl &url){opened<<url;});
            controller.start(&owner); QTRY_COMPARE(opened.size(),1);
            QCOMPARE(opened[0],QUrl(AboutLinks::SupportUrl)); QTest::qWait(30); QCOMPARE(opened.size(),1);
        }
    }
    void cancellation() {
        for (int mode=0;mode<5;++mode) {
            SupportTransport net; net.delay=60; int opens=0;
            SupportCheckout controller(nullptr,&net,100,[&](QWidget *,const QUrl &){++opens;});
            auto *owner=new QWidget; owner->show(); controller.start(owner);
            if(mode==0)controller.cancel();
            if(mode==1)owner->close();
            if(mode==2){delete owner;owner=nullptr;}
            if(mode==3)owner->hide();
            if(mode==4)QMetaObject::invokeMethod(qApp,"aboutToQuit",Qt::DirectConnection);
            QVERIFY(!controller.busy()); QTest::qWait(120); QCOMPARE(opens,0); delete owner;
        }
        SupportTransport net; net.delay=60; int opens=0; QWidget owner;
        auto *controller=new SupportCheckout(nullptr,&net,100,[&](QWidget *,const QUrl &){++opens;});
        controller->start(&owner); delete controller; QTest::qWait(120); QCOMPARE(opens,0);
    }
    void handoffReentrancyAndLaterAction() {
        SupportTransport net; QWidget owner; int opens=0;
        SupportCheckout *active=nullptr;
        SupportCheckout controller(nullptr,&net,100,[&](QWidget *,const QUrl &){++opens;active->start(&owner); /* browser failure must not retry */}); active=&controller;
        controller.start(&owner); QTRY_COMPARE(opens,1); QCOMPARE(net.calls,1);
        QTest::qWait(1050); controller.start(&owner); QTRY_COMPARE(opens,2); QCOMPARE(net.calls,2);
    }
    void browserFailureNeverOpensFallback() {
        SupportTransport net; QWidget owner; int calls=0; QUrl selected; bool warningSeen=false;
        SupportCheckout controller(nullptr,&net,100,[&](QWidget *parent,const QUrl &url){
            selected=url;
            QTimer::singleShot(10,[&]{
                auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                warningSeen=box && box->text().contains(SupportCheckout::checkoutUrl().toString());
                if(box)box->accept();
            });
            AboutLinks::open(parent,url,+[](const QUrl &){return false;}); ++calls;
        });
        controller.start(&owner);QTRY_COMPARE(calls,1);QVERIFY(warningSeen);
        QCOMPARE(selected,SupportCheckout::checkoutUrl());QTest::qWait(150);QCOMPARE(calls,1);QCOMPARE(net.calls,1);
    }
    void realHttpRedirectIsNotFollowed() {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost)); int hits=0;
        connect(&server,&QTcpServer::newConnection,this,[&]{
            auto *socket=server.nextPendingConnection();++hits;
            connect(socket,&QTcpSocket::readyRead,socket,[socket]{socket->readAll();socket->write("HTTP/1.1 302 Found\r\nLocation: /second\r\nContent-Length: 16\r\nConnection: close\r\n\r\nrfss-checkout-v1");socket->disconnectFromHost();});
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        class Loopback : public QNetworkAccessManager {
        public: QUrl local;
        protected: QNetworkReply *createRequest(Operation op,const QNetworkRequest &request,QIODevice *data) override {
            auto fixture=request;fixture.setUrl(local);return QNetworkAccessManager::createRequest(op,fixture,data);
        }} net;
        net.local=QUrl(QString("http://127.0.0.1:%1/availability").arg(server.serverPort()));
        QWidget owner; QList<QUrl> opened;
        SupportCheckout controller(nullptr,&net,1000,[&](QWidget *,const QUrl &url){opened<<url;});
        controller.start(&owner); QTRY_COMPARE(opened.size(),1); QCOMPARE(opened[0],QUrl(AboutLinks::SupportUrl));
        QTest::qWait(50); QCOMPARE(hits,1); QCOMPARE(opened.size(),1);
    }
};
QTEST_MAIN(SupportCheckoutTests)
#include "support_checkout_tests.moc"
