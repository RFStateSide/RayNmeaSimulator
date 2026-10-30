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

#include "pgntablecontroller.h"
#include "qstyleditemdelegate.h"
#include <QHeaderView>

PgnTableController::PgnTableController(QTableView *tableView, QObject *parent)
    : QObject(parent)
    , tableView(tableView)
    , pgnTableModel(nullptr)
{
    if (!tableView) {
        qWarning() << "PgnTableController: tableView is null! Please provide a valid QTableView.";
    }

    pgnTableModel = new PgnTableModel(this);
    tableView->setModel(pgnTableModel);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tableView->horizontalHeader()->setStretchLastSection(false);
    const int em = tableView->fontMetrics().averageCharWidth();
    const int widths[] = {24, 22, 8, 24};
    for (int column = 0; column < 4; ++column) tableView->setColumnWidth(column, widths[column] * em);
    tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    tableView->setEditTriggers(QAbstractItemView::AllEditTriggers);
}

void PgnTableController::setPgnTableModel(PgnTableModel *model)
{
    if (!model) {
        qWarning() << "PgnTableController: model is null!";
        return; // Safely return if the model is null to avoid exceptions
    }

    // Check if the model is different from the current one
    if (pgnTableModel != model) {
        pgnTableModel = model;

        // Set the new model to the table view
        tableView->setModel(pgnTableModel);

        // Set item delegate to customize how data is displayed/edited
        tableView->setItemDelegate(new QStyledItemDelegate(tableView));

        // Ensure all edit triggers are active
        tableView->setEditTriggers(QAbstractItemView::AllEditTriggers);

        // Keep user-adjusted widths; long content is accessible by scrolling.

        // Force a repaint/update to reflect any model changes
        tableView->viewport()->update();
    }
}

PgnTableModel *PgnTableController::getPgnTableModel() const
{
    return pgnTableModel;
}

void PgnTableController::addPgn(int idX,
                                const QString &label,
                                const QString &pgn,
                                bool enabled,
                                const QList<QList<QString> > &paramDetails,
                                const QList<QPair<double, double> > &ranges,
                                double sendInterval)
{
    if (pgnTableModel) {
        pgnTableModel->addPgn(idX, label, pgn, enabled, paramDetails, ranges, sendInterval);
        tableView->setModel(pgnTableModel);
        //qDebug() << "PgnTableController: Added PGN with idX" << idX << "label" << label << "PGN" << pgn;
        //tableView->reset(); // Refresh the view
    }
}

void PgnTableController::removePgnInstance(int idX)
{
    pgnTableModel->removePgn(idX);
}

int PgnTableController::getPgnInstanceCount(QString pgn) const
{
    return pgnTableModel->getPgnInstanceCount(pgn);
}

void PgnTableController::refreshPgnParameters(int idX)
{
    // Telemetry refresh is not a selection request. Bindings for other widgets
    // must not reset the selected parameter table or destroy its active editor.
    if (pgnTableModel && pgnTableModel->isPgnCurrentPgn(idX)) {
        tableView->viewport()->update();
    }
}

void PgnTableController::refresh()
{
    tableView->viewport()->update();
}

void PgnTableController::reset()
{
    tableView->reset();
}

void PgnTableController::selectParameter(const QString& field)
{
    for (int row = 0; row < pgnTableModel->rowCount(); ++row) {
        if (pgnTableModel->data(pgnTableModel->index(row, 0)).toString() != field) continue;
        const auto index = pgnTableModel->index(row, 1);
        const auto triggers = tableView->editTriggers();
        tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tableView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        tableView->scrollTo(index);
        tableView->setEditTriggers(triggers);
        return;
    }
}

QList<PgnData> &PgnTableController::getPgnList()
{
    return pgnTableModel->getPgnList();
}

void PgnTableController::setSendInterval(int idX, double sendInterval)
{
    pgnTableModel->setSendInterval(idX, sendInterval);
}

void PgnTableController::setPgnEnabled(int idX, bool enabled)
{
    pgnTableModel->setPgnEnabled(idX, enabled);
}

bool PgnTableController::isPgnEnabled(int idX) const
{
    return pgnTableModel->isPgnEnabled(idX);
}

bool PgnTableController::isPgnInList(const QString &pgn) const
{
    return pgnTableModel->isPgnInList(pgn);
}

QList<PgnData> PgnTableController::getPgns(const QString &pgn) const
{
    return pgnTableModel->getPgns(pgn);
}

void PgnTableController::changeParams(int idX, const QStringList &paramList)
{
    pgnTableModel->changeParams(idX, paramList);
    if (pgnTableModel->isPgnCurrentPgn(idX)) {
        tableView->viewport()->update();
    }
}

void PgnTableController::SetMode(Mode mode)
{
    m_mode = mode;
    pgnTableModel->SetMode(mode);
}

void PgnTableController::showPngParameters(int idX)
{
    pgnTableModel->setCurrentPgn(idX);
    //qDebug() << "PgnTableController: Showing PGN parameters for idX" << idX;
}

