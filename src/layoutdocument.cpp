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

#include "layoutdocument.h"
#include "apppaths.h"
#include "aisclassbstatic129810.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>

namespace LayoutDocument {
QString mapping(const QString &pgn,const QString &field) {
    if ((pgn=="129809" || pgn=="129810" || pgn=="129039") && field=="UserID") return "MMSI";
    if (pgn!="129810") return {};
    if (field=="VesselType" || field=="Length" || field=="Beam") return field;
    return field=="Callsign" ? "RadioCallsign" : QString();
}
bool read(const QString &path,QJsonObject &root,QString &error) {
    QFile file(path); QJsonParseError parse;
    if(!file.open(QIODevice::ReadOnly) || file.size()>4*1024*1024) {error="Document is missing, unreadable or exceeds 4 MiB.";return false;}
    const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);
    if(parse.error!=QJsonParseError::NoError || !doc.isObject()) {error="Invalid document JSON.";return false;}
    root=doc.object();return true;
}
bool write(const QString &path,const QJsonObject &root,QString &error) {
    if(path.startsWith(":/") || QFileInfo(path).isSymLink()) {error="Read-only or linked destination.";return false;}
    QSaveFile file(path);const auto bytes=QJsonDocument(root).toJson();
    if(bytes.size()>4*1024*1024) {error="Document exceeds 4 MiB.";return false;}
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {error="Save failed: "+file.errorString();return false;}
    return true;
}
bool resolve(const QJsonObject &source,QSettings &settings,QJsonObject &layout,QString &error) {
    auto fail=[&](const QString &message){error=message;return false;};
    if(source["formatVersion"]!=2) return fail("Unsupported development format. Recreate this document with format 2.");
    if(source["documentType"]=="layout") {layout=source;return true;}
    if(source["documentType"]!="template") return fail("Unknown document type.");
    const auto meta=source["template"].toObject();
    if(meta["id"].toString().isEmpty() || meta["id"].toString().size()>120 || meta["name"].toString().trimmed().isEmpty()
        || meta["name"].toString().size()>120 || meta["description"].toString().size()>4096)
        return fail("Template needs a bounded name and stable identity.");
    if(!source["setupPolicies"].isArray() || source["setupPolicies"].toArray().size()>256)
        return fail("Invalid or excessive Setup policies.");
    auto groups=source["layouts"].toArray(); QSet<QString> seen;
    for(const auto &entry:source["setupPolicies"].toArray()) {
        const auto policy=entry.toObject();
        if(!policy["widgetId"].isDouble() || policy["widgetId"].toDouble()!=policy["widgetId"].toInt()) return fail("Invalid policy widget ID.");
        const int id=policy["widgetId"].toInt(); const auto field=policy["field"].toString();
        const QString key=QString::number(id)+":"+field;
        if(seen.contains(key)) return fail("Duplicate Setup policy."); seen.insert(key);
        bool found=false;
        for(int g=0;g<groups.size();++g) {
            auto group=groups[g].toObject();auto widgets=group["widgets"].toArray();
            for(int w=0;w<widgets.size();++w) {
                auto widget=widgets[w].toObject();if(widget["idX"].toInt()!=id)continue;
                const auto setting=mapping(widget["pgn"].toString(),field);
                if(setting.isEmpty() || policy["setupKey"]!=setting) return fail("Unsupported Setup mapping: "+field);
                if(!settings.contains("Vessel/"+setting)) return fail("Missing Vessel Setup value: "+setting);
                const auto raw=settings.value("Vessel/"+setting);QString value=raw.toString();
                if(setting=="Length" || setting=="Beam") {
                    bool ok=false;const double n=raw.toDouble(&ok);
                    if(!ok || !std::isfinite(n))return fail("Malformed Vessel Setup value: "+setting);
                    value=QString::number(n,'g',17);
                }
                // Explicit 0/NA/empty values accepted by the family are intentional
                // unconfigured values; malformed input never falls back to saved data.
                if(!AisClassBStatic129810::editableField(field,value)) return fail("Invalid or incompatible Vessel Setup value: "+setting);
                auto rows=widget["parameters"].toArray();
                for(int p=0;p<rows.size();++p) {
                    auto row=rows[p].toArray();if(row.size()!=4 || row[0]!=field)continue;
                    row[1]=value;rows[p]=row;found=true;
                }
                widget["parameters"]=rows;widgets[w]=widget;
            }
            group["widgets"]=widgets;groups[g]=group;
        }
        if(!found)return fail("Setup policy references a missing field.");
    }
    layout=source;layout["documentType"]="layout";layout["layouts"]=groups;
    layout.remove("template");layout.remove("setupPolicies");return true;
}
QString directory() {return AppPaths::dataDirectory()+"/templates";}
QString savePath(const QString &name,QString &error) {
    return savePathInDirectory(name,directory(),error);
}
QString savePathInDirectory(const QString &name,const QString &targetDirectory,QString &error) {
    static const QRegularExpression invalid(R"([.<>:"/\\|?*\x00-\x1f])");
    static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$",QRegularExpression::CaseInsensitiveOption);
    if(name.isEmpty() || name.size()>120 || name!=name.trimmed() || name.contains(invalid) || reserved.match(name).hasMatch()) {error="Choose a portable name without an extension or path.";return {};}
    if(QFileInfo(targetDirectory).isSymLink() || !QDir().mkpath(targetDirectory)) {error="Template directory is unavailable.";return {};}
    for(const auto &file:QDir(targetDirectory).entryList({"*.json"},QDir::Files|QDir::System))
        if(file.compare(name+".json",Qt::CaseInsensitive)==0)return QDir(targetDirectory).filePath(file);
    return QDir(targetDirectory).filePath(name+".json");
}
QStringList templates() {
    QStringList paths;
    for(const auto &file:QDir(":/templates").entryList({"*.json"},QDir::Files,QDir::Name))paths<<":/templates/"+file;
    for(const auto &file:QDir(directory()).entryList({"*.json"},QDir::Files,QDir::Name))paths<<directory()+"/"+file;
    return paths;
}
QString summary(const QJsonObject &root) {
    int widgets=0,generators=0;
    for(const auto &g:root["layouts"].toArray())for(const auto &w:g.toObject()["widgets"].toArray()) {++widgets;if(w.toObject()["type"].toString().startsWith("Generator"))++generators;}
    return QString("%1\n\n%2 widgets; %3 generator sources; %4 Setup defaults.\nLoading never authorizes external Sending.")
        .arg(root["template"].toObject()["description"].toString()).arg(widgets).arg(generators).arg(root["setupPolicies"].toArray().size());
}
}
