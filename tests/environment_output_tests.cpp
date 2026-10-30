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
#include <QTemporaryDir>
#include <QMessageBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QAction>
#include <QNetworkProxy>
#include "mainwindow.h"
#include "appsettings.h"
#include "environmentalinstruments.h"
#include "generatorweather.h"
#include "outsideenvironmental130310.h"
#include "temperaturewidget130312.h"
#include "temperatureextended130316.h"
#include "pgntreecontroller.h"
#include "nmeareceiver.h"
#include "nmeasenderworker.h"
#include "signalk_testserver.h"
#include "themes.h"
namespace {
void confirm() {
    QTimer::singleShot(0, [] { for (auto *w : QApplication::topLevelWidgets())
        if (auto *b = qobject_cast<QMessageBox*>(w)) b->button(QMessageBox::Yes)->click(); });
}
QJsonObject mapped(const tN2kMsg &m) {
    QJsonObject r; for (const auto &v : SignalKPublisher::values(m)) {
        const auto o = v.toObject(); r[o["path"].toString()] = o["value"];
    } return r;
}
void near(double actual, double expected, double tolerance = .00011) {
    QVERIFY2(qAbs(actual-expected) < tolerance, qPrintable(QString("%1 != %2").arg(actual,0,'g',17).arg(expected,0,'g',17)));
}
struct Workspace {
    MainWindow window;
    MainController *controller = window.findChild<MainController*>();
    PgnTableController *table = window.findChild<PgnTableController*>();
    PgnTableModel *model = table->getPgnTableModel();
    Workspace() { window.resize(1400,950); window.show(); }
    GeneratorWeather *weather() {
        QTimer decline; decline.setInterval(1);
        QObject::connect(&decline,&QTimer::timeout,[] { for(auto *widget:QApplication::topLevelWidgets())
            if(auto *box=qobject_cast<QMessageBox*>(widget)) box->button(QMessageBox::No)->click(); });
        decline.start(); controller->addWidgetAsComboboxTextChanged("-130311", "Weather", "Generate");
        decline.stop();
        auto *w=window.findChild<GeneratorWeather*>(); w->setEnabled(true); return w;
    }
    EnvironmentalInstrument *add(int pgn) {
        controller->addWidgetAsComboboxTextChanged(QString::number(pgn), pgn == 130306 ? "Wind" : "Humidity", "Environment");
        auto all=window.findChildren<EnvironmentalInstrument*>();
        EnvironmentalInstrument *result=nullptr;
        for (auto *w : all) if (w->Pgn()==pgn && (!result || w->Id()>result->Id())) result=w;
        return result;
    }
    double value(PgnWidgetBase *w, const QString &field) {
        for (const auto &row : model->getPgnParams(w->Id())) if (row[0]==field) return row[1].toDouble();
        return qQNaN();
    }
};
}
class EnvironmentOutputTests : public QObject {
    Q_OBJECT
private slots:
    void windContracts() {
        tN2kMsg m; SetN2kPGN130306(m,9,7.5,M_PI/2,N2kWind_True_North);
        QCOMPARE(m.DataLen,8); QCOMPARE(m.Data[0],9); QCOMPARE(m.Data[1],0xee); QCOMPARE(m.Data[2],2);
        unsigned char sid; double speed,angle; tN2kWindReference ref;
        QVERIFY(ParseN2kPGN130306(m,sid,speed,angle,ref)); near(speed,7.5); near(angle,M_PI/2); QCOMPARE(int(ref),0);
        auto v=mapped(m); near(v["environment.wind.directionTrue"].toDouble(),M_PI/2); near(v["environment.wind.speedOverGround"].toDouble(),7.5);
        QVERIFY(!v.contains("environment.wind.angleApparent"));
        const QStringList paths{"environment.wind.directionTrue","environment.wind.directionMagnetic","environment.wind.angleApparent","environment.wind.angleTrueGround","environment.wind.angleTrueWater"};
        for (int i=0;i<5;++i) {
            SetN2kPGN130306(m,0,0,3*M_PI/2,tN2kWindReference(i)); v=mapped(m);
            near(v[paths[i]].toDouble(),i<2?3*M_PI/2:-M_PI/2); QCOMPARE(v.size(),2);
        }
        SetN2kPGN130306(m,0,N2kDoubleNA,0,N2kWind_True_North);
        QVERIFY(mapped(m)["environment.wind.speedOverGround"].isNull()); QCOMPARE(mapped(m)[paths[0]].toDouble(-1),0.);
        SetN2kPGN130306(m,0,0,N2kDoubleNA,N2kWind_True_North);
        QCOMPARE(mapped(m)["environment.wind.speedOverGround"].toDouble(-1),0.); QVERIFY(mapped(m)[paths[0]].isNull());
        SetN2kPGN130306(m,0,N2kDoubleNA,N2kDoubleNA,N2kWind_True_North);
        QVERIFY(mapped(m)[paths[0]].isNull());
        SetN2kPGN130306(m,0,0,0,N2kWind_True_North); QCOMPARE(mapped(m)[paths[0]].toDouble(-1),0.);
        SetN2kPGN130306(m,0,2,1,tN2kWindReference(7)); QVERIFY(mapped(m).isEmpty());
    }
    void humidityAndExistingMappings() {
        tN2kMsg m; unsigned char sid,id; tN2kHumiditySource source; double actual,set;
        SetN2kPGN130313(m,3,4,N2khs_OutsideHumidity,62.5,N2kDoubleNA);
        QVERIFY(ParseN2kPGN130313(m,sid,id,source,actual,set)); QCOMPARE(id,4); near(actual,62.5,.0041); QCOMPARE(set,N2kDoubleNA);
        near(mapped(m)["environment.outside.relativeHumidity"].toDouble(),.625);
        SetN2kPGN130313(m,3,5,N2khs_InsideHumidity,0,40);
        QCOMPARE(mapped(m)["environment.inside.sensor5.relativeHumidity"].toDouble(-1),0.);
        SetN2kPGN130313(m,3,4,N2khs_OutsideHumidity,N2kDoubleNA,N2kDoubleNA);
        QVERIFY(mapped(m)["environment.outside.relativeHumidity"].isNull());
        SetN2kPGN130310(m,0,284.12,296.34,100900); auto v=mapped(m);
        near(v["environment.water.temperature"].toDouble(),284.12); near(v["environment.outside.pressure"].toDouble(),100900,100.1);
        SetN2kPGN130312(m,0,2,N2kts_DewPointTemperature,285.12,N2kDoubleNA);
        near(mapped(m)["environment.outside.dewPointTemperature"].toDouble(),285.12);
        SetN2kPGN130316(m,0,3,N2kts_DewPointTemperature,N2kDoubleNA,N2kDoubleNA);
        QVERIFY(mapped(m)["environment.outside.dewPointTemperature"].isNull());
    }
    void manualModelAndUnits() {
        Workspace w; auto *wind=w.add(130306); auto *hum=w.add(130313);
        QCOMPARE(wind->value("WindSpeed"),N2kDoubleNA); // Base API tested through value below.
        wind->setValue("WindSpeed",10); wind->setValue("WindAngle",M_PI); hum->setValue("ActualHumidity",65);
        QTRY_COMPARE(w.value(wind,"WindSpeed"),10.); QTRY_COMPARE(w.value(hum,"ActualHumidity"),65.);
        w.model->setCurrentPgn(wind->Id());
        QVERIFY(w.model->setData(w.model->index(1,1),"5.25")); QCOMPARE(wind->value("WindSpeed"),5.25);
        QVERIFY(!w.model->setData(w.model->index(1,1),"bad"));
        QVERIFY(!w.model->setData(w.model->index(3,1),"5"));
        auto prefs=wind->UserPrefs(); prefs.speedUnitIndex=3; wind->setUserPrefs(prefs);
        near(wind->findChild<QLineEdit*>("WindSpeed")->text().toDouble(),5.25*3600/1852,.011);
        near(wind->findChild<QLineEdit*>("WindAngle")->text().toDouble(),180.,.011);
        auto *tree=w.window.findChild<PgnTreeController*>()->getTreeModel();
        QTRY_VERIFY(tree->indexFor(wind,"WindSpeed").isValid());
        auto idx=tree->indexFor(wind,"WindSpeed");
        QVERIFY(idx.siblingAtColumn(1).data().toString().contains("knots"));
        QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
    void observationBindingsAndOwnership() {
        Workspace w; auto *weather=w.weather(); auto *wind=w.add(130306); auto *hum=w.add(130313); auto *inside=w.add(130313);
        inside->setValue("HumidityInstance",1); inside->setValue("HumiditySource",0); inside->setValue("ActualHumidity",77);
        weather->setWindSpeed(7.5); weather->setWindAngle(M_PI/2); weather->setHumidity(62.5);
        QTRY_COMPARE(wind->value("WindSpeed"),7.5); QTRY_COMPARE(hum->value("ActualHumidity"),62.5);
        near(w.value(wind,"WindAngle"),M_PI/2); QCOMPARE(inside->value("ActualHumidity"),77.);
        wind->setValue("WindReference",2); wind->setValue("WindSpeed",3);
        weather->setWindSpeed(12); QTest::qWait(30); QCOMPARE(wind->value("WindSpeed"),3.);
        weather->setEnabled(false); weather->setHumidity(0); QTest::qWait(30); QCOMPARE(hum->value("ActualHumidity"),62.5);
        weather->setEnabled(true); weather->setHumidity(N2kDoubleNA); QTRY_COMPARE(hum->value("ActualHumidity"),N2kDoubleNA);
        QCOMPARE(inside->value("ActualHumidity"),77.);
        weather->setHumiditySource(N2khs_InsideHumidity); weather->setHumidity(42);
        auto *unbound=w.add(130313); QCOMPARE(unbound->value("ActualHumidity"),N2kDoubleNA);
        QTest::qWait(30); QCOMPARE(hum->value("ActualHumidity"),N2kDoubleNA);
    }
    void freshnessAndPartialObservations() {
        Workspace w; auto *weather=w.weather();
        weather->findChild<QCheckBox*>("weatherRetainObservations")->setChecked(false); auto *wind=w.add(130306); auto *hum=w.add(130313);
        weather->findChild<QComboBox*>("comboBoxApiSource")->setCurrentIndex(1);
        WeatherObservation o; o.provider="OpenWeatherMap"; o.observed=QDateTime::currentDateTimeUtc(); o.retrieved=o.observed;
        o.windSpeed=0; o.windAngle=0; o.humidity=0; weather->acceptObservation(o);
        QTRY_COMPARE(wind->value("WindSpeed"),0.); QTRY_COMPARE(hum->value("ActualHumidity"),0.);
        o.windSpeed=4; o.humidity=N2kDoubleNA; weather->acceptObservation(o);
        QTRY_COMPARE(wind->value("WindSpeed"),4.); QTRY_COMPARE(hum->value("ActualHumidity"),N2kDoubleNA);
        weather->setHumidity(55); o.humidity=N2kDoubleNA; weather->acceptObservation(o);
        QTRY_COMPARE(hum->value("ActualHumidity"),55.); // Generator's manual ownership stays authoritative.
        weather->cancelWeather("fixture cancellation"); QTRY_COMPARE(wind->value("WindSpeed"),N2kDoubleNA);
        QCOMPARE(hum->value("ActualHumidity"),55.);
        o.windSpeed=99; o.observed=o.observed.addSecs(-10000); weather->acceptObservation(o);
        QCOMPARE(wind->value("WindSpeed"),N2kDoubleNA);
    }
    void viewerLifecycleAndLayout() {
        Workspace w; auto *weather=w.weather(); auto *wind=w.add(130306); auto *hum=w.add(130313);
        hum->setValue("HumidityInstance",8); hum->setValue("HumiditySource",0); hum->setValue("ActualHumidity",34);
        wind->setValue("WindSpeed",4);
        weather->setWindSpeed(99); w.controller->setMode(Viewer); QTest::qWait(30);
        QCOMPARE(wind->value("WindSpeed"),4.);
        w.model->changeParams(wind->Id(),{"WindSpeed","11"}); QCOMPARE(wind->value("WindSpeed"),11.);
        w.controller->setMode(Sender); QTest::qWait(30); QCOMPARE(wind->value("WindSpeed"),11.);
        QTemporaryDir dir; const auto file=dir.filePath("environment.json"); QVERIFY(w.controller->saveLayoutToJson(file));
        QPointer<EnvironmentalInstrument> old=wind; confirm(); QVERIFY(w.controller->loadLayoutFromJson(file)); QTRY_VERIFY(old.isNull());
        wind=w.window.findChild<WindData130306*>(); hum=w.window.findChild<Humidity130313*>(); QVERIFY(wind&&hum);
        QCOMPARE(wind->value("WindSpeed"),11.); QCOMPARE(hum->value("HumidityInstance"),8.); QCOMPARE(hum->value("ActualHumidity"),34.);
        QCOMPARE(hum->findChild<QComboBox*>("HumiditySource")->currentData().toInt(),0);
        confirm(); QVERIFY(w.controller->loadLayoutFromJson(":/defaults/defaultLayout.json"));
    }
    void dewPointAndExistingBindings() {
        Workspace w; auto *weather=w.weather();
        QTimer picker; picker.setInterval(1);
        connect(&picker,&QTimer::timeout,[] { for (auto *widget : QApplication::topLevelWidgets())
            if (auto *dialog=qobject_cast<QDialog*>(widget); dialog && dialog->windowTitle()=="Select Temperature Source") {
                auto *combo=dialog->findChild<QComboBox*>(); combo->setCurrentIndex(combo->findData(9)); dialog->accept();
            }
        });
        picker.start(); w.controller->addWidgetAsComboboxTextChanged("130312","Dew point","Environment"); picker.stop();
        auto *newDew=w.window.findChild<TemperatureWidget130312*>(); QVERIFY(newDew);
        QCOMPARE(newDew->TemperatureSource(),N2kts_DewPointTemperature); QCOMPARE(newDew->ActualTemperature(),N2kDoubleNA);
        weather->setDewPoint(284); QTRY_COMPARE(newDew->ActualTemperature(),284.);
        w.controller->setMode(Viewer);
        w.controller->addRcvdPgn("130312", {"SID","0","TemperatureInstance","4","TemperatureSource","9","ActualTemperature","280","SetTemperature","-1000000000"}, "fixture");
        w.controller->addRcvdPgn("130316", {"SID","0","TempInstance","5","TempSource","9","ActualTemperature","281","SetTemperature","-1000000000"}, "fixture");
        w.controller->addRcvdPgn("130310", {"SID","0","WaterTemperature","282","OutsideAmbientAirTemperature","290","AtmosphericPressure","100900"}, "fixture");
        auto *a=w.window.findChild<TemperatureWidget130312*>(); auto *b=w.window.findChild<TemperatureExtended130316*>();
        auto *outside=w.window.findChild<OutsideEnvironmental130310*>(); QVERIFY(a&&b&&outside);
        QCOMPARE(outside->WaterTemperature(),282.); QCOMPARE(outside->OutsideAmbientAirTemperature(),290.);
        w.controller->setMode(Sender); weather->setDewPoint(286); weather->setAirTemperature(295); weather->setSeaTemperature(283); weather->setAtmosphericPressure(100800);
        QTRY_COMPARE(a->ActualTemperature(),286.); QTRY_COMPARE(b->ActualTemperature(),286.);
        QTRY_COMPARE(outside->WaterTemperature(),283.); QTRY_COMPARE(outside->OutsideAmbientAirTemperature(),295.); QTRY_COMPARE(outside->AtmosphericPressure(),100800.);
        weather->setDewPoint(N2kDoubleNA); QTRY_COMPARE(a->ActualTemperature(),N2kDoubleNA); QTRY_COMPARE(b->ActualTemperature(),N2kDoubleNA);
        a->setTemperatureSource(N2kts_InsideTemperature); a->setActualTemperature(300); weather->setDewPoint(287); QTest::qWait(40); QCOMPARE(a->ActualTemperature(),300.);
    }
    void receiveIdentity() {
        Workspace w; w.controller->setMode(Viewer); auto *handler=w.window.findChild<Nmea2000Handler*>(); QVERIFY(handler); handler->setMode(Viewer);
        tN2kMsg m;
        for (int id : {0,1,0}) { SetN2kPGN130313(m,0,id,N2khs_OutsideHumidity,40+id,N2kDoubleNA); m.Source=23; handler->nmea2000MessageReceived(m); }
        QTRY_COMPARE(w.window.findChildren<Humidity130313*>().size(),2);
        for (auto *h : w.window.findChildren<Humidity130313*>()) {
            QCOMPARE(h->ActualHumidity(),40.+h->HumidityInstance()); QCOMPARE(w.model->findPgn(h->Id())->source,QString("23"));
        }
        for (int ref : {0,2,0}) { SetN2kPGN130306(m,0,5,1,tN2kWindReference(ref)); m.Source=23; handler->nmea2000MessageReceived(m); }
        QTRY_COMPARE(w.window.findChildren<WindData130306*>().size(),2);
        SetN2kPGN130306(m,0,2,1,N2kWind_True_North); m.Source=24; handler->nmea2000MessageReceived(m);
        QTRY_COMPARE(w.window.findChildren<WindData130306*>().size(),3);
        SetN2kPGN130313(m,0,2,tN2kHumiditySource(3),120,-1); m.Source=23; handler->nmea2000MessageReceived(m);
        QTRY_COMPARE(w.window.findChildren<Humidity130313*>().size(),3);
        for (auto *h : w.window.findChildren<Humidity130313*>()) if (h->HumidityInstance()==2) {
            QCOMPARE(h->HumiditySource(),255); QCOMPARE(h->ActualHumidity(),N2kDoubleNA); QCOMPARE(h->SetHumidity(),N2kDoubleNA);
            QCOMPARE(w.value(h,"ActualHumidity"),N2kDoubleNA);
        }
        m.DataLen=2; QVERIFY(!handler->RcvdEnvironmental(m));
    }
    void authorizedLoopbackAndStop() {
        TestServer server; Workspace w; auto *weather=w.weather(); weather->setWindSpeed(8); weather->setWindAngle(1.5); weather->setHumidity(70);
        auto *wind=w.add(130306); auto *hum=w.add(130313); wind->setEnabled(true); hum->setEnabled(true);
        auto *second=w.add(130313); second->setValue("HumidityInstance",1); second->setValue("ActualHumidity",45); second->setEnabled(true);
        auto *handler=w.window.findChild<Nmea2000Handler*>(); handler->SetInterfaceType("SignalK"); handler->ConfigureSignalK(server.config());
        NmeaSenderWorker sender; sender.setPgnTableController(w.table);
        connect(&sender,&NmeaSenderWorker::sendPgn,handler,&Nmea2000Handler::sendPgn);
        QTest::qWait(100); QVERIFY(server.deltas.isEmpty()); QVERIFY(!handler->IsSignalKActive());
        handler->StartSignalKSending(); QTRY_VERIFY(handler->IsSignalKReady()); QVERIFY(server.deltas.isEmpty());
        sender.startSending(); QTRY_VERIFY(server.deltas.size()>=3);
        bool sawWind=false,sawHumidity=false,sawSecond=false;
        for (const auto &delta : server.deltas) for (const auto &u : delta["updates"].toArray()) {
            const auto update=u.toObject(); auto samples=update["values"].toArray();
            for (const auto &sample : samples) {
                const auto v=sample.toObject();
                if (v["path"]=="environment.wind.speedOverGround") { sawWind=true; near(v["value"].toDouble(),8); QCOMPARE(update["$source"].toString(),QString("test-publisher")); }
                if (v["path"]=="environment.outside.relativeHumidity") {
                    if (update["$source"].toString()=="test-publisher-humidity0") { sawHumidity=true; near(v["value"].toDouble(),.7); }
                    else { sawSecond=true; near(v["value"].toDouble(),.45); QCOMPARE(update["$source"].toString(),QString("test-publisher-humidity1")); }
                }
            }
        }
        QVERIFY(sawWind&&sawHumidity&&sawSecond); sender.stopSending(); handler->StopSignalKSending();
        const int count=server.deltas.size(); weather->setWindSpeed(9); QTest::qWait(350); QCOMPARE(server.deltas.size(),count);
        QVERIFY(!handler->sendPgn(130306,{"SID:0","WindSpeed:4","WindAngle:1","WindReference:0"}));
        QVERIFY(!handler->sendPgn(130306,{"SID:0","WindSpeed:bad","WindAngle:1","WindReference:0"}));
        sender.setMode(Viewer); sender.startSending(); QVERIFY(!sender.isSending());
    }
    void schedulerEnableAndMode() {
        Workspace w; auto *wind=w.add(130306); auto *hum=w.add(130313);
        wind->setEnabled(true); hum->setEnabled(false);
        NmeaSenderWorker sender; sender.setPgnTableController(w.table); QSignalSpy sent(&sender,&NmeaSenderWorker::sendPgn);
        sender.sendNmeaMessage(); QCOMPARE(sent.count(),0);
        sender.startSending(); sender.sendNmeaMessage(); QCOMPARE(sent.count(),1); QCOMPARE(sent[0][0].toInt(),130306);
        sender.sendNmeaMessage(); QCOMPARE(sent.count(),1); // Interval has not elapsed.
        hum->setEnabled(true); sender.sendNmeaMessage(); QCOMPARE(sent.count(),2); QCOMPARE(sent[1][0].toInt(),130313);
        sender.stopSending(); sender.sendNmeaMessage(); QCOMPARE(sent.count(),2);
        sender.startSending(); sender.setMode(Viewer); sender.sendNmeaMessage(); QCOMPARE(sent.count(),2);
        sender.setMode(Sender); sender.sendNmeaMessage(); QCOMPARE(sent.count(),2); QVERIFY(!sender.isSending());
    }
    void renderThemes() {
        Workspace w; auto *wind=w.add(130306); auto *hum=w.add(130313); wind->setValue("WindSpeed",8.75); wind->setValue("WindAngle",M_PI/2); hum->setValue("ActualHumidity",62.5);
        QTest::qWait(40);
        auto *tree=w.window.findChild<PgnTreeController*>();
        QVERIFY(QMetaObject::invokeMethod(tree,"navigate",Qt::DirectConnection,Q_ARG(QModelIndex,tree->getTreeModel()->indexFor(wind))));
        for (const QString name : {"Dark","Light"}) {
            AppSettings settings(APP_COMPANY,APP_NAME); settings.setValue("Preference/StyleSheetName",name); settings.sync();
            w.window.HotReloadSettings(); QTest::qWait(80);
            for (auto *instrument : {wind,hum}) {
                QVERIFY(instrument->isVisible());
                for (auto *edit : instrument->findChildren<QLineEdit*>())
                    QVERIFY(instrument->rect().contains(QRect(edit->mapTo(instrument,QPoint()),edit->size())));
            }
            const auto dir=qEnvironmentVariable("M217B_EVIDENCE");
            if (!dir.isEmpty()) { QDir().mkpath(dir); QVERIFY(w.window.grab().save(dir+"/"+name+"-environment.png")); }
        }
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile(QDir::currentPath()+"/m217b-profile-XXXXXX"); if(!profile.isValid()) return 2;
    qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QList<QByteArray> args; for(int i=0;i<argc;++i) args << argv[i]; args << "--no-nmea";
    QList<char*> pointers; for(auto &a:args) pointers<<a.data(); int count=pointers.size(); pointers<<nullptr;
    QApplication app(count,pointers.data()); app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    EnvironmentOutputTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "environment_output_tests.moc"
