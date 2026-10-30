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
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QAbstractButton>
#include <QNetworkProxy>
#include <QTemporaryDir>
#include "appsettings.h"
#include "layoutdocument.h"
#include "maincontroller.h"
#include "pgntreemodel.h"
#include "crosstrack129283.h"
#include "N2kMessages.h"

namespace {
void settle() { for (int i=0;i<6;++i) QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall); }
QJsonObject snapshot(bool sourceFirst=true, bool enabled=true, bool dispatch=true) {
    QFile file(":/defaults/defaultLayout.json");
    if (!file.open(QIODevice::ReadOnly)) return {};
    auto root=QJsonDocument::fromJson(file.readAll()).object();
    QJsonObject generatorGroup, instrumentGroup;
    for (const auto &entry:root["layouts"].toArray()) {
        auto group=entry.toObject();
        if (group["groupName"]=="Generate") {
            QJsonArray widgets;
            for (const auto &value:group["widgets"].toArray()) {
                auto widget=value.toObject();
                if (widget["type"]!="GeneratorVesselMovement") continue;
                widget["enabled"]=enabled; widget["dispatch"]=dispatch;
                widgets.append(widget);
            }
            group["widgets"]=widgets; generatorGroup=group;
        } else if (group["groupName"]=="Navigation") {
            auto card=group["widgets"].toArray().first().toObject();
            card["idX"]=1001; card["type"]="CrossTrack129283"; card["pgn"]="129283";
            card["label"]="XTE fixture"; card["enabled"]=true;
            card["parameters"]=QJsonArray{QJsonArray{"SID","0","","unsigned char"},
                QJsonArray{"XTEMode","3","enum","tN2kXTEMode"},
                QJsonArray{"NavigationTerminated","false","","bool"},
                QJsonArray{"XTE","13.125","m","double"}};
            group["widgets"]=QJsonArray{card}; instrumentGroup=group;
        }
    }
    root["layouts"]=sourceFirst?QJsonArray{generatorGroup,instrumentGroup}:QJsonArray{instrumentGroup,generatorGroup};
    return root;
}
struct Workspace {
    QMdiArea area; QTableView table; QTreeView tree;
    PgnTableController tables{&table}; PgnTreeController trees{&tree};
    MainController controller{&tables,&trees,&area,nullptr};
    QTimer dialogs;
    Workspace() {
        controller.setMyVessel(MyVessel{}); controller.setUserPrefs(UserUnitPrefs{});
        controller.setUserOptionPrefs(UserOptionPrefs{});
        for (const QString group:{"Generate","Navigation","Propulsion","Environment","Tanks","Electrical","Other","Communication"})
            controller.addSubWindowForGroup(group,group);
        QObject::connect(&dialogs,&QTimer::timeout,[]{
            if (auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                if(auto *no=box->button(QMessageBox::No)) no->click();
        });
        dialogs.start(0);
    }
    PgnTableModel *model() { return tables.getPgnTableModel(); }
    GeneratorVesselMovement *source() { return area.findChild<GeneratorVesselMovement*>(); }
    CrossTrack129283 *target() { return area.findChild<CrossTrack129283*>(); }
    bool load(const QJsonObject &root, Mode mode=Sender) {
        controller.setMode(mode);
        const bool ok=controller.loadLayoutDocument(root,{},[](const QString&){return true;});
        settle(); trees.getTreeModel()->bind(&controller,model(),&area); return ok;
    }
    QString value(const QString &field) {
        for(const auto &row:model()->getPgnParams(1001)) if(row[0]==field)return row[1];
        return {};
    }
};
void sample(GeneratorVesselMovement *source,double xte) {
    VesselWpBearing value; value.Xte=xte; value.DistanceToWaypointM=50;
    value.CourseRadians=value.BearingPositionToDestinationWaypoint=0;
    source->VesselWpBearingChanged(value);
}
void consistent(Workspace &w,double expected,bool terminated) {
    auto *card=w.target(); QVERIFY(card);
    QCOMPARE(card->XTE(),expected); QCOMPARE(w.value("XTE").toDouble(),expected);
    QCOMPARE(card->NavigationTerminated(),terminated);
    QCOMPARE(w.value("NavigationTerminated").toInt(),int(terminated));
    w.model()->setCurrentPgn(1001);
    bool editor=false;
    for(int row=0;row<w.model()->rowCount();++row) if(w.model()->data(w.model()->index(row,0)).toString()=="XTE") {
        QCOMPARE(w.model()->data(w.model()->index(row,1),Qt::EditRole).toDouble(),expected); editor=true;
    }
    QVERIFY(editor);
    const auto treeIndex=w.trees.getTreeModel()->indexFor(card,"XTE").siblingAtColumn(1);
    QVERIFY(treeIndex.isValid());
    const auto tree=w.trees.getTreeModel()->data(treeIndex).toString();
    const auto display=card->findChild<QLineEdit*>("lineEditXTE")->text();
    if(N2kIsNA(expected)) { QCOMPARE(display,QString("Unavailable")); QCOMPARE(tree,QString("Unavailable")); }
    else {
        const double preferred=card->UserPrefs().distanceUnitIndex==0?expected:expected/0.3048;
        QVERIFY(qAbs(display.toDouble()-preferred)<=0.050001);
        bool ok=false;const double treeNumber=tree.section(' ',0,0).toDouble(&ok);
        QVERIFY2(ok,qPrintable(tree)); QVERIFY(qAbs(treeNumber-preferred)<=0.050001);
    }
    // The real sender consumes this canonical snapshot, not the rounded card.
    const auto data=w.model()->getPgnDataToSend(1001);
    bool found=false;
    for(const auto &row:data.paramDetails) if(row[0]=="XTE") { QCOMPARE(row[1].toDouble(),expected); found=true; }
    QVERIFY(found);
}
}

class XteSampleTests:public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings settings("RFStateSide","RayNmeaSimulator"); settings.clear(); }
    void restoration_data() {
        QTest::addColumn<bool>("sourceFirst"); QTest::addColumn<int>("state"); QTest::addColumn<bool>("isTemplate");
        for(bool first:{false,true})for(int state=0;state<5;++state)for(bool templ:{false,true})
            QTest::newRow(qPrintable(QString("%1-state%2-%3").arg(first?"source-first":"instrument-first").arg(state).arg(templ?"template":"layout")))<<first<<state<<templ;
    }
    void restoration() {
        QFETCH(bool,sourceFirst);QFETCH(int,state);QFETCH(bool,isTemplate);
        auto root=snapshot(sourceFirst,state!=1&&state!=3,state!=2&&state!=3);
        if(isTemplate) {
            root["documentType"]="template";root["template"]=QJsonObject{{"id","xte-fixture"},{"name","XTE fixture"}};root["setupPolicies"]=QJsonArray{};
            QTemporaryDir dir;QSettings settings(dir.filePath("setup.ini"),QSettings::IniFormat);
            QJsonObject resolved;QString error;QVERIFY2(LayoutDocument::resolve(root,settings,resolved,error),qPrintable(error));root=resolved;
        }
        const auto original=root;Workspace w;QVERIFY(w.load(root,state==4?Viewer:Sender));
        consistent(w,13.125,false);QCOMPARE(root,original);
    }
    void firstAndRepeatedSamples() {
        Workspace w;QVERIFY(w.load(snapshot()));auto *source=w.source();QVERIFY(source);
        consistent(w,13.125,false);
        QSignalSpy changes(source,&GeneratorVesselMovement::XTEChanged);
        QSignalSpy measurements(source,&PgnWidgetBase::parametersSampled);
        QSignalSpy targetChanges(w.target(),&CrossTrack129283::XTEChanged);
        for(double value:{0.,2.5,-6.25,N2kDoubleNA}) {
            const int count=changes.size();sample(source,value);settle();consistent(w,value,true);
            QCOMPARE(changes.size(),count+1);QCOMPARE(measurements.size(),count+1);QCOMPARE(targetChanges.size(),count+1);
        }
        // A new sample equal to the producer's current value must replace an
        // independently held destination value, including zero and NA.
        for(double value:{0.,N2kDoubleNA}) {
            sample(source,value);settle();w.target()->setXTE(7.5);settle();
            changes.clear();targetChanges.clear();sample(source,value);settle();
            consistent(w,value,true);QCOMPARE(changes.size(),1);QCOMPARE(targetChanges.size(),1);
        }
        source->setNavigationTerminated(false);settle();consistent(w,N2kDoubleNA,false);
        source->setDispatch(false);source->setNavigationTerminated(true);sample(source,0);settle();consistent(w,N2kDoubleNA,false);
        source->setDispatch(true);settle();consistent(w,N2kDoubleNA,false);
        sample(source,0);settle();consistent(w,0,true);
    }
    void deliveryAndEpochs() {
        Workspace w;QVERIFY(w.load(snapshot()));auto *source=w.source();
        sample(source,0);source->setDispatch(false);source->setDispatch(true);settle();consistent(w,13.125,false);
        source->setEnabled(false);sample(source,-2.5);settle();consistent(w,13.125,false);
        source->setEnabled(true);settle();consistent(w,13.125,false);
        sample(source,0);settle();consistent(w,0,true);
        w.target()->setXTE(13.125);w.target()->setNavigationTerminated(false);settle();
        w.controller.addWidgetAsComboboxTextChanged("129025","Position","Navigation");settle();consistent(w,13.125,false);
        sample(source,-2.5);w.controller.setMode(Viewer);settle();consistent(w,13.125,false);
        sample(source,0);settle();consistent(w,13.125,false);
        w.controller.setMode(Sender);settle();consistent(w,13.125,false);
        sample(source,0);settle();consistent(w,0,true);
    }
    void replacementAndRecreation() {
        Workspace w;QVERIFY(w.load(snapshot()));QPointer<GeneratorVesselMovement> old=w.source();
        sample(old,-2.5);QVERIFY(w.load(snapshot(false)));QVERIFY(old.isNull());consistent(w,13.125,false);
        old=w.source();sample(old,0);w.controller.removeWidgetFromSubForm(old,"Generate");settle();consistent(w,13.125,false);
        w.controller.addGeneratorWidget("Vessel Movement (PGN -129025)");settle();QVERIFY(w.source());consistent(w,13.125,false);
        sample(w.source(),0);settle();consistent(w,0,true);
        QTemporaryDir dir;const auto path=dir.filePath("saved.json");QVERIFY(w.controller.saveLayoutToJson(path));
        auto *source=w.source();source->setDispatch(false);source->setXTE(9.25);settle();
        QVERIFY(w.controller.loadLayoutFromJson(path,[](const QString&){return true;}));settle();
        w.trees.getTreeModel()->bind(&w.controller,w.model(),&w.area);consistent(w,0,true);
    }
    void preferredUnitsAndWire() {
        Workspace w;QVERIFY(w.load(snapshot()));UserUnitPrefs prefs;prefs.distanceUnitIndex=1;w.controller.setUserPrefs(prefs);
        const QList<double> values{0.,2.5,-6.25,N2kDoubleNA};const QList<quint32> expected{0,250,quint32(0xfffffd8f),0x7fffffff};
        for(int i=0;i<values.size();++i) {
            sample(w.source(),values[i]);settle();consistent(w,values[i],true);
            tN2kMsg msg;SetN2kPGN129283(msg,0,N2kxtem_Autonomous,w.value("NavigationTerminated").toInt(),w.value("XTE").toDouble());
            quint32 raw=0;for(int j=0;j<4;++j)raw|=quint32(msg.Data[2+j])<<(8*j);QCOMPARE(raw,expected[i]);
        }
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(false);QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    XteSampleTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "xte_sample_tests.moc"
