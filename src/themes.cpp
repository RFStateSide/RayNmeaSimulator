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

#include "themes.h"
#include "apppaths.h"
#include "helper.h"
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStyle>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QSaveFile>
#include <QStyleFactory>
#include <QStyleHints>
#include <QWidget>

namespace {
QString nativeStyle;
QJsonObject catalog()
{
    QFile f(":/stylesheets/palettes.json");
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
QJsonObject colors(const QString &name) { return catalog()["themes"].toObject()[name].toObject(); }

bool validName(const QString& name)
{
    return !name.isEmpty() && name != "." && name != ".."
        && !name.contains('/') && !name.contains('\\') && !name.contains(':');
}
QString inDirectory(const QString& root, const QString& name)
{
    for (const QString& relative : {name + ".qss", name + "/" + name + ".qss"}) {
        const QString path = QDir(root).filePath(relative);
        if (QFileInfo::exists(path) || QFileInfo(path).isSymLink()) return path;
    }
    // Some original themes have a differently named QSS (ManjaroMin). A
    // single QSS is unambiguous; multiple nonmatching files require a named one.
    const auto files = QDir(QDir(root).filePath(name)).entryList({"*.qss"}, QDir::Files);
    if (files.size() == 1) return QDir(root).filePath(name + "/" + files.first());
    return {};
}
QString read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}
bool usable(const QString &path)
{
    QString text = read(path);
    text.remove(QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption));
    text.remove(QRegularExpression(R"qss("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')qss"));
    int depth = 0;
    for (const auto c : text) {
        if (c == '{') ++depth;
        if (c == '}' && --depth < 0) return false;
    }
    return depth == 0 && (text.contains('{') || text.contains("@COMMON@"));
}

}

void Themes::initializeEditableResources()
{
    const auto roots = externalDirectories();
    for (const auto &name : builtInThemes()) {
        if (isSystem(name)) continue;
        const auto folder=name=="Darkeum"?QString("Darkeum-Refined"):name;
        // An earlier external version owns this theme. Do not create a second
        // active copy ahead of it, including incomplete or unreadable overrides.
        bool legacy = false;
        for (const auto &root : roots.mid(1))
            if (QFileInfo::exists(QDir(root).filePath(folder))
                || QFileInfo::exists(QDir(root).filePath(folder + ".qss"))) legacy = true;
        if (legacy) continue;
        const auto source = ":/stylesheets/" + folder;
        QDirIterator files(source, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const auto file = files.next();
            const auto target=AppPaths::themesDirectory()+"/"+folder+"/"+file.mid(source.size()+1);
            // Upgrade only byte-identical former stock seeds. Owner edits/assets
            // are never overwritten; the old hash is part of the resource catalog.
            const auto previous=catalog()["previousSeeds"].toObject()[folder].toString();
            if (!previous.isEmpty() && file.endsWith("/"+folder+".qss")
                && QFile::exists(target)
                && QCryptographicHash::hash(read(target).replace("\r\n","\n").toUtf8(),QCryptographicHash::Sha256).toHex()==previous.toLatin1()) {
                QSaveFile output(target);output.setDirectWriteFallback(false);
                const auto bytes=read(file).toUtf8();
                if (!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit())
                    qWarning("Unable to refresh an unchanged stock theme; existing file preserved.");
            } else AppPaths::copyResourceIfMissing(file,target);
        }
    }
}

QStringList Themes::externalDirectories()
{
    return AppPaths::themeDirectories();
}

QString Themes::source(const QString& name)
{
    if (isSystem(name) || !validName(name)) return {};
    const auto storage=name=="Darkeum"?QString("Darkeum-Refined"):name;
    for (const auto& root : externalDirectories()) {
        const auto path = inDirectory(root, storage);
        if (!path.isEmpty()) return path;
    }
    return inDirectory(":/stylesheets", storage);
}

bool Themes::isSystem(const QString &name) { return name=="System" || name=="None"; }

void Themes::initializeSystemStyle()
{
    if (nativeStyle.isEmpty()) nativeStyle=QApplication::style()->objectName();
}
void Themes::apply(const QString &name)
{
    initializeSystemStyle();
    const auto wanted=isSystem(name)?nativeStyle:QString("fusion");
    if (QApplication::style()->objectName().compare(wanted,Qt::CaseInsensitive)!=0)
        if (auto *style=QStyleFactory::create(wanted)) QApplication::setStyle(style);
    if (isSystem(name)) {
        // An unresolved palette relinquishes explicit application colors, allowing
        // Qt's platform theme to supply and subsequently update them.
        QGuiApplication::styleHints()->unsetColorScheme();
        QPalette system;system.setResolveMask(0);QApplication::setPalette(system);
        // Stylesheet unpolish can leave a resolved palette on descendants. No
        // application widgets own semantic palettes; relinquish those cached
        // overrides too, so docks/toolbars/custom controls inherit the platform.
        for(auto *widget:QApplication::allWidgets()) widget->setPalette(system);
    } else QApplication::setPalette(palette(name));
}
QStringList Themes::builtInThemes()
{
    return {"System","Light","Dark","Darkeum","Teal Dark","Porcelain"};
}
QStringList Themes::available()
{
    QStringList names=builtInThemes(), additional;
    // Built-in curation never restricts external extensibility. Even files
    // named after retired themes remain available; user files are never deleted.
    for (const auto &root:externalDirectories()) {
        const QDir directory(root);
        for (const auto &file:directory.entryInfoList({"*.qss"},QDir::Files)) {
            const auto name=file.completeBaseName();
            if (usable(file.absoluteFilePath())) additional << name;
        }
        for (const auto &folder:directory.entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
            const auto name=folder=="Darkeum-Refined"?QString("Darkeum"):folder;
            const auto path=inDirectory(root,folder);
            if (usable(path)) additional << name;
        }
    }
    additional.removeDuplicates();additional.sort(Qt::CaseInsensitive);
    for (const auto &name:additional)
        if (!isSystem(name) && !names.contains(name)) names << name;
    return names;
}
QString Themes::selectionName(const QString &name)
{
    if (isSystem(name)) return "System";
    return usable(source(name))?name:QString("Dark");
}

QString Themes::styleSheet(const QString& name)
{
    if (isSystem(name)) return {};
    QString effective=name;
    QString path = source(name);
    if (path.isEmpty()) {
        qWarning("Selected theme unavailable; using bundled Dark theme.");
        effective="Dark";
        path = ":/stylesheets/Dark/Dark.qss";
    }
    QString text = read(path);
    if (!usable(path)) {
        qWarning().noquote() << "Selected theme invalid, empty or unreadable:" << path
                            << "; using bundled Dark theme. Check QSS syntax and permissions.";
        effective="Dark";
        path = ":/stylesheets/Dark/Dark.qss";
        text = read(path);
    }
    if (text.contains("@COMMON@")) {
        auto tokens=colors(effective);
        if (tokens.isEmpty()) tokens=colors("Dark"); // Missing/invalid candidate uses Dark.
        QString shared=read(":/stylesheets/Common.qss");
        for (auto it=tokens.begin();it!=tokens.end();++it) shared.replace("@"+it.key()+"@",it.value().toString());
        text.replace("@COMMON@",shared);
    }
    const QString directory = QFileInfo(path).absolutePath();
    const bool embedded = path.startsWith(":/");
    // Support historical themes, without corrupting http:// or file:// URLs.
    // Historical placeholders point into the selected theme's own directory,
    // including user edits to its images, rather than the executable directory.
    text.replace(QRegularExpression("%%APP_FOLDER%%/stylesheets/[^/]+/"), directory + "/");
    text.replace("%%APP_FOLDER%%", embedded ? ":" : directory);
    text.replace("/usr/share/icons/Adwaita/16x16/actions/", directory + "/QSS_IMG/");
    text.replace("url(://", "url(:/");
    // Qt resolves relative URLs against the process working directory. Resolve them
    // against the selected QSS instead, so user themes work from any shadow build.
    const QRegularExpression urls(R"qss(url\(\s*["']?([^\)"']+)["']?\s*\))qss");
    auto matches = urls.globalMatch(text);
    QList<QRegularExpressionMatch> replacements;
    while (matches.hasNext()) replacements << matches.next();
    for (auto it = replacements.crbegin(); it != replacements.crend(); ++it) {
        const QString value = it->captured(1).trimmed();
        if (value.startsWith(":/") || QDir::isAbsolutePath(value) || !QUrl(value).scheme().isEmpty()) continue;
        const QString resolved = QDir(directory).filePath(value);
        text.replace(it->capturedStart(), it->capturedLength(), "url(\"" + resolved + "\")");
    }
    return text;
}

QPalette Themes::palette(const QString& name)
{
    QPalette result = QApplication::style()->standardPalette();
    if (isSystem(name)) { result.setResolveMask(0); return result; }
    // Legacy and external QSS still control their own colors. A coherent base
    // palette supplies native Fusion arrows, indicators and inactive selections.
    const bool dark = name == "Dark" || source(name).isEmpty()
        || styleSheet(name).contains("isDarkTheme=true", Qt::CaseInsensitive);
    const bool fallback=source(name).isEmpty() || !usable(source(name));
    const auto tokens=colors(fallback?QString("Dark"):name);
    const bool candidate=!colors(name).isEmpty() || fallback;
    auto color=[&](const char *key,const QColor &fallback){return candidate?QColor(tokens[QLatin1String(key)].toString()):fallback;};
    const QColor window(dark ? "#20262e" : "#f2f5f8"), base(dark ? "#171d24" : "#ffffff");
    const QColor text(dark ? "#edf2f7" : "#182633"), disabled(dark ? "#99a5b2" : "#647180");
    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        result.setColor(group, QPalette::Window, color("window",window));
        result.setColor(group, QPalette::Base, color("base",base));
        result.setColor(group, QPalette::AlternateBase, color("alternate",dark ? QColor("#29323c") : QColor("#eaf0f5")));
        result.setColor(group, QPalette::Button, color("button",dark ? QColor("#303b47") : QColor("#e7edf3")));
        for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
            result.setColor(group, role, group == QPalette::Disabled ? color("disabled",disabled) : color("text",text));
        result.setColor(group, QPalette::PlaceholderText, color("disabled",disabled));
        result.setColor(group, QPalette::Highlight, color("accent",QColor("#2168a6")));
        result.setColor(group, QPalette::HighlightedText, Qt::white);
        result.setColor(group, QPalette::ToolTipBase, color("base",base));
        result.setColor(group, QPalette::ToolTipText, color("text",text));
        result.setColor(group, QPalette::Link, color("link",dark ? QColor("#8fc9ff") : QColor("#075b9e")));
        result.setColor(group, QPalette::Accent, color("accent",QColor("#2168a6")));
    }
    return result;
}
