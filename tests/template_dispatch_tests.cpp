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

#include "appsettings.h"
#include "layoutdocument.h"
#include "maincontroller.h"
#include "mainwindow.h"
#include "datetime129033.h"
#include "aisclassbstatic129810.h"
#include <QCheckBox>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QNetworkProxy>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>

namespace {
struct Workspace {
    QMdiArea area;QTableView table;QTreeView tree;
    PgnTableController tables{&table};PgnTreeController trees{&tree};
    MainController controller{&tables,&trees,&area,nullptr};
    Workspace(){controller.setMyVessel(MyVessel{});controller.setUserPrefs(UserUnitPrefs{});controller.setUserOptionPrefs(UserOptionPrefs{});
        for(const QString group:{"Generate","Navigation","Propulsion","Environment","Tanks","Electrical","Other","Communication"})controller.addSubWindowForGroup(group,group);}
    bool load(const QJsonObject &root){return controller.loadLayoutDocument(root,{},[](const QString &){return true;});}
};
QJsonObject read(const QString &path){QJsonObject root;QString error;LayoutDocument::read(path,root,error);return root;}
QJsonObject clockLayout(){auto root=read(":/defaults/defaultLayout.json");QJsonArray groups;
    for(const auto &v:root["layouts"].toArray()){auto g=v.toObject();QJsonArray widgets;for(const auto &w:g["widgets"].toArray())if(w.toObject()["type"]=="GeneratorDateTime"||w.toObject()["type"]=="DateTime129033")widgets.append(w);if(!widgets.isEmpty()){g["widgets"]=widgets;groups.append(g);}}
    root["layouts"]=groups;return root;}
QString value(const QJsonObject &root,int id,const QString &field){for(const auto &g:root["layouts"].toArray())for(const auto &v:g.toObject()["widgets"].toArray()){auto w=v.toObject();if(w["idX"].toInt()==id)for(const auto &row:w["parameters"].toArray())if(row.toArray()[0]==field)return row.toArray()[1].toString();}return {};}
QJsonObject policyTemplate(const QString &field){auto root=read(":/templates/synthetic-ais.json");root["setupPolicies"]=QJsonArray{QJsonObject{{"widgetId",1},{"field",field},{"setupKey",LayoutDocument::mapping("129810",field)}}};return root;}
void settle(){for(int i=0;i<8;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);}
}
class TemplateDispatchTests:public QObject {
    Q_OBJECT
private slots:
    void init(){AppSettings s("RFStateSide","RayNmeaSimulator");s.clear();}
    void bundled_data(){QTest::addColumn<QString>("path");for(const auto &path:LayoutDocument::templates())QTest::newRow(qPrintable(path))<<path;}
    void bundled(){QFETCH(QString,path);Workspace w;QTemporaryDir dir(QDir::currentPath()+"/build/m241b/test-XXXXXX");QSettings s(dir.filePath("s.ini"),QSettings::IniFormat);QJsonObject resolved;QString error;
        const auto raw=read(path);
        for(const auto &group:raw["layouts"].toArray()) {QSet<QPair<int,int>> cells;
            for(const auto &item:group.toObject()["widgets"].toArray()) {const auto widget=item.toObject();
                for(int row=widget["row"].toInt();row<widget["row"].toInt()+widget["rowSpan"].toInt();++row)
                    for(int column=widget["column"].toInt();column<widget["column"].toInt()+widget["columnSpan"].toInt();++column) {
                        const QPair<int,int> cell{row,column};QVERIFY(!cells.contains(cell));cells.insert(cell);
                    }
            }
        }
        QVERIFY2(LayoutDocument::resolve(raw,s,resolved,error),qPrintable(error));QVERIFY2(w.controller.validateDocument(resolved,&error),qPrintable(error));QVERIFY(w.load(resolved));
        QCOMPARE(w.controller.layoutFilePath(),QString());QVERIFY(w.controller.layoutNeedsSave());
        for(auto *engine:w.area.findChildren<GeneratorEngine*>())QVERIFY(!engine->EngineRunning());
        for(auto *movement:w.area.findChildren<GeneratorVesselMovement*>())QVERIFY(!movement->navigationControlActive());
        QCOMPARE(read(path),raw);
    }
    void mappings_data(){QTest::addColumn<QString>("field");QTest::addColumn<QVariant>("setup");QTest::addColumn<QString>("expected");
        QTest::newRow("mmsi")<<QString("UserID")<<QVariant("234567890")<<QString("234567890");
        QTest::newRow("type")<<QString("VesselType")<<QVariant(37)<<QString("37");
        QTest::newRow("callsign")<<QString("Callsign")<<QVariant("TEST123")<<QString("TEST123");
        QTest::newRow("fractional-length")<<QString("Length")<<QVariant(10.668)<<QString::number(10.668,'g',17);
        QTest::newRow("fractional-beam")<<QString("Beam")<<QVariant(3.6576)<<QString::number(3.6576,'g',17);
        QTest::newRow("unconfigured-mmsi")<<QString("UserID")<<QVariant("0")<<QString("0");
        QTest::newRow("unavailable-mmsi")<<QString("UserID")<<QVariant("4294967295")<<QString("4294967295");
        QTest::newRow("empty-callsign")<<QString("Callsign")<<QVariant("")<<QString("");
        QTest::newRow("unavailable-dimension")<<QString("Length")<<QVariant(-1000000000.)<<QString("-1000000000");
    }
    void mappings(){QFETCH(QString,field);QFETCH(QVariant,setup);QFETCH(QString,expected);QTemporaryDir dir(QDir::currentPath()+"/build/m241b/test-XXXXXX");QSettings s(dir.filePath("s.ini"),QSettings::IniFormat);
        const auto original=policyTemplate(field);s.setValue("Vessel/"+LayoutDocument::mapping("129810",field),setup);QJsonObject resolved;QString error;
        QVERIFY2(LayoutDocument::resolve(original,s,resolved,error),qPrintable(error));QCOMPARE(value(resolved,1,field),expected);QCOMPARE(value(resolved,2,field),value(original,2,field));
        QVERIFY(!resolved.contains("setupPolicies"));QVERIFY(!resolved.contains("template"));
        s.clear();QJsonObject again;QVERIFY(LayoutDocument::resolve(resolved,s,again,error));QCOMPARE(again,resolved);QCOMPARE(original,policyTemplate(field));
        Workspace w;QVERIFY(w.load(resolved));QVERIFY(w.controller.saveLayoutToJson(dir.filePath("concrete.json")));const auto saved=read(dir.filePath("concrete.json"));QCOMPARE(value(saved,1,field),expected);
    }
    void invalidPolicies_data(){QTest::addColumn<int>("kind");for(int n=0;n<10;++n)QTest::newRow(qPrintable(QString::number(n)))<<n;}
    void invalidPolicies(){QFETCH(int,kind);QTemporaryDir dir(QDir::currentPath()+"/build/m241b/test-XXXXXX");QSettings s(dir.filePath("s.ini"),QSettings::IniFormat);auto raw=policyTemplate("UserID");s.setValue("Vessel/MMSI","234567890");auto policies=raw["setupPolicies"].toArray();auto p=policies[0].toObject();
        switch(kind){case 0:s.clear();break;case 1:s.setValue("Vessel/MMSI","bad");break;case 2:p["field"]="CANIdentity";break;case 3:p["widgetId"]=999;break;case 4:p["setupKey"]="Password";break;case 5:policies.append(p);break;case 6:p["widgetId"]=1.5;break;case 7:raw["formatVersion"]=1;break;case 8:raw["documentType"]="unknown";break;case 9:s.setValue("Vessel/MMSI","123");break;}
        policies[0]=p;raw["setupPolicies"]=policies;QJsonObject result;QString error;QVERIFY(!LayoutDocument::resolve(raw,s,result,error));QVERIFY(!error.isEmpty());
    }
    void nonfiniteAndIncompatible(){QTemporaryDir dir(QDir::currentPath()+"/build/m241b/test-XXXXXX");QSettings s(dir.filePath("s.ini"),QSettings::IniFormat);QJsonObject result;QString error;
        for(const QVariant &v:{QVariant("nan"),QVariant("inf"),QVariant(6553.4),QVariant(-1.)}){s.setValue("Vessel/Length",v);QVERIFY(!LayoutDocument::resolve(policyTemplate("Length"),s,result,error));}
        s.setValue("Vessel/VesselType",200);QVERIFY(!LayoutDocument::resolve(policyTemplate("VesselType"),s,result,error));
    }
    void cancelInvalidAndVersion(){Workspace w;QVERIFY(w.load(clockLayout()));auto *clock=w.area.findChild<GeneratorDateTime*>();QPointer<GeneratorDateTime> guard=clock;const auto before=w.controller.captureLayout();
        QVERIFY(!w.controller.loadLayoutDocument(clockLayout(),"other.json",[](const QString &){return false;}));QVERIFY(guard);QCOMPARE(w.controller.captureLayout(),before);
        for(int version:{0,1,3}){auto bad=clockLayout();bad["formatVersion"]=version;QVERIFY(!w.load(bad));QVERIFY(guard);}
        auto bad=clockLayout();bad["documentType"]="template";QVERIFY(!w.load(bad));QVERIFY(guard);
        for(const QString field:{"enabled","dispatch"}) {
            auto incomplete=clockLayout();auto groups=incomplete["layouts"].toArray();auto group=groups[0].toObject();auto widgets=group["widgets"].toArray();auto source=widgets[0].toObject();source.remove(field);widgets[0]=source;group["widgets"]=widgets;groups[0]=group;incomplete["layouts"]=groups;
            QVERIFY(!w.load(incomplete));QVERIFY(guard);
        }
        auto incomplete=read(":/defaults/defaultLayout.json");auto groups=incomplete["layouts"].toArray();auto group=groups[0].toObject();auto widgets=group["widgets"].toArray();auto engine=widgets[0].toObject();engine.remove("engineProfile");widgets[0]=engine;group["widgets"]=widgets;groups[0]=group;incomplete["layouts"]=groups;QVERIFY(!w.load(incomplete));QVERIFY(guard);
    }
    void dispatchClock(){Workspace w;QVERIFY(w.load(clockLayout()));auto *clock=w.area.findChild<GeneratorDateTime*>();auto *date=w.area.findChild<DateTime129033*>();QVERIFY(clock&&date);
        auto *box=clock->findChild<QCheckBox*>("checkBoxDispatch");QVERIFY(box);QVERIFY(box->isChecked());
        clock->setDispatch(false);QVERIFY(!box->isChecked());date->setSecondsSinceMidnight(321.);clock->setSecondsSinceMidnight(123.);settle();QCOMPARE(date->SecondsSinceMidnight(),321.);QCOMPARE(clock->SecondsSinceMidnight(),123.);
        clock->setDispatch(true);QCOMPARE(date->SecondsSinceMidnight(),321.);clock->setSecondsSinceMidnight(456.);settle();QCOMPARE(date->SecondsSinceMidnight(),456.);
        // A queued sample created before an off/on cycle must never be replayed.
        clock->setSecondsSinceMidnight(789.);clock->setDispatch(false);clock->setDispatch(true);settle();QCOMPARE(date->SecondsSinceMidnight(),456.);
        clock->setSecondsSinceMidnight(790.);settle();QCOMPARE(date->SecondsSinceMidnight(),790.);
        clock->setDispatch(false);const auto local=clock->SecondsSinceMidnight();const auto instrument=date->SecondsSinceMidnight();QTest::qWait(clock->RefreshRate()+60);QVERIFY(clock->SecondsSinceMidnight()!=local);settle();QCOMPARE(date->SecondsSinceMidnight(),instrument);
        clock->setEnabled(false);const auto held=clock->SecondsSinceMidnight();QTest::qWait(clock->RefreshRate()+60);QCOMPARE(clock->SecondsSinceMidnight(),held);
        clock->setEnabled(true);QTest::qWait(clock->RefreshRate()+60);QVERIFY(clock->SecondsSinceMidnight()!=held);QCOMPARE(date->SecondsSinceMidnight(),instrument);
    }
    void persistenceAndModes(){Workspace w;QVERIFY(w.load(clockLayout()));auto *clock=w.area.findChild<GeneratorDateTime*>();clock->setDispatch(false);clock->setEnabled(true);QTemporaryDir dir(QDir::currentPath()+"/build/m241b/test-XXXXXX");const auto path=dir.filePath("snapshot.json");QVERIFY(w.controller.saveLayoutToJson(path));const auto saved=read(path);QVERIFY(!w.controller.layoutNeedsSave());
        QVERIFY(w.controller.loadLayoutFromJson(path,[](const QString &){return true;}));clock=w.area.findChild<GeneratorDateTime*>();QVERIFY(!clock->dispatch());QVERIFY(clock->isEnabled());QCOMPARE(w.controller.layoutFilePath(),path);
        w.controller.setMode(Viewer);w.controller.setMode(Sender);QVERIFY(!clock->dispatch());QVERIFY(clock->isEnabled());QCOMPARE(read(path),saved);
        clock->setDispatch(true);auto *date=w.area.findChild<DateTime129033*>();QSignalSpy samples(date,&DateTime129033::SecondsSinceMidnightChanged);clock->setSecondsSinceMidnight(222.);settle();QCOMPARE(samples.count(),1);
        w.controller.removeWidgetFromSubForm(clock,"Generate");settle();QVERIFY(w.controller.loadLayoutFromJson(path,[](const QString &){return true;}));QVERIFY(!w.area.findChild<GeneratorDateTime*>()->dispatch());
    }
    void instanceRouting(){Workspace w;auto raw=read(":/templates/twin-engine.json");QTemporaryDir dir(QDir::currentPath()+"/build/m241b/test-XXXXXX");QSettings settings(dir.filePath("s.ini"),QSettings::IniFormat);QJsonObject layout;QString error;QVERIFY(LayoutDocument::resolve(raw,settings,layout,error));QVERIFY(w.load(layout));
        auto engines=w.area.findChildren<GeneratorEngine*>();auto instruments=w.area.findChildren<EngineRapid127488*>();QCOMPARE(engines.size(),2);QCOMPARE(instruments.size(),2);
        for(auto *engine:engines)engine->setDispatch(engine->EngineInstance()!=0);
        for(auto *engine:engines)engine->setEngineSpeed(1500+engine->EngineInstance());settle();
        for(auto *instrument:instruments)QCOMPARE(instrument->EngineSpeed(),instrument->EngineInstance()==0?0.:1501.);
        for(auto *engine:engines)if(engine->EngineInstance()==0){engine->setDispatch(true);engine->setEngineSpeed(1600);}
        settle();for(auto *instrument:instruments)QCOMPARE(instrument->EngineSpeed(),instrument->EngineInstance()==0?1600.:1501.);
    }
    void otherGeneratorFamilies_data(){QTest::addColumn<QString>("sourceType");QTest::addColumn<QString>("targetType");QTest::addColumn<QString>("sourceField");QTest::addColumn<QString>("targetField");QTest::addColumn<double>("sample");
        QTest::newRow("lower-unit")<<QString("GeneratorTransLowerUnit")<<QString("TransmissionDynamic127493")<<QString("OilPressure")<<QString("OilPressure")<<123456.;
        QTest::newRow("movement")<<QString("GeneratorVesselMovement")<<QString("LatLong129025")<<QString("Latitude")<<QString("Latitude")<<42.125;
        QTest::newRow("weather")<<QString("GeneratorWeather")<<QString("OutsideEnvironmental130310")<<QString("SeaTemperature")<<QString("WaterTemperature")<<285.125;
    }
    void otherGeneratorFamilies(){QFETCH(QString,sourceType);QFETCH(QString,targetType);QFETCH(QString,sourceField);QFETCH(QString,targetField);QFETCH(double,sample);
        auto root=read(":/defaults/defaultLayout.json");QJsonArray groups;
        for(const auto &v:root["layouts"].toArray()){auto g=v.toObject();QJsonArray widgets;for(const auto &item:g["widgets"].toArray()){const auto widget=item.toObject();if(widget["type"]!=sourceType && widget["type"]!=targetType)continue;bool second=false;for(const auto &row:widget["parameters"].toArray())if(row.toArray()[0]=="EngineInstance"&&row.toArray()[1]=="1")second=true;if(!second)widgets.append(widget);}if(!widgets.isEmpty()){g["widgets"]=widgets;groups.append(g);}}
        root["layouts"]=groups;Workspace w;QVERIFY(w.load(root));PgnWidgetBase *source=nullptr,*target=nullptr;
        for(auto *widget:w.area.findChildren<PgnWidgetBase*>()){if(widget->metaObject()->className()==sourceType)source=widget;if(widget->metaObject()->className()==targetType)target=widget;}
        QVERIFY(source&&target);source->setDispatch(false);const double held=target->property(targetField.toUtf8()).toDouble();
        QVERIFY(source->setProperty(sourceField.toUtf8(),sample));settle();QCOMPARE(source->property(sourceField.toUtf8()).toDouble(),sample);QCOMPARE(target->property(targetField.toUtf8()).toDouble(),held);
        source->setDispatch(true);QCOMPARE(target->property(targetField.toUtf8()).toDouble(),held);QVERIFY(source->setProperty(sourceField.toUtf8(),sample+1));settle();QCOMPARE(target->property(targetField.toUtf8()).toDouble(),sample+1);
    }
    void portablePathsAndWrites(){QTemporaryDir dir(QDir::currentPath()+"/build/m241b/test-XXXXXX");QString error;const auto root=read(":/templates/synthetic-ais.json");
        for(const auto &name:QStringList{"../escape","CON","bad/name","trailing ","bad.name"})QVERIFY(LayoutDocument::savePath(name,error).isEmpty());
        const auto path=LayoutDocument::savePath("Disposable test",error);QVERIFY(!path.isEmpty());QVERIFY(LayoutDocument::write(path,root,error));QCOMPARE(read(path),root);QCOMPARE(LayoutDocument::savePath("DISPOSABLE TEST",error),path);QVERIFY(LayoutDocument::templates().contains(path));
        QVERIFY(!LayoutDocument::write(":/templates/synthetic-ais.json",root,error));QVERIFY(!LayoutDocument::write(dir.filePath("absent/file.json"),root,error));QCOMPARE(read(path),root);QVERIFY(QFile::remove(path));
        Workspace w;QVERIFY(w.load(clockLayout()));const auto layoutPath=dir.filePath("bounded.json");QVERIFY(w.controller.saveLayoutToJson(layoutPath));const auto saved=read(layoutPath);
        w.area.findChild<GeneratorDateTime*>()->setWindowTitle(QString(4*1024*1024,'x'));
        QVERIFY(!w.controller.saveLayoutToJson(layoutPath));QCOMPARE(read(layoutPath),saved);
    }
    void templateUiExportAndInstantiate(){
        MainWindow window;settle();auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        QVERIFY(controller->loadLayoutDocument(clockLayout(),{},[](const QString &){return true;}));
        auto action=[&](const QString &label)->QAction* {for(auto *a:window.findChildren<QAction*>())if(a->text()==label)return a;return nullptr;};
        auto *save=action("Save as Template…");auto *browse=action("Templates…");QVERIFY(save&&browse);
        const auto identity=controller->layoutFilePath();const bool dirty=controller->layoutNeedsSave();bool saved=false;
        QTimer driver;driver.setInterval(10);
        connect(&driver,&QTimer::timeout,&window,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog || dialog->windowTitle()!="Save Template" || saved)return;
            auto *name=dialog->findChild<QLineEdit*>("templateName");auto *table=dialog->findChild<QTableWidget*>();if(!name||!table)return;
            name->setText("UI disposable");for(int r=0;r<table->rowCount();++r)if(table->item(r,1)->text()=="Dispatch")qobject_cast<QComboBox*>(table->cellWidget(r,2))->setCurrentIndex(0);
            dialog->grab().save("build/m241b/template-editor.png");
            saved=true;dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        });driver.start();save->trigger();driver.stop();QVERIFY(saved);QCOMPARE(controller->layoutFilePath(),identity);QCOMPARE(controller->layoutNeedsSave(),dirty);
        QString error;const auto path=LayoutDocument::savePath("UI disposable",error);QVERIFY(QFileInfo::exists(path));auto exported=read(path);QCOMPARE(exported["documentType"].toString(),QString("template"));
        // Start from an empty workspace so this verifies the real browser without
        // an unrelated destructive-operation prompt.
        controller->removeAllWidgets();bool applied=false;
        disconnect(&driver,nullptr,&window,nullptr);
        connect(&driver,&QTimer::timeout,&window,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog || dialog->windowTitle()!="Layout Templates" || applied)return;
            auto *list=dialog->findChild<QListWidget*>("templateList");if(!list)return;
            for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString()==path)list->setCurrentRow(i);
            dialog->grab().save("build/m241b/template-browser.png");
            for(auto *b:dialog->findChildren<QPushButton*>())if(b->text()=="Create Layout"){applied=true;b->click();return;}
        });driver.start();browse->trigger();driver.stop();QVERIFY(applied);
        auto *clock=window.findChild<GeneratorDateTime*>();QVERIFY(clock);QVERIFY(clock->isEnabled());QVERIFY(!clock->dispatch());QCOMPARE(controller->layoutFilePath(),QString());QVERIFY(controller->layoutNeedsSave());QCOMPARE(read(path),exported);
        QVERIFY(QFile::remove(path));
    }
    void manageUserTemplateUi(){
        QString error;auto root=read(":/templates/synthetic-ais.json");auto meta=root["template"].toObject();meta["name"]="Manage disposable";root["template"]=meta;
        const auto original=LayoutDocument::savePath("Manage disposable",error);QVERIFY(LayoutDocument::write(original,root,error));
        const auto renamed=LayoutDocument::savePath("Renamed disposable",error);const auto copied=LayoutDocument::savePath("Copied disposable",error);
        MainWindow window;settle();QAction *browse=nullptr;for(auto *a:window.findChildren<QAction*>())if(a->text()=="Templates…")browse=a;QVERIFY(browse);
        int phase=0;bool timedOut=false;QTimer driver;driver.setInterval(10);QElapsedTimer elapsed;elapsed.start();
        connect(&driver,&QTimer::timeout,&window,[&] {
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            if(elapsed.elapsed()>8000){timedOut=true;dialog->reject();return;}
            auto click=[&](const QString &text){for(auto *button:dialog->findChildren<QPushButton*>())if(button->text()==text){QTimer::singleShot(0,button,&QPushButton::click);return true;}return false;};
            if(dialog->windowTitle()=="Layout Templates") {
                auto *list=dialog->findChild<QListWidget*>("templateList");if(!list)return;const auto selected=phase<4?original:renamed;
                for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString()==selected)list->setCurrentRow(i);
                if(phase==0){phase=1;click("Edit details…");}
                else if(phase==2){phase=3;click("Rename");}
                else if(phase==4){phase=5;click("Duplicate");}
                else if(phase==6){phase=7;click("Delete");}
                else if(phase==8){phase=9;dialog->reject();}
            } else if(dialog->windowTitle()=="Edit Template" && phase==1) {
                auto *table=dialog->findChild<QTableWidget*>();for(int r=0;r<table->rowCount();++r)if(table->item(r,1)->text()=="Length")table->item(r,2)->setText("13.125");
                phase=2;dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
            } else if(auto *input=qobject_cast<QInputDialog*>(dialog);input && phase==3) {phase=4;input->setTextValue("Renamed disposable");input->accept();}
            else if(dialog->windowTitle()=="Save Template" && phase==5){phase=6;dialog->findChild<QLineEdit*>("templateName")->setText("Copied disposable");dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();}
            else if(auto *message=qobject_cast<QMessageBox*>(dialog);message && phase==7){phase=8;message->button(QMessageBox::Yes)->click();}
        });driver.start();browse->trigger();driver.stop();QVERIFY(!timedOut);QCOMPARE(phase,9);
        QVERIFY(!QFileInfo::exists(original));QVERIFY(!QFileInfo::exists(renamed));QVERIFY(QFileInfo::exists(copied));QCOMPARE(value(read(copied),1,"Length"),QString("13.125"));
        QVERIFY(read(copied)["template"].toObject()["id"]!=root["template"].toObject()["id"]);QCOMPARE(read(":/templates/synthetic-ais.json")["template"].toObject()["name"].toString(),QString("Synthetic AIS Demonstration"));QVERIFY(QFile::remove(copied));
    }
    void startupSelectionUsesFileIdentity(){
        QString error;auto root=read(":/templates/synthetic-ais.json");auto meta=root["template"].toObject();meta["name"]="Same title";meta["id"]="startup-first";root["template"]=meta;
        const auto first=LayoutDocument::savePath("Startup first",error),second=LayoutDocument::savePath("Startup second",error);QVERIFY(LayoutDocument::write(first,root,error));
        meta["id"]="startup-second";root["template"]=meta;QVERIFY(LayoutDocument::write(second,root,error));
        MainWindow window;settle();QAction *configure=nullptr;for(auto *a:window.findChildren<QAction*>())if(a->text()=="Startup Workspace…")configure=a;QVERIFY(configure);
        int phase=0;bool timedOut=false;QString observedPath;QElapsedTimer elapsed;elapsed.start();QTimer driver;driver.setInterval(10);
        connect(&driver,&QTimer::timeout,&window,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;if(elapsed.elapsed()>8000){timedOut=true;dialog->reject();return;}
            if(dialog->windowTitle()=="Startup Workspace" && phase==0){phase=1;dialog->findChild<QComboBox*>()->setCurrentIndex(2);for(auto *b:dialog->findChildren<QPushButton*>())if(b->text()=="Browse…"){QTimer::singleShot(0,b,&QPushButton::click);break;}}
            else if(dialog->windowTitle()=="Startup Template" && phase==1){phase=2;auto *targets=dialog->findChild<QComboBox*>();const int index=targets->findData(second);QVERIFY(index>=0);targets->setCurrentIndex(index);QTimer::singleShot(0,dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok),&QPushButton::click);}
            else if(dialog->windowTitle()=="Startup Workspace" && phase==2){observedPath=dialog->findChild<QLineEdit*>()->text();if(observedPath!=second)return;phase=3;QTimer::singleShot(0,dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),&QPushButton::click);}
        });driver.start();configure->trigger();driver.stop();QCOMPARE(observedPath,second);QVERIFY(!timedOut);QCOMPARE(phase,3);
        AppSettings settings("RFStateSide","RayNmeaSimulator");QCOMPARE(settings.value("Workspace/StartupMode").toInt(),2);QCOMPARE(settings.value("Workspace/StartupPath").toString(),second);
        QVERIFY(!window.findChild<MainController*>()->hasLayoutWidgets());QVERIFY(QFile::remove(first));QVERIFY(QFile::remove(second));
    }
    void startupAndSending_data(){QTest::addColumn<int>("mode");for(int mode:{0,1,2,3})QTest::newRow(qPrintable(QString::number(mode)))<<mode;}
    void startupAndSending(){QFETCH(int,mode);QTemporaryDir dir(QDir::currentPath()+"/build/m241b/test-XXXXXX");const auto path=dir.filePath("startup.json");QString error;QVERIFY(LayoutDocument::write(path,clockLayout(),error));AppSettings settings("RFStateSide","RayNmeaSimulator");settings.setValue("Workspace/StartupMode",mode==3?1:mode);settings.setValue("Workspace/StartupPath",mode==2?":/templates/navigation-autopilot.json":mode==3?dir.filePath("missing.json"):path);settings.sync();
        MainWindow window;settle();auto *controller=window.findChild<MainController*>();QVERIFY(controller);QCOMPARE(controller->hasLayoutWidgets(),mode==1||mode==2);
        QAction *stop=nullptr;for(auto *a:window.findChildren<QAction*>())if(a->text()=="Stop Sending")stop=a;QVERIFY(stop);QVERIFY(!stop->isEnabled());
        for(auto *source:window.findChildren<GeneratorDateTime*>()){source->setDispatch(false);source->setDispatch(true);source->setEnabled(true);}settle();QVERIFY(!stop->isEnabled());
        controller->setMode(Viewer);controller->setMode(Sender);QVERIFY(!stop->isEnabled());
    }
};
int main(int argc,char **argv){QTemporaryDir profile(QDir::currentPath()+"/m241b-profile-XXXXXX");qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());qputenv("RAYNMEA_TEST_CREDENTIALS","1");QDir().mkpath(QDir::currentPath()+"/build/m241b");QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));TemplateDispatchTests tests;return QTest::qExec(&tests,argc,argv);}
#include "template_dispatch_tests.moc"
