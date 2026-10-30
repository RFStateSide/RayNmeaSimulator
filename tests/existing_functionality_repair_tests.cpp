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
#include <QJsonDocument>
#include <QNetworkProxy>
#include "maincontroller.h"
#include "nmeareceiver.h"
#include "N2kMessages.h"
#include "windlassstatus.h"
#include <algorithm>

namespace {
struct Workspace {
    QMdiArea area;
    QTableView table;
    QTreeView tree;
    PgnTableController tables{&table};
    PgnTreeController trees{&tree};
    MainController controller{&tables, &trees, &area, nullptr};
    Nmea2000Handler handler;
    Nmea0183Handler sentences;
    NmeaReceiver receiver{&controller, &tables, &handler, &sentences};
    Workspace() {
        controller.setMode(Viewer); handler.setMode(Viewer);
        controller.setMyVessel(MyVessel{}); controller.setUserPrefs(UserUnitPrefs{});
        controller.setUserOptionPrefs(UserOptionPrefs{});
        for (const QString group : {"Generate","Navigation","Propulsion","Environment","Tanks","Electrical","Other","Communication"})
            controller.addSubWindowForGroup(group, group);
    }
    PgnTableModel *model() { return tables.getPgnTableModel(); }
    QString value(int id, const QString &field) {
        for (const auto &row : model()->getPgnParams(id)) if (row[0] == field) return row[1];
        return {};
    }
};
void settle() { for(int i=0;i<4;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall); }
}
#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QSpinBox>
#include <QMessageBox>
#include <QAbstractButton>
#include "mainwindow.h"
#include "latlong129025.h"
#include "vesselheading127250.h"

// Drive the production modal dialogs; never substitute the creation/binding path.
struct DialogAnswers {
    QTimer timer;
    bool yes = false;
    int instance = 0;
    QStringList prompts;
    DialogAnswers() {
        QObject::connect(&timer,&QTimer::timeout,[this]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(auto *box=qobject_cast<QMessageBox*>(dialog)) {
                prompts << box->windowTitle()+": "+box->text();
                box->button(yes ? QMessageBox::Yes : QMessageBox::No)->click();
            } else if(dialog) if(auto *spin=dialog->findChild<QSpinBox*>()) {
                spin->setValue(instance+1);dialog->accept();
            }
        });
        timer.start(0);
    }
};
struct MetaCalls : QObject {
    int count=0;
    bool eventFilter(QObject*,QEvent *e) override {if(e->type()==QEvent::MetaCall)++count;return false;}
};
GeneratorVesselMovement *movement(Workspace &w) {
    w.controller.addGeneratorWidget("Vessel Movement (PGN -129025)");
    return w.area.findChild<GeneratorVesselMovement*>();
}
#include <QLineEdit>
#include <QJsonArray>
#include "fluidlevel127505.h"
#include "geographicpositiongll.h"
#include "recommendedmingnssrmc.h"
#include "trueheadinghdt.h"
#include "cogsogvtg.h"
#include "actisensecodec.h"
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode mode) override { return QIODevice::open(mode | QIODevice::Unbuffered); }
    qint64 bytesToWrite() const override { return 0; }
protected:
    qint64 writeData(const char *data,qint64 size) override { output.append(data,size);return size; }
};
class ExistingFunctionalityRepairTests : public QObject {
    Q_OBJECT
private slots:
    void fluidSynchronization() {
        Workspace w; tN2kMsg m;
        for(double capacity:{100.,200.}) {
            SetN2kFluidLevel(m,0,N2kft_Fuel,50.,capacity);m.Source=23;
            w.handler.nmea2000MessageReceived(m);settle();
            auto *tank=w.area.findChild<FluidLevel127505*>();QVERIFY(tank);
            QCOMPARE(tank->Capacity(),capacity);QCOMPARE(w.value(tank->Id(),"Capacity").toDouble(),capacity);
            QCOMPARE(w.area.findChildren<FluidLevel127505*>().size(),1);QCOMPARE(tank->Level(),50.);
        }
        auto *tank=w.area.findChild<FluidLevel127505*>();
        w.controller.setMode(Sender);w.model()->setCurrentPgn(tank->Id());
        for(int row=0;row<w.model()->rowCount();++row)if(w.model()->data(w.model()->index(row,0)).toString()=="Capacity")QVERIFY(w.model()->setData(w.model()->index(row,1),"300"));
        settle();QCOMPARE(tank->Capacity(),300.);
        QSignalSpy changed(tank,&FluidLevel127505::LevelChanged);
        tank->setLevel(25.125);settle();QCOMPARE(changed.size(),1);QCOMPARE(w.value(tank->Id(),"Level").toDouble(),25.125);
        tank->setLevel(25.125);QCOMPARE(changed.size(),1);
    }
    void hoursAndTorqueEditors() {
        Workspace w;DialogAnswers answers;w.controller.setMode(Sender);
        w.controller.addWidgetAsComboboxTextChanged("127489","Dynamic","Propulsion");
        auto *dynamic=w.area.findChild<EngineDynamic127489*>();QVERIFY(dynamic);
        dynamic->findChild<QLineEdit*>("lineEditEngineHours")->setText("2.50");
        QVERIFY(QMetaObject::invokeMethod(dynamic,"on_lineEditEngineHours_textEdited",Q_ARG(QString,QString("2.50"))));settle();
        QCOMPARE(dynamic->EngineHours(),9000.);QCOMPARE(w.value(dynamic->Id(),"EngineHours").toDouble(),9000.);
        QCOMPARE(dynamic->findChild<QLineEdit*>("lineEditEngineHours")->text(),QString("2.50"));
        auto *torque=dynamic->findChild<QLineEdit*>("lineEditEngineTorque");QVERIFY(torque);
        torque->setText("-20");QVERIFY(torque->hasAcceptableInput());
        QVERIFY(QMetaObject::invokeMethod(dynamic,"on_lineEditEngineTorque_textEdited",Q_ARG(QString,QString("-20"))));settle();
        QCOMPARE(dynamic->EngineTorque(),-20.);QCOMPARE(w.value(dynamic->Id(),"EngineTorque").toDouble(),-20.);
        // Actual application dispatcher -> experimental Debug memory transport ->
        // pinned decoder. No generator and no physical CAN interface participate.
        auto *port=new MemoryPort;Nmea2000Handler outbound(nullptr,port);outbound.SetInterfaceType("Actisense");
        QVERIFY(outbound.InitializeActisense("MEMORY",QSerialPort::Baud115200,"0x93"));
        QStringList values;for(const auto &row:w.model()->getPgnParams(dynamic->Id()))values<<row[0]+":"+row[1];
        QVERIFY(outbound.sendPgn(127489,values));ActisenseCodec::Parser parser;ActisenseCodec::Message frame;bool parsed=false;
        for(auto byte:port->output)if(parser.feed(quint8(byte),frame))parsed=true;
        QVERIFY(parsed);QCOMPARE(frame.pgn,127489u);tN2kMsg wire;wire.SetPGN(frame.pgn);wire.DataLen=frame.length;std::copy_n(frame.data.begin(),frame.length,wire.Data);
        Nmea2000Handler decoded;QSignalSpy rx(&decoded,&Nmea2000Handler::RcvdPgn);QVERIFY(decoded.RcvdPgn127489(wire));const auto fields=rx.first()[1].toStringList();
        QCOMPARE(fields[fields.indexOf("EngineHours")+1].toDouble(),9000.);QCOMPARE(fields[fields.indexOf("EngineTorque")+1].toInt(),-20);
        auto *load=dynamic->findChild<QLineEdit*>("lineEditEngineLoad");load->setText("-20");QVERIFY(!load->hasAcceptableInput());
        w.controller.addGeneratorWidget("Engine / Outboard (PGN -127489)");
        auto *g=w.area.findChild<GeneratorEngine*>();QVERIFY(g);g->setEnabled(true);
        QVERIFY(QMetaObject::invokeMethod(g,"on_lineEditEngineHours_textEdited",Q_ARG(QString,QString("3.25"))));settle();
        QCOMPARE(g->EngineHours(),11700.);QCOMPARE(dynamic->EngineHours(),11700.);QCOMPARE(w.value(g->Id(),"EngineHours").toDouble(),11700.);
        for(const auto &row:w.model()->getPgnParams(dynamic->Id()))if(row[0]=="EngineHours")QCOMPARE(row[2],QString("s"));
    }
    void hoursKeyboardEditing() {
        Workspace w;DialogAnswers answers;w.controller.setMode(Sender);
        w.controller.addWidgetAsComboboxTextChanged("127489","Dynamic","Propulsion");auto *dynamic=w.area.findChild<EngineDynamic127489*>();QVERIFY(dynamic);
        auto *edit=dynamic->findChild<QLineEdit*>("lineEditEngineHours");QVERIFY(edit);edit->clear();QTest::keyClicks(edit,"2.50");settle();QCOMPARE(edit->text(),QString("2.50"));QCOMPARE(dynamic->EngineHours(),9000.);
        w.controller.addGeneratorWidget("Engine / Outboard (PGN -127489)");auto *g=w.area.findChild<GeneratorEngine*>();QVERIFY(g);g->setEnabled(true);
        edit=g->findChild<QLineEdit*>("lineEditEngineHours");QVERIFY(edit);edit->clear();QTest::keyClicks(edit,"3.25");settle();QCOMPARE(edit->text(),QString("3.25"));QCOMPARE(g->EngineHours(),11700.);QCOMPARE(dynamic->EngineHours(),11700.);
    }
    void boostUnits_data() {
        QTest::addColumn<int>("unit");QTest::addColumn<QString>("entry");QTest::addColumn<double>("pa");
        QTest::newRow("Pa")<<0<<QString("100000.00")<<100000.;
        QTest::newRow("kPa")<<1<<QString("100.00")<<100000.;
        QTest::newRow("Bar")<<2<<QString("1.00")<<100000.;
        QTest::newRow("PSI")<<3<<QString("14.50")<<99973.98075;
    }
    void boostUnits() {
        QFETCH(int,unit);QFETCH(QString,entry);QFETCH(double,pa);
        Workspace w;DialogAnswers answers;w.controller.setMode(Sender);
        w.controller.addWidgetAsComboboxTextChanged("127488","Rapid","Propulsion");
        auto *rapid=w.area.findChild<EngineRapid127488*>();QVERIFY(rapid);
        auto prefs=rapid->UserPrefs();prefs.pressureUnitIndex=unit;rapid->setUserPrefs(prefs);
        auto *editor=rapid->findChild<QLineEdit*>("lineEditBoostPressure");QVERIFY(editor);editor->setText(entry);QVERIFY(editor->hasAcceptableInput());
        QVERIFY(QMetaObject::invokeMethod(rapid,"on_lineEditBoostPressure_textEdited",Q_ARG(QString,entry)));settle();
        QVERIFY(qAbs(rapid->EngineBoostPressure()-pa)<0.1);
    }
    void rmcOrders_data(){QTest::addColumn<bool>("first");QTest::newRow("source-first")<<true;QTest::newRow("instrument-first")<<false;}
    void rmcOrders() {
        QFETCH(bool,first);Workspace w;DialogAnswers answers;w.controller.setMode(Sender);
        if(!first)w.controller.addWidgetAsComboboxTextChanged("RMC","RMC","Navigation");
        auto *g=movement(w);QVERIFY(g);g->setEnabled(true);g->setHeadingReference(N2khr_true);
        if(first)w.controller.addWidgetAsComboboxTextChanged("RMC","RMC","Navigation");
        auto *rmc=w.area.findChild<RecommendedMinGnssRMC*>();QVERIFY(rmc);settle();
        // Movement must supply course/speed even without an optional clock.
        g->setCOG(1.25);g->setSOG(3.5);settle();QCOMPARE(rmc->TrueCOG(),1.25);QCOMPARE(rmc->SOG(),3.5);
        w.controller.addGeneratorWidget("Date Time (PGN -129033)");
        auto *clock=w.area.findChild<GeneratorDateTime*>();QVERIFY(clock);clock->setEnabled(true);settle();
        MetaCalls calls;rmc->installEventFilter(&calls);
        for(int i=0;i<3;++i) {
            calls.count=0;QVERIFY(QMetaObject::invokeMethod(g,"LatitudeChanged",Qt::DirectConnection));settle();QCOMPARE(calls.count,1);
            calls.count=0;QVERIFY(QMetaObject::invokeMethod(g,"LongitudeChanged",Qt::DirectConnection));settle();QCOMPARE(calls.count,1);
            calls.count=0;QVERIFY(QMetaObject::invokeMethod(clock,"SecondsSinceMidnightChanged",Qt::DirectConnection));settle();QCOMPARE(calls.count,1);
            w.controller.setMode(Viewer);w.controller.setMode(Sender);
        }
    }
    void gllStatusSnapshot() {
        Workspace w;DialogAnswers answers;
        tNMEA0183Msg msg;QVERIFY(msg.SetMessage("$GPGLL,4230.000,S,07015.000,E,123456.50,V*0A"));
        // Use production receive entry with a parsed sentence (SetMessage verifies checksum separately in framing tests).
        QVERIFY(w.sentences.RcvdSntGLL(msg,"GP"));settle();
        auto *gll=w.area.findChild<GeographicPositionGLL*>();QVERIFY(gll);QCOMPARE(gll->Status(),'V');QCOMPARE(w.value(gll->Id(),"Status"),QString("V"));
        QTemporaryDir dir;const auto file=dir.filePath("gll.json");QVERIFY(w.controller.saveLayoutToJson(file));answers.yes=true;
        QVERIFY(w.controller.loadLayoutFromJson(file));gll=w.area.findChild<GeographicPositionGLL*>();QVERIFY(gll);QCOMPARE(gll->Status(),'V');
        w.controller.setMode(Sender);w.model()->setCurrentPgn(gll->Id());
        for(int row=0;row<w.model()->rowCount();++row)if(w.model()->data(w.model()->index(row,0)).toString()=="Status")QVERIFY(w.model()->setData(w.model()->index(row,1),"A"));
        settle();QCOMPARE(gll->Status(),'A');gll->setStatus('V');settle();QCOMPARE(w.value(gll->Id(),"Status"),QString("V"));
        QFile f(file);QVERIFY(f.open(QIODevice::ReadOnly));auto doc=QJsonDocument::fromJson(f.readAll());f.close();
        auto root=doc.object();auto layouts=root["layouts"].toArray();
        for(int i=0;i<layouts.size();++i){auto group=layouts[i].toObject();auto widgets=group["widgets"].toArray();for(int j=0;j<widgets.size();++j){auto widget=widgets[j].toObject();if(widget["pgn"].toString()!="GLL")continue;auto params=widget["parameters"].toArray();for(int k=params.size()-1;k>=0;--k)if(params[k].toArray()[0].toString()=="Status")params.removeAt(k);widget["parameters"]=params;auto ranges=widget["ranges"].toArray();if(ranges.size()==4)ranges.removeLast();widget["ranges"]=ranges;widgets[j]=widget;}group["widgets"]=widgets;layouts[i]=group;}root["layouts"]=layouts;
        QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(root).toJson());f.close();QVERIFY(w.controller.loadLayoutFromJson(file));gll=w.area.findChild<GeographicPositionGLL*>();QVERIFY(gll);QCOMPARE(gll->Status(),'A');QCOMPARE(gll->Latitude(),-42.5);
    }
    void magneticSentenceReferences() {
        Workspace w;DialogAnswers answers;w.controller.setMode(Sender);auto *g=movement(w);QVERIFY(g);g->setEnabled(true);
        for(const auto &code:{"HDT","RMC","VTG"})w.controller.addWidgetAsComboboxTextChanged(code,code,"Navigation");
        auto *hdt=w.area.findChild<TrueHeadingHDT*>();auto *rmc=w.area.findChild<RecommendedMinGnssRMC*>();auto *vtg=w.area.findChild<CogSogVTG*>();QVERIFY(hdt);QVERIFY(rmc);QVERIFY(vtg);
        g->setHeadingReference(N2khr_magnetic);g->setVariation(0.1);g->setDeviation(0.02);g->setHeading(1.0);g->setCOG(1.5);settle();
        QVERIFY(qAbs(hdt->Heading()-1.12)<1e-12);QVERIFY(qAbs(rmc->TrueCOG()-1.6)<1e-12);QVERIFY(qAbs(vtg->TrueCOG()-1.6)<1e-12);QVERIFY(qAbs(vtg->MagneticCOG()-1.5)<1e-12);
        g->setVariation(0.2);settle();QVERIFY(qAbs(hdt->Heading()-1.22)<1e-12);QVERIFY(qAbs(rmc->TrueCOG()-1.7)<1e-12);
        g->setHeadingReference(N2khr_true);settle();QCOMPARE(hdt->Heading(),1.0);QCOMPARE(rmc->TrueCOG(),1.5);QVERIFY(qAbs(vtg->MagneticCOG()-1.3)<1e-12);
        g->setHeadingReference(N2khr_magnetic);g->setVariation(N2kDoubleNA);settle();QCOMPARE(hdt->Heading(),N2kDoubleNA);QCOMPARE(rmc->TrueCOG(),N2kDoubleNA);QCOMPARE(vtg->TrueCOG(),N2kDoubleNA);
    }
    void legacyUnitMetadata() {
        Workspace w;DialogAnswers answers;w.controller.setMode(Sender);
        w.controller.addWidgetAsComboboxTextChanged("127489","Dynamic","Propulsion");auto *engine=w.area.findChild<EngineDynamic127489*>();QVERIFY(engine);engine->setEngineHours(123456.789);
        auto *g=movement(w);QVERIFY(g);g->setRateOfTurn(0.0123456789);settle();
        QTemporaryDir dir;const auto file=dir.filePath("legacy.json");QVERIFY(w.controller.saveLayoutToJson(file));QFile f(file);QVERIFY(f.open(QIODevice::ReadOnly));auto root=QJsonDocument::fromJson(f.readAll()).object();f.close();auto layouts=root["layouts"].toArray();
        for(int i=0;i<layouts.size();++i){auto group=layouts[i].toObject();auto widgets=group["widgets"].toArray();for(int j=0;j<widgets.size();++j){auto widget=widgets[j].toObject();auto params=widget["parameters"].toArray();for(int k=0;k<params.size();++k){auto row=params[k].toArray();if(widget["pgn"].toString()=="127489"&&row[0].toString()=="EngineHours")row[2]="hr";if(widget["pgn"].toString()=="-129025"&&row[0].toString()=="RateOfTurn")row[2]="deg/s";params[k]=row;}widget["parameters"]=params;widgets[j]=widget;}group["widgets"]=widgets;layouts[i]=group;}root["layouts"]=layouts;
        const auto historical=QJsonDocument(root).toJson();QVERIFY(f.open(QIODevice::WriteOnly));f.write(historical);f.close();answers.yes=true;QVERIFY(w.controller.loadLayoutFromJson(file));
        engine=w.area.findChild<EngineDynamic127489*>();g=w.area.findChild<GeneratorVesselMovement*>();QVERIFY(engine);QVERIFY(g);QCOMPARE(engine->EngineHours(),123456.789);QCOMPARE(g->RateOfTurn(),0.0123456789);
        for(const auto &row:w.model()->getPgnParams(engine->Id()))if(row[0]=="EngineHours")QCOMPARE(row[2],QString("s"));
        for(const auto &row:w.model()->getPgnParams(g->Id()))if(row[0]=="RateOfTurn")QCOMPARE(row[2],QString("rad/s"));
        QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),historical);
    }
    void canonicalRxPrecision() {
        Workspace w;tN2kMsg m;
        SetN2kEngineDynamicParam(m,0,100000.,310.,320.,13.,4.,123456789.,N2kDoubleNA,N2kDoubleNA,50,-20,tN2kEngineDiscreteStatus1{},tN2kEngineDiscreteStatus2{});m.Source=23;
        w.handler.nmea2000MessageReceived(m);settle();auto *dynamic=w.area.findChild<EngineDynamic127489*>();QVERIFY(dynamic);QCOMPARE(w.value(dynamic->Id(),"EngineHours").toDouble(),123456789.);QCOMPARE(dynamic->EngineHours(),123456789.);QCOMPARE(dynamic->EngineTorque(),-20.);
        SetN2kSystemTime(m,1,20000,43200.1234,N2ktimes_GPS);m.Source=23;w.handler.nmea2000MessageReceived(m);settle();
        const auto rows=w.model()->getPgns("126992");QCOMPARE(rows.size(),1);QVERIFY(qAbs(w.value(rows[0].idX,"SystemTime").toDouble()-43200.1234)<0.00011);
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    ExistingFunctionalityRepairTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "existing_functionality_repair_tests.moc"
