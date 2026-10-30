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

#ifndef PGNTABLECONTROLLER_H
#define PGNTABLECONTROLLER_H

#include <QObject>
#include <QTableView>
#include "pgntablemodel.h"

class PgnTableController : public QObject
{
    Q_OBJECT

public:
    explicit PgnTableController(QTableView *tableView, QObject *parent = nullptr);

    // Set the model for the table view
    void setPgnTableModel(PgnTableModel *model);
    PgnTableModel *getPgnTableModel() const;

    QStringList getTableDataForPgn(int idX);

    void addPgn(int idX,
                const QString &label,
                const QString &pgn,
                bool enabled,
                const QList<QList<QString>> &paramDetails,
                const QList<QPair<double, double>> &ranges,
                double sendInterval = 1000);
    void removePgnInstance(int idX);
    int getPgnInstanceCount(QString pgn) const;

    // Update the table with the PGN parameters for the selected PGN
    void refreshPgnParameters(int idX);
    void refresh();
    void reset();
    void selectParameter(const QString& field);

    // Get the list of PGNs
    QList<PgnData> &getPgnList();

    void setSendInterval(int idX, double sendInterval);
    void setPgnEnabled(int idX, bool enabled);
    bool isPgnEnabled(int idX) const;

    // Methods to support recieve mode
    bool isPgnInList(const QString &pgn) const;
    QList<PgnData> getPgns(const QString &pgn) const;

    void changeParams(int idX, const QStringList &paramList);
    void SetMode(Mode mode);

public slots:
    void showPngParameters(int idX);

private:
    QTableView *tableView;        // Reference to the QTableView in the QDockWidget
    PgnTableModel *pgnTableModel; // The table model holding PGN data
    Mode m_mode = Sender;
};

#endif // PGNTABLECONTROLLER_H
