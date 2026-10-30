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
#pragma once
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>

namespace AboutLinks {
inline constexpr char SupportUrl[] = "https://www.paypal.com/ncp/payment/4SEDB7WY59T84";
using UrlOpener = bool (*)(const QUrl &);
// Match BrickSuite's explicit browser activation and selectable plain-text fallback.
inline void open(QWidget *parent, const QUrl &url, UrlOpener opener = &QDesktopServices::openUrl) {
    if (url.scheme() == "https" && opener(url)) return;
    QMessageBox warning(QMessageBox::Warning, QObject::tr("Open Link"),
        QObject::tr("Could not open this page in your web browser. You can open this address manually:\n\n%1").arg(url.toString()),
        QMessageBox::Ok, parent);
    warning.setTextFormat(Qt::PlainText);
    warning.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    warning.exec();
}
}
