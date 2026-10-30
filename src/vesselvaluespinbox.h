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

#ifndef VESSELVALUESPINBOX_H
#define VESSELVALUESPINBOX_H
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QLineEdit>

// Display rounding is independent of the canonical value held by Setup.
// Reveal fractional precision while editing; changing units never commits it.
class VesselValueSpinBox : public QDoubleSpinBox {
public:
    explicit VesselValueSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent) {
        setDecimals(12);
        setKeyboardTracking(false);
    }
protected:
    QString textFromValue(double value) const override {
        if (!editing) return locale().toString(value, 'f', 2);
        QString text = locale().toString(value, 'f', decimals());
        while (text.endsWith('0')) text.chop(1);
        if (text.endsWith(locale().decimalPoint())) text.chop(locale().decimalPoint().size());
        return text;
    }
    void focusInEvent(QFocusEvent* event) override {
        editing = true;
        QDoubleSpinBox::focusInEvent(event);
        lineEdit()->setText(textFromValue(value()));
        lineEdit()->setModified(false);
    }
    void focusOutEvent(QFocusEvent* event) override {
        QDoubleSpinBox::focusOutEvent(event);
        editing = false;
        lineEdit()->setText(textFromValue(value()));
    }
private:
    bool editing = false;
};
#endif
