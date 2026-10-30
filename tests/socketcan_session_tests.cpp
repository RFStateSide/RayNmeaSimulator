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
#include <QTemporaryDir>
#include <QScopeGuard>
#include "socketcansession.h"
#include "appsettings.h"
class Backend : public SocketCanBackend {
public:
 int configs=0,services=0,sends=0,closes=0,error=0;bool open=false,closed=false,result=true;
 std::function<void(const tN2kMsg&)> callback;std::function<void()> duringService;
 void configure(const SocketCanConfig&,std::function<void(const tN2kMsg&)> cb) override {++configs;callback=std::move(cb);}
 void service() override {QVERIFY(!closed);++services;if(duringService)duringService();}
 bool ready() const override{return open;}
 int openingError() const override{return error;}
 bool send(const tN2kMsg&) override{++sends;return result;}
 bool close() override{++closes;closed=true;callback={};return result;}
};
class SessionTests : public QObject {
 Q_OBJECT
private slots:
 void stagedRetry(){Backend b;SocketCanSession s(nullptr,&b);QCOMPARE(s.state(),SocketCanSession::Closed);QVERIFY(s.start({"can0",1}));s.service();QCOMPARE(s.state(),SocketCanSession::Initializing);b.error=5;s.service();QCOMPARE(s.state(),SocketCanSession::Failed);b.error=0;b.open=true;s.service();QCOMPARE(s.state(),SocketCanSession::Ready);QCOMPARE(b.configs,1);}
 void duplicateStart(){Backend b;SocketCanSession s(nullptr,&b);QVERIFY(s.start({"can0",1}));QVERIFY(s.start({"can0",1}));QCOMPARE(b.configs,1);QVERIFY(s.serviceActive());}
 void modesBeforeSending(){Backend b;SocketCanSession s(nullptr,&b);s.start({"can0",1});for(int i=0;i<100;++i){s.setViewer(i%2);s.service();}QCOMPARE(b.services,100);QCOMPARE(b.closes,0);}
 void viewerNeverSubmits(){Backend b;b.open=true;SocketCanSession s(nullptr,&b);s.start({"can0",1});s.service();s.setViewer(true);tN2kMsg m;QVERIFY(!s.send(m));s.stopSending();QCOMPARE(b.sends,0);QCOMPARE(b.closes,0);}
 void unsuccessfulAttemptStillCloses(){Backend b;b.result=false;b.open=true;SocketCanSession s(nullptr,&b);s.start({"can0",1});s.service();tN2kMsg m;QVERIFY(!s.send(m));s.setViewer(true);QCOMPARE(b.closes,1);QVERIFY(s.restartRequired());QVERIFY(s.status().contains("error"));}
 void attemptBeforeReady(){Backend b;SocketCanSession s(nullptr,&b);s.start({"can0",1});tN2kMsg m;QVERIFY(!s.send(m));s.stopSending();QCOMPARE(b.sends,0);QCOMPARE(b.closes,1);}
 void terminalGates(){Backend b;b.open=true;SocketCanSession s(nullptr,&b);s.start({"can0",1});s.service();tN2kMsg m;QVERIFY(s.send(m));s.stopSending();auto calls=b.services;for(int i=0;i<100;++i){s.service();QVERIFY(!s.send(m));QVERIFY(!s.start({"can0",1}));s.shutdown();}QCOMPARE(b.services,calls);QCOMPARE(b.closes,1);QVERIFY(!s.serviceActive());}
 void configurationRequiresRestart(){Backend b;SocketCanSession s(nullptr,&b);s.start({"can0",1});QVERIFY(!s.start({"can1",1}));QVERIFY(s.restartRequired());QCOMPARE(b.configs,1);QCOMPARE(b.closes,1);}
 void identityRequiresRestart(){Backend b;SocketCanSession s(nullptr,&b);s.start({"can0",1});QVERIFY(!s.start({"can0",2}));QVERIFY(s.restartRequired());}
 void destructionStopsCallbacks(){Backend b;{SocketCanSession s(nullptr,&b);s.start({"can0",1});}QCOMPARE(b.closes,1);QVERIFY(!b.callback);QCoreApplication::processEvents();QCOMPARE(b.services,0);}
 void reentryAndShutdown(){Backend b;SocketCanSession s(nullptr,&b);s.start({"can0",1});b.duringService=[&]{s.service();s.shutdown();QCOMPARE(b.closes,0);};s.service();QCOMPARE(b.services,1);QCOMPARE(b.closes,1);QCOMPARE(s.state(),SocketCanSession::Closed);}
 void statusShutdown(){Backend b;b.open=true;SocketCanSession s(nullptr,&b);s.start({"can0",1});connect(&s,&SocketCanSession::statusChanged,[&]{if(s.state()==SocketCanSession::Ready)s.shutdown();});s.service();QCOMPARE(b.closes,1);QCOMPARE(s.state(),SocketCanSession::Closed);QVERIFY(!s.serviceActive());s.service();QCOMPARE(b.closes,1);}
 void receivedModeGate(){Backend b;SocketCanSession s(nullptr,&b);s.start({"can0",1});int n=0;connect(&s,&SocketCanSession::received,[&](const tN2kMsg&){++n;});tN2kMsg m;b.callback(m);QCOMPARE(n,0);s.setViewer(true);b.callback(m);QCOMPARE(n,1);s.shutdown();QVERIFY(!b.callback);}
 void invalidConfiguration(){Backend b;SocketCanSession s(nullptr,&b);for(auto name:{"","a/b","can 0","abcdefghijklmnop"})QVERIFY(!s.start({name,1}));QVERIFY(!s.start({"can0",0}));QVERIFY(!s.start({"can0",2097152}));QCOMPARE(b.configs,0);}
#ifdef Q_OS_LINUX
 void missingIpUtility(){QTemporaryDir dir;QVERIFY(dir.isValid());const auto old=qgetenv("PATH");auto restore=qScopeGuard([&]{qputenv("PATH",old);});qputenv("PATH",dir.path().toUtf8());SocketCanSession s;QVERIFY(!s.start({"can0",42}));QCOMPARE(s.state(),SocketCanSession::Failed);QVERIFY(s.status().contains("iproute2"));QVERIFY(!s.serviceActive());}
#endif
 void structuredLink(){auto good=QByteArray(R"([{"ifname":"can0","link_type":"can","flags":["UP"],"operstate":"UNKNOWN"}])");QVERIFY(SocketCanSession::validateLink("can0",good).isEmpty());QVERIFY(!SocketCanSession::validateLink("can1",good).isEmpty());QVERIFY(!SocketCanSession::validateLink("can0","[]").isEmpty());QVERIFY(!SocketCanSession::validateLink("can0",QByteArray(good).replace("can\"","ether\"")).isEmpty());QVERIFY(!SocketCanSession::validateLink("can0",QByteArray(good).replace("UP","DOWN")).isEmpty());}
 void persistedIdentity(){QTemporaryDir dir;QVERIFY(dir.isValid());const auto old=qgetenv("RAYNMEA_TEST_HOME");qputenv("RAYNMEA_TEST_HOME",dir.path().toUtf8());AppSettings settings(APP_COMPANY,APP_NAME);auto c=SocketCanSession::loadConfig(settings);QCOMPARE(c.interfaceName,QString("can0"));QVERIFY(c.uniqueNumber>0&&c.uniqueNumber<=2097151);AppSettings reloaded(APP_COMPANY,APP_NAME);QCOMPARE(SocketCanSession::loadConfig(reloaded).uniqueNumber,c.uniqueNumber);reloaded.setValue("SocketCanUniqueNumber",2097152);QCOMPARE(SocketCanSession::loadConfig(reloaded).uniqueNumber,0u);reloaded.setValue("SocketCanUniqueNumber",42);QCOMPARE(SocketCanSession::loadConfig(reloaded).uniqueNumber,42u);qputenv("RAYNMEA_TEST_HOME",old);}
};
QTEST_GUILESS_MAIN(SessionTests)
#include "socketcan_session_tests.moc"
