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

#ifndef PGNTABLEMODEL_H
#define PGNTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QHash>
#include <QDateTime>
#include <limits>
#include <QStringList>
#include "datastructs.h"

struct PgnData
{
    int idX = -1;
    QString label;
    QString pgn;
    QString source;
    bool enabled = true;
    double sendInterval = 1000;
    QList<QList<QString>> paramDetails;       // Each parameter's details: Name, Value, Units, Type
    QList<QPair<double, double>> paramRanges; // Min and Max for each parameter
};

class PgnTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PgnTableModel(QObject *parent = nullptr);
    ~PgnTableModel();

    // Methods to add and manage PGNs
    void addPgn(int idX,
                const QString &label,
                const QString &pgn,
                bool enabled,
                const QList<QList<QString>> &paramDetails,
                const QList<QPair<double, double>> &ranges,
                double sendInterval = 1000);
    QDateTime parameterUpdatedAt(int idX, const QString &field) const;
    void recordParameterSample(int idX, const QString &field);
    // Runtime observation provenance; never serialized or used to advance a simulation.
    QDateTime parameterObservedAt(int idX, const QString &field) const;
    QDateTime parameterPublicationTime(int idX, const QString &field) const;
    void setParameterObservationTime(int idX, const QString &field, const QDateTime &time);
    void setParameterOwner(int idX, QObject *owner);
    void beginParameterInitialization(int first=std::numeric_limits<int>::min(), int last=std::numeric_limits<int>::max()) { parameterInitializationRanges.append({first,last}); }
    void endParameterInitialization() { Q_ASSERT(!parameterInitializationRanges.isEmpty()); parameterInitializationRanges.removeLast(); }
    void setParameterDescription(int idX, const QString &field, const QString &text);
    void setCurrentPgn(int idX); // Update to set PGN by idX (not label)
    void changeParam(int idX, const QString &paramName, const QString &paramNewValue);
    void changeParams(int idX, const QStringList &paramList);
    void removePgn(int id);
    void setSource(int idX, QString source);
    QString getSource(int idX);

    QList<QList<QString>> getPgnParams(int idX) const;

    // Get the PGN data to send to the NMEA2000 interface
    QList<PgnData> &getPgnList();
    int getPgnDataCount() const;
    int getPgnInstanceCount(QString pgn) const;
    PgnData getPgnDataToSend(int idX) const;
    const PgnData* findPgn(int idX) const;

    void setSendInterval(int idX, double sendInterval);
    void setPgnEnabled(int idX, bool enabled);
    bool isPgnEnabled(int idX) const;

    // QAbstractTableModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    double getParamMin(int row) const; // Get the min value for a parameter
    double getParamMax(int row) const; // Get the max value for a parameter

    void updatePgnParameter(int idX, const QString &paramName, const QVariant &value, bool emitSignal = true);
    bool isPgnCurrentPgn(int idX) const;
    int getMaxIdX() const;
    void clearModel();
    void refreashModel();
    QString getLabel(int idX) const;

    // Methods to support recieve mode
    bool isPgnInList(const QString &pgn) const;
    QList<PgnData> getPgns(const QString &pgn) const;
    void setIdxHasFocus(int idx);
    int IdxHasFocus() const;

    void SetMode(Mode mode);

signals:
    void enabledChanged();
    // Inspection-only notification; never drives instrument bindings.
    void valuesChanged(int idX);
    void parameterChanged(int idX, const QString &paramName, const QVariant &value);

private:
    bool stampParameter(int idX, const QString &field);
    QList<QPair<int,int>> parameterInitializationRanges;
    QHash<int, QHash<QString,QDateTime>> parameterUpdates;
    QHash<int, QHash<QString,QDateTime>> parameterObservations;
    QHash<int,QDateTime> parameterConfigurations;
    // Identity comparison only; never dereference. Destruction marks the entry dead.
    QHash<int, QObject*> parameterOwners;
    QHash<int, QHash<QString, QString>> parameterDescriptions;

    PgnData currentPgn;
    QList<PgnData> pgnList; // List of all PGNs and their instances
    int m_IdxHasFocus = -1;
    Mode m_mode = Sender;
};

#endif // PGNTABLEMODEL_H
