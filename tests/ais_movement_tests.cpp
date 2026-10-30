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
  if(!controller->loadLayoutFromJson("tests/fixtures/defaultLayout.json"))qFatal("Fixture Layout load failed");
  movement=window.findChild<GeneratorVesselMovement*>();if(!movement)qFatal("Missing movement fixture");
  for(auto *engine:window.findChildren<GeneratorEngine*>())engine->setEngineProfile("MerCruiser 7.4 MPI");
  for(auto *timer:window.findChildren<QTimer*>())timer->stop();
  window.resize(1500,1050);window.show();
 }
 AisClassBPosition129039 *add(uint id=123456789){
  controller->addWidgetAsComboboxTextChanged("129039","AIS Class B Position","Communication");
  auto *p=qobject_cast<AisClassBPosition129039*>(controller->layoutWidgets("Communication").last());p->setUserID(id);return p;
 }
 QCheckBox *manual(){return movement->findChild<QCheckBox*>("checkBoxManualMovementEnabled");}
 void start(){manual()->click();QVERIFY(manual()->isChecked());}
 void sample(double latitude=43.123456789,double speed=4.123456789){
  VesselStatus status;status.latitude=latitude;status.longitude=-70.123456789;status.heading=125.123456789;
  status.sog=speed;status.groundSpeed=speed;status.groundCourse=132.123456789;movement->setCurrentVesselStatus(status);
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
class AisMovementTests:public QObject {
 Q_OBJECT
private slots:
 void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");s.setValue("Vessel/MMSI",123456789u);}
 void lifecycleAndLateCreation(){
  Workspace w;auto *p=w.add();p->setLatitude(1.234567891);QVERIFY(!p->navigationControlled());
  w.movement->setEnabled(false);w.start();w.sample();QCoreApplication::processEvents();QCOMPARE(p->Latitude(),1.234567891);
  w.movement->setEnabled(true);QVERIFY(p->navigationControlled());near(p->Latitude(),w.movement->Latitude());
  w.sample(44.123456789);near(p->Latitude(),44.123456789);near(p->SOG(),4.123456789);
  auto *late=w.add();QVERIFY(late->navigationControlled());near(late->Latitude(),w.movement->Latitude());
  w.movement->setEnabled(false);const auto last=p->Latitude();w.sample(45.);QCoreApplication::processEvents();QVERIFY(!p->navigationControlled());QCOMPARE(p->Latitude(),last);
  p->setLatitude(2.);w.movement->setEnabled(true);near(p->Latitude(),45.);w.manual()->click();QVERIFY(!p->navigationControlled());const auto speed=p->SOG();w.sample(46.);QCoreApplication::processEvents();near(p->Latitude(),45.);near(p->SOG(),speed);
  w.start();near(p->Latitude(),46.);QPointer<AisClassBPosition129039> gone=late;w.remove(late,"Communication");QVERIFY(!gone);w.sample(47.);near(p->Latitude(),47.);
  w.remove(w.movement,"Generate");w.movement=nullptr;QVERIFY(!p->navigationControlled());p->setLatitude(3.);QCoreApplication::processEvents();QCOMPARE(p->Latitude(),3.);
  QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());QVERIFY(!p->isEnabled());
 }
 void identityAndMetadata(){
  Workspace w;auto *p=w.add(),*replica=w.add(),*target=w.add(987654321),*unconfigured=w.add(0);target->setLatitude(-1.);unconfigured->setLatitude(-2.);
  w.window.findChild<GeneratorDateTime*>()->setEnabled(false); // Explicitly preserve manual-time metadata without a clock.
  p->setSeconds(61);p->setSID(17);p->setRepeat(2);p->setRAIM(true);w.start();w.sample();QVERIFY(p->navigationControlled()&&replica->navigationControlled());QVERIFY(!target->navigationControlled()&&!unconfigured->navigationControlled());near(p->Latitude(),replica->Latitude());QCOMPARE(target->Latitude(),-1.);QCOMPARE(unconfigured->Latitude(),-2.);
  QCOMPARE(p->Seconds(),61);QCOMPARE(p->SID(),17);QCOMPARE(p->Repeat(),2);QVERIFY(p->RAIM());
  p->setUserID(222333444);QVERIFY(!p->navigationControlled());w.sample(44.);QCOMPARE(p->UserID(),222333444u);QCOMPARE(replica->UserID(),123456789u);near(replica->Latitude(),44.);
  AppSettings s(APP_COMPANY,APP_NAME);s.beginGroup("Vessel");auto vessel=readVesselSettings(s);vessel.MMSI=987654321;w.controller->setMyVessel(vessel);QVERIFY(!replica->navigationControlled());QVERIFY(target->navigationControlled());near(target->Latitude(),44.);QCOMPARE(replica->UserID(),123456789u);
  vessel.MMSI=0;w.controller->setMyVessel(vessel);QVERIFY(!target->navigationControlled());QVERIFY(!unconfigured->navigationControlled());
 }
 void referenceValidityAndPrecision(){
  Workspace w;auto *p=w.add();w.start();w.movement->setHeadingReference(N2khr_magnetic);w.movement->setVariation(Convert::DegreesToRadians(10));w.movement->setDeviation(Convert::DegreesToRadians(5));w.movement->setHeading(Convert::DegreesToRadians(90));w.movement->setCOG(Convert::DegreesToRadians(120));QCoreApplication::processEvents();near(p->Heading(),Convert::DegreesToRadians(105));near(p->COG(),Convert::DegreesToRadians(130));
  w.movement->setVariation(N2kDoubleNA);QCoreApplication::processEvents();QCOMPARE(p->Heading(),N2kDoubleNA);QCOMPARE(p->COG(),N2kDoubleNA);
  w.sample(42.123456789123);for(int n=0;n<25;++n){QCoreApplication::processEvents();near(p->Latitude(),42.123456789123);near(p->SOG(),w.movement->SOG());}
  w.movement->setLatitude(91.);w.movement->setLongitude(N2kDoubleNA);w.movement->setSOG(-1);QCoreApplication::processEvents();QCOMPARE(p->Latitude(),N2kDoubleNA);QCOMPARE(p->Longitude(),N2kDoubleNA);QCOMPARE(p->SOG(),N2kDoubleNA);
  w.movement->setLatitude(0.);w.movement->setLongitude(0.);w.movement->setSOG(0.);QCoreApplication::processEvents();QCOMPARE(p->Latitude(),0.);QCOMPARE(p->Longitude(),0.);QCOMPARE(p->SOG(),0.);
 }
 void pendingEditsAndInspection(){
  Workspace w;auto *p=w.add();QVERIFY(w.controller->selectLayoutWidget(p,"Latitude"));auto prefsBefore=p->UserPrefs();prefsBefore.latLongUnitIndex=0;p->setUserPrefs(prefsBefore);auto *latitude=p->findChild<QLineEdit*>("Latitude");latitude->setFocus();latitude->setText("12.3456789123");latitude->setModified(true);QVERIFY(QMetaObject::invokeMethod(latitude,"textEdited",Q_ARG(QString,latitude->text())));
  w.start();w.sample();QVERIFY(latitude->isReadOnly());QCOMPARE(latitude->text(),QString("12.3456789123"));near(p->Latitude(),w.movement->Latitude());
  w.model->setCurrentPgn(p->Id());QVERIFY(!(w.model->flags(w.model->index(3,1))&Qt::ItemIsEditable));QVERIFY(!w.model->setData(w.model->index(3,1),"1.0"));QVERIFY(w.model->flags(w.model->index(2,1))&Qt::ItemIsEditable);
  EnumComboBoxDelegate delegate;QStyleOptionViewItem option;auto *draft=qobject_cast<QLineEdit*>(delegate.createEditor(&w.window,option,w.model->index(4,1)));QVERIFY(draft);draft->setText("-69.123456789");draft->setModified(true);delegate.setEditorData(draft,w.model->index(4,1));QCOMPARE(draft->text(),QString("-69.123456789"));delegate.setModelData(draft,w.model,w.model->index(4,1));near(p->Longitude(),w.movement->Longitude());delete draft;
  auto *tree=w.window.findChild<PgnTreeController*>()->getTreeModel();QTRY_VERIFY(tree->indexFor(p,"Latitude").isValid());near(w.modelValue(p,"Latitude").toDouble(),p->Latitude());QVERIFY(w.model->parameterUpdatedAt(p->Id(),"Latitude").isValid());
  const auto stamp=w.model->parameterUpdatedAt(p->Id(),"Latitude");auto prefs=p->UserPrefs();prefs.latLongUnitIndex=(prefs.latLongUnitIndex+1)%3;p->setUserPrefs(prefs);QCOMPARE(w.model->parameterUpdatedAt(p->Id(),"Latitude"),stamp);
  QVERIFY(QMetaObject::invokeMethod(latitude,"editingFinished"));near(p->Latitude(),w.movement->Latitude());w.manual()->click();QVERIFY(!latitude->isReadOnly());QCOMPARE(latitude->text(),QString("12.3456789123"));QVERIFY(QMetaObject::invokeMethod(latitude,"editingFinished"));near(p->Latitude(),12.3456789123);
 }
 void manualMovementAndViewer(){
  Workspace w;auto *p=w.add();w.start();w.movement->setHeadingReference(N2khr_true);w.movement->setHeading(Convert::DegreesToRadians(125));w.movement->setTransmissionGear(N2kTG_Forward);w.movement->setTransmissionGearStbd(N2kTG_Forward);w.movement->setThrottlePosition(45);w.movement->setThrottlePositionStbd(45);
  const auto initial=w.movement->Latitude();for(int i=0;i<8;++i){QVERIFY(QMetaObject::invokeMethod(w.movement->findChild<MoveVesselSimulator*>(),"updateVesselPosition"));QCoreApplication::processEvents();near(p->Latitude(),w.movement->Latitude());near(p->COG(),w.movement->COG());near(p->SOG(),w.movement->SOG());near(p->Heading(),w.movement->Heading());}QVERIFY(p->Latitude()!=initial);
  w.controller->setMode(Viewer);const auto last=p->Latitude();QVERIFY(!p->navigationControlled());w.sample(55.);QCoreApplication::processEvents();QCOMPARE(p->Latitude(),last);w.controller->setMode(Sender);QVERIFY(!w.movement->navigationControlActive());QVERIFY(!p->navigationControlled());QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
 void autopilotMultiplier(){
  Workspace w;auto *p=w.add();QTemporaryDir dir;QFile route(dir.filePath("route.gpx"));QVERIFY(route.open(QIODevice::WriteOnly));route.write("<gpx><rte><rtept lat='42' lon='-70'/><rtept lat='42.1' lon='-70'/></rte></gpx>");route.close();w.movement->findChild<QLineEdit*>("lineEditFilePath")->setText(route.fileName());
  auto *ap=w.movement->findChild<AutoPilotSimulator*>();qint64 clock=0;ap->elapsedNow=[&]{return clock;};auto *enabled=w.movement->findChild<QCheckBox*>("checkBoxAutoPilotEnabled");enabled->click();QVERIFY(ap->isRunning());QCoreApplication::processEvents();ap->setSpeed(10);w.movement->findChild<QSpinBox*>("spinBoxMultSpeedByX")->setValue(2);clock=1000;ap->calculateNextCoordinate();QCoreApplication::processEvents();near(ap->totalDistanceTraveled,20.,0.001);near(p->SOG(),20.);near(p->Latitude(),w.movement->Latitude());near(p->Heading(),w.movement->Heading());near(p->COG(),w.movement->COG());QVERIFY(p->navigationControlled());
  w.movement->findChild<QSpinBox*>("spinBoxMultSpeedByX")->setValue(10);clock=2000;ap->calculateNextCoordinate();QCoreApplication::processEvents();near(ap->totalDistanceTraveled,120.,0.001);near(p->SOG(),100.);near(ap->getSpeed(),10.);const auto last=p->Latitude();enabled->click();QVERIFY(!p->navigationControlled());near(p->Latitude(),last);
 }
 void layoutAndSourceIsolation(){
  Workspace w;auto *p=w.add();w.start();w.sample(44.123456789);p->setEnabled(true);w.model->setSendInterval(p->Id(),1750);QTemporaryDir dir;const auto path=dir.filePath("layout.json");QVERIFY(w.controller->saveLayoutToJson(path));confirm();QVERIFY(w.controller->loadLayoutFromJson(path));p=w.window.findChild<AisClassBPosition129039*>();w.movement=w.window.findChild<GeneratorVesselMovement*>();QVERIFY(p&&w.movement);QVERIFY(!p->navigationControlled());near(p->Latitude(),44.123456789);QVERIFY(!w.movement->navigationControlActive());QVERIFY(p->isEnabled());QCOMPARE(w.model->findPgn(p->Id())->sendInterval,1750.);QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
  w.model->setSource(p->Id(),"23");w.start();w.sample(45.);QCoreApplication::processEvents();QVERIFY(!p->navigationControlled());near(p->Latitude(),44.123456789);
 }
 void scheduledMemoryOutput(){
  Workspace w;auto *p=w.add();auto *target=w.add(987654321);Q_UNUSED(target);w.start();w.sample();MemoryCan backend;Nmea2000Handler handler(nullptr,nullptr,&backend);handler.SetInterfaceType("SocketCan");handler.InitializeSocketCan({"testcan",42});QTRY_VERIFY(handler.IsCanInterfaceOpen());NmeaSenderWorker sender;sender.setPgnTableController(w.table);
  connect(&sender,&NmeaSenderWorker::sendPgn,&handler,[&](int pgn,const QStringList &fields){if(pgn==129039)handler.sendPgn(pgn,fields);});
  SenderLifecycleTests::at(sender,0);QVERIFY(backend.sent.isEmpty());sender.startSending();sender.findChild<QTimer*>()->stop();SenderLifecycleTests::at(sender,0);QVERIFY(backend.sent.isEmpty());p->setEnabled(true);w.model->setSendInterval(p->Id(),1000);SenderLifecycleTests::at(sender,0);QCOMPARE(backend.sent.size(),1);
  for(int i=1;i<=20;++i){w.sample(43.+i/1000.);SenderLifecycleTests::at(sender,i*40);}QCOMPARE(backend.sent.size(),1);SenderLifecycleTests::at(sender,1000);QCOMPARE(backend.sent.size(),2);QStringList decoded;QVERIFY(AisClassBPosition129039::decode(backend.sent.last(),decoded));near(decoded[7].toDouble(),p->Latitude(),1e-7);near(decoded[19].toDouble(),p->SOG(),.0051);sender.stopSending();w.sample(46.);SenderLifecycleTests::at(sender,10000);QCOMPARE(backend.sent.size(),2);near(p->Latitude(),46.);QVERIFY(w.movement->navigationControlActive());
 }
 void virtualGamepadMovement(){
  QVERIFY(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)==0);
  const int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);QVERIFY(device>=0);
  const auto detach=qScopeGuard([device]{SDL_JoystickDetachVirtual(device);SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);});
  Workspace w;auto *p=w.add();auto *list=w.movement->findChild<QComboBox*>("comboBoxGamePadDevice");const int index=list->findText("Virtual Controller");QVERIFY(index>=0);list->setCurrentIndex(index);
  auto *enabled=w.movement->findChild<QCheckBox*>("checkBoxGamePadEnabled");enabled->click();QVERIFY(enabled->isChecked());w.start();
  auto *pad=w.movement->findChild<GameController*>();QVERIFY(pad);pad->buttonPressed(3);pad->buttonPressed(1);pad->axisMoved(1,-32768);pad->axisMoved(3,-32768);
  QVERIFY(QMetaObject::invokeMethod(w.movement->findChild<MoveVesselSimulator*>(),"updateVesselPosition"));QCoreApplication::processEvents();QVERIFY(p->navigationControlled());QVERIFY(p->SOG()>0);near(p->Latitude(),w.movement->Latitude());near(p->Heading(),w.movement->Heading());
  pad->disconnected();QVERIFY(!p->navigationControlled());QVERIFY(!w.movement->navigationControlActive());QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
 void applicationStopWithActiveMovement(){
  AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Nmea2000/Nmea2000Enabled",true);settings.setValue("Nmea2000/InterfaceType","SocketCan");settings.setValue("Nmea2000/SocketCanInterface","m229fixture");settings.setValue("Nmea2000/SocketCanUniqueNumber",42);settings.sync();
  MemoryCan backend;Workspace w(&backend);auto *p=w.add();w.start();w.sample();p->setEnabled(true);w.select(p);
  auto *session=w.window.findChild<SocketCanSession*>();QVERIFY(session);session->service();auto *start=w.window.findChild<QAction*>("actionStart_Sending");QVERIFY(start->isEnabled());start->trigger();auto *sender=w.window.findChild<NmeaSenderWorker*>();QVERIFY(sender->isSending());sender->sendNmeaMessage();QVERIFY(std::any_of(backend.sent.begin(),backend.sent.end(),[](const tN2kMsg &m){return m.PGN==129039;}));
  QTest::keyClick(p,Qt::Key_F12);QVERIFY(!sender->isSending());QVERIFY(!start->isChecked());const int count=backend.sent.size();w.sample(48.);sender->sendNmeaMessage();QCoreApplication::processEvents();QCOMPARE(backend.sent.size(),count);near(p->Latitude(),48.);QVERIFY(p->navigationControlled());
 }
 void dateTimeObservationSource(){
  Workspace w;auto *p=w.add();auto *clock=w.window.findChild<GeneratorDateTime*>();QVERIFY(clock);w.start();
  for(auto *timer:w.window.findChildren<QTimer*>())timer->stop(); // Explicit samples only in this clock-ordering fixture.
  clock->setEnabled(true);clock->setDaysSince1970(20000);clock->setSecondsSinceMidnight(45296.75);w.model->recordParameterSample(clock->Id(),"SecondsSinceMidnight");w.sample();QCOMPARE(p->Seconds(),56);QVERIFY(p->observationTimeControlled());QVERIFY(p->findChild<QLineEdit*>("Seconds")->isReadOnly());
  QMap<QString,QString> packet;for(const auto &row:w.model->getPgnParams(p->Id()))packet[row[0]]=row[1];tN2kMsg message;QVERIFY(AisClassBPosition129039::encode(packet,message));QStringList decoded;QVERIFY(AisClassBPosition129039::decode(message,decoded));QCOMPARE(decoded[15],QString("56"));
  w.model->setCurrentPgn(p->Id());QVERIFY(!(w.model->flags(w.model->index(7,1))&Qt::ItemIsEditable));QVERIFY(!w.model->setData(w.model->index(7,1),"12"));
  clock->setSecondsSinceMidnight(45297.5);clock->setLocalOffset(-300);QCoreApplication::processEvents();QCOMPARE(p->Seconds(),56); // No new position sample from a clock tick or offset edit.
  w.sample(44.);QCOMPARE(p->Seconds(),57);
  clock->setEnabled(false);w.sample(45.);QCOMPARE(p->Seconds(),60);QVERIFY(!p->observationTimeControlled());
  clock->setEnabled(true);clock->setSecondsSinceMidnight(0.);w.model->recordParameterSample(clock->Id(),"SecondsSinceMidnight");w.sample(46.);QCOMPARE(p->Seconds(),0);
  clock->setSecondsSinceMidnight(86400.);w.sample(47.);QCOMPARE(p->Seconds(),60);
  clock->setSecondsSinceMidnight(59.9999);w.model->recordParameterSample(clock->Id(),"SecondsSinceMidnight");w.sample(48.);QCOMPARE(p->Seconds(),59);
  clock->setDaysSince1970(65535);w.sample(49.);QCOMPARE(p->Seconds(),60);
  clock->setDaysSince1970(20001);w.model->recordParameterSample(clock->Id(),"SecondsSinceMidnight");w.sample(50.);QCOMPARE(p->Seconds(),59);
  for(auto *timer:w.window.findChildren<QTimer*>())timer->stop();QTest::qWait(2100);w.sample(51.);QCOMPARE(p->Seconds(),60);QVERIFY(!p->observationTimeControlled());
  w.model->recordParameterSample(clock->Id(),"SecondsSinceMidnight");w.sample(52.);QCOMPARE(p->Seconds(),59);
  w.remove(clock,"Generate");w.sample(53.);QCOMPARE(p->Seconds(),60);QVERIFY(!p->observationTimeControlled());
  w.manual()->click();QVERIFY(!p->navigationControlled());QVERIFY(!p->findChild<QLineEdit*>("Seconds")->isReadOnly());
 }
 void renderThemes(){
  Workspace w;auto *p=w.add();w.start();auto *clock=w.window.findChild<GeneratorDateTime*>();clock->setDaysSince1970(20000);clock->setSecondsSinceMidnight(45296.789);w.model->recordParameterSample(clock->Id(),"SecondsSinceMidnight");w.sample();w.select(p);
  for(const QString theme:{"Dark","Light"}){AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.sync();w.window.HotReloadSettings();w.select(p);QTest::qWait(30);QVERIFY(p->navigationControlled());QVERIFY(p->findChild<QLabel*>("aisMovementStatus")->isVisible());QVERIFY(p->findChild<QLineEdit*>("Latitude")->isReadOnly());const auto dir=qEnvironmentVariable("M229E_EVIDENCE");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(w.window.grab().save(dir+"/"+theme+"-ais-movement.png"));w.manual()->click();w.select(p);QTest::qWait(20);QVERIFY(!p->findChild<QLabel*>("aisMovementStatus")->isVisible());QVERIFY(!p->findChild<QLineEdit*>("Latitude")->isReadOnly());QVERIFY(w.window.grab().save(dir+"/"+theme+"-ais-manual.png"));w.start();}QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());}
 }
};
int main(int argc,char **argv){QTemporaryDir profile;if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));AisMovementTests tests;return QTest::qExec(&tests,argc,argv);}
#include "ais_movement_tests.moc"
