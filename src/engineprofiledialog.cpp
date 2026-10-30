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

#include "engineprofiledialog.h"
#include "apppaths.h"
#include "appsettings.h"
#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QLineSeries>
#include <QScatterSeries>
#include <QStandardPaths>
#include <QTabWidget>
#include <QValueAxis>
#include <QPainter>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
QString numeric(const QJsonValue &v) { return v.isDouble() ? QLocale::c().toString(v.toDouble(), 'g', QLocale::FloatingPointShortest) : v.toVariant().toString(); }
QJsonValue numberOrInvalid(const QString &text) {
    bool ok = false;
    const double n = text.toDouble(&ok);
    // Keep invalid text in the draft so full-catalog validation cannot silently
    // turn a failed conversion into a valid zero.
    return ok && std::isfinite(n) ? QJsonValue(n) : QJsonValue(text);
}
}

EngineProfileDialog::EngineProfileDialog(std::function<QStringList()> runningEngines, QWidget *parent)
    : QDialog(parent), running(std::move(runningEngines))
{
    setObjectName("engineProfileEditor");
    setWindowModality(Qt::NonModal);
    setWindowTitle(tr("Engine Profiles"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(980, 720);
    auto *layout = new QVBoxLayout(this);
    location = new QLabel(this);
    location->setObjectName("catalogLocation");
    location->setTextFormat(Qt::PlainText);
    location->setToolTip(tr("Effective profiles identifies the catalog being read. Editable local target is where Save writes; saving a legacy or bundled source creates a local copy."));
    location->setWordWrap(true);
    location->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(location);
    auto *hint = new QLabel(tr("Edit a draft here. Stop all simulated engines before Save or Restore. Renaming or deleting a profile does not rewrite saved Layouts."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *split = new QSplitter(this);
    layout->addWidget(split, 1);
    auto *left = new QWidget(split);
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 8, 0);
    profiles = new QListWidget(left);
    profiles->setObjectName("engineProfiles");
    profiles->setAccessibleName(tr("Engine profiles"));
    ll->addWidget(profiles);
    auto *commands = new QHBoxLayout;
    ll->addLayout(commands);
    auto addButton = [this](QBoxLayout *where, const QString &text, const char *id, auto callback) {
        auto *button = new QPushButton(text, this);
        button->setObjectName(id);
        button->setAutoDefault(false);
        where->addWidget(button);
        connect(button, &QPushButton::clicked, this, callback);
        return button;
    };
    addButton(commands, tr("&New"), "newProfile", [this] { create(false); });
    addButton(commands, tr("&Duplicate"), "duplicateProfile", [this] { create(true); });
    addButton(commands, tr("&Delete"), "deleteProfile", [this] { remove(); });
    auto *right = new QWidget(split);
    auto *rl = new QVBoxLayout(right);
    rl->setContentsMargins(8, 0, 0, 0);
    auto *form = new QFormLayout;
    rl->addLayout(form);
    name = new QLineEdit(right); name->setObjectName("profileName");
    form->addRow(tr("Engine &name:"), name);
    auto optionalField = [&](const QString &label, const char *id, QWidget *field, QCheckBox **enabled) {
        auto *row = new QWidget(right);
        auto *rowLayout = new QHBoxLayout(row); rowLayout->setContentsMargins(0, 0, 0, 0);
        *enabled = new QCheckBox(tr("Specified"), row);
        (*enabled)->setObjectName(QString::fromLatin1(id) + "Specified");
        (*enabled)->setAccessibleName(label + tr(" specified"));
        field->setObjectName(id);
        field->setAccessibleName(label);
        (*enabled)->setToolTip(tr("Uncheck to leave this property unspecified. Existing absent properties remain absent."));
        rowLayout->addWidget(*enabled); rowLayout->addWidget(field, 1);
        form->addRow(label, row);
        connect(*enabled, &QCheckBox::toggled, field, &QWidget::setEnabled);
    };
    horsepower = new QLineEdit(right);
    optionalField(tr("Horsepower (HP):"), "horsepower", horsepower, &hasHorsepower);
    fuelType = new QComboBox(right); fuelType->setEditable(true); fuelType->addItems({"Gasoline", "Diesel"});
    fuelType->setInsertPolicy(QComboBox::NoInsert);
    fuelType->setToolTip(tr("Select Gasoline or Diesel, or enter another fuel type. Historical values are preserved."));
    optionalField(tr("Fuel type:"), "fuelType", fuelType, &hasFuel);
    voltage = new QComboBox(right); voltage->setEditable(true); voltage->setInsertPolicy(QComboBox::NoInsert);
    for (int volts : {12, 24, 32, 48}) voltage->addItem(tr("%1 VDC").arg(volts), volts);
    voltage->setToolTip(tr("Select a common DC voltage or enter whole volts (0–1000). This does not change the electrical simulation."));
    optionalField(tr("Alternator (V):"), "alternatorVoltage", voltage, &hasVoltage);
    injected = new QComboBox(right); injected->setObjectName("fuelInjected");
    induction = new QComboBox(right); induction->setObjectName("forcedInduction");
    for (auto *combo : {injected, induction}) combo->addItems({tr("Not specified"), tr("No"), tr("Yes")});
    form->addRow(tr("Fuel injected:"), injected);
    form->addRow(tr("Forced induction:"), induction);
    rl->addWidget(new QLabel(tr("Fuel curve — engine RPM / US gallons per hour (GPH)"), right));
    auto *tabs = new QTabWidget(right); tabs->setObjectName("fuelCurveTabs");
    rl->addWidget(tabs, 1);
    auto *tableTab = new QWidget(tabs); auto *tableLayout = new QVBoxLayout(tableTab);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tabs->addTab(tableTab, tr("RPM / GPH Table"));
    curve = new QTableWidget(0, 2, tableTab);
    curve->setObjectName("fuelCurve");
    curve->setAccessibleName(tr("Fuel curve RPM and US GPH"));
    curve->setToolTip(tr("Edit numeric RPM / US gallons per hour pairs in this draft. Save requires at least two points with unique RPM values; editing does not change running engines."));
    curve->setHorizontalHeaderLabels({tr("RPM"), tr("US GPH")});
    curve->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    curve->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableLayout->addWidget(curve, 1);
    auto *rows = new QHBoxLayout;
    tableLayout->addLayout(rows);
    addButton(rows, tr("Add &row"), "addCurveRow", [this] {
        const int row = curve->rowCount(); curve->insertRow(row);
        for (int c = 0; c < 2; ++c) curve->setItem(row, c, new QTableWidgetItem);
        curve->setCurrentCell(row, 0); curve->editItem(curve->item(row, 0));
    });
    addButton(rows, tr("Remove rows"), "removeCurveRows", [this] {
        const auto indices = curve->selectionModel()->selectedRows();
        QList<int> selectedRows;
        for (const auto &i : indices) selectedRows.append(i.row());
        std::sort(selectedRows.begin(), selectedRows.end(), std::greater<int>());
        for (int row : selectedRows) curve->removeRow(row);
    });
    addButton(rows, tr("Sort by RPM"), "sortCurve", [this] {
        flush();
        if (selected < 0) return;
        auto all = draft.value("engines").toArray(); auto p = all[selected].toObject();
        auto points = p.value(EngineProfileCatalog::curveKey(p)).toArray();
        QList<QJsonValue> ordered;
        for (const auto &point : points) {
            if (!point.toObject().value("rpm").isDouble()) { message(tr("Enter numeric RPM values before sorting."), true); return; }
            ordered.append(point);
        }
        std::stable_sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
            return a.toObject().value("rpm").toDouble() < b.toObject().value("rpm").toDouble();
        });
        QJsonArray sorted; for (const auto &p : ordered) sorted.append(p);
        p[EngineProfileCatalog::curveKey(p)] = sorted; all[selected] = p; draft["engines"] = all;
        select(selected);
    });
    auto *graphTab = new QWidget(tabs); auto *graphLayout = new QVBoxLayout(graphTab);
    graphLayout->setContentsMargins(0, 0, 0, 0);
    tabs->addTab(graphTab, tr("Fuel Consumption Graph"));
    auto *chart = new QChart;
    chart->legend()->hide(); chart->setBackgroundRoundness(0);
    graphLine = new QLineSeries(chart); graphLine->setName("Linear interpolation");
    graphPoints = new QScatterSeries(chart); graphPoints->setMarkerSize(8);
    chart->addSeries(graphLine); chart->addSeries(graphPoints);
    rpmAxis = new QValueAxis(chart); rpmAxis->setTitleText(tr("Engine RPM")); rpmAxis->setLabelFormat("%.0f");
    gphAxis = new QValueAxis(chart); gphAxis->setTitleText(tr("US gallons per hour (GPH)")); gphAxis->setLabelFormat("%g");
    chart->addAxis(rpmAxis, Qt::AlignBottom); chart->addAxis(gphAxis, Qt::AlignLeft);
    for (QAbstractSeries *series : QList<QAbstractSeries *>{graphLine, graphPoints}) {
        series->attachAxis(rpmAxis); series->attachAxis(gphAxis);
    }
    graph = new QChartView(chart, graphTab); graph->setObjectName("fuelCurveGraph");
    graph->setRenderHint(QPainter::Antialiasing);
    graph->setToolTip(tr("Preview of the draft fuel curve with linear interpolation between RPM / US GPH points. Edit the table to change it; the graph does not apply or save profiles."));
    graph->setAccessibleName(tr("Fuel consumption graph: engine RPM versus US gallons per hour"));
    graphLayout->addWidget(graph, 1);
    graphStatus = new QLabel(graphTab); graphStatus->setObjectName("fuelGraphStatus");
    graphStatus->setTextFormat(Qt::PlainText); graphStatus->setWordWrap(true);
    graphLayout->addWidget(graphStatus);
    split->setSizes({340, 600});
    status = new QLabel(this); status->setObjectName("catalogStatus");
    status->setTextFormat(Qt::PlainText); status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status);
    auto *bottom = new QHBoxLayout;
    layout->addLayout(bottom);
    addButton(bottom, tr("&Backup Profiles…"), "backupCatalog", [this] { backup(); });
    addButton(bottom, tr("Restore Profiles from File…"), "restoreCatalog", [this] { restore(false); });
    addButton(bottom, tr("Restore Shipped Profiles…"), "restoreBundled", [this] { restore(true); });
    bottom->addStretch();
    addButton(bottom, tr("&Save Profiles"), "saveCatalog", [this] { save(); });
    addButton(bottom, tr("&Close"), "closeEditor", [this] { reject(); });
    connect(profiles, &QListWidget::currentRowChanged, this, [this](int row) {
        if (loading) return;
        flush(); select(row);
    });
    // Keep the draft current without committing it to disk. Signals during a
    // selection refresh are blocked logically, preserving absent optional fields.
    for (auto *edit : {name, horsepower})
        connect(edit, &QLineEdit::textChanged, this, [this] { flush(); });
    for (auto *combo : {fuelType, voltage})
        connect(combo, &QComboBox::currentTextChanged, this, [this] { flush(); });
    for (auto *check : {hasHorsepower, hasFuel, hasVoltage})
        connect(check, &QCheckBox::toggled, this, [this] { flush(); });
    for (auto *combo : {injected, induction})
        connect(combo, &QComboBox::currentIndexChanged, this, [this] { flush(); });
    connect(curve, &QTableWidget::itemChanged, this, [this] { flush(); });
    const QList<QPair<const char *, QString>> tips {
        {"newProfile", tr("Create a draft profile. Enter at least two RPM / US GPH points before saving.")},
        {"duplicateProfile", tr("Make an independent draft copy with a unique name; the original is unchanged.")},
        {"deleteProfile", tr("Remove the selected draft profile after confirmation. Save to apply; existing Layout references are not rewritten.")},
        {"addCurveRow", tr("Add an empty row for a measured engine RPM / US GPH pair.")},
        {"removeCurveRows", tr("Remove selected rows from the draft curve. At least two valid points are required to save.")},
        {"sortCurve", tr("Sort draft curve points by numeric RPM. Duplicate RPM values still require correction.")},
        {"backupCatalog", tr("Export all saved profiles to a chosen JSON backup. Unsaved draft edits are excluded. Automatic .bak safety snapshots are separate.")},
        {"restoreCatalog", tr("Validate a JSON backup and, after confirmation, replace local profiles. Preserve a safety snapshot first; stop engines before restoring.")},
        {"restoreBundled", tr("Restore the original 25 shipped profiles after confirmation. This replaces custom local profiles, preserving a safety snapshot first.")},
        {"saveCatalog", tr("Validate and atomically save all draft profiles, preserving a safety snapshot. Stop every simulated engine first; Sending is unchanged.")},
        {"closeEditor", tr("Close this independent window; unsaved changes offer Save, Discard or Cancel.")}
    };
    for (const auto &tip : tips) findChild<QPushButton *>(tip.first)->setToolTip(tip.second);
    QString error;
    const bool valid = catalog.open(&error);
    draft = catalog.document();
    paths(); rebuild(0);
    message(valid ? tr("Saved profiles loaded. Changes remain a draft until Save Profiles.") :
        error + tr("\nThe source is preserved. Restore a valid backup or shipped profiles. If unreadable, repair access and reopen first."), !valid);
}

void EngineProfileDialog::paths() {
    location->setText(tr("Effective profiles: %1\nEditable local target: %2%3")
        .arg(catalog.sourcePath(), catalog.targetPath(), catalog.needsLocalCopy() ? tr("\nSave creates a local copy; the legacy/bundled source is preserved.") : QString()));
}
void EngineProfileDialog::message(const QString &text, bool error) {
    status->setText((error ? tr("Not saved: ") : QString()) + text);
    status->setAccessibleDescription(status->text());
}
bool EngineProfileDialog::confirm(const QString &text) {
    return QMessageBox::question(this, tr("Engine Profiles"), text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}
bool EngineProfileDialog::dirty() const { return draft != catalog.document(); }

QString EngineProfileDialog::backupDirectory() {
    AppSettings settings(APP_COMPANY, APP_NAME);
    const QString remembered = settings.value("EngineProfiles/BackupDirectory").toString();
    if (QDir::isAbsolutePath(remembered) && QFileInfo(remembered).isDir()) return remembered;
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#ifdef RAYNMEA_TESTING
    documents = TestProfile::root() + "/Documents";
#endif
    if (documents.isEmpty()) documents = QDir::homePath();
    const QString folder = QDir(documents).filePath("RFStateSide/RayNmeaSimulator/Backups/EngineProfiles");
    // First use provisions only the export directory; cancelling the chooser
    // never updates the remembered location or writes profile data.
    return AppPaths::ensureDirectory(folder) ? folder : documents;
}

void EngineProfileDialog::changeEvent(QEvent *event) {
    QDialog::changeEvent(event);
    if (graph && (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)) refreshGraph();
}

void EngineProfileDialog::refreshGraph() {
    if (!graph) return;
    auto *chart = graph->chart();
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    const auto theme = dark ? QChart::ChartThemeDark : QChart::ChartThemeLight;
    if (chart->theme() != theme) chart->setTheme(theme);
    graphLine->setColor(palette().color(QPalette::Highlight));
    graphPoints->setColor(palette().color(QPalette::Highlight));
    graphPoints->setBorderColor(palette().color(QPalette::WindowText));
    const auto engines = draft.value("engines").toArray();
    const auto profile = selected >= 0 && selected < engines.size() ? engines[selected].toObject() : QJsonObject();
    const auto points = profile.value(EngineProfileCatalog::curveKey(profile)).toArray();
    QList<QPointF> visible;
    double maxRpm = 1, maxGph = 0;
    for (const auto &point : points) {
        const auto pair = point.toObject(); const auto rpm = pair.value("rpm"), gph = pair.value("gph");
        const double x = rpm.toDouble(-1), y = gph.toDouble(-1);
        if (!rpm.isDouble() || !gph.isDouble() || !std::isfinite(x) || !std::isfinite(y) ||
            x <= 0 || x > 100000 || x != std::floor(x) || y < 0) continue;
        visible.append({x, y}); maxRpm = qMax(maxRpm, x); maxGph = qMax(maxGph, y);
    }
    QString error;
    const bool valid = EngineProfileCatalog::validate(QJsonObject{{"engines", QJsonArray{QJsonObject{
        {"name", "Preview"}, {"rpm_gph_pairs", points}}}}}, &error);
    // Extremely large finite user numbers can be valid JSON but exceed a useful
    // plotting range. Do not fabricate points or change the persisted draft.
    const bool plottable = maxGph < 1e300;
    graphPoints->replace(plottable ? visible : QList<QPointF>());
    graphLine->replace(valid && plottable ? visible : QList<QPointF>());
    rpmAxis->setRange(0, maxRpm * 1.05);
    gphAxis->setRange(0, plottable ? qMax(1.0, maxGph * 1.1) : 1.0);
    graphStatus->setText(!plottable ? tr("Values exceed the graph's display range; draft data is unchanged.") :
        valid ? tr("Draft points with linear interpolation. No extrapolated curve is shown.") :
        tr("Curve incomplete or invalid — valid points only; no interpolation line. %1").arg(error));
    graph->setAccessibleDescription(graphStatus->text());
}

void EngineProfileDialog::flush() {
    if (loading || selected < 0) return;
    auto all = draft.value("engines").toArray();
    if (selected >= all.size()) return;
    auto p = all[selected].toObject();
    p["name"] = name->text();
    auto optional = [&p](const char *key, bool present, const QJsonValue &value) {
        if (present) p[key] = value; else p.remove(key);
    };
    optional("horsepower", hasHorsepower->isChecked(), numberOrInvalid(horsepower->text()));
    optional("fuelType", hasFuel->isChecked(), fuelType->currentText());
    QString volts = voltage->currentText().trimmed();
    if (volts.endsWith(" VDC", Qt::CaseInsensitive)) volts.chop(4);
    optional("alternatorVoltage", hasVoltage->isChecked(), numberOrInvalid(volts));
    optional("fuelInjected", injected->currentIndex() != 0, injected->currentIndex() == 2);
    optional("forcedInduction", induction->currentIndex() != 0, induction->currentIndex() == 2);
    QJsonArray points;
    for (int row = 0; row < curve->rowCount(); ++row) {
        auto *rpm = curve->item(row, 0), *gph = curve->item(row, 1);
        auto pair = rpm ? rpm->data(Qt::UserRole).toJsonObject() : QJsonObject();
        pair["rpm"] = numberOrInvalid(rpm ? rpm->text() : QString());
        pair["gph"] = numberOrInvalid(gph ? gph->text() : QString());
        points.append(pair);
    }
    p[EngineProfileCatalog::curveKey(p)] = points;
    all[selected] = p; draft["engines"] = all;
    if (auto *item = profiles->item(selected)) item->setText(name->text());
    refreshGraph();
}

void EngineProfileDialog::select(int row) {
    loading = true; selected = row;
    const auto all = draft.value("engines").toArray();
    const bool exists = row >= 0 && row < all.size();
    for (const char *id : {"duplicateProfile", "deleteProfile", "addCurveRow", "removeCurveRows", "sortCurve"})
        findChild<QPushButton *>(id)->setEnabled(exists);
    const auto p = exists ? all[row].toObject() : QJsonObject();
    name->setText(p.value("name").toString());
    auto loadPresence = [&](const char *key, QWidget *field, QCheckBox *check) {
        check->setChecked(p.contains(key)); field->setEnabled(exists && p.contains(key)); check->setEnabled(exists);
    };
    loadPresence("horsepower", horsepower, hasHorsepower); horsepower->setText(numeric(p.value("horsepower")));
    loadPresence("fuelType", fuelType, hasFuel); fuelType->setCurrentText(p.value("fuelType").toString());
    loadPresence("alternatorVoltage", voltage, hasVoltage);
    const int voltageIndex = p.contains("alternatorVoltage") ? voltage->findData(p.value("alternatorVoltage").toInt()) : -1;
    if (voltageIndex >= 0) voltage->setCurrentIndex(voltageIndex);
    else voltage->setEditText(numeric(p.value("alternatorVoltage")));
    injected->setCurrentIndex(p.contains("fuelInjected") ? (p.value("fuelInjected").toBool() ? 2 : 1) : 0);
    induction->setCurrentIndex(p.contains("forcedInduction") ? (p.value("forcedInduction").toBool() ? 2 : 1) : 0);
    for (QWidget *w : QList<QWidget *>{name, injected, induction, curve}) w->setEnabled(exists);
    const auto points = p.value(EngineProfileCatalog::curveKey(p)).toArray();
    curve->setRowCount(points.size());
    for (int r = 0; r < points.size(); ++r) {
        const auto point = points[r].toObject();
        for (int c = 0; c < 2; ++c) {
            auto *item = new QTableWidgetItem(numeric(point.value(c ? "gph" : "rpm")));
            if (!c) item->setData(Qt::UserRole, point);
            curve->setItem(r, c, item);
        }
    }
    loading = false;
    refreshGraph();
}

void EngineProfileDialog::rebuild(int row) {
    const bool editable = !catalog.document().isEmpty();
    for (const char *id : {"newProfile", "duplicateProfile", "deleteProfile", "saveCatalog", "addCurveRow", "removeCurveRows", "sortCurve"})
        findChild<QPushButton *>(id)->setEnabled(editable);
    loading = true; profiles->clear();
    for (const auto &p : draft.value("engines").toArray()) profiles->addItem(p.toObject().value("name").toString());
    row = profiles->count() ? qBound(0, row, profiles->count() - 1) : -1;
    profiles->setCurrentRow(row); loading = false; select(row);
}
void EngineProfileDialog::create(bool duplicate) {
    flush();
    auto all = draft.value("engines").toArray();
    if (duplicate && (selected < 0 || selected >= all.size())) return;
    QJsonObject p = duplicate ? all[selected].toObject() : QJsonObject{{"name", "New engine"}, {"horsepower", 100},
        {"fuelType", "Gasoline"}, {"fuelInjected", false}, {"forcedInduction", false}, {"alternatorVoltage", 12},
        {"rpm_gph_pairs", QJsonArray()}};
    p["name"] = EngineProfileCatalog::uniqueName(draft, p.value("name").toString() + (duplicate ? tr(" copy") : QString()));
    all.append(p); draft["engines"] = all; rebuild(all.size() - 1);
    message(duplicate ? tr("Independent copy created; edit and Save Profiles to apply.") : tr("Add at least two measured RPM / US GPH pairs before saving. No performance curve has been invented."));
    name->setFocus(); name->selectAll();
}
void EngineProfileDialog::remove() {
    flush(); auto all = draft.value("engines").toArray();
    if (selected < 0 || selected >= all.size()) return;
    if (!confirm(tr("Delete profile '%1' from this draft? Active selections and saved Layouts using this exact name will become unavailable after Save. Layout files are not rewritten.").arg(all[selected].toObject().value("name").toString()))) return;
    all.removeAt(selected); draft["engines"] = all; rebuild(selected);
    message(tr("Profile removed from draft. Save to apply, or Close and discard to cancel."));
}
bool EngineProfileDialog::canApply() {
    const QStringList engines = running ? running() : QStringList();
    if (!engines.isEmpty()) {
        message(tr("Stop all simulated engines before saving or restoring. Still running:\n%1\nIn the Generate tab, uncheck Start Engine in each listed engine widget. Movement controls alone do not confirm that the engines have stopped. Your draft changes are retained.")
            .arg(engines.join('\n')), true);
        return false;
    }
    return true;
}
bool EngineProfileDialog::save() {
    flush(); QString error;
    if (!EngineProfileCatalog::validate(draft, &error)) { message(error, true); return false; }
    if (!canApply()) return false;
    QStringList removed;
    for (const auto &old : catalog.document().value("engines").toArray()) {
        const auto oldName = old.toObject().value("name").toString();
        bool exists = false;
        for (const auto &p : draft.value("engines").toArray()) if (p.toObject().value("name").toString() == oldName) exists = true;
        if (!exists) removed.append(oldName);
    }
    if (!removed.isEmpty() && !confirm(tr("Renamed or removed profiles:\n%1\n\nExisting selections using these exact names will be unavailable. Saved Layouts are not rewritten. Apply?").arg(removed.join('\n')))) return false;
    if (catalog.needsLocalCopy() && !confirm(tr("Create an editable local profile file at\n%1\nfrom\n%2?\nThe existing source is preserved. The local profiles take precedence on future starts.").arg(catalog.targetPath(), catalog.sourcePath()))) return false;
    if (!canApply()) return false; // A modeless editor's confirmation runs a nested event loop.
    if (!catalog.save(draft, &error)) { message(error, true); return false; }
    draft = catalog.document(); paths();
    emit catalogSaved();
    message(tr("Profiles saved. Stopped engine selections and Setup refreshed; missing names remain unavailable. Automatic safety snapshot: %1").arg(catalog.lastBackup()));
    return true;
}
void EngineProfileDialog::backup() {
    const QString path = QFileDialog::getSaveFileName(this, tr("Backup Profiles — saved values, not unsaved draft edits"),
        QDir(backupDirectory()).filePath("engine_profiles-backup.json"), tr("JSON profiles (*.json)"), nullptr, QFileDialog::DontConfirmOverwrite);
    if (path.isEmpty()) return;
    const bool exists = QFile::exists(path);
    if (exists && !confirm(tr("Replace existing backup %1?").arg(path))) return;
    QString error;
    if (!catalog.backup(path, exists, &error)) message(error, true);
    else {
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("EngineProfiles/BackupDirectory", QFileInfo(path).absolutePath()); settings.sync();
        message(tr("Saved profiles exported to %1. Unsaved draft edits are excluded; automatic .bak safety snapshots are separate.").arg(path));
    }
}
void EngineProfileDialog::restore(bool bundled) {
    if (!canApply()) return;
    const QString path = bundled ? QStringLiteral(":/Data/engine_profiles.json") :
        QFileDialog::getOpenFileName(this, tr("Restore Profiles from File"), backupDirectory(), tr("JSON profiles (*.json);;All files (*)"));
    if (path.isEmpty()) return;
    QFile file(path); QString error; QJsonObject valid;
    if (!file.open(QIODevice::ReadOnly)) { message(tr("Cannot read restore profiles: %1").arg(file.errorString()), true); return; }
    const auto bytes = file.readAll();
    if (file.error() != QFile::NoError || !EngineProfileCatalog::parse(bytes, &valid, &error)) { message(error.isEmpty() ? file.errorString() : error, true); return; }
    if (!canApply()) return;
    const QString sourceDescription = bundled ? tr("the original 25 shipped profiles (%1). This replaces any customized local profile set").arg(path) : path;
    if (!confirm(tr("Replace local profiles with %1?\nA complete automatic .bak safety snapshot of the current saved profiles will be kept beside the local file before replacement. Legacy sources and saved Layouts are preserved. Missing profile names become unavailable.%2")
        .arg(sourceDescription, dirty() ? tr("\nUnsaved draft edits will be discarded.") : QString()))) return;
    if (!canApply()) return;
    if (!catalog.restore(bytes, &error)) { message(error, true); return; }
    draft = catalog.document(); paths(); rebuild(0); emit catalogSaved();
    message(tr("Profiles restored. Previous saved profiles preserved at %1").arg(catalog.lastBackup()));
}
void EngineProfileDialog::reject() {
    flush();
    if (dirty()) {
        const auto answer = QMessageBox::warning(this, tr("Unsaved engine profiles"), tr("Save profile changes before closing?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer == QMessageBox::Cancel || (answer == QMessageBox::Save && !save())) return;
    }
    QDialog::reject();
}
