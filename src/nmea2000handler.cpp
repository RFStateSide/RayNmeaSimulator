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

#include "applicationlog.h"
#include "nmea2000diagnostics.h"
#include "nmeadevicediscovery.h"
#include "serialportdiagnostics.h"
#include "expansioninstruments.h"
#include "windlassstatus.h"
#include "aisclassbposition129039.h"
#include "aisclassbstatic129810.h"
#include "aisclassbstatic129809.h"
#include <cmath>
#include <limits>
#include "nmea2000handler.h"
#include "transportcapabilities.h"
#include "routeservicepolicy.h"
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QThread>
#include <QTimeZone>
#include <QtGlobal>
#include "N2kMessages.h"
#include "N2kTypes.h"



namespace {
// Legacy pinned parsers check PGN but synthesize NA for reads beyond DataLen.
// Require the bytes actually consumed; trailing reserved/extension bytes are allowed.
bool hasReceivePayload(const tN2kMsg &message, unsigned long pgn, int minimum)
{
    return message.PGN == pgn && message.DataLen >= minimum
        && message.DataLen <= tN2kMsg::MaxDataLen;
}
QString receivedCoordinate(double value)
{
    // Round-trip the decoded canonical double, not the preferred-unit display.
    return QString::number(value, 'g', std::numeric_limits<double>::max_digits10);
}
}

extern NMEA2000_Actisense nmea2000Actisense;

Nmea2000Handler* Nmea2000Handler::instance = nullptr;

Nmea2000Handler::Nmea2000Handler(QObject *parent, QSerialPort *port, SocketCanBackend *canBackend,
                               WaveshareSession::PortFactory waveshareFactory)
    : QObject(parent)
    , nmea2000Actisense(nullptr)
{
    instance = this;
    diagnosticState = new Nmea2000Diagnostics(this);
    networkSession=new NetworkCanSession(this);
    connect(networkSession,&NetworkCanSession::received,this,&Nmea2000Handler::nmea2000MessageReceived);
    connect(networkSession,&NetworkCanSession::statusChanged,this,[this]{emit InterfaceStateChanged();});
    connect(networkSession,&NetworkCanSession::failed,this,[this](const QString &m){
        // Catalogue/completed clients no longer poll readiness. Revoke the
        // session explicitly so a stale candidate cannot survive a lost lease.
        if(interfaceType=="Network NMEA2000")endRouteService(true);
        emit TransmitError(m);emit InterfaceStateChanged();
    });
    signalK = new SignalKPublisher(this);
    connect(signalK, &SignalKPublisher::statusChanged, this, [this](const QString &text) {
        signalKStatus=text;
        emit SignalKStatus(text);
    });
    socketSession = new SocketCanSession(this, canBackend);
    connect(socketSession,&SocketCanSession::received,this,&Nmea2000Handler::nmea2000MessageReceived);
    connect(socketSession,&SocketCanSession::statusChanged,this,&Nmea2000Handler::SocketCanStatus);
    connect(socketSession,&SocketCanSession::statusChanged,this,[this]{
        const auto state=socketSession->state();
        if(interfaceType=="SocketCan" && (state==SocketCanSession::Failed ||
            state==SocketCanSession::Closed || state==SocketCanSession::ShuttingDown))
            endRouteService(true);
    });
    waveshare = new WaveshareSession(this, std::move(waveshareFactory));
    connect(waveshare, &WaveshareSession::statusChanged, this, &Nmea2000Handler::WaveshareStatus);
    connect(waveshare, &WaveshareSession::received, this, &Nmea2000Handler::nmea2000MessageReceived);
    connect(waveshare, &WaveshareSession::failed, this, [this](const QString &text) {
        if(interfaceType=="CANable SLCAN" || interfaceType=="Waveshare USB-CAN-A")
            endRouteService(true);
        if (mode == Sender) emit TransmitError(text);
        else emit InterfaceStateChanged();
    });
    serialPort = port ? port : new QSerialPort;
    serialPort->setParent(this);
    connect(serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (interfaceType != "Actisense" || error == QSerialPort::NoError) return;
        const QString message = "Actisense serial error: " + serialPortDiagnostic(serialPort->portName(), error, serialPort->errorString());
        if (error == QSerialPort::ResourceError || error == QSerialPort::ReadError || error == QSerialPort::WriteError)
            if (serialPort->isOpen()) serialPort->close();
        emit TransmitError(message);
        emit InterfaceStateChanged();
    });

    connect(this,&Nmea2000Handler::InterfaceStateChanged,this,&Nmea2000Handler::synchronizeDiagnosticContext);
    connect(this,&Nmea2000Handler::SocketCanStatus,this,[this]{synchronizeDiagnosticContext();});
    connect(this,&Nmea2000Handler::WaveshareStatus,this,[this]{synchronizeDiagnosticContext();});
    connect(this,&Nmea2000Handler::RcvdPgn,this,[this](const QString &pgn,const QStringList &values,const QString &source){diagnosticState->markApplicationDecoded(pgn,source,values);});
    synchronizeDiagnosticContext();

    discovery = new NmeaDeviceDiscovery(diagnosticState,this);
    discoveryNetwork = new NetworkCanSession(this);
    discoveryNetwork->setObjectName("Device discovery management");
    discoveryNetwork->setRawReceptionLogging(false); // Primary RX already records the same bus frames.
    discovery->open = [this]() -> QString {
        if(routeAuthorized) return tr("Close the chartplotter route-service session before discovering devices.");
        if(interfaceType=="Network NMEA2000") {
            if(!networkSession->connected() || networkSession->openingOrActiveTx())
                return tr("Connect Network NMEA2000 in RX and stop instrument Sending before discovery.");
            const auto config=networkSession->configuration();
            if(!config.allowTx || config.token.isEmpty())
                return tr("Discovery requires Network NMEA2000 TX permission and its existing secure access key in Setup. Instrument Sending stays off.");
            discoveryNetwork->configure(config,"service");
            return {};
        }
        if(interfaceType=="SocketCan") {
            if(socketSession->state()!=SocketCanSession::Ready || !socketSession->routeServiceReady())
                return tr("Discovery requires an already connected, healthy SocketCAN interface with a claimed source address.");
            discoverySocketLease=true;socketSession->authorizeService(true);return {};
        }
        if(interfaceType=="Actisense") return tr("Diagnostics discovery is not supported by the Actisense serial management path. Release TX remains disabled; Debug TX remains experimental.");
        return tr("Diagnostics discovery is currently supported on Network NMEA2000 and SocketCAN. Serial adapters retain their existing RX/TX and route-service workflows.");
    };
    discovery->ready=[this] {return interfaceType=="Network NMEA2000" ? discoveryNetwork->ready() : discoverySocketLease&&socketSession->routeServiceReady();};
    discovery->source=[this] {return interfaceType=="Network NMEA2000" ? discoveryNetwork->sourceAddress() : socketSession->sourceAddress();};
    discovery->send=[this](const tN2kMsg &m) {
        // Deliberately narrower than the route-service policy: identity ISO requests only.
        if(m.PGN!=59904 || m.DataLen!=3 || !RouteServicePolicy::allowed(m))return false;
        const bool accepted=interfaceType=="Network NMEA2000" ? discoveryNetwork->send(m,true) : discoverySocketLease&&socketSession->sendService(m);
        ApplicationLog::n2k("TX",interfaceType,m,accepted?"device-discovery-software-accepted":"device-discovery-rejected",discovery->source());
        return accepted;
    };
    discovery->release=[this] {
        discoveryNetwork->stop();
        if(discoverySocketLease){discoverySocketLease=false;socketSession->authorizeService(false);}
    };
    connect(discoveryNetwork,&NetworkCanSession::failed,discovery,[this](const QString &reason){discovery->cancel(tr("Device discovery failed: ")+reason);});
    connect(this,&Nmea2000Handler::RouteServiceMessage,discovery,&NmeaDeviceDiscovery::ingest);
    connect(discovery,&NmeaDeviceDiscovery::changed,this,[this]{emit InterfaceStateChanged();});

}

Nmea2000Handler::~Nmea2000Handler()
{
    discovery->cancel();
    networkSession->stop();
    signalK->stop();
    socketSession->shutdown();
    waveshare->stop();
    ShutdownActisense();
    if (instance == this) instance = nullptr;
}

void Nmea2000Handler::HandleN2kRcvdMsg(const tN2kMsg &N2kMsg)
{
    if (instance) {
        instance->nmea2000MessageReceived(N2kMsg);
    }

}

void Nmea2000Handler::SetInterfaceType(QString interfaceType)
{
    if (this->interfaceType != interfaceType) {
        endRouteService(true);
        networkSession->stop();
        setProperty("windlassSignalKUnsupported",false);
        signalK->stop();
        ShutdownActisense();
        if (this->interfaceType == "SocketCan") socketSession->shutdown();
        waveshare->stop(); waveshareConfigured = false;
    }
    this->interfaceType = interfaceType;
    waveshare->setProtocol(interfaceType == "CANable SLCAN" ? WaveshareSession::CanableSlcan : WaveshareSession::Waveshare);
    synchronizeDiagnosticContext();
}

QString Nmea2000Handler::transportStatus() const
{
    if (interfaceType=="Network NMEA2000") return networkSession->status();
    if (interfaceType=="SocketCan") return socketSession->status();
    if (interfaceType=="Waveshare USB-CAN-A" || interfaceType=="CANable SLCAN") return waveshare->status();
    if (interfaceType=="SignalK") return signalKStatus;
    if (interfaceType=="Actisense")
        return serialPort->isOpen()?tr("Actisense: serial port open"):tr("Actisense: disconnected");
    return tr("No NMEA2000 interface selected");
}

bool Nmea2000Handler::InitializeActisense(const QString &portName, QSerialPort::BaudRate baudRate, QString preamble)
{
    ShutdownActisense();
    if (interfaceType != "Actisense" || portName.trimmed().isEmpty() || portName == "None") return false;
    serialPort->setPortName(portName);
    if (!serialPort->setBaudRate(baudRate) || !serialPort->setDataBits(QSerialPort::Data8)
        || !serialPort->setParity(QSerialPort::NoParity) || !serialPort->setStopBits(QSerialPort::OneStop)
        || !serialPort->setFlowControl(QSerialPort::NoFlowControl)) return false;
    nmea2000Actisense = new NMEA2000_Actisense(*serialPort, this);
    nmea2000Actisense->SetMode(tNMEA2000::N2km_ListenAndNode, 44);
    nmea2000Actisense->setPreamble(preamble);
    const auto generation = adapterGeneration;
    connect(nmea2000Actisense, &NMEA2000_Actisense::nmea2000MessageReceived, this,
            [this, generation](const tN2kMsg &message) {
        if (generation == adapterGeneration && mode == Viewer) nmea2000MessageReceived(message);
    });
    // SetMode above initializes device/source state. BST-93/94 serial framing
    // does not use inherited CAN buffers, address claiming or ParseMessages.
    // Open() can return false merely because its CAN startup deadline is not due.
    // CANOpen is this adapter's explicit QSerialPort open, with no scheduler gate.
    if (!nmea2000Actisense->CANOpen()) {
        qWarning() << "Actisense initialization failed";
        ShutdownActisense();
        return false;
    }
    emit InterfaceStateChanged();
    return true;
}

void Nmea2000Handler::ShutdownActisense()
{
    ++adapterGeneration;
    if (nmea2000Actisense) disconnect(nmea2000Actisense, nullptr, this, nullptr);
    if (serialPort->isOpen()) serialPort->close();
    delete nmea2000Actisense;
    nmea2000Actisense = nullptr;
    emit InterfaceStateChanged();
}

bool Nmea2000Handler::IsActisensePortOpen()
{
    return serialPort->isOpen();
}

void Nmea2000Handler::InitializeSocketCan(const SocketCanConfig &config)
{
    if (interfaceType == "SocketCan") socketSession->start(config);
}
void Nmea2000Handler::ShutdownSocketCan() { endRouteService(true); socketSession->shutdown(); }
void Nmea2000Handler::ConfigureWaveshare(const QString &name, const WaveshareConfig &identity) {
    endRouteService(true);
    wavesharePort = name; waveshareIdentity = identity; waveshareConfigured = true;
    if ((interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN") && mode == Viewer) waveshare->start(name);
    else waveshare->stop(tr("Serial CAN stopped. Connect Selected Device opens RX/standby; Enable Sending explicitly starts Generate TX."));
}
void Nmea2000Handler::StopWaveshare(bool forgetConfiguration) {
    endRouteService(true);
    waveshare->stop();
    if (forgetConfiguration) waveshareConfigured = false;
}
bool Nmea2000Handler::IsWaveshareActive() const {
    return waveshare->state() == WaveshareSession::Configuring
        || waveshare->state() == WaveshareSession::AwaitingTraffic
        || waveshare->state() == WaveshareSession::Receiving
        || waveshare->state() == WaveshareSession::Claiming
        || waveshare->state() == WaveshareSession::Transmitting;
}
bool Nmea2000Handler::IsWaveshareReady() const { return waveshare->isTransmitting(); }
bool Nmea2000Handler::IsWaveshareConfigured() const {
    return waveshareConfigured && waveshareIdentity.valid() && !wavesharePort.trimmed().isEmpty() && wavesharePort != "None";
}
void Nmea2000Handler::StartWaveshareSending() {
    if ((interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN") && mode == Sender && waveshareConfigured)
        waveshare->startTransmitting(wavesharePort, waveshareIdentity);
}
void Nmea2000Handler::ReconnectWaveshare() {
    if ((interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN") && waveshareConfigured && mode == Viewer)
        waveshare->start(wavesharePort);
}
void Nmea2000Handler::ConnectSerialCanDevice() {
    // Connection is deliberately independent of transmission authorization.
    // A running TX session must be explicitly stopped, never downgraded here.
    const auto state = waveshare->state();
    if ((interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")
        && IsWaveshareConfigured() && state != WaveshareSession::Configuring
        && state != WaveshareSession::Claiming && state != WaveshareSession::Transmitting)
        waveshare->start(wavesharePort);
}
void Nmea2000Handler::StopSocketCanSending() { endRouteService(true);networkSession->revoke();socketSession->stopSending(); }
bool Nmea2000Handler::IsCanInterfaceOpen() { return socketSession->state()==SocketCanSession::Ready; }
bool Nmea2000Handler::IsCanInterfaceUp(const QString &name)
{
    QProcess process;
    process.start("ip", {"-details", "-json", "link", "show", "dev", name});
    if (!process.waitForFinished(1500)) { process.kill(); process.waitForFinished(); return false; }
    return process.exitCode()==0 && SocketCanSession::validateLink(name,process.readAllStandardOutput()).isEmpty();
}

bool Nmea2000Handler::SendN2kProductInformation()
{
    tN2kMsg N2kMsg;
    unsigned int N2kVersion = 2000;
    unsigned int ProductCode = 4;
    const char *ModelID = APP_NAME;
    const char *SwCode = APP_VERSION;
    const char *ModelVersion = "Beta";
    const char *ModelSerialCode = "00000001";
    unsigned char CertificationLevel = 0x00;
    unsigned char LoadEquivalency = 0x01;

    SetN2kPGN126996(N2kMsg,
                    N2kVersion,
                    ProductCode,
                    ModelID,
                    SwCode,
                    ModelVersion,
                    ModelSerialCode,
                    CertificationLevel,
                    LoadEquivalency);

    // Send the message via NMEA2000
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "ProductInformation sent successfully";
        } else {
            qWarning() << "Failed to send ProductInformation";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send ProductInformation";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendN2kProductInformation' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::sendTestPgn129026()
{
    // Dummy data for PGN 129026 (COG & SOG, Rapid Update)
    unsigned char SID = 0; // System Instance
    double COG = 1.2345;   // Course over ground (in radians)
    double SOG = 2.3456; // Speed over ground (in m/s)
    tN2kHeadingReference COGReference = N2khr_magnetic;

    return SendPgn129026(SID, COGReference, COG, SOG);
}

bool Nmea2000Handler::sendTestPgn129033()
{
    // Dummy data for PGN 129033 (Time & Date)
    // Assume current date is October 7, 2024, at 3:00 PM in Boston
    QDate date(2024, 10, 7);
    QTime time(15, 0, 0);                // 3:00 PM
    QTimeZone bostonTimeZone(-4 * 3600); // Offset of -4 hours for Boston (UTC -4)

    // Create QDateTime with specified time zone
    QDateTime dateTime(date, time, bostonTimeZone);

    // Calculate DaysSince1970
    QDate epochStart(1970, 1, 1);
    uint16_t DaysSince1970 = static_cast<uint16_t>(epochStart.daysTo(date));

    // Calculate SecondsSinceMidnight with 0.0001 s resolution
    double SecondsSinceMidnight = ((time.hour() * 3600) + (time.minute() * 60) + time.second()) /* * 10000*/;

    // Calculate LocalOffset
    int16_t LocalOffset = -240; // Boston is UTC -4 hours (240 minutes behind UTC)

    return SendPgn129033(DaysSince1970, SecondsSinceMidnight, LocalOffset);
}

bool Nmea2000Handler::sendCurrentTimePgn129033()
{
    // Get the current date and time in UTC
    QDateTime dateTime = QDateTime::currentDateTimeUtc();
    QDateTime localDateTime = dateTime.toLocalTime();
    QDate date = dateTime.date();
    QTime time = dateTime.time();

    // 1. Calculate DaysSince1970
    QDate epochStart(1970, 1, 1);
    uint16_t DaysSince1970 = static_cast<uint16_t>(epochStart.daysTo(date));

    // 2. Calculate SecondsSinceMidnight in 0.0001 s resolution
    double SecondsSinceMidnight = (time.hour() * 3600 + time.minute() * 60 + time.second() + time.msec() / 1000.0);

    // 3. Calculate LocalOffset in 60 s resolution
    int16_t LocalOffset = static_cast<int16_t>(localDateTime.offsetFromUtc());
    qDebug() << "LocalOffset:" << LocalOffset;

    // Send PGN 129033 using the NMEA2000 library
    return SendPgn129033(DaysSince1970, SecondsSinceMidnight, LocalOffset);
}

bool Nmea2000Handler::SendPgn126992(unsigned char &SID, uint16_t &SystemDate, double &SystemTime, tN2kTimeSource &TimeSource)
{
    tN2kMsg N2kMsg;

    // Create the PGN 126992 message
    SetN2kPGN126992(N2kMsg, SID, SystemDate, SystemTime, TimeSource);

    // Send the message via NMEA2000
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 126992 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 126992";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 126992";
            return false;
        }

    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn126992' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn126992(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 126992, 8)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "126992";
    QStringList paramDetails;

    unsigned char SID;
    uint16_t SystemDate;
    double SystemTime;
    tN2kTimeSource TimeSource;

    if (ParseN2kPGN126992(N2kMsg, SID, SystemDate, SystemTime, TimeSource)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"SystemDate", QString::number(SystemDate)});
        paramDetails.append({"SystemTime", QString::number(SystemTime, 'g', 17)});
        paramDetails.append({"TimeSource", QString::number(TimeSource)});
    } else {
        qWarning() << "Failed to parse PGN 126992";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127245(double &RudderPosition, unsigned char &Instance, tN2kRudderDirectionOrder &RudderDirectionOrder, double &AngleOrder)
{
    // Create the PGN 127245 message
    tN2kMsg N2kMsg;
    SetN2kPGN127245(N2kMsg, RudderPosition, Instance, RudderDirectionOrder, AngleOrder);

    // Send the message via NMEA2000
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127245 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127245";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127245";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127245' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127245(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127245, 6)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127245";
    QStringList paramDetails;

    double RudderPosition;
    unsigned char Instance;
    tN2kRudderDirectionOrder RudderDirectionOrder;
    double AngleOrder;

    if (ParseN2kPGN127245(N2kMsg, RudderPosition, Instance, RudderDirectionOrder, AngleOrder)) {
        paramDetails.append({"RudderPosition", QString::number(RudderPosition, 'g', 17)});
        paramDetails.append({"Instance", QString::number(Instance)});
        paramDetails.append({"RudderDirectionOrder", QString::number(RudderDirectionOrder)});
        paramDetails.append({"AngleOrder", QString::number(AngleOrder, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 127245";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127250(unsigned char &SID, double &Heading, double &Deviation, double &Variation, tN2kHeadingReference &HeadingReference)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127250 message
    SetN2kPGN127250(N2kMsg, SID, Heading, Deviation, Variation, HeadingReference);

    // Send the message via NMEA2000
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127250 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127250";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127250";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127250' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127250(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127250, 8)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127250";
    QStringList paramDetails;

    unsigned char SID;
    double Heading;
    double Deviation;
    double Variation;
    tN2kHeadingReference HeadingReference;

    if (ParseN2kPGN127250(N2kMsg, SID, Heading, Deviation, Variation, HeadingReference)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"Heading", QString::number(Heading, 'g', 17)});
        paramDetails.append({"Deviation", QString::number(Deviation, 'g', 17)});
        paramDetails.append({"Variation", QString::number(Variation, 'g', 17)});
        paramDetails.append({"HeadingReference", QString::number(HeadingReference)});
    } else {
        qWarning() << "Failed to parse PGN 127250";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127251(unsigned char &SID, double &RateOfTurn)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127251 message
    SetN2kPGN127251(N2kMsg, SID, RateOfTurn);

    // Send the message via NMEA2000
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127251 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127251";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127251";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127251' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127251(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127251, 5)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127251";
    QStringList paramDetails;

    unsigned char SID;
    double RateOfTurn;

    if (ParseN2kPGN127251(N2kMsg, SID, RateOfTurn)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"RateOfTurn", QString::number(RateOfTurn, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 127251";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127252(unsigned char &SID, double &Heave, double &Delay, tN2kDelaySource &DelaySource)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127252 message
    SetN2kPGN127252(N2kMsg, SID, Heave, Delay, DelaySource);

    // Send the message via NMEA2000
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127252 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127252";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127252";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127252' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127252(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127252, 6)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127252";
    QStringList paramDetails;

    unsigned char SID;
    double Heave;
    double Delay;
    tN2kDelaySource DelaySource;

    if (ParseN2kPGN127252(N2kMsg, SID, Heave, Delay, DelaySource)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"Heave", QString::number(Heave, 'g', 17)});
        paramDetails.append({"Delay", QString::number(Delay, 'g', 17)});
        paramDetails.append({"DelaySource", QString::number(DelaySource)});
    } else {
        qWarning() << "Failed to parse PGN 127252";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127257(unsigned char &SID, double &Yaw, double &Pitch, double &Roll)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127257 message
    SetN2kPGN127257(N2kMsg, SID, Yaw, Pitch, Roll);

    // Send the message via NMEA2000
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127257 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127257";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127257";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127257' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127257(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127257, 7)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127257";
    QStringList paramDetails;

    unsigned char SID;
    double Yaw;
    double Pitch;
    double Roll;

    if (ParseN2kPGN127257(N2kMsg, SID, Yaw, Pitch, Roll)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"Yaw", QString::number(Yaw, 'g', 17)});
        paramDetails.append({"Pitch", QString::number(Pitch, 'g', 17)});
        paramDetails.append({"Roll", QString::number(Roll, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 127257";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127258(unsigned char &SID, tN2kMagneticVariation &Source, uint16_t &DaysSince1970, double &Variation)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127258 message
    SetN2kPGN127258(N2kMsg, SID, Source, DaysSince1970, Variation);

    // Send the message via NMEA2000
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127258 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127258";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127258";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127258' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127258(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127258, 6)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127258";
    QStringList paramDetails;

    unsigned char SID;
    tN2kMagneticVariation Source;
    uint16_t DaysSince1970;
    double Variation;

    if (ParseN2kPGN127258(N2kMsg, SID, Source, DaysSince1970, Variation)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"Source", QString::number(Source)});
        paramDetails.append({"DaysSince1970", QString::number(DaysSince1970)});
        paramDetails.append({"Variation", QString::number(Variation, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 127258";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127488(unsigned char &EngineInstance, double &EngineSpeed, double &EngineBoostPressure, int &EngineTiltTrim)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127488 message
    SetN2kPGN127488(N2kMsg, EngineInstance, EngineSpeed, EngineBoostPressure, EngineTiltTrim);

    // Send the message via NMEA2000
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127488 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127488";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127488";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127488' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127488(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127488, 6)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127488";
    QStringList paramDetails;

    unsigned char EngineInstance;
    double EngineSpeed;
    double EngineBoostPressure;
    int8_t EngineTiltTrim;

    if (ParseN2kPGN127488(N2kMsg, EngineInstance, EngineSpeed, EngineBoostPressure, EngineTiltTrim)) {
        paramDetails.append({"EngineInstance", QString::number(EngineInstance)});
        paramDetails.append({"EngineSpeed", QString::number(EngineSpeed, 'g', 17)});
        paramDetails.append({"EngineBoostPressure", QString::number(EngineBoostPressure, 'g', 17)});
        paramDetails.append({"EngineTiltTrim", QString::number(EngineTiltTrim)});
    } else {
        qWarning() << "Failed to parse PGN 127488";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127489(unsigned char &EngineInstance,
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
                                    tN2kEngineDiscreteStatus2 &Status2)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127489 message
    SetN2kPGN127489(N2kMsg,
                    EngineInstance,
                    EngineOilPress,
                    EngineOilTemp,
                    EngineCoolantTemp,
                    AlternatorVoltage,
                    FuelRate,
                    EngineHours,
                    EngineCoolantPress,
                    EngineFuelPress,
                    EngineLoad,
                    EngineTorque,
                    Status1,
                    Status2);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127489 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127489";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127489";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127489' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127489(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127489, 26)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127489";
    QStringList paramDetails;

    unsigned char EngineInstance;
    double EngineOilPress;
    double EngineOilTemp;
    double EngineCoolantTemp;
    double AltenatorVoltage;
    double FuelRate;
    double EngineHours;
    double EngineCoolantPress;
    double EngineFuelPress;
    int8_t EngineLoad;
    int8_t EngineTorque;
    tN2kEngineDiscreteStatus1 Status1;
    tN2kEngineDiscreteStatus2 Status2;

    if (ParseN2kPGN127489(N2kMsg,
                          EngineInstance,
                          EngineOilPress,
                          EngineOilTemp,
                          EngineCoolantTemp,
                          AltenatorVoltage,
                          FuelRate,
                          EngineHours,
                          EngineCoolantPress,
                          EngineFuelPress,
                          EngineLoad,
                          EngineTorque,
                          Status1,
                          Status2)) {
        paramDetails.append({"EngineInstance", QString::number(EngineInstance)});
        paramDetails.append({"EngineOilPress", QString::number(EngineOilPress, 'g', 17)});
        paramDetails.append({"EngineOilTemp", QString::number(EngineOilTemp, 'g', 17)});
        paramDetails.append({"EngineCoolantTemp", QString::number(EngineCoolantTemp, 'g', 17)});
        paramDetails.append({"AltenatorVoltage", QString::number(AltenatorVoltage, 'g', 17)});
        paramDetails.append({"FuelRate", QString::number(FuelRate, 'g', 17)});
        paramDetails.append({"EngineHours", QString::number(EngineHours, 'g', 17)});
        paramDetails.append({"EngineCoolantPress", QString::number(EngineCoolantPress, 'g', 17)});
        paramDetails.append({"EngineFuelPress", QString::number(EngineFuelPress, 'g', 17)});
        paramDetails.append({"EngineLoad", QString::number(EngineLoad)});
        paramDetails.append({"EngineTorque", QString::number(EngineTorque)});
        paramDetails.append({"Status1", QString::number(Status1.Status)});
        paramDetails.append({"Status2", QString::number(Status2.Status)});
    } else {
        qWarning() << "Failed to parse PGN 127489";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127493(
    unsigned char &EngineInstance, tN2kTransmissionGear &TransmissionGear, double &OilPressure, double &OilTemperature, unsigned char &DiscreteStatus1)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127493 message
    SetN2kPGN127493(N2kMsg, EngineInstance, TransmissionGear, OilPressure, OilTemperature, DiscreteStatus1);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127493 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127493";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127493";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127493' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127493(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127493, 7)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127493";
    QStringList paramDetails;

    unsigned char EngineInstance;
    tN2kTransmissionGear TransmissionGear;
    double OilPressure;
    double OilTemperature;
    unsigned char DiscreteStatus1;

    if (ParseN2kPGN127493(N2kMsg, EngineInstance, TransmissionGear, OilPressure, OilTemperature, DiscreteStatus1)) {
        paramDetails.append({"EngineInstance", QString::number(EngineInstance)});
        paramDetails.append({"TransmissionGear", QString::number(TransmissionGear)});
        paramDetails.append({"OilPressure", QString::number(OilPressure, 'g', 17)});
        paramDetails.append({"OilTemperature", QString::number(OilTemperature, 'g', 17)});
        paramDetails.append({"DiscreteStatus1", QString::number(DiscreteStatus1)});
    } else {
        qWarning() << "Failed to parse PGN 127493";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127505(unsigned char &Instance, tN2kFluidType &FluidType, double &Level, double &Capacity)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127505 message
    SetN2kPGN127505(N2kMsg, Instance, FluidType, Level, Capacity);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127505 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127505";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127505";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127505' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127505(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127505, 7)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127505";
    QStringList paramDetails;

    unsigned char Instance;
    tN2kFluidType FluidType;
    double Level;
    double Capacity;

    if (ParseN2kPGN127505(N2kMsg, Instance, FluidType, Level, Capacity)) {
        paramDetails.append({"Instance", QString::number(Instance)});
        paramDetails.append({"FluidType", QString::number(FluidType)});
        paramDetails.append({"Level", QString::number(Level, 'g', 17)});
        paramDetails.append({"Capacity", QString::number(Capacity, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 127505";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127506(unsigned char &SID,
                                    unsigned char &DCInstance,
                                    tN2kDCType &DCType,
                                    uint8_t &StateOfCharge,
                                    uint8_t &StateOfHealth,
                                    double &TimeRemaining,
                                    double &RippleVoltage,
                                    double &Capacity)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127506 message
    SetN2kPGN127506(N2kMsg, SID, DCInstance, DCType, StateOfCharge, StateOfHealth, TimeRemaining, RippleVoltage, Capacity);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127506 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127506";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127506";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127506' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127506(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127506, 11)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127506";
    QStringList paramDetails;

    unsigned char SID;
    unsigned char DCInstance;
    tN2kDCType DCType;
    uint8_t StateOfCharge;
    uint8_t StateOfHealth;
    double TimeRemaining;
    double RippleVoltage;
    double Capacity;

    if (ParseN2kPGN127506(N2kMsg, SID, DCInstance, DCType, StateOfCharge, StateOfHealth, TimeRemaining, RippleVoltage, Capacity)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"DCInstance", QString::number(DCInstance)});
        paramDetails.append({"DCType", QString::number(DCType)});
        paramDetails.append({"StateOfCharge", QString::number(StateOfCharge)});
        paramDetails.append({"StateOfHealth", QString::number(StateOfHealth)});
        paramDetails.append({"TimeRemaining", QString::number(TimeRemaining, 'g', 17)});
        paramDetails.append({"RippleVoltage", QString::number(RippleVoltage, 'g', 17)});
        paramDetails.append({"Capacity", QString::number(Capacity, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 127506";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127507(unsigned char &Instance,
                                    unsigned char &BatteryInstance,
                                    tN2kChargeState &ChargeState,
                                    tN2kChargerMode &ChargerMode,
                                    tN2kOnOff &Enabled,
                                    tN2kOnOff &EqualizationPending,
                                    double &EqualizationTimeRemaining)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127507 message
    SetN2kPGN127507(N2kMsg, Instance, BatteryInstance, ChargeState, ChargerMode, Enabled, EqualizationPending, EqualizationTimeRemaining);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127507 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127507";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127507";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127507' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127507(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127507, 6)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127507";
    QStringList paramDetails;

    unsigned char Instance;
    unsigned char BatteryInstance;
    tN2kChargeState ChargeState;
    tN2kChargerMode ChargerMode;
    tN2kOnOff Enabled;
    tN2kOnOff EqualizationPending;
    double EqualizationTimeRemaining;

    if (ParseN2kPGN127507(N2kMsg, Instance, BatteryInstance, ChargeState, ChargerMode, Enabled, EqualizationPending, EqualizationTimeRemaining)) {
        // The pinned library's parser multiplies this seconds field by 60,
        // unlike its encoder and documented API. Read the wire's 1 s field
        // directly, preserving the unavailable sentinel and dependency pins.
        int index = 4;
        EqualizationTimeRemaining = N2kMsg.Get2ByteUDouble(1, index);
        paramDetails.append({"Instance", QString::number(Instance)});
        paramDetails.append({"BatteryInstance", QString::number(BatteryInstance)});
        paramDetails.append({"ChargeState", QString::number(ChargeState)});
        paramDetails.append({"ChargerMode", QString::number(ChargerMode)});
        paramDetails.append({"Enabled", QString::number(Enabled)});
        paramDetails.append({"EqualizationPending", QString::number(EqualizationPending)});
        paramDetails.append({"EqualizationTimeRemaining", QString::number(EqualizationTimeRemaining, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 127507";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn127508(
    unsigned char &BatteryInstance, double &BatteryVoltage, double &BatteryCurrent, double &BatteryTemperature, unsigned char &SID)
{
    tN2kMsg N2kMsg;

    // Create the PGN 127508 message
    SetN2kPGN127508(N2kMsg, BatteryInstance, BatteryVoltage, BatteryCurrent, BatteryTemperature, SID);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 127508 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 127508";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 127508";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn127508' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn127508(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 127508, 8)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "127508";
    QStringList paramDetails;

    unsigned char BatteryInstance;
    double BatteryVoltage;
    double BatteryCurrent;
    double BatteryTemperature;
    unsigned char SID;

    if (ParseN2kPGN127508(N2kMsg, BatteryInstance, BatteryVoltage, BatteryCurrent, BatteryTemperature, SID)) {
        paramDetails.append({"BatteryInstance", QString::number(BatteryInstance)});
        paramDetails.append({"BatteryVoltage", QString::number(BatteryVoltage, 'g', 17)});
        paramDetails.append({"BatteryCurrent", QString::number(BatteryCurrent, 'g', 17)});
        paramDetails.append({"BatteryTemperature", QString::number(BatteryTemperature, 'g', 17)});
        paramDetails.append({"SID", QString::number(SID)});
    } else {
        qWarning() << "Failed to parse PGN 127508";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn128259(unsigned char &SID, double &WaterReferenced, double &GroundReferenced, tN2kSpeedWaterReferenceType &SWRT)
{
    tN2kMsg N2kMsg;

    // Create the PGN 128259 message
    SetN2kPGN128259(N2kMsg, SID, WaterReferenced, GroundReferenced, SWRT);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 128259 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 128259";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 128259";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn128259' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn128259(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 128259, 6)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "128259";
    QStringList paramDetails;

    unsigned char SID;
    double WaterReferenced;
    double GroundReferenced;
    tN2kSpeedWaterReferenceType SWRT;

    if (ParseN2kPGN128259(N2kMsg, SID, WaterReferenced, GroundReferenced, SWRT)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"WaterReferenced", QString::number(WaterReferenced, 'g', 17)});
        paramDetails.append({"GroundReferenced", QString::number(GroundReferenced, 'g', 17)});
        paramDetails.append({"SWRT", QString::number(SWRT)});
    } else {
        qWarning() << "Failed to parse PGN 128259";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn128267(unsigned char &SID, double &DepthBelowTransducer, double &Offset, double &Range)
{
    tN2kMsg N2kMsg;

    // Create the PGN 128267 message
    SetN2kPGN128267(N2kMsg, SID, DepthBelowTransducer, Offset, Range);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 128267 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 128267";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 128267";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn128267' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn128267(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 128267, 8)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "128267";
    QStringList paramDetails;

    unsigned char SID;
    double DepthBelowTransducer;
    double Offset;
    double Range;

    if (ParseN2kPGN128267(N2kMsg, SID, DepthBelowTransducer, Offset, Range)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"DepthBelowTransducer", QString::number(DepthBelowTransducer, 'g', 17)});
        paramDetails.append({"Offset", QString::number(Offset, 'g', 17)});
        paramDetails.append({"Range", QString::number(Range, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 128267";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn129025(double &Latitude, double &Longitude)
{
    tN2kMsg N2kMsg;

    // Create the PGN 129025 message
    SetN2kPGN129025(N2kMsg, Latitude, Longitude);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 129025 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 129025";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 129025";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn129025' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn129025(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 129025, 8)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "129025";
    QStringList paramDetails;

    double Latitude;
    double Longitude;

    if (ParseN2kPGN129025(N2kMsg, Latitude, Longitude)) {
        paramDetails.append({"Latitude", receivedCoordinate(Latitude)});
        paramDetails.append({"Longitude", receivedCoordinate(Longitude)});
    } else {
        qWarning() << "Failed to parse PGN 129025";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn129026(unsigned char &SID, tN2kHeadingReference &ref, double &COG, double &SOG)
{
    tN2kMsg N2kMsg;

    // Create the PGN 129026 message
    SetN2kPGN129026(N2kMsg, SID, ref, COG, SOG);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 129026 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 129026";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 129026";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn129026' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn129026(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 129026, 6)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "129026";
    QStringList paramDetails;

    unsigned char SID;
    tN2kHeadingReference ref;
    double COG;
    double SOG;

    if (ParseN2kPGN129026(N2kMsg, SID, ref, COG, SOG)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"ref", QString::number(ref)});
        paramDetails.append({"COG", QString::number(COG, 'g', 17)});
        paramDetails.append({"SOG", QString::number(SOG, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 129026";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn129283(unsigned char &SID, tN2kXTEMode &XTEMode, bool &NavigationTerminated, double &XTE)
{
    tN2kMsg N2kMsg;

    // Create the PGN 129283 message
    SetN2kPGN129283(N2kMsg, SID, XTEMode, NavigationTerminated, XTE);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 129283 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 129283";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 129283";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn129283' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn129283(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 129283, 6)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "129283";
    QStringList paramDetails;

    unsigned char SID;
    tN2kXTEMode XTEMode;
    bool NavigationTerminated;
    double XTE;

    if (ParseN2kPGN129283(N2kMsg, SID, XTEMode, NavigationTerminated, XTE)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"XTEMode", QString::number(XTEMode)});
        paramDetails.append({"NavigationTerminated", QString::number(NavigationTerminated)});
        paramDetails.append({"XTE", QString::number(XTE, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 129283";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn129284(unsigned char &SID,
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
                                    double &WaypointClosingVelocity)
{
    tN2kMsg N2kMsg;

    // Create the PGN 129284 message
    SetN2kPGN129284(N2kMsg,
                    SID,
                    DistanceToWaypoint,
                    BearingReference,
                    PerpendicularCrossed,
                    ArrivalCircleEntered,
                    CalculationType,
                    ETATime,
                    ETADate,
                    BearingOriginToDestinationWaypoint,
                    BearingPositionToDestinationWaypoint,
                    OriginWaypointNumber,
                    DestinationWaypointNumber,
                    DestinationLatitude,
                    DestinationLongitude,
                    WaypointClosingVelocity);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 129284 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 129284";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 129284";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn129284' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn129284(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 129284, 34)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "129284";
    QStringList paramDetails;

    unsigned char SID;
    double DistanceToWaypoint;
    tN2kHeadingReference BearingReference;
    bool PerpendicularCrossed;
    bool ArrivalCircleEntered;
    tN2kDistanceCalculationType CalculationType;
    double ETATime;
    int16_t ETADate;
    double BearingOriginToDestinationWaypoint;
    double BearingPositionToDestinationWaypoint;
    uint32_t OriginWaypointNumber;
    uint32_t DestinationWaypointNumber;
    double DestinationLatitude;
    double DestinationLongitude;
    double WaypointClosingVelocity;

    if (ParseN2kPGN129284(N2kMsg,
                          SID,
                          DistanceToWaypoint,
                          BearingReference,
                          PerpendicularCrossed,
                          ArrivalCircleEntered,
                          CalculationType,
                          ETATime,
                          ETADate,
                          BearingOriginToDestinationWaypoint,
                          BearingPositionToDestinationWaypoint,
                          OriginWaypointNumber,
                          DestinationWaypointNumber,
                          DestinationLatitude,
                          DestinationLongitude,
                          WaypointClosingVelocity)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"DistanceToWaypoint", QString::number(DistanceToWaypoint, 'g', 17)});
        paramDetails.append({"BearingReference", QString::number(BearingReference)});
        paramDetails.append({"PerpendicularCrossed", QString::number(PerpendicularCrossed)});
        paramDetails.append({"ArrivalCircleEntered", QString::number(ArrivalCircleEntered)});
        paramDetails.append({"CalculationType", QString::number(CalculationType)});
        paramDetails.append({"ETATime", QString::number(ETATime, 'g', 17)});
        paramDetails.append({"ETADate", QString::number(ETADate)});
        paramDetails.append({"BearingOriginToDestinationWaypoint", QString::number(BearingOriginToDestinationWaypoint, 'g', 17)});
        paramDetails.append({"BearingPositionToDestinationWaypoint", QString::number(BearingPositionToDestinationWaypoint, 'g', 17)});
        paramDetails.append({"OriginWaypointNumber", QString::number(OriginWaypointNumber)});
        paramDetails.append({"DestinationWaypointNumber", QString::number(DestinationWaypointNumber)});
        paramDetails.append({"DestinationLatitude", receivedCoordinate(DestinationLatitude)});
        paramDetails.append({"DestinationLongitude", receivedCoordinate(DestinationLongitude)});
        paramDetails.append({"WaypointClosingVelocity", QString::number(WaypointClosingVelocity, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 129284";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn129029(unsigned char &SID,
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
                                    double &AgeOfCorrection)
{
    tN2kMsg N2kMsg;

    // Create the PGN 129029 message
    SetN2kPGN129029(N2kMsg,
                    SID,
                    DaysSince1970,
                    SecondsSinceMidnight,
                    Latitude,
                    Longitude,
                    Altitude,
                    GNSStype,
                    GNSSmethod,
                    nSatellites,
                    HDOP,
                    PDOP,
                    GeoidalSeparation,
                    nReferenceStations,
                    ReferenceStationType,
                    ReferenceSationID,
                    AgeOfCorrection);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 129029 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 129029";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 129029";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn129029' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn129029(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 129029, 43)) return false;
    // Base is 43 bytes; each declared reference station adds four. The pinned
    // parser exposes only the first station, but a declared list must be complete.
    const int stations = N2kMsg.Data[42];
    if (stations != N2kUInt8NA && N2kMsg.DataLen < 43 + 4 * stations) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "129029";
    QStringList paramDetails;

    unsigned char SID;
    uint16_t DaysSince1970;
    double SecondsSinceMidnight;
    double Latitude;
    double Longitude;
    double Altitude;
    tN2kGNSStype GNSStype;
    tN2kGNSSmethod GNSSmethod;
    unsigned char nSatellites;
    double HDOP;
    double PDOP;
    double GeoidalSeparation;
    unsigned char nReferenceStations;
    tN2kGNSStype ReferenceStationType;
    uint16_t ReferenceSationID;
    double AgeOfCorrection;

    if (ParseN2kPGN129029(N2kMsg,
                          SID,
                          DaysSince1970,
                          SecondsSinceMidnight,
                          Latitude,
                          Longitude,
                          Altitude,
                          GNSStype,
                          GNSSmethod,
                          nSatellites,
                          HDOP,
                          PDOP,
                          GeoidalSeparation,
                          nReferenceStations,
                          ReferenceStationType,
                          ReferenceSationID,
                          AgeOfCorrection)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"DaysSince1970", QString::number(DaysSince1970)});
        paramDetails.append({"SecondsSinceMidnight", QString::number(SecondsSinceMidnight, 'g', 17)});
        paramDetails.append({"Latitude", receivedCoordinate(Latitude)});
        paramDetails.append({"Longitude", receivedCoordinate(Longitude)});
        paramDetails.append({"Altitude", QString::number(Altitude, 'g', 17)});
        paramDetails.append({"GNSStype", QString::number(GNSStype)});
        paramDetails.append({"GNSSmethod", QString::number(GNSSmethod)});
        paramDetails.append({"nSatellites", QString::number(nSatellites)});
        paramDetails.append({"HDOP", QString::number(HDOP, 'g', 17)});
        paramDetails.append({"PDOP", QString::number(PDOP, 'g', 17)});
        paramDetails.append({"GeoidalSeparation", QString::number(GeoidalSeparation, 'g', 17)});
        paramDetails.append({"nReferenceStations", QString::number(nReferenceStations)});
        paramDetails.append({"ReferenceStationType", QString::number(ReferenceStationType)});
        paramDetails.append({"ReferenceSationID", QString::number(ReferenceSationID)});
        paramDetails.append({"AgeOfCorrection", QString::number(AgeOfCorrection, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 129029";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}
/*
bool Nmea2000Handler::SendPgn129026(double COG, double SOG, tN2kHeadingReference COGReference)
{
    tN2kMsg N2kMsg;

    // Create the PGN 129026 message
    SetN2kPGN129026(N2kMsg, 1, COGReference, COG, SOG);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 129026 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 129026";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000") {
        return sendCanMessage(N2kMsg);
    } else if (interfaceType == "SocketCan") {
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn129026' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}
*/
bool Nmea2000Handler::SendPgn129033(uint16_t &DaysSince1970, double &SecondsSinceMidnight, int16_t &LocalOffset)
{
    tN2kMsg N2kMsg;

    // Create the PGN 129033 message
    SetN2kPGN129033(N2kMsg, DaysSince1970, SecondsSinceMidnight, LocalOffset);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 129033 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 129033";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 129033";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn129033' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn129033(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 129033, 8)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "129033";
    QStringList paramDetails;

    uint16_t DaysSince1970;
    double SecondsSinceMidnight;
    int16_t LocalOffset;

    if (ParseN2kPGN129033(N2kMsg, DaysSince1970, SecondsSinceMidnight, LocalOffset)) {
        paramDetails.append({"DaysSince1970", QString::number(DaysSince1970)});
        paramDetails.append({"SecondsSinceMidnight", QString::number(SecondsSinceMidnight, 'g', 17)});
        paramDetails.append({"LocalOffset", QString::number(LocalOffset)});
    } else {
        qWarning() << "Failed to parse PGN 129033";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn130310(unsigned char &SID, double &WaterTemperature, double &OutsideAmbientAirTemperature, double &AtmosphericPressure)
{
    tN2kMsg N2kMsg;

    // Create the PGN 130310 message
    SetN2kPGN130310(N2kMsg, SID, WaterTemperature, OutsideAmbientAirTemperature, AtmosphericPressure);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 130310 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 130310";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 130310";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn130310' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdEnvironmental(const tN2kMsg &message)
{
    if (message.DataLen != 8) return false;
    unsigned char sid, instance; double actual, set; QStringList fields;
    auto n = [](double v) { return QString::number(v, 'g', 17); };
    if (message.PGN == 130306) {
        tN2kWindReference reference;
        if (!ParseN2kPGN130306(message, sid, actual, set, reference)) return false;
        if (actual != N2kDoubleNA && (!std::isfinite(actual) || actual < 0 || actual > 655.32)) actual = N2kDoubleNA;
        if (set != N2kDoubleNA && (!std::isfinite(set) || set < 0 || set >= 2*M_PI)) set = N2kDoubleNA;
        if (!(int(reference) <= 4 || int(reference) == 6 || int(reference) == 7)) reference = N2kWind_Unavailable;
        fields = {"SID", n(sid), "WindSpeed", n(actual), "WindAngle", n(set), "WindReference", n(reference)};
    } else if (message.PGN == 130313) {
        tN2kHumiditySource source;
        if (!ParseN2kPGN130313(message, sid, instance, source, actual, set)) return false;
        if (actual != N2kDoubleNA && (!std::isfinite(actual) || actual < 0 || actual > 100)) actual = N2kDoubleNA;
        if (set != N2kDoubleNA && (!std::isfinite(set) || set < 0 || set > 100)) set = N2kDoubleNA;
        if (source != N2khs_InsideHumidity && source != N2khs_OutsideHumidity) source = N2khs_Undef;
        fields = {"SID", n(sid), "HumidityInstance", n(instance), "HumiditySource", n(source),
            "ActualHumidity", n(actual), "SetHumidity", n(set)};
    } else return false;
    emit RcvdPgn(QString::number(message.PGN), fields, QString::number(message.Source));
    return true;
}

bool Nmea2000Handler::RcvdPgn130310(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 130310, 7)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "130310";
    QStringList paramDetails;

    unsigned char SID;
    double WaterTemperature;
    double OutsideAmbientAirTemperature;
    double AtmosphericPressure;

    if (ParseN2kPGN130310(N2kMsg, SID, WaterTemperature, OutsideAmbientAirTemperature, AtmosphericPressure)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"WaterTemperature", QString::number(WaterTemperature, 'g', 17)});
        paramDetails.append({"OutsideAmbientAirTemperature", QString::number(OutsideAmbientAirTemperature, 'g', 17)});
        paramDetails.append({"AtmosphericPressure", QString::number(AtmosphericPressure, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 130310";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn130311(
    unsigned char &SID, tN2kTempSource &TempSource, double &Temperature, tN2kHumiditySource &HumiditySource, double &Humidity, double &AtmosphericPressure)
{
    tN2kMsg N2kMsg;

    // Create the PGN 130311 message
    SetN2kPGN130311(N2kMsg, SID, TempSource, Temperature, HumiditySource, Humidity, AtmosphericPressure);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 130311 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 130311";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 130311";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn130311' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn130311(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 130311, 8)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "130311";
    QStringList paramDetails;

    unsigned char SID;
    tN2kTempSource TempSource;
    double Temperature;
    tN2kHumiditySource HumiditySource;
    double Humidity;
    double AtmosphericPressure;

    if (ParseN2kPGN130311(N2kMsg, SID, TempSource, Temperature, HumiditySource, Humidity, AtmosphericPressure)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"TempSource", QString::number(TempSource)});
        paramDetails.append({"Temperature", QString::number(Temperature, 'g', 17)});
        paramDetails.append({"HumiditySource", QString::number(HumiditySource)});
        paramDetails.append({"Humidity", QString::number(Humidity, 'g', 17)});
        paramDetails.append({"AtmosphericPressure", QString::number(AtmosphericPressure, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 130311";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn130312(
    unsigned char &SID, double &TemperatureInstance, tN2kTempSource &TemperatureSource, double &ActualTemperature, double &SetTemperature)
{
    tN2kMsg N2kMsg;

    // Create the PGN 130312 message

    SetN2kPGN130312(N2kMsg, SID, TemperatureInstance, TemperatureSource, ActualTemperature, SetTemperature);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 130312 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 130312";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 130312";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn130312' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn130312(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 130312, 7)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "130312";
    QStringList paramDetails;

    unsigned char SID;
    unsigned char TemperatureInstance;
    tN2kTempSource TemperatureSource;
    double ActualTemperature;
    double SetTemperature;

    if (ParseN2kPGN130312(N2kMsg, SID, TemperatureInstance, TemperatureSource, ActualTemperature, SetTemperature)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"TemperatureInstance", QString::number(TemperatureInstance)});
        paramDetails.append({"TemperatureSource", QString::number(TemperatureSource)});
        paramDetails.append({"ActualTemperature", QString::number(ActualTemperature, 'g', 17)});
        paramDetails.append({"SetTemperature", QString::number(SetTemperature, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 130312";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn130316(
    unsigned char &SID, unsigned char &TempInstance, tN2kTempSource &TempSource, double &ActualTemperature, double &SetTemperature)
{
    tN2kMsg N2kMsg;

    // Create the PGN 130316 message
    SetN2kPGN130316(N2kMsg, SID, TempInstance, TempSource, ActualTemperature, SetTemperature);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 130316 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 130316";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 130316";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn130316' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn130316(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 130316, 8)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "130316";
    QStringList paramDetails;

    unsigned char SID;
    unsigned char TempInstance;
    tN2kTempSource TempSource;
    double ActualTemperature;
    double SetTemperature;

    if (ParseN2kPGN130316(N2kMsg, SID, TempInstance, TempSource, ActualTemperature, SetTemperature)) {
        paramDetails.append({"SID", QString::number(SID)});
        paramDetails.append({"TempInstance", QString::number(TempInstance)});
        paramDetails.append({"TempSource", QString::number(TempSource)});
        paramDetails.append({"ActualTemperature", QString::number(ActualTemperature, 'g', 17)});
        paramDetails.append({"SetTemperature", QString::number(SetTemperature, 'g', 17)});
    } else {
        qWarning() << "Failed to parse PGN 130316";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::SendPgn130576(int8_t &PortTrimTab, int8_t &StbdTrimTab)
{
    tN2kMsg N2kMsg;

    // Create the PGN 130576 message
    SetN2kPGN130576(N2kMsg, PortTrimTab, StbdTrimTab);

    // Send the message via interface type
    if (interfaceType == "Actisense") {
        // Send the message via NMEA2000
        if (sendActisenseMessage(N2kMsg)) {
            //qDebug() << "PGN 130576 sent successfully";
        } else {
            qWarning() << "Failed to send PGN 130576";
            return false;
        }
    } else if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (sendCanMessage(N2kMsg)) {
            return true;
        } else {
            qWarning() << "Failed to send PGN 130576";
            return false;
        }
    } else if (interfaceType == "SignalK") {
        return mode == Sender && signalK->publish(N2kMsg);
    } else {
        qWarning() << "'Nmea2000Handler::SendPgn130576' Invalid interface type:" << interfaceType;
        return false;
    }

    return true;
}

bool Nmea2000Handler::RcvdPgn130576(const tN2kMsg &N2kMsg)
{
    if (!hasReceivePayload(N2kMsg, 130576, 2)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString pgnLabel = "130576";
    QStringList paramDetails;

    int8_t PortTrimTab;
    int8_t StbdTrimTab;

    if (ParseN2kPGN130576(N2kMsg, PortTrimTab, StbdTrimTab)) {
        paramDetails.append({"PortTrimTab", QString::number(PortTrimTab)});
        paramDetails.append({"StbdTrimTab", QString::number(StbdTrimTab)});
    } else {
        qWarning() << "Failed to parse PGN 130576";
        return false;
    }

    emit RcvdPgn(pgnLabel, paramDetails, QString::number(N2kMsg.Source));
    return true;
}

bool Nmea2000Handler::sendPgn(int pgn, const QStringList &paramList, const QHash<QString,QDateTime> &sampleTimes)
{
    // Only permit sending PGNs if the mode is set to Sender
    if (mode != Sender) {
        return false;
    }

    if(Expansion::definition(QString::number(pgn))) {
        QMap<QString,QString> fields;for(const auto &parameter:paramList){const int colon=parameter.indexOf(':');if(colon<1||fields.contains(parameter.left(colon)))return false;fields[parameter.left(colon)]=parameter.mid(colon+1);}
        tN2kMsg message;if(!Expansion::encode(QString::number(pgn),fields,message)) {
            if(pgn==127496 || pgn==127498 || pgn==127509 || pgn==129285 || pgn==130074)emit TransmitError("Cannot encode PGN "+QString::number(pgn)+": check field values and packet capacity.");
            return false;
        }
        if(interfaceType=="Actisense")return sendActisenseMessage(message);
        if(interfaceType=="SocketCan"||interfaceType=="Waveshare USB-CAN-A"||interfaceType=="CANable SLCAN")return sendCanMessage(message);
        // No new native Signal K mappings are implied by NMEA codec support.
        if(interfaceType=="SignalK" && !property("expansionSignalKUnsupported").toBool()) {
            const QString reason="This instrument requires an NMEA2000 output. Native Signal K publishing is unavailable.";
            emit TransmitError(reason);setProperty("expansionSignalKUnsupported",true);
        }
        return false;
    }
    switch (pgn) {
    case 128777:
    case 128778: {
        QMap<QString,QString> fields;
        for(const auto &parameter:paramList){const int colon=parameter.indexOf(':');if(colon<1||fields.contains(parameter.left(colon)))return false;fields.insert(parameter.left(colon),parameter.mid(colon+1));}
        tN2kMsg message;if(!WindlassStatus::encode(pgn,fields,message))return false;
        if(interfaceType=="Actisense")return sendActisenseMessage(message);
        if(interfaceType=="SocketCan" || (interfaceType=="Waveshare USB-CAN-A" || interfaceType=="CANable SLCAN"))return sendCanMessage(message);
        if(interfaceType=="SignalK") {
            if(!property("windlassSignalKUnsupported").toBool()) {
                const QString reason="Windlass status: native Signal K mapping is unsupported; select an authorized NMEA2000 transport.";
                qWarning().noquote()<<reason;emit TransmitError(reason);setProperty("windlassSignalKUnsupported",true);
            }
        }
        return false;
    }
    case 129039: {
        QMap<QString,QString> fields;for(const auto &parameter:paramList){const int colon=parameter.indexOf(':');if(colon<1||fields.contains(parameter.left(colon)))return false;fields.insert(parameter.left(colon),parameter.mid(colon+1));}
        tN2kMsg message;if(!AisClassBPosition129039::encode(fields,message))return false;
        if(interfaceType=="Actisense")return sendActisenseMessage(message);
        if(interfaceType=="SocketCan" || (interfaceType=="Waveshare USB-CAN-A" || interfaceType=="CANable SLCAN"))return sendCanMessage(message);
        if(interfaceType=="SignalK")return signalK->publishAis(pgn,fields,sampleTimes);
        return false;
    }
    case 129810: {
        QMap<QString,QString> fields;
        for(const auto &parameter:paramList){const int colon=parameter.indexOf(':');if(colon<1 || fields.contains(parameter.left(colon)))return false;fields.insert(parameter.left(colon),parameter.mid(colon+1));}
        tN2kMsg message;if(!AisClassBStatic129810::encode(fields,message))return false;
        if(interfaceType=="Actisense")return sendActisenseMessage(message);
        if(interfaceType=="SocketCan" || (interfaceType=="Waveshare USB-CAN-A" || interfaceType=="CANable SLCAN"))return sendCanMessage(message);
        if(interfaceType=="SignalK")return signalK->publishAis(pgn,fields,sampleTimes);
        return false;
    }
    case 129809: {
        QMap<QString, QString> fields;
        for (const auto &parameter : paramList) {
            const int colon = parameter.indexOf(':');
            if (colon < 1 || fields.contains(parameter.left(colon))) return false;
            fields.insert(parameter.left(colon), parameter.mid(colon+1));
        }
        tN2kMsg message;
        if (!AisClassBStatic129809::encode(fields, message)) return false;
        if (interfaceType == "Actisense") return sendActisenseMessage(message);
        if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) return sendCanMessage(message);
        if (interfaceType == "SignalK") return signalK->publishAis(pgn,fields,sampleTimes);
        return false;
    }
    case 126992: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 126992, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        uint16_t SystemDate = 0;
        double SystemTime = 0.0;
        tN2kTimeSource TimeSource = N2ktimes_GPS;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "SystemDate") {
                SystemDate = paramParts[1].toUShort();
            } else if (paramName == "SystemTime") {
                SystemTime = paramParts[1].toDouble();
            } else if (paramName == "TimeSource") {
                TimeSource = static_cast<tN2kTimeSource>(paramParts[1].toUShort());
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn126992(SID, SystemDate, SystemTime, TimeSource);
        break;
    }
    case 127245: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 127245, size" << paramList.size();
            return false;
        }

        double RudderPosition = 0.0;
        unsigned char Instance = 0;
        tN2kRudderDirectionOrder RudderDirectionOrder = N2kRDO_NoDirectionOrder;
        double AngleOrder = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "RudderPosition") {
                RudderPosition = paramParts[1].toDouble();
            } else if (paramName == "Instance") {
                Instance = paramParts[1].toUShort();
            } else if (paramName == "RudderDirectionOrder") {
                RudderDirectionOrder = static_cast<tN2kRudderDirectionOrder>(paramParts[1].toUShort());
            } else if (paramName == "AngleOrder") {
                AngleOrder = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn127245(RudderPosition, Instance, RudderDirectionOrder, AngleOrder);
        break;
    }
    case 127250: {
        // Check if the parameter list is valid
        if (paramList.size() != 5) {
            qWarning() << "Invalid parameter list for PGN 127250, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double Heading = 0.0;
        double Deviation = 0.0;
        double Variation = 0.0;
        tN2kHeadingReference HeadingReference = N2khr_magnetic;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "Heading") {
                Heading = paramParts[1].toDouble();
            } else if (paramName == "Deviation") {
                Deviation = paramParts[1].toDouble();
            } else if (paramName == "Variation") {
                Variation = paramParts[1].toDouble();
            } else if (paramName == "HeadingReference") {
                HeadingReference = static_cast<tN2kHeadingReference>(paramParts[1].toUShort());
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn127250(SID, Heading, Deviation, Variation, HeadingReference);
        break;
    }
    case 127251: {
        // Check if the parameter list is valid
        if (paramList.size() != 2) {
            qWarning() << "Invalid parameter list for PGN 127251, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double RateOfTurn = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "RateOfTurn") {
                RateOfTurn = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }

        return SendPgn127251(SID, RateOfTurn);
        break;
    }
    case 127252: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 127252, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double Heave = 0.0;
        double Delay = 0.0;
        tN2kDelaySource DelaySource = N2kDD374_UserDefined;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "Heave") {
                Heave = paramParts[1].toDouble();
            } else if (paramName == "Delay") {
                Delay = paramParts[1].toDouble();
            } else if (paramName == "DelaySource") {
                DelaySource = static_cast<tN2kDelaySource>(paramParts[1].toUShort());
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn127252(SID, Heave, Delay, DelaySource);
        break;
    }
    case 127488: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 127488, size" << paramList.size();
            return false;
        }

        uchar EngineInstance = 0;
        double EngineSpeed = 0.0;
        double EngineBoostPressure = 0.0;
        int EngineTiltTrim = 0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "EngineInstance") {
                EngineInstance = paramParts[1].toUShort();
            } else if (paramName == "EngineSpeed") {
                EngineSpeed = paramParts[1].toDouble();
            } else if (paramName == "EngineBoostPressure") {
                EngineBoostPressure = paramParts[1].toDouble();
            } else if (paramName == "EngineTiltTrim") {
                EngineTiltTrim = paramParts[1].toInt();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }

        return SendPgn127488(EngineInstance, EngineSpeed, EngineBoostPressure, EngineTiltTrim);
        break;
    }
    case 127489: {
        // Check if the parameter list is valid
        if (paramList.size() != 13) {
            qWarning() << "Invalid parameter list for PGN 127489, size" << paramList.size();
            return false;
        }

        uchar EngineInstance = 0;
        double EngineOilPress = 0.0;
        double EngineOilTemp = 0.0;
        double EngineCoolantTemp = 0.0;
        double AltenatorVoltage = 0.0;
        double FuelRate = 0.0;
        double EngineHours = 0.0;
        double EngineCoolantPress = 0.0;
        double EngineFuelPress = 0.0;
        double EngineLoad = 0.0;
        double EngineTorque = 0.0;
        tN2kEngineDiscreteStatus1 Status1 = static_cast<tN2kEngineDiscreteStatus1>(0);
        tN2kEngineDiscreteStatus2 Status2 = static_cast<tN2kEngineDiscreteStatus2>(0);
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "EngineInstance") {
                EngineInstance = paramParts[1].toUShort();
            } else if (paramName == "EngineOilPress") {
                EngineOilPress = paramParts[1].toDouble();
            } else if (paramName == "EngineOilTemp") {
                EngineOilTemp = paramParts[1].toDouble();
            } else if (paramName == "EngineCoolantTemp") {
                EngineCoolantTemp = paramParts[1].toDouble();
            } else if (paramName == "AltenatorVoltage") {
                AltenatorVoltage = paramParts[1].toDouble();
            } else if (paramName == "FuelRate") {
                FuelRate = paramParts[1].toDouble();
            } else if (paramName == "EngineHours") {
                EngineHours = paramParts[1].toDouble();
            } else if (paramName == "EngineCoolantPress") {
                EngineCoolantPress = paramParts[1].toDouble();
            } else if (paramName == "EngineFuelPress") {
                EngineFuelPress = paramParts[1].toDouble();
            } else if (paramName == "EngineLoad") {
                EngineLoad = paramParts[1].toDouble();
            } else if (paramName == "EngineTorque") {
                EngineTorque = paramParts[1].toDouble();
            } else if (paramName == "Status1") {
                Status1 = static_cast<tN2kEngineDiscreteStatus1>(paramParts[1].toUShort());
            } else if (paramName == "Status2") {
                Status2 = static_cast<tN2kEngineDiscreteStatus2>(paramParts[1].toUShort());
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn127489(EngineInstance,
                             EngineOilPress,
                             EngineOilTemp,
                             EngineCoolantTemp,
                             AltenatorVoltage,
                             FuelRate,
                             EngineHours,
                             EngineCoolantPress,
                             EngineFuelPress,
                             EngineLoad,
                             EngineTorque,
                             Status1,
                             Status2);
        break;
    }
    case 127493: {
        // Check if the parameter list is valid
        if (paramList.size() != 5) {
            qWarning() << "Invalid parameter list for PGN 127493, size" << paramList.size();
            return false;
        }

        // unsigned char EngineInstance, tN2kTransmissionGear TransmissionGear, double OilPressure, double OilTemperature, unsigned char DiscreteStatus1
        uchar EngineInstance = 0;
        tN2kTransmissionGear TransmissionGear = N2kTG_Neutral;
        double OilPressure = 0.0;
        double OilTemperature = 0.0;
        uchar DiscreteStatus1 = 0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            // double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "EngineInstance") {
                EngineInstance = paramParts[1].toUShort();
            } else if (paramName == "TransmissionGear") {
                TransmissionGear = static_cast<tN2kTransmissionGear>(paramParts[1].toUShort());
            } else if (paramName == "OilPressure") {
                OilPressure = paramParts[1].toDouble();
            } else if (paramName == "OilTemperature") {
                OilTemperature = paramParts[1].toDouble();
            } else if (paramName == "DiscreteStatus1") {
                DiscreteStatus1 = paramParts[1].toUShort();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }

        return SendPgn127493(EngineInstance, TransmissionGear, OilPressure, OilTemperature, DiscreteStatus1);
        break;
    }
    case 128259: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 128259, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double WaterReferenced = 0.0;
        double GroundReferenced = 0.0;
        tN2kSpeedWaterReferenceType SWRT = N2kSWRT_Paddle_wheel;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "WaterReferenced") {
                WaterReferenced = paramParts[1].toDouble();
            } else if (paramName == "GroundReferenced") {
                GroundReferenced = paramParts[1].toDouble();
            } else if (paramName == "SWRT") {
                SWRT = static_cast<tN2kSpeedWaterReferenceType>(paramParts[1].toUShort());
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn128259(SID, WaterReferenced, GroundReferenced, SWRT);
        break;
    }
    case 128267: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 128267, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double DepthBelowTransducer = 0.0;
        double Offset = 0.0;
        double Range = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "DepthBelowTransducer") {
                DepthBelowTransducer = paramParts[1].toDouble();
            } else if (paramName == "Offset") {
                Offset = paramParts[1].toDouble();
            } else if (paramName == "Range") {
                Range = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn128267(SID, DepthBelowTransducer, Offset, Range);
        break;
    }
    case 129025: {
        // Check if the parameter list is valid
        if (paramList.size() != 2) {
            qWarning() << "Invalid parameter list for PGN 129025, size" << paramList.size();
            return false;
        }

        double Latitude = 0.0;
        double Longitude = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "Latitude") {
                Latitude = paramParts[1].toDouble();
            } else if (paramName == "Longitude") {
                Longitude = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn129025(Latitude, Longitude);
        break;
    }
    case 129026: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 129026, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double COG = 0.0;
        double SOG = 0.0;
        tN2kHeadingReference HeadingReference = N2khr_true;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "COG") {
                COG = paramParts[1].toDouble();
            } else if (paramName == "SOG") {
                SOG = paramParts[1].toDouble();
            } else if (paramName == "HeadingReference") {
                HeadingReference = static_cast<tN2kHeadingReference>(paramParts[1].toUShort());
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn129026(SID, HeadingReference, COG, SOG);
        break;
    }
    case 129029: {
        // Check if the parameter list is valid
        if (paramList.size() != 16) {
            qWarning() << "Invalid parameter list for PGN 129029, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        uint16_t DaysSince1970 = 0;
        double SecondsSinceMidnight = 0.0;
        double Latitude = 0.0;
        double Longitude = 0.0;
        double Altitude = 0.0;
        tN2kGNSStype GNSStype = N2kGNSSt_GPS;
        tN2kGNSSmethod GNSSmethod = N2kGNSSm_noGNSS;
        unsigned char nSatellites = 0;
        double HDOP = 0.0;
        double PDOP = 0.0;
        double GeoidalSeparation = 0.0;
        unsigned char nReferenceStations = 0;
        tN2kGNSStype ReferenceStationType = N2kGNSSt_GPS;
        uint16_t ReferenceSationID = 0;
        double AgeOfCorrection = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "DaysSince1970") {
                DaysSince1970 = paramParts[1].toUShort();
            } else if (paramName == "SecondsSinceMidnight") {
                SecondsSinceMidnight = paramParts[1].toDouble();
            } else if (paramName == "Latitude") {
                Latitude = paramParts[1].toDouble();
            } else if (paramName == "Longitude") {
                Longitude = paramParts[1].toDouble();
            } else if (paramName == "Altitude") {
                Altitude = paramParts[1].toDouble();
            } else if (paramName == "GNSStype") {
                GNSStype = static_cast<tN2kGNSStype>(paramParts[1].toUShort());
            } else if (paramName == "GNSSmethod") {
                GNSSmethod = static_cast<tN2kGNSSmethod>(paramParts[1].toUShort());
            } else if (paramName == "nSatellites") {
                nSatellites = paramParts[1].toUShort();
            } else if (paramName == "HDOP") {
                HDOP = paramParts[1].toDouble();
            } else if (paramName == "PDOP") {
                PDOP = paramParts[1].toDouble();
            } else if (paramName == "GeoidalSeparation") {
                GeoidalSeparation = paramParts[1].toDouble();
            } else if (paramName == "nReferenceStations") {
                nReferenceStations = paramParts[1].toUShort();
            } else if (paramName == "ReferenceStationType") {
                ReferenceStationType = static_cast<tN2kGNSStype>(paramParts[1].toUShort());
            } else if (paramName == "ReferenceSationID") {
                ReferenceSationID = paramParts[1].toUShort();
            } else if (paramName == "AgeOfCorrection") {
                AgeOfCorrection = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn129029(SID,
                             DaysSince1970,
                             SecondsSinceMidnight,
                             Latitude,
                             Longitude,
                             Altitude,
                             GNSStype,
                             GNSSmethod,
                             nSatellites,
                             HDOP,
                             PDOP,
                             GeoidalSeparation,
                             nReferenceStations,
                             ReferenceStationType,
                             ReferenceSationID,
                             AgeOfCorrection);
        break;
    }
    case 127257: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 127257, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double Yaw = 0.0;
        double Pitch = 0.0;
        double Roll = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "Yaw") {
                Yaw = paramParts[1].toDouble();
            } else if (paramName == "Pitch") {
                Pitch = paramParts[1].toDouble();
            } else if (paramName == "Roll") {
                Roll = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }

        return SendPgn127257(SID, Yaw, Pitch, Roll);
        break;
    }
    case 127258: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 127258, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        tN2kMagneticVariation Source = N2kmagvar_Manual;
        uint16_t DaysSince1970 = 0;
        double Variation = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "Source") {
                Source = static_cast<tN2kMagneticVariation>(paramParts[1].toUShort());
            } else if (paramName == "DaysSince1970") {
                DaysSince1970 = paramParts[1].toUShort();
            } else if (paramName == "Variation") {
                Variation = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn127258(SID, Source, DaysSince1970, Variation);
        break;
    }
    case 127505: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 127505, size" << paramList.size();
            return false;
        }

        unsigned char Instance = 0;
        tN2kFluidType FluidType = N2kft_Fuel;
        double Level = 0.0;
        double Capacity = 0.0;

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "Instance") {
                Instance = paramParts[1].toUShort();
            } else if (paramName == "FluidType") {
                FluidType = static_cast<tN2kFluidType>(paramParts[1].toUShort());
            } else if (paramName == "Level") {
                Level = paramParts[1].toDouble();
            } else if (paramName == "Capacity") {
                Capacity = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn127505(Instance, FluidType, Level, Capacity);
        break;
    }
    case 127506: {
        // Check if the parameter list is valid
        if (paramList.size() != 8) {
            qWarning() << "Invalid parameter list for PGN 127506, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        unsigned char DCInstance = 0;
        tN2kDCType DCType = N2kDCt_Battery;
        uint8_t StateOfCharge = 0.0;
        uint8_t StateOfHealth = 0.0;
        double TimeRemaining = 0.0;
        double RippleVoltage = 0.0;
        double Capacity = 0.0;

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "DCInstance") {
                DCInstance = paramParts[1].toUShort();
            } else if (paramName == "DCType") {
                DCType = static_cast<tN2kDCType>(paramParts[1].toUShort());
            } else if (paramName == "StateOfCharge") {
                bool valid = false;
                const double percentage = paramParts[1].toDouble(&valid);
                if (!valid || !std::isfinite(percentage) || std::floor(percentage) != percentage
                    || percentage < 0 || (percentage > 100 && percentage != 255)) return false;
                StateOfCharge = static_cast<uint8_t>(percentage);
            } else if (paramName == "StateOfHealth") {
                bool valid = false;
                const double percentage = paramParts[1].toDouble(&valid);
                if (!valid || !std::isfinite(percentage) || std::floor(percentage) != percentage
                    || percentage < 0 || (percentage > 100 && percentage != 255)) return false;
                StateOfHealth = static_cast<uint8_t>(percentage);
            } else if (paramName == "TimeRemaining") {
                TimeRemaining = paramParts[1].toDouble();
            } else if (paramName == "RippleVoltage") {
                RippleVoltage = paramParts[1].toDouble();
            } else if (paramName == "Capacity") {
                Capacity = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn127506(SID, DCInstance, DCType, StateOfCharge, StateOfHealth, TimeRemaining, RippleVoltage, Capacity);
        break;
    }
    case 127507: {
        // Check if the parameter list is valid
        if (paramList.size() != 7) {
            qWarning() << "Invalid parameter list for PGN 127507, size" << paramList.size();
            return false;
        }

        unsigned char Instance = 0;
        unsigned char BatteryInstance = 0;
        tN2kChargeState ChargeState = N2kCS_Not_Charging;
        tN2kChargerMode ChargerMode = N2kCM_Standalone;
        tN2kOnOff Enabled = N2kOnOff_Off;
        tN2kOnOff EqualizationPending = N2kOnOff_Off;
        double EqualizationTimeRemaining = 0.0;

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "Instance") {
                Instance = paramParts[1].toUShort();
            } else if (paramName == "BatteryInstance") {
                BatteryInstance = paramParts[1].toUShort();
            } else if (paramName == "ChargeState") {
                ChargeState = static_cast<tN2kChargeState>(paramParts[1].toUShort());
            } else if (paramName == "ChargerMode") {
                ChargerMode = static_cast<tN2kChargerMode>(paramParts[1].toUShort());
            } else if (paramName == "Enabled") {
                Enabled = static_cast<tN2kOnOff>(paramParts[1].toUShort());
            } else if (paramName == "EqualizationPending") {
                EqualizationPending = static_cast<tN2kOnOff>(paramParts[1].toUShort());
            } else if (paramName == "EqualizationTimeRemaining") {
                EqualizationTimeRemaining = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn127507(Instance, BatteryInstance, ChargeState, ChargerMode, Enabled, EqualizationPending, EqualizationTimeRemaining);
        break;
    }
    case 127508: {
        // Check if the parameter list is valid
        if (paramList.size() != 5) {
            qWarning() << "Invalid parameter list for PGN 127508, size" << paramList.size();
            return false;
        }

        /*
 * unsigned char BatteryInstance, double BatteryVoltage, double BatteryCurrent,
                     double BatteryTemperature, unsigned char SID
 */
        unsigned char BatteryInstance = 0;
        double BatteryVoltage = 0.0;
        double BatteryCurrent = 0.0;
        double BatteryTemperature = 0.0;
        unsigned char SID = 0;

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            // double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "BatteryInstance") {
                BatteryInstance = paramParts[1].toUShort();
            } else if (paramName == "BatteryVoltage") {
                BatteryVoltage = paramParts[1].toDouble();
            } else if (paramName == "BatteryCurrent") {
                BatteryCurrent = paramParts[1].toDouble();
            } else if (paramName == "BatteryTemperature") {
                BatteryTemperature = paramParts[1].toDouble();
            } else if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn127508(BatteryInstance, BatteryVoltage, BatteryCurrent, BatteryTemperature, SID);
        break;
    }
    case 129033: {
        // Check if the parameter list is valid
        if (paramList.size() != 3) {
            qWarning() << "Invalid parameter list for PGN 129033, size" << paramList.size();
            return false;
        }

        uint16_t DaysSince1970 = 0;
        double SecondsSinceMidnight = 0.0;
        int16_t LocalOffset = 0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "DaysSince1970") {
                DaysSince1970 = paramParts[1].toUShort();
            } else if (paramName == "SecondsSinceMidnight") {
                SecondsSinceMidnight = paramParts[1].toDouble();
            } else if (paramName == "LocalOffset") {
                LocalOffset = paramParts[1].toShort();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn129033(DaysSince1970, SecondsSinceMidnight, LocalOffset);
        break;
    }
    case 129283: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 129283, size" << paramList.size();
            return false;
        }
        unsigned char SID = 0;
        tN2kXTEMode XTEMode = N2kxtem_Simulator;
        bool NavigationTerminated = false;
        double XTE = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "XTEMode") {
                XTEMode = static_cast<tN2kXTEMode>(paramParts[1].toUShort());
            } else if (paramName == "NavigationTerminated") {
                NavigationTerminated = paramParts[1].toInt();
            } else if (paramName == "XTE") {
                XTE = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn129283(SID, XTEMode, NavigationTerminated, XTE);
        break;
    }
    case 129284: {
        // Check if the parameter list is valid
        if (paramList.size() != 15) {
            qWarning() << "Invalid parameter list for PGN 129284, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double DistanceToWaypoint = 0.0;
        tN2kHeadingReference BearingReference = N2khr_true;
        bool PerpendicularCrossed = false;
        bool ArrivalCircleEntered = false;
        tN2kDistanceCalculationType CalculationType = N2kdct_GreatCircle;
        double ETATime = 0.0;
        int16_t ETADate = 0;
        double BearingOriginToDestinationWaypoint = 0.0;
        double BearingPositionToDestinationWaypoint = 0.0;
        uint32_t OriginWaypointNumber = 0;
        uint32_t DestinationWaypointNumber = 0;
        double DestinationLatitude = 0.0;
        double DestinationLongitude = 0.0;
        double WaypointClosingVelocity = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "DistanceToWaypoint") {
                DistanceToWaypoint = paramParts[1].toDouble();
            } else if (paramName == "BearingReference") {
                BearingReference = static_cast<tN2kHeadingReference>(paramParts[1].toUShort());
            } else if (paramName == "PerpendicularCrossed") {
                PerpendicularCrossed = paramParts[1].toInt();
            } else if (paramName == "ArrivalCircleEntered") {
                ArrivalCircleEntered = paramParts[1].toInt();
            } else if (paramName == "CalculationType") {
                CalculationType = static_cast<tN2kDistanceCalculationType>(paramParts[1].toUShort());
            } else if (paramName == "ETATime") {
                ETATime = paramParts[1].toDouble();
            } else if (paramName == "ETADate") {
                ETADate = paramParts[1].toShort();
            } else if (paramName == "BearingOriginToDestinationWaypoint") {
                BearingOriginToDestinationWaypoint = paramParts[1].toDouble();
            } else if (paramName == "BearingPositionToDestinationWaypoint") {
                BearingPositionToDestinationWaypoint = paramParts[1].toDouble();
            } else if (paramName == "OriginWaypointNumber") {
                OriginWaypointNumber = paramParts[1].toUInt();
            } else if (paramName == "DestinationWaypointNumber") {
                DestinationWaypointNumber = paramParts[1].toUInt();
            } else if (paramName == "DestinationLatitude") {
                DestinationLatitude = paramParts[1].toDouble();
            } else if (paramName == "DestinationLongitude") {
                DestinationLongitude = paramParts[1].toDouble();
            } else if (paramName == "WaypointClosingVelocity") {
                WaypointClosingVelocity = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn129284(SID,
                             DistanceToWaypoint,
                             BearingReference,
                             PerpendicularCrossed,
                             ArrivalCircleEntered,
                             CalculationType,
                             ETATime,
                             ETADate,
                             BearingOriginToDestinationWaypoint,
                             BearingPositionToDestinationWaypoint,
                             OriginWaypointNumber,
                             DestinationWaypointNumber,
                             DestinationLatitude,
                             DestinationLongitude,
                             WaypointClosingVelocity);
        break;
    }
    case 130306:
    case 130313: {
        const QStringList required = pgn == 130306
            ? QStringList{"SID", "WindSpeed", "WindAngle", "WindReference"}
            : QStringList{"SID", "HumidityInstance", "HumiditySource", "ActualHumidity", "SetHumidity"};
        if (paramList.size() != required.size()) return false;
        QMap<QString, double> fields;
        for (const auto &param : paramList) {
            const auto pair = param.split(':');
            bool ok = false; const double v = pair.value(1).toDouble(&ok);
            if (pair.size() != 2 || !ok || !std::isfinite(v) || !required.contains(pair[0]) || fields.contains(pair[0])) return false;
            const auto &name = pair[0];
            if ((name == "SID" || name == "HumidityInstance") && (v < 0 || v > 255 || std::floor(v) != v)) return false;
            if (name == "WindReference" && !(v == 0 || v == 1 || v == 2 || v == 3 || v == 4 || v == 6 || v == 7)) return false;
            if (name == "HumiditySource" && !(v == 0 || v == 1 || v == 255)) return false;
            if (v != N2kDoubleNA && ((name == "WindAngle" && (v < 0 || v >= 2*M_PI)) || (name == "WindSpeed" && (v < 0 || v > 655.32))
                || ((name == "ActualHumidity" || name == "SetHumidity") && (v < 0 || v > 100)))) return false;
            fields[name] = v;
        }
        tN2kMsg message;
        if (pgn == 130306) SetN2kPGN130306(message, fields["SID"], fields["WindSpeed"], fields["WindAngle"], tN2kWindReference(int(fields["WindReference"])));
        else SetN2kPGN130313(message, fields["SID"], fields["HumidityInstance"], tN2kHumiditySource(int(fields["HumiditySource"])), fields["ActualHumidity"], fields["SetHumidity"]);
        if (interfaceType == "Actisense") return sendActisenseMessage(message);
        if (interfaceType == "Network NMEA2000" || interfaceType == "SocketCan" || (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) return sendCanMessage(message);
        if (interfaceType == "SignalK") return mode == Sender && signalK->publish(message);
        return false;
    }
    case 130310: {
        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid parameter list for PGN 130310, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double WaterTemperature = 0.0;
        double OutsideAmbientAirTemperature = 0.0;
        double AtmosphericPressure = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "WaterTemperature") {
                WaterTemperature = paramParts[1].toDouble();
            } else if (paramName == "OutsideAmbientAirTemperature") {
                OutsideAmbientAirTemperature = paramParts[1].toDouble();
            } else if (paramName == "AtmosphericPressure") {
                AtmosphericPressure = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn130310(SID, WaterTemperature, OutsideAmbientAirTemperature, AtmosphericPressure);
        break;
    }
    case 130312: {
        //check if the parameter list is valid
        if (paramList.size() != 5) {
            qWarning() << "Invalid parameter list for PGN 130312, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        double TemperatureInstance = 0.0;
        tN2kTempSource TemperatureSource = N2kts_SeaTemperature;
        double ActualTemperature = 0.0;
        double SetTemperature = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "TemperatureInstance") {
                TemperatureInstance = paramParts[1].toDouble();
            } else if (paramName == "TemperatureSource") {
                TemperatureSource = static_cast<tN2kTempSource>(paramParts[1].toUShort());
            } else if (paramName == "ActualTemperature") {
                ActualTemperature = paramParts[1].toDouble();
            } else if (paramName == "SetTemperature") {
                SetTemperature = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn130312(SID, TemperatureInstance, TemperatureSource, ActualTemperature, SetTemperature);
        break;
    }
    case 130316: {
        // Check if the parameter list is valid
        if (paramList.size() != 5) {
            qWarning() << "Invalid parameter list for PGN 130316, size" << paramList.size();
            return false;
        }

        unsigned char SID = 0;
        unsigned char TempInstance = 0;
        tN2kTempSource TempSource = N2kts_SeaTemperature;
        double ActualTemperature = 0.0;
        double SetTemperature = 0.0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "SID") {
                SID = paramParts[1].toUShort();
            } else if (paramName == "TempInstance") {
                TempInstance = paramParts[1].toUShort();
            } else if (paramName == "TempSource") {
                TempSource = static_cast<tN2kTempSource>(paramParts[1].toUShort());
            } else if (paramName == "ActualTemperature") {
                ActualTemperature = paramParts[1].toDouble();
            } else if (paramName == "SetTemperature") {
                SetTemperature = paramParts[1].toDouble();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }
        return SendPgn130316(SID, TempInstance, TempSource, ActualTemperature, SetTemperature);
        break;
    }
    case 130576: {
        // Check if the parameter list is valid
        if (paramList.size() != 2) {
            qWarning() << "Invalid parameter list for PGN 130576, size" << paramList.size();
            return false;
        }

        int8_t PortTrimTab = 0;
        int8_t StbdTrimTab = 0;
        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (const auto &paramDetail : paramList) {
            QStringList paramParts = paramDetail.split(':');
            if (paramParts.size() != 2) {
                qWarning() << "Invalid parameter format:" << paramDetail;
                return false;
            }

            QString paramName = paramParts[0];
            //double paramValue = paramParts[1].toDouble();

            // Check the parameter name and set the value accordingly
            if (paramName == "PortTrimTab") {
                PortTrimTab = paramParts[1].toUShort();
            } else if (paramName == "StbdTrimTab") {
                StbdTrimTab = paramParts[1].toUShort();
            } else {
                qWarning() << "Unknown parameter name:" << paramName;
                return false;
            }
        }

        return SendPgn130576(PortTrimTab, StbdTrimTab);
        break;
    }
    default:
        qWarning() << "'Nmea2000Handler::sendPgn' Unknown PGN:" << pgn;
        return false;
    }
}

bool Nmea2000Handler::beginRouteService(){
    if(discovery && discovery->busy())return false;
    if(routeAuthorized)return true;
    if(!TransportCapabilities::canTransmit(interfaceType))return false;
    if(interfaceType=="Network NMEA2000"){
        if(!networkSession->start("service"))return false;routeAuthorized=true;
    }else if(interfaceType=="SocketCan"){
        if(!IsCanInterfaceOpen())return false;routeAuthorized=true;socketSession->authorizeService(true);
    }else if((interfaceType=="Waveshare USB-CAN-A"||interfaceType=="CANable SLCAN")&&IsWaveshareConfigured()){
        if(waveshare->isTransmitting())return false;
        routeAuthorized=true;if(!waveshare->startService(wavesharePort,waveshareIdentity)){routeAuthorized=false;return false;}
    }else return false;
    emit InterfaceStateChanged();return true;
}
int Nmea2000Handler::routeServiceSource() const { return interfaceType=="Network NMEA2000" ? networkSession->sourceAddress() : interfaceType=="SocketCan" ? socketSession->sourceAddress() : waveshare->sourceAddress(); }
bool Nmea2000Handler::routeServiceReady() const{return routeAuthorized&&(interfaceType=="Network NMEA2000"?networkSession->ready():interfaceType=="SocketCan"?socketSession->routeServiceReady():waveshare->serviceReady());}
bool Nmea2000Handler::sendRouteService(const tN2kMsg &m){
    if(!routeAuthorized||!RouteServicePolicy::allowed(m))return false;
    const bool accepted=interfaceType=="Network NMEA2000"?networkSession->send(m,true):interfaceType=="SocketCan"?socketSession->sendService(m):waveshare->sendService(m);
    const int source=interfaceType=="Network NMEA2000"?networkSession->sourceAddress():interfaceType=="SocketCan"?socketSession->sourceAddress():waveshare->sourceAddress();
    ApplicationLog::n2k("TX",interfaceType,m,accepted?"route-service-software-accepted":"route-service-rejected",accepted?source:-1);return accepted;
}
void Nmea2000Handler::endRouteService(bool abort){
    if(abort && discovery)discovery->cancel(tr("Discovery cancelled by Stop/F12, mode or transport change; observations retained."));
    if(!routeAuthorized)return;routeAuthorized=false;socketSession->authorizeService(false);
    if(interfaceType=="Network NMEA2000"){networkSession->stop(true);}
    else if(interfaceType=="SocketCan"){if(abort)socketSession->stopSending();}
    else {waveshare->stop(tr("Route service ended; instrument Sending remains disabled."));if(!abort&&waveshareConfigured)waveshare->start(wavesharePort);}
    emit RouteServiceRevoked();emit InterfaceStateChanged();
}
bool Nmea2000Handler::routeServiceActive() const {return routeAuthorized || (discovery&&discovery->busy());}
void Nmea2000Handler::setMode(Mode mode)
{
    if(this->mode!=mode){endRouteService(true);networkSession->revoke();}
    if (mode != Sender) signalK->stop();
    const bool changed = this->mode != mode;
    const bool enteringViewer = mode == Viewer && changed;
    this->mode=mode;
    socketSession->setViewer(mode==Viewer);
    if ((interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN")) {
        if (enteringViewer && waveshareConfigured) waveshare->start(wavesharePort);
        else if (changed && mode != Viewer) waveshare->stop(tr("Serial CAN stopped. Connect Selected Device opens RX/standby; Enable Sending explicitly starts Generate TX."));
    }
}

Nmea2000Handler *Nmea2000Handler::getInstance()
{
    return instance;
}

void Nmea2000Handler::synchronizeDiagnosticContext()
{
    bool live = false;
    if(interfaceType=="Network NMEA2000") live=networkSession->connected();
    else if(interfaceType=="SocketCan") live=socketSession->state()==SocketCanSession::Initializing || socketSession->state()==SocketCanSession::Ready;
    else if(interfaceType=="CANable SLCAN" || interfaceType=="Waveshare USB-CAN-A") live=IsWaveshareActive();
    else if(interfaceType=="Actisense") live=serialPort->isOpen();
    diagnosticState->setSession(interfaceType+ (live?"/connected":"/inactive"),interfaceType+(live?" - connected":" - inactive / injected replay"));
}

void Nmea2000Handler::nmea2000MessageReceived(const tN2kMsg &N2kMsg)
{
    synchronizeDiagnosticContext();
    const auto diagnosticKey=diagnosticState->observe(N2kMsg);
    const auto diagnostic=diagnosticState->message(diagnosticKey);
    if(diagnostic.malformed)qWarning().noquote()<<"Malformed received NMEA2000 PGN:"<<N2kMsg.PGN<<diagnostic.status;
    ApplicationLog::n2k("RX",interfaceType,N2kMsg,"transport-received");
    emit RouteServiceMessage(N2kMsg);
    if (mode != Viewer) {
        return;
    }

    if(N2kMsg.PGN==60928 && N2kMsg.DataLen==8) {
        emit CanSourceIdentityObserved(QString::number(N2kMsg.Source),QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(N2kMsg.Data),8).toHex()));
        return;
    }
    if(N2kMsg.PGN==130067) {
        QStringList fields;if(Expansion::decodeRouteGeometry(N2kMsg,fields))emit RcvdPgn("130067",fields,QString::number(N2kMsg.Source));
        return;
    }
    if(N2kMsg.PGN==130066) {
        QStringList fields;if(Expansion::decodeRouteAttributes(N2kMsg,fields))emit RcvdPgn("130066",fields,QString::number(N2kMsg.Source));
        return;
    }
    if(Expansion::definition(QString::number(N2kMsg.PGN))) {
        QStringList fields;if(Expansion::decode(N2kMsg,fields))emit RcvdPgn(QString::number(N2kMsg.PGN),fields,QString::number(N2kMsg.Source));
        return;
    }
    switch (N2kMsg.PGN) {
    case 128777:
    case 128778: {
        QStringList fields;if(WindlassStatus::decode(N2kMsg,fields))emit RcvdPgn(QString::number(N2kMsg.PGN),fields,QString::number(N2kMsg.Source));
        break;
    }
    case 129039: {
        QStringList fields;if(AisClassBPosition129039::decode(N2kMsg,fields))emit RcvdPgn("129039",fields,QString::number(N2kMsg.Source));
        break;
    }
    case 129810: {
        QStringList fields;if(AisClassBStatic129810::decode(N2kMsg,fields))emit RcvdPgn("129810",fields,QString::number(N2kMsg.Source));
        break;
    }
    case 129809: {
        QStringList fields;
        if (AisClassBStatic129809::decode(N2kMsg, fields)) emit RcvdPgn("129809", fields, QString::number(N2kMsg.Source));
        break;
    }
    case 126992:
        RcvdPgn126992(N2kMsg);
        break;
    case 127245:
        RcvdPgn127245(N2kMsg);
        break;
    case 127250:
        RcvdPgn127250(N2kMsg);
        break;
    case 127251:
        RcvdPgn127251(N2kMsg);
        break;
    case 127252:
        RcvdPgn127252(N2kMsg);
        break;
    case 127257:
        RcvdPgn127257(N2kMsg);
        break;
    case 127258:
        RcvdPgn127258(N2kMsg);
        break;
    case 127488:
        RcvdPgn127488(N2kMsg);
        break;
    case 127489:
        RcvdPgn127489(N2kMsg);
        break;
    case 127493:
        RcvdPgn127493(N2kMsg);
        break;
    case 127505:
        RcvdPgn127505(N2kMsg);
        break;
    case 127506:
        RcvdPgn127506(N2kMsg);
        break;
    case 127507:
        RcvdPgn127507(N2kMsg);
        break;
    case 127508:
        RcvdPgn127508(N2kMsg);
        break;
    case 128259:
        RcvdPgn128259(N2kMsg);
        break;
    case 128267:
        RcvdPgn128267(N2kMsg);
        break;
    case 129025:
        RcvdPgn129025(N2kMsg);
        break;
    case 129026:
        RcvdPgn129026(N2kMsg);
        break;
    case 129029:
        RcvdPgn129029(N2kMsg);
        break;
    case 129033:
        RcvdPgn129033(N2kMsg);
        break;
    case 129283:
        RcvdPgn129283(N2kMsg);
        break;
    case 129284:
        RcvdPgn129284(N2kMsg);
        break;
    case 130306:
    case 130313:
        RcvdEnvironmental(N2kMsg);
        break;
    case 130310:
        RcvdPgn130310(N2kMsg);
        break;
    case 130311:
        RcvdPgn130311(N2kMsg);
        break;
    case 130312:
        RcvdPgn130312(N2kMsg);
        break;
    case 130316:
        RcvdPgn130316(N2kMsg);
        break;
    case 130576:
        RcvdPgn130576(N2kMsg);
        break;
    default:
        // Don't log 60928: ISO Address Claim, 126996: Product Information, 126998: Configuration Information
        if (N2kMsg.PGN != 60928 && N2kMsg.PGN != 126996 && N2kMsg.PGN != 126998) {
            if(!diagnostic.malformed && !diagnostic.qualified)ApplicationLog::reportUndecodedPgn(quint32(N2kMsg.PGN));
        }
        break;
    }
}

bool Nmea2000Handler::sendActisenseMessage(const tN2kMsg &message)
{
    if (!TransportCapabilities::canTransmit(interfaceType)) {
        emit TransmitError(TransportCapabilities::actisenseNotice());
        return false;
    }
    const bool accepted = mode == Sender && interfaceType == "Actisense" && nmea2000Actisense
        && serialPort->isOpen() && nmea2000Actisense->SendMessage(message);
    ApplicationLog::n2k("TX",interfaceType,message,accepted?"software-accepted":"rejected");
    if (!accepted) emit TransmitError("Actisense transmission failed. Check the serial connection; outbound delivery remains unverified (Issue #1).");
    return accepted;
}

bool Nmea2000Handler::sendCanMessage(const tN2kMsg &message)
{
    const bool wave = (interfaceType == "Waveshare USB-CAN-A" || interfaceType == "CANable SLCAN");
    const bool accepted = mode == Sender && (wave ? waveshare->send(message)
        : interfaceType == "Network NMEA2000" ? networkSession->send(message) : interfaceType == "SocketCan" && socketSession->send(message));
    const int source=interfaceType=="Network NMEA2000"?networkSession->sourceAddress():wave?waveshare->sourceAddress():socketSession->sourceAddress();
    ApplicationLog::n2k("TX",interfaceType,message,accepted?"software-accepted":"rejected",accepted?source:-1);
    if (!accepted) emit TransmitError(interfaceType=="Network NMEA2000" ? "Network CAN transmission is not authorized or not ready. Reconnect and explicitly Enable Sending to retry." : wave ? "Serial CAN transmission failed; explicitly Enable Sending to retry."
        : "SocketCAN transmission failed. Check the interface; a restart may be required.");
    return accepted;
}

void Nmea2000Handler::ConfigureSignalK(const SignalKConfig &config) { signalK->configure(config); }
void Nmea2000Handler::StartSignalKSending() { if (mode == Sender && interfaceType == "SignalK") signalK->start(); }
void Nmea2000Handler::StopSignalKSending() { signalK->stop(); }

bool Nmea2000Handler::IsSignalKReady() const { return signalK->isReady(); }
bool Nmea2000Handler::IsSignalKActive() const { return signalK->isActive(); }
