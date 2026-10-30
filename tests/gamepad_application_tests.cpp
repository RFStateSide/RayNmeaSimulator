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
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QNetworkProxy>
#include "appsettings.h"
#include "generatorvesselmovement.h"
#include "convert.h"
class GamepadApplicationTests : public QObject {
 Q_OBJECT
 int virtualPad=-1;
 void enable(GeneratorVesselMovement &w) {
  auto *list=w.findChild<QComboBox*>("comboBoxGamePadDevice");int index=list->findText("Virtual Controller");QVERIFY(index>=0);list->setCurrentIndex(index);
  w.findChild<QCheckBox*>("checkBoxGamePadEnabled")->click();QVERIFY(w.findChild<QCheckBox*>("checkBoxGamePadEnabled")->isChecked());
 }
private slots:
 void initTestCase(){QVERIFY(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)==0);virtualPad=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);QVERIFY(virtualPad>=0);}
 void cleanupTestCase(){SDL_JoystickDetachVirtual(virtualPad);SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);}
 void mappingsDeadZoneAndSync(){
  GeneratorVesselMovement w(1,"Vessel Movement",nullptr);w.setMyVessel(MyVessel{});enable(w);auto *g=w.findChild<GameController*>();QVERIFY(g);
  g->buttonPressed(3);QCOMPARE(w.TransmissionGear(),N2kTG_Forward);g->buttonPressed(2);QCOMPARE(w.TransmissionGear(),N2kTG_Reverse);
  g->buttonPressed(1);QCOMPARE(w.TransmissionGearStbd(),N2kTG_Forward);g->buttonPressed(0);QCOMPARE(w.TransmissionGearStbd(),N2kTG_Reverse);
  g->buttonPressed(9);QCOMPARE(w.TransmissionGear(),N2kTG_Neutral);QCOMPARE(w.TransmissionGearStbd(),N2kTG_Neutral);
  auto *sync=w.findChild<QCheckBox*>("checkBoxSyncThrottle");if(sync->isChecked())sync->click();
  g->axisMoved(1,-32768);QCOMPARE(w.ThrottlePosition(),100.0);QCOMPARE(w.ThrottlePositionStbd(),0.0);
  for(int v:{-1000,0,1000,4096,-4096})g->axisMoved(1,v);QCOMPARE(w.ThrottlePosition(),100.0);
  g->axisMoved(1,32767);QCOMPARE(w.ThrottlePosition(),0.0);g->axisMoved(3,-32768);QCOMPARE(w.ThrottlePositionStbd(),100.0);
  g->axisMoved(0,-32768);QVERIFY(qAbs(Convert::RadiansToDegrees(w.RudderPosition())+22.5)<.01);g->axisMoved(2,32767);QCOMPARE(w.RudderPosition(),0.0);
  g->axisMoved(2,0);sync->click();g->axisMoved(0,-32768);QVERIFY(qAbs(Convert::RadiansToDegrees(w.RudderPosition())+45)<.01);
  g->axisMoved(2,1000);QVERIFY(qAbs(Convert::RadiansToDegrees(w.RudderPosition())+45)<.01);
  g->axisMoved(0,0);QCOMPARE(w.RudderPosition(),0.0);
  g->axisMoved(1,-32768);QCOMPARE(w.ThrottlePosition(),100.0);QCOMPARE(w.ThrottlePositionStbd(),100.0);g->axisMoved(3,32767);QCOMPARE(w.ThrottlePositionStbd(),100.0);
  w.setLatitude(42);w.setLongitude(-70);w.findChild<QCheckBox*>("checkBoxManualMovementEnabled")->click();
  g->buttonPressed(3);g->buttonPressed(1);g->axisMoved(2,32767);g->axisMoved(0,1000);
  QVERIFY(QMetaObject::invokeMethod(w.findChild<MoveVesselSimulator*>(),"updateVesselPosition",Qt::DirectConnection));
  QVERIFY(w.Heading()>0);QVERIFY(w.SOG()>0);QVERIFY(w.Longitude()!=-70);
 }
 void disableDisconnectAndStop(){
  GeneratorVesselMovement w(1,"Vessel Movement",nullptr);w.setMyVessel(MyVessel{});w.setLatitude(42);w.setLongitude(-70);enable(w);
  auto *g=w.findChild<GameController*>();auto *manual=w.findChild<QCheckBox*>("checkBoxManualMovementEnabled");auto *enabled=w.findChild<QCheckBox*>("checkBoxGamePadEnabled");
  manual->click();g->buttonPressed(3);g->buttonPressed(1);g->axisMoved(1,-32768);g->axisMoved(3,-32768);
  auto *model=w.findChild<MoveVesselSimulator*>();QVERIFY(QMetaObject::invokeMethod(model,"updateVesselPosition",Qt::DirectConnection));QVERIFY(w.SOG()>0);QVERIFY(w.Latitude()!=42);
  g->disconnected();QVERIFY(!enabled->isChecked());QVERIFY(!manual->isChecked());QCOMPARE(w.SOG(),0.0);QCOMPARE(w.ThrottlePosition(),0.0);QCOMPARE(w.ThrottlePositionStbd(),0.0);QCOMPARE(w.RudderPosition(),0.0);QCOMPARE(w.TransmissionGear(),N2kTG_Neutral);
  g->axisMoved(1,-32768);QCOMPARE(w.ThrottlePosition(),0.0);enable(w);manual->click();g->axisMoved(1,-32768);manual->click();QVERIFY(!enabled->isChecked());QCOMPARE(w.ThrottlePosition(),0.0);
  enable(w);g->axisMoved(1,-32768);enabled->click();QCOMPARE(w.ThrottlePosition(),0.0);
 }
 void refreshAndAutopilotExclusion(){
  GeneratorVesselMovement w(1,"Vessel Movement",nullptr);enable(w);auto *g=w.findChild<GameController*>();auto *speed=w.findChild<QComboBox*>("comboBoxSpeedSource");int count=speed->count();
  w.findChild<QPushButton*>("pushButtonRefreshGameList")->click();QCOMPARE(speed->count(),count);QVERIFY(!w.findChild<QCheckBox*>("checkBoxGamePadEnabled")->isChecked());
  QTemporaryDir routeDir;QVERIFY(routeDir.isValid());QFile route(routeDir.filePath("route.gpx"));QVERIFY(route.open(QIODevice::WriteOnly));
  route.write("<gpx><rte><rtept lat='42' lon='-70'/><rtept lat='42.01' lon='-70'/></rte></gpx>");route.close();
  enable(w);w.findChild<QLineEdit*>("lineEditFilePath")->setText(route.fileName());auto *ap=w.findChild<QCheckBox*>("checkBoxAutoPilotEnabled");ap->click();QVERIFY(ap->isChecked());QVERIFY(!w.findChild<QCheckBox*>("checkBoxGamePadEnabled")->isChecked());
  double throttle=w.ThrottlePosition();g->axisMoved(1,-32768);QCOMPARE(w.ThrottlePosition(),throttle);ap->click();
 }
};
int main(int argc,char **argv){QApplication app(argc,argv);QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Vessel/NumEngines",2);settings.sync();QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));GamepadApplicationTests tests;return QTest::qExec(&tests,argc,argv);}
#include "gamepad_application_tests.moc"
