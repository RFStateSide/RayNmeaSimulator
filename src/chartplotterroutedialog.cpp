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

#include "chartplotterroutedialog.h"
#include "nmea2000handler.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

ChartplotterRouteDialog::ChartplotterRouteDialog(
    Nmea2000Handler *h, const QVector<ReceivedRoutes::Route> &p,
    QWidget *parent)
    : QDialog(parent), handler(h), client(this), passive(p) {
    setWindowTitle(tr("Select Chartplotter Route"));
    setObjectName("chartplotterRouteDialog");
    resize(760, 440);
    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(
        tr("Discover / Refresh / Retrieve authorize bounded read-only "
           "NMEA2000 requests. They do not enable instrument Sending, "
           "movement or Autopilot."),
        this);
    notice->setWordWrap(true);
    notice->setToolTip(tr("Physical Axiom route retrieval is qualified through Network NMEA2000. CANable, Waveshare and SocketCAN physical route retrieval remains unqualified; Actisense route TX is not supported."));
    layout->addWidget(notice);
    auto *top = new QHBoxLayout;
    devices = new QComboBox(this);
    devices->setObjectName("routeServiceDevice");
    devices->setPlaceholderText(tr("Select a chartplotter device"));
    devices->setMinimumContentsLength(28);
    devices->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    top->addWidget(devices, 1);
    discover = new QPushButton(tr("Discover Devices"), this);
    discover->setObjectName("routeServiceDiscover");
    discover->setToolTip(tr("Authorize bounded device requests on the selected transport. Instrument Sending remains disabled."));
    top->addWidget(discover);
    refresh = new QPushButton(tr("Refresh Routes"), this);
    refresh->setObjectName("routeServiceRefresh");
    refresh->setToolTip(tr("Request the selected chartplotter's route catalogue. This does not select or activate a route."));
    top->addWidget(refresh);
    layout->addLayout(top);
    routes = new QListWidget(this);
    routes->setObjectName("routeServiceRoutes");
    layout->addWidget(routes, 1);
    status = new QLabel(
        tr("Complete received snapshots remain selectable. Discover a "
           "connected chartplotter to browse actively."),
        this);
    status->setObjectName("routeServiceStatus");
    status->setWordWrap(true);
    layout->addWidget(status);
    auto *buttons = new QHBoxLayout;
    retrieve = new QPushButton(tr("Retrieve Waypoints"), this);
    retrieve->setObjectName("routeServiceRetrieve");
    retrieve->setToolTip(tr("Request this route's waypoint snapshot. No route is uploaded or activated."));
    cancelOperation = new QPushButton(tr("Cancel Retrieval"), this);
    cancelOperation->setToolTip(tr("Cancel pending route requests. The previous simulation route is retained; F12 also cancels."));
    buttons->addWidget(retrieve);
    buttons->addWidget(cancelOperation);
    buttons->addStretch();
    select = new QPushButton(tr("Select Route"), this);
    select->setObjectName("routeServiceSelect");
    select->setToolTip(tr("Choose a complete snapshot for the next explicit Autopilot activation. Selection does not start movement or Sending."));
    buttons->addWidget(select);
    auto *close = new QPushButton(tr("Close"), this);
    buttons->addWidget(close);
    layout->addLayout(buttons);
    client.send = [h](const tN2kMsg &m) { return h->sendRouteService(m); };
    client.ready = [h] { return h->routeServiceReady(); };
    client.localAddress = [h] { return h->routeServiceSource(); };
    connect(h, &Nmea2000Handler::RouteServiceMessage, &client,
            &ChartplotterRouteClient::ingest);
    connect(h, &Nmea2000Handler::RouteServiceRevoked, this, [this] {
        if (client.busy() ||
            client.state() == ChartplotterRouteClient::Complete)
            client.cancel(
                tr("Route-service permission revoked; previous simulation "
                   "selection is unchanged"));
    });
    connect(&client, &ChartplotterRouteClient::changed, this,
            [this] { update(); });
    connect(&client, &ChartplotterRouteClient::failed, this,
            [this](const QString &) { handler->endRouteService(true); });
    connect(devices, &QComboBox::currentIndexChanged, this, [this] {
        client.resetSelection();
    });
    connect(discover, &QPushButton::clicked, this, [this] {
        if (authorize())
            client.discover();
    });
    connect(refresh, &QPushButton::clicked, this, [this] {
        if (authorize()) {
            auto ds = client.devices();
            for (auto d : ds)
                if (d.name == devices->currentData().toString()) {
                    client.browse(d);
                    return;
                }
        }
    });
    connect(retrieve, &QPushButton::clicked, this, [this] {
        int n = routes->currentRow() - passive.size();
        if (n >= 0 && n < client.entries().size() && authorize())
            client.retrieve(client.entries()[n]);
    });
    connect(cancelOperation, &QPushButton::clicked, this, [this] {
        client.cancel();
        handler->endRouteService(true);
    });
    connect(select, &QPushButton::clicked, this,
            &ChartplotterRouteDialog::choose);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(routes, &QListWidget::currentRowChanged, this,
            [this] { update(); });
    update();
}
ChartplotterRouteDialog::~ChartplotterRouteDialog() {
    bool abort = client.busy();
    client.cancel();
    handler->endRouteService(abort);
}
bool ChartplotterRouteDialog::authorize() {
    if (handler->beginRouteService())
        return true;
    status->setText(
        tr("Route-service connection unavailable. Connect a "
           "supported Network NMEA2000/CANable/Waveshare/SocketCAN device. Actisense "
           "and Signal K route requests are not qualified; existing "
           "transport restrictions remain in force."));
    return false;
}
void ChartplotterRouteDialog::update() {
    const auto oldName = devices->currentData().toString();
    {
        QSignalBlocker b(devices);
        devices->clear();
        for (const auto &d : client.devices()) {
            const auto model = d.model.isEmpty() ? tr("Unidentified NMEA2000 device") : d.model;
            devices->addItem(tr("%1 - source %2").arg(model).arg(d.source), d.name);
            const auto details = tr("Model: %1\nProduct code: %2\nSoftware: %3\nModel version: %4\nSource address: %5\nNAME: %6")
                .arg(model).arg(d.productCode < 0 ? tr("Unavailable") : QString::number(d.productCode))
                .arg(d.software.isEmpty() ? tr("Unavailable") : d.software)
                .arg(d.modelVersion.isEmpty() ? tr("Unavailable") : d.modelVersion)
                .arg(d.source).arg(d.name);
            devices->setItemData(devices->count()-1, "<qt>" + details.toHtmlEscaped().replace("\n", "<br>") + "</qt>", Qt::ToolTipRole);
        }
        // Discoveries/metadata arrival must never silently choose another device.
        devices->setCurrentIndex(oldName.isEmpty() ? -1 : devices->findData(oldName));
    }
    devices->setToolTip(devices->currentData(Qt::ToolTipRole).toString());
    QStringList labels;
    for (auto p : passive)
        labels << tr("%1 — received source %2 — %3 waypoints")
                      .arg(p.name, p.source)
                      .arg(p.waypoints.size());
    for (auto e : client.entries())
        labels << tr("%1 — database %2 / route %3 — %4")
                      .arg(e.name)
                      .arg(e.database)
                      .arg(e.id)
                      .arg(client.state() ==
                                       ChartplotterRouteClient::Complete &&
                                   client.candidate()
                                           .attributes.value("RouteID")
                                           .toInt() == e.id
                               ? tr("%1 waypoints, complete")
                                     .arg(client.candidate().waypoints.size())
                               : tr("not retrieved"));
    const int row = routes->currentRow();
    bool rebuild = routes->count() != labels.size();
    for (int i = 0; !rebuild && i < labels.size(); ++i)
        rebuild = routes->item(i)->text() != labels[i];
    if (rebuild) {
        QSignalBlocker b(routes);
        routes->clear();
        routes->addItems(labels);
        if (row >= 0 && row < labels.size())
            routes->setCurrentRow(row);
    }
    bool busy = client.busy();
    discover->setEnabled(!busy);
    refresh->setEnabled(!busy && devices->currentIndex() >= 0);
    devices->setEnabled(!busy);
    cancelOperation->setEnabled(busy);
    const int n = routes->currentRow();
    retrieve->setEnabled(!busy && n >= passive.size() &&
                         n < passive.size() + client.entries().size());
    bool done = n >= 0 && n < passive.size();
    if (!done && n >= passive.size() &&
        n < passive.size() + client.entries().size()) {
        auto e = client.entries()[n - passive.size()];
        auto c = client.candidate();
        done = client.state() == ChartplotterRouteClient::Complete &&
               c.complete && c.attributes.value("RouteID").toInt() == e.id &&
               c.attributes.value("DatabaseID").toInt() == e.database;
    }
    select->setEnabled(!busy && done);
    if (!client.status().isEmpty())
        status->setText(client.status());
}
void ChartplotterRouteDialog::choose() {
    int n = routes->currentRow();
    if (n >= 0 && n < passive.size()) {
        if (passiveStillValid && !passiveStillValid(passive[n])) {
            status->setText(
                tr("Received route changed or expired. Reopen Select "
                   "Route; previous selection is unchanged."));
            return;
        }
        selected = passive[n];
    } else if (select->isEnabled())
        selected = client.candidate();
    if (selected.complete)
        accept();
}
