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
#include "gamecontroller.h"
class GameControllerTests : public QObject {
 Q_OBJECT
private slots:
 void filteredDeviceSelection() {
  GameController c;QVERIFY(c.initialize());
  int other=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,2,2,0);QVERIFY(other>=0);
  int pad=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);QVERIFY(pad>=0);
  auto names=c.availableControllers();QVERIFY(!names.isEmpty());
  int ordinal=0;for(int i=0;i<pad;++i)if(SDL_IsGameController(i))++ordinal;
  QVERIFY(c.open(ordinal));QCOMPARE(c.getAxisCount(),6);c.close();
  SDL_JoystickDetachVirtual(pad);SDL_JoystickDetachVirtual(other);
 }
 void selectedDeviceEventsOnly() {
  GameController c;QVERIFY(c.initialize());int pad=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);QVERIFY(pad>=0);
  int ordinal=0;for(int i=0;i<pad;++i)if(SDL_IsGameController(i))++ordinal;
  c.availableControllers();QVERIFY(c.open(ordinal));QVERIFY(QMetaObject::invokeMethod(&c,"pollEvents",Qt::DirectConnection));
  QSignalSpy axes(&c,&GameController::axisMoved);QSignalSpy buttons(&c,&GameController::buttonPressed);
  SDL_Event e{};e.type=SDL_CONTROLLERAXISMOTION;e.caxis.which=999999;e.caxis.axis=0;e.caxis.value=15000;SDL_PushEvent(&e);
  e={};e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.which=999999;e.cbutton.button=0;SDL_PushEvent(&e);
  QVERIFY(QMetaObject::invokeMethod(&c,"pollEvents",Qt::DirectConnection));QCOMPARE(axes.count(),0);QCOMPARE(buttons.count(),0);
  c.close();SDL_JoystickDetachVirtual(pad);
 }
 void independentSubsystemLifetime() {
  GameController a,b;QVERIFY(a.initialize());QVERIFY(b.initialize());a.disable();QVERIFY(SDL_WasInit(SDL_INIT_GAMECONTROLLER));QVERIFY(!a.isInitialized());
 }
 void neutralArmingAndUnplug() {
  GameController c;QVERIFY(c.initialize());int pad=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);QVERIFY(pad>=0);
  auto *joystick=SDL_JoystickOpen(pad);QVERIFY(joystick);SDL_JoystickSetVirtualAxis(joystick,0,20000);SDL_JoystickSetVirtualButton(joystick,0,1);SDL_JoystickUpdate();
  int ordinal=0;for(int i=0;i<pad;++i)if(SDL_IsGameController(i))++ordinal;c.availableControllers();QVERIFY(c.open(ordinal));
  QSignalSpy axes(&c,&GameController::axisMoved),buttons(&c,&GameController::buttonPressed),removed(&c,&GameController::disconnected);
  auto poll=[&]{QVERIFY(QMetaObject::invokeMethod(&c,"pollEvents",Qt::DirectConnection));};
  poll();QCOMPARE(axes.count(),0);QCOMPARE(buttons.count(),0);
  SDL_JoystickSetVirtualAxis(joystick,0,0);SDL_JoystickSetVirtualButton(joystick,0,0);poll();poll();axes.clear();
  SDL_JoystickSetVirtualAxis(joystick,0,18000);poll();QCOMPARE(axes.count(),1);QCOMPARE(axes.first()[1].toInt(),18000);
  SDL_JoystickDetachVirtual(pad);poll();QCOMPARE(removed.count(),1);QCOMPARE(c.getAxisCount(),0);SDL_JoystickClose(joystick);
  pad=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);poll();QCOMPARE(c.getAxisCount(),0);SDL_JoystickDetachVirtual(pad);
 }
};
QTEST_GUILESS_MAIN(GameControllerTests)
#include "gamecontroller_tests.moc"
