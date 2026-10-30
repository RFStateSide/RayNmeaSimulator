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
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QTabWidget>
#include <QInputDialog>
#include <QPushButton>
#include <QNetworkProxy>
#include <QComboBox>
#include <QListWidget>
#include <QAction>
#include "mainwindow.h"
#include "appsettings.h"
#include "chartplotterrouteclient.h"
#include "chartplotterroutedialog.h"
#include "routeservicepolicy.h"
#include "generatorvesselmovement.h"
#include "nmea2000handler.h"
#include "apppaths.h"
#include "NMEA2000.h"

// Shared synthetic wire fixtures; no private vessel data.
#include "route_service_fixture.h"
using namespace RouteFixture;
namespace {
class ServiceBackend : public SocketCanBackend {
  public:
    std::function<void(const tN2kMsg &)> callback;
    std::function<void(const tN2kMsg &)> onSend;
    QVector<tN2kMsg> sent;
    bool open = true;
    int closes = 0;
    void configure(const SocketCanConfig &,
                   std::function<void(const tN2kMsg &)> cb) override {
        callback = std::move(cb);
    }
    void service() override {}
    bool ready() const override { return open; }
    int sourceAddress() const override { return 43; }
    int openingError() const override { return 0; }
    bool send(const tN2kMsg &m) override {
        sent.append(m);
        if (onSend)
            onSend(m);
        return true;
    }
    bool close() override {
        ++closes;
        callback = {};
        return true;
    }
};
struct Harness {
    ChartplotterRouteClient c;
    QVector<tN2kMsg> sent;
    int consumed = 0;
    Harness() {
        c.ready = [] { return true; };
        c.localAddress = [] { return 43; };
        c.send = [this](const tN2kMsg &m) {
            if (!RouteServicePolicy::allowed(m))
                return false;
            sent.append(m);
            return true;
        };
        c.ingest(claim());
    }
    bool wait(int p) {
        QElapsedTimer t;
        t.start();
        while (consumed >= sent.size() && t.elapsed() < 2000)
            QTest::qWait(10);
        if (consumed >= sent.size())
            return false;
        return requested(sent[consumed++]) == p;
    }
    bool browse() {
        c.browse({1, name(), "Fixture"});
        if (!wait(60928))
            return false;
        c.ingest(claim());
        if (!wait(130064))
            return false;
        c.ingest(databases());
        if (!wait(130065))
            return false;
        c.ingest(routes());
        return c.state() == ChartplotterRouteClient::Idle &&
               c.entries().size() == 1;
    }
    bool retrieve() {
        c.retrieve(c.entries().first());
        if (!wait(60928))
            return false;
        c.ingest(claim());
        return wait(130066);
    }
    bool complete(bool pages = false) {
        if (!browse() || !retrieve())
            return false;
        c.ingest(attrs());
        if (!wait(130067))
            return false;
        if (pages) {
            c.ingest(geometry(0, 1));
            if (!wait(130067))
                return false;
            c.ingest(geometry(1, 2));
        } else
            c.ingest(geometry());
        if (!wait(130067))
            return false;
        c.ingest(geometry());
        if (!wait(130066))
            return false;
        c.ingest(attrs());
        return c.state() == ChartplotterRouteClient::Complete;
    }
};
} // namespace
class RouteServiceTests : public QObject {
    Q_OBJECT
  private slots:
    void roundTripAndSparseIdentifiers() {
        Harness h;
        QVERIFY(h.complete());
        auto r = h.c.candidate();
        QVERIFY(r.complete);
        QCOMPARE(r.waypoints.size(), 3);
        QCOMPARE(r.waypoints[0].name, QString("Point 0"));
        QCOMPARE(r.waypoints[2].coordinate, QGeoCoordinate(10.02, 20.02));
        QCOMPARE(r.attributes.value("Datestamp"), QString("65535"));
        QVERIFY(r.attributes.contains("VersionStatus"));
        QCOMPARE(h.c.entries().first().id, 7);
    }
    void geometryPaginationUsesRpsNotWaypointId() {
        Harness h;
        QVERIFY(h.complete(true));
        QCOMPARE(h.c.candidate().waypoints.size(), 3);
    }
    void changedMetadataAndGeometryAreRejected() {
        for (int change = 0; change < 2; ++change) {
            Harness h;
            QVERIFY(h.browse() && h.retrieve());
            h.c.ingest(attrs());
            QVERIFY(h.wait(130067));
            h.c.ingest(geometry());
            QVERIFY(h.wait(130067));
            if (change == 0) {
                auto wrong = geometry();
                wrong.Data[wrong.DataLen - 1] ^= 1;
                h.c.ingest(wrong);
            } else {
                h.c.ingest(geometry());
                QVERIFY(h.wait(130066));
                h.c.ingest(attrs(1));
            }
            QCOMPARE(h.c.state(), ChartplotterRouteClient::Failed);
            QVERIFY(!h.c.candidate().complete);
        }
    }
    void identityChangeAndDisconnect() {
        Harness h;
        QVERIFY(h.browse() && h.retrieve());
        auto wrong = claim();
        wrong.Data[0] ^= 1;
        h.c.ingest(wrong);
        QCOMPARE(h.c.state(), ChartplotterRouteClient::Failed);
        Harness d;
        QVERIFY(d.browse() && d.retrieve());
        d.c.ready = [] { return false; };
        d.c.advance(500);
        QCOMPARE(d.c.state(), ChartplotterRouteClient::Failed);
    }
    void unsupportedIncompleteAndOutOfOrder() {
        for (int bad = 0; bad < 4; ++bad) {
            Harness h;
            QVERIFY(h.browse() && h.retrieve());
            if (bad == 0) {
                h.c.ingest(attrs(1));
            } else {
                h.c.ingest(attrs());
                QVERIFY(h.wait(130067));
                auto m = geometry(bad == 1 ? 1 : 0, bad == 1 ? 2 : 3,
                                  bad == 2 ? 4 : 3);
                if (bad == 3)
                    --m.DataLen;
                h.c.ingest(m);
            }
            QCOMPARE(h.c.state(), ChartplotterRouteClient::Failed);
        }
    }
    void boundedRetryCancelAndFreshOperation() {
        Harness h;
        QVERIFY(h.browse() && h.retrieve());
        h.c.advance(10000);
        QCOMPARE(h.sent.size(), h.consumed + 1);
        h.c.advance(20000);
        QCOMPARE(h.c.state(), ChartplotterRouteClient::Failed);
        int n = h.sent.size();
        h.c.advance(30000);
        QCOMPARE(h.sent.size(), n);
        h.c.cancel();
        QCOMPARE(h.c.state(), ChartplotterRouteClient::Idle);
        QVERIFY(h.c.entries().size() == 1);
    }
    void policyRejectsWritesTimersAndUnrelatedPgn() {
        Harness h;
        QVERIFY(h.browse());
        auto m = h.sent.last();
        QVERIFY(RouteServicePolicy::allowed(m));
        m.Data[0] = 1;
        QVERIFY(!RouteServicePolicy::allowed(m));
        m = h.sent.last();
        m.Data[4] = 0;
        QVERIFY(!RouteServicePolicy::allowed(m));
        m = msg(127250, QByteArray(8, '\0'));
        QVERIFY(!RouteServicePolicy::allowed(m));
    }
    void selectionAndExplicitAutopilotPreserveFileWorkflow() {
        Harness h;
        QVERIFY(h.complete());
        GeneratorVesselMovement widget(1, "Fixture", nullptr);
        widget.setActiveRouteChooser(
            [&](QWidget *) { return h.c.candidate(); });
        auto *pilot = widget.findChild<AutoPilotSimulator *>();
        auto *manual =
            widget.findChild<QCheckBox *>("checkBoxManualMovementEnabled");
        auto *enabled =
            widget.findChild<QCheckBox *>("checkBoxAutoPilotEnabled");
        QVERIFY(pilot && manual && enabled);
        QSignalSpy loaded(pilot, &AutoPilotSimulator::routeLoaded);
        QTimer::singleShot(0, [] {
            for (auto *w : QApplication::topLevelWidgets())
                if (auto *d = qobject_cast<QInputDialog *>(w)) {
                    d->setTextValue("Select Chartplotter Route");
                    d->accept();
                }
        });
        QVERIFY(QMetaObject::invokeMethod(
            &widget, "on_pushButtonSelectRouteFile_clicked"));
        QVERIFY(!pilot->isRunning());
        QVERIFY(!manual->isChecked());
        QVERIFY(!enabled->isChecked());
        QCOMPARE(loaded.size(), 0);
        QCOMPARE(widget.findChild<QLineEdit*>("selectedRouteName")->text(), h.c.candidate().name);
        QVERIFY(!widget.findChild<QLabel*>("receivedRouteStatus")->text().contains(QChar::ReplacementCharacter));
        const auto selectedName=widget.findChild<QLineEdit*>("selectedRouteName")->text();
        if(!qEnvironmentVariable("RAY_B9_UI_IMAGE_DIR").isEmpty()) {
            widget.resize(550,760);widget.findChild<QTabWidget*>("tabWidget")->setCurrentIndex(1);widget.show();QTest::qWait(30);
            QVERIFY(widget.grab().save(qEnvironmentVariable("RAY_B9_UI_IMAGE_DIR")+"/chartplotter-route-name.png"));
        }
        enabled->click();
        QVERIFY(pilot->isRunning());
        QCOMPARE(loaded.size(), 1);
        // Cancelling a later route chooser preserves the active navigation.
        widget.setActiveRouteChooser([](QWidget*){return ReceivedRoutes::Route{};});
        QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto *d=qobject_cast<QInputDialog*>(w)){d->setTextValue("Select Chartplotter Route");d->accept();}});
        QVERIFY(QMetaObject::invokeMethod(&widget,"on_pushButtonSelectRouteFile_clicked"));
        QVERIFY(pilot->isRunning());QCOMPARE(loaded.size(),1);
        QCOMPARE(widget.findChild<QLineEdit*>("selectedRouteName")->text(),selectedName);
        enabled->click();
        QVERIFY(!pilot->isRunning());
        auto saved = widget.layoutConfiguration();
        QVERIFY(!QJsonDocument(saved).toJson().contains("Point 0"));
        widget.restoreLayoutConfiguration(saved);
        QVERIFY(!pilot->isRunning());
    }
    void acknowledgmentsAndUnsentReplies() {
        Harness h;
        QVERIFY(h.browse() && h.retrieve());
        QByteArray b;
        u(b, 2, 1);
        u(b, 130066, 3);
        u(b, 1, 1);
        u(b, 0, 1);
        auto ack = msg(126208, b);
        ack.Destination = 44;
        h.c.ingest(ack);
        QCOMPARE(h.c.state(), ChartplotterRouteClient::Attributes);
        ack.Destination = 43;
        h.c.ingest(ack);
        QCOMPARE(h.c.state(), ChartplotterRouteClient::Failed);
        Harness p;
        QVERIFY(p.browse() && p.retrieve());
        b.clear();
        u(b, 2, 1);
        u(b, 130066, 3);
        u(b, 0, 1);
        u(b, 1, 1);
        u(b, 0xf3, 1);
        ack = msg(126208, b);
        ack.Destination = 43;
        p.c.ingest(ack);
        QCOMPARE(p.c.state(), ChartplotterRouteClient::Failed);
        Harness q;
        q.c.browse({1, name(), "Fixture"});
        QVERIFY(q.wait(60928));
        q.c.ingest(claim());
        q.c.ingest(databases());
        QCOMPARE(q.c.state(),
                 ChartplotterRouteClient::Databases); // The throttled request
                                                      // has not been sent yet.
    }

    void acknowledgmentCorrelation_data() {
        QTest::addColumn<int>("mutation");
        for(int i=0;i<8;++i)QTest::newRow(qPrintable(QString::number(i)))<<i;
    }
    void acknowledgmentCorrelation() {
        QFETCH(int,mutation);Harness h;QVERIFY(h.browse()&&h.retrieve());
        QByteArray b;u(b,2,1);u(b,130066,3);u(b,0,1);u(b,2,1);u(b,0,1);
        auto ack=msg(126208,b);ack.Destination=43;
        // ACK is not route data and must not complete the request.
        if(mutation==1)ack.Source=2;
        if(mutation==2)ack.Destination=44;
        if(mutation==3)ack.Data[1]^=1;
        if(mutation==4)ack.Data[4]=1;
        if(mutation==5)ack.Data[6]=0x10; // Second selector error nibble.
        if(mutation==6)--ack.DataLen;
        if(mutation==7)ack.Data[0]=1; // Not an acknowledgment function.
        h.c.ingest(ack);
        const bool rejected=mutation>=4&&mutation<=6;
        QCOMPARE(h.c.state(),rejected?ChartplotterRouteClient::Failed:ChartplotterRouteClient::Attributes);
        QVERIFY(!h.c.candidate().complete);
        if(!rejected){h.c.ingest(attrs());QVERIFY(h.wait(130067));}
    }
    void isoAcknowledgmentCorrelation() {
        for(int change=0;change<5;++change) {
            Harness h;h.c.browse({1,name(),"Fixture"});QVERIFY(h.wait(60928));
            auto ack=msg(59392,QByteArray::fromHex("01ffffffff00ee00"));ack.Destination=43;
            if(change==0)ack.Data[0]=0;
            if(change==1)ack.Source=2;
            if(change==2)ack.Destination=44;
            if(change==3)ack.Data[5]^=1;
            h.c.ingest(ack);
            QCOMPARE(h.c.state(),change==4?ChartplotterRouteClient::Failed:ChartplotterRouteClient::Identity);
        }
    }
    void deadlineAndCancelledCallbacks() {
        Harness h;QVERIFY(h.browse()&&h.retrieve());h.c.advance(70000);
        QCOMPARE(h.c.state(),ChartplotterRouteClient::Failed);
        int count=h.sent.size();h.c.ingest(attrs());h.c.ingest(geometry());h.c.advance(80000);
        QCOMPARE(h.sent.size(),count);QVERIFY(!h.c.candidate().complete);
        h.c.cancel();h.c.ingest(claim());h.c.ingest(attrs());h.c.advance(90000);
        QCOMPARE(h.sent.size(),count);QVERIFY(!h.c.busy());
    }
    void serviceAuthorizationAndModeLifecycle() {
        ServiceBackend b;
        Nmea2000Handler h(nullptr, nullptr, &b);
        h.SetInterfaceType("SocketCan");
        h.setMode(Viewer);
        h.InitializeSocketCan({"can0", 23});
        QTRY_VERIFY(h.IsCanInterfaceOpen());
        tN2kMsg request;
        request.SetPGN(59904);
        request.Destination = 1;
        request.Add3ByteInt(60928);
        QVERIFY(!h.sendRouteService(request));
        QVERIFY(h.beginRouteService());
        QVERIFY(h.sendRouteService(request));
        QCOMPARE(b.sent.size(), 1);
        double lat = 10, lon = 20;
        QVERIFY(!h.SendPgn129025(lat, lon));
        auto prohibited = msg(127250, QByteArray(8, '\0'));
        QVERIFY(!h.sendRouteService(prohibited));
        QSignalSpy raw(&h, &Nmea2000Handler::RouteServiceMessage);
        b.callback(claim());
        QCOMPARE(raw.size(), 1);
        h.setMode(Sender);
        QVERIFY(!h.routeServiceActive());
        QVERIFY(!h.sendRouteService(request));
        QVERIFY(b.closes == 1);
        ServiceBackend g;
        Nmea2000Handler generate(nullptr, nullptr, &g);
        generate.SetInterfaceType("SocketCan");
        generate.setMode(Sender);
        generate.InitializeSocketCan({"can0", 24});
        QTRY_VERIFY(generate.IsCanInterfaceOpen());
        QVERIFY(generate.beginRouteService());
        QSignalSpy rx(&generate, &Nmea2000Handler::RouteServiceMessage);
        g.callback(claim());
        QCOMPARE(rx.size(), 1);
        QVERIFY(!generate.SendPgn129025(lat, lon));
        generate.StopWaveshare(true);
        QVERIFY(!generate.routeServiceActive());
    }
    void realDialogTransportWorkflowAndCancellation() {
        ServiceBackend b;
        Nmea2000Handler h(nullptr, nullptr, &b);
        h.SetInterfaceType("SocketCan");
        h.setMode(Sender);
        h.InitializeSocketCan({"can0", 25});
        QTRY_VERIFY(h.IsCanInterfaceOpen());
        b.onSend = [&](const tN2kMsg &m) {
            auto p = requested(m);
            tN2kMsg reply;
            if (p == 60928)
                reply = claim();
            else if (p == 130064)
                reply = databases();
            else if (p == 130065)
                reply = routes();
            else if (p == 130066)
                reply = attrs();
            else if (p == 130067)
                reply = geometry();
            else
                return;
            QTimer::singleShot(0, &h, [&, reply] {
                if (b.callback)
                    b.callback(reply);
            });
        };
        ChartplotterRouteDialog d(&h, {});
        d.show();
        auto *c = d.findChild<ChartplotterRouteClient *>();
        QVERIFY(c);
        d.findChild<QPushButton *>("routeServiceDiscover")->click();
        QTRY_VERIFY(!c->devices().isEmpty());
        c->advance(7000);
        QVERIFY(!c->busy());
        auto *deviceChoice=d.findChild<QComboBox*>("routeServiceDevice");
        QCOMPARE(deviceChoice->currentIndex(),-1);
        QVERIFY(!d.findChild<QPushButton*>("routeServiceRefresh")->isEnabled());
        deviceChoice->setCurrentIndex(0);
        d.findChild<QPushButton *>("routeServiceRefresh")->click();
        QTRY_COMPARE(c->entries().size(), 1);
        auto *list = d.findChild<QListWidget *>("routeServiceRoutes");
        list->setCurrentRow(0);
        auto *select = d.findChild<QPushButton *>("routeServiceSelect");
        QVERIFY(!select->isEnabled());
        d.findChild<QPushButton *>("routeServiceRetrieve")->click();
        QTRY_COMPARE(c->state(), ChartplotterRouteClient::Complete);
        QVERIFY(select->isEnabled());
        auto second=claim();second.Source=2;second.Data[0]^=0x40;c->ingest(second);
        auto *device=d.findChild<QComboBox*>("routeServiceDevice");int beforeSwitch=b.sent.size();
        device->setCurrentIndex(1);QVERIFY(!select->isEnabled());QCOMPARE(list->count(),0);QCOMPARE(b.sent.size(),beforeSwitch);
        device->setCurrentIndex(0);d.findChild<QPushButton*>("routeServiceRefresh")->click();QTRY_COMPARE(c->entries().size(),1);
        list->setCurrentRow(0);d.findChild<QPushButton*>("routeServiceRetrieve")->click();QTRY_COMPARE(c->state(),ChartplotterRouteClient::Complete);QVERIFY(select->isEnabled());
        if(!qEnvironmentVariable("RAYNMEA_ROUTE_UI_IMAGE").isEmpty())d.grab().save(qEnvironmentVariable("RAYNMEA_ROUTE_UI_IMAGE"));
        QVERIFY(h.routeServiceActive());
        for (const auto &m : b.sent)
            QVERIFY(RouteServicePolicy::allowed(m));
        select->click();
        QCOMPARE(d.result(), int(QDialog::Accepted));
        QVERIFY(d.selection().complete);
        QCOMPARE(d.selection().waypoints.size(), 3);
        // Revocation cannot leave a completed candidate selectable.
        h.endRouteService(true);
        QVERIFY(!select->isEnabled());
        QVERIFY(!h.routeServiceActive());
        Harness fixture;
        QVERIFY(fixture.complete());
        ChartplotterRouteDialog passive(&h, {fixture.c.candidate()});
        passive.passiveStillValid = [](const ReceivedRoutes::Route &) {
            return false;
        };
        passive.findChild<QListWidget *>("routeServiceRoutes")
            ->setCurrentRow(0);
        passive.findChild<QPushButton *>("routeServiceSelect")->click();
        QVERIFY(!passive.selection().complete);
    }
    void f12AndToolbarNeverAuthorizeInstruments() {
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Nmea2000/Nmea2000Enabled", true);
        settings.setValue("Nmea2000/InterfaceType", "SocketCan");
        settings.setValue("Nmea2000/SocketCanUniqueNumber", 26);
        settings.setValue("Nmea0183/Nmea0183Enabled", false);
        settings.sync();
        ServiceBackend b;
        MainWindow w(nullptr, nullptr, nullptr, &b);
        auto *h = w.findChild<Nmea2000Handler *>();
        auto *worker = w.findChild<NmeaSenderWorker *>();
        QVERIFY(h && worker);
        w.show();
        QTRY_VERIFY(h->IsCanInterfaceOpen());
        QVERIFY(h->beginRouteService());
        auto *start = w.findChild<QAction *>("actionStart_Sending");
        auto *stop = w.findChild<QAction *>("actionStop_Sending");
        QVERIFY(!start->isEnabled());
        QVERIFY(stop->isEnabled());
        QVERIFY(!worker->isSending());
        QTest::keyClick(&w, Qt::Key_F12);
        QVERIFY(!h->routeServiceActive());
        QVERIFY(!worker->isSending());
        QVERIFY(!stop->isEnabled());
        auto *controller=w.findChild<MainController*>();QVERIFY(controller);
        auto *origin=new QWidget(&w);QTimer::singleShot(0,origin,&QObject::deleteLater);
        QVERIFY(!controller->activeRouteChooser(origin).complete); // Layout/widget destruction closes the chooser.
        w.close();
    }

    void lifecycleStages_data() {
        QTest::addColumn<int>("stage");QTest::addColumn<int>("action");
        for(int stage=0;stage<5;++stage)for(int action=0;action<6;++action)
            QTest::newRow(qPrintable(QString("stage%1-action%2").arg(stage).arg(action)))<<stage<<action;
    }
    void lifecycleStages() {
        QFETCH(int,stage);QFETCH(int,action);
        AppSettings settings(APP_COMPANY,APP_NAME);settings.clear();
        settings.setValue("Nmea2000/Nmea2000Enabled",true);settings.setValue("Nmea2000/InterfaceType","SocketCan");
        settings.setValue("Nmea2000/SocketCanUniqueNumber",42);settings.setValue("Nmea0183/Nmea0183Enabled",false);settings.sync();
        ServiceBackend b;MainWindow window(nullptr,nullptr,nullptr,&b);window.show();
        auto *h=window.findChild<Nmea2000Handler*>();auto *worker=window.findChild<NmeaSenderWorker*>();QVERIFY(h&&worker);
        QTRY_VERIFY(h->IsCanInterfaceOpen());
        ChartplotterRouteClient client;client.ready=[&]{return h->routeServiceReady();};client.localAddress=[&]{return h->routeServiceSource();};
        client.send=[&](const tN2kMsg &m){return h->sendRouteService(m);};
        connect(h,&Nmea2000Handler::RouteServiceMessage,&client,&ChartplotterRouteClient::ingest);
        connect(h,&Nmea2000Handler::RouteServiceRevoked,&client,[&]{client.cancel();});
        const int hold=stage==1?60928:stage==2?130065:stage==3?130067:0;
        b.onSend=[&](const tN2kMsg &m){if(requested(m)==hold)return;auto reply=response(m);if(reply.PGN)QTimer::singleShot(0,h,[&,reply]{if(b.callback)b.callback(reply);});};
        QVERIFY(h->beginRouteService());client.ingest(claim());client.browse({1,name(),"Fixture"});
        if(stage==1)QTRY_VERIFY(!b.sent.isEmpty());
        if(stage>=2)QTRY_VERIFY(std::any_of(b.sent.begin(),b.sent.end(),[](const auto &m){return requested(m)==130065;}));
        if(stage>=3){QTRY_COMPARE(client.entries().size(),1);client.retrieve(client.entries().first());
            if(stage==3)QTRY_VERIFY(std::any_of(b.sent.begin(),b.sent.end(),[](const auto &m){return requested(m)==130067;}));
            else QTRY_COMPARE(client.state(),ChartplotterRouteClient::Complete);
        }
        QVERIFY(!worker->isSending());QSignalSpy revoked(h,&Nmea2000Handler::RouteServiceRevoked);
        if(action==0)QTest::keyClick(&window,Qt::Key_F12);
        if(action==1)window.findChild<QAction*>("actionStop_Sending")->trigger();
        if(action==2)h->setMode(window.currentMode()==Viewer?Sender:Viewer);
        if(action==3)window.findChild<MainController*>()->layoutAboutToReplace();
        if(action==4)QVERIFY(QMetaObject::invokeMethod(&window,"HotReloadSettings",Qt::DirectConnection));
        if(action==5)QVERIFY(window.close());
        QVERIFY(!h->routeServiceActive());QCOMPARE(revoked.size(),1);QVERIFY(!worker->isSending());QVERIFY(!client.candidate().complete);
        const int sent=b.sent.size();client.ingest(attrs());client.ingest(geometry());client.advance(90000);QTest::qWait(40);
        QCOMPARE(b.sent.size(),sent);QVERIFY(!client.busy());
        for(const auto &m:b.sent)QVERIFY(RouteServicePolicy::allowed(m));
    }
    void productMetadataAndExplicitDeviceSelection() {
        ServiceBackend backend;
        Nmea2000Handler handler(nullptr,nullptr,&backend);
        ChartplotterRouteDialog dialog(&handler,{});
        auto *client=dialog.findChild<ChartplotterRouteClient*>();
        auto *choice=dialog.findChild<QComboBox*>("routeServiceDevice");
        auto first=claim();client->ingest(first);
        tN2kMsg product;
        SetN2kPGN126996(product,2100,1234,"Axiom Pro 9 RVX","4.7.172","Test model","PRIVATE-FIXTURE");
        product.Source=first.Source;client->ingest(product);
        QCOMPARE(choice->currentIndex(),-1);
        QVERIFY(!dialog.findChild<QPushButton*>("routeServiceRefresh")->isEnabled());
        QVERIFY(choice->itemText(0).contains("Axiom Pro 9 RVX"));
        QVERIFY(!choice->itemText(0).contains(QChar::ReplacementCharacter));
        const auto details=choice->itemData(0,Qt::ToolTipRole).toString();
        QVERIFY(details.contains("4.7.172") && details.contains("1234"));
        QVERIFY(details.contains(client->devices().first().name));
        QVERIFY(!details.contains("PRIVATE-FIXTURE"));
        choice->setCurrentIndex(0);const auto selected=choice->currentData();
        auto second=claim();second.Source=2;second.Data[0]^=0x40;client->ingest(second);
        QCOMPARE(choice->currentData(),selected);
        client->ingest(first);client->ingest(product);
        QCOMPARE(choice->currentData(),selected);
        QVERIFY(choice->currentText().contains("Axiom"));
        if(!qEnvironmentVariable("RAY_B9_UI_IMAGE_DIR").isEmpty()) {
            dialog.show();QTest::qWait(30);
            QVERIFY(dialog.grab().save(qEnvironmentVariable("RAY_B9_UI_IMAGE_DIR")+"/device-identification.png"));
        }
        // Reusing an address with a different NAME must discard old product labels.
        first.Data[0]^=0x20;client->ingest(first);
        QCOMPARE(choice->currentIndex(),-1);
        QVERIFY(choice->itemText(0).contains("Unidentified"));
    }
    void physicalTranscript() {
        const auto path = qEnvironmentVariable("RAYNMEA_ROUTE_TRANSCRIPT");
        if (path.isEmpty())
            QSKIP("Private physical transcript supplied separately");
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        auto j = QJsonDocument::fromJson(f.readAll()).object();
        auto get = [&](const char *key) {
            auto x = j[key].toObject();
            return msg(
                x["pgn"].toInt(),
                QByteArray::fromHex(x["payload_hex"].toString().toLatin1()));
        };
        Harness h;
        auto identity = get("identity");
        auto nm = QString::fromLatin1(
            QByteArray(reinterpret_cast<const char *>(identity.Data), 8)
                .toHex());
        h.c.ingest(identity);
        h.c.browse({1, nm, "Axiom"});
        QVERIFY(h.wait(60928));
        h.c.ingest(identity);
        QVERIFY(h.wait(130064));
        QCOMPARE(QByteArray(reinterpret_cast<const char*>(h.sent.last().Data),h.sent.last().DataLen).toHex(),j["requestDatabases"].toObject()["payload_hex"].toString().toLatin1());
        h.c.ingest(get("databases"));
        QVERIFY(h.wait(130065));
        QCOMPARE(QByteArray(reinterpret_cast<const char*>(h.sent.last().Data),h.sent.last().DataLen).toHex(),j["requestRoutes"].toObject()["payload_hex"].toString().toLatin1());
        h.c.ingest(get("routes"));
        QCOMPARE(h.c.entries().size(), 1);
        QCOMPARE(h.c.entries()[0].name, QString("rfss-test"));
        h.c.retrieve(h.c.entries()[0]);
        QVERIFY(h.wait(60928));
        h.c.ingest(identity);
        QVERIFY(h.wait(130066));
        QCOMPARE(QByteArray(reinterpret_cast<const char*>(h.sent.last().Data),h.sent.last().DataLen).toHex(),j["requestAttributes"].toObject()["payload_hex"].toString().toLatin1());
        h.c.ingest(get("attributes"));
        QVERIFY(h.wait(130067));
        QCOMPARE(QByteArray(reinterpret_cast<const char*>(h.sent.last().Data),h.sent.last().DataLen).toHex(),j["requestGeometry"].toObject()["payload_hex"].toString().toLatin1());
        h.c.ingest(get("geometry"));
        QVERIFY(h.wait(130067));
        h.c.ingest(get("geometryVerification"));
        QVERIFY(h.wait(130066));
        h.c.ingest(get("attributesVerification"));
        QCOMPARE(h.c.state(), ChartplotterRouteClient::Complete);
        QCOMPARE(h.c.candidate().waypoints.size(), 3);
        const auto points=j["expectedPoints"].toArray();
        const auto ids=QJsonDocument::fromJson(h.c.candidate().attributes.value("WaypointIDs").toUtf8()).array();
        QCOMPARE(points.size(),3);QCOMPARE(ids.size(),3);
        for(int i=0;i<3;++i){auto p=points[i].toObject();auto w=h.c.candidate().waypoints[i];
            QVERIFY(w.name==p["name_private"].toString());QVERIFY(ids[i].toInt()==p["id"].toInt());
            QVERIFY(qAbs(w.coordinate.latitude()-p["latitude_private"].toDouble())<1e-9);
            QVERIFY(qAbs(w.coordinate.longitude()-p["longitude_private"].toDouble())<1e-9);
        }

    }
};
int main(int argc, char **argv) {
    QTemporaryDir profile(QDir::currentPath()+"/build/m239b/profile-XXXXXX");
    if(!profile.isValid())return 1;
    auto root = QDir(profile.path()).absolutePath();
    qputenv("RAYNMEA_TEST_HOME", root.toUtf8());
    qputenv("RAYNMEA_STANDALONE", "1");
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(
        QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    RouteServiceTests t;
    return QTest::qExec(&t, argc, argv);
}
#include "route_service_tests.moc"
