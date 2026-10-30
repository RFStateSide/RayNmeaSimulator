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

#include "appsettings.h"
#define main applicationMain
#include "../src/main.cpp"
#undef main
#include "standalone_acceptance.h"
int main(int argc, char **argv)
{
    TestProfile::root();
    if (qEnvironmentVariable("RAYNMEA_ACCEPTANCE_SMOKE") == "1")
        qputenv("QT_FORCE_STDERR_LOGGING", "1");
    return applicationMain(argc, argv);
}
