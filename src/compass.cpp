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

#include "compass.h"
#include <QPainter>
#include <QDebug>

Compass::Compass(QWidget *parent)
    : QWidget(parent),
    m_heading(0),
    m_waypointHeading(0),
    m_waypointNeedleEnabled(false),
    m_degreeTextColor(Qt::white),
    m_valueTextColor(Qt::white)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(175, 175);
}

void Compass::setHeading(float heading)
{
    if (heading != m_heading) {
        m_heading = heading;
        update(); // Trigger a repaint
    }
}

void Compass::setWaypointHeading(float heading)
{
    if (heading != m_waypointHeading) {
        m_waypointHeading = heading;
        update(); // Trigger a repaint
    }
}

void Compass::enableWaypointNeedle(bool enable)
{
    if (enable != m_waypointNeedleEnabled) {
        m_waypointNeedleEnabled = enable;
        update(); // Trigger a repaint
    }
}

BackgroundItem* Compass::addBackground(int radius)
{
    BackgroundItem *background = new BackgroundItem(radius);
    m_backgrounds.append(background);
    return background;
}

void Compass::setDegreeTextColor(const QColor &color)
{
    m_degreeTextColor = color;
    update();
}

void Compass::setValueTextColor(const QColor &color)
{
    m_valueTextColor = color;
    update();
}

void Compass::setValueSuffix(QString suffix)
{
    m_valueSuffix = suffix;
    update();
}

void Compass::setPrecision(int precision)
{
    m_percision = precision;
}

void Compass::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int side = qMin(width(), height());
    painter.setViewport((width() - side) / 2, (height() - side) / 2, side, side);
    painter.setWindow(-50, -50, 100, 100);

    // Draw backgrounds
    for (BackgroundItem *background : m_backgrounds) {
        QConicalGradient gradient(0, 0, 0);
        const auto &colors = background->colors();
        for (const auto &color : colors) {
            gradient.setColorAt(color.first, color.second);
        }
        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        int radius = background->radius();
        painter.drawEllipse(-radius, -radius, 2 * radius, 2 * radius);
    }

    // Rotate the painter based on the heading
    painter.rotate(-m_heading);

    // Draw the compass direction markers and minor ticks
    painter.setPen(Qt::black);
    for (int i = 0; i < 360; i += 15) {
        painter.save();
        painter.rotate(i);
        if (i % 30 == 0) {
            painter.drawLine(0, -43, 0, -50); // Major ticks
        } else {
            painter.drawLine(0, -45, 0, -50); // Minor ticks
        }
        painter.restore();
    }

    // Draw the degree numbers and compass directions closer to the outside of the compass
    painter.setFont(QFont("Arial", 8));
    painter.setPen(m_degreeTextColor);
    for (int i = 0; i < 360; i += 45) {
        painter.save();
        painter.rotate(i);
        painter.drawText(-10, -47, 20, 20, Qt::AlignCenter, QString::number(i));
        painter.restore();
    }

    // Draw the compass direction letters (N, E, S, W)
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(-10, -57, 20, 20, Qt::AlignCenter, "N");
    painter.rotate(90);
    painter.drawText(-10, -57, 20, 20, Qt::AlignCenter, "E");
    painter.rotate(90);
    painter.drawText(-10, -57, 20, 20, Qt::AlignCenter, "S");
    painter.rotate(90);
    painter.drawText(-10, -57, 20, 20, Qt::AlignCenter, "W");
    painter.rotate(90);

    // Draw the second (waypoint) needle if enabled
    if (m_waypointNeedleEnabled) {
        painter.save();
        painter.rotate(m_waypointHeading);
        painter.setPen(Qt::blue);
        painter.setBrush(Qt::blue);
        painter.drawPolygon(QPolygon() << QPoint(0, -30) << QPoint(-3, -35) << QPoint(3, -35));
        painter.restore();
    }

    // Reset rotation to draw the fixed pointer
    painter.resetTransform();
    painter.setViewport((width() - side) / 2, (height() - side) / 2, side, side);
    painter.setWindow(-50, -50, 100, 100);

    // Draw the fixed heading marker (pointer)
    painter.setPen(Qt::red);
    painter.setBrush(Qt::red);
    painter.drawPolygon(QPolygon() << QPoint(0, -45) << QPoint(-7, -50) << QPoint(7, -50));

    // Draw the current heading in the center
    painter.setPen(m_valueTextColor);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    //painter.drawText(-20, -10, 40, 20, Qt::AlignCenter, QString::number(qRound(m_heading)) + m_valueSuffix);
    painter.drawText(-25, -10, 45, 20, Qt::AlignCenter, QString::number(m_heading, 'f', m_percision) + m_valueSuffix);

    // Draw the waypoint direction text below the compass value text
    if (m_waypointNeedleEnabled) {
        painter.setPen(Qt::blue);
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        //painter.drawText(-20, 10, 40, 20, Qt::AlignCenter, QString::number(qRound(m_waypointHeading)) + m_valueSuffix);
        painter.drawText(-25, 10, 45, 20, Qt::AlignCenter, QString::number(m_waypointHeading, 'f', m_percision) + m_valueSuffix);
    }
}
