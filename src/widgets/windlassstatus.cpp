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

#include "windlassstatus.h"
#include "N2kMessages.h"
#include "n2khelper.h"
#include "clickablelabel.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <cmath>
#include <QSet>

namespace {
QString number(double v) { return QString::number(v,'g',17); }
double maximum(const QString &field) {
    if(field=="RodeCounterValue")return 6553.3;
    if(field=="WindlassLineSpeed")return 655.33;
    if(field=="ControllerVoltage")return 50.6;
    if(field=="MotorCurrent")return 253;
    if(field=="TotalMotorTime")return 3931980;
    return 255;
}
double step(const QString &field) {
    if(field=="RodeCounterValue")return 0.1;
    if(field=="WindlassLineSpeed")return 0.01;
    if(field=="ControllerVoltage")return 0.2;
    if(field=="TotalMotorTime")return 60;
    return 1;
}
QString units(const QString &field,const UserUnitPrefs &prefs) {
    if(field=="RodeCounterValue")return prefs.distanceUnitIndex==0?"m":"ft";
    // Minutes stay canonical; only the distance component follows near-distance units.
    if(field=="WindlassLineSpeed")return prefs.distanceUnitIndex==0?"m/min":"ft/min";
    if(field=="TotalMotorTime")return "s";
    if(field=="ControllerVoltage")return "V";
    if(field=="MotorCurrent")return "A";
    return {};
}
}
QList<QList<QString>> WindlassStatus::parameters(int pgn) {
    const auto na=number(N2kDoubleNA);
    QList<QList<QString>> rows{{"SID","255","","uint8_t"},{"WindlassIdentifier","0","","uint8_t"}};
    if(pgn==128777)rows.append({{"RodeCounterValue",na,"m","double"},{"WindlassLineSpeed",na,"m/min","double"},
        {"WindlassMotionStatus","3","","tN2kWindlassMotionStates"},{"RodeTypeStatus","3","","tN2kRodeTypeStates"},
        {"AnchorDockingStatus","3","","tN2kAnchorDockingStates"},{"OperatingEvents","0","","tN2kWindlassOperatingEvents"}});
    else if(pgn==128778)rows.append({{"TotalMotorTime",na,"s","double"},{"ControllerVoltage",na,"V","double"},
        {"MotorCurrent",na,"A","double"},{"MonitoringEvents","0","","tN2kWindlassMonitoringEvents"}});
    else rows.clear();
    return rows;
}
bool WindlassStatus::measurement(const QString &f) {
    return QStringList{"RodeCounterValue","WindlassLineSpeed","TotalMotorTime","ControllerVoltage","MotorCurrent"}.contains(f);
}
QList<QPair<int,QString>> WindlassStatus::choices(const QString &f) {
    QStringList names;
    if(f=="WindlassMotionStatus")names={"Stopped","Deploying","Retrieving","Unavailable"};
    if(f=="RodeTypeStatus")names={"Chain detected","Rope detected","Error","Unavailable"};
    if(f=="AnchorDockingStatus")names={"Not docked","Fully docked","Error","Unavailable"};
    QList<QPair<int,QString>> result;
    for(int i=0;i<names.size();++i)result.append({i,names[i]});
    if(f=="OperatingEvents" || f=="MonitoringEvents") {
        names=f=="OperatingEvents"?QStringList{"System error","Sensor error","No motion detected","Docking distance reached","End of rode reached"}
            :QStringList{"Under-voltage cutout","Over-current cutout","Over-temperature cutout"};
        for(int mask=0;mask<(1<<names.size());++mask) {
            QStringList active;for(int bit=0;bit<names.size();++bit)if(mask&(1<<bit))active.append(names[bit]);
            result.append({mask,active.isEmpty()?"No events":active.join(" + ")});
        }
    }
    return result;
}
bool WindlassStatus::validField(int pgn,const QString &f,const QString &text) {
    bool exists=false;for(const auto &row:parameters(pgn))if(row[0]==f)exists=true;
    bool ok=false;const double v=text.toDouble(&ok);if(!exists||!ok||!std::isfinite(v))return false;
    if(measurement(f))return v==N2kDoubleNA || (v>=0 && v<=maximum(f));
    bool integerOk=false;const auto integer=text.toUInt(&integerOk);
    if(!integerOk || integer!=v || std::floor(v)!=v || v<0)return false;
    const auto options=choices(f);
    if(!options.isEmpty()){for(const auto &option:options)if(option.first==v)return true;return false;}
    return v<=255; // Exact byte identities; zero is valid, never a CAN source address.
}
QString WindlassStatus::description(const QString &f) {
    if(f=="SID")return "Sequence identifier byte; 255 unavailable. No automatic sequence increment.";
    if(f=="WindlassIdentifier")return "Windlass identifier byte (0-255, including zero). Matching status IDs are informational only. Receive streams are also qualified by CAN source; this is not the CAN source address.";
    if(f=="WindlassLineSpeed")return "Manual line speed in m/min following the pinned library's per-minute contract, 0-655.33. Wire resolution 0.01 m/min. Display/input use m/min or ft/min via Distance preferences. Historical Canboat says m/s: cross-implementation units require qualification. Motion status never changes this value.";
    if(measurement(f))return QString("Manual measurement, canonical %1, range 0-%2 or Unavailable. Wire resolution %3; configured precision is retained until encoding. No automatic accumulation or physics.").arg(f=="RodeCounterValue"?"m":f=="TotalMotorTime"?"s":f=="ControllerVoltage"?"V":"A",QString::number(maximum(f),'g',12),QString::number(step(f),'g',12));
    if(f=="OperatingEvents")return "Exact 5-bit event mask: bit 0 system error; 1 sensor error; 2 no motion; 3 docking distance; 4 end of rode. Reserved bit 5 is unsupported by the pinned parser and rejected. No invented event-unavailable state.";
    if(f=="MonitoringEvents")return "Exact 3-bit event mask: bit 0 under-voltage; 1 over-current; 2 over-temperature cutout. Higher manufacturer/reserved bits are discarded by the pinned parser and rejected rather than silently lost. No invented event-unavailable state.";
    return "Manual reported status, not a command. All four codec-defined states, including error/unavailable where defined, are supported. No generator or windlass actuation.";
}
QString WindlassStatus::displayUnit(const QString &field,const UserUnitPrefs &prefs) { return units(field,prefs); }
QString WindlassStatus::displayValue(const QString &f,const QString &text,const UserUnitPrefs &prefs) {
    if(measurement(f)) {
        double v=text.toDouble();if(v==N2kDoubleNA)return "Unavailable";
        if(f=="RodeCounterValue" || f=="WindlassLineSpeed")v=N2kHelper::convertN2kDistanceNearToUserUnits(v,prefs);
        return QString::number(v,'g',12);
    }
    if(f=="SID" && text=="255")return "Unavailable";
    for(const auto &option:choices(f))if(option.first==text.toInt())return QString::number(option.first)+" - "+option.second;
    return text;
}
bool WindlassStatus::validParameters(int pgn,const QStringList &fields) {
    if(fields.size()!=parameters(pgn).size()*2)return false;
    QSet<QString> seen;
    for(int i=0;i<fields.size();i+=2){if(seen.contains(fields[i])||!validField(pgn,fields[i],fields[i+1]))return false;seen.insert(fields[i]);}
    return supported(pgn);
}
bool WindlassStatus::encode(int pgn,const QMap<QString,QString> &f,tN2kMsg &message) {
    if(!supported(pgn)||f.size()!=parameters(pgn).size())return false;
    for(const auto &row:parameters(pgn))if(!f.contains(row[0])||!validField(pgn,row[0],f[row[0]]))return false;
    // Round only a temporary wire snapshot. The pinned helpers round too, but
    // decimal multiplication avoids binary division at half-tick boundaries.
    auto wire=[&](const QString &field){double v=f[field].toDouble();if(v==N2kDoubleNA)return v;const double scale=step(field);return std::round(v*(1./scale))*scale;};
    if(pgn==128777) {
        tN2kWindlassOperatingEvents events;events.SetEvents(f["OperatingEvents"].toUInt());
        SetN2kPGN128777(message,f["SID"].toUInt(),f["WindlassIdentifier"].toUInt(),wire("RodeCounterValue"),wire("WindlassLineSpeed"),
            tN2kWindlassMotionStates(f["WindlassMotionStatus"].toInt()),tN2kRodeTypeStates(f["RodeTypeStatus"].toInt()),tN2kAnchorDockingStates(f["AnchorDockingStatus"].toInt()),events);
    } else {
        tN2kWindlassMonitoringEvents events;events.SetEvents(f["MonitoringEvents"].toUInt());
        SetN2kPGN128778(message,f["SID"].toUInt(),f["WindlassIdentifier"].toUInt(),wire("TotalMotorTime"),wire("ControllerVoltage"),wire("MotorCurrent"),events);
    }
    return true;
}
bool WindlassStatus::decode(const tN2kMsg &m,QStringList &out) {
    if(!supported(m.PGN)||m.DataLen!=8)return false;
    auto u16=[&](int offset){return int(m.Data[offset])|(int(m.Data[offset+1])<<8);};
    unsigned char sid,id;QStringList candidate;
    if(m.PGN==128777) {
        // Enforce the pinned codec's layout and refuse data its parser would erase.
        if((m.Data[2]&0xf0)!=0xf0 || (m.Data[7]&0x80) || u16(3)==0xfffe || u16(5)==0xfffe)return false;
        double rode,speed;tN2kWindlassMotionStates motion;tN2kRodeTypeStates type;tN2kAnchorDockingStates docking;tN2kWindlassOperatingEvents events;
        if(!ParseN2kPGN128777(m,sid,id,rode,speed,motion,type,docking,events))return false;
        candidate={"SID",QString::number(sid),"WindlassIdentifier",QString::number(id),"RodeCounterValue",number(rode),"WindlassLineSpeed",number(speed),
            "WindlassMotionStatus",QString::number(motion),"RodeTypeStatus",QString::number(type),"AnchorDockingStatus",QString::number(docking),"OperatingEvents",QString::number(events.Events)};
    } else {
        if((m.Data[2]&0xf8) || m.Data[7]!=0xff || m.Data[3]==0xfe || m.Data[4]==0xfe || u16(5)==0xfffe)return false;
        double time,voltage,current;tN2kWindlassMonitoringEvents events;
        if(!ParseN2kPGN128778(m,sid,id,time,voltage,current,events))return false;
        candidate={"SID",QString::number(sid),"WindlassIdentifier",QString::number(id),"TotalMotorTime",number(time),"ControllerVoltage",number(voltage),"MotorCurrent",number(current),"MonitoringEvents",QString::number(events.Events)};
    }
    if(!validParameters(m.PGN,candidate))return false;
    out=candidate;return true;
}
WindlassStatus::WindlassStatus(int id,const QString &label,int pgn,QWidget *parent):PgnWidgetBase(id,label,parent) {
    setPgn(pgn);setColumnSpan(3);setRowSpan(5);setEnabled(false);
    auto *box=new QVBoxLayout(this);auto *header=new QHBoxLayout;
    auto *enabled=new QCheckBox(this);enabled->setObjectName("checkBoxEnabled");enabled->setChecked(false);enabled->setToolTip("Enable this manual status report. Global Sending still requires explicit authorization.");
    connect(enabled,&QCheckBox::clicked,this,&PgnWidgetBase::setEnabled);
    auto *title=new QLabel(label+QString(" (%1)").arg(pgn),this);title->setObjectName("labelTitle");title->setWordWrap(true);auto font=title->font();font.setBold(true);title->setFont(font);
    auto *remove=new QPushButton(this);remove->setObjectName("pushButtonRemove");remove->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton));remove->setToolTip("Remove this status instrument");
    header->addWidget(enabled);header->addWidget(title,1);header->addWidget(remove);box->addLayout(header);
    auto *notice=new QLabel("Manual status only. No control commands or automatic motion. Native Signal K output is unsupported; use an authorized NMEA2000 transport.",this);notice->setWordWrap(true);box->addWidget(notice);
    auto *association=new QLabel(this);association->setObjectName("windlassAssociation");association->setWordWrap(true);box->addWidget(association);
    auto *form=new QGridLayout;form->setColumnStretch(1,1);box->addLayout(form);
    auto *validation=new QLabel(this);validation->setObjectName("windlassValidation");validation->setWordWrap(true);box->addWidget(validation);
    const QMap<QString,QString> titles{{"SID","Sequence identifier"},{"WindlassIdentifier","Windlass identifier"},{"RodeCounterValue","Deployed rode length"},{"WindlassLineSpeed","Line speed"},
        {"WindlassMotionStatus","Motion status"},{"RodeTypeStatus","Rode type"},{"AnchorDockingStatus","Anchor docking"},{"OperatingEvents","Operating events (mask)"},
        {"TotalMotorTime","Total motor time"},{"ControllerVoltage","Controller voltage"},{"MotorCurrent","Motor current"},{"MonitoringEvents","Monitoring events (mask)"}};
    for(const auto &row:parameters(pgn)) {
        const auto field=row[0];values[field]=row[1];const int n=form->rowCount();
        auto *caption=new QLabel(titles[field],this);caption->setObjectName("label"+field);caption->setAlignment(Qt::AlignRight|Qt::AlignVCenter);form->addWidget(caption,n,0);
        QLabel *unit;
        if(field=="RodeCounterValue" || field=="WindlassLineSpeed") {
            auto *click=new ClickableLabel(this);click->setMenuItems(unitsListDistance);unit=click;
            connect(click,&ClickableLabel::actionTriggered,this,&WindlassStatus::ChangeDistanceUnits_triggered);unit->setToolTip("Click to change Distance units. Canonical m or m/min values are preserved.");
        } else unit=new QLabel(this);
        unit->setObjectName("labelUnit"+field);form->addWidget(unit,n,2);
        const auto options=choices(field);
        if(!options.isEmpty()) {
            auto *combo=new QComboBox(this);combo->setObjectName(field);combo->setToolTip(description(field));
            for(const auto &option:options)combo->addItem(QString::number(option.first)+" - "+option.second,option.first);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);combo->setMinimumContentsLength(12);form->addWidget(combo,n,1);
            connect(combo,&QComboBox::activated,this,[this,combo,field,validation]{validation->clear();setValue(field,QString::number(combo->currentData().toInt()));});
        } else {
            auto *edit=new QLineEdit(this);edit->setObjectName(field);edit->setToolTip(description(field));caption->setBuddy(edit);editors[field]=edit;form->addWidget(edit,n,1);
            connect(edit,&QLineEdit::textEdited,this,[this,edit]{if(!edit->property("editDistanceUnit").isValid())edit->setProperty("editDistanceUnit",UserPrefs().distanceUnitIndex);});
            connect(edit,&QLineEdit::editingFinished,this,[this,edit,field,validation] {
                if(!edit->isModified())return;
                QString text=edit->text();
                if(measurement(field) && text.compare("Unavailable",Qt::CaseInsensitive)==0)text=number(N2kDoubleNA);
                else if(field=="RodeCounterValue" || field=="WindlassLineSpeed") {
                    auto prefs=UserPrefs();if(edit->property("editDistanceUnit").isValid())prefs.distanceUnitIndex=edit->property("editDistanceUnit").toUInt();
                    bool ok=false;double v=text.toDouble(&ok);
                    if(ok && text==edit->property("renderedText").toString() && prefs.distanceUnitIndex==edit->property("renderedUnit").toUInt())v=edit->property("renderedCanonical").toDouble();
                    else if(ok)v=N2kHelper::convertUserDistanceNearToN2kUnits(v,prefs);
                    text=ok?number(v):QString();
                }
                if(validField(Pgn(),field,text)){validation->clear();setValue(field,text);}
                else validation->setText("Invalid "+field+"; previous value retained. "+description(field));
                edit->setModified(false);edit->setProperty("editDistanceUnit",QVariant());refresh();
            });
        }
    }
    connect(this,&PgnWidgetBase::UserPrefsChanged,this,&WindlassStatus::refresh);refresh();
}
void WindlassStatus::showInvalidEdit(const QString &field) {
    if(auto *label=findChild<QLabel*>("windlassValidation"))label->setText("Invalid "+field+"; previous value retained. "+description(field));
}
void WindlassStatus::setValue(const QString &f,const QString &text) {
    if(!values.contains(f)||!validField(Pgn(),f,text))return;
    if(auto *label=findChild<QLabel*>("windlassValidation"))label->clear();
    if(values[f]==text)return;
    values[f]=text;refresh();emit parameterValueChanged(f,text);
}
void WindlassStatus::refresh() {
    for(auto it=editors.begin();it!=editors.end();++it) {
        auto *edit=it.value();if(edit->hasFocus() && edit->isModified())continue;
        // Identity editors remain numeric, including SID 255.
        const auto text=measurement(it.key())?displayValue(it.key(),values[it.key()],UserPrefs()):values[it.key()];
        const QSignalBlocker block(edit);edit->setText(text);
        edit->setProperty("renderedText",text);edit->setProperty("renderedCanonical",values[it.key()].toDouble());edit->setProperty("renderedUnit",UserPrefs().distanceUnitIndex);
    }
    for(const auto &row:parameters(Pgn())) {
        if(auto *unit=findChild<QLabel*>("labelUnit"+row[0])) {
            auto prefs=UserPrefs();auto *edit=editors.value(row[0]);
            if(edit && edit->hasFocus() && edit->isModified() && edit->property("editDistanceUnit").isValid())
                prefs.distanceUnitIndex=edit->property("editDistanceUnit").toUInt();
            unit->setText(displayUnit(row[0],prefs));
        }
        if(auto *combo=findChild<QComboBox*>(row[0])){const QSignalBlocker block(combo);combo->setCurrentIndex(combo->findData(values[row[0]].toInt()));}
    }
}
void WindlassStatus::setInitialGuiValues(){refresh();}
void WindlassStatus::initializeUnitsLimits(){refresh();}
