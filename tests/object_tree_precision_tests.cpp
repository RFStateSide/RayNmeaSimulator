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
#include <limits>
#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QInputDialog>
#include <QJsonArray>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QLocale>
#include "mainwindow.h"
#include "appsettings.h"
#include "pgntreecontroller.h"
#include "latlong129025.h"
#include "depth128267.h"
#include "N2kMessages.h"
#include "n2khelper.h"
#include "convert.h"

namespace {
struct Responses {
    QTimer timer;
    Responses() {QObject::connect(&timer,&QTimer::timeout,[]{
        auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d)return;
        if(auto *box=qobject_cast<QMessageBox*>(d)){
            if(box->objectName()=="viewerEntryDialog")box->findChild<QAbstractButton*>("viewerKeepExisting")->click();
            else if(auto *button=box->button(QMessageBox::No))button->click();else box->reject();
        }else if(auto *spin=d->findChild<QSpinBox*>()){spin->setValue(1);d->accept();}
        else if(d->findChild<QComboBox*>())d->accept();else d->reject();
    });timer.start(0);}
};
struct Workspace {
    Responses responses;
    MainWindow window;
    MainController *c=window.findChild<MainController*>();
    PgnTableModel *parameters=window.findChild<PgnTableController*>()->getPgnTableModel();
    PgnTreeController *tree=window.findChild<PgnTreeController*>();
    PgnTreeModel *model=tree->getTreeModel();
    PgnWidgetBase *add(const QString &pgn,const QString &group) {
        if(pgn=="-130311")c->addGeneratorWidget("Weather (PGN -130311)");
        else c->addWidgetAsComboboxTextChanged(pgn,pgn,group);
        QTest::qWait(25);return qobject_cast<PgnWidgetBase*>(c->layoutWidgets(group).last());
    }
    QString value(PgnWidgetBase *widget,const QString &field,int role=Qt::DisplayRole) {
        return model->data(model->indexFor(widget,field).siblingAtColumn(1),role).toString();
    }
    QByteArray snapshot() {
        QJsonArray result;for(const auto &p:parameters->getPgnList()){
            QJsonArray rows;for(const auto &row:p.paramDetails)rows.append(QJsonArray::fromStringList(row));
            result.append(QJsonObject{{"id",p.idX},{"enabled",p.enabled},{"pgn",p.pgn},{"source",p.source},{"interval",p.sendInterval},{"rows",rows}});
        }return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }
};
QByteArray bytes(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray{};}
}
class ObjectTreePrecisionTests:public QObject {
    Q_OBJECT
private slots:
    void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/Nmea2000Enabled",false);s.setValue("Nmea0183/Nmea0183Enabled",false);s.sync();}
    void cardAgreement_data(){
        QTest::addColumn<QString>("pgn");QTest::addColumn<QString>("group");QTest::addColumn<QString>("field");QTest::addColumn<QString>("control");QTest::addColumn<double>("raw");QTest::addColumn<QString>("expected");
        auto row=[](const char *name,const char *pgn,const char *group,const char *field,const char *control,double raw,const char *expected){QTest::newRow(name)<<QString(pgn)<<QString(group)<<QString(field)<<QString(control)<<raw<<QString::fromUtf8(expected);};
        row("depth-ft","128267","Navigation","DepthBelowTransducer","lineEditDepth",10,"32.8 ft");
        row("speed-mph","128259","Navigation","WaterReferenced","lineEditWaterReferenced",10,"22.4 MPH");
        row("heading","127250","Navigation","Heading","lineEditHeading",1.23456,"70.7 deg");
        row("rpm","127488","Propulsion","EngineSpeed","lineEditEngineSpeed",1234.126,"1234.13 RPM");
        row("oil-psi","127489","Propulsion","EngineOilPress","lineEditEngineOilPress",100000,"14.5 PSI");
        row("temperature-F","127489","Propulsion","EngineCoolantTemp","lineEditCoolantTemp",295.372,"72.0 °F");
        row("hours","127489","Propulsion","EngineHours","lineEditEngineHours",7201.25,"2.00 h");
        row("battery-V","127508","Electrical","BatteryVoltage","lineEditBatteryVoltage",12.345,"12.3 V");
        row("negative-current","127508","Electrical","BatteryCurrent","lineEditBatteryCurrent",-2.345,"-2.3 A");
        row("battery-temperature","127508","Electrical","BatteryTemperature","lineEditBatteryTemperature",295.372,"72.0 °F");
        row("tank-percent","127505","Tanks","Level","lineEditLevel",57.126,"57.1 %");
        row("tank-gallons","127505","Tanks","Capacity","lineEditCapacity",123.456,"32.6 gal");
        row("outside-temperature","130310","Environment","WaterTemperature","lineEditWaterTemperature",295.372,"72.00 °F");
        row("outside-mechanical-pressure","130310","Environment","AtmosphericPressure","lineEditAtmosphericPressure",100000,"14.50 PSI");
        row("weather-atmospheric-pressure","-130311","Generate","AtmosphericPressure","lineEditAtmosphericPressure",100012.3,"1000.12 hPa");
        row("weather-temperature","-130311","Generate","AirTemperature","lineEditAirTemperature",295.372,"72.0 °F");
        row("heave-ft","127252","Navigation","Heave","lineEditHeave",-0.123456,"-0.41 ft");
        row("waypoint-bearing","129284","Navigation","BearingPositionToDestinationWaypoint","lineEditBearingPositionToDestinationWaypoint",1.23456,"70.74 deg");
        row("waypoint-far","129284","Navigation","DistanceToWaypoint","lineEditDistanceToWaypoint",1000,"0.62 mi");
    }
    void cardAgreement(){
        QFETCH(QString,pgn);QFETCH(QString,group);QFETCH(QString,field);QFETCH(QString,control);QFETCH(double,raw);QFETCH(QString,expected);
        Workspace w;auto *widget=w.add(pgn,group);QVERIFY(widget);QVERIFY(widget->setProperty(field.toUtf8(),raw));
        // Supply the canonical fixture independently of widget notification coverage.
        w.parameters->changeParam(widget->Id(),field,QString::number(raw,'g',17));
        const auto before=w.snapshot();QSignalSpy edits(w.parameters,&PgnTableModel::parameterChanged);
        QCOMPARE(w.value(widget,field),expected);auto *edit=widget->findChild<QLineEdit*>(control);QVERIFY(edit);
        QCOMPARE(w.value(widget,field).section(' ',0,0),edit->text());QCOMPARE(w.value(widget,field,Qt::AccessibleTextRole),expected);
        QVERIFY(w.value(widget,field,Qt::ToolTipRole).contains(field+" = "));QCOMPARE(w.snapshot(),before);QCOMPARE(edits.count(),0);
    }
    void enginePercentPrecision_data(){
        QTest::addColumn<QString>("field");QTest::addColumn<double>("raw");QTest::addColumn<QString>("expected");
        for(const auto &field:QStringList{"EngineLoad","EngineTorque"}){
            const QList<double> values=field=="EngineTorque"?QList<double>{-80.250125822785,0,0.0125,80.250125822785}:QList<double>{0,0.0125,80.250125822785,100};
            for(double raw:values)QTest::newRow(qPrintable(field+QString::number(raw,'g',17)))<<field<<raw<<(QString::number(raw,'f',1)+" %");
        }
    }
    void enginePercentPrecision(){
        QFETCH(QString,field);QFETCH(double,raw);QFETCH(QString,expected);
        Workspace w;auto *widget=w.add("127489","Propulsion");QVERIFY(widget);QVERIFY(widget->setProperty(field.toUtf8(),raw));
        const auto canonical=QString::number(raw,'g',17);w.parameters->changeParam(widget->Id(),field,canonical);
        QVERIFY(w.c->selectLayoutWidget(widget,field));auto *table=w.window.findChild<QTableView*>("tableView");QVERIFY(table);
        const QPersistentModelIndex property=table->currentIndex();QVERIFY(property.isValid());
        QCOMPARE(property.data(Qt::EditRole).toString(),canonical);
        QTemporaryDir dir;const auto a=dir.filePath("before.json"),b=dir.filePath("after.json");QVERIFY(w.c->saveLayoutToJson(a));
        const auto before=w.snapshot();QSignalSpy edits(w.parameters,&PgnTableModel::parameterChanged);
        for(int i=0;i<10;++i){
            QCOMPARE(w.value(widget,field),expected);QCOMPARE(w.value(widget,field,Qt::AccessibleTextRole),expected);
            QCOMPARE(widget->findChild<QLineEdit*>("lineEdit"+field)->text(),expected.section(' ',0,0));
            QCOMPARE(property.data(Qt::EditRole).toString(),canonical);QCOMPARE(w.snapshot(),before);
        }
        QCOMPARE(edits.count(),0);QVERIFY(w.c->saveLayoutToJson(b));QCOMPARE(bytes(a),bytes(b));
        w.c->removeAllWidgets();QVERIFY(w.c->loadLayoutFromJson(a));QTest::qWait(50);
        auto *restored=qobject_cast<PgnWidgetBase*>(w.c->layoutWidgets("Propulsion").last());QVERIFY(restored);
        QCOMPARE(restored->property(field.toUtf8()).toDouble(),raw);QCOMPARE(w.value(restored,field),expected);
    }
    void unitCyclesWithoutWritebackOrReset(){
        Workspace w;w.window.show();auto *depth=w.add("128267","Navigation");auto *speed=w.add("128259","Navigation");auto *outside=w.add("130310","Environment");auto *weather=w.add("-130311","Generate");
        QVERIFY(depth->setProperty("DepthBelowTransducer",10.12345));QVERIFY(speed->setProperty("WaterReferenced",12.3456));QVERIFY(outside->setProperty("WaterTemperature",295.372));QVERIFY(outside->setProperty("AtmosphericPressure",100125.0));QVERIFY(weather->setProperty("AtmosphericPressure",100125.0));
        QTemporaryDir dir;const auto a=dir.filePath("before.json"),b=dir.filePath("after.json");QVERIFY(w.c->saveLayoutToJson(a));const auto original=w.snapshot();
        QSignalSpy resets(w.model,&QAbstractItemModel::modelReset),edits(w.parameters,&PgnTableModel::parameterChanged),changes(w.model,&QAbstractItemModel::dataChanged);QPersistentModelIndex stable(w.model->indexFor(depth,"DepthBelowTransducer"));
        auto defaults=depth->UserPrefs();
        for(int pass=0;pass<2;++pass)for(int i=0;i<5;++i){auto prefs=defaults;prefs.depthUnitIndex=i%3;prefs.speedUnitIndex=i%4;prefs.tempUnitIndex=i%3;prefs.pressureUnitIndex=i%4;prefs.atmosphericPressureUnitIndex=i;prefs.distanceUnitIndex=i%3;w.c->setUserPrefs(prefs);QTest::qWait(65);
            QCOMPARE(w.value(depth,"DepthBelowTransducer").section(' ',0,0),depth->findChild<QLineEdit*>("lineEditDepth")->text());
            QCOMPARE(w.value(speed,"WaterReferenced").section(' ',0,0),speed->findChild<QLineEdit*>("lineEditWaterReferenced")->text());
            QCOMPARE(w.value(outside,"WaterTemperature").section(' ',0,0),outside->findChild<QLineEdit*>("lineEditWaterTemperature")->text());
            QCOMPARE(w.value(outside,"AtmosphericPressure").section(' ',0,0),outside->findChild<QLineEdit*>("lineEditAtmosphericPressure")->text());
            QCOMPARE(w.value(weather,"AtmosphericPressure").section(' ',0,0),weather->findChild<QLineEdit*>("lineEditAtmosphericPressure")->text());
            QCOMPARE(w.snapshot(),original);QVERIFY(stable.isValid());
        }
        w.c->setUserPrefs(defaults);QTest::qWait(65);QCOMPARE(resets.count(),0);QCOMPARE(edits.count(),0);QVERIFY(changes.count()>0);QVERIFY(w.c->saveLayoutToJson(b));QCOMPARE(bytes(a),bytes(b));QCOMPARE(w.snapshot(),original);
    }
    void rapidRefreshUsesLivePrecisionAndSharedUnitLabels(){
        Workspace w;auto *rapid=w.add("127488","Propulsion");QVERIFY(rapid->setProperty("EngineSpeed",1234.126));QVERIFY(rapid->setProperty("EngineBoostPressure",105678.9));w.parameters->changeParam(rapid->Id(),"EngineSpeed","1234.126");w.parameters->changeParam(rapid->Id(),"EngineBoostPressure","105678.9");const auto before=w.snapshot();
        for(int i=0;i<4;++i){auto prefs=rapid->UserPrefs();prefs.pressureUnitIndex=i;rapid->setUserPrefs(prefs);
            QCOMPARE(rapid->findChild<QLineEdit*>("lineEditEngineSpeed")->text(),QString("1234.13"));QCOMPARE(w.value(rapid,"EngineSpeed"),QString("1234.13 RPM"));
            QCOMPARE(w.value(rapid,"EngineBoostPressure").section(' ',0,0),rapid->findChild<QLineEdit*>("lineEditBoostPressure")->text());
            QCOMPARE(rapid->findChild<QLabel*>("labelUnitsEngineBoostPressure")->text(),QStringList({"Pa","kPa","Bar","PSI"})[i]);QCOMPARE(w.snapshot(),before);
        }
    }
    void atmosphericRoundingUsesTheCardHelper(){
        Workspace w;auto *weather=w.add("-130311","Generate");auto prefs=weather->UserPrefs();prefs.atmosphericPressureUnitIndex=1;weather->setUserPrefs(prefs);
        const double raw=98256.0671634832;QVERIFY(weather->setProperty("AtmosphericPressure",raw));w.parameters->changeParam(weather->Id(),"AtmosphericPressure",QString::number(raw,'g',17));const auto before=w.snapshot();
        QCOMPARE(w.value(weather,"AtmosphericPressure"),QString("29.02 inHg"));QCOMPARE(weather->findChild<QLineEdit*>("lineEditAtmosphericPressure")->text(),QString("29.02"));QCOMPARE(w.snapshot(),before);
    }
    void meaningfulSmallLargeNegativeAndUnavailable(){
        Workspace w;auto *depth=w.add("128267","Navigation");auto prefs=depth->UserPrefs();prefs.depthUnitIndex=0;depth->setUserPrefs(prefs);
        for(double n:{0.0,0.0000123456,-0.0000123456,12345678.125,1e20}){QVERIFY(depth->setProperty("DepthBelowTransducer",n));const auto before=w.snapshot();const auto text=w.value(depth,"DepthBelowTransducer");const auto number=text.section(' ',0,0);bool ok=false;const double displayed=number.toDouble(&ok);QVERIFY(ok);if(n!=0)QVERIFY(displayed!=0);QVERIFY(text.size()<60);QCOMPARE(w.snapshot(),before);}
        for(double na:{N2kDoubleNA,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
            w.parameters->changeParam(depth->Id(),"DepthBelowTransducer",QString::number(na,'g',17));const auto before=w.snapshot();QCOMPARE(w.value(depth,"DepthBelowTransducer"),QString("Unavailable"));QCOMPARE(w.snapshot(),before);
        }
        auto *dc=w.add("127506","Electrical");w.parameters->changeParam(dc->Id(),"StateOfCharge","255");QCOMPARE(w.value(dc,"StateOfCharge"),QString("Unavailable"));w.parameters->changeParam(dc->Id(),"StateOfCharge","42");QVERIFY(w.value(dc,"StateOfCharge").startsWith("42"));
        w.parameters->changeParam(dc->Id(),"TimeRemaining","90123");QCOMPARE(w.value(dc,"TimeRemaining"),N2kHelper::convertN2kTimeToUserUnits(90123));
    }
    void coordinatesRxAndSavedCanonicalPrecision(){
        Workspace w;w.window.findChild<QAction*>("actionViewer_Rx")->trigger();auto *h=w.window.findChild<Nmea2000Handler*>();tN2kMsg m;SetN2kLatLonRapid(m,43.1234567,-70.9876543);m.Source=23;h->nmea2000MessageReceived(m);QTest::qWait(70);
        auto list=w.c->layoutWidgets("Navigation");QCOMPARE(list.size(),1);auto *position=qobject_cast<LatLong129025*>(list.first());QVERIFY(position);QVERIFY(qAbs(position->Latitude()-43.1234567)<1e-10);QVERIFY(qAbs(position->Longitude()+70.9876543)<1e-10);
        const auto before=w.snapshot();QTemporaryDir dir;const auto a=dir.filePath("coordinates.json");QVERIFY(w.c->saveLayoutToJson(a));
        for(int i=0;i<3;++i){auto prefs=position->UserPrefs();prefs.latLongUnitIndex=i;position->setUserPrefs(prefs);const auto text=w.value(position,"Latitude");QCOMPARE(text,position->findChild<QLineEdit*>("lineEditLatitude")->text()+(i==0?" deg":""));QCOMPARE(w.snapshot(),before);}
        QVERIFY(w.value(position,"Latitude",Qt::ToolTipRole).contains("43.123456"));w.c->removeAllWidgets();QVERIFY(w.c->loadLayoutFromJson(a));QTest::qWait(50);auto *restored=w.window.findChild<LatLong129025*>();QVERIFY(restored);QVERIFY(qAbs(restored->Latitude()-43.1234567)<1e-10);QVERIFY(qAbs(restored->Longitude()+70.9876543)<1e-10);
        const auto data=QJsonDocument::fromJson(bytes(a)).object()["layouts"].toArray()[0].toObject()["widgets"].toArray()[0].toObject()["parameters"].toArray();for(auto row:data)if(row.toArray()[0]=="Latitude")QVERIFY(qAbs(row.toArray()[1].toString().toDouble()-43.1234567)<1e-10);
    }
    void timesAnglesIdentityAndLocale(){
        Workspace w;auto *time=w.add("129033","Navigation");w.parameters->changeParam(time->Id(),"DaysSince1970","19800");w.parameters->changeParam(time->Id(),"SecondsSinceMidnight","12345.75");
        for(int i=0;i<3;++i){auto prefs=time->UserPrefs();prefs.dateUnitIndex=i;time->setUserPrefs(prefs);QCOMPARE(w.value(time,"DaysSince1970"),N2kHelper::convertN2kDateToUserFormat(19800,prefs));}QVERIFY(w.value(time,"SecondsSinceMidnight").endsWith(" UTC"));
        auto *rot=w.add("127251","Navigation");QVERIFY(rot->setProperty("RateOfTurn",0.00123456));for(int i=0;i<2;++i){auto prefs=rot->UserPrefs();prefs.rateOfTurnUnitIndex=i;rot->setUserPrefs(prefs);QCOMPARE(w.value(rot,"RateOfTurn").section(' ',0,0),rot->findChild<QLineEdit*>("lineEditRateOfTurn")->text());}
        auto *depth=w.add("128267","Navigation");depth->setProperty("DepthBelowTransducer",10.125);w.parameters->changeParam(depth->Id(),"SID","123456789012345");QCOMPARE(w.value(depth,"SID"),QString("123456789012345"));
        const auto before=w.snapshot();const auto old=QLocale();QLocale::setDefault(QLocale(QLocale::German));const auto text=w.value(depth,"DepthBelowTransducer");QLocale::setDefault(old);QVERIFY(text.contains('.'));QVERIFY(!text.contains(','));QCOMPARE(w.snapshot(),before);
    }
    void pendingPropertyEditAndThemes(){
        Workspace w;w.window.resize(1400,900);w.window.show();auto *depth=w.add("128267","Navigation");depth->setProperty("DepthBelowTransducer",10.125);QVERIFY(w.c->selectLayoutWidget(depth,"DepthBelowTransducer"));
        auto *table=w.window.findChild<QTableView*>("tableView");QVERIFY(table);table->setFocus();table->edit(table->currentIndex());QTest::qWait(25);auto *draft=table->viewport()->findChild<QLineEdit*>();QVERIFY(draft);draft->setFocus();draft->setText("10.1234567890123");draft->setModified(true);const auto before=w.snapshot();QSignalSpy edits(w.parameters,&PgnTableModel::parameterChanged);
        for(int i=0;i<3;++i){auto prefs=depth->UserPrefs();prefs.depthUnitIndex=i;depth->setUserPrefs(prefs);for(int j=0;j<50;++j)w.value(depth,"DepthBelowTransducer");QCOMPARE(draft->text(),QString("10.1234567890123"));QCOMPARE(w.snapshot(),before);QVERIFY(w.tree->navigationBlocked());}
        QTest::keyClick(draft,Qt::Key_Escape);QCOMPARE(edits.count(),0);QCOMPARE(w.snapshot(),before);
        for(const auto &theme:QStringList{"Dark","Light"}){AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.sync();w.window.HotReloadSettings();QTest::qWait(65);auto *view=w.tree->getTreeView();view->expand(w.model->indexFor(depth));view->setCurrentIndex(w.model->indexFor(depth,"DepthBelowTransducer"));view->scrollTo(view->currentIndex());QTest::qWait(20);const auto dir=qEnvironmentVariable("M234_EVIDENCE");if(!dir.isEmpty()){QVERIFY(QDir().mkpath(dir));QVERIFY(w.window.grab().save(dir+"/"+theme+"-precision.png"));}QCOMPARE(w.snapshot(),before);}
        QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
};
int main(int argc,char **argv){QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));ObjectTreePrecisionTests t;return QTest::qExec(&t,argc,argv);}
#include "object_tree_precision_tests.moc"
