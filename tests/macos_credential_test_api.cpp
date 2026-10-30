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

// In-memory unit substitute, NOT a Keychain integration fixture. No disk persistence.
#ifndef RAYNMEA_TESTING
#error The credential substitute must never be compiled into production
#endif
#include "macos_credential_test_api.h"
#include <QHash>
#include <QVector>

namespace MacCredentialTest {
namespace {
QHash<QString, QByteArray> items;
struct Failure { Operation operation; OSStatus status; QString suffix; };
QVector<Failure> failures;
int count = 0;
bool valid = true, malformed = false, invalidEncoding = false;
QString text(CFTypeRef value)
{
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) return {};
    auto string = static_cast<CFStringRef>(value);
    QVector<UniChar> characters(CFStringGetLength(string));
    CFStringGetCharacters(string, CFRangeMake(0, characters.size()), characters.data());
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(characters.constData()), characters.size());
}
OSStatus check(Operation op, CFDictionaryRef query, QString& id)
{
    ++count;
    const auto service = text(CFDictionaryGetValue(query, kSecAttrService));
    const auto account = text(CFDictionaryGetValue(query, kSecAttrAccount));
    const bool safe = service == "RFStateSide.RayNmeaSimulator" && account.startsWith("test-")
        && account.contains('/') && CFEqual(CFDictionaryGetValue(query, kSecClass), kSecClassGenericPassword)
        && CFDictionaryGetValue(query, kSecUseDataProtectionKeychain) == kCFBooleanFalse
        && !CFDictionaryContainsKey(query, kSecMatchSearchList)
        && !CFDictionaryContainsKey(query, kSecUseKeychain)
        && !CFDictionaryContainsKey(query, kSecAttrAccessible)
        && !CFDictionaryContainsKey(query, kSecAttrSynchronizable);
    valid &= safe;
    if (!safe) return errSecParam;
    id = service + "/" + account;
    for (qsizetype i = 0; i < failures.size(); ++i) {
        if (failures[i].operation == op && (failures[i].suffix.isEmpty() || account.endsWith("/" + failures[i].suffix))) {
            const auto status = failures.takeAt(i).status;
            return status;
        }
    }
    return errSecSuccess;
}
QByteArray bytes(CFDictionaryRef dictionary)
{
    const auto value = static_cast<CFDataRef>(CFDictionaryGetValue(dictionary, kSecValueData));
    if (!value || CFGetTypeID(value) != CFDataGetTypeID()) return {};
    return QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(value)), CFDataGetLength(value));
}
}
OSStatus copyMatching(CFDictionaryRef query, CFTypeRef* result)
{
    QString id;
    const auto status = check(Operation::Read, query, id);
    if (status) return status;
    valid &= CFDictionaryGetValue(query, kSecReturnData) == kCFBooleanTrue
        && CFEqual(CFDictionaryGetValue(query, kSecMatchLimit), kSecMatchLimitOne);
    if (!items.contains(id)) return errSecItemNotFound;
    if (malformed) { *result = CFRetain(CFSTR("invalid data type")); return errSecSuccess; }
    if (invalidEncoding) { const UInt8 invalid[] = {0xff}; *result = CFDataCreate(nullptr, invalid, 1); return errSecSuccess; }
    const auto& value = items[id];
    *result = CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(value.constData()), value.size());
    return errSecSuccess;
}
OSStatus add(CFDictionaryRef attributes, CFTypeRef*)
{
    QString id;
    const auto status = check(Operation::Add, attributes, id);
    if (status) return status;
    if (items.contains(id)) return errSecDuplicateItem;
    items.insert(id, bytes(attributes));
    return errSecSuccess;
}
OSStatus update(CFDictionaryRef query, CFDictionaryRef changes)
{
    QString id;
    const auto status = check(Operation::Update, query, id);
    if (status) return status;
    valid &= CFDictionaryGetCount(changes) == 1 && !CFDictionaryContainsKey(query, kSecValueData);
    if (!items.contains(id)) return errSecItemNotFound;
    items[id] = bytes(changes);
    return errSecSuccess;
}
OSStatus erase(CFDictionaryRef query)
{
    QString id;
    const auto status = check(Operation::Remove, query, id);
    if (status) return status;
    return items.remove(id) ? errSecSuccess : errSecItemNotFound;
}
void reset() { items.clear(); failures.clear(); count = 0; valid = true; malformed = false; invalidEncoding = false; }
void failNext(Operation operation, OSStatus status, const QString& suffix) { failures.append({operation, status, suffix}); }
void malformedRead(bool enabled) { malformed = enabled; }
void invalidEncodingRead(bool enabled) { invalidEncoding = enabled; }
int calls() { return count; }
bool validQueries() { return valid; }
}
