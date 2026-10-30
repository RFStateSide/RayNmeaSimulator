/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
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

#include "mainwindow.h"
#include "themes.h"
#include "applicationlog.h"
#include "desktopicons.h"
#include "appsettings.h"
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QStyleFactory>
#include <QStatusBar>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    DevelopmentProfile::initialize();
    // Identity, style and icon must be established before creating any windows.
    QApplication::setApplicationName(APP_NAME);
    QGuiApplication::setApplicationDisplayName(APP_NAME);
    QApplication::setOrganizationName(APP_COMPANY);
    QApplication::setApplicationVersion(APP_VERSION);
    QApplication::setOrganizationDomain("rfstateside.com");
    QGuiApplication::setDesktopFileName("com.rfstateside.RayNmeaSimulator");
    QApplication::setWindowIcon(DesktopIcons::forPalette(a.palette()));
    Themes::initializeSystemStyle();
    const bool loggingReady = ApplicationLog::initialize();
    qInfo() << "Software Version:" << APP_VERSION;
    int result;
    {
        MainWindow w;
        if (!loggingReady)
            w.statusBar()->showMessage("Application logging could not start. See Tools > Application Log.");
        w.show();
        result = a.exec();
    }
    ApplicationLog::shutdown();
    return result;
}
