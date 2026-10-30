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

#ifndef N2KCONST_H
#define N2KCONST_H

#include <QString>

namespace N2kConst {

// Angular Units
const QString DEG = "deg";
const QString RAD = "rad";

// Distance Units
const QString M = "m";
const QString DM = "dm";
const QString KM = "km";
const QString NM = "nm";
const QString FT = "ft";
const QString FATH = "fath";

// Speed Units
const QString KNOT = "knot";
const QString MPS = "m/s";
const QString KPH = "km/h";
const QString MPH = "mph";

// Pressure Units
const QString PA = "Pa";
const QString KPA = "kPa";
const QString BAR = "bar";
const QString PSI = "psi";
const QString MPA = "MPa";
const QString BARA = "barA";
const QString HPA = "hPa";

// Temperature Units
const QString K = "K";
const QString C = "°C";
const QString F = "°F";

// Volume and Flow Units
const QString LPH = "L/h";
const QString GPH = "gal/h";
const QString L = "L";
const QString GAL = "gal";
const QString M3 = "m³";
const QString LPM = "L/min";
const QString GPM = "gal/min";
const QString M3H = "m³/h";
const QString GPHR = "gal/h";

// Weight and Force Units
const QString KGF = "kgf";
const QString LBF = "lbf";
const QString TON = "ton";
const QString N = "N";

// Power and Energy Units
const QString W = "W";
const QString KW = "kW";
const QString WH = "Wh";
const QString KWH = "kWh";
const QString J = "J";
const QString KJ = "kJ";
const QString CAL = "cal";

// Electrical Units
const QString V = "V";
const QString A = "A";
const QString AH = "Ah";

// Percentage and Ratio Units
const QString PCT = "%";
const QString DB = "dB";

// Frequency Units
const QString HZ = "Hz";
const QString RPM = "RPM";

// Surface Area Units
const QString M2 = "m²";
const QString FT2 = "ft²";

// Derived Units for Flow and Volume (Extended)
const QString M3MIN = "m³/min";
const QString LPS = "L/s";
const QString GPS = "gal/s";
const QString M3S = "m³/s";
const QString M3M = "m³/m";
const QString FT3FT = "ft³/ft";
const QString M3FT = "m³/ft";
const QString FT3M = "ft³/m";

// Additional Flow Units
const QString M2M = "m²/m";
const QString FT2FT = "ft²/ft";
const QString M3L = "m³/L";
const QString GALFT3 = "gal/ft³";

// Time Units
const QString S = "s";
const QString MS = "ms";
const QString MIN = "min";
const QString HR = "hr";
const QString DAY = "day";
const QString MONT = "month";
const QString YR = "yr";

// Rotation Units
const QString DPS = "deg/s"; // Degrees per Second
const QString RPS = "rad/s"; // Radians per Second

// Miscellaneous Units
const QString T = "T";       // Tesla (Magnetic Field Strength)
const QString ENUM = "enum"; // Enumeration

} // namespace N2kConst

#endif // N2KCONST_H

