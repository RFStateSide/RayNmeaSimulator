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

#include "pgntreecontroller.h"
#include "generatorweather.h"
#include "maincontroller.h"
#include "pgntablecontroller.h"
#include "pgnwidgetbase.h"
#include <QApplication>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMetaProperty>
#include <QPainter>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <QTableView>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeView>

namespace {
class SelectionOutline final : public QWidget {
public:
    explicit SelectionOutline(QWidget* parent) : QWidget(parent) {
        setObjectName("objectTreeSelectionOutline");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(QPen(palette().color(QPalette::Highlight), 3));
        painter.setBrush(Qt::NoBrush); painter.drawRect(rect().adjusted(2, 2, -2, -2));
    }
};
PgnWidgetBase* instrument(QWidget* widget) {
    for (auto* current = widget; current; current = current->parentWidget())
        if (auto* base = qobject_cast<PgnWidgetBase*>(current)) return base;
    return nullptr;
}
}

PgnTreeController::PgnTreeController(QTreeView* view, QObject* parent)
    : QObject(parent), view(view), model(new PgnTreeModel(this))
{
    view->setModel(model);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setDragEnabled(false); view->setAcceptDrops(false); view->setDropIndicatorShown(false);
    view->setDragDropMode(QAbstractItemView::NoDragDrop);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setUniformRowHeights(true);
    // Mouse selection must not move focus out of an unfinished editor.
    view->setFocusPolicy(Qt::TabFocus);
    view->header()->setSectionResizeMode(QHeaderView::Interactive);
    view->header()->setStretchLastSection(false);
    const int em = view->fontMetrics().averageCharWidth();
    const int widths[] = {24, 20, 18};
    for (int column = 0; column < 3; ++column) view->setColumnWidth(column, widths[column] * em);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->viewport()->installEventFilter(this); view->installEventFilter(this);
    structureTimer.setSingleShot(true); structureTimer.setInterval(0);
    valueTimer.setSingleShot(true); valueTimer.setInterval(50);
    connect(&structureTimer, &QTimer::timeout, this, &PgnTreeController::refreshStructure);
    connect(&valueTimer, &QTimer::timeout, this, &PgnTreeController::flushValues);
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& index) { navigate(index); });
}
void PgnTreeController::bindLayout(MainController* c, PgnTableController* t, QMdiArea* a, QTableView* tableView)
{
    Q_ASSERT(!controller); controller = c; area = a; table = tableView;
    model->bind(c, t->getPgnTableModel(), a);
    connect(c, &MainController::layoutObjectsChanged, this, &PgnTreeController::scheduleStructure);
    connect(c, &MainController::layoutAboutToReplace, this, [this] {
        replacing = true; structureTimer.stop(); valueTimer.stop(); dirty.clear(); selectedWidget.clear();
        if (outline) delete outline.data();
        QScopedValueRollback<bool> guard(refreshing, true); model->clear();
    });
    connect(c, &MainController::layoutReplaced, this, [this] { replacing = false; scheduleStructure(); });
    connect(c, &MainController::layoutIdentityChanged, this, [this] { flushValues(); });
    connect(c, &MainController::layoutWidgetSelected, this, &PgnTreeController::selectFromWidget);
    connect(t->getPgnTableModel(), &PgnTableModel::valuesChanged, this, [this](int id) {
        for (auto* widget : observed) if (widget->property("Id").toInt() == id) markDirty(widget);
    });
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* current) {
        if (!controller || !view || refreshing || replacing || !current || view == current || view->isAncestorOf(current)) return;
        if (auto* base = instrument(current)) controller->selectLayoutWidget(base);
    });
    qApp->installEventFilter(this);
    scheduleStructure();
}
void PgnTreeController::scheduleStructure() { if (view && controller && !replacing && !structureTimer.isActive()) structureTimer.start(); }
void PgnTreeController::markDirty(QWidget* widget) { if (widget) dirty.insert(widget); if (view && view->isVisible() && !valueTimer.isActive()) valueTimer.start(); }
void PgnTreeController::onWidgetChanged() { markDirty(qobject_cast<QWidget*>(sender())); }
void PgnTreeController::flushValues()
{
    if (!view || !controller || replacing) return;
    if (!view->isVisible()) return; // Retain dirty objects until the dock is shown.
    const auto changed = dirty; dirty.clear(); model->refreshValues(changed);
}
void PgnTreeController::observeWidgets()
{
    if (auto* bar = area->findChild<QTabBar*>(QString(), Qt::FindDirectChildrenOnly); bar && watchedTabBar != bar) {
        watchedTabBar = bar;
        connect(bar, &QTabBar::tabMoved, this, [this] { scheduleStructure(); });
    }
    for (auto* tab : area->subWindowList()) for (auto* widget : controller->layoutWidgets(tab->windowTitle())) {
        if (observed.contains(widget)) continue;
        observed.insert(widget);
        connect(widget, &QObject::destroyed, this, [this, widget] {
            observed.remove(widget); dirty.remove(widget); scheduleStructure();
        });
        widget->installEventFilter(this);
        if (auto *weather = qobject_cast<GeneratorWeather*>(widget))
            connect(weather, &GeneratorWeather::ObservationMetadataChanged, this, [this, widget] { markDirty(widget); });
        QSet<int> notifySignals;
        const auto* meta = widget->metaObject();
        for (int i = PgnWidgetBase::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i) {
            const auto property = meta->property(i);
            if (!property.hasNotifySignal() || notifySignals.contains(property.notifySignalIndex())) continue;
            notifySignals.insert(property.notifySignalIndex());
            QObject::connect(widget, property.notifySignal(), this, this->metaObject()->method(this->metaObject()->indexOfSlot("onWidgetChanged()")));
        }
        if (auto* engine = qobject_cast<GeneratorEngine*>(widget))
            if (auto* profile = engine->findChild<QComboBox*>("comboBoxVesselEngine"))
                connect(profile, &QComboBox::currentTextChanged, this, [this, widget] { markDirty(widget); });
    }
}
void PgnTreeController::refreshStructure()
{
    if (!view || !controller || replacing) return;
    QScopedValueRollback<bool> guard(refreshing, true);
    struct Saved { QPointer<QWidget> widget, tab; QString field; bool expanded; };
    QList<Saved> expansion;
    QPointer<QWidget> selected, selectedTab; QString field;
    bool rootExpanded = !model->index(0, 0).isValid() || view->isExpanded(model->index(0, 0));
    if (auto* n = model->node(view->currentIndex())) { selected = n->widget; selectedTab = n->tab; field = n->field; }
    auto root = model->index(0, 0);
    for (int t = 0; t < model->rowCount(root); ++t) {
        auto tab = model->index(t, 0, root); auto* tn = model->node(tab);
        expansion.append({nullptr, tn->tab, {}, view->isExpanded(tab)});
        for (int w = 0; w < model->rowCount(tab); ++w) {
            auto object = model->index(w, 0, tab); auto* n = model->node(object);
            expansion.append({n->widget, n->tab, {}, view->isExpanded(object)});
        }
    }
    QSignalBlocker selection(view->selectionModel());
    model->rebuild(); observeWidgets(); root = model->index(0, 0);
    // On the first completed hierarchy, expose Layout/tabs, not parameter lists.
    view->setExpanded(root, expansion.isEmpty() || rootExpanded);
    for (auto* tab : area->subWindowList()) {
        bool expanded = true;
        for (const auto& saved : expansion) if (!saved.widget && saved.tab == tab) expanded = saved.expanded;
        view->setExpanded(model->indexForTab(tab), expanded);
    }
    for (const auto& saved : expansion) if (saved.widget) view->setExpanded(model->indexFor(saved.widget), saved.expanded);
    const auto restore = selected ? model->indexFor(selected, field) : model->indexForTab(selectedTab);
    if (restore.isValid()) view->setCurrentIndex(restore);
    if (selectedWidget && !model->indexFor(selectedWidget).isValid()) { selectedWidget.clear(); if (outline) delete outline.data(); }
}
bool PgnTreeController::navigationBlocked() const
{
    auto* focus = qApp->focusWidget();
    if (!view || !controller || !focus || view == focus || view->isAncestorOf(focus)) return false;
    if (table && table->viewport()->isAncestorOf(focus)) return true; // Active delegate editor.
    if (instrument(focus)) {
        auto* edit = qobject_cast<QLineEdit*>(focus);
        if (edit && !edit->isReadOnly() && edit->isModified()) return true;
        for (auto* parent = focus->parentWidget(); parent && parent != area; parent = parent->parentWidget())
            if (auto* spin = qobject_cast<QAbstractSpinBox*>(parent))
                if (auto* text = spin->findChild<QLineEdit*>()) return text->isModified();
    }
    return false;
}
bool PgnTreeController::eventFilter(QObject* object, QEvent* event)
{
    if (!view || !controller) return QObject::eventFilter(object, event);
    if (object == view->viewport() && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) && navigationBlocked()) {
        if (controller) emit controller->ShowStatusBarMessage(tr("Finish or cancel the current edit before navigating the Object Tree."), 5000);
        return true;
    }
    // Structural notifications are processed even while the dock is hidden.
    // Showing it only refreshes values; rebuilding here invalidates live indexes
    // during a delayed native Show event without any Layout change.
    if (object == view && event->type() == QEvent::Show) flushValues();
    if (outline && object == outline->parentWidget() && event->type() == QEvent::Resize) outline->setGeometry(outline->parentWidget()->rect());
    if (event->type() == QEvent::MouseButtonPress) {
        if (auto* base = instrument(qobject_cast<QWidget*>(object)); base && observed.contains(base)) {
            QPointer<QWidget> clicked(base);
            QTimer::singleShot(0, this, [this, clicked] { if (controller && clicked && !replacing) controller->selectLayoutWidget(clicked); });
        }
    }
    if (auto* widget = qobject_cast<QWidget*>(object); observed.contains(widget)) {
        if (event->type() == QEvent::WindowTitleChange || event->type() == QEvent::DynamicPropertyChange) markDirty(widget);
    }
    return QObject::eventFilter(object, event);
}
void PgnTreeController::showOutline(QWidget* widget)
{
    if (!widget) return;
    auto* frame = widget->parentWidget(); if (!frame) return;
    if (!outline || outline->parentWidget() != frame) { if (outline) delete outline.data(); outline = new SelectionOutline(frame); frame->installEventFilter(this); }
    outline->setGeometry(frame->rect()); outline->show(); outline->raise(); selectedWidget = widget;
}
void PgnTreeController::selectFromWidget(QWidget* widget)
{
    if (!view || !controller || refreshing || replacing) return;
    const auto current = model->node(view->currentIndex());
    if (current && current->widget == widget) { showOutline(widget); return; }
    const auto index = model->indexFor(widget); if (!index.isValid()) return;
    QScopedValueRollback<bool> guard(refreshing, true); QSignalBlocker selection(view->selectionModel());
    view->setCurrentIndex(index); view->scrollTo(index); showOutline(widget);
}
void PgnTreeController::navigate(const QModelIndex& index)
{
    if (!view || !controller || refreshing || replacing || navigationBlocked()) return;
    const auto* node = model->node(index); if (!node) return;
    if (node->kind == PgnTreeModel::Layout) { view->setFocus(Qt::OtherFocusReason); return; }
    if (!node->tab) return;
    QPointer<QWidget> widget = node->widget;
    auto* tab = qobject_cast<QMdiSubWindow*>(node->tab.data()); if (!tab || !area->subWindowList().contains(tab)) return;
    QScopedValueRollback<bool> guard(refreshing, true);
    // Activating an MDI tab would otherwise restore its last editor's focus.
    QSignalBlocker tabSignals(area);
    const auto oldPolicy = tab->focusPolicy(); tab->setFocusPolicy(Qt::NoFocus);
    area->setActiveSubWindow(tab); tab->setFocusPolicy(oldPolicy);
    if (!widget) { if (outline) outline->hide(); view->setFocus(Qt::OtherFocusReason); return; }
    if (!model->indexFor(widget).isValid()) return;
    const QString field = node->field;
    if (!controller->selectLayoutWidget(widget, field.startsWith('@') ? QString() : field)) return;
    // A conservative map: unknown/computed fields still select the parameter row.
    static const QMap<QString, QString> controls {
        {"EngineSpeed", "lineEditEngineSpeed"}, {"EngineHours", "lineEditEngineHours"},
        {"OilPressure", "lineEditOilPressure"}, {"OilTemperature", "lineEditOilTemperature"},
        {"Latitude", "lineEditLatitude"}, {"Longitude", "lineEditLongitude"},
        {"EngineCoolantTemp", "lineEditEngineCoolantTemp"}, {"DiscreteStatus1", "checkBoxCheckTransmission"}
    };
    const QString control = controls.value(field);
    auto* target = control.isEmpty() ? nullptr : widget->findChild<QWidget*>(control);
    if (target) for (auto* parent = target->parentWidget(); parent && parent != widget; parent = parent->parentWidget()) {
        if (auto* tabs = qobject_cast<QTabWidget*>(parent)) {
            for (int i = 0; i < tabs->count(); ++i) if (tabs->widget(i) == target || tabs->widget(i)->isAncestorOf(target)) { tabs->setCurrentIndex(i); break; }
        }
    }
    if (auto* scroll = tab->findChild<QScrollArea*>()) scroll->ensureWidgetVisible(target ? target : widget.data(), 12, 12);
    showOutline(widget);
    view->setFocus(Qt::OtherFocusReason);
}
