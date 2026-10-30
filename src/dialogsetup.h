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

#ifndef DIALOGSETUP_H
#define DIALOGSETUP_H

#include <QDialog>
#include <functional>
class QAction;
class LoggingPreferences;
#include "datastructs.h"
#include "credentialstore.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class SignalKSetupClient;
namespace Ui {
class DialogSetup;
}

class DialogSetup : public QDialog
{
    Q_OBJECT

public:
    explicit DialogSetup(QWidget *parent = nullptr);
    ~DialogSetup();
    void setSendingControls(QAction *stop, std::function<bool()> sendingActive);
    void setCredentialSnapshot(const CredentialStore::Result &signal, const CredentialStore::Result &weather);
    void setNetworkCanStatus(const QString &status);
    void setSerialCanStatus(bool canable, const QString &status);
    bool resolveForShutdown();
    void reject() override;
    void refreshEngineProfiles(const QString &selection = QString());

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void hideEvent(QHideEvent *event) override;

signals:
    void ReloadSettings();
    void ConnectSerialCanDevice();
    void ConnectNetworkCanDevice();
    void ShowStatusBarMessage(QString message, int timeout);

private slots:
    void on_pushButtonLoad_clicked();
    void on_pushButtonSave_clicked();
    void on_pushButtonClose_clicked();
    void on_pushButtonFont_clicked();

    void on_pushButtonShowOpenWeatherMapKey_clicked(bool checked);

private:
    Ui::DialogSetup *ui;
    LoggingPreferences *loggingPreferences;
    SignalKSetupClient *signalKSetup;
    QLineEdit *signalKServer, *signalKSource, *signalKToken;
    QLineEdit *networkHost,*networkInterface,*networkToken;
    QSpinBox *networkPort,*networkIdentity,*networkSource;
    QComboBox *networkMode;
    QString loadedNetworkToken;
    QString networkConnectionStatus;
    void connectSelectedNetworkCanDevice();
    QLineEdit *socketCanName;
    QSpinBox *socketCanIdentity;
    QComboBox *wavesharePort;
    QComboBox *canablePort;
    QSpinBox *canableIdentity, *canableSource;
    QSpinBox *waveshareIdentity, *waveshareSource;

    bool sessionOpen = false, dirty = false, haveCredentials = false;
    std::function<bool()> sendingActive;
    bool requireStopped();
    void connectSelectedSerialCanDevice();
    void refreshSerialPorts();
    void readCredentials();
    void finishSession();
    void InitilizeComboBoxes();
    void InitilizeQtStyleIcons();
    void SetupUserInputValidation();
    void loadSettingGeometry();
    void saveSettingGeometry();
    void loadSavedSettings();
    bool saveSettings();
    void initilizeUnits();

    QString lastStyleSheet = "";
    QString m_fontFamily = "Segoe UI";
    int m_fontSize = 10;
    int m_fontWeight = QFont::Normal;
    bool m_fontItalic = false;

    QList<QString> unitsTemp = {"Kelvin (K)", "Celsius (°C)", "Fahrenheit (°F)"};
    QList<QString> unitsPressure = {"Pa", "kPa", "Bar", "PSI"};
    QList<QString> unitsFlowRate = {"LPH", "GPH"};
    QList<QString> unitsVolume = {"Liters (L)", "Gallons (gal)"};
    QList<QString> unitsSpeed = {"m/s", "km/h", "MPH", "knots"};
    QList<QString> unitsDepth = {"Meters", "Feet", "Fathoms"};
    QList<QString> UnitsLatLong = {"DD.DDDD", "DD°MM’SS", "DD°MM.MMM’"};
    QList<QString> UnitsDistance = {"km & m", "NM & ft", "mi & ft"};
    QList<QString> UnitsEconomy = {"L/km", "L/nm", "gal/nm", "MPG", "nm/L", "nm/gal"};
    QList<QString> UnitsRateOfTurn = {"rad/s", "deg/s"};
    QList<QString> UnitsTime = {"HH:MM", "HH:MM AM/PM", "HH:MM:SS", "HH:MM:SS AM/PM"};
    QList<QString> UnitsDate = {"MM/DD/YYYY", "DD/MM/YYYY", "YYYY/MM/DD"};

    QList<QString> nnea2000Interfaces = {"None", "Actisense", "SocketCan", "SignalK", "Waveshare USB-CAN-A", "CANable SLCAN", "Network NMEA2000"};
    QList<QString> actisensePreamble = {"0x93", "0x94"};

    int volumeUnits = 1; //Default is gallons
    int speedUnits = 2;  //Default is MPH
    int distanceUnits = 2; //Default is ft
    int latLongUnits = 2;  //Default is DD°MM.MMM’

    MyVessel vessel;
    bool loading = false;
    QString loadedSignalKToken, loadedWeatherKey;
    CredentialStore::Result signalKCredential, weatherCredential;
    void refreshUnitEditors();

};

#endif // DIALOGSETUP_H
