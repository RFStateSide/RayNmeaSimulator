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

#ifndef WINDOWSTATE_H
#define WINDOWSTATE_H

#include <QGuiApplication>
#include <QMainWindow>
#include <QScreen>
#include <QSettings>

// Desktop window state is deliberately independent of instrument Layout files.
namespace WindowState {
inline QRect boundedFrame(QRect frame, const QList<QRect>& screens, int primary = 0)
{
    if (screens.isEmpty()) return frame;
    int selected = qBound(0, primary, int(screens.size()) - 1);
    qint64 largest = 0;
    for (int i = 0; i < screens.size(); ++i) {
        const QRect intersection = screens[i].intersected(frame);
        const qint64 area = qint64(intersection.width()) * intersection.height();
        if (area > largest) { largest = area; selected = i; }
    }
    const QRect available = screens[selected];
    if (!frame.isValid()) frame = QRect(QPoint(), QSize(1100, 700));
    frame.setSize(frame.size().boundedTo(available.size()));
    if (!largest) frame.moveCenter(available.center());
    frame.moveLeft(qBound(available.left(), frame.left(), available.right() - frame.width() + 1));
    frame.moveTop(qBound(available.top(), frame.top(), available.bottom() - frame.height() + 1));
    return frame;
}

inline void ensureVisible(QWidget& window)
{
    if (window.isMaximized() || window.isMinimized()) return;
    QList<QRect> screens;
    int primary = 0;
    for (auto* screen : QGuiApplication::screens()) {
        if (screen == QGuiApplication::primaryScreen()) primary = screens.size();
        screens << screen->availableGeometry();
    }
    const QRect oldFrame = window.frameGeometry();
    const QRect frame = boundedFrame(oldFrame, screens, primary);
    if (oldFrame == frame) return;
    const QSize decoration = oldFrame.size() - window.size();
    window.resize((frame.size() - decoration).expandedTo(QSize(1, 1)));
    window.move(frame.topLeft());
}

inline void restore(QMainWindow& window, QSettings& settings)
{
    settings.beginGroup("Window");
    const auto geometry = settings.value("Geometry").toByteArray();
    const auto state = settings.value("Panels").toByteArray();
    const auto defaults = window.saveState(1);
    if (!geometry.isEmpty() && !window.restoreGeometry(geometry)) {
        window.resize(1100, 700);
        if (auto* screen = QGuiApplication::primaryScreen())
            window.move(screen->availableGeometry().center() - window.rect().center());
    }
    // A minimized/full-screen previous session must reopen as a usable desktop window.
    window.setWindowState(window.windowState() & ~(Qt::WindowMinimized | Qt::WindowFullScreen));
    if (!state.isEmpty() && !window.restoreState(state, 1)) window.restoreState(defaults, 1);
    settings.endGroup();
    ensureVisible(window);
}

inline void save(const QMainWindow& window, QSettings& settings)
{
    settings.setValue("Window/Geometry", window.saveGeometry());
    settings.setValue("Window/Panels", window.saveState(1));
}
}
#endif
