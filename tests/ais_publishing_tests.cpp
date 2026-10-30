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
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QAction>
#include <QSpinBox>
#include <QComboBox>
#include <QScopeGuard>
#include <QSlider>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QJsonArray>
#include "convert.h"
#include "datastructs.h"
#include "environmentalmotion.h"
// Same deterministic Autopilot clock seam used by its existing regression suite.
#define private public
#include "autopilotsimulator.h"
#undef private
#include "mainwindow.h"
#include "appsettings.h"
#include "preferences.h"
#include "aisclassbposition129039.h"
#include "aisclassbstatic129809.h"
#include "aisclassbstatic129810.h"
#include "movevesselsimulator.h"
#include "pgntreecontroller.h"
#include "enumcomboboxdelegate.h"
#include "nmeasenderworker.h"
#include <SDL.h>
#include "signalk_testserver.h"
#include "credentialstore.h"

// Reuse the scheduler's existing test-friend seam; no production timer changes.
class SenderLifecycleTests {
public: static void at(NmeaSenderWorker &sender,qint64 time){sender.sendNmeaMessageAt(time);}
};
namespace {
void near(double a,double b,double tolerance=1e-10) {QVERIFY2(std::abs(a-b)<=tolerance,qPrintable(QString("%1 != %2").arg(a,0,'g',17).arg(b,0,'g',17)));}
void confirm(){QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto *b=qobject_cast<QMessageBox*>(w))b->button(QMessageBox::Yes)->click();});}
struct Workspace {
 MainWindow window;
 MainController *controller=window.findChild<MainController*>();
 PgnTableController *table=window.findChild<PgnTableController*>();
 PgnTableModel *model=table->getPgnTableModel();
 GeneratorVesselMovement *movement=nullptr;
 Workspace(SocketCanBackend *backend=nullptr):window(nullptr,nullptr,nullptr,backend){
  confirm();if(!controller->loadLayoutFromJson("tests/fixtures/defaultLayout.json"))qFatal("Fixture Layout load failed");
  movement=window.findChild<GeneratorVesselMovement*>();if(!movement)qFatal("Missing movement fixture");
  for(auto *engine:window.findChildren<GeneratorEngine*>())engine->setEngineProfile("MerCruiser 7.4 MPI");
  for(auto *timer:window.findChildren<QTimer*>())timer->stop();
  window.resize(1500,1050);window.show();
 }
 AisClassBPosition129039 *add(uint id=211000001){
  controller->addWidgetAsComboboxTextChanged("129039","AIS Class B Position","Communication");
  auto *p=qobject_cast<AisClassBPosition129039*>(controller->layoutWidgets("Communication").last());p->setUserID(id);return p;
 }
 QCheckBox *manual(){return movement->findChild<QCheckBox*>("checkBoxManualMovementEnabled");}
 void start(){manual()->click();QVERIFY(manual()->isChecked());}
 void sample(double latitude=43.211000001,double speed=4.211000001){
  VesselStatus status;status.latitude=latitude;status.longitude=-70.211000001;status.heading=125.211000001;
  status.sog=speed;status.groundSpeed=speed;status.groundCourse=132.211000001;movement->setCurrentVesselStatus(status);
 }
 void remove(QWidget *widget,const QString &group){model->removePgn(widget->property("Id").toInt());controller->removeWidgetFromSubForm(widget,group);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
 QString modelValue(AisClassBPosition129039 *p,const QString &field){for(const auto &row:model->getPgnParams(p->Id()))if(row[0]==field)return row[1];return {};}
 void select(QWidget *widget){auto *tree=window.findChild<PgnTreeController*>();QTRY_VERIFY(tree->getTreeModel()->indexFor(widget).isValid());QVERIFY(QMetaObject::invokeMethod(tree,"navigate",Qt::DirectConnection,Q_ARG(QModelIndex,tree->getTreeModel()->indexFor(widget))));}
};
class MemoryCan:public SocketCanBackend {
public:
 QList<tN2kMsg> sent;
 void configure(const SocketCanConfig &,std::function<void(const tN2kMsg &)>)override{}
 void service()override{}
 bool ready()const override{return true;}
 int openingError()const override{return 0;}
 bool send(const tN2kMsg &m)override{sent.append(m);return true;}
 bool close()override{return true;}
};
}

namespace {
QMap<QString,QString> fields(const QList<QList<QString>> &rows,uint mmsi=211000001) {
 QMap<QString,QString> f;for(const auto &r:rows)f[r[0]]=r[1];f["UserID"]=QString::number(mmsi);return f;
}
QStringList wire(const QMap<QString,QString> &f){QStringList r;for(auto i=f.begin();i!=f.end();++i)r<<i.key()+":"+i.value();return r;}
QJsonObject paths(const QJsonObject &delta){QJsonObject r;for(const auto &u:delta["updates"].toArray())for(const auto &v:u.toObject()["values"].toArray()){auto x=v.toObject();r[x["path"].toString()]=x["value"];}return r;}
QString stamp(const QJsonObject &d,const QString &path){for(const auto &u:d["updates"].toArray())for(const auto &v:u.toObject()["values"].toArray())if(v.toObject()["path"].toString()==path)return u.toObject()["timestamp"].toString();return {};}
}
class AisMemoryPort:public QSerialPort {
public:
 QByteArray output;
 bool open(OpenMode mode)override{return QIODevice::open(mode|QIODevice::Unbuffered);}
 void close()override{QIODevice::close();}
 qint64 bytesToWrite()const override{return 0;}
protected:
 qint64 readData(char*,qint64)override{return 0;}
 qint64 writeData(const char *data,qint64 size)override{output.append(data,size);return size;}
};
class AisPacketCore:public tNMEA2000 {
public:
 QList<QByteArray> frames;
 AisPacketCore(){SetMode(N2km_SendOnly,42);OpenScheduler=tN2kScheduler(0);Open();OpenScheduler=tN2kScheduler(0);Open();}
protected:
 bool CANOpen()override{return true;}
 bool CANGetFrame(unsigned long &,unsigned char &,unsigned char *)override{return false;}
 bool CANSendFrame(unsigned long,unsigned char n,const unsigned char *data,bool)override{frames.append(QByteArray(reinterpret_cast<const char*>(data),n));return true;}
};
class AisPublishingTests:public QObject {
 Q_OBJECT
private slots:
 void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");s.setValue("Vessel/MMSI",211000001u);s.setValue("Nmea0183/Nmea0183Enabled",false);}
 void canonicalStaticAndContexts(){
  auto a=fields(AisClassBStatic129809::parameters());a["Name"]="TEST VESSEL  A";
  auto d=SignalKPublisher::aisDelta(129809,a,{},"fixture");QCOMPARE(d["context"].toString(),"vessels.urn:mrn:imo:mmsi:211000001");QCOMPARE(paths(d)[""].toObject()["name"].toString(),"TEST VESSEL  A");
  auto b=fields(AisClassBStatic129810::parameters());b["Callsign"]="AB1CD";b["VesselType"]="37";b["Length"]=QString::number(35*.3048,'g',17);b["Beam"]=QString::number(12*.3048,'g',17);b["PosRefStbd"]="1";b["PosRefBow"]="2";
  d=SignalKPublisher::aisDelta(129810,b,{},"fixture");auto v=paths(d);near(v["design.length"].toObject()["overall"].toDouble(),35*.3048);near(v["design.beam"].toDouble(),12*.3048);near(v["sensors.ais.fromCenter"].toDouble(),12*.3048/2-1);QCOMPARE(v["design.aisShipType"].toObject()["name"].toString(),"Pleasure");QCOMPARE(v[""].toObject()["communication"].toObject()["callsignVhf"].toString(),"AB1CD");QVERIFY(!v.contains("Vendor"));
  b["MothershipID"]="211000003";QCOMPARE(paths(SignalKPublisher::aisDelta(129810,b,{},"fixture"))[""].toObject()["mothershipMmsi"].toString(),"211000003");
  b["UserID"]="211000002";QCOMPARE(SignalKPublisher::aisDelta(129810,b,{},"fixture")["context"].toString(),"vessels.urn:mrn:imo:mmsi:211000002");
  for(const QString invalid:{"0","4294967295","001234567","123456789","987654321","bad"}){b["UserID"]=invalid;QVERIFY(SignalKPublisher::aisDelta(129810,b,{},"fixture").isEmpty());}
 }
 void unavailableZeroAndTimestamps(){
  auto f=fields(AisClassBPosition129039::parameters());const auto t=QDateTime::fromString("2026-10-19T12:34:56.789Z",Qt::ISODateWithMs);
  QHash<QString,QDateTime> times;for(const auto &k:{"Latitude","Longitude","SOG","COG","Heading"}){f[k]="0";times[k]=t;}
  auto d=SignalKPublisher::aisDelta(129039,f,times,"fixture");auto v=paths(d);QCOMPARE(v["navigation.position"].toObject()["latitude"].toDouble(),0.);QCOMPARE(v["navigation.speedOverGround"].toDouble(),0.);QCOMPARE(stamp(d,"navigation.position"),"2026-10-19T12:34:56.789Z");
  QTest::qWait(5);QCOMPARE(SignalKPublisher::aisDelta(129039,f,times,"fixture"),d);
  times["Longitude"]=t.addSecs(-20);QCOMPARE(stamp(SignalKPublisher::aisDelta(129039,f,times,"fixture"),"navigation.position"),t.addSecs(-20).toString(Qt::ISODateWithMs));
  f["SOG"]=QString::number(N2kDoubleNA);f["Latitude"]="nan";v=paths(SignalKPublisher::aisDelta(129039,f,times,"fixture"));QVERIFY(!v.contains("navigation.position"));QVERIFY(!v.contains("navigation.speedOverGround"));QVERIFY(v.contains("navigation.headingTrue"));
  v=paths(SignalKPublisher::aisDelta(129039,f,{},"fixture"));QVERIFY(!v.contains("navigation.headingTrue"));
 }
 void loopbackAuthorizationRecoveryAndSelfIsolation(){
  TestServer server;SignalKPublisher p;connect(&p,&SignalKPublisher::statusChanged,&p,[](const QString &s){qDebug().noquote()<<s;});p.configure(server.config());auto a=fields(AisClassBStatic129809::parameters());
  QVERIFY(!p.publishAis(129809,a,{}));server.writer=false;p.start();QTest::qWait(100);QVERIFY(!p.isReady());QVERIFY(server.deltas.isEmpty());p.stop();server.writer=true;p.start();QTRY_VERIFY_WITH_TIMEOUT(p.isReady(),3000);
  QVERIFY(p.publishAis(129809,a,{}));QTRY_COMPARE(server.deltas.size(),1);QCOMPARE(server.deltas.last()["context"].toString(),"vessels.urn:mrn:imo:mmsi:211000001");
  server.peer->abort();QTRY_VERIFY(!p.isReady());QVERIFY(!p.publishAis(129809,a,{}));QTRY_VERIFY_WITH_TIMEOUT(p.isReady(),4000);QCOMPARE(server.deltas.size(),1);a["UserID"]="211000002";QVERIFY(p.publishAis(129809,a,{}));QTRY_COMPARE(server.deltas.size(),2);
  // The server's actual self identity must never be polluted by a target update.
  p.stop();server.self="vessels.urn:mrn:imo:mmsi:211000002";p.start();QTRY_VERIFY_WITH_TIMEOUT(p.isReady(),3000);QSignalSpy status(&p,&SignalKPublisher::statusChanged);QVERIFY(!p.publishAis(129809,a,{}));QVERIFY(status.last().first().toString().contains("matches destination self"));p.stop();QTest::qWait(50);QCOMPARE(server.deltas.size(),2);
 }
 void scheduledRoutingAndLatestMovement(){
  TestServer server;AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Nmea2000/InterfaceType","SignalK");s.setValue("Nmea2000/Nmea2000Enabled",true);s.setValue("Nmea2000/SignalKServer",server.config().server);s.setValue("Nmea2000/SignalKSource","fixture");s.sync();QVERIFY(CredentialStore::write("SignalKToken","test-token"));
  MemoryCan can;Workspace w(&can);auto *p=w.add(),*other=w.add(211000002),*duplicate=w.add();
  w.controller->addWidgetAsComboboxTextChanged("129809","AIS Static A","Communication");auto *a=qobject_cast<AisClassBStatic129809*>(w.controller->layoutWidgets("Communication").last());a->setUserID(211000001);
  w.controller->addWidgetAsComboboxTextChanged("129810","AIS Static B","Communication");auto *b=qobject_cast<AisClassBStatic129810*>(w.controller->layoutWidgets("Communication").last());b->setUserID(211000001);b->setVesselType(37);b->setCallsign("TEST1");b->setMothershipID(211000003);b->setLength(10.668);b->setBeam(3.6576);
  for(const auto &entry:w.model->getPgnList())w.model->setPgnEnabled(entry.idX,false);
  for(auto *x:{p,other,duplicate}){x->setEnabled(true);w.model->setSendInterval(x->Id(),1000);}a->setEnabled(true);b->setEnabled(true);w.model->setSendInterval(a->Id(),1000);w.model->setSendInterval(b->Id(),1000);
  other->setLatitude(2);other->setLongitude(3);other->setSOG(4);w.start();w.sample(43.1,5.2);
  auto *worker=w.window.findChild<NmeaSenderWorker*>();auto *start=w.window.findChild<QAction*>("actionStart_Sending");QVERIFY(!start->isChecked());worker->sendNmeaMessage();QVERIFY(server.deltas.isEmpty());start->trigger();QTRY_VERIFY_WITH_TIMEOUT(worker->isSending(),3000);for(auto *timer:w.window.findChildren<QTimer*>())timer->stop();
  SenderLifecycleTests::at(*worker,0);QTRY_COMPARE(server.deltas.size(),5);QVERIFY(can.sent.isEmpty());
  for(const auto &d:server.deltas)QVERIFY(d["context"].toString()!="vessels.self");
  const auto before=server.deltas.size();for(int i=0;i<30;++i)w.sample(44.+i*.001,6.);QCoreApplication::processEvents();QCOMPARE(server.deltas.size(),before);SenderLifecycleTests::at(*worker,500);QCoreApplication::processEvents();QCOMPARE(server.deltas.size(),before);
  SenderLifecycleTests::at(*worker,1000);QTRY_COMPARE(server.deltas.size(),before+5);int positions=0;for(int i=before;i<server.deltas.size();++i)if(server.deltas[i]["context"].toString().endsWith("211000001")){auto v=paths(server.deltas[i]);if(v.contains("navigation.position")){++positions;near(v["navigation.position"].toObject()["latitude"].toDouble(),44.029);near(v["navigation.speedOverGround"].toDouble(),6.);}}QCOMPARE(positions,2);
  duplicate->setEnabled(false);SenderLifecycleTests::at(*worker,2000);QTRY_COMPARE(server.deltas.size(),before+9);
  // Actual Manual simulator advancement and Autopilot 1x/2x/10x use the same
  // authoritative model and scheduler; no direct movement-to-publisher connection.
  a->setEnabled(false);b->setEnabled(false);other->setEnabled(false);
  auto latest=[&](const QString &path){QJsonValue v;for(const auto &d:server.deltas)if(d["context"].toString().endsWith("211000001")){auto m=paths(d);if(m.contains(path))v=m[path];}return v;};
  w.movement->setTransmissionGear(N2kTG_Forward);w.movement->setTransmissionGearStbd(N2kTG_Forward);
  w.movement->setThrottlePosition(50);w.movement->setThrottlePositionStbd(50);
  const auto old=p->Latitude();QVERIFY(QMetaObject::invokeMethod(w.movement->findChild<MoveVesselSimulator*>(),"updateVesselPosition"));QVERIFY(p->Latitude()!=old);
  SenderLifecycleTests::at(*worker,3000);QTRY_COMPARE(latest("navigation.position").toObject()["latitude"].toDouble(),p->Latitude());
  w.manual()->click();QTemporaryDir routeDir(QDir::currentPath()+"/build/m229f/route-XXXXXX");QVERIFY(routeDir.isValid());QFile route(routeDir.filePath("route.gpx"));QVERIFY(route.open(QIODevice::WriteOnly));route.write("<gpx><rte><rtept lat='44' lon='-70'/><rtept lat='44.1' lon='-70'/></rte></gpx>");route.close();w.movement->findChild<QLineEdit*>("lineEditFilePath")->setText(route.fileName());
  auto *ap=w.movement->findChild<AutoPilotSimulator*>();qint64 clock=0;ap->elapsedNow=[&]{return clock;};auto *apEnabled=w.movement->findChild<QCheckBox*>("checkBoxAutoPilotEnabled");apEnabled->click();QVERIFY(ap->isRunning());QCoreApplication::processEvents();ap->setSpeed(10);
  qint64 tick=4000;for(int factor:{1,2,10}){w.movement->findChild<QSpinBox*>("spinBoxMultSpeedByX")->setValue(factor);clock+=1000;ap->calculateNextCoordinate();QCoreApplication::processEvents();SenderLifecycleTests::at(*worker,tick);tick+=1000;QTRY_COMPARE(latest("navigation.speedOverGround").toDouble(),factor*10.);near(latest("navigation.position").toObject()["latitude"].toDouble(),p->Latitude());near(ap->getSpeed(),10.);}
  apEnabled->click();w.start();
  QKeyEvent stop(QEvent::KeyPress,Qt::Key_F12,Qt::NoModifier);QApplication::sendEvent(&w.window,&stop);QVERIFY(!worker->isSending());const auto stopped=server.deltas.size();w.sample(45.);SenderLifecycleTests::at(*worker,3000);QTest::qWait(50);QCOMPARE(server.deltas.size(),stopped);near(p->Latitude(),45.);
  w.window.findChild<QAction*>("actionViewer_Rx")->trigger();QVERIFY(!worker->isSending());QVERIFY(!w.window.findChild<SignalKPublisher*>()->isActive());QVERIFY(can.sent.isEmpty());
 }
 void modelProvenanceAndReplacement(){
  Workspace w;auto *p=w.add();QVERIFY(!w.model->parameterUpdatedAt(p->Id(),"Latitude").isValid());const auto initialized=w.model->parameterPublicationTime(p->Id(),"Latitude");QVERIFY(initialized.isValid());QTest::qWait(2);QCOMPARE(w.model->parameterPublicationTime(p->Id(),"Latitude"),initialized);w.start();auto *clock=w.window.findChild<GeneratorDateTime*>();clock->setDaysSince1970(20000);clock->setSecondsSinceMidnight(45296.789);w.model->recordParameterSample(clock->Id(),"SecondsSinceMidnight");w.sample();
  const auto t=w.model->parameterObservedAt(p->Id(),"Latitude");QCOMPARE(t,QDateTime::fromMSecsSinceEpoch(20000LL*86400000+45296789,Qt::UTC));const auto id=p->Id();
  w.sample(44.);QCOMPARE(w.model->parameterObservedAt(id,"Latitude"),t);clock->setEnabled(false);w.sample(45.);QVERIFY(w.model->parameterObservedAt(id,"Latitude")!=t);
  w.manual()->click();p->setLatitude(46.);QVERIFY(w.model->parameterObservedAt(id,"Latitude")!=t);
  confirm();QVERIFY(w.controller->loadLayoutFromJson("tests/fixtures/defaultLayout.json"));QVERIFY(!w.model->parameterObservedAt(id,"Latitude").isValid());
 }
 void nativeGuidanceThemes(){
  Workspace w;auto *p=w.add();
  w.controller->addWidgetAsComboboxTextChanged("129809","AIS Static A","Communication");auto *a=w.controller->layoutWidgets("Communication").last();
  w.controller->addWidgetAsComboboxTextChanged("129810","AIS Static B","Communication");auto *b=w.controller->layoutWidgets("Communication").last();
  const auto evidence=qEnvironmentVariable("M229F_EVIDENCE");if(!evidence.isEmpty())QDir().mkpath(evidence);
  for(const QString theme:{"Dark","Light"}){AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.sync();w.window.HotReloadSettings();
   for(auto *widget:QList<QWidget*>{a,b,p}){w.select(widget);QTest::qWait(30);bool guidance=false;for(auto *label:widget->findChildren<QLabel*>())if(label->text().contains("Signal K"))guidance=true;QVERIFY(guidance);if(!evidence.isEmpty())QVERIFY(w.window.grab().save(evidence+"/"+theme+"-"+QString::number(widget->property("Pgn").toInt())+".png"));}
   QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
  }
 }
 void actisenseFramesAndFastPackets(){
  const QList<int> pgns{129809,129810,129039};QList<QMap<QString,QString>> configs{fields(AisClassBStatic129809::parameters()),fields(AisClassBStatic129810::parameters()),fields(AisClassBPosition129039::parameters())};
  for(const QString preamble:{"0x94","0x93"}) {
   auto *port=new AisMemoryPort;Nmea2000Handler h(nullptr,port);h.SetInterfaceType("Actisense");QVERIFY(h.InitializeActisense("MEMORY",QSerialPort::Baud115200,preamble));
   for(int i=0;i<pgns.size();++i){port->output.clear();QVERIFY(h.sendPgn(pgns[i],wire(configs[i])));auto framed=port->output;QCOMPARE(framed.left(2),QByteArray::fromHex("1002"));QCOMPARE(framed.right(2),QByteArray::fromHex("1003"));
    QByteArray body;for(int j=2;j<framed.size()-2;++j){body+=framed[j];if(quint8(framed[j])==16){++j;QCOMPARE(quint8(framed[j]),quint8(16));}}
    QCOMPARE(quint8(body[0]),quint8(preamble=="0x94"?0x94:0x93));QCOMPARE(int(quint8(body[3]))+(int(quint8(body[4]))<<8)+(int(quint8(body[5]))<<16),pgns[i]);
    const int offset=preamble=="0x94"?8:13;QCOMPARE(int(quint8(body[offset-1])),i==1?35:27);unsigned sum=0;for(auto c:body)sum+=quint8(c);QCOMPARE(sum&255u,0u);
    tN2kMsg expected;QVERIFY(i==0?AisClassBStatic129809::encode(configs[i],expected):i==1?AisClassBStatic129810::encode(configs[i],expected):AisClassBPosition129039::encode(configs[i],expected));QCOMPARE(body.mid(offset,expected.DataLen),QByteArray(reinterpret_cast<const char*>(expected.Data),expected.DataLen));
    AisPacketCore core;QVERIFY(core.SendMsg(expected));QCOMPARE(core.frames.size(),i==1?6:4);QCOMPARE(quint8(core.frames[0][1]),quint8(expected.DataLen));QByteArray payload=core.frames[0].mid(2);for(int f=1;f<core.frames.size();++f){QCOMPARE(quint8(core.frames[f][0])&31,f);payload+=core.frames[f].mid(1);}QCOMPARE(payload.left(expected.DataLen),QByteArray(reinterpret_cast<const char*>(expected.Data),expected.DataLen));
   }
   h.ShutdownActisense();const auto before=port->output;QVERIFY(!h.sendPgn(129809,wire(configs[0])));QCOMPARE(port->output,before);
  }
 }
 void configuredNmea2000Only(){
  TestServer server;MemoryCan can;Nmea2000Handler h(nullptr,nullptr,&can);h.SetInterfaceType("SocketCan");h.InitializeSocketCan({"testcan",42});QTRY_VERIFY(h.IsCanInterfaceOpen());
  auto a=fields(AisClassBStatic129809::parameters()),b=fields(AisClassBStatic129810::parameters()),p=fields(AisClassBPosition129039::parameters());
  for(const auto &pair:QList<QPair<int,QMap<QString,QString>>>{{129809,a},{129810,b},{129039,p}}){QVERIFY(h.sendPgn(pair.first,wire(pair.second)));QCOMPARE(can.sent.last().PGN,quint32(pair.first));QCOMPARE(can.sent.last().Priority,quint8(pair.first==129039?4:6));}
  QCOMPARE(can.sent.size(),3);QVERIFY(server.deltas.isEmpty());h.setMode(Viewer);QVERIFY(!h.sendPgn(129809,wire(a)));h.SetInterfaceType("SignalK");h.setMode(Sender);QVERIFY(!h.sendPgn(129809,wire(a)));QCOMPARE(can.sent.size(),3);
 }
};
int main(int argc,char **argv){QDir().mkpath(QDir::currentPath()+"/build/m229f");QTemporaryDir profile(QDir::currentPath()+"/build/m229f/profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));AisPublishingTests tests;const int result=QTest::qExec(&tests,argc,argv);CredentialStore::remove("SignalKToken");return result;}
#include "ais_publishing_tests.moc"
