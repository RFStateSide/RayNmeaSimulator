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
#include <QActionGroup>
#include <QAbstractButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QJsonDocument>
#include "mainwindow.h"
#include "appsettings.h"
#include "apppaths.h"
#include "depth128267.h"
#include "trueheadinghdt.h"
#include "N2kMessages.h"

namespace {
MainController *controller(MainWindow &w){return w.findChild<MainController*>();}
QAction *generate(MainWindow &w){return w.findChild<QAction*>("actionSimulator_Tx");}
QAction *viewer(MainWindow &w){return w.findChild<QAction*>("actionViewer_Rx");}
QAction *sending(MainWindow &w){return w.findChild<QAction*>("actionStart_Sending");}
NmeaSenderWorker *worker(MainWindow &w){return w.findChild<NmeaSenderWorker*>();}
void settle(){for(int i=0;i<5;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);}
void add(MainWindow &w){controller(w)->addWidgetAsComboboxTextChanged("128267","Depth","Navigation");settle();}
QByteArray bytes(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
struct Dialogs {
    QTimer timer;
    QString entryChoice="viewerCancel",saveName="viewer-entry-test";
    bool cancelSave=false,overwrite=false,replace=false,consistentDuringEntry=true;
    int entries=0,saveDialogs=0;
    QStringList entryButtons;
    Dialogs(){
        QObject::connect(&timer,&QTimer::timeout,[this]{
            auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!d || d->property("AnsweredByTest").toBool())return;
            d->setProperty("AnsweredByTest",true);
            if(auto *box=qobject_cast<QMessageBox*>(d)){
                if(box->objectName()=="viewerEntryDialog"){
                    ++entries;for(auto *b:box->buttons())entryButtons<<b->objectName();
                    auto *window=qobject_cast<MainWindow*>(box->parentWidget());
                    consistentDuringEntry &= window && window->currentMode()==Sender && generate(*window)->isChecked() && !viewer(*window)->isChecked() && !worker(*window)->isSending();
                    auto *choice=box->findChild<QPushButton*>(entryChoice);if(choice)choice->click();else box->reject();
                }else if(box->windowTitle()=="Layout Exists")box->button(overwrite?QMessageBox::Yes:QMessageBox::No)->click();
                else box->button(replace?QMessageBox::Yes:QMessageBox::No)->click();
            }else if(auto *input=qobject_cast<QInputDialog*>(d)){
                ++saveDialogs;if(cancelSave)input->reject();else{input->setTextValue(saveName);input->accept();}
            }else d->reject();
        });timer.start(0);
    }
};
class SerialMemory : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode mode)override{return QIODevice::open(mode|QIODevice::Unbuffered);}
    void close()override{QIODevice::close();}
    qint64 bytesToWrite()const override{return 0;}
protected:qint64 writeData(const char *p,qint64 n)override{output.append(p,n);return n;}
};
}
class ModeViewerEntryTests : public QObject {
    Q_OBJECT
    void checkMode(MainWindow &w,Mode mode){
        QCOMPARE(w.currentMode(),mode);QCOMPARE(generate(w)->isChecked(),mode==Sender);QCOMPARE(viewer(w)->isChecked(),mode==Viewer);
        auto *group=w.findChild<QActionGroup*>("modeActions");QVERIFY(group);QVERIFY(group->isExclusive());QCOMPARE(group->checkedAction(),mode==Sender?generate(w):viewer(w));
    }
private slots:
    void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/Nmea2000Enabled",false);s.setValue("Nmea0183/Nmea0183Enabled",false);s.sync();}
    void idempotentActionsTipsAndEmptyEntry(){
        MainWindow w;Dialogs d;checkMode(w,Sender);QVERIFY(!controller(w)->hasLayoutWidgets());
        QVERIFY(generate(w)->statusTip().contains("Enable Sending"));QCOMPARE(generate(w)->toolTip(),generate(w)->statusTip());
        QVERIFY(viewer(w)->statusTip().contains("added automatically"));QVERIFY(viewer(w)->statusTip().contains("Sending is disabled"));
        for(int i=0;i<3;++i)generate(w)->trigger();checkMode(w,Sender);QCOMPARE(d.entries,0);
        viewer(w)->trigger();checkMode(w,Viewer);QVERIFY(!sending(w)->isEnabled());QVERIFY(!worker(w)->isSending());
        for(int i=0;i<3;++i)viewer(w)->trigger();checkMode(w,Viewer);QCOMPARE(d.entries,0);
        generate(w)->trigger();checkMode(w,Sender);QVERIFY(!worker(w)->isSending());
        generate(w)->setChecked(false);checkMode(w,Sender);viewer(w)->setChecked(true);checkMode(w,Viewer);
    }
    void choices_data(){
        QTest::addColumn<int>("state");QTest::addColumn<int>("choice");
        for(int state:{0,1,2})for(int choice:{0,1,2})QTest::newRow(qPrintable(QString("%1-%2").arg(state).arg(choice)))<<state<<choice;
    }
    void choices(){
        QFETCH(int,state);QFETCH(int,choice);MainWindow w;Dialogs d;auto *c=controller(w);add(w);QTemporaryDir dir;QString file=dir.filePath("loaded.json");
        if(state<2){QVERIFY(c->saveLayoutToJson(file));c->removeAllWidgets();QVERIFY(c->loadLayoutFromJson(file));}
        if(state==1)add(w);
        const bool saveFirst=state!=0;QCOMPARE(c->hasLayoutBaseline(),state<2);QCOMPARE(c->layoutStructurallyModified(),state!=0);
        auto old=w.findChildren<Depth128267*>();const int count=old.size();QPointer<Depth128267> retained=old.first();const auto original=bytes(file);
        d.entryChoice=choice==0?(saveFirst?"viewerSaveAndClear":"viewerClear"):choice==1?"viewerKeepExisting":"viewerCancel";
        d.saveName=QString("entry-%1-%2").arg(state).arg(choice);const auto saved=AppPaths::layoutSavePath(d.saveName);
        bool savedBeforeClear=false;QObject::connect(c,&MainController::layoutIdentityChanged,&w,[&]{if(QFile::exists(saved)&&c->hasLayoutWidgets())savedBeforeClear=true;});
        viewer(w)->trigger();QCOMPARE(d.entries,1);QVERIFY(d.consistentDuringEntry);QVERIFY(d.entryButtons.contains(saveFirst?"viewerSaveAndClear":"viewerClear"));QVERIFY(d.entryButtons.contains("viewerKeepExisting"));QVERIFY(d.entryButtons.contains("viewerCancel"));
        if(choice==2){checkMode(w,Sender);QCOMPARE(w.findChildren<Depth128267*>().size(),count);QVERIFY(retained);}
        else{checkMode(w,Viewer);if(choice==0){QVERIFY(!c->hasLayoutWidgets());QVERIFY(retained.isNull());if(saveFirst){QVERIFY(savedBeforeClear);QVERIFY(!bytes(saved).isEmpty());QCOMPARE(d.saveDialogs,1);}}else{QCOMPARE(w.findChildren<Depth128267*>().size(),count);QVERIFY(retained);}}
        QCOMPARE(bytes(file),original);QVERIFY(!sending(w)->isChecked());QVERIFY(!worker(w)->isSending());
        if(w.currentMode()==Viewer){viewer(w)->trigger();QCOMPARE(d.entries,1);QVERIFY(d.consistentDuringEntry);checkMode(w,Viewer);generate(w)->trigger();checkMode(w,Sender);}
    }
    void saveCancellationFailureAndOverwrite_data(){QTest::addColumn<int>("failure");QTest::newRow("cancel")<<0;QTest::newRow("IO-failure")<<1;QTest::newRow("overwrite-No")<<2;}
    void saveCancellationFailureAndOverwrite(){
        QFETCH(int,failure);MainWindow w;Dialogs d;add(w);auto *c=controller(w);d.entryChoice="viewerSaveAndClear";d.saveName=QString("save-failure-%1").arg(failure);
        const auto path=AppPaths::layoutSavePath(d.saveName);QByteArray original;
        if(failure==0)d.cancelSave=true;
        if(failure==1)QVERIFY(QDir().mkpath(path));
        if(failure==2){QVERIFY(c->saveLayoutToJson(path));original=bytes(path);add(w);}
        const int count=w.findChildren<Depth128267*>().size();QPointer<Depth128267> pointer=w.findChild<Depth128267*>();
        viewer(w)->trigger();checkMode(w,Sender);QVERIFY(pointer);QCOMPARE(w.findChildren<Depth128267*>().size(),count);QVERIFY(c->layoutStructurallyModified());QVERIFY(!sending(w)->isChecked());
        if(failure==1)QVERIFY(w.statusBar()->currentMessage().contains("save failed"));if(failure==2)QCOMPARE(bytes(path),original);
    }
    void trackingAndFailedLifecycle(){
        MainWindow w;Dialogs d;auto *c=controller(w);QTemporaryDir dir;const auto path=dir.filePath("baseline.json");
        QVERIFY(!c->hasLayoutBaseline());QVERIFY(!c->layoutStructurallyModified());add(w);QVERIFY(c->layoutStructurallyModified());QVERIFY(c->saveLayoutToJson(path));QVERIFY(!c->layoutStructurallyModified());
        auto *depth=w.findChild<Depth128267*>();depth->setDepthBelowTransducer(25);depth->setEnabled(false);depth->setRowSpan(depth->RowSpan()+1);settle();QVERIFY(!c->layoutStructurallyModified());
        c->addGeneratorWidget("DateTime (PGN -129033)");c->addGeneratorWidget("Weather (PGN -130311)");QVERIFY(c->layoutStructurallyModified());QVERIFY(c->saveLayoutToJson(path));
        auto *clock=w.findChild<GeneratorDateTime*>();auto *weather=w.findChild<GeneratorWeather*>();QVERIFY(clock);QVERIFY(weather);
        clock->setSecondsSinceMidnight(12345);weather->setLatitude(42);weather->setWindSpeed(9);QTest::qWait(1100);QVERIFY(!c->layoutStructurallyModified());
        add(w);QVERIFY(c->layoutStructurallyModified());QVERIFY(!c->loadLayoutFromJson(dir.filePath("absent.json")));QVERIFY(c->layoutStructurallyModified());QVERIFY(!c->loadLayoutFromJson(path));QVERIFY(c->layoutStructurallyModified());
        QVERIFY(!c->saveLayoutToJson(dir.filePath("missing/failed.json")));QVERIFY(c->layoutStructurallyModified());
        d.replace=true;QVERIFY(c->loadLayoutFromJson(path));QVERIFY(!c->layoutStructurallyModified());QVERIFY(c->hasLayoutBaseline());
        c->removeWidgetFromSubForm(w.findChild<Depth128267*>(),"Navigation");QVERIFY(c->layoutStructurallyModified());QVERIFY(c->saveLayoutToJson(path));QVERIFY(!c->layoutStructurallyModified());
        c->removeAllWidgetsFromGroup("Generate");QVERIFY(c->layoutStructurallyModified());c->removeAllWidgets();QVERIFY(!c->hasLayoutWidgets());QVERIFY(!c->hasLayoutBaseline());QVERIFY(!c->layoutStructurallyModified());
    }
    void receiveDiscoveryAndValuesAreNotStructural(){
        MainWindow w;Dialogs d;viewer(w)->trigger();auto *c=controller(w);auto *h=w.findChild<Nmea2000Handler*>();QVERIFY(h);
        auto receive=[&](int source,double depth){tN2kMsg m;SetN2kWaterDepth(m,1,depth,0,100);m.Source=source;h->nmea2000MessageReceived(m);settle();};
        receive(23,12);QCOMPARE(w.findChildren<Depth128267*>().size(),1);QVERIFY(!c->layoutStructurallyModified());QVERIFY(c->layoutWidgets("Generate").isEmpty());
        receive(23,15);receive(24,20);QCOMPARE(w.findChildren<Depth128267*>().size(),2);QVERIFY(!c->layoutStructurallyModified());QCOMPARE(d.entries,0);viewer(w)->trigger();QCOMPARE(d.entries,0);
        QTemporaryDir dir;QVERIFY(c->saveLayoutToJson(dir.filePath("rx.json")));receive(25,25);QVERIFY(!c->layoutStructurallyModified());QVERIFY(c->hasLayoutBaseline());
        generate(w)->trigger();d.entryChoice="viewerKeepExisting";viewer(w)->trigger();QVERIFY(d.entryButtons.contains("viewerClear"));QCOMPARE(w.findChildren<Depth128267*>().size(),3);checkMode(w,Viewer);
        QVERIFY(!sending(w)->isChecked());QVERIFY(!worker(w)->isSending());
    }
    void alternateEntryAndActiveSendingCancel(){
        AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Nmea0183/Nmea0183Enabled",true);s.setValue("Nmea0183/Nmea0183Port","MEMORY");s.sync();auto *port=new SerialMemory;MainWindow w(nullptr,nullptr,port);Dialogs d;
        controller(w)->addWidgetAsComboboxTextChanged("HDT","Heading","Navigation");sending(w)->setChecked(true);QTRY_VERIFY(worker(w)->isSending());QVERIFY(sending(w)->isChecked());
        generate(w)->trigger();checkMode(w,Sender);QVERIFY(worker(w)->isSending());
        viewer(w)->setChecked(true);checkMode(w,Sender);QVERIFY(d.consistentDuringEntry);QCOMPARE(d.entries,1);QVERIFY(!sending(w)->isChecked());QVERIFY(!worker(w)->isSending());QCOMPARE(w.findChildren<TrueHeadingHDT*>().size(),1);
        d.entryChoice="viewerKeepExisting";QVERIFY(QMetaObject::invokeMethod(&w,"on_actionViewer_Rx_toggled",Qt::DirectConnection,Q_ARG(bool,true)));checkMode(w,Viewer);QVERIFY(!sending(w)->isEnabled());
        for(int i=0;i<3;++i){generate(w)->trigger();viewer(w)->trigger();checkMode(w,Viewer);QVERIFY(!worker(w)->isSending());}
    }
};
int main(int argc,char **argv){QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));ModeViewerEntryTests t;return QTest::qExec(&t,argc,argv);}
#include "mode_viewer_entry_tests.moc"
