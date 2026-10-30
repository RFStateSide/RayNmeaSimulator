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
#include <QLabel>
#include <QScopeGuard>
#include <QLineEdit>
#include <QTabWidget>
#include <QTabBar>
#include <QScreen>
#include <QFontInfo>
#include <QFontDialog>
#include <QNetworkProxy>
#include <QScrollArea>
#include <QStyleOptionFrame>
#include <QAction>
#include <QMessageBox>
#include <QPushButton>
#include <QJsonDocument>
#include <QTreeWidget>
#include "generatorweather.h"
#include "generatorengine.h"
#include "generatortranslowerunit.h"
#include "generatorvesselmovement.h"
#include "mainwindow.h"
#include "dialogsetup.h"
#include "engineprofiledialog.h"
#include "appsettings.h"
#include "themes.h"
namespace {
QString family(){return qEnvironmentVariable("M227_FONT","Sans Serif");}
void settle(){QCoreApplication::sendPostedEvents();QTest::qWait(30);}
void readable(QWidget *root) {
 for(auto *label:root->findChildren<QLabel*>()) {
  if(!label->isVisibleTo(root)||label->wordWrap()||label->text().isEmpty()||!label->pixmap().isNull()||label->text().contains('<'))continue;
  if(qobject_cast<PgnWidgetBase*>(root))QVERIFY2(root->rect().contains(QRect(label->mapTo(root,QPoint()),label->size())),qPrintable(label->objectName()+" outside widget"));
  const int needed=label->fontMetrics().size(label->buddy() ? Qt::TextShowMnemonic : 0,label->text()).width();
  QVERIFY2(label->contentsRect().width()+1>=needed,qPrintable(QString("%1 [%2] needs %3; has %4").arg(root->metaObject()->className(),label->objectName()+" "+label->text()).arg(needed).arg(label->contentsRect().width())));
  QVERIFY2(label->contentsRect().height()>=label->fontMetrics().height(),qPrintable(label->objectName()+" clipped vertically"));
 }
 for(auto *edit:root->findChildren<QLineEdit*>()) {
  if(!edit->isVisibleTo(root)||edit->objectName().startsWith("qt_")||edit->echoMode()!=QLineEdit::Normal)continue;
  if(edit->text().isEmpty())continue;
  // Paths/names can scroll by design. Numeric/unavailable readouts must fit.
  bool numeric=false;edit->text().toDouble(&numeric);
  if(!numeric&&edit->text()!="Unavailable")continue;
  QStyleOptionFrame option;option.initFrom(edit);
  const auto rect=edit->style()->subElementRect(QStyle::SE_LineEditContents,&option,edit);
  QVERIFY2(rect.width()>=edit->fontMetrics().horizontalAdvance(edit->text())+4,qPrintable(edit->objectName()+" numeric text clipped"));
 }
}
void capture(QWidget *widget,const QString &name){const auto out=qEnvironmentVariable("M227_EVIDENCE");if(!out.isEmpty()){QDir().mkpath(out);QVERIFY(widget->grab().save(out+"/"+name+".png"));}}
}
class ResponsiveLayoutTests:public QObject {
 Q_OBJECT
private slots:
 void init(){qApp->setStyleSheet({});qApp->setFont(QFont(family(),10));AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");}
 void weather_data(){QTest::addColumn<QString>("theme");QTest::addColumn<int>("points");QTest::addColumn<int>("width");for(const auto &theme:{QString("Dark"),QString("Light")})for(int points:{10,14})for(int width:{300,550})QTest::newRow(qPrintable(QString("%1-%2pt-%3").arg(theme).arg(points).arg(width)))<<theme<<points<<width;}
 void weather(){
  QFETCH(QString,theme);QFETCH(int,points);QFETCH(int,width);
  qApp->setFont(QFont(family(),points));qApp->setPalette(Themes::palette(theme));
  GeneratorWeather w(1,"Weather",nullptr);w.setStyleSheet(Themes::styleSheet(theme));w.setFont(QFont(family(),points));
  QVERIFY(w.setProperty("AirTemperature",293.15));QVERIFY(w.setProperty("SeaTemperature",292.15));
  const auto configuration=w.layoutConfiguration();auto *tabs=w.findChild<QTabWidget*>();QVERIFY(tabs);
  QScrollArea viewport;viewport.setWidgetResizable(true);viewport.setWidget(&w);const auto detach=qScopeGuard([&]{viewport.takeWidget();});viewport.resize(width,650);viewport.show();settle();
  QCOMPARE(w.findChild<QLabel*>("labelSource")->font().pointSize(),points);
  for(int i=0;i<tabs->count();++i){tabs->setCurrentIndex(i);settle();readable(&w);
   auto *bar=tabs->tabBar();QVERIFY(bar->usesScrollButtons());QCOMPARE(bar->elideMode(),Qt::ElideNone);
   QVERIFY(bar->tabRect(i).width()>=bar->fontMetrics().horizontalAdvance(bar->tabText(i)));
   QVERIFY(bar->rect().contains(bar->tabRect(i).center()));
   for(auto *edit:tabs->widget(i)->findChildren<QLineEdit*>())for(auto *label:tabs->widget(i)->findChildren<QLabel*>()) {
    if(!edit->isVisibleTo(&w)||!label->isVisibleTo(&w))continue;
    const QRect a(edit->mapTo(&w,QPoint()),edit->size()),b(label->mapTo(&w,QPoint()),label->size());
    QVERIFY2(!a.intersects(b),qPrintable(edit->objectName()+" overlaps "+label->objectName()));
   }
  }
  tabs->setCurrentIndex(0);settle();capture(&viewport,QString("weather-%1-%2pt-%3").arg(theme).arg(points).arg(width));
  QCOMPARE(w.layoutConfiguration(),configuration);QCOMPARE(w.property("AirTemperature").toDouble(),293.15);QCOMPARE(w.property("SeaTemperature").toDouble(),292.15);
  QVERIFY(w.width()>=w.minimumSizeHint().width());
  qInfo()<<"title"<<w.findChild<QLabel*>("labelTitle")->font()<<w.findChild<QLabel*>("labelTitle")->palette().color(QPalette::WindowText)<<w.findChild<QLabel*>("labelTitle")->geometry();
  qInfo()<<"Weather"<<theme<<family()<<points<<"logical DPI"<<w.screen()->logicalDotsPerInch()<<"DPR"<<w.devicePixelRatioF()<<"minimum"<<w.minimumSizeHint()<<"viewport"<<width;
  viewport.takeWidget(); // Stack-owned instrument.
 }
 void instrumentFontChanges(){
  GeneratorEngine engine(2,"Engine",nullptr);GeneratorTransLowerUnit transmission(3,"Transmission",nullptr);GeneratorVesselMovement movement(4,"Movement",nullptr);
  for(auto *w:QList<PgnWidgetBase*>{&engine,&transmission,&movement}){
   const auto id=w->Id();const int row=w->RowSpan(),column=w->ColumnSpan();
   w->show();w->setFont(QFont(family(),14));w->resize(300,500);settle();
   for(auto *tabs:w->findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i){tabs->setCurrentIndex(i);settle();readable(w);}
   const auto large=w->minimumSize();capture(w,QString(w->metaObject()->className())+"-14pt");
   w->setFont(QFont(family(),10));settle();QVERIFY(w->minimumWidth()<=large.width());
   QCOMPARE(w->Id(),id);QCOMPARE(w->RowSpan(),row);QCOMPARE(w->ColumnSpan(),column);w->hide();
  }
 }
 void defaultLayoutAndModes(){
  AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Preference/FontFamily",family());settings.setValue("Preference/FontSize",10);settings.sync();
  MainWindow window;window.resize(1250,850);window.show();auto *controller=window.findChild<MainController*>();QVERIFY(controller);
  QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox*>(w))box->button(QMessageBox::Yes)->click();});
  QVERIFY(controller->loadLayoutFromJson(":/defaults/defaultLayout.json"));settle();
  auto widgets=window.findChildren<PgnWidgetBase*>();QVERIFY(widgets.size()>=30);
  QMap<int,QPair<int,int>> identity;for(auto*w:widgets)identity[w->Id()]={w->RowSpan(),w->ColumnSpan()};
  qApp->setFont(QFont(family(),14));window.setFont(QFont(family(),14));settle();
  for(auto*w:widgets){QVERIFY(w->minimumSizeHint().isValid());QCOMPARE(identity[w->Id()],qMakePair(w->RowSpan(),w->ColumnSpan()));
   // Inspect every existing type's active page even when its Layout group is hidden.
   if(w->layout())w->layout()->activate();readable(w);
  }
  auto *weather=window.findChild<GeneratorWeather*>();QVERIFY(weather);auto *tabs=weather->findChild<QTabWidget*>();tabs->setCurrentIndex(1);
  qApp->setFont(QFont(family(),10));window.setFont(QFont(family(),10));settle();QCOMPARE(tabs->currentIndex(),1);
  controller->setMode(Viewer);controller->setMode(Sender);settle();QCOMPARE(window.findChildren<PgnWidgetBase*>().size(),widgets.size());
  QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
  QTemporaryDir directory;QVERIFY(controller->saveLayoutToJson(directory.filePath("layout.json")));
  QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox*>(w))box->button(QMessageBox::Yes)->click();});
  QVERIFY(controller->loadLayoutFromJson(directory.filePath("layout.json")));settle();QCOMPARE(window.findChildren<PgnWidgetBase*>().size(),widgets.size());
  capture(&window,"default-layout");
 }
 void fontPreferenceAndAuxiliaryWindows(){
  AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Preference/FontFamily",family());settings.setValue("Preference/FontSize",14);settings.sync();
  {MainWindow window;window.show();QCOMPARE(window.font().pointSize(),14);
   window.findChild<QAction*>("actionOpen_Setup_Form")->trigger();auto *setup=window.findChild<DialogSetup*>();QVERIFY(setup);settle();
   QTimer::singleShot(30,[&]{for(auto*w:QApplication::topLevelWidgets())if(auto*d=qobject_cast<QFontDialog*>(w)){const auto close=qScopeGuard([&]{d->reject();});QCOMPARE(d->currentFont().pointSize(),14);}});
   QVERIFY(QMetaObject::invokeMethod(setup,"on_pushButtonFont_clicked",Qt::DirectConnection));QCOMPARE(window.font().pointSize(),14);
   for(auto*t:setup->findChildren<QTabWidget*>())for(int i=0;i<t->count();++i){t->setCurrentIndex(i);settle();readable(setup);}
   capture(setup,"setup-14pt");setup->reject();
   window.findChild<QAction*>("actionEngineProfiles")->trigger();auto*editor=window.findChild<EngineProfileDialog*>();QVERIFY(editor);settle();readable(editor);capture(editor,"engine-profile-editor-14pt");editor->close();
  }
  MainWindow restarted;QCOMPARE(restarted.font().pointSize(),14);QCOMPARE(settings.value("Preference/FontSize").toInt(),14);
 }
 void boundedRefresh(){GeneratorWeather w(1,"Weather",nullptr);w.show();settle();const auto minimum=w.minimumSize();for(int i=0;i<200;++i){w.setProperty("AirTemperature",293.15+i*.001);QCoreApplication::processEvents();}settle();QCOMPARE(w.minimumSize(),minimum);}
};
int main(int argc,char **argv){QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));ResponsiveLayoutTests tests;return QTest::qExec(&tests,argc,argv);}
#include "responsive_layout_tests.moc"
