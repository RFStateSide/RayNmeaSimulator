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

// Build a public test layout from controller defaults, without owner files/settings.
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QSettings>
#include <QTemporaryDir>
#include "maincontroller.h"
#include "helper.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 2) return 2;
    QTemporaryDir profile;
    if (!profile.isValid()) return 3;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    QMdiArea area;
    QTableView table;
    QTreeView tree;
    PgnTableController tables(&table);
    PgnTreeController trees(&tree);
    MainController controller(&tables,&trees,&area,nullptr);
    MyVessel vessel;
    vessel.Latitude=0; vessel.Longitude=0;
    controller.setMyVessel(vessel);
    controller.setUserPrefs(UserUnitPrefs{});
    controller.setUserOptionPrefs(UserOptionPrefs{});
    controller.setNumberOfWidgetWide(2);
    for (const QString group : {"Generate","Navigation","Propulsion","Environment","Tanks","Electrical","Other","Communication"})
        controller.addSubWindowForGroup(group,group);
    QTimer dialogs;
    QObject::connect(&dialogs,&QTimer::timeout,[] {
        if (auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))
            dialog->accept();
    });
    dialogs.start(10);
    struct Item { const char *group; const char *pgn; const char *label; int count; };
    const Item recipe[] = {
        {"Generate","-127489","Engine",2},
        {"Generate","-127493","Transmission",2},
        {"Generate","-129025","Vessel movement",1},
        {"Generate","-130311","Weather",1},
        {"Generate","-129033","Date and time",1},
        {"Navigation","129029","GNSS",1},
        {"Navigation","129025","Position",1},
        {"Navigation","129026","COG and SOG",1},
        {"Navigation","128259","Speed",1},
        {"Navigation","128267","Depth",1},
        {"Navigation","129284","Navigation",1},
        {"Navigation","127251","Rate of turn",1},
        {"Navigation","127257","Attitude",1},
        {"Navigation","127252","Heave",1},
        {"Navigation","129033","Local date and time",1},
        {"Propulsion","127488","Engine rapid",2},
        {"Propulsion","127489","Engine dynamic",2},
        {"Propulsion","127493","Transmission dynamic",2},
        {"Propulsion","127245","Rudder",1},
        {"Environment","130310","Outside environment",1},
        {"Environment","130316","Extended temperature",2},
        {"Tanks","127505","Tank",4},
        {"Electrical","127508","Battery",2},
        {"Electrical","127507","Charger",2}
    };
    for (const auto &item:recipe)
        for (int i=0;i<item.count;++i)
            controller.addWidgetAsComboboxTextChanged(item.pgn,
                QString("%1 %2").arg(item.label).arg(i+1),item.group);
    dialogs.stop();
    if (tables.getPgnTableModel()->getPgnList().size()!=35) return 4;
    const QString raw=profile.filePath("layout.json");
    controller.saveLayoutToJson(raw);
    QFile input(raw);
    if (!input.open(QIODevice::ReadOnly)) return 5;
    QJsonObject root=QJsonDocument::fromJson(input.readAll()).object();
    QJsonArray groups;
    // Freeze time/SID and synthetic coordinates after construction. Live generators
    // may tick in selection dialogs; the fixture must not depend on wall time.
    for (const auto &groupValue:root["layouts"].toArray()) {
        auto group=groupValue.toObject();
        QJsonArray widgets;
        for (const auto &widgetValue:group["widgets"].toArray()) {
            auto widget=widgetValue.toObject();
            QJsonArray parameters;
            for (const auto &parameterValue:widget["parameters"].toArray()) {
                auto parameter=parameterValue.toArray();
                const QString name=parameter[0].toString();
                if (name=="SID" || name=="SecondsSinceMidnight" || name=="LocalOffset"
                    || name=="Latitude" || name=="Longitude") parameter[1]="0";
                if (name=="DaysSince1970") parameter[1]="20000";
                // The creation default is the placeholder "enum" until the
                // first widget callback. Use the actual default enum value.
                if (name=="DelaySource") parameter[1]="0";
                bool numeric=false;
                const double value=parameter[1].toString().toDouble(&numeric);
                if (numeric) parameter[1]=QString::number(value,'g',15);
                parameters.append(parameter);
            }
            widget["parameters"]=parameters;
            widgets.append(widget);
        }
        group["widgets"]=widgets;
        groups.append(group);
    }
    root["layouts"]=groups;
    QFile output(QString::fromLocal8Bit(argv[1]));
    if (!output.open(QIODevice::WriteOnly)) return 6;
    const QByteArray bytes=QJsonDocument(root).toJson();
    if (output.write(bytes)!=bytes.size()) return 7;
    return 0;
}
