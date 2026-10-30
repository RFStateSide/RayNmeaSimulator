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
#include <QFile>
#include <limits>
#include "nmea0183compat.h"
#include "NMEA0183Messages.h"
class CompatibilityTests : public QObject {
 Q_OBJECT
private slots:
 void decimals() {
  for (double value : {-10.5,123.456,-179.1234,0.125}) {
   tNMEA0183Msg msg; QVERIFY(msg.Init("TST","GP"));
   QVERIFY(msg.AddDoubleField(value,1,"%.4f"));
   QCOMPARE(QString(msg.Field(0)),QString::number(value,'f',4));
  }
  tNMEA0183Msg msg; QVERIFY(msg.Init("TST","GP"));
  QVERIFY(!msg.AddDoubleField(1e100,1,"%.4f"));
 }
 void hdg() {
  for (const char *direction : {"E","W"}) {
   tNMEA0183Msg msg; QVERIFY(msg.Init("HDG","GP"));
   for(const char *field : {"123.4","2.5",direction,"10.5",direction}) QVERIFY(msg.AddStrField(field));
   double h=0,d=0,v=0; QVERIFY(Nmea0183Compat::parseHdg(msg,h,d,v));
   const double sign=direction[0]=='W'?-1:1;
   QVERIFY(qAbs(h-123.4*M_PI/180)<1e-12);
   QVERIFY(qAbs(d-sign*2.5*M_PI/180)<1e-12);
   QVERIFY(qAbs(v-sign*10.5*M_PI/180)<1e-12);
  }
 }
 void hdgEmptyAndInvalid() {
  tNMEA0183Msg msg; msg.Init("HDG","GP");
  for(const char *field : {"90","","","",""}) QVERIFY(msg.AddStrField(field));
  double h=1,d=2,v=3; QVERIFY(Nmea0183Compat::parseHdg(msg,h,d,v));
  QCOMPARE(d,NMEA0183DoubleNA); QCOMPARE(v,NMEA0183DoubleNA);
  for (const char *bad : {"Q","","East"}) {
   msg.Init("HDG","GP");
   for(const char *field : {"90","2.5",bad,"1","E"}) QVERIFY(msg.AddStrField(field));
   h=1;d=2;v=3; QVERIFY(!Nmea0183Compat::parseHdg(msg,h,d,v)); QCOMPARE(h,1.0);QCOMPARE(d,2.0);QCOMPARE(v,3.0);
  }
  for(const char *bad : {"abc","nan","inf"}) {
   msg.Init("HDG","GP"); for(const char *field : {bad,"1","E","1","W"}) msg.AddStrField(field);
   QVERIFY(!Nmea0183Compat::parseHdg(msg,h,d,v));
  }
 }
 void omittedRmcStatus() {
  tNMEA0183Msg msg;
  QVERIFY(NMEA0183SetRMC(msg,45296.5,-42.5,70.25,1,5,20000,0));

  QCOMPARE(msg.FieldCount(),uint8_t(11));
 }
 void mixedRmcFields() {
  for(int mask=0;mask<4;++mask) {
   tNMEA0183Msg msg;
   QVERIFY(NMEA0183SetRMC(msg,45296.5,-42.5,70.25,1,5,20000,0,mask&1?'S':char(0xff),mask&2?'V':char(0xff)));
   QCOMPARE(int(msg.FieldCount()),mask&2?13:mask&1?12:11);
   if(mask) QCOMPARE(QString(msg.Field(11)),mask&1?QString("S"):QString());
   if(mask&2) QCOMPARE(QString(msg.Field(12)),QString("V"));
  }
  tNMEA0183Msg msg;
  QVERIFY(NMEA0183SetRMC(msg,NMEA0183DoubleNA,NMEA0183DoubleNA,NMEA0183DoubleNA,NMEA0183DoubleNA,NMEA0183DoubleNA,20000,NMEA0183DoubleNA));
  QCOMPARE(QString(msg.Field(1)),QString("V"));
  double t,lat,lon,cog,sog,var; char status; unsigned long days;
  QVERIFY(NMEA0183ParseRMC_nc(msg,t,status,lat,lon,cog,sog,days,var));
  QCOMPARE(cog,NMEA0183DoubleNA); QCOMPARE(sog,NMEA0183DoubleNA);
 }
 void sevenSentences() {
  QFile output(qEnvironmentVariable("NMEA0183_SENTENCE_OUTPUT")); QVERIFY(output.open(QIODevice::WriteOnly));
  for(int i=0;i<7;++i) {
   tNMEA0183Msg msg; bool ok=false;
   switch(i) {
    case 0: ok=NMEA0183SetGGA(msg,45296.5,-42.5,70.25,2,12,0.8,-10.5,-25.5,1.5,42); break;
    case 1: ok=NMEA0183SetGLL(msg,45296.5,-42.5,70.25); break;
    case 2: ok=NMEA0183SetHDG(msg,123.4*M_PI/180,-2.5*M_PI/180,10.5*M_PI/180); break;
    case 3: ok=NMEA0183SetHDT(msg,123.4*M_PI/180); break;
    case 4: ok=NMEA0183SetRMC(msg,45296.5,-42.5,70.25,123.4*M_PI/180,5.144444444,20000,-10.5*M_PI/180,'S','S'); break;
    case 5: ok=NMEA0183SetVTG(msg,123.4*M_PI/180,120*M_PI/180,5.144444444); break;
    case 6: ok=NMEA0183SetZDA(msg,45296.5,13,10,2026,0,0); break;
   }
   QVERIFY(ok); char wire[128]; QVERIFY(msg.GetMessage(wire,sizeof(wire)));
   QByteArray line(wire); line+="\r\n"; QCOMPARE(output.write(line),qint64(line.size()));
   tNMEA0183Msg parsed; QVERIFY(parsed.SetMessage(wire)); QCOMPARE(parsed.FieldCount(),msg.FieldCount());
   for(int f=0;f<msg.FieldCount();++f) QCOMPARE(QString(parsed.Field(f)),QString(msg.Field(f)));
  }
 }
};
QTEST_GUILESS_MAIN(CompatibilityTests)
#include "nmea0183_compatibility_tests.moc"
