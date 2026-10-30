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

#include "enginesimulator.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QStringList>
#include <QVector>
#include "QObject"

EngineSimulator::EngineSimulator(QObject *parent)
    : QObject{parent}
{}

EngineSimulator::~EngineSimulator() {}

QStringList EngineSimulator::getEngineList(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open engine profiles file.";
        return QStringList();
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QStringList engineNames;
    if (doc.isObject()) {
        QJsonObject jsonObj = doc.object();
        QJsonArray enginesArray = jsonObj["engines"].toArray();
        for (const QJsonValue &engine : enginesArray) {
            engineNames << engine.toObject()["name"].toString();
        }
    }

    return engineNames;
}

QVector<QPair<int, double>> EngineSimulator::getEngineRpmGphData(const QString &filePath, const QString &engineName)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open engine profiles file.";
        return {};
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QVector<QPair<int, double>> rpmGphPairs;
    if (doc.isObject()) {
        QJsonObject jsonObj = doc.object();
        QJsonArray enginesArray = jsonObj["engines"].toArray();
        for (const QJsonValue &engine : enginesArray) {
            QJsonObject engineObj = engine.toObject();
            if (engineObj["name"].toString() == engineName) {
                QJsonArray rpmGphArray = engineObj["rpm_gph_pairs"].toArray();
                for (const QJsonValue &pair : rpmGphArray) {
                    QJsonObject pairObj = pair.toObject();
                    int rpm = pairObj["rpm"].toInt();
                    double gph = pairObj["gph"].toDouble();
                    rpmGphPairs.append(qMakePair(rpm, gph));
                }
                break;
            }
        }
    }

    return rpmGphPairs;
}

int EngineSimulator::getWOT(const std::vector<std::pair<int, double>> &rpmGphData)
{
    auto maxIt = std::max_element(rpmGphData.begin(), rpmGphData.end(), [](const std::pair<int, double> &a, const std::pair<int, double> &b) {
        return a.first < b.first; // Compare based on RPM (first element)
    });

    // Return the maximum RPM (WOT)
    if (maxIt != rpmGphData.end()) {
        return maxIt->first;
    } else {
        return -1; // In case the vector is empty
    }
}
