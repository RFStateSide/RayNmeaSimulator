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
#include <QPushButton>
#include <QAction>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QNetworkProxy>
#include "mainwindow.h"
#include "appsettings.h"
#include "temperaturewidget130312.h"
#include "batterystatus127508.h"
#include "rateofturn127251.h"
#include "navigation129284.h"
#include "generatorvesselmovement.h"
#include "clickablelabel.h"
#include "n2khelper.h"
#include "pgntreemodel.h"
#include "chartplotterroutedialog.h"
#include "logviewer.h"
#include "nmea2000diagnosticsdialog.h"
#include "nmea2000diagnostics.h"
#include "transportcapabilities.h"
#include "N2kMsg.h"

namespace {
void settle(){for(int i=0;i<5;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);QCoreApplication::processEvents();}
void capture(QWidget &widget,const QString &name){const auto out=qEnvironmentVariable("RAY_WORDING_OUTPUT");if(!out.isEmpty()){QDir().mkpath(out);QVERIFY(widget.grab().save(out+"/"+name+".png"));}}
}
class UiWordingTests:public QObject {
 Q_OBJECT
 QTimer answers;
private slots:
 void initTestCase(){connect(&answers,&QTimer::timeout,[]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))if(auto *no=box->button(QMessageBox::No))no->click();});answers.start(0);}
 void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Preference/PromptSaveLayoutOnClose",false);s.setValue("Nmea2000/Nmea2000Enabled",false);s.setValue("Nmea2000/InterfaceType","None");s.setValue("Nmea0183/Nmea0183Enabled",false);s.sync();}
 void kelvinAndCanonicalValues(){
  TemperatureWidget130312 temperature(1,"Temperature",nullptr);temperature.setActualTemperature(273.15);
  BatteryStatus127508 battery(2,"Battery",nullptr);battery.setBatteryVoltage(12.6);battery.setBatteryCurrent(-3.2);battery.setBatteryTemperature(273.15);
  for(int index=0;index<3;++index){UserUnitPrefs prefs;prefs.tempUnitIndex=index;temperature.setUserPrefs(prefs);battery.setUserPrefs(prefs);const auto unit=index==0?QString("K"):index==1?QString::fromUtf8("°C"):QString::fromUtf8("°F");
   QCOMPARE(temperature.findChild<QLabel*>("labelUnits")->text(),unit);QCOMPARE(battery.findChild<QLabel*>("labelUnitsBatteryTemperature")->text(),unit);
   const auto shown=QString::number(N2kHelper::convertN2kTemperatureToUserUnits(273.15,prefs),'f',1);QCOMPARE(temperature.findChild<QLineEdit*>("lineEdit")->text(),QString::number(N2kHelper::convertN2kTemperatureToUserUnits(273.15,prefs),'f',index==1?1:2));QCOMPARE(battery.findChild<QLineEdit*>("lineEditBatteryTemperature")->text(),shown);QCOMPARE(temperature.ActualTemperature(),273.15);QCOMPARE(battery.BatteryTemperature(),273.15);if(index==0){capture(temperature,"temperature-kelvin");capture(battery,"battery-kelvin");}
  }
  temperature.setActualTemperature(N2kDoubleNA);battery.setBatteryTemperature(N2kDoubleNA);QCOMPARE(temperature.findChild<QLineEdit*>("lineEdit")->text(),QString("Unavailable"));QCOMPARE(battery.findChild<QLineEdit*>("lineEditBatteryTemperature")->text(),QString("Unavailable"));
 }
 void turnRatePrecisionAndUnavailable(){
  RateOfTurn127251 turn(1,"Turn",nullptr);QCOMPARE(turn.RateOfTurn(),N2kDoubleNA);QCOMPARE(turn.findChild<QLineEdit*>("lineEditRateOfTurn")->text(),QString("Unavailable"));turn.setRateOfTurn(-0.1);
  for(int unit=0;unit<2;++unit){turn.setRateOfTurn(-0.1);UserUnitPrefs prefs;prefs.rateOfTurnUnitIndex=unit;turn.setUserPrefs(prefs);auto *edit=turn.findChild<QLineEdit*>("lineEditRateOfTurn");QVERIFY(edit);QString input=unit==0?"-0.10000":"-5.73";int at=0;QCOMPARE(edit->validator()->validate(input,at),QValidator::Acceptable);QCOMPARE(turn.RateOfTurn(),-0.1);
   for(double value:{0.,0.1,-0.1}){turn.setRateOfTurn(value);QCOMPARE(edit->text(),QString::number(N2kHelper::convertN2kRateOfTurnToUserUnits(value,prefs),'f',unit==0?5:2));QCOMPARE(turn.RateOfTurn(),value);}
   capture(turn,unit==0?"turn-radians":"turn-degrees");turn.setRateOfTurn(N2kDoubleNA);QCOMPARE(edit->text(),QString("Unavailable"));turn.setInitialGuiValues();QCOMPARE(edit->text(),QString("Unavailable"));QCOMPARE(turn.RateOfTurn(),N2kDoubleNA);
  }
 }
 void sharedControlsAndRouteGuidance(){
  MainWindow window;auto *controller=window.findChild<MainController*>();QVERIFY(controller->loadLayoutFromJson(":/defaults/defaultLayout.json",[](const QString&){return true;}));settle();QSet<QString> families;int legacy=0,menus=0;
  for(auto *widget:window.findChildren<PgnWidgetBase*>()){widget->ensurePolished();if(widget->isGenerator()){widget->initializeGeneratorControls();families.insert(widget->metaObject()->className());auto *enable=widget->findChild<QCheckBox*>("checkBoxEnabled");auto *dispatch=widget->findChild<QCheckBox*>("checkBoxDispatch");QVERIFY(enable && dispatch);QVERIFY(enable->toolTip().contains("local"));QVERIFY(dispatch->toolTip().contains("Enable Sending"));}
   else if(auto *enable=widget->findChild<QCheckBox*>("checkBoxEnabled")){QVERIFY2(enable->toolTip().contains("Sending"),qPrintable(enable->toolTip()));++legacy;}
   for(auto *unit:widget->findChildren<ClickableLabel*>())if(!unit->toolTip().isEmpty()){++menus;}
  }
  QCOMPARE(families.size(),5);QVERIFY(legacy>10);QVERIFY(menus>10);
  ClickableLabel selector("Units",{"A","B"});QVERIFY(selector.toolTip().contains("Click"));QVERIFY(selector.toolTip().contains("Stored"));
  auto *movement=window.findChild<GeneratorVesselMovement*>();QVERIFY(movement);const auto select=movement->findChild<QPushButton*>("pushButtonSelectRouteFile")->toolTip();QVERIFY(select.contains("GPX/KML"));QVERIFY(select.contains("chartplotter"));QVERIFY(select.contains("authorization"));QVERIFY(select.contains("does not start"));
  const auto reverse=movement->findChild<QCheckBox*>("checkBoxReverseRoute")->toolTip();QVERIFY(reverse.contains("route end"));QVERIFY(reverse.contains("first waypoint"));
  QVERIFY(window.findChild<QAction*>("actionViewer_Rx")->toolTip().contains("separate authorization"));QVERIFY(window.findChild<QAction*>("actionSimulator_Tx")->toolTip().contains("Enable Sending"));QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
  for(const auto &theme:QStringList{"Light","Dark"}){AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.sync();window.HotReloadSettings();movement->show();settle();capture(*movement,theme+"-movement");}
 }
 void setupAndOperationHints(){
  MainWindow window;auto *setup=window.findChild<DialogSetup*>();QVERIFY(setup);setup->show();settle();auto *saveClose=setup->findChild<QPushButton*>("pushButtonClose");QCOMPARE(saveClose->text(),QString("Save and Close"));QVERIFY(saveClose->toolTip().contains("Cancel"));QVERIFY(setup->findChild<QLabel*>("credentialStorageStatus")->text().contains("Save and Close"));QVERIFY(setup->findChild<QWidget*>("tabUnits")->toolTip().contains("Property Editor"));
  auto *capability=setup->findChild<QLabel*>("actisenseCapabilityNotice");QVERIFY(capability);QCOMPARE(capability->text(),TransportCapabilities::actisenseNotice());
  ChartplotterRouteDialog routes(window.findChild<Nmea2000Handler*>(),{},&window);QVERIFY(routes.findChild<QPushButton*>("routeServiceDiscover")->toolTip().contains("Authorize"));QVERIFY(routes.findChild<QPushButton*>("routeServiceRetrieve")->toolTip().contains("No route is uploaded"));QVERIFY(routes.findChild<QPushButton*>("routeServiceSelect")->toolTip().contains("does not start"));
  LogViewer logs(&window);QVERIFY(logs.findChild<QCheckBox*>("pauseDisplay")->toolTip().contains("Refresh"));QVERIFY(logs.findChild<QPushButton*>("trafficRecording")->toolTip().contains("NMEA Viewer"));
  Nmea2000DiagnosticsDialog diagnostics(window.findChild<Nmea2000Diagnostics*>(),&window);QVERIFY(diagnostics.findChild<QPushButton*>("diagnosticRefresh")->toolTip().contains("without transmitting"));
  for(const auto &theme:QStringList{"Light","Dark"}){AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.sync();setup->reject();window.HotReloadSettings();setup->show();routes.show();settle();QVERIFY(saveClose->width()>=saveClose->fontMetrics().horizontalAdvance(saveClose->text()));capture(*setup,theme+"-setup");capture(routes,theme+"-route-service");}setup->reject();
 }
 void storedUnitsTooltipsDoNotChangeValues(){
  MainWindow window;auto *c=window.findChild<MainController*>();c->addWidgetAsComboboxTextChanged("129284","Navigation","Navigation");settle();auto *model=window.findChild<PgnTableController*>()->getPgnTableModel();auto *widget=window.findChild<Navigation129284*>();QVERIFY(widget);widget->setDistanceToWaypoint(1234.567890123);settle();model->setCurrentPgn(widget->Id());auto rows=model->getPgnParams(widget->Id());QVERIFY(model->data(model->index(0,1),Qt::ToolTipRole).toString().contains("Stored value"));
  auto prefs=widget->UserPrefs();prefs.distanceUnitIndex=(prefs.distanceUnitIndex+1)%3;widget->setUserPrefs(prefs);settle();QCOMPARE(model->getPgnParams(widget->Id()),rows);QCOMPARE(widget->DistanceToWaypoint(),1234.567890123);
  auto *tree=window.findChild<PgnTreeController*>()->getTreeModel();QVERIFY(tree->data(tree->indexFor(widget,"DistanceToWaypoint").siblingAtColumn(1),Qt::ToolTipRole).toString().contains("Stored value"));
 }
};
int main(int argc,char **argv){QTemporaryDir profile(QDir::currentPath()+"/wording-profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*> ptrs;for(auto &a:args)ptrs<<a.data();int count=ptrs.size();ptrs<<nullptr;QApplication app(count,ptrs.data());QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));UiWordingTests tests;return QTest::qExec(&tests,argc,argv);}
#include "ui_wording_tests.moc"
