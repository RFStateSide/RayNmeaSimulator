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
#include "helper.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>

namespace {
QString child(const QString &root, const QString &name)
{
    return root.isEmpty() ? QString() : QDir(root).filePath(name);
}
QStringList directories(QStringList paths)
{
    paths.removeAll(QString());
    paths.removeDuplicates();
    return paths;
}
}
namespace AppPaths {
QString dataDirectory()
{
#ifdef RAYNMEA_TESTING
    return TestProfile::root() + "/data/RFStateSide/RayNmeaSimulator";
#else
    if (DevelopmentProfile::active()) return TestProfile::root() + "/data/RFStateSide/RayNmeaSimulator";
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#endif
}
QString layoutsDirectory() { return child(dataDirectory(), "layouts"); }
QString routesDirectory() { return child(dataDirectory(), "routes"); }
QString themesDirectory() { return child(dataDirectory(), "stylesheets"); }
QString logsDirectory() { return child(dataDirectory(), "logs"); }
QString resourcesDirectory() { return child(dataDirectory(), "resources"); }
QString cacheDirectory()
{
#ifdef RAYNMEA_TESTING
    return TestProfile::root() + "/cache/RFStateSide/RayNmeaSimulator";
#else
    if (DevelopmentProfile::active()) return TestProfile::root() + "/cache/RFStateSide/RayNmeaSimulator";
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#endif
}
QString legacyDirectory() { return helper::HomePath() + "/.raynmeasimulator"; }
QStringList layoutDirectories()
{
    return directories({layoutsDirectory(), legacyDirectory() + "/layouts"});
}
QFileInfoList layoutFiles()
{
    QFileInfoList files;
    for (const auto &path : layoutDirectories()) {
        const QFileInfo info(path);
        if (info.exists() && (!info.isDir() || !info.isReadable())) {
            qWarning().noquote() << "Cannot read Layout directory:" << QDir::toNativeSeparators(path);
            continue;
        }
        files.append(QDir(path).entryInfoList({"*.json"}, QDir::Files, QDir::Name | QDir::IgnoreCase));
    }
    return files;
}
QString defaultLayout(QString *error)
{
    if (error) error->clear();
    // Do not silently switch to a different Default when a higher-priority directory is inaccessible.
    for (const auto &directory : layoutDirectories()) {
        const QFileInfo info(directory);
        if (info.exists() && (!info.isDir() || !info.isReadable())) {
            const auto message = QString("Cannot read Layout directory: %1. Check the path and permissions.")
                                     .arg(QDir::toNativeSeparators(directory));
            if (error) *error = message;
            qWarning().noquote() << message;
            return {};
        }
        // Save as Default writes this canonical name; it must take precedence over aliases in current data.
        const auto canonical = QDir(directory).filePath("defaultLayout.json");
        if (directory == layoutsDirectory() && QFileInfo(canonical).isFile()) return canonical;
        const auto files = QDir(directory).entryInfoList({"*.json"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
        static const QRegularExpression pattern("^(default[-_]?layout)\\.json$", QRegularExpression::CaseInsensitiveOption);
        for (const auto &file : files)
            if (pattern.match(file.fileName()).hasMatch()) return file.absoluteFilePath();
    }
    return ":/defaults/defaultLayout.json";
}
bool ensureDirectory(const QString &directory, QString *error)
{
    if (error) error->clear();
    const QFileInfo before(directory);
    if (directory.isEmpty() || !QDir::isAbsolutePath(directory)
        || (before.exists() && !before.isDir())
        || (!before.exists() && !QDir().mkpath(directory)) || !QFileInfo(directory).isWritable()) {
        const auto message = QString("Cannot use application data directory: %1. Check the path and permissions.")
                                 .arg(QDir::toNativeSeparators(directory));
        if (error) *error = message;
        qWarning().noquote() << message;
        return false;
    }
    return true;
}
QString layoutSavePath(const QString &name, QString *error)
{
    if (error) error->clear();
    static const QRegularExpression invalid(R"([<>:"/\\|?*\x00-\x1f.])");
    if (name.trimmed().isEmpty() || invalid.match(name).hasMatch()) {
        if (error) *error = "Cannot save Layout: enter a filename without an extension or path separators.";
        return {};
    }
    const auto directory = layoutsDirectory();
    if (!ensureDirectory(directory, error)) return {};
    return QDir(directory).filePath(name + ".json");
}
QStringList themeDirectories()
{
    return directories({themesDirectory(), child(dataDirectory(), "themes"),
        legacyDirectory() + "/Themes", legacyDirectory() + "/stylesheets",
        QCoreApplication::applicationDirPath() + "/Themes",
        QCoreApplication::applicationDirPath() + "/stylesheets"});
}
bool copyResourceIfMissing(const QString &source, const QString &target)
{
    // QFile::copy refuses to replace an existing target, including a concurrent
    // creator. A broken symlink is also user content, never an initialization slot.
    if (QFileInfo::exists(target) || QFileInfo(target).isSymLink()) return true;
    if (!ensureDirectory(QFileInfo(target).absolutePath())) return false;
    if (!QFile::copy(source, target)) {
        qWarning().noquote() << "Cannot initialize editable resource:" << target
                            << "Check directory permissions; existing files are never replaced.";
        return false;
    }
    QFile file(target);
    if (!file.setPermissions(file.permissions() | QFileDevice::WriteOwner)) {
        qWarning().noquote() << "Cannot make initialized resource writable:" << target;
        return false;
    }
    return true;
}
QStringList routeDirectories()
{
    return directories({child(routesDirectory(), "gpx"), child(routesDirectory(), "kml"),
        routesDirectory(), legacyDirectory() + "/routes/gpx", legacyDirectory() + "/routes/kml",
        legacyDirectory() + "/routes",
        QCoreApplication::applicationDirPath() + "/routes",
        QCoreApplication::applicationDirPath() + "/routes/gpx",
        QCoreApplication::applicationDirPath() + "/routes/kml"});
}
QList<QUrl> routeSidebarUrls()
{
    // QFileDialog restores shared QtProject shortcuts, including locations
    // added by other applications or old test profiles. Do not append to them.
    QList<QUrl> urls{QUrl("file:")}; // Computer: includes external drives.
    QSet<QString> seen;
    const auto add = [&](const QString &path) {
        const QFileInfo info(path);
        if (!info.isDir() || !info.isReadable()) return;
        const QString canonical = info.canonicalFilePath();
#ifdef Q_OS_WIN
        const QString key = canonical.toCaseFolded();
#else
        const QString key = canonical;
#endif
        if (canonical.isEmpty() || seen.contains(key)) return;
        seen.insert(key);
        urls.append(QUrl::fromLocalFile(canonical));
    };
    add(QDir::homePath());
    add(routesDirectory()); // Contains the user's gpx/ and kml/ folders.
    const QFileInfo legacyRoutes(legacyDirectory() + "/routes");
    if (legacyRoutes.isDir() && legacyRoutes.canonicalFilePath() != QFileInfo(routesDirectory()).canonicalFilePath())
        add(legacyDirectory()); // A distinct .raynmeasimulator label, not another "routes".
    return urls;
}
void initializeRouteExamples()
{
    QDirIterator files(":/routes", QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const auto source = files.next();
        const auto relative = source.mid(QString(":/routes/").size());
        // Retain historical examples/customizations at their original paths.
        if (QFileInfo::exists(legacyDirectory() + "/routes/" + relative)) continue;
        copyResourceIfMissing(source, routesDirectory() + "/" + relative);
    }
}
QString routeDialogDirectory(const QString &selectedFile)
{
    if (!selectedFile.isEmpty() && !selectedFile.startsWith(":/") && QFileInfo(selectedFile).isFile())
        return QFileInfo(selectedFile).absolutePath();
    for (const auto &directory : routeDirectories())
        if (QFileInfo(directory).isDir() && QFileInfo(directory).isReadable()) return directory;
    return routesDirectory();
}
}
