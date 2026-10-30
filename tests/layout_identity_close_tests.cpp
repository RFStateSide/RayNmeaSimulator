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
#include <QCheckBox>
#include <QInputDialog>
#include <QLineEdit>
#include "engineprofiledialog.h"
#include <QMessageBox>
#include <QNetworkProxy>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "appsettings.h"
#include "apppaths.h"
#include "depth128267.h"
#include "N2kMessages.h"

namespace {
MainController *controller(MainWindow &w) { return w.findChild<MainController*>(); }
QAction *action(MainWindow &w,const char *name) { return w.findChild<QAction*>(name); }
void add(MainWindow &w) { controller(w)->addWidgetAsComboboxTextChanged("128267","Depth","Navigation"); }
QByteArray bytes(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray{}; }
void saveAs(MainWindow &w) { action(w,"actionSave")->trigger(); }
struct Dialogs {
    QTimer timer;
    QMessageBox::StandardButton closeChoice=QMessageBox::Cancel, draftChoice=QMessageBox::Cancel;
    bool cancelName=false, overwrite=false;
    QString name="captured", viewerChoice="viewerKeepExisting";
    QStringList defaults;
    int closes=0, confirmations=0, drafts=0;
    std::function<void()> duringClose;
    Dialogs() {
        QObject::connect(&timer,&QTimer::timeout,[this] {
            auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!d || d->property("testAnswered").toBool()) return;
            d->setProperty("testAnswered",true);
            if(auto *b=qobject_cast<QMessageBox*>(d)) {
                if(b->objectName()=="layoutCloseDialog") { ++closes; if(duringClose)duringClose(); b->button(closeChoice)->click(); }
                else if(b->objectName()=="viewerEntryDialog") b->findChild<QPushButton*>(viewerChoice)->click();
                else if(b->windowTitle()=="Layout Exists") { ++confirmations; b->button(overwrite?QMessageBox::Yes:QMessageBox::No)->click(); }
                else if(b->windowTitle()=="Unsaved engine profiles") b->button(draftChoice)->click();
                else if(b->windowTitle()=="Unsaved Setup changes") { ++drafts; b->button(draftChoice)->click(); }
                else if(b->button(QMessageBox::No)) b->button(QMessageBox::No)->click();
                else b->reject();
            } else if(auto *input=qobject_cast<QInputDialog*>(d)) {
                defaults<<input->textValue(); if(cancelName) input->reject(); else {input->setTextValue(name);input->accept();}
            } else d->reject();
        }); timer.start(0);
    }
};
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode m) override { return QIODevice::open(m|QIODevice::Unbuffered); }
    void close() override { QIODevice::close(); }
    qint64 bytesToWrite() const override { return 0; }
protected:
    qint64 writeData(const char *p,qint64 n) override { output.append(p,n);return n; }
};
}
class LayoutIdentityCloseTests : public QObject {
    Q_OBJECT
private slots:
    void init() {
        AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Preservation/Sentinel","keep");s.sync();
    }
    void identityAndSaveDefaults() {
        MainWindow w;Dialogs d;auto *c=controller(w);QTemporaryDir dir;
        QVERIFY(c->layoutFilePath().isEmpty());QCOMPARE(c->layoutSaveName(),QString("defaultLayout"));
        add(w);const auto loaded=dir.filePath("MyVessel.json");QVERIFY(c->saveLayoutToJson(loaded));c->removeAllWidgets();
        QVERIFY(c->loadLayoutFromJson(loaded));QCOMPARE(c->layoutFilePath(),loaded);QCOMPARE(c->layoutSaveName(),QString("MyVessel"));
        d.cancelName=true;saveAs(w);QCOMPARE(d.defaults.last(),QString("MyVessel"));QCOMPARE(c->layoutFilePath(),loaded);
        d.cancelName=false;d.name="NewVessel";saveAs(w);QCOMPARE(c->layoutSaveName(),d.name);QVERIFY(!c->layoutNeedsSave());
        const auto saved=AppPaths::layoutSavePath(d.name);QCOMPARE(c->layoutFilePath(),saved);QVERIFY(QFile::exists(saved));
        d.cancelName=true;saveAs(w);QCOMPARE(d.defaults.last(),QString("NewVessel"));QCOMPARE(c->layoutFilePath(),saved);
        c->removeAllWidgets();QVERIFY(c->layoutFilePath().isEmpty());QCOMPARE(c->layoutSaveName(),QString("defaultLayout"));saveAs(w);QCOMPARE(d.defaults.last(),QString("defaultLayout"));
    }
    void failedOperationsPreserveIdentity() {
        MainWindow w;Dialogs d;auto *c=controller(w);QTemporaryDir dir;add(w);const auto p=dir.filePath("Good.json");QVERIFY(c->saveLayoutToJson(p));add(w);
        const auto old=bytes(p);QVERIFY(!c->saveLayoutToJson(dir.filePath("missing/failure.json")));QCOMPARE(c->layoutFilePath(),p);QVERIFY(c->layoutNeedsSave());
        QVERIFY(!c->loadLayoutFromJson(dir.filePath("absent.json")));QCOMPARE(c->layoutFilePath(),p);
        QVERIFY(!c->loadLayoutFromJson(p));QCOMPARE(c->layoutFilePath(),p);QVERIFY(c->layoutNeedsSave());
        d.name="blocked";QVERIFY(QDir().mkpath(AppPaths::layoutSavePath(d.name)));saveAs(w);QCOMPARE(c->layoutFilePath(),p);QVERIFY(c->layoutNeedsSave());QCOMPARE(bytes(p),old);
    }
    void closeChoices_data() {
        QTest::addColumn<bool>("named");QTest::addColumn<int>("choice");
        for(bool named:{false,true}) for(int choice:{int(QMessageBox::Save),int(QMessageBox::Discard),int(QMessageBox::Cancel)})
            QTest::newRow(qPrintable(QString("%1-%2").arg(named).arg(choice)))<<named<<choice;
    }
    void closeChoices() {
        QFETCH(bool,named);QFETCH(int,choice);MainWindow w;w.show();Dialogs d;auto *c=controller(w);add(w);
        const auto path=AppPaths::layoutSavePath("close-baseline");if(named){QVERIFY(c->saveLayoutToJson(path));add(w);}
        const auto original=bytes(path);QPointer<Depth128267> pointer=w.findChild<Depth128267*>();const auto oldMode=w.currentMode();
        d.closeChoice=static_cast<QMessageBox::StandardButton>(choice);d.overwrite=true;d.name="close-new";
        bool intact=false;d.duringClose=[&]{intact=pointer && c->hasLayoutWidgets() && !w.findChild<NmeaSenderWorker*>()->isSending();};
        const bool closed=w.close();QCOMPARE(d.closes,1);QVERIFY(intact);QVERIFY(pointer);QCOMPARE(w.currentMode(),oldMode);
        QCOMPARE(closed,choice!=QMessageBox::Cancel);QVERIFY(!action(w,"actionStart_Sending")->isChecked());
        if(choice==QMessageBox::Save) {QVERIFY(!c->layoutNeedsSave());QVERIFY(QFile::exists(c->layoutFilePath()));QCOMPARE(d.defaults.size(),named?0:1);if(named)QVERIFY(bytes(path)!=original);}
        else {QCOMPARE(bytes(path),original);QVERIFY(c->layoutNeedsSave());}
        if(choice==QMessageBox::Cancel){QVERIFY(w.isVisible());pointer->setDepthBelowTransducer(45);QCOMPARE(pointer->DepthBelowTransducer(),45.0);add(w);QCOMPARE(w.findChildren<Depth128267*>().size(),named?3:2);}
    }
    void saveFailureAbortsClose_data() {
        QTest::addColumn<int>("failure");QTest::newRow("name-cancel")<<0;QTest::newRow("unnamed-write-fail")<<1;QTest::newRow("current-overwrite-refused")<<2;QTest::newRow("current-write-fail")<<3;
    }
    void saveFailureAbortsClose() {
        QFETCH(int,failure);MainWindow w;w.show();Dialogs d;auto *c=controller(w);add(w);d.closeChoice=QMessageBox::Save;d.name=QString("failure-%1").arg(failure);d.overwrite=failure==3;
        QString previous;QByteArray original;
        if(failure>=2){previous=AppPaths::layoutSavePath(d.name);QVERIFY(c->saveLayoutToJson(previous));original=bytes(previous);add(w);}
        if(failure==0)d.cancelName=true;
        if(failure==1)QVERIFY(QDir().mkpath(AppPaths::layoutSavePath(d.name)));
        if(failure==3){QVERIFY(QFile::remove(previous));QVERIFY(QDir().mkpath(previous));}
        const int count=w.findChildren<Depth128267*>().size();QPointer<Depth128267> pointer=w.findChild<Depth128267*>();
        QVERIFY(!w.close());QVERIFY(w.isVisible());QVERIFY(pointer);QCOMPARE(w.findChildren<Depth128267*>().size(),count);QVERIFY(c->layoutNeedsSave());QCOMPARE(c->layoutFilePath(),previous);
        if(failure==2)QCOMPARE(bytes(previous),original);if(failure==1||failure==3)QVERIFY(w.statusBar()->currentMessage().contains("save failed"));
    }
    void cleanValuesAndDisabledDoNotPrompt() {
        MainWindow w;Dialogs d;w.show();QVERIFY(w.close());QCOMPARE(d.closes,0);w.show();add(w);auto *c=controller(w);QTemporaryDir dir;QVERIFY(c->saveLayoutToJson(dir.filePath("clean.json")));
        auto *depth=w.findChild<Depth128267*>();depth->setDepthBelowTransducer(19);depth->setEnabled(false);QVERIFY(!c->layoutNeedsSave());QVERIFY(w.close());QCOMPARE(d.closes,0);
        AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/PromptSaveLayoutOnClose",false);s.sync();
        MainWindow disabled;disabled.show();add(disabled);QVERIFY(controller(disabled)->layoutNeedsSave());QVERIFY(disabled.close());QCOMPARE(d.closes,0);
    }
    void rxSnapshotUnnamedPromptsButSavedDiscoveryDoesNot() {
        MainWindow w;Dialogs d;w.show();action(w,"actionViewer_Rx")->trigger();auto *c=controller(w);auto *h=w.findChild<Nmea2000Handler*>();
        auto receive=[&](int source){tN2kMsg m;SetN2kWaterDepth(m,1,15,0,100);m.Source=source;h->nmea2000MessageReceived(m);for(int i=0;i<5;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);};
        receive(22);QVERIFY(c->hasLayoutWidgets());QVERIFY(!c->layoutStructurallyModified());QVERIFY(c->layoutNeedsSave());QVERIFY(!w.close());QCOMPARE(w.currentMode(),Viewer);QCOMPARE(d.closes,1);
        QTemporaryDir dir;QVERIFY(c->saveLayoutToJson(dir.filePath("rx.json")));receive(23);QCOMPARE(w.findChildren<Depth128267*>().size(),2);QVERIFY(!c->layoutNeedsSave());QVERIFY(w.close());QCOMPARE(d.closes,1);
    }
    void setupOptionDraftSaveCancelAndShutdown() {
        MainWindow w;Dialogs d;w.show();auto *c=controller(w);add(w);
        action(w,"actionOpen_Setup_Form")->trigger();auto *setup=w.findChild<DialogSetup*>();auto *option=setup->findChild<QCheckBox*>("checkBoxPromptSaveLayoutOnClose");QVERIFY(option);QVERIFY(option->isChecked());QCOMPARE(option->text(),QString("Prompt to save modified Layout when closing"));
        option->setChecked(false);setup->reject();action(w,"actionOpen_Setup_Form")->trigger();QVERIFY(option->isChecked());
        option->setChecked(false);setup->findChild<QPushButton*>("pushButtonSave")->click();AppSettings s(APP_COMPANY,APP_NAME);QCOMPARE(s.value("Preference/PromptSaveLayoutOnClose").toBool(),false);
        option->setChecked(true);setup->findChild<QPushButton*>("pushButtonLoad")->click();QVERIFY(!option->isChecked());
        option->setChecked(true);d.draftChoice=QMessageBox::Cancel;QVERIFY(!w.close());QCOMPARE(d.drafts,1);QCOMPARE(d.closes,0);QVERIFY(setup->isVisible());QVERIFY(option->isChecked());
        d.draftChoice=QMessageBox::Save;d.closeChoice=QMessageBox::Cancel;QVERIFY(!w.close());QCOMPARE(d.drafts,2);QCOMPARE(d.closes,1);QVERIFY(c->hasLayoutWidgets());QVERIFY(w.isVisible());s.sync();QCOMPARE(s.value("Preference/PromptSaveLayoutOnClose").toBool(),true);QCOMPARE(s.value("Preservation/Sentinel").toString(),QString("keep"));
        d.closeChoice=QMessageBox::Discard;QVERIFY(w.close());
    }
    void cancelledCloseStopsSendingAndKeepsBindings() {
        AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Nmea0183/Nmea0183Enabled",true);s.setValue("Nmea0183/Nmea0183Port","MEMORY");s.sync();
        auto *port=new MemoryPort;MainWindow w(nullptr,nullptr,port);Dialogs d;w.show();auto *c=controller(w);add(w);
        c->addWidgetAsComboboxTextChanged("HDT","Heading","Navigation");action(w,"actionStart_Sending")->setChecked(true);QTRY_VERIFY(w.findChild<NmeaSenderWorker*>()->isSending());
        QVERIFY(!w.close());QVERIFY(!w.findChild<NmeaSenderWorker*>()->isSending());QVERIFY(!action(w,"actionStart_Sending")->isChecked());const auto output=port->output;QTest::qWait(350);QCOMPARE(port->output,output);
        c->addGeneratorWidget("DateTime (PGN -129033)");auto *clock=w.findChild<GeneratorDateTime*>();QVERIFY(clock);clock->setSecondsSinceMidnight(12345);QVERIFY(c->hasLayoutWidgets());
        action(w,"actionViewer_Rx")->trigger();QCOMPARE(w.currentMode(),Viewer);action(w,"actionSimulator_Tx")->trigger();QCOMPARE(w.currentMode(),Sender);QVERIFY(!w.findChild<NmeaSenderWorker*>()->isSending());
    }
    void legacyCloseSaveCreatesCurrentCopy() {
        MainWindow w;Dialogs d;auto *c=controller(w);add(w);
        const auto legacy=AppPaths::legacyDirectory()+"/layouts/LegacyVessel.json";QVERIFY(QDir().mkpath(QFileInfo(legacy).absolutePath()));
        QVERIFY(c->saveLayoutToJson(legacy));const auto original=bytes(legacy);c->removeAllWidgets();QVERIFY(c->loadLayoutFromJson(legacy));add(w);
        d.closeChoice=QMessageBox::Save;d.overwrite=true;d.name="LegacyVessel";w.show();QVERIFY(w.close());
        QCOMPARE(d.defaults,QStringList{"LegacyVessel"});QCOMPARE(d.confirmations,1);QCOMPARE(bytes(legacy),original);
        QCOMPARE(c->layoutFilePath(),AppPaths::layoutSavePath("LegacyVessel"));QVERIFY(!c->layoutNeedsSave());QVERIFY(bytes(c->layoutFilePath())!=original);
    }
    void engineDraftCancelPrecedesLayoutDecision() {
        MainWindow w;Dialogs d;w.show();add(w);action(w,"actionEngineProfiles")->trigger();
        auto *editor=w.findChild<EngineProfileDialog*>();QVERIFY(editor);QVERIFY(editor->isVisible());
        auto *name=editor->findChild<QLineEdit*>("profileName");QVERIFY(name);const auto saved=name->text();name->setText(saved+" draft");
        QVERIFY(!w.close());QVERIFY(w.isVisible());QVERIFY(editor->isVisible());QCOMPARE(name->text(),saved+" draft");QCOMPARE(d.closes,0);
        d.draftChoice=QMessageBox::Discard;QVERIFY(!w.close());QCOMPARE(d.closes,1);QVERIFY(controller(w)->hasLayoutWidgets());QVERIFY(w.isVisible());
    }
    void removalStillPromptsWhenEmptyAndClearResetsIdentity() {
        MainWindow w;Dialogs d;auto *c=controller(w);add(w);QTemporaryDir dir;QVERIFY(c->saveLayoutToJson(dir.filePath("remove.json")));c->removeAllWidgetsFromGroup("Navigation");QVERIFY(!c->hasLayoutWidgets());QVERIFY(c->layoutNeedsSave());w.show();QVERIFY(!w.close());QCOMPARE(d.closes,1);c->removeAllWidgets();QVERIFY(!c->layoutNeedsSave());QVERIFY(c->layoutFilePath().isEmpty());QVERIFY(w.close());QCOMPARE(d.closes,1);
    }
};
int main(int argc,char **argv) {QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));LayoutIdentityCloseTests t;return QTest::qExec(&t,argc,argv);}
#include "layout_identity_close_tests.moc"
