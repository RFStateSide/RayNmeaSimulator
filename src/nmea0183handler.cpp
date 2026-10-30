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
#include "expansioninstruments.h"
#include "nmea0183handler.h"
#include "nmea0183compat.h"
#include "NMEA0183Messages.h"

#include "convert.h"
#include <QRegularExpression>
#include <cmath>

Nmea0183Handler *Nmea0183Handler::instance = nullptr;

Nmea0183Handler::Nmea0183Handler(QObject *parent, QSerialPort *serialPort)
    : QObject{parent}
{
    instance = this;
    spNmea0183 = serialPort ? serialPort : new QSerialPort;
    spNmea0183->setParent(this);
    spNmea0183->setReadBufferSize(4096);
    connect(spNmea0183, &QSerialPort::readyRead, this, &Nmea0183Handler::onSerialDataAvailable);
    connect(spNmea0183, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError) return;
        const QString message = "NMEA0183 serial error: " + spNmea0183->errorString();
        emit ShowStatusBarMessage(message, 3000);
        emit TransmitError(message);
        if (error == QSerialPort::ResourceError || error == QSerialPort::ReadError || error == QSerialPort::WriteError)
            CloseDevice();
    });
}

Nmea0183Handler::~Nmea0183Handler()
{
    if (spNmea0183->isOpen()) {
        spNmea0183->close();
    }
    if (instance == this) instance = nullptr;
    delete spNmea0183;
}

bool Nmea0183Handler::Initialize(const QString &portName, QSerialPort::BaudRate baudRate)
{
    CloseDevice();
    portInitilized = false;
    if (portName.trimmed().isEmpty()) return false;
    spNmea0183->setPortName(portName);
    portInitilized = spNmea0183->setBaudRate(baudRate)
        && spNmea0183->setParity(QSerialPort::NoParity)
        && spNmea0183->setDataBits(QSerialPort::Data8)
        && spNmea0183->setStopBits(QSerialPort::OneStop)
        && spNmea0183->setFlowControl(QSerialPort::NoFlowControl);
    if (!portInitilized) emit ShowStatusBarMessage("Initializing NMEA0183 failed", 3000);
    return portInitilized;
}

bool Nmea0183Handler::OpenDevice()
{
    if (!portInitilized) return false;
    // Only open device if not already opened
    if (!spNmea0183->isOpen()) {
        //QDateTime currentDateTime = QDateTime::currentDateTime();
        //timeLastValidPacket = currentDateTime.toSecsSinceEpoch() - 5;

        buffer.clear();
        types.clear();
        if (spNmea0183->open(QIODevice::ReadWrite)) {
            //txPollTimer->start(TX_POLL_TIME);
            emit InterfaceStateChanged();
            qInfo() << "Success opening NMEA0183 comm port" << spNmea0183->portName();
            return true;
        } else {
            emit ShowStatusBarMessage("Failed opening NMEA0183 comm port", 3000);
            qWarning() << "Failed opening NMEA0183 comm port" << spNmea0183->portName();
            return false;
        }
    }

    return true;
}

bool Nmea0183Handler::CloseDevice()
{
    buffer.clear();
    types.clear();
    if (spNmea0183->isOpen()) {
        spNmea0183->close();
        emit InterfaceStateChanged();
        qInfo("Closed NMEA0183 comm port");
        return true;
    }

    return false;
}

bool Nmea0183Handler::IsOpen()
{
    return spNmea0183->isOpen();
}

bool Nmea0183Handler::SendNmea0183Message(const QString &message)
{
    if (mode != Sender) return false;
    const QByteArray bytes = message.toLatin1();
    if (!message.endsWith("\r\n") || !validateChecksum(message.chopped(2)) || !spNmea0183->isWritable()) {
        emit ShowStatusBarMessage("NMEA0183 message rejected or port closed", 3000);
        emit TransmitError("NMEA0183 message rejected or port closed");
        return false;
    }
    // Success means all bytes accepted by Qt's asynchronous output buffer, not
    // physical delivery. Never silently retry a partially accepted sentence.
    if (spNmea0183->bytesToWrite() + bytes.size() > 4096) {
        emit ShowStatusBarMessage("NMEA0183 output backlog full", 3000);
        emit TransmitError("NMEA0183 output backlog full");
        return false;
    }
    const qint64 accepted = spNmea0183->write(bytes);
    ApplicationLog::sentence("TX", "NMEA0183 serial", bytes,accepted==bytes.size()?"Qt-buffer-accepted":accepted>0?"partially-accepted":"rejected");
    if (accepted != bytes.size()) {
        emit ShowStatusBarMessage("NMEA0183 write failed or was partial; connection closed", 3000);
        emit TransmitError("NMEA0183 write failed or was partial; connection closed");
        CloseDevice();
        return false;
    }
    return true;
}

bool Nmea0183Handler::SendTestGGA()
{
    tNMEA0183Msg NMEA0183Msg;

    double GPSTime = 45296.78;
    double Latitude = 42.813903;
    double Longitude = -70.867977;
    uint32_t GPSQualityIndicator = 1;
    uint32_t SatelliteCount = 12;
    double HDOP = 0.9;
    double Altitude = 0.0;
    double GeoidalSeparation = 0.0;
    double DGPSAge = 0.0;
    uint32_t DGPSReferenceStationID = 0;
    const char *Src = "GP";

    bool bRet = NMEA0183SetGGA(
        NMEA0183Msg, GPSTime, Latitude, Longitude, GPSQualityIndicator, SatelliteCount, HDOP, Altitude, GeoidalSeparation, DGPSAge, DGPSReferenceStationID, Src);

    if (!bRet) {
        emit ShowStatusBarMessage("Failed sending NMEA0183 message", 3000);
        qWarning() << "Failed sending NMEA0183 message";
        return false;
    }
    return SendSntGGA(NMEA0183Msg);
}

// $GPGGA,182435.00,6023.20859,N,02219.99442,E,2,10,0.9,4.0,M,20.6,M,5.0,0120*4D
bool Nmea0183Handler::SendSntGGA(const tNMEA0183Msg &NMEA0183Msg)
{
    QString message = constructMsgToSend(NMEA0183Msg);

    if (!SendNmea0183Message(message)) {
        qWarning() << "Failed sending NMEA0183 GGA message";
        return false;
    }

    return true;
}

bool Nmea0183Handler::RcvdSntGGA(const tNMEA0183Msg &NMEA0183Msg, const QString &talker)
{
    if (QString::fromLatin1(NMEA0183Msg.MessageCode()) != "GGA" || !Nmea0183Compat::validFields(NMEA0183Msg)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString SntLabel = "GGA";
    QStringList paramDetails;

    double GPSTime;
    double Latitude;
    double Longitude;
    int GPSQualityIndicator;
    int SatelliteCount;
    double HDOP;
    double Altitude;
    double GeoidalSeparation;
    double DGPSAge;
    int DGPSReferenceStationID;

    if (NMEA0183ParseGGA_nc(
            NMEA0183Msg, GPSTime, Latitude, Longitude, GPSQualityIndicator, SatelliteCount, HDOP, Altitude, GeoidalSeparation, DGPSAge, DGPSReferenceStationID)) {
        paramDetails.append({"GPSTime", QString::number(GPSTime, 'g', 17)});
        paramDetails.append({"Latitude", QString::number(Latitude, 'g', 15)});
        paramDetails.append({"Longitude", QString::number(Longitude, 'g', 15)});
        paramDetails.append({"GPSQualityIndicator", QString::number(GPSQualityIndicator)});
        paramDetails.append({"SatelliteCount", QString::number(SatelliteCount)});
        paramDetails.append({"HDOP", QString::number(HDOP, 'g', 17)});
        paramDetails.append({"Altitude", QString::number(Altitude, 'g', 17)});
        paramDetails.append({"GeoidalSeparation", QString::number(GeoidalSeparation, 'g', 17)});
        paramDetails.append({"DGPSAge", QString::number(DGPSAge, 'g', 17)});
        paramDetails.append({"DGPSReferenceStationID", QString::number(DGPSReferenceStationID)});
        emit RcvdSnt(SntLabel, paramDetails, talker);
    } else {
        qWarning() << "Failed parsing GGA sentence";
        return false;
    }

    return true;
}

bool Nmea0183Handler::SendSntRMC(const tNMEA0183Msg &NMEA0183Msg)
{
    QString message = constructMsgToSend(NMEA0183Msg);

    if (!SendNmea0183Message(message)) {
        qWarning() << "Failed sending NMEA0183 RMC message";
        return false;
    }

    return true;
}

bool Nmea0183Handler::RcvdSntRMC(const tNMEA0183Msg &NMEA0183Msg, const QString &talker)
{
    if (QString::fromLatin1(NMEA0183Msg.MessageCode()) != "RMC" || !Nmea0183Compat::validFields(NMEA0183Msg)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString SntLabel = "RMC";
    QStringList paramDetails;

    /*
 * const tNMEA0183Msg &NMEA0183Msg, double &GPSTime, char &Status, double &Latitude, double &Longitude,
                      double &TrueCOG, double &SOG, unsigned long &DaysSince1970, double &Variation, time_t *DateTime
 */
    double GPSTime;
    char Status;
    double Latitude;
    double Longitude;
    double TrueCOG;
    double SOG;
    unsigned long DaysSince1970;
    double Variation;
    time_t DateTime;

    if (NMEA0183ParseRMC_nc(NMEA0183Msg, GPSTime, Status, Latitude, Longitude, TrueCOG, SOG, DaysSince1970, Variation, &DateTime)) {
        // The pinned desktop parser uses localtime/mktime. RMC dates are UTC.
        const QString wireDate = QString::fromLatin1(NMEA0183Msg.Field(8));
        if (!wireDate.isEmpty()) {
            const QDate utcDate(2000 + wireDate.right(2).toInt(), wireDate.mid(2, 2).toInt(), wireDate.left(2).toInt());
            DaysSince1970 = QDate(1970, 1, 1).daysTo(utcDate);
            DateTime = QDateTime(utcDate, QTime(0, 0), QTimeZone::utc()).toSecsSinceEpoch();
            if (GPSTime != NMEA0183DoubleNA) DateTime += static_cast<time_t>(std::floor(GPSTime));
        }
        paramDetails.append({"GPSTime", QString::number(GPSTime, 'g', 17)});
        paramDetails.append({"Status", QString(QChar::fromLatin1(Status))});
        paramDetails.append({"Latitude", QString::number(Latitude, 'g', 15)});
        paramDetails.append({"Longitude", QString::number(Longitude, 'g', 15)});
        paramDetails.append({"TrueCOG", QString::number(TrueCOG, 'g', 17)});
        paramDetails.append({"SOG", QString::number(SOG, 'g', 17)});
        paramDetails.append({"DaysSince1970", QString::number(DaysSince1970)});
        paramDetails.append({"Variation", QString::number(Variation, 'g', 17)});
        paramDetails.append({"DateTime", QString::number(DateTime)});
        emit RcvdSnt(SntLabel, paramDetails, talker);
    } else {
        qWarning() << "Failed parsing RMC sentence";
        return false;
    }

    return true;
}

bool Nmea0183Handler::SendSntHDG(const tNMEA0183Msg &NMEA0183Msg)
{
    QString message = constructMsgToSend(NMEA0183Msg);

    if (!SendNmea0183Message(message)) {
        qWarning() << "Failed sending NMEA0183 HDG message";
        return false;
    }

    return true;
}

bool Nmea0183Handler::RcvdSntHDG(const tNMEA0183Msg &NMEA0183Msg, const QString &talker)
{
    if (QString::fromLatin1(NMEA0183Msg.MessageCode()) != "HDG" || !Nmea0183Compat::validFields(NMEA0183Msg)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString SntLabel = "HDG";
    QStringList paramDetails;

    double Heading;
    double Deviation;
    double Variation;

    if (Nmea0183Compat::parseHdg(NMEA0183Msg, Heading, Deviation, Variation)) {
        paramDetails.append({"Heading", QString::number(Heading, 'g', 17)});
        paramDetails.append({"Deviation", QString::number(Deviation, 'g', 17)});
        paramDetails.append({"Variation", QString::number(Variation, 'g', 17)});
        emit RcvdSnt(SntLabel, paramDetails, talker);
    } else {
        qWarning() << "Failed parsing HDG sentence";
        return false;
    }

    return true;
}

bool Nmea0183Handler::SendSntHDT(const tNMEA0183Msg &NMEA0183Msg)
{
    QString message = constructMsgToSend(NMEA0183Msg);

    if (!SendNmea0183Message(message)) {
        qWarning() << "Failed sending NMEA0183 HDG message";
        return false;
    }

    return true;
}

bool Nmea0183Handler::RcvdSntHDT(const tNMEA0183Msg &NMEA0183Msg, const QString &talker)
{
    if (QString::fromLatin1(NMEA0183Msg.MessageCode()) != "HDT" || !Nmea0183Compat::validFields(NMEA0183Msg)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString SntLabel = "HDT";
    QStringList paramDetails;

    double Heading;

    if (NMEA0183ParseHDT_nc(NMEA0183Msg, Heading)) {
        paramDetails.append({"Heading", QString::number(Heading, 'g', 17)});
        emit RcvdSnt(SntLabel, paramDetails, talker);
    } else {
        qWarning() << "Failed parsing HDT sentence";
        return false;
    }

    return true;
}

bool Nmea0183Handler::SendSntGLL(const tNMEA0183Msg &NMEA0183Msg)
{
    QString message = constructMsgToSend(NMEA0183Msg);

    if (!SendNmea0183Message(message)) {
        qWarning() << "Failed sending NMEA0183 HDG message";
        return false;
    }

    return true;
}

bool Nmea0183Handler::RcvdSntGLL(const tNMEA0183Msg &NMEA0183Msg, const QString &talker)
{
    if (QString::fromLatin1(NMEA0183Msg.MessageCode()) != "GLL" || !Nmea0183Compat::validFields(NMEA0183Msg)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString SntLabel = "GLL";
    QStringList paramDetails;

    //double GPSTime, double Latitude, double Longitude
    tGLL GLL;

    if (NMEA0183ParseGLL_nc(NMEA0183Msg, GLL)) {
        paramDetails.append({"GPSTime", QString::number(GLL.GPSTime, 'g', 17)});
        paramDetails.append({"Latitude", QString::number(GLL.latitude, 'g', 15)});
        paramDetails.append({"Longitude", QString::number(GLL.longitude, 'g', 15)});
        paramDetails.append({"Status", QString(QChar(GLL.status))});
        emit RcvdSnt(SntLabel, paramDetails, talker);
    } else {
        qWarning() << "Failed parsing GLL sentence";
        return false;
    }

    return true;
}

bool Nmea0183Handler::SendSntVTG(const tNMEA0183Msg &NMEA0183Msg)
{
    QString message = constructMsgToSend(NMEA0183Msg);

    if (!SendNmea0183Message(message)) {
        qWarning() << "Failed sending NMEA0183 HDG message";
        return false;
    }

    return true;
}

bool Nmea0183Handler::RcvdSntVTG(const tNMEA0183Msg &NMEA0183Msg, const QString &talker)
{
    if (QString::fromLatin1(NMEA0183Msg.MessageCode()) != "VTG" || !Nmea0183Compat::validFields(NMEA0183Msg)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString SntLabel = "VTG";
    QStringList paramDetails;

    double TrueCOG;
    double MagneticCOG;
    double SOG;

    if (NMEA0183ParseVTG_nc(NMEA0183Msg, TrueCOG, MagneticCOG, SOG)) {
        paramDetails.append({"TrueCOG", QString::number(TrueCOG, 'g', 17)});
        paramDetails.append({"MagneticCOG", QString::number(MagneticCOG, 'g', 17)});
        paramDetails.append({"SOG", QString::number(SOG, 'g', 17)});
        emit RcvdSnt(SntLabel, paramDetails, talker);
    } else {
        qWarning() << "Failed parsing VTG sentence";
        return false;
    }

    return true;
}

bool Nmea0183Handler::SendSntZDA(const tNMEA0183Msg &NMEA0183Msg)
{
    QString message = constructMsgToSend(NMEA0183Msg);

    if (!SendNmea0183Message(message)) {
        qWarning() << "Failed sending NMEA0183 ZDA message";
        return false;
    }

    return true;
}

bool Nmea0183Handler::RcvdSntZDA(const tNMEA0183Msg &NMEA0183Msg, const QString &talker)
{
    if (QString::fromLatin1(NMEA0183Msg.MessageCode()) != "ZDA" || !Nmea0183Compat::validFields(NMEA0183Msg)) return false;
    // Serialize the data to emit to NmeaReceiver for processing
    QString SntLabel = "ZDA";
    QStringList paramDetails;

    //double &GPSTime, int &GPSDay, int &GPSMonth, int &GPSYear, int &LZD, int &LZMD
    double GPSTime;
    int GPSDay;
    int GPSMonth;
    int GPSYear;
    int LZD;
    int LZMD;

    if (NMEA0183ParseZDA(NMEA0183Msg, GPSTime, GPSDay, GPSMonth, GPSYear, LZD, LZMD)) {
        paramDetails.append({"GPSTime", QString::number(GPSTime, 'g', 17)});
        paramDetails.append({"GPSDay", QString::number(GPSDay)});
        paramDetails.append({"GPSMonth", QString::number(GPSMonth)});
        paramDetails.append({"GPSYear", QString::number(GPSYear)});
        paramDetails.append({"LZD", QString::number(LZD)});
        paramDetails.append({"LZMD", QString::number(LZMD)});
        emit RcvdSnt(SntLabel, paramDetails, talker);
    } else {
        qWarning() << "Failed parsing ZDA sentence";
        return false;
    }
    return true;
}

bool Nmea0183Handler::sendSnt(const QString &snt, const QStringList &paramList)
{
    if(Expansion::definition(snt)) {
        QMap<QString,QString> fields;for(const auto &parameter:paramList){const int colon=parameter.indexOf(':');if(colon<1||fields.contains(parameter.left(colon)))return false;fields[parameter.left(colon)]=parameter.mid(colon+1);}
        tNMEA0183Msg message;if(!Expansion::encodeSentence(snt,fields,message)) {
            if(QStringList{"GSA","APB","RMB","RTE","WPL"}.contains(snt))emit TransmitError("Cannot encode "+snt+": check field values and the NMEA0183 sentence length limit.");
            return false;
        }
        return SendNmea0183Message(constructMsgToSend(message));
    }
    // Validate the named input before accessing fields; blank numeric values mean NA.
    const QMap<QString, QStringList> schemas = {
        {"GGA", {"GPSTime","Latitude","Longitude","GPSQualityIndicator","SatelliteCount","HDOP","Altitude","GeoidalSeparation","DGPSAge","DGPSReferenceStationID"}},
        {"RMC", {"GPSTime","Status","Latitude","Longitude","TrueCOG","SOG","DaysSince1970","Variation","DateTime"}},
        {"HDG", {"Heading","Deviation","Variation"}}, {"HDT", {"Heading"}},
        {"GLL", {"GPSTime","Latitude","Longitude","Status"}}, {"VTG", {"TrueCOG","MagneticCOG","SOG"}},
        {"ZDA", {"GPSTime","GPSDay","GPSMonth","GPSYear","LZD","LZMD"}}
    };
    if (!schemas.contains(snt)) return false;
    QStringList remaining = schemas.value(snt);
    for (const QString &entry : paramList) {
        const int colon = entry.indexOf(':');
        if (colon < 1) return false;
        const QString name = entry.left(colon), value = entry.mid(colon + 1);
        if (!remaining.removeOne(name)) return false;
        if (name == "Status") {
            if (value != "A" && value != "V") return false;
        } else if (!value.isEmpty()) {
            bool ok; const double number = value.toDouble(&ok);
            if (!ok || !std::isfinite(number)) return false;
            const QStringList integers={"GPSQualityIndicator","SatelliteCount","DGPSReferenceStationID","DaysSince1970","DateTime","GPSDay","GPSMonth","GPSYear","LZD","LZMD"};
            if (integers.contains(name) && (std::floor(number)!=number || number < (name=="LZD"?-13:0) || number>4294967295.0)) return false;
            if (number != NMEA0183DoubleNA) {
                if ((name == "Latitude" && qAbs(number) > 90) || (name == "Longitude" && qAbs(number) > 180)
                    || (name == "GPSTime" && (number < 0 || number >= 86400)) || (name == "SOG" && number < 0)) return false;
            }
        }
    }
    if (!remaining.isEmpty()) return false;
    if (snt == "GGA") {
        tNMEA0183Msg NMEA0183Msg;
        double GPSTime = 0.0;
        double Latitude = 0.0;
        double Longitude = 0.0;
        int GPSQualityIndicator = 0;
        int SatelliteCount = 0;
        double HDOP = 0.0;
        double Altitude = 0.0;
        double GeoidalSeparation = 0.0;
        double DGPSAge = 0.0;
        int DGPSReferenceStationID = 0.0;
        const char *Src = "GP";

        // Check if the parameter list is valid
        if (paramList.size() != 10) {
            qWarning() << "Invalid GGA parameter list size:" << paramList.size();
            return false;
        }

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (int i = 0; i < paramList.size(); i++) {
            QStringList paramDetail = paramList[i].split(":");
            if (paramDetail[0] == "GPSTime") {
                GPSTime = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "Latitude") {
                Latitude = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "Longitude") {
                Longitude = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "GPSQualityIndicator") {
                GPSQualityIndicator = (paramDetail[1].isEmpty() ? -1 : paramDetail[1].toInt());
            } else if (paramDetail[0] == "SatelliteCount") {
                SatelliteCount = (paramDetail[1].isEmpty() ? -1 : paramDetail[1].toInt());
            } else if (paramDetail[0] == "HDOP") {
                HDOP = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "Altitude") {
                Altitude = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "GeoidalSeparation") {
                GeoidalSeparation = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "DGPSAge") {
                DGPSAge = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "DGPSReferenceStationID") {
                DGPSReferenceStationID = (paramDetail[1].isEmpty() ? -1 : paramDetail[1].toInt());
            }
        }

        bool bRet = NMEA0183SetGGA(NMEA0183Msg,
                                   GPSTime,
                                   Latitude,
                                   Longitude,
                                   GPSQualityIndicator,
                                   SatelliteCount,
                                   HDOP,
                                   Altitude,
                                   GeoidalSeparation,
                                   DGPSAge,
                                   DGPSReferenceStationID,
                                   Src);
        if (!bRet) {
            emit ShowStatusBarMessage("Failed setting NMEA0183 GGAmessage", 3000);
            qWarning() << "Failed setting NMEA0183 GGA message";
            return false;
        }

        return SendSntGGA(NMEA0183Msg);
    } else if (snt == "RMC") {
        tNMEA0183Msg NMEA0183Msg;
        double GPSTime = 0.0;
        char Status = 'V';
        double Latitude = 0.0;
        double Longitude = 0.0;
        double TrueCOG = 0.0;
        double SOG = 0.0;
        unsigned long DaysSince1970 = 0;
        double Variation = 0.0;
        time_t DateTime = 0;
        const char *Src = "GP";

        // Check if the parameter list is valid
        if (paramList.size() != 9) {
            qWarning() << "Invalid RMC parameter list size:" << paramList.size();
            return false;
        }

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (int i = 0; i < paramList.size(); i++) {
            QStringList paramDetail = paramList[i].split(":");
            if (paramDetail[0] == "GPSTime") {
                GPSTime = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "Status") {
                Status = paramDetail[1].at(0).toLatin1();
            } else if (paramDetail[0] == "Latitude") {
                Latitude = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "Longitude") {
                Longitude = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "TrueCOG") {
                TrueCOG = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "SOG") {
                SOG = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "DaysSince1970") {
                DaysSince1970 = (paramDetail[1].isEmpty() ? NMEA0183UInt32NA : paramDetail[1].toULong());
            } else if (paramDetail[0] == "Variation") {
                Variation = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "DateTime") {
                DateTime = paramDetail[1].toLong();
            }
        }

        // Status is the RMC A/V validity field, not FAA mode or navigation status.
        // These widgets expose neither optional field; emit the legacy 11-field form.
        bool bRet = NMEA0183SetRMC(NMEA0183Msg, GPSTime, Latitude, Longitude, TrueCOG, SOG, DaysSince1970, Variation, Src);
        if (bRet) {
            tNMEA0183Msg withStatus;
            bRet = withStatus.Init("RMC", Src);
            const char validity = Status == 'A' && GPSTime != NMEA0183DoubleNA
                && Latitude != NMEA0183DoubleNA && Longitude != NMEA0183DoubleNA ? 'A' : 'V';
            const QByteArray utcDate = DaysSince1970 == NMEA0183UInt32NA ? QByteArray()
                : QDate(1970, 1, 1).addDays(DaysSince1970).toString("ddMMyy").toLatin1();
            for (int i = 0; bRet && i < NMEA0183Msg.FieldCount(); ++i) {
                if (i == 1) bRet = withStatus.AddStrField(validity);
                else if (i == 8) bRet = withStatus.AddStrField(utcDate.constData());
                else bRet = withStatus.AddStrField(NMEA0183Msg.Field(i));
            }
            NMEA0183Msg = withStatus;
        }
        Q_UNUSED(DateTime); // Derived receive metadata; days/time define transmitted UTC.
        if (!bRet) {
            emit ShowStatusBarMessage("Failed setting NMEA0183 RMC message", 3000);
            qWarning() << "Failed setting NMEA0183 RMC message";
            return false;
        }

        return SendSntRMC(NMEA0183Msg);
    } else if (snt == "HDG") {
        tNMEA0183Msg NMEA0183Msg;
        double Heading = 0.0;
        double Deviation = 0.0;
        double Variation = 0.0;
        const char *Src = "HC";

        // Check if the parameter list is valid
        if (paramList.size() != 3) {
            qWarning() << "Invalid HDG parameter list size:" << paramList.size();
            return false;
        }

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (int i = 0; i < paramList.size(); i++) {
            QStringList paramDetail = paramList[i].split(":");
            if (paramDetail[0] == "Heading") {
                Heading = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "Deviation") {
                Deviation = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "Variation") {
                Variation = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            }
        }

        bool bRet = NMEA0183SetHDG(NMEA0183Msg, Heading, Deviation, Variation, Src);
        if (!bRet) {
            emit ShowStatusBarMessage("Failed setting NMEA0183 HDG message", 3000);
            qWarning() << "Failed setting NMEA0183 HDG message";
            return false;
        }

        return SendSntHDG(NMEA0183Msg);
    } else if (snt == "HDT") {
        tNMEA0183Msg NMEA0183Msg;
        double Heading = 0.0;

        // Check if the parameter list is valid
        if (paramList.size() != 1) {
            qWarning() << "Invalid HDT parameter list size:" << paramList.size();
            return false;
        }

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (int i = 0; i < paramList.size(); i++) {
            QStringList paramDetail = paramList[i].split(":");
            if (paramDetail[0] == "Heading") {
                Heading = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            }
        }

        bool bRet = NMEA0183SetHDT(NMEA0183Msg, Heading);
        if (!bRet) {
            emit ShowStatusBarMessage("Failed setting NMEA0183 HDT message", 3000);
            qWarning() << "Failed setting NMEA0183 HDT message";
            return false;
        }

        return SendSntHDT(NMEA0183Msg);
    } else if (snt == "GLL") {
        tNMEA0183Msg NMEA0183Msg;
        char Status = 'A';
        double GPSTime = 0.0;
        double Latitude = 0.0;
        double Longitude = 0.0;
        const char *Src = "GP";

        // Check if the parameter list is valid
        if (paramList.size() != 4) {
            qWarning() << "Invalid GLL parameter list size:" << paramList.size();
            return false;
        }

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (int i = 0; i < paramList.size(); i++) {
            QStringList paramDetail = paramList[i].split(":");
            if (paramDetail[0] == "Status") {
                Status = paramDetail[1].at(0).toLatin1();
            } else if (paramDetail[0] == "GPSTime") {
                GPSTime = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "Latitude") {
                Latitude = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "Longitude") {
                Longitude = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            }
        }

        bool bRet = NMEA0183SetGLL(NMEA0183Msg, GPSTime, Latitude, Longitude, Src);
        if (bRet) {
            tNMEA0183Msg withStatus;
            bRet = withStatus.Init("GLL", Src);
            const char validity = Status == 'A' && GPSTime != NMEA0183DoubleNA
                && Latitude != NMEA0183DoubleNA && Longitude != NMEA0183DoubleNA ? 'A' : 'V';
            for (int i = 0; bRet && i < NMEA0183Msg.FieldCount(); ++i)
                bRet = i == 5 ? withStatus.AddStrField(validity) : withStatus.AddStrField(NMEA0183Msg.Field(i));
            NMEA0183Msg = withStatus;
        }
        if (!bRet) {
            emit ShowStatusBarMessage("Failed setting NMEA0183 GLL message", 3000);
            qWarning() << "Failed setting NMEA0183 GLL message";
            return false;
        }

        return SendSntGLL(NMEA0183Msg);
    } else if (snt == "VTG") {
        tNMEA0183Msg NMEA0183Msg;
        double TrueCOG = 0.0;
        double MagneticCOG = 0.0;
        double SOG = 0.0;
        const char *Src = "GP";

        // Check if the parameter list is valid
        if (paramList.size() != 3) {
            qWarning() << "Invalid VTG parameter list size:" << paramList.size();
            return false;
        }

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (int i = 0; i < paramList.size(); i++) {
            QStringList paramDetail = paramList[i].split(":");
            if (paramDetail[0] == "TrueCOG") {
                TrueCOG = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "MagneticCOG") {
                MagneticCOG = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "SOG") {
                SOG = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            }
        }

        bool bRet = NMEA0183SetVTG(NMEA0183Msg, TrueCOG, MagneticCOG, SOG, Src);
        if (!bRet) {
            emit ShowStatusBarMessage("Failed setting NMEA0183 VTG message", 3000);
            qWarning() << "Failed setting NMEA0183 VTG message";
            return false;
        }

        return SendSntVTG(NMEA0183Msg);
    } else if (snt == "ZDA") {
        tNMEA0183Msg NMEA0183Msg;
        double GPSTime = 0.0;
        int GPSDay = 0;
        int GPSMonth = 0;
        int GPSYear = 0;
        int LZD = 0;
        int LZMD = 0;
        const char *Src = "GP";

        // Check if the parameter list is valid
        if (paramList.size() != 6) {
            qWarning() << "Invalid ZDA parameter list size:" << paramList.size();
            return false;
        }

        // Extract the parameters from the list of name:value (paramList << paramDetail[0] + ":" + paramDetail[1])
        for (int i = 0; i < paramList.size(); i++) {
            QStringList paramDetail = paramList[i].split(":");
            if (paramDetail[0] == "GPSTime") {
                GPSTime = (paramDetail[1].isEmpty() ? NMEA0183DoubleNA : paramDetail[1].toDouble());
            } else if (paramDetail[0] == "GPSDay") {
                GPSDay = (paramDetail[1].isEmpty() ? -1 : paramDetail[1].toInt());
            } else if (paramDetail[0] == "GPSMonth") {
                GPSMonth = (paramDetail[1].isEmpty() ? -1 : paramDetail[1].toInt());
            } else if (paramDetail[0] == "GPSYear") {
                GPSYear = (paramDetail[1].isEmpty() ? -1 : paramDetail[1].toInt());
            } else if (paramDetail[0] == "LZD") {
                LZD = (paramDetail[1].isEmpty() ? -1 : paramDetail[1].toInt());
            } else if (paramDetail[0] == "LZMD") {
                LZMD = (paramDetail[1].isEmpty() ? -1 : paramDetail[1].toInt());
            }
        }

        bool bRet = NMEA0183SetZDA(NMEA0183Msg, GPSTime, GPSDay, GPSMonth, GPSYear, LZD, LZMD, Src);
        if (!bRet) {
            emit ShowStatusBarMessage("Failed setting NMEA0183 ZDA message", 3000);
            qWarning() << "Failed setting NMEA0183 ZDA message";
            return false;
        }

        return SendSntZDA(NMEA0183Msg);
    } else {
        qWarning() << "Unsupported NMEA0183 sentence type:" << snt;
        return false;
    }
}

void Nmea0183Handler::setMode(Mode mode)
{
    if (this->mode != mode) buffer.clear();
    this->mode = mode;
    qDebug() << "NMEA0183 Mode:" << mode;
}

Nmea0183Handler *Nmea0183Handler::getInstance()
{
    return instance;
}

void Nmea0183Handler::onSerialDataAvailable()
{
    const QByteArray data = spNmea0183->readAll(); // Drain even when not viewing.
    observeTrafficBytes(data);
    // The existing Viewer framing path is observed, never invoked by recording controls.
    if (mode == Viewer) processIncomingData(data);
}

void Nmea0183Handler::processIncomingData(const QByteArray &data)
{
    // Bound retained state to one 82-byte frame. A fresh '$' resynchronizes after
    // garbage or a truncated sentence; every complete frame is dispatched once.
    for (unsigned char byte : data) {
        if (byte == '$') { buffer = "$"; continue; }
        if (buffer.isEmpty()) continue;
        if (byte == '\n') {
            if (buffer.endsWith('\r')) {
                const QString frame = buffer.chopped(1);
                buffer.clear();
                if (validateChecksum(frame)) parseNMEAMessage(frame);
            } else buffer.clear();
            continue;
        }
        if ((byte < 32 && byte != '\r') || byte > 126 || buffer.size() >= 81) {
            buffer.clear(); continue;
        }
        buffer += QChar(byte);
    }
}

bool Nmea0183Handler::validateChecksum(const QString &message)
{
    static const QRegularExpression frame("^\\$[A-Z0-9]{5},[ -~]*\\*[0-9A-Fa-f]{2}$");
    if (message.size() > 80 || !frame.match(message).hasMatch()) return false;
    const int star = message.indexOf('*');
    if (star != message.size() - 3 || message.indexOf('$', 1) != -1) return false;
    bool ok;
    const int expected = message.mid(star + 1).toInt(&ok, 16);
    unsigned char checksum = 0;
    for (int i = 1; i < star; ++i) checksum ^= message[i].toLatin1();
    return ok && checksum == expected;
}

void Nmea0183Handler::parseNMEAMessage(const QString &message)
{
    tNMEA0183Msg NMEA0183Msg = tNMEA0183Msg();

    if (message.startsWith("$")) {
        QStringList parts = message.split(",");
        QString type = parts[0].mid(1);

#ifdef QT_DEBUG
        if (!types.contains(type) && types.size() < 64) {
            types.append(type);
            qDebug() << "On NMEA0183 bus:" << type;
        }
#endif

        QPair<QString, QString> result = extractTalkerAndSentenceType(type);
        QString talker = result.first;
        QString sentenceType = result.second;

        NMEA0183Msg.SourceID = 0;
        if(!NMEA0183Msg.SetMessage(message.toLatin1().constData()))return;
        if(Expansion::definition(sentenceType)) {
            QStringList fields;if(Expansion::decodeSentence(NMEA0183Msg,fields))emit RcvdSnt(sentenceType,fields,talker);
            return;
        }
        if(!Nmea0183Compat::validFields(NMEA0183Msg))return;

        if (sentenceType == "GGA") { // Global Positioning System Fix Data
            RcvdSntGGA(NMEA0183Msg, talker);
        } else if (sentenceType == "RMC") { // Recommended Minimum Specific GNSS Data
            RcvdSntRMC(NMEA0183Msg, talker);
        } else if (sentenceType == "HDG") { // Heading - Deviation & Variation
            RcvdSntHDG(NMEA0183Msg, talker);
        } else if (sentenceType == "HDT") { // Heading - True
            RcvdSntHDT(NMEA0183Msg, talker);
        } else if (sentenceType == "GLL") { // Geographic Position - Latitude/Longitude
            RcvdSntGLL(NMEA0183Msg, talker);
        } else if (sentenceType == "VTG") { // Course Over Ground and Ground Speed
            RcvdSntVTG(NMEA0183Msg, talker);
        } else if (sentenceType == "ZDA") { // Time & Date - UTC, Day, Month, Year and Local Time Zone
            RcvdSntZDA(NMEA0183Msg, talker);
        } else {
            qWarning() << "Unsupported parse NMEA0183 sentence type:" << sentenceType;
        }
    } else {
        qWarning() << "Invalid NMEA0183 message:" << message;
    }
}

QPair<QString, QString> Nmea0183Handler::extractTalkerAndSentenceType(const QString &nmeaSentence)
{
    if (nmeaSentence.length() >= 5) {
        QString talker = nmeaSentence.mid(0, 2);
        QString sentenceType = nmeaSentence.mid(2);
        return qMakePair(talker, sentenceType);
    }
    return qMakePair(QString(), QString());
}

QString Nmea0183Handler::constructMsgToSend(const tNMEA0183Msg &NMEA0183Msg)
{
    if(Expansion::definition(QString::fromLatin1(NMEA0183Msg.MessageCode()))) {
        QStringList fields;if(!Expansion::decodeSentence(NMEA0183Msg,fields))return {};
    } else if (!Nmea0183Compat::validFields(NMEA0183Msg)) return {};
    char wire[128];
    if (!NMEA0183Msg.GetMessage(wire, sizeof(wire))) return {};
    const QString message = QString::fromLatin1(wire);
    if (!validateChecksum(message)) return {};
    return message + "\r\n";
}

void Nmea0183Handler::observeTrafficBytes(const QByteArray &data)
{
    const auto generation=ApplicationLog::trafficGeneration();
    if(!ApplicationLog::recording()){trafficBuffer.clear();trafficEpoch=generation;return;}
    if(trafficEpoch!=generation){trafficBuffer.clear();trafficEpoch=generation;}
    for(const auto byte:data){
        if(byte=='$'||byte=='!'){trafficBuffer=QByteArray(1,byte);continue;}
        if(trafficBuffer.isEmpty())continue;
        if(byte=='\n'){
            if(trafficBuffer.endsWith('\r')){
                const auto frame=trafficBuffer.chopped(1);const int star=frame.indexOf('*');bool ok=false;const auto expected=frame.mid(star+1).toUInt(&ok,16);unsigned char sum=0;
                if(star>0)for(int i=1;i<star;++i)sum^=static_cast<unsigned char>(frame[i]);
                const bool valid=star>0 && frame.size()-star==3 && ok && expected==sum;
                ApplicationLog::sentence("RX","NMEA0183 serial",trafficBuffer+'\n',valid?"checksum-valid":"checksum-invalid");
            }
            trafficBuffer.clear();continue;
        }
        if((static_cast<unsigned char>(byte)<32&&byte!='\r') || static_cast<unsigned char>(byte)>126 || trafficBuffer.size()>=81){trafficBuffer.clear();continue;}
        trafficBuffer+=byte;
    }
}
