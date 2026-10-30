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

class SetupTests : public QObject {
    Q_OBJECT
private slots:
    void pendingApprovedAndNoDuplicates() {
        TestServer server; SignalKSetupClient client; client.setTimeouts(1000, 3000, 20);
        QSignalSpy tokens(&client, &SignalKSetupClient::tokenApproved);
        client.requestAccess(server.config()); client.requestAccess(server.config());
        QTRY_COMPARE(server.posts, 1); QTRY_VERIFY(server.polls > 0);
        QCOMPARE(server.accessBody.value("permissions").toString(), QString("readwrite"));
        QVERIFY(!server.accessBody.contains("password"));
        server.accessState = "COMPLETED";
        QTRY_COMPARE(tokens.size(), 1); QCOMPARE(tokens.first().first().toString(), QString("test-token"));
        QVERIFY(!client.isBusy()); QCOMPARE(server.posts, 1);
    }
    void deniedAndRetry() {
        TestServer server; SignalKSetupClient client; client.setTimeouts(1000, 3000, 20);
        QSignalSpy status(&client, &SignalKSetupClient::statusChanged), tokens(&client, &SignalKSetupClient::tokenApproved);
        server.accessState = "COMPLETED"; server.permission = "DENIED";
        client.requestAccess(server.config()); QTRY_VERIFY(!client.isBusy());
        QVERIFY(status.last().first().toString().contains("denied")); QVERIFY(tokens.isEmpty());
        server.permission = "APPROVED"; client.requestAccess(server.config());
        QTRY_COMPARE(tokens.size(), 1); QCOMPARE(server.posts, 2);
    }
    void cancelResumeAndExpiry() {
        TestServer server; SignalKSetupClient client; client.setTimeouts(1000, 150, 20);
        QSignalSpy tokens(&client, &SignalKSetupClient::tokenApproved), status(&client, &SignalKSetupClient::statusChanged);
        // Cancel synchronously at the first observed poll. QTRY's 50ms polling
        // can otherwise allow a second 20ms request to be in flight already.
        server.afterPoll = [&client] { client.cancel(); };
        client.requestAccess(server.config()); QTRY_VERIFY(server.polls > 0);
        server.afterPoll = {};
        const int polls = server.polls; QTest::qWait(100); QCOMPARE(server.polls, polls);
        client.requestAccess(server.config()); QTRY_VERIFY(!client.isBusy());
        QVERIFY(status.last().first().toString().contains("expired")); QCOMPARE(server.posts, 1);
        server.accessState = "COMPLETED"; client.requestAccess(server.config()); QTRY_COMPARE(tokens.size(), 1);
        QCOMPARE(server.posts, 1);
    }
    void cancelledSubmissionDoesNotDuplicate() {
        TestServer server; SignalKSetupClient client;
        QSignalSpy status(&client, &SignalKSetupClient::statusChanged), tokens(&client, &SignalKSetupClient::tokenApproved);
        client.requestAccess(server.config()); client.cancel();
        client.requestAccess(server.config()); QVERIFY(!client.isBusy());
        QVERIFY(status.last().first().toString().contains("outcome is unknown"));
        QTest::qWait(100); QVERIFY(server.posts <= 1); QVERIFY(tokens.isEmpty());
    }
    void malformedRequest() {
        TestServer server; server.malformedAccess = true; SignalKSetupClient client;
        QSignalSpy status(&client, &SignalKSetupClient::statusChanged);
        client.requestAccess(server.config()); QTRY_VERIFY(!client.isBusy());
        QVERIFY(status.last().first().toString().contains("Invalid server response"));
    }
    void failedAccessAndRetry() {
        TestServer server; server.accessCode = 403; SignalKSetupClient client;
        QSignalSpy status(&client, &SignalKSetupClient::statusChanged), tokens(&client, &SignalKSetupClient::tokenApproved);
        client.requestAccess(server.config()); QTRY_VERIFY(!client.isBusy());
        QVERIFY(status.last().first().toString().contains("refused")); QVERIFY(tokens.isEmpty());
        server.accessCode = 0; server.accessState = "COMPLETED";
        client.requestAccess(server.config()); QTRY_COMPARE(tokens.size(), 1); QCOMPARE(server.posts, 2);
    }
    void expiredServerRequest() {
        TestServer server; server.accessState = "EXPIRED"; SignalKSetupClient client;
        QSignalSpy status(&client, &SignalKSetupClient::statusChanged), tokens(&client, &SignalKSetupClient::tokenApproved);
        client.requestAccess(server.config()); QTRY_VERIFY(!client.isBusy());
        QVERIFY(status.last().first().toString().contains("expired")); QVERIFY(tokens.isEmpty());
        server.accessState = "COMPLETED"; client.requestAccess(server.config()); QTRY_COMPARE(tokens.size(), 1);
    }
    void readinessWithoutMeasurements() {
        TestServer server; SignalKSetupClient client; QSignalSpy ready(&client, &SignalKSetupClient::connectionReady);
        client.testConnection(server.config()); QTRY_COMPARE(ready.size(), 1);
        QVERIFY(server.headerSeen); QVERIFY(server.subscribedNone); QVERIFY(server.deltas.isEmpty()); QVERIFY(!client.isBusy());
        QTest::qWait(50); QVERIFY(server.deltas.isEmpty());
    }
    void tokenFailures_data() {
        QTest::addColumn<QString>("token"); QTest::addColumn<bool>("writer"); QTest::addColumn<int>("http");
        QTest::newRow("missing") << QString() << true << 200;
        QTest::newRow("invalid") << QString("invalid") << true << 200;
        QTest::newRow("readonly") << QString("test-token") << false << 200;
        QTest::newRow("unauthorized") << QString("test-token") << true << 401;
        QTest::newRow("forbidden") << QString("test-token") << true << 403;
    }
    void tokenFailures() {
        QFETCH(QString, token); QFETCH(bool, writer); QFETCH(int, http);
        TestServer server; server.writer = writer; server.loginCode = http;
        SignalKSetupClient client; auto config = server.config(); config.token = token;
        QSignalSpy ready(&client, &SignalKSetupClient::connectionReady), status(&client, &SignalKSetupClient::statusChanged);
        client.testConnection(config); QTRY_VERIFY(!client.isBusy());
        QVERIFY(ready.isEmpty()); QVERIFY(server.deltas.isEmpty()); QVERIFY(!status.last().first().toString().isEmpty());
    }
    void handshakeTimeoutCancelAndRetry() {
        TestServer server; server.sendHello = false; SignalKSetupClient client; client.setTimeouts(150, 1000, 20);
        QSignalSpy ready(&client, &SignalKSetupClient::connectionReady), status(&client, &SignalKSetupClient::statusChanged);
        client.testConnection(server.config()); QTRY_VERIFY(!client.isBusy());
        QVERIFY(status.last().first().toString().contains("timed out")); QVERIFY(ready.isEmpty());
        client.testConnection(server.config()); client.cancel(); QTest::qWait(200); QVERIFY(ready.isEmpty());
        server.sendHello = true; client.setTimeouts(1000, 1000, 20);
        client.testConnection(server.config()); QTRY_COMPARE(ready.size(), 1); QVERIFY(server.deltas.isEmpty());
    }
    void connectionRefusedAndUpgradeFailure() {
        TestServer server; SignalKSetupClient client; QSignalSpy ready(&client, &SignalKSetupClient::connectionReady);
        auto config = server.config(); server.http.close(); client.testConnection(config); QTRY_VERIFY(!client.isBusy()); QVERIFY(ready.isEmpty());
        TestServer other; other.rejectWebSocket = true; client.testConnection(other.config()); QTRY_VERIFY(!client.isBusy()); QVERIFY(ready.isEmpty());
    }
    void validationAndDestruction() {
        SignalKSetupClient client; SignalKConfig config;
        for (const auto &url : {"http://user:secret@host", "http://host/path", "http://host?token=x", "file:///tmp"}) {
            config.server = url; client.requestAccess(config); QVERIFY(!client.isBusy());
        }
        TestServer server; auto *pending = new SignalKSetupClient;
        pending->setTimeouts(1000, 1000, 20);
        server.afterPoll = [&pending] { delete pending; pending = nullptr; };
        pending->requestAccess(server.config()); QTRY_VERIFY(pending == nullptr);
        server.afterPoll = {};
        const auto polls = server.polls; QTest::qWait(100); QCOMPARE(server.polls, polls);
    }
};
QTEST_GUILESS_MAIN(SetupTests)
#include "signalk_setup_tests.moc"
