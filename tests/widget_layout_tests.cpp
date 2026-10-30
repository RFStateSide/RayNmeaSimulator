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
#include <QMessageBox>
#include <QNetworkProxy>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleOptionButton>
#include <QDockWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QBoxLayout>
#include "mainwindow.h"
#include "appsettings.h"
#include "themes.h"
#include "pgntreecontroller.h"
#include "aisclassbstatic129809.h"
#include "spacer000000.h"

namespace {
void settle(){QCoreApplication::sendPostedEvents();QTest::qWait(60);}
void confirm(){QTimer::singleShot(0,[]{for(auto*w:QApplication::topLevelWidgets())if(auto*b=qobject_cast<QMessageBox*>(w))b->button(QMessageBox::Yes)->click();});}
void addSpacer(MainWindow &window){for(auto *sub:window.findChild<QMdiArea*>()->subWindowList())if(sub->windowTitle()=="Other") {
 QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto*d=qobject_cast<QDialog*>(w);d && d->isVisible() && d->findChild<QSpinBox*>())d->accept();});sub->findChild<QComboBox*>()->setCurrentIndex(1);
}}
QByteArray bytes(const QString &name){QFile f(name);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
QJsonObject defaults(){return QJsonDocument::fromJson(bytes("tests/fixtures/defaultLayout.json")).object();}
QJsonObject navigationTemplate(){for(auto v:defaults()["layouts"].toArray())if(v.toObject()["groupName"]=="Navigation")for(auto w:v.toObject()["widgets"].toArray())if(w.toObject()["pgn"]=="129025")return w.toObject();return {};}
QJsonObject navigation(const QJsonArray &widgets){auto root=defaults();root["layouts"]=QJsonArray{QJsonObject{{"groupName","Navigation"},{"widgets",widgets}}};return root;}
QJsonObject instrument(int id,int row,int column,int rows=3,int columns=3){auto w=navigationTemplate();w["idX"]=id;w["row"]=row;w["column"]=column;w["rowSpan"]=rows;w["columnSpan"]=columns;return w;}
QVector<int> position(QWidget *w){auto *grid=qobject_cast<QGridLayout*>(w->parentWidget()->parentWidget()->layout());
 // The grid is nested below the page's VBox; locate the frame's owning layout.
 if(!grid)for(auto *g:w->parentWidget()->parentWidget()->findChildren<QGridLayout*>())if(g->indexOf(w->parentWidget())>=0){grid=g;break;}
 if(!grid)return {};int r,c,rs,cs;grid->getItemPosition(grid->indexOf(w->parentWidget()),&r,&c,&rs,&cs);return {r,c,rs,cs};}
QGridLayout *gridFor(QWidget *w){for(auto*g:w->parentWidget()->parentWidget()->findChildren<QGridLayout*>())if(g->indexOf(w->parentWidget())>=0)return g;return nullptr;}
void capture(QWidget *w,const QString &name){auto out=qEnvironmentVariable("ISSUE23_EVIDENCE");if(!out.isEmpty()){QDir().mkpath(out);QVERIFY(w->grab().save(out+"/"+name+".png"));}}
struct Workspace {
 MainWindow window;MainController *controller=window.findChild<MainController*>();QTemporaryDir dir{QDir::currentPath()+"/build/issue23/layout-XXXXXX"};
 Workspace(){qInfo()<<"Workspace constructed";window.resize(1500,1000);qInfo()<<"Workspace resized";window.show();qInfo()<<"Workspace shown";}
 bool load(const QJsonObject &root){qInfo()<<"Loading isolated fixture";QFile file(dir.filePath("input.json"));if(!file.open(QIODevice::WriteOnly))return false;file.write(QJsonDocument(root).toJson());file.close();confirm();qInfo()<<"Fixture written";return controller->loadLayoutFromJson(file.fileName());}
 QJsonObject save(const QString &name="saved.json"){const auto f=dir.filePath(name);if(!controller->saveLayoutToJson(f))return {};return QJsonDocument::fromJson(bytes(f)).object();}
 void activate(const QString &group){auto *mdi=window.findChild<QMdiArea*>();for(auto *sub:mdi->subWindowList())if(sub->windowTitle()==group){mdi->setActiveSubWindow(sub);break;}settle();}
};
QMap<int,QVector<int>> placements(Workspace &w){QMap<int,QVector<int>> result;for(const auto &group:w.controller->getGroupList())for(auto *widget:w.controller->layoutWidgets(group))result[widget->property("Id").toInt()]=position(widget);return result;}
void alignedHeader(QWidget *widget){
 auto *frame=widget->parentWidget();QVERIFY(frame && frame->layout());
 const auto context=QString(widget->metaObject()->className())+" root "+QString::number(widget->width())+" frame content "+QString::number(frame->contentsRect().width());
 QCOMPARE(frame->layout()->contentsMargins(),QMargins());
 QVERIFY2(qAbs(widget->width()-frame->contentsRect().width())<=1,qPrintable(context));
 auto *header=qobject_cast<QBoxLayout*>(widget->layout()->itemAt(0)?widget->layout()->itemAt(0)->layout():nullptr);
 auto *title=widget->findChild<QLabel*>("labelTitle");QVERIFY2(header && title,qPrintable(context));
 QCOMPARE(header->contentsMargins(),QMargins());
 int expectedHeight=0;
 for(int i=0;i<header->count();++i)if(auto *control=header->itemAt(i)->widget();control && control->isVisibleTo(widget)) {
  const int height=control->hasHeightForWidth()?control->heightForWidth(control->width()):control->sizeHint().height();
  expectedHeight=qMax(expectedHeight,qMax(height,control->minimumHeight()));
 }
 if(header->direction()==QBoxLayout::LeftToRight || header->direction()==QBoxLayout::RightToLeft)
  QVERIFY2(header->geometry().height()<=expectedHeight+1,qPrintable(context+QString(" surplus header height %1, required %2").arg(header->geometry().height()).arg(expectedHeight)));
 else if(title->isVisibleTo(widget))
  QVERIFY2(title->height()<=qMax(title->minimumHeight(),title->heightForWidth(title->width()))+1,qPrintable(context+" stretched vertical title"));

 if(title->isVisibleTo(widget)) {
  QVERIFY2(title->alignment()&Qt::AlignTop,qPrintable(context+" title text alignment"));
  QVERIFY2(header->itemAt(header->indexOf(title))->alignment()&Qt::AlignTop,qPrintable(context+" title layout alignment"));
  const int top=title->mapTo(frame,QPoint()).y();
  QVERIFY2(qAbs(top-(frame->contentsRect().top()+widget->layout()->contentsMargins().top()))<=1,qPrintable(context+" title top "+QString::number(top)));
  if(title->hasHeightForWidth())QVERIFY2(title->height()+1>=title->heightForWidth(title->width()),qPrintable(context+QString(" wrapped title clipped: height %1, required %2, width %3").arg(title->height()).arg(title->heightForWidth(title->width())).arg(title->width())));
 }
 if(auto *check=widget->findChild<QCheckBox*>("checkBoxEnabled");check && check->isVisibleTo(widget)) {
  const auto point=check->mapTo(frame,QPoint());const auto margins=widget->layout()->contentsMargins();
  QVERIFY2(qAbs(point.x()-(frame->contentsRect().left()+margins.left()))<=1,qPrintable(context+" checkbox left"));
  QVERIFY2(qAbs(point.y()-(frame->contentsRect().top()+margins.top()))<=1,qPrintable(context+" checkbox top "+QString::number(point.y())));
 }
 for(auto *button:widget->findChildren<QPushButton*>("pushButtonRemove"))if(button->isVisibleTo(widget)){
  const auto rect=QRect(button->mapTo(frame,QPoint()),button->size());const auto margins=widget->layout()->contentsMargins();
  QVERIFY2(qAbs(rect.right()-(frame->contentsRect().right()-margins.right()))<=1,qPrintable(context+" close right "+QString::number(rect.right())));
  QVERIFY2(qAbs(rect.top()-(frame->contentsRect().top()+margins.top()))<=1,qPrintable(context+" close top "+QString::number(rect.top())));
 }
}
void nonOverlapping(const QList<QWidget*> &widgets){for(int i=0;i<widgets.size();++i)for(int j=i+1;j<widgets.size();++j)QVERIFY(!widgets[i]->parentWidget()->geometry().intersects(widgets[j]->parentWidget()->geometry()));}
}
class WidgetLayoutTests:public QObject {
 Q_OBJECT
private slots:
 void init(){qApp->setStyleSheet({});qApp->setFont(QFont("Sans Serif",9));AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");s.setValue("Nmea2000/Nmea2000Enabled",false);s.setValue("Nmea0183/Nmea0183Enabled",false);}
 void commonPresentation_data(){QTest::addColumn<QString>("theme");QTest::addColumn<int>("points");for(QString theme:{"Dark","Light"})for(int n:{9,12,16})QTest::newRow(qPrintable(theme+QString::number(n)))<<theme<<n;}
 void commonPresentation(){
  QFETCH(QString,theme);QFETCH(int,points);Workspace w;QVERIFY(w.load(defaults()));
  w.controller->addWidgetAsComboboxTextChanged("129809","AIS Static A","Communication");
  w.controller->addWidgetAsComboboxTextChanged("129810","AIS Static B","Communication");
  w.controller->addWidgetAsComboboxTextChanged("129039","AIS Position","Communication");
  for(auto *sub:w.window.findChild<QMdiArea*>()->subWindowList())if(sub->windowTitle()=="Other") {
   QTimer::singleShot(0,[]{for(auto *widget:QApplication::topLevelWidgets())if(auto *dialog=qobject_cast<QDialog*>(widget);dialog && dialog->isVisible() && dialog->findChild<QSpinBox*>())dialog->accept();});
   sub->findChild<QComboBox*>()->setCurrentIndex(1);
  }
  AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.setValue("Preference/FontFamily","Sans Serif");s.setValue("Preference/FontSize",points);s.sync();w.window.HotReloadSettings();settle();
  const auto logical=placements(w);int closeSide=-1;
  for(auto *widget:w.window.findChildren<PgnWidgetBase*>()){
   widget->ensurePolished();if(widget->layout())widget->layout()->activate();
   for(auto *button:widget->findChildren<QPushButton*>("pushButtonRemove")){
    QCOMPARE(button->minimumSize(),button->maximumSize());QCOMPARE(button->width(),button->height());
    QVERIFY(button->width()>=button->iconSize().width());QVERIFY(button->height()>=button->fontMetrics().height());
    QVERIFY(button->width()<5*button->fontMetrics().height());
    if(closeSide<0)closeSide=button->width();else {
     if(button->width()!=closeSide)qInfo()<<widget->metaObject()->className()<<"root font"<<widget->font()<<"button font"<<button->font()<<"metrics"<<button->fontMetrics().height()<<"button"<<button->size();
     QCOMPARE(button->width(),closeSide);
    }
    QCOMPARE(button->sizePolicy().horizontalPolicy(),QSizePolicy::Fixed);QVERIFY(!button->accessibleName().isEmpty());
   }
  }
  for(int width:{750,1500,2400}){
   w.window.resize(width,1000);settle();
   for(const auto &group:QStringList{"Navigation","Propulsion","Environment","Tanks","Electrical","Communication","Other","Generate"}){
    w.activate(group);const auto widgets=w.controller->layoutWidgets(group);nonOverlapping(widgets);
    if(widgets.isEmpty())continue;
    auto *grid=gridFor(widgets.first());QVERIFY(grid);QCOMPARE(grid->contentsMargins(),QMargins());
    for(int i=0;i<grid->columnCount();++i)QCOMPARE(grid->columnMinimumWidth(i),0);
    for(auto *widget:widgets){QVERIFY(widget->width()>=widget->minimumSizeHint().width());QVERIFY(widget->height()>=widget->minimumSizeHint().height());
     for(auto *button:widget->findChildren<QPushButton*>("pushButtonRemove"))if(button->isVisibleTo(widget))QVERIFY(widget->rect().contains(QRect(button->mapTo(widget,QPoint()),button->size())));
     alignedHeader(widget);
     for(auto *edit:widget->findChildren<QLineEdit*>())if(edit->isVisibleTo(widget))QCOMPARE(edit->font().pointSize(),points);
     for(auto *label:widget->findChildren<QLabel*>())if(label->isVisibleTo(widget) && !label->wordWrap() && label->pixmap().isNull() && !label->text().isEmpty() && !label->text().contains('<'))
      QVERIFY2(label->contentsRect().width()+1>=label->fontMetrics().horizontalAdvance(label->text()),qPrintable(QString(widget->metaObject()->className())+" / "+label->objectName()));
    }
    if(width==1500 && (group=="Navigation" || group=="Propulsion" || group=="Environment" || group=="Communication" || group=="Generate"))capture(&w.window,theme+QString("-%1pt-").arg(points)+group);
   }
   QCOMPARE(placements(w),logical);
  }
  const auto beforeDock=placements(w);const auto docks=w.window.findChildren<QDockWidget*>();QVERIFY(docks.size()>=2);
  w.window.resizeDocks({docks.first()},{420},Qt::Horizontal);settle();QCOMPARE(placements(w),beforeDock);
  w.window.resizeDocks({docks.first()},{240},Qt::Horizontal);settle();QCOMPARE(placements(w),beforeDock);
  s.setValue("Preference/FontWeight",QFont::Bold);s.setValue("Preference/FontItalic",true);s.sync();w.window.HotReloadSettings();settle();
  s.setValue("Preference/FontWeight",QFont::Normal);s.setValue("Preference/FontItalic",false);s.sync();w.window.HotReloadSettings();settle();
  for(auto *widget:w.window.findChildren<PgnWidgetBase*>()) {
   for(auto *edit:widget->findChildren<QLineEdit*>()){QVERIFY(!edit->font().bold());QVERIFY(!edit->font().italic());}
   if(auto *title=widget->findChild<QLabel*>("labelTitle"))QVERIFY(title->font().bold());
  }
  QCOMPARE(placements(w),logical);
  QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
 void allInstrumentHeaders_data(){commonPresentation_data();}
 void allInstrumentHeaders(){
  QFETCH(QString,theme);QFETCH(int,points);Workspace w;
  AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Preference/StyleSheetName",theme);settings.setValue("Preference/FontFamily","Sans Serif");settings.setValue("Preference/FontSize",points);settings.sync();w.window.HotReloadSettings();
  const QStringList types={"Attitude127257","CogSog129026","CrossTrack129283","DateTime129033","EngineDynamic127489","EngineRapid127488","FluidLevel127505","GeneratorDateTime","GeneratorEngine","GeneratorTransLowerUnit","GeneratorVesselMovement","GeneratorWeather","GnssPositionData129029","Heave127252","LatLong129025","MagneticVariation127258","Navigation129284","AisClassBPosition129039","AisClassBStatic129810","AisClassBStatic129809","WindData130306","Humidity130313","OutsideEnvironmental130310","RateOfTurn127251","Speed128259","Depth128267","SystemTime126992","TemperatureExtended130316","TemperatureWidget130312","TransmissionDynamic127493","VesselHeading127250","DcStatus127506","ChargerStatus127507","BatteryStatus127508","Rudder127245","TrimTab130576","GlobalPositioningGGA","RecommendedMinGnssRMC","HeadingHDG","TrueHeadingHDT","GeographicPositionGLL","CogSogVTG","DateTimeZDA","Spacer000000","WindlassOperating128777","WindlassMonitoring128778"};QCOMPARE(types.size(),46);
  QList<QWidget*> widgets;QMap<QPushButton*,bool> hidden;int id=10000;
  for(const auto &type:types){
   WidgetConfig config{};config.idX=++id;config.label=type;config.widgetType=type;
   auto *widget=w.controller->createWidgetFromConfig(config);QVERIFY2(widget,qPrintable(type));
   for(auto *button:widget->findChildren<QPushButton*>("pushButtonRemove"))hidden[button]=button->isHidden();
   w.controller->addWidgetToSubForm(widget,"Other");widgets.append(widget);
  }
  w.activate("Other");
  for(bool wrapped:{false,true,false}) {
   for(auto *widget:widgets)if(auto *title=widget->findChild<QLabel*>("labelTitle"))
    title->setText(wrapped?QString(widget->metaObject()->className())+" - extended instrument title for header alignment verification":"Short title");
   for(int width:{900,1600}){
    w.window.resize(width,1000);settle();
    for(auto *widget:widgets){alignedHeader(widget);for(auto *button:widget->findChildren<QPushButton*>("pushButtonRemove"))QCOMPARE(button->isHidden(),hidden[button]);}
   }
  }
  QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
 void widthAllocationAndRemoval(){
  Workspace w;
#ifdef Q_OS_LINUX
  // This allocation fixture deliberately exceeds small remote-desktop screens.
  // Bypass WM size clamping here; the theme matrix uses normal managed windows.
  w.window.setWindowFlag(Qt::X11BypassWindowManagerHint,true);w.window.show();
#endif
  QVERIFY(w.load(navigation({instrument(801,1,0),instrument(802,1,3)})));w.activate("Navigation");
  auto widgets=w.controller->layoutWidgets("Navigation");QCOMPARE(widgets.size(),2);auto *grid=gridFor(widgets[0]);QVERIFY(grid);
  int oldWidth=0,oldAvailable=0;bool grew=false;QMargins edges;
  for(int width:{900,1200,1600}){w.window.resize(width,900);settle();auto left=widgets[0]->parentWidget()->geometry(),right=widgets[1]->parentWidget()->geometry();
   QVERIFY(qAbs(left.width()-right.width())<=2);
   const int available=grid->geometry().width();
   if(oldAvailable && available>oldAvailable){QVERIFY(left.width()>oldWidth);grew=true;}
   if(oldAvailable && available==oldAvailable)QCOMPARE(left.width(),oldWidth);
   oldWidth=left.width();oldAvailable=available;
   if(width==900)edges=QMargins(left.left()-grid->geometry().left(),0,grid->geometry().right()-right.right(),0);
   else {QCOMPARE(left.left()-grid->geometry().left(),edges.left());QCOMPARE(grid->geometry().right()-right.right(),edges.right());}
   QVERIFY(right.left()-left.right()>0);QVERIFY(right.left()-left.right()<widgets[0]->fontMetrics().height()*2);
  }
  QVERIFY(grew);
  auto *button=widgets[0]->findChild<QPushButton*>("pushButtonRemove");QVERIFY(button);QPointer<QWidget> removed=widgets[0];
  QTimer confirmation;connect(&confirmation,&QTimer::timeout,&w.window,[&]{for(auto*x:QApplication::topLevelWidgets())if(auto*b=qobject_cast<QMessageBox*>(x);b && b->isVisible()){confirmation.stop();b->button(QMessageBox::Yes)->click();}});
  confirmation.start(5);QTest::mouseClick(button,Qt::LeftButton);confirmation.stop();QTRY_VERIFY(removed.isNull());
  QCOMPARE(w.controller->layoutWidgets("Navigation").size(),1);w.window.resize(1200,900);settle();const auto before=widgets[1]->width(),beforeAvailable=grid->geometry().width();w.window.resize(1600,900);settle();QVERIFY(widgets[1]->width()>=before);if(grid->geometry().width()>beforeAvailable)QVERIFY(widgets[1]->width()>before);
  w.controller->setMode(Viewer);w.controller->addWidgetAsComboboxTextChanged("129025","Position","Navigation");w.controller->setMode(Sender);settle();QCOMPARE(w.controller->layoutWidgets("Navigation").size(),2);nonOverlapping(w.controller->layoutWidgets("Navigation"));
 }
 void authoritativeSparseAndSpans(){
  Workspace w;auto root=navigation({instrument(811,0,0,3,2),instrument(812,0,3,5,3),instrument(813,8,1,2,4)});QVERIFY(w.load(root));const auto original=bytes(w.dir.filePath("input.json"));
  const auto expected=placements(w);QCOMPARE(expected[811],QVector<int>({0,0,3,2}));QCOMPARE(expected[812],QVector<int>({0,3,5,3}));QCOMPARE(expected[813],QVector<int>({8,1,2,4}));
  for(int n:{9,12,16})for(int width:{700,1400,2300}){w.window.setFont(QFont("Sans Serif",n));w.window.resize(width,850);settle();QCOMPARE(placements(w),expected);}
  const auto saved=w.save();QVERIFY(!saved.isEmpty());QCOMPARE(bytes(w.dir.filePath("input.json")),original);QVERIFY(w.load(saved));QCOMPARE(placements(w),expected);
  for(const auto &v:saved["layouts"].toArray())if(v.toObject()["groupName"]=="Navigation"){
   const auto a=v.toObject()["widgets"].toArray(),b=root["layouts"].toArray().first().toObject()["widgets"].toArray();QCOMPARE(a.size(),b.size());
   for(int i=0;i<a.size();++i){QCOMPARE(a[i].toObject()["parameters"],b[i].toObject()["parameters"]);QCOMPARE(a[i].toObject()["row"],b[i].toObject()["row"]);QCOMPARE(a[i].toObject()["column"],b[i].toObject()["column"]);}
  }
  auto invalid=saved;auto groups=invalid["layouts"].toArray();auto g=groups[0].toObject();auto a=g["widgets"].toArray();auto bad=a[0].toObject();bad["type"]="UnknownWidget";a[0]=bad;g["widgets"]=a;groups[0]=g;invalid["layouts"]=groups;
  QVERIFY(!w.load(invalid));QCOMPARE(placements(w),expected);
 }
 void legacyMixedAndStableReload(){
  Workspace w;auto missing=instrument(824,0,0);missing.remove("row");missing.remove("column");
  auto root=navigation({instrument(821,30,0),instrument(822,30,0),instrument(823,1,0),missing});QVERIFY(w.load(root));
  auto expected=placements(w);QCOMPARE(expected[823],QVector<int>({1,0,3,3}));QCOMPARE(expected[821],QVector<int>({1,3,3,3}));w.activate("Navigation");nonOverlapping(w.controller->layoutWidgets("Navigation"));
  const auto saved=w.save();QVERIFY(!saved.isEmpty());QVERIFY(w.load(saved));QCOMPARE(placements(w),expected);
  w.controller->setMode(Viewer);w.controller->setMode(Sender);QCOMPARE(placements(w),expected);QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
  QVERIFY(w.load(defaults()));const auto legacy=placements(w);auto resolved=w.save();QVERIFY(w.load(resolved));QCOMPARE(placements(w),legacy);
 }
 void parameterUpdatesDoNotRepack(){
  Workspace w;QVERIFY(w.load(navigation({instrument(831,1,0),instrument(832,1,3)})));w.activate("Navigation");settle();const auto logical=placements(w);auto widgets=w.controller->layoutWidgets("Navigation");QList<QRect> geometry;for(auto*x:widgets)geometry<<x->parentWidget()->geometry();
  auto *model=w.window.findChild<PgnTableController*>()->getPgnTableModel();QSignalSpy resets(model,&QAbstractItemModel::modelReset);
  for(int i=0;i<200;++i)model->changeParam(831,"Latitude",QString::number(40+i*.0001,'g',17));settle();
  QCOMPARE(placements(w),logical);QCOMPARE(resets.count(),0);for(int i=0;i<widgets.size();++i)QCOMPARE(widgets[i]->parentWidget()->geometry(),geometry[i]);
  auto *inspection=w.window.findChild<PgnTreeController*>();QVERIFY(inspection);
  auto *tree=inspection->getTreeModel();QPersistentModelIndex stable(tree->indexFor(widgets[0]));QVERIFY(stable.isValid());
  QSignalSpy treeResets(tree,&QAbstractItemModel::modelReset);
  auto *parent=inspection->getTreeView()->parentWidget();while(parent && !qobject_cast<QDockWidget*>(parent))parent=parent->parentWidget();
  auto *dock=qobject_cast<QDockWidget*>(parent);QVERIFY(dock);dock->hide();settle();dock->show();settle();
  QCOMPARE(treeResets.count(),0);QVERIFY(stable.isValid());QCOMPARE(placements(w),logical);

 }
 void explicitSpanEdits(){
  Workspace w;QVERIFY(w.load(navigation({instrument(841,1,0),instrument(842,1,3),instrument(843,4,0)})));auto widgets=w.controller->layoutWidgets("Navigation");auto *first=qobject_cast<PgnWidgetBase*>(widgets[0]);auto second=position(widgets[1]);
  first->setRowSpan(6);settle();QCOMPARE(position(first),QVector<int>({1,0,6,3}));QCOMPARE(position(widgets[1]),second);nonOverlapping(widgets);
  first->setColumnSpan(4);settle();QCOMPARE(position(first),QVector<int>({1,0,6,4}));nonOverlapping(widgets);
  auto expected=placements(w);auto saved=w.save();QVERIFY(w.load(saved));QCOMPARE(placements(w),expected);
 }
 void spacerRowsAndVisibility(){
  Workspace w;QVERIFY(w.load(navigation({})));for(int i=0;i<3;++i)addSpacer(w.window);w.activate("Other");auto widgets=w.controller->layoutWidgets("Other");QCOMPARE(widgets.size(),3);
  auto *spacer=qobject_cast<Spacer000000*>(widgets.first());QVERIFY(spacer);spacer->setRows(5);settle();QCOMPARE(position(spacer)[2],5);QVERIFY(spacer->height()>=150);nonOverlapping(widgets);
  spacer->setShowTitleRow(false);settle();QVERIFY(!spacer->findChild<QPushButton*>("pushButtonRemove")->isVisible());spacer->setShowTitleRow(true);settle();QVERIFY(spacer->findChild<QPushButton*>("pushButtonRemove")->isVisible());
  const auto expected=placements(w);auto saved=w.save();QVERIFY(w.load(saved));QCOMPARE(placements(w),expected);
 }
 void customFontStylesSettle(){
  qApp->setStyleSheet("QLabel { font-size: 11pt; } QPushButton { font-size: 12pt; }");
  AisClassBStatic129809 widget(850,"Styled AIS");widget.setFont(QFont("Sans Serif",16));widget.show();settle();
  struct Counter:QObject {int changes=0;bool eventFilter(QObject *,QEvent *event)override{if(event->type()==QEvent::LayoutRequest)++changes;return false;}} counter;
  widget.installEventFilter(&counter);const auto minimum=widget.minimumSize();
  for(int i=0;i<100;++i)widget.setSID(i%250);settle();QVERIFY(counter.changes<20);QCOMPARE(widget.minimumSize(),minimum);
 }
};
int main(int argc,char **argv){QDir().mkpath(QDir::currentPath()+"/build/issue23");QTemporaryDir profile(QDir::currentPath()+"/build/issue23/profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));WidgetLayoutTests tests;return QTest::qExec(&tests,argc,argv);}
#include "widget_layout_tests.moc"
