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

#ifndef STANDALONE_ACCEPTANCE_H
#define STANDALONE_ACCEPTANCE_H
#include <QtTest>
#include <QAction>
#include <QSpinBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QNetworkProxy>
#include "maincontroller.h"
#include "temperaturewidget130312.h"
#include "appsettings.h"
#include "apppaths.h"
#include "dialogsetup.h"
#include <stdexcept>

// Opt-in automation of the real standalone entry point and MainWindow menus.
inline void standaloneAcceptance()
{
    try {
        auto require = [](bool ok, const char *what) { if (!ok) throw std::runtime_error(what); };
        MainWindow *window = nullptr;
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *candidate = qobject_cast<MainWindow*>(widget)) window = candidate;
        require(window, "MainWindow missing");
        AppSettings settings(APP_COMPANY, APP_NAME);
        require(settings.format() == QSettings::IniFormat && !settings.fallbacksEnabled(), "settings not isolated");
        require(settings.value("Vessel/NumEngines").toInt() == 2, "private settings not loaded");
        auto *controller = window->findChild<MainController*>();
        auto *table = window->findChild<PgnTableController*>();
        require(controller && table, "controllers missing");
        auto action = [&](const char *name) {
            auto *item = window->findChild<QAction*>(name);
            require(item, "menu action missing"); item->trigger();
        };
        for (int cycle = 0; cycle < 2; ++cycle) {
            if (!table->getPgnTableModel()->getPgnList().isEmpty()) {
                QTimer::singleShot(0, [] {
                    for (auto *w : QApplication::topLevelWidgets())
                        if (auto *d = qobject_cast<QMessageBox*>(w)) d->button(QMessageBox::Yes)->click();
                });
            }
            action("actionLoad_Default");
            require(table->getPgnTableModel()->getPgnList().size() == 35, "default layout count");
            QTimer::singleShot(0, [] {
                for (auto *w : QApplication::topLevelWidgets())
                    if (auto *d = qobject_cast<QDialog*>(w); d && d->windowTitle() == "Select Temperature Source") d->accept();
            });
            controller->addWidgetAsComboboxTextChanged("130312", "Temperature", "Environment");
            if (cycle)
                for (auto *engine : window->findChildren<GeneratorEngine*>()) engine->setEngineRunning(true);
            QTest::qWait(1200);
            const QString name = QString("F1-acceptance-%1").arg(cycle);
            QTimer::singleShot(0, [name] {
                for (auto *w : QApplication::topLevelWidgets())
                    if (auto *d = qobject_cast<QInputDialog*>(w)) { d->setTextValue(name); d->accept(); }
            });
            action("actionSave");
            require(QFile::exists(AppPaths::layoutsDirectory()+"/"+name+".json"), "saved layout missing");
            QTimer::singleShot(0, [] {
                for (auto *w : QApplication::topLevelWidgets())
                    if (auto *d = qobject_cast<QMessageBox*>(w)) d->button(QMessageBox::Yes)->click();
            });
            action("actionClear_All");
            require(table->getPgnTableModel()->getPgnList().isEmpty(), "clear failed");
            QTest::qWait(100);
            QTimer::singleShot(0, [name] {
                for (auto *w : QApplication::topLevelWidgets())
                    if (auto *d = qobject_cast<QDialog*>(w); d && d->windowTitle() == "Load Layout") {
                        d->findChild<QComboBox*>()->setCurrentText(name); d->accept();
                    }
            });
            action("actionLoad");
            require(table->getPgnTableModel()->getPgnList().size() == 36, "reload count");
            auto *temperature = window->findChild<TemperatureWidget130312*>();
            require(temperature && temperature->TemperatureSource() == N2kts_SeaTemperature
                    && temperature->SetTemperature() == 300.0, "restored temperature");
        }
        bool saved = false;
        QTimer::singleShot(0, [&saved, window] {
            auto *setup = window->findChild<DialogSetup*>();
            if (!setup) return;
            auto *columns = setup->findChild<QSpinBox*>("spinBoxWidgetColumns");
            if (columns) {
                columns->setValue(3);
                saved = QMetaObject::invokeMethod(setup, "on_pushButtonSave_clicked", Qt::DirectConnection);
            }
            setup->accept();
        });
        action("actionOpen_Setup_Form");
        settings.sync();
        require(saved && settings.value("Preference/WidgetColumns").toInt() == 3, "setup write not in private settings");
        qInfo("PASS standalone: private settings read/write, default, Sea Temperature, save/clear/reload, inactive/active engines");
        window->close();
    } catch (const std::exception &error) {
        qCritical() << "FAIL standalone acceptance:" << error.what();
        QCoreApplication::exit(1);
    }
}

inline void scheduleStandaloneAcceptance()
{
    if (qEnvironmentVariable("RAYNMEA_ACCEPTANCE_SMOKE") != "1") return;
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    QTimer::singleShot(180000, [] {
        qCritical("FAIL standalone acceptance: timed out"); QCoreApplication::exit(2);
    });
    QTimer::singleShot(0, [] {
        if (!QCoreApplication::arguments().contains("--no-nmea")) {
            qCritical("Standalone acceptance requires --no-nmea"); QCoreApplication::exit(1); return;
        }
        standaloneAcceptance();
    });
}
Q_COREAPP_STARTUP_FUNCTION(scheduleStandaloneAcceptance)
#endif
