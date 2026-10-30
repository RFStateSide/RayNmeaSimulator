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

#include "environmentalinstruments.h"
#include "n2khelper.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QApplication>
#include <cmath>

QList<QList<QString>> EnvironmentalInstrument::parameters(int pgn) {
    const QString na = QString::number(N2kDoubleNA, 'g', 17);
    if (pgn == 130306) return {{"SID", "0", "", "unsigned char"},
        {"WindSpeed", na, "m/s", "double"}, {"WindAngle", na, "rad", "double"},
        {"WindReference", "0", "", "unsigned char"}};
    return {{"SID", "0", "", "unsigned char"}, {"HumidityInstance", "0", "", "unsigned char"},
        {"HumiditySource", "1", "", "unsigned char"}, {"ActualHumidity", na, "%", "double"},
        {"SetHumidity", na, "%", "double"}};
}
EnvironmentalInstrument::EnvironmentalInstrument(int id, const QString &label, int pgn, QWidget *parent)
    : PgnWidgetBase(id, label, parent) {
    setPgn(pgn); setColumnSpan(3); setRowSpan(5); setMinimumWidth(300);
    auto *box = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *enabled = new QCheckBox(this); enabled->setObjectName("checkBoxEnabled");
    enabled->setChecked(isEnabled()); enabled->setToolTip("Enable " + label);
    connect(enabled, &QCheckBox::clicked, this, &PgnWidgetBase::setEnabled);
    auto *title = new QLabel(label + QString(" (%1)").arg(pgn), this);
    title->setObjectName("labelTitle"); auto font = title->font(); font.setBold(true); title->setFont(font);
    auto *remove = new QPushButton(this); remove->setObjectName("pushButtonRemove");
    remove->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton));
    remove->setToolTip("Remove " + label); remove->setFixedSize(24, 24);
    header->addWidget(enabled); header->addWidget(title, 1); header->addWidget(remove); box->addLayout(header);
    form = new QFormLayout; box->addLayout(form);
    for (const auto &row : parameters(pgn)) {
        const auto field = row[0]; values[field] = row[1].toDouble();
        if (field == "SID") continue; // Available in the Property Editor.
        if (field == "WindReference" || field == "HumiditySource") {
            auto *combo = new QComboBox(this); combo->setObjectName(field);
            if (field == "WindReference") {
                const QStringList names{"True north (from)", "Magnetic north (from)", "Apparent (from)",
                    "True boat / ground (from)", "True boat / water (from)"};
                for (int i=0;i<names.size();++i) combo->addItem(names[i], i);
                combo->addItem("Error", 6); combo->addItem("Unavailable", 7);
            } else { combo->addItem("Inside", 0); combo->addItem("Outside", 1); combo->addItem("Unavailable", 255); }
            combo->setCurrentIndex(combo->findData(int(values[field])));
            form->addRow(field == "WindReference" ? "Reference" : "Source", combo);
            connect(combo, &QComboBox::activated, this, [this, combo, field] { setValue(field, combo->currentData().toInt()); });
        } else if (field == "HumidityInstance") {
            auto *spin = new QSpinBox(this); spin->setObjectName(field); spin->setRange(0, 255);
            form->addRow("Instance", spin);
            connect(spin, &QSpinBox::valueChanged, this, [this, field](int v) { setValue(field, v); });
        } else {
            auto *edit = new QLineEdit(this); edit->setObjectName(field); editors[field] = edit;
            form->addRow(field == "WindSpeed" ? "Wind speed" : field == "WindAngle" ? "Direction / angle (deg, from)" : field == "ActualHumidity" ? "Humidity (%)" : "Set humidity (%)", edit);
            connect(edit, &QLineEdit::editingFinished, this, [this, edit, field] {
                if (!edit->isModified()) return;
                bool ok = false; double v = edit->text().toDouble(&ok);
                if (edit->text().compare("Unavailable", Qt::CaseInsensitive) == 0) { v = N2kDoubleNA; ok = true; }
                if (ok && v != N2kDoubleNA) {
                    if (field == "WindSpeed") v = N2kHelper::convertUserSpeedToN2kUnits(v, UserPrefs());
                    if (field == "WindAngle") v *= M_PI / 180.;
                }
                if (ok) setValue(field, v);
                edit->setModified(false); refresh();
            });
        }
    }
    connect(this, &PgnWidgetBase::UserPrefsChanged, this, &EnvironmentalInstrument::refresh);
    refresh();
}
double EnvironmentalInstrument::value(const QString &field) const { return values.value(field, N2kDoubleNA); }
void EnvironmentalInstrument::setValue(const QString &field, double v) {
    if (!values.contains(field) || !std::isfinite(v)) return;
    if (field == "WindReference" && !(v == 0 || v == 1 || v == 2 || v == 3 || v == 4 || v == 6 || v == 7)) return;
    if (field == "HumiditySource" && !(v == 0 || v == 1 || v == 255)) return;
    if ((field == "SID" || field == "HumidityInstance") && (v < 0 || v > 255 || std::floor(v) != v)) return;
    if (field == "WindAngle" && v != N2kDoubleNA && (v < 0 || v >= 2*M_PI)) return;
    if (field == "WindSpeed" && v != N2kDoubleNA && (v < 0 || v > 655.32)) return;
    if (field.contains("Humidity") && field != "HumiditySource" && field != "HumidityInstance" && v != N2kDoubleNA && (v < 0 || v > 100)) return;
    if (values[field] == v) return;
    values[field] = v; refresh(); emit parameterValueChanged(field, v);
}
void EnvironmentalInstrument::refresh() {
    for (const auto &field : {"WindReference", "HumiditySource"})
        if (auto *combo = findChild<QComboBox*>(field)) {
            const QSignalBlocker block(combo); combo->setCurrentIndex(combo->findData(int(value(field))));
        }
    if (auto *spin = findChild<QSpinBox*>("HumidityInstance")) {
        const QSignalBlocker block(spin); spin->setValue(int(value("HumidityInstance")));
    }
    for (auto i = editors.begin(); i != editors.end(); ++i) {
        if (i.value()->hasFocus() && i.value()->isModified()) continue;
        double v = values[i.key()]; QString unit;
        if (v == N2kDoubleNA) { i.value()->setText("Unavailable"); continue; }
        if (i.key() == "WindSpeed") {
            v = N2kHelper::convertN2kSpeedToUserUnits(v, UserPrefs());
            unit = " " + QStringList{"m/s", "km/h", "MPH", "knots"}.value(UserPrefs().speedUnitIndex);
        }
        if (i.key() == "WindAngle") v *= 180. / M_PI;
        i.value()->setText(QString::number(v, 'f', 2)); i.value()->setToolTip(unit.trimmed());
    }
    if (auto *speed = editors.value("WindSpeed")) {
        const auto unit = QStringList{"m/s", "km/h", "MPH", "knots"}.value(UserPrefs().speedUnitIndex);
        speed->setAccessibleName("Wind speed (" + unit + ")");
        if (auto *label = qobject_cast<QLabel*>(form->labelForField(speed))) label->setText("Wind speed (" + unit + ")");
    }
}
void EnvironmentalInstrument::setInitialGuiValues() { refresh(); }
void EnvironmentalInstrument::initializeUnitsLimits() { refresh(); }
