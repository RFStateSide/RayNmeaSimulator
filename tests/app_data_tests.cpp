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
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QProcess>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <memory>
#include "apppaths.h"
#include "appsettings.h"
#include "mainwindow.h"
#include "uihelper.h"
#include "themes.h"
#include "runtimeresources.h"
#include "heave127252.h"
namespace {
void write(const QString &p,const QByteArray &b){QVERIFY(QDir().mkpath(QFileInfo(p).absolutePath()));QFile f(p);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(b),b.size());}
QByteArray read(const QString &p){QFile f(p);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
QString legacy(const QString &name){return AppPaths::legacyDirectory()+"/layouts/"+name+".json";}
QString current(const QString &name){return AppPaths::layoutsDirectory()+"/"+name+".json";}
void trigger(MainWindow &w,const char *name){auto *a=w.findChild<QAction*>(name);Q_ASSERT(a);a->trigger();}
MainController *controller(MainWindow &w){return w.findChild<MainController*>();}
PgnTableModel *model(MainWindow &w){return w.findChild<PgnTableController*>()->getPgnTableModel();}
// Drive only these isolated test dialogs. No production polling or delayed workarounds.
void saveAs(MainWindow &w,const QString &name,bool confirm){
 QTimer responder;responder.setInterval(1);QObject::connect(&responder,&QTimer::timeout,&w,[&]{for(auto *widget:QApplication::topLevelWidgets()){
  if(auto *d=qobject_cast<QInputDialog*>(widget)){d->setTextValue(name);d->accept();}
  else if(auto *box=qobject_cast<QMessageBox*>(widget))box->button(confirm?QMessageBox::Yes:QMessageBox::No)->click();
 }});responder.start();trigger(w,"actionSave");
}
void confirmLoad(){QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto*b=qobject_cast<QMessageBox*>(w))b->button(QMessageBox::Yes)->click();});}
}
class AppDataTests:public QObject{
 Q_OBJECT
 std::unique_ptr<QTemporaryDir> profile;
private slots:
 void init(){profile=std::make_unique<QTemporaryDir>(QDir::currentPath()+"/build/m220-profile-XXXXXX");QVERIFY(profile->isValid());qputenv("RAYNMEA_TEST_HOME",profile->path().toUtf8());}
 void freshProfileSeedsResourcesWithoutMovingLayouts(){
  QVERIFY(!QFileInfo::exists(AppPaths::layoutsDirectory()));
  QCOMPARE(AppPaths::defaultLayout(),QString(":/defaults/defaultLayout.json"));QVERIFY(AppPaths::layoutFiles().isEmpty());
  QVERIFY(AppPaths::routesDirectory().endsWith("/routes"));QVERIFY(AppPaths::logsDirectory().endsWith("/logs"));
  QVERIFY(AppPaths::cacheDirectory()!=AppPaths::dataDirectory());
  Themes::available();Themes::styleSheet("Dark");RuntimeResources::engineProfiles();
  {MainWindow w;trigger(w,"actionLoad_Default");QCOMPARE(model(w)->getPgnList().size(),35);QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isChecked());}
  QVERIFY(!QFileInfo::exists(AppPaths::layoutsDirectory()));
  QString error;const auto p=AppPaths::layoutSavePath("First",&error);QCOMPARE(p,current("First"));QVERIFY(error.isEmpty());QVERIFY(QFileInfo(AppPaths::layoutsDirectory()).isDir());QVERIFY(!QFileInfo::exists(AppPaths::logsDirectory()));
 }
 void legacyDefaultAndPartialTransition(){
  const auto old=legacy("defaultLayout");const auto bytes=read(":/defaults/defaultLayout.json");write(old,bytes);write(legacy("StillLegacy"),bytes);
  const auto time=QFileInfo(old).lastModified();QCOMPARE(AppPaths::defaultLayout(),old);
  for(int i=0;i<3;++i){MainWindow w;trigger(w,"actionLoad_Default");QCOMPARE(model(w)->getPgnList().size(),35);QVERIFY(!QFileInfo::exists(AppPaths::layoutsDirectory()));}
  write(current("AlreadyMoved"),bytes);QCOMPARE(AppPaths::layoutFiles().size(),3);QCOMPARE(AppPaths::defaultLayout(),old);
  write(current("defaultLayout"),"{\"layouts\":[],\"numberOfWidgetColumns\":4}");QCOMPARE(AppPaths::defaultLayout(),current("defaultLayout"));
  MainWindow w;trigger(w,"actionLoad_Default");QCOMPARE(model(w)->getPgnList().size(),0);
  QCOMPARE(read(old),bytes);QCOMPARE(QFileInfo(old).lastModified(),time);QCOMPARE(AppPaths::layoutFiles().size(),4);
 }
 void defaultAliases(){
  write(legacy("Default_Layout"),"{\"layouts\":[]}");QCOMPARE(AppPaths::defaultLayout(),legacy("Default_Layout"));
  write(current("default-layout"),"{\"layouts\":[]}");QCOMPARE(AppPaths::defaultLayout(),current("default-layout"));
  write(current("defaultLayout"),"{\"layouts\":[]}");QCOMPARE(AppPaths::defaultLayout(),current("defaultLayout"));
 }
 void collisionsRemainSelectable(){
  write(legacy("Same"),"legacy-preserved");write(current("Same"),"current-preserved");
  QString selected;
  QTimer::singleShot(0,[&]{for(auto *w:QApplication::topLevelWidgets())if(auto*d=qobject_cast<QDialog*>(w);d&&d->windowTitle()=="Pick collision"){
   auto*c=d->findChild<QComboBox*>();QVERIFY(c);QCOMPARE(c->count(),2);QVERIFY(c->itemText(0).contains("current data"));QVERIFY(c->itemText(1).contains("legacy"));QCOMPARE(c->itemData(1,Qt::ToolTipRole).toString(),QDir::toNativeSeparators(legacy("Same")));c->setCurrentIndex(1);d->accept();}});
  selected=UiHelper::ShowLayoutPicker("Pick collision","Select:");QCOMPARE(selected,legacy("Same"));
  QCOMPARE(read(legacy("Same")),QByteArray("legacy-preserved"));QCOMPARE(read(current("Same")),QByteArray("current-preserved"));
 }
 void saveLegacyCopyAndCancellation(){
  const auto bytes=read(":/defaults/defaultLayout.json");write(legacy("TestFuel"),bytes);
  MainWindow w;QVERIFY(controller(w)->loadLayoutFromJson(legacy("TestFuel")));
  saveAs(w,"TestFuel",false);QVERIFY(!QFileInfo::exists(AppPaths::layoutsDirectory()));QCOMPARE(read(legacy("TestFuel")),bytes);
  saveAs(w,"TestFuel",true);QVERIFY(QFileInfo::exists(current("TestFuel")));QCOMPARE(read(legacy("TestFuel")),bytes);
  MainWindow restored;QVERIFY(controller(restored)->loadLayoutFromJson(current("TestFuel")));QCOMPARE(model(restored)->getPgnList().size(),35);
  const auto before=read(current("TestFuel"));controller(w)->addWidgetAsComboboxTextChanged("127252","Heave","Navigation");QCOMPARE(model(w)->getPgnList().size(),36);
  saveAs(w,"TestFuel",false);QCOMPARE(read(current("TestFuel")),before);
  saveAs(w,"TestFuel",true);QVERIFY(read(current("TestFuel"))!=before);
  auto*h=w.findChild<Heave127252*>();QVERIFY(h);model(w)->changeParam(h->Id(),"DelaySource","enum");const auto valid=read(current("TestFuel"));saveAs(w,"TestFuel",true);QCOMPARE(read(current("TestFuel")),valid);QCOMPARE(read(legacy("TestFuel")),bytes);
 }
 void failedDefaultLoadDoesNotReplaceObjects(){
  MainWindow w;trigger(w,"actionLoad_Default");const auto count=model(w)->getPgnList().size();write(current("defaultLayout"),"broken");trigger(w,"actionLoad_Default");QCOMPARE(model(w)->getPgnList().size(),count);
 }
 void cancelledLoadDoesNotReplaceObjects(){
  write(legacy("Only"),read(":/defaults/defaultLayout.json"));MainWindow w;trigger(w,"actionLoad_Default");const auto count=model(w)->getPgnList().size();
  QTimer::singleShot(0,[]{for(auto*w:QApplication::topLevelWidgets())if(auto*d=qobject_cast<QDialog*>(w);d&&d->windowTitle()=="Load Layout")d->reject();});trigger(w,"actionLoad");QCOMPARE(model(w)->getPgnList().size(),count);QVERIFY(!QFileInfo::exists(AppPaths::layoutsDirectory()));
 }
 void deleteSelectionAndCancellation(){
  write(legacy("DeleteCopy"),"legacy copy");write(current("DeleteCopy"),"current copy");MainWindow w;
  auto remove=[&](bool yes){QTimer responder;responder.setInterval(1);connect(&responder,&QTimer::timeout,&w,[&]{
   for(auto*top:QApplication::topLevelWidgets()){
    if(auto*box=qobject_cast<QMessageBox*>(top);box&&box->windowTitle()=="Delete Saved Layout"){
     QVERIFY(box->text().contains(QDir::toNativeSeparators(legacy("DeleteCopy"))));box->button(yes?QMessageBox::Yes:QMessageBox::No)->click();
    }else if(auto*d=qobject_cast<QDialog*>(top);d&&d->windowTitle()=="Remove Layout"){
     auto*c=d->findChild<QComboBox*>();QVERIFY(c);const int i=c->findData(legacy("DeleteCopy"));QVERIFY(i>=0);c->setCurrentIndex(i);d->accept();
    }
   }
  });responder.start();trigger(w,"actionRemove");};
  remove(false);QCOMPARE(read(legacy("DeleteCopy")),QByteArray("legacy copy"));QCOMPARE(read(current("DeleteCopy")),QByteArray("current copy"));
  remove(true);QVERIFY(!QFileInfo::exists(legacy("DeleteCopy")));QCOMPARE(read(current("DeleteCopy")),QByteArray("current copy"));
 }
 void blockedDirectoryDiagnostics(){
  QVERIFY(QDir().mkpath(AppPaths::dataDirectory()));write(AppPaths::layoutsDirectory(),"file blocks directory");QString error;
  QVERIFY(AppPaths::layoutSavePath("Blocked",&error).isEmpty());QVERIFY(error.contains("permissions"));QVERIFY(error.contains(QDir::toNativeSeparators(AppPaths::layoutsDirectory())));
  QVERIFY(AppPaths::defaultLayout(&error).isEmpty());QVERIFY(error.contains("Cannot read Layout directory"));QCOMPARE(read(AppPaths::layoutsDirectory()),QByteArray("file blocks directory"));
 }
 void readOnlyDirectory(){
  QVERIFY(QDir().mkpath(AppPaths::layoutsDirectory()));const auto dir=AppPaths::layoutsDirectory();
#ifdef Q_OS_WIN
  // QFile's read-only directory attribute is not a write-denying Windows ACL.
  // Apply/remove a deny only on this verified disposable, empty test directory.
  QVERIFY(QFileInfo(dir).canonicalFilePath().startsWith(QFileInfo(profile->path()).canonicalFilePath()+"/"));
  auto acl=[&](const QStringList &args){QProcess p;p.start("icacls.exe",args);return p.waitForFinished(5000)&&p.exitCode()==0;};
  QVERIFY(acl({QDir::toNativeSeparators(dir),"/deny","*S-1-1-0:(W)"}));
  const auto restore=qScopeGuard([&]{QVERIFY(acl({QDir::toNativeSeparators(dir),"/remove:d","*S-1-1-0"}));});
  MainWindow w;QSignalSpy status(controller(w),&MainController::ShowStatusBarMessage);
  QVERIFY(!controller(w)->saveLayoutToJson(current("NoWrite")));QVERIFY(!QFileInfo::exists(current("NoWrite")));
  QVERIFY(status.last()[0].toString().contains("Layout save failed"));
#else
  const auto permissions=QFileInfo(dir).permissions();
  const auto restore=qScopeGuard([&]{QFile::setPermissions(dir,permissions);});
  QVERIFY(QFile::setPermissions(dir,QFileDevice::ReadOwner|QFileDevice::ExeOwner));QVERIFY(!QFileInfo(dir).isWritable());
  QString error;QVERIFY(AppPaths::layoutSavePath("NoWrite",&error).isEmpty());QVERIFY(!error.isEmpty());
#endif
 }
 void invalidSaveNames(){for(const QString name:{"../escape","folder/name","folder\\name","a.json",""}){QString error;QVERIFY(AppPaths::layoutSavePath(name,&error).isEmpty());QVERIFY(!error.isEmpty());}QVERIFY(!QFileInfo::exists(AppPaths::layoutsDirectory()));}
 void themePrecedenceAndResources(){
  const auto old=AppPaths::legacyDirectory()+"/Themes/Owner/Owner.qss";const auto now=AppPaths::themesDirectory()+"/Owner/Owner.qss";
  write(old,"QLabel { color: #123456; image: url(icon.png); }");write(QFileInfo(old).absolutePath()+"/icon.png","legacy asset");QCOMPARE(Themes::source("Owner"),old);QVERIFY(Themes::styleSheet("Owner").contains(QFileInfo(old).absolutePath()+"/icon.png"));
  write(now,"QLabel { color: #654321; }");QCOMPARE(Themes::source("Owner"),now);QVERIFY(Themes::available().contains("Owner"));QVERIFY(Themes::source("Dark").startsWith(":/"));QCOMPARE(read(old),QByteArray("QLabel { color: #123456; image: url(icon.png); }"));
  const auto profiles=AppPaths::resourcesDirectory()+"/Data/engine_profiles.json";write(profiles,"user profile fixture");QCOMPARE(RuntimeResources::engineProfiles(),profiles);QVERIFY(QFile::remove(profiles));QCOMPARE(RuntimeResources::engineProfiles(),profiles);
 }
 void routeSelection_data(){QTest::addColumn<QString>("extension");QTest::newRow("GPX")<<"gpx";QTest::newRow("KML")<<"kml";}
 void routeSelection(){
  QFETCH(QString,extension);const auto route=profile->path()+"/existing routes/Owner."+extension;write(route,"unchanged route fixture");
  write(AppPaths::legacyDirectory()+"/routes/Legacy."+extension,"legacy route");
  QCOMPARE(AppPaths::routeDialogDirectory(),AppPaths::legacyDirectory()+"/routes");
  GeneratorVesselMovement w(1,"Movement",nullptr);auto*edit=w.findChild<QLineEdit*>("lineEditFilePath");QVERIFY(edit);edit->setText(route);QCOMPARE(AppPaths::routeDialogDirectory(route),QFileInfo(route).absolutePath());
  QTimer::singleShot(0,[&]{for(auto *top:QApplication::topLevelWidgets())if(auto*d=qobject_cast<QFileDialog*>(top)){
   QCOMPARE(d->fileMode(),QFileDialog::ExistingFile);QVERIFY(d->nameFilters().first().contains("*.gpx *.kml"));QCOMPARE(d->directory().absolutePath(),QFileInfo(route).absolutePath());d->selectFile(route);static_cast<QDialog*>(d)->accept();}});
  QVERIFY(QMetaObject::invokeMethod(&w,"on_pushButtonSelectRouteFile_clicked"));QCOMPARE(edit->text(),route);QCOMPARE(read(route),QByteArray("unchanged route fixture"));
  QTimer::singleShot(0,[]{for(auto*top:QApplication::topLevelWidgets())if(auto*d=qobject_cast<QFileDialog*>(top))d->reject();});QVERIFY(QMetaObject::invokeMethod(&w,"on_pushButtonSelectRouteFile_clicked"));QCOMPARE(edit->text(),route);QVERIFY(QFileInfo(AppPaths::routesDirectory()).isDir());
 }
 void settingsAndLifecycleIsolation(){
  AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Vessel/EnvironmentalInfluence",true);settings.setValue("Preference/StyleSheetName","Light");settings.sync();QVERIFY(settings.fileName().startsWith(profile->path()));QVERIFY(!settings.fileName().startsWith(AppPaths::dataDirectory()));
  write(legacy("defaultLayout"),read(":/defaults/defaultLayout.json"));MainWindow w;trigger(w,"actionLoad_Default");auto*movement=w.findChild<GeneratorVesselMovement*>();QVERIFY(movement);
  const auto count=model(w)->getPgnList().size();QPointer<GeneratorVesselMovement> before=movement;
  w.findChild<QAction*>("actionViewer_Rx")->setChecked(true);w.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);QCOMPARE(model(w)->getPgnList().size(),count);QVERIFY(before);QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isChecked());
  trigger(w,"actionSave_Layout_as_Default");QVERIFY(QFileInfo::exists(current("defaultLayout")));confirmLoad();trigger(w,"actionLoad_Default");QVERIFY(before.isNull());QCOMPARE(model(w)->getPgnList().size(),count);QVERIFY(settings.value("Vessel/EnvironmentalInfluence").toBool());QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
};
int main(int argc,char**argv){QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QList<QByteArray>args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*>ptrs;for(auto&a:args)ptrs<<a.data();int count=ptrs.size();ptrs<<nullptr;QApplication app(count,ptrs.data());QApplication::setApplicationName(APP_NAME);QApplication::setOrganizationName(APP_COMPANY);QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));AppDataTests tests;return QTest::qExec(&tests,argc,argv);}
#include "app_data_tests.moc"
