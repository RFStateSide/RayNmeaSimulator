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
#include <QLabel>
#include <QTcpServer>
#include <QMessageAuthenticationCode>
#include <QNetworkProxy>
#include <QAction>
#include <QScopeGuard>
#include <QInputDialog>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QTabWidget>
#include <QMessageBox>
#include "chartplotterroutedialog.h"
#include "generatorvesselmovement.h"
#include "networkcansession.h"
#include "nmea2000handler.h"
#include "N2kMessages.h"
#include "mainwindow.h"
#include "appsettings.h"
#include "credentialstore.h"
#include "chartplotterrouteclient.h"
#include "route_service_fixture.h"
#include "routeservicepolicy.h"
class NetworkCanTests : public QObject {
    Q_OBJECT
    NetworkCanConfig config(int identity = 101) {
        NetworkCanConfig c;
        c.interfaceName = "lab";
        c.host = "127.0.0.1";
        c.port = qEnvironmentVariableIntValue("RAY_NETWORK_TEST_PORT");
        c.uniqueNumber = identity;
        c.preferredSource = 45;
        c.allowTx = true;
        QFile f("build/m239b8/bridge-key-private.txt");
        if (f.open(QIODevice::ReadOnly))
            c.token = QString::fromUtf8(f.readAll()).trimmed();
        return c;
    }
    int count(const QSignalSpy &spy, int p) {
        int n = 0;
        for (const auto &args : spy)
            if (qvariant_cast<tN2kMsg>(args[0]).PGN == quint32(p))
                ++n;
        return n;
    }
  private slots:
    void rxOnlyAndAuthentication() {
        NetworkCanSession rx;
        auto c = config();
        c.allowTx = false;
        rx.configure(c);
        QTRY_VERIFY(rx.connected());
        tN2kMsg m;
        SetN2kLatLonRapid(m, 10, 20);
        QVERIFY(!rx.send(m));
        QVERIFY(!rx.start("tx"));
        c.allowTx = true;
        c.token = "incorrect-test-key";
        rx.configure(c);
        QSignalSpy failed(&rx, &NetworkCanSession::failed);
        QVERIFY(rx.start("tx"));
        QTRY_VERIFY(failed.size() > 0);
        QVERIFY(!rx.connected());
        QVERIFY(!rx.send(m));
    }
    void independentIdentitiesFastPacketAndStop() {
        NetworkCanSession a, b;
        auto ac = config(101), bc = config(202);
        a.configure(ac);
        b.configure(bc);
        QTRY_VERIFY(a.connected() && b.connected());
        QSignalSpy ar(&a, &NetworkCanSession::received),
            br(&b, &NetworkCanSession::received);
        QVERIFY(a.start("tx"));
        QVERIFY(b.start("tx"));
        QTRY_VERIFY_WITH_TIMEOUT(a.transmitting() && b.transmitting(), 10000);
        QVERIFY(a.sourceAddress() != b.sourceAddress());
        tN2kMsg position;
        SetN2kLatLonRapid(position, 10.1234567, 20.1234567);
        QVERIFY(a.send(position));
        QTRY_COMPARE(count(br, 129025), 1);
        QCOMPARE(count(ar, 129025), 0);
        tN2kMsg packet;
        packet.SetPGN(130067);
        for (int i = 0; i < 79; ++i)
            packet.AddByte(i);
        QVERIFY(a.send(packet));
        QTRY_COMPARE(count(br, 130067), 1);
        for (const auto &args : br) {
            auto m = qvariant_cast<tN2kMsg>(args[0]);
            if (m.PGN == 130067) {
                QCOMPARE(m.DataLen, 79);
                QCOMPARE(QByteArray(reinterpret_cast<const char *>(m.Data), 79),
                         QByteArray(reinterpret_cast<const char *>(packet.Data),
                                    79));
            }
        }
        a.stop(true);
        QTRY_VERIFY(a.connected());
        QVERIFY(!a.send(position));
        int before = count(br, 129025);
        QTest::qWait(150);
        QCOMPARE(count(br, 129025), before);
        QVERIFY(b.send(position));
        QTRY_COMPARE(count(ar, 129025), 1);
    }
    void duplicateIdentityRevokesActiveSender() {
        NetworkCanSession a, b;
        const auto c = config(505);
        a.configure(c);
        QVERIFY(a.start("tx"));
        QTRY_VERIFY_WITH_TIMEOUT(a.transmitting(), 10000);
        QSignalSpy failure(&a, &NetworkCanSession::failed);
        b.configure(c);
        QVERIFY(b.start("tx"));
        QTRY_VERIFY_WITH_TIMEOUT(failure.size() > 0, 10000);
        QVERIFY(!a.transmitting());
        tN2kMsg m;
        SetN2kLatLonRapid(m, 10, 20);
        QVERIFY(!a.send(m));
        b.stop();
    }
    void serviceGateAndModeChange() {
        Nmea2000Handler h;
        h.SetInterfaceType("Network NMEA2000");
        h.setMode(Viewer);
        h.networkCan()->configure(config(303));
        QTRY_VERIFY(h.networkCan()->connected());
        QVERIFY(h.beginRouteService());
        QTRY_VERIFY_WITH_TIMEOUT(h.routeServiceReady(), 10000);
        tN2kMsg m;
        m.SetPGN(59904);
        m.Destination = 1;
        m.Add3ByteInt(60928);
        QVERIFY(h.sendRouteService(m));
        double lat = 10, lon = 20;
        QVERIFY(!h.SendPgn129025(lat, lon));
        tN2kMsg bad;
        SetN2kLatLonRapid(bad, lat, lon);
        QVERIFY(!h.sendRouteService(bad));
        h.setMode(Sender);
        QVERIFY(!h.routeServiceActive());
        QTRY_VERIFY(h.networkCan()->connected());
        QVERIFY(!h.networkCan()->transmitting());
        QVERIFY(!h.sendRouteService(m));
    }


    void virtualRouteTransactionAndSelection() {

        // A scripted chartplotter peer uses the real local RN2K bridge. Using
        // an application node as the server would correctly NAK unsupported
        // route-serving PGNs before our fixture could answer them.
        QTcpSocket plotter;plotter.setProxy(QNetworkProxy::NoProxy);auto pc=config(990);
        QByteArray input;RouteFixture::RequestDecoder decoder;int consumed=0;quint8 sequence=0;bool online=false;
        QVector<tN2kMsg> requests;
        auto envelope=[&](const QJsonObject &j){auto bytes=QJsonDocument(j).toJson(QJsonDocument::Compact)+'\n';QCOMPARE(plotter.write(bytes),qint64(bytes.size()));};
        connect(&plotter,&QTcpSocket::readyRead,this,[&]{
            input+=plotter.readAll();
            while(input.contains('\n')) {
                int n=input.indexOf('\n');auto j=QJsonDocument::fromJson(input.left(n)).object();input.remove(0,n+1);
                const auto kind=j["kind"].toString();
                if(kind=="hello") {
                    auto material=(j["nonce"].toString()+'|'+j["client"].toString()+"|tx|lab").toUtf8();
                    auto proof=QMessageAuthenticationCode::hash(material,pc.token.toUtf8(),QCryptographicHash::Sha256).toHex();
                    envelope({{"kind","auth"},{"mode","tx"},{"interface","lab"},{"proof",QString::fromLatin1(proof)}});
                }else if(kind=="ready")online=true;
                else if(kind=="frame") {
                    decoder.receive(quint32(j["id"].toDouble())&0x1fffffff,QByteArray::fromHex(j["data"].toString().toLatin1()));
                    while(consumed<decoder.messages.size()) {
                        auto m=decoder.messages[consumed++];requests.append(m);auto reply=RouteFixture::response(m);
                        if(reply.PGN)for(const auto &f:RouteFixture::frames(reply,sequence++&7))
                            envelope({{"kind","frame"},{"id",double(f.id|0x80000000u)},{"dlc",f.data.size()},{"data",QString::fromLatin1(f.data.toHex())}});
                    }
                }
            }
        });
        QTimer ping;connect(&ping,&QTimer::timeout,this,[&]{if(online)envelope({{"kind","ping"}});});ping.start(1000);
        plotter.connectToHost(pc.host,pc.port);QTRY_VERIFY(online);
        Nmea2000Handler h;h.SetInterfaceType("Network NMEA2000");h.setMode(Viewer);h.networkCan()->configure(config(991));
        QTRY_VERIFY(h.networkCan()->connected());
        ChartplotterRouteDialog dialog(&h,{});dialog.show();auto *c=dialog.findChild<ChartplotterRouteClient*>();QVERIFY(c);
        dialog.findChild<QPushButton*>("routeServiceDiscover")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!c->devices().isEmpty()&&!c->devices().first().model.isEmpty(),10000);
        const auto device=c->devices().first();QCOMPARE(device.source,1);QCOMPARE(device.name,RouteFixture::name());
        c->advance(7000);auto *choice=dialog.findChild<QComboBox*>("routeServiceDevice");QCOMPARE(choice->currentIndex(),-1);
        choice->setCurrentIndex(0);dialog.findChild<QPushButton*>("routeServiceRefresh")->click();QTRY_COMPARE(c->entries().size(),1);
        dialog.findChild<QListWidget*>("routeServiceRoutes")->setCurrentRow(0);dialog.findChild<QPushButton*>("routeServiceRetrieve")->click();
        QTRY_COMPARE_WITH_TIMEOUT(c->state(),ChartplotterRouteClient::Complete,10000);
        dialog.findChild<QPushButton*>("routeServiceSelect")->click();QCOMPARE(dialog.result(),int(QDialog::Accepted));
        auto selected=dialog.selection();QVERIFY(selected.complete);QCOMPARE(selected.waypoints.size(),3);
        QCOMPARE(selected.waypoints[2].coordinate,QGeoCoordinate(10.02,20.02));
        QVERIFY(!h.networkCan()->transmitting());double lat=1,lon=2;QVERIFY(!h.SendPgn129025(lat,lon));
        QVERIFY(!requests.isEmpty());for(const auto &m:requests)QVERIFY(RouteServicePolicy::allowed(m));
        h.endRouteService(true);ping.stop();plotter.abort();
    }
    void routeFaultNotificationRevokesIdleLease() {
        Nmea2000Handler h;h.SetInterfaceType("Network NMEA2000");h.setMode(Viewer);
        h.networkCan()->configure(config(304));QTRY_VERIFY(h.networkCan()->connected());
        QVERIFY(h.beginRouteService());QTRY_VERIFY_WITH_TIMEOUT(h.routeServiceReady(),10000);
        QSignalSpy revoked(&h,&Nmea2000Handler::RouteServiceRevoked);
        // The session emits this same notification for disconnect, heartbeat,
        // bridge expiry and framing errors. No physical bridge is contacted.
        h.networkCan()->failed("Synthetic expired service lease");
        QVERIFY(!h.routeServiceActive());QCOMPARE(revoked.size(),1);
        QTRY_VERIFY(h.networkCan()->connected());QVERIFY(!h.networkCan()->transmitting());
        tN2kMsg request;request.SetPGN(59904);request.Destination=1;request.Add3ByteInt(60928);
        QVERIFY(!h.sendRouteService(request));QTest::qWait(40);QVERIFY(!h.routeServiceActive());
        // Only a new explicit operation may open a new service connection.
        QVERIFY(h.beginRouteService());QTRY_VERIFY_WITH_TIMEOUT(h.routeServiceReady(),10000);h.endRouteService(true);
    }
    void differentComputerVirtualBus() {
        if (qEnvironmentVariable("RAY_MULTI_COMPUTER") != "1")
            QSKIP("Explicit different-computer qualification only");
        auto c = config(1191748);
        c.host = qEnvironmentVariable("RAY_MULTI_COMPUTER_HOST").trimmed();
        QVERIFY2(!c.host.isEmpty(), "Set RAY_MULTI_COMPUTER_HOST explicitly for the authorized test endpoint");
        c.port = 29502;
        c.interfaceName = "lab";
        NetworkCanSession session;
        QSignalSpy rx(&session, &NetworkCanSession::received);
        session.configure(c);
        QTRY_VERIFY(session.connected());
        QVERIFY(session.start("tx"));
        QTRY_VERIFY_WITH_TIMEOUT(session.transmitting(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(count(rx, 129025) >= 5, 25000);
        QCOMPARE(count(rx, 129025), 5);
        for (int i = 0; i < 5; ++i) {
            tN2kMsg m;
            SetN2kLatLonRapid(m, 10.1234567, 20.1234567);
            QVERIFY(session.send(m));
            QTest::qWait(100);
        }
        QTest::qWait(250);
        QCOMPARE(count(rx, 129025), 5);
        session.stop();
    }
    void liveBoatViewerRouteAndControlledTx() {
        if (qEnvironmentVariable("RAY_NETWORK_LIVE") != "1")
            QSKIP("Explicit boat run only");
        auto c = config(1191747);
        c.host = qEnvironmentVariable("RAY_NETWORK_LIVE_HOST").trimmed();
        QVERIFY2(!c.host.isEmpty(), "Set RAY_NETWORK_LIVE_HOST explicitly for the authorized test endpoint");
        c.port = 29501;
        c.interfaceName = "can0";
        c.preferredSource =
            qEnvironmentVariableIntValue("RAY_NETWORK_LIVE_SOURCE");
        QFile identityFile(
            qEnvironmentVariable("RAY_NETWORK_IDENTITY_FIXTURE"));
        QVERIFY(identityFile.open(QIODevice::ReadOnly));
        auto old = QJsonDocument::fromJson(identityFile.readAll()).object();
        auto identity = old["identity"].toObject()["payload_hex"].toString();
        AppSettings s(APP_COMPANY, APP_NAME);
        s.clear();
        s.setValue("Nmea2000/Nmea2000Enabled", true);
        s.setValue("Nmea2000/InterfaceType", "Network NMEA2000");
        s.setValue("Nmea2000/NetworkCanHost", c.host);
        s.setValue("Nmea2000/NetworkCanPort", c.port);
        s.setValue("Nmea2000/NetworkCanInterface", c.interfaceName);
        s.setValue("Nmea2000/NetworkCanAllowTx", true);
        s.setValue("Nmea2000/NetworkCanUniqueNumber", c.uniqueNumber);
        s.setValue("Nmea2000/NetworkCanPreferredSource", c.preferredSource);
        s.setValue("Nmea0183/Nmea0183Enabled", false);
        s.sync();
        QVERIFY(CredentialStore::write("NetworkNmea2000Token", c.token));
        auto credentialCleanup = qScopeGuard(
            [] { CredentialStore::remove("NetworkNmea2000Token"); });
        {
            MainWindow w;
            w.show();
            auto *h = w.findChild<Nmea2000Handler *>();
            auto *worker = w.findChild<NmeaSenderWorker *>();
            auto *controller = w.findChild<MainController *>();
            connect(h->networkCan(), &NetworkCanSession::failed,
                    [](const QString &m) { qWarning().noquote() << m; });
            QTRY_VERIFY2(h->networkCan()->connected(),
                         qPrintable(h->networkCan()->status()));
            QVERIFY(!worker->isSending());
            GeneratorVesselMovement movement(
                700001, "Private network qualification", &w);
            movement.setActiveRouteChooser(controller->activeRouteChooser);
            ReceivedRoutes::Route selected;
            int stage = 0;
            QElapsedTimer deadline;
            deadline.start();
            QTimer drive;
            connect(&drive, &QTimer::timeout, [&] {
                for (auto *top : QApplication::topLevelWidgets()) {
                    if (auto *source = qobject_cast<QInputDialog *>(top);
                        source && source->isVisible()) {
                        source->setTextValue("Select Chartplotter Route");
                        source->accept();
                    }
                    if (auto *dialog =
                            qobject_cast<ChartplotterRouteDialog *>(top);
                        dialog && dialog->isVisible()) {
                        auto *client =
                            dialog->findChild<ChartplotterRouteClient *>();
                        if (deadline.elapsed() > 35000 ||
                            client->state() ==
                                ChartplotterRouteClient::Failed) {
                            dialog->reject();
                            drive.stop();
                            stage = -1;
                            return;
                        }
                        if (stage == 0) {
                            stage = 1;
                            dialog
                                ->findChild<QPushButton *>(
                                    "routeServiceDiscover")
                                ->click();
                        } else if (stage == 1 && !client->busy()) {
                            auto *devices = dialog->findChild<QComboBox *>(
                                "routeServiceDevice");
                            for (int i = 0; i < devices->count(); ++i)
                                if (devices->itemData(i).toString() ==
                                    identity) {
                                    devices->setCurrentIndex(i);
                                    stage = 2;
                                    dialog
                                        ->findChild<QPushButton *>(
                                            "routeServiceRefresh")
                                        ->click();
                                    break;
                                }
                        } else if (stage == 2 && !client->busy()) {
                            auto list = client->entries();
                            for (int i = 0; i < list.size(); ++i)
                                if (list[i].name == "rfss-test") {
                                    dialog
                                        ->findChild<QListWidget *>(
                                            "routeServiceRoutes")
                                        ->setCurrentRow(i);
                                    stage = 3;
                                    dialog
                                        ->findChild<QPushButton *>(
                                            "routeServiceRetrieve")
                                        ->click();
                                    break;
                                }
                        } else if (stage == 3 &&
                                   client->state() ==
                                       ChartplotterRouteClient::Complete) {
                            selected = client->candidate();
                            stage = 4;
                            dialog
                                ->findChild<QPushButton *>("routeServiceSelect")
                                ->click();
                            drive.stop();
                        }
                    }
                }
            });
            drive.start(25);
            QVERIFY(QMetaObject::invokeMethod(
                &movement, "on_pushButtonSelectRouteFile_clicked"));
            drive.stop();
            QCOMPARE(stage, 4);
            QVERIFY(selected.complete);
            QCOMPARE(selected.waypoints.size(), 3);
            QVERIFY(selected.attributes.value("SourceNAME") == identity);
            QVERIFY(!worker->isSending());
            QVERIFY(!movement.findChild<AutoPilotSimulator *>()->isRunning());
            QVERIFY(
                !movement
                     .findChild<QCheckBox *>("checkBoxManualMovementEnabled")
                     ->isChecked());
            QFile evidence("build/m239b8/live-route-private.json");
            QVERIFY(evidence.open(QIODevice::WriteOnly));
            QJsonArray points;
            for (const auto &point : selected.waypoints)
                points.append(
                    QJsonObject{{"name", point.name},
                                {"latitude", point.coordinate.latitude()},
                                {"longitude", point.coordinate.longitude()}});
            evidence.write(
                QJsonDocument(QJsonObject{{"name", selected.name},
                                          {"source", selected.source},
                                          {"NAME", identity},
                                          {"ids", selected.attributes.value(
                                                      "WaypointIDs")},
                                          {"points", points}})
                    .toJson());
            evidence.close();
            if (qEnvironmentVariable("RAY_NETWORK_LIVE_RX_ROUTE_ONLY") != "1") {
                auto *start = w.findChild<QAction *>("actionStart_Sending");
                QTRY_VERIFY(h->networkCan()->connected());
                QTRY_VERIFY(start->isEnabled());
                start->trigger();
                QTRY_VERIFY_WITH_TIMEOUT(worker->isSending() &&
                                             h->networkCan()->transmitting(),
                                         10000);
                unsigned char sid = 1;
                double instance = 250, actual = N2kDoubleNA,
                       setpoint = N2kDoubleNA;
                tN2kTempSource source = N2kts_SeaTemperature;
                QVERIFY(
                    h->SendPgn130312(sid, instance, source, actual, setpoint));
                QTest::qWait(250);
                QTest::keyClick(&w, Qt::Key_F12);
                QVERIFY(!worker->isSending());
                QVERIFY(!h->networkCan()->transmitting());
                tN2kMsg disallowed;
                SetN2kLatLonRapid(disallowed, 10, 20);
                QVERIFY(!h->networkCan()->send(disallowed));
            }
            w.findChild<QAction *>("actionViewer_Rx")->trigger();
            QSignalSpy received(h, &Nmea2000Handler::RcvdPgn);
            QTRY_VERIFY(h->networkCan()->connected());
            QTest::qWait(2500);
            QVERIFY(received.size() > 10);
            auto *model = w.findChild<PgnTableModel *>();
            QVERIFY(model);
            bool gps = false, heading = false;
            for (const auto &p : model->getPgnList()) {
                gps |= p.pgn == "129025" || p.pgn == "129029";
                heading |= p.pgn == "127250";
            }
            QVERIFY(gps);
            QVERIFY(heading);
            QVERIFY(!worker->isSending());
            QVERIFY(
                !w.findChild<QAction *>("actionStart_Sending")->isEnabled());
            qInfo("Live Viewer GPS/heading and explicit stored-route selection passed.");
            QTimer dismiss;
            connect(&dismiss, &QTimer::timeout, [] {
                for (auto *top : QApplication::topLevelWidgets())
                    if (auto *box = qobject_cast<QMessageBox *>(top)) {
                        if (box->button(QMessageBox::Discard))
                            box->button(QMessageBox::Discard)->click();
                        else if (box->button(QMessageBox::No))
                            box->button(QMessageBox::No)->click();
                        else
                            box->reject();
                    }
            });
            dismiss.start(20);
            w.close();
        }
        QVERIFY(CredentialStore::remove("NetworkNmea2000Token"));
    }
    void realToolbarF12AndSettings() {
        auto c = config(404);
        AppSettings s(APP_COMPANY, APP_NAME);
        s.setValue("Nmea2000/Nmea2000Enabled", true);
        s.setValue("Nmea2000/InterfaceType", "Network NMEA2000");
        s.setValue("Nmea2000/NetworkCanHost", c.host);
        s.setValue("Nmea2000/NetworkCanPort", c.port);
        s.setValue("Nmea2000/NetworkCanInterface", c.interfaceName);
        s.setValue("Nmea2000/NetworkCanAllowTx", true);
        s.setValue("Nmea2000/NetworkCanUniqueNumber", c.uniqueNumber);
        s.setValue("Nmea0183/Nmea0183Enabled", false);
        s.sync();
        QString error;
        QVERIFY(
            CredentialStore::write("NetworkNmea2000Token", c.token, &error));
        auto credentialCleanup = qScopeGuard(
            [] { CredentialStore::remove("NetworkNmea2000Token"); });
        {
            MainWindow w;
            w.show();
            auto *h = w.findChild<Nmea2000Handler *>();
            auto *worker = w.findChild<NmeaSenderWorker *>();
            auto *start = w.findChild<QAction *>("actionStart_Sending");
            auto *stop = w.findChild<QAction *>("actionStop_Sending");
            QTRY_VERIFY(h->networkCan()->connected());
            auto *setup=w.findChild<DialogSetup*>();setup->show();
            auto *feedback=setup->findChild<QLabel*>("networkCanConnectionStatus");
            QVERIFY(feedback);QCOMPARE(feedback->text(),h->networkCan()->status());
            QTRY_VERIFY(feedback->text().contains("RX-only"));
            auto *reconnect=setup->findChild<QPushButton*>("networkCanReconnect");
            reconnect->click();QVERIFY(feedback->text().contains("connecting"));
            QTRY_VERIFY(h->networkCan()->connected());
            QTRY_VERIFY(feedback->text().contains("RX-only"));
            // A transport failure must appear inline and recovery must reflect the new session.
            QTcpServer port;QVERIFY(port.listen(QHostAddress::LocalHost,0));
            auto unavailable=c;unavailable.port=port.serverPort();port.close();
            h->networkCan()->configure(unavailable);
            QTRY_VERIFY(feedback->text().contains("failed"));
            QVERIFY(!h->networkCan()->transmitting());
            h->networkCan()->configure(c);QTRY_VERIFY(h->networkCan()->ready());
            QTRY_VERIFY(feedback->text().contains("RX-only"));
            QVERIFY(!worker->isSending());
            QTRY_VERIFY(start->isEnabled());
            start->trigger();
            QTRY_VERIFY_WITH_TIMEOUT(
                worker->isSending() && h->networkCan()->transmitting(), 10000);
            QVERIFY(feedback->text().contains("Sending ready"));
            QVERIFY(!start->isEnabled());
            QVERIFY(stop->isEnabled());
            QTest::keyClick(&w, Qt::Key_F12);
            QVERIFY(!worker->isSending());
            QVERIFY(!h->networkCan()->transmitting());
            QTRY_VERIFY(h->networkCan()->connected());
            QTRY_VERIFY(start->isEnabled());
            QVERIFY(!stop->isEnabled());
            QTRY_VERIFY(feedback->text().contains("RX-only"));
            setup->reject();
            w.close();
        }
        QVERIFY(CredentialStore::remove("NetworkNmea2000Token"));
    }
    void networkSetupSaveCancel() {
        DialogSetup setup;
        setup.show();
        auto *host = setup.findChild<QLineEdit *>("networkCanHost");
        auto *mode = setup.findChild<QComboBox *>("networkCanMode");
        QVERIFY(host && mode);
        AppSettings settings(APP_COMPANY, APP_NAME);
        auto original = settings.value("Nmea2000/NetworkCanHost").toString();
        setup.setNetworkCanStatus("Network CAN RX-only connected; no CAN application TX");
        QSignalSpy requests(&setup,&DialogSetup::ConnectNetworkCanDevice);
        host->setText("discarded.invalid");
        setup.findChild<QPushButton*>("networkCanReconnect")->click();
        QCOMPARE(requests.size(),0);
        auto *feedback=setup.findChild<QLabel*>("networkCanConnectionStatus");
        QTRY_VERIFY(feedback->text().contains("RX-only"));
        QVERIFY(feedback->text().contains("Save"));
        QTimer::singleShot(0, [] {
            for (auto *w : QApplication::topLevelWidgets())
                if (auto *box = qobject_cast<QMessageBox *>(w)) {
                    if (box->button(QMessageBox::Discard))
                        box->button(QMessageBox::Discard)->click();
                    else if (box->button(QMessageBox::No))
                        box->button(QMessageBox::No)->click();
                    else
                        box->reject();
                }
        });
        setup.reject();
        QCOMPARE(settings.value("Nmea2000/NetworkCanHost").toString(),
                 original);
        setup.show();
        host->setText("127.0.0.1");
        mode->setCurrentIndex(0);
        QVERIFY(QMetaObject::invokeMethod(&setup, "on_pushButtonSave_clicked"));
        settings.sync();
        QCOMPARE(settings.value("Nmea2000/NetworkCanHost").toString(),
                 QString("127.0.0.1"));
        QVERIFY(!settings.value("Nmea2000/NetworkCanAllowTx").toBool());
        QVERIFY(!feedback->text().contains("Save the"));
        if (!qEnvironmentVariable("RAY_NETWORK_SETUP_IMAGE").isEmpty()) {
            setup.findChild<QTabWidget *>("tabWidget")->setCurrentIndex(1);
            setup.resize(1050, 950);
            QTest::qWait(30);
            setup.grab().save(qEnvironmentVariable("RAY_NETWORK_SETUP_IMAGE"));
        }
        setup.reject();
    }
};
int main(int argc, char **argv) {
    QTemporaryDir profile(QDir::currentPath() + "/build/m239b8/profile-XXXXXX");
    if (!profile.isValid())
        return 1;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QApplication app(argc, argv);
    QNetworkProxy::setApplicationProxy(
        QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    NetworkCanTests t;
    return QTest::qExec(&t, argc, argv);
}
#include "network_can_tests.moc"
