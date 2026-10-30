/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
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

#include "expansioninstruments.h"
#include "windlassstatus.h"
#include "nmeareceiver.h"
#include "maincontroller.h"
#include "nmea2000handler.h"
#include "nmea2000diagnostics.h"
#include <QScopedValueRollback>

NmeaReceiver::NmeaReceiver(
    MainController *mainController, PgnTableController *pgnTableController, Nmea2000Handler *nmea2000Handler, Nmea0183Handler *nmea0183Handler, QObject *parent)
    : QObject{parent}
    , m_mainController{mainController}
    , m_pgnTableController{pgnTableController}
    , m_nmea2000Handler{nmea2000Handler}
    , m_nmea0183Handler{nmea0183Handler}
{
    connect(m_nmea2000Handler,&Nmea2000Handler::CanSourceIdentityObserved,this,[this](const QString &source,const QString &name){
        bool ok=false;const int address=source.toInt(&ok);if(ok && address>=0 && address<=253)m_mainController->receivedRoutes.observeCanIdentity(source,name);
    });
    connect(m_nmea2000Handler, &Nmea2000Handler::RcvdPgn, this, &NmeaReceiver::processNmeaPgnSnt);
    connect(m_nmea0183Handler, &Nmea0183Handler::RcvdSnt, this, &NmeaReceiver::processNmeaPgnSnt);
    connect(this, &NmeaReceiver::addRcvdPgnSnt, m_mainController, &MainController::addRcvdPgn);
    connect(this, &NmeaReceiver::updateRcvdPgnSnt, m_mainController, &MainController::updateRcvdPgn);
    // Observe after the established direct model slots; retain their public
    // connection/disconnection contract (including decoder-only test fixtures).
    connect(this, &NmeaReceiver::addRcvdPgnSnt, this,
            [this](const QString &pgn, const QStringList &, const QString &source) {
        if (!m_incomingOwner.key.isEmpty())
            for (const auto &row : m_pgnTableController->getPgns(pgn))
                if (row.source == source && !m_liveOwners.contains(row.idX)) m_liveOwners[row.idX] = m_incomingOwner;
    });
    connect(this, &NmeaReceiver::updateRcvdPgnSnt, this,
            [this](int id, const QStringList &) {
        if (!m_incomingOwner.key.isEmpty()) m_liveOwners[id] = m_incomingOwner;
    });
    connect(m_mainController, &MainController::layoutReplaced, this, [this] { m_liveOwners.clear(); });
    connect(m_mainController, &MainController::layoutObjectsChanged, this, [this] {
        auto *model = m_pgnTableController->getPgnTableModel();
        for (auto it = m_liveOwners.begin(); it != m_liveOwners.end();)
            if (!model->findPgn(it.key())) it = m_liveOwners.erase(it); else ++it;
    });
}

QList<PgnData> NmeaReceiver::receiveCandidates(const QString &pgn, const QString &source)
{
    auto candidates = m_pgnTableController->getPgns(pgn);
    if (m_incomingOwner.key.isEmpty()) return candidates; // Sentences/legacy direct-call fixtures.
    auto *model = m_pgnTableController->getPgnTableModel();
    for (auto it = m_liveOwners.begin(); it != m_liveOwners.end();)
        if (!model->findPgn(it.key())) it = m_liveOwners.erase(it); else ++it;
    for (auto &row : candidates) {
        if (!m_liveOwners.contains(row.idX)) continue;
        const auto owner = m_liveOwners.value(row.idX);
        const bool sameOwner = owner.generation == m_incomingOwner.generation &&
            (owner.key == m_incomingOwner.key || owner.key == m_incomingOwner.promotedFrom);
        if (!sameOwner) {
            // Only this candidate copy is hidden from matching. Retain the real
            // last address/value/time and existing discovery-capacity accounting.
            row.source.clear();
        } else if (row.source != source) {
            model->setSource(row.idX, source);
            row.source = source;
        }
    }
    return candidates;
}

void NmeaReceiver::processNmeaPgnSnt(QString pgn, const QStringList &paramList, const QString &source)
{
    ReceiveOwner incoming;
    if (pgn.size() != 3) {
        bool ok = false;
        const int address = source.toInt(&ok);
        if (ok && address >= 0 && address <= 251) {
            const auto device = m_nmea2000Handler->diagnostics()->sourceObservation(address);
            if (device.conflict) return; // Inspect ambiguous traffic; never guess a live owner.
            incoming = {device.key, device.promotedFrom, device.session};
        }
    }
    QScopedValueRollback<ReceiveOwner> receiveScope(m_incomingOwner, incoming);
    if(pgn=="130066" || pgn=="130067") {
        bool ok=false;const int address=source.toInt(&ok);if(!ok || address<0 || address>253 || paramList.size()%2)return;
        QMap<QString,QString> fields;for(int i=0;i<paramList.size();i+=2)fields[paramList[i]]=paramList[i+1];
        m_mainController->receivedRoutes.receive(pgn,fields,source);return;
    }
    if(const auto *d=Expansion::definition(pgn)) {
        if(!Expansion::validParameters(pgn,paramList))return;
        if(pgn.size()!=3){bool ok;int address=source.toInt(&ok);if(!ok||address<0||address>253)return;}
        QMap<QString,QString> incoming;for(int i=0;i<paramList.size();i+=2)incoming[paramList[i]]=paramList[i+1];
        m_mainController->receivedRoutes.receive(pgn,incoming,source);
        bool matched=false;const auto existing=receiveCandidates(pgn,source);
        for(const auto &data:existing) {
            if(data.source!=source)continue;bool identity=true;
            for(const auto &key:d->identity)if(!isParamValueMatch(key,paramList,data.paramDetails))identity=false;
            if(identity){matched=true;emit updateRcvdPgnSnt(data.idX,paramList);}
        }
        if(!matched && existing.size()<64)emit addRcvdPgnSnt(pgn,paramList,source);
        return;
    }
    if(WindlassStatus::supported(pgn.toInt())) {
        bool sourceOk=false;const int address=source.toInt(&sourceOk);
        if(!sourceOk || address<0 || address>253 || !WindlassStatus::validParameters(pgn.toInt(),paramList))return;
    }
    bool match = false;
    int idX = -1;

    // Check if the PGN is really a SNT
    QString snt = QString();
    if (pgn.length() == 3) {
        snt = pgn;
    }

    QList<PgnData> pgnList = receiveCandidates(pgn,source);
    if (pgnList.isEmpty()) {
        // Send signal to MainController to add the PGN
        emit addRcvdPgnSnt(pgn, paramList, source);
    } else {
        // Update the PGN that matches the parameters
        for (PgnData &pgnData : pgnList) {
            idX = -1;
            // Check if the parameters match

            // PGNs that should only have one instance per source set match to true
            if (pgnData.source == source
                && (pgn == "126992" || pgn == "127250" || pgn == "127251" || pgn == "127252" || pgn == "127257" || pgn == "127258" || pgn == "128259"
                    || pgn == "128267" || pgn == "129025" || pgn == "129026" || pgn == "129029" || pgn == "129033" || pgn == "129283" || pgn == "129284"
                    || pgn == "130576" || pgn == "129285" || pgn == "129291" || pgn == "129539" || pgn == "130310" || snt == "GGA" || snt == "RMC"
                    || snt == "HDG" || snt == "HDT" || snt == "GLL" || snt == "VTG" || snt == "ZDA" || snt == "BWR" || snt == "RSA" || snt == "MTW"
                    || snt == "DPT" || snt == "VHW" || snt == "RPM" || snt == "ROT" || snt == "MWD" || snt == "MWV" || snt == "VWR")) {
                match = true;
                idX = pgnData.idX;
            } else if (pgnData.source == source && (pgn == "127488" || pgn == "127489" || pgn == "127493")) {
                // Check if the parameter matches the instance
                if (isParamValueMatch("EngineInstance", paramList, pgnData.paramDetails)) {
                    match = true;
                    idX = pgnData.idX;
                }
            } else if (pgnData.source == source && WindlassStatus::supported(pgn.toInt())) {
                QString identity;for(int i=0;i+1<paramList.size();i+=2)if(paramList[i]=="WindlassIdentifier")identity=paramList[i+1];
                for(const auto &row:pgnData.paramDetails)if(row[0]=="WindlassIdentifier" && row[1].toUInt()==identity.toUInt()){match=true;idX=pgnData.idX;}
            } else if (pgnData.source == source && (pgn == "129810" || pgn == "129039")) {
                // Layout restoration is named-field based; identity must not depend on row order.
                QString identity;
                for (int i=0;i+1<paramList.size();i+=2) if (paramList[i]=="UserID") identity=paramList[i+1];
                for (const auto &row:pgnData.paramDetails)
                    if (row[0]=="UserID" && row[1]==identity) { match=true; idX=pgnData.idX; }
            } else if (pgnData.source == source && pgn == "129809") {
                if (isParamValueMatch("UserID", paramList, pgnData.paramDetails)) { match = true; idX = pgnData.idX; }
            } else if (pgnData.source == source && pgn == "130306") {
                if (isParamValueMatch("WindReference", paramList, pgnData.paramDetails)) { match = true; idX = pgnData.idX; }
            } else if (pgnData.source == source && pgn == "130313") {
                if (isParamValueMatch("HumidityInstance", paramList, pgnData.paramDetails)
                    && isParamValueMatch("HumiditySource", paramList, pgnData.paramDetails)) { match = true; idX = pgnData.idX; }
            } else if (pgnData.source == source && pgn == "130312") {
                // Check if the parameter matches the instance and type
                if (isParamValueMatch("TemperatureInstance", paramList, pgnData.paramDetails)
                    && isParamValueMatch("TemperatureSource", paramList, pgnData.paramDetails)) {
                    match = true;
                    idX = pgnData.idX;
                }
            } else if (pgnData.source == source && pgn == "130316") {
                // Check if the parameter matches the instance and type
                if (isParamValueMatch("TempInstance", paramList, pgnData.paramDetails) && isParamValueMatch("TempSource", paramList, pgnData.paramDetails)) {
                    match = true;
                    idX = pgnData.idX;
                }
            } else if (pgnData.source == source && pgn == "127505") {
                // Check if the parameter matches the instance and type
                if (isParamValueMatch("Instance", paramList, pgnData.paramDetails) && isParamValueMatch("FluidType", paramList, pgnData.paramDetails)) {
                    match = true;
                    idX = pgnData.idX;
                }
            } else if (pgnData.source == source && pgn == "127506") {
                // Check if the parameter matches the instance and type
                if (isParamValueMatch("DCInstance", paramList, pgnData.paramDetails) && isParamValueMatch("DCType", paramList, pgnData.paramDetails)) {
                    match = true;
                    idX = pgnData.idX;
                }
            } else if (pgnData.source == source && pgn == "127507") {
                // Check if the parameter matches the instance and type
                if (isParamValueMatch("Instance", paramList, pgnData.paramDetails) && isParamValueMatch("BatteryInstance", paramList, pgnData.paramDetails)) {
                    match = true;
                    idX = pgnData.idX;
                }
            } else if (pgnData.source == source && pgn == "127508") {
                // Check if the parameter matches the instance and type
                if (isParamValueMatch("BatteryInstance", paramList, pgnData.paramDetails)) {
                    match = true;
                    idX = pgnData.idX;
                }
            } else if (pgnData.source == source && pgn == "127245") {
                // Check if the parameter matches the instance
                if (isParamValueMatch("Instance", paramList, pgnData.paramDetails)) {
                    match = true;
                    idX = pgnData.idX;
                }
            }
            // Every configured matching instrument receives the update.
            if (idX != -1) emit updateRcvdPgnSnt(idX, paramList);
        }

        if (!match) {
            if(WindlassStatus::supported(pgn.toInt()) && pgnList.size()>=64) {
                if(!property("windlassCapacityWarning").toBool()){qWarning()<<"Windlass Viewer discovery limit (64 streams per PGN) reached; existing streams continue updating.";setProperty("windlassCapacityWarning",true);}
                return;
            }
            // Bound automatic discovery for this AIS position PGN; existing streams
            // still update at capacity. No persistent target registry is introduced.
            if(pgn=="129039" && pgnList.size()>=64) {
                if(!property("aisPositionCapacityWarning").toBool()){qWarning()<<"PGN 129039: automatic Viewer discovery limit (64 streams) reached; existing streams continue updating.";setProperty("aisPositionCapacityWarning",true);}
                return;
            }
            if(pgn=="129039")setProperty("aisPositionCapacityWarning",false);
            // Send signal to MainController to add the PGN / SNT
            emit addRcvdPgnSnt(pgn, paramList, source);
        }
    }
}

bool NmeaReceiver::isParamValueMatch(QString paramName, const QStringList &params1, const QList<QList<QString> > &params2)
{
    // Incoming pairs and stored Layout rows can have independent field orders.
    for (int i = 0; i + 1 < params1.size(); i += 2) {
        if (params1.at(i) != paramName) continue;
        for (const auto &row : params2)
            if (row.size() >= 2 && row.at(0) == paramName)
                return row.at(1) == params1.at(i + 1);
        return false;
    }

    return false;
}
