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
#include <QAbstractItemModelTester>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QTemporaryDir>
#include <QNetworkProxy>
#include <QMessageBox>
#include <QAction>
#include <QHeaderView>
#include <QTableView>
#include <QLineEdit>
#include <QDockWidget>
#include "generatorvesselmovement.h"
#include "generatorweather.h"
#include "generatorengine.h"
#include "movevesselsimulator.h"
#include "autopilotsimulator.h"
#include "compass.h"
#include "convert.h"
#include "mainwindow.h"
#include "appsettings.h"
#include "pgntreecontroller.h"
#include "themes.h"
namespace {
void confirm(){QTimer::singleShot(0,[]{for(auto*w:QApplication::topLevelWidgets())if(auto*b=qobject_cast<QMessageBox*>(w))b->button(QMessageBox::Yes)->click();});}
void near(double a,double b,double epsilon=1e-7){QVERIFY2(qAbs(a-b)<epsilon,qPrintable(QString("%1 != %2").arg(a,0,'g',17).arg(b,0,'g',17)));}
struct Workspace {
 MainWindow window;MainController*c=window.findChild<MainController*>();
 PgnTableModel*values=window.findChild<PgnTableController*>()->getPgnTableModel();
 PgnTreeController*tree=window.findChild<PgnTreeController*>();PgnTreeModel*model=tree->getTreeModel();
 Workspace(){window.resize(1500,950);window.show();confirm();c->loadLayoutFromJson(":/defaults/defaultLayout.json");}
 template<class T>T* first(){return window.findChild<T*>();}
 QString shown(QWidget*w,const QString&field,int col=1,int role=Qt::DisplayRole){return model->data(model->indexFor(w,field).siblingAtColumn(col),role).toString();}
};
WeatherObservation weather(QString station,QDateTime observed=QDateTime::currentDateTimeUtc()){
 WeatherObservation o;o.provider="NOAA NDBC";o.station=station;o.location="Fixture "+station;o.observed=observed;o.retrieved=QDateTime::currentDateTimeUtc();o.windSpeed=7;o.waveHeight=3;return o;
}
}
class HeadingTimestampTests : public QObject {
 Q_OBJECT
private slots:
 void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");}
 void manualHeadingSurvivesMovement() {
  GeneratorVesselMovement w(1,"Vessel Movement",nullptr);w.show();
  w.setHeading(Convert::DegreesToRadians(10));
  auto* manual=w.findChild<QCheckBox*>("checkBoxManualMovementEnabled");manual->click();QVERIFY(manual->isChecked());
  auto* simulator=w.findChild<MoveVesselSimulator*>();QVERIFY(simulator);
  auto* slider=w.findChild<QSlider*>("horizontalSliderHeading");slider->setValue(1250);
  near(w.Heading(),Convert::DegreesToRadians(125));near(w.findChild<Compass*>()->heading(),125);
  for(int i=0;i<12;++i){QVERIFY(QMetaObject::invokeMethod(simulator,"updateVesselPosition",Qt::DirectConnection));near(w.Heading(),Convert::DegreesToRadians(125));QCOMPARE(slider->value(),1250);}
  w.setTransmissionGear(N2kTG_Reverse);w.setTransmissionGearStbd(N2kTG_Reverse);w.setThrottlePosition(45);w.setThrottlePositionStbd(45);
  QMetaObject::invokeMethod(simulator,"updateVesselPosition",Qt::DirectConnection);
  near(w.Heading(),Convert::DegreesToRadians(125));near(w.COG(),Convert::DegreesToRadians(305));QCOMPARE(slider->value(),1250);near(w.findChild<Compass*>()->heading(),125);
  manual->click();QVERIFY(!manual->isChecked());
 }
 void manualMagneticHeadingConversion() {
  GeneratorVesselMovement w(1,"Vessel Movement",nullptr);w.setHeadingReference(N2khr_magnetic);w.setVariation(Convert::DegreesToRadians(10));w.setDeviation(Convert::DegreesToRadians(5));
  w.findChild<QCheckBox*>("checkBoxManualMovementEnabled")->click();w.findChild<QSlider*>("horizontalSliderHeading")->setValue(900);near(w.findChild<Compass*>()->heading(),90);
  QMetaObject::invokeMethod(w.findChild<MoveVesselSimulator*>(),"updateVesselPosition",Qt::DirectConnection);near(w.Heading(),Convert::DegreesToRadians(105));QCOMPARE(w.HeadingReference(),N2khr_true);QCOMPARE(w.findChild<QSlider*>("horizontalSliderHeading")->value(),1050);
 }
 void headingWithEnginesAndWeather() {
  Workspace w;auto*m=w.first<GeneratorVesselMovement>();auto*weatherWidget=w.first<GeneratorWeather>();QVERIFY(m&&weatherWidget);
  for(auto*engine:w.window.findChildren<GeneratorEngine*>()){engine->setEngineProfile("MerCruiser 7.4 MPI");engine->setEngineRunning(true);QVERIFY(engine->EngineRunning());}
  weatherWidget->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(2);weatherWidget->findChild<QCheckBox*>("weatherAutoRefresh")->setChecked(true);
  m->setHeadingReference(N2khr_true);m->setRudderPosition(0);
  m->findChild<QCheckBox*>("checkBoxManualMovementEnabled")->click();QVERIFY(m->findChild<QCheckBox*>("checkBoxManualMovementEnabled")->isChecked());
  m->setTransmissionGear(N2kTG_Forward);m->setTransmissionGearStbd(N2kTG_Forward);m->setThrottlePosition(45);m->setThrottlePositionStbd(45);
  auto*slider=m->findChild<QSlider*>("horizontalSliderHeading");slider->setValue(1233);slider->setFocus();QTest::keyClick(slider,Qt::Key_Right);
  QCOMPARE(slider->value(),1234);near(m->findChild<Compass*>()->heading(),123.4,.0001);
  const double latitude=m->Latitude();
  for(int i=0;i<10;++i){
   for(auto*engine:w.window.findChildren<GeneratorEngine*>())QMetaObject::invokeMethod(engine,"PollTimerElapsed",Qt::DirectConnection);
   weatherWidget->acceptObservation(weather("44098"));
   QMetaObject::invokeMethod(m->findChild<MoveVesselSimulator*>(),"updateVesselPosition",Qt::DirectConnection);QCoreApplication::processEvents();
   near(m->Heading(),Convert::DegreesToRadians(123.4));QCOMPARE(slider->value(),1234);
  }
  QVERIFY(m->Latitude()!=latitude);QVERIFY(m->SOG()>0);
  const auto evidence=qEnvironmentVariable("M218_ACCEPTANCE_EVIDENCE");if(!evidence.isEmpty())QVERIFY(w.window.grab().save(evidence+"/Manual-heading.png"));QVERIFY(weatherWidget->hasFreshObservation());
  for(const auto&row:w.values->getPgnParams(m->Id()))if(row[0]=="Heading")near(row[1].toDouble(),m->Heading());
  QVERIFY(w.values->parameterUpdatedAt(m->Id(),"Heading").isValid());QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
  // Rudder motion still changes heading after an operator heading request.
  auto*rudder=m->findChild<QSlider*>("horizontalSliderRudder");rudder->setValue(100);QMetaObject::invokeMethod(m->findChild<MoveVesselSimulator*>(),"updateVesselPosition",Qt::DirectConnection);
  QVERIFY(qAbs(m->Heading()-Convert::DegreesToRadians(123.4))>1e-6);
 }
 void autopilotHeadingRemainsIndependent() {
  GeneratorVesselMovement w(1,"Vessel Movement",nullptr);w.show();w.setLatitude(42);w.setLongitude(-70);
  QTemporaryDir dir(QDir::currentPath()+"/m218-route-XXXXXX");QVERIFY(dir.isValid());QFile route(dir.filePath("route.gpx"));QVERIFY(route.open(QIODevice::WriteOnly));route.write("<gpx><rte><rtept lat='42' lon='-70'/><rtept lat='42.01' lon='-70'/></rte></gpx>");route.close();
  w.findChild<QLineEdit*>("lineEditFilePath")->setText(route.fileName());auto*ap=w.findChild<QCheckBox*>("checkBoxAutoPilotEnabled");ap->click();QVERIFY(ap->isChecked());
  auto*slider=w.findChild<QSlider*>("horizontalSliderHeading");QVERIFY(!slider->isEnabled());
  auto*sim=w.findChild<AutoPilotSimulator*>();QSignalSpy updates(sim,&AutoPilotSimulator::VesselWpBearingChanged);QTRY_VERIFY(!updates.isEmpty());
  const double heading=w.Heading();slider->setFocus();QTest::keyClick(slider,Qt::Key_Right);near(w.Heading(),heading);
  near(w.findChild<Compass*>()->heading(),Convert::RadiansToDegrees(w.Heading()),.0001);
  ap->click();QVERIFY(!ap->isChecked());QVERIFY(!w.findChild<QCheckBox*>("checkBoxManualMovementEnabled")->isChecked());
 }
 void authoritativeSamplesInitializationAndIdentity() {
  PgnTableModel model;const QList<QList<QString>> rows{{"Value","1","m","double"}};
  model.addPgn(1,"One","127508",true,rows,{});auto*old=new QObject;model.setParameterOwner(1,old);
  QVERIFY(!model.parameterUpdatedAt(1,"Value").isValid());model.beginParameterInitialization(1,1);model.changeParam(1,"Value","2");model.endParameterInitialization();QVERIFY(!model.parameterUpdatedAt(1,"Value").isValid());
  model.beginParameterInitialization(2);model.changeParam(1,"Value","2");model.endParameterInitialization();QVERIFY(model.parameterUpdatedAt(1,"Value").isValid());
  const auto first=model.parameterUpdatedAt(1,"Value");QTest::qWait(3);QSignalSpy edits(&model,&PgnTableModel::parameterChanged),display(&model,&PgnTableModel::dataChanged);
  model.changeParams(1,{"Value","2"});QVERIFY(model.parameterUpdatedAt(1,"Value")>first);QCOMPARE(edits.count(),0);QCOMPARE(display.count(),0);
  model.addPgn(2,"Two","127508",true,rows,{});model.recordParameterSample(2,"Value");const auto independent=model.parameterUpdatedAt(2,"Value");QVERIFY(independent.isValid());
  model.removePgn(1);model.addPgn(1,"New","127508",true,rows,{});auto*fresh=new QObject;model.setParameterOwner(1,fresh);QVERIFY(!model.parameterUpdatedAt(1,"Value").isValid());model.recordParameterSample(1,"Value");auto replacement=model.parameterUpdatedAt(1,"Value");delete old;QCOMPARE(model.parameterUpdatedAt(1,"Value"),replacement);
  delete fresh;QVERIFY(!model.parameterUpdatedAt(1,"Value").isValid());model.changeParams(1,{"Value","1"});QVERIFY(!model.parameterUpdatedAt(1,"Value").isValid());QCOMPARE(model.parameterUpdatedAt(2,"Value"),independent);
  model.clearModel();QVERIFY(!model.parameterUpdatedAt(2,"Value").isValid());
 }
 void treeColumnAndRapidIdenticalSamples() {
  Workspace w;auto*m=w.first<GeneratorVesselMovement>();auto*engine=w.first<GeneratorEngine>();QVERIFY(m&&engine);QTest::qWait(40);
  QAbstractItemModelTester tester(w.model,QAbstractItemModelTester::FailureReportingMode::QtTest);
  QCOMPARE(w.model->columnCount(),3);QCOMPARE(w.model->headerData(2,Qt::Horizontal).toString(),QString("Last Updated"));
  QVERIFY(!w.values->parameterUpdatedAt(engine->Id(),"EngineInstance").isValid());QCOMPARE(w.shown(engine,"EngineInstance",2),QString("Unknown"));
  auto*view=w.tree->getTreeView();auto selected=w.model->indexFor(m,"Heading");view->setExpanded(w.model->indexFor(m),true);view->setCurrentIndex(selected);QPersistentModelIndex stable(selected);
  QSignalSpy resets(w.model,&QAbstractItemModel::modelReset),changes(w.model,&QAbstractItemModel::dataChanged),edits(w.values,&PgnTableModel::parameterChanged);
  for(int i=0;i<10000;++i)w.values->changeParam(m->Id(),"Heading","1.25");
  const auto last=w.values->parameterUpdatedAt(m->Id(),"Heading");QVERIFY(last.isValid());QTest::qWait(100);
  QCOMPARE(resets.count(),0);QVERIFY(changes.count()<20);QCOMPARE(edits.count(),0);QCOMPARE(view->currentIndex(),QModelIndex(stable));QVERIFY(view->isExpanded(w.model->indexFor(m)));
  QVERIFY(w.shown(m,"Heading",2).contains(':'));QVERIFY(w.shown(m,"Heading",2,Qt::ToolTipRole).contains(last.toUTC().toString(Qt::ISODateWithMs)));
  for(int i=0;i<100;++i){w.model->data(selected.siblingAtColumn(2));view->viewport()->update();}QTest::qWait(20);QCOMPARE(w.values->parameterUpdatedAt(m->Id(),"Heading"),last);
  auto*trans=w.window.findChild<GeneratorTransLowerUnit*>();QVERIFY(trans);w.values->recordParameterSample(trans->Id(),"TransmissionGear");auto gearTime=w.values->parameterUpdatedAt(trans->Id(),"TransmissionGear");
  auto prefs=trans->UserPrefs();prefs.tempUnitIndex=(prefs.tempUnitIndex+1)%3;trans->setUserPrefs(prefs);QCOMPARE(w.values->parameterUpdatedAt(trans->Id(),"TransmissionGear"),gearTime);
  QVERIFY(QMetaObject::invokeMethod(trans,"ChangeTempUnits_triggered",Qt::DirectConnection,Q_ARG(int,(prefs.tempUnitIndex+1)%3)));QCOMPARE(w.values->parameterUpdatedAt(trans->Id(),"TransmissionGear"),gearTime);
 }
 void viewerNmea2000And0183IdenticalSamples() {
  Workspace w;w.c->setMode(Viewer);
  for(int instance:{3,4})w.c->addRcvdPgn("127508",{"BatteryInstance",QString::number(instance),"BatteryVoltage","12.7","BatteryCurrent","2","BatteryTemperature","295","SID","1"},"fixture-source");
  QList<int> ids;for(const auto&p:w.values->getPgnList())if(p.pgn=="127508")for(const auto&row:p.paramDetails)if(row[0]=="BatteryInstance" && (row[1]=="3"||row[1]=="4"))ids<<p.idX;
  QCOMPARE(ids.size(),2);auto first=w.values->parameterUpdatedAt(ids[0],"BatteryVoltage");auto other=w.values->parameterUpdatedAt(ids[1],"BatteryVoltage");QVERIFY(first.isValid());QVERIFY(other.isValid());QTest::qWait(3);
  w.c->updateRcvdPgn(ids[0],{"BatteryVoltage","12.7"});QVERIFY(w.values->parameterUpdatedAt(ids[0],"BatteryVoltage")>first);QCOMPARE(w.values->parameterUpdatedAt(ids[1],"BatteryVoltage"),other);
  w.c->addRcvdPgn("HDT",{"Heading","1.25"},"serial-fixture");int hdt=-1;for(const auto&p:w.values->getPgnList())if(p.pgn=="HDT")hdt=p.idX;QVERIFY(hdt>=0);QVERIFY(w.values->parameterUpdatedAt(hdt,"Heading").isValid());
  first=w.values->parameterUpdatedAt(hdt,"Heading");QTest::qWait(3);w.c->updateRcvdPgn(hdt,{"Heading","1.25"});QVERIFY(w.values->parameterUpdatedAt(hdt,"Heading")>first);
  auto stamp=w.values->parameterUpdatedAt(hdt,"Heading");w.c->setMode(Sender);QCOMPARE(w.values->parameterUpdatedAt(hdt,"Heading"),stamp);w.c->setMode(Viewer);QCOMPARE(w.values->parameterUpdatedAt(hdt,"Heading"),stamp);
  confirm();QVERIFY(w.c->loadLayoutFromJson(":/defaults/defaultLayout.json"));auto*engine=w.first<GeneratorEngine>();QVERIFY(!w.values->parameterUpdatedAt(engine->Id(),"EngineInstance").isValid());
 }
 void weatherOperationalTimesAndRetention() {
  Workspace w;auto*weatherWidget=w.first<GeneratorWeather>();QVERIFY(weatherWidget);QTest::qWait(40);
  QVERIFY(w.shown(weatherWidget,"@WeatherFetch").contains("No successful"));QCOMPARE(w.shown(weatherWidget,"@WeatherNext"),QString("Not scheduled"));
  weatherWidget->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(2);weatherWidget->findChild<QCheckBox*>("weatherAutoRefresh")->setChecked(true);
  QVERIFY(weatherWidget->nextAutomaticRefresh().isValid());QVERIFY(w.shown(weatherWidget,"@WeatherNext").contains(':'));
  auto a=weather("buoyA");a.maximumAgeSeconds=1;weatherWidget->acceptObservation(a);auto waveTime=w.values->parameterUpdatedAt(weatherWidget->Id(),"WaveHeight");QVERIFY(waveTime.isValid());
  QTest::qWait(3);auto b=weather("buoyB",QDateTime::currentDateTimeUtc().addSecs(-5));b.waveHeight=N2kDoubleNA;b.windSpeed=12;weatherWidget->acceptObservation(b);
  QCOMPARE(weatherWidget->latestObservationTime(),b.observed);QVERIFY(weatherWidget->lastSuccessfulFetch()>b.observed);QCOMPARE(w.values->parameterUpdatedAt(weatherWidget->Id(),"WaveHeight"),waveTime);
  QVERIFY(w.shown(weatherWidget,"@WeatherProvider").contains("buoyB"));QVERIFY(w.shown(weatherWidget,"@WeatherObserved",1,Qt::ToolTipRole).contains(b.observed.toString(Qt::ISODateWithMs)));QVERIFY(w.shown(weatherWidget,"@WeatherObserved",1,Qt::ToolTipRole).contains("not the time/source"));
  QVERIFY(w.shown(weatherWidget,"WaveHeight",2,Qt::ToolTipRole).contains("buoyA"));QVERIFY(w.shown(weatherWidget,"WaveHeight",2,Qt::ToolTipRole).contains("Retained"));
  const auto observationTime=weatherWidget->latestObservationTime(),fetched=weatherWidget->lastSuccessfulFetch();weatherWidget->cancelWeather();QCOMPARE(weatherWidget->lastSuccessfulFetch(),fetched);QVERIFY(weatherWidget->nextAutomaticRefresh().isValid());
  QTRY_COMPARE_WITH_TIMEOUT(weatherWidget->WaveHeight(),N2kDoubleNA,2200);QCOMPARE(weatherWidget->latestObservationTime(),observationTime);QVERIFY(w.values->parameterUpdatedAt(weatherWidget->Id(),"WaveHeight")>waveTime);
  w.c->setMode(Viewer);QVERIFY(!weatherWidget->nextAutomaticRefresh().isValid());QCOMPARE(w.shown(weatherWidget,"@WeatherNext"),QString("Not scheduled"));w.c->setMode(Sender);QVERIFY(weatherWidget->nextAutomaticRefresh().isValid());QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
 void pendingEditorAndNativePresentation() {
  Workspace w;auto*engine=w.first<GeneratorEngine>();QVERIFY(engine);QTest::qWait(40);w.c->selectLayoutWidget(engine,"EngineHours");
  auto*table=w.window.findChild<QTableView*>("tableView");table->setFocus();table->edit(table->currentIndex());QTest::qWait(20);auto*editor=table->viewport()->findChild<QLineEdit*>();QVERIFY(editor);editor->setText("unfinished");
  for(int i=0;i<100;++i)w.values->recordParameterSample(engine->Id(),"EngineHours");QTest::qWait(100);QCOMPARE(editor->text(),QString("unfinished"));QVERIFY(editor->hasFocus());QVERIFY(w.tree->navigationBlocked());QTest::keyClick(editor,Qt::Key_Escape);
  auto*view=w.tree->getTreeView();view->setColumnWidth(2,170);QCOMPARE(view->columnWidth(2),170);
  auto*weatherWidget=w.first<GeneratorWeather>();weatherWidget->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(2);weatherWidget->findChild<QCheckBox*>("weatherAutoRefresh")->setChecked(true);weatherWidget->acceptObservation(weather("44098"));
  view->setExpanded(w.model->indexFor(weatherWidget),true);view->setCurrentIndex(w.model->indexFor(weatherWidget,"@WeatherFetch"));view->scrollTo(view->currentIndex());
  for(auto*dock:w.window.findChildren<QDockWidget*>())if(dock->isAncestorOf(view))w.window.resizeDocks({dock},{650},Qt::Horizontal);
  view->setColumnWidth(0,240);view->setColumnWidth(1,230);view->setColumnWidth(2,140);
  for(const QString theme:{"Dark","Light"}){qApp->setPalette(Themes::palette(theme));qApp->setStyleSheet(Themes::styleSheet(theme));QTest::qWait(30);const auto dir=qEnvironmentVariable("M218_ACCEPTANCE_EVIDENCE");if(!dir.isEmpty())QVERIFY(w.window.grab().save(dir+"/"+theme+"-timestamps.png"));}
 }
};
int main(int argc,char**argv){QTemporaryDir profile(QDir::currentPath()+"/heading-timestamps-XXXXXX");qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QList<QByteArray>args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*>ptr;for(auto&a:args)ptr<<a.data();int count=ptr.size();ptr<<nullptr;QApplication app(count,ptr.data());app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));HeadingTimestampTests tests;return QTest::qExec(&tests,argc,argv);}
#include "heading_timestamp_tests.moc"
