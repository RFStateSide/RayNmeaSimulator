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
#include "apppaths.h"
#include "appsettings.h"
#include "layoutdocument.h"
#include "mainwindow.h"
#include "maincontroller.h"
#include "nmeasenderworker.h"
#include <QtTest>
#include <QAction>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>

namespace {
QByteArray bytes(const QString &path) { QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray(); }
QJsonObject read(const QString &path) { return QJsonDocument::fromJson(bytes(path)).object(); }
QMap<int,QJsonObject> widgets(const QJsonObject &root) {
    QMap<int,QJsonObject> result;
    for(const auto &g:root["layouts"].toArray())for(const auto &w:g.toObject()["widgets"].toArray())
        result.insert(w.toObject()["idX"].toInt(),w.toObject());
    return result;
}
void storedStates(const QJsonObject &source,const QJsonObject &loaded) {
    const auto original=widgets(source),actual=widgets(loaded);
    QCOMPARE(actual.keys(),original.keys());
    QCOMPARE(loaded["numberOfWidgetColumns"],source["numberOfWidgetColumns"]);
    QMap<QString,int> groups,loadedGroups;
    for(const auto &g:source["layouts"].toArray())groups[g.toObject()["groupName"].toString()]=g.toObject()["widgets"].toArray().size();
    for(const auto &g:loaded["layouts"].toArray())loadedGroups[g.toObject()["groupName"].toString()]=g.toObject()["widgets"].toArray().size();
    // The application retains empty workspace tabs as well as document groups.
    for(auto i=groups.cbegin();i!=groups.cend();++i)QCOMPARE(loadedGroups.value(i.key()),i.value());
    for(auto i=original.cbegin();i!=original.cend();++i) {
        for(const QString key:{"type","label","enabled","dispatch","engineProfile","configuration","electricalSimulation","row","column","rowSpan","columnSpan"})
            if(i.value().contains(key)) {
                auto expected=i.value()[key];
                // An absent weather-origin map has always meant an empty map.
                // Compare that optional default without rewriting the saved file.
                if(key=="configuration" && i.value()["type"]=="GeneratorWeather") {
                    auto config=expected.toObject();
                    if(!config.contains("origins"))config["origins"]=QJsonObject();
                    expected=config;
                }
                QCOMPARE(actual[i.key()][key],expected);
            }
    }
}
}

class BundledContentTests:public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings settings("RFStateSide","RayNmeaSimulator");settings.clear(); }
    void actualCompiledInventoryAndSourceBytes() {
        const QStringList files{"Default.json","environment-weather.json","navigation-autopilot.json","single-engine.json","synthetic-ais.json","twin-engine.json"};
        const QStringList names{"Default","Environment / Weather","Navigation / Autopilot","Single Engine","Synthetic AIS Demonstration","Twin Engine"};
        QCOMPARE(QDir(":/templates").entryList({"*.json"},QDir::Files,QDir::Name),files);
        for(int i=0;i<files.size();++i) {
            const auto resource=":/templates/"+files[i];const auto source=QFINDTESTDATA("../resources/templates/"+files[i]);
            QVERIFY(!source.isEmpty());QCOMPARE(bytes(resource),bytes(source));
            const auto meta=read(resource)["template"].toObject();QCOMPARE(meta["name"].toString(),names[i]);
            QVERIFY(!meta["description"].toString().isEmpty());
            QFile file(resource);QVERIFY(!file.open(QIODevice::ReadWrite));
            QString error;QVERIFY(!LayoutDocument::write(resource,read(resource),error));
        }
        const auto source=QFINDTESTDATA("../resources/defaults/defaultLayout.json");
        QVERIFY(!source.isEmpty());QCOMPARE(bytes(":/defaults/defaultLayout.json"),bytes(source));
        QVERIFY(!QDir(":/templates").exists("defaultLayout.json"));
    }
    void eachApprovedTemplateLoads_data() {
        QTest::addColumn<QString>("path");
        for(const auto &file:QDir(":/templates").entryList({"*.json"},QDir::Files))
            QTest::newRow(qPrintable(file))<<":/templates/"+file;
    }
    void eachApprovedTemplateLoads() {
        QFETCH(QString,path);MainWindow window;
        auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        AppSettings settings("RFStateSide","RayNmeaSimulator");
        settings.setValue("Vessel/MMSI",0);settings.setValue("Vessel/VesselType",0);
        settings.setValue("Vessel/RadioCallsign",QString());settings.setValue("Vessel/Length",0);settings.setValue("Vessel/Beam",0);
        const auto original=bytes(path);QJsonObject resolved;QString error;
        QVERIFY2(LayoutDocument::resolve(read(path),settings,resolved,error),qPrintable(error));
        QVERIFY2(controller->validateDocument(resolved,&error),qPrintable(error));
        QVERIFY(controller->loadLayoutDocument(resolved,{},[](const QString &){return true;}));
        storedStates(resolved,controller->captureLayout());
        QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
        QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
        QCOMPARE(bytes(path),original);
    }
    void userTemplatesRemainEditableAndSeparate() {
        QString error;const auto user=LayoutDocument::savePath("Independent owner fixture",error);QVERIFY2(!user.isEmpty(),qPrintable(error));
        const auto compiled=bytes(":/templates/Default.json");auto root=read(":/templates/Default.json");
        auto meta=root["template"].toObject();meta["name"]="Independent owner fixture";root["template"]=meta;
        QVERIFY(LayoutDocument::write(user,root,error));QVERIFY(LayoutDocument::templates().contains(user));
        meta["description"]="Edited private fixture";root["template"]=meta;QVERIFY(LayoutDocument::write(user,root,error));
        QCOMPARE(read(user),root);QCOMPARE(bytes(":/templates/Default.json"),compiled);
        QCOMPARE(QDir(":/templates").entryList({"*.json"},QDir::Files).size(),6);
    }
    void defaultMenuLoadsApprovedResourceWithoutSending() {
        QCOMPARE(AppPaths::defaultLayout(),QString(":/defaults/defaultLayout.json"));
        MainWindow window;auto *load=window.findChild<QAction*>("actionLoad_Default");QVERIFY(load);
        load->trigger();auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        const auto source=read(":/defaults/defaultLayout.json");QCOMPARE(source["documentType"].toString(),QString("layout"));
        storedStates(source,controller->captureLayout());
        QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
        QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
    void existingUserDefaultKeepsPrecedence() {
        const auto compiled=bytes(":/defaults/defaultLayout.json");const auto user=AppPaths::layoutsDirectory()+"/defaultLayout.json";
        QVERIFY(QDir().mkpath(QFileInfo(user).absolutePath()));QString error;const auto layout=read(":/defaults/defaultLayout.json");
        QVERIFY(LayoutDocument::write(user,layout,error));QCOMPARE(AppPaths::defaultLayout(),user);
        MainWindow window;window.findChild<QAction*>("actionLoad_Default")->trigger();
        storedStates(layout,window.findChild<MainController*>()->captureLayout());
        QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());QCOMPARE(bytes(":/defaults/defaultLayout.json"),compiled);
    }
};
QTEST_MAIN(BundledContentTests)
#include "bundled_content_tests.moc"
