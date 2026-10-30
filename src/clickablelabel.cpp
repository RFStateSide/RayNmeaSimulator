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

#include "clickablelabel.h"
#include <QMenu>

// Constructor for UI setup
ClickableLabel::ClickableLabel(QWidget *parent)
    : QLabel(parent)
{

}

// Constructor with text and menu items
ClickableLabel::ClickableLabel(const QString &text, const QList<QString> &menuItems, QWidget *parent)
    : QLabel(text, parent), m_menuItems(menuItems)
{
    setMenuItems(menuItems);
}

// Method to set menu items
void ClickableLabel::setMenuItems(const QList<QString> &menuItems)
{
    m_menuItems = menuItems;
    if (!menuItems.isEmpty() && toolTip().isEmpty())
        setToolTip(tr("Click to choose this widget's display units or format. Stored Property Editor values are unchanged."));
}

// Method to set label text
void ClickableLabel::setLabelText(const QString &text)
{
    setText(text);
}

void ClickableLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QMenu contextMenu(this);
        QList<QAction*> actions;

        // Populate the menu using the provided list of QStrings
        for (const QString &item : m_menuItems) {
            QAction *action = contextMenu.addAction(item);
            actions.append(action);
        }

        // Use globalPosition() and convert to QPoint for compatibility with exec()
        QAction *selectedAction = contextMenu.exec(event->globalPosition().toPoint());
        if (selectedAction) {
            int index = actions.indexOf(selectedAction);
            if (index != -1) {
                emit actionTriggered(index);
            }
        }
    }
    QLabel::mousePressEvent(event); // Call base class implementation
}
