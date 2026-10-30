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

#include "apppaths.h"
#include "runtimeresources.h"
#include "dialogsetup.h"
#include <QCheckBox>
#include <QStatusBar>
#include <QRegularExpression>
#include <QtTest>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include "appsettings.h"
#include <QStyleFactory>
#include <QNetworkProxy>
#include <QTemporaryDir>
#include <QMdiSubWindow>
#include "mainwindow.h"
#include "maincontroller.h"
#include "temperaturewidget130312.h"
#include "helper.h"
#include "globalpositioninggga.h"
#include "gnsspositiondata129029.h"
#include "outsideenvironmental130310.h"
#include "datetime129033.h"
#include "recommendedmingnssrmc.h"
#include "nmea0183handler.h"
#include "nmeasenderworker.h"
#include "cogsogvtg.h"
#include "datetimezda.h"
#include <QLineEdit>
#include <QSlider>
#include "n2khelper.h"
#include "generatorvesselmovement.h"
#include "navigation129284.h"
#include "crosstrack129283.h"
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QJsonObject>

class InstrumentApplicationTests : public QObject {
 Q_OBJECT
private slots:
 void layoutPreferenceHotReload() {
  AppSettings settings(APP_COMPANY,APP_NAME);
  settings.setValue("Preference/FontItalic",false);settings.setValue("Preference/WidgetColumns",2);settings.sync();
  MainWindow window;QVERIFY(!window.font().italic());auto *controller=window.findChild<MainController*>();QVERIFY(controller);
  QVERIFY(controller->loadLayoutFromJson(":/defaults/defaultLayout.json"));controller->setNumberOfWidgetWide(3);
  QTemporaryDir dir;auto path=dir.filePath("active.json");
  settings.setValue("Preference/FontItalic",true);settings.setValue("Preference/UnitsTemp",1);settings.sync();window.HotReloadSettings();
  QVERIFY(window.font().italic());QVERIFY(controller->saveLayoutToJson(path));
  QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(QJsonDocument::fromJson(file.readAll()).object()["numberOfWidgetColumns"].toInt(),3);file.close();
  for(auto *base:window.findChildren<PgnWidgetBase*>())QCOMPARE(base->UserPrefs().tempUnitIndex,1);
  settings.setValue("Preference/WidgetColumns",1);settings.sync();window.HotReloadSettings();QVERIFY(controller->saveLayoutToJson(path));
  QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(QJsonDocument::fromJson(file.readAll()).object()["numberOfWidgetColumns"].toInt(),1);file.close();
  controller->removeAllWidgets();controller->addWidgetAsComboboxTextChanged("HDT","New","Navigation");QVERIFY(controller->saveLayoutToJson(path));
  QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(QJsonDocument::fromJson(file.readAll()).object()["numberOfWidgetColumns"].toInt(),1);
  settings.setValue("Preference/WidgetColumns",2);settings.setValue("Preference/FontItalic",false);settings.setValue("Preference/UnitsTemp",2);settings.sync();window.close();
 }
 void receivedZdaInitializationAndUpdate() {
  MainWindow window;auto *controller=window.findChild<MainController*>();QVERIFY(controller);
  auto *handler=window.findChild<Nmea0183Handler*>();QVERIFY(handler);controller->removeAllWidgets();controller->setMode(Viewer);
  tNMEA0183Msg msg;QVERIFY(msg.Init("ZDA","GN"));
  for(const char *field:{"123456.50","15","10","2026","-05","30"})QVERIFY(msg.AddStrField(field));
  QVERIFY(handler->RcvdSntZDA(msg,"GN"));
  auto *zda=window.findChild<DateTimeZDA*>();QVERIFY(zda);
  QCOMPARE(zda->GPSDay(),15);QCOMPARE(zda->GPSMonth(),10);QCOMPARE(zda->GPSYear(),2026);
  QCOMPARE(zda->LZD(),-5);QCOMPARE(zda->LZMD(),30);
  QCOMPARE(zda->findChild<QLineEdit*>("lineEditDaysSince1970")->text(),QString("10/15/2026"));
  QVERIFY(msg.Init("ZDA","GN"));for(const char *field:{"123457.50","16","10","2026","00","00"})QVERIFY(msg.AddStrField(field));
  QVERIFY(handler->RcvdSntZDA(msg,"GN"));QCOMPARE(window.findChildren<DateTimeZDA*>().size(),1);QCOMPARE(zda->GPSDay(),16);
  QCOMPARE(zda->findChild<QLineEdit*>("lineEditDaysSince1970")->text(),QString("10/16/2026"));window.close();
 }
 void nmea0183CoursePropagation_data() {
  QTest::addColumn<bool>("astern");
  QTest::newRow("ahead")<<false;
  QTest::newRow("astern")<<true;
 }
 void nmea0183CoursePropagation() {
  QFETCH(bool,astern);
  MainWindow window;auto *controller=window.findChild<MainController*>();QVERIFY(controller);
  controller->loadLayoutFromJson(":/defaults/defaultLayout.json");
  controller->addWidgetAsComboboxTextChanged("RMC","Recommended Min Gnss","Navigation");
  controller->addWidgetAsComboboxTextChanged("VTG","SOG COG","Navigation");
  auto *movement=window.findChild<GeneratorVesselMovement*>();
  auto *rmc=window.findChild<RecommendedMinGnssRMC*>();auto *vtg=window.findChild<CogSogVTG*>();QVERIFY(movement&&rmc&&vtg);
  movement->setVariation(0.2);
  VesselStatus status;status.heading=90;status.sog=astern?-4:4;status.latitude=42.5;status.longitude=-70.25;
  movement->setCurrentVesselStatus(status);
  QTest::qWait(50);
  const double course=astern?3*M_PI/2:M_PI/2;
  QVERIFY(qAbs(rmc->TrueCOG()-course)<1e-8);
  QVERIFY(qAbs(vtg->TrueCOG()-course)<1e-8);
  QVERIFY(qAbs(vtg->MagneticCOG()-(course-0.2))<1e-8);
  QCOMPARE(rmc->SOG(),4.0);QCOMPARE(vtg->SOG(),4.0);
  QCOMPARE(rmc->Latitude(),42.5);QCOMPARE(rmc->Longitude(),-70.25);
  // Course can change while the vessel's heading stays fixed.
  movement->setCOG(2.1);QTest::qWait(50);
  QVERIFY(qAbs(rmc->TrueCOG()-2.1)<1e-8);QVERIFY(qAbs(vtg->TrueCOG()-2.1)<1e-8);
  movement->setVariation(-0.1);QTest::qWait(50);
  QVERIFY(qAbs(vtg->MagneticCOG()-2.2)<1e-8);
  window.close();
 }
 void nmea0183SentenceSelectionPersistence() {
  MainWindow window;auto *controller=window.findChild<MainController*>();
  auto *tables=window.findChild<PgnTableController*>();auto *sender=window.findChild<NmeaSenderWorker*>();QVERIFY(controller&&tables&&sender);
  controller->removeAllWidgets();
  const QStringList codes={"GGA","RMC","HDG","HDT","GLL","VTG","ZDA"};
  for(const auto &code:codes)controller->addWidgetAsComboboxTextChanged(code,code,"Navigation");
  QTest::qWait(50);
  const auto widgets=window.findChildren<PgnWidgetBase*>();QCOMPARE(widgets.size(),7);
  // Exercise the actual widget checkbox -> model -> scheduler path, without hardware.
  disconnect(sender,&NmeaSenderWorker::sendSnt,nullptr,nullptr);
  QSignalSpy sent(sender,&NmeaSenderWorker::sendSnt);
  auto transmit=[&] {sent.clear();sender->startSending();sender->sendNmeaMessage();sender->stopSending();};
  transmit();QCOMPARE(sent.count(),7);
  for(auto *widget:widgets) {
   auto *enabled=widget->findChild<QCheckBox*>("checkBoxEnabled");QVERIFY(enabled);
   if(qobject_cast<RecommendedMinGnssRMC*>(widget)==nullptr) enabled->click();
  }
  QTest::qWait(50);transmit();QCOMPARE(sent.count(),1);QCOMPARE(sent.first().at(0).toString(),QString("RMC"));
  const QString path=TestProfile::root()+"/nmea0183-selection.json";controller->saveLayoutToJson(path);
  controller->removeAllWidgets();controller->loadLayoutFromJson(path);
  transmit();QCOMPARE(sent.count(),1);QCOMPARE(sent.first().at(0).toString(),QString("RMC"));
  window.findChild<RecommendedMinGnssRMC*>()->findChild<QCheckBox*>("checkBoxEnabled")->click();
  QTest::qWait(50);transmit();QVERIFY(sent.isEmpty());
  sender->sendNmeaMessage();QVERIFY(sent.isEmpty());
  window.close();
 }
 void movementEngineProfileReadiness_data() {
  QTest::addColumn<int>("engineCount");
  QTest::addColumn<int>("missingMask");
  QTest::addColumn<QString>("message");
  const QString single="Cannot start vessel movement: Select an engine profile for Engine %1 in the Generate tab.";
  QTest::newRow("single-missing")<<1<<1<<single.arg(1);
  QTest::newRow("single-configured")<<1<<0<<QString();
  QTest::newRow("twin-engine-1-missing")<<2<<1<<single.arg(1);
  QTest::newRow("twin-engine-2-missing")<<2<<2<<single.arg(2);
  QTest::newRow("twin-both-missing")<<2<<3<<QString("Cannot start vessel movement: Select engine profiles for Engine 1, Engine 2 in the Generate tab.");
  QTest::newRow("twin-configured")<<2<<0<<QString();
 }
 void movementEngineProfileReadiness() {
  QFETCH(int,engineCount);QFETCH(int,missingMask);QFETCH(QString,message);
  AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Vessel/NumEngines",engineCount);settings.sync();
  MainWindow window;window.findChild<QAction*>("actionLoad_Default")->trigger();
  auto *movement=window.findChild<GeneratorVesselMovement*>();QVERIFY(movement);
  const auto engines=window.findChildren<GeneratorEngine*>();QVERIFY(engines.size()>=engineCount);
  for(auto *engine:engines) {
   const int instance=engine->EngineInstance();
   // Leave any non-required engine unconfigured too.
   engine->setEngineProfile(instance>=engineCount || (missingMask & (1<<instance)) ? "None" : "MerCruiser 7.4 MPI");
  }
  QFile route(TestProfile::root()+"/profile-readiness.kml");QVERIFY(route.open(QIODevice::WriteOnly));
  route.write("<kml><Placemark><coordinates>-70,42</coordinates></Placemark><Placemark><coordinates>-70,42.1</coordinates></Placemark></kml>");route.close();
  movement->findChild<QLineEdit*>("lineEditFilePath")->setText(route.fileName());
  for(const auto *name:{"checkBoxAutoPilotEnabled","checkBoxManualMovementEnabled"}) {
   auto *enabled=movement->findChild<QCheckBox*>(name);QVERIFY(enabled);
   QSignalSpy messages(movement,&GeneratorVesselMovement::ShowStatusBarMessage);
   QSignalSpy useVessel(movement,&GeneratorVesselMovement::UseGenVessel);
   enabled->click();
   QCOMPARE(enabled->isChecked(),missingMask==0);
   QCOMPARE(movement->StartEngineEnabled(),missingMask==0);
   QVERIFY(!messages.isEmpty());
   if(missingMask) {
    QCOMPARE(messages.last().at(0).toString(),message);
    QCOMPARE(messages.last().at(1).toInt(),5000);
    QVERIFY(useVessel.isEmpty());
    for(auto *engine:engines) QVERIFY(!engine->EngineRunning());
   } else {
    QCOMPARE(messages.last().at(0).toString(),QString(name==QString("checkBoxAutoPilotEnabled") ? "Starting Autopilot Simulator..." : "Starting Vessel Movement Simulator..."));
    QVERIFY(!useVessel.isEmpty());enabled->click();
   }
  }
  window.close();
 }
 void navigationMetadataPropagation() {
  MainWindow window;window.findChild<QAction*>("actionLoad_Default")->trigger();
  window.findChild<MainController*>()->addWidgetAsComboboxTextChanged("129283","Cross Track","Navigation");
  auto*m=window.findChild<GeneratorVesselMovement*>();auto*n=window.findChild<Navigation129284*>();auto*x=window.findChild<CrossTrack129283*>();QVERIFY(m&&n&&x);
  VesselWpBearing v;v.DistanceToWaypointM=12.5;v.EtaTimeS=43210.25;v.RemainingTimeS=10.25;v.EtaDate=20740;
  v.BearingOriginToDestinationWaypoint=1.2;v.BearingPositionToDestinationWaypoint=1.3;v.CourseRadians=1.3;
  v.OriginWaypointNumber=3;v.DestinationWaypointNumber=4;v.DestinationLatitude=42;v.DestinationLongitude=-70;v.Xte=-2.5;
  v.PerpendicularCrossed=false;v.ArrivalCircleEntered=false;m->VesselWpBearingChanged(v);m->setNavigationTerminated(false);
  QCoreApplication::processEvents();QCOMPARE(n->ETATime(),43210.25);QCOMPARE(n->ETADate(),int16_t(20740));QCOMPARE(x->XTE(),-2.5);QVERIFY(!x->NavigationTerminated());QVERIFY(!n->PerpendicularCrossed());
  QCOMPARE(n->OriginWaypointNumber(),3u);QCOMPARE(n->DestinationWaypointNumber(),4u);
  const auto data=window.findChild<PgnTableController*>()->getPgnTableModel()->getPgnDataToSend(n->Id());
  bool foundEta=false;
  for(const auto &p:data.paramDetails) if(p[0]=="ETATime") {foundEta=true;QCOMPARE(p[1].toDouble(),43210.25);}
  QVERIFY(foundEta);
  QVERIFY(m->findChild<QLineEdit*>("lineEditTimeToWp")->text()!=n->findChild<QLineEdit*>("lineEditETATime")->text());
  v.EtaTimeS=v.RemainingTimeS=v.BearingPositionToDestinationWaypoint=N2kDoubleNA;m->VesselWpBearingChanged(v);m->setNavigationTerminated(true);QCoreApplication::processEvents();
  QCOMPARE(n->ETATime(),N2kDoubleNA);QCOMPARE(m->findChild<QLineEdit*>("lineEditTimeToWp")->text(),QString("Unavailable"));QCOMPARE(n->findChild<QLineEdit*>("lineEditETATime")->text(),QString("Unavailable"));QVERIFY(x->NavigationTerminated());window.close();
 }
 void movementNavigationReferenceAndReverse() {
  MainWindow window;window.findChild<QAction*>("actionLoad_Default")->trigger();
  auto *movement=window.findChild<GeneratorVesselMovement*>();QVERIFY(movement);
  movement->setVariation(0.2);movement->setDeviation(0.1);
  VesselStatus status;status.heading=90;status.sog=-4;
  movement->setCurrentVesselStatus(status);
  QCOMPARE(movement->HeadingReference(),N2khr_true);
  QVERIFY(qAbs(movement->Heading()-M_PI/2)<1e-8);
  QVERIFY(qAbs(movement->COG()-3*M_PI/2)<1e-8);
  QCOMPARE(movement->SOG(),4.0);
  QTest::qWait(1100); // A display refresh must not become a delayed helm edit.
  QVERIFY(qAbs(movement->Heading()-M_PI/2)<1e-8);
  window.close();
 }
 void routeManualSpeedUsesDisplayUnits() {
  MainWindow window;window.findChild<QAction*>("actionLoad_Default")->trigger();
  auto *m=window.findChild<GeneratorVesselMovement*>();QVERIFY(m);
  QFile route(TestProfile::root()+"/speed-test.kml");QVERIFY(route.open(QIODevice::WriteOnly));
  route.write("<kml><Placemark><coordinates>-70,42</coordinates></Placemark><Placemark><coordinates>-70,42.1</coordinates></Placemark></kml>");route.close();
  m->findChild<QLineEdit*>("lineEditFilePath")->setText(route.fileName());
  m->findChild<QComboBox*>("comboBoxSpeedSource")->setCurrentIndex(0);
  auto *speed=m->findChild<QDoubleSpinBox*>("doubleSpinBoxSpeed");speed->setValue(12);
  auto *enabled=m->findChild<QCheckBox*>("checkBoxAutoPilotEnabled");enabled->click();QVERIFY(enabled->isChecked());
  auto *sim=m->findChild<AutoPilotSimulator*>();QVERIFY(sim);
  QCOMPARE(sim->getSpeed(),N2kHelper::convertUserSpeedToN2kUnits(12,m->UserPrefs()));
  speed->setValue(15);QCOMPARE(sim->getSpeed(),N2kHelper::convertUserSpeedToN2kUnits(15,m->UserPrefs()));
  enabled->click();window.close();
 }
 void invalidRouteDoesNotEnableAutopilot() {
  MainWindow window;window.findChild<QAction*>("actionLoad_Default")->trigger();
  auto *movement=window.findChild<GeneratorVesselMovement*>();QVERIFY(movement);
  movement->findChild<QLineEdit*>("lineEditFilePath")->setText(TestProfile::root()+"/missing.kml");
  auto *enabled=movement->findChild<QCheckBox*>("checkBoxAutoPilotEnabled");enabled->click();
  QVERIFY(!enabled->isChecked());QVERIFY(movement->NavigationTerminated());window.close();
 }
 void themePreference_data() {
  QTest::addColumn<QString>("saved");
  QTest::addColumn<QString>("expected");
  QTest::newRow("first-run") << QString() << QString("Dark");
  QTest::newRow("saved-darkeum") << QString("darkeum") << QString("Dark");
  QTest::newRow("saved-alternative") << QString("ConsoleStyle") << QString("Dark");
  QTest::newRow("saved-none") << QString("None") << QString("System");
  QTest::newRow("missing-resource") << QString("missing-m23-theme") << QString("Dark");
  QTest::newRow("empty-preference") << QString("") << QString("Dark");
 }
 void themePreference() {
  QFETCH(QString,saved); QFETCH(QString,expected);
  AppSettings settings(APP_COMPANY,APP_NAME);
  const QString key="Preference/StyleSheetName";
  if(saved.isNull()) settings.remove(key); else settings.setValue(key,saved);
  settings.setValue("M23Preservation/Sentinel","unchanged");settings.sync();
  for(int restart=0;restart<2;++restart) {
   MainWindow window;
   QCOMPARE(window.styleSheet(),RuntimeResources::styleSheet(expected));
   if(!Themes::isSystem(expected)) QVERIFY(!window.styleSheet().isEmpty());
   window.show();QTest::qWait(10);window.close();
  }
  settings.sync();
  QCOMPARE(settings.contains(key),!saved.isNull());
  if(!saved.isNull()) QCOMPARE(settings.value(key).toString(),saved);
  QCOMPARE(settings.value("M23Preservation/Sentinel").toString(),QString("unchanged"));
  settings.setValue(key,"darkeum");settings.sync();
 }
 void themeSelectionPersistence() {
  AppSettings settings(APP_COMPANY,APP_NAME);
  settings.remove("Preference/StyleSheetName");settings.sync();
  {
   DialogSetup setup;setup.show();QTest::qWait(10);
   auto *theme=setup.findChild<QComboBox*>("comboBoxAppTheme");QVERIFY(theme);
   QCOMPARE(theme->currentText(),QString("Dark"));
   theme->setCurrentText("Teal Dark");
   QVERIFY(QMetaObject::invokeMethod(&setup,"on_pushButtonSave_clicked",Qt::DirectConnection));
   setup.hide();
  }
  settings.sync();QCOMPARE(settings.value("Preference/StyleSheetName").toString(),QString("Teal Dark"));
  {
   MainWindow restarted;
   QCOMPARE(restarted.styleSheet(),RuntimeResources::styleSheet("Teal Dark"));
   DialogSetup setup;setup.show();QTest::qWait(10);
   QCOMPARE(setup.findChild<QComboBox*>("comboBoxAppTheme")->currentText(),QString("Teal Dark"));
   setup.hide();restarted.close();
  }
  settings.setValue("Preference/StyleSheetName","darkeum");settings.sync();
 }
 void runtimeDefaults() {
  QVERIFY(QFile::exists(":/Data/engine_profiles.json"));
  QVERIFY(helper::getEngineList(RuntimeResources::engineProfiles()).size()>1);
  for (const QString theme : {QString("Light"),QString("Dark"),QString("Darkeum"),QString("Teal Dark"),QString("Porcelain")}) {
   const auto css=RuntimeResources::styleSheet(theme); QVERIFY(!css.isEmpty());
   QVERIFY(!css.contains("%%APP_FOLDER%%"));
   QRegularExpression urls("url\\(\"?([^\")]+)\"?\\)");
   auto it=urls.globalMatch(css);
   while(it.hasNext()) { const auto path=it.next().captured(1); QVERIFY2(QFile::exists(path),qPrintable(path)); }
  }
  QFile::remove(TestProfile::root()+"/.raynmeasimulator/layouts/defaultLayout.json");
  MainWindow window;
  window.findChild<QAction*>("actionLoad_Default")->trigger();
  QCOMPARE(window.findChild<PgnTableController*>()->getPgnTableModel()->getPgnList().size(),35);
  QVERIFY(!QFile::exists(TestProfile::root()+"/.raynmeasimulator/layouts/defaultLayout.json"));
  window.findChild<MainController*>()->removeAllWidgets();
  const QString customPath=TestProfile::root()+"/.raynmeasimulator/layouts/defaultLayout.json";
  QFile custom(customPath);QVERIFY(custom.open(QIODevice::WriteOnly));
  const QByteArray customized=R"({"layouts":[],"numberOfWidgetColumns":4})";
  custom.write(customized);custom.close();
  window.findChild<QAction*>("actionLoad_Default")->trigger();
  QCOMPARE(window.findChild<PgnTableController*>()->getPgnTableModel()->getPgnList().size(),0);
  QVERIFY(custom.open(QIODevice::ReadOnly));QCOMPARE(custom.readAll(),customized);custom.close();custom.remove();
  window.close();
 }
 void unavailableProfiles_data() {
  QTest::addColumn<QByteArray>("contents");
  QTest::newRow("missing")<<QByteArray();
  QTest::newRow("malformed")<<QByteArray("{");
  QTest::newRow("empty")<<QByteArray("{\"engines\":[]}");
  QTest::newRow("empty-curve")<<QByteArray(R"({"engines":[{"name":"Broken","rpm_gph_pairs":[]}]})");
  QTest::newRow("duplicate-rpm")<<QByteArray(R"({"engines":[{"name":"Broken","rpm_gph_pairs":[{"rpm":600,"gph":1},{"rpm":600,"gph":2}]}]})");
  QTest::newRow("invalid-number")<<QByteArray(R"({"engines":[{"name":"Broken","rpm_gph_pairs":[{"rpm":"bad","gph":1},{"rpm":900,"gph":2}]}]})");
 }
 void unavailableProfiles() {
  QFETCH(QByteArray,contents); QTemporaryDir dir;QVERIFY(dir.isValid());
  const QString path=dir.path()+"/profiles.json";
  if(!contents.isEmpty()) {QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write(contents);}
  qputenv("RAYNMEA_TEST_ENGINE_PROFILES",path.toUtf8());
  { GeneratorEngine engine(1,"Engine");
    auto *combo=engine.findChild<QComboBox*>("comboBoxVesselEngine");
    if(combo->count()>1) combo->setCurrentIndex(1);
    QVERIFY(!engine.profileReady());engine.setEngineRunning(true);QVERIFY(!engine.EngineRunning());
    for(int i=0;i<4;++i) QVERIFY(QMetaObject::invokeMethod(&engine,"PollTimerElapsed",Qt::DirectConnection));
    QCOMPARE(engine.EngineSpeed(),0.0);QCOMPARE(engine.FuelRate(),0.0);
    QVERIFY(engine.findChild<QStatusBar*>()->currentMessage().contains("unavailable"));
  }
  qunsetenv("RAYNMEA_TEST_ENGINE_PROFILES");
 }
 void cleanup() {
  AppSettings reset(APP_COMPANY,APP_NAME);reset.remove("Vessel/EngineProfile");reset.sync();
  qunsetenv("RAYNMEA_TEST_ENGINE_PROFILES");
  AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Vessel/NumEngines",2);settings.sync();
 }
 void creationWorkflow_data() {
  QTest::addColumn<int>("count");
  QTest::newRow("single")<<1;QTest::newRow("twin")<<2;QTest::newRow("eight")<<8;
 }
 void creationWorkflow() {
  QFETCH(int,count);
  AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Vessel/NumEngines",count);settings.sync();
  for(int cycle=0;cycle<2;++cycle) {
   MainWindow window;auto *controller=window.findChild<MainController*>();QVERIFY(controller);
   controller->removeAllWidgets();
   QTimer dialogs;
   connect(&dialogs,&QTimer::timeout,[] {
    for(auto *w:QApplication::topLevelWidgets()) {
     if(!w->isVisible()) continue;
     if(auto *m=qobject_cast<QMessageBox*>(w)) {m->button(QMessageBox::Yes)->click();return;}
     if(auto *d=qobject_cast<QDialog*>(w)) {d->accept();return;}
    }
   });
   dialogs.start(20);
   QComboBox *picker=nullptr;
   for(auto *combo:window.findChildren<QComboBox*>())
    if(combo->findText("Vessel Movement (PGN -129025)")>=0) picker=combo;
   QVERIFY(picker);picker->setCurrentText("Vessel Movement (PGN -129025)");
   dialogs.stop();
   auto *v=window.findChild<GeneratorVesselMovement*>();QVERIFY(v);
   auto engines=window.findChildren<GeneratorEngine*>();QCOMPARE(engines.size(),count);
   QCOMPARE(window.findChildren<GeneratorTransLowerUnit*>().size(),count);
   for(auto *e:engines) QVERIFY(e->profileReady());
   auto *start=v->findChild<QCheckBox*>("checkBoxManualMovementEnabled");QVERIFY(start);
   // A missing selection blocks movement and all dependent engine starts.
   engines.first()->findChild<QComboBox*>("comboBoxVesselEngine")->setCurrentIndex(0);
   start->click();QVERIFY(!start->isChecked());QVERIFY(!v->StartEngineEnabled());
   engines.first()->findChild<QComboBox*>("comboBoxVesselEngine")->setCurrentIndex(1);
   start->click();QVERIFY(start->isChecked());
   QTest::qWait(50);v->setThrottlePosition(45);v->setThrottlePositionStbd(55);
   QTest::qWait(1300);
   for(auto *e:engines) {QVERIFY(e->EngineRunning());QVERIFY(e->EngineSpeed()>1000);QVERIFY(e->FuelRate()>0);}
   v->setTransmissionGear(N2kTG_Forward);v->setTransmissionGearStbd(N2kTG_Reverse);
   QTest::qWait(100);
   for(auto *trans:window.findChildren<GeneratorTransLowerUnit*>()) {
    QVERIFY(trans->EngineRunning());
    QCOMPARE(trans->TransmissionGear(),trans->EngineInstance()%2 ? N2kTG_Reverse : N2kTG_Forward);
   }
   const auto file=TestProfile::root()+"/m17-cycle.json";controller->saveLayoutToJson(file);
   controller->removeAllWidgets();QTest::qWait(50);controller->loadLayoutFromJson(file);
   QCOMPARE(window.findChildren<GeneratorEngine*>().size(),count);
   controller->removeAllWidgets();window.close();
  }
 }
 void profilePersistence_data() {
  QTest::addColumn<QString>("setup"); QTest::addColumn<QString>("mode");
  QTest::newRow("twin-setup-none")<<QString("None")<<QString("explicit");
  QTest::newRow("twin-different-default")<<QString("Mercury 90 HP FourStroke")<<QString("explicit");
  QTest::newRow("single")<<QString("None")<<QString("single");
  QTest::newRow("legacy-valid")<<QString("Mercury 25 HP FourStroke")<<QString("legacy");
  QTest::newRow("legacy-none")<<QString("None")<<QString("legacy");
  QTest::newRow("legacy-missing-default")<<QString("Removed default")<<QString("legacy");
  QTest::newRow("missing-saved")<<QString("MerCruiser 7.4 MPI")<<QString("missing");
  QTest::newRow("explicit-none")<<QString("MerCruiser 7.4 MPI")<<QString("none");
  QTest::newRow("invalid-field")<<QString("MerCruiser 7.4 MPI")<<QString("invalid");
  QTest::newRow("invalid-curve")<<QString("MerCruiser 7.4 MPI")<<QString("curve");
 }
 void profilePersistence() {
  QFETCH(QString,setup); QFETCH(QString,mode);
  AppSettings settings(APP_COMPANY,APP_NAME);
  settings.setValue("Vessel/EngineProfile",setup);settings.sync();
  QTimer emptyGroupWarnings;
  connect(&emptyGroupWarnings,&QTimer::timeout,[] {
   // The single-engine fixture leaves other tabs empty. Clear All currently
   // warns for those tabs; acknowledge only that existing informational dialog.
   for(auto *w:QApplication::topLevelWidgets())
    if(auto *m=qobject_cast<QMessageBox*>(w);m && m->isVisible() && m->windowTitle()=="No Widgets Found")m->accept();
  });
  emptyGroupWarnings.start(20);
  MainWindow window;auto *c=window.findChild<MainController*>();QVERIFY(c);
  window.findChild<QAction*>("actionLoad_Default")->trigger();
  const QString first="MerCruiser 7.4 MPI",second="Mercury 25 HP FourStroke";
  auto engines=window.findChildren<GeneratorEngine*>();QCOMPARE(engines.size(),2);
  for(auto *e:engines) {
   e->setEngineProfile(e->EngineInstance()==0?first:second);
   e->setEngineHours(12345+e->EngineInstance());
   e->setEngineRunning(true);QVERIFY(e->EngineRunning());
  }
  QTemporaryDir dir;QVERIFY(dir.isValid());const auto path=dir.path()+"/profiles-layout.json";
  QTest::qWait(30); // Publish queued widget changes into the saved parameter model.
  c->saveLayoutToJson(path); // Saving a running engine must not persist running state.
  QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));auto root=QJsonDocument::fromJson(f.readAll()).object();f.close();
  auto groups=root["layouts"].toArray();QJsonArray modified;
  for(auto group:groups) {
   auto g=group.toObject();QJsonArray widgets;
   for(auto item:g["widgets"].toArray()) {
    auto w=item.toObject();const bool engine=w["type"]=="GeneratorEngine";
    if(mode=="single" && (!engine || w["parameters"].toArray()[0].toArray()[1].toString()!="0"))continue;
    if(engine) {
     const int instance=w["parameters"].toArray()[0].toArray()[1].toString().toInt();
     QCOMPARE(w["engineProfile"].toString(),instance==0?first:second);
     if(mode=="legacy")w.remove("engineProfile");
     else if(mode=="missing")w["engineProfile"]="Unavailable saved engine";
     else if(mode=="none")w["engineProfile"]="None";
     else if(mode=="invalid")w["engineProfile"]=42;
    }
    widgets.append(w);
   }
   g["widgets"]=widgets;modified.append(g);
  }
  root["layouts"]=modified;
  QVERIFY(f.open(QIODevice::WriteOnly|QIODevice::Truncate));f.write(QJsonDocument(root).toJson());f.close();
   if(mode=="invalid") {
    const auto before=engines;QSignalSpy replacing(c,&MainController::layoutAboutToReplace);
    QVERIFY(!c->loadLayoutFromJson(path));QCOMPARE(replacing.size(),0);
    QCOMPARE(window.findChildren<GeneratorEngine*>(),before);
    for(auto *engine:before)QVERIFY(engine->EngineRunning());
    window.close();return;
   }
  if(mode=="curve") {
   QFile bad(dir.path()+"/bad.json");QVERIFY(bad.open(QIODevice::WriteOnly));
   bad.write(R"({"engines":[{"name":"MerCruiser 7.4 MPI","rpm_gph_pairs":[]},{"name":"Mercury 25 HP FourStroke","rpm_gph_pairs":[]}]})");bad.close();
   qputenv("RAYNMEA_TEST_ENGINE_PROFILES",bad.fileName().toUtf8());
  }
  for(int cycle=0;cycle<3;++cycle) {
   c->removeAllWidgets();QTest::qWait(30);c->loadLayoutFromJson(path);
   engines=window.findChildren<GeneratorEngine*>();QCOMPARE(engines.size(),mode=="single"?1:2);
   for(auto *v:window.findChildren<GeneratorVesselMovement*>())QVERIFY(!v->StartEngineEnabled());
   for(auto *e:engines) {
    const int instance=e->EngineInstance();QVERIFY(instance==0 || instance==1);
    QString expected=instance==0?first:second;
    if(mode=="legacy")expected=(setup=="Removed default"?QString("None"):setup);
    else if(mode=="missing")expected="Unavailable saved engine";
    else if(mode=="none")expected="None";
    else if(mode=="invalid")expected="";
    QCOMPARE(e->engineProfile(),expected);
    QCOMPARE(e->findChild<QComboBox*>("comboBoxVesselEngine")->currentText(),expected);
    QVERIFY(!e->EngineRunning());
    if(cycle==0)QCOMPARE(e->EngineHours(),double(12345+instance));
    const auto data=window.findChild<PgnTableController*>()->getPgnTableModel()->getPgnDataToSend(e->property("Id").toInt());
    for(const auto &p:data.paramDetails) {
     if(p[0]=="EngineHours")QCOMPARE(p[1].toDouble(),e->EngineHours());
     if(p[0]=="EngineSpeed")QCOMPARE(p[1].toDouble(),e->EngineSpeed());
     if(p[0]=="FuelRate")QCOMPARE(p[1].toDouble(),e->FuelRate());
    }
    const bool valid=(expected==first || expected==second) && mode!="curve";
    QCOMPARE(e->profileReady(),valid);
    if(valid) {
     e->findChild<QCheckBox*>("checkBoxEng1Rand")->setChecked(false);
     e->setEngineRunning(true);QVERIFY(e->EngineRunning());
     e->findChild<QSlider*>("verticalSliderThrottle")->setValue(2000);
     for(int i=0;i<4;++i)QVERIFY(QMetaObject::invokeMethod(e,"PollTimerElapsed",Qt::DirectConnection));
     QCOMPARE(e->EngineSpeed(),2000.0);
     const double gph=expected==first?6.1:0.8;
     QVERIFY(qAbs(e->FuelRate()-N2kHelper::convertUserFlowRateToN2kUnits(gph,UserUnitPrefs()))<1e-8);
     e->setEngineRunning(false);
    } else {
     e->setEngineRunning(true);QVERIFY(!e->EngineRunning());
     QVERIFY(e->findChild<QStatusBar*>()->currentMessage().contains("unavailable"));
     QVERIFY(e->findChild<QStatusBar*>()->currentMessage().contains(expected));
    }
   }
   if(mode=="explicit") {
    auto *v=window.findChild<GeneratorVesselMovement*>();QVERIFY(v);
    auto *start=v->findChild<QCheckBox*>("checkBoxManualMovementEnabled");QVERIFY(start);
    start->click();QVERIFY(start->isChecked());
    QTest::qWait(30);v->setThrottlePosition(45);v->setThrottlePositionStbd(55);
    for(auto *e:engines) {QTRY_VERIFY(e->EngineRunning());QTRY_VERIFY(e->EngineSpeed()>1000);}
    for(auto *t:window.findChildren<GeneratorTransLowerUnit*>())QTRY_VERIFY(t->EngineRunning());
    start->click();QVERIFY(!start->isChecked());
   }
   QTest::qWait(30);
   c->saveLayoutToJson(path); // Includes unavailable identities on subsequent round trips.
  }
  qunsetenv("RAYNMEA_TEST_ENGINE_PROFILES");
  for(auto *e:engines) {
   e->setEngineProfile(first);QVERIFY(e->profileReady());e->setEngineRunning(true);QVERIFY(e->EngineRunning());e->setEngineRunning(false);
  }
  c->removeAllWidgets();window.close();
 }
 void movingPositions() {
  MainWindow window;auto *c=window.findChild<MainController*>();
  window.findChild<QAction*>("actionLoad_Default")->trigger();
  c->addWidgetAsComboboxTextChanged("GGA","M17 GGA","Navigation");
  auto *v=window.findChild<GeneratorVesselMovement*>();QVERIFY(v);v->setEnabled(true);
  auto *gga=window.findChild<GlobalPositioningGGA*>();auto *gnss=window.findChild<GnssPositionData129029*>();QVERIFY(gga && gnss);
  const double lat=v->Latitude();
  v->setCOG(0);v->setTransmissionGear(N2kTG_Forward);v->setTransmissionGearStbd(N2kTG_Forward);
  v->findChild<QCheckBox*>("checkBoxManualMovementEnabled")->click();
  QTest::qWait(50);v->setThrottlePosition(45);v->setThrottlePositionStbd(45);
  QTRY_VERIFY_WITH_TIMEOUT(v->Latitude()!=lat,5000);
  QTest::qWait(100);
  QVERIFY(qAbs(gga->Latitude()-gnss->Latitude())<1e-7);
  QVERIFY(qAbs(gga->Longitude()-gnss->Longitude())<1e-7);
  QVERIFY(qAbs(gga->Latitude()-v->Latitude())<1e-7);
  for(auto *e:window.findChildren<GeneratorEngine*>()) QVERIFY(e->EngineRunning());
  c->removeAllWidgets();window.close();
 }
 void receivedRmcRouting() {
  MainWindow window;
  auto *controller=window.findChild<MainController*>();QVERIFY(controller);
  auto *handler=window.findChild<Nmea0183Handler*>();QVERIFY(handler);
  auto *tables=window.findChild<PgnTableController*>();QVERIFY(tables);
  controller->removeAllWidgets();controller->setMode(Viewer);
  tNMEA0183Msg msg;QVERIFY(msg.Init("RMC","GN"));
  for(const char *f:{"123456.50","V","4230.000","S","07015.000","E","10.0","123.4","031024","10.5","W"})QVERIFY(msg.AddStrField(f));
  QVERIFY(handler->RcvdSntRMC(msg,"GN"));
  auto widgets=window.findChildren<RecommendedMinGnssRMC*>();QCOMPARE(widgets.size(),1);
  QCOMPARE(widgets.first()->Status(),'V');QCOMPARE(widgets.first()->Latitude(),-42.5);QCOMPARE(widgets.first()->Longitude(),70.25);
  QVERIFY(handler->RcvdSntRMC(msg,"GN"));QCOMPARE(window.findChildren<RecommendedMinGnssRMC*>().size(),1);
  QVERIFY(handler->RcvdSntRMC(msg,"GP"));QCOMPARE(window.findChildren<RecommendedMinGnssRMC*>().size(),2);
  const auto list=tables->getPgnTableModel()->getPgnList();QCOMPARE(list.size(),2);
  QVERIFY(list[0].source!=list[1].source);
  controller->removeAllWidgets();window.close();
 }
 void nativeAcceptanceSequence() {
  AppSettings settings(APP_COMPANY,APP_NAME);
  settings.setValue("Vessel/EngineProfile","None");settings.sync();
  MainWindow window;
  qApp->setStyle(QStyleFactory::create("Fusion"));
  qApp->setStyleSheet(window.styleSheet());
  window.show();
  auto *controller=window.findChild<MainController*>();QVERIFY(controller);
  auto *tables=window.findChild<PgnTableController*>();QVERIFY(tables);
  auto *model=tables->getPgnTableModel();
  auto action=[&](const char *name) {
   auto *a=window.findChild<QAction*>(name);
   Q_ASSERT(a);a->trigger();
  };
  for(int cycle=0;cycle<3;++cycle) {
   action("actionLoad_Default");
   QCOMPARE(model->getPgnList().size(),35);
   auto *area=window.findChild<QMdiArea*>();QVERIFY(area);
   for(auto *clock:window.findChildren<GeneratorDateTime*>())
    for(auto *timer:clock->findChildren<QTimer*>()) timer->stop();
   auto *weather=window.findChild<GeneratorWeather*>();QVERIFY(weather);
   weather->setEnabled(true);
   weather->weatherFetched("M14 synthetic","No live requests",293.15,50,1006.1,0,0);
   auto *clock=window.findChild<GeneratorDateTime*>();QVERIFY(clock);
   clock->setDaysSince1970(QDate(1970,1,1).daysTo(QDate(2024,3,1)));
   clock->setSecondsSinceMidnight(43200);clock->setLocalOffset(-240);
   auto *movement=window.findChild<GeneratorVesselMovement*>();QVERIFY(movement);
   movement->setEnabled(true);movement->setLatitude(-42.123456);movement->setLongitude(70.654321);
   controller->addWidgetAsComboboxTextChanged("GGA","M14 GGA","Navigation");
   auto *gga=window.findChild<GlobalPositioningGGA*>();QVERIFY(gga);
   gga->setGPSQualityIndicator(1);gga->setSatelliteCount(12);gga->setHDOP(0.8);
   QTest::qWait(100);
   QCOMPARE(model->getPgnList().size(),36);
   auto *outside=window.findChild<OutsideEnvironmental130310*>();QVERIFY(outside);
   QCOMPARE(outside->AtmosphericPressure(),100610.0);
   auto *local=window.findChild<DateTime129033*>();QVERIFY(local);
   QCOMPARE(local->findChild<QLineEdit*>("lineEditDaysSince1970")->text(),QString("03/01/2024"));
   auto *gnss=window.findChild<GnssPositionData129029*>();QVERIFY(gnss);
   auto prefs=gnss->UserPrefs();prefs.latLongUnitIndex=1;gnss->setUserPrefs(prefs);gnss->setInitialGuiValues();
   QVERIFY(gnss->findChild<QLineEdit*>("lineEditLatitude")->text().endsWith("S"));
   QVERIFY(gnss->findChild<QLineEdit*>("lineEditLongitude")->text().endsWith("E"));
   for(auto *trans:window.findChildren<GeneratorTransLowerUnit*>()) {
    QVERIFY(trans->OilTemperature()>273.15);
    QVERIFY(trans->findChild<QLineEdit*>("lineEditOilTemperature")->text()!="-459.7");
   }
   for(auto *e:window.findChildren<GeneratorEngine*>())
    e->findChild<QComboBox*>("comboBoxVesselEngine")->setCurrentText(e->EngineInstance()==0?"MerCruiser 7.4 MPI":"Mercury 25 HP FourStroke");
   const QString name=QString("M14-acceptance-%1").arg(cycle);
   QTimer::singleShot(0, [name] {
    for(auto *w:QApplication::topLevelWidgets())
     if(auto *d=qobject_cast<QInputDialog*>(w)) {d->setTextValue(name);d->accept();}
   });
   action("actionSave");
   QVERIFY(QFile::exists(AppPaths::layoutsDirectory()+"/"+name+".json"));
   QTimer dialogResponder;
   connect(&dialogResponder,&QTimer::timeout,[name] {
    for(auto *w:QApplication::topLevelWidgets()) {
     if(auto *d=qobject_cast<QDialog*>(w);d && d->windowTitle()=="Load Layout") {
      d->findChild<QComboBox*>()->setCurrentText(name);d->accept();
     } else if(auto *box=qobject_cast<QMessageBox*>(w)) box->button(QMessageBox::Yes)->click();
    }
   });
   dialogResponder.start(25);action("actionLoad");dialogResponder.stop();
   QCOMPARE(model->getPgnList().size(),36);
   QTimer::singleShot(0, [] {
    for(auto *w:QApplication::topLevelWidgets())
     if(auto *d=qobject_cast<QMessageBox*>(w)) d->button(QMessageBox::Yes)->click();
   });
   action("actionClear_All");
   QCOMPARE(model->getPgnList().size(),0);
   QTest::qWait(100);
   QTimer::singleShot(0, [name] {
    for(auto *w:QApplication::topLevelWidgets())
     if(auto *d=qobject_cast<QDialog*>(w);d && d->windowTitle()=="Load Layout") {
      d->findChild<QComboBox*>()->setCurrentText(name);d->accept();
     }
   });
   action("actionLoad");
   QTest::qWait(100);
   QCOMPARE(model->getPgnList().size(),36);
   for(auto *restoredClock:window.findChildren<GeneratorDateTime*>())
    for(auto *timer:restoredClock->findChildren<QTimer*>()) timer->stop();
   gga=window.findChild<GlobalPositioningGGA*>();QVERIFY(gga);
   QCOMPARE(gga->GPSQualityIndicator(),1);QCOMPARE(gga->SatelliteCount(),12);QCOMPARE(gga->HDOP(),0.8);
   QCOMPARE(window.findChild<OutsideEnvironmental130310*>()->AtmosphericPressure(),100610.0);
   QCOMPARE(window.findChild<GeneratorDateTime*>()->LocalOffset(),int16_t(-240));
   QCOMPARE(window.findChild<GnssPositionData129029*>()->Latitude(),-42.123456);
   for(auto *e:window.findChildren<GeneratorEngine*>()) {
    QCOMPARE(e->engineProfile(),e->EngineInstance()==0?QString("MerCruiser 7.4 MPI"):QString("Mercury 25 HP FourStroke"));
    QVERIFY(!e->EngineRunning());QVERIFY(e->profileReady());
    e->setEngineRunning(true);QVERIFY(e->EngineRunning());
    for(int i=0;i<4;++i)QVERIFY(QMetaObject::invokeMethod(e,"PollTimerElapsed",Qt::DirectConnection));
    QVERIFY(e->EngineSpeed()>0);QVERIFY(e->FuelRate()>0);e->setEngineRunning(false);
   }
   controller->removeAllWidgets();
   QTest::qWait(100);
  }
  window.close();
 }
};
int main(int argc,char **argv) {
 // QtTest receives the original arguments; the application always sees --no-nmea.
 QList<QByteArray> storage;
 for(int i=0;i<argc;++i) storage.append(argv[i]);
 storage.append("--no-nmea");
 QList<char*> appArgs;for(auto &arg:storage) appArgs.append(arg.data());
 int appArgc=appArgs.size();appArgs.append(nullptr);
 QApplication app(appArgc,appArgs.data());
 QTemporaryDir profile; if(!profile.isValid()) return 1;
 qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
 AppSettings settings(APP_COMPANY,APP_NAME);
 settings.setValue("Preference/StyleSheetName","darkeum");
 settings.setValue("Vessel/NumEngines",2);settings.sync();
 QDir().mkpath(profile.path()+"/.raynmeasimulator/layouts");
 if(!QFile::copy(qEnvironmentVariable("RAYNMEA_TEST_DEFAULT"),profile.path()+"/.raynmeasimulator/layouts/defaultLayout.json")) return 1;
 QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
 InstrumentApplicationTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "instrument_application_tests.moc"
