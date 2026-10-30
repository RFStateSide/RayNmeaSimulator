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
#include <QLocale>
#include <limits>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include "routeexport.h"

namespace {
RouteExport::Snapshot sample() {
    RouteExport::Snapshot s;
    s.name=QString::fromUtf8("航路 café & <Harbor> \"A\"");
    const QVector<QGeoCoordinate> points={{42.123456789012344,179.99999999999997},
        {-0.000000000000123456,-179.99999999999997},{90,180},{-90,-0.0},{1e-30,0}};
    const QStringList names={QString::fromUtf8("灯台 & <入港>"),"duplicate","duplicate","South","Tiny"};
    for(int i=0;i<points.size();++i) { Waypoint p;p.coordinate=points[i];p.name=names[i];s.waypoints.append(p); }
    s.sourceDescription="Private source NAME; sparse IDs 7/108/65532";
    return s;
}
QByteArray read(const QString &path) { QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll(); }
void put(const QString &path,const QByteArray &bytes) { QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size()); }
}
class RouteExportTests : public QObject {
    Q_OBJECT
private slots:
    void formats_data() {
        QTest::addColumn<bool>("kml");QTest::newRow("GPX")<<false;QTest::newRow("KML")<<true;
    }
    void formats() {
        QFETCH(bool,kml);const auto s=sample();const auto format=kml?RouteExport::Format::Kml:RouteExport::Format::Gpx;
        const auto serialized=RouteExport::serialize(s,format);QVERIFY2(bool(serialized),qPrintable(serialized.error));
        QVERIFY(!serialized.bytes.contains("Private source"));QVERIFY(!serialized.bytes.contains("<speed"));
        QVERIFY(!serialized.bytes.contains("<ele"));QVERIFY(!serialized.bytes.contains("<time"));
        QDomDocument doc;QVERIFY(doc.setContent(serialized.bytes));
        const auto points=doc.elementsByTagName(kml?"Point":"rtept");QCOMPARE(points.size(),5);
        QCOMPARE(doc.elementsByTagName("name").at(0).toElement().text(),s.name);
        for(int i=0;i<points.size();++i) {
            const auto p=points.at(i).toElement();
            QCOMPARE(p.firstChildElement("name").text(),kml?QString():s.waypoints[i].name);
            if(!kml) {
                const auto lat=p.attribute("lat"),lon=p.attribute("lon");
                QVERIFY(!lat.contains('e',Qt::CaseInsensitive));QVERIFY(!lon.contains('e',Qt::CaseInsensitive));
                QVERIFY(!lat.contains(','));QVERIFY(lat.toDouble()==s.waypoints[i].coordinate.latitude());
                QVERIFY(lon.toDouble()==(i==2?-180:s.waypoints[i].coordinate.longitude()));
            }
        }
        if(kml) {
            QCOMPARE(doc.elementsByTagName("LineString").size(),1);
            const auto coords=doc.elementsByTagName("LineString").at(0).toElement().firstChildElement("coordinates").text().split(' ',Qt::SkipEmptyParts);
            QCOMPARE(coords.size(),5);QCOMPARE(coords[2],QString("-180,90"));QCOMPARE(coords[3],QString("0,-90"));
            const auto markers=doc.elementsByTagName("Placemark");QCOMPARE(markers.size(),6);
            for(int i=0;i<5;++i)QCOMPARE(markers.at(i+1).toElement().firstChildElement("name").text(),s.waypoints[i].name);
        }
        QTemporaryDir dir;const auto file=dir.filePath("roundtrip."+RouteExport::suffix(format));
        const auto destination=RouteExport::inspectDestination(file);QVERIFY(destination);
        QVERIFY(RouteExport::writeAtomic(destination,serialized.bytes).isEmpty());
        const auto imported=RouteImport::read(file);QVERIFY2(bool(imported),qPrintable(imported.error));
        QCOMPARE(imported.name,s.name);QCOMPARE(imported.waypoints.size(),5);QVERIFY(!imported.pointSequence);
        for(int i=0;i<5;++i) {
            QCOMPARE(imported.waypoints[i].name,s.waypoints[i].name);
            QVERIFY(imported.waypoints[i].coordinate.latitude()==s.waypoints[i].coordinate.latitude());
            QVERIFY(imported.waypoints[i].coordinate.longitude()==(i==2?-180:s.waypoints[i].coordinate.longitude()));
        }
        QCOMPARE(s.waypoints[2].coordinate.longitude(),180.0); // normalization never writes back
        const auto evidence=qEnvironmentVariable("RAYNMEA_EXPORT_SCHEMA_DIR");
        if(!evidence.isEmpty()) { QVERIFY(QDir().mkpath(evidence));put(QDir(evidence).filePath(kml?"export.kml":"export.gpx"),serialized.bytes); }
    }
    void localeAndXmlValidity() {
        const auto old=QLocale();QLocale::setDefault(QLocale(QLocale::German));
        const auto a=RouteExport::serialize(sample(),RouteExport::Format::Gpx);QLocale::setDefault(old);
        QVERIFY(a);QVERIFY(a.bytes.contains("179.99999999999997"));
        auto s=sample();s.name.append(QChar(1));QVERIFY(!RouteExport::serialize(s,RouteExport::Format::Gpx));
        s=sample();s.waypoints[1].name=QString(QChar(0xd800));QVERIFY(!RouteExport::serialize(s,RouteExport::Format::Kml));
        s=sample();s.waypoints[0].name=QString::fromUtf8("⚓ 🙂");QVERIFY(RouteExport::serialize(s,RouteExport::Format::Kml));
    }
    void invalidAndBounded() {
        RouteExport::Snapshot s;QVERIFY(!RouteExport::validate(s).isEmpty());
        s=sample();s.waypoints.resize(1);QVERIFY(!RouteExport::validate(s).isEmpty());
        s=sample();s.waypoints.resize(25001);QVERIFY(!RouteExport::validate(s).isEmpty());
        s=sample();s.waypoints[0].coordinate=QGeoCoordinate(91,0);QVERIFY(!RouteExport::validate(s).isEmpty());
        s=sample();s.waypoints[0].coordinate=QGeoCoordinate(0,std::numeric_limits<double>::infinity());QVERIFY(!RouteExport::validate(s).isEmpty());
        s=sample();s.waypoints[1].coordinate=s.waypoints[0].coordinate;QVERIFY(!RouteExport::validate(s).isEmpty());
        s=sample();s.waypoints.last().coordinate=s.waypoints.first().coordinate;QVERIFY(RouteExport::validate(s).isEmpty());
        s=sample();for(auto &p:s.waypoints)p.name=QString(300000,'&');
        for(auto f:{RouteExport::Format::Gpx,RouteExport::Format::Kml}) { auto result=RouteExport::serialize(s,f);QVERIFY(!result);QVERIFY(result.bytes.isEmpty()); }
    }
    void atomicWritesAndDestinationChanges() {
        QTemporaryDir dir;const auto path=dir.filePath("out.gpx");
        const auto data=RouteExport::serialize(sample(),RouteExport::Format::Gpx);QVERIFY(data);
        auto token=RouteExport::inspectDestination(path);QVERIFY(token);QVERIFY(!token.exists);
        QVERIFY(RouteExport::writeAtomic(token,data.bytes).isEmpty());QCOMPARE(read(path),data.bytes);
        token=RouteExport::inspectDestination(path);QVERIFY(token.exists);
        QVERIFY(RouteExport::writeAtomic(token,"replacement").isEmpty());QCOMPARE(read(path),QByteArray("replacement"));
        token=RouteExport::inspectDestination(path);put(path,"external edit");
        QVERIFY(!RouteExport::writeAtomic(token,data.bytes).isEmpty());QCOMPARE(read(path),QByteArray("external edit"));
        token=RouteExport::inspectDestination(dir.filePath("new.gpx"));put(token.path,"external creation");
        QVERIFY(!RouteExport::writeAtomic(token,data.bytes).isEmpty());QCOMPARE(read(token.path),QByteArray("external creation"));
        QVERIFY(!RouteExport::inspectDestination(dir.path()));
        QVERIFY(!RouteExport::inspectDestination(dir.filePath("missing/out.gpx")));
        token=RouteExport::inspectDestination(path);QVERIFY(QFile::remove(path));QVERIFY(QDir().mkdir(path));
        QVERIFY(!RouteExport::writeAtomic(token,data.bytes).isEmpty());QVERIFY(QFileInfo(path).isDir());
    }
    void sourceAndSymlinkProtection() {
        QTemporaryDir dir;const auto path=dir.filePath("source.gpx");put(path,"owner input");
        QVERIFY(!RouteExport::inspectDestination(path,path));
        QVERIFY(!RouteExport::inspectDestination(dir.path()+"/./source.gpx",path));
        const auto alias=dir.filePath("alias.gpx");
#ifdef Q_OS_UNIX
        QVERIFY(QFile::link(path,alias));QVERIFY(QFileInfo(alias).isSymLink());
        QVERIFY(!RouteExport::inspectDestination(alias,path));
        QVERIFY(!RouteExport::inspectDestination(alias));
        const auto parentAlias=dir.filePath("folder-alias");QVERIFY(QFile::link(dir.path(),parentAlias));
        QVERIFY(!RouteExport::inspectDestination(parentAlias+"/source.gpx",path));
#endif
        QCOMPARE(read(path),QByteArray("owner input"));
    }
    void permissionFailurePreservesDestination() {
#ifdef Q_OS_UNIX
        QTemporaryDir dir;const auto path=dir.filePath("out.gpx");put(path,"original");
        const auto token=RouteExport::inspectDestination(path);QVERIFY(token);
        QVERIFY(QFile::setPermissions(dir.path(),QFile::ReadOwner|QFile::ExeOwner));
        const auto error=RouteExport::writeAtomic(token,"new");
        QVERIFY(QFile::setPermissions(dir.path(),QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QVERIFY(!error.isEmpty());QCOMPARE(read(path),QByteArray("original"));
#else
        QSKIP("POSIX directory-permission failure is qualified on Ubuntu.");
#endif
    }
    void lockedDestinationPreservesBytes() {
#ifdef Q_OS_WIN
        QTemporaryDir dir;const auto path=dir.filePath("locked.gpx");put(path,"original");
        const auto token=RouteExport::inspectDestination(path);QVERIFY(token);
        const HANDLE handle=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,
            nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        QVERIFY(handle!=INVALID_HANDLE_VALUE);
        const auto error=RouteExport::writeAtomic(token,"new");CloseHandle(handle);
        QVERIFY(!error.isEmpty());QCOMPARE(read(path),QByteArray("original"));
#else
        QSKIP("Windows deny-delete sharing fixture; POSIX atomic replacement permits open readers.");
#endif
    }
};
QTEST_GUILESS_MAIN(RouteExportTests)
#include "route_export_tests.moc"
