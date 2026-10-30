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

#pragma once
#include <QByteArray>
#include <QString>
#include "N2kMessages.h"
namespace RouteFixture {
inline void u(QByteArray &b, quint32 v, int n) {
    for (int i = 0; i < n; ++i)
        b += char(v >> (8 * i));
}
inline void str(QByteArray &b, const QString &s) {
    auto v = s.toLatin1();
    b += char(v.size() + 2);
    b += char(1);
    b += v;
}
inline tN2kMsg msg(int p, const QByteArray &b, int source = 1) {
    tN2kMsg m;
    m.SetPGN(p);
    m.Source = source;
    m.Destination = 255;
    for (auto c : b)
        m.AddByte(quint8(c));
    return m;
}
inline tN2kMsg claim() { return msg(60928, QByteArray::fromHex("01020304050607c0")); }
inline QString name() {
    return QString::fromLatin1(QByteArray::fromHex("01020304050607c0").toHex());
}
inline tN2kMsg databases() {
    QByteArray b;
    u(b, 0, 2);
    u(b, 1, 2);
    u(b, 1, 2);
    u(b, 0, 2);
    str(b, "Fixture DB");
    u(b, 0xffffffff, 4);
    u(b, 0xffff, 2);
    u(b, 4, 1);
    u(b, 1, 2);
    u(b, 3, 4);
    u(b, 0xffffffff, 4);
    return msg(130064, b);
}
inline tN2kMsg routes() {
    QByteArray b;
    u(b, 0, 2);
    u(b, 1, 2);
    u(b, 1, 2);
    u(b, 0, 2);
    u(b, 7, 2);
    str(b, "fixture-route");
    u(b, 3, 1);
    return msg(130065, b);
}
inline tN2kMsg attrs(int critical = 0) {
    QByteArray b;
    u(b, 0, 2);
    u(b, 7, 2);
    str(b, "fixture-route");
    u(b, 0xffffffff, 4);
    u(b, 0xffff, 2);
    u(b, 255, 1);
    u(b, 3, 2);
    u(b, critical, 1);
    u(b, 0, 1);
    u(b, 0xffff, 2);
    return msg(130066, b);
}
inline tN2kMsg geometry(int start = 0, int count = 3, int total = 3) {
    QByteArray b;
    u(b, start, 2);
    u(b, count, 2);
    u(b, total, 2);
    u(b, 0, 2);
    u(b, 7, 2);
    for (int i = start; i < start + count; ++i) {
        u(b, 11 + i * 17, 2);
        str(b, QString("Point %1").arg(i));
        u(b, quint32(qint32((10 + i * .01) * 1e7)), 4);
        u(b, quint32(qint32((20 + i * .01) * 1e7)), 4);
    }
    return msg(130067, b);
}
inline int requested(const tN2kMsg &m) {
    if (m.PGN == 59904)
        return m.Data[0] | (m.Data[1] << 8) | (m.Data[2] << 16);
    return m.PGN == 126208 ? m.Data[1] | (m.Data[2] << 8) | (m.Data[3] << 16)
                           : -1;
}

// Independent classic-CAN fragmentation/reassembly for synthetic service peers.
// This deliberately does not call the application's RawCanProtocol or client.
struct Frame { quint32 id; QByteArray data; };
inline unsigned pgn(quint32 id) {
    unsigned p=(id>>8)&0x3ffff;return (p&0xff00)<0xf000?p&0x3ff00:p;
}
inline QList<Frame> frames(const tN2kMsg &m, quint8 sequence=0) {
    quint32 id=(quint32(m.Priority)<<26)|(quint32(m.PGN)<<8)|m.Source;
    if((m.PGN&0xff00)<0xf000)id|=quint32(m.Destination)<<8;
    const QByteArray data(reinterpret_cast<const char*>(m.Data),m.DataLen);
    if(m.PGN!=126208 && m.PGN!=126996 && (m.PGN<130064||m.PGN>130067))return {{id,data}};
    QList<Frame> result;int at=0,index=0;
    do {
        QByteArray part(1,char((sequence<<5)|index));
        if(!index)part+=char(data.size());
        int n=qMin(int(8-part.size()),int(data.size()-at));part+=data.mid(at,n);at+=n;
        part.append(8-part.size(),char(0xff));result.append({id,part});++index;
    }while(at<data.size());
    return result;
}
struct RequestDecoder {
    QByteArray partial;int next=0,length=0,sequence=-1;quint32 identity=0;
    QVector<tN2kMsg> messages;
    void receive(quint32 id,const QByteArray &data) {
        auto p=pgn(id);if(p!=59904&&p!=126208)return;
        QByteArray payload=data;
        if(p==126208) {
            if(data.isEmpty())return;
            int index=quint8(data[0])&31,seq=quint8(data[0])>>5;
            if(!index) {if(data.size()<2)return;partial=data.mid(2);length=quint8(data[1]);next=1;sequence=seq;identity=id;}
            else {if(index!=next||seq!=sequence||id!=identity){partial.clear();return;}partial+=data.mid(1);++next;}
            if(partial.size()<length)return;
            payload=partial.left(length);partial.clear();sequence=-1;
        }
        auto m=msg(p,payload,id&255);m.Priority=(id>>26)&7;m.Destination=(id>>8)&255;
        messages.append(m);
    }
};
inline tN2kMsg response(const tN2kMsg &request) {
    switch(requested(request)) {
    case 60928:return claim();
    case 126996:{tN2kMsg m;SetN2kPGN126996(m,2100,42,"Synthetic chartplotter","fixture","fixture","SYNTHETIC");m.Source=1;return m;}
    case 130064:return databases();case 130065:return routes();case 130066:return attrs();case 130067:return geometry();
    default:return {};
    }
}
} // namespace RouteFixture
