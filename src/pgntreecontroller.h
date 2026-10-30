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

#ifndef PGNTREECONTROLLER_H
#define PGNTREECONTROLLER_H
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QTreeView>
#include "pgntreemodel.h"
class MainController;
class PgnTableController;
class QTreeView;
class QTableView;
class QMdiArea;
class PgnTreeController final : public QObject {
    Q_OBJECT
public:
    explicit PgnTreeController(QTreeView*, QObject* parent = nullptr);
    void bindLayout(MainController*, PgnTableController*, QMdiArea*, QTableView*);
    PgnTreeModel* getTreeModel() const { return model; }
    QTreeView* getTreeView() const { return view; }
    bool navigationBlocked() const;
protected:
    bool eventFilter(QObject*, QEvent*) override;
private slots:
    void refreshStructure();
    void flushValues();
    void onWidgetChanged();
    void navigate(const QModelIndex&);
private:
    void scheduleStructure();
    void selectFromWidget(QWidget*);
    void markDirty(QWidget*);
    void showOutline(QWidget*);
    void observeWidgets();
    QPointer<QTreeView> view;
    PgnTreeModel* model;
    QPointer<MainController> controller;
    QPointer<QTableView> table;
    QPointer<QMdiArea> area;
    QTimer structureTimer, valueTimer;
    QSet<QWidget*> observed, dirty;
    QPointer<QWidget> outline, selectedWidget;
    QPointer<QObject> watchedTabBar;
    bool replacing = false, refreshing = false;
};
#endif
