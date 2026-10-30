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
#include <QDomDocument>
#include "datastructs.h"
#include "routeimport.h"
#define private public
#include "autopilotsimulator.h"
#undef private

namespace {
QStringList warnings, debugMessages;
void messages(QtMsgType type, const QMessageLogContext &, const QString &message) {
    if (type == QtWarningMsg) warnings.append(message);
    if (type == QtDebugMsg) debugMessages.append(message);
}
const QByteArray gpx = "<gpx><rte><name>Harbor</name><rtept lat='42' lon='-70'><name>A &amp; B</name></rtept><rtept lat='42.01' lon='-70'/></rte></gpx>";
const QByteArray kml = "<kml xmlns='http://www.opengis.net/kml/2.2'><Document><Folder><Placemark><name>Harbor</name><LineString><coordinates> -70,42,0\n -70,42.01 </coordinates></LineString></Placemark></Folder></Document></kml>";
QString write(QTemporaryDir &dir, const QString &name, const QByteArray &bytes) {
    const auto path = dir.filePath(name); QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) qFatal("Fixture write failed");
    return path;
}
}
class RouteImportTests : public QObject {
    Q_OBJECT
private slots:
    void extensionGeometryAndFilters() {
        QTemporaryDir dir;
        for (const auto &format : {QString("gpx"), QString("kml")}) {
            for (int mask = 0; mask < 8; ++mask) {
                auto ext = format;
                for (int i=0;i<3;++i) if (mask & (1<<i)) ext[i]=ext[i].toUpper();
                const auto r = RouteImport::read(write(dir,"route."+ext,format=="gpx"?gpx:kml));
                QVERIFY2(bool(r),qPrintable(r.error)); QCOMPARE(r.waypoints.size(),2);
                QCOMPARE(r.waypoints[0].coordinate,QGeoCoordinate(42,-70));
                QCOMPARE(r.waypoints[1].coordinate,QGeoCoordinate(42.01,-70));
                QVERIFY(RouteImport::fileFilter().contains("*."+ext));
            }
        }
    }
    void structures_data() {
        QTest::addColumn<QByteArray>("xml"); QTest::addColumn<QString>("ext"); QTest::addColumn<bool>("valid");
        QTest::newRow("gpx-track") << QByteArray("<gpx xmlns='http://www.topografix.com/GPX/1/0'><wpt lat='1' lon='1'/><trk><trkseg><trkpt lat='42' lon='-70'/><trkpt lat='43' lon='-71'/></trkseg></trk></gpx>") << "gpx" << true;
        QTest::newRow("prefix") << QByteArray("<g:gpx xmlns:g='http://www.topografix.com/GPX/1/1'><g:rte><g:rtept lat='0' lon='0'/><g:rtept lat='90' lon='180'/></g:rte></g:gpx>") << "GPX" << true;
        QTest::newRow("route-precedence") << QByteArray("<gpx><rte><rtept lat='1' lon='1'/><rtept lat='2' lon='2'/></rte><trk><trkseg><trkpt lat='3' lon='3'/><trkpt lat='4' lon='4'/></trkseg></trk></gpx>") << "gpx" << true;
        QTest::newRow("standalone") << QByteArray("<gpx><wpt lat='0' lon='0'/><wpt lat='1' lon='1'/></gpx>") << "gpx" << false;
        QTest::newRow("multi-route") << QByteArray("<gpx><rte/><rte/></gpx>") << "gpx" << false;
        QTest::newRow("multi-segment") << QByteArray("<gpx><trk><trkseg><trkpt lat='1' lon='1'/></trkseg><trkseg><trkpt lat='2' lon='2'/></trkseg></trk></gpx>") << "gpx" << false;
        QTest::newRow("foreign-root") << QByteArray("<gpx xmlns='urn:unrelated'><rte/></gpx>") << "gpx" << false;
        QTest::newRow("gx-track") << QByteArray("<k:kml xmlns:k='http://www.opengis.net/kml/2.2' xmlns:gx='http://www.google.com/kml/ext/2.2'><k:Document><k:Placemark><gx:Track><k:when>2026-01-01T00:00:00Z</k:when><gx:coord>-70 42 0</gx:coord><gx:coord>-71 43 12</gx:coord></gx:Track></k:Placemark></k:Document></k:kml>") << "Kml" << true;
        QTest::newRow("legacy-points") << QByteArray("<kml><Document><Placemark><Point><coordinates>-70,42,0.</coordinates></Point></Placemark><Placemark><Point><coordinates>-71,43</coordinates></Point></Placemark></Document></kml>") << "kml" << true;
        QTest::newRow("separate-point-containers") << QByteArray("<kml><Folder><Placemark><Point><coordinates>1,1</coordinates></Point></Placemark></Folder><Folder><Placemark><Point><coordinates>2,2</coordinates></Point></Placemark></Folder></kml>") << "kml" << false;
        QTest::newRow("multi-path") << QByteArray("<kml><Placemark><MultiGeometry><LineString><coordinates>1,1 2,2</coordinates></LineString><LineString><coordinates>3,3 4,4</coordinates></LineString></MultiGeometry></Placemark></kml>") << "kml" << false;
        QTest::newRow("polygon") << QByteArray("<kml><Placemark><Polygon/></Placemark></kml>") << "kml" << false;
        QTest::newRow("empty") << QByteArray("<gpx/>") << "gpx" << false;
        QTest::newRow("singleton") << QByteArray("<gpx><rte><rtept lat='0' lon='0'/></rte></gpx>") << "gpx" << false;
    }
    void structures() {
        QFETCH(QByteArray,xml); QFETCH(QString,ext); QFETCH(bool,valid); QTemporaryDir dir;
        const auto r=RouteImport::read(write(dir,"route."+ext,xml)); QCOMPARE(bool(r),valid);
        if (!valid) { QVERIFY(!r.error.isEmpty()); QVERIFY(r.waypoints.isEmpty()); }
    }
    void diagnosticFailures_data() {
        QTest::addColumn<QByteArray>("xml"); QTest::addColumn<QString>("ext"); QTest::addColumn<QString>("detail");
        QTest::newRow("latitude") << QByteArray("<gpx><rte><rtept lat='95.123' lon='0'/><rtept lat='0' lon='1'/></rte></gpx>") << "GPX" << "latitude";
        QTest::newRow("longitude") << QByteArray("<kml><Placemark><LineString><coordinates>0,0 abc,1</coordinates></LineString></Placemark></kml>") << "KML" << "Placemark 1, coordinate 2";
        QTest::newRow("missing") << QByteArray("<gpx><rte><rtept lat='0'/><rtept lat='1' lon='1'/></rte></gpx>") << "gpx" << "longitude";
        QTest::newRow("nonfinite") << QByteArray("<kml><Placemark><LineString><coordinates>0,0 1,NaN</coordinates></LineString></Placemark></kml>") << "kml" << "latitude";
        QTest::newRow("altitude") << QByteArray("<kml><Placemark><LineString><coordinates>0,0,abc 1,1</coordinates></LineString></Placemark></kml>") << "kml" << "altitude";
        QTest::newRow("tuple") << QByteArray("<kml><Placemark><LineString><coordinates>0,0,0,4 1,1</coordinates></LineString></Placemark></kml>") << "kml" << "tuple";
        QTest::newRow("syntax") << QByteArray("<gpx>\n<rte></gpx>") << "gpx" << "Line 2, column";
        QTest::newRow("doctype") << QByteArray("<!DOCTYPE gpx [<!ENTITY secret SYSTEM 'file:///not-read'>]><gpx/>") << "gpx" << "DTD/entity";
        QTest::newRow("speed") << QByteArray("<kml><Placemark><ExtendedData><Data name='speed'><value>-1</value></Data></ExtendedData><LineString><coordinates>0,0 1,1</coordinates></LineString></Placemark></kml>") << "kml" << "speed";
    }
    void diagnosticFailures() {
        QFETCH(QByteArray,xml); QFETCH(QString,ext); QFETCH(QString,detail); QTemporaryDir dir;
        const auto path=write(dir,"bad."+ext,xml); warnings.clear();
        const auto previous=qInstallMessageHandler(messages); const auto r=RouteImport::read(path); qInstallMessageHandler(previous);
        QVERIFY(!r); QCOMPARE(warnings.size(),1); QVERIFY(warnings[0].contains(path));
        QVERIFY(warnings[0].contains(ext.toUpper()+" import failed")); QVERIFY2(warnings[0].contains(detail),qPrintable(warnings[0]));
        QVERIFY(!warnings[0].contains(xml)); QVERIFY(r.waypoints.isEmpty());
    }
    void fileAndResourceLimits() {
        QTemporaryDir dir;
        QVERIFY(RouteImport::read(dir.filePath("missing.gpx")).error.contains("Cannot read"));
        QVERIFY(RouteImport::read(dir.path()+"/.gpx").error.contains("Cannot read"));
        QVERIFY(RouteImport::read(write(dir,"route.txt",gpx)).error.contains("Unsupported format"));
        QVERIFY(RouteImport::read(write(dir,"huge.gpx",QByteArray(4*1024*1024+1,' '))).error.contains("4 MiB"));
        QByteArray deep="<gpx>";for(int i=0;i<65;++i)deep+="<x>";for(int i=0;i<65;++i)deep+="</x>";deep+="</gpx>";
        QVERIFY(RouteImport::read(write(dir,"deep.gpx",deep)).error.contains("64-level"));
        QByteArray many="<gpx>";for(int i=0;i<100001;++i)many+="<x/>";many+="</gpx>";
        QVERIFY(RouteImport::read(write(dir,"many.gpx",many)).error.contains("100000-element"));
        QByteArray points="<gpx><rte>";for(int i=0;i<25001;++i)points+="<rtept lat='0' lon='1'/>";points+="</rte></gpx>";
        QVERIFY(RouteImport::read(write(dir,"points.gpx",points)).error.contains("25000-point"));
    }
    void encodingNamesSpeedAndDuplicates() {
        QTemporaryDir dir;
        const QString text=QString::fromUtf8("<?xml version='1.0' encoding='UTF-16'?>")+QString::fromUtf8(gpx).replace("Harbor","Hárbor");
        QByteArray bytes("\xff\xfe",2);for(auto c:text){bytes.append(char(c.unicode()&255));bytes.append(char(c.unicode()>>8));}
        auto r=RouteImport::read(write(dir,"utf16.GpX",bytes)); QVERIFY2(bool(r),qPrintable(r.error));
        QCOMPARE(r.waypoints[0].name,"A & B"); QVERIFY(r.selection.contains(QString::fromUtf8("Hárbor")));
        r=RouteImport::read(write(dir,"repeat.kml","<kml><Placemark><ExtendedData><Data name='speed'><value>10</value></Data></ExtendedData><LineString><coordinates>0,0 0,0 1,1 0,0</coordinates></LineString></Placemark></kml>"));
        QVERIFY(r);QCOMPARE(r.waypoints.size(),3);QCOMPARE(r.waypoints[0].speedMs,5.14444);
        QCOMPARE(r.waypoints.first().coordinate,r.waypoints.last().coordinate);
    }
    void interchangeNamesOrderAndPrecision_data() {
        QTest::addColumn<QString>("format");
        QTest::newRow("GPX-current-import-domain") << "gpx";
        QTest::newRow("KML-2.2-path-and-labels") << "kml";
    }
    void interchangeNamesOrderAndPrecision() {
        QFETCH(QString,format);
        QTemporaryDir dir;
        const QVector<QGeoCoordinate> points = {
            {42.123456789012344,179.99999999999997},
            {-0.000000000000123456,-179.99999999999997},
            {-89.99999999999999,180},{90,-180},{-90,0},{0,0}};
        const QString routeName = QString::fromUtf8("航路 café & <Harbor> \"A\"");
        const QStringList names = {QString::fromUtf8("灯台 & <入港>"),"duplicate","duplicate",
                                   QString::fromUtf8("Écluse \"Nord\""),"South","Zero"};
        // Author synthetic input documents, not a production export implementation.
        // The current importer accepts +180 and exponent notation; a future GPX
        // writer must meet the stricter XSD decimal / longitude requirements.
        QDomDocument doc;
        auto element=[&doc](QDomElement parent,const QString &tag,const QString &text) {
            auto e=doc.createElement(tag);e.appendChild(doc.createTextNode(text));parent.appendChild(e);return e;
        };
        auto root=doc.createElement(format);doc.appendChild(root);
        if(format=="gpx") {
            root.setAttribute("xmlns","http://www.topografix.com/GPX/1/1");
            root.setAttribute("version","1.1");root.setAttribute("creator","Synthetic import fixture");
            root.setAttribute("xmlns:test","urn:raynmea:synthetic-test");
            auto route=doc.createElement("rte");root.appendChild(route);element(route,"name",routeName);
            for(int i=0;i<points.size();++i) {
                auto p=doc.createElement("rtept");route.appendChild(p);
                p.setAttribute("lat",QString::number(points[i].latitude(),'g',17));
                p.setAttribute("lon",QString::number(points[i].longitude(),'g',17));
                element(p,"name",names[i]);
                auto extension=doc.createElement("extensions");p.appendChild(extension);
                element(extension,"test:sourceWaypointID",QString::number(7+i*101));
            }
        } else {
            root.setAttribute("xmlns","http://www.opengis.net/kml/2.2");
            auto document=doc.createElement("Document");root.appendChild(document);
            auto path=doc.createElement("Placemark");document.appendChild(path);element(path,"name",routeName);
            auto line=doc.createElement("LineString");path.appendChild(line);
            QStringList coordinates;
            for(const auto &point:points)
                coordinates << QString::number(point.longitude(),'g',17)+","+QString::number(point.latitude(),'g',17);
            element(line,"coordinates",coordinates.join(' '));
            for(int i=0;i<points.size();++i) {
                auto marker=doc.createElement("Placemark");document.appendChild(marker);element(marker,"name",names[i]);
                auto point=doc.createElement("Point");marker.appendChild(point);element(point,"coordinates",coordinates[i]);
            }
        }
        const auto r=RouteImport::read(write(dir,"synthetic."+format,doc.toByteArray()));
        QVERIFY2(bool(r),qPrintable(r.error));QCOMPARE(r.name,routeName);QVERIFY(!r.pointSequence);
        QCOMPARE(r.waypoints.size(),points.size());
        for(int i=0;i<points.size();++i) {
            QCOMPARE(r.waypoints[i].name,names[i]);
            // Exact binary double equality, rather than display-rounded/fuzzy comparisons.
            QVERIFY(r.waypoints[i].coordinate.latitude()==points[i].latitude());
            QVERIFY(r.waypoints[i].coordinate.longitude()==points[i].longitude());
            QCOMPARE(r.waypoints[i].speedMs,Waypoint{}.speedMs);
        }
    }
    void interchangeContractFixtures() {
        const auto gpx=RouteImport::read(QFINDTESTDATA("fixtures/routes/interchange-route.gpx"));
        const auto kml=RouteImport::read(QFINDTESTDATA("fixtures/routes/interchange-route.kml"));
        QVERIFY2(bool(gpx),qPrintable(gpx.error));QVERIFY2(bool(kml),qPrintable(kml.error));
        QCOMPARE(gpx.name,QString::fromUtf8("航路 café & <Harbor> \"A\""));QCOMPARE(gpx.name,kml.name);
        const QVector<QGeoCoordinate> points={{42.123456789012344,179.99999999999997},
            {-0.000000000000123456,-179.99999999999997},{90,-180}};
        const QStringList names={QString::fromUtf8("灯台 & <入港>"),"duplicate","duplicate"};
        QCOMPARE(gpx.waypoints.size(),points.size());QCOMPARE(kml.waypoints.size(),points.size());
        for(int i=0;i<points.size();++i) {
            for(const auto *result:{&gpx,&kml}) {
                QVERIFY(result->waypoints[i].coordinate.latitude()==points[i].latitude());
                QVERIFY(result->waypoints[i].coordinate.longitude()==points[i].longitude());
                QCOMPARE(result->waypoints[i].name,names[i]);
            }
        }
        QVERIFY(!gpx.pointSequence);QVERIFY(!kml.pointSequence);
    }
    void interchangeInvalidCoordinates_data() {
        QTest::addColumn<QString>("format");QTest::addColumn<QString>("lat");QTest::addColumn<QString>("lon");
        for(const auto &format:{QString("gpx"),QString("kml")}) {
            for(const auto &pair:QList<QPair<QString,QString>>{{"90.000000000001","0"},{"-90.000000000001","0"},
                    {"0","180.000000000001"},{"0","-180.000000000001"},{"nan","0"},{"0","inf"},{"","0"}}) {
                const auto label=(format+":"+pair.first+","+pair.second).toUtf8();
                QTest::newRow(label.constData()) << format << pair.first << pair.second;
            }
        }
    }
    void interchangeInvalidCoordinates() {
        QFETCH(QString,format);QFETCH(QString,lat);QFETCH(QString,lon);QTemporaryDir dir;
        const auto xml=format=="gpx"
            ? QString("<gpx><rte><rtept lat='%1' lon='%2'/><rtept lat='1' lon='1'/></rte></gpx>").arg(lat,lon)
            : QString("<kml><Placemark><LineString><coordinates>%2,%1 1,1</coordinates></LineString></Placemark></kml>").arg(lat,lon);
        const auto r=RouteImport::read(write(dir,"invalid."+format,xml.toUtf8()));
        QVERIFY(!r);QVERIFY(!r.error.isEmpty());QVERIFY(r.waypoints.isEmpty());
    }
    void originalExamplesAndSuccessSummary() {
        QDir root(QDir::currentPath()+"/routes"); int count=0;
        for (const auto &folder:{QString("gpx"),QString("kml")})
            for(const auto &file:QDir(root.filePath(folder)).entryInfoList(QDir::Files)) {
                debugMessages.clear();const auto previous=qInstallMessageHandler(messages);
                const auto r=RouteImport::read(file.absoluteFilePath());qInstallMessageHandler(previous);
                QVERIFY2(bool(r),qPrintable(file.fileName()+": "+r.error));QVERIFY(r.waypoints.size()>=2);
                QCOMPARE(debugMessages.size(),1);QVERIFY(debugMessages[0].contains("loaded"));QVERIFY(debugMessages[0].contains(r.selection));++count;
            }
        QCOMPARE(count,5);
    }
    void standardsFixturesAndKnownGeometry() {
        const QDir fixtures(QFINDTESTDATA("fixtures/routes"));
        for(const auto &file:{"gpx10-route.gpx","gpx11-track.gpx","kml22-linestring.kml","kml23-track.kml"}) {
            const auto r=RouteImport::read(fixtures.filePath(file));QVERIFY2(bool(r),qPrintable(r.error));
            QCOMPARE(r.waypoints.size(),2);QCOMPARE(r.waypoints[0].coordinate,QGeoCoordinate(42.125,-70.25));
            QCOMPARE(r.waypoints[1].coordinate,QGeoCoordinate(42.25,-70.5));QVERIFY(!r.pointSequence);
        }
        const auto r=RouteImport::read(fixtures.filePath("google-earth-track.kml"));QVERIFY2(bool(r),qPrintable(r.error));
        const QVector<QGeoCoordinate> expected={{37.371915,-122.207881},{37.373288,-122.205712},
            {37.373939,-122.204678},{37.374630,-122.203572},{37.374706,-122.203451},
            {37.374780,-122.203329},{37.374857,-122.203207}};
        QCOMPARE(r.waypoints.size(),expected.size());for(int i=0;i<expected.size();++i)QCOMPARE(r.waypoints[i].coordinate,expected[i]);
    }
    void pointAnnotationsRequireExplicitConsent() {
        AutoPilotSimulator a;QTemporaryDir dir;
        const auto file=write(dir,"points.kml","<kml><Document><Placemark><Point><coordinates>1,1</coordinates></Point></Placemark><Placemark><Point><coordinates>2,2</coordinates></Point></Placemark></Document></kml>");
        const auto candidate=RouteImport::read(file);QVERIFY(candidate);QVERIFY(candidate.pointSequence);
        QVERIFY(!a.loadRoute(file));QVERIFY(a.route.isEmpty());QVERIFY(a.lastRouteError().contains("Confirm"));
        QVERIFY(a.loadRoute(file,true));QVERIFY(a.route.size()>=2);QVERIFY(!a.isRunning());
    }
    void existingKmlWaypointMetadata() {
        const auto r=RouteImport::read(QDir::currentPath()+"/routes/kml/seabrook.kml");QVERIFY(r);QVERIFY(!r.pointSequence);
        QCOMPARE(r.waypoints.size(),11);QCOMPARE(r.waypoints.first().name,"001");
        QCOMPARE(r.waypoints.first().speedMs,5.5*0.514444);
        QCOMPARE(r.waypoints.last().coordinate,QGeoCoordinate(42.8882,-70.8208));
    }
    void permissionError() {
#ifdef Q_OS_UNIX
        QTemporaryDir dir;const auto path=write(dir,"private.gpx",gpx);
        QVERIFY(QFile::setPermissions(path,{}));const auto r=RouteImport::read(path);
        QVERIFY(QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner));
        QVERIFY(!r);QVERIFY(r.error.contains("Cannot read route file"));
#else
        QSKIP("POSIX mode-based permission fixture runs on Ubuntu; Windows uses ACLs.");
#endif
    }
    void failedImportPreservesRunningProgression() {
        QTemporaryDir dir;AutoPilotSimulator a;
        QVERIFY(a.loadRoute(write(dir,"good.gpx",gpx)));a.setSpeed(5);a.setReverseRoute(true);a.start();a.advance(3);
        const auto position=a.position;const auto distance=a.totalDistanceTraveled;const auto index=a.currentIndex;
        QSignalSpy state(&a,&AutoPilotSimulator::RunningStateChanged), metadata(&a,&AutoPilotSimulator::VesselWpBearingChanged), loaded(&a,&AutoPilotSimulator::routeLoaded);
        QVERIFY(!a.loadRoute(write(dir,"bad.gpx","<gpx>")));
        QVERIFY(a.isRunning());QVERIFY(a.updateTimer.isActive());QCOMPARE(a.position,position);QCOMPARE(a.currentIndex,index);QCOMPARE(a.totalDistanceTraveled,distance);
        QCOMPARE(a.route.size(),2);QCOMPARE(state.count(),0);QCOMPARE(metadata.count(),0);QCOMPARE(loaded.count(),0);QVERIFY(!a.lastRouteError().isEmpty());
        a.advance(1);QVERIFY(a.totalDistanceTraveled>distance);
        QVERIFY(a.loadRoute(write(dir,"next.KML",kml)));QVERIFY(!a.running);QCOMPARE(a.currentIndex,0);QCOMPARE(a.totalDistanceTraveled,0.0);QVERIFY(!a.position.isValid());QVERIFY(a.lastRouteError().isEmpty());
    }
};
QTEST_GUILESS_MAIN(RouteImportTests)
#include "route_import_tests.moc"
