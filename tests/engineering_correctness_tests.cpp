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
#include <QAction>
#include <QLineEdit>
#include <QLabel>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QNetworkProxy>
#include <QMessageBox>
#include <QPushButton>
#include <QJsonDocument>
#include <limits>
#include "mainwindow.h"
#include "appsettings.h"
#include "navigation129284.h"
#include "generatorvesselmovement.h"
#include "nmeasenderworker.h"
#include "themes.h"
#include "N2kMessages.h"

namespace {
void settle(){for(int i=0;i<5;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);}
class Backend:public SocketCanBackend {
public:
 bool opened=false;int error=0,sends=0;
 void configure(const SocketCanConfig&,std::function<void(const tN2kMsg&)>) override{}
 void service() override{}
 bool ready() const override{return opened;}
 int openingError() const override{return error;}
 bool send(const tN2kMsg&) override{++sends;return true;}
 bool close() override{return true;}
};
QString distance(Navigation129284 *w){return w->findChild<QLineEdit*>("lineEditDistanceToWaypoint")->text();}
QString status(MainWindow &w){return w.findChild<QLabel*>("transportStatus")->text();}
void capture(QWidget &w,const QString &name){const auto out=qEnvironmentVariable("RAY_CORRECTNESS_OUTPUT");if(!out.isEmpty()){QDir().mkpath(out);QVERIFY(w.grab().save(out+"/"+name+".png"));}}
}
class EngineeringCorrectnessTests:public QObject {
 Q_OBJECT
 QTimer modalAnswers;
private slots:
 void initTestCase(){
  connect(&modalAnswers,&QTimer::timeout,[]{
   if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
    if(auto *no=box->button(QMessageBox::No))no->click();
   } else for(auto *top:QApplication::topLevelWidgets())
    if(auto *keep=top->findChild<QPushButton*>("viewerKeepExisting"))keep->click();
  });
  modalAnswers.start(0);
 }
 void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/Nmea2000Enabled",false);s.setValue("Nmea2000/InterfaceType","None");s.setValue("Nmea0183/Nmea0183Enabled",false);s.setValue("Preference/PromptSaveLayoutOnClose",false);s.sync();}
 void initialUnavailable(){Navigation129284 w(1,"Navigation",nullptr);QCOMPARE(w.DistanceToWaypoint(),N2kDoubleNA);QCOMPARE(distance(&w),QString("Unavailable"));QSignalSpy changed(&w,&Navigation129284::DistanceToWaypointChanged);w.setDistanceToWaypoint(N2kDoubleNA);QCOMPARE(changed.size(),0);QCOMPARE(distance(&w),QString("Unavailable"));}
 void distanceValues_data(){QTest::addColumn<int>("unit");QTest::addColumn<double>("value");for(int u=0;u<3;++u)for(double d:{N2kDoubleNA,0.,1852.,1234.567890123})QTest::newRow(qPrintable(QString::number(u)+"/"+QString::number(d)))<<u<<d;}
 void distanceValues(){
  QFETCH(int,unit);QFETCH(double,value);MainWindow w;auto *c=w.findChild<MainController*>();c->addWidgetAsComboboxTextChanged("129284","Navigation","Navigation");settle();auto *n=w.findChild<Navigation129284*>();QVERIFY(n);auto prefs=n->UserPrefs();prefs.distanceUnitIndex=unit;n->setUserPrefs(prefs);n->setDistanceToWaypoint(value);settle();QCOMPARE(n->DistanceToWaypoint(),value);
  const double divisor=unit==0?1000.:unit==1?1852.:1609.344;QCOMPARE(distance(n),N2kIsNA(value)?QString("Unavailable"):QString::number(value/divisor,'f',2));
  auto *m=w.findChild<PgnTableController*>()->getPgnTableModel();const auto rows=m->getPgnParams(n->Id());bool found=false;for(const auto &row:rows)if(row[0]=="DistanceToWaypoint"){found=true;QCOMPARE(row[1].toDouble(),value);}QVERIFY(found);
  QSignalSpy edits(m,&PgnTableModel::parameterChanged);n->setInitialGuiValues();prefs.distanceUnitIndex=(unit+1)%3;n->setUserPrefs(prefs);settle();QCOMPARE(m->getPgnParams(n->Id()),rows);QCOMPARE(edits.size(),0);
  n->setDistanceToWaypoint(42.125);n->setDistanceToWaypoint(N2kDoubleNA);QCOMPARE(distance(n),QString("Unavailable"));n->setDistanceToWaypoint(0);QCOMPARE(distance(n),QString("0.00"));n->setDistanceToWaypoint(42.125);QCOMPARE(n->DistanceToWaypoint(),42.125);
  QString text="12.34";int pos=0;QCOMPARE(n->findChild<QLineEdit*>("lineEditDistanceToWaypoint")->validator()->validate(text,pos),QValidator::Acceptable);
 }
 void layoutRestorationAndThemes(){
  MainWindow w;auto *c=w.findChild<MainController*>();c->addWidgetAsComboboxTextChanged("129284","Navigation","Navigation");settle();auto *n=w.findChild<Navigation129284*>();QVERIFY(n);
  for(const auto &theme:QStringList{"Light","Dark"}){AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Preference/StyleSheetName",theme);settings.sync();w.HotReloadSettings();n->setDistanceToWaypoint(100);n->setDistanceToWaypoint(N2kDoubleNA);n->show();QCOMPARE(distance(n),QString("Unavailable"));capture(*n,theme+"-navigation-unavailable");}
  settle();QTemporaryDir dir(QDir::currentPath()+"/saved-XXXXXX");QVERIFY(dir.isValid());auto file=dir.filePath("layout.json");QVERIFY(c->saveLayoutToJson(file));QVERIFY(c->loadLayoutFromJson(file,[](const QString&){return true;}));settle();n=w.findChild<Navigation129284*>();QVERIFY(n);QCOMPARE(n->DistanceToWaypoint(),N2kDoubleNA);QCOMPARE(distance(n),QString("Unavailable"));
  // The encoder still receives canonical NA; presentation never converts it to metres.
  tN2kMsg msg;SetN2kPGN129284(msg,1,n->DistanceToWaypoint(),N2khr_true,false,false,N2kdct_GreatCircle,0,0,0,0,1,2,10,20,0);unsigned char sid;double d,t,b1,b2,lat,lon,speed;tN2kHeadingReference ref;bool crossed,arrived;tN2kDistanceCalculationType calc;int16_t day;uint32_t a,b;QVERIFY(ParseN2kPGN129284(msg,sid,d,ref,crossed,arrived,calc,t,day,b1,b2,a,b,lat,lon,speed));QCOMPARE(d,N2kDoubleNA);
 }
 void receivedAndGeneratedDistance(){
  MainWindow w;auto *c=w.findChild<MainController*>();auto *h=w.findChild<Nmea2000Handler*>();c->setMode(Viewer);h->setMode(Viewer);
  for(double value:{N2kDoubleNA,0.,1852.25}) {
   tN2kMsg m;SetN2kPGN129284(m,1,value,N2khr_true,false,false,N2kdct_GreatCircle,0,0,0,0,1,2,10,20,0);m.Source=33;h->nmea2000MessageReceived(m);settle();auto *n=w.findChild<Navigation129284*>();QVERIFY(n);QCOMPARE(n->DistanceToWaypoint(),value);QVERIFY(!distance(n).startsWith("-"));if(N2kIsNA(value))QCOMPARE(distance(n),QString("Unavailable"));
  }
  c->removeAllWidgets();c->setMode(Sender);h->setMode(Sender);
  c->addWidgetAsComboboxTextChanged("129284","Navigation","Navigation");
  QTimer answers;connect(&answers,&QTimer::timeout,[]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->button(QMessageBox::No)->click();});answers.start(0);
  c->addGeneratorWidget("Vessel Movement (PGN -129025)");settle();auto *g=w.findChild<GeneratorVesselMovement*>();auto *n=w.findChild<Navigation129284*>();QVERIFY(g && n);
  for(double value:{123.456,N2kDoubleNA,0.,12.25}){g->setDistanceToWaypoint(value);settle();QCOMPARE(n->DistanceToWaypoint(),value);QVERIFY(!distance(n).startsWith("-"));}
  QVERIFY(!w.findChild<NmeaSenderWorker*>()->isSending());
 }
 void sharedSerialProtocolIdentity(){
  WaveshareSession session;session.setProtocol(WaveshareSession::CanableSlcan);QVERIFY(session.status().startsWith("CANable SLCAN"));session.setProtocol(WaveshareSession::Waveshare);QVERIFY(session.status().startsWith("Waveshare"));QCOMPARE(session.state(),WaveshareSession::Closed);
 }
 void selectedTransportOwnership(){
  AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Nmea2000/Nmea2000Enabled",true);s.setValue("Nmea2000/InterfaceType","SocketCan");s.setValue("Nmea2000/SocketCanInterface","fixture0");s.setValue("Nmea2000/SocketCanUniqueNumber",123);s.sync();Backend backend;MainWindow w(nullptr,nullptr,nullptr,&backend);auto *h=w.findChild<Nmea2000Handler*>();auto *socket=h->findChild<SocketCanSession*>();auto *wave=h->findChild<WaveshareSession*>();QVERIFY(socket && wave);backend.opened=true;socket->service();QCOMPARE(status(w),socket->status());QVERIFY(!status(w).isEmpty());
  auto good=status(w);wave->stop();QCOMPARE(status(w),good);h->WaveshareStatus("stale inactive serial error");h->SignalKStatus("stale inactive Signal K error");QCOMPARE(status(w),good);
  // Failure and recovery use the real session state machine with a memory backend.
  socket->shutdown();QCOMPARE(status(w),socket->status());QVERIFY(status(w)!=good);
  // Setup-style reload recreates configuration; a former queued event is only an invalidation.
  QMetaObject::invokeMethod(h,[h]{h->SocketCanStatus("old queued socket status");},Qt::QueuedConnection);
  s.setValue("Nmea2000/InterfaceType","SignalK");s.sync();w.HotReloadSettings();auto signal=h->findChild<SignalKPublisher*>();QVERIFY(signal);signal->statusChanged("Signal K: synthetic selected failure");QCOMPARE(status(w),QString("Signal K: synthetic selected failure"));settle();QCOMPARE(status(w),QString("Signal K: synthetic selected failure"));
  signal->statusChanged("Signal K: synthetic reconnect ready");QCOMPARE(status(w),QString("Signal K: synthetic reconnect ready"));
  w.ShowStatusBarMessage("Synthetic application notice",10000);QCOMPARE(w.statusBar()->currentMessage(),QString("Synthetic application notice"));QCOMPARE(status(w),QString("Signal K: synthetic reconnect ready"));
  for(const auto &mode:QStringList{"actionViewer_Rx","actionSimulator_Tx"}){QTimer::singleShot(0,[]{for(auto *top:QApplication::topLevelWidgets())if(auto *button=top->findChild<QPushButton*>("viewerKeepExisting"))button->click();});w.findChild<QAction*>(mode)->setChecked(true);settle();QCOMPARE(status(w),h->transportStatus());QVERIFY(!w.findChild<NmeaSenderWorker*>()->isSending());}
  w.findChild<QAction*>("actionStop_Sending")->trigger();QTest::keyClick(&w,Qt::Key_F12);QCOMPARE(status(w),h->transportStatus());QVERIFY(!w.findChild<NmeaSenderWorker*>()->isSending());
  for(const auto &theme:QStringList{"Light","Dark"}){s.setValue("Preference/StyleSheetName",theme);s.sync();w.HotReloadSettings();w.show();QTest::qWait(20);capture(w,theme+"-selected-status");}
  s.setValue("Nmea2000/InterfaceType","Network NMEA2000");s.setValue("Nmea2000/NetworkCanHost","");s.sync();w.HotReloadSettings();QCOMPARE(status(w),h->networkCan()->status());QVERIFY(status(w).contains("Invalid"));
  h->SocketCanStatus("old socket disconnect");QCOMPARE(status(w),h->networkCan()->status());
  for(const auto &adapter:QStringList{"CANable SLCAN","Waveshare USB-CAN-A"}){s.setValue("Nmea2000/InterfaceType",adapter);s.sync();w.HotReloadSettings();QCOMPARE(status(w),wave->status());QCOMPARE(wave->protocol(),adapter=="CANable SLCAN"?WaveshareSession::CanableSlcan:WaveshareSession::Waveshare);h->SignalKStatus("old Signal K event");QCOMPARE(status(w),wave->status());}
  s.setValue("Nmea2000/Nmea2000Enabled",false);s.sync();w.HotReloadSettings();settle();QCOMPARE(status(w),QString("NMEA2000 disabled"));h->WaveshareStatus("late disconnect");QCOMPARE(status(w),QString("NMEA2000 disabled"));QCOMPARE(backend.sends,0);
 }
};
int main(int argc,char **argv){QApplication app(argc,argv);QTemporaryDir profile(QDir::currentPath()+"/correctness-profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));EngineeringCorrectnessTests tests;return QTest::qExec(&tests,argc,argv);}
#include "engineering_correctness_tests.moc"
