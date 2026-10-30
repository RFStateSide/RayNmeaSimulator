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

// Adapted from RF StateSide BrickSuite; owner-attested internal reuse (2026-10-28).
// BrickSuite retains its existing license; this adaptation is AGPL-3.0-or-later.
#include "logviewer.h"
#include "appsettings.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
namespace
{
QString readableRecord(const QJsonObject &o)
{
    auto prefix = o["utc"].toString() + " ";
    if (o.contains("category")) {
        prefix += o["severity"].toString() + " [" + o["category"].toString() + "] ";
        if (o.contains("message"))
            return prefix + o["message"].toString() +
                   (o.contains("suppressed_repeats")
                        ? " (suppressed repeats: " + o["suppressed_repeats"].toString() + ")"
                        : QString());
        if (!o.contains("source"))
            return prefix + o["event"].toString() +
                   QString(" epoch=%1 connections=%2")
                       .arg(o["binding_epoch"].toString())
                       .arg(o["connections"].toInt());
        auto identity = [](const QJsonObject &widget) {
            auto name = widget["type"].toString();
            if (widget.contains("widget_id"))
                name += QString(" #%1").arg(widget["widget_id"].toInt());
            if (widget.contains("EngineInstance"))
                name += QString(" engine=%1").arg(widget["EngineInstance"].toInt());
            return name;
        };
        return prefix + o["event"].toString() + " " + identity(o["source"].toObject()) + "." +
               o["signal"].toString() + " -> " + identity(o["destination"].toObject()) + " " +
               o["slot_or_property"].toString() + " " + o["reason"].toString();
    }
    prefix +=
        o["direction"].toString() + " " + o["protocol"].toString() + " " + o["transport"].toString() + " ";
    const auto kind = o["kind"].toString();
    if (kind == "capture-session")
        return o["utc"].toString() + " Capture session — schema 1, Qt " + o["qt"].toString() + "; " +
               o["delivery_semantics"].toString();
    if (kind == "can-frame")
        prefix += "CAN frame id=0x" + o["id"].toString() + QString(" dlc=%1 ").arg(o["dlc"].toInt());
    else if (kind == "nmea2000-message")
        prefix += QString("PGN=%1 source=%2 destination=%3 ")
                      .arg(o["pgn"].toInt())
                      .arg(o["source"].toInt())
                      .arg(o["destination"].toInt());
    else
        prefix += o["talker"].toString() + o["sentence"].toString() + " ";
    prefix += o["status"].toString() + " ";
    return prefix +
           (kind == "nmea0183-sentence"
                ? QString::fromLatin1(QByteArray::fromHex(o["payload_hex"].toString().toLatin1())).trimmed()
                : o["payload_hex"].toString());
}
} // namespace
LoggingPreferences::LoggingPreferences(QWidget *parent) : QWidget(parent)
{
    setObjectName("loggingPreferences");
    auto *form = new QFormLayout(this);
    level = new QComboBox(this);
    level->setObjectName("loggingLevel");
    level->addItems({tr("Debug"), tr("Information"), tr("Warning"), tr("Critical")});
    form->addRow(tr("Application verbosity"), level);
    bindings = new QCheckBox(tr("Show Widget Signal Connections/Disconnections"), this);
    bindings->setObjectName("loggingBindings");
    form->addRow(bindings);
    excluded = new QLineEdit(this);
    excluded->setObjectName("loggingExcludedCategories");
    excluded->setPlaceholderText(tr("Optional comma-separated category prefixes"));
    form->addRow(tr("Quiet categories"), excluded);
    auto check = [&](const QString &label, const QString &name) {
        auto *c = new QCheckBox(label, this);
        c->setObjectName(name);
        form->addRow(c);
        return c;
    };
    rx = check(tr("Capture RX"), "loggingRX");
    tx = check(tr("Capture TX submissions"), "loggingTX");
    n2k = check(tr("Capture NMEA2000"), "loggingNMEA2000");
    n183 = check(tr("Capture NMEA0183"), "loggingNMEA0183");
    raw = check(tr("Include available raw CAN RX frames (TX is logical only)"), "loggingRawCAN");
    applicationSize = new QSpinBox(this);
    applicationSize->setRange(1, 16);
    applicationSize->setSuffix(tr(" MiB per file"));
    applicationSize->setObjectName("loggingApplicationMiB");
    form->addRow(tr("Application retention (5 files)"), applicationSize);
    trafficSize = new QSpinBox(this);
    trafficSize->setRange(1, 64);
    trafficSize->setSuffix(tr(" MiB per file"));
    trafficSize->setObjectName("loggingTrafficMiB");
    form->addRow(tr("Traffic retention (5 files)"), trafficSize);
    auto *hint = new QLabel(
        tr("Traffic recording is Off on every launch. Start/Stop recording explicitly in Tools → Application "
           "Log. Raw traffic may contain private vessel identities, positions and routes. Logging does not "
           "authorize Sending. Clear Log removes the current file only; four archives are retained."),
        this);
    hint->setWordWrap(true);
    form->addRow(hint);
    load();
}
void LoggingPreferences::load()
{
    AppSettings s("RFStateSide", "RayNmeaSimulator");
    auto o = ApplicationLog::Options::load(s);
    level->setCurrentIndex(o.level);
    bindings->setChecked(o.bindings);
    rx->setChecked(o.rx);
    tx->setChecked(o.tx);
    n2k->setChecked(o.nmea2000);
    n183->setChecked(o.nmea0183);
    raw->setChecked(o.rawCan);
    applicationSize->setValue(o.applicationMiB);
    trafficSize->setValue(o.trafficMiB);
    excluded->setText(o.disabledCategories.join(", "));
}
ApplicationLog::Options LoggingPreferences::value() const
{
    ApplicationLog::Options o;
    o.level = level->currentIndex();
    o.bindings = bindings->isChecked();
    o.rx = rx->isChecked();
    o.tx = tx->isChecked();
    o.nmea2000 = n2k->isChecked();
    o.nmea0183 = n183->isChecked();
    o.rawCan = raw->isChecked();
    o.applicationMiB = applicationSize->value();
    o.trafficMiB = trafficSize->value();
    for (const auto &part : excluded->text().split(','))
        if (!part.trimmed().isEmpty())
            o.disabledCategories << part.trimmed();
    return o;
}
LogViewer::LogViewer(QWidget *parent) : QDialog(parent)
{
    setObjectName("applicationLogViewer");
    setWindowTitle(tr("RayNmeaSimulator Application Log"));
    resize(1000, 650);
    setModal(false);
    AppSettings saved("RFStateSide", "RayNmeaSimulator");
    restoreGeometry(saved.value("Logging/ViewerGeometry").toByteArray());
    connect(this, &QDialog::finished, this, [this] {
        AppSettings s("RFStateSide", "RayNmeaSimulator");
        s.setValue("Logging/ViewerGeometry", saveGeometry());
    });
    auto *form = new QVBoxLayout(this);
    auto *filters = new QHBoxLayout;
    form->addLayout(filters);
    channel = new QComboBox(this);
    channel->setObjectName("logChannel");
    channel->addItems({tr("Application Log"), tr("NMEA Traffic Log")});
    filters->addWidget(channel);
    severity = new QComboBox(this);
    severity->addItems({tr("All severities"), tr("Information+"), tr("Warning+"), tr("Critical+")});
    filters->addWidget(severity);
    category = new QLineEdit(this);
    category->setPlaceholderText(tr("Category prefix"));
    filters->addWidget(category);
    search = new QLineEdit(this);
    search->setObjectName("logSearch");
    search->setPlaceholderText(tr("Find text"));
    filters->addWidget(search);
    rawJson = new QCheckBox(tr("Raw JSON"), this);
    rawJson->setObjectName("logRawJSON");
    filters->addWidget(rawJson);
    connect(rawJson, &QCheckBox::toggled, this, [this] { refresh(); });
    location = new QLabel(this);
    location->setTextFormat(Qt::PlainText);
    location->setWordWrap(true);
    location->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addWidget(location);
    status = new QLabel(this);
    status->setObjectName("loggingHealth");
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    form->addWidget(status);
    text = new QPlainTextEdit(this);
    text->setObjectName("logText");
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setMaximumBlockCount(2000);
    form->addWidget(text);
    auto *controls = new QHBoxLayout;
    form->addLayout(controls);
    pause = new QCheckBox(tr("Pause display"), this);
    pause->setObjectName("pauseDisplay");
    pause->setToolTip(tr("Pause automatic display updates. Refresh and Clear still work; recording continues."));
    follow = new QCheckBox(tr("Follow"), this);
    follow->setToolTip(tr("Keep the newest visible log lines in view."));
    follow->setChecked(true);
    controls->addWidget(pause);
    controls->addWidget(follow);
    auto add = [&](const QString &label, auto callback) {
        auto *b = new QPushButton(label, this);
        controls->addWidget(b);
        connect(b, &QPushButton::clicked, this, callback);
        return b;
    };
    add(tr("Refresh"), [this] { refresh(); })->setObjectName("refreshLog");
    add(tr("Clear Log"), [this] {
        if (QMessageBox::question(this, tr("Clear current log"),
                                  tr("Clear this current file? The four rotated archives remain. Preserve "
                                     "evidence before clearing."),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            const auto selected = channel->currentIndex();
            ApplicationLog::clear(selected == 1);
            // Do not show pre-clear bytes while the asynchronous writer catches up.
            text->clear();
            refreshPending = false;
            refreshDisplay();
        }
    })->setObjectName("clearLog");
    add(tr("Open Log Folder"), [this] {
        const auto folder = QFileInfo(ApplicationLog::filePath(channel->currentIndex() == 1)).absolutePath();
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
            QMessageBox::warning(this, tr("Log Folder"), tr("The log folder could not be opened."));
    });
    add(tr("Export current…"), [this] {
        const bool traffic = channel->currentIndex() == 1;
        if (traffic &&
            QMessageBox::question(this, tr("Private NMEA traffic"),
                                  tr("Traffic may contain vessel identities, positions and routes. Export a "
                                     "copy of the current file?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        const auto path =
            QFileDialog::getSaveFileName(this, tr("Export current log"), {}, tr("JSON Lines (*.jsonl)"));
        if (!path.isEmpty())
            ApplicationLog::exportCurrent(traffic, path);
    });
    auto *bottom = new QHBoxLayout;
    form->addLayout(bottom);
    record = new QPushButton(this);
    record->setObjectName("trafficRecording");
    record->setToolTip(tr("Record NMEA traffic in separate rotating files. Independent of NMEA Viewer and Enable Sending; recordings may contain vessel identities, positions and routes."));
    bottom->addWidget(record);
    connect(record, &QPushButton::clicked, this, [this] {
        const auto state = ApplicationLog::trafficState();
        if (state == "Recording" || state == "Starting") {
            ApplicationLog::stopTraffic();
            refresh();
            return;
        }
        if (QMessageBox::question(this, tr("Start NMEA Traffic Recording"),
                                  tr("Record private NMEA traffic, including vessel identities, positions "
                                     "and routes, in separate rotating files? This only observes existing "
                                     "activity; it does not connect a transport or authorize Sending."),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            ApplicationLog::startTraffic();
        refresh();
    });
    auto *settings = new QPushButton(tr("Logging Settings…"), this);
    bottom->addWidget(settings);
    auto *retry = new QPushButton(tr("Retry Application Log"), this);
    bottom->addWidget(retry);
    connect(retry, &QPushButton::clicked, this, [] { ApplicationLog::retryApplication(); });
    connect(settings, &QPushButton::clicked, this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Logging Settings"));
        auto *layout = new QVBoxLayout(&dialog);
        LoggingPreferences prefs(&dialog);
        layout->addWidget(&prefs);
        QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        layout->addWidget(&buttons);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&] {
            AppSettings s("RFStateSide", "RayNmeaSimulator");
            auto value = prefs.value();
            value.save(s);
            s.sync();
            if (s.status() != QSettings::NoError) {
                QMessageBox::warning(&dialog, tr("Logging Settings"), tr("Settings could not be saved."));
                return;
            }
            ApplicationLog::configure(value);
            dialog.accept();
        });
        dialog.exec();
        refresh();
    });
    bottom->addStretch();
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    bottom->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, [this] { refreshDisplay(); });
    timer->start();
    connect(channel, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(search, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(category, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(severity, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    refresh();
}
void LogViewer::refresh()
{
    refreshPending = true;
    refreshDisplay();
}
void LogViewer::refreshDisplay()
{
    const bool traffic = channel->currentIndex() == 1;
    severity->setEnabled(!traffic);
    category->setEnabled(!traffic);
    location->setText(ApplicationLog::filePath(traffic));
    status->setText(
        tr("Application: %1; NMEA traffic: %2. Selected log: %3 records written; %4 dropped. Health: %5. "
           "Latest 256 KiB / 2000 lines.")
            .arg(ApplicationLog::health(false), ApplicationLog::trafficState())
            .arg(ApplicationLog::written(traffic))
            .arg(ApplicationLog::dropped(traffic))
            .arg(ApplicationLog::health(traffic)));
    const auto state = ApplicationLog::trafficState();
    record->setText(state == "Recording" || state == "Starting" ? tr("Stop Traffic Recording")
                                                                : tr("Start Traffic Recording"));
    record->setEnabled(state != "Stopping");
    if (ApplicationLog::clearing(traffic))
        return;
    if (pause->isChecked() && !refreshPending)
        return; // Pause suspends automatic text refresh, never explicit actions.
    QByteArray tail;
    const auto available = ApplicationLog::readTail(traffic, tail);
    if (available == 0)
        return;
    refreshPending = false;
    QString content;
    if (available == 1) {
        for (const auto &line : QString::fromUtf8(tail).split('\n')) {
            if (line.isEmpty())
                continue;
            const auto o = QJsonDocument::fromJson(line.toUtf8()).object();
            if (o.isEmpty())
                continue;
            if (!traffic) {
                const int level = QStringList{"Debug", "Information", "Warning", "Critical", "Fatal"}.indexOf(
                    o["severity"].toString());
                if (level < severity->currentIndex() ||
                    !o["category"].toString().startsWith(category->text()))
                    continue;
            }
            if (line.contains(search->text(), Qt::CaseInsensitive))
                content += (rawJson->isChecked() ? line : readableRecord(o)) + '\n';
        }
    } else
        content = traffic ? tr("No recorded traffic file. Recording is Off until explicitly started.")
                          : tr("Application log is unavailable. See logging-health status.");
    const auto scroll = text->verticalScrollBar()->value();
    const bool bottom = text->verticalScrollBar()->value() >= text->verticalScrollBar()->maximum();
    if (text->toPlainText() != content) {
        text->setPlainText(content);
        if (follow->isChecked() && bottom)
            text->verticalScrollBar()->setValue(text->verticalScrollBar()->maximum());
        else
            text->verticalScrollBar()->setValue(qMin(scroll, text->verticalScrollBar()->maximum()));
    }
}
