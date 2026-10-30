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

#include "gamecontroller.h"
#include <QDebug>

GameController::GameController(QObject *parent)
    : QObject(parent), controller(nullptr)
{
    eventTimer = new QTimer(this);
    connect(eventTimer, &QTimer::timeout, this, &GameController::pollEvents);
}

GameController::~GameController() { disable(); }

bool GameController::initialize()
{
    if (m_isInitialized) return true;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        qWarning() << "Unable to initialize SDL:" << SDL_GetError();
        return false;
    }
    m_isInitialized = true;
    return true;
}

bool GameController::open(int controllerIndex)
{
    close();
    if (!m_isInitialized || controllerIndex < 0) return false;
    // The combo box contains only mapped controllers, not every SDL joystick.
    int ordinal = 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        if (ordinal++ != controllerIndex) continue;
        controller = SDL_GameControllerOpen(i);
        break;
    }
    if (!controller) {
        qWarning() << "Failed to open game controller:" << SDL_GetError();
        return false;
    }
    qDebug() << "Game controller connected:" << SDL_GameControllerName(controller);
    m_acceptInput = false;
    eventTimer->start(16); // Approximately 60 Hz; simulation keeps its own cadence.
    return true;
}

void GameController::close()
{
    eventTimer->stop();
    m_acceptInput = false;
    if (controller) {
        qDebug() << "Closed game controller:" << SDL_GameControllerName(controller);
        SDL_GameControllerClose(controller);
        controller = nullptr;
    }
}

void GameController::disable()
{
    close();
    if (m_isInitialized) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    m_isInitialized = false;
}

QStringList GameController::availableControllers()
{
    QStringList names;
    if (!m_isInitialized) return names;
    SDL_PumpEvents();
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        const char *name = SDL_GameControllerNameForIndex(i);
        names << (name ? QString::fromUtf8(name) : QString("Unknown Controller %1").arg(i));
    }
    return names;
}

int GameController::getAxisCount() const
{
    return controller ? SDL_JoystickNumAxes(SDL_GameControllerGetJoystick(controller)) : 0;
}

bool GameController::isInitialized() const { return m_isInitialized; }

void GameController::pollEvents()
{
    if (!controller) return;
    SDL_PumpEvents();
    if (!SDL_GameControllerGetAttached(controller)) {
        close();
        emit disconnected();
        return;
    }
    const auto id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
    bool neutral = true;
    for (int axis = 0; axis < 4; ++axis)
        neutral &= qAbs(int(SDL_GameControllerGetAxis(controller, SDL_GameControllerAxis(axis)))) <= 4096;
    for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button)
        neutral &= !SDL_GameControllerGetButton(controller, SDL_GameControllerButton(button));
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_CONTROLLERBUTTONDOWN:
            if (m_acceptInput && event.cbutton.which == id) emit buttonPressed(event.cbutton.button);
            break;
        case SDL_CONTROLLERBUTTONUP:
            if (m_acceptInput && event.cbutton.which == id) emit buttonReleased(event.cbutton.button);
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (m_acceptInput && event.caxis.which == id) emit axisMoved(event.caxis.axis, event.caxis.value);
            break;
        case SDL_JOYHATMOTION:
            if (m_acceptInput && event.jhat.which == id) emit hatPositionChanged(event.jhat.hat, event.jhat.value);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (event.cdevice.which == id) {
                close();
                emit disconnected();
                return;
            }
            break;
        default: break;
        }
    }
    if (neutral) m_acceptInput = true;
}
