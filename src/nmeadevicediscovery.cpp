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

#include "nmeadevicediscovery.h"
#include "nmea2000diagnostics.h"
#include "NMEA2000.h"

NmeaDeviceDiscovery::NmeaDeviceDiscovery(Nmea2000Diagnostics *r, QObject *p) : QObject(p), registry(r)
{
    timer.setInterval(25);
    connect(&timer, &QTimer::timeout, this, [this] { advance(clock.elapsed()); });
}
void NmeaDeviceDiscovery::start()
{
    if (running)
        return;
    expected.clear();
    claims.clear();
    products.clear();
    rejected.clear();
    attempts.clear();
    addressClaims.clear();
    submissions = 0;
    lastRequest = -250;
    requesting = false;
    generation = registry ? registry->generation() : 0;
    if (!registry) {
        fail("Observation registry unavailable.");
        return;
    }
    clock.restart();
    deadline = 5000;
    running = true;
    text = "Opening authorized device-discovery session; instrument Sending remains disabled.";
    emit changed();
    if (!running)
        return;
    const auto error = open ? open() : QString("Discovery transport is not available.");
    if (!running)
        return;
    if (!error.isEmpty()) {
        finish(error);
        return;
    }
    // Opening notifications may finish publishing an already-connected RX
    // context. Capture the settled registry epoch, never a stale pre-open one.
    generation = registry->generation();
    for (const auto &d : registry->devices())
        if (d.session == generation && d.active && d.source <= 251)
            expected.insert(d.source);
    if (running)
        timer.start();
}
void NmeaDeviceDiscovery::cancel(const QString &reason)
{
    if (running)
        finish(reason);
}
void NmeaDeviceDiscovery::fail(const QString &reason)
{
    if (running)
        finish(reason);
    else {
        text = reason;
        emit changed();
    }
}
void NmeaDeviceDiscovery::finish(const QString &message)
{
    if (finishing || !running)
        return;
    finishing = true;
    timer.stop();
    running = false;
    requesting = false;
    if (release)
        release();
    text = message;
    finishing = false;
    emit changed();
}
void NmeaDeviceDiscovery::progress()
{
    int missing = 0, models = 0, manufacturers = 0;
    QStringList descriptions, missingAddresses, missingProducts;
    const int own = source ? source() : -1;
    for (int address : expected)
        if (address != own && !addressClaims.contains(address)) {
            ++missing;
            missingAddresses << QString::number(address);
        }
    for (const auto &key : claims) {
        const auto d = registry->device(key);
        if (d.source == own)
            continue;
        if (d.manufacturer >= 0)
            ++manufacturers;
        if (products.contains(key) && !d.model.isEmpty())
            ++models;
        if (!products.contains(key))
            missingProducts << QString::number(d.source);
        descriptions << registry->describe(d);
    }
    text = QString("%1 identities, %2 manufacturer codes, %3 Product Information observations, %4 models; %5 "
                   "observed "
                   "addresses without a fresh claim, %6 identities without a product reply, %7 rejected. %8")
               .arg(claims.size())
               .arg(manufacturers)
               .arg(products.size())
               .arg(models)
               .arg(missing)
               .arg(claims.size() - products.size())
               .arg(rejected.size())
               .arg(descriptions.join("; "));
    if (attempts.size() >= 32)
        text += " Product request limit: 32 identities per run.";
    if (claims.size() >= 64)
        text += " Reply accounting limit: 64 identities; other observations remain in Diagnostics.";
    if (!missingAddresses.isEmpty())
        text += " No fresh claim at SA: " + missingAddresses.join(", ") + ".";
    if (!missingProducts.isEmpty())
        text += " No product reply at SA: " + missingProducts.join(", ") + ".";
    text += QString(" Local management source %1 is excluded from remote counts.").arg(own);
    emit changed();
}
void NmeaDeviceDiscovery::advance(qint64 now)
{
    if (!running)
        return;
    if (!registry) {
        finish("Observation registry was closed.");
        return;
    }
    if (registry->generation() != generation) {
        finish("Receive connection changed; discovery cancelled, historical observations retained.");
        return;
    }
    if (!requesting) {
        if (now >= deadline) {
            finish("Device discovery transport did not become ready within 5 seconds.");
            return;
        }
        if (ready && ready()) {
            requesting = true;
            deadline = now + 12000;
            tN2kMsg request;
            SetN2kPGNISORequest(request, 255, 60928);
            ++submissions;
            lastRequest = now;
            if (!send || !send(request)) {
                finish("Address Claim request rejected by transport.");
                return;
            }
            if (!running)
                return;
            text = "Discovering: requested Address Claims; waiting for replies. Instrument Sending remains "
                   "disabled.";
            emit changed();
        } else if (now >= deadline)
            finish("Device discovery transport did not become ready within 5 seconds.");
        return;
    }
    if (!ready || !ready()) {
        finish("Discovery transport lost readiness; observations retained.");
        return;
    }
    if (now >= deadline) {
        progress();
        if (running)
            finish("Discovery finished. " + text +
                   " Management authorization released; no automatic retries.");
        return;
    }
    if (now - lastRequest < 250)
        return;
    for (const auto &key : claims) {
        const auto d = registry->device(key);
        if (!d.active || d.conflict || d.session != generation || d.source > 251 ||
            d.source == (source ? source() : -1) || products.contains(key) || rejected.contains(key))
            continue;
        if (!attempts.contains(key) && attempts.size() >= 32)
            continue;
        auto &attempt = attempts[key];
        if (attempt.count >= 2 || attempt.address == d.source)
            continue;
        if (submissions >= 65) {
            finish("Discovery request budget exhausted; partial observations retained.");
            return;
        }
        attempt.address = d.source;
        ++attempt.count;
        ++submissions;
        lastRequest = now;
        tN2kMsg request;
        SetN2kPGNISORequest(request, d.source, 126996);
        if (!send || !send(request)) {
            finish("Product Information request rejected by transport; partial observations retained.");
            return;
        }
        if (!running)
            return;
        progress();
        break;
    }
}
void NmeaDeviceDiscovery::ingest(const tN2kMsg &m)
{
    if (!running || !requesting || !registry || registry->generation() != generation || m.Source > 251 ||
        m.Source == (source ? source() : -1))
        return;
    const auto d = registry->sourceObservation(m.Source);
    if (m.PGN == 60928 && m.DataLen == 8 && d.hasName && !d.conflict) {
        if (addressClaims.contains(m.Source) && addressClaims.value(m.Source) != d.key) {
            finish("Conflicting NAME/address claims during discovery; partial observations retained. "
                   "Discover again explicitly after inspection.");
            return;
        }
        if (!claims.contains(d.key) && claims.size() >= 64)
            return;
        addressClaims[m.Source] = d.key;
        claims.insert(d.key);
        progress();
    } else if (m.PGN == 126996 && m.DataLen == 134 && d.hasName && !d.conflict && claims.contains(d.key)) {
        // Product Information is broadcast without a transaction ID. A fresh,
        // validated observation is useful even before our targeted request.
        products.insert(d.key);
        progress();
    } else if (m.PGN == 59392 && m.DataLen == 8 &&
               (m.Destination == (source ? source() : -1) || m.Destination == 255)) {
        const unsigned p = m.Data[5] | (unsigned(m.Data[6]) << 8) | (unsigned(m.Data[7]) << 16);
        if (p == 126996 && m.Data[0] >= 1 && m.Data[0] <= 3 && claims.contains(d.key) &&
            attempts.value(d.key).address == m.Source) {
            rejected.insert(d.key);
            progress();
        }
    }
}
