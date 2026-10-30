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

#ifndef COORDINATEEDITOR_H
#define COORDINATEEDITOR_H
#include <QDoubleValidator>
#include <QLineEdit>
#include <QLocale>
#include <QRegularExpression>
#include <limits>

// Shared decimal/DMS/DMM validation. Incomplete input stays editable but never enters the model.
class CoordinateValidator final : public QValidator {
public:
    CoordinateValidator(int format, bool latitude, QObject* parent, int decimals = 6)
        : QValidator(parent)
        , format(format)
        , latitude(latitude)
        , decimals(decimals)
    {
    }
    State validate(QString& text, int& position) const override
    {
        if (format == 0) {
            QDoubleValidator number(latitude ? -90 : -180, latitude ? 90 : 180, decimals);
            number.setLocale(QLocale::c());
            number.setNotation(QDoubleValidator::StandardNotation);
            return number.validate(text, position);
        }
        const QString direction = latitude ? "[NS]" : "[EW]";
        const QString pattern = format == 1
            ? QString::fromUtf8(R"coord(^([0-9]+)°\s+([0-9]+)'\s+([0-9]+(?:\.[0-9]+)?)"\s+(%1)$)coord").arg(direction)
            : QString::fromUtf8(R"coord(^([0-9]+)°\s+([0-9]+(?:\.[0-9]+)?)'\s+(%1)$)coord").arg(direction);
        const auto match = QRegularExpression(pattern).match(text);
        if (!match.hasMatch())
            return text.size() > 128 ? Invalid : Intermediate;
        const double degrees = match.captured(1).toDouble(), minutes = match.captured(2).toDouble();
        const double seconds = format == 1 ? match.captured(3).toDouble() : 0;
        if (minutes >= 60 || seconds >= 60 || degrees + minutes / 60 + seconds / 3600 > (latitude ? 90 : 180))
            return Intermediate;
        return Acceptable;
    }

private:
    int format;
    bool latitude;
    int decimals;
};

inline void setCoordinateValidator(QLineEdit* editor, int format, bool latitude)
{
    const auto* old = editor->validator();
    editor->setValidator(new CoordinateValidator(format, latitude, editor, std::numeric_limits<double>::max_digits10));
    if (old && old->parent() == editor)
        delete old;
}
#endif
