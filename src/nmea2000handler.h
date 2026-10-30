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

#ifndef NMEA2000HANDLER_H
#define NMEA2000HANDLER_H
#define DEFAULT_SENDER_ID 43

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include "datastructs.h"
#include "socketcansession.h"
#include "wavesharesession.h"
#include "networkcansession.h"
#include "signalkpublisher.h"
#include "nmea2000_actisense.h"

class Nmea2000Diagnostics;
class NmeaDeviceDiscovery;
class Nmea2000Handler : public QObject {
    Q_OBJECT

public:
    // An injected serial port is owned by this handler; tests use an in-memory port.
    explicit Nmea2000Handler(QObject *parent = nullptr, QSerialPort *port = nullptr, SocketCanBackend *canBackend = nullptr,
                             WaveshareSession::PortFactory waveshareFactory = {});
    ~Nmea2000Handler();

    Nmea2000Diagnostics *diagnostics() const { return diagnosticState; }
    NmeaDeviceDiscovery *deviceDiscovery() const { return discovery; }
    static void HandleN2kRcvdMsg(const tN2kMsg &N2kMsg);
    void SetInterfaceType(QString interfaceType);
    // Snapshot of the currently selected adapter, independent of queued notices.
    QString transportStatus() const;
    bool InitializeActisense(const QString &portName, QSerialPort::BaudRate baudRate = QSerialPort::Baud115200, QString preamble = "0x94");
    void ShutdownActisense();
    bool IsActisensePortOpen();
    void InitializeSocketCan(const SocketCanConfig &config = {});
    void StopSocketCanSending();
    void ShutdownSocketCan();
    void ConfigureWaveshare(const QString &portName, const WaveshareConfig &identity = {});
    void StartWaveshareSending();
    bool IsWaveshareReady() const;
    bool IsWaveshareConfigured() const;
    void StopWaveshare(bool forgetConfiguration = false);
    void ReconnectWaveshare();
    void ConnectSerialCanDevice();
    bool IsWaveshareActive() const;
    void ConfigureSignalK(const SignalKConfig &config);
    void StartSignalKSending();
    void StopSignalKSending();
    bool IsSignalKReady() const;
    bool IsSignalKActive() const;
    bool IsCanInterfaceOpen();
    bool IsCanInterfaceUp(const QString &interfaceName);

    bool SendN2kProductInformation();
    bool sendTestPgn129026();
    bool sendTestPgn129033();
    bool sendCurrentTimePgn129033();

    bool SendPgn126992(unsigned char &SID, uint16_t &SystemDate, double &SystemTime, tN2kTimeSource &TimeSource);
    bool RcvdPgn126992(const tN2kMsg &N2kMsg);

    bool SendPgn127245(double &RudderPosition, unsigned char &Instance, tN2kRudderDirectionOrder &RudderDirectionOrder, double &AngleOrder);
    bool RcvdPgn127245(const tN2kMsg &N2kMsg);

    bool SendPgn127250(unsigned char &SID, double &Heading, double &Deviation, double &Variation, tN2kHeadingReference &HeadingReference);
    bool RcvdPgn127250(const tN2kMsg &N2kMsg);

    bool SendPgn127251(unsigned char &SID, double &RateOfTurn);
    bool RcvdPgn127251(const tN2kMsg &N2kMsg);

    bool SendPgn127252(unsigned char &SID, double &Heave, double &Delay, tN2kDelaySource &DelaySource);
    bool RcvdPgn127252(const tN2kMsg &N2kMsg);

    bool SendPgn127257(unsigned char &SID, double &Yaw, double &Pitch, double &Roll);
    bool RcvdPgn127257(const tN2kMsg &N2kMsg);

    bool SendPgn127258(unsigned char &SID, tN2kMagneticVariation &Source, uint16_t &DaysSince1970, double &Variation);
    bool RcvdPgn127258(const tN2kMsg &N2kMsg);

    bool SendPgn127488(unsigned char &EngineInstance, double &EngineSpeed, double &EngineBoostPressure, int &EngineTiltTrim);
    bool RcvdPgn127488(const tN2kMsg &N2kMsg);

    bool SendPgn127489(unsigned char &EngineInstance,
                       double &EngineOilPress,
                       double &EngineOilTemp,
                       double &EngineCoolantTemp,
                       double &AlternatorVoltage,
                       double &FuelRate,
                       double &EngineHours,
                       double &EngineCoolantPress,
                       double &EngineFuelPress,
                       double &EngineLoad,
                       double &EngineTorque,
                       tN2kEngineDiscreteStatus1 &Status1,
                       tN2kEngineDiscreteStatus2 &Status2);
    bool RcvdPgn127489(const tN2kMsg &N2kMsg);

    bool SendPgn127493(
        unsigned char &EngineInstance, tN2kTransmissionGear &TransmissionGear, double &OilPressure, double &OilTemperature, unsigned char &DiscreteStatus1);
    bool RcvdPgn127493(const tN2kMsg &N2kMsg);

    bool SendPgn127505(unsigned char &Instance, tN2kFluidType &FluidType, double &Level, double &Capacity);
    bool RcvdPgn127505(const tN2kMsg &N2kMsg);

    bool SendPgn127506(unsigned char &SID,
                       unsigned char &DCInstance,
                       tN2kDCType &DCType,
                       uint8_t &StateOfCharge,
                       uint8_t &StateOfHealth,
                       double &TimeRemaining,
                       double &RippleVoltage,
                       double &Capacity);
    bool RcvdPgn127506(const tN2kMsg &N2kMsg);

    bool SendPgn127507(unsigned char &Instance,
                       unsigned char &BatteryInstance,
                       tN2kChargeState &ChargeState,
                       tN2kChargerMode &ChargerMode,
                       tN2kOnOff &Enabled,
                       tN2kOnOff &EqualizationPending,
                       double &EqualizationTimeRemaining);
    bool RcvdPgn127507(const tN2kMsg &N2kMsg);

    bool SendPgn127508(unsigned char &BatteryInstance, double &BatteryVoltage, double &BatteryCurrent, double &BatteryTemperature, unsigned char &SID);
    bool RcvdPgn127508(const tN2kMsg &N2kMsg);

    bool SendPgn128259(unsigned char &SID, double &WaterReferenced, double &GroundReferenced, tN2kSpeedWaterReferenceType &SWRT);
    bool RcvdPgn128259(const tN2kMsg &N2kMsg);

    bool SendPgn128267(unsigned char &SID, double &DepthBelowTransducer, double &Offset, double &Range);
    bool RcvdPgn128267(const tN2kMsg &N2kMsg);

    bool SendPgn129025(double &Latitude, double &Longitude);
    bool RcvdPgn129025(const tN2kMsg &N2kMsg);

    bool SendPgn129026(unsigned char &SID, tN2kHeadingReference &ref, double &COG, double &SOG);
    bool RcvdPgn129026(const tN2kMsg &N2kMsg);

    bool SendPgn129283(unsigned char &SID, tN2kXTEMode &XTEMode, bool &NavigationTerminated, double &XTE);
    bool RcvdPgn129283(const tN2kMsg &N2kMsg);

    bool SendPgn129284(unsigned char &SID,
                       double &DistanceToWaypoint,
                       tN2kHeadingReference &BearingReference,
                       bool &PerpendicularCrossed,
                       bool &ArrivalCircleEntered,
                       tN2kDistanceCalculationType &CalculationType,
                       double &ETATime,
                       int16_t &ETADate,
                       double &BearingOriginToDestinationWaypoint,
                       double &BearingPositionToDestinationWaypoint,
                       uint32_t &OriginWaypointNumber,
                       uint32_t &DestinationWaypointNumber,
                       double &DestinationLatitude,
                       double &DestinationLongitude,
                       double &WaypointClosingVelocity);
    bool RcvdPgn129284(const tN2kMsg &N2kMsg);

    bool SendPgn129029(unsigned char &SID,
                       uint16_t &DaysSince1970,
                       double &SecondsSinceMidnight,
                       double &Latitude,
                       double &Longitude,
                       double &Altitude,
                       tN2kGNSStype &GNSStype,
                       tN2kGNSSmethod &GNSSmethod,
                       unsigned char &nSatellites,
                       double &HDOP,
                       double &PDOP,
                       double &GeoidalSeparation,
                       unsigned char &nReferenceStations,
                       tN2kGNSStype &ReferenceStationType,
                       uint16_t &ReferenceSationID,
                       double &AgeOfCorrection);
    bool RcvdPgn129029(const tN2kMsg &N2kMsg);

    bool SendPgn129033(uint16_t &DaysSince1970, double &SecondsSinceMidnight, int16_t &LocalOffset);
    bool RcvdPgn129033(const tN2kMsg &N2kMsg);

    bool SendPgn130310(unsigned char &SID, double &WaterTemperature, double &OutsideAmbientAirTemperature, double &AtmosphericPressure);
    bool RcvdPgn130310(const tN2kMsg &N2kMsg);
    bool RcvdEnvironmental(const tN2kMsg &message);

    bool SendPgn130311(
        unsigned char &SID, tN2kTempSource &TempSource, double &Temperature, tN2kHumiditySource &HumiditySource, double &Humidity, double &AtmosphericPressure);
    bool RcvdPgn130311(const tN2kMsg &N2kMsg);

    bool SendPgn130312(unsigned char &SID, double &TemperatureInstance, tN2kTempSource &TemperatureSource, double &ActualTemperature, double &SetTemperature);
    bool RcvdPgn130312(const tN2kMsg &N2kMsg);

    bool SendPgn130316(unsigned char &SID, unsigned char &TempInstance, tN2kTempSource &TempSource, double &ActualTemperature, double &SetTemperature);
    bool RcvdPgn130316(const tN2kMsg &N2kMsg);

    bool SendPgn130576(int8_t &PortTrimTab, int8_t &StbdTrimTab);
    bool RcvdPgn130576(const tN2kMsg &N2kMsg);

    bool SendPgn(const QString &pgnLabel, const QList<QList<QString>> &paramDetails);
    bool sendPgn(int pgn, const QStringList &paramList, const QHash<QString,QDateTime> &sampleTimes = {});

    NetworkCanSession *networkCan() const {return networkSession;}
    bool beginRouteService();
    void endRouteService(bool abort=false);
    bool routeServiceReady() const;
    int routeServiceSource() const;
    bool sendRouteService(const tN2kMsg &message);
    bool routeServiceActive() const;
    void setMode(Mode mode);
    static Nmea2000Handler *getInstance();

public slots:
    void nmea2000MessageReceived(const tN2kMsg &N2kMsg);

private slots:


signals:
    void RouteServiceMessage(const tN2kMsg &message);
    void RouteServiceRevoked();
    void CanSourceIdentityObserved(const QString &source,const QString &name);
    void InterfaceStateChanged();
    void TransmitError(const QString &message);
    void SignalKStatus(const QString &status);
    void SocketCanStatus(const QString &status);
    void WaveshareStatus(const QString &status);
    void RcvdPgn(const QString &pgnLabel, const QStringList &paramDetails, const QString &source = "255");

private:
    static Nmea2000Handler* instance;
    QSerialPort *serialPort;
    quint64 adapterGeneration = 0;
    NMEA2000_Actisense *nmea2000Actisense;
    SocketCanSession *socketSession;
    NetworkCanSession *networkSession;
    WaveshareSession *waveshare;
    WaveshareConfig waveshareIdentity;
    QString wavesharePort;
    bool waveshareConfigured = false;
    SignalKPublisher *signalK;

    bool routeAuthorized=false;
    NmeaDeviceDiscovery *discovery = nullptr;
    NetworkCanSession *discoveryNetwork = nullptr;
    bool discoverySocketLease = false;
    Mode mode = Sender;
    QString interfaceType = "Actisense";
    QString signalKStatus = "Signal K: stopped";

    bool sendActisenseMessage(const tN2kMsg &message);
    bool sendCanMessage(const tN2kMsg &N2kMsg);

    Nmea2000Diagnostics *diagnosticState;
    void synchronizeDiagnosticContext();
};

#endif // NMEA2000HANDLER_H
