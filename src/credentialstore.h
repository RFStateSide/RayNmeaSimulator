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

#ifndef CREDENTIALSTORE_H
#define CREDENTIALSTORE_H
#include <QString>

// OS-backed per-user credentials. No settings/plaintext fallback.
class CredentialStore {
public:
    struct Result {
        bool success = false;
        bool found = false;
        QString value;
        QString error;
    };
    static Result read(const QString& name);
    static bool write(const QString& name, const QString& value, QString* error = nullptr);
    static bool remove(const QString& name, QString* error = nullptr);
    static QString backendName();
};
#endif
