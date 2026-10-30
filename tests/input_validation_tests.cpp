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
#include <cmath>
#include <memory>
#include <vector>
#include <QAction>
#include <QDoubleValidator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QNetworkProxy>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include "mainwindow.h"
#include "appsettings.h"
#include "temperaturewidget130312.h"
#include "environmentalinstruments.h"
#include "expansioninstruments.h"
#include "aisclassbstatic129809.h"
#include "n2khelper.h"

namespace {
void settle(){QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);QCoreApplication::processEvents();}
void type(QLineEdit *edit,const QString &text){edit->setFocus();edit->selectAll();QTest::keyClick(edit,Qt::Key_Backspace);QTest::keyClicks(edit,text);}
void leave(QLineEdit *edit){QFocusEvent event(QEvent::FocusOut,Qt::TabFocusReason);QApplication::sendEvent(edit,&event);}
void capture(QWidget &widget,const QString &name){const auto out=qEnvironmentVariable("RAY_INPUT_OUTPUT");if(!out.isEmpty()){QDir().mkpath(out);QVERIFY(widget.grab().save(out+"/"+name+".png"));}}
struct TemperatureForm {
 QWidget host; QVBoxLayout layout{&host}; TemperatureWidget130312 temperature{1,"Temperature",&host}; QPushButton other{"Next field",&host};
 QLineEdit *edit=temperature.findChild<QLineEdit*>("lineEdit");
 TemperatureForm(){layout.addWidget(&temperature);layout.addWidget(&other);UserUnitPrefs prefs;prefs.tempUnitIndex=2;temperature.setUserPrefs(prefs);temperature.setActualTemperature(300.123456789);host.resize(460,210);host.show();(void)QTest::qWaitForWindowExposed(&host);host.activateWindow();QTest::qWait(50);edit->setFocus();settle();}
};
}
class InputValidationTests:public QObject {
 Q_OBJECT
private slots:
 void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Preference/PromptSaveLayoutOnClose",false);s.setValue("Nmea2000/InterfaceType","None");s.setValue("Nmea2000/Nmea2000Enabled",false);s.setValue("Nmea0183/Nmea0183Enabled",false);s.sync();}
 void reportedTemperatureEnterAndFocus(){
  TemperatureForm f;QSignalSpy measurements(&f.temperature,&TemperatureWidget130312::ActualTemperatureChanged);const auto original=f.edit->text();
  QVERIFY(f.edit->property("_rayInstrumentCommitSlot").isValid());
  type(f.edit,"-99999");QCOMPARE(f.edit->text(),QString("-99999"));QCOMPARE(f.temperature.ActualTemperature(),300.123456789);QCOMPARE(measurements.count(),0);capture(f.host,"invalid-while-typing");
  QTest::keyClick(f.edit,Qt::Key_Return);QCOMPARE(f.edit->text(),original);QCOMPARE(f.temperature.ActualTemperature(),300.123456789);QCOMPARE(measurements.count(),0);QVERIFY(!f.edit->isModified());capture(f.host,"invalid-after-enter");
  type(f.edit,"-99999");QTRY_VERIFY(f.edit->hasFocus());QTest::mouseClick(&f.other,Qt::LeftButton);QTRY_VERIFY(!f.edit->hasFocus());settle();QCOMPARE(f.edit->text(),original);QCOMPARE(f.temperature.ActualTemperature(),300.123456789);QCOMPARE(measurements.count(),0);capture(f.host,"invalid-after-focus-loss");
 }
 void limits_data(){QTest::addColumn<int>("unit");QTest::newRow("kelvin")<<0;QTest::newRow("celsius")<<1;QTest::newRow("fahrenheit")<<2;}
 void limits(){QFETCH(int,unit);TemperatureForm f;auto prefs=f.temperature.UserPrefs();prefs.tempUnitIndex=unit;f.temperature.setUserPrefs(prefs);const auto *v=qobject_cast<const QDoubleValidator*>(f.edit->validator());QVERIFY(v);const double step=std::pow(10.,-v->decimals());
  for(double bad:{v->bottom()-step,v->top()+step}){const auto before=f.edit->text();const double canonical=f.temperature.ActualTemperature();QSignalSpy spy(&f.temperature,&TemperatureWidget130312::ActualTemperatureChanged);f.edit->setText(QString::number(bad,'f',v->decimals()));f.edit->setModified(true);QTest::keyClick(f.edit,Qt::Key_Return);QCOMPARE(f.edit->text(),before);QCOMPARE(f.temperature.ActualTemperature(),canonical);QCOMPARE(spy.count(),0);}
  for(double good:{v->bottom(),v->bottom()+step,v->top()-step,v->top()}){const QString text=QString::number(good,'f',v->decimals());const double before=f.temperature.ActualTemperature();QSignalSpy spy(&f.temperature,&TemperatureWidget130312::ActualTemperatureChanged);type(f.edit,text);QCOMPARE(f.temperature.ActualTemperature(),before);QTest::keyClick(f.edit,Qt::Key_Return);QVERIFY(std::abs(f.temperature.ActualTemperature()-N2kHelper::convertUserTemperatureToN2kUnits(good,prefs))<1e-9);QCOMPARE(spy.count(),1);leave(f.edit);QCOMPARE(spy.count(),1);}
 }
 void emptyMalformedAndIntermediate(){TemperatureForm f;for(const auto &text:QStringList{"","-","not a number","1e","-99999"}){const auto original=f.edit->text();QSignalSpy spy(&f.temperature,&TemperatureWidget130312::ActualTemperatureChanged);f.edit->setText(text);f.edit->setModified(true);leave(f.edit);QCOMPARE(f.edit->text(),original);QCOMPARE(f.temperature.ActualTemperature(),300.123456789);QCOMPARE(spy.count(),0);}}
 void precisionUnitsUnavailableAndExternalUpdate(){TemperatureForm f;QSignalSpy spy(&f.temperature,&TemperatureWidget130312::ActualTemperatureChanged);
  type(f.edit,f.edit->text());QTest::keyClick(f.edit,Qt::Key_Return);QCOMPARE(f.temperature.ActualTemperature(),300.123456789);QCOMPARE(spy.count(),0);
  type(f.edit,"-99999");f.temperature.setActualTemperature(310.987654321);spy.clear();f.edit->setText("-");f.edit->setModified(true);leave(f.edit);QCOMPARE(f.edit->text(),QString("100.11"));QCOMPARE(f.temperature.ActualTemperature(),310.987654321);QCOMPARE(spy.count(),0);
  type(f.edit,"-99999");auto prefs=f.temperature.UserPrefs();prefs.tempUnitIndex=1;f.temperature.setUserPrefs(prefs);QCOMPARE(f.edit->text(),QString("37.8"));QTest::keyClick(f.edit,Qt::Key_Return);QCOMPARE(f.temperature.ActualTemperature(),310.987654321);QCOMPARE(spy.count(),0);
  type(f.edit,"0.0");QTest::keyClick(f.edit,Qt::Key_Return);QCOMPARE(f.temperature.ActualTemperature(),273.15);QCOMPARE(spy.count(),1);spy.clear();type(f.edit,"-18.0");QTest::keyClick(f.edit,Qt::Key_Return);QVERIFY(std::abs(f.temperature.ActualTemperature()-255.15)<1e-9);QCOMPARE(spy.count(),1);
  f.temperature.setActualTemperature(N2kDoubleNA);spy.clear();type(f.edit,"-");leave(f.edit);QCOMPARE(f.edit->text(),QString("Unavailable"));QCOMPARE(f.temperature.ActualTemperature(),N2kDoubleNA);QCOMPARE(spy.count(),0);
  f.temperature.setActualTemperature(600.123456789);spy.clear();type(f.edit,"-");leave(f.edit);QCOMPARE(f.temperature.ActualTemperature(),600.123456789);QCOMPARE(f.edit->text(),QString("327.0"));QCOMPARE(spy.count(),0);
 }
 void legacyInventoryAndRollback(){
  MainWindow window;auto *c=window.findChild<MainController*>();c->setMode(Viewer);auto *model=window.findChild<PgnTableController*>()->getPgnTableModel();QJsonArray inventory;int covered=0;
  const QStringList codes{"127257","127508","129026","VTG","129283","ZDA","127506","128267","127489","127488","127505","GLL","GGA","129029","HDG","127252","129025","127258","129284","130310","127251","127245","128259","130316","130312","127493","130576","HDT","127250","RMC"};
  // Use the existing category routing; no generators or external interfaces.
  const QMap<QString,QString> groups{{"127257","Navigation"},{"127508","Electrical"},{"129026","Navigation"},{"VTG","Navigation"},{"129283","Navigation"},{"ZDA","Navigation"},{"127506","Electrical"},{"128267","Navigation"},{"127489","Propulsion"},{"127488","Propulsion"},{"127505","Tanks"},{"GLL","Navigation"},{"GGA","Navigation"},{"129029","Navigation"},{"HDG","Navigation"},{"127252","Navigation"},{"129025","Navigation"},{"127258","Navigation"},{"129284","Navigation"},{"130310","Environment"},{"127251","Navigation"},{"127245","Propulsion"},{"128259","Navigation"},{"130316","Environment"},{"130312","Environment"},{"127493","Propulsion"},{"130576","Propulsion"},{"HDT","Navigation"},{"127250","Navigation"},{"RMC","Navigation"}};
  for(const auto &code:codes)c->addWidgetAsComboboxTextChanged(code,code,groups.value(code));settle();
  for(auto *widget:window.findChildren<PgnWidgetBase*>()){widget->ensurePolished();widget->setInitialGuiValues();QJsonArray fields;
   for(auto *edit:widget->findChildren<QLineEdit*>()){if(!edit->property("_rayInstrumentCommitSlot").isValid())continue;++covered;const auto original=edit->text();const auto rows=model->getPgnParams(widget->Id());QSignalSpy changes(model,&PgnTableModel::parameterChanged);
    std::vector<std::unique_ptr<QSignalSpy>> notifications;QSet<int> observed;
    for(int property=0;property<widget->metaObject()->propertyCount();++property){const auto p=widget->metaObject()->property(property);if(p.hasNotifySignal() && !observed.contains(p.notifySignalIndex())){observed.insert(p.notifySignalIndex());notifications.push_back(std::make_unique<QSignalSpy>(widget,p.notifySignal()));}}
    edit->setText("");edit->setModified(true);QTest::keyClick(edit,Qt::Key_Return);QCOMPARE(edit->text(),original);QCOMPARE(model->getPgnParams(widget->Id()),rows);QCOMPARE(changes.count(),0);for(const auto &spy:notifications)QCOMPARE(spy->count(),0);fields.append(QJsonObject{{"name",edit->objectName()},{"validator",edit->validator()->metaObject()->className()}});}
   inventory.append(QJsonObject{{"class",widget->metaObject()->className()},{"fields",fields}});
  }
  const auto out=qEnvironmentVariable("RAY_INPUT_OUTPUT",QDir::currentPath());if(!out.isEmpty()){QFile file(out+"/runtime-inventory.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(inventory).toJson());}
  QCOMPARE(covered,64);
  QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
 void fieldSpecificValidators(){
  MainWindow window;auto *c=window.findChild<MainController*>();c->setMode(Viewer);
  c->addWidgetAsComboboxTextChanged("127252","Heave","Navigation");c->addWidgetAsComboboxTextChanged("127505","Fluid","Tanks");c->addWidgetAsComboboxTextChanged("129284","Navigation","Navigation");settle();
  auto find=[&](int pgn){for(auto *w:window.findChildren<PgnWidgetBase*>())if(w->Pgn()==pgn){w->ensurePolished();w->setInitialGuiValues();return w;}return static_cast<PgnWidgetBase*>(nullptr);};
  auto *heave=find(127252);auto *fluid=find(127505);auto *navigation=find(129284);QVERIFY(heave&&fluid&&navigation);
  auto prefs=heave->UserPrefs();prefs.distanceUnitIndex=0;heave->setUserPrefs(prefs);auto *h=heave->findChild<QLineEdit*>("lineEditHeave");type(h,"-1.25");QTest::keyClick(h,Qt::Key_Return);QCOMPARE(heave->property("Heave").toDouble(),-1.25);type(h,"-101");leave(h);QCOMPARE(h->text(),QString("-1.25"));QCOMPARE(heave->property("Heave").toDouble(),-1.25);
  auto *level=fluid->findChild<QLineEdit*>("lineEditLevel");type(level,"-1.0");QTest::keyClick(level,Qt::Key_Return);QCOMPARE(fluid->property("Level").toDouble(),-1.);type(level,"131.1");leave(level);QCOMPARE(fluid->property("Level").toDouble(),-1.);QCOMPARE(level->text(),QString("-1.0"));
  auto *capacity=fluid->findChild<QLineEdit*>("lineEditCapacity");prefs=fluid->UserPrefs();prefs.volumeUnitIndex=0;fluid->setUserPrefs(prefs);type(capacity,"12.3");QTest::keyClick(capacity,Qt::Key_Return);QCOMPARE(fluid->property("Capacity").toDouble(),12.3);prefs.volumeUnitIndex=1;fluid->setUserPrefs(prefs);type(capacity,"1.0");QTest::keyClick(capacity,Qt::Key_Return);QCOMPARE(fluid->property("Capacity").toDouble(),N2kHelper::convertUserVolumeToN2kUnits(1.,prefs));const auto volume=fluid->property("Capacity");type(capacity,"1e99");leave(capacity);QCOMPARE(fluid->property("Capacity"),volume);
  auto *bearing=navigation->findChild<QLineEdit*>("lineEditBearingPositionToDestinationWaypoint");type(bearing,"360.00");QTest::keyClick(bearing,Qt::Key_Return);QVERIFY(std::abs(navigation->property("BearingPositionToDestinationWaypoint").toDouble()-2*M_PI)<1e-9);type(bearing,"361");leave(bearing);QCOMPARE(bearing->text(),QString("360.00"));
  auto *eta=navigation->findChild<QLineEdit*>("lineEditETATime");const QList<QPair<QString,double>> valid{{"0",0.},{"60",60.},{"90:00",5400.},{"12:34:56",45296.},{"23:59:60",86400.},{"1 day 00:00:01",86401.},{"1day 00:00:00",86400.}};
  for(const auto &sample:valid){type(eta,sample.first);QTest::keyClick(eta,Qt::Key_Return);QCOMPARE(navigation->property("ETATime").toDouble(),sample.second);}
  const auto before=eta->text();const auto seconds=navigation->property("ETATime");const auto property=navigation->metaObject()->property(navigation->metaObject()->indexOfProperty("ETATime"));QSignalSpy etaChanges(navigation,property.notifySignal());
  for(const auto &bad:QStringList{"86402","12:99:00","1 day 00:00:02","bad","-1","12:00:99"}){type(eta,bad);QTest::keyClick(eta,Qt::Key_Return);QCOMPARE(eta->text(),before);QCOMPARE(navigation->property("ETATime"),seconds);QCOMPARE(etaChanges.count(),0);}
 }
 void modernEditorsKeepOwnPolicy(){
  Humidity130313 humidity(1,"Humidity");humidity.setValue("ActualHumidity",55.123456789);humidity.show();settle();auto *e=humidity.findChild<QLineEdit*>("ActualHumidity");QVERIFY(!e->property("_rayInstrumentCommitSlot").isValid());QSignalSpy spy(&humidity,&EnvironmentalInstrument::parameterValueChanged);
  for(const auto &bad:QStringList{"-99999","101","","not a number"}){type(e,bad);QTest::keyClick(e,Qt::Key_Return);QCOMPARE(humidity.value("ActualHumidity"),55.123456789);QCOMPARE(e->text(),QString("55.12"));QCOMPARE(spy.count(),0);}
  type(e,"0");QTest::keyClick(e,Qt::Key_Return);QCOMPARE(humidity.value("ActualHumidity"),0.);QCOMPARE(spy.count(),1);leave(e);QCOMPARE(spy.count(),1);type(e,"Unavailable");QTest::keyClick(e,Qt::Key_Return);QCOMPARE(humidity.value("ActualHumidity"),N2kDoubleNA);
 }
 void modernAisAndExpansionRollback(){
  AisClassBStatic129809 ais(1,"AIS");ais.setUserID(123456789);ais.show();settle();auto *mmsi=ais.findChild<QLineEdit*>("UserID");QVERIFY(mmsi);QVERIFY(!mmsi->property("_rayInstrumentCommitSlot").isValid());QSignalSpy aisChanges(&ais,&AisClassBStatic129809::parameterValueChanged);
  type(mmsi,"-99999");QTest::keyClick(mmsi,Qt::Key_Return);QCOMPARE(ais.UserID(),quint32(123456789));QCOMPARE(mmsi->text(),QString("123456789"));QCOMPARE(aisChanges.count(),0);
  ExpansionInstrument pressure(2,"Pressure","130314");pressure.setValue("ActualPressure","100123.456789");pressure.show();settle();auto *edit=pressure.findChild<QLineEdit*>("lineEditActualPressure");QVERIFY(edit);QVERIFY(!edit->property("_rayInstrumentCommitSlot").isValid());const auto before=edit->text();QSignalSpy changes(&pressure,&ExpansionInstrument::parameterValueChanged);
  for(const auto &bad:QStringList{"1e99","","not a number"}){type(edit,bad);QTest::keyClick(edit,Qt::Key_Return);QCOMPARE(pressure.value("ActualPressure"),QString("100123.456789"));QCOMPARE(edit->text(),before);QCOMPARE(changes.count(),0);}
 }
 void layoutAndSendingPreserved(){MainWindow window;auto *c=window.findChild<MainController*>();c->setMode(Viewer);c->addWidgetAsComboboxTextChanged("130312","Temperature","Environment");settle();auto *w=window.findChild<TemperatureWidget130312*>();QVERIFY(w);w->setActualTemperature(300.123456789);w->ensurePolished();auto *e=w->findChild<QLineEdit*>("lineEdit");type(e,"-99999");QTest::keyClick(e,Qt::Key_Return);QCOMPARE(w->ActualTemperature(),300.123456789);
  auto *model=window.findChild<PgnTableController*>()->getPgnTableModel();type(e,"-");model->setCurrentPgn(w->Id());bool updated=false;for(int row=0;row<model->rowCount();++row)if(model->data(model->index(row,0)).toString()=="ActualTemperature")updated=model->setData(model->index(row,1),QString("310.987654321"));QVERIFY(updated);settle();e->setText("-");e->setModified(true);leave(e);QCOMPARE(w->ActualTemperature(),310.987654321);w->setActualTemperature(300.123456789);settle();
  QTemporaryDir temp;const auto file=temp.filePath("layout.json");QVERIFY(c->saveLayoutToJson(file));MainWindow restored;QVERIFY(restored.findChild<MainController*>()->loadLayoutFromJson(file));QCOMPARE(restored.findChild<TemperatureWidget130312*>()->ActualTemperature(),300.123456789);QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());QVERIFY(!restored.findChild<QAction*>("actionStart_Sending")->isChecked());}
};
int main(int argc,char **argv){QTemporaryDir profile(QDir::currentPath()+"/input-profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*> pointers;for(auto &a:args)pointers<<a.data();int count=pointers.size();pointers<<nullptr;QApplication app(count,pointers.data());QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));InputValidationTests tests;return QTest::qExec(&tests,argc,argv);}
#include "input_validation_tests.moc"
