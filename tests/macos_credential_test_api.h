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

// Test-only SecItem substitute. No production target includes this implementation.
#pragma once
#include <Security/Security.h>
#include <QString>
namespace MacCredentialTest {
enum class Operation { Read, Add, Update, Remove };
OSStatus copyMatching(CFDictionaryRef query, CFTypeRef* result);
OSStatus add(CFDictionaryRef attributes, CFTypeRef* result);
OSStatus update(CFDictionaryRef query, CFDictionaryRef changes);
OSStatus erase(CFDictionaryRef query);
void reset();
void failNext(Operation operation, OSStatus status, const QString& accountSuffix = {});
void malformedRead(bool enabled);
void invalidEncodingRead(bool enabled);
int calls();
bool validQueries();
}
