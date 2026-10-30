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
#include "nmea0183handler.h"
#include "NMEA0183Messages.h"
#include <QFile>
#include <QScopeGuard>
#include <ctime>

class FakeSerialPort : public QSerialPort {
public:
 QByteArray output, input; qint64 result = -2, pending = 0; bool failOpen=false;
 bool open(OpenMode mode) override { return !failOpen && QIODevice::open(mode | QIODevice::Unbuffered); }
 void close() override { QIODevice::close(); pending=0; input.clear(); }
 qint64 bytesToWrite() const override { return pending; }
qint64 bytesAvailable() const override { return input.size(); }
 void feed(const QByteArray &data) { input+=data; emit readyRead(); }
protected:
 qint64 readData(char *data, qint64 max) override { const auto n=qMin(max,qint64(input.size())); memcpy(data,input.constData(),n); input.remove(0,n); return n; }
 qint64 writeData(const char *data, qint64 size) override {
  const qint64 n=result==-2?size:qMin(result,size);
  if(n>0) { output.append(data,n); pending+=n; }
  return n;
 }
};
class HandlerTests : public QObject {
 Q_OBJECT
static QByteArray frame(const QByteArray &body) {
 unsigned char checksum=0; for(unsigned char c:body) checksum^=c;
 return "$"+body+"*"+QByteArray::number(checksum,16).rightJustified(2,'0').toUpper()+"\r\n";
}
static QStringList parameters(const QString &code) {
 if(code=="GGA") return {"GPSTime:45296.5","Latitude:-42.5","Longitude:70.25","GPSQualityIndicator:2","SatelliteCount:12","HDOP:0.8","Altitude:-10.5","GeoidalSeparation:-25.5","DGPSAge:1.5","DGPSReferenceStationID:42"};
 if(code=="GLL") return {"GPSTime:45296.5","Latitude:-42.5","Longitude:70.25","Status:A"};
 if(code=="RMC") return {"GPSTime:45296.5","Status:A","Latitude:-42.5","Longitude:70.25","TrueCOG:2.1537362969610028","SOG:5.144444444","DaysSince1970:20000","Variation:-0.1832595714594046","DateTime:0"};
 if(code=="HDG") return {"Heading:2.1537362969610028","Deviation:-0.04363323129985824","Variation:0.1832595714594046"};
 if(code=="HDT") return {"Heading:2.1537362969610028"};
 if(code=="VTG") return {"TrueCOG:2.1537362969610028","MagneticCOG:2.0943951023931953","SOG:5.144444444"};
 return {"GPSTime:45296.5","GPSDay:13","GPSMonth:10","GPSYear:2026","LZD:0","LZMD:0"};
}
private slots:
 void rmcUtcDate_data() {
  QTest::addColumn<QByteArray>("zone");
  QTest::newRow("utc")<<QByteArray("UTC0");
  QTest::newRow("west")<<QByteArray("EST5EDT");
  QTest::newRow("east")<<QByteArray("JST-9");
 }
 void rmcUtcDate() {
  QFETCH(QByteArray,zone);
  const bool hadZone=qEnvironmentVariableIsSet("TZ");const QByteArray original=qgetenv("TZ");
  const auto refresh=[] {
#ifdef Q_OS_WIN
   _tzset();
#else
   tzset();
#endif
  };
  const auto restore=qScopeGuard([&] {if(hadZone)qputenv("TZ",original);else qunsetenv("TZ");refresh();});
  qputenv("TZ",zone);refresh();
  auto *port=new FakeSerialPort;Nmea0183Handler h(nullptr,port);QVERIFY(h.Initialize("M25_FAKE"));QVERIFY(h.OpenDevice());
  auto params=parameters("RMC");params[6]="DaysSince1970:20741";
  QVERIFY(h.sendSnt("RMC",params));QVERIFY(port->output.contains(",151026,"));
  QSignalSpy received(&h,&Nmea0183Handler::RcvdSnt);h.processIncomingData(port->output);QCOMPARE(received.count(),1);
  const auto values=received.first()[1].toStringList();
  QCOMPARE(values[values.indexOf("DaysSince1970")+1].toULong(),20741ul);
  QCOMPARE(values[values.indexOf("DateTime")+1].toLongLong(),QDateTime(QDate(2026,10,15),QTime(12,34,56),QTimeZone::utc()).toSecsSinceEpoch());
 }
 void applicationSentences() {
  auto *port=new FakeSerialPort; Nmea0183Handler handler(nullptr,port);
  QVERIFY(handler.Initialize("M21A_FAKE")); QVERIFY(handler.OpenDevice());
  const QList<QByteArray> bodies={
   "GPGGA,123456.50,4230.000,S,07015.000,E,2,12,0.8,-10.5,M,-25.5,M,1.5,42",
   "GPGLL,4230.000,S,07015.000,E,123456.50,A",
   "GPRMC,123456.50,A,4230.000,S,07015.000,E,10.0,123.4,041024,10.5,W",
   "HCHDG,123.4,2.5,W,10.5,E", "GPHDT,123.4,T",
   "GPVTG,123.4,T,120.0,M,10.0,N,18.5,K", "GPZDA,123456.50,13,10,2026,00,00"};
  QSignalSpy received(&handler,&Nmea0183Handler::RcvdSnt);
  for(const auto &body:bodies) {
   const QString code=QString::fromLatin1(body.mid(2,3)); port->output.clear();
   QVERIFY2(handler.sendSnt(code,parameters(code)),qPrintable(code));
   QCOMPARE(port->output,frame(body)); QVERIFY(port->output.size()<=82);
   handler.processIncomingData(port->output);
   QCOMPARE(received.last()[0].toString(),code);
   QCOMPARE(received.last()[2].toString(),QString::fromLatin1(body.left(2)));
  }
  QCOMPARE(received.count(),7);
  const auto rmc=received[2][1].toStringList(); QCOMPARE(rmc[rmc.indexOf("Status")+1],QString("A"));
 }
 void gllValidityAndFractionalTimes() {
  auto *port=new FakeSerialPort;Nmea0183Handler h(nullptr,port);QVERIFY(h.Initialize("M236B_FAKE"));QVERIFY(h.OpenDevice());
  for(const QString status:{"A","V"}) {
   auto params=parameters("GLL");params[3]="Status:"+status;
   QVERIFY(h.sendSnt("GLL",params));QVERIFY(port->output.contains(","+status.toLatin1()+"*"));
   QSignalSpy received(&h,&Nmea0183Handler::RcvdSnt);h.processIncomingData(port->output);QCOMPARE(received.size(),1);
   const auto decoded=received.first()[1].toStringList();QCOMPARE(decoded[decoded.indexOf("Status")+1],status);port->output.clear();
  }
  auto params=parameters("GLL");params[0]="GPSTime:";QVERIFY(h.sendSnt("GLL",params));QVERIFY(port->output.contains(",V*"));port->output.clear();
  params=parameters("GLL");params[3]="Status:X";QVERIFY(!h.sendSnt("GLL",params));QVERIFY(port->output.isEmpty());
  for(const QByteArray body:{QByteArray("GPGLL,4230.000,S,07015.000,E,120000.1234,V"),QByteArray("GPGGA,120000.1234,4230.000,S,07015.000,E,2,12,0.8,-10.5,M,-25.5,M,1.5,42"),QByteArray("GPRMC,120000.1234,A,4230.000,S,07015.000,E,10.0,123.4,041024,10.5,W")}) {
   QSignalSpy received(&h,&Nmea0183Handler::RcvdSnt);h.processIncomingData(frame(body));QCOMPARE(received.size(),1);
   const auto decoded=received.first()[1].toStringList();QVERIFY(qAbs(decoded[decoded.indexOf("GPSTime")+1].toDouble()-43200.1234)<1e-9);
  }
 }
 void writesAndLifecycle() {
  auto *port=new FakeSerialPort; Nmea0183Handler h(nullptr,port);
  QVERIFY(!h.OpenDevice()); QVERIFY(!h.Initialize(""));
  QVERIFY(h.Initialize("M21A_FAKE"));
  port->failOpen=true; QVERIFY(!h.OpenDevice());port->failOpen=false;
  QSignalSpy errors(&h,&Nmea0183Handler::ShowStatusBarMessage);
  for(qint64 n:{qint64(-1),qint64(0),qint64(3)}) {
   QVERIFY(h.OpenDevice());port->result=n;
   QVERIFY(!h.SendNmea0183Message(QString::fromLatin1(frame("GPHDT,123.4,T"))));
   QVERIFY(!h.IsOpen()); QVERIFY(!errors.isEmpty());
  }
  port->result=-2;
  for(int i=0;i<4;++i) {
   QVERIFY(h.OpenDevice()); QVERIFY(h.OpenDevice());
   QVERIFY(h.SendNmea0183Message(QString::fromLatin1(frame("GPHDT,123.4,T"))));
   QVERIFY(port->bytesToWrite()>0); // accepted asynchronously, not physical delivery
   h.processIncomingData("$GPHDT,12"); QVERIFY(!h.buffer.isEmpty());
   QVERIFY(h.CloseDevice()); QVERIFY(h.buffer.isEmpty()); QVERIFY(!h.IsOpen());
  }
  QVERIFY(h.OpenDevice());port->pending=4096;
  QVERIFY(!h.SendNmea0183Message(QString::fromLatin1(frame("GPHDT,123.4,T"))));
  emit port->errorOccurred(QSerialPort::ResourceError); QVERIFY(!h.IsOpen());
 }
 void readyReadAndModeReset() {
  auto *p=new FakeSerialPort;Nmea0183Handler h(nullptr,p);QVERIFY(h.Initialize("M21A_FAKE"));QVERIFY(h.OpenDevice());
  QSignalSpy received(&h,&Nmea0183Handler::RcvdSnt); const auto good=frame("GPHDT,123.4,T");
  p->feed(good);QCOMPARE(received.count(),0);QVERIFY(p->input.isEmpty());
  h.setMode(Viewer);p->feed(good.left(5));QCOMPARE(received.count(),0);p->feed(good.mid(5)+good);QCOMPARE(received.count(),2);
  p->feed(good.left(5));h.setMode(Sender);h.setMode(Viewer);p->feed(good.mid(5));QCOMPARE(received.count(),2);
  p->feed(good);QCOMPARE(received.count(),3);
 }
 void receiveFraming() {
  Nmea0183Handler h; QSignalSpy received(&h,&Nmea0183Handler::RcvdSnt);
  const auto good=frame("GPHDT,123.4,T");
  for(int split=0;split<=good.size();++split) {
   received.clear();h.processIncomingData(good.left(split));h.processIncomingData(good.mid(split)); QCOMPARE(received.count(),1);
  }
  received.clear();h.processIncomingData(good+good+good);QCOMPARE(received.count(),3);
  received.clear();h.processIncomingData("garbage$GPHDT,broken"+good);QCOMPARE(received.count(),1);
  received.clear();h.processIncomingData(QByteArray(100000,'x')+"$"+QByteArray(100000,'x')+good);QCOMPARE(received.count(),1);QVERIFY(h.buffer.isEmpty());
  received.clear();
  for(const auto &bad : {QByteArray("$GPHDT,123.4,T*00\r\n"),frame("GPHDT,nan,T"),frame("GPHDT,12,Q"),frame("GPRMC,1,A"),frame("GPHDT,123.4,T")+QByteArray()}) {
   if(bad==good) continue;
   h.processIncomingData(bad+good);
  }
  QCOMPARE(received.count(),4);
  h.processIncomingData("$GPHDT,12");h.CloseDevice();received.clear();h.processIncomingData("3.4,T*31\r\n"+good);QCOMPARE(received.count(),1);
 }
 void rmcValidityAndUnavailable() {
  auto *p=new FakeSerialPort;Nmea0183Handler h(nullptr,p);QVERIFY(h.Initialize("M21A_FAKE"));QVERIFY(h.OpenDevice());
  auto params=parameters("RMC");params[1]="Status:V";QVERIFY(h.sendSnt("RMC",params));QVERIFY(p->output.contains(",V,"));
  p->output.clear();params[1]="Status:A";params[2]="Latitude:";QVERIFY(h.sendSnt("RMC",params));QVERIFY(p->output.contains(",V,,,"));
  params[1]="Status:";QVERIFY(!h.sendSnt("RMC",params));
  params=parameters("RMC");params[0]="broken";QVERIFY(!h.sendSnt("RMC",params));
  params=parameters("RMC");params[0]="GPSTime:nan";QVERIFY(!h.sendSnt("RMC",params));
  QSignalSpy received(&h,&Nmea0183Handler::RcvdSnt);
  h.processIncomingData(frame("GPRMC,,V,,,,,,,031024,,"));QCOMPARE(received.count(),1);
  const auto pairs=received.first()[1].toStringList();QCOMPARE(pairs[pairs.indexOf("SOG")+1].toDouble(),NMEA0183DoubleNA);
  for(const auto &bad:{"GPRMC,123456,A,9999,N,07015,E,1,2,031024,,", "GPRMC,123456,Q,4230,S,07015,E,1,2,031024,,", "GPRMC,123456,A,4230,S,07015,E,1,2,321024,,"})h.processIncomingData(frame(bad));
  QCOMPARE(received.count(),1);
 }
 void checksumPaddingAndLimits() {
  Nmea0183Handler h;bool found=false;
  for(int i=0;i<3600 && !found;++i) {
   tNMEA0183Msg msg;QVERIFY(NMEA0183SetHDT(msg,i*0.1*M_PI/180));
   if(msg.GetCheckSum()<16) {found=true;const auto wire=h.constructMsgToSend(msg);QVERIFY(wire.contains("*0"));QVERIFY(h.validateChecksum(wire.chopped(2)));}
  }
  QVERIFY(found);
  QVERIFY(!h.validateChecksum("$GPHDT,123.4,T*1"));QVERIFY(!h.validateChecksum("$GPHDT,123.4,T*31*31"));
  QVERIFY(!h.validateChecksum(QString::fromLatin1(frame("GPHDT,"+QByteArray(90,'1')+",T").chopped(2))));
 }

 void hdgIntegration() {
  Nmea0183Handler handler; QVERIFY(!handler.IsOpen());
  QSignalSpy received(&handler,&Nmea0183Handler::RcvdSnt);
  tNMEA0183Msg msg; QVERIFY(msg.Init("HDG","GP"));
  for(const char *field : {"123.4","2.5","W","10.5","E"}) QVERIFY(msg.AddStrField(field));
  QVERIFY(handler.RcvdSntHDG(msg)); QCOMPARE(received.count(),1);
  auto pairs=received.first()[1].toStringList();
  QCOMPARE(pairs[0],QString("Heading")); QCOMPARE(pairs[2],QString("Deviation")); QCOMPARE(pairs[4],QString("Variation"));
  QVERIFY(qAbs(pairs[1].toDouble()-123.4*M_PI/180)<1e-5);
  QVERIFY(qAbs(pairs[3].toDouble()+2.5*M_PI/180)<1e-6);
  QVERIFY(qAbs(pairs[5].toDouble()-10.5*M_PI/180)<1e-6);
 }
 void closedPortError() {
  Nmea0183Handler handler; QVERIFY(!handler.IsOpen());

  QVERIFY(!handler.SendNmea0183Message("$GPHDT,123.4,T*31\r\n"));
 }
};
QTEST_GUILESS_MAIN(HandlerTests)
#include "nmea0183_handler_tests.moc"
