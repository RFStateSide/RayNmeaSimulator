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

#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include "networkcansession.h"
#include "N2kMessages.h"
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto args = app.arguments();
    if (args.size() != 8)
        return 2;
    NetworkCanConfig c;
    c.host = args[1];
    c.port = args[2].toInt();
    c.interfaceName = args[3];
    QFile key(args[4]);
    if (!key.open(QIODevice::ReadOnly))
        return 3;
    c.token = QString::fromUtf8(key.readAll()).trimmed();
    c.uniqueNumber = args[5].toUInt();
    c.allowTx = true;
    c.preferredSource = 45;
    const auto role = args[6];
    if (role != "rx" && !(role == "tx" && (c.interfaceName == "lab" ||
                                           c.interfaceName.startsWith("vcan"))))
        return 4;
    NetworkCanSession session;
    int received = 0, sent = 0;
    bool ready = false, failed = false;
    QObject::connect(&session, &NetworkCanSession::received,
                     [&](const tN2kMsg &m) {
                         if (m.PGN == 129025)
                             ++received;
                     });
    QObject::connect(&session, &NetworkCanSession::failed,
                     [&](const QString &) {
                         failed = true;
                         app.quit();
                     });
    session.configure(c);
    if (role == "tx")
        session.start("tx");
    QTimer send;
    QObject::connect(&send, &QTimer::timeout, [&] {
        ready |= session.ready();
        if (role == "tx" && session.transmitting() && sent < 5) {
            tN2kMsg m;
            SetN2kLatLonRapid(m, 10.1234567, 20.1234567);
            if (session.send(m))
                ++sent;
        }
    });
    send.start(200);
    QTimer::singleShot(args[7].toInt(), &app, &QCoreApplication::quit);
    app.exec();
    session.stop();
    auto b = QJsonDocument(QJsonObject{{"ready", ready},
                                       {"failed", failed},
                                       {"position_rx", received},
                                       {"position_tx", sent}})
                 .toJson(QJsonDocument::Compact);
    std::puts(b.constData());
    return ready && !failed ? 0 : 1;
}
