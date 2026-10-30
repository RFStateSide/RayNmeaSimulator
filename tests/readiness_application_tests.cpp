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
#include <QTemporaryDir>
#include "appsettings.h"
#include "mainwindow.h"
#include "nmeasenderworker.h"
class ReadinessTests:public QObject {
 Q_OBJECT
private slots:
 void unavailableSocketCanCannotStart() {
  AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Nmea2000/Nmea2000Enabled",false);s.setValue("Nmea0183/Nmea0183Enabled",false);s.setValue("Nmea2000/InterfaceType","None");s.sync();
  MainWindow w;auto*a=w.findChild<QAction*>("actionStart_Sending");auto*sender=w.findChild<NmeaSenderWorker*>();QVERIFY(a&&sender);QVERIFY(!a->isEnabled());
  s.setValue("Nmea2000/Nmea2000Enabled",true);s.setValue("Nmea2000/InterfaceType","SocketCan");s.setValue("Nmea2000/SocketCanInterface","m24offline");s.sync();w.HotReloadSettings();
  QVERIFY(!a->isEnabled());a->setChecked(true);QVERIFY(!a->isChecked());QVERIFY(!sender->isSending());
  w.findChild<QAction*>("actionViewer_Rx")->setChecked(true);QVERIFY(!a->isEnabled());
  w.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);QVERIFY(!a->isEnabled());
  w.close();
 }
};
int main(int argc,char**argv){QApplication app(argc,argv);QTemporaryDir p;if(!p.isValid())return 1;qputenv("RAYNMEA_TEST_HOME",p.path().toUtf8());ReadinessTests t;return QTest::qExec(&t,argc,argv);}
#include "readiness_application_tests.moc"
