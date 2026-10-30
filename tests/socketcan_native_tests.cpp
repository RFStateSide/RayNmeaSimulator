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

#include <QCoreApplication>
#include <QDebug>
#include "socketcansession.h"
#include "chartplotterrouteclient.h"
#include "routeservicepolicy.h"
#include "route_service_fixture.h"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <cstdarg>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <deque>
static unsigned checks=0,acquired=0,released=0,attempts=0,accepted=0,simulation=0,management=0,callbacks=0;
static unsigned claims=0,heartbeats=0,products=0,lastClaimSource=0;
static std::set<int> owned;
static std::deque<can_frame> incoming;
static QVector<RouteFixture::Frame> outgoing;
static uint64_t tick=100;
static int failAt=0,budget=-1;
static bool controlled=false;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);abort();}}while(0)
extern "C" int __real_close(int);
extern "C" int __real_fcntl(int,int,...);
extern "C" int __real_clock_gettime(clockid_t, timespec*);
extern "C" int __wrap_clock_gettime(clockid_t id,timespec* t){if(!controlled||id!=CLOCK_MONOTONIC)return __real_clock_gettime(id,t);t->tv_sec=tick/1000;t->tv_nsec=tick%1000*1000000;return 0;}
extern "C" int __wrap_socket(int d,int t,int p){CHECK(d==PF_CAN&&t==SOCK_RAW&&p==CAN_RAW);if(failAt==1){errno=EMFILE;return -1;}int fd=::open("/dev/null",O_RDWR|O_CLOEXEC);CHECK(fd>=0);CHECK(owned.insert(fd).second);++acquired;return fd;}
extern "C" int __wrap_close(int fd){CHECK(owned.erase(fd)==1);++released;return __real_close(fd);}
extern "C" int __wrap_ioctl(int fd,unsigned long req,...){CHECK(owned.count(fd));CHECK(req==SIOCGIFINDEX);if(failAt==2){errno=ENODEV;return -1;}va_list a;va_start(a,req);auto* r=va_arg(a,ifreq*);va_end(a);r->ifr_ifindex=7;return 0;}
extern "C" int __wrap_bind(int fd,const sockaddr*,socklen_t){CHECK(owned.count(fd));if(failAt==3){errno=EINVAL;return -1;}return 0;}
extern "C" int __wrap_fcntl(int fd,int cmd,...){CHECK(owned.count(fd));if((cmd==F_GETFL&&failAt==4)||(cmd==F_SETFL&&failAt==5)){errno=EIO;return -1;}if(cmd==F_GETFL)return __real_fcntl(fd,cmd);CHECK(cmd==F_SETFL);va_list a;va_start(a,cmd);int flags=va_arg(a,int);va_end(a);return __real_fcntl(fd,cmd,flags);}
extern "C" int __wrap_select(int n,fd_set* r,fd_set*,fd_set*,timeval*){CHECK(owned.count(n-1));if(incoming.empty()){FD_ZERO(r);return 0;}return 1;}
extern "C" ssize_t __wrap_read(int fd,void* out,size_t n){CHECK(owned.count(fd));CHECK(n==sizeof(can_frame));CHECK(!incoming.empty());memcpy(out,&incoming.front(),n);incoming.pop_front();return n;}
extern "C" ssize_t __wrap___read_chk(int fd,void* out,size_t n,size_t cap){CHECK(cap>=n);return __wrap_read(fd,out,n);}
extern "C" ssize_t __wrap_write(int fd,const void* in,size_t n){CHECK(owned.count(fd));CHECK(n==sizeof(can_frame));++attempts;if(budget==0){errno=EAGAIN;return -1;}if(budget>0)--budget;auto* f=static_cast<const can_frame*>(in);CHECK(f->can_dlc<=8);++accepted;outgoing.append({f->can_id&CAN_EFF_MASK,QByteArray(reinterpret_cast<const char*>(f->data),f->can_dlc)});unsigned pgn=(f->can_id>>8)&0x1ffff;if(((f->can_id>>16)&255)<240)pgn&=0x1ff00;if(pgn==60928){++claims;lastClaimSource=f->can_id&255;}if(pgn==126993)++heartbeats;if(pgn==126996)++products;if(pgn==127250||pgn==129029)++simulation;else ++management;return n;}
int main(int argc,char** argv){QCoreApplication app(argc,argv);QString scenario=argc>1?argv[1]:"protocol";controlled=true;
 SocketCanSession session(nullptr,nullptr,[](const QString &name){return SocketCanSession::validateLink(name,R"([{"ifname":"can0","link_type":"can","flags":["UP"]}])");});
 QObject::connect(&session,&SocketCanSession::received,[](const tN2kMsg&){++callbacks;});
 CHECK(session.start({"can0",42}));session.service();CHECK(acquired==0);CHECK(session.state()==SocketCanSession::Initializing);
 if(scenario=="cancel"){session.shutdown();CHECK(released==0);CHECK(!session.start({"can0",42}));}
 else {
 if(scenario.startsWith("failure"))failAt=scenario.right(1).toInt();
 tick=101;session.service();
 if(failAt){CHECK(session.state()==SocketCanSession::Failed);CHECK(owned.empty());failAt=0;tick=1102;session.service();}
 CHECK(session.state()==SocketCanSession::Initializing);
 if(scenario=="cancel-pending"){session.shutdown();CHECK(owned.empty());}
 else {
 tick+=201;session.service();CHECK(session.state()==SocketCanSession::Ready);CHECK(claims==1);tick+=300;session.service();
 for(int i=0;i<100;++i){session.setViewer(i%2);session.service();}CHECK(!session.restartRequired());
 if(scenario=="route"){
 ChartplotterRouteClient client;RouteFixture::RequestDecoder decoder;int consumed=0,rawAt=outgoing.size();quint8 sequence=0;
 session.setViewer(false);session.authorizeService(true);
 client.ready=[&]{return session.routeServiceReady();};client.localAddress=[&]{return session.sourceAddress();};
 client.send=[&](const tN2kMsg &m){CHECK(RouteServicePolicy::allowed(m));return session.sendService(m);};
 QObject::connect(&session,&SocketCanSession::received,&client,&ChartplotterRouteClient::ingest);
 auto progress=[&](const std::function<bool()> &done){
  for(int step=0;step<20000&&!done();step+=25){
   tick+=25;session.service();client.advance(step);
   while(rawAt<outgoing.size()){const auto f=outgoing[rawAt++];decoder.receive(f.id,f.data);}
   while(consumed<decoder.messages.size()){
    const auto request=decoder.messages[consumed++];CHECK(RouteServicePolicy::allowed(request));
    auto reply=RouteFixture::response(request);if(!reply.PGN)continue;
    for(const auto &part:RouteFixture::frames(reply,sequence++&7)){
     can_frame raw={};raw.can_id=CAN_EFF_FLAG|part.id;raw.can_dlc=part.data.size();memcpy(raw.data,part.data.constData(),part.data.size());incoming.push_back(raw);
    }
   }
  }
  CHECK(done());
 };
 client.discover();progress([&]{return !client.devices().isEmpty()&&!client.devices().first().model.isEmpty();});
 CHECK(client.devices().first().name==RouteFixture::name());client.advance(7000);
 client.browse(client.devices().first());progress([&]{return !client.busy()&&client.entries().size()==1;});
 client.retrieve(client.entries().first());progress([&]{return client.state()==ChartplotterRouteClient::Complete;});
 CHECK(client.candidate().complete);CHECK(client.candidate().waypoints.size()==3);
 CHECK(client.candidate().waypoints[2].coordinate==QGeoCoordinate(10.02,20.02));CHECK(simulation==0);
 session.stopSending();CHECK(session.restartRequired());client.cancel();
 }else if(scenario=="protocol"){
 can_frame f={};f.can_id=CAN_EFF_FLAG|0x09f80164;f.can_dlc=8;incoming.push_back(f);session.service();CHECK(callbacks==1);
 // ISO request for product information, addressed to source 43.
 f={};f.can_id=CAN_EFF_FLAG|0x18ea2b64;f.can_dlc=3;f.data[0]=0x14;f.data[1]=0xf0;f.data[2]=1;incoming.push_back(f);auto before=management;session.service();tick+=1000;session.service();CHECK(management>before);CHECK(products>0);
 // A lower NAME on our address forces the real core to claim another address.
 f={};f.can_id=CAN_EFF_FLAG|0x18eeff2b;f.can_dlc=8;incoming.push_back(f);session.service();tick+=300;session.service();CHECK(claims>=2);CHECK(lastClaimSource!=43);
 tick+=60000;before=management;session.service();CHECK(management>before);CHECK(heartbeats>0);CHECK(simulation==0);
 session.shutdown();
 } else if(scenario=="config") {CHECK(!session.start({"can1",43}));CHECK(session.restartRequired());}
 else {
 session.setViewer(false);tN2kMsg m;m.SetPGN(scenario=="fast"?129029:127250);for(int i=0;i<(scenario=="fast"?20:8);++i)m.AddByte(0);
 budget=scenario=="fast"?1:0;session.send(m);auto sent=simulation;
 if(scenario=="stop")session.stopSending();else session.setViewer(true);
 budget=-1;CHECK(session.restartRequired());CHECK(simulation==sent);
 }
 }
 }
 auto writes=attempts;for(int i=0;i<100;++i){session.service();tN2kMsg m;CHECK(!session.send(m));CHECK(!session.start({"can0",42}));}
 CHECK(attempts==writes);CHECK(owned.empty());CHECK(acquired==released);CHECK(!session.serviceActive());controlled=false;
 printf("%s: checks=%u acquired=%u released=%u accepted=%u simulation=%u management=%u postclose=0\n",qPrintable(scenario),checks,acquired,released,accepted,simulation,management);
}
