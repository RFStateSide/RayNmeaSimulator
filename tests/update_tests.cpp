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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QAction>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QDir>
#include <QMap>
#include "updatechecker.h"
#include "updatedialog.h"
#include "mainwindow.h"
#include "helpdialog.h"
#include "appsettings.h"
#include "themes.h"
#include "movevesselsimulator.h"
namespace {
const QString base="https://github.com/RFStateSide/RayNmeaSimulator/releases/";
QJsonObject release(QString v="0.2.0") {return {{"id",12},{"tag_name","v"+v},{"html_url",base+"tag/v"+v},{"draft",false},{"prerelease",false},{"body","Release notes <script>alert(1)</script>"},{"assets",QJsonArray{}}};}
QByteArray json(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
void addAsset(QJsonObject &r,const QString &name,int size=100,const QString &sha=QString(64,'a')){
 auto a=r["assets"].toArray();a.append(QJsonObject{{"name",name},{"state","uploaded"},{"size",size},{"digest","sha256:"+sha},{"browser_download_url",base+"download/"+r["tag_name"].toString()+"/"+name}});r["assets"]=a;
}
QJsonObject manifest(QJsonObject &r,QString key="windows-x86_64",QString glibc="2.36"){
 const QMap<QString,QString> names{{"windows-x86_64","Windows-x64.zip"},{"linux-x86_64","Linux-x86_64.tar.gz"},{"linux-arm64","Linux-aarch64.tar.gz"},{"linux-arm64-debian12","Linux-aarch64-Debian12.tar.gz"}};const QString name="RayNmeaSimulator-0.2.0-"+names.value(key);addAsset(r,name);
 QJsonObject p{{"name",name},{"url",base+"download/v0.2.0/"+name},{"bytes",100},{"sha256",QString(64,'a')},{"required_abi",QJsonObject{{"GLIBC",glibc},{"GLIBCXX","3.4.30"},{"CXXABI","1.3.13"}}}};
 return {{"schema",1},{"repository","RFStateSide/RayNmeaSimulator"},{"version","0.2.0"},{"downloads",QJsonObject{{key,p}}}};
}
struct Response {int status=200;QByteArray data;QUrl redirect;bool hang=false;};
class Reply:public QNetworkReply {
 QByteArray body;qint64 position=0;bool done=false;
public:
 Reply(const QNetworkRequest &r,Response response,QObject *parent):QNetworkReply(parent),body(response.data){setRequest(r);setUrl(r.url());setOperation(QNetworkAccessManager::GetOperation);setAttribute(QNetworkRequest::HttpStatusCodeAttribute,response.status);if(!response.redirect.isEmpty())setAttribute(QNetworkRequest::RedirectionTargetAttribute,response.redirect);open(QIODevice::ReadOnly|QIODevice::Unbuffered);
 if(!response.hang)QTimer::singleShot(0,this,[this]{if(done)return;done=true;emit readyRead();setFinished(true);emit finished();});}
 void abort() override {done=true;setError(OperationCanceledError,"fixture cancellation");setFinished(true);}
 qint64 bytesAvailable() const override{return body.size()-position+QNetworkReply::bytesAvailable();}
 bool isSequential() const override{return true;}
 qint64 readData(char *data,qint64 max) override{const qint64 n=qMin(max,body.size()-position);if(n<=0)return -1;memcpy(data,body.constData()+position,n);position+=n;return n;}
};
class MemoryPort:public QSerialPort {
public:bool open(OpenMode mode) override{return QIODevice::open(mode|QIODevice::Unbuffered);}void close()override{QIODevice::close();}
protected:qint64 writeData(const char*,qint64 size)override{return size;}
};
class Network:public QNetworkAccessManager {
public:
 QList<Response> responses;QList<QNetworkRequest> requests;
 QNetworkReply *createRequest(Operation op,const QNetworkRequest &req,QIODevice *body=nullptr) override {
  Q_UNUSED(body);Q_ASSERT(op==GetOperation);requests<<req;auto response=responses.isEmpty()?Response{500,{},{},false}:responses.takeFirst();return new Reply(req,response,this);
 }
};
}
class UpdateTests:public QObject {
 Q_OBJECT
 QUrl opened;
private slots:
 void capture(const QUrl &url){opened=url;}
 void semver_data(){QTest::addColumn<QString>("a");QTest::addColumn<QString>("b");QTest::addColumn<int>("sign");
  QTest::newRow("numeric")<<"0.10.0"<<"0.9.0"<<1;QTest::newRow("same")<<"0.1.0"<<"0.1.0"<<0;QTest::newRow("older")<<"0.0.9"<<"0.1.0"<<-1;
  QTest::newRow("stable")<<"0.1.0"<<"0.1.0-rc.1"<<1;QTest::newRow("pre-number")<<"0.1.0-beta.10"<<"0.1.0-beta.2"<<1;QTest::newRow("dev")<<"0.1.0-dev"<<"0.1.0"<<-1;QTest::newRow("build")<<"0.1.0+private.123"<<"0.1.0+other"<<0;}
 void semver(){QFETCH(QString,a);QFETCH(QString,b);QFETCH(int,sign);auto c=UpdateChecker::compareVersions(a,b);QVERIFY(c);QCOMPARE(*c==0?0:(*c>0?1:-1),sign);}
 void invalidVersions(){for(const auto &v:{"","0.1","0.1.0junk","01.1.0","0.1.0-beta.01","0.1.0-","0.1.0<script>"})QVERIFY(!UpdateChecker::compareVersions(v,"0.1.0"));}
 void releaseStates(){auto r=release("0.1.0");QCOMPARE(UpdateChecker::parseRelease(json(r),APP_VERSION).state,UpdateResult::Current);r=release("0.0.9");QCOMPARE(UpdateChecker::parseRelease(json(r),APP_VERSION).state,UpdateResult::Older);r=release();QCOMPARE(UpdateChecker::parseRelease(json(r),APP_VERSION).state,UpdateResult::Newer);r["prerelease"]=true;QCOMPARE(UpdateChecker::parseRelease(json(r),APP_VERSION).state,UpdateResult::Unavailable);r=release();r["draft"]=true;QCOMPARE(UpdateChecker::parseRelease(json(r),APP_VERSION).state,UpdateResult::Unavailable);r=release();r["html_url"]="https://github.com/RFStateSide/RayNmeaSimulator-Archive/releases/tag/v0.2.0";QCOMPARE(UpdateChecker::parseRelease(json(r),APP_VERSION).state,UpdateResult::Error);for(const auto &bad:{QByteArray("garbage"),QByteArray("[]"),QByteArray("{}")})QCOMPARE(UpdateChecker::parseRelease(bad,APP_VERSION).state,UpdateResult::Error);}
 void windowsZipSelectionAndIdentityGuards(){
  UpdateHost host{"windows","x86_64",{}, {}, {}};auto r=release();auto m=manifest(r);auto parsed=UpdateChecker::parseRelease(json(r),APP_VERSION);auto selected=UpdateChecker::selectPackage(parsed,json(m),host);
  QCOMPARE(selected.state,UpdateResult::Newer);QCOMPARE(selected.packageName,QString("RayNmeaSimulator-0.2.0-Windows-x64.zip"));QCOMPARE(selected.checksum,QString(64,'a'));
  auto entries=m["downloads"].toObject();auto p=entries["windows-x86_64"].toObject();p["sha256"]=QString(64,'b');entries["windows-x86_64"]=p;auto bad=m;bad["downloads"]=entries;QCOMPARE(UpdateChecker::selectPackage(parsed,json(bad),host).state,UpdateResult::Incompatible);
  p=entries["windows-x86_64"].toObject();p["name"]="RayNmeaSimulator-0.2.0-Windows-x64-Setup.exe";p["url"]=base+"download/v0.2.0/"+p["name"].toString();entries["windows-x86_64"]=p;bad["downloads"]=entries;QCOMPARE(UpdateChecker::selectPackage(parsed,json(bad),host).state,UpdateResult::Incompatible);
  auto other=r;auto assets=other["assets"].toArray();auto asset=assets[0].toObject();asset["browser_download_url"]=base+"download/v9.0.0/"+asset["name"].toString();assets[0]=asset;other["assets"]=assets;auto wrong=UpdateChecker::parseRelease(json(other),APP_VERSION);QCOMPARE(UpdateChecker::selectPackage(wrong,json(m),host).state,UpdateResult::Incompatible);
 }
 void platformMatching(){
  UpdateHost host{"windows","x86_64",{}, {}, {}};auto r=release();auto m=manifest(r);auto parsed=UpdateChecker::parseRelease(json(r),APP_VERSION);QCOMPARE(UpdateChecker::selectPackage(parsed,json(m),host).state,UpdateResult::Newer);
  host.architecture="arm64";QVERIFY(host.platformKey().isEmpty());QCOMPARE(UpdateChecker::selectPackage(parsed,json(m),host).state,UpdateResult::Incompatible);
  host={"linux","arm64","debian","12",{{"GLIBC","2.36"},{"GLIBCXX","3.4.30"},{"CXXABI","1.3.13"}}};QCOMPARE(host.platformKey(),QString("linux-arm64-debian12"));
  r=release();m=manifest(r,"linux-arm64","2.38");parsed=UpdateChecker::parseRelease(json(r),APP_VERSION);QCOMPARE(UpdateChecker::selectPackage(parsed,json(m),host).state,UpdateResult::Incompatible);
  r=release();m=manifest(r,"linux-arm64-debian12","2.36");parsed=UpdateChecker::parseRelease(json(r),APP_VERSION);QCOMPARE(UpdateChecker::selectPackage(parsed,json(m),host).state,UpdateResult::Newer);
  host.abi["GLIBC"]="2.35";QCOMPARE(UpdateChecker::selectPackage(parsed,json(m),host).state,UpdateResult::Incompatible);host.abi["GLIBC"]="2.36";host.abi["GLIBCXX"]="3.4.29";QCOMPARE(UpdateChecker::selectPackage(parsed,json(m),host).state,UpdateResult::Incompatible);
  host={"linux","arm64","debian","13",{{"GLIBC","2.41"},{"GLIBCXX","3.4.33"},{"CXXABI","1.3.15"}}};r=release();m=manifest(r,"linux-arm64","2.38");parsed=UpdateChecker::parseRelease(json(r),APP_VERSION);QCOMPARE(UpdateChecker::selectPackage(parsed,json(m),host).state,UpdateResult::Newer);
  host.architecture="x86_64";QCOMPARE(host.platformKey(),QString("linux-x86_64"));QCOMPARE(UpdateChecker::selectPackage(parsed,json(m),host).state,UpdateResult::Incompatible);
 }
 void hostileMetadata(){UpdateHost host{"windows","x86_64",{},{},{}};auto r=release();auto m=manifest(r);auto parsed=UpdateChecker::parseRelease(json(r),APP_VERSION);
  for(const auto &bad:{"http://github.com/RFStateSide/RayNmeaSimulator/releases/tag/v0.2.0","https://github.com.evil.test/RFStateSide/RayNmeaSimulator/releases/tag/v0.2.0","https://user@github.com/RFStateSide/RayNmeaSimulator/releases/tag/v0.2.0","https://github.com:443/RFStateSide/RayNmeaSimulator/releases/tag/v0.2.0","file:///tmp/a","https://github.com/RFStateSide/RayNmeaSimulator/releases/tag/v0.2.0?x=1"})QVERIFY(!UpdateChecker::officialPage(QUrl(bad),"v0.2.0"));
  for(const QString key:{"repository","version","schema"}){auto bad=m;bad[key]="wrong";QCOMPARE(UpdateChecker::selectPackage(parsed,json(bad),host).state,UpdateResult::Incompatible);}
  for(const QString key:{"url","sha256","bytes","name"}){auto bad=m;auto downloads=bad["downloads"].toObject();auto p=downloads[host.platformKey()].toObject();p[key]="invalid";downloads[host.platformKey()]=p;bad["downloads"]=downloads;QCOMPARE(UpdateChecker::selectPackage(parsed,json(bad),host).state,UpdateResult::Incompatible);}
  auto duplicate=r["assets"].toArray();duplicate.append(duplicate[0]);r["assets"]=duplicate;QCOMPARE(UpdateChecker::selectPackage(UpdateChecker::parseRelease(json(r),APP_VERSION),json(m),host).state,UpdateResult::Incompatible);
 }
 void transportErrors_data(){QTest::addColumn<int>("status");QTest::addColumn<QByteArray>("body");QTest::addColumn<int>("state");QTest::newRow("no-public-release")<<404<<QByteArray("{}")<<int(UpdateResult::Unavailable);QTest::newRow("rate-limited")<<429<<QByteArray("{}")<<int(UpdateResult::Error);QTest::newRow("denied")<<403<<QByteArray("{}")<<int(UpdateResult::Error);QTest::newRow("network")<<0<<QByteArray()<<int(UpdateResult::Error);QTest::newRow("server")<<503<<QByteArray()<<int(UpdateResult::Error);QTest::newRow("invalid")<<200<<QByteArray("not json")<<int(UpdateResult::Error);QTest::newRow("oversize")<<200<<QByteArray(600*1024,'x')<<int(UpdateResult::Error);}
 void transportErrors(){QFETCH(int,status);QFETCH(QByteArray,body);QFETCH(int,state);Network n;n.responses<<Response{status,body,{},false};UpdateChecker c(nullptr,&n);QSignalSpy spy(&c,&UpdateChecker::completed);QCOMPARE(n.requests.size(),0);c.check();c.check();QTRY_COMPARE(spy.size(),1);QCOMPARE(int(qvariant_cast<UpdateResult>(spy[0][0]).state),state);QCOMPARE(n.requests.size(),1);QCOMPARE(n.requests[0].url(),UpdateChecker::endpoint());QVERIFY(!n.requests[0].hasRawHeader("Authorization"));QVERIFY(!n.requests[0].hasRawHeader("Cookie"));}
 void timeoutCancelRedirect(){Network n;n.responses<<Response{200,{}, {},true};UpdateChecker c(nullptr,&n,15);QSignalSpy spy(&c,&UpdateChecker::completed);c.check();QTRY_COMPARE(spy.size(),1);QVERIFY(qvariant_cast<UpdateResult>(spy[0][0]).message.contains("timed out"));n.responses<<Response{200,{},{},true};c.check();c.cancel();QTest::qWait(35);QCOMPARE(spy.size(),1);n.responses<<Response{302,{},QUrl("https://evil.test/metadata"),false};c.check();QTRY_COMPARE(spy.size(),2);QVERIFY(qvariant_cast<UpdateResult>(spy[1][0]).message.contains("redirect"));QCOMPARE(n.requests.size(),3);}
 void metadataOnlyAndDialog(){Network n;auto r=release();auto m=manifest(r,UpdateHost::current().platformKey());auto bytes=json(m);addAsset(r,"RayNmeaSimulator-update.json",bytes.size(),QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()));n.responses<<Response{200,json(r),{},false}<<Response{302,{},QUrl("https://release-assets.githubusercontent.com/fixture/metadata"),false}<<Response{200,bytes,{},false};UpdateChecker c(nullptr,&n);UpdateDialog d(nullptr,&c);QSignalSpy spy(&c,&UpdateChecker::completed);d.show();QVERIFY(d.findChild<QLabel*>("installedVersion")->text().contains(APP_VERSION));QCOMPARE(n.requests.size(),0);d.startCheck();QTRY_COMPARE(spy.size(),1);QCOMPARE(qvariant_cast<UpdateResult>(spy[0][0]).state,UpdateResult::Newer);QCOMPARE(n.requests.size(),3);QVERIFY(n.requests[1].url().path().endsWith("RayNmeaSimulator-update.json"));QCOMPARE(n.requests[2].url().host(),QString("release-assets.githubusercontent.com"));QVERIFY(d.findChild<QPlainTextEdit*>("releaseNotes")->toPlainText().contains("<script>"));QVERIFY(d.findChild<QPushButton*>("openReleasePage")->isEnabled());QDesktopServices::setUrlHandler("https",this,"capture");d.findChild<QPushButton*>("openReleasePage")->click();QCOMPARE(opened,QUrl(base+"tag/v0.2.0"));QDesktopServices::unsetUrlHandler("https");
  const auto dir=qEnvironmentVariable("RAY_UPDATE_IMAGES");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(d.grab().save(dir+"/update-dialog.png"));}
 }
 void badMetadataIntegrityAndRedirectLimit(){
  for(bool redirectLoop:{false,true}){Network n;auto r=release();auto m=manifest(r);auto bytes=json(m);addAsset(r,"RayNmeaSimulator-update.json",bytes.size(),QString(64,'0'));n.responses<<Response{200,json(r),{},false};
   if(redirectLoop){for(int i=0;i<4;++i)n.responses<<Response{302,{},QUrl("https://release-assets.githubusercontent.com/fixture/metadata"),false};}
   else n.responses<<Response{200,bytes,{},false};
   UpdateChecker c(nullptr,&n);QSignalSpy spy(&c,&UpdateChecker::completed);c.check();QTRY_COMPARE(spy.size(),1);const auto result=qvariant_cast<UpdateResult>(spy[0][0]);QCOMPARE(result.state,UpdateResult::Error);QVERIFY(result.packageName.isEmpty());QVERIFY(result.message.contains(redirectLoop?"redirect":"integrity"));QCOMPARE(n.requests.size(),redirectLoop?5:2);
  }
 }
 void mainWindowAndMovementPreserved(){AppSettings settings(APP_COMPANY,APP_NAME);settings.clear();settings.setValue("Preservation/Sentinel","unchanged");settings.setValue("Nmea0183/Nmea0183Enabled",true);settings.setValue("Nmea0183/Nmea0183Port","MEMORY");settings.sync();MainWindow w(nullptr,nullptr,new MemoryPort);w.show();auto *authorize=w.findChild<QAction*>("actionStart_Sending");QVERIFY(authorize->isEnabled());authorize->trigger();QVERIFY(authorize->isChecked());Network n;n.responses<<Response{404,{},{},false};UpdateChecker c(nullptr,&n);UpdateDialog dialog(&w,&c);auto *action=w.findChild<QAction*>("actionCheckForUpdates");QVERIFY(action);QVERIFY(w.findChild<QAction*>("actionHelpContents"));QVERIFY(w.findChild<QAction*>("actionAbout"));QVERIFY(w.findChild<QAction*>("actionSupportRayNmeaSimulator"));const auto mode=w.currentMode();auto *start=w.findChild<QAction*>("actionStart_Sending");const bool sending=start->isChecked();MoveVesselSimulator movement;movement.setLastLatLong(42,-70);movement.setHeading(90);movement.setGearPort(N2kTG_Forward);movement.setThrottlePositionPort(0.4);movement.setEnabled(true);QSignalSpy moves(&movement,&MoveVesselSimulator::vesselStatusUpdated);action->trigger();QVERIFY(dialog.isVisible());dialog.startCheck();QTRY_VERIFY(dialog.findChild<QLabel*>("updateStatus")->text().contains("No public release"));QTRY_VERIFY(moves.size()>0);QCOMPARE(w.currentMode(),mode);QCOMPARE(start->isChecked(),sending);QCOMPARE(settings.value("Preservation/Sentinel").toString(),QString("unchanged"));QTest::keyClick(&dialog,Qt::Key_F1);QTRY_VERIFY(w.findChild<HelpDialog*>());QCOMPARE(w.findChild<HelpDialog*>()->currentTopic(),QString("check-for-updates"));w.findChild<HelpDialog*>()->close();QVERIFY(start->isChecked());dialog.close();QCOMPARE(start->isChecked(),sending);movement.setEnabled(false);}
};
int main(int argc,char **argv){QTemporaryDir profile;if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());qputenv("RAYNMEA_TEST_CREDENTIALS","1");QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];/* The only MainWindow uses an injected in-memory serial port and isolated settings. */QList<char*> ptr;for(auto &a:args)ptr<<a.data();int count=ptr.size();ptr<<nullptr;QApplication app(count,ptr.data());app.setApplicationName(APP_NAME);app.setOrganizationName(APP_COMPANY);app.setApplicationVersion(APP_VERSION);Themes::initializeSystemStyle();Themes::apply("Darkeum");UpdateTests tests;return QTest::qExec(&tests,argc,argv);}
#include "update_tests.moc"
