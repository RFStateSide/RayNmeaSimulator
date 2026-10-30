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
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "appsettings.h"
#include "depth128267.h"
#include "n2khelper.h"
#include "dcstatus127506.h"
#include "chargerstatus127507.h"
#include "batterystatus127508.h"
#include "generatorweather.h"
#include "pgntreecontroller.h"
#include "signalk_testserver.h"

namespace {
void near(double a,double b,double tolerance=1e-7){QVERIFY2(qAbs(a-b)<=tolerance,qPrintable(QString("%1 != %2").arg(a,0,'g',17).arg(b,0,'g',17)));}
void answer(QMessageBox::StandardButton choice){QTimer::singleShot(0,[choice]{for(auto* w:QApplication::topLevelWidgets())if(auto* box=qobject_cast<QMessageBox*>(w))box->button(choice)->click();});}
struct Workspace {
    MainWindow window;
    MainController* controller=window.findChild<MainController*>();
    PgnTableModel* model=window.findChild<PgnTableController*>()->getPgnTableModel();
    Workspace(){window.resize(1400,900);window.show();QTest::qWait(30);}
    QWidget* add(QString pgn,QStringList params={}) {
        const QString group=pgn=="128267"?"Navigation":"Electrical";
        QTimer::singleShot(0,[]{for(auto* w:QApplication::topLevelWidgets())if(auto* d=qobject_cast<QDialog*>(w);d&&d->windowTitle()=="Select Battery Instance")d->reject();});
        controller->addWidgetAsComboboxTextChanged(pgn,pgn,group,params);
        return controller->layoutWidgets(group).last();
    }
    QList<QString> field(QWidget* widget,QString name){for(auto row:model->findPgn(widget->property("Id").toInt())->paramDetails)if(row[0]==name)return row;return {};}
    bool edit(QWidget* widget,QString field,QVariant value){model->setCurrentPgn(widget->property("Id").toInt());for(int i=0;i<model->rowCount();++i)if(model->data(model->index(i,0)).toString()==field)return model->setData(model->index(i,1),value);return false;}
    QStringList outbound(QWidget* widget){QStringList out;for(auto row:model->findPgn(widget->property("Id").toInt())->paramDetails)out<<row[0]+":"+row[1];return out;}
};
void textEdit(QWidget* w,const char* name,QString text){auto* e=w->findChild<QLineEdit*>(name);QVERIFY(e);e->setText(text);QVERIFY(QMetaObject::invokeMethod(e,"textEdited",Q_ARG(QString,text)));}
QJsonObject values(const TestServer& server){QJsonObject result;for(auto delta:server.deltas)for(auto u:delta["updates"].toArray())for(auto v:u.toObject()["values"].toArray()){auto o=v.toObject();result[o["path"].toString()]=o["value"];}return result;}
}
class DepthElectricalTests: public QObject {
    Q_OBJECT
private slots:
    void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");s.setValue("Preference/StyleSheetName","Dark");QLocale::setDefault(QLocale::c());}
    void depthUnits_data(){QTest::addColumn<int>("unit");QTest::addColumn<double>("input");QTest::newRow("meters")<<0<<10.;QTest::newRow("feet")<<1<<32.8;QTest::newRow("fathoms")<<2<<5.5;}
    void depthUnits(){QFETCH(int,unit);QFETCH(double,input);Workspace w;auto* d=qobject_cast<Depth128267*>(w.add("128267"));QVERIFY(d);QCOMPARE(d->DepthBelowTransducer(),0.);QCOMPARE(d->findChild<QLineEdit*>("lineEditDepth")->text(),"0.0");UserUnitPrefs prefs=d->UserPrefs();prefs.depthUnitIndex=unit;d->setUserPrefs(prefs);
        textEdit(d,"lineEditDepth",QString::number(input));const double meters=N2kHelper::convertUserDepthToN2kUnits(input,prefs);near(d->DepthBelowTransducer(),meters);near(w.field(d,"DepthBelowTransducer")[1].toDouble(),meters);QCOMPARE(w.field(d,"DepthBelowTransducer")[2],"m");
        d->setInitialGuiValues();QCOMPARE(d->findChild<QLineEdit*>("lineEditDepth")->text(),QString::number(input,'f',1));const auto upper=QString::number(N2kHelper::convertN2kDepthToUserUnits(1000,prefs),'f',1);textEdit(d,"lineEditDepth",upper);near(d->DepthBelowTransducer(),N2kHelper::convertUserDepthToN2kUnits(upper.toDouble(),prefs));QVERIFY(w.edit(d,"DepthBelowTransducer",15.25));near(d->DepthBelowTransducer(),15.25);QCOMPARE(d->findChild<QLineEdit*>("lineEditDepth")->text(),QString::number(N2kHelper::convertN2kDepthToUserUnits(15.25,prefs),'f',1));
        const double before=d->DepthBelowTransducer();textEdit(d,"lineEditDepth","-1");near(d->DepthBelowTransducer(),before);textEdit(d,"lineEditDepth","bad");near(d->DepthBelowTransducer(),before);d->setInitialGuiValues();
        auto* checkbox=d->findChild<QCheckBox*>("checkBoxEnabled");checkbox->click();QVERIFY(!d->isEnabled());QTRY_VERIFY(!w.model->isPgnEnabled(d->Id()));
        prefs.depthUnitIndex=(unit+1)%3;d->setUserPrefs(prefs);near(d->DepthBelowTransducer(),before);
    }
    void batteryInitialization(){
        Workspace w;
        BatteryStatus127508 standalone(999,"Uninitialized",nullptr);
        QCOMPARE(standalone.BatteryTemperature(),N2kDoubleNA);
        QCOMPARE(standalone.findChild<QLineEdit*>("lineEditBatteryTemperature")->text(),"Unavailable");
        auto* first=qobject_cast<BatteryStatus127508*>(w.add("127508"));
        auto* second=qobject_cast<BatteryStatus127508*>(w.add("127508"));QVERIFY(first&&second);
        QVERIFY(w.edit(second,"BatteryInstance",1));
        near(first->BatteryTemperature(),293.15);near(second->BatteryTemperature(),293.15);
        near(w.field(first,"BatteryTemperature")[1].toDouble(),293.15);QCOMPARE(w.field(first,"BatteryTemperature")[2],"K");
        auto* tree=w.window.findChild<PgnTreeController*>()->getTreeModel();
        QTRY_VERIFY(tree->indexFor(first,"BatteryTemperature").isValid());
        auto* field=first->findChild<QLineEdit*>("lineEditBatteryTemperature");
        for(int unit:{0,1,2}){
            auto prefs=first->UserPrefs();prefs.tempUnitIndex=unit;first->setUserPrefs(prefs);first->setInitialGuiValues();
            QCOMPARE(field->text(),QString::number(N2kHelper::convertN2kTemperatureToUserUnits(293.15,prefs),'f',1));
            if(unit!=0)QCOMPARE(field->text(),unit==1?QString("20.0"):QString("68.0"));
            const auto display=tree->data(tree->indexFor(first,"BatteryTemperature").siblingAtColumn(1)).toString();
            QVERIFY2(display.contains(unit==0?"293.15":unit==1?"20":"68"),qPrintable(display));
            near(first->BatteryTemperature(),293.15);near(w.field(first,"BatteryTemperature")[1].toDouble(),293.15);
        }
        auto prefs=first->UserPrefs();prefs.tempUnitIndex=1;first->setUserPrefs(prefs);
        textEdit(first,"lineEditBatteryTemperature","24.0");near(first->BatteryTemperature(),297.15);
        near(second->BatteryTemperature(),293.15);near(w.field(first,"BatteryTemperature")[1].toDouble(),297.15);
        for(int i=0;i<5;++i){first->setInitialGuiValues();w.window.HotReloadSettings();near(first->BatteryTemperature(),297.15);near(second->BatteryTemperature(),293.15);}
        QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
    void batteryLayoutAndViewer(){
        Workspace w;auto* battery=qobject_cast<BatteryStatus127508*>(w.add("127508"));QVERIFY(w.edit(battery,"BatteryTemperature",305.75));
        QTemporaryDir dir(QDir::currentPath()+"/m216b-battery-layout-XXXXXX");auto file=dir.filePath("battery.json");
        QVERIFY(w.controller->saveLayoutToJson(file));answer(QMessageBox::Yes);QVERIFY(w.controller->loadLayoutFromJson(file));
        battery=w.window.findChild<BatteryStatus127508*>();near(battery->BatteryTemperature(),305.75);
        // Explicit historical zero-K and unavailable Layout values stay authoritative.
        for(double saved:{0.,N2kDoubleNA}){QVERIFY(w.edit(battery,"BatteryTemperature",saved));QVERIFY(w.controller->saveLayoutToJson(file));answer(QMessageBox::Yes);QVERIFY(w.controller->loadLayoutFromJson(file));battery=w.window.findChild<BatteryStatus127508*>();near(battery->BatteryTemperature(),saved);}
        for(int i=0;i<3;++i){w.window.findChild<QAction*>("actionViewer_Rx")->setChecked(true);near(battery->BatteryTemperature(),N2kDoubleNA);w.window.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);near(battery->BatteryTemperature(),N2kDoubleNA);}
        w.window.findChild<QAction*>("actionViewer_Rx")->setChecked(true);
        auto* waiting=qobject_cast<BatteryStatus127508*>(w.add("127508"));near(waiting->BatteryTemperature(),N2kDoubleNA);QCOMPARE(waiting->findChild<QLineEdit*>("lineEditBatteryTemperature")->text(),"Unavailable");
        Nmea2000Handler handler;QSignalSpy received(&handler,&Nmea2000Handler::RcvdPgn);
        for(double measured:{310.25,N2kDoubleNA,0.}){
            tN2kMsg message;SetN2kPGN127508(message,7,12.5,-4,measured,1);QVERIFY(handler.RcvdPgn127508(message));
            const auto parameters=received.takeLast()[1].toStringList();auto* incoming=qobject_cast<BatteryStatus127508*>(w.add("127508",parameters));
            near(incoming->BatteryTemperature(),measured,.01);near(w.field(incoming,"BatteryTemperature")[1].toDouble(),measured,.01);
            const auto mapped=SignalKPublisher::values(message);bool temperatureFound=false;
            for(auto entry:mapped){auto value=entry.toObject();if(value["path"]=="electrical.batteries.7.temperature"){temperatureFound=true;near(value["value"].toDouble(),measured,.01);}}
            QCOMPARE(temperatureFound,measured!=N2kDoubleNA);
        }
        QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
    void batteryWeatherIsolation_data(){QTest::addColumn<bool>("weatherFirst");QTest::newRow("weather-before-battery")<<true;QTest::newRow("weather-after-battery")<<false;}
    void batteryWeatherIsolation(){
        QFETCH(bool,weatherFirst);Workspace w;GeneratorWeather* weather=nullptr;BatteryStatus127508* battery=nullptr;
        auto addWeather=[&]{QTimer declineMovement;declineMovement.setInterval(10);
            connect(&declineMovement,&QTimer::timeout,[]{for(auto* window:QApplication::topLevelWidgets())if(auto* box=qobject_cast<QMessageBox*>(window);box&&box->windowTitle()=="No GeneratorVesselMovement Widget")box->button(QMessageBox::No)->click();});
            declineMovement.start();w.controller->addWidgetAsComboboxTextChanged("-130311","Weather","Generate");declineMovement.stop();weather=w.window.findChild<GeneratorWeather*>();QVERIFY(weather);weather->weatherFetched("offline fixture","No qualified observation timestamp",303.15,50,1000,0,0);};
        if(weatherFirst)addWeather();battery=qobject_cast<BatteryStatus127508*>(w.add("127508"));if(!weatherFirst)addWeather();QVERIFY(battery&&weather);
        // A plausible weather number alone proves neither provenance nor freshness.
        near(weather->AirTemperature(),303.15);near(battery->BatteryTemperature(),293.15);
        QVERIFY(w.edit(battery,"BatteryTemperature",299.15));
        for(double air:{298.15,N2kDoubleNA,0.}){weather->setAirTemperature(air);QTest::qWait(10);battery->setInitialGuiValues();near(battery->BatteryTemperature(),299.15);}
        auto* other=qobject_cast<BatteryStatus127508*>(w.add("127508"));near(other->BatteryTemperature(),293.15);near(battery->BatteryTemperature(),299.15);
        w.window.findChild<QAction*>("actionViewer_Rx")->setChecked(true);weather->weatherFetched("late","ignored",280,20,1000,0,0);near(battery->BatteryTemperature(),299.15);
        w.window.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);near(battery->BatteryTemperature(),299.15);near(other->BatteryTemperature(),293.15);
    }
    void electricalInitialEditingAndStates(){Workspace w;auto* dc=qobject_cast<DcStatus127506*>(w.add("127506"));auto* charger=qobject_cast<ChargerStatus127507*>(w.add("127507"));auto* battery=qobject_cast<BatteryStatus127508*>(w.add("127508"));QVERIFY(dc&&charger&&battery);QCOMPARE(dc->Pgn(),127506);QCOMPARE(dc->findChild<QLineEdit*>("lineEdit")->text(),"80");QCOMPARE(dc->findChild<QLabel*>("labelSource")->text(),"Battery");QCOMPARE(charger->BatteryInstance(),0);QCOMPARE(w.field(charger,"BatteryInstance")[1],"0");
        QCOMPARE(w.field(dc,"TimeRemaining")[2],"s");QCOMPARE(w.field(dc,"Capacity")[2],"C");QCOMPARE(w.field(dc,"StateOfCharge")[3],"uint8_t");QCOMPARE(w.field(charger,"EqualizationTimeRemaining")[2],"s");
        textEdit(dc,"lineEdit","73");QCOMPARE(dc->StateOfCharge(),73);QCOMPARE(w.field(dc,"StateOfCharge")[1],"73");textEdit(dc,"lineEdit","101");QCOMPARE(dc->StateOfCharge(),73);textEdit(dc,"lineEdit","7.5");QCOMPARE(dc->StateOfCharge(),73);QVERIFY(w.edit(dc,"StateOfCharge",61));QCOMPARE(dc->findChild<QLineEdit*>("lineEdit")->text(),"61");QVERIFY(w.edit(dc,"DCType",int(N2kDCt_SolarCell)));QCOMPARE(dc->findChild<QLabel*>("labelSource")->text(),"Solar Cell");
        auto* combo=charger->findChild<QComboBox*>("comboBoxChargeState");for(int state:{0,1,2,3,4,5,6,7,8,9,14,15,11}){QVERIFY(w.edit(charger,"ChargeState",state));charger->setInitialGuiValues();QCOMPARE(int(charger->ChargeState()),state);QCOMPARE(combo->currentData().toInt(),state);QCOMPARE(w.field(charger,"ChargeState")[1].toInt(),state);}
        combo->setCurrentIndex(combo->findData(5));QCOMPARE(charger->ChargeState(),N2kCS_Float);QCOMPARE(w.field(charger,"ChargeState")[1],"5");QVERIFY(w.edit(charger,"BatteryInstance",200));QCOMPARE(charger->BatteryInstance(),200);QVERIFY(w.edit(charger,"Enabled",1));charger->PgnWidgetBase::setEnabled(false);QCOMPARE(charger->Enabled(),N2kOnOff_On);QVERIFY(!charger->PgnWidgetBase::isEnabled());
        textEdit(battery,"lineEditBatteryVoltage","12.6");textEdit(battery,"lineEditBatteryCurrent","-8.5");near(battery->BatteryVoltage(),12.6);near(w.field(battery,"BatteryCurrent")[1].toDouble(),-8.5);UserUnitPrefs prefs=battery->UserPrefs();prefs.tempUnitIndex=1;battery->setUserPrefs(prefs);textEdit(battery,"lineEditBatteryTemperature","25.0");near(battery->BatteryTemperature(),298.15);prefs.tempUnitIndex=2;battery->setUserPrefs(prefs);QCOMPARE(battery->findChild<QLineEdit*>("lineEditBatteryTemperature")->text(),"77.0");near(w.field(battery,"BatteryTemperature")[1].toDouble(),298.15);textEdit(battery,"lineEditBatteryVoltage","nan");near(battery->BatteryVoltage(),12.6);
    }
    void invalidAndUnavailable(){Workspace w;auto* d=qobject_cast<Depth128267*>(w.add("128267"));auto* dc=qobject_cast<DcStatus127506*>(w.add("127506"));auto* battery=qobject_cast<BatteryStatus127508*>(w.add("127508"));QVERIFY(w.edit(d,"DepthBelowTransducer",N2kDoubleNA));QCOMPARE(d->findChild<QLineEdit*>("lineEditDepth")->text(),"Unavailable");d->setInitialGuiValues();QCOMPARE(d->findChild<QLineEdit*>("lineEditDepth")->text(),"Unavailable");QVERIFY(w.edit(dc,"DCType",255));QCOMPARE(dc->findChild<QLabel*>("labelSource")->text(),"Unknown");QVERIFY(w.edit(dc,"StateOfCharge",255));QCOMPARE(dc->findChild<QLineEdit*>("lineEdit")->text(),"Unavailable");QVERIFY(w.edit(battery,"BatteryTemperature",N2kDoubleNA));QCOMPARE(battery->findChild<QLineEdit*>("lineEditBatteryTemperature")->text(),"Unavailable");
        for(QString input:{"bad","nan","inf","-1"})QVERIFY(!w.edit(d,"DepthBelowTransducer",input));QVERIFY(!w.edit(dc,"StateOfCharge",101));QVERIFY(!w.edit(dc,"StateOfCharge",20.5));QVERIFY(!w.edit(battery,"BatteryInstance",256));QVERIFY(!w.edit(battery,"BatteryVoltage",-1));QVERIFY(w.edit(battery,"BatteryCurrent",-10));
        QTest::qWait(30);auto* tree=w.window.findChild<PgnTreeController*>()->getTreeModel();auto index=tree->indexFor(d,"DepthBelowTransducer");QVERIFY(index.isValid());QCOMPARE(tree->data(index.siblingAtColumn(1)).toString(),"Unavailable");
    }
    void layoutCompatibility(){Workspace w;auto* d=w.add("128267");auto* dc=w.add("127506");auto* c=w.add("127507");auto* b=w.add("127508");QVERIFY(w.edit(d,"DepthBelowTransducer",12.34));QVERIFY(w.edit(dc,"Capacity",360000));QVERIFY(w.edit(dc,"TimeRemaining",600));QVERIFY(w.edit(c,"ChargeState",5));QVERIFY(w.edit(b,"BatteryCurrent",-4));
        QTemporaryDir dir(QDir::currentPath()+"/m216b-layout-XXXXXX");auto file=dir.filePath("layout.json");QVERIFY(w.controller->saveLayoutToJson(file));answer(QMessageBox::Yes);QVERIFY(w.controller->loadLayoutFromJson(file));d=w.window.findChild<Depth128267*>();c=w.window.findChild<ChargerStatus127507*>();near(d->property("DepthBelowTransducer").toDouble(),12.34);QCOMPARE(c->property("ChargeState").toInt(),5);
        QFile input(file);QVERIFY(input.open(QIODevice::ReadOnly));auto root=QJsonDocument::fromJson(input.readAll()).object();input.close();auto groups=root["layouts"].toArray();for(int i=0;i<groups.size();++i){auto group=groups[i].toObject();auto widgets=group["widgets"].toArray();for(int j=0;j<widgets.size();++j){auto widget=widgets[j].toObject();auto rows=widget["parameters"].toArray();for(int k=0;k<rows.size();++k){auto row=rows[k].toArray();if(widget["pgn"]=="128267"&&row[0]=="DepthBelowTransducer"){row[1]="100";row[2]="dm";}if(widget["pgn"]=="127506"&&row[0]=="TimeRemaining")row[2]="m";if(widget["pgn"]=="127506"&&row[0]=="Capacity")row[2]="%";rows[k]=row;}widget["parameters"]=rows;widgets[j]=widget;}group["widgets"]=widgets;groups[i]=group;}root["layouts"]=groups;const auto original=QJsonDocument(root).toJson();QVERIFY(input.open(QIODevice::WriteOnly|QIODevice::Truncate));input.write(original);input.close();
        QPointer<QWidget> retained(d);answer(QMessageBox::No);QVERIFY(!w.controller->loadLayoutFromJson(file));QVERIFY(retained);near(d->property("DepthBelowTransducer").toDouble(),12.34);answer(QMessageBox::Yes);QVERIFY(w.controller->loadLayoutFromJson(file));QVERIFY(!retained);d=w.window.findChild<Depth128267*>();dc=w.window.findChild<DcStatus127506*>();near(d->property("DepthBelowTransducer").toDouble(),10);QCOMPARE(w.field(d,"DepthBelowTransducer")[2],"m");QCOMPARE(w.field(dc,"TimeRemaining")[2],"s");near(w.field(dc,"TimeRemaining")[1].toDouble(),600);QCOMPARE(w.field(dc,"Capacity")[2],"C");near(w.field(dc,"Capacity")[1].toDouble(),360000);QVERIFY(input.open(QIODevice::ReadOnly));QCOMPARE(input.readAll(),original);input.close();
        const auto converted=dir.filePath("converted.json");QVERIFY(w.controller->saveLayoutToJson(converted));for(int i=0;i<3;++i){answer(QMessageBox::Yes);QVERIFY(w.controller->loadLayoutFromJson(converted));near(w.window.findChild<Depth128267*>()->DepthBelowTransducer(),10);}
    }
    void protocolReceiveAndModes(){Workspace w;Nmea2000Handler handler;QSignalSpy received(&handler,&Nmea2000Handler::RcvdPgn);w.window.findChild<QAction*>("actionViewer_Rx")->setChecked(true);
        for(unsigned char id:{0,1}){tN2kMsg m;SetN2kPGN128267(m,1,10.25,-.5,50);QVERIFY(handler.RcvdPgn128267(m));auto call=received.takeLast();auto* d=qobject_cast<Depth128267*>(w.add("128267",call[1].toStringList()));near(d->DepthBelowTransducer(),10.25);near(d->Offset(),-.5);near(d->Range(),50);
            SetN2kPGN127506(m,1,id,N2kDCt_Battery,75,90,600,.12,360000);QVERIFY(handler.RcvdPgn127506(m));call=received.takeLast();auto* dc=qobject_cast<DcStatus127506*>(w.add("127506",call[1].toStringList()));QCOMPARE(dc->DCInstance(),id);QCOMPARE(dc->StateOfCharge(),75);QCOMPARE(dc->StateOfHealth(),90);near(dc->RippleVoltage(),.12);near(dc->TimeRemaining(),600);near(dc->Capacity(),360000);
            SetN2kPGN127507(m,id,id,N2kCS_Float,N2kCM_Primary,N2kOnOff_On,N2kOnOff_Off,120);QCOMPARE(int(m.Data[4]),120);QCOMPARE(int(m.Data[5]),0);QVERIFY(handler.RcvdPgn127507(m));call=received.takeLast();auto* c=qobject_cast<ChargerStatus127507*>(w.add("127507",call[1].toStringList()));QCOMPARE(c->Instance(),id);QCOMPARE(c->BatteryInstance(),id);QCOMPARE(c->ChargeState(),N2kCS_Float);QCOMPARE(c->ChargerMode(),N2kCM_Primary);QCOMPARE(c->Enabled(),N2kOnOff_On);QCOMPARE(c->EqualizationPending(),N2kOnOff_Off);near(c->EqualizationTimeRemaining(),120);
            SetN2kPGN127508(m,id,12.5+id,-4,298.15,1);QVERIFY(handler.RcvdPgn127508(m));call=received.takeLast();auto* b=qobject_cast<BatteryStatus127508*>(w.add("127508",call[1].toStringList()));QCOMPARE(b->BatteryInstance(),id);near(b->BatteryVoltage(),12.5+id);near(b->BatteryTemperature(),298.15,.01);
        }
        tN2kMsg missing;SetN2kPGN127507(missing,0,1,N2kCS_Unavailable,N2kCM_Unavailable,N2kOnOff_Unavailable,N2kOnOff_Unavailable,N2kDoubleNA);QVERIFY(handler.RcvdPgn127507(missing));auto unavailable=received.takeLast()[1].toStringList();near(unavailable[unavailable.indexOf("EqualizationTimeRemaining")+1].toDouble(),N2kDoubleNA);
        missing.DataLen=0;QVERIFY(!handler.RcvdPgn127507(missing));QVERIFY(!handler.RcvdPgn127506(missing));QVERIFY(!handler.RcvdPgn127508(missing));QVERIFY(!handler.RcvdPgn128267(missing));
        auto* table=w.window.findChild<PgnTableController*>()->getPgnTableModel();table->setCurrentPgn(w.window.findChild<Depth128267*>()->Id());QVERIFY(!(table->flags(table->index(1,1))&Qt::ItemIsEditable));
        for(int i=0;i<3;++i){w.window.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);w.window.findChild<QAction*>("actionViewer_Rx")->setChecked(true);}QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());QCOMPARE(w.window.findChildren<BatteryStatus127508*>().size(),2);QCOMPARE(w.window.findChildren<ChargerStatus127507*>()[1]->ChargeState(),N2kCS_Float);
    }
    void nativeRendering(){Workspace w;auto* d=w.add("128267");auto* dc=w.add("127506");auto* c=w.add("127507");auto* b=w.add("127508");QVERIFY(w.edit(d,"DepthBelowTransducer",10));QVERIFY(w.edit(dc,"StateOfCharge",75));QVERIFY(w.edit(c,"ChargeState",5));QVERIFY(w.edit(b,"BatteryVoltage",12.5));QVERIFY(w.edit(b,"BatteryTemperature",298.15));
        const auto dir=qEnvironmentVariable("M216B_EVIDENCE");if(dir.isEmpty())return;QDir().mkpath(dir);for(QString theme:{"Dark","Light"}){AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Preference/StyleSheetName",theme);settings.sync();w.window.HotReloadSettings();QTest::qWait(30);for(auto* widget:{d,dc,c,b})QVERIFY(widget->grab().save(dir+"/"+theme+"-"+widget->metaObject()->className()+".png"));}
    }
    void outputAndSignalK(){QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));TestServer server;Nmea2000Handler handler;handler.SetInterfaceType("SignalK");handler.ConfigureSignalK(server.config());handler.StartSignalKSending();QTRY_VERIFY(handler.IsSignalKReady());Workspace w;auto* d=w.add("128267");auto* dc=w.add("127506");auto* c=w.add("127507");auto* b=w.add("127508");QVERIFY(w.edit(d,"DepthBelowTransducer",10));QVERIFY(w.edit(d,"Offset",1));QVERIFY(w.edit(dc,"StateOfCharge",75));QVERIFY(w.edit(dc,"TimeRemaining",600));QVERIFY(w.edit(c,"ChargeState",5));QVERIFY(w.edit(b,"BatteryVoltage",12.5));QVERIFY(w.edit(b,"BatteryCurrent",-4));
        for(auto* widget:{d,dc,c,b})QVERIFY(handler.sendPgn(widget->property("Pgn").toInt(),w.outbound(widget)));QTRY_COMPARE(server.deltas.size(),4);auto v=values(server);near(v["environment.depth.belowTransducer"].toDouble(),10);near(v["environment.depth.belowSurface"].toDouble(),11);near(v["electrical.batteries.0.capacity.stateOfCharge"].toDouble(),.75);near(v["electrical.batteries.0.capacity.timeRemaining"].toDouble(),600);near(v["electrical.batteries.0.voltage"].toDouble(),12.5);near(v["electrical.batteries.0.current"].toDouble(),-4);near(v["electrical.batteries.0.temperature"].toDouble(),293.15,.01);QCOMPARE(v["electrical.chargers.charger0battery0.chargingMode"].toString(),"float");QVERIFY(!v.contains("electrical.batteries.0.capacity.actual"));
        auto params=w.outbound(dc);params.replaceInStrings("StateOfCharge:75","StateOfCharge:75.0");QVERIFY(handler.sendPgn(127506,params));params.replaceInStrings("StateOfCharge:75.0","StateOfCharge:75.5");QVERIFY(!handler.sendPgn(127506,params));handler.StopSignalKSending();
    }
};
int main(int argc,char** argv){QTemporaryDir profile(QDir::currentPath()+"/m216b-profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*> ptr;for(auto& a:args)ptr<<a.data();int count=ptr.size();ptr<<nullptr;QApplication app(count,ptr.data());app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));DepthElectricalTests tests;return QTest::qExec(&tests,argc,argv);}
#include "depth_electrical_tests.moc"
