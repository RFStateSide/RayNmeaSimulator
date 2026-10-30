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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QScopeGuard>
#include <QAction>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QGeoCoordinate>
#include <functional>
#include <cmath>
#include "datastructs.h"
#include "environmentalmotion.h"
// Deterministic advancement of the existing simulation loops, without new timers.
#define private public
#include "autopilotsimulator.h"
#include "movevesselsimulator.h"
#undef private
#include "mainwindow.h"
#include "dialogsetup.h"
#include "preferences.h"
#include "appsettings.h"
#include "generatorvesselmovement.h"
#include "generatorweather.h"
#include "generatorengine.h"
#include "pgntreecontroller.h"
#include "signalk_testserver.h"
#include "themes.h"
#include "N2kMessages.h"
#include "nmeasenderworker.h"
#include "nmea0183handler.h"
namespace {
class OfflineProxy final:public QNetworkProxyFactory {
 QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery&q)override{
  if(q.peerHostName()=="127.0.0.1" || q.peerHostName()=="localhost")return {QNetworkProxy(QNetworkProxy::NoProxy)};
  return {QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9)};
 }
};
class CapturePort final:public QSerialPort {
public:
 QByteArray output;
 bool open(OpenMode mode)override{return QIODevice::open(mode|QIODevice::Unbuffered);}
 void close()override{QIODevice::close();}
 qint64 bytesToWrite()const override{return 0;}
protected:
 qint64 writeData(const char*d,qint64 n)override{output.append(d,n);return n;}
};
void near(double a,double b,double tolerance=1e-8){QVERIFY2(std::abs(a-b)<tolerance,qPrintable(QString("%1 != %2").arg(a,0,'g',17).arg(b,0,'g',17)));}
void confirm(){QTimer::singleShot(0,[]{for(auto*w:QApplication::topLevelWidgets())if(auto*b=qobject_cast<QMessageBox*>(w))b->button(QMessageBox::Yes)->click();});}
EnvironmentalConditions conditions(){return {true,true,20,3*M_PI/2,2,8,0};}
struct Workspace {
 MainWindow window;MainController*c=window.findChild<MainController*>();
 PgnTableModel*model=window.findChild<PgnTableController*>()->getPgnTableModel();
 GeneratorVesselMovement*movement=nullptr;GeneratorWeather*weather=nullptr;
 Workspace(){window.resize(1400,1000);window.show();confirm();c->loadLayoutFromJson(":/defaults/defaultLayout.json");movement=window.findChild<GeneratorVesselMovement*>();weather=window.findChild<GeneratorWeather*>();}
 void enable(){MyVessel v;v.EnvironmentalInfluence=true;c->setMyVessel(v);}
 void start(){for(auto*e:window.findChildren<GeneratorEngine*>()){e->setEngineProfile("MerCruiser 7.4 MPI");e->setEngineRunning(true);}movement->setHeadingReference(N2khr_true);movement->setRudderPosition(0);movement->findChild<QCheckBox*>("checkBoxManualMovementEnabled")->click();movement->setTransmissionGear(N2kTG_Forward);movement->setTransmissionGearStbd(N2kTG_Forward);movement->setThrottlePosition(40);movement->setThrottlePositionStbd(40);movement->findChild<QSlider*>("horizontalSliderHeading")->setValue(0);}
 void tick(int n=1){for(int i=0;i<n;++i)QMetaObject::invokeMethod(movement->findChild<MoveVesselSimulator*>(),"updateVesselPosition",Qt::DirectConnection);}
};
}
class EnvironmentMotionTests:public QObject {
 Q_OBJECT
private slots:
 void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");}
 void offMatchesApprovedBaseline(){
  QFile f(QFINDTESTDATA("data/m219-movement-off.json"));QVERIFY(f.open(QIODevice::ReadOnly));auto baseline=QJsonDocument::fromJson(f.readAll()).object();
  QCOMPARE(baseline["baseline"].toString(),QString("cff48bff31972af429f2b2cc12875bec02dda240"));
  MoveVesselSimulator m;MyVessel v;m.setMyVessel(v);m.setLastLatLong(42,-70);m.setHeading(350);m.setEnabled(true);m.setGearPort(N2kTG_Forward);m.setGearStarboard(N2kTG_Forward);m.environment.source=[]{return conditions();};
  VesselStatus s;connect(&m,&MoveVesselSimulator::vesselStatusUpdated,this,[&](const VesselStatus&x){s=x;});const auto frames=baseline["manual"].toArray();
  for(int i=0;i<20;++i){m.setThrottlePositionPort(i<5?20:i<10?70:40);m.setThrottlePositionStarboard(i<5?20:i<10?60:40);m.setRudderPositionPort(i<10?5:-10);m.setRudderPositionStarboard(i<10?5:-10);if(i==15){m.setGearPort(N2kTG_Reverse);m.setGearStarboard(N2kTG_Reverse);}m.updateVesselPosition();QList<double>actual{s.latitude,s.longitude,s.heading,s.sog,s.pitch,s.roll,s.heave,s.rudderPosition,s.rateOfTurn};auto expected=frames[i].toArray();for(int j=0;j<actual.size();++j)near(actual[j],expected[j].toDouble());QCOMPARE(s.groundSpeed,-1.0);}
  m.setEnabled(false);AutoPilotSimulator a;QGeoCoordinate p(42,-70);a.route={{p,"A"},{p.atDistanceAndAzimuth(100,0),"B"},{p.atDistanceAndAzimuth(100,0).atDistanceAndAzimuth(100,90),"C"}};a.setSpeed(13);a.start();const auto ap=baseline["autopilot"].toArray();
  for(int i=0;i<20;++i){a.advance(1);auto v=a.vesselWpBearing;QList<double>actual{a.position.latitude(),a.position.longitude(),v.CourseRadians,v.DistanceToWaypointM,v.Xte,v.TotalDistanceTraveledM,v.WaypointClosingVelocity,double(v.OriginWaypointNumber),double(v.DestinationWaypointNumber)};auto expected=ap[i].toArray();for(int j=0;j<actual.size();++j)near(actual[j],expected[j].toDouble(),1e-6);QCOMPARE(v.GroundSpeed,-1.0);}
 }
 void windCardinals_data(){QTest::addColumn<double>("from");QTest::addColumn<double>("to");QTest::newRow("north")<<0.<<180.;QTest::newRow("east")<<90.<<270.;QTest::newRow("south")<<180.<<0.;QTest::newRow("west")<<270.<<90.;}
 void windCardinals(){QFETCH(double,from);QFETCH(double,to);EnvironmentalMotion e;e.vessel.Length=10;e.vessel.Beam=3.9;e.enabled=true;e.source=[=]{auto c=conditions();c.wavesValid=false;c.windFrom=qDegreesToRadians(from);return c;};EnvironmentalMotion::Result r;for(int i=0;i<240;++i)r=e.advance(.5,0,0);near(r.speed,.5,1e-6);near(r.course,to,1e-6);auto p=EnvironmentalMotion::integrate(QGeoCoordinate(89.9,179.999),r,100);QVERIFY(p.isValid());QVERIFY(p.longitude()>=-180&&p.longitude()<=180);}
 void groundTrackReverseAndHeading(){MoveVesselSimulator m;MyVessel v;m.setMyVessel(v);m.setHeading(0);m.setLastLatLong(42,-70);m.environment.enabled=true;m.environment.source=[]{auto c=conditions();c.wavesValid=false;return c;};m.setGearPort(N2kTG_Forward);m.setGearStarboard(N2kTG_Forward);m.setThrottlePositionPort(50);m.setThrottlePositionStarboard(50);VesselStatus s;connect(&m,&MoveVesselSimulator::vesselStatusUpdated,this,[&](const VesselStatus&x){s=x;});m.setEnabled(true);for(int i=0;i<120;++i)m.updateVesselPosition();near(s.heading,0);QVERIFY(s.groundCourse>0&&s.groundCourse<10);QVERIFY(s.longitude>-70);QVERIFY(s.groundSpeed>s.sog);m.setGearPort(N2kTG_Reverse);m.setGearStarboard(N2kTG_Reverse);m.updateVesselPosition();near(s.heading,0);QVERIFY(s.groundCourse>170&&s.groundCourse<180);QVERIFY(s.sog<0);}
 void waveDirectionsAndBounds(){
  for(double from:{0.,90.,180.}){EnvironmentalMotion e;e.enabled=true;e.source=[=]{auto c=conditions();c.windValid=false;c.waveFrom=qDegreesToRadians(from);return c;};double pitch=0,roll=0,heave=0;for(int i=0;i<400;++i){const auto r=e.advance(.5,0,0);pitch=std::max(pitch,std::abs(r.pitch));roll=std::max(roll,std::abs(r.roll));heave=std::max(heave,std::abs(r.heave));QVERIFY(std::abs(r.pitch)<=10&&std::abs(r.roll)<=15&&std::abs(r.heave)<=1.5);}QVERIFY(heave>.1);if(from==90){QVERIFY(roll>1);QVERIFY(pitch<1e-6);}else{QVERIFY(pitch>1);QVERIFY(roll<1e-6);}}
  EnvironmentalMotion head,following;head.enabled=following.enabled=true;head.source=[]{auto c=conditions();c.windValid=false;return c;};following.source=[]{auto c=conditions();c.windValid=false;c.waveFrom=M_PI;return c;};for(int i=0;i<100;++i){auto a=head.advance(.5,0,0),b=following.advance(.5,0,0);near(a.pitch,-b.pitch);near(a.heave,b.heave);}
 }
 void windOnlyPreservesExistingAttitude(){
  MoveVesselSimulator m;MyVessel v;v.Draft=20;v.Displacement=100;m.setMyVessel(v);m.setHeading(0);m.setGearPort(N2kTG_Forward);m.setGearStarboard(N2kTG_Forward);m.setThrottlePositionPort(50);m.setThrottlePositionStarboard(50);m.setRudderPositionPort(40);m.setRudderPositionStarboard(40);VesselStatus s;connect(&m,&MoveVesselSimulator::vesselStatusUpdated,this,[&](const VesselStatus&x){s=x;});m.setEnabled(true);m.updateVesselPosition();const double baseline=s.heave;QVERIFY(baseline>3);m.environment.enabled=true;m.environment.source=[]{auto c=conditions();c.wavesValid=false;return c;};m.updateVesselPosition();near(s.heave,baseline);
  GeneratorVesselMovement widget(1,"Vessel",nullptr);widget.setPitch(.2);widget.setRoll(-.1);widget.setHeave(.3);VesselWpBearing nav;nav.GroundSpeed=2;nav.WaveHeave=N2kDoubleNA;widget.VesselWpBearingChanged(nav);near(widget.Pitch(),.2);near(widget.Roll(),-.1);near(widget.Heave(),.3);
 }
 void transitionsPeriodZeroAndInvalid(){EnvironmentalMotion e;e.enabled=true;auto c=conditions();e.source=[&]{return c;};double previous=0;for(int i=0;i<100;++i){auto r=e.advance(.5,0,0);QVERIFY(std::abs(r.east-previous)<.06);previous=r.east;}c.windSpeed=1e6;c.waveHeight=1e6;c.wavePeriod=.01;auto r=e.advance(.5,0,0);QVERIFY(r.speed<=1.5);QVERIFY(std::abs(r.heave)<=1.5);e.enabled=false;double before=r.speed;for(int i=0;i<300;++i){r=e.advance(.5,0,0);QVERIFY(r.speed<=before+1e-8);before=r.speed;}QVERIFY(!e.active());e.enabled=true;c={true,true,0,0,0,8,0};r=e.advance(.5,0,0);near(r.speed,0);QCOMPARE(r.course,-1.0);near(r.pitch,0);near(r.roll,0);near(r.heave,0);c={};QVERIFY(!e.advance(.5,0,0).applied);}
 void manualWeatherAndLivePreference(){Workspace w;QVERIFY(w.weather&&w.movement);QCOMPARE(w.weather->findChild<QComboBox*>("comboBoxApiSource")->currentIndex(),0);w.weather->setWindSpeed(20);w.weather->setWindAngle(3*M_PI/2);w.weather->setWaveHeight(2);w.weather->setDominantWavePeriod(8);w.weather->setMeanWaveDirection(0);w.enable();w.start();w.tick(120);near(w.movement->Heading(),0);QVERIFY(w.movement->COG()>0);QVERIFY(w.movement->Roll()==0);QVERIFY(w.movement->findChild<QLabel*>("environmentalMotionStatus")->text().contains("wind and waves"));
  auto*tree=w.window.findChild<PgnTreeController*>();QTRY_VERIFY(tree->getTreeModel()->indexFor(w.movement,"COG").isValid());near(w.model->getPgnDataToSend(w.movement->Id()).paramDetails.isEmpty()?qQNaN():w.movement->Heading(),0);QVERIFY(w.model->parameterUpdatedAt(w.movement->Id(),"COG").isValid());QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
  w.movement->findChild<QSlider*>("horizontalSliderHeading")->setValue(1234);w.tick(10);near(qRadiansToDegrees(w.movement->Heading()),123.4);near(w.movement->findChild<Compass*>()->heading(),123.4,.0001);
  const auto throttle=w.movement->ThrottlePosition();MyVessel v;w.c->setMyVessel(v);w.tick(300);near(w.movement->COG(),w.movement->Heading());near(w.movement->ThrottlePosition(),throttle);QVERIFY(w.movement->findChild<QCheckBox*>("checkBoxManualMovementEnabled")->isChecked());
 }
 void providerFreshnessRetentionAndMissing(){Workspace w;w.enable();auto*c=w.movement->findChild<MoveVesselSimulator*>();QVERIFY(c);w.weather->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(2);
  WeatherObservation a;a.provider="NOAA NDBC";a.station="A";a.observed=a.retrieved=QDateTime::currentDateTimeUtc();a.maximumAgeSeconds=1;a.windSpeed=12;a.windAngle=3*M_PI/2;a.waveHeight=2;a.averageWavePeriod=7;a.meanWaveDirection=0;w.weather->acceptObservation(a);auto input=c->environment.source();QVERIFY(input.windValid&&input.wavesValid);near(input.wavePeriod,7);
  auto b=a;b.station="B";b.maximumAgeSeconds=7200;b.windSpeed=5;b.waveHeight=N2kDoubleNA;b.averageWavePeriod=N2kDoubleNA;b.meanWaveDirection=N2kDoubleNA;w.weather->acceptObservation(b);QVERIFY(w.weather->fieldOrigin("WaveHeight").retained);input=c->environment.source();QVERIFY(input.windValid&&input.wavesValid);QTest::qWait(1200);input=c->environment.source();QVERIFY(input.windValid&&!input.wavesValid);
  w.weather->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(1);b.provider="OpenWeatherMap";b.station.clear();b.windSpeed=9;b.observed=QDateTime::currentDateTimeUtc();w.weather->acceptObservation(b);input=c->environment.source();QVERIFY(input.windValid&&!input.wavesValid);w.weather->setWaveHeight(1);w.weather->setDominantWavePeriod(4);w.weather->setMeanWaveDirection(M_PI/2);QVERIFY(c->environment.source().wavesValid);
  w.c->setMode(Viewer);QVERIFY(!c->environment.source().windValid);const auto lat=w.movement->Latitude();w.tick();near(w.movement->Latitude(),lat);w.c->setMode(Sender);QVERIFY(!w.movement->findChild<QCheckBox*>("checkBoxManualMovementEnabled")->isChecked());
 }
 void gamepadWithEnvironmentalMotion(){
  QVERIFY(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)==0);const int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);QVERIFY(device>=0);
  const auto cleanup=qScopeGuard([device]{SDL_JoystickDetachVirtual(device);SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);});
  GeneratorVesselMovement w(1,"Vessel",nullptr);MyVessel vessel;vessel.EnvironmentalInfluence=true;w.setMyVessel(vessel);w.setEnvironmentalSource([]{return conditions();});w.setLatitude(42);w.setLongitude(-70);w.setHeadingReference(N2khr_true);w.setHeading(0);
  auto*list=w.findChild<QComboBox*>("comboBoxGamePadDevice");int index=list->findText("Virtual Controller");QVERIFY(index>=0);list->setCurrentIndex(index);w.findChild<QCheckBox*>("checkBoxGamePadEnabled")->click();w.findChild<QCheckBox*>("checkBoxManualMovementEnabled")->click();
  auto*g=w.findChild<GameController*>();g->buttonPressed(3);g->buttonPressed(1);g->axisMoved(1,-16384);g->axisMoved(3,-16384);auto*sim=w.findChild<MoveVesselSimulator*>();for(int i=0;i<80;++i)QVERIFY(QMetaObject::invokeMethod(sim,"updateVesselPosition",Qt::DirectConnection));near(w.Heading(),0);QVERIFY(w.COG()>0);QVERIFY(w.Longitude()>-70);
  g->axisMoved(0,32767);QMetaObject::invokeMethod(sim,"updateVesselPosition",Qt::DirectConnection);QVERIFY(w.Heading()!=0);g->axisMoved(0,0);w.findChild<QSlider*>("horizontalSliderHeading")->setValue(900);QMetaObject::invokeMethod(sim,"updateVesselPosition",Qt::DirectConnection);near(qRadiansToDegrees(w.Heading()),90);
  g->disconnected();const auto latitude=w.Latitude();QMetaObject::invokeMethod(sim,"updateVesselPosition",Qt::DirectConnection);near(w.Latitude(),latitude);near(w.SOG(),0);QVERIFY(!w.findChild<QCheckBox*>("checkBoxManualMovementEnabled")->isChecked());
 }
 void autopilotDriftAndOffTransition(){AutoPilotSimulator a;QGeoCoordinate p(42,-70);a.route={{p,"A"},{p.atDistanceAndAzimuth(10000,0),"B"}};a.setSpeed(5);a.environment.enabled=true;a.environment.source=[]{return conditions();};a.start();a.advance(60);QVERIFY(a.position.longitude()>-70);auto v=a.vesselWpBearing;QVERIFY(v.Xte>0);QVERIFY(v.CourseRadians!=v.HeadingRadians);QVERIFY(v.GroundSpeed>0);QVERIFY(std::abs(v.WaveHeave)>.001);QCOMPARE(v.DestinationWaypointNumber,1u);const auto before=a.position;a.environment.enabled=false;a.advance(1);QVERIFY(before.distanceTo(a.position)<8);a.advance(200);QCOMPARE(a.destinationIndex(),1);QVERIFY(!a.environment.active());QVERIFY(a.position.isValid());}
 void autopilotGroundSpeedIsNotPropulsionFeedback(){
  QTemporaryDir dir(QDir::currentPath()+"/build/m219-ap-XXXXXX");QVERIFY(dir.isValid());QFile route(dir.filePath("route.gpx"));QVERIFY(route.open(QIODevice::WriteOnly));route.write("<gpx><rte><rtept lat='42' lon='-70'/><rtept lat='42.1' lon='-70'/></rte></gpx>");route.close();
  GeneratorVesselMovement w(1,"Vessel",nullptr);MyVessel vessel;vessel.EnvironmentalInfluence=true;w.setMyVessel(vessel);w.setEnvironmentalSource([]{return conditions();});w.findChild<QLineEdit*>("lineEditFilePath")->setText(route.fileName());auto*enabled=w.findChild<QCheckBox*>("checkBoxAutoPilotEnabled");enabled->click();QVERIFY(enabled->isChecked());auto*ap=w.findChild<AutoPilotSimulator*>();QCoreApplication::processEvents();ap->setSpeed(5);
  for(int i=0;i<30;++i){ap->advance(1);QCoreApplication::processEvents();near(ap->getSpeed(),5);near(w.SOG(),ap->vesselWpBearing.GroundSpeed);near(w.Heading(),ap->vesselWpBearing.HeadingRadians);}
  QVERIFY(w.SOG()!=ap->getSpeed());QVERIFY(w.COG()!=w.Heading());QVERIFY(!w.findChild<QSlider*>("horizontalSliderHeading")->isEnabled());enabled->click();QVERIFY(!ap->running);near(w.SOG(),0);
 }
 void setupPersistenceLayoutAndLifetime(){Workspace w;auto*d=w.window.findChild<DialogSetup*>();QVERIFY(d);d->show();auto*check=d->findChild<QCheckBox*>("environmentalInfluence");QVERIFY(check);QVERIFY(!check->isChecked());check->setChecked(true);QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));AppSettings s(APP_COMPANY,APP_NAME);s.beginGroup("Vessel");QVERIFY(readVesselSettings(s).EnvironmentalInfluence);s.endGroup();QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonLoad_clicked"));QVERIFY(check->isChecked());d->close();
  QTemporaryDir dir(QDir::currentPath()+"/m219-layout-XXXXXX");auto file=dir.filePath("layout.json");QVERIFY(w.c->saveLayoutToJson(file));auto old=QPointer<GeneratorWeather>(w.weather);confirm();QVERIFY(w.c->loadLayoutFromJson(file));QVERIFY(old.isNull());w.weather=w.window.findChild<GeneratorWeather*>();w.movement=w.window.findChild<GeneratorVesselMovement*>();QVERIFY(w.weather&&w.movement);QVERIFY(!w.movement->findChild<QCheckBox*>("checkBoxManualMovementEnabled")->isChecked());w.enable();QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
  auto*sim=w.movement->findChild<MoveVesselSimulator*>();w.weather->setWindSpeed(20);w.weather->setWindAngle(0);QVERIFY(sim->environment.source().windValid);delete w.weather;QVERIFY(!sim->environment.source().windValid);
 }
 void existingOutputPathsAndStop(){
  Workspace w;w.enable();w.weather->setWindSpeed(20);w.weather->setWindAngle(3*M_PI/2);w.weather->setWaveHeight(2);w.weather->setDominantWavePeriod(8);w.weather->setMeanWaveDirection(M_PI/3);w.start();w.tick(100);w.movement->findChild<MoveVesselSimulator*>()->setEnabled(false);
  // Stop just the live timers for deterministic output snapshots, retaining the measured sample.
  const double heading=w.movement->Heading(),course=w.movement->COG(),speed=w.movement->SOG();
  w.c->addWidgetAsComboboxTextChanged("127252","Heave","Navigation");
  w.c->addWidgetAsComboboxTextChanged("HDT","True Heading","Navigation");
  w.c->addWidgetAsComboboxTextChanged("VTG","Course / Speed","Navigation");
  QCoreApplication::processEvents();
  // Feed the actual existing generator signals into their guarded instrument bindings.
  w.movement->HeadingChanged();w.movement->COGChanged();w.movement->SOGChanged();w.movement->HeaveChanged();QCoreApplication::processEvents();
  TestServer server;Nmea2000Handler fixtureHandler;auto*handler=&fixtureHandler;handler->setMode(Sender);handler->SetInterfaceType("SignalK");handler->ConfigureSignalK(server.config());
  auto*port=new CapturePort;Nmea0183Handler serial(nullptr,port);QVERIFY(serial.Initialize("M219_IN_MEMORY"));QVERIFY(serial.OpenDevice());
  NmeaSenderWorker sender;sender.setPgnTableController(w.window.findChild<PgnTableController*>());
  connect(&sender,&NmeaSenderWorker::sendPgn,handler,&Nmea2000Handler::sendPgn);connect(&sender,&NmeaSenderWorker::sendSnt,&serial,&Nmea0183Handler::sendSnt);
  handler->StartSignalKSending();QTRY_VERIFY(handler->IsSignalKReady());sender.startSending();QTRY_VERIFY(!server.deltas.isEmpty());QTRY_VERIFY(port->output.contains("VTG"));QTRY_VERIFY(port->output.contains("HDT"));
  bool sawCourse=false,sawSpeed=false,sawHeave=false;
  for(const auto&delta:server.deltas)for(const auto&update:delta["updates"].toArray())for(const auto&value:update.toObject()["values"].toArray()){
    const auto v=value.toObject();if(v["path"]=="environment.heave"){near(v["value"].toDouble(),w.movement->Heave(),.011);sawHeave=true;}if(v["path"]=="navigation.courseOverGroundTrue"){near(v["value"].toDouble(),course,.00011);sawCourse=true;}
    if(v["path"]=="navigation.speedOverGround"){near(v["value"].toDouble(),speed,.011);sawSpeed=true;}
  }
  QVERIFY(sawCourse&&sawSpeed&&sawHeave);QVERIFY(course!=heading);
  for(const auto&line:port->output.split('\n'))if(line.startsWith('$')){const int star=line.indexOf('*');QVERIFY(star>0);unsigned char checksum=0;for(int i=1;i<star;++i)checksum^=static_cast<unsigned char>(line[i]);QCOMPARE(line.mid(star+1,2).toUInt(nullptr,16),unsigned(checksum));const auto fields=line.left(star).split(',');if(fields[0].endsWith("HDT"))near(fields[1].toDouble(),qRadiansToDegrees(heading),.02);if(fields[0].endsWith("VTG")){near(fields[1].toDouble(),qRadiansToDegrees(course),.02);near(fields[5].toDouble(),speed/0.514444444,.051);}}
  sender.stopSending();handler->StopSignalKSending();const auto count=server.deltas.size();QTest::qWait(200);QCOMPARE(server.deltas.size(),count);QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
 void outputAndNativePresentation(){Workspace w;w.weather->setWindSpeed(20);w.weather->setWindAngle(3*M_PI/2);w.weather->setWaveHeight(3);w.weather->setDominantWavePeriod(8);w.weather->setMeanWaveDirection(M_PI/3);w.enable();w.start();w.tick(80);tN2kMsg nav,att;SetN2kPGN129026(nav,0,N2khr_true,w.movement->COG(),w.movement->SOG());SetN2kPGN127257(att,0,w.movement->Heading(),w.movement->Pitch(),w.movement->Roll());
  QJsonObject mapped;for(auto m:{nav,att})for(const auto&v:SignalKPublisher::values(m)){const auto o=v.toObject();mapped[o["path"].toString()]=o["value"];}
  near(mapped["navigation.courseOverGroundTrue"].toDouble(),w.movement->COG(),.00011);near(mapped["navigation.speedOverGround"].toDouble(),w.movement->SOG(),.011);QVERIFY(mapped.contains("navigation.attitude"));near(mapped["navigation.attitude"].toObject()["pitch"].toDouble(),w.movement->Pitch(),.00011);near(mapped["navigation.attitude"].toObject()["roll"].toDouble(),w.movement->Roll(),.00011);
  auto*tree=w.window.findChild<PgnTreeController*>();QTRY_VERIFY(tree->getTreeModel()->indexFor(w.movement,"COG").isValid());tree->getTreeView()->setCurrentIndex(tree->getTreeModel()->indexFor(w.movement,"COG"));tree->getTreeView()->setFocus();auto selection=tree->getTreeView()->currentIndex();w.tick(100);QCoreApplication::processEvents();QCOMPARE(tree->getTreeView()->currentIndex(),selection);
  auto evidence=qEnvironmentVariable("M219_EVIDENCE");for(const auto&theme:{"Dark","Light"}){qApp->setPalette(Themes::palette(theme));qApp->setStyleSheet(Themes::styleSheet(theme));QTest::qWait(40);if(!evidence.isEmpty())QVERIFY(w.window.grab().save(evidence+"/"+theme+"-motion.png"));auto*d=w.window.findChild<DialogSetup*>();d->show();QTest::qWait(20);if(!evidence.isEmpty())QVERIFY(d->grab().save(evidence+"/"+theme+"-setup.png"));d->close();}
 }
};
int main(int argc,char**argv){QDir().mkpath("build");QTemporaryDir profile(QDir::currentPath()+"/build/environment-motion-XXXXXX");qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QList<QByteArray>args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*>ptr;for(auto&a:args)ptr<<a.data();int count=ptr.size();ptr<<nullptr;QApplication app(count,ptr.data());app.setStyle("Fusion");QNetworkProxyFactory::setApplicationProxyFactory(new OfflineProxy);EnvironmentMotionTests tests;return QTest::qExec(&tests,argc,argv);}
#include "environment_motion_tests.moc"
