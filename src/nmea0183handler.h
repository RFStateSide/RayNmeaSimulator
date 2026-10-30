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

#ifndef NMEA0183HANDLER_H
#define NMEA0183HANDLER_H

#include <QFile>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QtSerialPort>
#include "datastructs.h"
#include <NMEA0183.h>
#include <NMEA0183Msg.h>
#include <NMEA2000.h>

class Nmea0183Handler : public QObject
{
    Q_OBJECT
public:
    // An injected serial port is owned by this handler (also used by deterministic tests).
    explicit Nmea0183Handler(QObject *parent = nullptr, QSerialPort *serialPort = nullptr);
    ~Nmea0183Handler();

    bool Initialize(const QString &portName, QSerialPort::BaudRate baudRate = QSerialPort::Baud4800);
    bool OpenDevice();
    bool CloseDevice();
    bool IsOpen();

    bool SendNmea0183Message(const QString &message);
    bool SendTestGGA();

    bool SendSntGGA(const tNMEA0183Msg &NMEA0183Msg);
    bool RcvdSntGGA(const tNMEA0183Msg &NMEA0183Msg, const QString &talker = "GP");

    bool SendSntRMC(const tNMEA0183Msg &NMEA0183Msg);
    bool RcvdSntRMC(const tNMEA0183Msg &NMEA0183Msg, const QString &talker = "GP");

    bool SendSntHDG(const tNMEA0183Msg &NMEA0183Msg);
    bool RcvdSntHDG(const tNMEA0183Msg &NMEA0183Msg, const QString &talker = "HE");

    bool SendSntHDT(const tNMEA0183Msg &NMEA0183Msg);
    bool RcvdSntHDT(const tNMEA0183Msg &NMEA0183Msg, const QString &talker = "HE");

    bool SendSntGLL(const tNMEA0183Msg &NMEA0183Msg);
    bool RcvdSntGLL(const tNMEA0183Msg &NMEA0183Msg, const QString &talker = "GP");

    bool SendSntVTG(const tNMEA0183Msg &NMEA0183Msg);
    bool RcvdSntVTG(const tNMEA0183Msg &NMEA0183Msg, const QString &talker = "GP");

    bool SendSntZDA(const tNMEA0183Msg &NMEA0183Msg);
    bool RcvdSntZDA(const tNMEA0183Msg &NMEA0183Msg, const QString &talker = "GP");

    bool sendSnt(const QString &snt, const QStringList &paramList);

    void setMode(Mode mode);
    static Nmea0183Handler *getInstance();

signals:
    void InterfaceStateChanged();
    void TransmitError(const QString &message);
    void ShowStatusBarMessage(QString message, int timeout);
    void RcvdSnt(const QString &SntLabel, const QStringList &paramDetails, const QString &source = "GP");

private slots:
    void onSerialDataAvailable();

private:
    static Nmea0183Handler *instance;
    friend class HandlerTests;
    QSerialPort *spNmea0183;
    void processIncomingData(const QByteArray &data);

    bool validateChecksum(const QString &message);
    void parseNMEAMessage(const QString &message);
    QPair<QString, QString> extractTalkerAndSentenceType(const QString &nmeaSentence);
    QString constructMsgToSend(const tNMEA0183Msg &NMEA0183Msg);

    bool portInitilized = false;
    QString buffer;
    QByteArray trafficBuffer;
    quint64 trafficEpoch=0;
    void observeTrafficBytes(const QByteArray &);

    QList<QString> types;
    Mode mode = Sender;
};

#endif // NMEA0183HANDLER_H
