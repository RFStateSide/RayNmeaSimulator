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

#include <QtTest>
#include "viewer_test_driver.h"
#include <QAction>
#include <QTemporaryDir>
#include <QMessageBox>
#include "appsettings.h"
#include "mainwindow.h"
#include "transportcapabilities.h"
#include "actisensecodec.h"
#include "latlong129025.h"

class PolicyPort : public QSerialPort {
public:
    QByteArray input, output;
    bool open(OpenMode mode) override { return QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { QIODevice::close(); input.clear(); }
    qint64 bytesAvailable() const override { return input.size(); }
    void feed(const QByteArray &bytes) { input += bytes; emit readyRead(); }
protected:
    qint64 readData(char *data, qint64 size) override {
        const auto n = qMin(size, qint64(input.size()));
        memcpy(data, input.constData(), n); input.remove(0, n); return n;
    }
    qint64 writeData(const char *data, qint64 size) override { output.append(data, size); return size; }
};

class ActisensePolicyTests : public QObject {
    Q_OBJECT
    QTemporaryDir profile{QDir::currentPath()+"/build/m231u-profile-XXXXXX"};
private slots:
    void initTestCase() {
        QVERIFY(profile.isValid()); qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
        QApplication::setStyle("Fusion");
    }
    void init() {
        AppSettings s(APP_COMPANY, APP_NAME); s.clear();
        s.setValue("Nmea2000/Nmea2000Enabled", true);
        s.setValue("Nmea2000/InterfaceType", "Actisense");
        s.setValue("Nmea2000/ActisensePort", "memory");
        s.setValue("Sending/Enabled", true); // Untrusted persisted authorization.
    }
    void buildPolicy() {
        // Assert against the actual Qt build flags, independently of the policy.
#ifdef QT_NO_DEBUG
        QVERIFY(!TransportCapabilities::canTransmit("Actisense"));
#else
        QVERIFY(TransportCapabilities::canTransmit("Actisense"));
#endif
        QVERIFY(TransportCapabilities::canTransmit("CANable SLCAN"));
        QVERIFY(TransportCapabilities::canTransmit("Waveshare USB-CAN-A"));
        QVERIFY(TransportCapabilities::canTransmit("SocketCan"));
        QVERIFY(TransportCapabilities::canTransmit("SignalK"));
    }
    void adapterAndHandlerBoundaries() {
        auto *port = new PolicyPort;
        Nmea2000Handler h(nullptr, port); h.SetInterfaceType("Actisense"); h.setMode(Sender);
        QVERIFY(h.InitializeActisense("memory", QSerialPort::Baud115200, "0x94"));
        QVERIFY(port->isOpen()); QVERIFY(port->output.isEmpty());
        QCOMPARE(h.sendTestPgn129026(), TransportCapabilities::experimentalActisenseTx);
        if (!TransportCapabilities::experimentalActisenseTx) QVERIFY(port->output.isEmpty());
        h.setMode(Viewer); port->output.clear(); QVERIFY(!h.sendTestPgn129026()); QVERIFY(port->output.isEmpty());
        QSignalSpy received(&h, &Nmea2000Handler::RcvdPgn);
        ActisenseCodec::Message msg; msg.pgn = 129025; msg.priority = 6; msg.destination = 255;
        msg.length = 8; msg.data = {0x40,0x42,0x0f,0,0x80,0x84,0x1e,0};
        const auto frame = ActisenseCodec::encode(msg, 0x93);
        port->feed(QByteArray(reinterpret_cast<const char*>(frame.bytes.data()), frame.size));
        QTRY_COMPARE(received.size(), 1);
        h.ShutdownActisense(); QVERIFY(!port->isOpen());
        QVERIFY(h.InitializeActisense("memory", QSerialPort::Baud115200, "0x94"));
        QVERIFY(port->output.isEmpty());
        PolicyPort direct; NMEA2000_Actisense adapter(direct); QVERIFY(adapter.CANOpen());
        tN2kMsg raw; raw.SetPGN(129025); raw.AddByte(1);
        QCOMPARE(adapter.SendMessage(raw), TransportCapabilities::experimentalActisenseTx);
        if (!TransportCapabilities::experimentalActisenseTx) QVERIFY(direct.output.isEmpty());
    }
    void setupAndAuthorization() {
        QTimer modalWatchdog;
        connect(&modalWatchdog, &QTimer::timeout, this, [] {
            if (auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                QTest::qFail(qPrintable("Unexpected modal dialog: " + box->text()), __FILE__, __LINE__);
                box->reject();
            }
        });
        modalWatchdog.start(10000);
        auto *port = new PolicyPort;
        MainWindow w(nullptr, port); w.show();
        auto *start = w.findChild<QAction*>("actionStart_Sending");
        auto *worker = w.findChild<NmeaSenderWorker*>();
        auto *controller = w.findChild<MainController*>();
        auto *model = w.findChild<PgnTableModel*>();
        QVERIFY(!start->isChecked()); QVERIFY(!worker->isSending()); QVERIFY(port->isOpen());
        controller->removeAllWidgets();
        controller->setMode(Viewer); // Receive fixture creation must not offer generators.
        controller->addRcvdPgn("129025", {"Latitude", "42", "Longitude", "-70"}, "123");
        controller->setMode(Sender);
        const auto positions = model->getPgns("129025"); QVERIFY(!positions.isEmpty());
        auto *position = w.findChild<LatLong129025*>(); QVERIFY(position);
        position->setEnabled(true);
        QTemporaryDir layouts{QDir::currentPath()+"/build/m231u-layout-XXXXXX"};
        const auto path = layouts.filePath("enabled.json");
        QVERIFY(controller->saveLayoutToJson(path));
        controller->removeAllWidgets(); // Avoid the normal interactive replace confirmation.
        QVERIFY(controller->loadLayoutFromJson(path));
        QVERIFY(model->getPgns("129025").first().enabled);
        QCOMPARE(start->isEnabled(), TransportCapabilities::experimentalActisenseTx);
        start->setChecked(true); // Bypass disabled QAction presentation.
        QCOMPARE(worker->isSending(), TransportCapabilities::experimentalActisenseTx);
        QCOMPARE(start->isChecked(), TransportCapabilities::experimentalActisenseTx);
        auto *setup = w.findChild<DialogSetup*>();
        // Model an enumerated port for Save; no operating-system port is opened.
        setup->findChild<QComboBox*>("comboBoxNmea2000ActisensePort")->addItem("memory");
        w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();
        setup->findChild<QTabWidget*>("tabWidget")->setCurrentIndex(1);
        auto *notice = setup->findChild<QLabel*>("actisenseCapabilityNotice");
        QVERIFY(notice->isVisible());
        QVERIFY(notice->text().contains(TransportCapabilities::experimentalActisenseTx ? "experimental" : "receive-only"));
        QTest::keyClick(setup, Qt::Key_F12); QVERIFY(!worker->isSending()); QVERIFY(!start->isChecked());
        auto *type = setup->findChild<QComboBox*>("comboBoxNmea2000Interface");
        for (const auto &name : {"CANable SLCAN", "Waveshare USB-CAN-A", "SocketCan", "SignalK"}) {
            type->setCurrentText(name); QVERIFY(!notice->isVisible());
        }
        setup->reject(); w.findChild<QAction*>("actionOpen_Setup_Form")->trigger();
        QCOMPARE(type->currentText(), QString("Actisense")); QVERIFY(notice->isVisible());
        QVERIFY(QMetaObject::invokeMethod(setup, "on_pushButtonSave_clicked"));
        QVERIFY(!worker->isSending()); QVERIFY(!start->isChecked());
        QVERIFY(TestDialogs::enterViewerKeepingLayout(w));
        QVERIFY(port->isOpen()); QVERIFY(!start->isEnabled());
        w.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);
        QVERIFY(!worker->isSending());
        if (!TransportCapabilities::experimentalActisenseTx) {
            QVERIFY(!start->isEnabled()); QVERIFY(port->output.isEmpty());
            QVERIFY(w.findChild<QLabel*>("sendingStatus")->toolTip().contains("receive-only"));
        }
        setup->reject();
    }
};
QTEST_MAIN(ActisensePolicyTests)
#include "actisense_policy_tests.moc"
