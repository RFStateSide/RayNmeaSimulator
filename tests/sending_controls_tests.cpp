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
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QToolBar>
#include <QStatusBar>
#include <QToolButton>
#include <QTemporaryDir>
#include <cstdio>
#include <memory>
#include "appsettings.h"
#include "transportcapabilities.h"
#include "mainwindow.h"
#include "themes.h"
#include "credentialstore.h"
#include <QScopeGuard>
#include "signalk_testserver.h"

class SendingMemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool failOpen = false, partialWrite = false;
    bool open(OpenMode mode) override { return !failOpen && QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { QIODevice::close(); }
    qint64 bytesToWrite() const override { return 0; }
protected:
    qint64 writeData(const char* data, qint64 size) override {
        const qint64 accepted = partialWrite ? qMin(qint64(3), size) : size;
        output.append(data, accepted); return accepted;
    }
};
class SendingCanBackend : public SocketCanBackend {
public:
    bool available = false, accept = true;
    int submissions = 0, closes = 0, error = 0;
    void configure(const SocketCanConfig&, std::function<void(const tN2kMsg&)>) override {}
    void service() override {}
    bool ready() const override { return available; }
    int openingError() const override { return error; }
    bool send(const tN2kMsg&) override { ++submissions; return accept; }
    bool close() override { ++closes; available = false; return true; }
};

namespace {
QAction* start(MainWindow& window) { return window.findChild<QAction*>("actionStart_Sending"); }
QAction* stop(MainWindow& window) { return window.findChild<QAction*>("actionStop_Sending"); }
NmeaSenderWorker* worker(MainWindow& window) { return window.findChild<NmeaSenderWorker*>(); }
QString status(MainWindow& window) { return window.findChild<QLabel*>("sendingStatus")->toolTip(); }
PgnTableModel* model(MainWindow& window) { return window.findChild<PgnTableModel*>(); }
void serialSettings()
{
    AppSettings settings(APP_COMPANY, APP_NAME);
    settings.setValue("Nmea0183/Nmea0183Enabled", true);
    settings.setValue("Nmea0183/Nmea0183Port", "MEMORY"); settings.sync();
}
void canSettings()
{
    AppSettings settings(APP_COMPANY, APP_NAME);
    settings.setValue("Nmea2000/Nmea2000Enabled", true);
    settings.setValue("Nmea2000/InterfaceType", "SocketCan");
    settings.setValue("Nmea2000/SocketCanInterface", "m211fixture");
    settings.setValue("Nmea2000/SocketCanUniqueNumber", 42); settings.sync();
}
void signalKSettings(const TestServer& server)
{
    AppSettings settings(APP_COMPANY, APP_NAME);
    settings.setValue("Nmea2000/Nmea2000Enabled", true);
    settings.setValue("Nmea2000/InterfaceType", "SignalK");
    settings.setValue("Nmea2000/SignalKServer", server.config().server); settings.sync();
}
void addPosition(MainWindow& window)
{
    model(window)->addPgn(1, "Fixture", "129025", true,
        {{"Latitude", "42", "deg", "double"}, {"Longitude", "-70", "deg", "double"}}, {}, 250);
}
void capture(QWidget& widget, const QString& name)
{
    const QString directory = qEnvironmentVariable("M211_EVIDENCE");
    if (directory.isEmpty()) return;
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(widget.grab().save(directory + "/" + name + ".png"));
}
}

class SendingControlsTests : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings settings(APP_COMPANY, APP_NAME); settings.clear();
        settings.setValue("Preservation/Sentinel", "unchanged"); settings.sync();
    }
    void firstRunAndSharedActions()
    {
        MainWindow window; window.show(); QTest::qWait(30);
        QVERIFY(!worker(window)->isSending()); QVERIFY(!start(window)->isChecked());
        QVERIFY(!start(window)->isEnabled()); QVERIFY(!stop(window)->isEnabled());
        QVERIFY(status(window).contains("Setup"));
        auto* toolbar = window.findChild<QToolBar*>("toolBarSending"); QVERIFY(toolbar);
        auto* menu = window.findChild<QMenu*>("menuSending"); QVERIFY(menu);
        QCOMPARE(qobject_cast<QToolButton*>(toolbar->widgetForAction(start(window)))->defaultAction(), start(window));
        QVERIFY(menu->actions().contains(start(window))); QVERIFY(menu->actions().contains(stop(window)));
        QVERIFY(!window.findChild<QMenu*>("menuTest")->actions().contains(start(window)));
        start(window)->setChecked(true); QVERIFY(!start(window)->isChecked());
        QVERIFY(!worker(window)->isSending());
        capture(window, "first-run");
        toolbar->hide(); window.close();
        MainWindow restored; restored.show();
        QVERIFY(restored.findChild<QToolBar*>("toolBarSending")->isVisible());
        QVERIFY(!start(restored)->isChecked());
    }
    void singleLineStatusBar_data()
    {
        QTest::addColumn<QString>("theme"); QTest::addColumn<int>("points");
        QTest::newRow("Dark normal font") << QString("Dark") << 10;
        QTest::newRow("Light normal font") << QString("Light") << 10;
        QTest::newRow("Dark large font") << QString("Dark") << 14;
        QTest::newRow("Light large font") << QString("Light") << 14;
    }
    void singleLineStatusBar()
    {
        QFETCH(QString, theme); QFETCH(int, points);
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Preference/StyleSheetName", theme); settings.sync();
        settings.setValue("Nmea2000/Nmea2000Enabled", true);
        settings.setValue("Nmea2000/InterfaceType", "SignalK"); settings.sync();
        MainWindow window; auto* bar = window.statusBar();
        auto* label = window.findChild<QLabel*>("transportStatus"); QVERIFY(label);
        QFont font = label->font(); font.setPointSize(points); label->setFont(font); bar->setFont(font);
        window.resize(1100, 700); window.show(); QTest::qWait(40);
        const int height = bar->height();
        QVERIFY(label->alignment().testFlag(Qt::AlignRight)); QVERIFY(!label->wordWrap());
        QVERIFY(!label->hasHeightForWidth()); QCOMPARE(label->minimumSizeHint().width(), 0);
        auto* handler = window.findChild<Nmea2000Handler*>();
        const QStringList messages {"Signal K: stopped", "Signal K: checking write authorization",
            "Signal K: connected; waiting for server hello", "Signal K: publishing as raynmeasimulator",
            "Signal K: disconnected or unauthorized; retrying (check server and write token)",
            "Signal K: publishing as " + QString(180, 'x')};
        for (int width : {720, 1100}) {
            window.resize(width, 700); QTest::qWait(30);
            for (int index = 0; index < messages.size(); ++index) {
                const QString& message = messages[index];
                emit handler->findChild<SignalKPublisher*>()->statusChanged(message); QTest::qWait(20);
                QCOMPARE(label->text(), message); QCOMPARE(label->toolTip(), message);
                QCOMPARE(bar->height(), height); QVERIFY(label->isVisible());
                QVERIFY(label->height() >= label->fontMetrics().height());
                QVERIFY(label->height() < label->fontMetrics().height() * 2);
                QVERIFY(bar->rect().contains(label->geometry()));
                // The permanent widget stays at the right edge, ahead of the size grip.
                QVERIFY(bar->width() - label->geometry().right() <= 40);
                if (index == messages.size() - 1) QVERIFY(label->width() < label->fontMetrics().horizontalAdvance(message));
                if (index == 0) QVERIFY(label->width() >= label->fontMetrics().horizontalAdvance(message));
                capture(window, "statusbar-" + theme + QString::number(points) + "-" + QString::number(width) + "-" + QString::number(index));
            }
        }
        bar->showMessage("Temporary application message");
        emit handler->findChild<SignalKPublisher*>()->statusChanged(messages.first()); QTest::qWait(20);
        QVERIFY(label->isVisible()); QCOMPARE(bar->height(), height);
        // An inactive SocketCAN notice must not take ownership of this label.
        emit handler->SocketCanStatus("SocketCAN Ready"); QTest::qWait(20);
        QCOMPARE(label->text(), messages.first()); QCOMPARE(label->toolTip(), label->text());
        QVERIFY(!start(window)->isChecked()); QVERIFY(!worker(window)->isSending());
    }
    void compactToolbar_data()
    {
        QTest::addColumn<QString>("theme"); QTest::addColumn<int>("points");
        QTest::newRow("Dark normal font") << QString("Dark") << 10;
        QTest::newRow("Light normal font") << QString("Light") << 10;
        QTest::newRow("Dark large font") << QString("Dark") << 14;
        QTest::newRow("Light large font") << QString("Light") << 14;
    }
    void compactToolbar()
    {
        QFETCH(QString, theme); QFETCH(int, points);
        serialSettings(); canSettings(); SendingCanBackend backend;
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Preference/StyleSheetName", theme); settings.sync();
        auto* port = new SendingMemoryPort; MainWindow window(nullptr, nullptr, port, &backend);
        QFont font = window.font(); font.setPointSize(points); window.setFont(font);
        qApp->setPalette(Themes::palette(theme)); window.setStyleSheet(Themes::styleSheet(theme));
        auto* toolbar = window.findChild<QToolBar*>("toolBarSending");
        auto* label = window.findChild<QLabel*>("sendingStatus");
        toolbar->setFont(font); label->setFont(font);
        for (auto* action : {start(window), stop(window)}) toolbar->widgetForAction(action)->setFont(font);
        window.resize(1100, 700); window.show(); QTest::qWait(30);
        const int height = toolbar->height();
        QVERIFY(!label->wordWrap()); QVERIFY(!label->hasHeightForWidth());
        QVERIFY(height <= window.fontMetrics().height() + 18);
        QVERIFY(label->text().contains("Stopped"));
        capture(window, "compact-" + theme + QString::number(points) + "-stopped");
        start(window)->trigger(); QTest::qWait(20);
        QVERIFY(label->text().contains("Sending: NMEA0183"));
        QVERIFY(label->text().contains("NMEA2000 (SocketCan): unavailable"));
        QVERIFY(label->toolTip().contains("SocketCAN"));
        QVERIFY(label->accessibleDescription().contains("SocketCAN"));
        for (int width : {720, 1100, 1500}) {
            window.resize(width, 700); QTest::qWait(30);
            QCOMPARE(toolbar->height(), height);
            QVERIFY(label->isVisible()); QVERIFY(label->width() >= 120);
            QCOMPARE(label->font().pointSize(), points);
            for (auto* action : {start(window), stop(window)}) {
                auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(action));
                QVERIFY(button->isVisible()); QVERIFY(toolbar->rect().contains(button->geometry()));
                QVERIFY(button->height() >= button->fontMetrics().height());
                QVERIFY(button->width() >= button->fontMetrics().horizontalAdvance(action->text()));
            }
            capture(window, "compact-" + theme + QString::number(points) + "-" + QString::number(width));
        }
        // Recreate the previous wrapped status policy at the same narrow width.
        window.resize(720, 700); QTest::qWait(20);
        QMainWindow previous; previous.setFont(font); previous.setPalette(window.palette());
        previous.setStyleSheet(window.styleSheet()); previous.resize(window.size());
        auto* oldToolbar = previous.addToolBar("Previous");
        oldToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        oldToolbar->addAction(start(window)); oldToolbar->addAction(stop(window)); oldToolbar->addSeparator();
        auto* oldStatus = new QLabel(label->accessibleDescription()); oldStatus->setWordWrap(true);
        oldStatus->setMinimumWidth(240); oldStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        oldToolbar->addWidget(oldStatus); previous.show(); QTest::qWait(30);
        qInfo() << theme << points << "toolbar height old/new" << oldToolbar->height() << toolbar->height();
        QVERIFY(oldToolbar->height() > toolbar->height()); previous.hide();
        emit port->errorOccurred(QSerialPort::ResourceError); QTest::qWait(20);
        QVERIFY(label->text().contains("Stopped")); QVERIFY(label->text().contains("NMEA0183: unavailable"));
        QVERIFY(label->toolTip().contains("serial error")); QCOMPARE(toolbar->height(), height);
        capture(window, "compact-" + theme + QString::number(points) + "-error");
        QVERIFY(!start(window)->isChecked()); QVERIFY(!stop(window)->isEnabled());
        // Changing theme and resizing must not alter authorization or toolbar height.
        qApp->setPalette(Themes::palette(theme == "Dark" ? "Light" : "Dark"));
        window.setStyleSheet(Themes::styleSheet(theme == "Dark" ? "Light" : "Dark"));
        QTest::qWait(20); QVERIFY(qAbs(toolbar->height() - height) <= 1); QVERIFY(!worker(window)->isSending());
    }
    void independentStartStopCommands()
    {
        serialSettings(); auto *port = new SendingMemoryPort;
        MainWindow window(nullptr, nullptr, port); window.show();
        auto *toolbar = window.findChild<QToolBar*>("toolBarSending");
        auto *enableButton = qobject_cast<QToolButton*>(toolbar->widgetForAction(start(window)));
        QVERIFY(enableButton); QVERIFY(start(window)->isEnabled()); QVERIFY(!stop(window)->isEnabled());
        QVERIFY(status(window).contains("stopped"));
        QSignalSpy finished(worker(window), &NmeaSenderWorker::finished);
        int stoppingObservations = 0;
        connect(worker(window), &NmeaSenderWorker::finished, &window, [&] {
            // Reentrant commands must see the stopping transaction, not a new
            // ready state between worker stop and transport cancellation.
            QVERIFY(!start(window)->isEnabled()); QVERIFY(!stop(window)->isEnabled());
            QVERIFY(window.findChild<QLabel*>("sendingStatus")->text().contains("Stopping"));
            QVERIFY(QMetaObject::invokeMethod(&window, "on_actionStart_Sending_toggled", Q_ARG(bool, true)));
            QVERIFY(!worker(window)->isSending()); ++stoppingObservations;
        });
        for (int cycle = 0; cycle < 4; ++cycle) {
            stop(window)->trigger(); QVERIFY(!worker(window)->isSending()); QCOMPARE(finished.size(), cycle);
            enableButton->click(); QVERIFY(worker(window)->isSending()); QVERIFY(start(window)->isChecked());
            QVERIFY(!start(window)->isEnabled()); QVERIFY(!enableButton->isEnabled()); QVERIFY(stop(window)->isEnabled());
            QVERIFY(status(window).contains("Sending enabled"));
            enableButton->click(); start(window)->trigger();
            QVERIFY(worker(window)->isSending()); QCOMPARE(finished.size(), cycle);
            // Even an old check-state API or queued legacy invocation cannot
            // route Enable Sending to STOP or submit a duplicate START.
            start(window)->setChecked(false); QVERIFY(worker(window)->isSending()); QVERIFY(start(window)->isChecked());
            QVERIFY(QMetaObject::invokeMethod(&window, "on_actionStart_Sending_toggled", Q_ARG(bool, false)));
            QVERIFY(QMetaObject::invokeMethod(&window, "on_actionStart_Sending_toggled", Q_ARG(bool, true)));
            QVERIFY(worker(window)->isSending()); QCOMPARE(finished.size(), cycle);
            if (cycle % 2) QTest::keyClick(&window, Qt::Key_F12); else stop(window)->trigger();
            QVERIFY(!worker(window)->isSending()); QVERIFY(!start(window)->isChecked());
            QVERIFY(start(window)->isEnabled()); QVERIFY(!stop(window)->isEnabled());
            QVERIFY(status(window).contains("Sending stopped")); QCOMPARE(finished.size(), cycle + 1);
            stop(window)->trigger(); QVERIFY(QMetaObject::invokeMethod(&window, "on_actionStop_Sending_triggered"));
            QCOMPARE(finished.size(), cycle + 1);
        }
        QCOMPARE(stoppingObservations, 4);
        window.findChild<QAction*>("actionViewer_Rx")->setChecked(true);
        QVERIFY(!start(window)->isEnabled()); QVERIFY(!stop(window)->isEnabled());
        window.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);
        QVERIFY(start(window)->isEnabled()); QVERIFY(!stop(window)->isEnabled()); QVERIFY(!worker(window)->isSending());
    }
    void pendingAndRecoveringCommands()
    {
        TestServer server; server.sendHello = false; signalKSettings(server);
        MainWindow window; auto *publisher = window.findChild<SignalKPublisher*>(); publisher->configure(server.config());
        QSignalSpy connections(&server.ws, &QWebSocketServer::newConnection);
        QVERIFY(start(window)->isEnabled()); QVERIFY(!stop(window)->isEnabled());
        start(window)->trigger(); QTRY_VERIFY_WITH_TIMEOUT(server.peer, 3000);
        QVERIFY(!start(window)->isEnabled()); QVERIFY(stop(window)->isEnabled()); QVERIFY(!worker(window)->isSending());
        QVERIFY(status(window).contains("connecting/recovering")); QCOMPARE(connections.size(), 1);
        const auto firstPeer = server.peer;
        start(window)->trigger(); QVERIFY(QMetaObject::invokeMethod(&window, "on_actionStart_Sending_toggled", Q_ARG(bool, true)));
        QTest::qWait(50); QCOMPARE(connections.size(), 1); QCOMPARE(server.peer, firstPeer); QVERIFY(publisher->isActive());
        stop(window)->trigger(); QVERIFY(!publisher->isActive()); QVERIFY(!stop(window)->isEnabled()); QVERIFY(start(window)->isEnabled());
        server.sendHello = true; start(window)->trigger(); QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(), 3000);
        QVERIFY(worker(window)->isSending()); QVERIFY(!start(window)->isEnabled()); QVERIFY(stop(window)->isEnabled());
        server.writer = false; server.peer->abort(); QTRY_VERIFY_WITH_TIMEOUT(!publisher->isReady(), 1500);
        QVERIFY(publisher->isActive()); QVERIFY(!start(window)->isEnabled()); QVERIFY(stop(window)->isEnabled());
        start(window)->trigger(); QVERIFY(publisher->isActive());
        server.writer = true; QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(), 5000);
        QVERIFY(worker(window)->isSending()); QVERIFY(!start(window)->isEnabled()); QVERIFY(stop(window)->isEnabled());
        stop(window)->trigger(); QVERIFY(!publisher->isActive()); QVERIFY(!worker(window)->isSending());
        QVERIFY(start(window)->isEnabled()); QVERIFY(!stop(window)->isEnabled());
    }
    void serialEnableFilteringAndStop()
    {
        serialSettings(); auto* port = new SendingMemoryPort;
        MainWindow window(nullptr, nullptr, port); window.show();
        QVERIFY(start(window)->isEnabled()); QVERIFY(port->output.isEmpty());
        auto* toolbar = window.findChild<QToolBar*>("toolBarSending");
        auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(start(window)));
        model(window)->addPgn(1, "Fixture", "HDT", false, {{"Heading", "1", "rad", "double"}}, {}, 250);
        button->click(); QVERIFY(worker(window)->isSending()); QVERIFY(button->isChecked());
        QVERIFY(status(window).contains("Waiting for enabled instruments"));
        QTest::qWait(300); QVERIFY(port->output.isEmpty());
        model(window)->setPgnEnabled(1, true);
        QVERIFY(status(window).contains("Enabled instruments only"));
        QTRY_VERIFY_WITH_TIMEOUT(!port->output.isEmpty(), 1500);
        model(window)->setPgnEnabled(1, false);
        const auto filtered = port->output; QTest::qWait(300); QCOMPARE(port->output, filtered);
        capture(window, "sending-serial");
        stop(window)->trigger(); QVERIFY(!button->isChecked()); QVERIFY(!worker(window)->isSending());
        QTest::qWait(300); QCOMPARE(port->output, filtered);
        start(window)->trigger(); QVERIFY(button->isChecked());
        button->click(); QVERIFY(start(window)->isChecked()); QVERIFY(worker(window)->isSending());
        stop(window)->trigger(); QVERIFY(!start(window)->isChecked()); QVERIFY(!worker(window)->isSending());
    }
    void serialFailureAndExplicitRecovery()
    {
        serialSettings(); auto* port = new SendingMemoryPort; port->failOpen = true;
        MainWindow window(nullptr, nullptr, port);
        QVERIFY(!start(window)->isEnabled()); QVERIFY(!start(window)->isChecked());
        port->failOpen = false;
        auto* handler = window.findChild<Nmea0183Handler*>(); QVERIFY(handler);
        QVERIFY(handler->Initialize("MEMORY")); QVERIFY(handler->OpenDevice());
        QVERIFY(start(window)->isEnabled()); QVERIFY(!worker(window)->isSending());
        model(window)->addPgn(1, "Fixture", "HDT", true, {{"Heading", "1", "rad", "double"}}, {}, 250);
        port->partialWrite = true; start(window)->trigger(); worker(window)->sendNmeaMessage();
        QVERIFY(!start(window)->isChecked()); QVERIFY(!worker(window)->isSending());
        QVERIFY(status(window).contains("error"));
        window.show(); QTest::qWait(20); capture(window, "serial-error");
        const auto partial = port->output; QTest::qWait(300); QCOMPARE(port->output, partial);
        port->partialWrite = false; QVERIFY(handler->OpenDevice());
        QVERIFY(!worker(window)->isSending()); start(window)->trigger();
        QVERIFY(worker(window)->isSending());
        emit port->errorOccurred(QSerialPort::ResourceError);
        QVERIFY(!worker(window)->isSending()); QVERIFY(!start(window)->isChecked());
        QVERIFY(!handler->IsOpen()); QVERIFY(status(window).contains("error"));
    }
    void modesLayoutsAndSettingsNeverResume()
    {
        // Retain the established layout schema, with one navigation instrument;
        // generator/engine acceptance is independent of this TX lifecycle check.
        QFile baseline(":/defaults/defaultLayout.json"); QVERIFY(baseline.open(QIODevice::ReadOnly));
        auto layout = QJsonDocument::fromJson(baseline.readAll()).object();
        QJsonArray groups;
        for (auto groupValue : layout["layouts"].toArray()) {
            auto group = groupValue.toObject(); QJsonArray widgets;
            for (auto value : group["widgets"].toArray())
                if (value.toObject()["type"] == "LatLong129025") widgets.append(value);
            if (!widgets.isEmpty()) { group["widgets"] = widgets; groups.append(group); }
        }
        QVERIFY(!groups.isEmpty()); layout["layouts"] = groups;
        const QString directory = TestProfile::root() + "/.raynmeasimulator/layouts";
        QVERIFY(QDir().mkpath(directory)); QFile fixture(directory + "/defaultLayout.json");
        QVERIFY(fixture.open(QIODevice::WriteOnly));
        QVERIFY(fixture.write(QJsonDocument(layout).toJson()) > 0); fixture.close();
        serialSettings(); MainWindow window(nullptr, nullptr, new SendingMemoryPort);
        QTimer confirmation;
        confirmation.setInterval(10);
        connect(&confirmation, &QTimer::timeout, &window, [] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                auto* box = qobject_cast<QMessageBox*>(widget);
                if (!box || !box->isVisible()) continue;
                if (box->objectName() == "viewerEntryDialog")
                    box->findChild<QAbstractButton*>("viewerKeepExisting")->click();
                else if (box->objectName() == "layoutOperationDialog")
                    box->button(box->standardButtons().testFlag(QMessageBox::Discard)
                        ? QMessageBox::Discard : QMessageBox::Yes)->click();
            }
        });
        confirmation.start();
        for (int cycle = 0; cycle < 3; ++cycle) {
            start(window)->trigger(); QVERIFY(worker(window)->isSending());
            window.findChild<QAction*>("actionViewer_Rx")->setChecked(true);
            QVERIFY(!worker(window)->isSending()); QVERIFY(!start(window)->isEnabled());
            window.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);
            QVERIFY(start(window)->isEnabled()); QVERIFY(!start(window)->isChecked());
            start(window)->trigger(); QVERIFY(worker(window)->isSending());
            window.findChild<QAction*>("actionLoad_Default")->trigger();
            QVERIFY(!worker(window)->isSending()); QVERIFY(!start(window)->isChecked());
            start(window)->trigger(); QVERIFY(worker(window)->isSending());
            window.HotReloadSettings(); QVERIFY(!worker(window)->isSending());
            QVERIFY(!start(window)->isChecked());
        }
        AppSettings settings(APP_COMPANY, APP_NAME);
        QCOMPARE(settings.value("Preservation/Sentinel").toString(), QString("unchanged"));
    }
    void socketCanReadinessAndTerminalStop()
    {
        canSettings(); SendingCanBackend backend;
        MainWindow window(nullptr, nullptr, nullptr, &backend);
        auto* session = window.findChild<SocketCanSession*>(); QVERIFY(session);
        QVERIFY(!start(window)->isEnabled()); backend.available = true; session->service();
        QVERIFY(start(window)->isEnabled()); QVERIFY(!start(window)->isChecked());
        start(window)->trigger(); QVERIFY(worker(window)->isSending());
        addPosition(window); worker(window)->sendNmeaMessage(); QVERIFY(backend.submissions > 0);
        stop(window)->trigger(); QVERIFY(!worker(window)->isSending());
        QVERIFY(!start(window)->isEnabled()); QVERIFY(session->restartRequired()); QCOMPARE(backend.closes, 1);
        QVERIFY(status(window).contains("Restart", Qt::CaseInsensitive));
    }
    void socketCanWriteFailure()
    {
        canSettings(); SendingCanBackend backend; backend.available = true; backend.accept = false;
        MainWindow window(nullptr, nullptr, nullptr, &backend);
        window.findChild<SocketCanSession*>()->service(); addPosition(window);
        start(window)->trigger(); worker(window)->sendNmeaMessage();
        QVERIFY(!worker(window)->isSending()); QVERIFY(!start(window)->isChecked());
        QVERIFY(status(window).contains("transmission failed")); QCOMPARE(backend.closes, 1);
    }
    void independentCanAndSerial_data()
    {
        QTest::addColumn<bool>("canReady"); QTest::addColumn<bool>("serialReady");
        QTest::newRow("CAN only available") << true << false;
        QTest::newRow("serial only available") << false << true;
        QTest::newRow("both available") << true << true;
        QTest::newRow("neither available") << false << false;
    }
    void independentCanAndSerial()
    {
        QFETCH(bool, canReady); QFETCH(bool, serialReady);
        canSettings(); serialSettings(); SendingCanBackend backend; backend.available = canReady;
        auto* port = new SendingMemoryPort; port->failOpen = !serialReady;
        MainWindow window(nullptr, nullptr, port, &backend);
        window.findChild<SocketCanSession*>()->service();
        addPosition(window);
        model(window)->addPgn(2, "Fixture", "HDT", true, {{"Heading", "1", "rad", "double"}}, {}, 250);
        QCOMPARE(start(window)->isEnabled(), canReady || serialReady);
        if (!start(window)->isEnabled()) {
            QVERIFY(!worker(window)->isSending()); QVERIFY(!stop(window)->isEnabled()); return;
        }
        start(window)->trigger(); worker(window)->sendNmeaMessage();
        QCOMPARE(backend.submissions > 0, canReady); QCOMPARE(!port->output.isEmpty(), serialReady);
        QVERIFY(start(window)->isChecked());
        if (!serialReady) QVERIFY(status(window).contains("NMEA0183 unavailable"));
        if (!canReady) QVERIFY(status(window).contains("NMEA2000 (SocketCan) unavailable"));
        stop(window)->trigger(); auto bytes = port->output; int submissions = backend.submissions;
        QTest::qWait(300); QCOMPARE(port->output, bytes); QCOMPARE(backend.submissions, submissions);
        QVERIFY(!worker(window)->isSending()); QVERIFY(!start(window)->isChecked());
    }
    void independentFailures_data()
    {
        QTest::addColumn<bool>("failCan");
        QTest::newRow("CAN failure preserves serial") << true;
        QTest::newRow("serial failure preserves CAN") << false;
    }
    void independentFailures()
    {
        QFETCH(bool, failCan);
        canSettings(); serialSettings(); SendingCanBackend backend; backend.available = true;
        auto* port = new SendingMemoryPort;
        MainWindow window(nullptr, nullptr, port, &backend);
        window.findChild<SocketCanSession*>()->service(); addPosition(window);
        model(window)->addPgn(2, "Fixture", "HDT", true, {{"Heading", "1", "rad", "double"}}, {}, 250);
        start(window)->trigger(); worker(window)->sendNmeaMessage();
        if (failCan) { backend.accept = false; QTRY_VERIFY_WITH_TIMEOUT(status(window).contains("transmission failed"), 1500); }
        else emit port->errorOccurred(QSerialPort::ResourceError);
        QVERIFY(worker(window)->isSending()); QVERIFY(start(window)->isChecked()); QVERIFY(stop(window)->isEnabled());
        QVERIFY(status(window).contains(failCan ? "NMEA2000 (SocketCan) unavailable" : "NMEA0183 unavailable"));
        auto bytes = port->output; int submissions = backend.submissions;
        if (failCan) {
            QTRY_VERIFY_WITH_TIMEOUT(port->output.size() > bytes.size(), 1500);
            QCOMPARE(backend.submissions, submissions);
        } else {
            QTRY_VERIFY_WITH_TIMEOUT(backend.submissions > submissions, 1500);
            QCOMPARE(port->output, bytes);
        }
        stop(window)->trigger(); QVERIFY(!worker(window)->isSending());
    }
    void signalKConfigurationStatus_data()
    {
        QTest::addColumn<QString>("serverUrl"); QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("token"); QTest::addColumn<QString>("expected");
        QTest::newRow("configured") << "http://127.0.0.1:3000" << "fixture" << "test-token" << "ready to connect";
        QTest::newRow("missing server") << "" << "fixture" << "test-token" << "invalid server origin";
        QTest::newRow("missing identity") << "http://127.0.0.1:3000" << "" << "test-token" << "invalid publisher identity";
        QTest::newRow("missing credentials") << "http://127.0.0.1:3000" << "fixture" << "" << "missing credentials";
        QTest::newRow("invalid identity") << "http://127.0.0.1:3000" << "bad identity!" << "test-token" << "invalid publisher identity";
    }
    void signalKConfigurationStatus()
    {
        QFETCH(QString, serverUrl); QFETCH(QString, source); QFETCH(QString, token); QFETCH(QString, expected);
        const auto cleanup = qScopeGuard([] { CredentialStore::remove("SignalKToken"); });
        if (!token.isEmpty()) QVERIFY(CredentialStore::write("SignalKToken", token));
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Nmea2000/Nmea2000Enabled", true); settings.setValue("Nmea2000/InterfaceType", "SignalK");
        settings.setValue("Nmea2000/SignalKServer", serverUrl); settings.setValue("Nmea2000/SignalKSource", source);
        settings.setValue("Nmea0183/Nmea0183Enabled", true); settings.setValue("Nmea0183/Nmea0183Port", "None"); settings.sync();
        auto* port = new SendingMemoryPort; port->failOpen = true; MainWindow window(nullptr, nullptr, port);
        auto* label = window.findChild<QLabel*>("sendingStatus");
        QVERIFY(label->text().contains(expected)); QVERIFY(label->toolTip().contains(expected));
        QVERIFY(label->accessibleDescription().contains(expected)); QVERIFY(label->text().contains("NMEA0183: unavailable"));
        QVERIFY(start(window)->isEnabled()); QVERIFY(!start(window)->isChecked()); QVERIFY(!stop(window)->isEnabled());
        QVERIFY(!worker(window)->isSending()); QVERIFY(!window.findChild<SignalKPublisher*>()->isActive());
        QVERIFY(!label->wordWrap()); QVERIFY(!label->text().contains("test-token"));
    }
    void signalKStatusTransitions()
    {
        TestServer server; server.sendHello = false; signalKSettings(server);
        const auto cleanup = qScopeGuard([] { CredentialStore::remove("SignalKToken"); });
        QVERIFY(CredentialStore::write("SignalKToken", "test-token"));
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Nmea0183/Nmea0183Enabled", true); settings.setValue("Nmea0183/Nmea0183Port", "None"); settings.sync();
        auto* port = new SendingMemoryPort; port->failOpen = true; MainWindow window(nullptr, nullptr, port);
        auto* label = window.findChild<QLabel*>("sendingStatus"); auto* publisher = window.findChild<SignalKPublisher*>();
        window.resize(1100, 700); window.show(); QTest::qWait(20);
        QVERIFY(label->text().contains("ready to connect")); QVERIFY(start(window)->isEnabled());
        QVERIFY(!label->text().contains("SignalK): unavailable")); QVERIFY(server.deltas.isEmpty());
        capture(window, "status-ready-to-connect"); addPosition(window); start(window)->trigger();
        QVERIFY(label->text().contains("authenticating")); QVERIFY(!start(window)->isChecked());
        QTRY_VERIFY_WITH_TIMEOUT(server.peer, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(label->text().contains("connecting (waiting for hello)"), 1500);
        QVERIFY(server.deltas.isEmpty());
        capture(window, "status-connecting");
        server.peer->sendTextMessage("{\"self\":\"vessels.test\",\"version\":\"2.11.0\"}");
        QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(), 1500);
        QVERIFY(label->text().contains("Sending: NMEA2000 (SignalK)")); QVERIFY(start(window)->isChecked());
        QTRY_VERIFY_WITH_TIMEOUT(!server.deltas.isEmpty(), 1500); capture(window, "status-sending");
        server.writer = false; server.peer->abort();
        QTRY_VERIFY_WITH_TIMEOUT(label->text().contains("recovering (connection/authorization error)"), 1500);
        QVERIFY(!start(window)->isChecked()); QVERIFY(stop(window)->isEnabled());
        QVERIFY(label->toolTip().contains("unauthorized")); capture(window, "status-recovering");
        stop(window)->trigger(); QVERIFY(!publisher->isActive()); QVERIFY(!stop(window)->isEnabled());
        QVERIFY(label->text().contains("Stopped")); QVERIFY(label->text().contains("ready to connect"));
        QVERIFY(label->text().contains("NMEA0183: unavailable"));
        capture(window, "status-stopped"); const int count = server.deltas.size();
        QTest::qWait(2200); QCOMPARE(server.deltas.size(), count); QVERIFY(!publisher->isActive());
    }
    void signalKWithIndependentSerial_data()
    {
        QTest::addColumn<bool>("serialReady");
        QTest::newRow("owner configuration serial None") << false;
        QTest::newRow("simultaneous independent outputs") << true;
    }
    void signalKWithIndependentSerial()
    {
        QFETCH(bool, serialReady);
        TestServer server; server.sendHello = false; signalKSettings(server);
        serialSettings();
        if (!serialReady) {
            AppSettings settings(APP_COMPANY, APP_NAME);
            settings.setValue("Nmea0183/Nmea0183Port", "None"); settings.sync();
        }
        auto* port = new SendingMemoryPort; port->failOpen = !serialReady;
        MainWindow window(nullptr, nullptr, port);
        auto* publisher = window.findChild<SignalKPublisher*>(); publisher->configure(server.config());
        addPosition(window);
        model(window)->addPgn(2, "Fixture", "HDT", true, {{"Heading", "1", "rad", "double"}}, {}, 250);
        QVERIFY(start(window)->isEnabled()); start(window)->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(server.peer, 3000);
        QCOMPARE(worker(window)->isSending(), serialReady); QCOMPARE(start(window)->isChecked(), serialReady);
        QTRY_VERIFY_WITH_TIMEOUT(status(window).contains("connecting (waiting for hello)"), 1500);
        QVERIFY(stop(window)->isEnabled());
        QTest::qWait(300); QVERIFY(server.deltas.isEmpty()); QCOMPARE(!port->output.isEmpty(), serialReady);
        server.peer->sendTextMessage("{\"self\":\"vessels.test\",\"version\":\"2.11.0\"}");
        QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(), 1500);
        QTRY_VERIFY_WITH_TIMEOUT(!server.deltas.isEmpty(), 1500);
        QVERIFY(start(window)->isChecked()); QVERIFY(status(window).contains("NMEA2000 (SignalK)"));
        if (!serialReady) QVERIFY(status(window).contains("NMEA0183 unavailable"));
        server.writer = false; server.peer->abort();
        QTRY_VERIFY_WITH_TIMEOUT(!publisher->isReady(), 1500);
        QCOMPARE(start(window)->isChecked(), serialReady); QCOMPARE(worker(window)->isSending(), serialReady);
        const auto bytes = port->output; const int count = server.deltas.size();
        QTest::qWait(350); QCOMPARE(server.deltas.size(), count);
        // Observe progress across normal 250 ms scheduler ticks without assuming
        // a particular event-loop phase at the 350 ms observation boundary.
        if (serialReady) { QTRY_VERIFY_WITH_TIMEOUT(port->output.size() > bytes.size(), 1500); }
        server.writer = true; server.sendHello = true;
        QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(server.deltas.size() > count, 1500);
        QVERIFY(start(window)->isChecked());
        stop(window)->trigger(); QVERIFY(!publisher->isActive()); QVERIFY(!worker(window)->isSending());
        QVERIFY(!start(window)->isChecked()); QVERIFY(!stop(window)->isEnabled());
        auto stopped = port->output; int stoppedCount = server.deltas.size();
        QTest::qWait(400); QCOMPARE(port->output, stopped); QCOMPARE(server.deltas.size(), stoppedCount);
        // Reload and Viewer must cancel the mixed request too.
        start(window)->trigger(); window.HotReloadSettings(); QVERIFY(!publisher->isActive());
        QVERIFY(!worker(window)->isSending());
        publisher->configure(server.config()); start(window)->trigger();
        window.findChild<QAction*>("actionViewer_Rx")->setChecked(true);
        QVERIFY(!publisher->isActive()); QVERIFY(!worker(window)->isSending());
        window.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);
        QVERIFY(!start(window)->isChecked()); QVERIFY(!publisher->isActive());
    }
    void signalKPendingCancelAndReady()
    {
        TestServer server; server.sendHello = false; signalKSettings(server);
        MainWindow window; window.show();
        auto* publisher = window.findChild<SignalKPublisher*>(); publisher->configure(server.config());
        QVERIFY(!publisher->isActive()); QVERIFY(server.deltas.isEmpty());
        QVERIFY(status(window).contains("connect and verify")); capture(window, "disabled-signalk");
        addPosition(window); start(window)->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(server.peer, 3000);
        QVERIFY(!worker(window)->isSending()); QVERIFY(!start(window)->isChecked());
        QVERIFY(!start(window)->isEnabled()); QVERIFY(stop(window)->isEnabled());
        QVERIFY(server.deltas.isEmpty()); capture(window, "connecting-signalk");
        stop(window)->trigger(); QVERIFY(!publisher->isActive());
        QVERIFY(!start(window)->isChecked()); QVERIFY(start(window)->isEnabled());
        server.sendHello = true; start(window)->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(), 3000);
        QVERIFY(worker(window)->isSending()); QVERIFY(start(window)->isChecked());
        QTRY_VERIFY_WITH_TIMEOUT(!server.deltas.isEmpty(), 1500);
        capture(window, "sending-signalk"); stop(window)->trigger();
        const int count = server.deltas.size(); QTest::qWait(400); QCOMPARE(server.deltas.size(), count);
        window.HotReloadSettings(); QVERIFY(!worker(window)->isSending()); QVERIFY(!publisher->isActive());
    }
    void signalKAuthorizationErrorRetryAndStop()
    {
        TestServer server; server.writer = false; signalKSettings(server);
        MainWindow window;
        auto* publisher = window.findChild<SignalKPublisher*>(); publisher->configure(server.config());
        addPosition(window); start(window)->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(status(window).contains("unauthorized"), 3000);
        QVERIFY(!start(window)->isChecked()); QVERIFY(!worker(window)->isSending());
        QVERIFY(stop(window)->isEnabled()); QVERIFY(server.deltas.isEmpty());
        server.writer = true; QTRY_VERIFY_WITH_TIMEOUT(publisher->isReady(), 5000);
        QVERIFY(start(window)->isChecked()); QTRY_VERIFY_WITH_TIMEOUT(!server.deltas.isEmpty(), 1500);
        server.writer = false; server.peer->abort();
        QTRY_VERIFY_WITH_TIMEOUT(!worker(window)->isSending(), 1500);
        QVERIFY(!start(window)->isChecked()); QVERIFY(stop(window)->isEnabled());
        stop(window)->trigger(); const int count = server.deltas.size(); server.writer = true;
        QTest::qWait(2400); QVERIFY(!publisher->isReady()); QVERIFY(!worker(window)->isSending());
        QCOMPARE(server.deltas.size(), count);
    }
    void destroyDuringPendingSignalKConnection()
    {
        TestServer server; server.sendHello = false; signalKSettings(server);
        QSignalSpy connections(&server.ws, &QWebSocketServer::newConnection);
        auto window = std::make_unique<MainWindow>();
        window->findChild<SignalKPublisher*>()->configure(server.config());
        QPointer<SignalKPublisher> publisher = window->findChild<SignalKPublisher*>();
        QPointer<NmeaSenderWorker> scheduler = worker(*window);
        start(*window)->trigger(); QTRY_VERIFY_WITH_TIMEOUT(server.peer, 3000);
        QVERIFY(!scheduler->isSending()); window.reset();
        QVERIFY(publisher.isNull()); QVERIFY(scheduler.isNull());
        const int count = connections.count(); QTest::qWait(2200);
        QCOMPARE(connections.count(), count); QVERIFY(server.deltas.isEmpty());
    }
    void diagnosticsBuildPolicy()
    {
        canSettings(); SendingCanBackend backend; backend.available = true;
        MainWindow window(nullptr, nullptr, nullptr, &backend);
        window.findChild<SocketCanSession*>()->service();
        auto* debug = window.findChild<QMenu*>("menuTest");
        auto* diagnostic = window.findChild<QAction*>("actionSend_Test_129026");
        QVERIFY(!diagnostic->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&window, "on_actionSend_Test_129026_triggered"));
        QCOMPARE(backend.submissions, 0);
        start(window)->trigger(); QVERIFY(worker(window)->isSending());
#ifdef QT_DEBUG
        QVERIFY(debug->menuAction()->isVisible()); QVERIFY(diagnostic->isEnabled());
        diagnostic->trigger(); QCOMPARE(backend.submissions, 1);
#else
        QVERIFY(!debug->menuAction()->isVisible()); QVERIFY(!diagnostic->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&window, "on_actionSend_Test_129026_triggered"));
        QCOMPARE(backend.submissions, 0);
#endif
        stop(window)->trigger(); QVERIFY(!diagnostic->isEnabled());
    }
    void invalidSignalKDoesNotBlockSerial()
    {
        TestServer server; signalKSettings(server); serialSettings();
        auto* port = new SendingMemoryPort; MainWindow window(nullptr, nullptr, port);
        window.findChild<SignalKPublisher*>()->configure({"not-a-server", "", "fixture"});
        model(window)->addPgn(1, "Fixture", "HDT", true, {{"Heading", "1", "rad", "double"}}, {}, 250);
        start(window)->trigger(); QVERIFY(start(window)->isChecked()); QVERIFY(worker(window)->isSending());
        QVERIFY(status(window).contains("http(s)")); QVERIFY(status(window).contains("NMEA0183"));
        QTRY_VERIFY_WITH_TIMEOUT(!port->output.isEmpty(), 1500);
        stop(window)->trigger(); QVERIFY(!worker(window)->isSending());
    }
    void invalidSignalKRequestDoesNotActivate()
    {
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Nmea2000/Nmea2000Enabled", true);
        settings.setValue("Nmea2000/InterfaceType", "SignalK"); settings.sync();
        MainWindow window;
        window.findChild<SignalKPublisher*>()->configure({"not-a-server", "", "fixture"});
        start(window)->trigger();
        QVERIFY(!worker(window)->isSending()); QVERIFY(!start(window)->isChecked());
        QVERIFY(!stop(window)->isEnabled()); QVERIFY(status(window).contains("http(s)"));
    }
    void standaloneCannotAuthorizeAnyTransport()
    {
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {"--standalone-child", "--no-nmea"});
        QVERIFY(child.waitForFinished(15000));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit); QCOMPARE(child.exitCode(), 0);
        QVERIFY(child.readAllStandardOutput().contains("Standalone gate PASS"));
    }
    void actisenseReadinessWithoutTransmission()
    {
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Nmea2000/Nmea2000Enabled", true);
        settings.setValue("Nmea2000/InterfaceType", "Actisense");
        settings.setValue("Nmea2000/ActisensePort", "MEMORY"); settings.sync();
        auto* port = new SendingMemoryPort;
        MainWindow window(nullptr, port);
        QCOMPARE(start(window)->isEnabled(), TransportCapabilities::experimentalActisenseTx);
        start(window)->setChecked(true);
        QCOMPARE(start(window)->isChecked(), TransportCapabilities::experimentalActisenseTx);
        QVERIFY(status(window).contains(TransportCapabilities::experimentalActisenseTx ? "Issue #1" : "receive-only"));
        QVERIFY(port->output.isEmpty());
        emit port->errorOccurred(QSerialPort::ResourceError);
        QVERIFY(!start(window)->isChecked()); QVERIFY(!worker(window)->isSending());
        QVERIFY(port->output.isEmpty()); QVERIFY(!start(window)->isEnabled());
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv); app.setStyle("Fusion");
    QTemporaryDir profile(QCoreApplication::applicationDirPath() + "/m211-profile-XXXXXX");
    if (!profile.isValid()) return 2;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    if (app.arguments().contains("--standalone-child")) {
        serialSettings(); canSettings(); SendingCanBackend backend; backend.available = true;
        auto* port = new SendingMemoryPort;
        MainWindow window(nullptr, nullptr, port, &backend);
        start(window)->setChecked(true);
        QMetaObject::invokeMethod(&window, "on_actionSend_Test_129026_triggered");
        if (start(window)->isEnabled() || start(window)->isChecked() || worker(window)->isSending()
            || backend.submissions || !port->output.isEmpty() || !status(window).contains("standalone")) return 3;
        fprintf(stdout, "Standalone gate PASS\n");
        return 0;
    }
    SendingControlsTests tests;
    const int result=QTest::qExec(&tests, argc, argv);
    // Remove only this disposable profile's test-namespaced credential.
    CredentialStore::remove("SignalKToken");
    return result;
}
#include "sending_controls_tests.moc"
