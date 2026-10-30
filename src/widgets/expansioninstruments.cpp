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

#include "navigationstatistics.h"
#include "expansioninstruments.h"
#include "N2kMessages.h"
#include "clickablelabel.h"
#include "convert.h"
#include "coordinateeditor.h"
#include "n2khelper.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeyEvent>
#include <QSpinBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>

namespace
{
// Cell editors handle Return themselves; an unhandled Return must never invoke OK.
class SatelliteSelection : public QObject
{
  public:
    explicit SatelliteSelection(QTableWidget *table) : QObject(table), table(table)
    {
    }
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress)
            for (int row = 0; row < table->rowCount(); ++row)
                for (int col = 0; col < table->columnCount(); ++col)
                    if (table->cellWidget(row, col) == object)
                    {
                        // Select the record without changing the current cell. selectRow()
                        // moves current to column zero and fights Qt's cell-focus filter.
                        table->selectionModel()->select(table->model()->index(row, col),
                                                        QItemSelectionModel::ClearAndSelect |
                                                            QItemSelectionModel::Rows);
                        return false;
                    }
        return false;
    }

  private:
    QTableWidget *table;
};
class SatelliteDialog : public QDialog
{
  public:
    using QDialog::QDialog;

  protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            event->accept();
        else
            QDialog::keyPressEvent(event);
    }
};
} // namespace

QList<QPair<QVariant, QString>> Expansion::choices(const QString &type)
{
    QList<QPair<QVariant, QString>> out;
    auto names = [&](const QStringList &list) {
        for (int i = 0; i < list.size(); ++i)
            out.append({i, list[i]});
    };
    if (type == "tN2kGNSSDOPmode")
        names({"1D", "2D", "3D", "Auto", "Reserved (4)", "Reserved (5)", "Error", "Unavailable"});
    if (type == "tN2kRangeResidualMode")
        names({"Used to calculate position", "Calculated after position", "Error", "Unavailable"});
    if (type == "tN2kTempSource (6 bit)")
    {
        names({"Sea", "Outside", "Inside", "Engine room", "Main cabin", "Live well", "Bait well", "Refrigeration",
               "Heating system", "Dew point", "Apparent wind chill", "Theoretical wind chill", "Heat index", "Freezer",
               "Exhaust gas", "Shaft seal"});
        out.append({62, "Reserved (62)"});
        out.append({63, "Unavailable"});
    }
    if (type == "tN2kHumiditySource (2 bit)")
        names({"Inside", "Outside", "Reserved (2)", "Unavailable"});
    if (type == "tN2kPressureSource")
    {
        names({"Atmospheric", "Water", "Steam", "Compressed air", "Hydraulic", "Filter", "Altimeter setting", "Oil",
               "Fuel"});
        out.append({253, "Reserved (253)"});
        out.append({254, "Error"});
        out.append({255, "Unavailable"});
    }
    if (type == "tNMEA0183WindReference")
        out = {{"T", "True (relative to bow)"}, {"R", "Apparent (relative to bow)"}};
    if (type == "Arrival indication")
        out = {{"A", "Arrived"}, {"V", "Not arrived"}};
    if (type == "Perpendicular indication")
        out = {{"A", "Passed"}, {"V", "Not passed"}};
    if (type == "tN2kNavigationDirection")
        names({"Forward", "Reverse", "Reserved (2)", "Reserved (3)", "Reserved (4)", "Reserved (5)", "Error",
               "Unavailable"});
    if (type == "tN2kGenericStatusPair")
        names({"No", "Yes", "Error", "Unavailable"});
    if (type == "Bearing reference")
        out = {{"T", "True"}, {"M", "Magnetic"}};
    if (type == "Steer direction")
        out = {{"L", "Left"}, {"R", "Right"}};
    if (type == "RTE route type")
        out = {{"c", "Complete route"}, {"w", "Working route"}};
    if (type == "GSA selection")
        out = {{"M", "Manual"}, {"A", "Automatic"}};
    if (type == "GSA fix")
        out = {{1, "Not available"}, {2, "2D"}, {3, "3D"}};
    if (type == "OffOn (2 bit)")
        names({"Off", "On", "Error", "Unavailable"});
    if (type == "InverterState (4 bit)")
        names({"Invert", "AC passthru", "Load sense", "Fault", "Disabled", "Reserved (5)", "Reserved (6)",
               "Reserved (7)", "Reserved (8)", "Reserved (9)", "Reserved (10)", "Reserved (11)", "Reserved (12)",
               "Reserved (13)", "Error", "Unavailable"});
    if (type == "char (A/V)")
        out = {{"A", "Valid"}, {"V", "Invalid"}};
    return out;
}
QString Expansion::fieldLabel(const QString &field)
{
    static const QMap<QString, QString> labels{{"DeltaLatitude", "Latitude offset"},{"DeltaLongitude", "Longitude offset"},{"DeltaAltitude", "Altitude offset"},{"LocalDatum", "Local datum"},{"ReferenceDatum", "Reference datum"},{"RMSPositionUncertainty", "RMS position uncertainty"},{"RMSRangeInputDeviation", "RMS range input deviation"},{"MajorAxisDeviation", "Major axis STD"},{"MinorAxisDeviation", "Minor axis STD"},{"MajorAxisOrientation", "Major axis orientation"},{"MajorErrorEllipseDeviation", "Major error ellipse STD"},{"MinorErrorEllipseDeviation", "Minor error ellipse STD"},{"ErrorEllipseOrientation", "Ellipse orientation"},{"LatitudeDeviation", "Latitude STD"},{"LongitudeDeviation", "Longitude STD"},{"AltitudeDeviation", "Altitude STD"},{"HeaderReserved", "Header reserved bits"},{"ReservedByte", "Reserved byte"},{"ManufacturerCode", "Payload manufacturer"},{"IndustryCode", "Payload industry"},{"XTE", "Cross track error"},
                                               {"VIN", "VIN"},
                                               {"ACInstance", "AC instance"},
                                               {"DCInstance", "DC instance"},
                                               {"PDOP", "Position DOP"},
                                               {"SID", "Sequence ID"},
                                               {"HDOP", "Horizontal DOP"},
                                               {"VDOP", "Vertical DOP"},
                                               {"TDOP", "Time DOP"},
                                               {"DesiredMode", "Desired mode"},
                                               {"ActualMode", "Actual mode"},
                                               {"Mode", "Range residual mode"},
                                               {"Log", "Total distance"},
                                               {"TripLog", "Trip distance"},
                                               {"DaysSince1970", "Date"},
                                               {"SecondsSinceMidnight", "UTC time"},
                                               {"FuelRateEconomy", "Economy fuel rate"},
                                               {"InstantaneousFuelEconomy", "Instant. economy rate"},
                                               {"TotalMessages", "Total pages"},
                                               {"MessageNumber", "Page number"},
                                               {"SatelliteCount", "Satellites in view"},
                                               {"DepthBelowTransducer", "Depth below transducer"}};
    if (labels.contains(field))
        return labels[field];
    QString text = field;
    for (int i = text.size() - 1; i > 0; --i)
        if (text[i].isUpper() && text[i - 1].isLower())
            text.insert(i, ' ');
    return text.left(1) + text.mid(1).toLower();
}
namespace
{
bool selected(const QVariant &option, const QString &text)
{
    if (option.metaType().id() == QMetaType::QString)
        return option.toString() == text;
    bool ok = false;
    const double value = text.toDouble(&ok);
    return ok && std::isfinite(value) && value == option.toDouble();
}
bool farDistance(const QString &field)
{
    return field == "Log" || field == "TripLog";
}
QString unit(const Expansion::Field &f, const UserUnitPrefs &p)
{
    if (f.name == "Latitude" || f.name == "Longitude")
        return QStringList{"DD", "DMS", "DMM"}.value(p.latLongUnitIndex);
    if (f.unit == "K")
        return QStringList{"K", "°C", "°F"}.value(p.tempUnitIndex);
    if (f.unit == "Pa")
        return f.name == "AtmosphericPressure"
                   ? QStringList{"hPa", "inHg", "mmHg", "Pa", "atm"}.value(p.atmosphericPressureUnitIndex)
                   : QStringList{"Pa", "kPa", "bar", "PSI"}.value(p.pressureUnitIndex);
    if (f.unit == "m/s")
        return QStringList{"m/s", "km/h", "MPH", "knots"}.value(p.speedUnitIndex);
    if (f.unit == "L/h")
        return p.flowRateUnitIndex ? "GPH" : "LPH";
    if (f.unit == "L")
        return p.volumeUnitIndex ? "gal" : "L";
    if (f.unit == "rad")
        return "deg";
    if (f.name == "DistanceToWaypoint" || f.name == "DistanceToEmpty")
        return QStringList{"km", "NM", "mi"}.value(p.distanceUnitIndex);
    if (f.name == "XTE")
        return p.distanceUnitIndex == 0 ? "m" : "ft";
    if (f.unit == "m")
        return farDistance(f.name) ? QStringList{"km", "NM", "mi"}.value(p.distanceUnitIndex)
                                   : QStringList{"m", "ft", "fathoms"}.value(p.depthUnitIndex);
    return f.unit;
}
double convert(const Expansion::Field &f, double v, const UserUnitPrefs &p, bool toUser)
{
    if (v == N2kDoubleNA)
        return v;
    if (f.unit == "K")
        return toUser ? N2kHelper::convertN2kTemperatureToUserUnits(v, p)
                      : N2kHelper::convertUserTemperatureToN2kUnits(v, p);
    if (f.unit == "Pa")
    {
        if (f.name == "AtmosphericPressure")
            return toUser ? N2kHelper::convertN2kAtmosphericPressureToUserUnits(v, p)
                          : N2kHelper::convertUserAtmosphericPressureToN2kUnits(v, p);
        return toUser ? N2kHelper::convertN2kPressureToUserUnits(v, p) : N2kHelper::convertUserPressureToN2kUnits(v, p);
    }
    if (f.unit == "m/s")
        return toUser ? N2kHelper::convertN2kSpeedToUserUnits(v, p) : N2kHelper::convertUserSpeedToN2kUnits(v, p);
    if (f.unit == "L/h")
        return toUser ? N2kHelper::convertN2kFlowRateToUserUnits(v, p) : N2kHelper::convertUserFlowRateToN2kUnits(v, p);
    if (f.unit == "L")
        return toUser ? N2kHelper::convertN2kVolumeToUserUnits(v, p) : N2kHelper::convertUserVolumeToN2kUnits(v, p);
    if (f.name == "DistanceToWaypoint" || f.name == "DistanceToEmpty")
        return toUser ? N2kHelper::convertN2kDistanceFarToUserUnits(v, p)
                      : N2kHelper::convertUserDistanceFarToN2kUnits(v, p);
    if (f.name == "XTE")
        return toUser ? N2kHelper::convertN2kDistanceNearToUserUnits(v, p)
                      : N2kHelper::convertUserDistanceNearToN2kUnits(v, p);
    if (f.unit == "m")
    {
        if (farDistance(f.name))
            return toUser ? N2kHelper::convertN2kDistanceFarToUserUnits(v, p)
                          : N2kHelper::convertUserDistanceFarToN2kUnits(v, p);
        return toUser ? N2kHelper::convertN2kDepthToUserUnits(v, p) : N2kHelper::convertUserDepthToN2kUnits(v, p);
    }
    if (f.unit == "rad")
        return toUser ? Convert::RadiansToDegrees(v) : Convert::DegreesToRadians(v);
    return v;
}
bool unavailable(const Expansion::Field &f, const QString &text)
{
    if (f.type == "uint16_t" && text.toDouble() == 65535)
        return true;
    return (f.type == "unsigned char" && text == "255") || (f.type == "double" && text.toDouble() == N2kDoubleNA) ||
           (farDistance(f.name) && text == "4294967295") || (f.name == "DaysSince1970" && text == "65535");
}
QString editorText(const Expansion::Field &f, const QString &text, const UserUnitPrefs &p)
{
    if (unavailable(f, text))
        return "Unavailable";
    if (f.name == "Latitude")
        return N2kHelper::convertN2kLatitudeToUserUnits(text.toDouble(), p);
    if (f.name == "Longitude")
        return N2kHelper::convertN2kLongitudeToUserUnits(text.toDouble(), p);
    if (f.name == "DaysSince1970")
        return N2kHelper::convertN2kDateToUserFormat(text.toUInt(), p);
    if (f.name == "SecondsSinceMidnight")
        return N2kHelper::convertN2kTimeToUserFormat(text.toDouble(), p);
    if (f.type == "QString")
        return (f.name == "LocalDatum" || f.name == "ReferenceDatum") && text.isEmpty() ? QString("Unavailable") : text;
    const double v = convert(f, text.toDouble(), p, true);
    if (f.name == "DeltaLatitude" || f.name == "DeltaLongitude") return QString::number(v, 'f', 7);
    return f.type == "double" || farDistance(f.name) ? QString::number(v, 'f', 2) : text;
}
} // namespace
QString Expansion::displayValue(const QString &code, const QString &field, const QString &text, const UserUnitPrefs &p)
{
    const auto *d = definition(code);
    if (!d)
        return text;
    for (const auto &f : d->fields)
        if (f.name == field)
        {
            if (code == "65359" && field == "ReservedByte") return text;
            if (NavigationStatistics::supported(code)) {
                const auto state = NavigationStatistics::status(f, text);
                if (!state.isEmpty()) return state;
            }
            const auto options = choices(f.type);
            for (const auto &entry : options)
                if (selected(entry.first, text))
                    return entry.second;
            if (field == "Satellites")
                return QString::number(QJsonDocument::fromJson(text.toUtf8()).array().size()) +
                       (QJsonDocument::fromJson(text.toUtf8()).array().size() == 1 ? " record" : " records");
            const QString shown = editorText(f, text, p), suffix = unit(f, p);
            return shown == "Unavailable" || field == "DaysSince1970" || field == "SecondsSinceMidnight" ||
                           suffix.isEmpty()
                       ? shown
                       : shown + " " + suffix;
        }
    return text;
}
ExpansionInstrument::ExpansionInstrument(int id, const QString &label, const QString &code, QWidget *parent)
    : PgnWidgetBase(id, label, parent), messageCode(code)
{
    const auto *d = Expansion::definition(code);
    Q_ASSERT(d);
    setPgn(d->id);
    setColumnSpan(3);
    setRowSpan(qMin(14, d->fields.size() + 2));
    setMinimumWidth(300);
    setMinimumHeight(RowSpan() * 30);
    setEnabled(false);
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *enabled = new QCheckBox(this);
    enabled->setObjectName("checkBoxEnabled");
    enabled->setToolTip(tr("Enable ") + label);
    auto *title = new QLabel(label + " (" + code + ")", this);
    title->setObjectName("labelTitle");
    title->setWordWrap(true);
    auto font = title->font();
    font.setBold(true);
    title->setFont(font);
    auto *remove = new QPushButton(this);
    remove->setObjectName("pushButtonRemove");
    remove->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton));
    remove->setFixedSize(24, 24);
    remove->setToolTip(tr("Remove ") + label);
    header->addWidget(enabled);
    header->addWidget(title, 1);
    header->addWidget(remove);
    layout->addLayout(header);
    connect(enabled, &QCheckBox::clicked, this, &PgnWidgetBase::setEnabled);
    connect(this, &PgnWidgetBase::enabledChanged, this, [this, enabled] {
        const QSignalBlocker b(enabled);
        enabled->setChecked(isEnabled());
    });
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    for (const auto &field : d->fields)
    {
        values[field.name] = field.initial;
        const auto options = Expansion::choices(field.type);
        const QString validityTip = NavigationStatistics::supported(code) ? tr(" Enter Unavailable, Error or Reserved for measurement validity; Property Editor uses canonical numeric sentinels -1000000000, -1000000001, -1000000002. Datum codes: 3 or 4 ASCII characters; empty is unavailable. Reserved header/body bytes are retained without semantic interpretation.") : QString();
        const QString tip = Expansion::fieldLabel(field.name) + ". " + field.type +
                            (field.unit.isEmpty() ? QString() : "; canonical " + field.unit) +
                            ". Property Editor retains canonical values." + validityTip;
        // Match the established Designer rows: label | flexible value | units.
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        auto *caption = new QLabel(Expansion::fieldLabel(field.name), this);
        caption->setObjectName("labelField" + field.name);
        caption->setMinimumSize(150, 22);
        caption->setMaximumWidth(150);
        caption->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(caption);
        layout->addLayout(row);
        if (code == "129044" && NavigationStatistics::datumField(field.name)) {
            auto *combo = NavigationStatistics::datumEditor(this, field.initial);
            combo->setObjectName(field.name);
            combo->lineEdit()->setObjectName("lineEdit" + field.name);
            combo->setMinimumSize(75, 22); combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            selectors[field.name] = combo; row->addWidget(combo, 1);
            auto *space = new QLabel(this); space->setMinimumWidth(50); row->addWidget(space);
            auto commit = [this, combo, name = field.name] {
                setValue(name, NavigationStatistics::datumValue(combo));
                NavigationStatistics::setDatumEditor(combo, values[name]);
            };
            connect(combo, &QComboBox::activated, this, commit);
            connect(combo->lineEdit(), &QLineEdit::editingFinished, this, commit);
            continue;
        }
        if (!options.isEmpty())
        {
            auto *combo = new QComboBox(this);
            combo->setObjectName(field.name);
            combo->setMinimumWidth(75);
            combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            combo->setMinimumHeight(22);
            combo->setToolTip(tip);
            for (const auto &entry : options)
                combo->addItem(entry.second, entry.first);
            selectors[field.name] = combo;
            row->addWidget(combo, 1);
            auto *unitSpace = new QLabel(this);
            unitSpace->setMinimumWidth(50);
            row->addWidget(unitSpace);
            connect(combo, &QComboBox::activated, this,
                    [this, combo, name = field.name] { setValue(name, combo->currentData().toString()); });
            continue;
        }
        auto *edit = new QLineEdit(this);
        edit->setObjectName("lineEdit" + field.name);
        edit->setToolTip(tip);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        edit->setMinimumSize(75, 22);
        editors[field.name] = edit;
        row->addWidget(edit, 1);
        auto *units = new ClickableLabel(this);
        units->setObjectName("labelUnits" + field.name);
        units->setToolTip(tr("Click to select display units. Canonical values are preserved."));
        units->setMinimumWidth(50);
        row->addWidget(units);
        if (field.name == "Latitude" || field.name == "Longitude")
        {
            units->setMenuItems(unitsListLatLong);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeLatLongUnits_triggered);
        }
        else if (field.unit == "K")
        {
            units->setMenuItems(unitsListTemp);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeTempUnits_triggered);
        }
        else if (field.unit == "Pa" && field.name == "AtmosphericPressure")
        {
            units->setMenuItems(unitsListAtmosphericPressure);
            connect(units, &ClickableLabel::actionTriggered, this,
                    &ExpansionInstrument::ChangeAtmosphericPressureUnits_triggered);
        }
        else if (field.unit == "Pa")
        {
            units->setMenuItems(unitsListPressure);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangePressureUnits_triggered);
        }
        else if (field.unit == "m/s")
        {
            units->setMenuItems(unitsListSpeed);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeSpeedUnits_triggered);
        }
        else if (field.unit == "L/h")
        {
            units->setMenuItems(unitsListFlowRate);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeFuelRateUnits_triggered);
        }
        else if (field.unit == "L")
        {
            units->setMenuItems(unitsListVolume);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeVolumeUnits_triggered);
        }
        else if (field.name == "XTE" || field.name == "DistanceToWaypoint" || field.name == "DistanceToEmpty")
        {
            units->setMenuItems(unitsListDistance);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeDistanceUnits_triggered);
        }
        else if (field.unit == "m" && farDistance(field.name))
        {
            units->setMenuItems(unitsListDistance);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeDistanceUnits_triggered);
        }
        else if (field.unit == "m")
        {
            units->setMenuItems(unitsListDepth);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeDepthUnits_triggered);
        }
        else if (field.name == "DaysSince1970")
        {
            units->setMenuItems(unitsListDate);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeDateUnits_triggered);
        }
        else if (field.name == "SecondsSinceMidnight")
        {
            units->setMenuItems(unitsListTime);
            connect(units, &ClickableLabel::actionTriggered, this, &ExpansionInstrument::ChangeTimeUnits_triggered);
        }
        if (field.name == "Waypoints")
        {
            edit->setReadOnly(true);
            auto *button = new QPushButton(tr("Edit waypoints…"), this);
            button->setObjectName("editWaypoints");
            row->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this] {
                SatelliteDialog dialog(this);
                dialog.setObjectName("waypointRecordsDialog");
                dialog.setWindowTitle(tr("Waypoint records"));
                auto *layout = new QVBoxLayout(&dialog);
                auto *notice = new QLabel(
                    tr("Up to 18 records; one NMEA2000 fast packet must fit 223 bytes. Split longer lists into "
                       "explicit pages using Start RPS/ID. Angles are degrees; blank coordinates mean unavailable."),
                    &dialog);
                notice->setWordWrap(true);
                layout->addWidget(notice);
                auto *table = new QTableWidget(0, 4, &dialog);
                table->setObjectName("waypointRecords");
                table->setHorizontalHeaderLabels({tr("ID"), tr("Name"), tr("Latitude (deg)"), tr("Longitude (deg)")});
                table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
                table->setSelectionBehavior(QAbstractItemView::SelectRows);
                layout->addWidget(table);
                const QStringList keys{"ID", "Name", "Latitude", "Longitude"};
                const auto original = value("Waypoints");
                auto append = [&](const QJsonObject &point) {
                    const int row = table->rowCount();
                    table->insertRow(row);
                    for (int col = 0; col < 4; ++col)
                    {
                        const auto v = point.value(keys[col]);
                        const QString text = col == 1                      ? v.toString()
                                             : v.toDouble() == N2kDoubleNA ? QString()
                                                                           : QString::number(v.toDouble(), 'g', 12);
                        auto *item = new QTableWidgetItem(text);
                        item->setData(Qt::UserRole, v.toVariant());
                        item->setData(Qt::UserRole + 1, false);
                        table->setItem(row, col, item);
                    }
                };
                for (const auto point : QJsonDocument::fromJson(original.toUtf8()).array())
                    append(point.toObject());
                bool dirty = false, totalEdited = false;
                QSpinBox *total = nullptr;
                if (messageCode == "130074")
                {
                    auto *label = new QLabel(tr("Total waypoints in database (65535 = unavailable)"), &dialog);
                    layout->addWidget(label);
                    total = new QSpinBox(&dialog);
                    total->setObjectName("totalDatabaseWaypoints");
                    total->setRange(0, 65535);
                    total->setValue(int(value("TotalWaypoints").toDouble()));
                    layout->addWidget(total);
                    connect(total, &QSpinBox::valueChanged, &dialog, [&] {
                        dirty = true;
                        totalEdited = true;
                    });
                }
                connect(table, &QTableWidget::itemChanged, &dialog, [&](QTableWidgetItem *item) {
                    dirty = true;
                    const QSignalBlocker block(table);
                    item->setData(Qt::UserRole + 1, true);
                });
                auto *bar = new QHBoxLayout;
                auto *add = new QPushButton(tr("Add Record"), &dialog);
                auto *remove = new QPushButton(tr("Remove Record"), &dialog);
                auto *count = new QLabel(&dialog);
                add->setObjectName("addWaypoint");
                remove->setObjectName("removeWaypoint");
                count->setObjectName("waypointRecordCount");
                bar->addWidget(add);
                bar->addWidget(remove);
                bar->addStretch();
                bar->addWidget(count);
                layout->addLayout(bar);
                auto update = [=] {
                    add->setEnabled(table->rowCount() < 18);
                    remove->setEnabled(!table->selectionModel()->selectedRows().isEmpty());
                    count->setText(tr("Records: %1 / 18").arg(table->rowCount()));
                };
                connect(table->selectionModel(), &QItemSelectionModel::selectionChanged, &dialog, update);
                connect(add, &QPushButton::clicked, &dialog, [&] {
                    if (table->rowCount() >= 18)
                        return;
                    {
                        const QSignalBlocker block(table);
                        QSet<int> used;
                        for (int row = 0; row < table->rowCount(); ++row)
                        {
                            bool ok = false;
                            const int id = table->item(row, 0)->text().toInt(&ok);
                            if (ok)
                                used.insert(id);
                        }
                        int id = 1;
                        while (used.contains(id))
                            ++id;
                        append(QJsonObject{{"ID", id},
                                           {"Name", tr("WP%1").arg(id)},
                                           {"Latitude", N2kDoubleNA},
                                           {"Longitude", N2kDoubleNA}});
                    }
                    dirty = true;
                    table->selectRow(table->rowCount() - 1);
                    update();
                });
                connect(remove, &QPushButton::clicked, &dialog, [&] {
                    const auto rows = table->selectionModel()->selectedRows();
                    if (rows.isEmpty())
                        return;
                    table->removeRow(rows.first().row());
                    dirty = true;
                    update();
                });
                auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                layout->addWidget(buttons);
                connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
                    if (auto *focus = QApplication::focusWidget(); focus && table->isAncestorOf(focus))
                        focus->clearFocus();
                    if (!dirty)
                    {
                        dialog.accept();
                        return;
                    }
                    bool valid = true;
                    QJsonArray records;
                    for (int row = 0; row < table->rowCount(); ++row)
                    {
                        QJsonObject point;
                        for (int col = 0; col < 4; ++col)
                        {
                            auto *item = table->item(row, col);
                            QVariant v = item->data(Qt::UserRole);
                            if (item->data(Qt::UserRole + 1).toBool())
                            {
                                if (col == 1)
                                    v = item->text();
                                else
                                {
                                    bool ok = false;
                                    const auto text = item->text().trimmed();
                                    const double n = (col >= 2 && text.isEmpty()) ? N2kDoubleNA : text.toDouble(&ok);
                                    if (!(col >= 2 && text.isEmpty()) && !ok)
                                        valid = false;
                                    v = n;
                                }
                            }
                            point[keys[col]] = QJsonValue::fromVariant(v);
                        }
                        records.append(point);
                    }
                    const auto text = QString::fromUtf8(QJsonDocument(records).toJson(QJsonDocument::Compact));
                    if (!valid || !Expansion::validField(messageCode, "Waypoints", text))
                    {
                        notice->setText(tr("Invalid waypoint values: use unique integral IDs, short names, and valid "
                                           "latitude/longitude or blank coordinates."));
                        return;
                    }
                    auto candidate = values;
                    candidate["Waypoints"] = text;
                    if (total && totalEdited)
                        candidate["TotalWaypoints"] = QString::number(total->value());
                    tN2kMsg encoded;
                    if (!Expansion::encode(messageCode, candidate, encoded))
                    {
                        notice->setText(tr("This page exceeds packet capacity or has inconsistent headers. Remove "
                                           "records or shorten names, and configure a separate page."));
                        return;
                    }
                    if (total && totalEdited)
                        setValue("TotalWaypoints", candidate["TotalWaypoints"]);
                    setValue("Waypoints", text);
                    dialog.accept();
                });
                for (auto *b : dialog.findChildren<QPushButton *>())
                {
                    b->setAutoDefault(false);
                    b->setDefault(false);
                }
                update();
                dialog.resize(750, 400);
                dialog.exec();
            });
            continue;
        }
        if (field.name == "WaypointIDs")
        {
            edit->setReadOnly(true);
            auto *button = new QPushButton(tr("Edit waypoints…"), this);
            button->setObjectName("editWaypointIDs");
            row->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this] {
                SatelliteDialog dialog(this);
                dialog.setObjectName("routeIDsDialog");
                dialog.setWindowTitle(tr("Route waypoint identifiers"));
                auto *layout = new QVBoxLayout(&dialog);
                auto *notice = new QLabel(
                    tr("Up to 12 ordered identifiers per page. Each identifier: 1–16 printable characters without "
                       "sentence delimiters. Keep transmitted pages within the NMEA0183 sentence limit."),
                    &dialog);
                notice->setWordWrap(true);
                layout->addWidget(notice);
                auto *table = new QTableWidget(0, 1, &dialog);
                table->setObjectName("routeIDsTable");
                table->setHorizontalHeaderLabels({tr("Waypoint identifier")});
                table->horizontalHeader()->setStretchLastSection(true);
                table->setSelectionBehavior(QAbstractItemView::SelectRows);
                layout->addWidget(table);
                const auto original = value("WaypointIDs");
                const auto ids = QJsonDocument::fromJson(original.toUtf8()).array();
                for (const auto id : ids)
                {
                    const int row = table->rowCount();
                    table->insertRow(row);
                    table->setItem(row, 0, new QTableWidgetItem(id.toString()));
                }
                bool dirty = false;
                auto *bar = new QHBoxLayout;
                auto *add = new QPushButton(tr("Add Record"), &dialog);
                auto *remove = new QPushButton(tr("Remove Record"), &dialog);
                bar->addWidget(add);
                bar->addWidget(remove);
                layout->addLayout(bar);
                auto update = [=] {
                    add->setEnabled(table->rowCount() < 12);
                    remove->setEnabled(!table->selectionModel()->selectedRows().isEmpty());
                };
                connect(table, &QTableWidget::itemChanged, &dialog, [&] { dirty = true; });
                connect(table->selectionModel(), &QItemSelectionModel::selectionChanged, &dialog, update);
                connect(add, &QPushButton::clicked, &dialog, [&] {
                    const int row = table->rowCount();
                    if (row >= 12)
                        return;
                    table->insertRow(row);
                    table->setItem(row, 0, new QTableWidgetItem(tr("WP%1").arg(row + 1)));
                    table->selectRow(row);
                    dirty = true;
                    update();
                });
                connect(remove, &QPushButton::clicked, &dialog, [&] {
                    const auto rows = table->selectionModel()->selectedRows();
                    if (rows.isEmpty())
                        return;
                    table->removeRow(rows.first().row());
                    dirty = true;
                    update();
                });
                auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                layout->addWidget(buttons);
                connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
                    if (!dirty)
                    {
                        dialog.accept();
                        return;
                    }
                    QJsonArray result;
                    for (int i = 0; i < table->rowCount(); ++i)
                        result.append(table->item(i, 0) ? table->item(i, 0)->text() : QString());
                    const auto text = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
                    if (!Expansion::validField("RTE", "WaypointIDs", text))
                    {
                        notice->setText(
                            tr("Invalid identifier. Keep 1–16 printable characters and remove sentence delimiters."));
                        return;
                    }
                    setValue("WaypointIDs", text);
                    dialog.accept();
                });
                for (auto *b : dialog.findChildren<QPushButton *>())
                {
                    b->setAutoDefault(false);
                    b->setDefault(false);
                }
                update();
                dialog.resize(520, 350);
                dialog.exec();
            });
            continue;
        }
        if (field.name == "Satellites")
        {
            edit->setReadOnly(true);
            auto *button = new QPushButton(tr("Edit records…"), this);
            button->setObjectName("editSatellites");
            row->addWidget(button);
            button->setToolTip(tr("Edit the bounded supported satellite records. Display angles: degrees; "
                                  "SNR: dB; residual: metres. Unavailable measurements use the existing NA sentinel."));
            connect(button, &QPushButton::clicked, this, &ExpansionInstrument::editSatelliteRecords);
            continue;
        }
        connect(edit, &QLineEdit::textEdited, this, [this, edit, name = field.name] {
            if (!edit->property("draftUnitsCaptured").toBool())
            {
                draftPrefs[name] = UserPrefs();
                edit->setProperty("draftUnitsCaptured", true);
            }
        });
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, field] {
            if (!edit->isModified())
                return;
            const auto p = edit->property("draftUnitsCaptured").toBool() ? draftPrefs.value(field.name) : UserPrefs();
            const auto entered = edit->text();
            const auto normalized = NavigationStatistics::supported(messageCode) ? NavigationStatistics::editValue(field, entered) : entered;
            if (NavigationStatistics::supported(messageCode) && normalized != entered) {
                setValue(field.name, normalized);
                edit->setModified(false); edit->setProperty("draftUnitsCaptured", false); setInitialGuiValues(); return;
            }
            const auto text = entered;
            QString canonical;
            bool ok = false;
            if (field.type != "QString" && text.compare("Unavailable", Qt::CaseInsensitive) == 0)
            {
                canonical = field.type == "unsigned char" ? "255"
                            : field.type == "uint16_t"    ? "65535"
                            : farDistance(field.name)     ? "4294967295"
                                                          : QString::number(N2kDoubleNA, 'g', 17);
                ok = true;
            }
            else if (field.name == "Latitude" || field.name == "Longitude")
            {
                const double coordinate = field.name == "Latitude" ? N2kHelper::convertUserLatitudeToN2kUnits(text, p)
                                                                   : N2kHelper::convertUserLongitudeToN2kUnits(text, p);
                ok = std::isfinite(coordinate) && coordinate != N2kDoubleNA;
                canonical = QString::number(coordinate, 'g', 17);
            }
            else if (field.type == "QString")
            {
                canonical = text;
                ok = true;
            }
            else if (field.name == "DaysSince1970")
            {
                const auto day = N2kHelper::convertUserFormattedDateToN2k(text, p);
                canonical = QString::number(day);
                ok = day != 65535;
            }
            else if (field.name == "SecondsSinceMidnight")
            {
                double v = N2kHelper::convertUserFormattedTimeToN2k(text, p);
                canonical = QString::number(v, 'g', 17);
                ok = v != N2kDoubleNA;
            }
            else
            {
                double v = text.toDouble(&ok);
                if (ok && std::isfinite(v))
                {
                    v = convert(field, v, p, false);
                    if (farDistance(field.name))
                        v = std::round(v);
                    canonical = QString::number(v, 'g', 17);
                }
                else
                    ok = false;
            }
            if (ok)
                setValue(field.name, canonical);
            edit->setModified(false);
            edit->setProperty("draftUnitsCaptured", false);
            setInitialGuiValues();
        });
    }
    layout->addStretch();
    setInitialGuiValues();
}
void ExpansionInstrument::setValue(const QString &field, const QString &text)
{
    if (!Expansion::validField(messageCode, field, text) || values.value(field) == text)
        return;
    values[field] = text;
    setInitialGuiValues();
    emit parameterValueChanged(field, text);
}
void ExpansionInstrument::editSatelliteRecords()
{
    const bool can = messageCode == "129540";
    const auto original = QJsonDocument::fromJson(values["Satellites"].toUtf8()).array();
    SatelliteDialog dialog(this);
    dialog.setObjectName("satelliteRecordsDialog");
    dialog.setWindowTitle(tr("Satellite records"));
    auto *box = new QVBoxLayout(&dialog);
    auto *notice = new QLabel(tr("Angles are shown in degrees. SNR is dB; range residuals are metres. Empty "
                                 "measurements are unavailable. Changes apply only after validation."),
                              &dialog);
    notice->setWordWrap(true);
    box->addWidget(notice);
    auto *controls = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add Record"), &dialog);
    add->setObjectName("addRecord");
    auto *remove = new QPushButton(tr("Remove Record"), &dialog);
    remove->setObjectName("removeRecord");
    auto *count = new QLabel(&dialog);
    count->setObjectName("recordCount");
    controls->addWidget(add);
    controls->addWidget(remove);
    controls->addStretch();
    controls->addWidget(count);
    box->addLayout(controls);
    auto *table = new QTableWidget(&dialog);
    table->setObjectName("satelliteRecords");
    const QStringList keys = can ? QStringList{"PRN", "Elevation", "Azimuth", "SNR", "RangeResiduals", "UsageStatus"}
                                 : QStringList{"PRN", "Elevation", "Azimuth", "SNR"};
    table->setColumnCount(keys.size());
    table->setHorizontalHeaderLabels(
        can ? QStringList{"PRN", "Elevation (deg)", "Azimuth (deg)", "SNR (dB)", "Residual (m)", "Usage"}
            : QStringList{"PRN", "Elevation (deg)", "Azimuth (deg)", "SNR (dB)"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    box->addWidget(table);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *selection = new SatelliteSelection(table);
    bool dirty = false;
    auto append = [&](const QJsonObject &object) {
        const int row = table->rowCount();
        table->insertRow(row);
        for (int col = 0; col < keys.size(); ++col)
        {
            const auto key = keys[col];
            const double value = object.contains(key)   ? object[key].toDouble()
                                 : key == "PRN"         ? row + 1
                                 : key == "UsageStatus" ? 15
                                                        : N2kDoubleNA;
            if (key == "UsageStatus")
            {
                auto *combo = new QComboBox(table);
                const QStringList names{"Not tracked",
                                        "Tracked, not used",
                                        "Used without differential corrections",
                                        "Differential corrections available",
                                        "Tracked with corrections",
                                        "Used with corrections"};
                for (int i = 0; i < 16; ++i)
                    combo->addItem(i < names.size() ? names[i]
                                   : i == 14        ? tr("Error")
                                   : i == 15        ? tr("Unavailable")
                                                    : tr("Reserved (%1)").arg(i),
                                   i);
                combo->setCurrentIndex(combo->findData(int(value)));
                combo->setProperty("canonicalValue", value);
                combo->setProperty("edited", false);
                table->setCellWidget(row, col, combo);
                combo->installEventFilter(selection);
                connect(combo, &QComboBox::activated, &dialog, [&, combo] {
                    dirty = true;
                    combo->setProperty("edited", true);
                });
            }
            else
            {
                auto *edit = new QLineEdit(table);
                edit->setAlignment(Qt::AlignRight);
                const double shown = can && (key == "Elevation" || key == "Azimuth") && value != N2kDoubleNA
                                         ? Convert::RadiansToDegrees(value)
                                         : value;
                edit->setText(shown == N2kDoubleNA ? QString() : QString::number(shown, 'g', 12));
                edit->setProperty("canonicalValue", value);
                edit->setProperty("edited", false);
                edit->setToolTip(key == "PRN"
                                     ? tr("Integral satellite identifier; 255 means unavailable for NMEA2000.")
                                     : tr("Empty means unavailable. Unedited canonical precision is retained."));
                table->setCellWidget(row, col, edit);
                edit->installEventFilter(selection);
                connect(edit, &QLineEdit::textEdited, &dialog, [&, edit] {
                    dirty = true;
                    edit->setProperty("edited", true);
                });
            }
        }
    };
    for (const auto &record : original)
        append(record.toObject());
    auto updateControls = [&] {
        const int maximum = can ? 18 : 4;
        count->setText(tr("Records: %1 / %2").arg(table->rowCount()).arg(maximum));
        add->setEnabled(table->rowCount() < maximum);
        remove->setEnabled(!table->selectionModel()->selectedRows().isEmpty());
    };
    connect(table->selectionModel(), &QItemSelectionModel::selectionChanged, &dialog, updateControls);
    connect(add, &QPushButton::clicked, &dialog, [&] {
        if (table->rowCount() >= (can ? 18 : 4))
            return;
        append(QJsonObject());
        dirty = true;
        table->selectRow(table->rowCount() - 1);
        updateControls();
    });
    connect(remove, &QPushButton::clicked, &dialog, [&] {
        const auto rows = table->selectionModel()->selectedRows();
        if (rows.isEmpty())
            return;
        table->removeRow(rows.first().row());
        dirty = true;
        table->clearSelection();
        updateControls();
    });
    updateControls();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    box->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (!dirty)
        {
            dialog.accept();
            return;
        }
        QJsonArray result;
        bool valid = true;
        for (int row = 0; row < table->rowCount(); ++row)
        {
            QJsonObject object;
            for (int col = 0; col < keys.size(); ++col)
            {
                const auto key = keys[col];
                auto *cell = table->cellWidget(row, col);
                double value = cell->property("canonicalValue").toDouble();
                if (cell->property("edited").toBool())
                {
                    if (auto *combo = qobject_cast<QComboBox *>(cell))
                        value = combo->currentData().toDouble();
                    else
                    {
                        const auto text = qobject_cast<QLineEdit *>(cell)->text();
                        bool ok = false;
                        value = text.trimmed().isEmpty() ? N2kDoubleNA : text.toDouble(&ok);
                        if (!text.trimmed().isEmpty() && !ok)
                            valid = false;
                        if (can && (key == "Elevation" || key == "Azimuth") && value != N2kDoubleNA)
                            value = Convert::DegreesToRadians(value);
                    }
                }
                if (!std::isfinite(value))
                    valid = false;
                object[key] = value;
            }
            result.append(object);
        }
        const QString text = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
        if (!valid || !Expansion::validField(messageCode, "Satellites", text))
        {
            notice->setText(tr("Invalid satellite values. Check identifiers, integral GSV values and measurement "
                               "bounds. Previous instrument values are unchanged."));
            return;
        }
        setValue("Satellites", text);
        dialog.accept();
    });
    for (auto *button : dialog.findChildren<QPushButton *>())
    {
        button->setAutoDefault(false);
        button->setDefault(false);
    }
    dialog.resize(can ? 850 : 620, 360);
    dialog.exec();
}
void ExpansionInstrument::setInitialGuiValues()
{
    const auto *d = Expansion::definition(messageCode);
    for (const auto &f : d->fields)
    {
        if (auto *combo = selectors.value(f.name))
        {
            if (combo->property("datumEditor").toBool()) {
                if (!combo->lineEdit()->isModified()) NavigationStatistics::setDatumEditor(combo, values[f.name]);
                continue;
            }
            const QSignalBlocker block(combo);
            for (int i = 0; i < combo->count(); ++i)
                if (selected(combo->itemData(i), values[f.name]))
                    combo->setCurrentIndex(i);
        }
        auto *edit = editors.value(f.name);
        if (!edit || edit->isModified())
            continue;
        const QSignalBlocker block(edit);
        if (f.name == "Latitude" || f.name == "Longitude")
            setCoordinateValidator(edit, UserPrefs().latLongUnitIndex, f.name == "Latitude");
        edit->setText(messageCode == "65359" && f.name == "ReservedByte" ? values[f.name]
                      : f.name == "Waypoints"
                          ? tr("%1 waypoints").arg(QJsonDocument::fromJson(values[f.name].toUtf8()).array().size())
                      : f.name == "WaypointIDs"
                          ? tr("%1 waypoints").arg(QJsonDocument::fromJson(values[f.name].toUtf8()).array().size())
                      : f.name == "Satellites"
                          ? Expansion::displayValue(messageCode, f.name, values[f.name], UserPrefs())
                          : NavigationStatistics::supported(messageCode) && !NavigationStatistics::status(f, values[f.name]).isEmpty()
                              ? NavigationStatistics::status(f, values[f.name]) : editorText(f, values[f.name], UserPrefs()));
        if (auto *label = findChild<QLabel *>("labelUnits" + f.name))
            label->setText(f.name == "SecondsSinceMidnight" ? "UTC"
                           : f.name == "DaysSince1970"      ? tr("Date")
                                                            : unit(f, UserPrefs()));
    }
}

ExpansionInstrument *ExpansionInstrument::create(int id, const QString &label, const QString &code, QWidget *parent)
{
    if (code == "129044") return new Datum129044(id, label, parent);
    if (code == "129542") return new GnssNoise129542(id, label, parent);
    if (code == "129547") return new GnssError129547(id, label, parent);
    if (code == "65359") return new PilotHeading65359(id, label, parent);
    if (code == "129285")
        return new RouteInfo129285(id, label, parent);
    if (code == "130074")
        return new WaypointList130074(id, label, parent);
    if (code == "APB")
        return new AutopilotAPB(id, label, parent);
    if (code == "RMB")
        return new NavigationRMB(id, label, parent);
    if (code == "RTE")
        return new RouteRTE(id, label, parent);
    if (code == "WPL")
        return new WaypointWPL(id, label, parent);
    if (code == "GSA")
        return new GnssActiveGSA(id, label, parent);
    if (code == "127498")
        return new EngineStatic127498(id, label, parent);
    if (code == "127509")
        return new InverterStatus127509(id, label, parent);
    if (code == "127496")
        return new VesselTrip127496(id, label, parent);
    if (code == "130311")
        return new EnvironmentalLegacy130311(id, label, parent);
    if (code == "129539")
        return new GnssDops129539(id, label, parent);
    if (code == "129540")
        return new GnssSatellites129540(id, label, parent);
    if (code == "128275")
        return new DistanceLog128275(id, label, parent);
    if (code == "130314")
        return new Pressure130314(id, label, parent);
    if (code == "127497")
        return new EngineTrip127497(id, label, parent);
    if (code == "DPT")
        return new DepthDPT(id, label, parent);
    if (code == "DBT")
        return new DepthDBT(id, label, parent);
    if (code == "MWV")
        return new WindMWV(id, label, parent);
    if (code == "MTW")
        return new WaterTemperatureMTW(id, label, parent);
    if (code == "VHW")
        return new WaterSpeedHeadingVHW(id, label, parent);
    if (code == "GSV")
        return new SatellitesGSV(id, label, parent);
    return nullptr;
}
