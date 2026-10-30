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
#include <QAbstractButton>
#include <QComboBox>
#include <QInputDialog>
#include <QJsonArray>
#include <QMessageBox>
#include <QMdiSubWindow>
#include <QNetworkProxy>
#include <QPushButton>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "appsettings.h"
#include "apppaths.h"
#include "depth128267.h"
#include "trueheadinghdt.h"

namespace {
MainController *controller(MainWindow &w) { return w.findChild<MainController*>(); }
QAction *action(MainWindow &w,const char *name) { return w.findChild<QAction*>(name); }
QByteArray bytes(const QString &path) { QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray{}; }
bool write(const QString &path,const QByteArray &data) { QFile f(path);return QDir().mkpath(QFileInfo(path).absolutePath()) && f.open(QIODevice::WriteOnly) && f.write(data)==data.size(); }
int count(const QByteArray &data) {int n=0;for(const auto &g:QJsonDocument::fromJson(data).object()["layouts"].toArray())n+=g.toObject()["widgets"].toArray().size();return n;}
void settle() { for(int i=0;i<5;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall); }
Depth128267 *add(MainWindow &w,const QString &group="Navigation",double value=12) {
    auto *c=controller(w);c->addWidgetAsComboboxTextChanged("128267","Depth",group);
    auto list=c->layoutWidgets(group);auto *d=qobject_cast<Depth128267*>(list.last());d->setDepthBelowTransducer(value);settle();return d;
}
struct Dialogs {
    QTimer timer;
    QMessageBox::StandardButton choice=QMessageBox::Cancel;
    bool yes=true,cancelPicker=false,cancelName=false,overwrite=true;
    QString target,group="Navigation",saveName="Snapshot";
    int guards=0,pickers=0,names=0,overwrites=0,otherMessages=0;
    QStringList guardText;
    std::function<void()> atPicker,atGuard;
    Dialogs() {
        QObject::connect(&timer,&QTimer::timeout,[this] {
            auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!d || d->property("answeredFixture").toBool())return;
            d->setProperty("answeredFixture",true);
            if(auto *b=qobject_cast<QMessageBox*>(d)) {
                if(b->objectName()=="layoutOperationDialog") {
                    ++guards;guardText<<b->text();if(atGuard)atGuard();
                    b->button(b->button(QMessageBox::Save)?choice:(yes?QMessageBox::Yes:QMessageBox::No))->click();
                } else if(b->windowTitle()=="Layout Exists") {++overwrites;b->button(overwrite?QMessageBox::Yes:QMessageBox::No)->click();}
                else {++otherMessages;if(auto *no=b->button(QMessageBox::No))no->click();else b->reject();}
            } else if(auto *input=qobject_cast<QInputDialog*>(d)) {
                ++names;if(cancelName)input->reject();else{input->setTextValue(saveName);input->accept();}
            } else if(auto *combo=d->findChild<QComboBox*>()) {
                ++pickers;if(atPicker)atPicker();if(cancelPicker){d->reject();return;}
                const int i=combo->objectName()=="layoutFileChoices"?combo->findData(target):combo->findText(group);
                if(i<0){d->reject();return;}combo->setCurrentIndex(i);d->accept();
            } else d->reject();
        });timer.start(0);
    }
};
void invoke(MainWindow &w,int operation) {
    const char *names[]={"actionClear_All","actionClear","actionLoad","actionLoad_Default"};action(w,names[operation])->trigger();settle();
}
QString targetFixture(MainWindow &w) {
    auto *c=controller(w);auto *d=add(w,"Navigation",99);d->setEnabled(false);settle();
    const auto path=AppPaths::layoutSavePath("Target");if(!c->saveLayoutToJson(path))return {};c->removeAllWidgets();return path;
}
QString working(MainWindow &w,bool named=true,bool modified=true) {
    auto *c=controller(w);add(w,"Navigation",12)->setEnabled(false);c->addWidgetAsComboboxTextChanged("130310","Environment","Environment");
    const auto p=AppPaths::layoutSavePath("Working");if(named && !c->saveLayoutToJson(p))return {};
    if(modified)add(w,"Navigation",32);return named?p:QString{};
}
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode m)override{return QIODevice::open(m|QIODevice::Unbuffered);}
    void close()override{QIODevice::close();}
    qint64 bytesToWrite()const override{return 0;}
protected:qint64 writeData(const char *p,qint64 n)override{output.append(p,n);return n;}
};
}
class LayoutDestructionTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> profile;
private slots:
    void init() {
        profile=std::make_unique<QTemporaryDir>();QVERIFY(profile->isValid());qputenv("RAYNMEA_TEST_HOME",profile->path().toUtf8());
        AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Preference/PromptSaveLayoutOnClose",false);s.setValue("Preservation/Sentinel","keep");s.sync();
    }
    void modifiedChoices_data() {
        QTest::addColumn<int>("operation");QTest::addColumn<int>("choice");
        for(int op=0;op<4;++op)for(int choice:{int(QMessageBox::Save),int(QMessageBox::Discard),int(QMessageBox::Cancel)})QTest::newRow(qPrintable(QString("%1-%2").arg(op).arg(choice)))<<op<<choice;
    }
    void modifiedChoices() {
        QFETCH(int,operation);QFETCH(int,choice);MainWindow w;Dialogs d;auto *c=controller(w);d.target=targetFixture(w);QVERIFY(!d.target.isEmpty());
        const auto defaultPath=AppPaths::layoutSavePath("defaultLayout");QVERIFY(write(defaultPath,bytes(d.target)));
        const auto path=working(w);QVERIFY(!path.isEmpty());const auto original=bytes(path);const auto targetBytes=bytes(d.target);QCOMPARE(c->layoutWidgets("Environment").size(),1);
        d.choice=static_cast<QMessageBox::StandardButton>(choice);QPointer<Depth128267> old=w.findChild<Depth128267*>();
        bool savedBeforeRemoval=false;QObject::connect(c,&MainController::layoutIdentityChanged,&w,[&]{if(c->layoutFilePath()==path && c->layoutWidgets("Navigation").size()==2 && count(bytes(path))==3)savedBeforeRemoval=true;});
        bool intactAtDecision=false;d.atGuard=[&]{intactAtDecision=old && c->hasLayoutWidgets() && w.currentMode()==Sender && !w.findChild<NmeaSenderWorker*>()->isSending();};
        d.otherMessages=0; // Creation-time optional Weather source prompt is unrelated to this command.
        invoke(w,operation);QCOMPARE(d.guards,1);QCOMPARE(d.otherMessages,0);QVERIFY(intactAtDecision);QCOMPARE(w.currentMode(),Sender);QVERIFY(!action(w,"actionStart_Sending")->isChecked());
        if(choice==QMessageBox::Cancel) {
            QVERIFY(old);QCOMPARE(c->layoutWidgets("Navigation").size(),2);QCOMPARE(c->layoutWidgets("Environment").size(),1);QCOMPARE(c->layoutFilePath(),path);QVERIFY(c->layoutNeedsSave());
        } else if(operation==0) {QVERIFY(!c->hasLayoutWidgets());QVERIFY(c->layoutFilePath().isEmpty());QVERIFY(!c->layoutNeedsSave());QVERIFY(old.isNull());}
        else if(operation==1) {QVERIFY(c->layoutWidgets("Navigation").isEmpty());QCOMPARE(c->layoutWidgets("Environment").size(),1);QCOMPARE(c->layoutFilePath(),path);QVERIFY(c->layoutStructurallyModified());QVERIFY(c->layoutNeedsSave());}
        else {QCOMPARE(c->layoutWidgets("Navigation").size(),1);QVERIFY(c->layoutWidgets("Environment").isEmpty());QCOMPARE(c->layoutFilePath(),operation==2?d.target:defaultPath);QVERIFY(!c->layoutNeedsSave());auto *depth=w.findChild<Depth128267*>();QVERIFY(depth);QCOMPARE(depth->DepthBelowTransducer(),99.0);QVERIFY(!depth->isEnabled());QVERIFY(c->layoutWidgets("Generate").isEmpty());QVERIFY(old.isNull());}
        if(choice==QMessageBox::Save){QVERIFY(savedBeforeRemoval);QCOMPARE(count(bytes(path)),3);QVERIFY(bytes(path)!=original);QCOMPARE(d.overwrites,1);const auto groups=QJsonDocument::fromJson(bytes(path)).object()["layouts"].toArray();bool disabled=false;for(auto g:groups)for(auto widget:g.toObject()["widgets"].toArray())disabled|=!widget.toObject()["enabled"].toBool();QVERIFY(disabled);}
        else {QCOMPARE(bytes(path),original);QCOMPARE(d.overwrites,0);}
        QCOMPARE(bytes(d.target),targetBytes);
    }
    void cleanConfirmations_data() {
        QTest::addColumn<int>("operation");QTest::addColumn<bool>("yes");
        for(int op:{0,2,3})for(bool yes:{false,true})QTest::newRow(qPrintable(QString("%1-%2").arg(op).arg(yes)))<<op<<yes;
    }
    void cleanConfirmations() {
        QFETCH(int,operation);QFETCH(bool,yes);MainWindow w;Dialogs d;auto *c=controller(w);d.target=targetFixture(w);QVERIFY(write(AppPaths::layoutSavePath("defaultLayout"),bytes(d.target)));const auto path=working(w,true,false);const auto original=bytes(path);QPointer<Depth128267> old=w.findChild<Depth128267*>();d.yes=yes;
        d.otherMessages=0;
        invoke(w,operation);QCOMPARE(d.guards,1);QCOMPARE(d.names,0);QCOMPARE(d.overwrites,0);QCOMPARE(d.otherMessages,0);QCOMPARE(bytes(path),original);
        if(!yes){QVERIFY(old);QCOMPARE(c->layoutFilePath(),path);QVERIFY(!c->layoutNeedsSave());}else QVERIFY(old.isNull());
    }
    void saveFailures_data() {
        QTest::addColumn<int>("operation");QTest::addColumn<int>("failure");
        for(int op=0;op<4;++op)for(int f=0;f<3;++f)QTest::newRow(qPrintable(QString("%1-%2").arg(op).arg(f)))<<op<<f;
    }
    void saveFailures() {
        QFETCH(int,operation);QFETCH(int,failure);MainWindow w;Dialogs d;auto *c=controller(w);d.target=targetFixture(w);QVERIFY(write(AppPaths::layoutSavePath("defaultLayout"),bytes(d.target)));const auto path=working(w,failure==2);const auto original=bytes(path);QPointer<Depth128267> old=w.findChild<Depth128267*>();d.choice=QMessageBox::Save;
        if(failure==0)d.cancelName=true;if(failure==1)QVERIFY(QDir().mkpath(AppPaths::layoutSavePath(d.saveName)));if(failure==2)d.overwrite=false;
        invoke(w,operation);QCOMPARE(d.guards,1);QVERIFY(old);QCOMPARE(c->layoutWidgets("Navigation").size(),2);QCOMPARE(c->layoutWidgets("Environment").size(),1);QCOMPARE(c->layoutFilePath(),path);QVERIFY(c->layoutNeedsSave());QCOMPARE(bytes(path),original);QCOMPARE(w.currentMode(),Sender);QVERIFY(!w.findChild<NmeaSenderWorker*>()->isSending());
    }
    void emptyAndSelectedEmptyGroup() {
        MainWindow w;Dialogs d;auto *c=controller(w);invoke(w,0);invoke(w,1);QCOMPARE(d.guards,0);QCOMPARE(d.pickers,0);
        add(w);auto *area=w.findChild<QMdiArea*>();for(auto *sub:area->subWindowList())if(sub->windowTitle()=="Environment")area->setActiveSubWindow(sub);
        d.group="Environment";invoke(w,1);QCOMPARE(d.pickers,1);QCOMPARE(d.guards,0);QCOMPARE(c->layoutWidgets("Navigation").size(),1);
        d.cancelPicker=true;invoke(w,1);QCOMPARE(d.guards,0);QCOMPARE(c->layoutWidgets("Navigation").size(),1);
    }
    void cleanGroupRemovalAndNewSaveAs() {
        MainWindow w;Dialogs d;auto *c=controller(w);const auto path=working(w,true,false);const auto original=bytes(path);invoke(w,1);QCOMPARE(d.guards,0);QVERIFY(c->layoutWidgets("Navigation").isEmpty());QCOMPARE(c->layoutWidgets("Environment").size(),1);QVERIFY(c->layoutNeedsSave());QCOMPARE(bytes(path),original);
        c->removeAllWidgets();add(w);d.choice=QMessageBox::Save;invoke(w,0);QCOMPARE(d.guards,1);QCOMPARE(d.names,1);QVERIFY(!c->hasLayoutWidgets());QVERIFY(c->layoutFilePath().isEmpty());QCOMPARE(count(bytes(AppPaths::layoutSavePath(d.saveName))),1);
    }
    void cancelledAndInvalidTargets() {
        MainWindow w;Dialogs d;auto *c=controller(w);d.target=targetFixture(w);const auto path=working(w);const auto original=bytes(path);QPointer<Depth128267> old=w.findChild<Depth128267*>();
        d.cancelPicker=true;invoke(w,2);QCOMPARE(d.guards,0);QVERIFY(old);d.cancelPicker=false;
        for(const auto &invalid:QList<QByteArray>{"{broken",R"({"formatVersion":999,"layouts":[]})"}){QVERIFY(write(d.target,invalid));invoke(w,2);QCOMPARE(d.guards,0);QVERIFY(old);QCOMPARE(c->layoutFilePath(),path);QVERIFY(c->layoutNeedsSave());}
        d.atPicker=[&]{QFile::remove(d.target);};invoke(w,2);QCOMPARE(d.guards,0);QVERIFY(old);QCOMPARE(bytes(path),original);
    }
    void defaultPrecedenceAndInvalidHigherPriority() {
        MainWindow w;Dialogs d;auto *c=controller(w);const auto fixture=targetFixture(w);const auto data=bytes(fixture);QVERIFY(!data.isEmpty());QVERIFY(QFile::remove(fixture));
        const auto legacy=AppPaths::legacyDirectory()+"/layouts/defaultLayout.json";const auto current=AppPaths::layoutSavePath("defaultLayout");QVERIFY(write(legacy,data));QVERIFY(write(current,"broken"));working(w);QPointer<Depth128267> old=w.findChild<Depth128267*>();d.choice=QMessageBox::Discard;
        invoke(w,3);QCOMPARE(d.guards,0);QVERIFY(old);QVERIFY(c->layoutNeedsSave());QVERIFY(QFile::remove(current));
        invoke(w,3);QCOMPARE(c->layoutFilePath(),legacy);QVERIFY(!c->layoutNeedsSave());QCOMPARE(d.guards,1);
        auto currentData=QJsonDocument::fromJson(data).object();currentData["numberOfWidgetColumns"]=3;QVERIFY(write(current,QJsonDocument(currentData).toJson()));d.yes=true;invoke(w,3);QCOMPARE(c->layoutFilePath(),current);QVERIFY(!c->layoutNeedsSave());
        c->removeAllWidgets();QVERIFY(QFile::remove(current));QVERIFY(QFile::remove(legacy));const int before=d.guards;invoke(w,3);QCOMPARE(c->layoutFilePath(),QString(":/defaults/defaultLayout.json"));QVERIFY(c->hasLayoutWidgets());QVERIFY(!c->layoutNeedsSave());QCOMPARE(d.guards,before);QVERIFY(!action(w,"actionStart_Sending")->isChecked());
    }
    void legacyDepthWarningSharesDecision() {
        MainWindow w;Dialogs d;auto *c=controller(w);d.target=targetFixture(w);auto root=QJsonDocument::fromJson(bytes(d.target)).object();auto groups=root["layouts"].toArray();auto g=groups[0].toObject();auto widgets=g["widgets"].toArray();auto widget=widgets[0].toObject();auto parameters=widget["parameters"].toArray();
        for(int i=0;i<parameters.size();++i){auto row=parameters[i].toArray();if(row[0].toString()=="DepthBelowTransducer"){row[1]="100";row[2]="dm";parameters[i]=row;}}
        widget["parameters"]=parameters;widgets[0]=widget;g["widgets"]=widgets;groups[0]=g;root["layouts"]=groups;QVERIFY(write(d.target,QJsonDocument(root).toJson()));working(w);d.otherMessages=0;d.choice=QMessageBox::Cancel;invoke(w,2);QCOMPARE(d.guards,1);QVERIFY(d.guardText.last().contains("decimeters"));QCOMPARE(d.otherMessages,0);d.choice=QMessageBox::Discard;invoke(w,2);QCOMPARE(w.findChild<Depth128267*>()->DepthBelowTransducer(),10.0);QCOMPARE(d.otherMessages,0);
    }
    void savingToSelectedTargetKeepsValidatedSnapshot() {
        MainWindow w;Dialogs d;auto *c=controller(w);d.target=working(w);const auto selected=bytes(d.target);QCOMPARE(count(selected),2);
        d.choice=QMessageBox::Save;invoke(w,2);QCOMPARE(d.guards,1);QCOMPARE(d.overwrites,1);
        // The full working snapshot was saved before removal. Load uses the
        // validated selection captured before the decision, never an unvalidated reread.
        QCOMPARE(count(bytes(d.target)),3);QCOMPARE(c->layoutWidgets("Navigation").size(),1);QCOMPARE(c->layoutWidgets("Environment").size(),1);QCOMPARE(c->layoutFilePath(),d.target);QVERIFY(!c->layoutNeedsSave());
    }
    void activeSendingAndViewerCommands() {
        AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Nmea0183/Nmea0183Enabled",true);s.setValue("Nmea0183/Nmea0183Port","MEMORY");s.sync();auto *port=new MemoryPort;MainWindow w(nullptr,nullptr,port);Dialogs d;auto *c=controller(w);working(w);
        c->addWidgetAsComboboxTextChanged("HDT","Heading","Navigation");action(w,"actionStart_Sending")->setChecked(true);QTRY_VERIFY(w.findChild<NmeaSenderWorker*>()->isSending());invoke(w,0);QVERIFY(c->hasLayoutWidgets());QVERIFY(!w.findChild<NmeaSenderWorker*>()->isSending());QVERIFY(!action(w,"actionStart_Sending")->isChecked());const auto output=port->output;QTest::qWait(350);QCOMPARE(port->output,output);
        c->removeAllWidgets();action(w,"actionViewer_Rx")->trigger();QCOMPARE(w.currentMode(),Viewer);add(w);d.choice=QMessageBox::Cancel;invoke(w,0);QCOMPARE(w.currentMode(),Viewer);QVERIFY(c->hasLayoutWidgets());QVERIFY(!action(w,"actionStart_Sending")->isEnabled());d.choice=QMessageBox::Discard;invoke(w,0);QCOMPARE(w.currentMode(),Viewer);QVERIFY(!c->hasLayoutWidgets());QVERIFY(!w.findChild<NmeaSenderWorker*>()->isSending());
    }
};
int main(int argc,char **argv){QTemporaryDir root;qputenv("RAYNMEA_TEST_HOME",root.path().toUtf8());QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));LayoutDestructionTests t;return QTest::qExec(&t,argc,argv);}
#include "layout_destruction_tests.moc"
