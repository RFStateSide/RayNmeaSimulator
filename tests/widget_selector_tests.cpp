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
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QAbstractButton>
#include <QNetworkProxy>
#include <QSpinBox>
#include <QAbstractItemView>
#include "mainwindow.h"
#include "appsettings.h"
#include "pgntreecontroller.h"
#include "expansioninstruments.h"

namespace {
QMdiSubWindow *tab(MainWindow &window, const QString &name)
{
    for (auto *item : window.findChild<QMdiArea *>()->subWindowList())
        if (item->windowTitle() == name) return item;
    return nullptr;
}
void acceptSpacer()
{
    QTimer::singleShot(0, [] {
        for (auto *window : QApplication::topLevelWidgets())
            if (auto *dialog = qobject_cast<QDialog *>(window); dialog && dialog->findChild<QSpinBox *>())
                dialog->accept();
    });
}
void acceptReplacement()
{
    QTimer::singleShot(0, [] {
        for (auto *window : QApplication::topLevelWidgets())
            if (auto *box = qobject_cast<QMessageBox *>(window)) box->button(QMessageBox::Yes)->click();
    });
}
QByteArray bytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class WidgetSelectorTests : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.clear();
        settings.setValue("Nmea2000/InterfaceType", "None");
    }
    void selectors_data()
    {
        QTest::addColumn<QString>("group");
        QTest::newRow("Communication") << QString("Communication");
        QTest::newRow("Other") << QString("Other");
    }
    void selectors()
    {
        QFETCH(QString, group);
        MainWindow window;
        window.show();
        auto *page = tab(window, group);
        QVERIFY(page);
        auto *combo = page->findChild<QComboBox *>();
        QVERIFY(combo);
        QVERIFY(combo->isEnabled());
        QVERIFY(!combo->isEditable());
        QCOMPARE(combo->count(), group == "Communication" ? 5 : 4);
        QCOMPARE(combo->itemText(0), QString("None"));
        QCOMPARE(combo->itemText(1), QString("Spacer (PGN 000000)"));
        if(group=="Other") {
            QCOMPARE(combo->itemText(2),QString("Windlass Operating Status (PGN 128777)"));
            QCOMPARE(combo->itemText(3),QString("Windlass Monitoring Status (PGN 128778)"));
        }
        for (int i = 1; i <= 3; ++i) QCOMPARE(combo->findText(group + " Item " + QString::number(i)), -1);
        auto *controller = window.findChild<MainController *>();
        combo->setCurrentIndex(0);
        QVERIFY(controller->layoutWidgets(group).isEmpty());
        // A noneditable selector cannot choose the removed text or create an object.
        combo->setCurrentText(group + " Item 1");
        QCOMPARE(combo->currentIndex(), 0);
        QVERIFY(controller->layoutWidgets(group).isEmpty());
        const auto evidence = qEnvironmentVariable("M228_EVIDENCE");
        if (!evidence.isEmpty()) {
            QDir().mkpath(evidence);
            window.findChild<QMdiArea *>()->setActiveSubWindow(page);
            combo->showPopup();
            QTest::qWait(30);
            QVERIFY(combo->view()->window()->grab().save(evidence + "/" + group + "-selector.png"));
            combo->hidePopup();
        }
    }
    void spacerRoundTripAndInspection()
    {
        MainWindow window;
        window.show();
        auto *controller = window.findChild<MainController *>();
        auto *tree = window.findChild<PgnTreeController *>();
        QMap<QString, int> ids;
        for (const auto &group : {QString("Communication"), QString("Other")}) {
            auto *page = tab(window, group);
            QVERIFY(page);
            auto *combo = page->findChild<QComboBox *>();
            acceptSpacer();
            combo->setCurrentIndex(1);
            QCOMPARE(combo->currentIndex(), 0);
            QCOMPARE(controller->layoutWidgets(group).size(), 1);
            auto *widget = controller->layoutWidgets(group).first();
            QCOMPARE(QString(widget->metaObject()->className()), QString("Spacer000000"));
            ids[group] = qobject_cast<PgnWidgetBase *>(widget)->Id();
            QTRY_VERIFY(tree->getTreeModel()->indexFor(widget).isValid());
            tree->getTreeView()->setCurrentIndex(tree->getTreeModel()->indexFor(widget));
            QTRY_COMPARE(window.findChild<QMdiArea *>()->activeSubWindow(), page);
            auto *model = window.findChild<PgnTableController *>()->getPgnTableModel();
            QTRY_COMPARE(model->IdxHasFocus(), ids[group]);
        }
        QTemporaryDir directory;
        const auto path = directory.filePath("categories.json");
        QVERIFY(controller->saveLayoutToJson(path));
        controller->setMode(Viewer);
        controller->setMode(Sender);
        acceptReplacement();
        QVERIFY(controller->loadLayoutFromJson(path));
        for (auto it = ids.begin(); it != ids.end(); ++it) {
            QVERIFY(tab(window, it.key()));
            QCOMPARE(controller->layoutWidgets(it.key()).size(), 1);
            QCOMPARE(qobject_cast<PgnWidgetBase *>(controller->layoutWidgets(it.key()).first())->Id(), it.value());
        }
        QVERIFY(!window.findChild<QAction *>("actionStart_Sending")->isChecked());
        QTest::keyClick(&window, Qt::Key_F12);
        QVERIFY(!window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void unsupportedHistory_data()
    {
        QTest::addColumn<QString>("group");
        QTest::addColumn<QString>("type");
        for (const auto &group : {QString("Communication"), QString("Other")})
            for (int i = 1; i <= 3; ++i) {
                const auto type = group + " Item " + QString::number(i);
                QTest::newRow(qPrintable(type)) << group << type;
            }
    }
    void unsupportedHistory()
    {
        QFETCH(QString, group);
        QFETCH(QString, type);
        MainWindow window;
        auto *controller = window.findChild<MainController *>();
        acceptSpacer();
        tab(window, group)->findChild<QComboBox *>()->setCurrentIndex(1);
        auto *original = controller->layoutWidgets(group).first();
        QTemporaryDir directory;
        const auto path = directory.filePath("valid.json");
        QVERIFY(controller->saveLayoutToJson(path));
        const auto saved = bytes(path);
        auto root = QJsonDocument::fromJson(saved).object();
        auto groups = root["layouts"].toArray();
        for (int i = 0; i < groups.size(); ++i) {
            auto item = groups[i].toObject();
            if (item["groupName"] != group) continue;
            auto widgets = item["widgets"].toArray();
            auto widget = widgets[0].toObject();
            widget["type"] = type;
            widgets[0] = widget;
            item["widgets"] = widgets;
            groups[i] = item;
        }
        root["layouts"] = groups;
        const auto invalid = directory.filePath("unsupported.json");
        QFile file(invalid);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const auto input = QJsonDocument(root).toJson();
        QCOMPARE(file.write(input), qint64(input.size()));
        file.close();
        QVERIFY(!controller->loadLayoutFromJson(invalid));
        QCOMPARE(controller->layoutWidgets(group), QList<QWidget *>{original});
        QCOMPARE(bytes(path), saved);
        QCOMPARE(bytes(invalid), input);
    }
    void defaultLayoutAndOtherCategories()
    {
        MainWindow window;
        auto *controller = window.findChild<MainController *>();
        const QMap<QString, int> expected{{"Navigation", 23}, {"Propulsion", 7}, {"Environment", 7},
                                        {"Tanks", 3}, {"Electrical", 5}, {"Generate", 7}};
        for (auto it = expected.begin(); it != expected.end(); ++it) {
            auto *page = tab(window, it.key());
            QVERIFY(page);
            int expansionCount = 0;
            for (const auto &d : Expansion::definitions()) if (d.group == it.key()) ++expansionCount;
            QCOMPARE(page->findChild<QComboBox *>()->count(), it.value() + expansionCount);
        }
        QVERIFY(controller->loadLayoutFromJson(":/defaults/defaultLayout.json"));
        const auto count = window.findChildren<PgnWidgetBase *>().size();
        QVERIFY(count >= 30);
        QTemporaryDir directory;
        const auto path = directory.filePath("default-copy.json");
        QVERIFY(controller->saveLayoutToJson(path));
        acceptReplacement();
        QVERIFY(controller->loadLayoutFromJson(path));
        QCOMPARE(window.findChildren<PgnWidgetBase *>().size(), count);
        controller->setMode(Viewer);
        controller->setMode(Sender);
        QCOMPARE(window.findChildren<PgnWidgetBase *>().size(), count);
        QVERIFY(!window.findChild<QAction *>("actionStart_Sending")->isChecked());
        QVERIFY(tab(window, "Communication"));
        QVERIFY(tab(window, "Other"));
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir profile;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    WidgetSelectorTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "widget_selector_tests.moc"
