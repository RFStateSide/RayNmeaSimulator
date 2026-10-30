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

// Run the real entry point against the isolated object variant and fresh profile.
#include "appsettings.h"
#include "nmeasenderworker.h"
#include <QNetworkProxy>
#include <QTimer>
#include <cstdio>
#define main applicationMain
#include "../src/main.cpp"
#undef main

static void scheduleStartupCheck()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    QTimer::singleShot(0, [] {
        MainWindow *window=nullptr;
        for(auto *widget:QApplication::topLevelWidgets())
            if(auto *candidate=qobject_cast<MainWindow*>(widget))window=candidate;
        AppSettings settings(APP_COMPANY,APP_NAME);
        auto *sender=window?window->findChild<NmeaSenderWorker*>():nullptr;
        if(!window || !sender || sender->isSending() || settings.format()!=QSettings::IniFormat
            || QApplication::applicationVersion()!=QString(APP_VERSION)) {
            std::fprintf(stderr,"FAIL isolated real-entry startup/state check\n");
            QCoreApplication::exit(1);
            return;
        }
        std::puts("PASS real application startup, identity, isolated INI and stopped Sending");
        window->close();
    });
    QTimer::singleShot(10000, [] {QCoreApplication::exit(2);});
}
Q_COREAPP_STARTUP_FUNCTION(scheduleStartupCheck)
int main(int argc,char **argv)
{
    TestProfile::root();
    QList<QByteArray> arguments;
    for(int i=0;i<argc;++i)arguments.append(argv[i]);
    arguments.append("--no-nmea");
    QList<char*> pointers;
    for(auto &argument:arguments)pointers.append(argument.data());
    int count=pointers.size(); pointers.append(nullptr);
    return applicationMain(count,pointers.data());
}
