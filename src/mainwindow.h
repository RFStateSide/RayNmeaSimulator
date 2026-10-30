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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>

#include "dialogsetup.h"
#include "maincontroller.h"
#include "nmea0183handler.h"
#include "nmea2000handler.h"
#include "nmeareceiver.h"
#include "pgntablecontroller.h"
#include "pgntreecontroller.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

public:
    // Optional transport fixtures follow the handlers' existing injection pattern.
    // Serial ports are owned by the handlers; an injected CAN backend outlives this window.
    MainWindow(QWidget *parent = nullptr, QSerialPort *actisensePort = nullptr,
               QSerialPort *serial0183Port = nullptr, SocketCanBackend *canBackend = nullptr,
               WaveshareSession::PortFactory waveshareFactory = {});
    ~MainWindow();

    const QString &getOpenWeatherMapApiKey() const { return m_openWeatherMapApiKey; }
    Mode currentMode() const { return applicationMode; }

private slots:
    void PollTimerElapsed();
    void on_actionPrint_Tree_Model_triggered();
    void on_actionViewer_Rx_toggled(bool arg1);
    void on_actionSimulator_Tx_toggled(bool arg1);
    void on_actionOpen_Setup_Form_triggered();
    void on_actionExit_triggered();
    void on_actionStart_Sending_toggled(bool arg1);
    void on_actionStop_Sending_triggered();
    void on_actionSend_Test_129026_triggered();
    void on_actionSend_Test_129033_triggered();
    void on_actionSave_Layout_as_Default_triggered();
    void on_actionLoad_triggered();
    void on_actionSave_triggered();
    void on_actionClear_triggered();
    void on_actionClear_All_triggered();
    void on_actionRemove_triggered();
    void on_actionLoad_Default_triggered();
    void updateMenuIconsColor();

public slots:
    void ShowStatusBarMessage(QString message, int timeout = 3000);
    void HotReloadSettings();

private:
    Ui::MainWindow *ui;
    Nmea2000Handler *nmea2000Handler;
    Nmea0183Handler *nmea0183Handler;
    MainController *mainController;
    PgnTableController *pgnTableController;
    PgnTreeController *pgnTreeController;
    DialogSetup *dialogSetup;
    void showContextHelp(QWidget *origin = nullptr);
    void stopApplicationSending();
    void updateSendingAvailability();
    QString sendingBlocker() const;
    bool nmea2000SendingReady() const;
    bool nmea0183SendingReady() const;
    bool diagnosticSendingAllowed() const;
    void sendingFailed(const QString &message, bool nmea2000 = true);
    QLabel *sendingStatus = nullptr;
    bool showTooltips = true;
    bool sendingRequested = false; // Explicit authorization, retained only across Signal K retries.
    bool updatingSending = false, stoppingSending = false;
    QString sendingError2000, sendingError0183, signalKStatus, socketCanStatus;
    QString waveshareStatus, wavesharePort;
    WaveshareConfig waveshareIdentity;
    Mode applicationMode = Sender;
    bool selectingMode = false;
    void selectMode(Mode mode);
    void synchronizeModeActions();
    bool prepareLayoutForViewer();
    bool saveLayoutAs(bool layoutOnly = false);
    void initializeTemplateUi();
    bool saveTemplate(QJsonObject root, const QString &existingPath = {}, bool readOnly = false);
    void manageTemplates();
    void configureStartupWorkspace();
    void restoreStartupWorkspace();
    bool applyTemplate(const QString &path);
    bool checkTemplate(const QJsonObject &root, QString &error) const;
    bool saveCurrentLayout();
    bool prepareLayoutDestruction(const QString &operation, bool confirmUnchanged,
                                  const QString &conversionWarning = {});
    bool loadWorkingLayout(const QString &path, const QString &operation);
    bool resolveLayoutForClose();
    bool resolvingClose = false;
    NmeaSenderWorker *nmeaSenderWorker; // Pointer to the sender worker
    //QThread *nmeaReceiverThread;        // Thread for the receiver worker
    NmeaReceiver *nmeaReceiver; // Pointer to the receiver worker

    bool isDarkMode(); //
    QIcon createColoredIcon(const QString &pngPath, const QColor &color);
    QIcon createColoredIcon(const QIcon &originalIcon, const QColor &color);
    QPixmap logoPixmap;

    void initilizeUiWidgets();
    void confSignalsSlots();
    void initilizeQtStyleIcons();
    void loadSettingGeometry();
    void loadSettings();
    void saveSettings();
    void initilizeOthers();
    void initilizeUnits();

    QTimer *pollTimer = new QTimer(this);

    QString fontFamily = "Sans Serif";
    int fontSize = 11;
    int fontWeight = QFont::Normal;
    bool fontItalic = false;
    //QString styleSheetName = "darkeum";
    QString styleSheetName = "None";
    UserUnitPrefs userUnitPrefs;
    MyVessel myVessel;
    bool isNmea2000Enabled = false;
    QString nmea2000InterfaceType = "None"; // None, Actisense, SocketCan, SignalK
    SocketCanConfig socketCanConfig;
    NetworkCanConfig networkCanConfig;
    SignalKConfig signalKConfig;
    QString actisensePort = "None";
    QString actisenseBaudRate = "115200";
    QString actisensePreamble = "0x94";
    bool isNmea0183Enabled = false;
    QString nmea0183Port = "None";
    QString nmea0183BaudRate = "4800";
    UserOptionPrefs userOptionPrefs;
    QString m_openWeatherMapApiKey;
    QString m_credentialNotice;

};
#endif // MAINWINDOW_H
