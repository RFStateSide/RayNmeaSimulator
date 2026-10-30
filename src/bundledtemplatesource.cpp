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

#include "bundledtemplatesource.h"
#ifdef QT_DEBUG
#include "appsettings.h"
#include "layoutdocument.h"
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>

namespace BundledTemplateSource {
static bool hasCatalogMarker(const QByteArray &contents)
{
    const QByteArray marker="# RayNmeaSimulator bundled Template catalog (parallel CMake authoring).";
    // Legal notices and blank lines may precede this standalone catalog comment.
    for (const auto &line : contents.split('\n'))
        if (line.trimmed() == marker) return true;
    return false;
}
QString project()
{
    AppSettings settings("RFStateSide", "RayNmeaSimulator");
#ifdef RAYNMEA_TESTING
    return settings.value("Authoring/TemplateProject").toString(); // Tests must explicitly choose a disposable project.
#else
    return settings.value("Authoring/TemplateProject", QString::fromUtf8(RAYNMEA_TEMPLATE_PROJECT_ROOT)).toString();
#endif
}
QString directory(const QString &root, QString &error)
{
    const QFileInfo info(root);
    if (!QDir::isAbsolutePath(root) || !info.isDir() || info.isSymLink()
        || info.canonicalFilePath() != QDir::cleanPath(info.absoluteFilePath())) {
        error = "Choose the physical RayNmeaSimulator project folder."; return {};
    }
    const auto path = QDir(root).filePath("resources/templates");
    auto validProject = [&](const QString &catalogName, const QString &projectName, const QByteArray &reference) {
        QFile catalog(path + "/" + catalogName), projectFile(root + "/" + projectName);
        return !QFileInfo(catalog.fileName()).isSymLink() && !QFileInfo(projectFile.fileName()).isSymLink()
            && catalog.open(QIODevice::ReadOnly) && catalog.size() <= 65536
            && hasCatalogMarker(catalog.read(65536))
            && projectFile.open(QIODevice::ReadOnly) && projectFile.size() <= 1024*1024
            && projectFile.readAll().contains(reference);
    };
    if (QFileInfo(root + "/resources").isSymLink() || QFileInfo(path).isSymLink()
        || !QFileInfo(path+"/catalog.txt").isFile() || QFileInfo(path+"/catalog.txt").isSymLink()
        || !validProject("templates.cmake", "CMakeLists.txt", "resources/templates/templates.cmake")) {
        error = "This folder does not contain the bundled Template source catalog. Choose the updated project folder.";
        return {};
    }
    return QDir::cleanPath(path);
}
QStringList files(const QString &root, QString &error)
{
    const auto dir = directory(root,error); QStringList paths;
    if (dir.isEmpty()) return paths;
    for (const auto &file : QDir(dir).entryList({"*.json"},QDir::Files,QDir::Name))
        paths << QDir(dir).filePath(file);
    return paths;
}
bool contains(const QString &root, const QString &path)
{
    QString error; const auto dir=directory(root,error); const QFileInfo file(path);
    return !dir.isEmpty() && !file.isSymLink() && file.suffix()=="json"
        && file.absoluteDir().canonicalPath()==QFileInfo(dir).canonicalFilePath();
}
QString savePath(const QString &root, const QString &name, QString &error)
{
    const auto dir=directory(root,error);
    return dir.isEmpty() ? QString() : LayoutDocument::savePathInDirectory(name,dir,error);
}
static bool backup(const QString &root, const QString &path, QString &error)
{
    if (!QFileInfo::exists(path)) return true;
    const auto dir=root+"/build/template-authoring-backups";
    if (QFileInfo(root+"/build").isSymLink() || QFileInfo(dir).isSymLink() || !QDir().mkpath(dir)) {
        error="Cannot create a local source backup; original retained."; return false;
    }
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly) || input.size()>4*1024*1024) {
        error="Cannot back up this source file; original retained."; return false;
    }
    const auto bytes=input.readAll(); input.close();
    QSaveFile output(dir+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"
                     +QUuid::createUuid().toString(QUuid::WithoutBraces)+"-"+QFileInfo(path).fileName());
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit()) {
        error="Cannot save a source backup; original retained."; return false;
    }
    return true;
}
bool write(const QString &root, const QString &path, const QJsonObject &document, QString &error)
{
    if (!contains(root,path)) {error="Destination is outside the selected bundled source catalog.";return false;}
    if (!backup(root,path,error)) return false;
    return LayoutDocument::write(path,document,error);
}
bool rename(const QString &root, const QString &path, const QString &name, QString &error)
{
    if (!contains(root,path)) {error="Not an editable bundled source file.";return false;}
    const auto target=savePath(root,name,error); if(target.isEmpty())return false;
    QJsonObject document;
    if (!LayoutDocument::read(path,document,error)) return false;
    auto metadata=document["template"].toObject();metadata["name"]=name;document["template"]=metadata;
    if(target==path)return write(root,path,document,error); // Display-name/case-only rename.
    if(QFileInfo::exists(target)){error="A source file already has this name.";return false;}
    if(!backup(root,path,error) || !LayoutDocument::write(target,document,error))return false;
    if(QFile::remove(path))return true;
    // Retain the old source if removal fails; undo only our newly created copy.
    QFile::remove(target);error="Original source could not be removed; rename did not complete.";return false;
}
bool remove(const QString &root, const QString &path, QString &error)
{
    if(!contains(root,path)){error="Not an editable bundled source file.";return false;}
    if(!backup(root,path,error))return false;
    if(QFile::remove(path))return true;
    error="Source could not be deleted; original retained.";return false;
}
}
#endif
