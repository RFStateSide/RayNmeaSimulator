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
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QAction>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include "mainwindow.h"
#include "aboutdialog.h"
#include "signalkpublisher.h"
#include "appsettings.h"
#include "themes.h"
#include "generatorvesselmovement.h"

class PublicReleaseTests : public QObject {
 Q_OBJECT
private slots:
 void internalReuseProvenance() {
  const QStringList names={"aboutdialog.cpp","aboutlinks.h","credentialstore.cpp",
                          "applicationlog.cpp","applicationlog.h","logviewer.cpp","logviewer.h"};
  const auto source=QFileInfo(QFINDTESTDATA("../src/aboutdialog.cpp")).absolutePath();
  for(const auto &name:names){
   QFile file(source+"/"+name);QVERIFY2(file.open(QIODevice::ReadOnly),qPrintable(name));
   const auto text=file.readAll();
   QVERIFY(text.startsWith("/*\n * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator"));
   QCOMPARE(text.count("This file is part of " "RayNmeaSimulator."),1);
   QVERIFY(text.contains("Adapted from RF StateSide BrickSuite"));
   QVERIFY(text.contains("this adaptation is AGPL-3.0-or-later"));
  }
 }
 void licenseAndResourceBoundary() {
  QFile license(QFINDTESTDATA("../LICENSE"));QVERIFY(license.open(QIODevice::ReadOnly));
  auto text=license.readAll();QVERIFY(text.contains("GNU AFFERO GENERAL PUBLIC LICENSE"));QVERIFY(text.contains("END OF TERMS AND CONDITIONS"));
  QFile notes(":/resources/ABOUT-NOTICES.txt");QVERIFY(notes.open(QIODevice::ReadOnly));QVERIFY(notes.readAll().contains("AGPL-3.0-or-later"));
  QVERIFY(!QFile::exists(":/icons/icons8-setup-100.png"));QVERIFY(!QFile::exists(":/icons/engine_32x32.png"));
  QVERIFY(QFile::exists(":/images/RayNmeaSimulator_1024.png"));QVERIFY(QFile::exists(":/tree-open.png"));
  QFile source(QFINDTESTDATA("../RayNmeaSimulator.qrc"));QVERIFY(source.open(QIODevice::ReadOnly));QVERIFY(!source.readAll().contains("icons8-"));
 }
 void newAndSavedEndpoints() {
  QCOMPARE(SignalKConfig{}.server,QString("http://127.0.0.1:3000"));
  AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Nmea2000/SignalKServer","http://example.invalid:3000");settings.sync();
  MainWindow window;window.findChild<QAction*>("actionOpen_Setup_Form")->trigger();bool found=false;for(auto *field:window.findChildren<QLineEdit*>())if(field->text()=="http://example.invalid:3000")found=true;
  QVERIFY(found);QCOMPARE(settings.value("Nmea2000/SignalKServer").toString(),QString("http://example.invalid:3000"));
  QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());settings.remove("Nmea2000/SignalKServer");
 }
 void icons_data(){QTest::addColumn<QString>("theme");for(const auto &theme:Themes::builtInThemes())QTest::newRow(qPrintable(theme))<<theme;}
 void icons(){
  QFETCH(QString,theme);AppSettings preferences(APP_COMPANY,APP_NAME);preferences.setValue("Preference/StyleSheetName",theme);preferences.sync();MainWindow window;
  auto *setup=window.findChild<QAction*>("actionOpen_Setup_Form");auto *exit=window.findChild<QAction*>("actionExit");QVERIFY(setup);QVERIFY(exit);QVERIFY(!setup->icon().isNull());QVERIFY(!exit->icon().isNull());
  GeneratorVesselMovement movement(901,"Fixture movement",nullptr);auto *refresh=movement.findChild<QPushButton*>("pushButtonRefreshGameList");QVERIFY(refresh);QVERIFY(!refresh->icon().isNull());
  QWidget panel;auto *layout=new QVBoxLayout(&panel);layout->addWidget(new QLabel("Original controls glyph + Qt icons"));auto *row=new QHBoxLayout;layout->addLayout(row);
  for(const auto &item:QList<QPair<QString,QIcon>>{{"Setup",setup->icon()},{"Exit",exit->icon()},{"Controller refresh",refresh->icon()}}){auto *column=new QVBoxLayout;auto *image=new QLabel;image->setPixmap(item.second.pixmap(48,48));column->addWidget(image);column->addWidget(new QLabel(item.first));row->addLayout(column);}
  // System uses the native platform palette; explicit themes use our palette.
  if(theme!="System")QCOMPARE(panel.palette().color(QPalette::Window),Themes::palette(theme).color(QPalette::Window));
  panel.show();QVERIFY(QTest::qWaitForWindowExposed(&panel));QTest::qWait(30);
  const auto folder=qEnvironmentVariable("RAY_PUBLIC_IMAGES");if(!folder.isEmpty()){QDir().mkpath(folder);QVERIFY(panel.grab().save(folder+"/"+theme+"-icons.png"));}
  QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
 }
};
int main(int argc,char **argv){QTemporaryDir profile;if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());qputenv("RAYNMEA_TEST_CREDENTIALS","1");
 QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*> pointers;for(auto &arg:args)pointers<<arg.data();int count=pointers.size();pointers<<nullptr;
 QApplication app(count,pointers.data());QApplication::setApplicationName(APP_NAME);QApplication::setOrganizationName(APP_COMPANY);QApplication::setApplicationVersion(APP_VERSION);Themes::initializeSystemStyle();PublicReleaseTests t;return QTest::qExec(&t,argc,argv);}
#include "public_release_tests.moc"
