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

#ifndef ENGINEPROFILECATALOG_H
#define ENGINEPROFILECATALOG_H

#include <QJsonObject>
#include <QString>

// A saved catalog snapshot, separate from the editor's draft. Never writes a
// legacy source or the immutable resource; optimistic checks protect external edits.
class EngineProfileCatalog
{
public:
    bool open(QString *error);
    bool save(const QJsonObject &draft, QString *error);
    bool restore(const QByteArray &candidate, QString *error);
    bool backup(const QString &destination, bool overwrite, QString *error) const;
    static bool parse(const QByteArray &bytes, QJsonObject *catalog, QString *error);
    static bool validate(const QJsonObject &catalog, QString *error);
    static QString curveKey(const QJsonObject &profile);
    static QString uniqueName(const QJsonObject &catalog, const QString &base);

    QJsonObject document() const { return m_document; }
    QString sourcePath() const { return m_source; }
    QString targetPath() const { return m_target; }
    QString lastBackup() const { return m_lastBackup; }
    bool needsLocalCopy() const { return m_source != m_target; }
private:
    bool unchanged(QString *error) const;
    bool replace(const QByteArray &bytes, QString *error);
    static bool write(const QString &path, const QByteArray &bytes, QString *error);
    QJsonObject m_document;
    QByteArray m_sourceBytes, m_targetBytes;
    QString m_source, m_target, m_lastBackup;
    bool m_sourceExists = false, m_targetExists = false, m_open = false;
};
#endif
