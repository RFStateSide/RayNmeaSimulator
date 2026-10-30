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

#include "encryptdecrypt.h"
#include <QCryptographicHash>

// Method to encrypt a QString
EncryptDecrypt::EncryptDecrypt()
{
}

QString EncryptDecrypt::encrypt(const QString &data, const QString &key)
{
    // Create a SHA-256 hash of the key
    //QByteArray keyHash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256);
    //return xorWithKey(data, keyHash);
    QByteArray encrypted = xorWithKey(data, key.toUtf8()).toUtf8();
    return encrypted.toBase64();  // Convert binary data to Base64
}

// Method to decrypt a QString
QString EncryptDecrypt::decrypt(const QString &data, const QString &key)
{
    // Decrypt is the same as encrypt with XOR and the same hashed key
    //QByteArray keyHash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256);
    //return xorWithKey(data, keyHash);
    QByteArray encrypted = QByteArray::fromBase64(data.toUtf8());  // Convert Base64 back to binary
    return xorWithKey(QString::fromUtf8(encrypted), key.toUtf8());
}

// Helper method to apply XOR operation with hashed key
QString EncryptDecrypt::xorWithKey(const QString &data, const QByteArray &keyHash)
{
    QByteArray dataBytes = data.toUtf8();
    QByteArray result(dataBytes.size(), 0);

    for (int i = 0; i < dataBytes.size(); ++i) {
        result[i] = dataBytes[i] ^ keyHash[i % keyHash.size()];
    }

    return QString::fromUtf8(result);
}

