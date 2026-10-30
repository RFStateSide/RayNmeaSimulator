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

#include "nmea2000diagnosticsdialog.h"
#include "nmea2000diagnostics.h"
#include "nmeadevicediscovery.h"
#include "appsettings.h"
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

namespace
{
QString selectedKey(QTableWidget *table)
{
    auto *item = table->item(table->currentRow(), 0);
    return item ? item->data(Qt::UserRole).toString() : QString();
}
QString codeName(int code, const QString &name)
{
    return code < 0 ? QString("Unavailable")
                    : QString::number(code) + (name.isEmpty() ? " (unlisted)" : " - " + name);
}
QString stamp(const QDateTime &d)
{
    return d.isValid() ? d.toUTC().toString(Qt::ISODateWithMs) : QString("Unavailable");
}
} // namespace
Nmea2000DiagnosticsDialog::Nmea2000DiagnosticsDialog(Nmea2000Diagnostics *state, QWidget *parent,
                                                   NmeaDeviceDiscovery *discovery)
    : QDialog(parent), model(state)
{
    setObjectName("nmea2000Diagnostics");
    setWindowTitle(tr("NMEA2000 Diagnostics"));
    resize(1200, 800);
    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(
        tr("Refresh view is local only. Discover Devices explicitly requests device identity/product information; "
           "it does not enable instrument Sending or Traffic Recording. Values are latest snapshots, not a complete traffic history."),
        this);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *controls = new QHBoxLayout;
    layout->addLayout(controls);
    auto *refreshButton = new QPushButton(tr("Refresh view"), this);
    refreshButton->setObjectName("diagnosticRefresh");
    refreshButton->setToolTip(tr("Refresh these local snapshots without transmitting requests."));
    controls->addWidget(refreshButton);
    connect(refreshButton, &QPushButton::clicked, this, [this] { refresh(); });
    auto *discoverButton=new QPushButton(tr("Discover Devices"),this);
    discoverButton->setObjectName("diagnosticDiscover");controls->addWidget(discoverButton);
    discoverButton->setToolTip(tr("Explicitly request device identity on a supported transport. This does not authorize instrument Sending or Traffic Recording."));
    auto *cancelDiscovery=new QPushButton(tr("Cancel discovery"),this);
    cancelDiscovery->setObjectName("diagnosticCancelDiscovery");controls->addWidget(cancelDiscovery);
    discoverButton->setEnabled(discovery);cancelDiscovery->setEnabled(false);
    connect(discoverButton,&QPushButton::clicked,this,&Nmea2000DiagnosticsDialog::discoveryRequested);
    discoveryStatus=new QLabel(this);discoveryStatus->setObjectName("discoveryStatus");
    discoveryStatus->setTextFormat(Qt::PlainText);discoveryStatus->setWordWrap(true);
    if(discovery) {
        auto update=[this,discovery,discoverButton,cancelDiscovery]{
            discoveryFeedback(discovery->status());discoverButton->setEnabled(!discovery->busy());cancelDiscovery->setEnabled(discovery->busy());
        };
        connect(discovery,&NmeaDeviceDiscovery::changed,this,update);update();
        connect(cancelDiscovery,&QPushButton::clicked,discovery,[discovery]{discovery->cancel();});
        connect(this,&QDialog::finished,discovery,[discovery]{discovery->cancel();});
    } else discoveryFeedback(tr("Active discovery is unavailable in this standalone observation view."));
    layout->addWidget(discoveryStatus);
    pause = new QCheckBox(tr("Pause display"), this);
    pause->setObjectName("diagnosticPause");
    controls->addWidget(pause);
    filter = new QLineEdit(this);
    filter->setObjectName("diagnosticFilter");
    filter->setPlaceholderText(tr("Filter PGN, source, model or interpretation"));
    controls->addWidget(filter, 1);
    connect(filter, &QLineEdit::textChanged, this, [this] { refresh(); });
    auto *logs = new QPushButton(tr("Application / Traffic Log…"), this);
    controls->addWidget(logs);
    connect(logs, &QPushButton::clicked, this, &Nmea2000DiagnosticsDialog::openApplicationLog);
    auto *split = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(split, 1);
    auto *left = new QSplitter(Qt::Vertical, split);
    devices = new QTableWidget(left);
    devices->setObjectName("observedDevices");
    devices->setColumnCount(6);
    devices->setHorizontalHeaderLabels({tr("Source"), tr("Manufacturer"), tr("Observed model"),
                                        tr("Identity / state"), tr("Messages"), tr("Last seen (UTC)")});
    messages = new QTableWidget(left);
    messages->setObjectName("observedPgns");
    messages->setColumnCount(7);
    messages->setHorizontalHeaderLabels({tr("PGN"), tr("Source device at receipt"), tr("Function / variant"),
                                         tr("Interpretation"), tr("Count"), tr("Bytes"),
                                         tr("Last seen (UTC)")});
    for (auto *table : {devices, messages}) {
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        table->horizontalHeader()->setStretchLastSection(true);
        table->setWordWrap(false);
    }
    devices->setColumnWidth(0, 65);
    devices->setColumnWidth(1, 140);
    devices->setColumnWidth(2, 140);
    devices->setColumnWidth(3, 230);
    messages->setColumnWidth(0, 75);
    messages->setColumnWidth(1, 250);
    messages->setColumnWidth(2, 230);
    messages->setColumnWidth(3, 240);
    auto *right = new QWidget(split);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    details = new QTreeWidget(right);
    details->setObjectName("diagnosticDetails");
    details->setColumnCount(3);
    details->setHeaderLabels({tr("Property"), tr("Value"), tr("Qualification / units")});
    details->setColumnWidth(0, 170);
    details->setColumnWidth(1, 220);
    rightLayout->addWidget(details, 3);
    raw = new QPlainTextEdit(right);
    raw->setObjectName("diagnosticRaw");
    raw->setReadOnly(true);
    raw->setMaximumBlockCount(32);
    raw->setPlaceholderText(tr("Complete latest logical payload (hex), including unknown/reserved bytes"));
    rightLayout->addWidget(raw, 1);
    split->setSizes({750, 450});
    left->setSizes({220, 400});
    connect(devices, &QTableWidget::itemSelectionChanged, this, &Nmea2000DiagnosticsDialog::showDevice);
    connect(messages, &QTableWidget::itemSelectionChanged, this, &Nmea2000DiagnosticsDialog::showMessage);
    status = new QLabel(this);
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    layout->addWidget(status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, [this] { refresh(false); });
    timer->start();
    AppSettings s("RFStateSide", "RayNmeaSimulator");
    restoreGeometry(s.value("Diagnostics/Geometry").toByteArray());
    connect(this, &QDialog::finished, this, [this] {
        AppSettings s("RFStateSide", "RayNmeaSimulator");
        s.setValue("Diagnostics/Geometry", saveGeometry());
    });
    refresh();
    if(discovery) connect(discovery,&NmeaDeviceDiscovery::changed,this,[this,discovery]{
        // Completion/cancellation belongs to the explicit user action. Show its
        // results even while automatic display updates remain paused.
        if(!discovery->busy())refresh();
    });
}
void Nmea2000DiagnosticsDialog::discoveryFeedback(const QString &message){discoveryStatus->setText(message);}
void Nmea2000DiagnosticsDialog::refresh(bool explicitRequest)
{
    if (!model) {
        status->setText(tr("Observation context is no longer available."));
        return;
    }
    status->setText(tr("%1 | %2 devices/history, %3 latest PGN rows; %4 cache evictions. Traffic recording "
                       "is independent. Select a row for complete details.")
                        .arg(model->sessionLabel())
                        .arg(model->devices().size())
                        .arg(model->messages().size())
                        .arg(model->evictions()));
    QString deviceStates;
    for (const auto &d : model->devices())
        deviceStates += d.key + model->deviceState(d) + '\n';
    if (!explicitRequest && (pause->isChecked() || (displayedRevision == model->revision() &&
                                                    displayedDeviceStates == deviceStates)))
        return;
    displayedDeviceStates = deviceStates;
    const auto deviceKey = selectedKey(devices), messageKey = selectedKey(messages);
    QSignalBlocker deviceSignals(devices), messageSignals(messages);
    auto setRow = [](QTableWidget *t, int row, const QString &key, const QStringList &cells) {
        for (int c = 0; c < cells.size(); ++c) {
            auto *item = new QTableWidgetItem(cells[c]);
            item->setData(Qt::UserRole, key);
            item->setToolTip(cells[c]);
            t->setItem(row, c, item);
        }
    };
    const auto allDevices = model->devices();
    devices->setRowCount(allDevices.size());
    int at = 0, selectedDevice = -1;
    for (const auto &d : allDevices) {
        setRow(devices, at, d.key,
               {QString::number(d.source), codeName(d.manufacturer, NmeaLookup::manufacturer(d.manufacturer)),
                d.model.isEmpty() ? tr("Unavailable") : d.model, model->deviceState(d),
                QString::number(d.messages), stamp(d.lastUtc)});
        if (d.key == deviceKey)
            selectedDevice = at;
        ++at;
    }
    const auto allMessages = model->messages();
    messages->setRowCount(allMessages.size());
    at = 0;
    int selectedMessage = -1;
    for (const auto &m : allMessages) {
        const auto searchable = QString::number(m.pgn) + " " + m.sourceDescription + " " + m.function + " " +
                                m.status + " " + m.variant;
        if (!searchable.contains(filter->text(), Qt::CaseInsensitive))
            continue;
        setRow(messages, at, m.key,
               {QString::number(m.pgn), m.sourceDescription,
                m.function + (m.variant.isEmpty() ? QString() : " / " + m.variant), m.status,
                QString::number(m.count), QString::number(m.payload.size()), stamp(m.lastUtc)});
        if (m.key == messageKey)
            selectedMessage = at;
        ++at;
    }
    messages->setRowCount(at);
    if (selectedDevice >= 0)
        devices->setCurrentCell(selectedDevice, 0);
    if (selectedMessage >= 0)
        messages->setCurrentCell(selectedMessage, 0);
    displayedRevision = model->revision();
    if (selectedMessage >= 0)
        showMessage();
    else if (selectedDevice >= 0)
        showDevice();
    else {
        details->clear();
        raw->clear();
    }
}
void Nmea2000DiagnosticsDialog::addField(const QString &name, const QString &value, const QString &meaning)
{
    auto *row = new QTreeWidgetItem(details, {name, value, meaning});
    for (int i = 0; i < 3; ++i)
        row->setToolTip(i, row->text(i));
}
void Nmea2000DiagnosticsDialog::showDevice()
{
    if (!model)
        return;
    QSignalBlocker other(messages);
    messages->setCurrentItem(nullptr);
    messages->clearSelection();
    const auto d = model->device(selectedKey(devices));
    details->clear();
    raw->clear();
    if (d.key.isEmpty())
        return;
    addField(tr("Current / last source address"), QString::number(d.source), model->deviceState(d));
    addField(tr("Source manufacturer"), codeName(d.manufacturer, NmeaLookup::manufacturer(d.manufacturer)));
    addField(tr("NAME (numeric hex)"), d.hasName ? d.nameHex : tr("Unavailable"),
             tr("Observed identity; private unique identifier"));
    addField(tr("NAME (wire byte order)"), d.wireName);
    addField(
        tr("Unique number / arbitrary address capable"),
        d.hasName
            ? QString("%1 / %2").arg(d.uniqueNumber).arg(d.arbitraryAddressCapable ? tr("Yes") : tr("No"))
            : tr("Unavailable"));
    addField(tr("Device class"), codeName(d.cls, NmeaLookup::deviceClass(d.cls)));
    addField(tr("Device function"), codeName(d.function, NmeaLookup::deviceFunction(d.cls, d.function)),
             tr("Resolved using device class"));
    addField(tr("Industry"), codeName(d.industry, NmeaLookup::industry(d.industry)));
    addField(tr("Device instance lower / upper"),
             QString("%1 / %2").arg(d.lowerInstance).arg(d.upperInstance));
    addField(tr("System instance"), QString::number(d.systemInstance));
    addField(tr("Observed model"), d.model.isEmpty() ? tr("Unavailable") : d.model,
             d.provisionalProduct ? tr("Provisional product before claim") : tr("Product Information"));
    addField(tr("Product code / NMEA version"), QString("%1 / %2").arg(d.productCode).arg(d.nmeaVersion));
    addField(tr("Software / model version"), d.software + " / " + d.modelVersion);
    addField(tr("Product serial"), d.serial.isEmpty() ? tr("Unavailable") : d.serial,
             tr("Private source data; do not publish"));
    addField(tr("Last message (UTC)"), stamp(d.lastUtc));
    addField(tr("First observation (UTC)"), stamp(d.firstUtc));
    if (d.heartbeatMs >= 0)
        addField(tr("Observed heartbeat interval"),
                 d.heartbeatInterval >= 0 ? QString::number(d.heartbeatInterval) + " ms" : tr("Unavailable"),
                 tr("Passive metadata; no scheduler or disconnect action"));
}
void Nmea2000DiagnosticsDialog::showMessage()
{
    if (!model)
        return;
    QSignalBlocker other(devices);
    devices->setCurrentItem(nullptr);
    devices->clearSelection();
    const auto m = model->message(selectedKey(messages));
    details->clear();
    raw->clear();
    if (m.key.isEmpty())
        return;
    addField(tr("PGN / function"), QString::number(m.pgn) + " / " + m.function, m.status);
    addField(tr("Source address at receipt"), QString::number(m.source), m.sourceDescription);
    const auto identity = m.receiptIdentity;
    const int sourceManufacturer = identity["source_manufacturer"].toInt(-1);
    addField(tr("Source device manufacturer"),
             codeName(sourceManufacturer, NmeaLookup::manufacturer(sourceManufacturer)),
             tr("Observed source NAME; independent of payload"));
    addField(tr("Source NAME at receipt"), identity["name_at_receipt"].toString(),
             identity["confidence"].toString());
    addField(tr("Source model at receipt"), identity["source_model"].toString());
    if (m.payloadManufacturer >= 0)
        addField(m.manufacturerRole,
                 codeName(m.payloadManufacturer, NmeaLookup::manufacturer(m.payloadManufacturer)),
                 tr("Proprietary data; does not change source identity"));
    addField(tr("Destination / priority"), QString("%1 / %2").arg(m.destination).arg(m.priority));
    addField(tr("Transport / observation session"), m.transport + " / " + QString::number(m.session));
    addField(tr("First / last (UTC)"), stamp(m.firstUtc) + " / " + stamp(m.lastUtc));
    addField(tr("Interpretation"), m.status,
             m.session == model->generation() ? tr("Current observation session")
                                              : tr("Historical connection snapshot"));
    for (const auto &f : m.fields)
        addField(f.name, f.value, f.interpretation);
    raw->setPlainText(QString::fromLatin1(m.payload.toHex(' ')));
}
