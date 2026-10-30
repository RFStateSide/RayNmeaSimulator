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
#include "bundledtemplatesource.h"
#include "layoutdocument.h"
#include "mainwindow.h"
#include "maincontroller.h"
#include "templatevaluedelegate.h"
#include <QtTest>
#include <QAction>
#include <QAbstractButton>
#include <QInputDialog>
#include <QValidator>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QLabel>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QProcess>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <filesystem>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QByteArray bytes(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool put(const QString &path,const QByteArray &data){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size();}
QJsonObject document(){QJsonObject root;QString error;LayoutDocument::read(":/templates/synthetic-ais.json",root,error);return root;}
QAction *action(MainWindow &window,const QString &name){for(auto *a:window.findChildren<QAction*>())if(a->text()==name)return a;return nullptr;}
QPushButton *button(QDialog *dialog,const QString &name){for(auto *b:dialog->findChildren<QPushButton*>())if(b->text()==name)return b;return nullptr;}
void click(QAbstractButton *button){if(button)QTimer::singleShot(0,button,&QAbstractButton::click);}
void settle(){for(int i=0;i<8;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);}
struct Driver {
    QTimer timer;QElapsedTimer elapsed;bool timeout=false;
    Driver(QObject *owner,std::function<void(QDialog*)> step){elapsed.start();timer.setInterval(10);QObject::connect(&timer,&QTimer::timeout,owner,[this,step]{auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d)return;if(elapsed.elapsed()>15000){timeout=true;d->reject();return;}step(d);});timer.start();}
};
#ifdef QT_DEBUG
struct Project {
    QTemporaryDir home{QDir::currentPath()+"/build/m241c/project with spaces-XXXXXX"};
    bool initialize(){
        if(!home.isValid()||!QDir().mkpath(home.path()+"/resources/templates"))return false;
        const auto cmakeCatalog=bytes(QFINDTESTDATA("../resources/templates/templates.cmake"));
        const QByteArray cmakeProject="cmake_minimum_required(VERSION 3.24)\nproject(CatalogProbe LANGUAGES CXX)\nfind_package(Qt6 REQUIRED COMPONENTS Core)\nqt_standard_project_setup()\nqt_add_executable(catalog_probe probe.cpp)\ninclude(resources/templates/templates.cmake)\nraynmea_template_files(files FALSE)\nqt_add_resources(catalog_probe templates PREFIX /templates BASE \"${PROJECT_SOURCE_DIR}/resources/templates\" FILES ${files})\n";
        return !cmakeCatalog.isEmpty() && put(home.path()+"/resources/templates/templates.cmake",cmakeCatalog)
            && put(home.path()+"/CMakeLists.txt",cmakeProject)
            && put(home.path()+"/resources/templates/catalog.txt",bytes(QFINDTESTDATA("../resources/templates/catalog.txt")))
            && put(home.path()+"/probe.cpp","#include <QCoreApplication>\n#include <QFile>\nint main(int c,char**v){QCoreApplication a(c,v);return c>1&&QFile::exists(QString::fromUtf8(v[1]))?0:2;}\n");
    }
    QString root()const{return home.path();}
    void select(){AppSettings s("RFStateSide","RayNmeaSimulator");s.setValue("Authoring/TemplateProject",root());s.sync();}
};
#endif
}
class TemplateAuthoringTests:public QObject {
    Q_OBJECT
private slots:
    void init(){AppSettings s("RFStateSide","RayNmeaSimulator");s.clear();}
    void buildModeAndDefaultDestination(){MainWindow window;settle();auto *save=action(window,"Save as Template…");QVERIFY(save);bool visited=false;
        Driver driver(&window,[&](QDialog *dialog){if(dialog->windowTitle()!="Save Template"||visited)return;visited=true;auto *where=dialog->findChild<QComboBox*>("templateDestination");QVERIFY(where);QCOMPARE(where->currentData().toString(),QString("user"));
#ifdef QT_DEBUG
            QCOMPARE(where->count(),2);QVERIFY(dialog->findChild<QLineEdit*>("templateProjectRoot"));
            where->setCurrentIndex(1);QVERIFY(dialog->findChild<QLabel*>("templateOrigin")->text().startsWith("Bundled source"));where->setCurrentIndex(0);
            QVERIFY(BundledTemplateSource::project().isEmpty()); // A test cannot default to the real source tree.
#else
            QCOMPARE(where->count(),1);QVERIFY(!dialog->findChild<QLineEdit*>("templateProjectRoot"));
#endif
            click(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel));});
        save->trigger();QVERIFY(visited);QVERIFY(!driver.timeout);QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
    }
    void compiledDetailsAreReadOnly(){MainWindow window;settle();const auto original=bytes(":/templates/synthetic-ais.json");int phase=0;
        Driver driver(&window,[&](QDialog *dialog){
            if(dialog->windowTitle()=="Layout Templates") {
                auto *list=dialog->findChild<QListWidget*>("templateList");if(phase==0){for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==":/templates/synthetic-ais.json")list->setCurrentRow(i);
                    QVERIFY(!button(dialog,"Rename")->isEnabled());QVERIFY(!button(dialog,"Delete")->isEnabled());
#ifndef QT_DEBUG
                    QVERIFY(!dialog->findChild<QCheckBox*>("showBundledSources"));
#endif
                    phase=1;click(button(dialog,"View details…"));
                } else if(phase==2){phase=3;dialog->reject();}
            } else if(dialog->windowTitle().contains("read-only")&&phase==1){
                auto *table=dialog->findChild<QTableWidget*>("templateParameters");QVERIFY(table);QCOMPARE(table->editTriggers(),QAbstractItemView::EditTriggers(QAbstractItemView::NoEditTriggers));
                QVERIFY(!dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save));
                for(auto *combo:table->findChildren<QComboBox*>())QVERIFY(!combo->isEnabled());
                phase=2;click(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close));
            }
        });action(window,"Templates…")->trigger();QCOMPARE(phase,3);QVERIFY(!driver.timeout);QCOMPARE(bytes(":/templates/synthetic-ais.json"),original);
        QString error;QVERIFY(!LayoutDocument::write(":/templates/synthetic-ais.json",document(),error));
    }
    void numericPrecisionAndSyntax(){QTableWidget table(1,5);QTableWidgetItem item("3.6576123456789012");item.setData(TemplateValueDelegate::TypeRole,"double");table.setItem(0,2,item.clone());table.setItem(0,1,new QTableWidgetItem("Beam"));TemplateValueDelegate delegate;
        auto *editor=qobject_cast<QLineEdit*>(delegate.createEditor(&table,QStyleOptionViewItem(),table.model()->index(0,2)));QVERIFY(editor);delegate.setEditorData(editor,table.model()->index(0,2));QCOMPARE(editor->text(),item.text());
        QString valid="3.6576123456789012";int position=valid.size();QCOMPARE(editor->validator()->validate(valid,position),QValidator::Acceptable);
        QString invalid="nan";position=3;QCOMPARE(editor->validator()->validate(invalid,position),QValidator::Invalid);delete editor;
        table.item(0,2)->setData(TemplateValueDelegate::TypeRole,"uint64_t");editor=qobject_cast<QLineEdit*>(delegate.createEditor(&table,QStyleOptionViewItem(),table.model()->index(0,2)));QVERIFY(editor);
        valid="18446744073709551615";position=valid.size();QCOMPARE(editor->validator()->validate(valid,position),QValidator::Acceptable);invalid="4.5";position=3;QCOMPARE(editor->validator()->validate(invalid,position),QValidator::Invalid);delete editor;
    }
    void enumAndBooleanPreserveSavedValues(){QWidget parent;
        auto *boolean=TemplateValueDelegate::choices(&parent,"Enabled","bool","false");QVERIFY(boolean);QCOMPARE(boolean->currentData().toString(),QString("false"));
        auto *type=TemplateValueDelegate::choices(&parent,"VesselType","uint8_t","37");QVERIFY(type);QCOMPARE(type->currentData().toInt(),37);QVERIFY(type->currentText().contains("Pleasure"));
        auto *unknown=TemplateValueDelegate::choices(&parent,"TimeSource","tN2kTimeSource","14");QVERIFY(unknown);QCOMPARE(unknown->currentData().toString(),QString("14"));
        QVERIFY(!TemplateValueDelegate::choices(&parent,"Beam","double","3.6576"));
    }
    void editorValidationAndCancel(){MainWindow window;settle();auto *controller=window.findChild<MainController*>();auto root=document();root["documentType"]="layout";root.remove("template");root.remove("setupPolicies");QVERIFY(controller->loadLayoutDocument(root,{},[](const QString &){return true;}));const auto before=controller->captureLayout();int phase=0;
        Driver driver(&window,[&](QDialog *dialog){if(dialog->windowTitle()!="Save Template")return;auto *table=dialog->findChild<QTableWidget*>("templateParameters");
            if(phase==0){dialog->findChild<QLineEdit*>("templateName")->setText("Invalid disposable");for(int r=0;r<table->rowCount();++r)if(table->item(r,1)->text()=="Length")table->item(r,2)->setText("-2");phase=1;click(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save));}
            else if(phase==1 && dialog->findChild<QLabel*>("templateValidationError")->text().contains("Length")){phase=2;click(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel));}
        });action(window,"Save as Template…")->trigger();QCOMPARE(phase,2);QVERIFY(!driver.timeout);QCOMPARE(controller->captureLayout(),before);QString error;QVERIFY(!QFileInfo::exists(LayoutDocument::savePath("Invalid disposable",error)));
    }
#ifdef QT_DEBUG
    void actualRepositoryCatalog() {
        const auto catalogFile=QFINDTESTDATA("../resources/templates/templates.cmake");
        QVERIFY(!catalogFile.isEmpty());
        QDir source(QFileInfo(catalogFile).absolutePath());
        QVERIFY(source.cdUp());QVERIFY(source.cdUp());
        const auto root=source.canonicalPath();
        const auto catalog=bytes(catalogFile);
        QVERIFY(!catalog.isEmpty());
        QMap<QString,QByteArray> before;
        const auto directory=root+"/resources/templates";
        const auto originalNames=QDir(directory).entryList(QDir::Files);
        for(const auto &name:originalNames)before[name]=bytes(directory+"/"+name);
        QString error;
        QCOMPARE(BundledTemplateSource::directory(root,error),directory);
        const auto target=BundledTemplateSource::savePath(root,"Read-only catalog acceptance probe",error);
        QCOMPARE(target,directory+"/Read-only catalog acceptance probe.json");
        QVERIFY(!QFileInfo::exists(target));
        QVERIFY(BundledTemplateSource::contains(root,target)); // Valid Debug source destination, without writing it.
        QVERIFY(!BundledTemplateSource::contains(root,":/templates/synthetic-ais.json"));
        QVERIFY(!BundledTemplateSource::contains(root,root+"/outside.json"));
        MainWindow window;settle();bool visited=false;
        Driver driver(&window,[&](QDialog *dialog){
            if(dialog->windowTitle()!="Save Template"||visited)return;visited=true;
            auto *destination=dialog->findChild<QComboBox*>("templateDestination");QVERIFY(destination);
            QCOMPARE(destination->currentData().toString(),QString("user"));
            QVERIFY(destination->findData("bundled")>=0); // Debug requires an explicit authoring choice.
            QVERIFY(BundledTemplateSource::project().isEmpty()); // Never select the real repository in test settings.
            click(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel));
        });action(window,"Save as Template…")->trigger();QVERIFY(visited);QVERIFY(!driver.timeout);
        QCOMPARE(QDir(directory).entryList(QDir::Files),originalNames);
        for(auto it=before.cbegin();it!=before.cend();++it)QCOMPARE(bytes(directory+"/"+it.key()),it.value());
        Project invalid;QVERIFY(invalid.initialize());
        const auto invalidCatalog=bytes(QFINDTESTDATA("fixtures/template-catalog-invalid.cmake"));
        QVERIFY(!invalidCatalog.isEmpty());
        QVERIFY(put(invalid.root()+"/resources/templates/templates.cmake",invalidCatalog));
        QVERIFY(BundledTemplateSource::directory(invalid.root(),error).isEmpty());
        QVERIFY(BundledTemplateSource::savePath(invalid.root(),"Rejected",error).isEmpty());
    }
    void catalogMarker_data() {
        QTest::addColumn<QByteArray>("catalog");QTest::addColumn<bool>("accepted");
        const QByteArray marker="# RayNmeaSimulator bundled Template catalog (parallel CMake authoring).";
        QTest::newRow("first-line")<<marker+"\n"<<true;
        QTest::newRow("license-prefixed")<<QByteArray("# Copyright; SPDX-License-Identifier: AGPL-3.0-or-later\n\n")+marker+"\n"<<true;
        QTest::newRow("whitespace-crlf")<<QByteArray("\r\n# License\r\n  ")+marker+"\t\r\n"<<true;
        QTest::newRow("missing")<<QByteArray("# unrelated catalog\n")<<false;
        QTest::newRow("incorrect")<<QByteArray("# RayNmeaSimulator bundled Template catalog WRONG\n")<<false;
        QTest::newRow("embedded-string")<<QByteArray("set(text \"")+marker+"\")\n"<<false;
        QTest::newRow("embedded-comment")<<QByteArray("# unrelated ")+marker+"\n"<<false;
        QTest::newRow("trailing-text")<<marker+" unrelated\n"<<false;
        QTest::newRow("separate-negative-fixture")<<bytes(QFINDTESTDATA("fixtures/template-catalog-invalid.cmake"))<<false;
    }
    void catalogMarker() {
        QFETCH(QByteArray,catalog);QFETCH(bool,accepted);Project p;QVERIFY(p.initialize());
        QVERIFY(put(p.root()+"/resources/templates/templates.cmake",catalog));QString error;
        QCOMPARE(!BundledTemplateSource::directory(p.root(),error).isEmpty(),accepted);
        if(!accepted)QVERIFY(!error.isEmpty());
    }
    void catalogProtections_data() {
        QTest::addColumn<QString>("defect");
        for(const auto &defect:{"relative-root","missing-root","missing-catalog","missing-marker-file","oversize-catalog","oversize-project","missing-reference","root-link","resources-link","templates-link","catalog-link","marker-link","project-link"})
            QTest::newRow(defect)<<QString::fromLatin1(defect);
    }
    void catalogProtections() {
        QFETCH(QString,defect);Project p;QVERIFY(p.initialize());QString root=p.root(),error;
        const auto catalog=root+"/resources/templates/templates.cmake";
        const auto project=root+"/CMakeLists.txt";
        if(defect=="relative-root")root="relative-project";
        else if(defect=="missing-root")root+="/missing";
        else if(defect=="missing-catalog")QVERIFY(QFile::remove(catalog));
        else if(defect=="missing-marker-file")QVERIFY(QFile::remove(root+"/resources/templates/catalog.txt"));
        else if(defect=="oversize-catalog")QVERIFY(put(catalog,bytes(catalog)+QByteArray(65536,' ')));
        else if(defect=="oversize-project")QVERIFY(put(project,bytes(project)+QByteArray(1024*1024,' ')));
        else if(defect=="missing-reference")QVERIFY(put(project,"project(Unrelated)\n"));
        else {
            QString link;
            if(defect=="root-link")link=root+"-link";
            else if(defect=="resources-link")link=root+"/resources";
            else if(defect=="templates-link")link=root+"/resources/templates";
            else if(defect=="catalog-link")link=catalog;
            else if(defect=="marker-link")link=root+"/resources/templates/catalog.txt";
            else link=project;
            const auto target=defect=="root-link"?root:link+"-target";
            if(defect!="root-link")QVERIFY(QDir().rename(link,target));
#ifdef Q_OS_WIN
            const DWORD flags=(QFileInfo(target).isDir()?SYMBOLIC_LINK_FLAG_DIRECTORY:0)|0x2; // Allow unprivileged creation on supported Windows.
            QVERIFY2(CreateSymbolicLinkW(reinterpret_cast<LPCWSTR>(link.utf16()),reinterpret_cast<LPCWSTR>(target.utf16()),flags),
                     qPrintable(QString("CreateSymbolicLinkW failed: %1").arg(GetLastError())));
#else
            std::error_code ec;
            if(QFileInfo(target).isDir())std::filesystem::create_directory_symlink(target.toStdString(),link.toStdString(),ec);
            else std::filesystem::create_symlink(target.toStdString(),link.toStdString(),ec);
            QVERIFY2(!ec,qPrintable(QString::fromStdString(ec.message())));
#endif
            if(defect=="root-link")root=link;
        }
        QVERIFY(BundledTemplateSource::directory(root,error).isEmpty());QVERIFY(!error.isEmpty());
        if(defect=="root-link") {
#ifdef Q_OS_WIN
            QVERIFY(QDir().rmdir(root));
#else
            QVERIFY(std::filesystem::remove(root.toStdString()));
#endif
        }
    }
    void sourceCrudAndBackups(){Project p;QVERIFY(p.initialize());p.select();QString error;const auto path=BundledTemplateSource::savePath(p.root(),"Created source",error);QVERIFY2(!path.isEmpty(),qPrintable(error));const auto snapshot=bytes(":/templates/synthetic-ais.json");
        auto root=document();QVERIFY(BundledTemplateSource::write(p.root(),path,root,error));const auto first=bytes(path);auto meta=root["template"].toObject();meta["description"]="Changed source";root["template"]=meta;QVERIFY(BundledTemplateSource::write(p.root(),path,root,error));
        const auto backups=QDir(p.root()+"/build/template-authoring-backups").entryList({"*.json"},QDir::Files);QCOMPARE(backups.size(),1);QCOMPARE(bytes(p.root()+"/build/template-authoring-backups/"+backups.first()),first);
        QVERIFY(!BundledTemplateSource::write(p.root(),p.root()+"/outside.json",root,error));QVERIFY(BundledTemplateSource::savePath(p.root(),"../escape",error).isEmpty());
        QVERIFY(BundledTemplateSource::rename(p.root(),path,"Renamed source",error));QVERIFY(!QFileInfo::exists(path));const auto renamed=BundledTemplateSource::savePath(p.root(),"Renamed source",error);QJsonObject saved;QVERIFY(LayoutDocument::read(renamed,saved,error));QCOMPARE(saved["template"].toObject()["id"],root["template"].toObject()["id"]);QCOMPARE(saved["template"].toObject()["name"].toString(),QString("Renamed source"));
        QVERIFY(BundledTemplateSource::remove(p.root(),renamed,error));QVERIFY(BundledTemplateSource::files(p.root(),error).isEmpty());QCOMPARE(bytes(":/templates/synthetic-ais.json"),snapshot);
    }
    void sourceFailurePreservesOriginal(){Project p;QVERIFY(p.initialize());QString error;const auto path=BundledTemplateSource::savePath(p.root(),"Keep",error);QVERIFY(BundledTemplateSource::write(p.root(),path,document(),error));const auto original=bytes(path);QVERIFY(put(p.root()+"/build","obstruction"));
        QVERIFY(!BundledTemplateSource::write(p.root(),path,document(),error));QCOMPARE(bytes(path),original);QVERIFY(!BundledTemplateSource::remove(p.root(),path,error));QCOMPARE(bytes(path),original);QVERIFY(!BundledTemplateSource::rename(p.root(),path,"Other",error));QCOMPARE(bytes(path),original);
        QVERIFY(BundledTemplateSource::directory(p.root()+"/missing",error).isEmpty());
    }
    void authoringUiRequiresExplicitChoice(){Project p;QVERIFY(p.initialize());p.select();MainWindow window;settle();int phase=0;QString sourcePath;
        Driver driver(&window,[&](QDialog *dialog){
            if(auto *message=qobject_cast<QMessageBox*>(dialog);message && message->windowTitle()=="Save bundled Template source" && phase==1){phase=2;click(message->button(QMessageBox::Yes));return;}
            if(dialog->windowTitle()!="Save Template"||phase!=0)return;auto *destination=dialog->findChild<QComboBox*>("templateDestination");QCOMPARE(destination->currentData().toString(),QString("user"));destination->setCurrentIndex(1);dialog->findChild<QLineEdit*>("templateName")->setText("Authored source");sourcePath=p.root()+"/resources/templates/Authored source.json";phase=1;click(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save));
        });action(window,"Save as Template…")->trigger();QCOMPARE(phase,2);QVERIFY(!driver.timeout);QVERIFY(QFileInfo::exists(sourcePath));QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());QVERIFY(window.findChild<MainController*>()->layoutFilePath().isEmpty());
        // The source is separate from both the compiled catalog and user documents.
        QVERIFY(!LayoutDocument::templates().contains(sourcePath));QString error;QVERIFY(BundledTemplateSource::files(p.root(),error).contains(sourcePath));
    }
    void sourceManagerEditRenameDelete(){Project p;QVERIFY(p.initialize());p.select();QString error;auto root=document();auto meta=root["template"].toObject();meta["name"]="Manage source";root["template"]=meta;const auto original=BundledTemplateSource::savePath(p.root(),"Manage source",error);QVERIFY(BundledTemplateSource::write(p.root(),original,root,error));const auto renamed=BundledTemplateSource::savePath(p.root(),"Changed source",error);MainWindow window;settle();int phase=0;
        Driver driver(&window,[&](QDialog *dialog){
            if(auto *message=qobject_cast<QMessageBox*>(dialog)){if(phase==2){phase=3;click(message->button(QMessageBox::Yes));}else if(phase==5){phase=6;click(message->button(QMessageBox::Yes));}else if(phase==7){phase=8;click(message->button(QMessageBox::Yes));}return;}
            if(dialog->windowTitle()=="Layout Templates"){
                auto *show=dialog->findChild<QCheckBox*>("showBundledSources");QVERIFY(show);show->setChecked(true);auto *list=dialog->findChild<QListWidget*>("templateList");const auto selected=phase<6?original:renamed;
                for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==selected)list->setCurrentRow(i);
                if(phase==0){phase=1;click(button(dialog,"Edit details…"));}
                else if(phase==3){phase=4;click(button(dialog,"Rename"));}
                else if(phase==6){phase=7;click(button(dialog,"Delete"));}
                else if(phase==8){phase=9;dialog->reject();}
            } else if(dialog->windowTitle()=="Edit bundled Template source"&&phase==1){phase=2;auto *table=dialog->findChild<QTableWidget*>("templateParameters");for(int r=0;r<table->rowCount();++r)if(table->item(r,1)->text()=="Beam")table->item(r,2)->setText("3.6576123456789012");click(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save));}
            else if(auto *input=qobject_cast<QInputDialog*>(dialog);input&&phase==4){phase=5;input->setTextValue("Changed source");QTimer::singleShot(0,input,&QInputDialog::accept);}
        });action(window,"Templates…")->trigger();QCOMPARE(phase,9);QVERIFY(!driver.timeout);QVERIFY(!QFileInfo::exists(original));QVERIFY(!QFileInfo::exists(renamed));QVERIFY(QDir(p.root()+"/build/template-authoring-backups").entryList({"*.json"},QDir::Files).size()>=3);
    }
    void approvedCatalogExcludesUnapprovedRenameAndDeletion(){Project p;QVERIFY(p.initialize());
        // Exercise the real approval catalog. Extra saved sources remain editable,
        // but their creation/rename/deletion cannot change shipped snapshots.
        for(const QString name:{"Default","environment-weather","navigation-autopilot","single-engine","synthetic-ais","twin-engine"})
            QVERIFY(put(p.root()+"/resources/templates/"+name+".json",bytes(QFINDTESTDATA("../resources/templates/"+name+".json"))));
        QString error;const auto path=BundledTemplateSource::savePath(p.root(),"Added source",error);QVERIFY(BundledTemplateSource::write(p.root(),path,document(),error));QVERIFY(QDir().mkpath(p.root()+"/compile"));
        auto run=[&](const QString &tool,const QStringList &arguments){QProcess command;command.setWorkingDirectory(p.root()+"/compile");command.start(tool,arguments);if(!command.waitForFinished(60000))return -1;QFile log(p.root()+"/commands.txt");if(log.open(QIODevice::Append)){log.write(tool.toUtf8()+" "+arguments.join(' ').toUtf8()+"\n");log.write(command.readAllStandardOutput());log.write(command.readAllStandardError());}return command.exitCode();};
#ifdef Q_OS_WIN
        const auto executableName=QString("catalog_probe.exe");
#else
        const auto executableName=QString("catalog_probe");
#endif
        const auto cmake=QStandardPaths::findExecutable("cmake");
        QVERIFY(!cmake.isEmpty());
        QString executable;
        auto build=[&]{
            if(run(cmake,{"-S",p.root(),"-B",p.root()+"/compile","-G","Ninja","-DCMAKE_BUILD_TYPE=Release","-DCMAKE_PREFIX_PATH="+QLibraryInfo::path(QLibraryInfo::PrefixPath)})!=0
                ||run(cmake,{"--build",p.root()+"/compile","--parallel","2"})!=0)return false;
            QDirIterator found(p.root()+"/compile",{executableName},QDir::Files,QDirIterator::Subdirectories);if(!found.hasNext())return false;executable=found.next();return true;};
        QVERIFY(build());QCOMPARE(run(executable,{":/templates/Default.json"}),0);QCOMPARE(run(executable,{":/templates/Added source.json"}),2);
        QVERIFY(BundledTemplateSource::rename(p.root(),path,"Renamed source",error));QVERIFY(build());QCOMPARE(run(executable,{":/templates/Added source.json"}),2);QCOMPARE(run(executable,{":/templates/Renamed source.json"}),2);
        const auto renamed=BundledTemplateSource::savePath(p.root(),"Renamed source",error);QVERIFY(BundledTemplateSource::remove(p.root(),renamed,error));QVERIFY(build());QCOMPARE(run(executable,{":/templates/Renamed source.json"}),2);
    }
#endif
};
int main(int argc,char **argv){QDir().mkpath(QDir::currentPath()+"/build/m241c");QTemporaryDir profile(QDir::currentPath()+"/build/m241c/profile-XXXXXX");qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));TemplateAuthoringTests tests;return QTest::qExec(&tests,argc,argv);}
#include "template_authoring_tests.moc"
