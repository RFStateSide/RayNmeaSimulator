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
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QAbstractButton>
#include <QLineEdit>
#include <QPointer>
#include <QAccessible>
#include <QNetworkProxy>
#include "maincontroller.h"
#include "fluidlevel127505.h"
#include "globalpositioninggga.h"
#include "helper.h"
#include "convert.h"

struct Workspace {
 QMdiArea area;
 QTableView table;
 QTreeView tree;
 PgnTableController tables{&table};
 PgnTreeController trees{&tree};
 MainController controller{&tables,&trees,&area,nullptr};
 Workspace() {
  controller.setMyVessel(MyVessel{});
  controller.setUserPrefs(UserUnitPrefs{});
  controller.setUserOptionPrefs(UserOptionPrefs{});
  for(const QString g : {"Generate","Navigation","Propulsion","Environment","Tanks","Electrical","Other","Communication"})
   controller.addSubWindowForGroup(g,g);
 }
 PgnTableModel *model() {return tables.getPgnTableModel();}
};
static void confirmReplacement() {
 QTimer::singleShot(0, [] {
  for(QWidget *w: QApplication::topLevelWidgets())
   if(auto *box=qobject_cast<QMessageBox*>(w)) box->button(QMessageBox::Yes)->click();
 });
}
static int paramRow(PgnTableModel *m,int id,const QString &name) {
 const auto params=m->getPgnParams(id);
 for(int i=0;i<params.size();++i) if(params[i][0]==name) return i;
 return -1;
}
class MetaCallCounter : public QObject {
public:
 int count = 0;
 bool eventFilter(QObject *, QEvent *event) override {
  if (event->type() == QEvent::MetaCall) ++count;
  return false;
 }
};
class LayoutTests : public QObject {
 Q_OBJECT
private slots:
 void factories_data() {
  QTest::addColumn<QString>("type");
  for(const QString t:{"Attitude127257","CogSog129026","CrossTrack129283","DateTime129033","EngineDynamic127489","EngineRapid127488","FluidLevel127505","GeneratorDateTime","GeneratorEngine","GeneratorTransLowerUnit","GeneratorVesselMovement","GeneratorWeather","GnssPositionData129029","Heave127252","LatLong129025","MagneticVariation127258","Navigation129284","OutsideEnvironmental130310","RateOfTurn127251","Speed128259","SystemTime126992","TemperatureExtended130316","TemperatureWidget130312","TransmissionDynamic127493","VesselHeading127250","ChargerStatus127507","BatteryStatus127508","Rudder127245","TrimTab130576","GlobalPositioningGGA","RecommendedMinGnssRMC","HeadingHDG","TrueHeadingHDT","GeographicPositionGLL","CogSogVTG","DateTimeZDA","Spacer000000","DcStatus127506","Depth128267"}) QTest::newRow(qPrintable(t))<<t;
 }
 void factories() {
  QFETCH(QString,type);
  Workspace w;
  WidgetConfig c{};c.idX=1;c.widgetType=type;
  QScopedPointer<QWidget> widget(w.controller.createWidgetFromConfig(c));
  QVERIFY2(widget,qPrintable(type));
  QCOMPARE(QString(widget->metaObject()->className()),type);
 }
 void pendingWidgetRemoval() {
  Workspace w;
  w.controller.addWidgetAsComboboxTextChanged("HDT","Heading","Navigation");
  auto *widget=w.controller.getListWidgetsOnSubForm("Navigation")->first();
  // Queue a real widget notification, destroy it before delivery, then drain.
  QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  MetaCallCounter calls;w.controller.installEventFilter(&calls);
  const auto accessibleId=QAccessible::uniqueId(QAccessible::queryAccessibleInterface(widget));
  QSignalSpy destroyed(widget, &QObject::destroyed);
  QVERIFY(widget->setProperty("Heading",1.2));
  w.controller.removeWidgetFromSubForm(widget,"Navigation");
  QCOMPARE(destroyed.count(),1);
  QVERIFY(QAccessible::accessibleInterface(accessibleId)==nullptr);
  QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  QCOMPARE(w.model()->getPgnList().size(),0);
  QCOMPARE(calls.count,0);
 }
 void sentenceRoundTrip() {
  QTemporaryDir dir;
  const QString path=dir.filePath("sentences.json");
  { Workspace w;
    for(const QString s:{"GGA","RMC","HDG","HDT","GLL","VTG","ZDA"})
     w.controller.addWidgetAsComboboxTextChanged(s,s,"Navigation");
    auto *m=w.model();
    for(const auto &p:QList<PgnData>(m->getPgnList())) {
     m->setCurrentPgn(p.idX);
     QString name=(p.pgn=="HDT"||p.pgn=="HDG")?"Heading":(p.pgn=="VTG"?"SOG":"GPSTime");
     const int row=paramRow(m,p.idX,name);QVERIFY(row>=0);
     QVERIFY(m->setData(m->index(row,1),12.5));
    }
    QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
    w.controller.saveLayoutToJson(path);
  }
  Workspace restored;
  restored.controller.loadLayoutFromJson(path);
  QCOMPARE(restored.model()->getPgnList().size(),7);
  const auto *widgets=restored.controller.getListWidgetsOnSubForm("Navigation");
  QCOMPARE(widgets->size(),7);
  for(auto *widget:*widgets) {
   const auto p=restored.model()->getPgnDataToSend(widget->property("Id").toInt());
   QString name=(p.pgn=="HDT"||p.pgn=="HDG")?"Heading":(p.pgn=="VTG"?"SOG":"GPSTime");
   QCOMPARE(widget->property(qPrintable(name)).toDouble(),12.5);
  }
  confirmReplacement();restored.controller.loadLayoutFromJson(path);
  QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  QCOMPARE(restored.model()->getPgnList().size(),7);
 }
 void tankEditingRoundTrip() {
  QTemporaryDir dir;const auto path=dir.filePath("tanks.json");
  Workspace w;
  WidgetConfig c{};c.idX=3;c.widgetType="FluidLevel127505";c.pgn="127505";c.label="Tank";c.rowSpan=4;c.columnSpan=3;c.interval=1000;
  c.parameters={{"Instance","0","","unsigned char"},{"FluidType","0","enum","tN2kFluidType"},{"Level","0","%","double"},{"Capacity","100","L","double"}};
  c.ranges={{0,255},{0,7},{0,100},{0,10000}};
  w.controller.restoreLayoutFromWidgetConfig(c,"Tanks");
  c.idX=4;c.parameters[0][1]="1";
  w.controller.restoreLayoutFromWidgetConfig(c,"Tanks");
  auto *tank=qobject_cast<FluidLevel127505*>(w.controller.getListWidgetsOnSubForm("Tanks")->first());QVERIFY(tank);
  auto *edit=tank->findChild<QLineEdit*>("lineEditLevel");QVERIFY(edit);
  edit->setFocus();
  edit->selectAll();
  QTest::keyClicks(edit,"25");
  QTRY_COMPARE(w.model()->getPgnParams(3)[2][1].toDouble(),25.0);
  w.model()->setCurrentPgn(3);
  QVERIFY(w.model()->setData(w.model()->index(2,1),40));
  QCOMPARE(tank->Level(),40.0);
  QVERIFY(w.model()->isPgnCurrentPgn(3));
  QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  QCOMPARE(w.model()->getPgnParams(3)[2][1].toDouble(),40.0);
  QVERIFY(w.model()->setData(w.model()->index(0,1),2));
  QVERIFY(w.model()->setData(w.model()->index(1,1),1));
  w.controller.saveLayoutToJson(path);
  Workspace restored;restored.controller.loadLayoutFromJson(path);
  auto *r=qobject_cast<FluidLevel127505*>(restored.controller.getListWidgetsOnSubForm("Tanks")->first());QVERIFY(r);
  QCOMPARE(r->Level(),40.0);
  QCOMPARE(int(r->Instance()),2);
  QCOMPARE(int(r->FluidType()),1);
 }
 void clearGeneratorGroupWithPendingUpdate() {
  const QString file=qEnvironmentVariable("RAYNMEA_TEST_DEFAULT");QVERIFY(QFile::exists(file));
  Workspace w;w.controller.loadLayoutFromJson(file);
  QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  auto engines=w.area.findChildren<GeneratorEngine*>();QVERIFY(!engines.isEmpty());
  auto rapids=w.area.findChildren<EngineRapid127488*>();QVERIFY(!rapids.isEmpty());
  auto *rapid=rapids.first();const double before=rapid->EngineSpeed();
  for(auto *engine:engines) engine->setEngineSpeed(1234);
  w.controller.removeAllWidgetsFromGroup("Generate");
  QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  QCOMPARE(rapid->EngineSpeed(),before);
 }
 void clearThenNavigation() {
  const QString file=qEnvironmentVariable("RAYNMEA_TEST_DEFAULT");QVERIFY(QFile::exists(file));
  Workspace w;w.controller.loadLayoutFromJson(file);
  w.controller.removeAllWidgets();
  QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  w.controller.addWidgetAsComboboxTextChanged("HDT","Heading","Navigation");
  QCOMPARE(w.model()->getPgnList().size(),1);
 }
 void clearAndReload_data() {
  QTest::addColumn<QString>("addition");
  QTest::addColumn<bool>("active");
  for(const QString addition : {"130312", "HDT", ""})
   for(bool active : {false,true})
    QTest::newRow(qPrintable((addition.isEmpty()?QString("default"):addition)+(active?"-active":"-inactive"))) << addition << active;
 }
 void clearAndReload() {
  QFETCH(QString,addition);QFETCH(bool,active);
  const QString file=qEnvironmentVariable("RAYNMEA_TEST_DEFAULT");QVERIFY(QFile::exists(file));
  QTemporaryDir dir;QVERIFY(dir.isValid());
  Workspace w;
  for(int cycle=0;cycle<3;++cycle) {
   w.controller.loadLayoutFromJson(file);
   if(addition=="130312") {
    QTimer::singleShot(0, [] {
     for(QWidget *widget: QApplication::topLevelWidgets())
      if(auto *dialog=qobject_cast<QDialog*>(widget);dialog && dialog->windowTitle()=="Select Temperature Source") dialog->accept();
    });
   }
   if(!addition.isEmpty()) w.controller.addWidgetAsComboboxTextChanged(addition,addition,addition=="130312"?"Environment":"Navigation");
   QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
   const int count=addition.isEmpty()?35:36;
   QCOMPARE(w.model()->getPgnList().size(),count);
   int addedId=-1;
   QString parameter=addition=="130312"?"SetTemperature":"Heading";
   if(!addition.isEmpty()) {
    addedId=w.model()->getMaxIdX();w.model()->setCurrentPgn(addedId);
    const int row=paramRow(w.model(),addedId,parameter);QVERIFY(row>=0);
    QVERIFY(w.model()->setData(w.model()->index(row,1),addition=="130312"?305.25:1.25));
   }
   double expected = addition=="130312"?305.25:1.25;
   if(addedId>=0) {
    const int row=paramRow(w.model(),addedId,parameter);
    QCOMPARE(w.model()->getPgnParams(addedId)[row][1].toDouble(),expected);
   }
   if(active) {
    for(auto *engine:w.area.findChildren<GeneratorEngine*>()) engine->setEngineRunning(true);
    QTest::qWait(300);
    if(addition=="HDT") {
     // The live generator owns Heading in Sender mode. Force its existing
     // one-second publication instead of depending on elapsed GUI setup time.
     auto *movement=w.area.findChild<GeneratorVesselMovement*>(); QVERIFY(movement);
     QVERIFY(QMetaObject::invokeMethod(movement,"PollTimerElapsed",Qt::DirectConnection));
     QVERIFY(QMetaObject::invokeMethod(movement,"PollTimerElapsed",Qt::DirectConnection));
     for(int drain=0;drain<3;++drain) QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
     const int row=paramRow(w.model(),addedId,parameter);
     expected=movement->Heading();
     // HDT declares true heading even when the saved source uses magnetic.
     if(movement->HeadingReference()==N2khr_magnetic)
      expected=Convert::DegreesToRadians(Convert::NormalizeHeading(Convert::RadiansToDegrees(expected+movement->Variation()+movement->Deviation())));
     QCOMPARE(w.model()->getPgnParams(addedId)[row][1].toDouble(),expected);
     qInfo() << "HDT editor 1.25 replaced before save by live generator:" << expected;
    }
   }
   const QString saved=dir.filePath(QString("Test1-%1.json").arg(cycle));
   w.controller.saveLayoutToJson(saved);QVERIFY(QFile::exists(saved));
   QList<QPointer<QWidget>> oldWidgets;
   for(const QString &group:w.controller.getGroupList())
    for(auto *widget:*w.controller.getListWidgetsOnSubForm(group)) oldWidgets.append(widget);
   w.controller.removeAllWidgets();
   for(const auto &widget:oldWidgets) QVERIFY(widget.isNull());
   QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
   QCOMPARE(w.model()->getPgnList().size(),0);
   QCOMPARE(w.model()->rowCount(),0);
   w.controller.loadLayoutFromJson(saved);
   QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
   QCOMPARE(w.model()->getPgnList().size(),count);
   if(addedId>=0) {
    auto *widgets=w.controller.getListWidgetsOnSubForm(addition=="130312"?"Environment":"Navigation");
    QWidget *restored=nullptr;
    for(auto *widget:*widgets) if(widget->property("Id").toInt()==addedId) restored=widget;
    QVERIFY(restored);
    QCOMPARE(restored->property(qPrintable(parameter)).toDouble(),expected);
    const int row=paramRow(w.model(),addedId,parameter);QVERIFY(row>=0);
    QCOMPARE(w.model()->getPgnParams(addedId)[row][1].toDouble(),expected);
   }
   w.controller.removeAllWidgets();
   QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  }
 }
 void defaultLifecycle() {
  const QString file=qEnvironmentVariable("RAYNMEA_TEST_DEFAULT");QVERIFY(QFile::exists(file));
  Workspace w;
  for(int cycle=0;cycle<5;++cycle) {
   if(cycle)confirmReplacement();
   w.controller.loadLayoutFromJson(file);
   QCOMPARE(w.model()->getPgnList().size(),35);
   auto engines=w.area.findChildren<GeneratorEngine*>();QCOMPARE(engines.size(),2);
   for(auto *engine:engines) QVERIFY(!engine->EngineRunning());
   if(cycle%2) {for(auto *engine:engines)engine->setEngineRunning(true);QTest::qWait(300);}
   QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  }
  w.controller.removeAllWidgets();
  QTest::qWait(300);
  QCOMPARE(w.model()->getPgnList().size(),0);
 }
};
int main(int argc,char **argv) {
 QApplication app(argc,argv);
 QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
 LayoutTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "layout_tests.moc"
