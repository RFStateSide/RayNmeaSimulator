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

#include "aboutdialog.h"
#include "updatedialog.h"
#include "helpdialog.h"
#include "aboutlinks.h"
#include "applicationlog.h"
#include "logviewer.h"
#include "nmea2000diagnosticsdialog.h"
#include "nmeadevicediscovery.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include "chartplotterroutedialog.h"
#include "engineprofiledialog.h"
#include "runtimeresources.h"
#include "transportcapabilities.h"
#include "windowstate.h"
#include "themes.h"
#include "desktopicons.h"
#include <QMenu>
#include <QKeyEvent>
#include <QToolTip>
#include <QToolButton>
#include <QGridLayout>
#include <QToolBar>
#include <QScopedValueRollback>
#include <QLabel>
#include "mainwindow.h"
#include "apppaths.h"
#include <QAction>
#include <QActionGroup>
#include <QColor>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include "appsettings.h"
#include "credentialstore.h"
#include "preferences.h"
#include <QShowEvent>
#include <QTableView>
#include <QTextStream>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include "datastructs.h"
#include "enumcomboboxdelegate.h"
#include "helper.h"
#include "ui_mainwindow.h"
#include "uihelper.h"

namespace {
// QWidget::isAncestorOf stops at top-level window boundaries. Contextual Help
// follows ownership across dialogs, including their own modal child notices.
bool hasWidgetOwner(const QWidget *widget, const QWidget *owner)
{
    for (auto *parent = widget; parent; parent = parent->parentWidget())
        if (parent == owner) return true;
    return false;
}
// Independent controls glyph: three adjustment bars, never traced from a bitmap.
QIcon originalSettingsIcon(const QColor &color)
{
    QIcon icon;
    for (int size : {16,24,32,48,64,128,256}) {
        QPixmap image(size,size); image.fill(Qt::transparent);
        QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size/64.0,size/64.0);
        painter.setPen(QPen(color,4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        for (const auto &control : {QPointF(20,16),QPointF(44,32),QPointF(30,48)}) {
            painter.drawLine(QPointF(10,control.y()),QPointF(control.x()-7,control.y()));
            painter.drawLine(QPointF(control.x()+7,control.y()),QPointF(54,control.y()));
            painter.drawEllipse(control,6,6);
        }
        painter.end(); icon.addPixmap(image);
    }
    return icon;
}
// A shrinking, single-line status must not dictate the toolbar/status bar height or
// minimum window width. Keep the complete explanation in the tooltip/accessibility.
class SendingStatusLabel final : public QLabel {
public:
    using QLabel::QLabel;
    QSize sizeHint() const override {
        return {alignment().testFlag(Qt::AlignRight) ? fontMetrics().horizontalAdvance(text()) : 240,
                fontMetrics().height()};
    }
    QSize minimumSizeHint() const override { return {0, fontMetrics().height()}; }
    bool hasHeightForWidth() const override { return false; }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
        painter.drawText(contentsRect(), alignment() | Qt::AlignVCenter | Qt::TextSingleLine,
                         fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()));
    }
};
}

MainWindow::MainWindow(QWidget *parent, QSerialPort *actisensePort, QSerialPort *serial0183Port, SocketCanBackend *canBackend,
                       WaveshareSession::PortFactory waveshareFactory)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , nmea2000Handler(new Nmea2000Handler(this, actisensePort, canBackend, std::move(waveshareFactory)))
    , nmea0183Handler(new Nmea0183Handler(this, serial0183Port))
{
    // Qt-owned choosers share our STOP event path; native OS dialog loops do not.
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    Themes::initializeSystemStyle();
    Themes::initializeEditableResources();
    AppPaths::initializeRouteExamples();
    ui->setupUi(this);
    auto *view = new QMenu(tr("&View"), this);
    view->setObjectName("menuView");
    ui->menubar->insertMenu(ui->menuTest->menuAction(), view);
    view->addAction(ui->dockWidgetTable->toggleViewAction());
    view->addAction(ui->dockWidgetTree->toggleViewAction());
    // The diagnostic action has never performed an operation. Keep it visible
    // but clearly unavailable; operational test actions retain their wiring.
    ui->actionPrint_Tree_Model->setEnabled(false);
    ui->actionPrint_Tree_Model->setToolTip(tr("Tree-model logging is not implemented."));

    ui->actionOpen_Setup_Form->setToolTip(tr("Open modeless Setup. Changes remain drafts until Save; Stop Sending before Save or Re-load."));
    auto *modes = new QActionGroup(this);
    modes->setObjectName("modeActions");
    modes->setExclusive(true);
    modes->addAction(ui->actionSimulator_Tx);
    modes->addAction(ui->actionViewer_Rx);
    synchronizeModeActions();
    const auto generateTip = tr("Create and simulate instrument data with optional generators. Transmission requires Enable Sending.");
    const auto viewerTip = tr("Receive supported NMEA 2000 and NMEA 0183 data. Compatible instruments are added automatically. Instrument Sending is disabled. Generators are suspended; bounded route requests need separate authorization.");
    ui->actionSimulator_Tx->setToolTip(generateTip);
    ui->actionSimulator_Tx->setStatusTip(generateTip);
    ui->actionViewer_Rx->setToolTip(viewerTip);
    ui->actionViewer_Rx->setStatusTip(viewerTip);
    ui->actionLoad->setToolTip(tr("Replace the current Layout after confirmation. Sending stops; a loaded Layout never enables Sending."));
    ui->actionSave->setToolTip(tr("Atomically save widget configuration and parameters as a Layout. This does not authorize Sending."));
    ui->actionSave_Layout_as_Default->setToolTip(tr("Replace the user Default with the current Layout. Existing Default is preserved if validation or saving fails."));
    ui->actionLoad_Default->setToolTip(tr("Load the user Default when available, otherwise the bundled Default. Replacing a Layout stops Sending."));
    ui->actionClear->setToolTip(tr("Choose a group and confirm OK to remove its widgets. Opening the dialog does not remove anything."));
    ui->actionClear_All->setToolTip(tr("Remove all Layout widgets after confirmation. Stop Sending first or use F12 in the confirmation."));
    ui->actionRemove->setToolTip(tr("Select a saved Layout file to delete after confirmation. This does not remove the current live widgets."));
    ui->treeView->setToolTip(tr("Read-only Layout navigation. Select a tab, widget or property to locate the live widget; values use preferred units. Last Updated is the local model update time."));
    ui->tableView->setToolTip(tr("Properties of the selected live widget. Commit a valid edit to update its model; unfinished edits are protected during selection changes."));
    auto *helpMenu = ui->menubar->addMenu(tr("&Help"));
    helpMenu->setObjectName("menuHelp");
    auto *helpAction = helpMenu->addAction(tr("&Help"));
    helpAction->setObjectName("actionHelpContents");
    auto helpKeys = QKeySequence::keyBindings(QKeySequence::HelpContents);
    if (!helpKeys.contains(QKeySequence(Qt::Key_F1))) helpKeys.append(QKeySequence(Qt::Key_F1));
    helpAction->setShortcuts(helpKeys);
    helpAction->setShortcutContext(Qt::ApplicationShortcut);
    helpAction->setStatusTip(tr("Open the offline Help manual. F1 follows the current workspace context."));
    connect(helpAction, &QAction::triggered, this, [this] { showContextHelp(); });
    helpMenu->addSeparator();
    auto *updatesAction = helpMenu->addAction(tr("Check for Updates..."));
    updatesAction->setObjectName("actionCheckForUpdates");
    connect(updatesAction, &QAction::triggered, this, [this] {
        auto *dialog=findChild<UpdateDialog*>();
        if(!dialog) { dialog=new UpdateDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->show(); dialog->startCheck(); }
        else { dialog->show(); dialog->raise(); dialog->activateWindow(); }
    });
    auto *aboutAction = helpMenu->addAction(tr("&About…"));
    aboutAction->setObjectName("actionAbout");
    connect(aboutAction, &QAction::triggered, this, [this] {
        AboutDialog dialog(this); dialog.exec();
    });
    auto *supportAction = helpMenu->addAction(tr("Support…"));
    supportAction->setObjectName("actionSupportRayNmeaSimulator");
    supportAction->setStatusTip(tr("Open the voluntary PayPal support page in your web browser."));
    supportAction->setToolTip(tr("Voluntary support does not unlock features, technical support, warranties or other services."));
    connect(supportAction, &QAction::triggered, this, [this] {
        AboutLinks::open(this, QUrl(QString::fromLatin1(AboutLinks::SupportUrl)));
    });
    dialogSetup = new DialogSetup(this);
    dialogSetup->setProperty("rayHelpTopic", "connection-troubleshooting");
    ui->treeView->setProperty("rayHelpTopic", "object-tree");
    ui->tableView->setProperty("rayHelpTopic", "property-editor");
    auto *tools = new QMenu(tr("&Tools"), this);
    tools->setObjectName("menuTools");
    auto *logAction=tools->addAction(tr("Application Log…"));
    connect(logAction,&QAction::triggered,this,[this]{
        if(auto *existing=findChild<LogViewer*>()){existing->show();existing->raise();existing->activateWindow();return;}
        auto *viewer=new LogViewer(this);viewer->setAttribute(Qt::WA_DeleteOnClose);viewer->show();
    });
    auto *diagnosticsAction=tools->addAction(tr("NMEA2000 Diagnostics..."));
    diagnosticsAction->setObjectName("actionNmea2000Diagnostics");
    connect(diagnosticsAction,&QAction::triggered,this,[this,logAction]{
        if(auto *existing=findChild<Nmea2000DiagnosticsDialog*>()){existing->show();existing->raise();existing->activateWindow();return;}
        auto *dialog=new Nmea2000DiagnosticsDialog(nmea2000Handler->diagnostics(),this,nmea2000Handler->deviceDiscovery());
        connect(dialog,&Nmea2000DiagnosticsDialog::discoveryRequested,this,[this,dialog]{
            if(sendingRequested || nmeaSenderWorker->isSending() || stoppingSending) {
                dialog->discoveryFeedback(tr("Stop instrument Sending before Discover Devices. Discovery never stops or starts instrument Sending for you."));return;
            }
            nmea2000Handler->deviceDiscovery()->start();
        });
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog,&Nmea2000DiagnosticsDialog::openApplicationLog,logAction,&QAction::trigger);
        dialog->show();
    });
    auto *logHealth=new QTimer(this);logHealth->setInterval(1000);
    connect(logHealth,&QTimer::timeout,this,[this,last=QString() ]() mutable {
        const auto health=ApplicationLog::health(false);if(health==last)return;last=health;
        if(health.startsWith("Failed"))statusBar()->showMessage(tr("Application logging unavailable: ")+health,15000);
    });logHealth->start();

    ui->menubar->insertMenu(view->menuAction(), tools);
    auto *profilesAction = tools->addAction(tr("Engine &Profiles…"));
    profilesAction->setObjectName("actionEngineProfiles");
    profilesAction->setToolTip(tr("Open modeless profile management. Unsaved edits do not change simulations; stop simulated engines before Save or Restore."));
    connect(profilesAction, &QAction::triggered, this, [this] {
        if (auto *existing = findChild<EngineProfileDialog *>(); existing && existing->isVisible()) {
            if (existing->isMinimized()) existing->showNormal(); else existing->show();
            existing->raise(); existing->activateWindow(); return;
        }
        auto *editor = new EngineProfileDialog([this] {
            QStringList running;
            for (auto *engine : findChildren<GeneratorEngine *>())
                if (engine->EngineRunning())
                    running.append(tr("%1 (engine instance %2, profile '%3')")
                        .arg(engine->Label()).arg(engine->EngineInstance()).arg(engine->engineProfile()));
            return running;
        }, this);
        connect(editor, &EngineProfileDialog::catalogSaved, this, [this] {
            dialogSetup->refreshEngineProfiles();
            for (auto *engine : findChildren<GeneratorEngine *>()) engine->reloadEngineProfiles();
        });
        editor->show();
    });

    pgnTableController = new PgnTableController(ui->tableView, this);
    PgnTreeController *pgnTreeController = new PgnTreeController(ui->treeView, this);

    // Apply the EnumComboBoxDelegate for the value column (column 1)
    EnumComboBoxDelegate *enumComboBoxDelegate = new EnumComboBoxDelegate(this);
    ui->tableView->setItemDelegateForColumn(1, enumComboBoxDelegate);

    // Persistent GUI-thread scheduler: the model and handlers have the same affinity.
    nmeaSenderWorker = new NmeaSenderWorker(this);
    nmeaSenderWorker->setPgnTableController(pgnTableController);

    // Handle the NMEA messages sent by the worker
    connect(nmeaSenderWorker, &NmeaSenderWorker::sendPgn, this, [this](int pgn, const QStringList &parameters, const QHash<QString,QDateTime> &sampleTimes) {
        if (sendingRequested && nmeaSenderWorker->isSending() && nmea2000SendingReady())
            nmea2000Handler->sendPgn(pgn, parameters, sampleTimes);
    });
    connect(nmeaSenderWorker, &NmeaSenderWorker::sendSnt, this, [this](const QString &sentence, const QStringList &parameters) {
        if (sendingRequested && nmeaSenderWorker->isSending() && nmea0183SendingReady())
            nmea0183Handler->sendSnt(sentence, parameters);
    });

    mainController = new MainController(pgnTableController, pgnTreeController, ui->mdiAreaMain, nmeaSenderWorker, this);
    mainController->activeRouteChooser=[this](QWidget *parent){
        if(sendingRequested||nmeaSenderWorker->isSending()||nmea2000Handler->deviceDiscovery()->busy()){QMessageBox::information(parent,tr("Route Service"),tr("Stop instrument Sending and finish or cancel device discovery before opening the route-service session."));return ReceivedRoutes::Route{};}
        QVector<ReceivedRoutes::Route> passive;for(const auto &r:mainController->receivedRoutes.collection())if(r.complete)passive.append(r);
        QPointer<QWidget> origin(parent);
        ChartplotterRouteDialog dialog(nmea2000Handler,passive,this);
        dialog.passiveStillValid=[this](const ReceivedRoutes::Route &snapshot){
            for(const auto &r:mainController->receivedRoutes.collection()){
                if(!r.complete||r.id!=snapshot.id||r.attributes!=snapshot.attributes||r.waypoints.size()!=snapshot.waypoints.size())continue;
                bool same=true;for(int i=0;i<r.waypoints.size();++i)if(r.waypoints[i].coordinate!=snapshot.waypoints[i].coordinate||r.waypoints[i].name!=snapshot.waypoints[i].name||r.waypoints[i].speedMs!=snapshot.waypoints[i].speedMs)same=false;
                if(same)return true;
            }return false;
        };
        if(parent)connect(parent,&QObject::destroyed,&dialog,&QDialog::reject);
        if(dialog.exec()==QDialog::Accepted && origin)return dialog.selection();return ReceivedRoutes::Route{};
    };
    pgnTreeController->bindLayout(mainController, pgnTableController, ui->mdiAreaMain, ui->tableView);

    nmeaReceiver = new NmeaReceiver(mainController, pgnTableController, nmea2000Handler, nmea0183Handler, this);

    sendingStatus = new SendingStatusLabel(this);
    sendingStatus->setObjectName("sendingStatus");
    sendingStatus->setWordWrap(false);
    sendingStatus->setMinimumWidth(0);
    sendingStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->toolBarSending->setIconSize(QSize(16, 16));
    ui->toolBarSending->setStyleSheet("QToolBar#toolBarSending { spacing: 4px; padding: 1px 2px; }");
    sendingStatus->setTextFormat(Qt::PlainText);
    ui->toolBarSending->addSeparator();
    ui->toolBarSending->addWidget(sendingStatus);
    // This toolbar is the primary operational control, not an optional panel.
    ui->toolBarSending->toggleViewAction()->setVisible(false);
    ui->actionStart_Sending->setToolTip(tr("Authorize external output from enabled instruments in Generate mode. Startup, Layout loading and settings reload never start Sending."));
    ui->actionLoad->setToolTip(tr("Replace the working Layout with a saved snapshot. Loading never authorizes Sending."));
    ui->actionSave->setToolTip(tr("Save the current instrument values and configuration as a Layout snapshot."));
    ui->actionLoad_Default->setToolTip(tr("Load the saved Default Layout or bundled fallback. Sending remains stopped."));
    ui->actionStop_Sending->setShortcut(QKeySequence(Qt::Key_F12));
    ui->actionStop_Sending->setToolTip(tr("Stop external Sending and pending connection retries (F12). Route requests are cancelled; local engines and movement keep running."));
    dialogSetup->setSendingControls(ui->actionStop_Sending, [this] {
        return sendingRequested || nmeaSenderWorker->isSending();
    });
#ifndef QT_DEBUG
    ui->menuTest->menuAction()->setVisible(false);
#endif
    auto *transportStatus = new SendingStatusLabel(this);
    transportStatus->setObjectName("transportStatus");
    transportStatus->setWordWrap(false);
    transportStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    transportStatus->setMinimumWidth(0);
    transportStatus->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    transportStatus->setTextFormat(Qt::PlainText);
    ui->statusbar->addPermanentWidget(transportStatus);
    connect(nmea2000Handler, &Nmea2000Handler::SocketCanStatus, this, [this](const QString &) {
        updateSendingAvailability();
    });
    connect(nmea2000Handler, &Nmea2000Handler::SignalKStatus, this, [this](const QString &) {
        updateSendingAvailability();
    });
    connect(nmea2000Handler, &Nmea2000Handler::WaveshareStatus, this, [this](const QString &) {
        const auto *session = nmea2000Handler->findChild<WaveshareSession *>();
        dialogSetup->setSerialCanStatus(session && session->protocol() == WaveshareSession::CanableSlcan, session->status());
        updateSendingAvailability();
    });
    connect(nmea2000Handler->networkCan(), &NetworkCanSession::statusChanged,
            dialogSetup, &DialogSetup::setNetworkCanStatus);
    dialogSetup->setNetworkCanStatus(nmea2000Handler->networkCan()->status());
    connect(dialogSetup, &DialogSetup::ConnectNetworkCanDevice, this, [this] {
        if (sendingRequested || nmeaSenderWorker->isSending() || nmea2000Handler->routeServiceActive()) {
            dialogSetup->setNetworkCanStatus(nmea2000Handler->networkCan()->status() +
                tr("\nStop Sending or finish/cancel the route-service operation before reconnecting."));
            return;
        }
        if (isNmea2000Enabled && nmea2000InterfaceType == "Network NMEA2000")
            nmea2000Handler->networkCan()->configure(networkCanConfig);
        else
            dialogSetup->setNetworkCanStatus(tr("Network interface is disabled or not selected. Save and reload Network NMEA2000 Setup first. TX is not authorized."));
    });
    connect(dialogSetup, &DialogSetup::ConnectSerialCanDevice, this, [this] {
        if (sendingRequested || nmeaSenderWorker->isSending()) {
            ShowStatusBarMessage(tr("Stop Sending before connecting a device. Use Stop Sending or F12."));
            return;
        }
        if (isNmea2000Enabled) {
            sendingError2000.clear();
            nmea2000Handler->ConnectSerialCanDevice();
        }
        else ShowStatusBarMessage(tr("Enable and Save the selected NMEA2000 interface before connecting."));
    });
    connect(nmea2000Handler, &Nmea2000Handler::InterfaceStateChanged, this, &MainWindow::updateSendingAvailability);
    connect(nmea0183Handler, &Nmea0183Handler::InterfaceStateChanged, this, &MainWindow::updateSendingAvailability);
    connect(nmea2000Handler, &Nmea2000Handler::TransmitError, this, [this](const QString &text) {
        if (isNmea2000Enabled) sendingFailed(text);
        // Preserve actionable serial error detail after the failure lifecycle closes the session.
        if (text.startsWith("Waveshare") || text.startsWith("CANable SLCAN"))
            dialogSetup->setSerialCanStatus(text.startsWith("CANable SLCAN"), text);
    });
    connect(nmea0183Handler, &Nmea0183Handler::TransmitError, this, [this](const QString &text) {
        if (isNmea0183Enabled) sendingFailed(text, false);
    });
    connect(nmeaSenderWorker, &NmeaSenderWorker::finished, this, [this] {
        if (!updatingSending && !stoppingSending) stopApplicationSending();
    });
    auto *model = pgnTableController->getPgnTableModel();
    connect(model, &PgnTableModel::enabledChanged, this, &MainWindow::updateSendingAvailability);
    connect(model, &QAbstractItemModel::modelReset, this, &MainWindow::updateSendingAvailability);
    connect(model, &QAbstractItemModel::dataChanged, this, &MainWindow::updateSendingAvailability);
    connect(model, &QAbstractItemModel::rowsInserted, this, &MainWindow::updateSendingAvailability);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateSendingAvailability);
    loadSettings();
    initilizeUiWidgets();
    confSignalsSlots();
    initilizeQtStyleIcons();
    loadSettingGeometry();
    initilizeOthers();
    initializeTemplateUi();
    QTimer::singleShot(0,this,&MainWindow::restoreStartupWorkspace);
    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    stopApplicationSending();
    // Transport children notify on shutdown. Detach UI listeners before the
    // QMainWindow/QObject base destructors tear those children down.
    for (auto *child : findChildren<QObject*>()) disconnect(child, nullptr, this, nullptr);
    delete nmeaSenderWorker;
    nmeaSenderWorker = nullptr;
    delete ui;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    WindowState::ensureVisible(*this);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (resolvingClose) { event->ignore(); return; }
    QScopedValueRollback<bool> resolving(resolvingClose, true);
    if (!dialogSetup->resolveForShutdown()) { event->ignore(); return; }
    // Ask about the independent editor's draft before changing any operational
    // state. Cancel leaves both windows and the current Sending state intact.
    if (auto *editor = findChild<EngineProfileDialog *>(); editor && editor->isVisible() && !editor->close()) {
        event->ignore();
        return;
    }
    if (!resolveLayoutForClose()) { event->ignore(); return; }
    if (event->isAccepted()) {
        pollTimer->stop();
        stopApplicationSending();
        saveSettings();
    }

    // Close NMEA2000 interface
    if (nmea2000InterfaceType == "Actisense") {
        nmea2000Handler->ShutdownActisense();
    } else if (nmea2000InterfaceType == "SocketCan") {
        nmea2000Handler->ShutdownSocketCan();
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::showContextHelp(QWidget *origin)
{
    if (!origin) origin = QApplication::focusWidget();
    if (!origin) origin = QApplication::activeModalWidget();
    auto *help = findChild<HelpDialog *>();
    QString topic = "introduction";
    if (help && hasWidgetOwner(origin, help)) {
        topic = help->currentTopic();
    } else {
        // Inherited context hints also cover editors nested in a Template dialog.
        for (auto *widget = origin; widget; widget = widget->parentWidget()) {
            const auto hint = widget->property("rayHelpTopic").toString();
            if (!hint.isEmpty()) { topic = hint; break; }
        }
    }
    QWidget *owner = this;
    if (auto *modal = QApplication::activeModalWidget(); modal && hasWidgetOwner(modal, this)) owner = modal;
    // A modeless child of the active modal dialog remains usable without another
    // exec() or any change to the Template dialog's modality/selection/draft.
    if (help && hasWidgetOwner(owner, help)) return; // Never reparent below Help's own child.
    if (!help) { help = new HelpDialog(owner); help->setAttribute(Qt::WA_DeleteOnClose); }
    else if (help->parentWidget() != owner) { help->hide(); help->setParent(owner, Qt::Dialog); }
    help->showTopic(topic); help->show(); help->raise(); help->activateWindow();
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    // Preserve widget/action/model tooltip contents; only suppress their presentation.
    // Application filters run before widget and item-view tooltip handlers.
    if (!showTooltips && event->type() == QEvent::ToolTip) {
        QToolTip::hideText(); event->accept(); return true;
    }
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        auto *origin = qobject_cast<QWidget *>(object);
        if (key->key() == Qt::Key_F1 && key->modifiers() == Qt::NoModifier
            && hasWidgetOwner(origin, this)) {
            // A main-window QAction is disabled by Qt's modal shortcut routing.
            // Consume the override and handle the key once for owned windows.
            event->accept();
            if (event->type() == QEvent::KeyPress) showContextHelp(origin);
            return true;
        }
        if (key->key() == Qt::Key_F12 && key->modifiers() == Qt::NoModifier) {
            event->accept();
            if (event->type() == QEvent::KeyPress) stopApplicationSending();
            return true;
        }
    }
    // Keep necessary confirmations modal, but give every Qt modal dialog an
    // explicit pointer-accessible STOP sharing the main action. Never accepts it.
    if (event->type() == QEvent::Show) {
        auto *dialog = qobject_cast<QDialog *>(object);
        if (dialog && dialog->isModal() && dialog->layout()
            && !dialog->findChild<QToolButton *>("applicationStopSending")) {
            auto *stop = new QToolButton(dialog);
            stop->setObjectName("applicationStopSending");
            stop->setDefaultAction(ui->actionStop_Sending);
            stop->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            if (auto *grid = qobject_cast<QGridLayout *>(dialog->layout()))
                grid->addWidget(stop, grid->rowCount(), 0, 1, grid->columnCount(), Qt::AlignRight);
            else if (auto *box = qobject_cast<QBoxLayout *>(dialog->layout()))
                box->addWidget(stop, 0, Qt::AlignRight);
            else delete stop; // F12 still works for custom layout types.
        }
    }
    return QMainWindow::eventFilter(object, event);
}

bool MainWindow::event(QEvent *event)
{
    const bool handled = QMainWindow::event(event);
    if (event->type() == QEvent::ApplicationPaletteChange) {
        QTimer::singleShot(0, this, [this] {
            if (Themes::isSystem(styleSheetName)) {
                QToolTip::setPalette(qApp->palette());
                qApp->setWindowIcon(DesktopIcons::forPalette(qApp->palette()));
            }
            updateMenuIconsColor();
        });
    }
    return handled;
}

void MainWindow::PollTimerElapsed() {}

void MainWindow::initilizeUiWidgets()
{
    mainController->setNumberOfWidgetWide(userOptionPrefs.widgetColumns);

    mainController->addSubWindowForGroup("Navigation", "Covers data related to the vessel’s navigation, including position, course, speed, and waypoints.");
    mainController->addSubWindowForGroup("Propulsion",
                                         "Covers data related to the vessel’s propulsion system, such as engine RPM, temperature, and fuel consumption.");
    mainController->addSubWindowForGroup("Environment",
                                         "Includes information about environmental conditions such as tempetaure, air pressure, water depth, etc.");
    mainController->addSubWindowForGroup("Tanks", "Displays information about the levels of various tanks on the vessel, such as fuel, water, and waste.");
    mainController->addSubWindowForGroup("Electrical",
                                         "Includes data about the vessel’s electrical system, such as battery voltage, current, and power consumption.");
    mainController->addSubWindowForGroup("Communication",
                                         "Includes data about the vessel’s communication systems, such as VHF radio, AIS, and other communication equipment.");
    mainController->addSubWindowForGroup("Other", "Includes any other data that does not fit into the other categories.");
    mainController->addSubWindowForGroup("Generate", "");
}

void MainWindow::confSignalsSlots()
{
    connect(dialogSetup, &::DialogSetup::ReloadSettings, this, &MainWindow::HotReloadSettings);
    connect(dialogSetup, &::DialogSetup::ShowStatusBarMessage, this, &MainWindow::ShowStatusBarMessage);
    connect(mainController, &MainController::ShowStatusBarMessage, this, &MainWindow::ShowStatusBarMessage);
    connect(mainController, &MainController::layoutAboutToReplace, this, &MainWindow::stopApplicationSending);
}

void MainWindow::initilizeQtStyleIcons() {}

void MainWindow::loadSettingGeometry()
{
    AppSettings settings(APP_COMPANY, APP_NAME);
    // Modest font-scaled minima keep the inspectors usable without forcing
    // long metadata to determine dock width. Saved dock sizes still win.
    const int em = fontMetrics().horizontalAdvance(QLatin1Char('M'));
    for (auto *dock : {ui->dockWidgetTable, ui->dockWidgetTree})
        dock->setMinimumSize(24 * em, 6 * fontMetrics().height());
    WindowState::restore(*this, settings);
    if (!settings.contains("Window/Panels"))
        resizeDocks({ui->dockWidgetTable, ui->dockWidgetTree}, {width() / 3, width() / 3}, Qt::Horizontal);
    ui->toolBarSending->show();
}

void MainWindow::loadSettings()
{
    QStringList credentialNotices;
    AppSettings settings(APP_COMPANY, APP_NAME);

    settings.beginGroup("Vessel");
    myVessel = readVesselSettings(settings);
    settings.endGroup();

    settings.beginGroup("Nmea2000");
    isNmea2000Enabled = settings.value("Nmea2000Enabled", false).toBool();
    nmea2000InterfaceType = settings.value("InterfaceType", "None").toString();
    signalKConfig.server = settings.value("SignalKServer", "http://127.0.0.1:3000").toString();
    signalKConfig.source = settings.value("SignalKSource", "raynmeasimulator").toString();
    const auto signalCredential = CredentialStore::read("SignalKToken");
    signalKConfig.token = signalCredential.value;
    if (!signalCredential.success) credentialNotices << "Signal K: " + signalCredential.error;

    actisensePort = settings.value("ActisensePort", "None").toString();
    const QString serialPrefix = nmea2000InterfaceType == "CANable SLCAN" ? "Canable" : "Waveshare";
    wavesharePort = settings.value(serialPrefix + "Port", "None").toString();
    if ((nmea2000InterfaceType == "Waveshare USB-CAN-A" || nmea2000InterfaceType == "CANable SLCAN")) waveshareIdentity = WaveshareConfig::load(settings, serialPrefix);
    actisenseBaudRate = settings.value("ActisenseBaudRate", "115200").toString();
    actisensePreamble = settings.value("ActisensePreamble", "0x94").toString();
    if (nmea2000InterfaceType=="SocketCan" && !QApplication::arguments().contains("--no-nmea"))
        socketCanConfig=SocketCanSession::loadConfig(settings);
    if(nmea2000InterfaceType=="Network NMEA2000")networkCanConfig=NetworkCanConfig::load(settings);
    settings.endGroup();

    settings.beginGroup("Nmea0183");
    isNmea0183Enabled = settings.value("Nmea0183Enabled", false).toBool();
    nmea0183Port = settings.value("Nmea0183Port", "None").toString();
    nmea0183BaudRate = settings.value("Nmea0183BaudRate", "4800").toString();
    settings.endGroup();

    settings.beginGroup("Preference");
    showTooltips = settings.value("ShowTooltips", true).toBool();
    if (!showTooltips) QToolTip::hideText();
    styleSheetName = settings.value("StyleSheetName", "Dark").toString();
    setStyleSheet({});
    dialogSetup->setStyleSheet({});
    Themes::apply(styleSheetName);
    QToolTip::setPalette(qApp->palette()); // Qt keeps a separate native tooltip palette.
    qApp->setWindowIcon(DesktopIcons::forPalette(qApp->palette()));
    DesktopIcons::clearLayoutTabs(*ui->mdiAreaMain);
    this->setStyleSheet(RuntimeResources::styleSheet(styleSheetName));
    ensurePolished();
    updateMenuIconsColor();
    dialogSetup->setStyleSheet(styleSheet());

    fontSize = settings.value("FontSize", "10").toInt();
    fontFamily = settings.value("FontFamily", "Sans Serif").toString();
    fontWeight = settings.value("FontWeight", QFont::Normal).toInt();
    fontItalic = settings.value("FontItalic", false).toBool();
    // Set the app font
    QFont font(fontFamily, fontSize, fontWeight, fontItalic);
    QApplication::setFont(font); // Include themed descendants and independent auxiliary windows.
    this->setFont(font);
    // Styled MDI descendants can retain a resolved font from the previous
    // palette/style. Keep instrument roots on the explicit application preference.
    for(auto *instrument:findChildren<PgnWidgetBase*>())instrument->setFont(font);
    qDebug() << "Font set:" << font;
    userUnitPrefs = readUnitPreferences(settings);
    userOptionPrefs = readOptionPreferences(settings);
    settings.endGroup();

    settings.beginGroup("APIs");
    const auto weatherCredential = CredentialStore::read("OpenWeatherMapApiKey");
    m_openWeatherMapApiKey = weatherCredential.value;
    ApplicationLog::registerSecret(signalCredential.value);
    ApplicationLog::registerSecret(weatherCredential.value);
    dialogSetup->setCredentialSnapshot(signalCredential, weatherCredential);
    if (!weatherCredential.success) credentialNotices << "Weather: " + weatherCredential.error;

    settings.endGroup();
    if ((!signalCredential.found && !settings.value("Credentials/SignalKTokenConfigured").toBool() && !settings.value("Nmea2000/SignalKToken").toString().isEmpty())
        || (!weatherCredential.found && !settings.value("Credentials/OpenWeatherMapApiKeyConfigured").toBool() && !settings.value("APIs/OpenWeatherMapApiKey").toString().isEmpty()))
        credentialNotices << "Legacy credentials are preserved. Open Setup to re-enter the weather key or request Signal K access for OS secure storage.";
    m_credentialNotice = credentialNotices.join(" ");

}

void MainWindow::saveSettings()
{
    AppSettings settings(APP_COMPANY, APP_NAME);
    WindowState::save(*this, settings);
}

void MainWindow::initilizeOthers()
{
    stopApplicationSending();
    nmea2000Handler->ShutdownActisense();
    nmea0183Handler->CloseDevice();
    // Initilize the main controller
    mainController->setUserOptionPrefs(userOptionPrefs);
    mainController->setUserPrefs(userUnitPrefs);
    mainController->setMyVessel(myVessel);

    // Keep standalone development checks off the marine interfaces, including
    // after a settings reload. Do not change the user's saved configuration.
    if (QApplication::arguments().contains(QStringLiteral("--no-nmea"))) {
        isNmea2000Enabled = false;
        nmea2000InterfaceType = "None";
        isNmea0183Enabled = false;
        nmea0183Port = "None";
        ui->actionStart_Sending->setEnabled(false);
        ui->actionSend_Test_129026->setEnabled(false);
        ui->actionSend_Test_129033->setEnabled(false);
        qInfo() << "Standalone mode: NMEA interfaces and transmission controls disabled.";
    }

    // Forget the old RX device before applying saved configuration. In particular,
    // disabling NMEA2000 must not leave a device eligible for a later Viewer switch.
    nmea2000Handler->StopWaveshare(true);
    // Initialize the NMEA 2000 handler
    nmea2000Handler->SetInterfaceType(nmea2000InterfaceType);
    nmea2000Handler->setMode(applicationMode);
    if (isNmea2000Enabled) {
        if (nmea2000InterfaceType == "Actisense") {
            nmea2000Handler->InitializeActisense(actisensePort, helper::GetBaudRateFromString(actisenseBaudRate), actisensePreamble);
        } else if (nmea2000InterfaceType == "Network NMEA2000") {
            nmea2000Handler->networkCan()->configure(networkCanConfig);
        } else if (nmea2000InterfaceType == "SocketCan") {
            nmea2000Handler->InitializeSocketCan(socketCanConfig);
        } else if ((nmea2000InterfaceType == "Waveshare USB-CAN-A" || nmea2000InterfaceType == "CANable SLCAN")) {
            nmea2000Handler->ConfigureWaveshare(wavesharePort, waveshareIdentity);
        } else if (nmea2000InterfaceType == "SignalK") {
            nmea2000Handler->ConfigureSignalK(signalKConfig);
        } else {
            //nmea2000Handler->initSimulator();
        }
    }

    if (!isNmea2000Enabled){nmea2000Handler->networkCan()->stop();nmea2000Handler->ShutdownSocketCan();}

    // Initialize the NMEA 0183 handler
    nmea0183Handler->setMode(applicationMode);
    if (isNmea0183Enabled && nmea0183Port != "None") {
        if (nmea0183Handler->IsOpen()) {
            nmea0183Handler->CloseDevice();
        }

        nmea0183Handler->Initialize(nmea0183Port, helper::GetBaudRateFromString(nmea0183BaudRate));
        nmea0183Handler->OpenDevice();
    } else {
        nmea0183Handler->CloseDevice();
    }

    // Set Api key for the weather service
    mainController->setOpenWeatherMapApiKey(m_openWeatherMapApiKey);
    updateSendingAvailability();
    if (!m_credentialNotice.isEmpty()) ui->statusbar->showMessage(m_credentialNotice, 15000);
}

void MainWindow::initilizeUnits()
{
        
}

void MainWindow::updateMenuIconsColor()
{
    // Project-drawn controls glyph plus Qt style icons; no historical bitmap art.
    const QColor color = palette().color(QPalette::WindowText);
    ui->actionStart_Sending->setIcon(createColoredIcon(style()->standardIcon(QStyle::SP_MediaPlay), color));
    ui->actionStop_Sending->setIcon(createColoredIcon(style()->standardIcon(QStyle::SP_MediaStop), color));
    ui->actionOpen_Setup_Form->setIcon(originalSettingsIcon(color));
    ui->actionExit->setIcon(createColoredIcon(style()->standardIcon(QStyle::SP_DialogCloseButton), color));
}

void MainWindow::on_actionPrint_Tree_Model_triggered()
{
    //pgnTreeController->printTreeModel();
}

void MainWindow::on_actionViewer_Rx_toggled(bool arg1)
{
    if (selectingMode) return;
    if (arg1) selectMode(Viewer);
    else if (!ui->actionSimulator_Tx->isChecked()) synchronizeModeActions();
}

void MainWindow::synchronizeModeActions()
{
    // Leave QActionGroup's own signals enabled so checkedAction also stays correct.
    QScopedValueRollback<bool> updating(selectingMode, true);
    ui->actionSimulator_Tx->setChecked(applicationMode == Sender);
    ui->actionViewer_Rx->setChecked(applicationMode == Viewer);
}

bool MainWindow::prepareLayoutForViewer()
{
    if (!mainController->hasLayoutWidgets()) return true;
    const bool saveFirst = !mainController->hasLayoutBaseline() || mainController->layoutStructurallyModified();
    QMessageBox dialog(QMessageBox::Question, tr("Prepare Layout for Viewer (RX)"),
        tr("Viewer automatically discovers received instruments. Clearing existing widgets is recommended for a clean receive session.\n\n"
           "Keep Existing retains the current widgets and their values. Cancel stays in Generate. Sending remains stopped."),
        QMessageBox::NoButton, this);
    dialog.setObjectName("viewerEntryDialog");
    auto *clear = dialog.addButton(saveFirst ? tr("Save && Clear") : tr("Clear"), QMessageBox::AcceptRole);
    clear->setObjectName(saveFirst ? "viewerSaveAndClear" : "viewerClear");
    auto *keep = dialog.addButton(tr("Keep Existing"), QMessageBox::ActionRole);
    keep->setObjectName("viewerKeepExisting");
    auto *cancel = dialog.addButton(QMessageBox::Cancel);
    cancel->setObjectName("viewerCancel");
    dialog.setDefaultButton(cancel);
    dialog.setEscapeButton(cancel);
    dialog.exec();
    if (dialog.clickedButton() == keep) return true;
    if (dialog.clickedButton() != clear) return false;
    if (saveFirst && !saveLayoutAs(true)) return false;
    mainController->removeAllWidgets();
    if (mainController->hasLayoutWidgets()) {
        statusBar()->showMessage(tr("Cannot enter Viewer: the working Layout could not be cleared."), 15000);
        return false;
    }
    return true;
}

void MainWindow::selectMode(Mode mode)
{
    if (selectingMode) return;
    QScopedValueRollback<bool> updating(selectingMode, true);
    if (mode == applicationMode) { synchronizeModeActions(); return; }
    synchronizeModeActions(); // Preparation has not changed the actual mode yet.
    // Revoke authorization before any nested dialog, even if entry is cancelled.
    stopApplicationSending();
    if (mode == Viewer && !prepareLayoutForViewer()) { synchronizeModeActions(); return; }
    applicationMode = mode;
    nmeaSenderWorker->setMode(applicationMode);
    nmea2000Handler->setMode(applicationMode);
    nmea0183Handler->setMode(applicationMode);
    mainController->setMode(applicationMode);
    synchronizeModeActions();
    updateSendingAvailability();
}

void MainWindow::on_actionSimulator_Tx_toggled(bool arg1)
{
    if (selectingMode) return;
    if (arg1) selectMode(Sender);
    else if (!ui->actionViewer_Rx->isChecked()) synchronizeModeActions();
}

void MainWindow::on_actionOpen_Setup_Form_triggered()
{
    dialogSetup->setStyleSheet(this->styleSheet());
    dialogSetup->setFont(this->font());
    if (dialogSetup->isMinimized()) dialogSetup->showNormal(); else dialogSetup->show();
    dialogSetup->raise(); dialogSetup->activateWindow();
}

void MainWindow::ShowStatusBarMessage(QString message, int timeout)
{
    ApplicationLog::event("raynmea.app", message);
    statusBar()->showMessage(message, timeout);
}

void MainWindow::HotReloadSettings()
{
    // Support for not having to close and re-open application after changing setup setting values.
    ui->statusbar->showMessage("Hot-reloading saved settings...", 3000);
    stopApplicationSending();
    const int previousColumns = userOptionPrefs.widgetColumns;
    loadSettings();
    initilizeOthers();
    if (previousColumns != userOptionPrefs.widgetColumns) mainController->setNumberOfWidgetWide(userOptionPrefs.widgetColumns);
    initilizeUnits();
}

QIcon MainWindow::createColoredIcon(const QString &pngPath, const QColor &color)
{
    QPixmap pixmap(pngPath);

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();

    return QIcon(pixmap);
}

QIcon MainWindow::createColoredIcon(const QIcon &originalIcon, const QColor &color)
{
    QPixmap pixmap = originalIcon.pixmap(originalIcon.actualSize(QSize(100, 100))); // Adjust size as needed
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    return QIcon(pixmap);
}

void MainWindow::on_actionExit_triggered()
{
    // Close the application
    close();
}

void MainWindow::stopApplicationSending()
{
    if (stoppingSending) return;
    {
        QScopedValueRollback<bool> guard(stoppingSending, true);
        sendingRequested = false;
        sendingError2000.clear();
        sendingError0183.clear();
        // Revoke commands before synchronous worker/transport notifications can
        // reenter the UI. Readiness is recomputed after the whole stop completes.
        updateSendingAvailability();
        nmeaSenderWorker->stopSending();
        nmea2000Handler->StopSocketCanSending();
        nmea2000Handler->StopWaveshare();
        nmea2000Handler->StopSignalKSending();
    }
    updateSendingAvailability();
}

bool MainWindow::nmea2000SendingReady() const
{
    if (!isNmea2000Enabled || !sendingError2000.isEmpty()
        || !TransportCapabilities::canTransmit(nmea2000InterfaceType)) return false;
    if (nmea2000InterfaceType == "Network NMEA2000") return nmea2000Handler->networkCan()->transmitting();
    if (nmea2000InterfaceType == "SocketCan") return nmea2000Handler->IsCanInterfaceOpen();
    if (nmea2000InterfaceType == "Actisense") return nmea2000Handler->IsActisensePortOpen();
    if ((nmea2000InterfaceType == "Waveshare USB-CAN-A" || nmea2000InterfaceType == "CANable SLCAN")) return nmea2000Handler->IsWaveshareReady();
    return nmea2000InterfaceType == "SignalK" && nmea2000Handler->IsSignalKReady();
}

bool MainWindow::nmea0183SendingReady() const
{
    return isNmea0183Enabled && sendingError0183.isEmpty() && nmea0183Handler->IsOpen();
}

QString MainWindow::sendingBlocker() const
{
    if (QApplication::arguments().contains("--no-nmea")) return tr("standalone mode (--no-nmea).");
    if (applicationMode != Sender) return tr("Viewer mode. Select Generate to transmit.");
    if (isNmea2000Enabled && !TransportCapabilities::canTransmit(nmea2000InterfaceType)
        && nmea2000InterfaceType == "Actisense") return TransportCapabilities::actisenseNotice();
    if(isNmea2000Enabled && nmea2000InterfaceType=="Network NMEA2000" && !networkCanConfig.allowTx)return tr("Network NMEA2000 configured RX-only.");
    if (!isNmea2000Enabled && !isNmea0183Enabled) return tr("select and enable an output interface in Setup.");
    return {};
}

bool MainWindow::diagnosticSendingAllowed() const
{
#ifdef QT_DEBUG
    return sendingRequested && nmeaSenderWorker->isSending() && nmea2000SendingReady()
        && sendingBlocker().isEmpty();
#else
    return false;
#endif
}

void MainWindow::updateSendingAvailability()
{
    if (!sendingStatus) return;
    // Notifications only invalidate presentation. Read current adapter state so
    // inactive adapters and delayed notifications cannot own the selected status.
    socketCanStatus=nmea2000Handler->findChild<SocketCanSession*>()->status();
    waveshareStatus=nmea2000Handler->findChild<WaveshareSession*>()->status();
    if (nmea2000InterfaceType=="SignalK") signalKStatus=nmea2000Handler->transportStatus();
    if (updatingSending) return;
    QScopedValueRollback<bool> guard(updatingSending, true);
    const QString blocker = sendingBlocker();
    const bool signalK = isNmea2000Enabled && nmea2000InterfaceType == "SignalK";
    const bool wave = isNmea2000Enabled && (nmea2000InterfaceType == "Waveshare USB-CAN-A" || nmea2000InterfaceType == "CANable SLCAN");
    const bool ready2000 = nmea2000SendingReady(), ready0183 = nmea0183SendingReady();
    const bool anyReady = ready2000 || ready0183;
    if (sendingRequested && !stoppingSending) {
        if (!blocker.isEmpty()) stopApplicationSending();
        else if (anyReady) nmeaSenderWorker->startSending();
        else {
            nmeaSenderWorker->stopSending();
            // Retain authorization while an explicitly started asynchronous session
            // is opening/claiming. A stopped/failed Waveshare never retries itself.
            if (!(signalK && nmea2000Handler->IsSignalKActive())
                && !(wave && nmea2000Handler->IsWaveshareActive())
                && !(nmea2000InterfaceType=="Network NMEA2000" && nmea2000Handler->networkCan()->openingOrActiveTx())) sendingRequested = false;
        }
    }
    const bool operational = sendingRequested && blocker.isEmpty() && anyReady && nmeaSenderWorker->isSending();
    const QSignalBlocker blocked(ui->actionStart_Sending);
    ui->actionStart_Sending->setChecked(operational);
    // Signal K validates configuration/authentication asynchronously on request.
    // Error latches can be retried explicitly; dispatch still requires local readiness.
    const bool canRequest = (isNmea2000Enabled && (signalK
        || (wave && nmea2000Handler->IsWaveshareConfigured())
        || (nmea2000InterfaceType == "Network NMEA2000" && nmea2000Handler->networkCan()->connected())
        || (nmea2000InterfaceType == "SocketCan" && nmea2000Handler->IsCanInterfaceOpen())
        || (nmea2000InterfaceType == "Actisense" && nmea2000Handler->IsActisensePortOpen())))
        || (isNmea0183Enabled && nmea0183Handler->IsOpen());
    ui->actionStart_Sending->setEnabled(!stoppingSending && !sendingRequested
        && !nmeaSenderWorker->isSending() && !nmea2000Handler->routeServiceActive() && blocker.isEmpty() && canRequest);
    // A receive/standby session alone is not global Sending authorization.
    ui->actionStop_Sending->setEnabled(!stoppingSending
        && (sendingRequested || nmeaSenderWorker->isSending() || nmea2000Handler->routeServiceActive()));
    ui->actionSend_Test_129026->setEnabled(diagnosticSendingAllowed());
    ui->actionSend_Test_129033->setEnabled(diagnosticSendingAllowed());
    // Configuration readiness is presentation only; it must not authorize a
    // connection or change the existing anonymous-server/start/retry behavior.
    QString signalKState;
    if (signalK) {
        const QUrl url(signalKConfig.server);
        if (sendingRequested && nmea2000Handler->IsSignalKActive()) {
            if (signalKStatus.contains("retrying")) signalKState = tr("recovering (connection/authorization error)");
            else if (signalKStatus.contains("checking write authorization")) signalKState = tr("authenticating");
            else signalKState = tr("connecting (waiting for hello)");
        } else if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")
                   || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
                   || (!url.path().isEmpty() && url.path() != "/"))
            signalKState = tr("not configured: invalid server origin");
        else if (signalKConfig.source.isEmpty() || signalKConfig.source.size() > 64
                 || signalKConfig.source.contains(QRegularExpression("[^A-Za-z0-9_-]")))
            signalKState = tr("not configured: invalid publisher identity");
        else if (signalKConfig.token.trimmed().isEmpty()) signalKState = tr("missing credentials: configure a device token");
        else if (signalKConfig.token.contains('\r') || signalKConfig.token.contains('\n'))
            signalKState = tr("not configured: invalid token format");
        else signalKState = tr("ready to connect");
    }
    QStringList details, outputs;
    if (isNmea2000Enabled) {
        const QString name = tr("NMEA2000 (%1)").arg(nmea2000InterfaceType);
        if (ready2000) outputs << name;
        else if (!sendingError2000.isEmpty()) details << name + tr(" unavailable: ") + sendingError2000;
        else if (signalK) details << name + ": " + signalKState
            + (sendingRequested ? ". " : tr(". Enable Sending to connect and verify Signal K. ")) + signalKStatus;
        else if (nmea2000InterfaceType == "Network NMEA2000") details << name + ": " + nmea2000Handler->networkCan()->status();
        else if (nmea2000InterfaceType == "SocketCan") details << name + tr(" unavailable: ") + socketCanStatus;
        else if ((nmea2000InterfaceType == "Waveshare USB-CAN-A" || nmea2000InterfaceType == "CANable SLCAN")) details << name + ": " + waveshareStatus;
        else details << name + tr(" unavailable: serial port is not open. Check Setup and the connection.");
    }
    if (isNmea0183Enabled) {
        if (ready0183) outputs << "NMEA0183";
        else details << tr("NMEA0183 unavailable: ") + (!sendingError0183.isEmpty() ? tr("error - ") + sendingError0183
            : tr("No open serial port. Select a valid port in Setup and check the connection."));
    }
    QString status;
    if (stoppingSending) status = tr("Stopping Sending...");
    else if (nmea2000Handler->deviceDiscovery()->busy()) status = tr("Device discovery authorized; instrument Sending disabled. Stop Sending or F12 cancels discovery.");
    else if (nmea2000Handler->routeServiceActive()) status = tr("Route service authorized; instrument Sending disabled. Stop Sending or F12 cancels route requests.");
    else if (!blocker.isEmpty()) status = tr("Sending stopped - ") + blocker;
    else if (operational) {
        bool eligible = false;
        for (const auto &entry : pgnTableController->getPgnTableModel()->getPgnList()) {
            if (!entry.enabled || entry.pgn.toInt() < 0) continue;
            eligible |= entry.pgn.size() == 3 ? ready0183 : (ready2000 && entry.pgn.toInt() != 0);
        }
        status = tr("Sending enabled - ") + outputs.join(" + ")
            + (eligible ? tr(". Enabled instruments only.") : tr(". Waiting for enabled instruments."));
        if (ready2000 && (nmea2000InterfaceType == "Waveshare USB-CAN-A" || nmea2000InterfaceType == "CANable SLCAN")) status += " " + waveshareStatus;
        if (ready2000 && nmea2000InterfaceType == "Actisense") status += tr(" Outbound delivery unverified (Issue #1).");
    } else if (sendingRequested) status = tr("Sending not active - connecting/recovering. Stop cancels.");
    else status = tr("Sending stopped (disabled). ") + (outputs.isEmpty() ? QString() : tr("Ready: ") + outputs.join(" + ") + ".");
    if (!details.isEmpty()) status += " " + details.join(" ");
    // Summary presentation only: detailed status and readiness logic stay intact.
    QStringList summary;
    if (stoppingSending) summary << tr("Stopping");
    else if (nmea2000Handler->deviceDiscovery()->busy()) summary << tr("Device discovery") << tr("Instrument Sending disabled");
    else if (nmea2000Handler->routeServiceActive()) summary << tr("Route service") << tr("Instrument Sending disabled");
    else if (!blocker.isEmpty()) summary << tr("Stopped") << blocker;
    else {
        summary << (operational ? tr("Sending: ") + outputs.join(" + ")
                                : sendingRequested ? tr("Connecting/recovering") : tr("Stopped"));
        if (isNmea2000Enabled && !ready2000)
            summary << tr("NMEA2000 (%1): %2").arg(nmea2000InterfaceType,
                nmea2000InterfaceType=="Network NMEA2000" ? nmea2000Handler->networkCan()->status() : signalK ? signalKState : wave && sendingError2000.isEmpty()
                    ? (sendingRequested ? tr("configuring / claiming") : tr("stopped; Enable Sending to connect"))
                    : tr("unavailable"));
        if (isNmea0183Enabled && !ready0183) summary << tr("NMEA0183: unavailable");
    }
    sendingStatus->setText(summary.join(" | "));
    sendingStatus->setAccessibleName(tr("Sending status"));
    sendingStatus->setAccessibleDescription(status);
    sendingStatus->setToolTip(status + tr(" Readiness and local write acceptance do not confirm delivery at a receiver."));
    // Lifecycle operations above may synchronously stop/reconfigure a session.
    if (auto *label=findChild<QLabel*>("transportStatus")) {
        const auto text=isNmea2000Enabled?nmea2000Handler->transportStatus():tr("NMEA2000 disabled");
        label->setText(text);label->setToolTip(text);
    }
}

void MainWindow::sendingFailed(const QString &message, bool nmea2000)
{
    if (nmea2000) {
        sendingError2000 = message;
        nmea2000Handler->StopSocketCanSending();
        nmea2000Handler->StopWaveshare();
    } else sendingError0183 = message;
    updateSendingAvailability();
}

void MainWindow::on_actionStart_Sending_toggled(bool requested)
{
    // Enable Sending is START-only. Its check mark reflects operational status;
    // an unchecked notification must never act as the separate STOP command.
    if (!requested || sendingRequested || nmeaSenderWorker->isSending()
        || stoppingSending || updatingSending || !ui->actionStart_Sending->isEnabled()) {
        updateSendingAvailability();
        return;
    }
    if (!sendingBlocker().isEmpty()) { updateSendingAvailability(); return; }
    sendingError2000.clear();
    sendingError0183.clear();
    sendingRequested = true;
    if(isNmea2000Enabled&&nmea2000InterfaceType=="Network NMEA2000"){QScopedValueRollback<bool> guard(updatingSending,true);nmea2000Handler->networkCan()->start("tx");}
    if (isNmea2000Enabled && nmea2000InterfaceType == "SignalK") {
        // Avoid reentrant readiness changes clearing the request during start().
        QScopedValueRollback<bool> guard(updatingSending, true);
        nmea2000Handler->StartSignalKSending();
    }
    if (isNmea2000Enabled && (nmea2000InterfaceType == "Waveshare USB-CAN-A" || nmea2000InterfaceType == "CANable SLCAN")) {
        QScopedValueRollback<bool> guard(updatingSending, true);
        nmea2000Handler->StartWaveshareSending();
    }
    updateSendingAvailability();
}

void MainWindow::on_actionStop_Sending_triggered()
{
    if (ui->actionStop_Sending->isEnabled() || nmea2000Handler->routeServiceActive()) stopApplicationSending();
}

void MainWindow::on_actionSend_Test_129026_triggered()
{
    if (diagnosticSendingAllowed() && !nmea2000Handler->sendTestPgn129026())
        sendingFailed(tr("Test PGN 129026 was rejected by the output interface."));
}

void MainWindow::on_actionSend_Test_129033_triggered()
{
    if (diagnosticSendingAllowed() && !nmea2000Handler->sendCurrentTimePgn129033())
        sendingFailed(tr("Test PGN 129033 was rejected by the output interface."));
}

void MainWindow::on_actionSave_Layout_as_Default_triggered()
{
    QString error;
    const auto path = AppPaths::layoutSavePath("defaultLayout", &error);
    if (path.isEmpty()) { statusBar()->showMessage(error, 15000); return; }
    mainController->saveLayoutToJson(path);
}
void MainWindow::on_actionLoad_Default_triggered()
{
    QString error;
    const auto path = AppPaths::defaultLayout(&error);
    if (path.isEmpty()) { statusBar()->showMessage(error, 15000); return; }
    if (!loadWorkingLayout(path, tr("Load Default Layout"))) return;
    qInfo() << "Default layout loaded:" << path;
}
void MainWindow::on_actionLoad_triggered()
{
    const auto path = UiHelper::ShowLayoutPicker("Load Layout", "Select the layout to load:");
    if (path.isEmpty()) return;
    if (!loadWorkingLayout(path, tr("Load Layout"))) return;
    qInfo() << "Layout loaded:" << path;
}
void MainWindow::on_actionSave_triggered()
{
    saveLayoutAs();
}

bool MainWindow::resolveLayoutForClose()
{
    if (!userOptionPrefs.promptSaveLayoutOnClose || !mainController->layoutNeedsSave()) return true;
    // Resolve drafts first, then revoke Sending before the Layout decision.
    // No timer, generator or transport teardown occurs until every decision succeeds.
    stopApplicationSending();
    QMessageBox dialog(QMessageBox::Question, tr("Unsaved Layout changes"),
        tr("Save the current Layout before closing? Widget additions or removals have not been saved.\n\n"
           "Discard leaves the saved Layout unchanged. Cancel keeps this session open. Sending remains stopped."),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    dialog.setObjectName("layoutCloseDialog");
    dialog.setDefaultButton(QMessageBox::Cancel);
    dialog.setEscapeButton(QMessageBox::Cancel);
    const auto answer = dialog.exec();
    if (answer == QMessageBox::Discard) return true;
    return answer == QMessageBox::Save && saveCurrentLayout();
}

bool MainWindow::saveCurrentLayout()
{
    const QFileInfo current(mainController->layoutFilePath());
    // Legacy, resource and externally loaded Layouts keep the existing Save As
    // copy policy. Only a current-data Layout can be saved directly in place.
    if (mainController->layoutFilePath().isEmpty() || current.absolutePath() != AppPaths::layoutsDirectory())
        return saveLayoutAs(true);
    QString error;
    const auto validated = AppPaths::layoutSavePath(mainController->layoutSaveName(), &error);
    if (validated.isEmpty()) { statusBar()->showMessage(error, 15000); return false; }
    if (QMessageBox::question(this, tr("Layout Exists"), tr("Save changes to the current Layout: %1?").arg(current.fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return false;
    return mainController->saveLayoutToJson(current.absoluteFilePath());
}

bool MainWindow::prepareLayoutDestruction(const QString &operation, bool confirmUnchanged,
                                        const QString &conversionWarning)
{
    stopApplicationSending();
    const bool needsSave = mainController->layoutNeedsSave();
    if (!needsSave && !(confirmUnchanged && mainController->hasLayoutWidgets()) && conversionWarning.isEmpty())
        return true;
    const auto text = needsSave
        ? tr("Save the working Layout before %1?\n\nDiscard proceeds without saving. Cancel keeps the current Layout. Sending remains stopped.").arg(operation)
        : tr("Proceed with %1? Existing widgets will be replaced or removed. Sending remains stopped.").arg(operation);
    QMessageBox dialog(QMessageBox::Question, operation,
        conversionWarning.isEmpty() ? text : text + "\n\n" + conversionWarning,
        needsSave ? QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel : QMessageBox::Yes | QMessageBox::No, this);
    dialog.setObjectName("layoutOperationDialog");
    dialog.setDefaultButton(needsSave ? QMessageBox::Cancel : QMessageBox::No);
    dialog.setEscapeButton(needsSave ? QMessageBox::Cancel : QMessageBox::No);
    const auto answer = dialog.exec();
    if (!needsSave) return answer == QMessageBox::Yes;
    return answer == QMessageBox::Discard || (answer == QMessageBox::Save && saveCurrentLayout());
}

bool MainWindow::loadWorkingLayout(const QString &path, const QString &operation)
{
    // Selection is complete. Invalid targets also revoke Sending, but validation
    // happens before any save warning or widget/binding mutation. The loader keeps
    // its parsed target even if Save writes to the same pathname during the guard.
    stopApplicationSending();
    return mainController->loadLayoutFromJson(path, [this, &operation](const QString &warning) {
        return prepareLayoutDestruction(operation, true, warning);
    });
}

bool MainWindow::saveLayoutAs(bool layoutOnly)
{
    QDialog choice(this); choice.setWindowTitle(tr("Save As"));
    QVBoxLayout form(&choice); QComboBox type; type.setObjectName("saveDocumentType");
    type.addItems({tr("Layout"),tr("Template")}); form.addWidget(new QLabel(tr("Save Type:"))); form.addWidget(&type);
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addWidget(&buttons);
    connect(&buttons,&QDialogButtonBox::accepted,&choice,&QDialog::accept);
    connect(&buttons,&QDialogButtonBox::rejected,&choice,&QDialog::reject);
    if(!layoutOnly && choice.exec()!=QDialog::Accepted)return false;
    if(type.currentIndex()==1)return saveTemplate(mainController->captureLayout());
    const auto name = UiHelper::GetLayoutNameInput(this, "Save Layout", "Enter the name of the layout to save:", mainController->layoutSaveName());
    if (name.isEmpty()) return false;
    for (const auto &file : AppPaths::layoutFiles()) {
        if (file.fileName().compare(name + ".json", Qt::CaseInsensitive) != 0) continue;
        const bool current = file.absolutePath() == AppPaths::layoutsDirectory();
        const auto message = current ? tr("The layout file already exists. Do you want to overwrite it?")
            : tr("A legacy Layout has this name. Save an updated copy in the current data directory? The legacy original will remain unchanged.");
        if (QMessageBox::question(this, "Layout Exists", message, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return false;
        break;
    }
    QString error;
    const auto path = AppPaths::layoutSavePath(name, &error);
    if (path.isEmpty()) { statusBar()->showMessage(error, 15000); return false; }
    if (!mainController->saveLayoutToJson(path)) return false;
    qInfo() << "Layout saved:" << path;
    return true;
}

void MainWindow::on_actionClear_triggered()
{
    if (!mainController->hasLayoutWidgets()) return;
    QStringList groupList = mainController->getGroupList();
    if (groupList.isEmpty()) return;

    const auto *activeGroup = ui->mdiAreaMain->activeSubWindow();
    // An empty active tab may not yet have an entry in the widget registry.
    if (activeGroup && !groupList.contains(activeGroup->windowTitle())) groupList.append(activeGroup->windowTitle());
    QString groupName = UiHelper::ShowGroupPicker("Clear Widgets", "Select the group to clear:", groupList,
                                                activeGroup ? activeGroup->windowTitle() : QString());
    if (groupName.isEmpty() || mainController->layoutWidgets(groupName).isEmpty()) return;
    if (!prepareLayoutDestruction(tr("Clear Group: %1").arg(groupName), false)) return;
    mainController->removeAllWidgetsFromGroup(groupName);
}

void MainWindow::on_actionClear_All_triggered()
{
    if (!mainController->hasLayoutWidgets()) return;
    if (!prepareLayoutDestruction(tr("Clear All Widgets"), true)) return;
    mainController->removeAllWidgets();
}

void MainWindow::on_actionRemove_triggered()
{
    const auto path = UiHelper::ShowLayoutPicker("Remove Layout", "Select the layout to remove:");
    if (path.isEmpty()) return;
    if (QMessageBox::question(this, "Delete Saved Layout", tr("Delete this saved Layout file?\n%1").arg(QDir::toNativeSeparators(path)),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (!QFile::remove(path)) {
        const auto message = tr("Cannot delete saved Layout: check the file permissions.");
        qWarning().noquote() << message;
        statusBar()->showMessage(message, 10000);
    }
}
