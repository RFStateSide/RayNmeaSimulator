// Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
// Build-time verifier: the very same resource objects linked into the application.
// Never installed. No GUI, settings, profiles, credentials or network access.
#include <QtCore/QtCore>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QMap<QString,QString> expected {
        {"Default.json", "Default"}, {"environment-weather.json", "Environment / Weather"},
        {"navigation-autopilot.json", "Navigation / Autopilot"}, {"single-engine.json", "Single Engine"},
        {"synthetic-ais.json", "Synthetic AIS Demonstration"}, {"twin-engine.json", "Twin Engine"}
    };
    auto fail=[](const QString &error) { std::fprintf(stderr,"Bundled content: %s\n",qPrintable(error));return 1; };
    if (QDir(":/templates").entryList({"*.json"},QDir::Files,QDir::Name) != expected.keys())
        return fail("Expected exactly the six owner-approved compiled Templates");
    const bool sources = !app.arguments().contains("--resources-only");
    auto inspect=[&](const QString &relative, const QString &resource, QJsonObject &entry) {
        QFile compiled(resource);
        if (!compiled.open(QIODevice::ReadOnly)) return false;
        const auto bytes=compiled.readAll();
        const auto document=QJsonDocument::fromJson(bytes);
        if (!document.isObject()) return false;
        if (sources) {
            QFile source(QString::fromUtf8(RAYNMEA_CONTENT_SOURCE_ROOT)+"/"+relative);
            if (!source.open(QIODevice::ReadOnly) || source.readAll()!=bytes) return false;
        }
        compiled.close();
        if (compiled.open(QIODevice::ReadWrite)) return false;
        entry={{"source",relative},{"resource",resource},
               {"sha256",QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())}};
        return true;
    };
    QJsonArray templates;
    QSet<QString> ids;
    for (auto it=expected.cbegin();it!=expected.cend();++it) {
        const auto resource=":/templates/"+it.key(); QJsonObject entry;
        if (!inspect("resources/templates/"+it.key(),resource,entry)) return fail("Resource/source mismatch or writable resource: "+resource);
        QFile file(resource);if(!file.open(QIODevice::ReadOnly))return fail("Cannot reopen "+resource);
        const auto root=QJsonDocument::fromJson(file.readAll()).object();
        const auto meta=root["template"].toObject();const auto id=meta["id"].toString();
        if (root["documentType"]!="template" || root["formatVersion"]!=2 || meta["name"]!=it.value()
            || meta["description"].toString().isEmpty() || id.isEmpty() || ids.contains(id))
            return fail("Invalid owner-approved Template metadata: "+resource);
        ids.insert(id);entry["name"]=it.value();entry["description"]=meta["description"];entry["id"]=id;templates.append(entry);
    }
    QJsonObject layout;
    if (!inspect("resources/defaults/defaultLayout.json",":/defaults/defaultLayout.json",layout))
        return fail("Default Layout differs from its source or is not read-only");
    QFile file(":/defaults/defaultLayout.json");if(!file.open(QIODevice::ReadOnly))return fail("Cannot reopen Default Layout");
    const auto root=QJsonDocument::fromJson(file.readAll()).object();
    if (root["documentType"]!="layout" || root["formatVersion"]!=2 || !root["layouts"].isArray())
        return fail("Default must be a concrete format-2 Layout, not a Template");
    layout["groups"]=root["layouts"].toArray().size();
    int widgets=0;for(const auto &g:root["layouts"].toArray())widgets+=g.toObject()["widgets"].toArray().size();
    layout["widgets"]=widgets;
    const QJsonObject result{{"templates",templates},{"default_layout",layout},{"source_bytes_verified",sources}};
    std::puts(QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
    return 0;
}
