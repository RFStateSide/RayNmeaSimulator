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
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include "mainwindow.h"
#include "dialogsetup.h"
#include "appsettings.h"
#include "nmea2000handler.h"
#include "slcancodec.h"
#include "serialportdiscovery.h"
class TxPort : public QSerialPort {
public:
    QByteArray output, input;
    qint64 chunk = 10000;
    bool acknowledge = true, failWrite = false;
    bool open(OpenMode mode) override { return QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { input.clear(); QIODevice::close(); }
    qint64 bytesAvailable() const override { return input.size(); }
    void feed(const QByteArray &bytes) { input += bytes; emit readyRead(); }
protected:
    qint64 readData(char *data, qint64 n) override {
        n = qMin(n, qint64(input.size())); memcpy(data, input.constData(), n); input.remove(0, n); return n;
    }
    qint64 writeData(const char *data, qint64 n) override {
        if (failWrite) return -1;
        n = qMin(n, chunk); output.append(data, n);
        if (n && acknowledge) QTimer::singleShot(0, this, [this, n] { emit bytesWritten(n); });
        return n;
    }
};

class UsbCanSetupTests : public QObject {
 Q_OBJECT
 QTemporaryDir profile{QDir::currentPath()+"/build/m231j-profile-XXXXXX"};
 void configure(bool canable) {
  AppSettings s(APP_COMPANY,APP_NAME);s.clear();
  s.setValue("Nmea2000/Nmea2000Enabled",true);
  s.setValue("Nmea2000/InterfaceType",canable?"CANable SLCAN":"Waveshare USB-CAN-A");
  for(const auto &prefix:QStringList{"Canable","Waveshare"}) {
   s.setValue("Nmea2000/"+prefix+"Port",prefix=="Canable"?"memory-canable":"memory-waveshare");
   s.setValue("Nmea2000/"+prefix+"UniqueNumber",prefix=="Canable"?123456:654321);
   s.setValue("Nmea2000/"+prefix+"PreferredSource",prefix=="Canable"?44:45);
  } s.sync();
 }
 void rows() { QTest::addColumn<bool>("canable");QTest::newRow("Waveshare")<<false;QTest::newRow("CANable")<<true; }
 QString prefix(bool canable) { return canable?"canable":"waveshare"; }
 QPushButton *button(DialogSetup &s,bool c) { return s.findChild<QPushButton*>(prefix(c)+"Reconnect"); }
 QByteArray config(bool c) { return c?SlcanCodec::configuration():WaveshareCodec::receiveConfiguration(); }
private slots:
 void initTestCase() { QVERIFY(profile.isValid());qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());qputenv("RAYNMEA_TEST_CREDENTIALS","1");QApplication::setStyle("Fusion"); }
 void discoveredSelectionSavesPathAndRefreshPreservesDrafts() {
  configure(false); DialogSetup s; s.show();
  auto *port=s.findChild<QComboBox*>("wavesharePort");
  const QString path="/dev/serial/by-id/fixture-stable-device";
  SerialPortDiscovery::refresh(port, {{path,"USB serial","Fixture maker",true,"abcd:1234"}},false);
  port->setCurrentIndex(1); QCOMPARE(port->currentText(),path);
  QVERIFY(QMetaObject::invokeMethod(&s,"on_pushButtonSave_clicked"));
  {AppSettings saved(APP_COMPANY,APP_NAME);QCOMPARE(saved.value("Nmea2000/WavesharePort").toString(),path);}
  QSignalSpy reload(&s,&DialogSetup::ReloadSettings);
  s.findChild<QPushButton*>("refreshSerialPorts")->click(); QCOMPARE(port->currentText(),path);
  s.findChild<QCheckBox*>("showAllSerialPorts")->setChecked(true); QCOMPARE(port->currentText(),path);
  QCOMPARE(reload.size(),0);
  auto *actisense=s.findChild<QComboBox*>("comboBoxNmea2000ActisensePort");
  QVERIFY(actisense->isEditable()); actisense->setCurrentText("COM88");
  s.findChild<QPushButton*>("refreshSerialPorts")->click(); QCOMPARE(actisense->currentText(),QString("COM88"));
  QVERIFY(QMetaObject::invokeMethod(&s,"on_pushButtonSave_clicked"));
  {AppSettings saved(APP_COMPANY,APP_NAME);QCOMPARE(saved.value("Nmea2000/ActisensePort").toString(),QString("COM88"));}
  s.reject();s.show();QCOMPARE(port->currentText(),path);s.reject();
 }
 void selectedDraftCannotConnectAnotherDevice_data() { rows(); }
 void selectedDraftCannotConnectAnotherDevice() {
  QFETCH(bool,canable);configure(canable);DialogSetup s;s.show();
  QSignalSpy connectRequested(&s,&DialogSetup::ConnectSerialCanDevice);
  auto *b=button(s,canable);QCOMPARE(b->text(),QString("Connect Selected Device"));
  b->click();QCOMPARE(connectRequested.size(),1);
  auto *port=s.findChild<QComboBox*>(prefix(canable)+"Port");auto original=port->currentText();
  port->setCurrentText("unsaved");b->click();QCOMPARE(connectRequested.size(),1);
  QVERIFY(s.findChild<QLabel*>(prefix(canable)+"ConnectionStatus")->text().contains("Save"));
  port->setCurrentText(original);
  auto *identity=s.findChild<QSpinBox*>(prefix(canable)+"Identity");auto n=identity->value();identity->setValue(n+1);b->click();QCOMPARE(connectRequested.size(),1);identity->setValue(n);
  auto *source=s.findChild<QSpinBox*>(prefix(canable)+"Source");auto a=source->value();source->setValue(a+1);b->click();QCOMPARE(connectRequested.size(),1);source->setValue(a);
  s.findChild<QCheckBox*>("checkBoxNmea2000Enabled")->setChecked(false);b->click();QCOMPARE(connectRequested.size(),1);
  s.reject();s.show();b->click();QCOMPARE(connectRequested.size(),2);s.reject();
 }
 void independentSaveCancelReload_data() { rows(); }
 void independentSaveCancelReload() {
  QFETCH(bool,canable);configure(canable);DialogSetup s;s.show();
  auto *port=s.findChild<QComboBox*>(prefix(canable)+"Port");auto original=port->currentText();port->setCurrentText("draft");s.reject();s.show();QCOMPARE(port->currentText(),original);
  port->setCurrentText("saved-port");QVERIFY(QMetaObject::invokeMethod(&s,"on_pushButtonSave_clicked"));
  AppSettings settings(APP_COMPANY,APP_NAME);auto savedPrefix=canable?"Canable":"Waveshare",other=canable?"Waveshare":"Canable";
  QCOMPARE(settings.value(QString("Nmea2000/")+savedPrefix+"Port").toString(),QString("saved-port"));
  QCOMPARE(settings.value(QString("Nmea2000/")+other+"Port").toString(),canable?QString("memory-waveshare"):QString("memory-canable"));
  port->setCurrentText("discard-on-reload");QVERIFY(QMetaObject::invokeMethod(&s,"on_pushButtonLoad_clicked"));QCOMPARE(port->currentText(),QString("saved-port"));
  QSignalSpy requested(&s,&DialogSetup::ConnectSerialCanDevice);button(s,canable)->click();QCOMPARE(requested.size(),1);s.reject();
 }
 void connectGenerateViewerStopAndExplicitTx_data() { rows(); }
 void connectGenerateViewerStopAndExplicitTx() {
  QFETCH(bool,canable);configure(canable);TxPort *p=nullptr;int opened=0;
  MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){++opened;p=new TxPort;p->setParent(o);return p;});w.show();
  auto *start=w.findChild<QAction*>("actionStart_Sending");auto *stop=w.findChild<QAction*>("actionStop_Sending");
  w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();auto *s=w.findChild<DialogSetup*>();auto *b=button(*s,canable);
  auto *session=w.findChild<WaveshareSession*>();QVERIFY(session);QCOMPARE(opened,0);
  b->click();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);QCOMPARE(p->output,config(canable));QVERIFY(!start->isChecked());QVERIFY(!session->isTransmitting());
  auto old=QPointer<TxPort>(p);b->click();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);QCOMPARE(opened,2);QVERIFY(!old || !old->isOpen());QCOMPARE(p->output,config(canable));
  start->trigger();QTRY_VERIFY(start->isChecked());QVERIFY(session->isTransmitting());auto count=opened;b->click();QCOMPARE(opened,count);QVERIFY(s->findChild<QLabel*>(prefix(canable)+"ConnectionStatus")->text().contains("Stop Sending"));
  QTest::keyClick(s,Qt::Key_F12);QVERIFY(!start->isChecked());QVERIFY(!p->isOpen());
  b->click();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);QCOMPARE(p->output,config(canable));QVERIFY(!start->isChecked());
  w.findChild<QAction*>("actionViewer_Rx")->setChecked(true);QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);QVERIFY(!start->isEnabled());QCOMPARE(p->output,config(canable));
  QVERIFY(!stop->isEnabled());QTest::keyClick(s,Qt::Key_F12);QVERIFY(!p->isOpen());b->click();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);QCOMPARE(p->output,config(canable));
  w.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);QVERIFY(!p->isOpen());QVERIFY(!start->isChecked());b->click();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);QCOMPARE(p->output,config(canable));s->reject();
 }
 void protocolSwitchUsesSavedSelection_data() { rows(); }
 void waveshareSavedLinuxPathExplicitScheduledTx() {
  configure(false);
  { AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Nmea2000/WavesharePort","None"); }
  TxPort *p=nullptr;int opened=0;
  MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){++opened;p=new TxPort;p->setParent(o);return p;});
  w.show();QCOMPARE(opened,0);
  auto *start=w.findChild<QAction*>("actionStart_Sending");
  auto *model=w.findChild<PgnTableModel*>();QVERIFY(model);model->clearModel();
  model->addPgn(99031,"Linux serial path fixture","129025",true,
      {{"Latitude","42","deg","double"},{"Longitude","-70","deg","double"}}, {},250);
  w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();auto *s=w.findChild<DialogSetup*>();
  auto *type=s->findChild<QComboBox*>("comboBoxNmea2000Interface");
  QVERIFY(type->findText("Waveshare USB-CAN-A")>=0);QCOMPARE(type->currentText(),QString("Waveshare USB-CAN-A"));
  auto *port=s->findChild<QComboBox*>("wavesharePort");QVERIFY(port->isEditable());
  const QString path="/dev/serial/by-id/usb-Waveshare-software-fixture";
  port->setCurrentText(path);s->findChild<QSpinBox*>("waveshareIdentity")->setValue(321654);
  s->findChild<QSpinBox*>("waveshareSource")->setValue(47);
  button(*s,false)->click();QCOMPARE(opened,0); // Draft cannot bypass Save.
  QVERIFY(QMetaObject::invokeMethod(s,"on_pushButtonSave_clicked"));
  { AppSettings saved(APP_COMPANY,APP_NAME);QCOMPARE(saved.value("Nmea2000/WavesharePort").toString(),path); }
  button(*s,false)->click();auto *session=w.findChild<WaveshareSession*>();
  QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);
  // Qt normalizes Linux names by stripping /dev/; the saved path stays absolute.
  QVERIFY(p->portName()==path || "/dev/"+p->portName()==path);
  QCOMPARE(p->output,WaveshareCodec::receiveConfiguration());
  QTest::qWait(300);QCOMPARE(p->output,WaveshareCodec::receiveConfiguration());QVERIFY(!start->isChecked());
  start->trigger();QTRY_VERIFY(session->isTransmitting());QVERIFY(start->isChecked());
  QCOMPARE(p->output.left(20),WaveshareCodec::transmitConfiguration());
  const auto frames=[&](quint32 wanted) {
   WaveshareCodec::Parser parser;QList<WaveshareCodec::CanFrame> found;
   for(const auto &f:parser.feed(p->output.mid(20))) {
    auto pgn=(f.id>>8)&0x3ffff;if(((f.id>>16)&255)<240)pgn&=0x3ff00;
    if(pgn==wanted)found.append(f);
   } return found;
  };
  QTRY_VERIFY(!frames(129025).isEmpty());const auto position=frames(129025).first();
  QCOMPARE(position.id,quint32((2u<<26)|(129025u<<8)|47u));QCOMPARE(position.length,quint8(8));
  QCOMPARE(QByteArray(reinterpret_cast<const char*>(position.data.data()),8),QByteArray::fromHex("00b1081900d946d6"));
  QVERIFY(!frames(60928).isEmpty());const auto claim=frames(60928).first();quint64 name=0;
  for(int i=0;i<8;++i)name|=quint64(claim.data[i])<<(8*i);
  QCOMPARE(name&0x1fffff,quint64(321654));
  p->chunk=0;QTest::keyClick(s,Qt::Key_F12);QVERIFY(!p->isOpen());QVERIFY(!start->isChecked());
  button(*s,false)->click();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);
  QCOMPARE(p->output,WaveshareCodec::receiveConfiguration());QVERIFY(!start->isChecked());s->reject();
 }
 void protocolSwitchUsesSavedSelection() {
  QFETCH(bool,canable);configure(canable);TxPort *p=nullptr;int opened=0;MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){++opened;p=new TxPort;p->setParent(o);return p;});
  w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();auto *s=w.findChild<DialogSetup*>();auto *type=s->findChild<QComboBox*>("comboBoxNmea2000Interface");
  type->setCurrentText(canable?"Waveshare USB-CAN-A":"CANable SLCAN");button(*s,!canable)->click();QCOMPARE(opened,0);
  QVERIFY(QMetaObject::invokeMethod(s,"on_pushButtonSave_clicked"));button(*s,!canable)->click();auto *session=w.findChild<WaveshareSession*>();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);
  QCOMPARE(session->protocol(),canable?WaveshareSession::Waveshare:WaveshareSession::CanableSlcan);QCOMPARE(p->output,config(!canable));QCOMPARE(p->portName(),canable?QString("memory-waveshare"):QString("memory-canable"));QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isChecked());s->reject();
 }
 void cancelPreservesConnectionSaveReconfigures_data() { rows(); }
 void cancelPreservesConnectionSaveReconfigures() {
  QFETCH(bool,canable);configure(canable);TxPort *p=nullptr;int opened=0;MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){++opened;p=new TxPort;p->setParent(o);return p;});
  w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();auto *s=w.findChild<DialogSetup*>();button(*s,canable)->click();auto *session=w.findChild<WaveshareSession*>();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);
  auto old=QPointer<TxPort>(p);s->findChild<QComboBox*>(prefix(canable)+"Port")->setCurrentText("draft");s->reject();QVERIFY(old && old->isOpen());QCOMPARE(opened,1);
  w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();s->findChild<QComboBox*>(prefix(canable)+"Port")->setCurrentText("replacement");QVERIFY(QMetaObject::invokeMethod(s,"on_pushButtonSave_clicked"));QVERIFY(!old || !old->isOpen());QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isChecked());
  button(*s,canable)->click();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);QCOMPARE(p->portName(),QString("replacement"));QCOMPARE(p->output,config(canable));s->reject();
 }
 void connectedGenerateDoesNotConsumeViewerData_data() { rows(); }
 void connectedGenerateDoesNotConsumeViewerData() {
  QFETCH(bool,canable);TxPort *p=nullptr;Nmea2000Handler h(nullptr,nullptr,nullptr,[&](QObject *o){p=new TxPort;p->setParent(o);return p;});h.SetInterfaceType(canable?"CANable SLCAN":"Waveshare USB-CAN-A");h.setMode(Sender);h.ConfigureWaveshare("memory",{123456,44});QSignalSpy received(&h,&Nmea2000Handler::RcvdPgn);
  auto *session=h.findChild<WaveshareSession*>();h.ConnectSerialCanDevice();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);
  WaveshareCodec::CanFrame f;f.id=0x09f7092b;f.length=8;f.data={255,0,240,0,0,30,0,1};SlcanCodec::CanFrame sf;sf.id=f.id;sf.length=f.length;sf.data=f.data;
  auto bytes=canable?SlcanCodec::encode(sf):WaveshareCodec::encode(f);p->feed(bytes);QTRY_COMPARE(session->state(),WaveshareSession::Receiving);QTest::qWait(40);QCOMPARE(received.size(),0);QCOMPARE(p->output,config(canable));
  h.setMode(Viewer);QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);p->feed(bytes);QTRY_COMPARE(received.size(),1);QCOMPARE(received.first().at(0).toString(),QString("128777"));QCOMPARE(received.first().at(2).toString(),QString("43"));QCOMPARE(p->output,config(canable));
  h.setMode(Sender);h.StartWaveshareSending();auto pending=QPointer<TxPort>(p);h.ConnectSerialCanDevice();QCOMPARE(p,pending.data());QTRY_VERIFY(h.IsWaveshareReady());h.ConnectSerialCanDevice();QCOMPARE(p,pending.data());QVERIFY(h.IsWaveshareReady());
 }
 void setupPresentation_data() {
  QTest::addColumn<bool>("canable");QTest::addColumn<QString>("theme");QTest::addColumn<int>("points");
  for(bool c:{false,true})for(const auto &theme:QStringList{"Dark","Light"})for(int points:{9,12,16})
   QTest::newRow(qPrintable(QString("%1-%2-%3").arg(c?"CANable":"Waveshare",theme).arg(points)))<<c<<theme<<points;
 }
 void setupPresentation() {
  QFETCH(bool,canable);QFETCH(QString,theme);QFETCH(int,points);configure(canable);
  {AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.setValue("Preference/FontSize",points);}
  MainWindow w(nullptr,nullptr,nullptr,nullptr,[](QObject *o){auto *p=new TxPort;p->setParent(o);return p;});w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();auto *s=w.findChild<DialogSetup*>();s->findChild<QTabWidget*>("tabWidget")->setCurrentIndex(1);s->resize(1100,1000);QTest::qWait(25);
  auto *selector=s->findChild<QComboBox*>("comboBoxNmea2000Interface");auto selected=selector->currentText();
  auto *actisense=s->findChild<QComboBox*>("comboBoxNmea2000ActisensePort");QVERIFY(!actisense->isVisible());selector->setCurrentText("Actisense");QVERIFY(actisense->isVisible());selector->setCurrentText(selected);QVERIFY(!actisense->isVisible());QTest::qWait(20);
  auto *b=button(*s,canable);QVERIFY(b->isVisible());QVERIFY(b->isEnabled());QCOMPARE(s->font().pointSize(),points);QVERIFY(b->width()>=b->fontMetrics().horizontalAdvance(b->text()));
  QString notes;for(auto *label:b->parentWidget()->findChildren<QLabel*>())notes+=label->text();
  QVERIFY(notes.contains(canable?"115200 baud SLCAN":"2 Mbaud serial"));QVERIFY(notes.contains("250 kbps extended CAN"));
  QVERIFY(notes.contains("Connect opens RX/standby mode"));QVERIFY(notes.contains("Enable Sending explicitly"));QVERIFY(notes.contains("STOP/F12 stops the session"));
  QVERIFY(!notes.contains("experimental",Qt::CaseInsensitive));QVERIFY(!notes.contains("unqualified",Qt::CaseInsensitive));
  const auto out=qEnvironmentVariable("M231J_EVIDENCE");if(!out.isEmpty()){QDir().mkpath(out);QVERIFY(s->grab().save(out+QString("/%1-%2-%3.png").arg(prefix(canable),theme).arg(points)));}s->reject();
 }
 void failedConnectDoesNotRetryOrAuthorize_data() {
  QTest::addColumn<bool>("canable");QTest::addColumn<int>("serialError");
  for (bool c : {false,true}) for (int error : {int(QSerialPort::ResourceError),int(QSerialPort::PermissionError),int(QSerialPort::OpenError)})
   QTest::newRow(qPrintable(prefix(c)+"-"+QString::number(error)))<<c<<error;
 }
 void failedConnectDoesNotRetryOrAuthorize() {
  QFETCH(int,serialError);QFETCH(bool,canable);configure(canable);TxPort *p=nullptr;int opened=0;MainWindow w(nullptr,nullptr,nullptr,nullptr,[&](QObject *o){++opened;p=new TxPort;p->setParent(o);return p;});
  w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();auto *s=w.findChild<DialogSetup*>();button(*s,canable)->click();auto *session=w.findChild<WaveshareSession*>();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);
  emit p->errorOccurred(QSerialPort::SerialPortError(serialError));QVERIFY(!p->isOpen());QVERIFY(s->findChild<QLabel*>(prefix(canable)+"ConnectionStatus")->text().contains(serialError==QSerialPort::ResourceError?"disconnected":serialError==QSerialPort::PermissionError?"busy":"permissions"));QVERIFY(w.findChild<QLabel*>("sendingStatus")->toolTip().contains("serial error"));QTest::qWait(300);QCOMPARE(opened,1);QVERIFY(!w.findChild<QAction*>("actionStart_Sending")->isChecked());
  button(*s,canable)->click();QTRY_COMPARE(session->state(),WaveshareSession::AwaitingTraffic);QCOMPARE(opened,2);QCOMPARE(p->output,config(canable));QVERIFY(!w.findChild<QLabel*>("sendingStatus")->toolTip().contains("serial error"));s->reject();
 }
};
QTEST_MAIN(UsbCanSetupTests)
#include "usb_can_setup_tests.moc"
