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

#include "signalk_testserver.h"
#include "signalksetupclient.h"
#include "mainwindow.h"
#include "appsettings.h"
#include "helper.h"
#include "credentialstore.h"
#include <QAction>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class SetupApplicationTests : public QObject {
    Q_OBJECT
private slots:
    void controlsPersistenceAndLifecycle() {
        TestServer server; AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Nmea0183/Nmea0183Enabled", false);
        settings.setValue("Nmea2000/Nmea2000Enabled", true);
        settings.setValue("Nmea2000/InterfaceType", "SignalK");
        settings.setValue("Nmea2000/SignalKServer", server.config().server);
        settings.setValue("Nmea2000/SignalKSource", "setup-application"); settings.sync();
        MainWindow window;
        auto *dialog = window.findChild<DialogSetup*>(); QVERIFY(dialog);
        auto *client = dialog->findChild<SignalKSetupClient*>(); QVERIFY(client); client->setTimeouts(1000, 3000, 20);
        auto *worker = window.findChild<NmeaSenderWorker*>(); QVERIFY(worker);
        auto *token = dialog->findChild<QLineEdit*>("signalKToken"); QVERIFY(token);
        auto *request = dialog->findChild<QPushButton*>("signalKRequestAccess"); QVERIFY(request);
        auto *test = dialog->findChild<QPushButton*>("signalKTestConnection"); QVERIFY(test);
        auto *cancel = dialog->findChild<QPushButton*>("signalKCancel"); QVERIFY(cancel);
        auto *status = dialog->findChild<QLabel*>("signalKSetupStatus"); QVERIFY(status);
        dialog->show(); QCoreApplication::processEvents(); request->click();
        QTRY_VERIFY(server.polls > 0); QVERIFY(!request->isEnabled()); QVERIFY(!test->isEnabled()); QVERIFY(cancel->isEnabled());
        dialog->reject(); QVERIFY(!client->isBusy());
        // An already-sent HTTP request can reach the server after abort. Drain it,
        // then assert no new polling starts; cancellation cannot unsend bytes.
        QTest::qWait(100); const int polls = server.polls; QTest::qWait(100); QCOMPARE(server.polls, polls);
        dialog->show(); request->click(); server.accessState = "COMPLETED";
        QTRY_VERIFY(!token->text().isEmpty()); QCOMPARE(token->echoMode(), QLineEdit::Password); QCOMPARE(server.posts, 1);
        QSignalSpy ready(client, &SignalKSetupClient::connectionReady); test->click(); QTRY_COMPARE(ready.size(), 1);
        QVERIFY(!worker->isSending()); QVERIFY(server.deltas.isEmpty()); QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
        dialog->findChild<QPushButton*>("pushButtonSave")->click();
        settings.sync();
        QVERIFY(!settings.contains("Nmea2000/SignalKToken"));
        const auto stored = CredentialStore::read("SignalKToken");
        QVERIFY(stored.success); QVERIFY(stored.found); QVERIFY(stored.value == token->text());
        token->clear(); dialog->findChild<QPushButton*>("pushButtonLoad")->click(); QVERIFY(!token->text().isEmpty());
        QVERIFY(status->text().contains("Settings loaded")); test->click(); QTRY_COMPARE(ready.size(), 2);
        QVERIFY(!worker->isSending()); QVERIFY(server.deltas.isEmpty());
        // Manual entry survives Save, while Escape does not save a subsequent edit.
        token->setText("manual-fixture-token"); dialog->findChild<QPushButton*>("pushButtonSave")->click();
        token->setText("unsaved-fixture-token"); dialog->reject(); dialog->show(); QVERIFY(token->text() == QString("manual-fixture-token"));
        token->setText("close-fixture-token"); dialog->close(); dialog->show(); QVERIFY(token->text() == QString("close-fixture-token"));
        QSignalSpy lateApproval(client, &SignalKSetupClient::tokenApproved);
        const auto beforeClosePolls = server.polls;
        server.accessState = "PENDING"; request->click(); QTRY_VERIFY(client->isBusy());
        QTRY_COMPARE(server.posts, 2); QTRY_VERIFY(server.polls > beforeClosePolls); dialog->close(); QVERIFY(!client->isBusy());
        server.accessState = "COMPLETED"; // A late response must not approve a canceled operation.
        QTest::qWait(100); QCOMPARE(lateApproval.size(), 0);
        const auto closedPolls = server.polls; QTest::qWait(100); QCOMPARE(server.polls, closedPolls);
        server.accessState = "PENDING";
        dialog->show(); request->click(); QTRY_VERIFY(server.polls > closedPolls); QCOMPARE(server.posts, 2);
        QVERIFY(client->isBusy()); dialog->hide(); QVERIFY(!client->isBusy());
        QVERIFY(CredentialStore::remove("SignalKToken"));
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv); QTemporaryDir profile;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8()); SetupApplicationTests tests; const int result = QTest::qExec(&tests, argc, argv);
    CredentialStore::remove("SignalKToken"); return result;
}
#include "signalk_setup_application_tests.moc"
