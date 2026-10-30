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

#include "engineprofilecatalog.h"
#include "apppaths.h"
#include "runtimeresources.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <cmath>

namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
bool read(const QString &path, QByteArray *bytes, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, QStringLiteral("Cannot read %1: %2").arg(path, file.errorString()));
    *bytes = file.readAll();
    if (file.error() != QFile::NoError) return fail(error, file.errorString());
    return true;
}
bool number(const QJsonValue &value) { return value.isDouble() && std::isfinite(value.toDouble()); }
}

QString EngineProfileCatalog::curveKey(const QJsonObject &profile) {
    return profile.contains("rpm_gph_pairs") ? QStringLiteral("rpm_gph_pairs") : QStringLiteral("rpm_gph_data");
}

bool EngineProfileCatalog::validate(const QJsonObject &catalog, QString *error) {
    if (!catalog.value("engines").isArray()) return fail(error, "Engine profiles require an engines array.");
    QSet<QString> names;
    int index = 0;
    for (const auto &entry : catalog.value("engines").toArray()) {
        ++index;
        const auto p = entry.toObject();
        const QString name = p.value("name").toString();
        const QString where = QStringLiteral("Engine %1 (%2): ").arg(index).arg(name);
        if (!entry.isObject() || !p.value("name").isString() || name.trimmed().isEmpty())
            return fail(error, where + "a nonempty name is required.");
        if (name == "None") return fail(error, where + "None is reserved for no engine selection.");
        if (names.contains(name)) return fail(error, where + "duplicate engine name.");
        names.insert(name);
        for (const auto &key : {QStringLiteral("horsepower"), QStringLiteral("alternatorVoltage")}) {
            if (!p.contains(key)) continue;
            const auto v = p.value(key);
            if (!number(v) || v.toDouble() < 0 ||
                (key == "alternatorVoltage" && (v.toDouble() != std::floor(v.toDouble()) || v.toDouble() > 1000)))
                return fail(error, where + key + " must be a finite nonnegative number (voltage: whole volts, at most 1000).");
        }
        if (p.contains("fuelType") && !p.value("fuelType").isString()) return fail(error, where + "fuelType must be text.");
        for (const auto &key : {QStringLiteral("fuelInjected"), QStringLiteral("forcedInduction")})
            if (p.contains(key) && !p.value(key).isBool()) return fail(error, where + key + " must be true or false.");
        const QString key = curveKey(p);
        if (!p.value(key).isArray() || p.value(key).toArray().size() < 2)
            return fail(error, where + "fuel curve requires at least two RPM / US GPH pairs.");
        double previous = 0;
        int row = 0;
        for (const auto &point : p.value(key).toArray()) {
            ++row;
            const auto pair = point.toObject();
            const auto rpm = pair.value("rpm"), gph = pair.value("gph");
            if (!point.isObject() || !number(rpm) || rpm.toDouble() <= 0 || rpm.toDouble() > 100000 ||
                rpm.toDouble() != std::floor(rpm.toDouble()) || rpm.toDouble() <= previous)
                return fail(error, where + QStringLiteral("curve row %1: RPM must be unique, increasing whole numbers from 1 to 100000.").arg(row));
            if (!number(gph) || gph.toDouble() < 0)
                return fail(error, where + QStringLiteral("curve row %1: US GPH must be finite and nonnegative.").arg(row));
            previous = rpm.toDouble();
        }
    }
    return true;
}

bool EngineProfileCatalog::parse(const QByteArray &bytes, QJsonObject *catalog, QString *error) {
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(error, QStringLiteral("Invalid profiles JSON at byte %1: %2").arg(parseError.offset).arg(parseError.errorString()));
    if (!doc.isObject()) return fail(error, "Engine profiles JSON must contain a root object with an engines array.");
    if (!validate(doc.object(), error)) return false;
    *catalog = doc.object();
    return true;
}

bool EngineProfileCatalog::open(QString *error) {
    m_source = RuntimeResources::engineProfiles();
    m_target = AppPaths::resourcesDirectory() + "/Data/engine_profiles.json";
    m_sourceExists = QFileInfo::exists(m_source);
    m_targetExists = QFileInfo::exists(m_target);
    m_sourceBytes.clear(); m_targetBytes.clear(); m_document = {};
    m_open = false;
    if (!read(m_source, &m_sourceBytes, error)) return false;
    if (m_targetExists && !read(m_target, &m_targetBytes, error)) return false;
    m_open = true; // An invalid but readable catalog can be explicitly recovered, with backup.
    return parse(m_sourceBytes, &m_document, error);
}

bool EngineProfileCatalog::unchanged(QString *error) const {
    if (!m_open) return fail(error, "The profile file could not be read. Correct access permissions and reopen the editor.");
    QByteArray current;
    if (RuntimeResources::engineProfiles() != m_source || QFileInfo::exists(m_source) != m_sourceExists ||
        !read(m_source, &current, error) || current != m_sourceBytes)
        return fail(error, "The effective profiles changed outside this editor. Close and reopen before saving.");
    if (QFileInfo::exists(m_target) != m_targetExists) return fail(error, "The local profiles changed outside this editor. Close and reopen.");
    if (m_targetExists && (!read(m_target, &current, error) || current != m_targetBytes))
        return fail(error, "The local profiles changed outside this editor. Close and reopen.");
    return true;
}

bool EngineProfileCatalog::write(const QString &path, const QByteArray &bytes, QString *error) {
    if (QFileInfo(path).isSymLink()) return fail(error, "Refusing to replace a symbolic-link profile file or backup: " + path);
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, QStringLiteral("Cannot atomically save %1: %2").arg(path, file.errorString()));
    return true;
}

bool EngineProfileCatalog::replace(const QByteArray &bytes, QString *error) {
    const QString dir = QFileInfo(m_target).absolutePath();
    if (!AppPaths::ensureDirectory(dir)) return fail(error, "Cannot create or write local profiles directory: " + dir);
    QLockFile lock(m_target + ".lock");
    if (!lock.tryLock(0)) return fail(error, "Another profiles save is in progress, or the profiles lock cannot be created. Retry after checking directory access.");
    if (!unchanged(error)) return false;
    // Keep an exact, complete saved snapshot before any replacement, including
    // malformed user JSON during explicit recovery. Never rewrite the source.
    const QString backupPath = m_target + "." + QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzz") +
                               "-" + QUuid::createUuid().toString(QUuid::Id128) + ".bak";
    if (!write(backupPath, m_sourceBytes, error)) return false;
    if (!write(m_target, bytes, error)) return false;
    m_lastBackup = backupPath;
    return open(error);
}

bool EngineProfileCatalog::save(const QJsonObject &draft, QString *error) {
    if (m_document.isEmpty()) return fail(error, "The saved profiles are invalid or unreadable. Use an explicit validated Restore (or repair access and reopen); the original has been preserved.");
    if (!validate(draft, error)) return false;
    return replace(QJsonDocument(draft).toJson(QJsonDocument::Indented), error);
}

bool EngineProfileCatalog::restore(const QByteArray &candidate, QString *error) {
    QJsonObject valid;
    if (!parse(candidate, &valid, error)) return false;
    return replace(candidate, error);
}

bool EngineProfileCatalog::backup(const QString &destination, bool overwrite, QString *error) const {
    if (!unchanged(error)) return false;
    const auto info = QFileInfo(destination);
    if (info.absoluteFilePath() == QFileInfo(m_target).absoluteFilePath() ||
        info.absoluteFilePath() == QFileInfo(m_source).absoluteFilePath() ||
        (!info.canonicalFilePath().isEmpty() && (info.canonicalFilePath() == QFileInfo(m_source).canonicalFilePath() ||
                                               info.canonicalFilePath() == QFileInfo(m_target).canonicalFilePath())))
        return fail(error, "Choose a backup destination different from the active profiles.");
    if (info.exists() && !overwrite) return fail(error, "Backup already exists; explicit overwrite confirmation is required.");
    return write(destination, m_sourceBytes, error);
}

QString EngineProfileCatalog::uniqueName(const QJsonObject &catalog, const QString &base) {
    QSet<QString> names;
    for (const auto &p : catalog.value("engines").toArray()) names.insert(p.toObject().value("name").toString());
    QString name = base;
    for (int n = 2; names.contains(name) || name == "None"; ++n) name = base + QStringLiteral(" (%1)").arg(n);
    return name;
}
