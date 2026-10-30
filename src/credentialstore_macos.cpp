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

// Service/account design follows BrickSuite's LGPL-3.0 CredentialStore.
// SecItem adapter: file-based Keychain compatibility without deprecated SecKeychain APIs.
#include "credentialstore_macos_p.h"
#include "appsettings.h"
#include <Security/Security.h>
#include <memory>
#include <type_traits>
#ifdef RAYNMEA_TESTING
#include "../tests/macos_credential_test_api.h"
#endif

namespace {
struct Release { void operator()(const void* p) const { if (p) CFRelease(p); } };
template<class T> using CFHandle = std::unique_ptr<typename std::remove_pointer<T>::type, Release>;

CFHandle<CFMutableDictionaryRef> query(const QString& account)
{
    CFHandle<CFMutableDictionaryRef> attributes(CFDictionaryCreateMutable(nullptr, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFHandle<CFStringRef> name(CFStringCreateWithCharacters(nullptr,
        reinterpret_cast<const UniChar*>(account.utf16()), account.size()));
    CFDictionarySetValue(attributes.get(), kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(attributes.get(), kSecAttrService, CFSTR("RFStateSide.RayNmeaSimulator"));
    CFDictionarySetValue(attributes.get(), kSecAttrAccount, name.get());
    // Same file-based storage model as BrickSuite. Data Protection requires
    // separately provisioned signing entitlements; never silently switch stores.
    CFDictionarySetValue(attributes.get(), kSecUseDataProtectionKeychain, kCFBooleanFalse);
    // Native file-based ACLs and normal Security authorization UI apply. Do not
    // claim kSecAttrAccessible or Data Protection-only no-UI flags protect this store.
    return attributes;
}

QString message(OSStatus status)
{
    // Fixed messages only: never expose backend data, account identifiers or values.
    switch (status) {
    case errSecAuthFailed:
    case errSecUserCanceled:
    case errSecMissingEntitlement:
        return QStringLiteral("macOS Keychain access denied or canceled. Authorize this application and retry.");
    case errSecInteractionNotAllowed:
        return QStringLiteral("macOS Keychain is locked or interaction is unavailable. Unlock it in a local login session and retry.");
    case errSecNotAvailable:
    case errSecNoSuchKeychain:
    case errSecNoDefaultKeychain:
        return QStringLiteral("macOS Keychain unavailable. Check the local login session and retry.");
    case errSecDuplicateItem:
        return QStringLiteral("macOS Keychain replacement failed. Keep Setup open and retry.");
    case errSecItemNotFound:
        return QStringLiteral("macOS Keychain credential changed during replacement. Keep Setup open and retry.");
    default:
        return QStringLiteral("macOS Keychain operation failed. Keep Setup open and retry.");
    }
}

bool allowed(QString* error)
{
    if (!DevelopmentProfile::active()) return true;
    if (error) *error = QStringLiteral("macOS Keychain unavailable in an isolated development profile. Credentials were not accessed.");
    return false;
}

#ifdef RAYNMEA_TESTING
// There are deliberately no references to real SecItem functions in test objects.
constexpr auto nativeRead = MacCredentialTest::copyMatching;
constexpr auto nativeAdd = MacCredentialTest::add;
constexpr auto nativeUpdate = MacCredentialTest::update;
constexpr auto nativeDelete = MacCredentialTest::erase;
#else
constexpr auto nativeRead = SecItemCopyMatching;
constexpr auto nativeAdd = SecItemAdd;
constexpr auto nativeUpdate = SecItemUpdate;
constexpr auto nativeDelete = SecItemDelete;
#endif
}

CredentialStore::Result MacCredentialStore::read(const QString& account)
{
    CredentialStore::Result result;
    if (!allowed(&result.error)) return result;
    auto attributes = query(account);
    CFDictionarySetValue(attributes.get(), kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(attributes.get(), kSecMatchLimit, kSecMatchLimitOne);
    CFTypeRef raw = nullptr;
    const OSStatus status = nativeRead(attributes.get(), &raw);
    CFHandle<CFTypeRef> data(raw);
    if (status == errSecItemNotFound) { result.success = true; return result; }
    if (status != errSecSuccess) { result.error = message(status); return result; }
    if (!data || CFGetTypeID(data.get()) != CFDataGetTypeID()) {
        result.error = QStringLiteral("macOS Keychain returned invalid credential data.");
        return result;
    }
    const auto bytes = static_cast<CFDataRef>(data.get());
    const auto length = CFDataGetLength(bytes);
    const auto value = QString::fromUtf8(reinterpret_cast<const char*>(CFDataGetBytePtr(bytes)), length);
    // Fail closed on invalid UTF-8 instead of silently replacing credential bytes.
    if (value.toUtf8() != QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(bytes)), length)) {
        result.error = QStringLiteral("macOS Keychain returned invalid credential encoding.");
        return result;
    }
    result.success = result.found = true;
    result.value = value;
    return result;
}

bool MacCredentialStore::write(const QString& account, const QString& value, QString* error)
{
    if (!allowed(error)) return false;
    auto attributes = query(account);
    QByteArray bytes = value.toUtf8();
    CFHandle<CFDataRef> data(CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(bytes.constData()), bytes.size()));
    bytes.fill('\0');
    CFDictionarySetValue(attributes.get(), kSecValueData, data.get());
    const OSStatus created = nativeAdd(attributes.get(), nullptr);
    OSStatus status = created;
    if (created == errSecDuplicateItem) {
        CFDictionaryRemoveValue(attributes.get(), kSecValueData);
        CFHandle<CFMutableDictionaryRef> changes(CFDictionaryCreateMutable(nullptr, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionarySetValue(changes.get(), kSecValueData, data.get());
        status = nativeUpdate(attributes.get(), changes.get());
    }
    if (status != errSecSuccess && error) *error = message(status);
    return status == errSecSuccess;
}

bool MacCredentialStore::remove(const QString& account, QString* error)
{
    if (!allowed(error)) return false;
    auto attributes = query(account);
    const OSStatus status = nativeDelete(attributes.get());
    if (status == errSecSuccess || status == errSecItemNotFound) return true;
    if (error) *error = message(status);
    return false;
}
