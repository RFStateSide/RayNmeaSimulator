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

#ifndef NMEA0183COMPAT_H
#define NMEA0183COMPAT_H
#include "NMEA0183Msg.h"
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QDate>
#include <cmath>
namespace Nmea0183Compat {
// Validate wire fields before permissive upstream atof/atoi parsers run.
inline bool validFields(const tNMEA0183Msg &msg) {
    const QString type = QString::fromLatin1(msg.MessageCode());
    QStringList f;
    for (int i=0;i<msg.FieldCount();++i) f << QString::fromLatin1(msg.Field(i));
    auto number = [&](int i, double lo, double hi, bool integer=false) {
        if (i>=f.size()) return false;
        if (f[i].isEmpty()) return true;
        static const QRegularExpression decimal("^[+-]?[0-9]+(?:\\.[0-9]+)?$");
        bool ok; double v=f[i].toDouble(&ok);
        return decimal.match(f[i]).hasMatch() && ok && std::isfinite(v) && v>=lo && v<=hi && (!integer || std::floor(v)==v);
    };
    auto time = [&](int i) {
        if (f[i].isEmpty()) return true;
        static const QRegularExpression utc("^[0-9]{6}(?:\\.[0-9]+)?$");
        return utc.match(f[i]).hasMatch() && f[i].left(2).toInt()<24 && f[i].mid(2,2).toInt()<60 && f[i].mid(4).toDouble()<60;
    };
    auto coordinate = [&](int i, bool lat) {
        if (f[i].isEmpty()) return f[i+1].isEmpty();
        if (f[i+1] != (lat?"N":"E") && f[i+1] != (lat?"S":"W")) return false;
        if (!number(i,0,lat?9000:18000)) return false;
        const double v=f[i].toDouble(); return std::fmod(v,100)<60;
    };
    auto correction = [&](int i) {
        return f[i].isEmpty() ? f[i+1].isEmpty() : number(i,0,180) && (f[i+1]=="E" || f[i+1]=="W");
    };
    auto status = [&](int i) { return f[i]=="A" || f[i]=="V"; };
    auto mode = [&](int i) { return f[i].isEmpty() || (f[i].size()==1 && QString("ADEFMNPRS").contains(f[i])); };
    if(type=="GGA") return f.size()==14 && time(0) && coordinate(1,true) && coordinate(3,false)
        && number(5,0,8,true) && number(6,0,99,true) && number(7,0,9999) && number(8,-100000,100000)
        && f[9]=="M" && number(10,-100000,100000) && f[11]=="M" && number(12,0,99999) && number(13,0,9999,true);
    if(type=="GLL") return (f.size()==6 || f.size()==7) && coordinate(0,true) && coordinate(2,false) && time(4) && status(5) && (f.size()==6 || mode(6));
    if(type=="HDT") return f.size()==2 && number(0,0,360) && f[1]=="T";
    if(type=="HDG") return f.size()==5 && number(0,0,360) && correction(1) && correction(3);
    if(type=="VTG") return (f.size()==8 || f.size()==9) && number(0,0,360) && f[1]=="T" && number(2,0,360) && f[3]=="M"
        && number(4,0,100000) && f[5]=="N" && number(6,0,100000) && f[7]=="K" && (f.size()==8 || mode(8));
    if(type=="RMC") {
        if(f.size()<11 || f.size()>13) return false;
        const QString date=f[8];
        const bool dateOk=date.isEmpty() || (QRegularExpression("^[0-9]{6}$").match(date).hasMatch()
            && QDate(2000+date.right(2).toInt(),date.mid(2,2).toInt(),date.left(2).toInt()).isValid());
        return time(0) && status(1) && coordinate(2,true) && coordinate(4,false) && number(6,0,100000) && number(7,0,360)
            && dateOk && correction(9) && (f.size()<12 || mode(11))
            && (f.size()<13 || f[12].isEmpty() || (f[12].size()==1 && QString("SCUV").contains(f[12])));
    }
    if(type=="ZDA") return f.size()==6 && time(0) && number(1,1,31,true) && number(2,1,12,true)
        && number(3,1,9999,true) && QDate(f[3].toInt(),f[2].toInt(),f[1].toInt()).isValid()
        && number(4,-13,13,true) && number(5,0,59,true);
    return false;
}
// HDG has heading, deviation, E/W, variation, E/W. Outputs use radians.
inline bool parseHdg(const tNMEA0183Msg &msg, double &heading, double &deviation, double &variation) {
    if (QString::fromLatin1(msg.MessageCode()) != "HDG" || msg.FieldCount() != 5) return false;
    constexpr double radians = 0.017453292519943295;
    auto number = [](const char *field, double &value) {
        bool ok = false;
        value = QString::fromLatin1(field).toDouble(&ok);
        return ok && std::isfinite(value);
    };
    auto correction = [&](int index, double &value) {
        const QString amount = QString::fromLatin1(msg.Field(index));
        const QString direction = QString::fromLatin1(msg.Field(index + 1));
        if (amount.isEmpty() && direction.isEmpty()) { value = NMEA0183DoubleNA; return true; }
        if (!number(msg.Field(index), value) || value < 0 || (direction != "E" && direction != "W")) return false;
        value *= radians * (direction == "W" ? -1 : 1);
        return true;
    };
    double h, d, v;
    if (QString::fromLatin1(msg.Field(0)).isEmpty()) h = NMEA0183DoubleNA;
    else if (!number(msg.Field(0), h)) return false;
    if (!correction(1, d) || !correction(3, v)) return false;
    heading = h == NMEA0183DoubleNA ? h : h * radians; deviation = d; variation = v;
    return true;
}
}
#endif
