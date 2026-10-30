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

#ifndef PGNTREEMODEL_H
#define PGNTREEMODEL_H
#include <QAbstractItemModel>
#include <QPointer>
#include <QSet>
#include <QWidget>
#include <memory>
#include <vector>
class MainController;
class PgnTableModel;
class QMdiArea;

// A presentation index over live Layout objects. Nodes own no simulator state.
class PgnTreeModel final : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Kind { Invisible, Layout, Tab, Widget, Property };
    struct Node {
        Kind kind = Invisible;
        Node* parent = nullptr;
        QPointer<QWidget> widget, tab;
        QString field;
        std::vector<std::unique_ptr<Node>> children;
    };
    explicit PgnTreeModel(QObject* parent = nullptr);
    void bind(MainController*, PgnTableModel*, QMdiArea*);
    void rebuild();
    void clear();
    void refreshValues(const QSet<QWidget*>& dirty);
    Node* node(const QModelIndex&) const;
    QModelIndex indexFor(QWidget* widget, const QString& field = {}) const;
    QModelIndex indexForTab(QWidget*) const;
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex&) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& = {}) const override { return 3; }
    QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;
    QVariant headerData(int, Qt::Orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex&) const override;
private:
    QModelIndex indexForNode(const Node*, int column = 0) const;
    Node* append(Node*, Kind, QWidget* widget = nullptr, QWidget* tab = nullptr, const QString& field = {});
    Node root;
    QPointer<MainController> controller;
    QPointer<PgnTableModel> parameters;
    QPointer<QMdiArea> area;
};
#endif
