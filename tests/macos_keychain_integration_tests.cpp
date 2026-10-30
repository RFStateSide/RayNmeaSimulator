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

// Actual Security integration in an owned disposable file-based Keychain only.
// Reviewed lifecycle API deprecations: see docs/macos-credentials.md. Never use
// security create-keychain (changes search list), normal login items or DP storage.
#ifndef RAYNMEA_TESTING
#error This isolated integration fixture is test-only
#endif
#include "credentialstore.h"
#include "appsettings.h"
#include "macos_credential_test_api.h"
#include <QtTest>
#include <QDataStream>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QTemporaryDir>

// Apple still documents private temporary Keychains via these deprecated APIs.
// Their use is confined to this explicit opt-in fixture, never production.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
namespace {
SecKeychainRef owned = nullptr;
QString ownedPath;
int apiCalls = 0;
bool validBoundary = true;

bool verifyOwned()
{
    if (!owned || ownedPath.isEmpty()) return false;
    char path[4096] = {};
    UInt32 length = sizeof(path);
    if (SecKeychainGetPath(owned, &length, path) != errSecSuccess) return false;
    return QFileInfo(QString::fromUtf8(path, length)).canonicalFilePath() == QFileInfo(ownedPath).canonicalFilePath()
        && QFileInfo(ownedPath).isFile() && !QFileInfo(ownedPath).isSymLink();
}
CFMutableDictionaryRef scoped(CFDictionaryRef source, bool adding)
{
    // Check the ownership boundary BEFORE making any item API call.
    if (!verifyOwned() || !CFEqual(CFDictionaryGetValue(source, kSecAttrService), CFSTR("RFStateSide.RayNmeaSimulator"))
        || CFDictionaryGetValue(source, kSecUseDataProtectionKeychain) != kCFBooleanFalse
        || CFDictionaryContainsKey(source, kSecUseKeychain) || CFDictionaryContainsKey(source, kSecMatchSearchList)) {
        validBoundary = false;
        return nullptr;
    }
    auto result = CFDictionaryCreateMutableCopy(nullptr, 0, source);
    if (adding) CFDictionarySetValue(result, kSecUseKeychain, owned);
    else {
        const void* one[] = { owned };
        auto list = CFArrayCreate(nullptr, one, 1, &kCFTypeArrayCallBacks);
        CFDictionarySetValue(result, kSecMatchSearchList, list);
        CFRelease(list);
    }
    return result;
}
QByteArray configuration()
{
    QByteArray result;
    for (const auto& verb : {QStringLiteral("list-keychains"), QStringLiteral("default-keychain")}) {
        QProcess process;
        process.start("/usr/bin/security", {verb, "-d", "user"});
        if (!process.waitForFinished(10000) || process.exitCode() != 0) return {};
        result += process.readAllStandardOutput();
    }
    return result;
}
QString fixtureValue() { return QString::fromUtf8("  \xc3\xa9 \xf0\x9f\x9a\xa2  ") + QChar(0) + QStringLiteral("tail"); }
}
namespace MacCredentialTest {
OSStatus copyMatching(CFDictionaryRef query, CFTypeRef* result)
{
    auto scopedQuery = scoped(query, false);
    if (!scopedQuery) return errSecParam;
    ++apiCalls;
    const auto status = SecItemCopyMatching(scopedQuery, result);
    CFRelease(scopedQuery);
    return status;
}
OSStatus add(CFDictionaryRef attributes, CFTypeRef* result)
{
    auto scopedQuery = scoped(attributes, true);
    if (!scopedQuery) return errSecParam;
    ++apiCalls;
    const auto status = SecItemAdd(scopedQuery, result);
    CFRelease(scopedQuery);
    return status;
}
OSStatus update(CFDictionaryRef query, CFDictionaryRef changes)
{
    auto scopedQuery = scoped(query, false);
    if (!scopedQuery) return errSecParam;
    ++apiCalls;
    const auto status = SecItemUpdate(scopedQuery, changes);
    CFRelease(scopedQuery);
    return status;
}
OSStatus erase(CFDictionaryRef query)
{
    auto scopedQuery = scoped(query, false);
    if (!scopedQuery) return errSecParam;
    ++apiCalls;
    const auto status = SecItemDelete(scopedQuery);
    CFRelease(scopedQuery);
    return status;
}
}
class MacKeychainIntegrationTests : public QObject {
    Q_OBJECT
private slots:
    void disposableKeychainCrudAndProcessRestart()
    {
        const auto before = configuration();
        QVERIFY(!before.isEmpty());
        QTemporaryDir directory(TestProfile::root() + "/keychain-XXXXXX");
        QVERIFY(directory.isValid());
        ownedPath = directory.filePath("raynmea-m246d.keychain");
        QByteArray password;
        for (int i = 0; i < 8; ++i) password += QByteArray::number(QRandomGenerator::system()->generate(), 16);
        const auto status = SecKeychainCreate(ownedPath.toUtf8().constData(), password.size(), password.constData(), false, nullptr, &owned);
        // Cleanup always targets the owned reference, including assertion failures.
        const auto cleanup = qScopeGuard([&] {
            if (owned) { const auto deleted = SecKeychainDelete(owned); CFRelease(owned); owned = nullptr;
                if (deleted != errSecSuccess) QTest::qFail("Owned disposable Keychain cleanup failed", __FILE__, __LINE__); }
            password.fill('\0');
        });
        QCOMPARE(status, errSecSuccess);
        QVERIFY(verifyOwned());
        QVERIFY(configuration() == before); // Do not access any items before this check.
        auto missing = CredentialStore::read("integration");
        QVERIFY(missing.success && !missing.found);
        QString error;
        const auto value = fixtureValue();
        QVERIFY(CredentialStore::write("integration", value, &error));
        const auto first = CredentialStore::read("integration");
        QVERIFY(first.success && first.found && first.value.toUtf8() == value.toUtf8());
        QVERIFY(CredentialStore::write("integration", QStringLiteral("replacement-fixture"), &error));
        QVERIFY(CredentialStore::read("integration").value == QStringLiteral("replacement-fixture"));
        QVERIFY(!CredentialStore::write("integration", QString(2501, 'x'), &error));
        QVERIFY(CredentialStore::read("integration").value == QStringLiteral("replacement-fixture"));
        QVERIFY(CredentialStore::write("integration", value, &error));
        // Child reopens only this Keychain; path/password travel over stdin,
        // never command arguments, environment variables, logs or settings.
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {"--restart-probe"});
        QVERIFY(child.waitForStarted(5000));
        QByteArray payload;
        { QDataStream stream(&payload, QIODevice::WriteOnly); stream << ownedPath << password; }
        const auto written = child.write(payload);
        QVERIFY(written == payload.size());
        child.closeWriteChannel();
        QVERIFY(child.waitForFinished(15000));
        payload.fill('\0');
        QCOMPARE(child.exitCode(), 0);
        QVERIFY(child.readAllStandardOutput().isEmpty() && child.readAllStandardError().isEmpty());
        QVERIFY(CredentialStore::write("integration", QString(), &error));
        QVERIFY(CredentialStore::remove("integration", &error));
        missing = CredentialStore::read("integration");
        QVERIFY(missing.success && !missing.found);
        QVERIFY(validBoundary && apiCalls >= 12);
        QCOMPARE(SecKeychainDelete(owned), errSecSuccess);
        CFRelease(owned); owned = nullptr;
        QVERIFY(configuration() == before);
    }
};
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("--restart-probe")) {
        QFile input;
        if (!input.open(stdin, QIODevice::ReadOnly)) return 10;
        QByteArray payload = input.readAll(), password;
        QDataStream stream(payload);
        stream >> ownedPath >> password;
        payload.fill('\0');
        const auto root = QFileInfo(TestProfile::root()).canonicalFilePath();
        if (stream.status() != QDataStream::Ok || !QFileInfo(ownedPath).canonicalFilePath().startsWith(root + "/")
            || QFileInfo(ownedPath).isSymLink()) return 11;
        const auto before = configuration();
        const auto opened = SecKeychainOpen(ownedPath.toUtf8().constData(), &owned);
        if (opened != errSecSuccess || !verifyOwned()) return 12;
        const auto unlock = SecKeychainUnlock(owned, password.size(), password.constData(), true);
        password.fill('\0');
        if (unlock != errSecSuccess || before.isEmpty() || configuration() != before) return 13;
        const auto result = CredentialStore::read("integration");
        const bool ok = result.success && result.found && result.value.toUtf8() == fixtureValue().toUtf8()
            && validBoundary && apiCalls == 1 && configuration() == before;
        CFRelease(owned); owned = nullptr;
        return ok ? 0 : 14;
    }
    MacKeychainIntegrationTests test;
    return QTest::qExec(&test, argc, argv);
}
#pragma clang diagnostic pop
#include "macos_keychain_integration_tests.moc"
