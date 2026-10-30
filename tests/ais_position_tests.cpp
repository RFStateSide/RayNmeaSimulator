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
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include "clickablelabel.h"
#include <QMessageBox>
#include <QNetworkProxy>
#include "mainwindow.h"
#include "aisclassbposition129039.h"
#include "dialogsetup.h"
#include "preferences.h"
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QTabWidget>
#include "appsettings.h"
#include "aisclassbstatic129809.h"
#include "aisclassbstatic129810.h"
#include "enumcomboboxdelegate.h"
#include "pgntreecontroller.h"
#include "nmeasenderworker.h"
#include "N2kMessages.h"
namespace {
QMap<QString,QString> fields() {
 QMap<QString,QString> f;for(const auto &row:AisClassBPosition129039::parameters())f[row[0]]=row[1];
 f["UserID"]="123456789";f["Repeat"]="2";f["Latitude"]="42.25";f["Longitude"]="-70.125";f["Accuracy"]="true";f["RAIM"]="true";
 f["Seconds"]="45";f["COG"]="1.23456";f["SOG"]="5.675";f["AISInfo"]="1";f["Heading"]="2.34567";f["Display"]="true";f["Band"]="true";f["Msg22"]="true";f["Mode"]="1";f["SID"]="7";return f;
}
QStringList wireFields(const QMap<QString,QString> &f) {QStringList out;for(auto i=f.begin();i!=f.end();++i)out<<i.key()+":"+i.value();return out;}
void confirm() {QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto *b=qobject_cast<QMessageBox *>(w))b->button(QMessageBox::Yes)->click();});}
QByteArray read(const QString &p){QFile f(p);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
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
struct Workspace {
    MainWindow window;
    MainController *controller=window.findChild<MainController *>();
    PgnTableController *table=window.findChild<PgnTableController *>();
    PgnTableModel *model=table->getPgnTableModel();
    Workspace(){window.resize(1500,1050);window.show();}
    AisClassBStatic129810 *addB(uint32_t id=123456789){controller->addWidgetAsComboboxTextChanged("129810","AIS Class B Static Part B","Communication");auto *b=qobject_cast<AisClassBStatic129810 *>(controller->layoutWidgets("Communication").last());b->setUserID(id);return b;}
    AisClassBStatic129809 *addA(uint32_t id=123456789){controller->addWidgetAsComboboxTextChanged("129809","AIS Class B Static Part A","Communication");auto *a=qobject_cast<AisClassBStatic129809 *>(controller->layoutWidgets("Communication").last());a->setUserID(id);return a;}
    AisClassBPosition129039 *addP(uint32_t id=123456789){controller->addWidgetAsComboboxTextChanged("129039","AIS Class B Position","Communication");auto *p=qobject_cast<AisClassBPosition129039*>(controller->layoutWidgets("Communication").last());p->setUserID(id);return p;}
    void select(QWidget *widget){auto *tree=window.findChild<PgnTreeController *>();QTRY_VERIFY(tree->getTreeModel()->indexFor(widget).isValid());QVERIFY(QMetaObject::invokeMethod(tree,"navigate",Qt::DirectConnection,Q_ARG(QModelIndex,tree->getTreeModel()->indexFor(widget))));}
};
}

class AisPositionTests:public QObject {
 Q_OBJECT
private slots:
 void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");}
 void independentBytes() {
  auto f=fields();tN2kMsg m;QVERIFY(AisClassBPosition129039::encode(f,m));QCOMPARE(m.PGN,129039UL);QCOMPARE(m.Priority,4);QCOMPARE(m.DataLen,27);
  const auto expected=QByteArray::fromHex("9215cd5b0730c633d6a0d62e19b73a303802ffff0fa15bffecff07");QCOMPARE(QByteArray(reinterpret_cast<const char*>(m.Data),m.DataLen),expected);
  uint8_t id,seconds,sid;uint32_t user;double lat,lon,cog,sog,heading;bool accuracy,raim,display,dsc,band,msg22,state;tN2kAISRepeat repeat;tN2kAISTransceiverInformation info;tN2kAISUnit unit;tN2kAISMode mode;
  QVERIFY(ParseN2kPGN129039(m,id,repeat,user,lat,lon,accuracy,raim,seconds,cog,sog,info,heading,unit,display,dsc,band,msg22,mode,state,sid));
  QCOMPARE(id,18);QCOMPARE(user,123456789u);QCOMPARE(lat,42.25);QCOMPARE(lon,-70.125);QVERIFY(qAbs(cog-1.2346)<1e-12);QCOMPARE(sog,5.68);QVERIFY(qAbs(heading-2.3457)<1e-12);QVERIFY(accuracy&&raim&&display&&band&&msg22&&state);QVERIFY(!dsc);QCOMPARE(int(unit),1);QCOMPARE(int(mode),1);QCOMPARE(sid,7);
  QStringList out;QVERIFY(AisClassBPosition129039::decode(m,out));QCOMPARE(out[5],QString("123456789"));QCOMPARE(f["COG"],QString("1.23456"));
 }
 void invalidFields_data() {
  QTest::addColumn<QString>("field");QTest::addColumn<QString>("value");const QList<QPair<QString,QString>> cases{{"MessageID","24"},{"Repeat","4"},{"UserID","0123456789"},{"UserID","12"},{"UserID","1000000000"},{"Latitude","90.0001"},{"Latitude","-90.0001"},{"Longitude","180.01"},{"Longitude","-180.01"},{"Latitude","nan"},{"SOG","inf"},{"SOG","-1"},{"SOG","655.34"},{"COG","-1"},{"Heading","6.3"},{"Seconds","64"},{"Accuracy","2"},{"Unit","2"},{"Mode","2"},{"AISInfo","5"},{"SID","256"}};
  int i=0;for(const auto &v:cases)QTest::newRow(qPrintable(QString::number(i++)))<<v.first<<v.second;
 }
 void invalidFields(){QFETCH(QString,field);QFETCH(QString,value);auto f=fields();f[field]=value;tN2kMsg m;QVERIFY(!AisClassBPosition129039::validField(field,value));QVERIFY(!AisClassBPosition129039::encode(f,m));AisClassBPosition129039 p(1,"Position");const auto old=p.value(field);p.setValue(field,value);QCOMPARE(p.value(field),old);}
 void unavailableZeroAndBoundaries() {
  auto f=fields();tN2kMsg m;QStringList out;
  for(const auto &row:AisClassBPosition129039::parameters())if(AisClassBPosition129039::measurement(row[0]))f[row[0]]=QString::number(N2kDoubleNA,'g',17);
  f["AISInfo"]="31";f["Seconds"]="60";QVERIFY(AisClassBPosition129039::encode(f,m));QCOMPARE(m.Data[8],0x7f);QCOMPARE(m.Data[12],0x7f);QCOMPARE(m.Data[14],0xff);QCOMPARE(m.Data[16],0xff);QCOMPARE(m.Data[21],0xff);QCOMPARE(m.Data[20],0xff);QVERIFY(AisClassBPosition129039::decode(m,out));
  for(int i=0;i<out.size();i+=2)if(AisClassBPosition129039::measurement(out[i]))QCOMPARE(out[i+1].toDouble(),N2kDoubleNA);
  for(const auto &row:AisClassBPosition129039::parameters())if(AisClassBPosition129039::measurement(row[0]))f[row[0]]="0";
  for(int seconds:{0,59,60,61,62,63}){f["Seconds"]=QString::number(seconds);QVERIFY(AisClassBPosition129039::encode(f,m));QVERIFY(AisClassBPosition129039::decode(m,out));QCOMPARE(out[7].toDouble(),0.);QCOMPARE(out[9].toDouble(),0.);QCOMPARE(out[15].toInt(),seconds);}
  f["Latitude"]="-90";f["Longitude"]="180";f["SOG"]="655.33";f["Heading"]=QString::number(2*M_PI,'g',17);QVERIFY(AisClassBPosition129039::encode(f,m));QVERIFY(AisClassBPosition129039::decode(m,out));QCOMPARE(out[7].toDouble(),-90.);QCOMPARE(out[9].toDouble(),180.);QVERIFY(qAbs(out[23].toDouble()-6.2832)<1e-12);
  for(const auto &identity:{QString("0"),QString("4294967295")}){f["UserID"]=identity;QVERIFY(!AisClassBPosition129039::encode(f,m));}
 }
 void malformedAndIgnoredRegionalFields() {
  tN2kMsg m;QVERIFY(AisClassBPosition129039::encode(fields(),m));const auto original=m;QStringList out{"unchanged"};
  for(int len:{0,1,13,20,24,25,26,28}){m=original;m.DataLen=len;QVERIFY(!AisClassBPosition129039::decode(m,out));QCOMPARE(out,QStringList{"unchanged"});}
  m=original;m.PGN=129038;QVERIFY(!AisClassBPosition129039::decode(m,out));m=original;m.Data[0]=19;QVERIFY(!AisClassBPosition129039::decode(m,out));
  m=original;m.Data[20]=(5<<3)|7;QVERIFY(!AisClassBPosition129039::decode(m,out));m=original;m.Data[25]=1;QVERIFY(!AisClassBPosition129039::decode(m,out));
  m=original;m.Data[16]=0xfe;m.Data[17]=0xff;QVERIFY(!AisClassBPosition129039::decode(m,out));
  m=original;m.Data[5]=m.Data[6]=m.Data[7]=0xff;m.Data[8]=0x7e;QVERIFY(!AisClassBPosition129039::decode(m,out));
  // Genuine regional and communication-state bits are discarded by the library,
  // not reserved fields to reject merely because the encoder defaults differ.
  m=original;m.Data[18]=0x12;m.Data[19]=0x34;m.Data[20]&=0xf8;m.Data[23]=0;m.Data[24]|=3;QVERIFY(AisClassBPosition129039::decode(m,out));
 }
 void modelTreePreferencesAndPendingEdit() {
  Workspace w;auto *p=w.addP(0);QVERIFY(!p->isEnabled());AppSettings settings(APP_COMPANY,APP_NAME);settings.beginGroup("Vessel");QCOMPARE(p->Latitude(),readVesselSettings(settings).Latitude);QCOMPARE(p->SOG(),N2kDoubleNA);QVERIFY(!w.model->findPgn(p->Id())->enabled);
  w.model->setCurrentPgn(p->Id());QVERIFY(w.model->setData(w.model->index(3,1),"42.123456789"));QCOMPARE(p->Latitude(),42.123456789);QVERIFY(!w.model->setData(w.model->index(3,1),"91"));
  p->setLongitude(-70.987654321);p->setCOG(1.23456789);p->setSOG(5.675);p->setHeading(2.3);w.select(p);
  auto *tree=w.window.findChild<PgnTreeController*>()->getTreeModel();QTRY_VERIFY(tree->indexFor(p,"SOG").isValid());auto prefs=p->UserPrefs();prefs.latLongUnitIndex=0;prefs.speedUnitIndex=3;p->setUserPrefs(prefs);
  QVERIFY(tree->indexFor(p,"SOG").siblingAtColumn(1).data().toString().contains("knots"));QVERIFY(tree->indexFor(p,"COG").siblingAtColumn(1).data().toString().contains("true"));
  const auto stamp=w.model->parameterUpdatedAt(p->Id(),"Latitude");QVERIFY(stamp.isValid());
  auto *latitude=p->findChild<QLineEdit*>("Latitude");for(int i=0;i<9;++i){prefs.latLongUnitIndex=i%3;p->setUserPrefs(prefs);latitude->setModified(true);QVERIFY(QMetaObject::invokeMethod(latitude,"editingFinished",Qt::DirectConnection));QCOMPARE(p->Latitude(),42.123456789);}
  prefs.latLongUnitIndex=0;prefs.speedUnitIndex=3;p->setUserPrefs(prefs);auto *speed=p->findChild<QLineEdit*>("SOG");speed->setFocus();QTRY_VERIFY(speed->hasFocus());speed->selectAll();QTest::keyClicks(speed,"12.345");prefs.speedUnitIndex=0;p->setUserPrefs(prefs);p->setSID(9);QCOMPARE(speed->text(),QString("12.345"));
  QVERIFY(QMetaObject::invokeMethod(speed,"editingFinished",Qt::DirectConnection));QCOMPARE(p->SOG(),12.345*1852.0/3600.0);
  latitude->setText("-12.3456789123");latitude->setModified(true);QVERIFY(QMetaObject::invokeMethod(latitude,"editingFinished",Qt::DirectConnection));QCOMPARE(p->Latitude(),-12.3456789123);
  auto *heading=p->findChild<QLineEdit*>("Heading");heading->setText("123.456789");heading->setModified(true);QVERIFY(QMetaObject::invokeMethod(heading,"editingFinished",Qt::DirectConnection));QVERIFY(qAbs(p->Heading()-123.456789*M_PI/180.)<1e-12);
  QVERIFY(w.model->setData(w.model->index(3,1),"Unavailable"));QCOMPARE(p->Latitude(),N2kDoubleNA);QTRY_COMPARE(tree->indexFor(p,"Latitude").siblingAtColumn(1).data().toString(),QString("Unavailable"));
  EnumComboBoxDelegate delegate;QStyleOptionViewItem option;auto *editor=delegate.createEditor(&w.window,option,w.model->index(12,1));auto *combo=qobject_cast<QComboBox*>(editor);QVERIFY(combo);combo->setCurrentIndex(0);delegate.setModelData(combo,w.model,w.model->index(12,1));QCOMPARE(p->Unit(),0);delete editor;
 }

 void startingPositionUsesNavigationDefaults() {
  AppSettings settings(APP_COMPANY,APP_NAME);
  settings.setValue("Vessel/Latitude",-12.3456789123);settings.setValue("Vessel/Longitude",123.456789123);
  Workspace w;auto *p=w.addP();QCOMPARE(p->Latitude(),-12.3456789123);QCOMPARE(p->Longitude(),123.456789123);
  QVERIFY(!w.model->parameterUpdatedAt(p->Id(),"Latitude").isValid());QVERIFY(w.controller->layoutWidgets("Generate").isEmpty());
  QTimer::singleShot(0,[]{for(auto *widget:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox*>(widget))box->button(QMessageBox::No)->click();});
  w.controller->addWidgetAsComboboxTextChanged("129025","Lat Long","Navigation");
  auto *navigation=w.controller->layoutWidgets("Navigation").last();
  // Existing Navigation creation formats six decimals; AIS retains all configured precision.
  QVERIFY(qAbs(navigation->property("Latitude").toDouble()-p->Latitude())<=0.0000005);
  QVERIFY(qAbs(navigation->property("Longitude").toDouble()-p->Longitude())<=0.0000005);
  p->setLatitude(1.234567891);auto *other=w.addP(987654321);QCOMPARE(other->Latitude(),-12.3456789123);QCOMPARE(p->Latitude(),1.234567891);
  p->setSOG(12.3);QTest::qWait(50);QCOMPARE(p->Latitude(),1.234567891);
  QVERIFY(!p->isEnabled());QVERIFY(!other->isEnabled());QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
  AisClassBPosition129039 uninitialized(1,"No initial coordinates");QCOMPARE(uninitialized.Latitude(),N2kDoubleNA);QCOMPARE(uninitialized.Longitude(),N2kDoubleNA);
  uninitialized.setLatitude(91.);uninitialized.setLongitude(-181.);QCOMPARE(uninitialized.Latitude(),N2kDoubleNA);QCOMPARE(uninitialized.Longitude(),N2kDoubleNA);
  w.controller->setMode(Viewer);auto *viewer=w.addP(111222333);QCOMPARE(viewer->Latitude(),N2kDoubleNA);QCOMPARE(viewer->Longitude(),N2kDoubleNA);
  w.controller->setMode(Sender);QCOMPARE(viewer->Latitude(),N2kDoubleNA);QCOMPARE(p->Latitude(),1.234567891);
 }
 void separateUnitsAndLeftClickMenus() {
  AisClassBPosition129039 p(1,"Position");AisClassBStatic129810 b(2,"Part B");AisClassBStatic129809 a(3,"Part A");
  p.resize(650,700);b.resize(650,850);a.resize(650,500);p.show();QVERIFY(QTest::qWaitForWindowExposed(&p));
  auto prefs=p.UserPrefs();prefs.latLongUnitIndex=0;prefs.speedUnitIndex=3;prefs.headingReferenceIndex=1;p.setUserPrefs(prefs);
  p.setLatitude(42.123456789123);p.setLongitude(-70.123456789123);p.setSOG(3.123456789);p.setCOG(1.23456789);p.setHeading(2.34567891);
  b.setLength(35.*0.3048);b.setBeam(12.*0.3048);
  auto clickUnit=[](ClickableLabel *label,int index) {
   bool selected=false;
   QTimer::singleShot(0,[index,&selected]{auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(menu&&index<menu->actions().size()){selected=true;QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions()[index]).center());}});
   QTest::mouseClick(label,Qt::LeftButton);QVERIFY(selected);
  };
  for(PgnWidgetBase *widget:QList<PgnWidgetBase*>{&a,&b,&p}) {
   const auto rows=widget==&a?AisClassBStatic129809::parameters():widget==&b?AisClassBStatic129810::parameters():AisClassBPosition129039::parameters();
   for(const auto &row:rows){auto *label=widget->findChild<QLabel*>("label"+row[0]);auto *units=widget->findChild<QLabel*>("labelUnit"+row[0]);QVERIFY(label);QVERIFY(units);QVERIFY(widget->findChild<QWidget*>(row[0]));}
  }
  auto *latitude=p.findChild<QLineEdit*>("Latitude");auto *latUnits=p.findChild<ClickableLabel*>("labelUnitLatitude");auto *speedUnits=p.findChild<ClickableLabel*>("labelUnitSOG");QVERIFY(latUnits&&speedUnits);
  for(int i=0;i<9;++i){clickUnit(latUnits,i%3);QCOMPARE(p.UserPrefs().latLongUnitIndex,uint(i%3));QCOMPARE(p.Latitude(),42.123456789123);QCOMPARE(p.Longitude(),-70.123456789123);QVERIFY(!latUnits->text().isEmpty());latitude->setModified(true);QVERIFY(QMetaObject::invokeMethod(latitude,"editingFinished"));QCOMPARE(p.Latitude(),42.123456789123);}
  clickUnit(speedUnits,0);QCOMPARE(p.UserPrefs().speedUnitIndex,0u);QCOMPARE(speedUnits->text(),QString("m/s"));QCOMPARE(p.SOG(),3.123456789);
  QCOMPARE(p.findChild<QLabel*>("labelUnitCOG")->text(),QString::fromUtf8("°T"));QCOMPARE(p.findChild<QLabel*>("labelUnitHeading")->text(),QString::fromUtf8("°T"));QCOMPARE(p.COG(),1.23456789);QCOMPARE(p.Heading(),2.34567891);
  // Pending coordinate input keeps the original format when another preference refresh arrives.
  clickUnit(latUnits,2);p.activateWindow();QApplication::setActiveWindow(&p);latitude->setFocus();QTRY_VERIFY(latitude->hasFocus());latitude->setText(QString::fromUtf8("43° 15.123456' N"));latitude->setModified(true);QVERIFY(QMetaObject::invokeMethod(latitude,"textEdited",Q_ARG(QString,latitude->text())));
  prefs=p.UserPrefs();prefs.latLongUnitIndex=0;p.setUserPrefs(prefs);QCOMPARE(latUnits->text(),QString::fromUtf8("DD°MM.MMM’"));
  QVERIFY(QMetaObject::invokeMethod(latitude,"editingFinished"));QVERIFY(qAbs(p.Latitude()-(43.+15.123456/60.))<1e-10);
  clickUnit(latUnits,1);latitude->setText(QString::fromUtf8("44° 12' 30.123456\" N"));latitude->setModified(true);QVERIFY(QMetaObject::invokeMethod(latitude,"textEdited",Q_ARG(QString,latitude->text())));QVERIFY(QMetaObject::invokeMethod(latitude,"editingFinished"));QVERIFY(qAbs(p.Latitude()-(44.+12./60.+30.123456/3600.))<1e-10);
  clickUnit(latUnits,0);
  latitude->setText("91");latitude->setModified(true);const double good=p.Latitude();QVERIFY(QMetaObject::invokeMethod(latitude,"editingFinished"));QCOMPARE(p.Latitude(),good);
  b.show();b.activateWindow();QApplication::setActiveWindow(&b);QVERIFY(QTest::qWaitForWindowExposed(&b));auto *lengthUnits=b.findChild<ClickableLabel*>("labelUnitLength");QVERIFY(lengthUnits);clickUnit(lengthUnits,1);QCOMPARE(b.UserPrefs().distanceUnitIndex,1u);QCOMPARE(lengthUnits->text(),QString("ft"));QCOMPARE(b.Length(),35.*0.3048);
  QCOMPARE(b.findChild<QLabel*>("labelLength")->text(),QString("Length"));
  auto *length=b.findChild<QLineEdit*>("Length");length->setFocus();QTRY_VERIFY(length->hasFocus());length->selectAll();QTest::keyClicks(length,"40");auto distance=b.UserPrefs();distance.distanceUnitIndex=0;b.setUserPrefs(distance);QCOMPARE(lengthUnits->text(),QString("ft"));QVERIFY(QMetaObject::invokeMethod(length,"editingFinished"));QCOMPARE(b.Length(),40.*0.3048);QCOMPARE(lengthUnits->text(),QString("m"));
  const auto dir=qEnvironmentVariable("M229D_EVIDENCE");
  for(int points:{9,12,15}){auto font=p.font();font.setPointSize(points);p.setFont(font);p.adjustSize();QTest::qWait(10);for(auto *units:p.findChildren<QLabel*>())if(units->isVisible()&&units->objectName().startsWith("labelUnit"))QVERIFY(units->width()>=units->fontMetrics().horizontalAdvance(units->text()));}
  if(!dir.isEmpty()){QVERIFY(p.grab().save(dir+"/ais-unit-rows.png"));QVERIFY(b.grab().save(dir+"/ais-dimension-rows.png"));}
 }
 void setupMmsiDefaultsPersistenceAndIndependentLayouts() {
  Workspace w;auto *d=w.window.findChild<DialogSetup*>();d->show();QTest::qWait(20);QVERIFY(!d->isModal());auto *m=d->findChild<QLineEdit*>("vesselMmsi");QVERIFY(m);QCOMPARE(m->text(),QString("0"));QSignalSpy saved(d,&DialogSetup::ReloadSettings);
  for(const auto &bad:{QString("0123456789"),QString("123"),QString("1000000000"),QString(" 123456789"),QString("abc")}){m->setText(bad);QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));QCOMPARE(saved.count(),0);QVERIFY(d->findChild<QLabel*>("credentialStorageStatus")->text().contains("MMSI"));}
  m->setText("123456789");QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));QCOMPARE(saved.count(),1);d->reject();
  auto create=[&w](const QString &pgn){w.controller->addWidgetAsComboboxTextChanged(pgn,"SIMULATED AIS","Communication");return w.controller->layoutWidgets("Communication").last();};
  auto *a=qobject_cast<AisClassBStatic129809*>(create("129809"));auto *b=qobject_cast<AisClassBStatic129810*>(create("129810"));auto *p=qobject_cast<AisClassBPosition129039*>(create("129039"));QVERIFY(a&&b&&p);
  QCOMPARE(a->UserID(),123456789u);QCOMPARE(b->UserID(),123456789u);QCOMPARE(p->UserID(),123456789u);
  for(QWidget *widget:QList<QWidget*>{a,b,p})QVERIFY(widget->findChild<QLabel*>("aisAssociation")->text().contains("Matching"));
  QVERIFY(a->findChild<QLabel*>("aisAssociation")->text().contains("Position"));QVERIFY(b->findChild<QLabel*>("aisAssociation")->text().contains("Position"));QVERIFY(p->findChild<QLabel*>("aisAssociation")->text().contains("Part A"));QVERIFY(p->findChild<QLabel*>("aisAssociation")->text().contains("Part B"));
  p->setLatitude(42.123456789);p->setLongitude(-70.12);p->setSOG(1.23456789);
  d->show();m->setText("987654321");d->reject();d->show();QCOMPARE(m->text(),QString("123456789"));m->setText("987654321");QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));d->reject();
  QCOMPARE(a->UserID(),123456789u);QCOMPARE(b->UserID(),123456789u);QCOMPARE(p->UserID(),123456789u);auto *other=qobject_cast<AisClassBPosition129039*>(create("129039"));QCOMPARE(other->UserID(),987654321u);other->setLatitude(-12.3456789123);
  QTemporaryDir dir;const auto path=dir.filePath("ais.json");QVERIFY(w.controller->saveLayoutToJson(path));const auto original=read(path);
  for(int i=0;i<4;++i){confirm();QVERIFY(w.controller->loadLayoutFromJson(path));for(auto *loaded:w.window.findChildren<AisClassBPosition129039*>())if(loaded->UserID()==123456789u){QCOMPARE(loaded->Latitude(),42.123456789);QCOMPARE(loaded->SOG(),1.23456789);}else QCOMPARE(loaded->Latitude(),-12.3456789123);QVERIFY(w.controller->saveLayoutToJson(path));}
  QPointer<AisClassBPosition129039> held=w.window.findChildren<AisClassBPosition129039*>().first();
  auto doc=QJsonDocument::fromJson(original).object();auto groups=doc["layouts"].toArray();for(int g=0;g<groups.size();++g){auto group=groups[g].toObject();auto widgets=group["widgets"].toArray();for(int n=0;n<widgets.size();++n){auto widget=widgets[n].toObject();if(widget["pgn"]!="129039")continue;auto rows=widget["parameters"].toArray();auto row=rows[3].toArray();row[1]="91";rows[3]=row;widget["parameters"]=rows;widgets[n]=widget;}group["widgets"]=widgets;groups[g]=group;}doc["layouts"]=groups;QFile invalid(dir.filePath("invalid.json"));QVERIFY(invalid.open(QIODevice::WriteOnly));invalid.write(QJsonDocument(doc).toJson());invalid.close();QVERIFY(!w.controller->loadLayoutFromJson(invalid.fileName()));QVERIFY(held);
  w.controller->setMode(Viewer);w.controller->setMode(Sender);QVERIFY(held);QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
 void layoutCompositions_data(){QTest::addColumn<bool>("partA");QTest::addColumn<bool>("partB");QTest::newRow("position-only")<<false<<false;QTest::newRow("A-position")<<true<<false;QTest::newRow("B-position")<<false<<true;QTest::newRow("A-B-position")<<true<<true;}
 void layoutCompositions(){
  QFETCH(bool,partA);QFETCH(bool,partB);Workspace w;if(partA)w.addA();if(partB)w.addB();auto *p=w.addP();p->setLatitude(1.234567891);p->setLongitude(-12.345678912);p->setHeading(1.23456789);p->setEnabled(true);w.model->setSendInterval(p->Id(),1750);QTemporaryDir dir;const auto path=dir.filePath("composition.json");QVERIFY(w.controller->saveLayoutToJson(path));confirm();QVERIFY(w.controller->loadLayoutFromJson(path));
  QCOMPARE(w.window.findChildren<AisClassBStatic129809*>().size(),partA?1:0);QCOMPARE(w.window.findChildren<AisClassBStatic129810*>().size(),partB?1:0);QCOMPARE(w.window.findChildren<AisClassBPosition129039*>().size(),1);p=w.window.findChildren<AisClassBPosition129039*>().first();QCOMPARE(p->UserID(),123456789u);QCOMPARE(p->Latitude(),1.234567891);QCOMPARE(p->Longitude(),-12.345678912);QCOMPARE(p->Heading(),1.23456789);QVERIFY(p->isEnabled());QCOMPARE(w.model->findPgn(p->Id())->sendInterval,1750);QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
 void malformedStoredMmsiFallsBackWithoutRewrite(){
  AppSettings settings(APP_COMPANY,APP_NAME);for(const auto &raw:{QString("0123456789"),QString("+123456789"),QString(" 123456789"),QString("12")}){settings.setValue("Vessel/MMSI",raw);settings.beginGroup("Vessel");QCOMPARE(readVesselSettings(settings).MMSI,0u);settings.endGroup();QCOMPARE(settings.value("Vessel/MMSI").toString(),raw);}
 }
 void receiveIdentitiesSourceIsolationAndCapacity() {
  AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Vessel/MMSI",987654321u);Workspace w;w.controller->setMode(Viewer);auto *handler=w.window.findChild<Nmea2000Handler*>();handler->setMode(Viewer);
  auto receive=[&](uint32_t identity,int source,double latitude){auto f=fields();f["UserID"]=QString::number(identity);f["Latitude"]=QString::number(latitude,'g',17);tN2kMsg msg;QVERIFY(AisClassBPosition129039::encode(f,msg));msg.Source=source;handler->nmea2000MessageReceived(msg);};
  receive(123456789,23,42.1);receive(111222333,23,43.2);receive(123456789,24,44.3);receive(123456789,23,45.4);QCOMPARE(w.window.findChildren<AisClassBPosition129039*>().size(),3);
  for(auto *p:w.window.findChildren<AisClassBPosition129039*>()){QVERIFY(p->UserID()!=987654321u);if(p->UserID()==123456789u && w.model->findPgn(p->Id())->source=="23")QVERIFY(qAbs(p->Latitude()-45.4)<1e-8);if(w.model->findPgn(p->Id())->source=="24")QVERIFY(qAbs(p->Latitude()-44.3)<1e-8);QVERIFY(w.model->parameterUpdatedAt(p->Id(),"Latitude").isValid());}
  for(auto &data:w.model->getPgnList())if(data.pgn=="129039")data.paramDetails.swapItemsAt(0,2);receive(123456789,23,41.5);QCOMPARE(w.window.findChildren<AisClassBPosition129039*>().size(),3);
  for(int i=0;i<61;++i)receive(200000000u+i,23,1.);QCOMPARE(w.window.findChildren<AisClassBPosition129039*>().size(),64);receive(300000000,23,2.);QCOMPARE(w.window.findChildren<AisClassBPosition129039*>().size(),64);receive(123456789,23,40.);for(auto *p:w.window.findChildren<AisClassBPosition129039*>())if(p->UserID()==123456789u && w.model->findPgn(p->Id())->source=="23")QCOMPARE(p->Latitude(),40.);
  w.controller->setMode(Sender);handler->setMode(Sender);receive(400000000,23,3.);QCOMPARE(w.window.findChildren<AisClassBPosition129039*>().size(),64);
 }
 void authorizedMemoryOutputAndNativeBoundary() {
  Workspace w;auto *p=w.addP();const auto f=fields();for(auto i=f.begin();i!=f.end();++i)p->setValue(i.key(),i.value());
  MemoryCan backend;Nmea2000Handler handler(nullptr,nullptr,&backend);handler.SetInterfaceType("SocketCan");handler.InitializeSocketCan({"testcan",42});QTRY_VERIFY(handler.IsCanInterfaceOpen());NmeaSenderWorker sender;sender.setPgnTableController(w.table);connect(&sender,&NmeaSenderWorker::sendPgn,&handler,&Nmea2000Handler::sendPgn);
  sender.sendNmeaMessage();QVERIFY(backend.sent.isEmpty());sender.startSending();sender.sendNmeaMessage();QVERIFY(backend.sent.isEmpty());p->setEnabled(true);sender.sendNmeaMessage();QCOMPARE(backend.sent.size(),1);QCOMPARE(backend.sent.first().PGN,129039UL);sender.stopSending();sender.sendNmeaMessage();QCOMPARE(backend.sent.size(),1);sender.setMode(Viewer);sender.startSending();QVERIFY(!sender.isSending());handler.setMode(Viewer);QVERIFY(!handler.sendPgn(129039,wireFields(f)));handler.setMode(Sender);handler.SetInterfaceType("SignalK");QVERIFY(!handler.sendPgn(129039,wireFields(f)));QVERIFY(!handler.IsSignalKActive());
 }
 void renderThemes() {
  Workspace w;auto *a=w.addA();auto *b=w.addB();auto *p=w.addP();p->setLatitude(42.123456789);p->setLongitude(-70.123456789);p->setSOG(3.2);p->setCOG(1.5);p->setHeading(2.1);w.select(p);
  for(const QString theme:{"Dark","Light"}){AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.sync();w.window.HotReloadSettings();w.select(p);QTest::qWait(50);QCOMPARE(p->ColumnSpan(),3);QVERIFY(!p->isEnabled());for(auto *edit:p->findChildren<QLineEdit*>())if(edit->isVisible())QVERIFY(p->rect().contains(QRect(edit->mapTo(p,QPoint()),edit->size())));const auto dir=qEnvironmentVariable("M229D_EVIDENCE");if(!dir.isEmpty()){QDir().mkpath(dir);w.select(a);QTest::qWait(20);QVERIFY(w.window.grab().save(dir+"/"+theme+"-ais-static.png"));w.select(p);QTest::qWait(20);QVERIFY(w.window.grab().save(dir+"/"+theme+"-ais-position.png"));auto *tabs=p->findChild<QTabWidget*>();tabs->setCurrentIndex(1);QTest::qWait(20);QVERIFY(w.window.grab().save(dir+"/"+theme+"-ais-flags.png"));tabs->setCurrentIndex(0);auto *d=w.window.findChild<DialogSetup*>();d->show();QTest::qWait(20);QVERIFY(d->grab().save(dir+"/"+theme+"-mmsi-setup.png"));d->reject();}QVERIFY(a&&b);}
 }
};
int main(int argc,char **argv){QTemporaryDir profile(QDir::currentPath()+"/m229d-profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));AisPositionTests tests;return QTest::qExec(&tests,argc,argv);}
#include "ais_position_tests.moc"
