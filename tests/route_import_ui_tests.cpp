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
#include <QCheckBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QPushButton>
#include <QTabWidget>
#include <QSettings>
#include <QSet>
#include <QAction>
#include <QToolButton>
#include <QScopeGuard>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include "apppaths.h"
#include "mainwindow.h"
#include "generatorvesselmovement.h"
#include "appsettings.h"
#include "themes.h"
#include "routeexport.h"

namespace {
#ifdef Q_OS_WIN
// Qt's NativeFormat FileDialog settings use HKCU even in an isolated AppSettings
// profile. Redirect only this test process to a disposable volatile registry key.
class IsolatedUserRegistry {
    QString path=QString("Software\\RayNmeaRoutePickerTest-%1").arg(QCoreApplication::applicationPid());
    HKEY hive=nullptr;
public:
    IsolatedUserRegistry() {
        if(RegCreateKeyExW(HKEY_CURRENT_USER,reinterpret_cast<LPCWSTR>(path.utf16()),0,nullptr,
                           REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&hive,nullptr)!=ERROR_SUCCESS
           || RegOverridePredefKey(HKEY_CURRENT_USER,hive)!=ERROR_SUCCESS)
            qFatal("Cannot isolate test registry");
    }
    ~IsolatedUserRegistry() {
        if(RegOverridePredefKey(HKEY_CURRENT_USER,nullptr)!=ERROR_SUCCESS)qFatal("Cannot restore test registry mapping");
        RegCloseKey(hive);
        if(RegDeleteTreeW(HKEY_CURRENT_USER,reinterpret_cast<LPCWSTR>(path.utf16()))!=ERROR_SUCCESS)
            qFatal("Cannot remove isolated test registry");
    }
};
#endif
QString write(QTemporaryDir &dir, const QString &name, const QByteArray &bytes) {
    QFile f(dir.filePath(name));if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size())qFatal("Fixture write failed");return f.fileName();
}
void chooseFile(QFileDialog *dialog, const QString &path) {
    // selectFile() alone can leave a prefilled Save filename unchanged on Linux.
    // Exercise the same editor used by the operator before accepting the dialog.
    dialog->setDirectory(QFileInfo(path).absolutePath());
    auto *editor=dialog->findChild<QLineEdit*>("fileNameEdit");
    if(!editor)qFatal("Expected the nonnative file-dialog editor");
    editor->setText(QFileInfo(path).fileName());
    static_cast<QDialog*>(dialog)->accept();
}
class MemoryPort : public QSerialPort {
public:
    bool open(OpenMode mode) override {return QIODevice::open(mode|QIODevice::Unbuffered);}
    void close() override {QIODevice::close();}
    qint64 bytesToWrite() const override {return 0;}
protected:
    qint64 writeData(const char *,qint64 size) override {return size;}
};
const QByteArray gpx="<gpx><rte><rtept lat='42' lon='-70'/><rtept lat='42.01' lon='-70'/></rte></gpx>";
}
class RouteImportUiTests : public QObject {
    Q_OBJECT
private slots:
    void selectedExportSourceIsIndependent() {
        QTemporaryDir dir;GeneratorVesselMovement widget(1,"Fixture",nullptr);
        const auto file=write(dir,"old.gpx",gpx);
        widget.restoreLayoutConfiguration({{"route",file}});
        auto *pilot=widget.findChild<AutoPilotSimulator*>();QVERIFY(pilot->loadRoute(file));pilot->start();
        QSignalSpy loaded(pilot,&AutoPilotSimulator::routeLoaded),running(pilot,&AutoPilotSimulator::RunningStateChanged);
        QString error;auto snapshot=widget.selectedRouteForExport(&error);QVERIFY(error.isEmpty());QCOMPARE(snapshot.sourceFile,file);
        // Re-read source on export, rather than using pilot state or old file bytes.
        write(dir,"old.gpx","<gpx><rte><name>New file contents</name><rtept lat='3' lon='4'/><rtept lat='5' lon='6'/></rte></gpx>");
        snapshot=widget.selectedRouteForExport(&error);QVERIFY(error.isEmpty());QCOMPARE(snapshot.name,QString("New file contents"));
        QCOMPARE(snapshot.waypoints[0].coordinate,QGeoCoordinate(3,4));
        ReceivedRoutes::Route received;received.complete=true;received.name="Snapshot";
        Waypoint a,b;a.coordinate=QGeoCoordinate(10,20);b.coordinate=QGeoCoordinate(11,21);a.name="ID 65532";b.name="ID 7";received.waypoints={a,b};
        widget.setActiveRouteChooser([received](QWidget*){return received;});
        QTimer::singleShot(0,[]{for(auto *top:QApplication::topLevelWidgets())if(auto *d=qobject_cast<QInputDialog*>(top)){d->setTextValue("Select Chartplotter Route");d->accept();}});
        QVERIFY(QMetaObject::invokeMethod(&widget,"on_pushButtonSelectRouteFile_clicked"));
        snapshot=widget.selectedRouteForExport(&error);QVERIFY(error.isEmpty());QVERIFY(snapshot.sourceFile.isEmpty());
        QCOMPARE(snapshot.name,received.name);QCOMPARE(snapshot.waypoints[0].coordinate,a.coordinate);
        QCOMPARE(widget.layoutConfiguration()["route"].toString(),file); // hidden file still exists, but is not exported
        QVERIFY(pilot->isRunning());QCOMPARE(loaded.size(),0);QCOMPARE(running.size(),0);
        widget.restoreLayoutConfiguration({{"route",dir.filePath("missing.gpx")}});
        widget.selectedRouteForExport(&error);QVERIFY(!error.isEmpty());QVERIFY(pilot->isRunning());
    }
    void exportDialogWritesAndCancellation_data() {
        QTest::addColumn<bool>("kml");QTest::newRow("GPX")<<false;QTest::newRow("KML")<<true;
    }
    void exportDialogWritesAndCancellation() {
        QFETCH(bool,kml);QTemporaryDir dir;GeneratorVesselMovement widget(1,"Fixture",nullptr);
        widget.resize(650,850);widget.show();
        auto *autopilotTabs=widget.findChild<QTabWidget*>("tabWidget");QVERIFY(autopilotTabs);autopilotTabs->setCurrentIndex(1);
        QSignalSpy exportStatus(&widget,&GeneratorVesselMovement::ShowStatusBarMessage);
        const auto file=write(dir,"input.gpx",gpx);widget.restoreLayoutConfiguration({{"route",file}});
        const auto preview=qEnvironmentVariable("RAYNMEA_EXPORT_UI_DIR");
        if(!preview.isEmpty()) {
            QVERIFY(QDir().mkpath(preview));widget.resize(650,850);widget.show();
            auto *tabs=widget.findChild<QTabWidget*>("tabWidget");QVERIFY(tabs);tabs->setCurrentIndex(1);
            QTest::qWait(30);QVERIFY(widget.grab().save(preview+(kml?"/export-kml.png":"/export-gpx.png")));
        }
        auto *pilot=widget.findChild<AutoPilotSimulator*>();QVERIFY(pilot->loadRoute(file));pilot->start();
        const auto saved=widget.layoutConfiguration();widget.setDispatch(false);const auto epoch=widget.dispatchEpoch();
        const auto output=dir.filePath(kml?"out.kml":"out.gpx");
        const auto invoke=[&](int action,QMessageBox::StandardButton overwrite) {
            bool seen=false;QTimer answer;
            connect(&answer,&QTimer::timeout,this,[&]{for(auto *top:QApplication::topLevelWidgets()) {
                if(auto *d=qobject_cast<QFileDialog*>(top);d&&d->isVisible()) {
                    seen=true;d->selectNameFilter(kml?"KML route (*.kml)":"GPX route (*.gpx)");
                    if(action==0)d->reject();
                    else chooseFile(d,action==2?file:output);
                }
                if(auto *box=qobject_cast<QMessageBox*>(top);box&&box->isVisible()) {
                    if(box->standardButtons()&QMessageBox::Yes)box->button(overwrite)->click();else box->accept();
                }
            }});answer.start(10);QVERIFY(QMetaObject::invokeMethod(&widget,"exportSelectedRoute"));answer.stop();QVERIFY(seen);
        };
        invoke(0,QMessageBox::No);QVERIFY(!QFileInfo::exists(output));
        invoke(1,QMessageBox::Yes);const auto imported=RouteImport::read(output);
        QVERIFY2(imported,qPrintable(imported.error+"; "+(exportStatus.isEmpty()?QString():exportStatus.last()[0].toString())));QCOMPARE(imported.waypoints.size(),2);
        write(dir,kml?"out.kml":"out.gpx","keep destination");
        invoke(1,QMessageBox::No);QFile preserved(output);QVERIFY(preserved.open(QIODevice::ReadOnly));QCOMPARE(preserved.readAll(),QByteArray("keep destination"));preserved.close();
        invoke(1,QMessageBox::Yes);QVERIFY(RouteImport::read(output));
        invoke(2,QMessageBox::Yes);QFile source(file);QVERIFY(source.open(QIODevice::ReadOnly));QCOMPARE(source.readAll(),gpx);
        QCOMPARE(widget.layoutConfiguration(),saved);QVERIFY(pilot->isRunning());QVERIFY(!widget.dispatch());QCOMPARE(widget.dispatchEpoch(),epoch);
    }
    void exportDoesNotAuthorizeOrStopSending() {
        AppSettings settings(APP_COMPANY,APP_NAME);settings.clear();settings.setValue("Nmea2000/Nmea2000Enabled",false);
        settings.setValue("Nmea0183/Nmea0183Enabled",true);settings.setValue("Nmea0183/Nmea0183Port","MEMORY");settings.sync();
        MainWindow window(nullptr,nullptr,new MemoryPort);window.show();
        auto *start=window.findChild<QAction*>("actionStart_Sending");auto *worker=window.findChild<NmeaSenderWorker*>();
        auto *model=window.findChild<PgnTableModel*>();QVERIFY(start&&worker&&model);
        model->addPgn(99001,"Fixture","HDT",true,{{"Heading","1","rad","double"}}, {},250);
        QTemporaryDir dir;GeneratorVesselMovement widget(1,"Fixture",&window);widget.restoreLayoutConfiguration({{"route",write(dir,"input.gpx",gpx)}});
        auto cancel=[&]{QTimer::singleShot(0,[]{for(auto *top:QApplication::topLevelWidgets())if(auto *d=qobject_cast<QFileDialog*>(top))d->reject();});QVERIFY(QMetaObject::invokeMethod(&widget,"exportSelectedRoute"));};
        cancel();QVERIFY(!worker->isSending());start->trigger();QVERIFY(worker->isSending());
        cancel();QVERIFY(worker->isSending());QTest::keyClick(&window,Qt::Key_F12);QVERIFY(!worker->isSending());
    }
    void sidebarLocationsAndAliases() {
        QVERIFY(QDir().mkpath(AppPaths::routesDirectory()+"/gpx"));
        QVERIFY(QDir().mkpath(AppPaths::routesDirectory()+"/kml"));
        QVERIFY(QDir().mkpath(AppPaths::legacyDirectory()+"/routes"));
        const auto urls=AppPaths::routeSidebarUrls();QVERIFY(urls.size()<=4);
        QVERIFY(urls.contains(QUrl("file:")));
        QVERIFY(urls.contains(QUrl::fromLocalFile(QFileInfo(AppPaths::routesDirectory()).canonicalFilePath())));
        QVERIFY(urls.contains(QUrl::fromLocalFile(QFileInfo(AppPaths::legacyDirectory()).canonicalFilePath())));
        QSet<QUrl> unique;for(const auto &url:urls){QVERIFY(!unique.contains(url));unique.insert(url);}
        QCOMPARE(AppPaths::routeSidebarUrls(),urls); // Getter has no registration side effects.
        QTemporaryDir external;const auto route=write(external,"external.GPX",gpx);
        QCOMPARE(AppPaths::routeDialogDirectory(route),external.path());
        QVERIFY(QFileInfo(AppPaths::routesDirectory()+"/gpx").isDir());
        QVERIFY(QFileInfo(AppPaths::routesDirectory()+"/kml").isDir());
#ifndef Q_OS_WIN
        // Canonical aliases of the same route location do not create a second shortcut.
        QVERIFY(QDir(AppPaths::legacyDirectory()+"/routes").isEmpty());
        QVERIFY(QDir().rmdir(AppPaths::legacyDirectory()+"/routes"));
        QVERIFY(QFile::link(AppPaths::routesDirectory(),AppPaths::legacyDirectory()+"/routes"));
        const auto aliases=AppPaths::routeSidebarUrls();QCOMPARE(aliases.size(),urls.size()-1);
        QVERIFY(QFile::remove(AppPaths::legacyDirectory()+"/routes"));
#endif
    }
    void repeatedPickerWithPersistedSidebar_data() {
        QTest::addColumn<bool>("exporting");
        QTest::newRow("Select Route File") << false;
        QTest::newRow("Export Route") << true;
    }
    void repeatedPickerWithPersistedSidebar() {
        QFETCH(bool,exporting);
        QTemporaryDir directory;
        QVERIFY(QDir().mkpath(AppPaths::routesDirectory()+"/gpx"));
        QVERIFY(QDir().mkpath(AppPaths::routesDirectory()+"/kml"));
        const QString legacyRoutes=AppPaths::legacyDirectory()+"/routes";
        QVERIFY(QDir().mkpath(legacyRoutes));
        QSettings qt(QSettings::UserScope,"QtProject");
        // NativeFormat is isolated before QApplication on both platforms.
        QStringList polluted{"file:",QUrl::fromLocalFile(QDir::homePath()).toString()};
        for(int i=0;i<26;++i){const auto path=directory.filePath(QString::number(i)+"/routes");QVERIFY(QDir().mkpath(path));polluted.append(QUrl::fromLocalFile(path).toString());}
        {QFileDialog seed;seed.setSidebarUrls(QUrl::fromStringList(polluted));} // Save a complete valid Qt dialog state.
        qt.setValue("FileDialog/shortcuts",polluted);
        qt.setValue("FileDialog/sidebarWidth",180);qt.sync();
        {
            QFileDialog old;
#ifndef Q_OS_WIN
            QCOMPARE(old.sidebarUrls().size(),polluted.size()); // Ubuntu reproduction of the persisted-list defect.
#else
            // Windows does not restore this fixture's sidebar identically.
            // The application's curated policy below must work independently.
            qInfo()<<"Windows Qt inherited sidebar entries:"<<old.sidebarUrls().size();
#endif
        }
        const auto original=qt.value("FileDialog/shortcuts");
        GeneratorVesselMovement widget(1,"Vessel Movement",nullptr);widget.show();
        const auto route=write(directory,"selected.GPX",gpx);
        auto *edit=widget.findChild<QLineEdit*>("lineEditFilePath");
        widget.restoreLayoutConfiguration({{"route",route}});
        QList<QUrl> previous;int opens=0;
        for(int attempt=0;attempt<5;++attempt) {
            QTimer::singleShot(0,[&]{for(auto *top:QApplication::topLevelWidgets())if(auto *d=qobject_cast<QFileDialog*>(top);d&&d->isVisible()){
                const auto closeDialog=qScopeGuard([&]{d->reject();});
                ++opens;const auto urls=d->sidebarUrls();QCOMPARE(urls,AppPaths::routeSidebarUrls());QVERIFY(urls.size()<=4);
                if(!previous.isEmpty())QCOMPARE(urls,previous);previous=urls;
                QSet<QUrl> unique;for(const auto &url:urls){QVERIFY(!unique.contains(url));unique.insert(url);}
                QCOMPARE(d->directory().canonicalPath(),QFileInfo(exporting?AppPaths::routesDirectory():QFileInfo(route).absolutePath()).canonicalFilePath());
                // Both format folders and legacy/external routes remain navigable.
                d->setDirectory(AppPaths::routesDirectory()+"/gpx");QCOMPARE(d->directory().dirName(),QString("gpx"));
                d->setDirectory(AppPaths::routesDirectory()+"/kml");QCOMPARE(d->directory().dirName(),QString("kml"));
                d->setDirectory(legacyRoutes);QCOMPARE(d->directory().canonicalPath(),QFileInfo(legacyRoutes).canonicalFilePath());
                d->setDirectory(directory.path());d->reject();
            }});
            if(exporting) QVERIFY(QMetaObject::invokeMethod(&widget,"exportSelectedRoute",Qt::DirectConnection));
            else QVERIFY(QMetaObject::invokeMethod(&widget,"on_pushButtonSelectRouteFile_clicked",Qt::DirectConnection));
            QCOMPARE(edit->text(),route);qt.sync();QCOMPARE(qt.value("FileDialog/shortcuts"),original);
            QVERIFY(!widget.findChild<AutoPilotSimulator*>()->isRunning());
        }
        QCOMPARE(opens,5);QVERIFY(QFileInfo(legacyRoutes).isDir());QVERIFY(QFileInfo(route).isFile());
    }
    void pickerStopAccess_data(){QTest::addColumn<bool>("keyboard");QTest::newRow("F12")<<true;QTest::newRow("button")<<false;}
    void pickerStopAccess() {
        QFETCH(bool,keyboard);AppSettings settings(APP_COMPANY,APP_NAME);settings.clear();
        settings.setValue("Nmea2000/InterfaceType","None");settings.setValue("Nmea0183/Nmea0183Enabled",true);settings.setValue("Nmea0183/Nmea0183Port","MEMORY");settings.sync();
        MainWindow window(nullptr,nullptr,new MemoryPort);window.show();
        auto *start=window.findChild<QAction*>("actionStart_Sending");auto *worker=window.findChild<NmeaSenderWorker*>();QVERIFY(start);QVERIFY(worker);
        auto *model=window.findChild<PgnTableModel*>();QVERIFY(model);model->addPgn(99001,"Fixture","HDT",true,{{"Heading","1","rad","double"}}, {},250);
        start->trigger();QVERIFY(worker->isSending());
        GeneratorVesselMovement widget(1,"Vessel Movement",&window);widget.show();bool seen=false;
        QTimer::singleShot(20,[&]{for(auto *top:QApplication::topLevelWidgets())if(auto *d=qobject_cast<QFileDialog*>(top);d&&d->isVisible()){
            const auto closeDialog=qScopeGuard([&]{d->reject();});
            seen=true;auto *stop=d->findChild<QToolButton*>("applicationStopSending");QVERIFY(stop);QTRY_VERIFY_WITH_TIMEOUT(stop->isVisible(),1000);
            if(keyboard)QTest::keyClick(d,Qt::Key_F12);else stop->click();
            QVERIFY(!worker->isSending());QVERIFY(!start->isChecked());
            const auto out=qEnvironmentVariable("M225_PICKER_EVIDENCE");
            if(!out.isEmpty()){QDir().mkpath(out);d->grab().save(out+(keyboard?"/picker-F12.png":"/picker-stop-button.png"));}
            d->reject();
        }});
        QVERIFY(QMetaObject::invokeMethod(&widget,"on_pushButtonSelectRouteFile_clicked",Qt::DirectConnection));
        QVERIFY(seen);QVERIFY(!worker->isSending());QVERIFY(!start->isChecked());
    }
    void manualMovementAutopilotHint_data() {
        QTest::addColumn<bool>("kml");
        QTest::newRow("GPX") << false;
        QTest::newRow("owner-KML") << true;
    }
    void manualMovementAutopilotHint() {
        QFETCH(bool,kml);
        QTemporaryDir dir;
        QString file;
        if(kml) {
            const auto source=qEnvironmentVariable("RAY_ROUTE_OWNER_FIXTURE",QDir::currentPath()+"/routes/kml/mr_haverhill.kml");
            file=dir.filePath("mr_haverhill.kml");QVERIFY(QFile::copy(source,file));
        } else file=write(dir,"fixture.gpx",gpx);
        const auto candidate=RouteImport::read(file);QVERIFY(candidate);
        GeneratorVesselMovement widget(1,"Fixture",nullptr);widget.resize(550,760);widget.show();
        widget.findChild<QTabWidget*>("tabWidget")->setCurrentIndex(1);
        auto *manual=widget.findChild<QCheckBox*>("checkBoxManualMovementEnabled");
        auto *enable=widget.findChild<QCheckBox*>("checkBoxAutoPilotEnabled");
        auto *hint=widget.findChild<QLabel*>("autopilotManualMovementHint");
        auto *pilot=widget.findChild<AutoPilotSimulator*>();QSignalSpy loaded(pilot,&AutoPilotSimulator::routeLoaded);
        QVERIFY(hint && hint->isHidden());QVERIFY(enable->isEnabled());
        manual->click();QVERIFY(manual->isChecked());QVERIFY(!enable->isEnabled());QVERIFY(hint->isVisible());
        QVERIFY(hint->text().contains("Stop Manual movement"));
        widget.restoreLayoutConfiguration({{"route",file},{"speed",3.0}});
        QCOMPARE(widget.layoutConfiguration()["route"].toString(),file);
        QVERIFY(manual->isChecked());QVERIFY(hint->isVisible());QVERIFY(!enable->isEnabled());
        QVERIFY(!pilot->isRunning());QCOMPARE(loaded.size(),0);
        if(!qEnvironmentVariable("RAY_B11_IMAGE_DIR").isEmpty())
            QVERIFY(widget.grab().save(qEnvironmentVariable("RAY_B11_IMAGE_DIR")+(kml?"/manual-hint-kml.png":"/manual-hint-gpx.png")));
        manual->click();QVERIFY(!manual->isChecked());QVERIFY(hint->isHidden());QVERIFY(enable->isEnabled());
        enable->click();QVERIFY(pilot->isRunning());QCOMPARE(loaded.size(),1);
        enable->click();QVERIFY(!pilot->isRunning());
        manual->click();QVERIFY(hint->isVisible());widget.stopOperationalActivity();
        QVERIFY(hint->isHidden());QVERIFY(!manual->isChecked());QVERIFY(!pilot->isRunning());
        widget.setEngineReadinessCheck([]{return QString("Fixture readiness blocked");});
        manual->click();QVERIFY(!manual->isChecked());QVERIFY(hint->isHidden());QVERIFY(enable->isEnabled());
    }
    void selectedRouteNameMetadataAndPersistence() {
        QTemporaryDir dir;
        GeneratorVesselMovement widget(1,"Fixture",nullptr);
        auto *name=widget.findChild<QLineEdit*>("selectedRouteName");
        auto *path=widget.findChild<QLineEdit*>("lineEditFilePath");
        QVERIFY(name && name->isReadOnly() && path->isHidden());
        auto named=write(dir,"route-file.gpx","<gpx><rte><name>Harbour Loop</name><rtept lat='42' lon='-70'/><rtept lat='42.01' lon='-70'/></rte></gpx>");
        widget.restoreLayoutConfiguration({{"route",named}});
        QCOMPARE(name->text(),QString("Harbour Loop"));QCOMPARE(name->toolTip(),named);
        if(!qEnvironmentVariable("RAY_B9_UI_IMAGE_DIR").isEmpty()) {
            widget.resize(550,760);widget.findChild<QTabWidget*>("tabWidget")->setCurrentIndex(1);widget.show();QTest::qWait(30);
            QVERIFY(widget.grab().save(qEnvironmentVariable("RAY_B9_UI_IMAGE_DIR")+"/route-name.png"));
        }
        QCOMPARE(widget.layoutConfiguration()["route"].toString(),named);
        const auto kml=write(dir,"route.kml","<kml><Placemark><name>Channel Approach</name><LineString><coordinates>-70,42 -70,42.01</coordinates></LineString></Placemark></kml>");
        widget.restoreLayoutConfiguration({{"route",kml}});QCOMPARE(name->text(),QString("Channel Approach"));
        const auto fallback=write(dir,"fallback-name.gpx",gpx);
        widget.restoreLayoutConfiguration({{"route",fallback}});QCOMPARE(name->text(),QString("fallback-name"));
        QTimer::singleShot(0,[]{for(auto *top:QApplication::topLevelWidgets())if(auto *d=qobject_cast<QFileDialog*>(top))d->reject();});
        QVERIFY(QMetaObject::invokeMethod(&widget,"on_pushButtonSelectRouteFile_clicked"));
        QCOMPARE(name->text(),QString("fallback-name"));QCOMPARE(widget.layoutConfiguration()["route"].toString(),fallback);
        QVERIFY(!widget.findChild<AutoPilotSimulator*>()->isRunning());
    }
    void fileChoiceWithActiveRouteServiceAvailable() {
        QTemporaryDir dir;const auto file=write(dir,"service-menu.GPX",gpx);
        GeneratorVesselMovement widget(1,"Fixture",nullptr);widget.setActiveRouteChooser([](QWidget*){return ReceivedRoutes::Route{};});
        bool sourceSeen=false,fileSeen=false;QTimer answer;
        connect(&answer,&QTimer::timeout,this,[&]{for(auto *top:QApplication::topLevelWidgets()){
            if(auto *source=qobject_cast<QInputDialog*>(top);source&&source->isVisible()){sourceSeen=true;source->setTextValue("Select Route File");source->accept();}
            if(auto *dialog=qobject_cast<QFileDialog*>(top);dialog&&dialog->isVisible()){fileSeen=true;chooseFile(dialog,file);}
        }});answer.start(20);
        QVERIFY(QMetaObject::invokeMethod(&widget,"on_pushButtonSelectRouteFile_clicked"));answer.stop();
        QVERIFY(sourceSeen&&fileSeen);QCOMPARE(widget.findChild<QLineEdit*>("lineEditFilePath")->text(),file);
        auto *pilot=widget.findChild<AutoPilotSimulator*>();auto *enabled=widget.findChild<QCheckBox*>("checkBoxAutoPilotEnabled");
        QVERIFY(!pilot->isRunning());QVERIFY(!enabled->isChecked());enabled->click();QVERIFY(pilot->isRunning());enabled->click();QVERIFY(!pilot->isRunning());
    }
    void chooserAndTransactionalSelection() {
        QTemporaryDir dir;
        GeneratorVesselMovement widget(1,"Vessel Movement",nullptr);widget.resize(700,700);widget.show();
        auto *path=widget.findChild<QLineEdit*>("lineEditFilePath");
        const auto original=write(dir,"original.gpx",gpx);path->setText(original);
        auto *simulator=widget.findChild<AutoPilotSimulator*>();QVERIFY(simulator->loadRoute(original));simulator->setSpeed(2);simulator->start();
        QStringList names; int sequence=0;
        for(const auto &extension:{"gpx","GPX","GpX","gPx","kml","KML","KmL","kMl"})
            names.append(QFileInfo(write(dir,QString("route%1.").arg(++sequence)+extension,extension[0]=='g'||extension[0]=='G'?gpx:QByteArray("<kml><Placemark><LineString><coordinates>-70,42 -70,42.01</coordinates></LineString></Placemark></kml>"))).fileName());
        const auto bad=write(dir,"bad.GPX","<gpx><rte><rtept lat='95.123' lon='0'/></rte></gpx>");
        bool sawChooser=false,sawError=false,allFilesVisible=true,errorSpecific=false;
        QTimer closer;connect(&closer,&QTimer::timeout,this,[&]{
            for(auto *w:QApplication::topLevelWidgets()) {
                if(auto *dialog=qobject_cast<QFileDialog*>(w);dialog&&dialog->isVisible()) {
                    sawChooser=true;
                    QDir visible(dir.path());visible.setNameFilters(dialog->nameFilters().first().section('(',1).section(')',0,0).split(' '));
                    const auto files=visible.entryList(QDir::Files);for(const auto &name:names)allFilesVisible &= files.contains(name);
                    chooseFile(dialog,bad);
                }
                if(auto *box=qobject_cast<QMessageBox*>(w);box&&box->isVisible()) {sawError=true;errorSpecific=box->text().contains("latitude");box->accept();}
            }
        });closer.start(20);
        QVERIFY(QMetaObject::invokeMethod(&widget,"on_pushButtonSelectRouteFile_clicked",Qt::DirectConnection));closer.stop();
        QVERIFY(sawChooser);QVERIFY(allFilesVisible);QVERIFY(sawError);QVERIFY(errorSpecific);QCOMPARE(path->text(),original);QCOMPARE(widget.findChild<QLineEdit*>("selectedRouteName")->text(),QString("original"));QVERIFY(simulator->isRunning());
        simulator->stop();
    }
    void mixedCaseStartAndLayoutRestore() {
        QTemporaryDir dir;GeneratorVesselMovement widget(1,"Vessel Movement",nullptr);widget.show();
        auto *path=widget.findChild<QLineEdit*>("lineEditFilePath");auto *enabled=widget.findChild<QCheckBox*>("checkBoxAutoPilotEnabled");
        const auto file=write(dir,"chosen.GpX",gpx);
        const QJsonObject config{{"route",file},{"reverse",true},{"speedSource",0},{"speed",3.0}};
        widget.restoreLayoutConfiguration(config);QCOMPARE(path->text(),file);
        auto *simulator=widget.findChild<AutoPilotSimulator*>();QVERIFY(!simulator->isRunning());QVERIFY(!enabled->isChecked());
        enabled->click();QVERIFY(enabled->isChecked());QVERIFY(simulator->isRunning());
        QTest::qWait(30);enabled->click();QVERIFY(!simulator->isRunning());
        const auto saved=widget.layoutConfiguration();QCOMPARE(saved["route"].toString(),file);
        GeneratorVesselMovement other(2,"Other",nullptr);other.restoreLayoutConfiguration(saved);
        QCOMPARE(other.layoutConfiguration()["route"].toString(),file);QVERIFY(!other.findChild<AutoPilotSimulator*>()->isRunning());
        path->setText(write(dir,"broken.kMl","<kml/>"));enabled->click();QVERIFY(!enabled->isChecked());QVERIFY(!simulator->isRunning());
    }
    void renderedRouteControls() {
        GeneratorVesselMovement widget(1,"Vessel Movement",nullptr);widget.resize(750,750);widget.show();
        for (const auto &theme:{QString("Dark"),QString("Light")}) {
            qApp->setPalette(Themes::palette(theme));qApp->setStyleSheet(Themes::styleSheet(theme));
            auto *tabs=widget.findChild<QTabWidget*>();QVERIFY(tabs);
            for(int i=0;i<tabs->count();++i)if(tabs->tabText(i).contains("Autopilot",Qt::CaseInsensitive))tabs->setCurrentIndex(i);
            QTest::qWait(60);
            const auto output=qEnvironmentVariable("M225_EVIDENCE");
            if(!output.isEmpty()){QDir().mkpath(output);QVERIFY(widget.grab().save(output+"/route-"+theme+".png"));}
            auto *button=widget.findChild<QPushButton*>("pushButtonSelectRouteFile");QVERIFY(button);QVERIFY(button->isEnabled());
        }
        qApp->setStyleSheet({});
    }
    void legacyPointConfirmationPreservesActiveRouteOnNo() {
        QTemporaryDir dir;GeneratorVesselMovement widget(1,"Vessel Movement",nullptr);widget.show();
        auto *simulator=widget.findChild<AutoPilotSimulator*>();QVERIFY(simulator->loadRoute(write(dir,"old.gpx",gpx)));simulator->start();
        auto *enabled=widget.findChild<QCheckBox*>("checkBoxAutoPilotEnabled");enabled->setChecked(true);
        widget.findChild<QLineEdit*>("lineEditFilePath")->setText(write(dir,"points.KML","<kml><Document><Placemark><Point><coordinates>1,1</coordinates></Point></Placemark><Placemark><Point><coordinates>2,2</coordinates></Point></Placemark></Document></kml>"));
        QSignalSpy state(simulator,&AutoPilotSimulator::RunningStateChanged);
        const auto answer=[](QMessageBox::StandardButton button) {
            QTimer::singleShot(30,[button]{for(auto *w:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox*>(w))box->button(button)->click();});
        };
        answer(QMessageBox::No);
        QVERIFY(QMetaObject::invokeMethod(&widget,"on_checkBoxAutoPilotEnabled_clicked",Qt::DirectConnection,Q_ARG(bool,true)));
        QVERIFY(enabled->isChecked());QVERIFY(simulator->isRunning());QCOMPARE(state.count(),0);
        answer(QMessageBox::Yes);
        QVERIFY(QMetaObject::invokeMethod(&widget,"on_checkBoxAutoPilotEnabled_clicked",Qt::DirectConnection,Q_ARG(bool,true)));
        QVERIFY(simulator->isRunning());QVERIFY(state.count()>0);
        enabled->click();QVERIFY(!simulator->isRunning());
    }
};
int main(int argc,char **argv) {
#ifdef Q_OS_WIN
    IsolatedUserRegistry registry;
#endif
    QTemporaryDir profile(QDir::currentPath()+"/build/m239b/route-file-profile-XXXXXX"); qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
#ifndef Q_OS_WIN
    // AppSettings isolation alone does not isolate Qt's shared FileDialog state.
    qputenv("XDG_CONFIG_HOME",(profile.path()+"/qt-config").toUtf8());
    QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,profile.path()+"/qt-config");
#endif
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs); // Match the application's STOP-accessible chooser policy.
    QApplication app(argc,argv);app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    RouteImportUiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "route_import_ui_tests.moc"
