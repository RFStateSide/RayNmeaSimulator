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
#include <QAction>
#include <QAbstractButton>
#include <QMessageBox>
#include <QTimer>
#include <QLineEdit>
#include "mainwindow.h"
#include "appsettings.h"
#include "helper.h"
#include "credentialstore.h"
#include "generatorvesselmovement.h"

class SignalKApplicationTests : public QObject {
    Q_OBJECT
private slots:
    void senderViewerReloadAndShutdown() {
        TestServer server;
        AppSettings settings(APP_COMPANY,APP_NAME);
        settings.setValue("Vessel/NumEngines",2);
        settings.setValue("Nmea0183/Nmea0183Enabled",false);
        settings.setValue("Nmea2000/Nmea2000Enabled",true);
        settings.setValue("Nmea2000/InterfaceType","SignalK");
        settings.setValue("Nmea2000/SignalKServer",server.config().server);
        settings.setValue("Nmea2000/SignalKSource","application-test");
        settings.sync();
        {
            MainWindow window; // Uses isolated settings and the isolated OS credential namespace.
            QVERIFY(CredentialStore::write("SignalKToken", "test-token"));
            settings.sync();window.HotReloadSettings();
            auto *publisher=window.findChild<SignalKPublisher*>();QVERIFY(publisher);
            auto *worker=window.findChild<NmeaSenderWorker*>();QVERIFY(worker);
            auto *start=window.findChild<QAction*>("actionStart_Sending");QVERIFY(start);
            auto *stop=window.findChild<QAction*>("actionStop_Sending");QVERIFY(stop);
            auto *layout=window.findChild<QAction*>("actionLoad_Default");QVERIFY(layout);layout->trigger();
            auto *movement=window.findChild<GeneratorVesselMovement*>();QVERIFY(movement);
            movement->setTransmissionGear(N2kTG_Forward);
            movement->setTransmissionGearStbd(N2kTG_Reverse);
            movement->PitchChanged(); // Attitude updates must not overwrite either gear.
            QVERIFY(!publisher->isReady());QVERIFY(!worker->isSending());
            start->trigger();QVERIFY(!worker->isSending());
            QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(),3000);
            QVERIFY(worker->isSending());
            QTRY_VERIFY_WITH_TIMEOUT(server.deltas.size()>5,3000);
            auto lastValue=[&](const QString &path) {
                QJsonValue result;
                for(const auto &delta:server.deltas)
                    for(const auto &update:delta["updates"].toArray())
                        for(const auto &entry:update.toObject()["values"].toArray())
                            if(entry.toObject()["path"].toString()==path) result=entry.toObject()["value"];
                return result;
            };
            QTRY_COMPARE_WITH_TIMEOUT(lastValue("propulsion.0.transmission.gear").toString(),QString("Forward"),3000);
            QTRY_COMPARE_WITH_TIMEOUT(lastValue("propulsion.1.transmission.gear").toString(),QString("Reverse"),3000);
            server.deltas.clear();
            movement->PitchChanged();
            QTest::qWait(1500);
            QCOMPARE(lastValue("propulsion.1.transmission.gear").toString(),QString("Reverse"));
            auto *dialog=window.findChild<DialogSetup*>();QVERIFY(dialog);
            dialog->show();QCoreApplication::processEvents();
            auto *token=dialog->findChild<QLineEdit*>("signalKToken");QVERIFY(token);
            QCOMPARE(token->echoMode(),QLineEdit::Password);QVERIFY(token->text()=="test-token");
            dialog->hide();
            stop->trigger();QVERIFY(!worker->isSending());QVERIFY(!publisher->isReady());
            const int stopped=server.deltas.size();QTest::qWait(1200);QCOMPARE(server.deltas.size(),stopped);
            start->trigger();QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(),3000);
            QTimer::singleShot(0, [] {
                for (auto* widget : QApplication::topLevelWidgets())
                    if (auto* box = qobject_cast<QMessageBox*>(widget); box && box->objectName() == "viewerEntryDialog")
                        box->findChild<QAbstractButton*>("viewerKeepExisting")->click();
            });
            window.findChild<QAction*>("actionViewer_Rx")->trigger();
            QVERIFY(!worker->isSending());QVERIFY(!publisher->isReady());QVERIFY(!start->isEnabled());
            window.findChild<QAction*>("actionSimulator_Tx")->trigger();
            QVERIFY(start->isEnabled());QVERIFY(!worker->isSending());
            start->trigger();QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(),3000);
            window.HotReloadSettings();QVERIFY(!worker->isSending());QVERIFY(!publisher->isReady());QVERIFY(!start->isChecked());
            start->trigger();QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(),3000);
        }
        const int shutDown=server.deltas.size();QTest::qWait(1200);QCOMPARE(server.deltas.size(),shutDown);
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);QTemporaryDir profile;
    qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    SignalKApplicationTests tests; const int result = QTest::qExec(&tests,argc,argv);
    CredentialStore::remove("SignalKToken"); return result;
}
#include "signalk_application_tests.moc"
