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
#include <QApplication>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QDomDocument>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QTemporaryDir>
#include <memory>
#include "themes.h"
#include "apppaths.h"
#include "appsettings.h"

namespace {
QByteArray bytes(const QString &path) { QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray(); }
void write(const QString &path,const QByteArray &data) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(data),data.size());
}
const QByteArray custom1="/* DiskRevision1 */ QLabel { background:#123456; color:white; } QPushButton { image:url(mark.png); }";
const QByteArray custom2="/* DiskRevision2 */ QLabel { background:#654321; color:white; } QPushButton { image:url(mark.png); }";
}
class ThemeCatalogTests:public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> profile;
private slots:
    void init() {
        profile=std::make_unique<QTemporaryDir>(QDir::currentPath()+"/catalog-profile-XXXXXX");
        QVERIFY(profile->isValid());qputenv("RAYNMEA_TEST_HOME",profile->path().toUtf8());
    }
    void builtInsAndActualResources() {
        const QStringList expected{"System","Light","Dark","Darkeum","Teal Dark","Porcelain"};
        QCOMPARE(Themes::builtInThemes(),expected);Themes::initializeEditableResources();
        QCOMPARE(Themes::available(),expected);QVERIFY(!Themes::available().contains("None"));
        QStringList actual;QDirIterator it(":/stylesheets",QDir::Files,QDirIterator::Subdirectories);
        while(it.hasNext())actual<<it.next();actual.sort();
        QStringList files{":/stylesheets/Common.qss",":/stylesheets/palettes.json"};
        for(const auto &name:expected) {
            if(name=="System") {QVERIFY(Themes::source(name).isEmpty());QVERIFY(Themes::styleSheet(name).isEmpty());continue;}
            const auto storage=name=="Darkeum"?QString("Darkeum-Refined"):name;
            files<<":/stylesheets/"+storage+"/"+storage+".qss";
            QVERIFY(QFile::exists(Themes::source(name)));
            const auto sheet=Themes::styleSheet(name);
            QVERIFY(sheet.contains("QTabWidget::pane"));QVERIFY(!sheet.contains("@COMMON@"));
            QVERIFY(!sheet.contains("url("));
        }
        files.sort();QCOMPARE(actual,files);
        for(const auto &name:QStringList{"Aqua","darkeum","ConsoleStyle","Custom","EasyCode","ElegantDark","MacOS","ManjaroMin","ManjaroMix","MaterialDark","NeonButtons","Ubuntu","adaptic"}) {
            QVERIFY(Themes::source(name).isEmpty());QVERIFY(!QDir(":/stylesheets/"+name).exists());
            QVERIFY(!QFile::exists(AppPaths::themesDirectory()+"/"+name+"/"+name+".qss"));
        }
        // Actual resource manifest, including retained compatibility aliases.
        const auto qrc=QFINDTESTDATA("../RayNmeaSimulator.qrc");QDomDocument doc;
        QVERIFY(doc.setContent(bytes(qrc)));const auto entries=doc.elementsByTagName("file");
        for(int i=0;i<entries.size();++i) {
            const auto e=entries.at(i).toElement();const auto relative=e.text();
            QCOMPARE(bytes(":"+QString("/")+e.attribute("alias",relative)),bytes(QFileInfo(qrc).dir().filePath(relative)));
        }
        for(const auto &alias:QStringList{"tree-open.png","tree-closed.png","arrow-down.png","arrow-up.png","arrow-left.png","arrow-right.png","checkbox.png","Icons/resources/icons/downarrow.png"})
            QVERIFY(!QImage(":/"+alias).isNull());
        QJsonArray values;
        for(const auto &name:expected) {
            const auto palette=Themes::palette(name);
            values.append(QJsonObject{{"name",name},{"qssSha256",QString::fromLatin1(QCryptographicHash::hash(Themes::styleSheet(name).toUtf8(),QCryptographicHash::Sha256).toHex())},
                {"window",palette.color(QPalette::Window).name()},{"text",palette.color(QPalette::Text).name()}});
        }
        const auto out=qEnvironmentVariable("RAY_THEME_CATALOG_OUTPUT");
        if(!out.isEmpty())write(out,QJsonDocument(values).toJson());
    }
    void aliasesAndMissingSelections() {
        Themes::initializeEditableResources();AppSettings settings(APP_COMPANY,APP_NAME);
        settings.setValue("Preference/StyleSheetName","None");settings.setValue("Sentinel","preserved");settings.sync();
        QCOMPARE(Themes::selectionName("None"),QString("System"));QVERIFY(Themes::styleSheet("None").isEmpty());
        for(const auto &name:QStringList{"Aqua","darkeum","missing-theme"}) {
            settings.setValue("Preference/StyleSheetName",name);settings.sync();
            QCOMPARE(Themes::selectionName(name),QString("Dark"));
            QCOMPARE(Themes::styleSheet(name),Themes::styleSheet("Dark"));
            QCOMPARE(Themes::palette(name),Themes::palette("Dark"));
            AppSettings reopened(APP_COMPANY,APP_NAME);QCOMPARE(reopened.value("Preference/StyleSheetName").toString(),name);
        }
        write(AppPaths::themesDirectory()+"/Broken.qss","QLabel { color:red;");
        QVERIFY(!Themes::available().contains("Broken"));QCOMPARE(Themes::selectionName("Broken"),QString("Dark"));
        QCOMPARE(settings.value("Sentinel").toString(),QString("preserved"));
    }
    void retiredNamesRemainCustomAndNeverReseed() {
        const auto path=AppPaths::themesDirectory()+"/Aqua/Aqua.qss";
        write(path,custom1);write(AppPaths::themesDirectory()+"/Aqua/owner.asset","owner bytes");
        Themes::initializeEditableResources();Themes::initializeEditableResources();
        QCOMPARE(bytes(path),custom1);QVERIFY(Themes::available().contains("Aqua"));
        QCOMPARE(Themes::source("Aqua"),path);QCOMPARE(Themes::selectionName("Aqua"),QString("Aqua"));
        QCOMPARE(bytes(AppPaths::themesDirectory()+"/Aqua/owner.asset"),QByteArray("owner bytes"));
        QVERIFY(!QFile::exists(AppPaths::themesDirectory()+"/Aqua/QSS_IMG/go-down-symbolic.symbolic.png"));
    }
    void installLoadAndRealProcessRestart() {
        Themes::initializeEditableResources();QVERIFY(!Themes::available().contains("Harbor Blue"));
        const auto root=AppPaths::themesDirectory()+"/Harbor Blue";
        write(root+"/Harbor Blue.qss",custom1);
        QImage asset(4,4,QImage::Format_ARGB32);asset.fill(Qt::blue);QVERIFY(asset.save(root+"/mark.png"));
        QVERIFY(Themes::available().contains("Harbor Blue"));
        QVERIFY(Themes::styleSheet("Harbor Blue").contains(root+"/mark.png"));
        QLabel live("Runtime theme");live.resize(160,45);live.setStyleSheet(Themes::styleSheet("Harbor Blue"));live.show();
        QTest::qWait(20);QCOMPARE(live.grab().toImage().pixelColor(2,2),QColor("#123456"));live.hide();
        AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Preference/StyleSheetName","Harbor Blue");settings.sync();
        write(root+"/Harbor Blue.qss",custom2);Themes::initializeEditableResources();QCOMPARE(bytes(root+"/Harbor Blue.qss"),custom2);
        const auto executable=QCoreApplication::applicationFilePath();const auto before=QCryptographicHash::hash(bytes(executable),QCryptographicHash::Sha256);
        QProcess restart;restart.start(executable,{"--custom-restart-probe"});QVERIFY(restart.waitForStarted(10000));
        QVERIFY(restart.waitForFinished(20000));QCOMPARE(restart.exitStatus(),QProcess::NormalExit);QCOMPARE(restart.exitCode(),0);
        QCOMPARE(QCryptographicHash::hash(bytes(executable),QCryptographicHash::Sha256),before);
        QCOMPARE(bytes(root+"/Harbor Blue.qss"),custom2);
        QVERIFY(Themes::available().size()>Themes::builtInThemes().size());
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    if(app.arguments().contains("--custom-restart-probe")) {
        AppSettings saved(APP_COMPANY,APP_NAME);const auto name=saved.value("Preference/StyleSheetName").toString();
        if(name!="Harbor Blue" || !Themes::available().contains(name) || Themes::selectionName(name)!=name) return 2;
        const auto sheet=Themes::styleSheet(name);if(!sheet.contains("DiskRevision2") || !sheet.contains("/Harbor Blue/mark.png"))return 3;
        QLabel label("Restarted custom theme");label.resize(160,45);label.setStyleSheet(sheet);label.show();app.processEvents();
        return label.grab().toImage().pixelColor(2,2)==QColor("#654321")?0:4;
    }
    ThemeCatalogTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "theme_catalog_tests.moc"
