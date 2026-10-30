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

// RF StateSide internal adaptation of BrickSuite HelpManager (1e2b8f870200b634806ac37dc3629049162a7915).
#include "helpcatalog.h"
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextDocument>

namespace {
struct Catalog { QList<HelpTopicInfo> topics; QSet<QString> images; };
const Catalog &catalog() {
    static const Catalog value=[] {
        Catalog result;
        QFile file(":/help/topics.json");
        if (!file.open(QIODevice::ReadOnly)) return result;
        const auto document=QJsonDocument::fromJson(file.readAll());
        if (document.object().value("version").toInt()!=1) return result;
        QSet<QString> ids, groups;
        const QRegularExpression idPattern("^[a-z][a-z0-9-]*$");
        for (const auto &groupValue:document.object().value("groups").toArray()) {
            const auto group=groupValue.toObject();
            const auto groupId=group.value("id").toString();
            if (!idPattern.match(groupId).hasMatch() || groups.contains(groupId)
                || group.value("title").toString().isEmpty()) return Catalog{};
            groups.insert(groupId);
            for (const auto &topicValue:group.value("topics").toArray()) {
                const auto topic=topicValue.toObject();
                const auto id=topic.value("id").toString(), name=topic.value("title").toString();
                if (!idPattern.match(id).hasMatch() || ids.contains(id) || name.isEmpty()
                    || topic.value("file").toString()!=id+".html") return Catalog{};
                ids.insert(id);
                result.topics.append({id,name,group.value("title").toString(),":/help/"+id+".html"});
            }
        }
        for (const auto &image:document.object().value("images").toArray()) {
            const auto path=image.toString();
            if (!path.startsWith("images/") || path.contains("..") || path.contains('\\')
                || !QRegularExpression("^images/[a-zA-Z0-9_./-]+$").match(path).hasMatch()) return Catalog{};
            result.images.insert("/help/"+path);
        }
        return result;
    }();
    return value;
}
class PlainDocument final:public QTextDocument {
    QVariant loadResource(int,const QUrl &) override { return {}; }
};
}
const QList<HelpTopicInfo> &HelpCatalog::topics() { return catalog().topics; }
const HelpTopicInfo *HelpCatalog::topic(const QString &id) {
    for (const auto &info:topics()) if (info.id==id) return &info;
    return nullptr;
}
QString HelpCatalog::article(const QString &id) {
    const auto *info=topic(id);
    if (!info) return {};
    QFile file(info->resource);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString{};
}
QStringList HelpCatalog::search(const QString &query) {
    static const QHash<QString,QString> text=[] {
        QHash<QString,QString> index;
        for (const auto &info:topics()) {
            PlainDocument document; document.setHtml(article(info.id));
            index.insert(info.id,info.title+'\n'+document.toPlainText());
        }
        return index;
    }();
    QStringList matches;
    for (const auto &info:topics())
        if (text.value(info.id).contains(query.trimmed(),Qt::CaseInsensitive)) matches.append(info.id);
    return matches;
}
QUrl HelpCatalog::resolve(const QUrl &url,const QUrl &base) {
    if (url.isEmpty() || !url.isValid() || url.path(QUrl::FullyDecoded).split('/').contains("..")
        || url.path().contains('\\')) return {};
    const auto resolved=base.resolved(url);
    if (resolved.scheme()!="qrc" || !resolved.host().isEmpty() || !resolved.userInfo().isEmpty()
        || resolved.hasQuery()) return {};
    return resolved;
}
QUrl HelpCatalog::topicUrl(const QString &id) { return topic(id) ? QUrl("qrc:/help/"+id+".html") : QUrl{}; }
QString HelpCatalog::topicForUrl(const QUrl &url) {
    const auto resolved=resolve(url);
    if (resolved.isEmpty()) return {};
    for (const auto &info:topics()) if (resolved.path()=="/help/"+info.id+".html") return info.id;
    return {};
}
bool HelpCatalog::isImage(const QUrl &url) {
    const auto resolved=resolve(url);
    return !resolved.hasFragment() && catalog().images.contains(resolved.path())
        && QFile::exists(':'+resolved.path());
}
