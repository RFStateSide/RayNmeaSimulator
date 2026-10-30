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

#ifndef DESKTOPICONS_H
#define DESKTOPICONS_H

#include <QIcon>
#include <QMdiArea>
#include <QPalette>
#include <QTabBar>

namespace DesktopIcons {
inline QIcon forPalette(const QPalette& palette)
{
    Q_UNUSED(palette);
    // Owner-approved transparent artwork on every theme; never add a backing tile.
    return QIcon(QPixmap(":/images/RayNmeaSimulator_1024.png"));
}

inline void clearLayoutTabs(QMdiArea& area)
{
    // Qt copies each subwindow's inherited application icon into its MDI tab
    // when adding it and on WindowIconChange. Clear only the tab presentation.
    for (auto* tabs : area.findChildren<QTabBar*>(QString(), Qt::FindDirectChildrenOnly))
        for (int index = 0; index < tabs->count(); ++index)
            tabs->setTabIcon(index, QIcon());
}
}
#endif
