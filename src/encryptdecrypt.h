/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
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

#ifndef ENCRYPTDECRYPT_H
#define ENCRYPTDECRYPT_H

#include <QString>
#include <QCryptographicHash>

// Historical XOR obfuscation, not cryptographic encryption. Not in active CMake
// targets and not used for credentials; retained only as documented legacy source.
class EncryptDecrypt
{
public:
    EncryptDecrypt();
    // Method to encrypt a QString
    QString encrypt(const QString &data, const QString &key);

    // Method to decrypt a QString
    QString decrypt(const QString &data, const QString &key);

private:
    // Helper method to apply XOR operation with hashed key
    QString xorWithKey(const QString &data, const QByteArray &keyHash);
};

#endif // ENCRYPTDECRYPT_H
