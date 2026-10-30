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
#include "serialportdiscovery.h"
#include "serialportdiagnostics.h"

class SerialPortDiscoveryTests : public QObject {
    Q_OBJECT
    QList<SerialPortDiscovery::Device> devices{
        {"/dev/ttyS0", "Built-in serial", "", false, ""},
        {"COM17", "USB Serial", "Example maker", true, "1234:abcd"},
        {"/dev/ttyUSB9", "USB serial fixture", "Example maker", true, "4321:dcba"}};
private slots:
    void usbFirstAndMetadata() {
        QComboBox box; SerialPortDiscovery::initialize(&box);
        SerialPortDiscovery::refresh(&box, devices, false);
        QCOMPARE(box.count(), 3); QCOMPARE(box.currentText(), "None");
        QVERIFY(box.itemText(1).contains("Example maker"));
        QVERIFY(box.itemText(1).contains("USB 4321:dcba"));
        box.setCurrentIndex(1); QCOMPARE(box.currentText(), "/dev/ttyUSB9");
        QCOMPARE(box.itemData(1).toString(), "/dev/ttyUSB9");
        // Re-selecting the same row after manual editing does not change the index.
        box.setEditText(box.itemText(1));
        QVERIFY(QMetaObject::invokeMethod(&box, "activated", Q_ARG(int, 1)));
        QCOMPARE(box.currentText(), "/dev/ttyUSB9");
        SerialPortDiscovery::refresh(&box, devices, true);
        QCOMPARE(box.count(), 4); QCOMPARE(box.itemData(3).toString(), "/dev/ttyS0");
        QCOMPARE(box.currentText(), "/dev/ttyUSB9");
    }
    void hotplugManualAndDisconnected() {
        QComboBox box; SerialPortDiscovery::initialize(&box);
        box.setCurrentText("/dev/serial/by-id/owner-manual-path");
        QSignalSpy edits(&box, &QComboBox::currentTextChanged);
        SerialPortDiscovery::refresh(&box, devices, false);
        QCOMPARE(edits.size(), 0);
        QCOMPARE(box.currentText(), "/dev/serial/by-id/owner-manual-path");
        box.setCurrentIndex(2); QCOMPARE(box.currentText(), "COM17");
        SerialPortDiscovery::refresh(&box, {}, false);
        QCOMPARE(box.currentText(), "COM17"); QCOMPARE(box.count(), 1);
        SerialPortDiscovery::refresh(&box, devices, false);
        QCOMPARE(box.currentText(), "COM17"); QCOMPARE(box.currentData().toString(), "COM17");
        QVERIFY(box.itemText(2).contains("USB Serial"));
        box.setCurrentText("ttyUSB0"); // Legacy Linux saved port names remain usable.
        SerialPortDiscovery::refresh(&box, devices, false);
        QCOMPARE(box.currentText(), "ttyUSB0");
    }
    void failureAdvice() {
        for (const auto error : {QSerialPort::DeviceNotFoundError, QSerialPort::PermissionError,
                               QSerialPort::OpenError, QSerialPort::ResourceError}) {
            auto text = serialPortDiagnostic("fixture-device", error, "native detail");
            QVERIFY(text.contains("fixture-device")); QVERIFY(text.contains("native detail"));
            QVERIFY(text.size() > 40);
        }
        QVERIFY(serialPortDiagnostic("x", QSerialPort::PermissionError, "denied").contains("busy"));
        QVERIFY(serialPortDiagnostic("x", QSerialPort::ResourceError, "gone").contains("disconnected"));
        QSerialPort missing; missing.setPortName("raynmea-definitely-missing-port");
        QVERIFY(!missing.open(QIODevice::ReadWrite));
        QVERIFY(serialPortDiagnostic(missing.portName(), missing.error(), missing.errorString()).contains("missing"));
    }
    void hostDiscovery() {
        const auto found = SerialPortDiscovery::available();
        int usb = 0;
        for (const auto &d : found) {
            QVERIFY(!d.path.isEmpty());
            if (d.usb) { ++usb; qInfo().noquote() << SerialPortDiscovery::label(d); }
        }
        qInfo() << "Detected ports:" << found.size() << "USB:" << usb;
    }
};
QTEST_MAIN(SerialPortDiscoveryTests)
#include "serial_port_discovery_tests.moc"
