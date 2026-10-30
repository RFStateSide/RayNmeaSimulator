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
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <limits>
#include "maincontroller.h"
#include "nmeareceiver.h"
#include "datetime129033.h"
#include "crosstrack129283.h"
#include "generatordatetime.h"
#include "generatorvesselmovement.h"
#include "N2kMessages.h"
#include "convert.h"
#include "wavesharecodec.h"
#include "systemtime126992.h"
#include "n2khelper.h"
#include "expansioninstruments.h"
#include "recommendedmingnssrmc.h"
#include "globalpositioninggga.h"
#include "datetimezda.h"

namespace {
QString dateText(DateTime129033 &w) { return w.findChild<QLineEdit*>("lineEditDaysSince1970")->text(); }
QString timeText(DateTime129033 &w) { return w.findChild<QLineEdit*>("lineEditSecondsSinceMidnight")->text(); }
QString xteText(CrossTrack129283 &w) { return w.findChild<QLineEdit*>("lineEditXTE")->text(); }
void settle() { for (int i=0;i<5;++i) QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall); }
struct Workspace {
    QMdiArea area; QTableView table; QTreeView tree;
    PgnTableController tables{&table}; PgnTreeController trees{&tree};
    MainController controller{&tables,&trees,&area,nullptr};
    Nmea2000Handler handler; Nmea0183Handler sentences;
    NmeaReceiver receiver{&controller,&tables,&handler,&sentences};
    Workspace() {
        controller.setMode(Viewer);handler.setMode(Viewer);
        controller.setMyVessel(MyVessel{});controller.setUserPrefs(UserUnitPrefs{});
        controller.setUserOptionPrefs(UserOptionPrefs{});
        for (const auto &g : QStringList{"Generate","Navigation","Propulsion","Environment","Tanks","Electrical","Other","Communication"})
            controller.addSubWindowForGroup(g,g);
    }
    PgnTableModel *model() { return tables.getPgnTableModel(); }
    template<class T> T *widget(int source) {
        for(auto *w:area.findChildren<T*>()) {
            for(const auto &p:model()->getPgns(QString::number(w->Pgn())))
                if(p.idX==w->Id() && p.source==QString::number(source)) return w;
        }
        return nullptr;
    }
};
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode mode) override { return QIODevice::open(mode|QIODevice::Unbuffered); }
    void close() override { QIODevice::close(); }
protected:
    qint64 writeData(const char *data,qint64 n) override {
        output.append(data,n);QTimer::singleShot(0,this,[this,n]{emit bytesWritten(n);});return n;
    }
};
}

class LiveInstrumentTests : public QObject {
    Q_OBJECT
private slots:
    void systemTimeFields_data() {
        QTest::addColumn<int>("day");QTest::addColumn<double>("seconds");
        QTest::addColumn<QString>("date");QTest::addColumn<QString>("time");
        QTest::newRow("owner-unavailable")<<65535<<N2kDoubleNA<<"Unavailable"<<"Unavailable";
        QTest::newRow("epoch-zero")<<0<<0.<<"01/01/1970"<<"00:00:00";
        QTest::newRow("owner-valid-date")<<int(QDate(1970,1,1).daysTo(QDate(2026,10,24)))<<27274.1254<<"10/24/2026"<<"07:34:34";
        for(int day:{65533,65534}) QTest::newRow(qPrintable(QString("invalid-date-%1").arg(day)))<<day<<0.<<"Unavailable"<<"00:00:00";
        QTest::newRow("date-only")<<0<<N2kDoubleNA<<"01/01/1970"<<"Unavailable";
        QTest::newRow("time-only")<<65535<<43200.<<"Unavailable"<<"12:00:00";
        for(double seconds:{-1.,86401.,429496.7294,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
            QTest::newRow(qPrintable(QString("invalid-time-%1").arg(seconds)))<<0<<seconds<<"01/01/1970"<<"Unavailable";
        QTest::newRow("last-valid-date-and-time")<<65532<<86399.9999<<QDate(1970,1,1).addDays(65532).toString("MM/dd/yyyy")<<"23:59:59";
        QTest::newRow("leap-second")<<0<<86400.25<<"01/01/1970"<<"23:59:60";
    }
    void systemTimeFields() {
        QFETCH(int,day);QFETCH(double,seconds);QFETCH(QString,date);QFETCH(QString,time);
        SystemTime126992 w(1,"Fixture",nullptr);w.setSystemDate(day);w.setSystemTime(seconds);
        QCOMPARE(w.findChild<QLineEdit*>("lineEditDate")->text(),date);
        QCOMPARE(w.findChild<QLineEdit*>("lineEditTime")->text(),time);
        QCOMPARE(w.SystemDate(),uint16_t(day));QVERIFY(std::isnan(seconds)?std::isnan(w.SystemTime()):w.SystemTime()==seconds);
        if(day==0 && seconds==0) for(int d:{0,1,2})for(int t:{0,1,2,3}) {
            auto prefs=w.UserPrefs();prefs.dateUnitIndex=d;prefs.timeUnitIndex=t;w.setUserPrefs(prefs);
            QCOMPARE(w.findChild<QLineEdit*>("lineEditDate")->text(),QDate(1970,1,1).toString(QStringList{"MM/dd/yyyy","dd/MM/yyyy","yyyy/MM/dd"}[d]));
            QCOMPARE(w.findChild<QLineEdit*>("lineEditTime")->text(),QTime(0,0).toString(QStringList{"HH:mm","hh:mm AP","HH:mm:ss","hh:mm:ss AP"}[t]));
        }
    }
    void sharedDateTimeValidity() {
        UserUnitPrefs prefs;
        for(uint16_t day:{uint16_t(65533),uint16_t(65534),uint16_t(65535)})
            QCOMPARE(N2kHelper::convertN2kDateToUserFormat(day,prefs),QString("Unavailable"));
        for(double seconds:{N2kDoubleNA,-1.,86401.,429496.7293,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
            QCOMPARE(N2kHelper::convertN2kTimeToUserFormat(seconds,prefs),QString("Unavailable"));
            QCOMPARE(N2kHelper::convertN2kTimeToUserFormat(seconds,prefs,60),QString("Unavailable"));
        }
        QCOMPARE(N2kHelper::convertN2kDateToUserFormat(0,prefs),QString("01/01/1970"));
        QCOMPARE(N2kHelper::convertN2kTimeToUserFormat(0,prefs),QString("00:00:00"));
        QCOMPARE(N2kHelper::convertN2kTimeToUserFormat(86400.25,prefs),QString("23:59:60"));
        QCOMPARE(N2kHelper::convertN2kTimeToUserFormat(86400.25,prefs,60),QString("00:59:60"));
        QCOMPARE(N2kHelper::convertN2kDateToUserFormat(0,prefs,-60,0),QString("12/31/1969"));
        for(int16_t offset:{int16_t(32765),int16_t(32766),int16_t(32767)}) {
            QCOMPARE(N2kHelper::convertN2kTimeToUserFormat(0,prefs,offset),QString("Unavailable"));
            QCOMPARE(N2kHelper::convertN2kDateToUserFormat(0,prefs,offset,0),QString("Unavailable"));
        }
        QCOMPARE(N2kHelper::convertN2kDateToUserFormat(65535,prefs,0,0),QString("Unavailable"));
        QCOMPARE(Expansion::displayValue("128275","DaysSince1970","65534",prefs),QString("Unavailable"));
        QCOMPARE(Expansion::displayValue("128275","SecondsSinceMidnight","86401",prefs),QString("Unavailable"));
        GeneratorDateTime clock(1,"Clock",nullptr);clock.setDaysSince1970(65535);clock.setSecondsSinceMidnight(N2kDoubleNA);
        QCOMPARE(clock.findChild<QLineEdit*>("lineEditDaysSince1970")->text(),QString("Unavailable"));
        QCOMPARE(clock.findChild<QLineEdit*>("lineEditSecondsSinceMidnight")->text(),QString("Unavailable"));
        RecommendedMinGnssRMC rmc(1,"RMC",nullptr);rmc.setDaysSince1970(65535);rmc.setGPSTime(N2kDoubleNA);
        QCOMPARE(rmc.findChild<QLineEdit*>("lineEditDate")->text(),QString("Unavailable"));
        QCOMPARE(rmc.findChild<QLineEdit*>("lineEditTime")->text(),QString("Unavailable"));
        GlobalPositioningGGA gga(1,"GGA",nullptr);gga.setGPSTime(N2kDoubleNA);
        QCOMPARE(gga.findChild<QLineEdit*>("lineEditTime")->text(),QString("Unavailable"));
        DateTimeZDA zda(1,"ZDA",nullptr);zda.setGPSTime(N2kDoubleNA);
        QCOMPARE(zda.findChild<QLineEdit*>("lineEditSecondsSinceMidnight")->text(),QString("Unavailable"));
        zda.setGPSYear(2026);zda.setGPSMonth(10);zda.setGPSDay(24);
        QCOMPARE(zda.findChild<QLineEdit*>("lineEditDaysSince1970")->text(),QString("10/24/2026"));
        for(int day:{0,32}){zda.setGPSDay(day);QCOMPARE(zda.findChild<QLineEdit*>("lineEditDaysSince1970")->text(),QString("Unavailable"));}
        zda.setGPSYear(2024);zda.setGPSMonth(2);zda.setGPSDay(29);
        QCOMPARE(zda.findChild<QLineEdit*>("lineEditDaysSince1970")->text(),QString("02/29/2024"));
        zda.setGPSYear(2023);QCOMPARE(zda.findChild<QLineEdit*>("lineEditDaysSince1970")->text(),QString("Unavailable"));
        for(int year:{0,32767}){zda.setGPSYear(year);QCOMPARE(zda.findChild<QLineEdit*>("lineEditDaysSince1970")->text(),QString("Unavailable"));QCOMPARE(zda.GPSYear(),year);}
    }
    void systemTimeSourcesTreeAndLayout() {
        Workspace w;tN2kMsg m;const uint16_t day=QDate(1970,1,1).daysTo(QDate(2026,10,24));
        // Unknown SID/time-source metadata must not invalidate independent valid fields.
        SetN2kPGN126992(m,255,day,27274.1254,tN2kTimeSource(15));m.Source=31;w.handler.nmea2000MessageReceived(m);
        SetN2kPGN126992(m,255,N2kUInt16NA,N2kDoubleNA,tN2kTimeSource(15));m.Source=32;w.handler.nmea2000MessageReceived(m);settle();
        auto *valid=w.widget<SystemTime126992>(31);auto *missing=w.widget<SystemTime126992>(32);QVERIFY(valid && missing);
        QCOMPARE(w.model()->getPgns("126992").size(),2);QCOMPARE(valid->findChild<QLineEdit*>("lineEditDate")->text(),QString("10/24/2026"));
        QCOMPARE(missing->findChild<QLineEdit*>("lineEditDate")->text(),QString("Unavailable"));
        QCOMPARE(missing->findChild<QLineEdit*>("lineEditTime")->text(),QString("Unavailable"));
        w.trees.getTreeModel()->bind(&w.controller,w.model(),&w.area);
        auto shown=[&](PgnWidgetBase *widget,const QString &field) {
            auto *tree=w.trees.getTreeModel();return tree->data(tree->indexFor(widget,field).siblingAtColumn(1)).toString();
        };
        for(const auto &field:QStringList{"SID","SystemDate","SystemTime","TimeSource"})QCOMPARE(shown(missing,field),QString("Unavailable"));
        QCOMPARE(missing->SID(),uchar(255));QCOMPARE(missing->SystemDate(),N2kUInt16NA);QCOMPARE(missing->SystemTime(),N2kDoubleNA);QCOMPARE(int(missing->TimeSource()),15);
        auto rows=w.model()->getPgnParams(missing->Id());QSignalSpy changes(w.model(),&PgnTableModel::parameterChanged);
        auto prefs=missing->UserPrefs();prefs.dateUnitIndex=2;prefs.timeUnitIndex=3;missing->setUserPrefs(prefs);
        QCOMPARE(w.model()->getPgnParams(missing->Id()),rows);QCOMPARE(changes.size(),0);
        m.Source=31;w.handler.nmea2000MessageReceived(m);settle();QCOMPARE(valid->findChild<QLineEdit*>("lineEditDate")->text(),QString("Unavailable"));
        SetN2kPGN126992(m,0,0,0,N2ktimes_GPS);m.Source=31;w.handler.nmea2000MessageReceived(m);settle();
        QCOMPARE(valid->findChild<QLineEdit*>("lineEditDate")->text(),QString("01/01/1970"));QCOMPARE(valid->findChild<QLineEdit*>("lineEditTime")->text(),QString("00:00:00"));
        QCOMPARE(shown(valid,"SystemDate"),QString("01/01/1970"));QCOMPARE(shown(valid,"SystemTime"),QString("00:00:00 UTC"));QCOMPARE(shown(valid,"SID"),QString("0"));
        QTemporaryDir dir(QDir::currentPath()+"/build/m240a-layout-XXXXXX");QVERIFY(dir.isValid());auto file=dir.filePath("system.json");QVERIFY(w.controller.saveLayoutToJson(file));
        Workspace restored;QVERIFY(restored.controller.loadLayoutFromJson(file));settle();int zero=0,na=0;
        for(auto *widget:restored.area.findChildren<SystemTime126992*>()) {
            if(widget->SystemDate()==0){++zero;QCOMPARE(widget->SystemTime(),0.);}
            if(widget->SystemDate()==N2kUInt16NA){++na;QCOMPARE(widget->SystemTime(),N2kDoubleNA);QCOMPARE(widget->findChild<QLineEdit*>("lineEditDate")->text(),QString("Unavailable"));}
        }
        QCOMPARE(zero,1);QCOMPARE(na,1);
    }
    void relatedTreeDates() {
        Workspace w;tN2kMsg m;
        SetN2kPGN129284(m,1,1,N2khr_true,false,false,N2kdct_GreatCircle,43200,int16_t(-1),0,0,1,2,0,0,0);
        m.Source=21;w.handler.nmea2000MessageReceived(m);settle();auto rows=w.model()->getPgns("129284");QCOMPARE(rows.size(),1);
        PgnWidgetBase *widget=nullptr;for(auto *candidate:w.area.findChildren<PgnWidgetBase*>())if(candidate->Id()==rows[0].idX)widget=candidate;QVERIFY(widget);
        auto *tree=w.trees.getTreeModel();tree->bind(&w.controller,w.model(),&w.area);
        auto shown=[&]{return tree->data(tree->indexFor(widget,"ETADate").siblingAtColumn(1)).toString();};
        QCOMPARE(shown(),QString("Unavailable"));
        SetN2kPGN129284(m,1,1,N2khr_true,false,false,N2kdct_GreatCircle,0,int16_t(-32768),0,0,1,2,0,0,0);
        m.Source=21;w.handler.nmea2000MessageReceived(m);settle();
        QCOMPARE(shown(),QDate(1970,1,1).addDays(32768).toString("MM/dd/yyyy"));
        // The signed application holder remains canonical; only presentation reinterprets the wire bits.
        bool found=false;for(const auto &row:w.model()->getPgnParams(widget->Id()))if(row[0]=="ETADate"){QCOMPARE(row[1],QString("-32768"));found=true;}QVERIFY(found);
    }
    void dateFields_data() {
        QTest::addColumn<int>("days");QTest::addColumn<double>("seconds");QTest::addColumn<int>("offset");
        QTest::addColumn<QString>("date");QTest::addColumn<QString>("time");
        QTest::newRow("epoch-midnight-zero")<<0<<0.<<0<<"01/01/1970"<<"00:00:00";
        QTest::newRow("positive-rollover")<<0<<84600.1254<<120<<"01/02/1970"<<"01:30:00";
        QTest::newRow("negative-rollover")<<1<<1800.<<-60<<"01/01/1970"<<"23:30:00";
        QTest::newRow("before-epoch-local-date")<<0<<0.<<-60<<"12/31/1969"<<"23:00:00";
        QTest::newRow("last-valid-date")<<65532<<0.<<0<<QDate(1970,1,1).addDays(65532).toString("MM/dd/yyyy")<<"00:00:00";
        QTest::newRow("live-valid-offset-minus-one")<<20000<<43200.1234<<-1<<"10/04/2024"<<"11:59:00";
        QTest::newRow("live-all-unavailable")<<65535<<N2kDoubleNA<<32767<<"Unavailable"<<"Unavailable";
        QTest::newRow("date-only-UTC")<<1<<N2kDoubleNA<<120<<"01/02/1970"<<"Unavailable";
        QTest::newRow("time-only")<<65535<<43200.<<60<<"Unavailable"<<"13:00:00";
        for(int offset:{32765,32766,32767})
            QTest::newRow(qPrintable(QString("offset-invalid-%1").arg(offset)))<<1<<43200.<<offset<<"01/02/1970"<<"12:00:00";
        for(int day:{65533,65534})
            QTest::newRow(qPrintable(QString("date-invalid-%1").arg(day)))<<day<<43200.<<0<<"Unavailable"<<"12:00:00";
        for(double value:{-1.,86401.,429496.7294,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
            QTest::newRow(qPrintable(QString("time-invalid-%1").arg(value)))<<1<<value<<0<<"01/02/1970"<<"Unavailable";
        QTest::newRow("last-ordinary-second")<<1<<86399.9999<<0<<"01/02/1970"<<"23:59:59";
        QTest::newRow("leap-second")<<1<<86400.25<<0<<"01/02/1970"<<"23:59:60";
        QTest::newRow("leap-second-local-rollover")<<1<<86400.25<<60<<"01/03/1970"<<"00:59:60";
    }
    void dateFields() {
        QFETCH(int,days);QFETCH(double,seconds);QFETCH(int,offset);QFETCH(QString,date);QFETCH(QString,time);
        DateTime129033 w(1,"Fixture");w.setDaysSince1970(days);w.setSecondsSinceMidnight(seconds);w.setLocalOffset(offset);
        QCOMPARE(dateText(w),date);QCOMPARE(timeText(w),time);
        QCOMPARE(w.DaysSince1970(),uint16_t(days));QCOMPARE(w.LocalOffset(),int16_t(offset));
        QVERIFY(std::isnan(seconds)?std::isnan(w.SecondsSinceMidnight()):w.SecondsSinceMidnight()==seconds);
        if(offset>=32765) QVERIFY(w.findChild<QLabel*>("labelUnitsSecondsSinceMidnight")->text().contains("UTC"));
    }
    void xteFields_data() {
        QTest::addColumn<double>("value");QTest::addColumn<bool>("valid");
        for(double v:{-123.45,0.,123.45,-21474836.48,21474836.44}) QTest::newRow(qPrintable(QString::number(v,'g',17)))<<v<<true;
        for(double v:{N2kDoubleNA,21474836.45,21474836.46,-21474836.49,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
            QTest::newRow(qPrintable("invalid-"+QString::number(v,'g',17)))<<v<<false;
    }
    void xteFields() {
        QFETCH(double,value);QFETCH(bool,valid);CrossTrack129283 w(1,"Fixture",nullptr);w.setXTE(value);
        for(int unit:{0,1,2}) {
            auto prefs=w.UserPrefs();prefs.distanceUnitIndex=unit;w.setUserPrefs(prefs);
            QCOMPARE(xteText(w),valid?QString::number(unit==0?value:Convert::MetersToFeet(value),'f',1):QString("Unavailable"));
            QVERIFY(std::isnan(value)?std::isnan(w.XTE()):w.XTE()==value);
        }
        if(valid && value==-123.45) {
            auto *edit=w.findChild<QLineEdit*>("lineEditXTE");QString draft="-12.3";int pos=draft.size();
            QCOMPARE(edit->validator()->validate(draft,pos),QValidator::Acceptable);
            edit->setText(draft);QVERIFY(QMetaObject::invokeMethod(&w,"on_lineEditXTE_textEdited",Q_ARG(QString,draft)));
            QVERIFY(qAbs(w.XTE()-Convert::FeetToMeters(-12.3))<1e-9);
        }
    }
    void receiveSourcesAndLayout() {
        Workspace w;tN2kMsg m;
        SetN2kPGN129033(m,20000,43200.1234,-1);m.Source=21;w.handler.nmea2000MessageReceived(m);
        SetN2kPGN129033(m,N2kUInt16NA,N2kDoubleNA,N2kInt16NA);m.Source=22;w.handler.nmea2000MessageReceived(m);
        SetN2kPGN129283(m,1,N2kxtem_Autonomous,false,N2kDoubleNA);m.Source=21;w.handler.nmea2000MessageReceived(m);settle();
        auto *valid=w.widget<DateTime129033>(21);auto *unavailable=w.widget<DateTime129033>(22);auto *xte=w.widget<CrossTrack129283>(21);
        QVERIFY(valid && unavailable && xte);QCOMPARE(w.model()->getPgns("129033").size(),2);
        QCOMPARE(dateText(*valid),QString("10/04/2024"));QCOMPARE(timeText(*valid),QString("11:59:00"));
        QCOMPARE(dateText(*unavailable),QString("Unavailable"));QCOMPARE(timeText(*unavailable),QString("Unavailable"));QCOMPARE(xteText(*xte),QString("Unavailable"));
        w.trees.getTreeModel()->bind(&w.controller,w.model(),&w.area);
        auto shown=[&](PgnWidgetBase *widget,const QString &field) {
            auto *tree=w.trees.getTreeModel();return tree->data(tree->indexFor(widget,field).siblingAtColumn(1)).toString();
        };
        QCOMPARE(shown(unavailable,"DaysSince1970"),QString("Unavailable"));
        QCOMPARE(shown(unavailable,"LocalOffset"),QString("Unavailable"));
        QCOMPARE(shown(xte,"XTE"),QString("Unavailable"));
        SetN2kPGN129033(m,N2kUInt16NA,N2kDoubleNA,N2kInt16NA);m.Source=21;w.handler.nmea2000MessageReceived(m);settle();
        QCOMPARE(dateText(*valid),QString("Unavailable"));QCOMPARE(timeText(*valid),QString("Unavailable"));
        SetN2kPGN129033(m,20000,43200.1234,-1);m.Source=21;w.handler.nmea2000MessageReceived(m);settle();
        QCOMPARE(dateText(*valid),QString("10/04/2024"));QCOMPARE(timeText(*valid),QString("11:59:00"));
        QCOMPARE(w.model()->getPgns("129033").size(),2);QCOMPARE(dateText(*unavailable),QString("Unavailable"));
        QSignalSpy writeback(w.model(),&PgnTableModel::parameterChanged);const auto original=w.model()->getPgnParams(xte->Id());
        auto prefs=xte->UserPrefs();prefs.distanceUnitIndex=0;xte->setUserPrefs(prefs);QCOMPARE(w.model()->getPgnParams(xte->Id()),original);QCOMPARE(writeback.size(),0);
        const auto dateOriginal=w.model()->getPgnParams(unavailable->Id());
        auto datePrefs=unavailable->UserPrefs();datePrefs.dateUnitIndex=2;unavailable->setUserPrefs(datePrefs);
        QCOMPARE(w.model()->getPgnParams(unavailable->Id()),dateOriginal);QCOMPARE(writeback.size(),0);
        SetN2kPGN129283(m,2,N2kxtem_Autonomous,false,21474836.46);m.Source=21;w.handler.nmea2000MessageReceived(m);settle();
        QCOMPARE(xteText(*xte),QString("Unavailable"));QCOMPARE(shown(xte,"XTE"),QString("Unavailable"));
        for(double value:{-12.34,0.,12.34,N2kDoubleNA}) {
            SetN2kPGN129283(m,2,N2kxtem_Autonomous,false,value);m.Source=21;w.handler.nmea2000MessageReceived(m);settle();QCOMPARE(xte->XTE(),value);
            QCOMPARE(xteText(*xte),value==N2kDoubleNA?QString("Unavailable"):QString::number(value,'f',1));
        }
        if(!qEnvironmentVariable("RAY_M240_IMAGES").isEmpty()) {
            const auto dir=qEnvironmentVariable("RAY_M240_IMAGES");QVERIFY(QDir().mkpath(dir));
            valid->resize(500,120);unavailable->resize(500,120);xte->resize(500,100);
            QVERIFY(valid->grab().save(dir+"/date-valid.png"));QVERIFY(unavailable->grab().save(dir+"/date-unavailable.png"));
            QVERIFY(xte->grab().save(dir+"/xte-unavailable.png"));
        }
        QTemporaryDir dir(QDir::currentPath()+"/build/m240-layout-XXXXXX");QVERIFY(dir.isValid());
        const auto file=dir.filePath("layout.json");QVERIFY(w.controller.saveLayoutToJson(file));
        Workspace restored;QVERIFY(restored.controller.loadLayoutFromJson(file));settle();
        auto *rx=restored.area.findChild<CrossTrack129283*>();QVERIFY(rx);QCOMPARE(rx->XTE(),N2kDoubleNA);QCOMPARE(xteText(*rx),QString("Unavailable"));
    }
    void truncatedPacketsDoNotOverwrite() {
        Workspace w;
        for(int pgn:{126992,129033,129283}) {
            tN2kMsg m;
            if(pgn==126992)SetN2kPGN126992(m,1,20000,43200,N2ktimes_GPS);
            else if(pgn==129033)SetN2kPGN129033(m,20000,43200,-1);
            else SetN2kPGN129283(m,1,N2kxtem_Autonomous,false,-12.34);
            m.Source=21;w.handler.nmea2000MessageReceived(m);settle();
            const auto rows=w.model()->getPgns(QString::number(pgn));QCOMPARE(rows.size(),1);
            const auto saved=w.model()->getPgnParams(rows[0].idX);
            QSignalSpy received(&w.handler,&Nmea2000Handler::RcvdPgn);
            for(int length=0;length<(pgn==129283?6:8);++length) {
                m.DataLen=length;w.handler.nmea2000MessageReceived(m);settle();
                QCOMPARE(received.size(),0);QCOMPARE(w.model()->getPgnParams(rows[0].idX),saved);
            }
        }
    }
    void generatorsAndFormats() {
        Workspace w;w.controller.setMode(Sender);
        QTimer dismiss;connect(&dismiss,&QTimer::timeout,[]{for(auto *widget:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox*>(widget))box->button(QMessageBox::No)->click();});dismiss.start(0);
        w.controller.addGeneratorWidget("Date Time (PGN -129033)");w.controller.addGeneratorWidget("Vessel Movement (PGN -129025)");
        // Use the real selector: the slot deliberately requires a combo-box sender.
        for(const auto &label:QStringList{"System Date Time (PGN 126992)","Local Date Time (PGN 129033)","Cross Track Error (PGN 129283)"}) {
            bool selected=false;
            for(auto *combo:w.area.findChildren<QComboBox*>())if(combo->findText(label)>=0) {
                combo->setCurrentIndex(combo->findText(label));selected=true;break;
            }
            QVERIFY(selected);
        }
        auto *clock=w.area.findChild<GeneratorDateTime*>();auto *movement=w.area.findChild<GeneratorVesselMovement*>();
        auto *date=w.area.findChild<DateTime129033*>();auto *system=w.area.findChild<SystemTime126992*>();auto *xte=w.area.findChild<CrossTrack129283*>();QVERIFY(clock && movement && date && system && xte);
        clock->setDaysSince1970(20000);clock->setSecondsSinceMidnight(43200.1254);clock->setLocalOffset(60);movement->setXTE(-12.34);settle();
        QCOMPARE(date->DaysSince1970(),uint16_t(20000));QCOMPARE(date->SecondsSinceMidnight(),43200.1254);QCOMPARE(date->LocalOffset(),int16_t(60));QCOMPARE(xte->XTE(),-12.34);
        QCOMPARE(system->SystemDate(),uint16_t(20000));QCOMPARE(system->SystemTime(),43200.1254);QCOMPARE(system->findChild<QLineEdit*>("lineEditTime")->text(),QString("12:00:00"));
        clock->setDaysSince1970(N2kUInt16NA);clock->setSecondsSinceMidnight(N2kDoubleNA);clock->setLocalOffset(N2kInt16NA);movement->setXTE(N2kDoubleNA);settle();
        QCOMPARE(dateText(*date),QString("Unavailable"));QCOMPARE(timeText(*date),QString("Unavailable"));QCOMPARE(xteText(*xte),QString("Unavailable"));
        QCOMPARE(system->findChild<QLineEdit*>("lineEditDate")->text(),QString("Unavailable"));QCOMPARE(system->findChild<QLineEdit*>("lineEditTime")->text(),QString("Unavailable"));
        clock->setDaysSince1970(20000);clock->setSecondsSinceMidnight(43200.1254);clock->setLocalOffset(60);movement->setXTE(-12.34);settle();
        for(int d:{0,1,2})for(int t:{0,1,2,3}) {
            auto prefs=date->UserPrefs();prefs.dateUnitIndex=d;prefs.timeUnitIndex=t;date->setUserPrefs(prefs);
            QCOMPARE(dateText(*date),QDate(2024,10,4).toString(QStringList{"MM/dd/yyyy","dd/MM/yyyy","yyyy/MM/dd"}[d]));
            QCOMPARE(timeText(*date),QTime(13,0).toString(QStringList{"HH:mm","hh:mm AP","HH:mm:ss","hh:mm:ss AP"}[t]));
            QCOMPARE(date->SecondsSinceMidnight(),43200.1254);
        }
    }
    void capturedTrafficReplay() {
        const auto path=qEnvironmentVariable("RAY_M240_CAPTURE");
        if(path.isEmpty())QSKIP("Optional private CAN capture; synthetic live-pattern fixtures run independently.");
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));Workspace w;int count=0;
        QMap<int,tN2kMsg> dates;QMap<int,tN2kMsg> errors;QMap<int,tN2kMsg> systems;
        for(const auto &line:file.readAll().split('\n')) {
            const auto match=QRegularExpression("([0-9A-Fa-f]{8})#([0-9A-Fa-f]{16})$").match(QString::fromLatin1(line).trimmed());
            if(!match.hasMatch())continue;
            const auto id=match.captured(1).toUInt(nullptr,16);const auto pgn=(id>>8)&0x3ffff;
            if(pgn!=126992 && pgn!=129033 && pgn!=129283)continue;
            const auto bytes=QByteArray::fromHex(match.captured(2).toLatin1());
            tN2kMsg m;m.SetPGN(pgn);m.Source=id&255;for(auto byte:bytes)m.AddByte(quint8(byte));
            w.handler.nmea2000MessageReceived(m);settle();++count;
            (pgn==126992?systems:pgn==129033?dates:errors)[m.Source]=m;
        }
        QVERIFY(count>0 && !dates.isEmpty() && !errors.isEmpty());
        QCOMPARE(w.model()->getPgns("129033").size(),dates.size());
        QCOMPARE(w.model()->getPgns("126992").size(),systems.size());
        for(auto it=systems.cbegin();it!=systems.cend();++it) {
            auto *widget=w.widget<SystemTime126992>(it.key());QVERIFY(widget);const auto &m=it.value();
            const auto day=quint16(m.Data[2])|(quint16(m.Data[3])<<8);
            const auto raw=quint32(m.Data[4])|(quint32(m.Data[5])<<8)|(quint32(m.Data[6])<<16)|(quint32(m.Data[7])<<24);
            QCOMPARE(widget->SystemDate(),uint16_t(day));QCOMPARE(widget->SystemTime(),raw==0xffffffff?N2kDoubleNA:double(raw)*0.0001);
            QCOMPARE(widget->findChild<QLineEdit*>("lineEditDate")->text(),day>=65533?QString("Unavailable"):QDate(1970,1,1).addDays(day).toString("MM/dd/yyyy"));
            if(raw==0xffffffff)QCOMPARE(widget->findChild<QLineEdit*>("lineEditTime")->text(),QString("Unavailable"));
        }
        for(auto it=dates.cbegin();it!=dates.cend();++it) {
            auto *widget=w.widget<DateTime129033>(it.key());QVERIFY(widget);
            const auto &m=it.value();const auto day=quint16(m.Data[0])|(quint16(m.Data[1])<<8);
            const auto raw=quint32(m.Data[2])|(quint32(m.Data[3])<<8)|(quint32(m.Data[4])<<16)|(quint32(m.Data[5])<<24);
            QCOMPARE(widget->DaysSince1970(),uint16_t(day));
            QCOMPARE(widget->SecondsSinceMidnight(),raw==0xffffffff?N2kDoubleNA:double(raw)*0.0001);
            if(day>=65533)QCOMPARE(dateText(*widget),QString("Unavailable"));else QVERIFY(!dateText(*widget).isEmpty());
            if(raw==0xffffffff)QCOMPARE(timeText(*widget),QString("Unavailable"));
        }
        for(auto it=errors.cbegin();it!=errors.cend();++it) {
            auto *widget=w.widget<CrossTrack129283>(it.key());QVERIFY(widget);const auto &m=it.value();
            const auto raw=quint32(m.Data[2])|(quint32(m.Data[3])<<8)|(quint32(m.Data[4])<<16)|(quint32(m.Data[5])<<24);
            if(raw==0x7fffffff){QCOMPARE(widget->XTE(),N2kDoubleNA);QCOMPARE(xteText(*widget),QString("Unavailable"));}
        }
        qInfo()<<"Replayed target frames:"<<count<<"System Time sources:"<<systems.size()<<"Date/Time sources:"<<dates.size()<<"XTE sources:"<<errors.size();
    }
    void transmitRoundTrip() {
        MemoryPort *port=nullptr;Nmea2000Handler h(nullptr,nullptr,nullptr,[&](QObject *parent){port=new MemoryPort;port->setParent(parent);return port;});
        h.SetInterfaceType("Waveshare USB-CAN-A");h.setMode(Sender);h.ConfigureWaveshare("memory",{123456,44});h.StartWaveshareSending();
        auto *session=h.findChild<WaveshareSession*>();QVERIFY(session);QTRY_VERIFY(session->serviceReady());
        for(double xte:{-12.34,0.,12.34,N2kDoubleNA}) {
            QVERIFY(h.sendPgn(129283,{"SID:1","XTEMode:0","NavigationTerminated:0","XTE:"+QString::number(xte,'g',17)}));
            QTest::qWait(20);
        }
        QVERIFY(h.sendPgn(129033,{"DaysSince1970:20000","SecondsSinceMidnight:43200.1254","LocalOffset:-60"}));
        QVERIFY(h.sendPgn(129033,{"DaysSince1970:65535","SecondsSinceMidnight:-1000000000","LocalOffset:32767"}));QTest::qWait(40);
        QVERIFY(h.sendPgn(126992,{"SID:0","SystemDate:0","SystemTime:0","TimeSource:0"}));
        QVERIFY(h.sendPgn(126992,{"SID:255","SystemDate:65535","SystemTime:-1000000000","TimeSource:15"}));QTest::qWait(60);
        WaveshareCodec::Parser parser;QList<double> errors;int dates=0,systems=0;
        for(const auto &frame:parser.feed(port->output.mid(20))) {
            const auto pgn=(frame.id>>8)&0x3ffff;if(pgn!=126992 && pgn!=129033 && pgn!=129283)continue;
            tN2kMsg m;m.SetPGN(pgn);for(int i=0;i<frame.length;++i)m.AddByte(frame.data[i]);
            if(pgn==126992){unsigned char sid;uint16_t day;double seconds;tN2kTimeSource source;QVERIFY(ParseN2kPGN126992(m,sid,day,seconds,source));
                if(systems++==0){QCOMPARE(sid,uchar(0));QCOMPARE(day,uint16_t(0));QCOMPARE(seconds,0.);QCOMPARE(int(source),0);}
                else {QCOMPARE(sid,uchar(255));QCOMPARE(day,N2kUInt16NA);QCOMPARE(seconds,N2kDoubleNA);QCOMPARE(int(source),15);}}
            else if(pgn==129283){unsigned char sid;tN2kXTEMode mode;bool terminated;double x;QVERIFY(ParseN2kPGN129283(m,sid,mode,terminated,x));errors<<x;}
            else {uint16_t day;double seconds;int16_t offset;QVERIFY(ParseN2kPGN129033(m,day,seconds,offset));
                if(dates++==0){QCOMPARE(day,uint16_t(20000));QVERIFY(qAbs(seconds-43200.1254)<0.0001);QCOMPARE(offset,int16_t(-60));}
                else {QCOMPARE(day,N2kUInt16NA);QCOMPARE(seconds,N2kDoubleNA);QCOMPARE(offset,N2kInt16NA);}}
        }
        QCOMPARE(errors,QList<double>({-12.34,0.,12.34,N2kDoubleNA}));QCOMPARE(dates,2);QCOMPARE(systems,2);h.StopWaveshare();
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile(QDir::currentPath()+"/build/m240-profile-XXXXXX");qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());qputenv("RAYNMEA_TEST_CREDENTIALS","1");
    QApplication app(argc,argv);app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    LiveInstrumentTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "live_instrument_tests.moc"
