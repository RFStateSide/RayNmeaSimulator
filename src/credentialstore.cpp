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

// Adapted from RF StateSide BrickSuite; owner-attested internal reuse (2026-10-28).
// BrickSuite retains its existing license; this adaptation is AGPL-3.0-or-later.
// Follows the RF StateSide BrickSuite CredentialStore design.
// RayNmeaSimulator integration: isolated test namespace and redacted backend errors.
#include "credentialstore.h"
#include <QCryptographicHash>
#include <QProcess>
#include <QStandardPaths>
#ifdef Q_OS_MACOS
#include "credentialstore_macos_p.h"
#endif
#include "appsettings.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace {
QString account(const QString& name)
{
#ifdef RAYNMEA_TESTING
    // Tests never touch the normal application's credentials.
    return "test-" + QCryptographicHash::hash(TestProfile::root().toUtf8(), QCryptographicHash::Sha256).toHex() + "/" + name;
#else
    return name;
#endif
}
#ifdef Q_OS_WIN
QString target(const QString& name) { return "RFStateSide.RayNmeaSimulator/" + account(name); }
#endif
#ifdef Q_OS_LINUX
bool run(const QString& operation, const QString& name, const QByteArray& input, QByteArray* output, bool* missing, QString* error)
{
    const QString tool = QStandardPaths::findExecutable("secret-tool");
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };
    if (tool.isEmpty())
        return fail("Secure storage unavailable: install the Secret Service client secret-tool and unlock a desktop keyring.");
    QProcess process;
    QStringList args { operation };
    if (operation == "store")
        args << "--label=RayNmeaSimulator credential";
    args << "application" << "RayNmeaSimulator" << "credential" << account(name);
    process.start(tool, args);
    if (!process.waitForStarted(3000))
        return fail("Unable to start the Secret Service client.");
    if (operation == "store")
        process.write(input);
    process.closeWriteChannel();
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        return fail("Secure storage timed out. Unlock the desktop keyring and retry.");
    }
    const auto data = process.readAllStandardOutput();
    const auto detail = process.readAllStandardError(); // Never expose backend output: it may contain a secret.
    if (process.exitStatus() != QProcess::NormalExit)
        return fail("Secret Service client failed.");
    if (process.exitCode() != 0) {
        if (operation != "store" && process.exitCode() == 1 && data.isEmpty() && detail.isEmpty()) {
            if (missing)
                *missing = true;
            return true;
        }
        return fail("Secure storage failed. Check that the desktop Secret Service is running and unlocked.");
    }
    if (output) {
        *output = data;
    }
    return true;
}
#endif
}

CredentialStore::Result CredentialStore::read(const QString& name)
{
    Result result;
    if (DevelopmentProfile::active()) {
        result.error = "Secure storage unavailable in an isolated qualification profile. Credentials were not accessed.";
        return result;
    }
#ifdef Q_OS_WIN
    const auto id = target(name).toStdWString();
    PCREDENTIALW entry = nullptr;
    if (!CredReadW(id.c_str(), CRED_TYPE_GENERIC, 0, &entry)) {
        if (GetLastError() == ERROR_NOT_FOUND)
            result.success = true;
        else
            result.error = "Windows Credential Manager could not read the credential.";
        return result;
    }
    result.success = result.found = true;
    result.value = QString::fromUtf8(reinterpret_cast<const char*>(entry->CredentialBlob), entry->CredentialBlobSize);
    SecureZeroMemory(entry->CredentialBlob, entry->CredentialBlobSize);
    CredFree(entry);
#elif defined(Q_OS_LINUX)
    QByteArray data;
    bool missing = false;
    result.success = run("lookup", name, { }, &data, &missing, &result.error);
    result.found = result.success && !missing;
    if (result.found)
        result.value = QString::fromUtf8(data);
#elif defined(Q_OS_MACOS)
    return MacCredentialStore::read(account(name));
#else
    result.error = "No secure credential backend is available on this platform.";
#endif
    return result;
}

bool CredentialStore::write(const QString& name, const QString& value, QString* error)
{
    if (DevelopmentProfile::active()) {
        if (error) *error = "Secure storage unavailable in an isolated qualification profile. Credentials were not accessed.";
        return false;
    }
    if (error)
        error->clear();
    if (value.isEmpty())
        return remove(name, error);
    if (value.contains('\n') || value.contains('\r') || value.toUtf8().size() > 2500) {
        if (error)
            *error = "Credential must be a single line of at most 2500 UTF-8 bytes.";
        return false;
    }
#ifdef Q_OS_WIN
    const auto id = target(name).toStdWString();
    QByteArray bytes = value.toUtf8();
    CREDENTIALW entry { };
    entry.Type = CRED_TYPE_GENERIC;
    entry.TargetName = const_cast<LPWSTR>(id.c_str());
    entry.CredentialBlobSize = bytes.size();
    entry.CredentialBlob = reinterpret_cast<LPBYTE>(bytes.data());
    entry.Persist = CRED_PERSIST_LOCAL_MACHINE; // Per-user, persistent on this machine.
    const bool ok = CredWriteW(&entry, 0);
    SecureZeroMemory(bytes.data(), bytes.size());
    if (!ok && error)
        *error = "Windows Credential Manager could not save the credential. Keep Setup open and retry.";
    return ok;
#elif defined(Q_OS_LINUX)
    return run("store", name, value.toUtf8(), nullptr, nullptr, error);
#elif defined(Q_OS_MACOS)
    return MacCredentialStore::write(account(name), value, error);
#else
    if (error)
        *error = "No secure credential backend is available on this platform.";
    return false;
#endif
}

bool CredentialStore::remove(const QString& name, QString* error)
{
    if (DevelopmentProfile::active()) {
        if (error) *error = "Secure storage unavailable in an isolated qualification profile. Credentials were not accessed.";
        return false;
    }
    if (error)
        error->clear();
#ifdef Q_OS_WIN
    const auto id = target(name).toStdWString();
    if (CredDeleteW(id.c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND)
        return true;
    if (error)
        *error = "Windows Credential Manager could not remove the credential.";
    return false;
#elif defined(Q_OS_LINUX)
    return run("clear", name, { }, nullptr, nullptr, error);
#elif defined(Q_OS_MACOS)
    return MacCredentialStore::remove(account(name), error);
#else
    if (error)
        *error = "No secure credential backend is available on this platform.";
    return false;
#endif
}

QString CredentialStore::backendName()
{
#ifdef Q_OS_WIN
    return "Windows Credential Manager";
#elif defined(Q_OS_LINUX)
    return "Linux Secret Service";
#elif defined(Q_OS_MACOS)
#ifdef RAYNMEA_MACOS_KEYCHAIN_INTEGRATION
    return "macOS Keychain (isolated integration fixture)";
#elif defined(RAYNMEA_TESTING)
    return "macOS Keychain (unit test double)";
#else
    return "macOS Keychain";
#endif
#else
    return "Unavailable";
#endif
}
