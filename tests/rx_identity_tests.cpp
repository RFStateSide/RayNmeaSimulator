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
QStringList pairs(const QList<QList<QString>> &rows) {QStringList p;for(const auto &r:rows)p<<r[0]<<r[1];return p;}
void reversePairs(QStringList &p) {QStringList r;for(int i=p.size()-2;i>=0;i-=2)r<<p[i]<<p[i+1];p=r;}
void engine(Workspace &w,int instance,int source,double rpm=1200) {tN2kMsg m;SetN2kEngineParamRapid(m,instance,rpm,N2kDoubleNA,N2kInt8NA);m.Source=source;w.handler.nmea2000MessageReceived(m);}
}
class RxIdentityTests : public QObject {
    Q_OBJECT
private slots:
    void independentlyReorderedFields_data() {
        QTest::addColumn<bool>("stored"); QTest::addColumn<bool>("incoming");
        QTest::newRow("stored")<<true<<false;
        QTest::newRow("incoming")<<false<<true;
        QTest::newRow("both")<<true<<true;
    }
    void independentlyReorderedFields() {
        QFETCH(bool,stored);QFETCH(bool,incoming);Workspace w;engine(w,1,23);
        const auto initial=w.model()->getPgns("127488").first();
        auto input=pairs(initial.paramDetails);
        if(stored)std::reverse(w.model()->getPgnList()[0].paramDetails.begin(),w.model()->getPgnList()[0].paramDetails.end());
        if(incoming)reversePairs(input);
        input[input.indexOf("EngineSpeed")+1]="1700";
        w.receiver.processNmeaPgnSnt("127488",input,"23");settle();
        QCOMPARE(w.model()->getPgns("127488").size(),1);
        QCOMPARE(w.value(initial.idX,"EngineSpeed").toDouble(),1700.);
        w.receiver.processNmeaPgnSnt("127488",input,"23");
        QCOMPARE(w.model()->getPgns("127488").size(),1);
        QVERIFY(w.controller.layoutWidgets("Generate").isEmpty());
    }
    void enginesAndCanSourcesRemainDistinct() {
        Workspace w;engine(w,0,23);engine(w,1,23);engine(w,0,24);
        for(auto &d:w.model()->getPgnList())std::reverse(d.paramDetails.begin(),d.paramDetails.end());
        engine(w,0,23,1500);engine(w,1,23,2200);engine(w,0,24,3300);
        QCOMPARE(w.model()->getPgns("127488").size(),3);
        for(const auto &d:w.model()->getPgns("127488"))
            QCOMPARE(w.value(d.idX,"EngineSpeed").toDouble(),d.source=="24"?3300.:w.value(d.idX,"EngineInstance")=="0"?1500.:2200.);
    }
    void temperatures_data() {
        QTest::addColumn<int>("pgn");QTest::newRow("130312")<<130312;QTest::newRow("130316")<<130316;
    }
    void temperatures() {
        QFETCH(int,pgn);Workspace w;
        auto receive=[&](int instance,int type,int source) {
            tN2kMsg m;
            if(pgn==130312)SetN2kPGN130312(m,8,instance,static_cast<tN2kTempSource>(type),290,291);
            else SetN2kPGN130316(m,8,instance,static_cast<tN2kTempSource>(type),290,291);
            m.Source=source;w.handler.nmea2000MessageReceived(m);
        };
        receive(0,0,23);receive(1,0,23);receive(0,1,23);receive(0,2,23);receive(0,0,24);
        for(auto &d:w.model()->getPgnList())std::reverse(d.paramDetails.begin(),d.paramDetails.end());
        receive(0,0,23);receive(1,0,23);receive(0,1,23);receive(0,2,23);receive(0,0,24);
        QCOMPARE(w.model()->getPgns(QString::number(pgn)).size(),5);
    }
    void intentionalDuplicateViews() {
        Workspace w;engine(w,0,23);auto rows=w.model()->getPgns("127488");
        w.controller.addRcvdPgn("127488",pairs(rows.first().paramDetails),"23");
        for(auto &d:w.model()->getPgnList())std::reverse(d.paramDetails.begin(),d.paramDetails.end());
        QSignalSpy updated(&w.receiver,&NmeaReceiver::updateRcvdPgnSnt);
        engine(w,0,23,1800);settle();QCOMPARE(updated.size(),2);
        QCOMPARE(w.model()->getPgns("127488").size(),2);
        for(const auto &d:w.model()->getPgns("127488"))QCOMPARE(w.value(d.idX,"EngineSpeed").toDouble(),1800.);
    }
    void otherNamedIdentityRules_data() {
        QTest::addColumn<QString>("pgn");QTest::addColumn<QStringList>("keys");
        const QList<QPair<QString,QString>> cases{{"127489","EngineInstance"},{"127493","EngineInstance"},
            {"127505","Instance,FluidType"},{"127506","DCInstance,DCType"},{"127507","Instance,BatteryInstance"},
            {"127508","BatteryInstance"},{"127245","Instance"},{"130306","WindReference"},
            {"130313","HumidityInstance,HumiditySource"},{"129809","UserID"},{"129810","UserID"},
            {"129039","UserID"},{"128777","WindlassIdentifier"},{"128778","WindlassIdentifier"}};
        for(const auto &entry:cases)QTest::newRow(qPrintable(entry.first))<<entry.first<<entry.second.split(',');
    }
    void otherNamedIdentityRules() {
        QFETCH(QString,pgn);QFETCH(QStringList,keys);Workspace w;
        // Unit-level routing fixtures do not invent widgets for partial payloads.
        disconnect(&w.receiver,&NmeaReceiver::addRcvdPgnSnt,&w.controller,&MainController::addRcvdPgn);
        QList<QList<QString>> rows;
        if(WindlassStatus::supported(pgn.toInt()))rows=WindlassStatus::parameters(pgn.toInt());
        else {rows.append({"Other","77","","int"});for(const auto &key:keys)rows.append({key,key=="UserID"?"234567891":key=="HumiditySource"?"0":"1","","int"});}
        auto input=pairs(rows);std::reverse(rows.begin(),rows.end());
        w.model()->addPgn(7,"unit identity",pgn,false,rows,{},1000);w.model()->setSource(7,"23");
        QSignalSpy updated(&w.receiver,&NmeaReceiver::updateRcvdPgnSnt),added(&w.receiver,&NmeaReceiver::addRcvdPgnSnt);
        w.receiver.processNmeaPgnSnt(pgn,input,"23");QCOMPARE(updated.size(),1);QCOMPARE(added.size(),0);
        reversePairs(input);w.receiver.processNmeaPgnSnt(pgn,input,"23");QCOMPARE(updated.size(),2);QCOMPARE(added.size(),0);
        for(const auto &key:keys) {
            auto different=input;const int value=different.indexOf(key)+1;
            different[value]=QString::number(different[value].toInt()+1);
            w.receiver.processNmeaPgnSnt(pgn,different,"23");
        }
        QCOMPARE(added.size(),keys.size());QCOMPARE(updated.size(),2);
        w.receiver.processNmeaPgnSnt(pgn,input,"24");QCOMPARE(added.size(),keys.size()+1);
    }
    void sentenceTalkers() {
        Workspace w;
        w.receiver.processNmeaPgnSnt("HDT",{"Heading","1.2"},"GP");
        w.receiver.processNmeaPgnSnt("HDT",{"Heading","1.3"},"HC");
        w.receiver.processNmeaPgnSnt("HDT",{"Heading","1.4"},"GP");
        QCOMPARE(w.model()->getPgns("HDT").size(),2);
    }
    void ordinarySnapshotRoundTrip() {
        Workspace w;engine(w,1,23,1750);
        const auto saved=w.model()->getPgns("127488").first();
        QTemporaryDir dir;const auto file=dir.filePath("received.json");
        QVERIFY(w.controller.saveLayoutToJson(file));Workspace r;
        QVERIFY(r.controller.loadLayoutFromJson(file));settle();
        const auto loaded=r.model()->getPgns("127488").first();
        QCOMPARE(loaded.paramDetails,saved.paramDetails);QCOMPARE(loaded.idX,saved.idX);
        QCOMPARE(loaded.enabled,saved.enabled);QVERIFY(loaded.source.isEmpty());
        QVERIFY(r.controller.layoutWidgets("Generate").isEmpty());
        QCOMPARE(r.controller.layoutWidgets("Propulsion").size(),1);
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QApplication app(argc,argv);QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    RxIdentityTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "rx_identity_tests.moc"
