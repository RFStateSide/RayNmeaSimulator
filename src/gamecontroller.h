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

#ifndef GAMECONTROLLER_H
#define GAMECONTROLLER_H

#include <QObject>
#include <QTimer>

// Define SDL_MAIN_HANDLED to avoid main redefinition issues
#define SDL_MAIN_HANDLED
#include <SDL.h>

class GameController : public QObject
{
    Q_OBJECT

public:
    explicit GameController(QObject *parent = nullptr);
    ~GameController();

    bool initialize();
    bool open(int joystickIndex);
    void close();
    void disable();
    QStringList availableControllers();
    int getAxisCount() const;
    bool isInitialized() const;

signals:
    void disconnected();
    void buttonPressed(int button);
    void buttonReleased(int button);
    void axisMoved(int axis, int value);
    void hatPositionChanged(int hat, uint8_t value);

private slots:
    void pollEvents();

private:
    QTimer *eventTimer;
    SDL_GameController *controller;
    bool m_isInitialized = false;
    bool m_acceptInput = false;
};

#endif // GAMECONTROLLER_H
