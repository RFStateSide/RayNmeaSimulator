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

// Contract tests through the native adapter, with a compile-time in-memory SecItem substitute.
// These do not qualify Apple's Keychain, authorization UI, disk persistence or process restart.
#include "credentialstore.h"
#include "appsettings.h"
#include "macos_credential_test_api.h"
#include <QtTest>
#include <QScopeGuard>
#include <QTemporaryDir>

class MacCredentialUnitTests : public QObject {
    Q_OBJECT
private slots:
    void init() { MacCredentialTest::reset(); }
    void cleanup() { QVERIFY(MacCredentialTest::validQueries()); }
    void createReadReplaceRemoveExactBytes()
    {
        auto missing = CredentialStore::read("contract");
        QVERIFY(missing.success && !missing.found && missing.error.isEmpty() && missing.value.isEmpty());
        const auto value = QString::fromUtf8("  caf\xc3\xa9 \xf0\x9f\x9a\xa2  ") + QChar(0) + QStringLiteral("tail");
        QString error;
        QVERIFY(CredentialStore::write("contract", value, &error));
        QVERIFY(error.isEmpty());
        auto found = CredentialStore::read("contract");
        QVERIFY(found.success && found.found && found.value.toUtf8() == value.toUtf8());
        QVERIFY(CredentialStore::write("contract", QString(2500, 'x')));
        QVERIFY(CredentialStore::read("contract").value.size() == 2500);
        QVERIFY(CredentialStore::write("contract", QString(), &error));
        QVERIFY(CredentialStore::remove("contract", &error));
        QVERIFY(CredentialStore::read("contract").success && !CredentialStore::read("contract").found);
        QVERIFY(MacCredentialTest::calls() > 8);
    }
    void invalidWriteDoesNotTouchBackend()
    {
        for (const auto& value : {QString(2501, 'x'), QString(1251, QChar(0xe9)), QString("a\nb"), QString("a\rb")}) {
            QString error;
            QVERIFY(!CredentialStore::write("contract", value, &error));
            QVERIFY(!error.isEmpty() && !error.contains(value));
        }
        QCOMPARE(MacCredentialTest::calls(), 0);
    }
    void nativeFailuresAreNotMissingOrSuccess()
    {
        using namespace MacCredentialTest;
        for (const auto code : {errSecAuthFailed, errSecUserCanceled, errSecInteractionNotAllowed,
                 errSecNotAvailable, errSecNoSuchKeychain, errSecNoDefaultKeychain, errSecMissingEntitlement, errSecParam}) {
            failNext(Operation::Read, code);
            const auto result = CredentialStore::read("contract");
            QVERIFY(!result.success && !result.found && result.value.isEmpty() && !result.error.isEmpty());
            QString error;
            failNext(Operation::Add, code);
            QVERIFY(!CredentialStore::write("contract", QStringLiteral("test-only-payload"), &error));
            QVERIFY(!error.isEmpty() && !error.contains("test-only-payload"));
            failNext(Operation::Remove, code);
            QVERIFY(!CredentialStore::remove("contract", &error));
            QVERIFY(!error.isEmpty());
        }
    }
    void replacementFailureKeepsPreviousValue()
    {
        using namespace MacCredentialTest;
        const auto original = QStringLiteral("retained-fixture");
        QVERIFY(CredentialStore::write("contract", original));
        for (const auto status : {errSecAuthFailed, errSecNotAvailable, errSecItemNotFound, errSecDuplicateItem}) {
            failNext(Operation::Update, status);
            QString error;
            QVERIFY(!CredentialStore::write("contract", QStringLiteral("rejected-fixture"), &error));
            const auto result = CredentialStore::read("contract");
            QVERIFY(result.success && result.found && result.value == original);
            QVERIFY(!error.isEmpty() && !error.contains(original) && !error.contains("rejected-fixture"));
        }
    }
    void malformedNativeReadFailsClosed()
    {
        QVERIFY(CredentialStore::write("contract", QStringLiteral("fixture")));
        MacCredentialTest::malformedRead(true);
        const auto result = CredentialStore::read("contract");
        QVERIFY(!result.success && !result.found && result.value.isEmpty() && !result.error.isEmpty());
    }
    void invalidNativeEncodingFailsClosed()
    {
        QVERIFY(CredentialStore::write("contract", QStringLiteral("fixture")));
        MacCredentialTest::invalidEncodingRead(true);
        const auto result = CredentialStore::read("contract");
        QVERIFY(!result.success && !result.found && result.value.isEmpty() && !result.error.isEmpty());
    }
    void profilesDoNotShareCredentialsOrWriteSettings()
    {
        const auto old = qgetenv("RAYNMEA_TEST_HOME");
        const auto restore = qScopeGuard([old] { qputenv("RAYNMEA_TEST_HOME", old); });
        QTemporaryDir other;
        QVERIFY(other.isValid());
        const auto first = QStringLiteral("first-profile-fixture");
        const auto second = QStringLiteral("second-profile-fixture");
        QVERIFY(CredentialStore::write("contract", first));
        qputenv("RAYNMEA_TEST_HOME", other.path().toUtf8());
        const auto absent = CredentialStore::read("contract");
        QVERIFY(absent.success && !absent.found);
        QVERIFY(CredentialStore::write("contract", second));
        QVERIFY(CredentialStore::read("contract").value == second);
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.sync();
        QFile file(settings.fileName());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto content = file.readAll();
        QVERIFY(!content.contains(first.toUtf8()) && !content.contains(second.toUtf8()));
        qputenv("RAYNMEA_TEST_HOME", old);
        QVERIFY(CredentialStore::read("contract").value == first);
    }
    void developmentProfileCannotCallKeychain()
    {
        // Same guard is compiled into production. A child tests it with the flag
        // present, without invoking DevelopmentProfile::initialize or any UI.
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {"--isolated-profile", "guard-probe"});
        QVERIFY(child.waitForFinished(10000));
        QCOMPARE(child.exitCode(), 0);
        QVERIFY(child.readAllStandardOutput().isEmpty() && child.readAllStandardError().isEmpty());
    }
};
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("guard-probe")) {
        QString error;
        const auto read = CredentialStore::read("guard");
        const bool write = CredentialStore::write("guard", QStringLiteral("guard-fixture"), &error);
        const bool remove = CredentialStore::remove("guard", &error);
        return !read.success && !read.found && !write && !remove && !error.isEmpty()
            && MacCredentialTest::calls() == 0 ? 0 : 1;
    }
    MacCredentialUnitTests test;
    return QTest::qExec(&test, argc, argv);
}
#include "macos_credential_unit_tests.moc"
