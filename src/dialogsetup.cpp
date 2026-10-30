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

#include "logviewer.h"
#include "applicationlog.h"
#include "runtimeresources.h"
#include "transportcapabilities.h"


#include "dialogsetup.h"
#include "networkcansession.h"
#include "wavesharesession.h"
#include "serialportdiscovery.h"
#include "apppaths.h"
#include "themes.h"
#include "windowstate.h"
#include "preferences.h"
#include "aisclassbstatic129810.h"
#include "aisclassbstatic129809.h"
#include "coordinateeditor.h"
#include <QScopedValueRollback>
#include <QDoubleSpinBox>
#include "n2khelper.h"
#include "ui_dialogsetup.h"

#include <QDebug>
#include <QCloseEvent>
#include <QToolButton>
#include <QAction>
#include <QAbstractButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHideEvent>
#include "signalksetupclient.h"
#include "socketcansession.h"
#include <QErrorMessage>
#include <QFile>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMessageBox>
#include "appsettings.h"
#include <QShowEvent>
#include "convert.h"
#include "enginesimulator.h"
#include "helper.h"
#include <cmath>

DialogSetup::DialogSetup(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogSetup)
{
    ui->setupUi(this);
    loggingPreferences=new LoggingPreferences(this);
    ui->tabWidget->addTab(loggingPreferences,tr("Logging"));
    for (const auto &entry : AisClassBStatic129810::vesselTypes())
        ui->vesselType->addItem(entry.second, entry.first);
    ui->vesselType->setToolTip(AisClassBStatic129810::fieldDescription("VesselType"));
    ui->radioCallsign->setToolTip("Up to seven uppercase AIS characters (ASCII 32–95, excluding @). Empty means unavailable. Invalid input is rejected on Save, never truncated.");
    ui->radioCallsign->setAccessibleName(tr("Radio callsign"));
    ui->vesselMmsi->setToolTip(AisClassBStatic129809::fieldDescription("UserID"));
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setWindowModality(Qt::NonModal);
    setSizeGripEnabled(true);
    auto *cancelEdits = new QPushButton(tr("Cancel"), this);
    cancelEdits->setObjectName("setupCancel"); cancelEdits->setAutoDefault(false);
    ui->horizontalLayout->addWidget(cancelEdits);
    connect(cancelEdits, &QPushButton::clicked, this, &DialogSetup::reject);
    auto *transportLayout = qobject_cast<QVBoxLayout *>(ui->comboBoxNmea2000Interface->parentWidget()->layout());
    // Runtime transport panels belong before the Designer's trailing stretch.
    transportLayout->removeItem(ui->verticalSpacer_4);
    auto *actisenseNotice = new QLabel(TransportCapabilities::actisenseNotice(), this);
    actisenseNotice->setObjectName("actisenseCapabilityNotice");
    actisenseNotice->setWordWrap(true);
    transportLayout->addWidget(actisenseNotice);
    const auto showActisense = [this, actisenseNotice](const QString &type) {
        actisenseNotice->setVisible(type == "Actisense");
        for (auto *widget : QList<QWidget *>{ui->labelNmea2000ActisensePort,
             ui->comboBoxNmea2000ActisensePort, ui->labelNmea2000ActisensePort_2,
             ui->comboBoxNmea2000ActisenseBaudRate, ui->labelNmea2000ActisensePort_3,
             ui->comboBoxNmea2000ActisensePreamble}) widget->setVisible(type == "Actisense");
    };
    connect(ui->comboBoxNmea2000Interface, &QComboBox::currentTextChanged, this, showActisense);
    showActisense(ui->comboBoxNmea2000Interface->currentText());
    auto *canBox=new QWidget(ui->comboBoxNmea2000Interface->parentWidget());
    auto *canForm=new QFormLayout(canBox);
    socketCanName=new QLineEdit("can0",canBox);socketCanName->setMaxLength(15);
    socketCanName->setToolTip(tr("Existing SocketCAN interface. RayNmeaSimulator does not configure bitrate or bring the interface up."));
    socketCanIdentity=new QSpinBox(canBox);socketCanIdentity->setRange(1,2097151);
    canForm->addRow("SocketCAN interface",socketCanName);
    canForm->addRow("Unique device number",socketCanIdentity);
    auto *note=new QLabel("Use a distinct device number for each simultaneous node. Interface and identity changes require restart. Viewer permits protocol management traffic and separately authorized bounded route requests. Cancel/Stop/F12 after route requests requires application restart; closing a completed route chooser normally releases its authorization.",canBox);
    note->setWordWrap(true);canForm->addRow(note);
    ui->comboBoxNmea2000Interface->parentWidget()->layout()->addWidget(canBox);
    canBox->setObjectName("socketCanSettings");
    connect(ui->comboBoxNmea2000Interface, &QComboBox::currentTextChanged, canBox,
            [canBox](const QString &text) { canBox->setVisible(text == "SocketCan"); });
    canBox->hide();

    auto *networkBox=new QWidget(ui->comboBoxNmea2000Interface->parentWidget());networkBox->setObjectName("networkCanSettings");
    auto *networkForm=new QFormLayout(networkBox);
    networkHost=new QLineEdit(networkBox);networkHost->setObjectName("networkCanHost");networkHost->setMaxLength(253);
    networkPort=new QSpinBox(networkBox);networkPort->setRange(1,65535);networkPort->setObjectName("networkCanPort");
    networkInterface=new QLineEdit(networkBox);networkInterface->setMaxLength(15);networkInterface->setObjectName("networkCanInterface");
    networkMode=new QComboBox(networkBox);networkMode->addItems({tr("RX only"),tr("RX/TX capable (Sending still requires authorization)")});networkMode->setObjectName("networkCanMode");
    networkIdentity=new QSpinBox(networkBox);networkIdentity->setRange(1,2097151);networkIdentity->setObjectName("networkCanIdentity");
    networkSource=new QSpinBox(networkBox);networkSource->setRange(0,251);
    networkToken=new QLineEdit(networkBox);networkToken->setEchoMode(QLineEdit::Password);networkToken->setMaxLength(128);networkToken->setObjectName("networkCanToken");
    networkForm->addRow(tr("Host / IP"),networkHost);networkForm->addRow(tr("TCP port"),networkPort);networkForm->addRow(tr("Remote CAN / virtual interface"),networkInterface);networkForm->addRow(tr("Connection capability"),networkMode);networkForm->addRow(tr("Unique device number"),networkIdentity);networkForm->addRow(tr("Preferred source"),networkSource);networkForm->addRow(tr("TX access key (secure storage)"),networkToken);
    auto *networkNote=new QLabel(tr("Connection opens RX only. Enable Sending authorizes instruments in Generate. Discover Devices / Refresh Routes / Retrieve Waypoints separately authorize bounded route requests in Viewer or Generate. The bridge service lease expires after 60 seconds or 512 frames; reconnect and explicitly authorize a new operation after expiry. Use a distinct device number per computer. Use an SSH tunnel on untrusted networks."),networkBox);networkNote->setWordWrap(true);networkForm->addRow(networkNote);
    auto *networkReconnect=new QPushButton(tr("Connect / reconnect saved network configuration"),networkBox);networkReconnect->setObjectName("networkCanReconnect");networkForm->addRow(networkReconnect);connect(networkReconnect,&QPushButton::clicked,this,&DialogSetup::connectSelectedNetworkCanDevice);
    auto *networkStatus=new QLabel(tr("Network CAN disconnected; TX not authorized"),networkBox);networkStatus->setObjectName("networkCanConnectionStatus");networkStatus->setTextFormat(Qt::PlainText);networkStatus->setWordWrap(true);networkForm->addRow(networkStatus);setNetworkCanStatus(networkStatus->text());
    transportLayout->addWidget(networkBox);networkBox->hide();connect(ui->comboBoxNmea2000Interface,&QComboBox::currentTextChanged,networkBox,[networkBox](const QString &t){networkBox->setVisible(t=="Network NMEA2000");});

    auto *waveshareBox = new QWidget(ui->comboBoxNmea2000Interface->parentWidget());
    auto *waveshareForm = new QFormLayout(waveshareBox);
    wavesharePort = new QComboBox(waveshareBox); wavesharePort->setObjectName("wavesharePort");
    SerialPortDiscovery::initialize(wavesharePort);
    wavesharePort->setAccessibleName(tr("Waveshare serial device"));
    wavesharePort->setToolTip(tr("Select the USB-CAN-A port (2,000,000 baud). Viewer receives in mode 02; mode 00 requires explicit instrument Sending or separately authorized route requests."));
    ui->comboBoxNmea2000Interface->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->comboBoxNmea2000Interface->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    waveshareForm->addRow(tr("Waveshare USB-CAN-A port"), wavesharePort);
    waveshareIdentity = new QSpinBox(waveshareBox); waveshareIdentity->setRange(1, 2097151);
    waveshareIdentity->setObjectName("waveshareIdentity");
    waveshareSource = new QSpinBox(waveshareBox); waveshareSource->setRange(0, 251);
    waveshareSource->setObjectName("waveshareSource");
    waveshareForm->addRow(tr("Unique CAN device number"), waveshareIdentity);
    waveshareForm->addRow(tr("Preferred CAN source address"), waveshareSource);
    waveshareIdentity->setToolTip(tr("Use a distinct number for each simultaneous Waveshare node. This is unrelated to AIS MMSI."));
    waveshareSource->setToolTip(tr("Initial address only; the CAN core claims it and resolves conflicts. The Pi normally uses 43."));
    auto *rxNote = new QLabel(tr("2 Mbaud serial, 250 kbps extended CAN. Connect opens RX/standby mode. Enable Sending authorizes instrument TX in Generate. Discover / Refresh / Retrieve separately authorize bounded route requests in Viewer or Generate. Cancel/Stop/F12 revokes route authorization; reconnect explicitly after an abort or fault. Save transport changes before connecting."), waveshareBox);
    rxNote->setWordWrap(true); waveshareForm->addRow(rxNote);
    auto *reconnect = new QPushButton(tr("Connect Selected Device"), waveshareBox);
    reconnect->setObjectName("waveshareReconnect"); reconnect->setAutoDefault(false);
    reconnect->setToolTip(tr("Connect the saved device for RX/standby. Save changes and Stop Sending first; connection does not authorize Sending."));
    connect(reconnect, &QPushButton::clicked, this, &DialogSetup::connectSelectedSerialCanDevice);
    waveshareForm->addRow(reconnect);
    auto *connectionStatus = new QLabel(waveshareBox);
    connectionStatus->setObjectName("waveshareConnectionStatus");
    connectionStatus->setWordWrap(true); waveshareForm->addRow(connectionStatus);
    ui->comboBoxNmea2000Interface->parentWidget()->layout()->addWidget(waveshareBox);
    connect(ui->comboBoxNmea2000Interface, &QComboBox::currentTextChanged, waveshareBox,
            [waveshareBox](const QString &text) { waveshareBox->setVisible(text == "Waveshare USB-CAN-A"); });
    waveshareBox->hide();

    auto *canableBox = new QWidget(ui->comboBoxNmea2000Interface->parentWidget());
    auto *canableForm = new QFormLayout(canableBox);
    canablePort = new QComboBox(canableBox); canablePort->setObjectName("canablePort");
    SerialPortDiscovery::initialize(canablePort);
    canablePort->setAccessibleName(tr("CANable SLCAN serial device"));
    canablePort->setToolTip(tr("Select or enter the CANable port. Serial: 115200 baud, 8N1; CAN: 250 kbit/s, extended frames. Connection alone does not authorize Sending."));
    ui->comboBoxNmea2000Interface->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->comboBoxNmea2000Interface->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    canableForm->addRow(tr("CANable SLCAN port"), canablePort);
    canableIdentity = new QSpinBox(canableBox); canableIdentity->setRange(1, 2097151);
    canableIdentity->setObjectName("canableIdentity");
    canableSource = new QSpinBox(canableBox); canableSource->setRange(0, 251);
    canableSource->setObjectName("canableSource");
    canableForm->addRow(tr("Unique CAN device number"), canableIdentity);
    canableForm->addRow(tr("Preferred CAN source address"), canableSource);
    canableIdentity->setToolTip(tr("Use a distinct number for each simultaneous CANable node. This is unrelated to AIS MMSI."));
    canableSource->setToolTip(tr("Initial address only; the CAN core claims it and resolves conflicts. The Pi normally uses 43."));
    auto *canableNote = new QLabel(tr("115200 baud SLCAN, 250 kbps extended CAN. Connect opens RX/standby mode. Enable Sending authorizes instrument TX in Generate. Discover / Refresh / Retrieve separately authorize bounded route requests in Viewer or Generate. Cancel/Stop/F12 revokes route authorization; reconnect explicitly after an abort or fault. Save transport changes before connecting."), canableBox);
    canableNote->setWordWrap(true); canableForm->addRow(canableNote);
    auto *canableReconnect = new QPushButton(tr("Connect Selected Device"), canableBox);
    canableReconnect->setObjectName("canableReconnect"); canableReconnect->setAutoDefault(false);
    canableReconnect->setToolTip(reconnect->toolTip());
    connect(canableReconnect, &QPushButton::clicked, this, &DialogSetup::connectSelectedSerialCanDevice);
    canableForm->addRow(canableReconnect);
    auto *canableConnectionStatus = new QLabel(canableBox);
    canableConnectionStatus->setObjectName("canableConnectionStatus");
    canableConnectionStatus->setWordWrap(true); canableForm->addRow(canableConnectionStatus);
    ui->comboBoxNmea2000Interface->parentWidget()->layout()->addWidget(canableBox);
    connect(ui->comboBoxNmea2000Interface, &QComboBox::currentTextChanged, canableBox,
            [canableBox](const QString &text) { canableBox->setVisible(text == "CANable SLCAN"); });
    canableBox->hide();


    auto *signalBox = new QWidget(ui->comboBoxNmea2000Interface->parentWidget());
    auto *signalForm = new QFormLayout(signalBox);
    signalKServer = new QLineEdit(signalBox); signalKServer->setObjectName("signalKServer");
    signalKSource = new QLineEdit(signalBox); signalKSource->setObjectName("signalKSource"); signalKSource->setMaxLength(64);
    signalKToken = new QLineEdit(signalBox); signalKToken->setObjectName("signalKToken"); signalKToken->setEchoMode(QLineEdit::Password);
    signalForm->addRow("Signal K server (http/https)", signalKServer);
    signalForm->addRow("Publisher source label", signalKSource);
    signalForm->addRow("Signal K write token", signalKToken);
    signalKSetup = new SignalKSetupClient(this);
    auto *actions = new QHBoxLayout;
    auto *access = new QPushButton("Request Access", signalBox); access->setObjectName("signalKRequestAccess");
    auto *test = new QPushButton("Test Connection", signalBox); test->setObjectName("signalKTestConnection");
    auto *cancel = new QPushButton("Cancel Request/Test", signalBox); cancel->setObjectName("signalKCancel"); cancel->setEnabled(false);
    for (auto *button : {access, test, cancel}) { button->setAutoDefault(false); actions->addWidget(button); }
    signalForm->addRow(actions);
    auto *status = new QLabel(signalBox); status->setObjectName("signalKSetupStatus"); status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText); signalForm->addRow(status);
    connect(signalKSetup, &SignalKSetupClient::statusChanged, status, &QLabel::setText);
    connect(signalKSetup, &SignalKSetupClient::tokenApproved, signalKToken, &QLineEdit::setText);
    connect(signalKSetup, &SignalKSetupClient::busyChanged, this, [=](bool busy) {
        access->setEnabled(!busy); test->setEnabled(!busy); cancel->setEnabled(busy);
    });
    const auto config = [this] { return SignalKConfig{signalKServer->text().trimmed(), signalKToken->text().trimmed(), signalKSource->text().trimmed()}; };
    connect(access, &QPushButton::clicked, this, [=] { signalKSetup->requestAccess(config()); });
    connect(test, &QPushButton::clicked, this, [=] { signalKSetup->testConnection(config()); });
    connect(cancel, &QPushButton::clicked, signalKSetup, &SignalKSetupClient::cancel);
    for (auto *field : {signalKServer, signalKSource, signalKToken})
        connect(field, &QLineEdit::textEdited, this, [this, status] {
            signalKSetup->cancel(); status->setText("Settings changed. Use Test Connection to verify the current values.");
        });
    connect(ui->comboBoxNmea2000Interface, &QComboBox::currentTextChanged, this, [this] { signalKSetup->cancel(); });
    auto *signalNote = new QLabel("Use a read/write device token approved by the Signal K administrator. Each simultaneous publisher needs a distinct source label. Use HTTPS outside a trusted laboratory network.", signalBox);
    signalNote->setWordWrap(true); signalForm->addRow(signalNote);
    ui->comboBoxNmea2000Interface->parentWidget()->layout()->addWidget(signalBox);
    transportLayout->addItem(ui->verticalSpacer_4);
    connect(ui->comboBoxNmea2000Interface, &QComboBox::currentTextChanged, signalBox,
            [signalBox](const QString &text) { signalBox->setVisible(text == "SignalK"); });
    signalBox->hide();

    InitilizeComboBoxes();
    SerialPortDiscovery::initialize(ui->comboBoxNmea2000ActisensePort);
    auto *serialDiscovery = new QWidget(this);
    auto *discoveryLayout = new QVBoxLayout(serialDiscovery);
    auto *refreshPorts = new QPushButton(tr("Refresh ports"), serialDiscovery);
    refreshPorts->setObjectName("refreshSerialPorts"); refreshPorts->setAutoDefault(false);
    auto *showAll = new QCheckBox(tr("Show other serial ports"), serialDiscovery);
    showAll->setObjectName("showAllSerialPorts");
    auto *discoveryNote = new QLabel(tr("USB serial devices are listed first with available identification. Other ports may be built-in or inactive. Refresh after connecting an adapter. You can enter a device path manually, including a stable device link. Discovery does not prove that a device is a CAN adapter or that it is accessible."), serialDiscovery);
    discoveryNote->setWordWrap(true); discoveryNote->setTextFormat(Qt::PlainText);
    discoveryLayout->addWidget(refreshPorts); discoveryLayout->addWidget(showAll); discoveryLayout->addWidget(discoveryNote);
    transportLayout->insertWidget(transportLayout->count() - 1, serialDiscovery);
    const auto serialVisible = [serialDiscovery](const QString &type) {
        serialDiscovery->setVisible(type == "Actisense" || type == "CANable SLCAN" || type == "Waveshare USB-CAN-A");
    };
    connect(ui->comboBoxNmea2000Interface, &QComboBox::currentTextChanged, this, serialVisible);
    serialVisible(ui->comboBoxNmea2000Interface->currentText());
    connect(refreshPorts, &QPushButton::clicked, this, &DialogSetup::refreshSerialPorts);
    connect(showAll, &QCheckBox::toggled, this, &DialogSetup::refreshSerialPorts);
    refreshSerialPorts();
    InitilizeQtStyleIcons();
    SetupUserInputValidation();
    ui->pushButtonShowOpenWeatherMapKey->setAutoDefault(false);
    // A short text control stays legible in both palettes without a fixed-color bitmap.
    ui->pushButtonShowOpenWeatherMapKey->setIcon(QIcon());
    ui->pushButtonShowOpenWeatherMapKey->setText("Show");
    ui->pushButtonShowOpenWeatherMapKey->setMinimumSize(0, 0);
    ui->pushButtonShowOpenWeatherMapKey->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    ui->pushButtonShowOpenWeatherMapKey->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->pushButtonShowOpenWeatherMapKey->setAccessibleName("Show OpenWeatherMap API key");
    ui->pushButtonShowOpenWeatherMapKey->setToolTip("Show API key (Space to toggle)");
    ui->lineEditOpenWeatherMapKey->setPlaceholderText("Enter API key (stored in OS secure storage)");
    auto *credentialStatus = new QLabel(this);
    credentialStatus->setObjectName("credentialStorageStatus");
    credentialStatus->setWordWrap(true);
    layout()->addWidget(credentialStatus);
    for (auto *combo : {ui->comboBoxUnitsSpeed, ui->comboBoxUnitsVolume, ui->comboBoxUnitsDistance, ui->comboBoxUnitsLatLong})
        connect(combo, &QComboBox::currentIndexChanged, this, [this] { if (!loading) refreshUnitEditors(); });
    auto speedEdit = [this](double value, double MyVessel::*field) {
        if (!loading) { UserUnitPrefs prefs; prefs.speedUnitIndex = speedUnits;
            vessel.*field = N2kHelper::convertUserSpeedToN2kUnits(value, prefs); }
    };
    connect(ui->doubleSpinBoxCrusingSpeed, &QDoubleSpinBox::valueChanged, this, [speedEdit](double v) { speedEdit(v, &MyVessel::CruisingSpeed); });
    connect(ui->doubleSpinBoxWotSpeed, &QDoubleSpinBox::valueChanged, this, [speedEdit](double v) { speedEdit(v, &MyVessel::WotSpeed); });
    auto distanceEdit = [this](double value, double MyVessel::*field) {
        if (!loading) { UserUnitPrefs prefs; prefs.distanceUnitIndex = distanceUnits;
            vessel.*field = N2kHelper::convertUserDistanceNearToN2kUnits(value, prefs); }
    };
    connect(ui->doubleSpinBoxLength, &QDoubleSpinBox::valueChanged, this, [distanceEdit](double v) { distanceEdit(v, &MyVessel::Length); });
    connect(ui->doubleSpinBoxBeam, &QDoubleSpinBox::valueChanged, this, [distanceEdit](double v) { distanceEdit(v, &MyVessel::Beam); });
    connect(ui->doubleSpinBoxHeight, &QDoubleSpinBox::valueChanged, this, [distanceEdit](double v) { distanceEdit(v, &MyVessel::Height); });
    connect(ui->doubleSpinBoxDraft, &QDoubleSpinBox::valueChanged, this, [distanceEdit](double v) { distanceEdit(v, &MyVessel::Draft); });
    auto volumeEdit = [this](double value, double MyVessel::*field) {
        if (!loading) { UserUnitPrefs prefs; prefs.volumeUnitIndex = volumeUnits;
            vessel.*field = N2kHelper::convertUserVolumeToN2kUnits(value, prefs); }
    };
    connect(ui->spinBoxPortFuelTankSize, &QDoubleSpinBox::valueChanged, this, [volumeEdit](double v) { volumeEdit(v, &MyVessel::PortFuelTankSize); });
    connect(ui->spinBoxStarboardFuelTankSize, &QDoubleSpinBox::valueChanged, this, [volumeEdit](double v) { volumeEdit(v, &MyVessel::StbdFuelTankSize); });
    connect(ui->spinBoxPortFuelLevel, &QDoubleSpinBox::valueChanged, this, [volumeEdit](double v) { volumeEdit(v, &MyVessel::PortFuelTankLevel); });
    connect(ui->spinBoxStarboardFuelLevel, &QDoubleSpinBox::valueChanged, this, [volumeEdit](double v) { volumeEdit(v, &MyVessel::StbdFuelTankLevel); });
    connect(ui->comboBoxUnitsTemp, &QComboBox::currentIndexChanged, this, [this] { if (!loading) refreshUnitEditors(); });
    connect(ui->electricalNominal, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (loading) return;
        const int voltage = index == 1 ? 24 : 12;
        vessel.Electrical.initialVoltage *= double(voltage) / vessel.Electrical.nominalVoltage;
        vessel.Electrical.nominalVoltage = voltage; refreshUnitEditors();
    });
    connect(ui->electricalCount, &QSpinBox::valueChanged, this, [this](int value) { if (!loading) { vessel.Electrical.batteryCount = value; refreshUnitEditors(); } });
    connect(ui->electricalType, &QComboBox::currentIndexChanged, this, [this](int value) { if (!loading) vessel.Electrical.batteryType = value; });
    connect(ui->electricalCapacity, &QDoubleSpinBox::valueChanged, this, [this](double value) { if (!loading) vessel.Electrical.capacityC = value * 3600; });
    connect(ui->electricalSoc, &QSpinBox::valueChanged, this, [this](int value) { if (!loading) vessel.Electrical.initialSoc = value; });
    connect(ui->electricalVoltage, &QDoubleSpinBox::valueChanged, this, [this](double value) { if (!loading) vessel.Electrical.initialVoltage = value; });
    connect(ui->electricalTemperature, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!loading) { UserUnitPrefs prefs; prefs.tempUnitIndex = ui->comboBoxUnitsTemp->currentIndex(); vessel.Electrical.temperatureK = N2kHelper::convertUserTemperatureToN2kUnits(value, prefs); }
    });
    connect(ui->electricalSimulate, &QCheckBox::toggled, this, [this](bool value) { if (!loading) vessel.Electrical.simulateNew = value; });
    connect(ui->electricalBattery, &QComboBox::currentIndexChanged, this, [this] { if (!loading) refreshUnitEditors(); });
    connect(ui->electricalEngine, &QComboBox::currentIndexChanged, this, [this](int value) { if (!loading) vessel.Electrical.engineForBattery[ui->electricalBattery->currentIndex()] = value - 1; });
    connect(ui->lineEditDefaultLatitude, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (!ui->lineEditDefaultLatitude->hasAcceptableInput()) return;
        UserUnitPrefs prefs; prefs.latLongUnitIndex = latLongUnits;
        vessel.Latitude = N2kHelper::convertUserLatitudeToN2kUnits(text, prefs);
    });
    connect(ui->lineEditDefaultLongitude, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (!ui->lineEditDefaultLongitude->hasAcceptableInput()) return;
        UserUnitPrefs prefs; prefs.latLongUnitIndex = latLongUnits;
        vessel.Longitude = N2kHelper::convertUserLongitudeToN2kUnits(text, prefs);
    });
    ui->pushButtonClose->setText(tr("Save and Close"));
    ui->pushButtonSave->setToolTip(tr("Apply and save this draft without closing. Stop Sending first; saving does not authorize Sending."));
    ui->pushButtonClose->setToolTip(tr("Save and apply changes, then close. Stop Sending first. Cancel or Escape discards this draft."));
    cancelEdits->setToolTip(tr("Discard unsaved Setup edits and close. Active Sending and saved settings remain unchanged."));
    ui->pushButtonLoad->setToolTip(tr("Discard this draft and reload saved settings and secure credentials. Stop Sending first."));
    ui->checkBoxNmea2000Enabled->setToolTip(tr("Include the selected NMEA2000 output when Sending is explicitly enabled. An unavailable output does not block an independent healthy NMEA0183 output."));
    ui->checkBoxNmea0183Enabled->setToolTip(tr("Include the selected NMEA0183 serial output when Sending is explicitly enabled. Select a valid port; sentences also require their individual Enable selection."));
    ui->comboBoxNmea2000Interface->setToolTip(tr("Choose the transport; Save applies it after Stop Sending. Route requests require separate authorization. Network CAN has Axiom route acceptance; serial CAN and SocketCAN physical route tests remain pending."));
    signalKServer->setToolTip(tr("Signal K HTTP/HTTPS server address. Test Connection checks readiness without enabling measurement Sending."));
    signalKSource->setToolTip(tr("Distinct source label for this publisher. Use different labels for simultaneous simulators."));
    signalKToken->setToolTip(tr("Administrator-approved read/write device token, stored in the operating system credential store on Save."));
    access->setToolTip(tr("Request publisher access; an administrator must approve before the device token is retrieved into this draft."));
    test->setToolTip(tr("Check authorization and the WebSocket handshake using these draft settings. Does not publish measurements or enable Sending."));
    ui->lineEditOpenWeatherMapKey->setToolTip(tr("OpenWeatherMap API key. Save stores it in the operating system credential store; NOAA does not require this key."));
    ui->comboBoxVesselEngine->setToolTip(tr("Default engine profile for new generators. Running engines and saved Layout selections are not silently changed."));
    ui->checkBoxEnableNewWidgets->setToolTip(tr("Initial individual Enable selection for newly added instruments. This does not authorize Sending."));
    ui->tabUnits->setToolTip(tr("Preferred instrument units. Property Editor values keep their stored units; Save applies display preferences."));
    // Only draft fields participate; telemetry and window activation never mark Setup dirty.
    const auto changed = [this] { if (sessionOpen && !loading) dirty = true; };
    for (auto *field : findChildren<QLineEdit *>())
        connect(field, &QLineEdit::textChanged, this, changed);
    for (auto *field : findChildren<QComboBox *>())
        if (field != ui->electricalBattery) // Inspecting an association is navigation, not an edit.
            connect(field, &QComboBox::currentTextChanged, this, changed);
    for (auto *field : findChildren<QAbstractButton *>())
        if (field->isCheckable() && field != ui->pushButtonShowOpenWeatherMapKey && field->objectName() != "showAllSerialPorts")
            connect(field, &QAbstractButton::toggled, this, changed);
    for (auto *field : findChildren<QSpinBox *>())
        connect(field, &QSpinBox::valueChanged, this, changed);
    for (auto *field : findChildren<QDoubleSpinBox *>())
        connect(field, &QDoubleSpinBox::valueChanged, this, changed);
}

DialogSetup::~DialogSetup()
{
    signalKSetup->cancel();
    delete ui;
}

void DialogSetup::setSendingControls(QAction *stop, std::function<bool()> active)
{
    sendingActive = std::move(active);
    auto *button = new QToolButton(this);
    button->setObjectName("setupStopSending"); button->setDefaultAction(stop);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("F12 stops Sending in any application window or Qt dialog."), this));
    row->addStretch(); row->addWidget(button); layout()->addItem(row);
}

void DialogSetup::setCredentialSnapshot(const CredentialStore::Result &signal, const CredentialStore::Result &weather)
{
    // In-memory snapshot only. Activation while Sending never waits on the OS keyring.
    signalKCredential = signal; weatherCredential = weather; haveCredentials = true;
}

void DialogSetup::readCredentials()
{
    setCredentialSnapshot(CredentialStore::read("SignalKToken"), CredentialStore::read("OpenWeatherMapApiKey"));
}

bool DialogSetup::requireStopped()
{
    if (!sendingActive || !sendingActive()) return true;
    findChild<QLabel *>("credentialStorageStatus")->setText(tr(
        "Stop Sending before saving or reloading Setup. Your edits are retained. Use Stop Sending or F12."));
    return false;
}

void DialogSetup::setNetworkCanStatus(const QString &status)
{
    networkConnectionStatus = status;
    findChild<QLabel*>("networkCanConnectionStatus")->setText(status);
}

void DialogSetup::connectSelectedNetworkCanDevice()
{
    auto *status = findChild<QLabel*>("networkCanConnectionStatus");
    const auto explain = [&](const QString &message) {
        status->setText(networkConnectionStatus + "\n" + message);
    };
    if (!requireStopped()) {
        explain(tr("Stop Sending before reconnecting. Use Stop Sending or F12."));
        return;
    }
    AppSettings settings(APP_COMPANY, APP_NAME);
    settings.beginGroup("Nmea2000");
    const auto saved = NetworkCanConfig::load(settings);
    if (!ui->checkBoxNmea2000Enabled->isChecked() ||
        ui->comboBoxNmea2000Interface->currentText() != "Network NMEA2000" ||
        !settings.value("Nmea2000Enabled").toBool() ||
        settings.value("InterfaceType").toString() != "Network NMEA2000" ||
        networkHost->text().trimmed() != saved.host || networkPort->value() != saved.port ||
        networkInterface->text().trimmed() != saved.interfaceName ||
        networkIdentity->value() != int(saved.uniqueNumber) ||
        networkSource->value() != saved.preferredSource ||
        (networkMode->currentIndex() == 1) != saved.allowTx ||
        networkToken->text() != loadedNetworkToken) {
        explain(tr("Save the enabled Network NMEA2000 configuration before connecting. Unsaved edits are drafts."));
        return;
    }
    emit ConnectNetworkCanDevice(); // Live session signals supply the actual result.
}

void DialogSetup::connectSelectedSerialCanDevice()
{
    const QString type = ui->comboBoxNmea2000Interface->currentText();
    const bool canable = type == "CANable SLCAN";
    if (!canable && type != "Waveshare USB-CAN-A") return;
    auto *status = findChild<QLabel *>(canable ? "canableConnectionStatus" : "waveshareConnectionStatus");
    if (!requireStopped()) {
        status->setText(tr("Stop Sending before connecting. Your Setup edits are retained. Use Stop Sending or F12."));
        return;
    }
    AppSettings settings(APP_COMPANY, APP_NAME);
    settings.beginGroup("Nmea2000");
    const QString prefix = canable ? "Canable" : "Waveshare";
    const auto identity = WaveshareConfig::load(settings, prefix);
    const QString port = (canable ? canablePort : wavesharePort)->currentText().trimmed();
    // An explicit connect must never silently use a different saved device or
    // apply unrelated Setup drafts. Save is the existing reconfiguration boundary.
    if (!ui->checkBoxNmea2000Enabled->isChecked()
        || !settings.value("Nmea2000Enabled", false).toBool()
        || settings.value("InterfaceType").toString() != type
        || settings.value(prefix + "Port").toString().trimmed() != port
        || (canable ? canableIdentity : waveshareIdentity)->value() != int(identity.uniqueNumber)
        || (canable ? canableSource : waveshareSource)->value() != identity.preferredSource) {
        status->setText(tr("Save the selected NMEA2000 interface, enabled state, port and CAN identity before connecting. Other unsaved Setup edits remain drafts."));
        return;
    }
    if (port.isEmpty() || port == "None" || !identity.valid()) {
        status->setText(tr("Select a serial port and valid CAN identity, then Save before connecting."));
        return;
    }
    status->setText(tr("Connection requested for %1. Sending remains OFF. Inspect the transport status for connection errors; physical CAN communication is not established by a serial connection.").arg(type));
    emit ConnectSerialCanDevice();
}

void DialogSetup::setSerialCanStatus(bool canable, const QString &status)
{
    auto *label = findChild<QLabel *>(canable ? "canableConnectionStatus" : "waveshareConnectionStatus");
    label->setTextFormat(Qt::PlainText);
    label->setText(status);
}

void DialogSetup::refreshSerialPorts()
{
    const auto devices = SerialPortDiscovery::available();
    const bool showAll = findChild<QCheckBox *>("showAllSerialPorts")->isChecked();
    for (auto *box : {wavesharePort, canablePort, ui->comboBoxNmea2000ActisensePort})
        SerialPortDiscovery::refresh(box, devices, showAll);
}

void DialogSetup::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!sessionOpen) {
        loadSettingGeometry();
        loadSavedSettings();
        refreshSerialPorts();
        initilizeUnits();
        dirty = false; sessionOpen = true;
        WindowState::ensureVisible(*this);
    }
}

void DialogSetup::hideEvent(QHideEvent *event)
{
    ui->pushButtonShowOpenWeatherMapKey->setChecked(false);
    on_pushButtonShowOpenWeatherMapKey_clicked(false);
    signalKSetup->cancel();
    QDialog::hideEvent(event);
}

void DialogSetup::finishSession()
{
    signalKSetup->cancel(); saveSettingGeometry();
    sessionOpen = false; dirty = false;
}

void DialogSetup::reject()
{
    finishSession();
    QDialog::reject(); // Escape/Cancel discards drafts; never invokes the save-on-close path.
}

void DialogSetup::closeEvent(QCloseEvent *event)
{
    // Preserve existing Save-and-Close semantics. An unchanged close does not reconfigure.
    if (dirty) {
        if (!saveSettings()) { event->ignore(); return; }
        emit ReloadSettings();
    }
    finishSession(); event->accept();
}

bool DialogSetup::resolveForShutdown()
{
    if (!sessionOpen) return true;
    if (dirty) {
        const auto answer = QMessageBox::question(this, tr("Unsaved Setup changes"),
            tr("Save Setup changes before exiting? Stop Sending before saving. Discard leaves saved settings unchanged."),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer == QMessageBox::Cancel) return false;
        if (answer == QMessageBox::Save) {
            if (!saveSettings()) { show(); raise(); activateWindow(); return false; }
            emit ReloadSettings();
        }
    }
    reject(); return true;
}

void DialogSetup::on_pushButtonLoad_clicked()
{
    if (!requireStopped()) return;
    readCredentials(); loadSavedSettings(); initilizeUnits(); dirty = false;
}

void DialogSetup::on_pushButtonSave_clicked()
{
    if (!saveSettings()) return;
    emit ReloadSettings();
    emit ShowStatusBarMessage("Setup Settings Saved and Hot-reloading", 5000);
}

void DialogSetup::on_pushButtonClose_clicked() { close(); }

void DialogSetup::on_pushButtonFont_clicked()
{
    bool ok;
    QFont font;
    //font.setFamily(font.defaultFamily());//
    font = QFontDialog::getFont(&ok, QFont(m_fontFamily, m_fontSize, m_fontWeight, m_fontItalic),
                                this, tr("Choose Application Font"));
    if (ok) {
        dirty = true;
        m_fontFamily = font.family();
        m_fontSize = font.pointSize();
        m_fontWeight = font.weight();
        m_fontItalic = font.italic();

        // Stage the selection; Save/Close applies it and Cancel leaves the application unchanged.
    }
}

void DialogSetup::InitilizeComboBoxes()
{
    QStringList ports = helper::GetCommPorts();
    QStringList baudRates = helper::GetBaudRates();
    ui->comboBoxNmea2000Interface->addItems(nnea2000Interfaces);
    ui->comboBoxNmea2000ActisensePort->addItems(ports);
    ui->comboBoxNmea2000ActisenseBaudRate->addItems(baudRates);
    ui->comboBoxNmea2000ActisensePreamble->addItems(actisensePreamble);
    ui->comboBoxNmea0183Port->addItems(ports);
    ui->comboBoxNmea0183BaudRate->addItems(baudRates);

    // Units
    ui->comboBoxUnitsCurrency->addItem("USD ($)");
    ui->comboBoxUnitsCurrency->setToolTip("USD only; currency conversion is not implemented.");
    ui->comboBoxUnitsTemp->addItems(unitsTemp);
    ui->comboBoxUnitsPressure->addItems(unitsPressure);
    ui->comboBoxUnitsFlowRate->addItems(unitsFlowRate);
    ui->comboBoxUnitsVolume->addItems(unitsVolume);
    ui->comboBoxUnitsSpeed->addItems(unitsSpeed);
    ui->comboBoxUnitsDepth->addItems(unitsDepth);
    ui->comboBoxUnitsLatLong->addItems(UnitsLatLong);
    ui->comboBoxUnitsDistance->addItems(UnitsDistance);
    ui->comboBoxUnitsFuelEconomy->addItems(UnitsEconomy);
    ui->comboBoxUnitsDate->addItems(UnitsDate);
    ui->comboBoxUnitsTime->addItems(UnitsTime);

    QStringList engineList = helper::getEngineList(RuntimeResources::engineProfiles());
    ui->comboBoxVesselEngine->addItem("None");
    ui->comboBoxVesselEngine->addItems(engineList);
}

void DialogSetup::refreshEngineProfiles(const QString &selection)
{
    const QString selected = selection.isNull() ? ui->comboBoxVesselEngine->currentText() : selection;
    const auto names = helper::getEngineList(RuntimeResources::engineProfiles());
    const QSignalBlocker blocker(ui->comboBoxVesselEngine);
    ui->comboBoxVesselEngine->clear();
    ui->comboBoxVesselEngine->addItem("None");
    ui->comboBoxVesselEngine->addItems(names);
    if (ui->comboBoxVesselEngine->findText(selected, Qt::MatchExactly | Qt::MatchCaseSensitive) < 0)
        ui->comboBoxVesselEngine->addItem(selected);
    ui->comboBoxVesselEngine->setCurrentText(selected);
    ui->comboBoxVesselEngine->setToolTip(selected == "None" || names.contains(selected) ? tr("Default profile for new engine generators; existing engines and saved Layout selections retain their configuration.") :
        tr("Selected engine profile is unavailable. Select an existing profile or restore its original name in Engine Profiles."));
}

void DialogSetup::InitilizeQtStyleIcons()
{
    QIcon iconEdit = QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton);
    QIcon iconCancel = QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton);
    QIcon iconSave = QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton);
    QIcon iconClose = QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton);
    QIcon iconAbout = QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
    QIcon iconNew = QApplication::style()->standardIcon(QStyle::SP_FileDialogNewFolder);
    QIcon iconDelete = QApplication::style()->standardIcon(QStyle::SP_DialogDiscardButton);
    //QIcon iconAdd = QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton);

    ui->pushButtonSave->setIcon(iconSave);
    ui->pushButtonClose->setIcon(iconClose);
}

void DialogSetup::SetupUserInputValidation()
{
    ui->lineEditOpenWeatherMapKey->setEchoMode(QLineEdit::Password);
}

void DialogSetup::loadSettingGeometry()
{
    AppSettings settings(APP_COMPANY, APP_NAME);
    settings.beginGroup("Geometry");
    const QByteArray geometry = settings.value("SetupGeometry", QByteArray()).toByteArray();

    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);

        // Check if the restored geometry is fully visible on any screen
        bool geometryIsOffScreen = true;
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen->geometry().contains(this->frameGeometry())) {
                geometryIsOffScreen = false;
                break;
            }
        }

        // If the geometry is off-screen, move the dialog to the center of the primary screen
        if (geometryIsOffScreen) {
            QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();
            int x = screenGeometry.x() + (screenGeometry.width() - this->width()) / 2;
            int y = screenGeometry.y() + (screenGeometry.height() - this->height()) / 2;
            this->move(x, y);
        }
    } else {
        // If no saved geometry, center the dialog by default
        QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();
        int x = screenGeometry.x() + (screenGeometry.width() - this->width()) / 2;
        int y = screenGeometry.y() + (screenGeometry.height() - this->height()) / 2;
        this->move(x, y);
    }

    settings.endGroup();
}

void DialogSetup::saveSettingGeometry()
{
    AppSettings settings(APP_COMPANY, APP_NAME);
    settings.beginGroup("Geometry");
    settings.setValue("SetupGeometry", saveGeometry());
    settings.endGroup();
}

void DialogSetup::loadSavedSettings()
{
    loggingPreferences->load();
    setNetworkCanStatus(networkConnectionStatus);
    ui->pushButtonShowOpenWeatherMapKey->setChecked(false);
    on_pushButtonShowOpenWeatherMapKey_clicked(false);
    QScopedValueRollback<bool> guard(loading, true);
    signalKSetup->cancel();
    findChild<QLabel*>("signalKSetupStatus")->setText("Settings loaded. Request Access or Test Connection to continue.");
    AppSettings settings(APP_COMPANY, APP_NAME);

    settings.beginGroup("Vessel");
    vessel = readVesselSettings(settings);
    ui->vesselType->setCurrentIndex(ui->vesselType->findData(vessel.VesselType));
    ui->radioCallsign->setText(vessel.RadioCallsign);
    ui->vesselMmsi->setText(settings.value("MMSI",QString::number(vessel.MMSI)).toString());
    refreshEngineProfiles(vessel.EngineProfile);
    ui->spinBoxVesselNumEngines->setValue(vessel.NumEngines);
    ui->environmentalInfluence->setChecked(vessel.EnvironmentalInfluence);
    settings.endGroup();

    settings.beginGroup("Nmea2000");
    signalKServer->setText(settings.value("SignalKServer", "http://127.0.0.1:3000").toString());
    signalKSource->setText(settings.value("SignalKSource", "raynmeasimulator").toString());
    if (!haveCredentials) readCredentials();
    signalKToken->setText(loadedSignalKToken = signalKCredential.value);
    const auto net=NetworkCanConfig::load(settings);
    networkHost->setText(net.host);networkPort->setValue(net.port);networkInterface->setText(net.interfaceName);networkMode->setCurrentIndex(net.allowTx?1:0);networkIdentity->setValue(net.uniqueNumber);networkSource->setValue(net.preferredSource);networkToken->setText(loadedNetworkToken=net.token);
    const auto can=SocketCanSession::loadConfig(settings);
    socketCanName->setText(can.interfaceName);socketCanIdentity->setValue(can.uniqueNumber);
    wavesharePort->setCurrentText(settings.value("WavesharePort", "None").toString());
    const auto wave = WaveshareConfig::load(settings);
    canablePort->setCurrentText(settings.value("CanablePort", "None").toString());
    const auto canable = WaveshareConfig::load(settings, "Canable");
    canableIdentity->setValue(canable.uniqueNumber); canableSource->setValue(canable.preferredSource);
    waveshareIdentity->setValue(wave.uniqueNumber); waveshareSource->setValue(wave.preferredSource);
    ui->checkBoxNmea2000Enabled->setChecked(settings.value("Nmea2000Enabled", false).toBool());
    ui->comboBoxNmea2000Interface->setCurrentText(settings.value("InterfaceType", "None").toString());
    ui->comboBoxNmea2000ActisensePort->setCurrentText(settings.value("ActisensePort", "None").toString());
    ui->comboBoxNmea2000ActisenseBaudRate->setCurrentText(settings.value("ActisenseBaudRate", "115200").toString());
    ui->comboBoxNmea2000ActisensePreamble->setCurrentText(settings.value("ActisensePreamble", "0x94").toString());
    settings.endGroup();

    settings.beginGroup("Nmea0183");
    ui->checkBoxNmea0183Enabled->setChecked(settings.value("Nmea0183Enabled", false).toBool());
    ui->comboBoxNmea0183Port->setCurrentText(settings.value("Nmea0183Port", "None").toString());
    ui->comboBoxNmea0183BaudRate->setCurrentText(settings.value("Nmea0183BaudRate", "4800").toString());
    settings.endGroup();

    settings.beginGroup("Preference");
    QFont font;
    font = QApplication::font();
    m_fontFamily = settings.value("FontFamily", font.family()).toString();
    m_fontSize = settings.value("FontSize", 10).toInt();
    m_fontWeight = settings.value("FontWeight", QFont::Normal).toInt();
    m_fontItalic = settings.value("FontItalic", false).toBool();
    ui->checkBoxShowTooltips->setChecked(settings.value("ShowTooltips", true).toBool());
    lastStyleSheet = settings.value("StyleSheetName", "Dark").toString();
    ui->comboBoxAppTheme->clear();
    ui->comboBoxAppTheme->addItems(Themes::available());
    for (int i=0;i<ui->comboBoxAppTheme->count();++i) {
        const auto name=ui->comboBoxAppTheme->itemText(i);
        ui->comboBoxAppTheme->setItemData(i,Themes::builtInThemes().contains(name)
            ? tr("Supported built-in theme") : tr("External custom theme"),Qt::ToolTipRole);
    }
    const auto selected=Themes::selectionName(lastStyleSheet);
    ui->comboBoxAppTheme->setCurrentText(selected);
    QString themeTip=tr("System follows the Qt platform appearance. Choose a supported built-in or external custom theme. "
        "Add or edit custom QSS in %1; reopen Setup to refresh. Save applies the selection. "
        "Missing or invalid themes use Dark; custom files are never deleted.").arg(QDir::toNativeSeparators(AppPaths::themesDirectory()));
    if (lastStyleSheet!=selected && !Themes::isSystem(lastStyleSheet))
        themeTip+=tr(" Saved theme '%1' is unavailable; Dark is displayed. The saved preference is retained until Save.").arg(lastStyleSheet);
    ui->comboBoxAppTheme->setToolTip(themeTip);
    const auto prefs = readUnitPreferences(settings);
    ui->comboBoxUnitsTemp->setCurrentIndex(prefs.tempUnitIndex);
    ui->comboBoxUnitsPressure->setCurrentIndex(prefs.pressureUnitIndex);
    ui->comboBoxUnitsFlowRate->setCurrentIndex(prefs.flowRateUnitIndex);
    ui->comboBoxUnitsVolume->setCurrentIndex(prefs.volumeUnitIndex);
    ui->comboBoxUnitsSpeed->setCurrentIndex(prefs.speedUnitIndex);
    ui->comboBoxUnitsDepth->setCurrentIndex(prefs.depthUnitIndex);
    ui->comboBoxUnitsLatLong->setCurrentIndex(prefs.latLongUnitIndex);
    ui->comboBoxUnitsDistance->setCurrentIndex(prefs.distanceUnitIndex);
    ui->comboBoxUnitsFuelEconomy->setCurrentIndex(prefs.fuelEconomyUnitIndex);
    ui->comboBoxUnitsCurrency->setCurrentIndex(prefs.currencyUnitIndex);
    ui->comboBoxUnitsTime->setCurrentIndex(prefs.timeUnitIndex);
    ui->comboBoxUnitsDate->setCurrentIndex(prefs.dateUnitIndex);
    const auto options = readOptionPreferences(settings);
    ui->checkBoxEnableNewWidgets->setChecked(options.newWidgetsEnabled);
    ui->checkBoxPromptSaveLayoutOnClose->setChecked(options.promptSaveLayoutOnClose);
    ui->spinBoxWidgetColumns->setValue(options.widgetColumns);
    ui->spinBoxWidgetBorderWidth->setValue(options.borderWidth);
    settings.endGroup();

    settings.beginGroup("APIs");
    // Loaded together in readCredentials(), or supplied by MainWindow at a stopped boundary.
    ui->lineEditOpenWeatherMapKey->setText(loadedWeatherKey = weatherCredential.value);
    settings.endGroup();
    QStringList notices {"Credentials: " + CredentialStore::backendName() + ". Save or Save and Close stores changes; Cancel/Escape discards drafts."};
    if (!signalKCredential.success) notices << "Signal K: " + signalKCredential.error;
    if (!weatherCredential.success) notices << "Weather: " + weatherCredential.error;
    if ((!signalKCredential.found && !settings.value("Credentials/SignalKTokenConfigured").toBool() && !settings.value("Nmea2000/SignalKToken").toString().isEmpty())
        || (!weatherCredential.found && !settings.value("Credentials/OpenWeatherMapApiKeyConfigured").toBool() && !settings.value("APIs/OpenWeatherMapApiKey").toString().isEmpty()))
        notices << "Legacy credentials were preserved but are no longer used. Re-enter the weather key or use Signal K Request Access, then Save to secure storage. Keep the original settings and .secret.key for recovery.";
    findChild<QLabel*>("credentialStorageStatus")->setText(notices.join("\n"));

}

bool DialogSetup::saveSettings()
{


    if (!requireStopped()) return false;
    if(!AisClassBStatic129809::validField("UserID",ui->vesselMmsi->text())) {
        findChild<QLabel*>("credentialStorageStatus")->setText(tr("Vessel MMSI was not saved. Use 0 (unconfigured), a nine-digit simulated MMSI, or 4294967295 (unavailable). No spaces or leading zeroes."));
        ui->tabWidget->setCurrentWidget(ui->tabVessel);ui->vesselMmsi->setFocus();return false;
    }
    const auto type = QString::number(ui->vesselType->currentData().toInt());
    if (!AisClassBStatic129810::editableField("VesselType", type)
        || !AisClassBStatic129810::validField("Callsign", ui->radioCallsign->text())) {
        findChild<QLabel*>("credentialStorageStatus")->setText(tr(
            "Vessel identity was not saved. Choose vessel type 0–99 or Unavailable; callsign must contain at most seven uppercase AIS characters (ASCII 32–95, excluding @)."));
        ui->tabWidget->setCurrentWidget(ui->tabVessel);
        if (!AisClassBStatic129810::validField("Callsign", ui->radioCallsign->text())) ui->radioCallsign->setFocus();
        else ui->vesselType->setFocus();
        return false;
    }
    // Do not overwrite an unavailable credential with the empty placeholder shown on load.
    QString error;
    const QString token = signalKToken->text().trimmed();
    const QString weather = ui->lineEditOpenWeatherMapKey->text().trimmed();
    AppSettings settings(APP_COMPANY, APP_NAME);
    if (!settings.isWritable()) {
        findChild<QLabel*>("credentialStorageStatus")->setText("Settings are not writable. No credential changes were attempted.");
        return false;
    }
    const bool tokenChanged=token!=loadedSignalKToken, weatherChanged=weather!=loadedWeatherKey;
    struct Change {QString name,value;CredentialStore::Result previous;};
    QVector<Change> changes;
    const auto addChange=[&](const QString &name,const QString &value,const QString &old){if(value!=old)changes.append({name,value,CredentialStore::read(name)});};
    addChange("SignalKToken",token,loadedSignalKToken);addChange("OpenWeatherMapApiKey",weather,loadedWeatherKey);addChange("NetworkNmea2000Token",networkToken->text(),loadedNetworkToken);
    for(const auto &change:changes)if(!change.previous.success){findChild<QLabel*>("credentialStorageStatus")->setText(tr("Credentials unavailable; nothing saved."));return false;}
    int applied=0;for(const auto &change:changes){if(!CredentialStore::write(change.name,change.value,&error)){
        bool restored=true;for(int i=applied-1;i>=0;--i)restored=CredentialStore::write(changes[i].name,changes[i].previous.value)&&restored;
        if(!restored){loadedSignalKToken=CredentialStore::read("SignalKToken").value;loadedWeatherKey=CredentialStore::read("OpenWeatherMapApiKey").value;loadedNetworkToken=CredentialStore::read("NetworkNmea2000Token").value;}
        findChild<QLabel*>("credentialStorageStatus")->setText(error+(restored?QString():tr(" Credential rollback incomplete; retry Save.")));return false;
    }++applied;}
    loadedNetworkToken=networkToken->text();

    settings.beginGroup("Vessel");
    settings.setValue("EnvironmentalInfluence", ui->environmentalInfluence->isChecked());
    settings.setValue("EngineProfile", ui->comboBoxVesselEngine->currentText());
    settings.setValue("NumEngines", ui->spinBoxVesselNumEngines->value());
    settings.setValue("CruisingSpeed", vessel.CruisingSpeed);
    settings.setValue("WotSpeed", vessel.WotSpeed);
    settings.setValue("PortFuelTankSize", vessel.PortFuelTankSize);
    settings.setValue("StarboardFuelTankSize", vessel.StbdFuelTankSize);
    settings.setValue("PortFuelLevel", vessel.PortFuelTankLevel);
    settings.setValue("StarboardFuelLevel", vessel.StbdFuelTankLevel);
    settings.setValue("MMSI",ui->vesselMmsi->text().toUInt());
    settings.setValue("VesselType", ui->vesselType->currentData());
    settings.setValue("RadioCallsign", ui->radioCallsign->text());
    settings.setValue("Length", vessel.Length);
    settings.setValue("Beam", vessel.Beam);
    settings.setValue("Height", vessel.Height);
    settings.setValue("Draft", vessel.Draft);
    settings.setValue("Latitude", vessel.Latitude);
    settings.setValue("Longitude", vessel.Longitude);
    writeVesselElectricalSettings(settings, vessel.Electrical);
    settings.endGroup();

    settings.beginGroup("Nmea2000");
    settings.setValue("Nmea2000Enabled", ui->checkBoxNmea2000Enabled->isChecked());
    settings.setValue("InterfaceType", ui->comboBoxNmea2000Interface->currentText());
    settings.setValue("SignalKServer", signalKServer->text().trimmed());
    settings.setValue("SignalKSource", signalKSource->text().trimmed());
    settings.setValue("NetworkCanHost",networkHost->text().trimmed());settings.setValue("NetworkCanPort",networkPort->value());settings.setValue("NetworkCanInterface",networkInterface->text().trimmed());settings.setValue("NetworkCanAllowTx",networkMode->currentIndex()==1);settings.setValue("NetworkCanUniqueNumber",networkIdentity->value());settings.setValue("NetworkCanPreferredSource",networkSource->value());
    settings.setValue("SocketCanInterface",socketCanName->text());
    settings.setValue("SocketCanUniqueNumber",socketCanIdentity->value());
    settings.setValue("CanablePort", canablePort->currentText().trimmed());
    settings.setValue("CanableUniqueNumber", canableIdentity->value());
    settings.setValue("CanablePreferredSource", canableSource->value());
    settings.setValue("WavesharePort", wavesharePort->currentText().trimmed());
    settings.setValue("WaveshareUniqueNumber", waveshareIdentity->value());
    settings.setValue("WavesharePreferredSource", waveshareSource->value());
    settings.setValue("ActisensePort", ui->comboBoxNmea2000ActisensePort->currentText());
    settings.setValue("ActisenseBaudRate", ui->comboBoxNmea2000ActisenseBaudRate->currentText());
    settings.setValue("ActisensePreamble", ui->comboBoxNmea2000ActisensePreamble->currentText());
    settings.endGroup();

    settings.beginGroup("Nmea0183");
    settings.setValue("Nmea0183Enabled", ui->checkBoxNmea0183Enabled->isChecked());
    settings.setValue("Nmea0183Port", ui->comboBoxNmea0183Port->currentText());
    settings.setValue("Nmea0183BaudRate", ui->comboBoxNmea0183BaudRate->currentText());
    settings.endGroup();

    settings.beginGroup("Preference");
    settings.setValue("ShowTooltips", ui->checkBoxShowTooltips->isChecked());
    settings.setValue("StyleSheetName", ui->comboBoxAppTheme->currentText());
    settings.setValue("FontFamily", m_fontFamily);
    settings.setValue("FontSize", m_fontSize);
    settings.setValue("FontWeight", m_fontWeight);
    settings.setValue("FontItalic", m_fontItalic);
    settings.setValue("UnitsTemp", ui->comboBoxUnitsTemp->currentIndex());
    settings.setValue("UnitsPressure", ui->comboBoxUnitsPressure->currentIndex());
    settings.setValue("UnitsFlowRate", ui->comboBoxUnitsFlowRate->currentIndex());
    settings.setValue("UnitsVolume", ui->comboBoxUnitsVolume->currentIndex());
    settings.setValue("UnitsSpeed", ui->comboBoxUnitsSpeed->currentIndex());
    settings.setValue("UnitsLatLong", ui->comboBoxUnitsLatLong->currentIndex());
    settings.setValue("UnitsDistance", ui->comboBoxUnitsDistance->currentIndex());
    settings.setValue("UnitsDepth", ui->comboBoxUnitsDepth->currentIndex());
    settings.setValue("UnitsFuelEconomy", ui->comboBoxUnitsFuelEconomy->currentIndex());
    settings.setValue("UnitsCurrency", ui->comboBoxUnitsCurrency->currentIndex());
    settings.setValue("UnitsTime", ui->comboBoxUnitsTime->currentIndex());
    settings.setValue("UnitsDate", ui->comboBoxUnitsDate->currentIndex());
    settings.setValue("EnableNewWidgets", ui->checkBoxEnableNewWidgets->isChecked());
    settings.setValue("PromptSaveLayoutOnClose", ui->checkBoxPromptSaveLayoutOnClose->isChecked());
    settings.setValue("WidgetColumns", ui->spinBoxWidgetColumns->value());
    settings.setValue("WidgetBorderWidth", ui->spinBoxWidgetBorderWidth->value());
    settings.endGroup();

    if (token != loadedSignalKToken) settings.setValue("Credentials/SignalKTokenConfigured", true);
    if (weather != loadedWeatherKey) settings.setValue("Credentials/OpenWeatherMapApiKeyConfigured", true);
    loggingPreferences->value().save(settings);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        findChild<QLabel*>("credentialStorageStatus")->setText("Settings could not be saved. Check settings storage permissions and retry.");
        return false;
    }
    ApplicationLog::configure(loggingPreferences->value());
    ApplicationLog::registerSecret(token); ApplicationLog::registerSecret(weather);
    loadedSignalKToken = token; loadedWeatherKey = weather;
    if (tokenChanged) signalKCredential = {true, !token.isEmpty(), token, {}};
    if (weatherChanged) weatherCredential = {true, !weather.isEmpty(), weather, {}};
    dirty = false;
    setNetworkCanStatus(networkConnectionStatus);
    return true;
}

void DialogSetup::initilizeUnits() { refreshUnitEditors(); }

void DialogSetup::refreshUnitEditors()
{
    QScopedValueRollback<bool> guard(loading, true);
    speedUnits = ui->comboBoxUnitsSpeed->currentIndex();
    volumeUnits = ui->comboBoxUnitsVolume->currentIndex();
    distanceUnits = ui->comboBoxUnitsDistance->currentIndex();
    latLongUnits = ui->comboBoxUnitsLatLong->currentIndex();
    UserUnitPrefs prefs;
    prefs.speedUnitIndex = speedUnits; prefs.volumeUnitIndex = volumeUnits;
    prefs.distanceUnitIndex = distanceUnits; prefs.latLongUnitIndex = latLongUnits;
    const auto& e = vessel.Electrical;
    ui->electricalNominal->setCurrentIndex(e.nominalVoltage == 24 ? 1 : 0);
    ui->electricalCount->setValue(e.batteryCount); ui->electricalType->setCurrentIndex(e.batteryType);
    ui->electricalCapacity->setValue(e.capacityC / 3600); ui->electricalSoc->setValue(e.initialSoc);
    ui->electricalVoltage->setRange(e.nominalVoltage * .5, e.nominalVoltage * 1.3); ui->electricalVoltage->setValue(e.initialVoltage);
    prefs.tempUnitIndex = ui->comboBoxUnitsTemp->currentIndex();
    ui->electricalTemperature->setRange(N2kHelper::convertN2kTemperatureToUserUnits(213.15, prefs), N2kHelper::convertN2kTemperatureToUserUnits(523.15, prefs));
    ui->electricalTemperature->setValue(N2kHelper::convertN2kTemperatureToUserUnits(e.temperatureK, prefs));
    ui->electricalTemperatureUnit->setText(QStringList{"K", "°C", "°F"}[prefs.tempUnitIndex]);
    ui->electricalSimulate->setChecked(e.simulateNew);
    const int battery = qBound(0, ui->electricalBattery->currentIndex(), e.batteryCount - 1);
    ui->electricalBattery->clear(); for (int i = 0; i < e.batteryCount; ++i) ui->electricalBattery->addItem(QString::number(i + 1), i);
    ui->electricalBattery->setCurrentIndex(battery); ui->electricalEngine->setCurrentIndex(e.engineForBattery.value(battery, -1) + 1);
    ui->labelUnitsWotSpeed->setText(unitsSpeed.at(speedUnits));
    ui->labelUnitsLBHD->setText(distanceUnits == 0 ? "m" : "ft");
    ui->labelUnitsPortFuelTank->setText(volumeUnits == 0 ? "L" : "gal");
    ui->labelUnitsStarboardFuelTank->setText(volumeUnits == 0 ? "L" : "gal");
    ui->labelUnitsPortFuelLevel->setText(volumeUnits == 0 ? "L" : "gal");
    ui->labelUnitsStarboardFuelLevel->setText(volumeUnits == 0 ? "L" : "gal");
    ui->doubleSpinBoxCrusingSpeed->setValue(N2kHelper::convertN2kSpeedToUserUnits(vessel.CruisingSpeed, prefs));
    ui->doubleSpinBoxWotSpeed->setValue(N2kHelper::convertN2kSpeedToUserUnits(vessel.WotSpeed, prefs));
    ui->doubleSpinBoxLength->setValue(N2kHelper::convertN2kDistanceNearToUserUnits(vessel.Length, prefs));
    ui->doubleSpinBoxBeam->setValue(N2kHelper::convertN2kDistanceNearToUserUnits(vessel.Beam, prefs));
    ui->doubleSpinBoxHeight->setValue(N2kHelper::convertN2kDistanceNearToUserUnits(vessel.Height, prefs));
    ui->doubleSpinBoxDraft->setValue(N2kHelper::convertN2kDistanceNearToUserUnits(vessel.Draft, prefs));
    ui->spinBoxPortFuelTankSize->setValue(N2kHelper::convertN2kVolumeToUserUnits(vessel.PortFuelTankSize, prefs));
    ui->spinBoxStarboardFuelTankSize->setValue(N2kHelper::convertN2kVolumeToUserUnits(vessel.StbdFuelTankSize, prefs));
    ui->spinBoxPortFuelLevel->setValue(N2kHelper::convertN2kVolumeToUserUnits(vessel.PortFuelTankLevel, prefs));
    ui->spinBoxStarboardFuelLevel->setValue(N2kHelper::convertN2kVolumeToUserUnits(vessel.StbdFuelTankLevel, prefs));
    setCoordinateValidator(ui->lineEditDefaultLatitude, latLongUnits, true);
    setCoordinateValidator(ui->lineEditDefaultLongitude, latLongUnits, false);
    ui->lineEditDefaultLatitude->setText(N2kHelper::convertN2kLatitudeToUserUnits(vessel.Latitude, prefs));
    ui->lineEditDefaultLongitude->setText(N2kHelper::convertN2kLongitudeToUserUnits(vessel.Longitude, prefs));
    ui->labelUnitsDefaultLatitude->setText(UnitsLatLong.at(latLongUnits));
    ui->labelUnitsDefaultLongitude->setText(UnitsLatLong.at(latLongUnits));
}

void DialogSetup::on_pushButtonShowOpenWeatherMapKey_clicked(bool checked)
{
    ui->lineEditOpenWeatherMapKey->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    const QString action = checked ? "Hide" : "Show";
    ui->pushButtonShowOpenWeatherMapKey->setText(action);
    ui->pushButtonShowOpenWeatherMapKey->setToolTip(action + " API key (Space to toggle)");
    ui->pushButtonShowOpenWeatherMapKey->setAccessibleName(action + " OpenWeatherMap API key");
}
