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
#include <QLineEdit>
#include <QPointer>
#include <QAccessible>
#include <QNetworkProxy>
#include "maincontroller.h"
#include "fluidlevel127505.h"
#include "globalpositioninggga.h"
#include "helper.h"

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
#include "n2khelper.h"
#include "datetime129033.h"
#include "gnsspositiondata129029.h"
#include "latlong129025.h"
#include "generatorweather.h"
#include "outsideenvironmental130310.h"
#include <QLabel>
#include "datetimezda.h"
#include "nmea0183handler.h"
#include "NMEA0183Messages.h"
#include "N2kMessages.h"
#include "recommendedmingnssrmc.h"
#include "geographicpositiongll.h"

static QString editText(QWidget *w, const char *name) {
 auto *edit=w->findChild<QLineEdit*>(name);
 return edit ? edit->text() : QString("MISSING");
}
static void drain() {
 for(int i=0;i<5;++i) QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
}
static double parameter(PgnTableModel *m,int id,const QString &name) {
 for(const auto &p:m->getPgnParams(id)) if(p[0]==name) return p[1].toDouble();
 return qQNaN();
}
static bool editParameter(PgnTableModel *m,int id,const QString &name,double value) {
 m->setCurrentPgn(id);
 const auto params=m->getPgnParams(id);
 for(int i=0;i<params.size();++i) if(params[i][0]==name) return m->setData(m->index(i,1),value);
 return false;
}
static void stopClocks(QWidget *root) {
 for(auto *clock:root->findChildren<GeneratorDateTime*>())
  for(auto *timer:clock->findChildren<QTimer*>()) timer->stop();
}
class InstrumentTests : public QObject {
 Q_OBJECT
private slots:
 void ggaSchema() {
  Workspace w;
  w.controller.setMode(Viewer);
  w.controller.addWidgetAsComboboxTextChanged("GGA","GGA","Navigation");
  auto *gga=w.area.findChild<GlobalPositioningGGA*>();QVERIFY(gga);
  QStringList names;
  for(const auto &p:w.model()->getPgnParams(gga->Id())) names<<p[0];
  QCOMPARE(names,QStringList({"GPSTime","Latitude","Longitude","GPSQualityIndicator","SatelliteCount","HDOP","Altitude","GeoidalSeparation","DGPSAge","DGPSReferenceStationID"}));
 }
 void ggaEditingMappingAndLegacyRestore() {
  Workspace w;w.controller.setMode(Viewer);
  // Exercise the existing receive handler's field mapping without opening any port.
  Nmea0183Handler handler;
  QVERIFY(!handler.IsOpen());
  QSignalSpy received(&handler,&Nmea0183Handler::RcvdSnt);
  tNMEA0183Msg sentence;
  QVERIFY(sentence.Init("GGA","GP"));
  for(const char *field:{"123456.50","4230.0000","S","07015.0000","E","2","12","0.8","-4.5","M","-25.5","M","1.5","42"})
   QVERIFY(sentence.AddStrField(field));
  QVERIFY(handler.RcvdSntGGA(sentence));QCOMPARE(received.size(),1);
  const auto pairs=received.first()[1].toStringList();QCOMPARE(pairs.size(),20);
  w.controller.addWidgetAsComboboxTextChanged("GGA","GGA","Navigation",pairs);
  auto *gga=w.area.findChild<GlobalPositioningGGA*>();QVERIFY(gga);
  QCOMPARE(gga->Latitude(),-42.5);QCOMPARE(gga->Longitude(),70.25);
  QCOMPARE(gga->GPSQualityIndicator(),2);QCOMPARE(gga->SatelliteCount(),12);
  QCOMPARE(gga->Altitude(),-4.5);QCOMPARE(gga->GeoidalSeparation(),-25.5);
  QCOMPARE(gga->DGPSReferenceStationID(),42);
  QVERIFY(editParameter(w.model(),gga->Id(),"Latitude",-42.123456));
  QVERIFY(editParameter(w.model(),gga->Id(),"Longitude",70.654321));
  QVERIFY(editParameter(w.model(),gga->Id(),"GPSTime",86399.999));
  gga->setSatelliteCount(18);gga->setAltitude(-10.5);drain();
  QCOMPARE(parameter(w.model(),gga->Id(),"Latitude"),-42.123456);
  QCOMPARE(parameter(w.model(),gga->Id(),"Longitude"),70.654321);
  QCOMPARE(parameter(w.model(),gga->Id(),"SatelliteCount"),18.0);
  w.model()->setCurrentPgn(gga->Id());
  QCOMPARE(w.model()->getParamMin(1),-90.0);QCOMPARE(w.model()->getParamMax(2),180.0);
  // The model's ordered values match the unchanged library's ten GGA arguments.
  const auto params=w.model()->getPgnParams(gga->Id());
  QVERIFY(NMEA0183SetGGA(sentence,params[0][1].toDouble(),params[1][1].toDouble(),params[2][1].toDouble(),
   params[3][1].toInt(),params[4][1].toInt(),params[5][1].toDouble(),params[6][1].toDouble(),
   params[7][1].toDouble(),params[8][1].toDouble(),params[9][1].toInt()));
  QCOMPARE(QString(sentence.Field(2)),QString("S"));QCOMPARE(QString(sentence.Field(4)),QString("E"));
  QCOMPARE(QString(sentence.Field(5)).toInt(),2);QCOMPARE(QString(sentence.Field(6)).toInt(),18);
  QCOMPARE(QString(sentence.Field(9)),QString("M"));QCOMPARE(QString(sentence.Field(11)),QString("M"));
  QCOMPARE(QString(sentence.Field(13)).toInt(),42);
  // M1.5B.1A: graduated after the unchanged test produced XPASS with the isolated
  // bounded printf patch. Original M1.4/M1.5B.1 expected failure remains preserved.
  QCOMPARE(QString(sentence.Field(8)).toDouble(),-10.5);
  QTemporaryDir dir;const auto path=dir.filePath("GGA.json");w.controller.saveLayoutToJson(path);
  for(int cycle=0;cycle<3;++cycle) {
   w.controller.removeAllWidgets();drain();w.controller.loadLayoutFromJson(path);drain();
   gga=w.area.findChild<GlobalPositioningGGA*>();QVERIFY(gga);
   QCOMPARE(gga->Latitude(),-42.123456);QCOMPARE(gga->Longitude(),70.654321);
   QCOMPARE(gga->SatelliteCount(),18);QCOMPARE(gga->GPSTime(),86399.999);
   QCOMPARE(w.model()->getPgnParams(gga->Id()),params);
  }
  w.controller.removeAllWidgets();
  WidgetConfig legacy{};legacy.idX=500;legacy.widgetType="GlobalPositioningGGA";legacy.pgn="GGA";
  legacy.rowSpan=4;legacy.columnSpan=3;legacy.interval=1000;
  legacy.parameters={{"GPSTime","12345","s","double"},{"Status","A","","char"},
    {"Latitude","42.123456","deg","double"},{"Longitude","-70.654321","deg","double"},
    {"SOG","2","m/s","double"},{"COG","1","rad","double"},{"Date","20000","day","int"},
    {"MagneticVariation","0","rad","double"},{"MagneticVariationDirection","E","","char"},{"ModeIndicator","A","","char"}};
  w.controller.restoreLayoutFromWidgetConfig(legacy,"Navigation");drain();
  gga=w.area.findChild<GlobalPositioningGGA*>();QVERIFY(gga);
  QCOMPARE(gga->GPSTime(),12345.0);QCOMPARE(gga->Latitude(),42.123456);
  QCOMPARE(gga->Longitude(),-70.654321);QCOMPARE(gga->GPSQualityIndicator(),0);
  QCOMPARE(w.model()->getPgnParams(500).size(),10);
  QCOMPARE(w.model()->getPgnParams(500)[3][0],QString("GPSQualityIndicator"));
 }
 void pressureLayoutUnits() {
  Workspace w;w.controller.setMode(Viewer);
  WidgetConfig c{};c.idX=80;c.pgn="-130311";c.widgetType="GeneratorWeather";
  c.label="Legacy hPa";c.rowSpan=13;c.columnSpan=3;c.interval=2000;
  // Exercise legacy hPa migration with a complete, saveable schema. Keep
  // pressure first so its historical range migration is checked explicitly.
  c.parameters={{"AtmosphericPressure","1006.1","hPa","double"},
    {"SID","7","","unsigned char"}, {"AirTemperature","293.15","K","double"},
    {"SeaTemperature","290.15","K","double"}, {"Humidity","50","%","double"},
    {"WindSpeed","0","m/s","double"}, {"WindAngle","0","rad","double"},
    {"TemperatureSource","1","enum","tN2kTempSource"}, {"HumiditySource","1","enum","tN2kHumiditySource"},
    {"Latitude","42","deg","double"}, {"Longitude","-71","deg","double"}};
  c.ranges={{0,1100},{0,255},{0,1000},{0,1000},{0,100},{0,100},{0,2*M_PI},{0,255},{0,255},{-90,90},{-180,180}};
  w.controller.restoreLayoutFromWidgetConfig(c,"Generate");drain();
  auto *weather=w.area.findChild<GeneratorWeather*>();QVERIFY(weather);
  QCOMPARE(weather->Id(),80);QCOMPARE(weather->SID(),uchar(7));
  QCOMPARE(weather->AtmosphericPressure(),100610.0);
  QCOMPARE(w.model()->getPgnParams(80)[0][2],QString("Pa"));
  w.model()->setCurrentPgn(80);QCOMPARE(w.model()->getParamMax(0),110000.0);
  WidgetConfig outsideConfig=c;outsideConfig.idX=81;outsideConfig.pgn="130310";
  outsideConfig.widgetType="OutsideEnvironmental130310";outsideConfig.rowSpan=4;
  outsideConfig.parameters={{"AtmosphericPressure","1006.1","hPa","double"},
    {"SID","8","","unsigned char"}, {"OutsideAmbientAirTemperature","293.15","K","double"},
    {"WaterTemperature","290.15","K","double"}};
  outsideConfig.ranges={{0,1100},{0,255},{0,1000},{0,1000}};
  w.controller.restoreLayoutFromWidgetConfig(outsideConfig,"Environment");drain();
  auto *outsideRestored=w.area.findChild<OutsideEnvironmental130310*>();QVERIFY(outsideRestored);
  QCOMPARE(outsideRestored->Id(),81);QCOMPARE(outsideRestored->AtmosphericPressure(),100610.0);
  QTemporaryDir dir;QVERIFY(dir.isValid());const auto path=dir.filePath("pressure.json");
  QVERIFY(w.controller.saveLayoutToJson(path));
  for(int cycle=0;cycle<3;++cycle) {
   w.controller.removeAllWidgets();drain();QVERIFY(w.controller.loadLayoutFromJson(path));drain();
   const auto generators=w.area.findChildren<GeneratorWeather*>();QCOMPARE(generators.size(),1);
   const auto instruments=w.area.findChildren<OutsideEnvironmental130310*>();QCOMPARE(instruments.size(),1);
   auto *restored=generators.first();auto *outside=instruments.first();
   QCOMPARE(restored->Id(),80);QCOMPARE(restored->Pgn(),-130311);QCOMPARE(restored->Label(),QString("Legacy hPa"));
   QCOMPARE(restored->SID(),uchar(7));QCOMPARE(restored->AirTemperature(),293.15);
   QCOMPARE(restored->SeaTemperature(),290.15);QCOMPARE(restored->Humidity(),50.0);
   QCOMPARE(restored->Latitude(),42.0);QCOMPARE(restored->Longitude(),-71.0);
   QCOMPARE(restored->AtmosphericPressure(),100610.0);
   QCOMPARE(outside->Id(),81);QCOMPARE(outside->SID(),uchar(8));
   QCOMPARE(outside->AtmosphericPressure(),100610.0);
   QCOMPARE(w.model()->getPgnParams(80)[0][2],QString("Pa"));
   w.model()->setCurrentPgn(80);QCOMPARE(w.model()->getParamMax(0),110000.0);
  }
  w.controller.removeAllWidgets();
  w.controller.addWidgetAsComboboxTextChanged("130310","Outside","Environment");
  auto *outside=w.area.findChild<OutsideEnvironmental130310*>();QVERIFY(outside);
  const auto params=w.model()->getPgnParams(outside->Id());
  QCOMPARE(params[3][2],QString("Pa"));w.model()->setCurrentPgn(outside->Id());
  QCOMPARE(w.model()->getParamMax(3),110000.0);
 }
 void transmissionLegacyNames() {
  Workspace w;w.controller.setMode(Viewer);
  WidgetConfig c{};c.idX=50;c.pgn="-127493";c.widgetType="GeneratorTransLowerUnit";
  c.label="Legacy transmission";c.rowSpan=6;c.columnSpan=3;c.interval=1000;
  c.parameters={{"TransmissionInstance","1","","int"},{"TransmissionGear","2","enum","tN2kTransmissionGear"},
    {"TransmissionOilPressure","100000","Pa","double"},{"TransmissionOilTemperature","0","K","double"},{"DiscreteStatus1","0","","unsigned char"}};
  w.controller.restoreLayoutFromWidgetConfig(c,"Generate");drain();
  auto *g=w.area.findChild<GeneratorTransLowerUnit*>();QVERIFY(g);
  QCOMPARE(g->EngineInstance(),1);QCOMPARE(g->OilTemperature(),295.372);QCOMPARE(g->OilPressure(),100000.0);
  QCOMPARE(g->TransmissionGear(),N2kTG_Reverse);
  QCOMPARE(w.model()->getPgnParams(50)[3][0],QString("OilTemperature"));
  QVERIFY(editParameter(w.model(),50,"OilTemperature",310.15));drain();QCOMPARE(g->OilTemperature(),310.15);
 }
 void controllerPropagationAndPersistence() {
  Workspace w;w.controller.loadLayoutFromJson(qEnvironmentVariable("RAYNMEA_TEST_DEFAULT"));
  stopClocks(&w.area);drain();
  auto *weather=w.area.findChild<GeneratorWeather*>();QVERIFY(weather);weather->setEnabled(true);
  auto *outside=w.area.findChild<OutsideEnvironmental130310*>();QVERIFY(outside);
  weather->weatherFetched("fixture","synthetic",293.15,50,1006.1,0,0);drain();
  QCOMPARE(outside->AtmosphericPressure(),100610.0);
  QCOMPARE(parameter(w.model(),outside->Id(),"AtmosphericPressure"),100610.0);
  QCOMPARE(parameter(w.model(),weather->Id(),"AtmosphericPressure"),100610.0);
  for(int i=0;i<4;++i) {
   auto prefs=outside->UserPrefs();prefs.pressureUnitIndex=i;outside->setUserPrefs(prefs);outside->setInitialGuiValues();
   QCOMPARE(outside->findChild<QLabel*>("labelUnitsAtmosphericPressure")->text(),QStringList({"Pa","kPa","Bar","PSI"})[i]);
   QCOMPARE(editText(outside,"lineEditAtmosphericPressure"),QString::number(N2kHelper::convertN2kPressureToUserUnits(100610,prefs),'f',2));
   QCOMPARE(outside->AtmosphericPressure(),100610.0);
  }
  for(int i=0;i<5;++i) {
   auto prefs=weather->UserPrefs();prefs.atmosphericPressureUnitIndex=i;weather->setUserPrefs(prefs);weather->setInitialGuiValues();
   QCOMPARE(weather->findChild<QLabel*>("labelUnitsAtmosphericPressure")->text(),QStringList({"hPa","inHg","mmHg","Pa","atm"})[i]);
   QCOMPARE(editText(weather,"lineEditAtmosphericPressure"),QString::number(N2kHelper::convertN2kAtmosphericPressureToUserUnits(100610,prefs),'f',2));
   QCOMPARE(weather->AtmosphericPressure(),100610.0);
  }
  auto prefs=weather->UserPrefs();prefs.atmosphericPressureUnitIndex=0;weather->setUserPrefs(prefs);
  QVERIFY(QMetaObject::invokeMethod(weather,"on_lineEditAtmosphericPressure_textEdited",Q_ARG(QString,"1013.0")));
  QTRY_COMPARE(outside->AtmosphericPressure(),101300.0);drain();
  weather->setAtmosphericPressure(100610.25);drain();
  QCOMPARE(parameter(w.model(),weather->Id(),"AtmosphericPressure"),100610.25);
  QCOMPARE(parameter(w.model(),outside->Id(),"AtmosphericPressure"),100610.25);
  QVERIFY(editParameter(w.model(),weather->Id(),"AtmosphericPressure",100000));drain();
  QCOMPARE(outside->AtmosphericPressure(),100000.0);
  tN2kMsg msg;SetN2kOutsideEnvironmentalParameters(msg,1,290,293,100000);
  unsigned char sid;double water,air,pressure;
  QVERIFY(ParseN2kOutsideEnvironmentalParameters(msg,sid,water,air,pressure));QCOMPARE(pressure,100000.0);
  auto *clock=w.area.findChild<GeneratorDateTime*>();QVERIFY(clock);
  auto *local=w.area.findChild<DateTime129033*>();QVERIFY(local);
  const auto days=QDate(1970,1,1).daysTo(QDate(2024,3,1));
  clock->setDaysSince1970(days);clock->setSecondsSinceMidnight(43200);clock->setLocalOffset(-240);drain();
  QCOMPARE(local->LocalOffset(),int16_t(-240));QCOMPARE(local->DaysSince1970(),uint16_t(days));
  QCOMPARE(editText(local,"lineEditDaysSince1970"),QString("03/01/2024"));
  QVERIFY(editParameter(w.model(),clock->Id(),"LocalOffset",345));drain();
  QCOMPARE(local->LocalOffset(),int16_t(345));QCOMPARE(editText(clock,"lineEditLocalOffset"),QString("5.75"));
  QVERIFY(QMetaObject::invokeMethod(clock,"PollTimerElapsed"));drain();QCOMPARE(clock->LocalOffset(),int16_t(345));
  clock->setDaysSince1970(days);clock->setSecondsSinceMidnight(43200);drain();
  auto *movement=w.area.findChild<GeneratorVesselMovement*>();QVERIFY(movement);movement->setEnabled(true);
  movement->setLatitude(-42.123456);movement->setLongitude(70.654321);drain();
  auto *gnss=w.area.findChild<GnssPositionData129029*>();QVERIFY(gnss);
  QCOMPARE(gnss->Latitude(),-42.123456);QCOMPARE(gnss->Longitude(),70.654321);
  QCOMPARE(parameter(w.model(),gnss->Id(),"Latitude"),-42.123456);
  const auto generators=w.area.findChildren<GeneratorTransLowerUnit*>();QCOMPARE(generators.size(),2);
  WidgetConfig trans{};trans.idX=500;trans.pgn="127493";trans.widgetType="TransmissionDynamic127493";
  trans.label="M14 engine 2 transmission";trans.rowSpan=7;trans.columnSpan=3;trans.interval=1000;
  trans.parameters={{"EngineInstance","1","","unsigned char"},{"TransmissionGear","1","enum","tN2kTransmissionGear"},
    {"OilPressure","0","Pa","double"},{"OilTemperature","295.372","K","double"},{"DiscreteStatus1","0","","unsigned char"}};
  w.controller.restoreLayoutFromWidgetConfig(trans,"Propulsion");drain();
  const auto instruments=w.area.findChildren<TransmissionDynamic127493*>();QCOMPARE(instruments.size(),3);
  for(auto *g:generators) {
   QVERIFY(!g->EngineRunning());g->setEnabled(true);
   QVERIFY(editParameter(w.model(),g->Id(),"OilTemperature",300+g->EngineInstance()));drain();
   for(auto *instrument:instruments) if(instrument->EngineInstance()==g->EngineInstance()) QCOMPARE(instrument->OilTemperature(),g->OilTemperature());
   for(int i=0;i<3;++i) {
    auto units=g->UserPrefs();units.tempUnitIndex=i;g->setUserPrefs(units);
    QCOMPARE(editText(g,"lineEditOilTemperature"),QString::number(N2kHelper::convertN2kTemperatureToUserUnits(g->OilTemperature(),units),'f',1));
    QVERIFY(qAbs(N2kHelper::convertUserTemperatureToN2kUnits(N2kHelper::convertN2kTemperatureToUserUnits(g->OilTemperature(),units),units)-g->OilTemperature())<0.00001);
   }
   g->setTransmissionGear(N2kTG_Forward);g->setEngineLoad(50);g->setEngineRunning(true);
   QVERIFY(QMetaObject::invokeMethod(g,"on_radioButtonNeutral_clicked"));QCOMPARE(g->TransmissionGear(),N2kTG_Neutral);
   g->setTransmissionGear(N2kTG_Forward);
   const double before=g->OilTemperature();QVERIFY(QMetaObject::invokeMethod(g,"PollTimerElapsed"));
   QVERIFY(g->OilTemperature()>before);g->setEngineRunning(false);
   const double warm=g->OilTemperature();QVERIFY(QMetaObject::invokeMethod(g,"CoolDownTimerElapsed"));
   QVERIFY(g->OilTemperature()<warm);QVERIFY(g->OilTemperature()>=295.372);drain();
  }
  QTemporaryDir dir;const auto path=dir.filePath("M14.json");w.controller.saveLayoutToJson(path);
  for(int cycle=0;cycle<3;++cycle) {
   w.controller.removeAllWidgets();drain();w.controller.loadLayoutFromJson(path);stopClocks(&w.area);drain();
   QCOMPARE(w.area.findChild<GeneratorWeather*>()->AtmosphericPressure(),100000.0);
   QCOMPARE(w.area.findChild<OutsideEnvironmental130310*>()->AtmosphericPressure(),100000.0);
   QCOMPARE(w.area.findChild<GeneratorDateTime*>()->LocalOffset(),int16_t(345));
   QCOMPARE(w.area.findChild<GnssPositionData129029*>()->Latitude(),-42.123456);
   for(auto *g:w.area.findChildren<GeneratorTransLowerUnit*>()) QVERIFY(g->OilTemperature()>295.372);
  }
 }
 void zdaUsesSimulatedUtc() {
  DateTimeZDA w(1,"ZDA",nullptr);UserUnitPrefs prefs;prefs.timeUnitIndex=2;w.setUserPrefs(prefs);
  const QDateTime utc(QDate(2024,2,29),QTime(23,30),QTimeZone::utc());
  w.setLZD(-5);w.setLZMD(30);w.setUnixTime(utc.toSecsSinceEpoch());w.setInitialGuiValues();
  QCOMPARE(w.GPSDay(),29);QCOMPARE(w.GPSMonth(),2);QCOMPARE(w.GPSYear(),2024);QCOMPARE(w.GPSTime(),84600.0);
  QCOMPARE(editText(&w,"lineEditDaysSince1970"),QString("02/29/2024"));
  QCOMPARE(editText(&w,"lineEditSecondsSinceMidnight"),QString("23:30:00"));
  int day=29,month=2,year=2024,hours=-5,minutes=30;
  QCOMPARE(Convert::Nmea0183ZDAToDaysSince1970(day,month,year,hours,minutes),uint16_t(QDate(1970,1,1).daysTo(QDate(2024,2,29))));
  for(int format=0;format<4;++format) {
   prefs.timeUnitIndex=format;w.setUserPrefs(prefs);
   const auto input=N2kHelper::convertN2kTimeToUserFormat(45296,prefs);
   QVERIFY(QMetaObject::invokeMethod(&w,"on_lineEditSecondsSinceMidnight_textEdited",Q_ARG(QString,input)));
   QVERIFY(QMetaObject::invokeMethod(&w,"onTimerTimeout"));
   QCOMPARE(w.GPSTime(),format<2 ? 45240.0 : 45296.0);
  }
 }
 void transmissionInitialization() {
  Workspace w;w.controller.loadLayoutFromJson(qEnvironmentVariable("RAYNMEA_TEST_DEFAULT"));
  auto generators=w.area.findChildren<GeneratorTransLowerUnit*>();QCOMPARE(generators.size(),2);
  for(auto *g:generators) {
   QVERIFY2(g->OilTemperature()>273.15,"Transmission initialized to zero Kelvin");
   QCOMPARE(editText(g,"lineEditOilTemperature"),QString("72.0"));
  }
 }
 void coordinates_data() {
  QTest::addColumn<int>("format");QTest::addColumn<double>("coordinate");
  for(int f=0;f<3;++f) for(double c:{42.123456,-42.123456,0.0,89.999999})
   QTest::newRow(qPrintable(QString("%1/%2").arg(f).arg(c,0,'f',6)))<<f<<c;
 }
 void coordinates() {
  QFETCH(int,format);QFETCH(double,coordinate);
  UserUnitPrefs prefs;prefs.latLongUnitIndex=format;
  GlobalPositioningGGA gga(1,"GGA",nullptr);GnssPositionData129029 gnss(2,"GNSS",nullptr);
  LatLong129025 latlong(3,"LatLong",nullptr);RecommendedMinGnssRMC rmc(4,"RMC",nullptr);GeographicPositionGLL gll(5,"GLL",nullptr);
  for(PgnWidgetBase *w:{static_cast<PgnWidgetBase*>(&gga),static_cast<PgnWidgetBase*>(&gnss),static_cast<PgnWidgetBase*>(&latlong),static_cast<PgnWidgetBase*>(&rmc),static_cast<PgnWidgetBase*>(&gll)}) {
   w->setUserPrefs(prefs);w->setProperty("Latitude",coordinate);w->setProperty("Longitude",coordinate);
   w->setInitialGuiValues();
   QCOMPARE(editText(w,"lineEditLatitude"),N2kHelper::convertN2kLatitudeToUserUnits(coordinate,prefs));
   QCOMPARE(editText(w,"lineEditLongitude"),N2kHelper::convertN2kLongitudeToUserUnits(coordinate,prefs));
   QVERIFY(!editText(w,"lineEditLatitude").contains("60.00"));
   if(format>0) {
    QVERIFY(editText(w,"lineEditLatitude").endsWith(coordinate<0 ? "S" : "N"));
    QVERIFY(editText(w,"lineEditLongitude").endsWith(coordinate<0 ? "W" : "E"));
   }
   const double tolerance=format==2 ? 0.000009 : (format==1 ? 0.000002 : 0.0000001);
   QVERIFY(qAbs(N2kHelper::convertUserLatitudeToN2kUnits(editText(w,"lineEditLatitude"),prefs)-coordinate)<tolerance);
   QVERIFY(qAbs(N2kHelper::convertUserLongitudeToN2kUnits(editText(w,"lineEditLongitude"),prefs)-coordinate)<tolerance);
   w->setProperty("Latitude",-coordinate);
   QCOMPARE(editText(w,"lineEditLatitude"),N2kHelper::convertN2kLatitudeToUserUnits(-coordinate,prefs));
   w->setProperty("Longitude",-coordinate);
   QCOMPARE(editText(w,"lineEditLongitude"),N2kHelper::convertN2kLongitudeToUserUnits(-coordinate,prefs));
  }
 }
 void pressureConversions() {
  UserUnitPrefs prefs;
  for(int i=0;i<4;++i) {
   prefs.pressureUnitIndex=i;
   double display=N2kHelper::convertN2kPressureToUserUnits(100610,prefs);
   QVERIFY(qAbs(N2kHelper::convertUserPressureToN2kUnits(display,prefs)-100610)<0.01);
  }
  for(int i=0;i<5;++i) {
   prefs.atmosphericPressureUnitIndex=i;
   double display=N2kHelper::convertN2kAtmosphericPressureToUserUnits(100610,prefs);
   if(i==0) QCOMPARE(display,1006.1);
   if(i==3) QCOMPARE(display,100610.0);
   QVERIFY(qAbs(N2kHelper::convertUserAtmosphericPressureToN2kUnits(display,prefs)-100610)<0.1);
  }
 }
 void weatherInput() {
  GeneratorWeather w(1,"Weather");
  w.weatherFetched("synthetic","fixture",293.15,50,1006.1,0,0);
  QCOMPARE(w.AtmosphericPressure(),100610.0);
  w.dataFetched("fixture",293.15,290.0,0,0,0,1013.25,50);
  QCOMPARE(w.AtmosphericPressure(),101325.0);
 }
 void dateBoundaries_data() {
  QTest::addColumn<QDate>("date");QTest::addColumn<double>("seconds");QTest::addColumn<int>("offset");QTest::addColumn<QString>("expectedDate");QTest::addColumn<QString>("expectedTime");
  QTest::newRow("same-day-west")<<QDate(2024,3,1)<<43200.0<<-240<<"03/01/2024"<<"08:00:00";
  QTest::newRow("leap-day-west")<<QDate(2024,3,1)<<3600.0<<-240<<"02/29/2024"<<"21:00:00";
  QTest::newRow("year-east")<<QDate(2023,12,31)<<84600.0<<330<<"01/01/2024"<<"05:00:00";
  QTest::newRow("year-west")<<QDate(2024,1,1)<<0.0<<-60<<"12/31/2023"<<"23:00:00";
  QTest::newRow("leap-east")<<QDate(2024,2,28)<<84600.0<<345<<"02/29/2024"<<"05:15:00";
  QTest::newRow("last-tenth-millisecond")<<QDate(2024,2,29)<<86399.9999<<0<<"02/29/2024"<<"23:59:59";
  QTest::newRow("utc")<<QDate(2024,2,29)<<43200.0<<0<<"02/29/2024"<<"12:00:00";
 }
 void dateBoundaries() {
  QFETCH(QDate,date);QFETCH(double,seconds);QFETCH(int,offset);QFETCH(QString,expectedDate);QFETCH(QString,expectedTime);
  DateTime129033 w(1,"Date");UserUnitPrefs prefs;prefs.timeUnitIndex=2;prefs.dateUnitIndex=0;w.setUserPrefs(prefs);
  auto days=static_cast<uint16_t>(QDate(1970,1,1).daysTo(date));
  w.setLocalOffset(offset);w.setDaysSince1970(days);w.setSecondsSinceMidnight(seconds);w.setInitialGuiValues();
  QCOMPARE(editText(&w,"lineEditDaysSince1970"),expectedDate);
  QCOMPARE(editText(&w,"lineEditSecondsSinceMidnight"),expectedTime);
  QDateTime local(date,QTime(0,0),QTimeZone::utc());local=local.addSecs(seconds+offset*60);
  QCOMPARE(Convert::ToLocalDateTime(days,seconds,offset),local.toString("M/d/yyyy H:mm:ss"));
  w.setLocalOffset(0);
  QCOMPARE(editText(&w,"lineEditDaysSince1970"),date.toString("MM/dd/yyyy"));
  w.setLocalOffset(60);w.setSecondsSinceMidnight(86399.999);
  QCOMPARE(editText(&w,"lineEditDaysSince1970"),date.addDays(1).toString("MM/dd/yyyy"));
  w.setSecondsSinceMidnight(0);
  QCOMPARE(editText(&w,"lineEditDaysSince1970"),date.toString("MM/dd/yyyy"));
 }
};
int main(int argc,char **argv) {
 QApplication app(argc,argv);
 QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
 InstrumentTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "instrument_tests.moc"


