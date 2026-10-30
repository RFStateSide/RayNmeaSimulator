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

#include "navigationstatistics.h"
#include "applicationlog.h"
#include "socketcansession.h"
#include "routeservicepolicy.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QThread>
#include <QStandardPaths>
#include <cerrno>
#ifdef Q_OS_LINUX
#include "NMEA2000_SocketCAN.h"
namespace {
class LinuxBackend final : public SocketCanBackend, public tNMEA2000_SocketCAN {
public:
    static LinuxBackend *instance;
    std::function<void(const tN2kMsg&)> callback;
    int error=0;
    bool CANOpen() override {
        bool ok=tNMEA2000_SocketCAN::CANOpen(); error=ok?0:errno; return ok;
    }
    void configure(const SocketCanConfig &c, std::function<void(const tN2kMsg&)> cb) override {
        callback=std::move(cb); instance=this;
        QByteArray name=c.interfaceName.toLatin1(); SetCANport(name.data());
        SetN2kCANSendFrameBufSize(50); // Preserve the previous application queue capacity.
        SetProductInformation("RayNmeaSimulator",105,APP_NAME,APP_VERSION,APP_NAME,0,2101,0);
        SetDeviceInformation(c.uniqueNumber,210,80,2046);
        ExtendTransmitMessages(NavigationStatistics::QualifiedPgns);
        ExtendReceiveMessages(NavigationStatistics::QualifiedPgns);
        SetMode(N2km_ListenAndNode,43);
        // This API sets the default retained across staged Open initialization.
        SetHeartbeatInterval(30000);
        SetMsgHandler([](const tN2kMsg &m){if(instance && instance->callback)instance->callback(m);});
    }
    bool CANGetFrame(unsigned long &id,unsigned char &length,unsigned char *data) override {
        const bool received=tNMEA2000_SocketCAN::CANGetFrame(id,length,data);
        if(received)ApplicationLog::socketCanFrame(quint32(id),length,data);
        return received;
    }
    void service() override { ParseMessages(); }
    bool ready() const override { return IsOpen(); }
    bool serviceReady() const override { return IsOpen() && !const_cast<LinuxBackend*>(this)->IsAddressClaimStarted(0) && GetN2kSource()<=251; }
    int sourceAddress() const override { return GetN2kSource(); }
    int openingError() const override { return error; }
    bool send(const tN2kMsg &m) override { return SendMsg(m); }
    bool close() override { SetMsgHandler(nullptr); SetOnOpen(nullptr); callback={}; return Close(); }
};
LinuxBackend *LinuxBackend::instance=nullptr;
// One owner for the process, never deleted through the nonvirtual core base.
LinuxBackend *processBackend=nullptr;
}
#endif
SocketCanSession::SocketCanSession(QObject *parent,SocketCanBackend *b,LinkValidator v):QObject(parent),backend(b),validator(std::move(v)) {
    timer.setTimerType(Qt::PreciseTimer); timer.setInterval(1);
    connect(&timer,&QTimer::timeout,this,&SocketCanSession::service);
}
SocketCanSession::~SocketCanSession(){shutdown();}
void SocketCanSession::report(State s,const QString &text){
    if(current==s && message==text)return;
    current=s;message=text;ApplicationLog::event("raynmea.transport.socketcan",text);emit statusChanged(message);
}
bool SocketCanSession::validName(const QString &n){
    return n.size()>0 && n.toLatin1().size()<16 && QRegularExpression("^[A-Za-z0-9_-]+$").match(n).hasMatch();
}
QString SocketCanSession::validateLink(const QString &name,const QByteArray &json){
    if(!validName(name))return "Invalid CAN interface name";
    auto doc=QJsonDocument::fromJson(json);
    if(!doc.isArray() || doc.array().size()!=1)return "CAN interface missing or unreadable";
    auto link=doc.array().first().toObject();
    if(link.value("ifname").toString()!=name)return "CAN interface identity mismatch";
    if(link.value("link_type").toString()!="can")return "Selected interface is not CAN";
    if(!link.value("flags").toArray().contains("UP"))return "CAN interface is administratively down";
    auto state=link.value("linkinfo").toObject().value("info_data").toObject().value("state").toString();
    if(state=="BUS-OFF" || state=="STOPPED")return "CAN interface is not operational: "+state;
    return {};
}
SocketCanConfig SocketCanSession::loadConfig(QSettings &s){
    SocketCanConfig c;c.interfaceName=s.value("SocketCanInterface","can0").toString();
    bool ok=false;auto id=s.value("SocketCanUniqueNumber").toUInt(&ok);
    if(!s.contains("SocketCanUniqueNumber")){
        id=QRandomGenerator::global()->bounded(1u,2097152u);s.setValue("SocketCanUniqueNumber",id);s.sync();ok=s.status()==QSettings::NoError;
    }
    c.uniqueNumber=ok && id>0 && id<=2097151 ? id:0;return c;
}
bool SocketCanSession::start(const SocketCanConfig &c){
    Q_ASSERT(thread()==QThread::currentThread());
    if(terminal){emit statusChanged(message);return false;}
    if(configured){
        if(!(config==c)){shutdown("SocketCAN configuration changed. Restart required to apply interface or identity.");return false;}
        return current==Ready || current==Initializing || current==Failed;
    }
    if(!validName(c.interfaceName)||c.uniqueNumber==0||c.uniqueNumber>2097151){report(Failed,"Invalid SocketCAN interface or 21-bit device identity");return false;}
    if(!backend){
#ifdef Q_OS_LINUX
        QString error;
        if(validator)error=validator(c.interfaceName);
        else {
            const QString ip=QStandardPaths::findExecutable("ip");
            if(ip.isEmpty()){report(Failed,"SocketCAN requires the Linux ip utility (iproute2) on PATH");return false;}
            QProcess probe;probe.start(ip,{"-details","-json","link","show","dev",c.interfaceName});
            if(!probe.waitForFinished(1500)){probe.kill();probe.waitForFinished();report(Failed,"Cannot inspect CAN interface: "+probe.errorString());return false;}
            error=validateLink(c.interfaceName,probe.readAllStandardOutput());
            if(probe.exitCode()!=0 && error.isEmpty())error="Cannot inspect CAN interface";
        }
        if(!error.isEmpty()){report(Failed,error);return false;}
        if(processBackend){report(Failed,"SocketCAN session already allocated. Restart required.");terminal=true;return false;}
        processBackend=new LinuxBackend;backend=processBackend;
#else
        report(Failed,"SocketCAN is available only on Linux");return false;
#endif
    }
    config=c;configured=true;
    backend->configure(c,[this](const tN2kMsg &m){if(!terminal && (viewer || serviceAuthorized))emit received(m);});
    elapsed.start();last=0;report(Initializing,"SocketCAN initializing; protocol traffic remains active in Viewer");if(!terminal)timer.start();return !terminal;
}
void SocketCanSession::service(){
    Q_ASSERT(thread()==QThread::currentThread());
    if(terminal||!configured||servicing)return;
    QScopedValueRollback<bool> guard(servicing,true);
    auto now=elapsed.nsecsElapsed();if(last)maxGap=qMax(maxGap,now-last);last=now;++services;
    backend->service();
    if(terminal){finishClose();return;}
    if(backend->ready())report(Ready,"SocketCAN active; address claiming and protocol management enabled");
    else if(backend->openingError())report(Failed,QString("SocketCAN open failed (%1); retrying same configuration").arg(backend->openingError()));
    else report(Initializing,"SocketCAN initializing");
    // Status listeners can request shutdown while this service call is active.
    if(terminal)finishClose();
}
void SocketCanSession::setViewer(bool v){if(v && !viewer && attempted)shutdown("SocketCAN stopped. Restart required to receive in Viewer mode.");viewer=v;}
bool SocketCanSession::send(const tN2kMsg &m){
    if(terminal||viewer||serviceAuthorized||!configured)return false;
    attempted=true; // Even unsuccessful submissions can leave partial/queued traffic.
    if(current!=Ready)return false;
    return backend->send(m);
}
bool SocketCanSession::sendService(const tN2kMsg &m){if(!serviceAuthorized||!routeServiceReady()||!RouteServicePolicy::allowed(m))return false;serviceAttempted=true;return backend->send(m);}
void SocketCanSession::stopSending(){serviceAuthorized=false;if(attempted||serviceAttempted)shutdown("SocketCAN stopped. Restart required to send or receive again.");}
void SocketCanSession::shutdown(const QString &reason){
    if(terminal)return;
    if(!configured){timer.stop();return;}
    serviceAuthorized=false;terminal=true;timer.stop();report(ShuttingDown,"SocketCAN stopping");
    closeReason=reason;
    if(!servicing)finishClose();
}
void SocketCanSession::finishClose(){
    const bool closed=backend->close();
    qInfo("SocketCAN service calls=%llu maximum gap ns=%lld",static_cast<unsigned long long>(services),static_cast<long long>(maxGap));
    report(Closed,closeReason+(closed?QString():QString(" Close reported an error; restart required.")));
}
