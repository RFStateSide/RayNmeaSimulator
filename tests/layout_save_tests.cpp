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
#include <QNetworkProxy>
#include <QMessageBox>
#include <QCheckBox>
#include <QComboBox>
#include "maincontroller.h"
#include "heave127252.h"
#include "generatorweather.h"
#include "generatorvesselmovement.h"
#include "autopilotsimulator.h"
namespace {
struct Workspace {
 QMdiArea area; QTableView table; QTreeView tree;
 PgnTableController tables{&table}; PgnTreeController trees{&tree};
 MainController c{&tables,&trees,&area,nullptr};
 Workspace(){c.setMode(Viewer); c.setMyVessel(MyVessel{}); c.setUserPrefs(UserUnitPrefs{}); c.setUserOptionPrefs(UserOptionPrefs{});
  for(const QString g:{"Generate","Navigation","Propulsion","Environment","Tanks","Electrical","Other","Communication"}) c.addSubWindowForGroup(g,g);}
 PgnTableModel *model(){return tables.getPgnTableModel();}
};
QByteArray read(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
void write(const QString &path,const QByteArray &bytes){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());}
QJsonObject root(const QString&p){return QJsonDocument::fromJson(read(p)).object();}
}
class LayoutSaveTests:public QObject {
 Q_OBJECT
private slots:
 void newHeaveRoundTrip(){
  Workspace w;w.c.addWidgetAsComboboxTextChanged("127252","Heave","Navigation");
  auto *h=w.area.findChild<Heave127252*>();QVERIFY(h);
  QCOMPARE(h->DelaySource(),N2kDD374_UserDefined);
  const auto p=w.model()->getPgnParams(h->Id());QCOMPARE(p[3],QList<QString>({"DelaySource","0","enum","tN2kDelaySource"}));
  h->setHeave(-1.25);h->setDelay(0.5);h->setDelaySource(N2kDD374_FactoryDefault);
  QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);
  QTemporaryDir d(QDir::currentPath()+"/build/layout-save-XXXXXX");auto file=d.filePath("new.json");QVERIFY(w.c.saveLayoutToJson(file));
  Workspace restored;QVERIFY(restored.c.loadLayoutFromJson(file));
  auto *r=restored.area.findChild<Heave127252*>();QVERIFY(r);QCOMPARE(r->Heave(),-1.25);QCOMPARE(r->Delay(),0.5);QCOMPARE(r->DelaySource(),N2kDD374_FactoryDefault);
 }
 void invalidSavePreservesFileAndReportsField(){
  Workspace w;w.c.addWidgetAsComboboxTextChanged("127252","Heave","Navigation");
  auto*h=w.area.findChild<Heave127252*>();QVERIFY(h);
  QTemporaryDir d(QDir::currentPath()+"/build/layout-save-XXXXXX");const auto file=d.filePath("replace.json");QVERIFY(w.c.saveLayoutToJson(file));const auto before=read(file);
  w.model()->changeParam(h->Id(),"DelaySource","enum");
  QSignalSpy status(&w.c,&MainController::ShowStatusBarMessage);
  const QString expected=QString("Layout save failed: Group Navigation / Heave [Heave127252, id %1]: parameter DelaySource must contain a finite numeric value").arg(h->Id());
  QTest::ignoreMessage(QtWarningMsg,qPrintable(expected));
  QVERIFY(!w.c.saveLayoutToJson(file));QCOMPARE(read(file),before);QCOMPARE(status.last()[0].toString(),expected);
  QTest::ignoreMessage(QtWarningMsg,qPrintable(expected));const auto absent=d.filePath("absent.json");QVERIFY(!w.c.saveLayoutToJson(absent));QVERIFY(!QFile::exists(absent));
  QCOMPARE(w.area.findChild<Heave127252*>(),h);QCOMPARE(w.model()->getPgnParams(h->Id())[3][1],QString("enum"));
  w.model()->changeParam(h->Id(),"DelaySource","0");QVERIFY(w.c.saveLayoutToJson(file));
  Workspace restored;QVERIFY(restored.c.loadLayoutFromJson(file));QCOMPARE(restored.area.findChild<Heave127252*>()->DelaySource(),N2kDD374_UserDefined);
 }
 void duplicateGenerator_data(){QTest::addColumn<QString>("pgn");QTest::newRow("engines")<<"-127489";QTest::newRow("transmissions")<<"-127493";}
 void duplicateGenerator(){
  QFETCH(QString,pgn);Workspace w;
  QVERIFY(w.c.loadLayoutFromJson(":/defaults/defaultLayout.json"));
  QList<QWidget*> widgets;for(auto *widget:*w.c.getListWidgetsOnSubForm("Generate"))
   if(QString(widget->metaObject()->className())==(pgn=="-127489"?"GeneratorEngine":"GeneratorTransLowerUnit")) widgets<<widget;
  QCOMPARE(widgets.size(),2);
  const auto first=widgets[0]->property("Id").toInt(),second=widgets[1]->property("Id").toInt();
  w.model()->changeParam(second,"EngineInstance",w.model()->getPgnParams(first)[0][1]);
  QSignalSpy status(&w.c,&MainController::ShowStatusBarMessage);QTemporaryDir d(QDir::currentPath()+"/build/layout-save-XXXXXX");QVERIFY(!w.c.saveLayoutToJson(d.filePath("dup.json")));
  const auto error=status.last()[0].toString();QVERIFY(error.contains("duplicate generator"));QVERIFY(error.contains("EngineInstance=0"));QVERIFY(error.contains(QString("id %1").arg(first)));QVERIFY(error.contains(QString("id %1").arg(second)));
 }
 void weatherAndMovementRoundTrip(){
  Workspace w;MyVessel vessel;vessel.EnvironmentalInfluence=true;w.c.setMyVessel(vessel);
  QVERIFY(w.c.loadLayoutFromJson(":/defaults/defaultLayout.json"));
  w.c.addWidgetAsComboboxTextChanged("127252","Heave","Navigation");
  auto*weather=w.area.findChild<GeneratorWeather*>();auto*movement=w.area.findChild<GeneratorVesselMovement*>();QVERIFY(weather);QVERIFY(movement);
  w.c.setMode(Sender);
  weather->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(2);
  WeatherObservation o;o.provider="NOAA NDBC";o.station="44098";o.location="Fixture";o.observed=QDateTime::currentDateTimeUtc().addSecs(-60);o.retrieved=QDateTime::currentDateTimeUtc();o.maximumAgeSeconds=7200;o.waveHeight=2;o.dominantWavePeriod=6;
  weather->acceptObservation(o);o.station="44030";o.waveHeight=N2kDoubleNA;o.dominantWavePeriod=N2kDoubleNA;o.windSpeed=8;weather->acceptObservation(o);
  QVERIFY(weather->fieldOrigin("WaveHeight").retained);
  auto cfg=weather->layoutConfiguration();cfg["autoRefresh"]=true;cfg["refreshMinutes"]=30;weather->restoreLayoutConfiguration(cfg);
  const auto movementConfig=movement->layoutConfiguration();QTemporaryDir d(QDir::currentPath()+"/build/layout-save-XXXXXX");auto file=d.filePath("weather.json");QVERIFY(w.c.saveLayoutToJson(file));
  Workspace restored;restored.c.setMyVessel(vessel);restored.c.setMode(Sender);QVERIFY(restored.c.loadLayoutFromJson(file));
  auto*r=restored.area.findChild<GeneratorWeather*>();QVERIFY(r);QVERIFY(r->automaticRefresh());QCOMPARE(r->refreshIntervalMinutes(),30);QCOMPARE(r->WaveHeight(),2.);QCOMPARE(r->fieldOrigin("WaveHeight").station,QString("44098"));QVERIFY(r->fieldOrigin("WaveHeight").retained);QCOMPARE(r->fieldOrigin("WaveHeight").observed.toSecsSinceEpoch(),o.observed.toSecsSinceEpoch());
  QCOMPARE(restored.area.findChild<GeneratorVesselMovement*>()->layoutConfiguration(),movementConfig);
  QVERIFY(restored.c.saveLayoutToJson(d.filePath("second.json")));
 }
 void bundledDefaultCompatibility(){Workspace w;QVERIFY(w.c.loadLayoutFromJson(":/defaults/defaultLayout.json"));QTemporaryDir d(QDir::currentPath()+"/build/layout-save-XXXXXX");QVERIFY(w.c.saveLayoutToJson(d.filePath("default-copy.json")));Workspace r;QVERIFY(r.c.loadLayoutFromJson(d.filePath("default-copy.json")));QCOMPARE(r.model()->getPgnList().size(),w.model()->getPgnList().size());}
 void existingAutopilotRoute(){AutoPilotSimulator a;QVERIFY(a.loadRoute(QFINDTESTDATA("../routes/gpx/test_route1.gpx")));}
 void capturedSessionQualification(){
  const auto path=qEnvironmentVariable("M219_CAPTURE");if(path.isEmpty())QSKIP("Optional private live-session recovery qualification");
  auto document=root(path);auto groups=document["layouts"].toArray();int corrected=0;
  for(int i=0;i<groups.size();++i){auto g=groups[i].toObject();auto widgets=g["widgets"].toArray();for(int j=0;j<widgets.size();++j){auto item=widgets[j].toObject();if(item["type"]=="Heave127252"){auto p=item["parameters"].toArray();for(int k=0;k<p.size();++k){auto row=p[k].toArray();if(row[0]=="DelaySource"&&row[1]=="enum"){row[1]="0";row[2]="enum";p[k]=row;++corrected;}}item["parameters"]=p;widgets[j]=item;}}g["widgets"]=widgets;groups[i]=g;}document["layouts"]=groups;QCOMPARE(corrected,1);
  QTemporaryDir d(QDir::currentPath()+"/build/layout-save-XXXXXX");auto file=d.filePath("recovery-copy.json");write(file,QJsonDocument(document).toJson());Workspace w;w.c.setMode(Sender);QVERIFY(w.c.loadLayoutFromJson(file));QCOMPARE(w.model()->getPgnList().size(),36);QVERIFY(w.c.saveLayoutToJson(d.filePath("roundtrip.json")));Workspace r;r.c.setMode(Sender);QVERIFY(r.c.loadLayoutFromJson(d.filePath("roundtrip.json")));QCOMPARE(r.model()->getPgnList().size(),36);
  write(QFileInfo(path).absolutePath()+"/capture-roundtrip.json",read(d.filePath("roundtrip.json")));
  QMap<int,QJsonObject> expected,actual;
  auto inventory=[](const QJsonObject &value,QMap<int,QJsonObject> &out){for(const auto &group:value["layouts"].toArray())for(const auto &widget:group.toObject()["widgets"].toArray()){const auto item=widget.toObject();out[item["idX"].toInt()]=item;}};
  inventory(document,expected);inventory(root(d.filePath("roundtrip.json")),actual);QCOMPARE(expected.keys(),actual.keys());
  for(auto it=expected.begin();it!=expected.end();++it)for(const QString key:{"type","pgn","parameters","ranges","rowSpan","columnSpan","enabled","configuration","engineProfile","electricalSimulation"})
   QVERIFY2(it.value()[key]==actual[it.key()][key],qPrintable(QString("Recovered widget id %1 field %2").arg(it.key()).arg(key)));
  const auto recovery=qEnvironmentVariable("M219_VALIDATED_RECOVERY");if(!recovery.isEmpty())write(recovery,QJsonDocument(document).toJson());
 }
};
int main(int argc,char**argv){QTemporaryDir profile(QDir::currentPath()+"/build/layout-save-profile-XXXXXX"); qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8()); QApplication app(argc,argv);QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));LayoutSaveTests t;return QTest::qExec(&t,argc,argv);}
#include "layout_save_tests.moc"
