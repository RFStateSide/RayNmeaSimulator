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
void settle() { for (int i=0; i<4; ++i) QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall); }
tN2kMsg position(int pgn, double latitude, double longitude) {
    tN2kMsg message;
    if (pgn==129025) SetN2kPGN129025(message,latitude,longitude);
    else if (pgn==129029)
        SetN2kPGN129029(message,1,20000,43200,latitude,longitude,12,N2kGNSSt_GPS,N2kGNSSm_GNSSfix,8,1,1,0);
    else SetN2kPGN129284(message,1,10,N2khr_true,false,false,N2kdct_GreatCircle,
                         43200,20000,1,1,1,2,latitude,longitude,2);
    message.Source=23;
    return message;
}
// Independent reference is the pinned decoder's binary result, before any QString
// serialization. This verifies precision retention, not a claim of sensor accuracy.
QPair<double,double> coordinates(const tN2kMsg &message) {
    double lat=0,lon=0,a,b,c,d,e,f,g; unsigned char sid,ns,nr; uint16_t days,station;
    tN2kGNSStype type,ref; tN2kGNSSmethod method;
    if (message.PGN==129025) ParseN2kPGN129025(message,lat,lon);
    else if (message.PGN==129029)
        ParseN2kPGN129029(message,sid,days,a,lat,lon,b,type,method,ns,c,d,e,nr,ref,station,f);
    else {
        tN2kHeadingReference heading; tN2kDistanceCalculationType calculation;
        bool perpendicular,arrival; int16_t date; uint32_t origin,destination;
        ParseN2kPGN129284(message,sid,a,heading,perpendicular,arrival,calculation,b,date,c,d,
                          origin,destination,lat,lon,g);
    }
    return {lat,lon};
}
}

class RxCorrectnessTests : public QObject {
    Q_OBJECT
private slots:
    void canonicalTimePrecision_data() {
        QTest::addColumn<int>("pgn");QTest::addColumn<double>("seconds");
        for(int pgn:{126992,129029,129033}) {
            QTest::newRow(qPrintable(QString::number(pgn)+"-fraction"))<<pgn<<43200.1234;
            QTest::newRow(qPrintable(QString::number(pgn)+"-zero"))<<pgn<<0.;
            QTest::newRow(qPrintable(QString::number(pgn)+"-NA"))<<pgn<<N2kDoubleNA;
        }
    }
    void canonicalTimePrecision() {
        QFETCH(int,pgn);QFETCH(double,seconds);Workspace w;tN2kMsg m;
        if(pgn==126992)SetN2kSystemTime(m,1,20000,seconds,N2ktimes_GPS);
        else if(pgn==129033)SetN2kLocalOffset(m,20000,seconds,60);
        else SetN2kGNSS(m,1,20000,seconds,43.,-70.,123456.123456,N2kGNSSt_GPS,N2kGNSSm_GNSSfix,8,1.23,2.34,10.25);
        int offset=pgn==126992?4:pgn==129033?2:3;const double expected=m.Get4ByteUDouble(0.0001,offset);
        m.Source=23;w.handler.nmea2000MessageReceived(m);settle();const auto rows=w.model()->getPgns(QString::number(pgn));QCOMPARE(rows.size(),1);
        const auto field=pgn==126992?"SystemTime":"SecondsSinceMidnight";QCOMPARE(w.value(rows[0].idX,field).toDouble(),expected);
        if(pgn==129029){int index=23;const double altitude=m.Get8ByteDouble(1e-6,index);QCOMPARE(w.value(rows[0].idX,"Altitude").toDouble(),altitude);}
    }
    void coordinatePipeline_data() {
        QTest::addColumn<int>("pgn"); QTest::addColumn<double>("latitude"); QTest::addColumn<double>("longitude");
        for (int pgn : {129025,129029,129284}) {
            QTest::newRow(qPrintable(QString::number(pgn)+"-fraction")) << pgn << 43.1234567 << -70.7654321;
            QTest::newRow(qPrintable(QString::number(pgn)+"-gnss-fraction")) << pgn << -12.3456789123456 << 123.456789123456;
            QTest::newRow(qPrintable(QString::number(pgn)+"-boundary")) << pgn << -90.0 << 180.0;
            QTest::newRow(qPrintable(QString::number(pgn)+"-zero")) << pgn << 0.0 << 0.0;
            QTest::newRow(qPrintable(QString::number(pgn)+"-NA")) << pgn << N2kDoubleNA << N2kDoubleNA;
        }
    }
    void coordinatePipeline() {
        QFETCH(int,pgn); QFETCH(double,latitude); QFETCH(double,longitude);
        Workspace w;
        const auto message=position(pgn,latitude,longitude);
        const auto expected=coordinates(message);
        if (latitude!=N2kDoubleNA) {
            const double tolerance=pgn==129029 ? 1e-12 : 1.1e-7;
            QVERIFY(qAbs(expected.first-latitude)<=tolerance);
            QVERIFY(qAbs(expected.second-longitude)<=tolerance);
        } else { QCOMPARE(expected.first,N2kDoubleNA); QCOMPARE(expected.second,N2kDoubleNA); }
        const auto originalLocale=QLocale(); QLocale::setDefault(QLocale(QLocale::German));
        w.handler.nmea2000MessageReceived(message); settle(); QLocale::setDefault(originalLocale);
        const auto rows=w.model()->getPgns(QString::number(pgn)); QCOMPARE(rows.size(),1);
        const int id=rows[0].idX;
        const QString lat=pgn==129284 ? "DestinationLatitude" : "Latitude";
        const QString lon=pgn==129284 ? "DestinationLongitude" : "Longitude";
        QCOMPARE(w.value(id,lat).toDouble(),expected.first); QCOMPARE(w.value(id,lon).toDouble(),expected.second);
        QVERIFY(!w.value(id,lat).contains(','));
        QWidget *widget=nullptr;
        for (auto *candidate : w.controller.layoutWidgets("Navigation"))
            if (candidate->property("Id").toInt()==id) widget=candidate;
        QVERIFY(widget);
        QCOMPARE(widget->property(lat.toLatin1().constData()).toDouble(),expected.first);
        QCOMPARE(widget->property(lon.toLatin1().constData()).toDouble(),expected.second);
        QTemporaryDir dir; const auto file=dir.filePath("coordinates.json");
        QVERIFY(w.controller.saveLayoutToJson(file));
        Workspace restored; QVERIFY(restored.controller.loadLayoutFromJson(file)); settle();
        QCOMPARE(restored.value(id,lat).toDouble(),expected.first);
        QCOMPARE(restored.value(id,lon).toDouble(),expected.second);
        // Runtime source persistence is intentionally outside this Layout contract.
    }
    void legacyLengths_data() {
        QTest::addColumn<int>("pgn"); QTest::addColumn<int>("minimum");
        const QList<QPair<int,int>> lengths{{126992,8},{127245,6},{127250,8},{127251,5},
            {127252,6},{127257,7},{127258,6},{127488,6},{127489,26},{127493,7},
            {127505,7},{127506,11},{127507,6},{127508,8},{128259,6},{128267,8},
            {129025,8},{129026,6},{129283,6},{129284,34},{129029,43},{129033,8},
            {130310,7},{130311,8},{130312,7},{130316,8},{130576,2}};
        for (const auto &entry : lengths) QTest::newRow(qPrintable(QString::number(entry.first)))<<entry.first<<entry.second;
    }
    void legacyLengths() {
        QFETCH(int,pgn); QFETCH(int,minimum);
        Nmea2000Handler handler; handler.setMode(Viewer); QSignalSpy received(&handler,&Nmea2000Handler::RcvdPgn);
        tN2kMsg message; message.SetPGN(pgn); message.Source=23;
        for (int i=0;i<minimum+3;++i) message.AddByte(0xff);
        if (pgn==129029) message.Data[42]=0;
        for (int n=0;n<minimum;++n) { message.DataLen=n; handler.nmea2000MessageReceived(message); QCOMPARE(received.size(),0); }
        message.DataLen=minimum; handler.nmea2000MessageReceived(message); QCOMPARE(received.size(),1);
        // Missing trailing reserved padding is tolerated; valid extensions preserved.
        message.DataLen=minimum+3; handler.nmea2000MessageReceived(message); QCOMPARE(received.size(),2);
        message.DataLen=-1; handler.nmea2000MessageReceived(message); QCOMPARE(received.size(),2);
        message.DataLen=tN2kMsg::MaxDataLen+1; handler.nmea2000MessageReceived(message); QCOMPARE(received.size(),2);
    }
    void rejectedEnginePreservesSample() {
        Workspace w; tN2kMsg message; SetN2kEngineParamRapid(message,0,1200,1000,0); message.Source=23;
        w.handler.nmea2000MessageReceived(message); settle();
        const int id=w.model()->getPgns("127488").first().idX;
        const auto stamp=w.model()->parameterUpdatedAt(id,"EngineSpeed"); QVERIFY(stamp.isValid());
        QTest::qWait(2); const auto valid=message;
        for (int n=0;n<6;++n) {message.DataLen=n; w.handler.nmea2000MessageReceived(message);settle();
            QCOMPARE(w.value(id,"EngineSpeed").toDouble(),1200.);QCOMPARE(w.model()->parameterUpdatedAt(id,"EngineSpeed"),stamp);}
        QCOMPARE(w.model()->getPgns("127488").size(),1);
        message=valid; SetN2kEngineParamRapid(message,0,1350,1000,0);message.Source=23;
        w.handler.nmea2000MessageReceived(message);settle();QCOMPARE(w.value(id,"EngineSpeed").toDouble(),1350.);
        QVERIFY(w.model()->parameterUpdatedAt(id,"EngineSpeed")>stamp);
        SetN2kEngineParamRapid(message,0,N2kDoubleNA,N2kDoubleNA,N2kInt8NA);message.Source=23;
        w.handler.nmea2000MessageReceived(message);settle();QCOMPARE(w.value(id,"EngineSpeed").toDouble(),N2kDoubleNA);
    }
    void gnssReferenceLists() {
        Nmea2000Handler handler;handler.setMode(Viewer);QSignalSpy received(&handler,&Nmea2000Handler::RcvdPgn);
        auto message=position(129029,43.1234567,-70.7654321);
        QCOMPARE(message.DataLen,43);
        message.Data[42]=1;handler.nmea2000MessageReceived(message);QCOMPARE(received.size(),0);
        for(int i=0;i<4;++i)message.AddByte(0xff);
        handler.nmea2000MessageReceived(message);QCOMPARE(received.size(),1);
        message.Data[42]=2;handler.nmea2000MessageReceived(message);QCOMPARE(received.size(),1);
        for(int i=0;i<4;++i)message.AddByte(0xff);
        handler.nmea2000MessageReceived(message);QCOMPARE(received.size(),2);
        message.DataLen=43;message.Data[42]=255;handler.nmea2000MessageReceived(message);QCOMPARE(received.size(),3);
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile; qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QApplication app(argc,argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    RxCorrectnessTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "rx_correctness_tests.moc"
