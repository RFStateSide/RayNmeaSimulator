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

class ApplicationLifecycleTests : public QObject {
 Q_OBJECT
private slots:
 void ownerSequence() {
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
  for(int cycle=0;cycle<4;++cycle) {
   action("actionLoad_Default");
   QCOMPARE(model->getPgnList().size(),35);
   auto *area=window.findChild<QMdiArea*>();QVERIFY(area);
   for(auto *sub:area->subWindowList())
    if(sub->windowTitle()=="Environment") area->setActiveSubWindow(sub);
   QTimer::singleShot(0, [] {
    for(auto *w:QApplication::topLevelWidgets())
     if(auto *d=qobject_cast<QDialog*>(w);d && d->windowTitle()=="Select Temperature Source") d->accept();
   });
   controller->addWidgetAsComboboxTextChanged("130312","Temperature","Environment");
   QTest::qWait(100);
   QCOMPARE(model->getPgnList().size(),36);
   auto *temp=window.findChild<TemperatureWidget130312*>();QVERIFY(temp);
   QCOMPARE(temp->TemperatureSource(),N2kts_SeaTemperature);
   if(cycle%2) {
    for(auto *engine:window.findChildren<GeneratorEngine*>()) engine->setEngineRunning(true);
    QTest::qWait(1200);
   }
   const QString name=QString("Test1-%1").arg(cycle);
   QTimer::singleShot(0, [name] {
    for(auto *w:QApplication::topLevelWidgets())
     if(auto *d=qobject_cast<QInputDialog*>(w)) {d->setTextValue(name);d->accept();}
   });
   action("actionSave");
   QVERIFY(QFile::exists(AppPaths::layoutsDirectory()+"/"+name+".json"));
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
   temp=window.findChild<TemperatureWidget130312*>();QVERIFY(temp);
   QCOMPARE(temp->TemperatureSource(),N2kts_SeaTemperature);
   QCOMPARE(temp->SetTemperature(),300.0);
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
 ApplicationLifecycleTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "application_lifecycle_tests.moc"
