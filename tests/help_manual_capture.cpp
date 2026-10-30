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
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QPlainTextEdit>
#include <QSerialPort>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeView>
#include <QTableWidget>
#include <QTimer>
#include "mainwindow.h"
#include "appsettings.h"
#include "apppaths.h"
#include "themes.h"
#include "dialogsetup.h"
#include "maincontroller.h"
#include "temperaturewidget130312.h"
#include "latlong129025.h"
#include "navigation129284.h"
#include "crosstrack129283.h"
#include "expansioninstruments.h"
#include "windlassstatus.h"
#include "logviewer.h"
#include "engineprofiledialog.h"
#include "nmea2000diagnosticsdialog.h"
#include "nmea2000diagnostics.h"
#include "chartplotterroutedialog.h"
#include "serialportdiscovery.h"
#include "aboutdialog.h"
#include "N2kMessages.h"
#include "nmea2000handler.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
class IsolatedRegistry {
    QString path=QString("Software\\RayNmeaHelpCapture-%1").arg(QCoreApplication::applicationPid());HKEY hive=nullptr;
public:
    IsolatedRegistry(){if(RegCreateKeyExW(HKEY_CURRENT_USER,reinterpret_cast<LPCWSTR>(path.utf16()),0,nullptr,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&hive,nullptr)!=ERROR_SUCCESS || RegOverridePredefKey(HKEY_CURRENT_USER,hive)!=ERROR_SUCCESS)qFatal("Cannot isolate native dialog preferences");}
    ~IsolatedRegistry(){RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);RegCloseKey(hive);RegDeleteTreeW(HKEY_CURRENT_USER,reinterpret_cast<LPCWSTR>(path.utf16()));}
};
#endif

// Explicit, offline authoring helper. It is not a production target or automatic
// CTest. Real widgets render synthetic state; only the chosen output directory
// receives images. Native serial/CAN, provider requests and live data are excluded.
class MemorySerial final:public QSerialPort {
public:
    QByteArray submitted;
    bool open(OpenMode mode) override{return QIODevice::open(mode|QIODevice::Unbuffered);}
    void close()override{QIODevice::close();}
    qint64 bytesToWrite()const override{return 0;}
protected:qint64 writeData(const char *data,qint64 size)override{submitted.append(data,size);return size;}
};
class ManualCapture:public QObject {
    Q_OBJECT
    QString output,fixtures;
    QJsonArray manifest;
    void shot(const QString &topic,const QString &name,QWidget &widget,const QString &caption){
        widget.ensurePolished();QTest::qWait(45);
        const QString path=topic+"/"+name+".png";QVERIFY(QDir().mkpath(output+"/"+topic));
        const auto pixmap=widget.grab();QVERIFY(!pixmap.isNull());QVERIFY(pixmap.save(output+"/"+path,"PNG"));
        manifest.append(QJsonObject{{"topic",topic},{"name",name},{"file","images/"+path},{"caption",caption},{"theme","Darkeum"},{"widget",widget.metaObject()->className()},{"window",widget.windowTitle()},{"width",pixmap.width()},{"height",pixmap.height()},{"synthetic",true},{"capture","Native Qt QWidget::grab; no AI or pixel replacement"}});
        QFile log(output+"/capture-manifest.json");QVERIFY(log.open(QIODevice::WriteOnly));log.write(QJsonDocument(manifest).toJson());
    }
    void tab(QTabWidget *tabs,int index){QVERIFY(tabs);QVERIFY(index<tabs->count());tabs->setCurrentIndex(index);QTest::qWait(20);}
    template<class T>T *child(QWidget &w,const char *name){auto *p=w.findChild<T*>(name);if(!p)qFatal("Missing real control: %s",name);return p;}
    void modal(QAction *action,const QString &topic,const QString &name,const QString &caption,std::function<void(QDialog*)> prepare={}){
        bool captured=false;QTimer timer;
        connect(&timer,&QTimer::timeout,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog || captured)return;captured=true;if(prepare)prepare(dialog);shot(topic,name,*dialog,caption);dialog->reject();});
        timer.start(80);action->trigger();QVERIFY(captured);
    }
    void widgetTabs(PgnWidgetBase &w,const QString &topic,const QStringList &names,const QStringList &captions){
        w.setStyleSheet(Themes::styleSheet("Darkeum"));w.resize(540,620);w.show();
        auto *tabs=w.findChild<QTabWidget*>();
        if(!tabs){shot(topic,names[0],w,captions[0]);w.hide();return;}
        for(int i=0;i<qMin(names.size(),tabs->count());++i){tab(tabs,i);w.adjustSize();w.resize(qMax(520,w.width()),w.sizeHint().height());shot(topic,names[i],w,captions[i]);}w.hide();
    }
    void setupTab(DialogSetup &setup,QWidget *control){for(auto *tabs:setup.findChildren<QTabWidget*>())for(int i=0;i<tabs->count();++i)if(tabs->widget(i)==control || tabs->widget(i)->isAncestorOf(control))tabs->setCurrentIndex(i);}
private slots:
    void capture(){
        output=qEnvironmentVariable("RAY_HELP_CAPTURE_ROOT");QVERIFY(!output.isEmpty());QVERIFY(QDir().mkpath(output));fixtures=TestProfile::root()+"/routes";QVERIFY(QDir().mkpath(fixtures));
        AppSettings settings(APP_COMPANY,APP_NAME);settings.clear();settings.setValue("Preference/StyleSheetName","Darkeum");settings.setValue("Nmea2000/Nmea2000Enabled",false);settings.setValue("Nmea0183/Nmea0183Enabled",false);settings.setValue("Vessel/Latitude",0.0);settings.setValue("Vessel/Longitude",0.0);settings.setValue("Vessel/Name","DEMONSTRATION ONLY");settings.setValue("Vessel/RadioCallsign","DEMO");settings.sync();
        Themes::apply("Darkeum");qApp->setStyleSheet(Themes::styleSheet("Darkeum"));
        MainWindow window;window.resize(1240,800);window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *controller=window.findChild<MainController*>();QVERIFY(controller);MyVessel vessel;vessel.Name="DEMONSTRATION ONLY";vessel.MMSI=0;vessel.RadioCallsign="DEMO";vessel.Latitude=0;vessel.Longitude=0;controller->setMyVessel(vessel);
        auto *setup=window.findChild<DialogSetup*>();QVERIFY(setup);
        setup->show();setup->resize(850,760);
        auto *interfaces=child<QComboBox>(*setup,"comboBoxNmea2000Interface");setupTab(*setup,interfaces);
        if(qEnvironmentVariable("RAY_HELP_CAPTURE_ONLY")=="socketcan"){
            interfaces->setCurrentText("SocketCan");shot("socketcan","linux-interface",*interfaces->parentWidget(),"Native Linux SocketCAN Setup page, focused on interface controls. This disposable example is not connected to a physical interface.");setup->reject();return;
        }
        setup->reject();shot("first-launch","empty-workspace",window,"A fresh disposable workspace with Sending stopped and no interfaces enabled.");
        if(qEnvironmentVariable("RAY_HELP_CAPTURE_ONLY").startsWith("m255")){
            window.findChild<QAction*>("actionViewer_Rx")->trigger();
            QCOMPARE(window.currentMode(),Viewer);
            auto *area=window.findChild<QMdiArea*>();QVERIFY(area);
            for(auto *sub:area->subWindowList())if(sub->windowTitle().contains("Navigation"))area->setActiveSubWindow(sub);
            QVERIFY(!controller->hasLayoutWidgets());
            shot("viewer-generate","viewer-empty",window,"Viewer before reception: Empty Layout, no instruments, Sending disabled. All transports are off.");
            auto *handler=window.findChild<Nmea2000Handler*>();QVERIFY(handler);
            tN2kMsg msg;
            SetN2kLatLonRapid(msg,0.01,0.02);msg.Source=120;handler->nmea2000MessageReceived(msg);
            SetN2kTrueHeading(msg,1,0.75);msg.Source=120;handler->nmea2000MessageReceived(msg);
            SetN2kWaterDepth(msg,1,7.2,0);msg.Source=120;handler->nmea2000MessageReceived(msg);
            QTest::qWait(100);
            QCOMPARE(controller->layoutWidgets("Navigation").size(),3);
            shot("viewer-generate","viewer-received",window,"Viewer after three synthetic in-process RX messages: position, heading, depth, in first-arrival order. Real parsers and discovery; no live bus or external Sending.");
            window.hide();
            if(qEnvironmentVariable("RAY_HELP_CAPTURE_ONLY")=="m255-viewer")return;
            GeneratorVesselMovement movement(1200,"Vessel Movement",nullptr);movement.setMyVessel(vessel);
            movement.setLatitude(0);movement.setLongitude(0);movement.setStyleSheet(Themes::styleSheet("Darkeum"));
            movement.resize(590,820);movement.show();tab(child<QTabWidget>(movement,"tabWidget"),2);
            auto *devices=child<QComboBox>(movement,"comboBoxGamePadDevice");
            QVERIFY2(devices->currentText()!="None Found","No physical SDL-mapped controller detected; never synthesize its name.");
            child<QCheckBox>(movement,"checkBoxGamePadEnabled")->click();
            QVERIFY(child<QCheckBox>(movement,"checkBoxGamePadEnabled")->isChecked());
            shot("game-controller","game-controller-controls",movement,"Actual SDL-detected controller selected and Game Pad enabled. Sticks/buttons must first be neutral. Local movement and external Sending remain stopped; demonstration vessel values are synthetic.");
            if(qEnvironmentVariableIsSet("RAY_HELP_CAPTURE_PREVIEW")){QTimer::singleShot(120000,qApp,&QCoreApplication::quit);qApp->exec();}
            return;
        }
        window.findChild<QAction*>("actionLoad_Default")->trigger();QTest::qWait(60);
        auto *area=window.findChild<QMdiArea*>();QVERIFY(area);
        area->setViewMode(QMdiArea::TabbedView);
        auto selectGroup=[&](const QString &name){for(auto *sub:area->subWindowList())if(sub->windowTitle().contains(name,Qt::CaseInsensitive)){area->setActiveSubWindow(sub);QTest::qWait(30);return;}};
        selectGroup("Navigation");shot("main-window","workspace-overview",window,"The actual Main Window with the bundled Default Layout loaded. Values are seeded examples, not live vessel reception.");
        for(const QString group:{"Navigation","Propulsion","Environment","Tanks","Electrical","Communication","Other","Generate"}){selectGroup(group);shot("instruments-generators","category-"+group.toLower(),window,"The "+group+" category in the demonstration Layout. Categories organize widgets; selecting a tab does not authorize Sending.");}
        for(const auto &pair:QList<QPair<QString,QString>>{{"menuLayout","layout"},{"menuMode","mode"},{"menuTools","tools"},{"menuView","view"},{"menuSending","transmission"},{"menuHelp","help"}}){
            auto *menu=window.findChild<QMenu*>(pair.first);if(!menu)continue;menu->popup(window.mapToGlobal(QPoint(55,45)));QTest::qWait(40);shot("keyboard-shortcuts",pair.second+"-menu",*menu,"The actual "+pair.second+" menu. Use labels and shortcuts shown by your build.");menu->hide();
        }
        QTimer::singleShot(80,[&]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))if(auto *keep=d->findChild<QPushButton*>("viewerKeepExisting")){shot("viewer-generate","viewer-entry",*d,"Viewer entry offers Clear, Keep Existing or Cancel. Sending is revoked before this choice.");keep->click();}});
        window.findChild<QAction*>("actionViewer_Rx")->trigger();selectGroup("Navigation");shot("viewer-generate","viewer-stopped",window,"Viewer mode disables ordinary instrument Sending. The displayed seeded Layout is not evidence of live traffic.");
        window.findChild<QAction*>("actionSimulator_Tx")->trigger();shot("viewer-generate","generate-stopped",window,"Generate mode permits editing and simulation; switching modes does not authorize transmission.");
        auto *table=window.findChild<QTableView*>("tableView");auto *tree=window.findChild<QTreeView*>("treeView");
        controller->setMode(Viewer);controller->addWidgetAsComboboxTextChanged("130312","Temperature","Environment");controller->setMode(Sender);
        if(auto *temperature=window.findChild<TemperatureWidget130312*>()){
            selectGroup("Environment");auto *tables=window.findChild<PgnTableController*>();tables->getPgnTableModel()->setCurrentPgn(temperature->Id());temperature->setActualTemperature(293.15);
            auto *dock=window.findChild<QDockWidget*>("dockWidgetTable");dock->setMinimumWidth(630);window.resize(1500,800);table->resizeColumnsToContents();
            shot("property-editor","canonical-values",*table,"The Property Editor retains canonical units and full precision independently of preferred display units.");
            dock->setMinimumWidth(0);window.resize(1240,800);
            shot("units","temperature-display",*temperature,"A canonical 293.15 K measurement displayed in the selected temperature units.");
        }
        tree->expandAll();shot("object-tree","expanded-properties",*tree,"Expanded Object Tree properties show formatted values and source/instance distinctions.");
        auto *toolbar=window.findChild<QToolBar*>("toolBarSending");shot("enable-dispatch-sending","stopped-toolbar",*toolbar,"Both authorization and transport readiness matter. This no-interface workspace remains stopped.");
        setup->show();setup->resize(850,760);
        for(auto *box:setup->findChildren<QComboBox*>())if(box->objectName().contains("Port",Qt::CaseInsensitive) && box->isEditable()){
            SerialPortDiscovery::refresh(box,{{"DEMO_PORT","Synthetic USB serial adapter","Demonstration",true,"0000:0000"}},false);box->setEditText("DEMO_PORT");
        }
        for(const auto &item:QList<QPair<QString,QString>>{{"comboBoxAppTheme","themes"},{"vesselMmsi","ais"},{"comboBoxNmea0183BaudRate","nmea0183"}}){
            auto *control=setup->findChild<QWidget*>(item.first);if(!control)continue;setupTab(*setup,control);shot(item.second,"setup-tab",*setup,"The "+item.second+" Setup page in a fresh synthetic profile. Changes shown are drafts until Save.");
        }
        auto *theme=child<QComboBox>(*setup,"comboBoxAppTheme");setupTab(*setup,theme);theme->showPopup();QTest::qWait(20);shot("themes","theme-choices",*theme->view()->window(),"Choose Darkeum for the manual's appearance; all six built-ins and valid external QSS themes remain available.");theme->hidePopup();
        setupTab(*setup,interfaces);
        for(const auto &pair:QList<QPair<QString,QString>>{{"None","nmea2000"},{"Network NMEA2000","network-nmea2000"},{"CANable SLCAN","canable"},{"Waveshare USB-CAN-A","waveshare"},{"Actisense","actisense"},{"SignalK","signal-k"}}){
            interfaces->setCurrentText(pair.first);
            if(auto *host=setup->findChild<QLineEdit*>("networkCanHost"))host->setText("127.0.0.1");
            if(auto *name=setup->findChild<QLineEdit*>("networkCanInterface"))name->setText("can0");
            if(auto *server=setup->findChild<QLineEdit*>("signalKServer"))server->setText("http://127.0.0.1:3000");
            shot(pair.second,"setup-interface",*setup,"Actual "+pair.first+" configuration controls. No Connect, Request Access or Test Connection operation is executed.");
        }
        interfaces->setCurrentText("Network NMEA2000");child<QComboBox>(*setup,"networkCanMode")->setCurrentIndex(1);shot("network-nmea2000","tx-capable-draft",*setup,"RX/TX-capable network configuration is separate from explicit instrument Sending and route-service authorization.");
        interfaces->setCurrentText("CANable SLCAN");auto *ports=child<QComboBox>(*setup,"canablePort");ports->showPopup();QTest::qWait(20);shot("connection-troubleshooting","synthetic-port-list",*ports->view()->window(),"A synthetic discovery result illustrates the path/description display without revealing connected owner devices.");ports->hidePopup();setup->reject();
        auto *logPrefs=setup->findChild<QWidget*>("loggingPreferences");QVERIFY(logPrefs);setup->show();setupTab(*setup,logPrefs);shot("application-log","logging-preferences",*setup,"Application verbosity, binding logging, traffic filters and per-file limits are independent preferences.");setup->reject();
        modal(window.findChild<QAction*>("actionSave"),"layouts","save-layout","Save Layout As uses the live workspace snapshot; saving does not enable Sending.",[](QDialog *d){if(auto *input=qobject_cast<QInputDialog*>(d))input->setTextValue("Help Bench Layout");});
        modal(window.findChild<QAction*>("actionClear"),"layouts","clear-group","Clear Group is a confirmed destructive Layout operation; this capture cancels it.");
        modal(window.findChild<QAction*>("actionClear_All"),"layouts","clear-all-confirmation","Clear All Groups asks before replacing the current workspace. F12 remains available.");
        auto menuAction=[&](const QString &text){for(auto *action:window.findChildren<QAction*>())if(action->text().contains(text))return action;return static_cast<QAction*>(nullptr);};
        QVERIFY(menuAction("Templates…"));modal(menuAction("Templates…"),"templates","template-manager","The Template manager distinguishes bundled and User entries and provides separate management actions.");
        QVERIFY(menuAction("Startup Workspace"));modal(menuAction("Startup Workspace"),"default-layout","startup-workspace","Startup Workspace chooses Empty, a Layout or a Template. Startup never authorizes Sending.");
        modal(menuAction("Save as Template"),"templates","detailed-template-editor","The detailed Template editor uses canonical values and per-field Saved Value / Setup Default policies.");
        EngineProfileDialog profiles([]{return QStringList{};},&window);profiles.setAttribute(Qt::WA_DeleteOnClose,false);profiles.resize(900,680);profiles.show();shot("engine-transmission","engine-profile-editor",profiles,"Engine Profiles edits local performance data; it does not control a real engine.");profiles.hide();

        TemperatureWidget130312 temperature(801,"Temperature",nullptr);temperature.setActualTemperature(293.15);temperature.resize(540,150);temperature.setStyleSheet(Themes::styleSheet("Darkeum"));temperature.show();
        for(int unit=0;unit<3;++unit){auto prefs=temperature.UserPrefs();prefs.tempUnitIndex=unit;temperature.setUserPrefs(prefs);shot("units",QString("temperature-unit-%1").arg(unit),temperature,"The same 293.15 K model value rendered with temperature unit choice "+QString::number(unit)+"; changing units is not a new measurement.");}
        temperature.setActualTemperature(N2kDoubleNA);shot("error-messages","unavailable-temperature",temperature,"Unavailable is a protocol validity state, not a huge negative temperature.");temperature.setActualTemperature(293.15);
        auto *entry=temperature.findChild<QLineEdit*>();if(entry){entry->setFocus();entry->selectAll();QTest::keyClicks(entry,"-99999");QTest::keyClick(entry,Qt::Key_Return);shot("property-editor","rejected-input-restored",temperature,"An out-of-range committed edit restores the current model value rather than silently clamping.");}temperature.hide();
        LatLong129025 position(802,"Latitude / Longitude",nullptr);position.setLatitude(0.0123456789);position.setLongitude(-0.023456789);position.resize(540,160);position.setStyleSheet(Themes::styleSheet("Darkeum"));position.show();shot("navigation-measurements","position-precision",position,"Synthetic coordinates illustrate preferred display formatting; full canonical coordinate precision is retained.");position.hide();
        CrossTrack129283 xte(803,"Cross Track Error",nullptr);xte.setStyleSheet(Themes::styleSheet("Darkeum"));xte.resize(540,140);xte.show();
        for(const auto &sample:QList<QPair<QString,double>>{{"negative",-1.9288},{"zero",0},{"unavailable",N2kDoubleNA}}){xte.setXTE(sample.second);shot("navigation-measurements","xte-"+sample.first,xte,"Cross Track Error: "+sample.first+" synthetic measurement. Sign, zero and unavailable have distinct meanings.");}xte.hide();
        Navigation129284 navigation(804,"Navigation",nullptr);navigation.setDistanceToWaypoint(500);navigation.setETATime(43200);navigation.setBearingPositionToDestinationWaypoint(1.2);navigation.setStyleSheet(Themes::styleSheet("Darkeum"));navigation.show();shot("navigation-measurements","navigation-data",navigation,"Distance, ETA and bearing are navigation measurements, not route authorization or movement commands.");navigation.hide();
        for(const auto &code:QStringList{"129539","129540","129044","130314","128275","GGA","RMC","DPT","MWV","APB","RMB","129285","130074"}){
            auto *instrument=ExpansionInstrument::create(900,Expansion::definition(code)->label,code,nullptr);if(!instrument)continue;instrument->setLabel(Expansion::definition(code)->label);instrument->setStyleSheet(Themes::styleSheet("Darkeum"));instrument->resize(550,instrument->sizeHint().height());instrument->show();
            shot(code.contains(QRegularExpression("^[A-Z]"))?"nmea0183":"messages","instrument-"+code,*instrument,"Actual "+code+" property rows with synthetic defaults. Consult the capability table for field and direction limits.");
            auto *edit=instrument->findChild<QPushButton*>(code=="129540"?"editSatellites":"editWaypoints");
            if(edit && (code=="129540" || code=="130074")){
                bool captured=false;QTimer timer;connect(&timer,&QTimer::timeout,[&]{if(auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget())){if(captured)return;captured=true;shot("property-editor","records-"+code,*d,"The real bounded record editor. Add/edit/remove rows in the draft, then accept or cancel as a unit.");d->reject();}});timer.start(80);edit->click();
            }
            instrument->hide();delete instrument;
        }
        for(int pgn:{128777,128778}){WindlassStatus windlass(1100,"Windlass "+QString::number(pgn),pgn);windlass.setValue("WindlassIdentifier","0");windlass.setStyleSheet(Themes::styleSheet("Darkeum"));windlass.show();shot("windlass","status-"+QString::number(pgn),windlass,"Manual property-driven windlass status simulation; this is not a physical motor-control interface.");}

        GeneratorVesselMovement movement(1200,"Vessel Movement",nullptr);movement.setMyVessel(vessel);movement.setLatitude(0);movement.setLongitude(0);movement.setStyleSheet(Themes::styleSheet("Darkeum"));movement.resize(570,820);movement.show();auto *movementTabs=child<QTabWidget>(movement,"tabWidget");
        const QStringList movementNames{"manual-controls","autopilot-controls","game-controller-controls"};const QStringList movementTopics{"manual-movement","autopilot","game-controller"};
        for(int i=0;i<3;++i){tab(movementTabs,i);shot(movementTopics[i],movementNames[i],movement,"Vessel Movement "+movementTabs->tabText(i)+" controls in a stopped synthetic session.");}
        tab(movementTabs,0);auto *manual=child<QCheckBox>(movement,"checkBoxManualMovementEnabled");manual->click();shot("manual-movement","manual-enabled",movement,"Manual movement operates locally; generator Dispatch and external Sending remain separate gates.");manual->click();
        QFile route(fixtures+"/demo-course.gpx");QVERIFY(route.open(QIODevice::WriteOnly));route.write("<gpx version='1.1' creator='RayNmeaSimulator Help'><rte><name>Demo Course</name><rtept lat='0' lon='0'><name>Start</name></rtept><rtept lat='0.01' lon='0.02'><name>Turn</name></rtept><rtept lat='0.02' lon='0.02'><name>Finish</name></rtept></rte></gpx>");route.close();movement.restoreLayoutConfiguration({{"route",route.fileName()}});tab(movementTabs,1);
        shot("route-import","file-route-selected",movement,"Demo Course is selected from a synthetic GPX. Autopilot, motion and Sending remain off until separately enabled.");
        auto *speed=child<QComboBox>(movement,"comboBoxSpeedSource");speed->showPopup();QTest::qWait(20);shot("autopilot","speed-source-choices",*speed->view()->window(),"Autopilot speed may use Manual, Throttle Position or Route File where data are available.");speed->hidePopup();
        auto fileDialogShot=[&](const QString &name,const QString &topic,const char *slot){bool captured=false;QTimer timer;connect(&timer,&QTimer::timeout,[&]{if(auto *d=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){if(captured)return;captured=true;d->setOption(QFileDialog::DontUseNativeDialog);d->setSidebarUrls({QUrl::fromLocalFile(fixtures)});d->setDirectory(fixtures);d->selectFile(name.contains("export")?"demo-export.gpx":"demo-course.gpx");shot(topic,name,*d,"Real route file dialog with disposable demonstration files; no owner route or preference is used.");d->reject();}});timer.start(80);QMetaObject::invokeMethod(&movement,slot);QVERIFY(captured);};
        fileDialogShot("select-gpx-file","route-import","on_pushButtonSelectRouteFile_clicked");fileDialogShot("export-gpx-file","route-export","exportSelectedRoute");
        ReceivedRoutes::Route received;received.complete=true;received.source="Synthetic source 120";received.id="demo-route";received.name="Demo Chartplotter Route";received.status="Complete synthetic snapshot";Waypoint a,bp;a.name="Start";a.coordinate=QGeoCoordinate(0,0);bp.name="Finish";bp.coordinate=QGeoCoordinate(.01,.02);received.waypoints={a,bp};
        movement.setActiveRouteChooser([received](QWidget*){return received;});bool choiceCaptured=false;QTimer choose;
        connect(&choose,&QTimer::timeout,[&]{if(auto *d=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())){if(choiceCaptured)return;choiceCaptured=true;shot("route-overview","route-source-choice",*d,"Choose Select Route File or Select Chartplotter Route; the chooser does not activate navigation.");d->setTextValue("Select Chartplotter Route");d->accept();}});choose.start(80);QMetaObject::invokeMethod(&movement,"on_pushButtonSelectRouteFile_clicked");choose.stop();
        shot("chartplotter-routes","selected-snapshot",movement,"A complete synthetic chartplotter snapshot is selected for the next explicit Autopilot activation. No device was queried.");movement.hide();
        ChartplotterRouteDialog routes(window.findChild<Nmea2000Handler*>(),{received},&window);routes.passiveStillValid=[](const ReceivedRoutes::Route&){return true;};routes.show();shot("chartplotter-routes","route-catalogue",routes,"The actual chartplotter route dialog with a complete synthetic passive snapshot. Request buttons are separate explicit operations.");
        if(auto *list=routes.findChild<QListWidget*>("routeServiceRoutes"))if(list->count())list->setCurrentRow(0);shot("route-authorization","snapshot-selection",routes,"Select Route requires a complete snapshot; Discover, Refresh Routes and Retrieve Waypoints have their own authorization and cancellation.");routes.close();

        GeneratorEngine engine(1300,"Generate Engine #1 / Outboard",nullptr);engine.setMyVessel(vessel);engine.setEngineSpeed(1200);engine.initializeGeneratorControls();widgetTabs(engine,"engine-transmission",{"engine-main","engine-status"},{"Engine generator main controls and local start state. No external engine is controlled.","Engine status values are optional sources for compatible instrument properties."});
        GeneratorTransLowerUnit transmission(1301,"Generate Transmission #1 / Lower Unit",nullptr);transmission.initializeGeneratorControls();widgetTabs(transmission,"engine-transmission",{"transmission-main","transmission-status"},{"Transmission generator controls feed compatible instance-specific instruments.","Lower-unit status is a local simulation source; manual instrument TX remains independent."});
        GeneratorDateTime clock(1302,"Generate Date Time",nullptr);clock.initializeGeneratorControls();widgetTabs(clock,"generating-data",{"date-time-generator"},{"The clock generator can supply compatible date/time instruments without authorizing external Sending."});
        GeneratorWeather weather(1303,"Weather",nullptr);weather.initializeGeneratorControls();weather.setProperty("AirTemperature",293.15);weather.setProperty("WaterTemperature",291.15);weather.setProperty("Humidity",55.0);weather.setProperty("WindSpeed",4.0);weather.setProperty("WindAngle",1.0);weather.setProperty("Latitude",0.0);weather.setProperty("Longitude",0.0);
        widgetTabs(weather,"weather",{"atmosphere","marine","more","description","refresh"},{"Weather atmosphere values are synthetic manual inputs; no provider was queried.","Marine values and waves are distinct from atmospheric observations.","Additional provider fields may be unavailable; the simulator does not invent missing observations.","Provider descriptions and observation context must be reviewed before using data.","Automatic refresh is explicitly configured; fetching weather does not enable Sending."});
        weather.show();auto *api=child<QComboBox>(weather,"comboBoxApiSource");for(int i=0;i<api->count();++i){api->setCurrentIndex(i);shot("weather","provider-"+QString::number(i),weather,"Weather provider choice "+api->itemText(i)+". No Fetch Weather action is executed; API-key fields are empty.");}weather.hide();
        // Logical Sending is demonstrated only against an in-memory serial sink.
        {AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Nmea2000/Nmea2000Enabled",false);s.setValue("Nmea0183/Nmea0183Enabled",true);s.setValue("Nmea0183/Nmea0183Port","DEMO_MEMORY");s.sync();}
        auto *memory=new MemorySerial;MainWindow virtualWindow(nullptr,nullptr,memory);virtualWindow.show();auto *model=virtualWindow.findChild<PgnTableModel*>();model->addPgn(999,"Demo heading","HDT",true,{{"Heading","1.0","rad","double"}},{},1000);
        auto *virtualBar=virtualWindow.findChild<QToolBar*>("toolBarSending");shot("enable-dispatch-sending","ready-toolbar",*virtualBar,"Ready state using a synthetic in-memory serial sink. No physical port is opened.");
        virtualWindow.findChild<QAction*>("actionStart_Sending")->trigger();QTest::qWait(100);QVERIFY(virtualWindow.findChild<NmeaSenderWorker*>()->isSending());shot("enable-dispatch-sending","active-toolbar",*virtualBar,"Actual active toolbar state produced by the existing sender and a memory-only sink. This is not hardware qualification.");
        virtualWindow.findChild<QAction*>("actionStop_Sending")->trigger();shot("enable-dispatch-sending","stopped-after-stop",*virtualBar,"Stop returns the sender to stopped state; F12 invokes the same safety lifecycle.");virtualWindow.hide();

        Nmea2000Diagnostics diagnostics;diagnostics.setSession("help-synthetic","Synthetic offline capture");tN2kMsg message;SetN2kLatLonRapid(message,0.01,0.02);message.Source=120;diagnostics.observe(message);diagnostics.markApplicationDecoded("129025","120");Nmea2000DiagnosticsDialog diagnosticView(&diagnostics,&window);diagnosticView.show();shot("device-diagnostics","passive-diagnostics",diagnosticView,"Diagnostics from a synthetic message. No device discovery or live traffic capture occurred.");
        for(auto *check:diagnosticView.findChildren<QCheckBox*>())if(check->text().contains("Pause"))check->setChecked(true);shot("device-diagnostics","paused-diagnostics",diagnosticView,"Pause affects display refresh; it is not a transport disconnect or a stop of incoming traffic.");diagnosticView.close();
        ApplicationLog::initialize();ApplicationLog::event("help.demo","Synthetic instructional log event; no vessel data.");ApplicationLog::flush();LogViewer logs(&window);logs.resize(900,620);logs.show();shot("application-log","application-viewer",logs,"Application Log viewer with one synthetic instructional event in a disposable file.");
        child<QCheckBox>(logs,"pauseDisplay")->setChecked(true);shot("application-log","paused-display",logs,"Pause Display suspends automatic refresh. Explicit Refresh and confirmed Clear remain functional.");child<QCheckBox>(logs,"pauseDisplay")->setChecked(false);
        child<QComboBox>(logs,"logChannel")->setCurrentIndex(1);shot("traffic-recording","recording-off",logs,"Traffic Recording is Off by default and is independent of the on-screen NMEA viewer.");
        QVERIFY(ApplicationLog::startTraffic());ApplicationLog::flush();ApplicationLog::n2k("RX","Synthetic",message,"decoded");ApplicationLog::flush();logs.refresh();shot("traffic-recording","synthetic-recording",logs,"Traffic Recording enabled only for synthetic in-process data. No CAN traffic was generated or captured.");ApplicationLog::stopTraffic();
        child<QCheckBox>(logs,"logRawJSON")->setChecked(true);logs.refresh();shot("traffic-recording","raw-records",logs,"Raw structured records distinguish logical TX submissions from physical wire delivery.");logs.close();ApplicationLog::shutdown();
        AboutDialog about(&window);about.show();shot("engineering-reference","about-status",about,"The approved About dialog identifies version, publisher, license and AS-IS engineering-reference status.");about.reject();
        QFile manifestFile(output+"/capture-manifest.json");QVERIFY(manifestFile.open(QIODevice::WriteOnly));manifestFile.write(QJsonDocument(manifest).toJson());
        qInfo()<<"Native Darkeum captures:"<<manifest.size();window.hide();
    }
};
int main(int argc,char **argv){
    QTemporaryDir profile;if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());qputenv("RAYNMEA_TEST_CREDENTIALS","1");
#ifdef Q_OS_WIN
    // Full-manual captures isolate native file-dialog preferences. The M2.55
    // subset opens no native dialogs and must let SDL read real device mapping
    // configuration; AppSettings still uses its disposable test profile.
    QScopedPointer<IsolatedRegistry> registry;
    if(!qEnvironmentVariable("RAY_HELP_CAPTURE_ONLY").startsWith("m255"))registry.reset(new IsolatedRegistry);
#endif
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);QApplication::setApplicationName(APP_NAME);QApplication::setOrganizationName(APP_COMPANY);QApplication::setApplicationVersion(APP_VERSION);Themes::initializeSystemStyle();QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    ManualCapture capture;return QTest::qExec(&capture,argc,argv);
}
#include "help_manual_capture.moc"
