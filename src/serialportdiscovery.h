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

#ifndef SERIALPORTDISCOVERY_H
#define SERIALPORTDISCOVERY_H

#include <QComboBox>
#include <QLineEdit>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <algorithm>

namespace SerialPortDiscovery {
struct Device {
    QString path, description, manufacturer;
    bool usb = false;
    QString vidPid;
};

inline QList<Device> available()
{
    QList<Device> devices;
    for (const auto &port : QSerialPortInfo::availablePorts()) {
        Device d;
#ifdef Q_OS_WIN
        d.path = port.portName(); // Preserve the established COM-port settings format.
#else
        d.path = port.systemLocation();
#endif
        d.description = port.description(); d.manufacturer = port.manufacturer();
        d.usb = port.hasVendorIdentifier() || port.hasProductIdentifier();
        if (port.hasVendorIdentifier() && port.hasProductIdentifier())
            d.vidPid = QString("%1:%2").arg(port.vendorIdentifier(), 4, 16, QLatin1Char('0'))
                .arg(port.productIdentifier(), 4, 16, QLatin1Char('0'));
        devices.append(d);
    }
    return devices;
}

inline QString label(const Device &d)
{
    QStringList parts{d.path};
    if (!d.description.isEmpty()) parts << d.description;
    if (!d.manufacturer.isEmpty() && d.manufacturer != d.description) parts << d.manufacturer;
    if (!d.vidPid.isEmpty()) parts << "USB " + d.vidPid;
    return parts.join(" | ");
}

inline void initialize(QComboBox *box)
{
    box->setEditable(true);
    box->setInsertPolicy(QComboBox::NoInsert);
    box->setCompleter(nullptr); // Completion must never put a descriptive label in the device path.
    box->setMinimumContentsLength(25);
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    const auto selectPath = [box](int index) {
        if (index >= 0) box->setEditText(box->itemData(index).toString());
    };
    QObject::connect(box, &QComboBox::currentIndexChanged, box, selectPath);
    QObject::connect(box, &QComboBox::activated, box, selectPath);
}

inline void refresh(QComboBox *box, QList<Device> devices, bool showAll)
{
    const QString selected = box->currentText();
    const QSignalBlocker blockBox(box), blockEdit(box->lineEdit());
    std::sort(devices.begin(), devices.end(), [](const Device &a, const Device &b) {
        if (a.usb != b.usb) return a.usb;
        return a.path.compare(b.path, Qt::CaseInsensitive) < 0;
    });
    box->clear(); box->addItem("None", "None");
    for (const auto &d : devices) {
        if (!showAll && !d.usb) continue;
        box->addItem(label(d), d.path);
        box->setItemData(box->count()-1, label(d), Qt::ToolTipRole);
    }
    // Retain saved, disconnected and manually entered paths verbatim. Refresh is not a settings edit.
    const QString path = selected.isEmpty() ? QString("None") : selected;
    box->setCurrentIndex(box->findData(path));
    box->setEditText(path);
}
}
#endif
