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

#include <QtTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QMessageBox>
#include <QAbstractButton>
#include <QScrollArea>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QDockWidget>
#include <QHeaderView>
#include <QNetworkProxy>
#include <QTabBar>
#include "mainwindow.h"
#include "appsettings.h"
#include "pgntreecontroller.h"
#include "themes.h"
#include "convert.h"

namespace {
struct Workspace {
    MainWindow window;
    MainController* controller;
    PgnTableModel* values;
    PgnTreeController* tree;
    PgnTreeModel* model;
    QTreeView* view;
    QMdiArea* area;
    QTableView* table;
    Workspace() {
        window.resize(1400, 900); window.show();
        controller = window.findChild<MainController*>();
        values = window.findChild<PgnTableController*>()->getPgnTableModel();
        tree = window.findChild<PgnTreeController*>(); model = tree->getTreeModel(); view = tree->getTreeView();
        area = window.findChild<QMdiArea*>(); table = window.findChild<QTableView*>("tableView");
        QTest::qWait(30);
    }
    void load() { window.findChild<QAction*>("actionLoad_Default")->trigger(); QTest::qWait(50); }
    QWidget* first(const QString& type) {
        for (auto* tab : area->subWindowList()) for (auto* widget : controller->layoutWidgets(tab->windowTitle()))
            if (QString(widget->metaObject()->className()) == type) return widget;
        return nullptr;
    }
    QString value(QWidget* widget, const QString& field) {
        return model->data(model->indexFor(widget, field).siblingAtColumn(1)).toString();
    }
    QByteArray snapshot() {
        QJsonArray result;
        for (const auto& d : values->getPgnList()) {
            QJsonArray fields; for (const auto& p : d.paramDetails) fields.append(QJsonArray::fromStringList(p));
            result.append(QJsonObject{{"id",d.idX},{"enabled",d.enabled},{"fields",fields}});
        }
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }
};
void acceptInstance() {
    QTimer::singleShot(0, [] {
        for(auto* widget:QApplication::topLevelWidgets())
            if(auto* dialog=qobject_cast<QDialog*>(widget); dialog && dialog->findChild<QSpinBox*>()) {
                dialog->findChild<QSpinBox*>()->setValue(1); dialog->accept();
            }
    });
}
void answer(QMessageBox::StandardButton choice) {
    QTimer::singleShot(0, [choice] {
        for (auto* widget : QApplication::topLevelWidgets())
            if (auto* message = qobject_cast<QMessageBox*>(widget)) message->button(choice)->click();
    });
}
}
class ObjectTreeTests : public QObject {
    Q_OBJECT
private slots:
    void init() {
        AppSettings settings(APP_COMPANY, APP_NAME); settings.clear();
        settings.setValue("Nmea2000/InterfaceType", "None"); settings.setValue("Preference/StyleSheetName", "Dark");
    }
    void hierarchyAndReadOnly() {
        Workspace w; w.load();
        QAbstractItemModelTester tester(w.model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(w.model->rowCount(), 1); const auto root = w.model->index(0,0);
        QCOMPARE(w.model->data(root).toString(), QString("Default Layout"));
        QCOMPARE(w.model->rowCount(root), 8); QVERIFY(w.view->isExpanded(root));
        int count = 0;
        for (int t = 0; t < w.area->subWindowList().size(); ++t) {
            auto tab = w.model->index(t, 0, root); auto* live = w.area->subWindowList()[t];
            QCOMPARE(w.model->data(tab).toString(), live->windowTitle()); QVERIFY(w.view->isExpanded(tab));
            auto registered = w.controller->layoutWidgets(live->windowTitle());
            QCOMPARE(w.model->rowCount(tab), registered.size());
            for (int i = 0; i < registered.size(); ++i) {
                auto object = w.model->index(i,0,tab); QCOMPARE(w.model->node(object)->widget.data(), registered[i]);
                QVERIFY(!w.view->isExpanded(object)); ++count;
                for (int p = 0; p < w.model->rowCount(object); ++p) {
                    auto property = w.model->index(p,0,object); QCOMPARE(w.model->rowCount(property),0);
                    QVERIFY(!(w.model->flags(property) & (Qt::ItemIsEditable|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled)));
                    QVERIFY(!w.model->setData(property,"changed"));
                }
            }
        }
        QCOMPARE(count,w.values->getPgnList().size());
        QVERIFY(!w.view->dragEnabled()); QCOMPARE(w.view->editTriggers(),QAbstractItemView::NoEditTriggers);
        auto* bar = w.area->findChild<QTabBar*>(QString(),Qt::FindDirectChildrenOnly); QVERIFY(bar);
        bar->moveTab(0,2); QTest::qWait(30);
        QCOMPARE(w.model->data(w.model->index(0,0,w.model->index(0,0))).toString(), w.area->subWindowList()[0]->windowTitle());
        acceptInstance(); w.controller->addWidgetAsComboboxTextChanged("127488","Same name","Propulsion");
        acceptInstance(); w.controller->addWidgetAsComboboxTextChanged("127488","Same name","Propulsion"); QTest::qWait(30);
        QList<QWidget*> duplicates;
        for(auto* widget:w.controller->layoutWidgets("Propulsion")) if(widget->windowTitle()=="Same name") duplicates<<widget;
        QCOMPARE(duplicates.size(),2);
        for(auto* widget:duplicates) w.values->changeParam(widget->property("Id").toInt(),"EngineInstance","0");
        QCOMPARE(w.model->node(w.model->indexFor(duplicates[0]))->widget.data(),duplicates[0]);
        QVERIFY(w.model->data(w.model->indexFor(duplicates[0])).toString()!=w.model->data(w.model->indexFor(duplicates[1])).toString());
    }
    void liveValuesUnitsAndPerformance() {
        Workspace w; w.load(); auto* engine=w.first("GeneratorEngine"); auto* movement=w.first("GeneratorVesselMovement"); QVERIFY(engine&&movement);
        auto index=w.model->indexFor(engine); w.view->expand(index); w.view->setCurrentIndex(index);
        QPersistentModelIndex stable(index); QSignalSpy resets(w.model,&QAbstractItemModel::modelReset),changes(w.model,&QAbstractItemModel::dataChanged),binding(w.values,&PgnTableModel::parameterChanged);
        w.values->changeParam(movement->property("Id").toInt(),"SOG","10"); QTRY_VERIFY(changes.count()>0);
        QVERIFY(w.value(movement,"SOG").endsWith("MPH")); QVERIFY(changes.count()>0); QCOMPARE(resets.count(),0); QVERIFY(stable.isValid()); QVERIFY(w.view->isExpanded(stable));
        auto* base=qobject_cast<PgnWidgetBase*>(movement); auto prefs=base->UserPrefs(); prefs.speedUnitIndex=3; prefs.latLongUnitIndex=0; base->setUserPrefs(prefs); QTest::qWait(80);
        QVERIFY(w.value(movement,"SOG").endsWith("knots"));
        w.values->changeParam(engine->property("Id").toInt(),"EngineHours","7200"); QCOMPARE(w.value(engine,"EngineHours"),QString("2.00 h"));
        w.values->changeParam(engine->property("Id").toInt(),"EngineOilTemp","273.15");
        QVERIFY(w.value(engine,"EngineOilTemp").contains("32.0 °F"));
        auto* engineBase=qobject_cast<PgnWidgetBase*>(engine); auto enginePrefs=engineBase->UserPrefs(); enginePrefs.tempUnitIndex=1; enginePrefs.pressureUnitIndex=1; enginePrefs.flowRateUnitIndex=0;
        engineBase->setUserPrefs(enginePrefs); QTest::qWait(70);
        QCOMPARE(w.value(engine,"EngineOilTemp"),QString("0.0 °C"));
        w.values->changeParam(engine->property("Id").toInt(),"EngineOilPress","100000"); QCOMPARE(w.value(engine,"EngineOilPress"),QString("100.0 kPa"));
        w.values->changeParam(engine->property("Id").toInt(),"FuelRate","12"); QCOMPARE(w.value(engine,"FuelRate"),QString("12.00 L/h"));
        w.values->changeParam(movement->property("Id").toInt(),"Latitude","37.123456"); QVERIFY(w.value(movement,"Latitude").contains("37.123456"));
        changes.clear(); QElapsedTimer elapsed; elapsed.start();
        for(int i=0;i<10000;++i) w.values->changeParam(movement->property("Id").toInt(),"SOG",QString::number(i/100.0));
        const auto duration=elapsed.elapsed();
        QVERIFY(w.value(movement,"SOG").contains(QString::number(Convert::MetersPerSecondToKnots(99.99),'f',1)));
        QTest::qWait(100);
        qInfo()<<"10000 updates ms:"<<duration<<"tree notifications:"<<changes.count();
        QVERIFY(duration<3000); QVERIFY(changes.count()<20); QCOMPARE(resets.count(),0); QCOMPARE(binding.count(),0);
    }
    void navigationWithoutMutation() {
        Workspace w; w.load(); auto* transmission=w.first("GeneratorTransLowerUnit"); QVERIFY(transmission);
        auto before=w.snapshot(); QSignalSpy binding(w.values,&PgnTableModel::parameterChanged);
        const auto property=w.model->indexFor(transmission,"DiscreteStatus1"); w.view->setCurrentIndex(property); QTest::qWait(30);
        QCOMPARE(w.area->activeSubWindow()->windowTitle(),QString("Generate"));
        QCOMPARE(w.table->model()->data(w.table->model()->index(w.table->currentIndex().row(),0)).toString(),QString("DiscreteStatus1"));
        auto* control=transmission->findChild<QCheckBox*>("checkBoxCheckTransmission"); QVERIFY(control); QVERIFY(control->isVisible());
        QVERIFY(transmission->parentWidget()->findChild<QWidget*>("objectTreeSelectionOutline"));
        QVERIFY(w.table->findChildren<QLineEdit*>().isEmpty());
        w.view->setFocus(); QTest::keyClick(w.view,Qt::Key_Down); QTest::qWait(30);
        QCOMPARE(w.snapshot(),before); QCOMPARE(binding.count(),0);
        QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
        auto* scroll=w.area->activeSubWindow()->findChild<QScrollArea*>(); QVERIFY(scroll);
        auto* last=w.controller->layoutWidgets("Generate").last(); w.view->setCurrentIndex(w.model->indexFor(last)); QTest::qWait(30);
        const auto rect=QRect(last->mapTo(scroll->viewport(),QPoint()),last->size()); QVERIFY(rect.intersects(scroll->viewport()->rect()));
        auto* edit=last->findChild<QLineEdit*>(); if(edit && !edit->isReadOnly()) { edit->setFocus(); QTest::qWait(20); QVERIFY(w.model->node(w.view->currentIndex())->widget==last); }
    }
    void pendingEditProtection() {
        Workspace w; w.load(); auto* engine=w.first("GeneratorEngine"); auto* edit=engine->findChild<QLineEdit*>("lineEditEngineHours"); QVERIFY(edit);
        w.view->setCurrentIndex(w.model->indexFor(engine)); edit->setFocus(); QTest::qWait(20);
        edit->setText("unfinished"); edit->setModified(true); const auto before=w.snapshot();
        QVERIFY(w.tree->navigationBlocked());
        auto target=w.model->indexForTab(w.area->subWindowList().first()); w.view->scrollTo(target); QTest::qWait(20);
        QTest::mouseClick(w.view->viewport(),Qt::LeftButton,Qt::NoModifier,w.view->visualRect(target).center());
        QCOMPARE(edit->text(),QString("unfinished")); QVERIFY(edit->hasFocus()); QCOMPARE(w.snapshot(),before);
        // Discard only this synthetic edit without emitting editingFinished.
        edit->setText("150"); edit->setModified(false);
        w.controller->selectLayoutWidget(engine,"EngineHours"); w.table->setFocus(); w.table->edit(w.table->currentIndex()); QTest::qWait(20);
        auto* delegate=w.table->viewport()->findChild<QLineEdit*>(); QVERIFY(delegate); delegate->setFocus();
        QVERIFY(w.tree->navigationBlocked()); const auto tableBefore=w.snapshot();
        QTest::mouseClick(w.view->viewport(),Qt::LeftButton,Qt::NoModifier,w.view->visualRect(target).center());
        QVERIFY(delegate->hasFocus()); QCOMPARE(w.snapshot(),tableBefore); QTest::keyClick(delegate,Qt::Key_Escape);
    }
    void lifecycleAndModes() {
        Workspace w; w.load(); auto* old=w.first("GeneratorEngine"); QPointer<QWidget> guard(old); QTRY_VERIFY(w.model->indexFor(old).isValid()); QPersistentModelIndex stale(w.model->indexFor(old));
        answer(QMessageBox::No); QVERIFY(!w.controller->loadLayoutFromJson(":/defaults/defaultLayout.json")); QVERIFY(stale.isValid());
        QVERIFY(!w.controller->loadLayoutFromJson("missing-layout-m214.json")); QVERIFY(stale.isValid());
        answer(QMessageBox::Yes); QVERIFY(w.controller->loadLayoutFromJson(":/defaults/defaultLayout.json")); QTest::qWait(40);
        QVERIFY(!guard); QVERIFY(!stale.isValid()); auto* current=w.first("GeneratorEngine"); QVERIFY(current);
        for(int i=0;i<3;++i) {
            // Accepted M2.33A entry now requires an explicit Keep Existing choice.
            QTimer::singleShot(0, [] {
                auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                QVERIFY(box); QCOMPARE(box->objectName(),QString("viewerEntryDialog"));
                auto *keep=box->findChild<QAbstractButton*>("viewerKeepExisting");QVERIFY(keep);keep->click();
            });
            w.window.findChild<QAction*>("actionViewer_Rx")->setChecked(true); QTest::qWait(70);
            QCOMPARE(w.first("GeneratorEngine"),current); QVERIFY(w.model->data(w.model->indexFor(current).siblingAtColumn(1)).toString().contains("Suspended"));
            QVERIFY(!(w.values->flags(w.values->index(0,1)) & Qt::ItemIsEditable));
            w.window.findChild<QAction*>("actionSimulator_Tx")->setChecked(true); QTest::qWait(30); QCOMPARE(w.first("GeneratorEngine"),current);
            QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
        }
        auto* rapid=w.first("EngineRapid127488"); QPointer<QWidget> removed(rapid);
        w.controller->removeWidgetFromSubForm(rapid,"Propulsion"); QTest::qWait(30); QVERIFY(!removed);
        // Independent QObject destruction must also invalidate inspection references.
        auto* position=w.first("LatLong129025"); QPersistentModelIndex deleted(w.model->indexFor(position)); delete position; QTest::qWait(30); QVERIFY(!deleted.isValid());
        w.controller->setMode(Viewer);
        w.controller->addRcvdPgn("127508",{"BatteryInstance","7","BatteryVoltage","12.7","BatteryCurrent","2","BatteryTemperature","295","SID","1"},"test-source"); QTest::qWait(40);
        QWidget* received=nullptr; for(auto* widget:w.controller->layoutWidgets("Electrical")) if(w.values->getSource(widget->property("Id").toInt())=="test-source") received=widget;
        QVERIFY(received); QVERIFY(w.model->indexFor(received).isValid()); QCOMPARE(w.value(received,"@Source"),QString("test-source"));
    }
    void themesAndRendering() {
        Workspace w; w.load();
        for(const QString theme:{"Dark","Light"}) {
            AppSettings settings(APP_COMPANY,APP_NAME); settings.setValue("Preference/StyleSheetName",theme); settings.sync();
            w.window.HotReloadSettings(); QTest::qWait(60);
            auto* engine=w.first("GeneratorEngine"); w.view->expand(w.model->indexFor(engine)); w.view->setCurrentIndex(w.model->indexFor(engine));
            w.view->scrollTo(w.model->indexFor(engine,"EngineHours"),QAbstractItemView::PositionAtCenter);
            QCOMPARE(w.window.styleSheet(),Themes::styleSheet(theme));
            const QString dir=qEnvironmentVariable("M214_EVIDENCE"); if(!dir.isEmpty()) { QDir().mkpath(dir); QVERIFY(w.window.grab().save(dir+"/"+theme+"-tree.png")); }
        }
    }
};
int main(int argc,char** argv) {
    QTemporaryDir profile(QDir::currentPath()+"/m214-profile-XXXXXX"); if(!profile.isValid()) return 2; qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QList<QByteArray> arguments; for(int i=0;i<argc;++i) arguments<<argv[i]; arguments<<"--no-nmea";
    QList<char*> pointers; for(auto& a:arguments)pointers<<a.data();int count=pointers.size();pointers<<nullptr;
    QApplication app(count,pointers.data());app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    ObjectTreeTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "object_tree_tests.moc"
