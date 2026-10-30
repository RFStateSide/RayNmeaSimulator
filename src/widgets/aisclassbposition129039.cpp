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

#include "aisclassbposition129039.h"
#include "aisclassbstatic129809.h"
#include "N2kMessages.h"
#include "n2khelper.h"
#include "coordinateeditor.h"
#include "convert.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include "clickablelabel.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QStyle>
#include <QVBoxLayout>
#include <cmath>
QList<QList<QString>> AisClassBPosition129039::parameters() {
    return {{"MessageID","18","","uint8_t"},
        {"Repeat","0","","tN2kAISRepeat"},
        {"UserID","0","MMSI","uint32_t"},
        {"Latitude","-1000000000","degrees","double"},
        {"Longitude","-1000000000","degrees","double"},
        {"Accuracy","false","","bool"},
        {"RAIM","false","","bool"},
        {"Seconds","60","UTC second","uint8_t"},
        {"COG","-1000000000","rad","double"},
        {"SOG","-1000000000","m/s","double"},
        {"AISInfo","4","","tN2kAISTransceiverInformation"},
        {"Heading","-1000000000","rad","double"},
        {"Unit","1","","tN2kAISUnit"},
        {"Display","false","","bool"},
        {"DSC","false","","bool"},
        {"Band","false","","bool"},
        {"Msg22","false","","bool"},
        {"Mode","0","","tN2kAISMode"},
        {"State","true","","bool"},
        {"SID","255","","uint8_t"}};
}
bool AisClassBPosition129039::measurement(const QString &f) {
    return f=="Latitude" || f=="Longitude" || f=="COG" || f=="SOG" || f=="Heading";
}
bool AisClassBPosition129039::validField(const QString &f,const QString &text) {
    if(measurement(f)) {
        bool ok=false;const double n=text.toDouble(&ok);if(!ok || !std::isfinite(n))return false;
        if(n==N2kDoubleNA)return true;
        if(f=="Latitude")return n>=-90 && n<=90;
        if(f=="Longitude")return n>=-180 && n<=180;
        if(f=="SOG")return n>=0 && n<=655.33;
        // The last rounded full-turn wire tick is 6.2832 rad (approximately 360 degrees).
        return n>=0 && n<=6.2832;
    }
    if(f=="MessageID")return text=="18";
    if(f=="Seconds")return AisClassBStatic129809::validField("SID",text) && text.toUInt()<=63;
    if(f=="Unit" || f=="Mode")return text=="0" || text=="1";
    for(const auto &row:parameters())if(row[0]==f && row[3]=="bool")return text=="0"||text=="1"||text=="true"||text=="false";
    return QStringList{"Repeat","UserID","AISInfo","SID"}.contains(f) && AisClassBStatic129809::validField(f,text);
}
QList<QPair<int,QString>> AisClassBPosition129039::choices(const QString &f) {
    if(f=="Repeat")return {{0,"Initial"},{1,"First"},{2,"Second"},{3,"Final"}};
    if(f=="AISInfo")return {{0,"Channel A reception"},{1,"Channel B reception"},{2,"Channel A transmission"},{3,"Channel B transmission"},{4,"Own information, not broadcast"},{31,"Unavailable"}};
    if(f=="Unit")return {{0,"SOTDMA"},{1,"CS (CSTDMA)"}};
    if(f=="Mode")return {{0,"Autonomous"},{1,"Assigned"}};
    for(const auto &row:parameters())if(row[0]==f && row[3]=="bool") {
        if(f=="State")return {{0,"SOTDMA state selector"},{1,"ITDMA state selector"}};
        if(f=="Band")return {{0,"Top 525 kHz"},{1,"Entire marine band"}};
        if(f=="Accuracy")return {{0,"Low (>10 m)"},{1,"High (<=10 m)"}};
        if(f=="RAIM")return {{0,"Not in use"},{1,"In use"}};
        return {{0,"No"},{1,"Yes"}};
    }
    return {};
}
QString AisClassBPosition129039::fieldDescription(const QString &f) {
    if(f=="MessageID")return "Standard Class B position report: AIS message 18. No radio activation.";
    if(f=="Latitude" || f=="Longitude")return "Manual position, canonical signed degrees; latitude -90..90, longitude -180..180. Preferred coordinate format or Unavailable. Packet resolution 1e-7 degrees; configuration precision is retained.";
    if(f=="COG" || f=="Heading")return "True angle. Widget uses degrees true, model uses radians (0..6.2832, rounded full-turn tick). Packet resolution 0.0001 rad. COG and heading are independent; Unavailable is supported.";
    if(f=="SOG")return "Speed over ground, canonical m/s (0..655.33); preferred speed units in the widget. Packet resolution 0.01 m/s. Zero is valid; Unavailable is distinct.";
    if(f=="Seconds")return "UTC observation second 0-59; 60 unavailable, 61 manual position input, 62 dead reckoning, 63 positioning inoperative. Generator-owned position samples use a valid, fresh enabled Date/Time generator; its local offset is not applied. Clock ticks and transmission alone do not timestamp a position.";
    if(f=="Accuracy")return "Position accuracy flag: false low (>10 m); true high (<=10 m). This single bit has no unavailable code.";
    if(f=="RAIM")return "Receiver autonomous integrity monitoring in use. This single bit has no unavailable code.";
    if(f=="Unit")return "Pinned tN2kAISUnit: 0 Class B SOTDMA; 1 Class B CS/CSTDMA.";
    if(f=="Mode")return "Pinned tN2kAISMode: 0 autonomous; 1 assigned.";
    if(f=="Display")return "Integrated display capable of AIS messages 12 and 14; a metadata flag, not a display simulator.";
    if(f=="DSC")return "DSC capability flag; does not operate a DSC radio.";
    if(f=="Band")return "0 top 525 kHz of marine band; 1 entire marine band.";
    if(f=="Msg22")return "Frequency management through AIS message 22 capability; does not change frequencies.";
    if(f=="State")return "Communication state selector: 0 SOTDMA; 1 ITDMA (Class B CS normally 1). The actual 19-bit state is not exposed by the pinned codec.";
    return AisClassBStatic129809::fieldDescription(f);
}
QString AisClassBPosition129039::displayValue(const QString &f,const QString &text,UserUnitPrefs prefs) {
    const double n=text.toDouble();
    if(measurement(f)) {
        if(n==N2kDoubleNA)return "Unavailable";
        if(f=="Latitude" || f=="Longitude") {
            if(prefs.latLongUnitIndex==1)return Convert::DecimalToDMS(n,f=="Latitude");
            if(prefs.latLongUnitIndex==2)return Convert::DecimalToDMM(n,f=="Latitude");
            return QString::number(n,'g',12);
        }
        return QString::number(f=="SOG"?N2kHelper::convertN2kSpeedToUserUnits(n,prefs):Convert::RadiansToDegrees(n),'g',12);
    }
    return text;
}
bool AisClassBPosition129039::encode(const QMap<QString,QString> &f,tN2kMsg &message) {
    if(f.size()!=parameters().size())return false;
    for(const auto &row:parameters())if(!f.contains(row[0]) || !validField(row[0],f.value(row[0])))return false;
    const auto id=f.value("UserID").toUInt();if(id<100000000 || id>999999999)return false;
    auto quantize=[&f](const QString &field,double scale){const double n=f.value(field).toDouble();return n==N2kDoubleNA?n:std::round(n*scale)/scale;};
    auto flag=[&f](const QString &field){return f.value(field)=="1" || f.value(field)=="true";};
    SetN2kPGN129039(message,18,tN2kAISRepeat(f.value("Repeat").toInt()),id,quantize("Latitude",1e7),quantize("Longitude",1e7),
        flag("Accuracy"),flag("RAIM"),f.value("Seconds").toUInt(),quantize("COG",1e4),quantize("SOG",100),
        tN2kAISTransceiverInformation(f.value("AISInfo")=="31"?4:f.value("AISInfo").toInt()),quantize("Heading",1e4),
        tN2kAISUnit(f.value("Unit").toInt()),flag("Display"),flag("DSC"),flag("Band"),flag("Msg22"),tN2kAISMode(f.value("Mode").toInt()),flag("State"),f.value("SID").toUInt());
    if(f.value("AISInfo")=="31")message.Data[20]=(message.Data[20]&7)|0xf8;
    return true;
}
bool AisClassBPosition129039::decode(const tN2kMsg &message,QStringList &out) {
    if(message.PGN!=129039 || message.DataLen!=27 || (message.Data[25]&0xfe)!=0xfe)return false;
    const auto info=(message.Data[20]>>3)&31;if(info>4 && info!=31)return false;
    auto safe=message;if(info==31)safe.Data[20]=(safe.Data[20]&7)|(4<<3);
    uint8_t msg,seconds,sid;uint32_t id;double latitude,longitude,cog,sog,heading;
    bool accuracy,raim,display,dsc,band,msg22,state;tN2kAISRepeat repeat;tN2kAISTransceiverInformation transceiver;tN2kAISUnit unit;tN2kAISMode mode;
    if(!ParseN2kPGN129039(safe,msg,repeat,id,latitude,longitude,accuracy,raim,seconds,cog,sog,transceiver,heading,unit,display,dsc,band,msg22,mode,state,sid))return false;
    QStringList candidate{"MessageID",QString::number(msg),"Repeat",QString::number(repeat),"UserID",QString::number(id),
        "Latitude",QString::number(latitude,'g',17),"Longitude",QString::number(longitude,'g',17),"Accuracy",accuracy?"true":"false","RAIM",raim?"true":"false",
        "Seconds",QString::number(seconds),"COG",QString::number(cog,'g',17),"SOG",QString::number(sog,'g',17),"AISInfo",QString::number(info),"Heading",QString::number(heading,'g',17),
        "Unit",QString::number(unit),"Display",display?"true":"false","DSC",dsc?"true":"false","Band",band?"true":"false","Msg22",msg22?"true":"false","Mode",QString::number(mode),"State",state?"true":"false","SID",QString::number(sid)};
    for(int i=0;i<candidate.size();i+=2)if(!validField(candidate[i],candidate[i+1]))return false;
    out=candidate;return true;
}
AisClassBPosition129039::AisClassBPosition129039(int id,const QString &label,QWidget *parent):PgnWidgetBase(id,label,parent) {
    setPgn(129039);setColumnSpan(3);setRowSpan(9);
    auto *box=new QVBoxLayout(this);auto *header=new QHBoxLayout;
    auto *enabled=new QCheckBox(this);enabled->setObjectName("checkBoxEnabled");enabled->setChecked(isEnabled());connect(enabled,&QCheckBox::clicked,this,&PgnWidgetBase::setEnabled);
    enabled->setToolTip("Individual instrument Enable; global Sending requires explicit authorization.");
    auto *title=new QLabel(label+" (129039)",this);title->setObjectName("labelTitle");title->setWordWrap(true);auto font=title->font();font.setBold(true);title->setFont(font);
    auto *remove=new QPushButton(this);remove->setObjectName("pushButtonRemove");remove->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton));remove->setToolTip("Remove AIS Position report");
    header->addWidget(enabled);header->addWidget(title,1);header->addWidget(remove);box->addLayout(header);
    auto *notice=new QLabel("Own-vessel MMSI follows active Vessel Movement; other targets remain manual. AIS publishes to MMSI vessel contexts on Signal K. No AIS radio activation.",this);notice->setWordWrap(true);box->addWidget(notice);
    auto *association=new QLabel(this);association->setObjectName("aisAssociation");association->setWordWrap(true);box->addWidget(association);
    auto *controlStatus=new QLabel(this);controlStatus->setObjectName("aisMovementStatus");controlStatus->setWordWrap(true);box->addWidget(controlStatus);controlStatus->hide();
    auto *tabs=new QTabWidget(this);auto *navigation=new QWidget(tabs),*metadata=new QWidget(tabs);auto *nav=new QGridLayout(navigation),*flags=new QGridLayout(metadata);
    for(auto *form:{nav,flags})form->setColumnStretch(1,1);
    tabs->addTab(navigation,"Position");tabs->addTab(metadata,"Report flags");box->addWidget(tabs);
    auto *validation=new QLabel(this);validation->setObjectName("aisValidation");validation->setWordWrap(true);box->addWidget(validation);
    const QMap<QString,QString> names{{"UserID","Simulated MMSI"},{"COG","Course over ground"},{"SOG","Speed over ground"},{"Heading","True heading"},{"Seconds","Observation UTC second"},{"MessageID","AIS message ID (18)"},{"AISInfo","Transceiver information"},{"Accuracy","High position accuracy"},{"RAIM","RAIM in use"},{"Unit","Class B unit"},{"Display","Integrated display"},{"DSC","DSC capability"},{"Band","Band capability"},{"Msg22","Message 22 capability"},{"Mode","Station mode"},{"State","Communication state selector"},{"SID","SID (255 unavailable)"}};
    for(const auto &row:parameters()) {
        const auto field=row[0];values[field]=row[1];auto *form=(measurement(field)||field=="UserID"||field=="Seconds")?nav:flags;
        auto *caption=new QLabel(names.value(field,field),this);captions[field]=caption;
        caption->setObjectName("label"+field);caption->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        const int gridRow=form->rowCount();form->addWidget(caption,gridRow,0);
        QLabel *units=nullptr;
        if(field=="Latitude"||field=="Longitude"||field=="SOG") {
            auto *clickable=new ClickableLabel(this);units=clickable;
            if(field=="SOG") {
                clickable->setMenuItems(unitsListSpeed);
                connect(clickable,&ClickableLabel::actionTriggered,this,&AisClassBPosition129039::ChangeSpeedUnits_triggered);
            } else {
                clickable->setMenuItems(unitsListLatLong);
                connect(clickable,&ClickableLabel::actionTriggered,this,&AisClassBPosition129039::ChangeLatLongUnits_triggered);
            }
            units->setToolTip("Left-click to change the instrument's display units or coordinate format.");
        } else units=new QLabel(this);
        units->setObjectName("labelUnit"+field);form->addWidget(units,gridRow,2);
        if(field=="Heading"||field=="COG") {
            units->setText(QString::fromUtf8("°T"));
            units->setToolTip("AIS angles are always degrees true; magnetic reference does not apply.");
        }
        if(field=="UserID")units->setText("MMSI");
        if(field=="Seconds")units->setText("s (UTC)");
        if(!choices(field).isEmpty()) {
            auto *combo=new QComboBox(this);combo->setObjectName(field);combo->setToolTip(fieldDescription(field));combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);combo->setMinimumContentsLength(12);
            for(const auto &entry:choices(field))combo->addItem(entry.second,entry.first);form->addWidget(combo,gridRow,1);
            connect(combo,&QComboBox::activated,this,[this,combo,field]{setValue(field,QString::number(combo->currentData().toInt()));});
        } else {
            auto *edit=new QLineEdit(this);edit->setObjectName(field);edit->setReadOnly(field=="MessageID");edit->setToolTip(fieldDescription(field));editors[field]=edit;form->addWidget(edit,gridRow,1);caption->setBuddy(edit);
            connect(edit,&QLineEdit::textEdited,this,[this,edit]{if(!edit->property("_rayAisInputPrefs").isValid())edit->setProperty("_rayAisInputPrefs",QVariantList{UserPrefs().latLongUnitIndex,UserPrefs().speedUnitIndex});});
            connect(edit,&QLineEdit::editingFinished,this,[this,edit,field,validation]{
                if(!edit->isModified())return;
                if((m_navigationControlled && measurement(field)) || (m_observationTimeControlled && field=="Seconds"))return; // Keep an unfinished manual draft.
                auto prefs=UserPrefs();const auto saved=edit->property("_rayAisInputPrefs").toList();if(saved.size()==2){prefs.latLongUnitIndex=saved[0].toUInt();prefs.speedUnitIndex=saved[1].toUInt();}
                QString text=edit->text();
                if(measurement(field)) {
                    bool ok=false;double n=N2kDoubleNA;
                    if(text.compare("Unavailable",Qt::CaseInsensitive)==0)ok=true;
                    else if(text==edit->property("_rayAisDisplayedText").toString() && prefs.latLongUnitIndex==edit->property("_rayAisDisplayCoordinates").toUInt() && prefs.speedUnitIndex==edit->property("_rayAisDisplaySpeed").toUInt()) {n=edit->property("_rayAisCanonicalValue").toDouble();ok=true;}
                    else if((field=="Latitude"||field=="Longitude") && prefs.latLongUnitIndex!=0) {
                        CoordinateValidator validator(prefs.latLongUnitIndex,field=="Latitude",nullptr);int pos=0;ok=validator.validate(text,pos)==QValidator::Acceptable;
                        if(ok)n=prefs.latLongUnitIndex==1?Convert::DMSToDecimal(text,field=="Latitude"):Convert::DMMToDecimal(text,field=="Latitude");
                    } else {
                        n=text.toDouble(&ok);if(ok && field=="SOG")n=N2kHelper::convertUserSpeedToN2kUnits(n,prefs);
                        if(ok && (field=="Heading"||field=="COG"))n=Convert::DegreesToRadians(n);
                    }
                    text=ok?QString::number(n,'g',17):QString();
                }
                if(validField(field,text)){validation->clear();setValue(field,text);}else validation->setText("Invalid "+field+"; previous value retained. "+fieldDescription(field));
                edit->setModified(false);edit->setProperty("_rayAisInputPrefs",QVariant());refresh();
            });
        }
    }
    // The flags page is taller; keep measurement rows compact at the top of its sibling page.
    for(auto *form:{nav,flags})form->setRowStretch(form->rowCount(),1);
    connect(this,&PgnWidgetBase::UserPrefsChanged,this,&AisClassBPosition129039::refresh);refresh();
}
void AisClassBPosition129039::setValue(const QString &field,const QString &text) {
    if(!values.contains(field)||!validField(field,text)||values[field]==text)return;
    values[field]=text;refresh();emit parameterValueChanged(field,text);
}
void AisClassBPosition129039::refresh() {
    for(auto i=editors.begin();i!=editors.end();++i) {
        if(i.value()->isModified())continue; // Includes drafts held across generator takeover/release.
        const auto field=i.key();const auto text=displayValue(field,values.value(field),UserPrefs());QSignalBlocker block(i.value());i.value()->setText(text);
        i.value()->setProperty("_rayAisDisplayedText",text);i.value()->setProperty("_rayAisCanonicalValue",values.value(field).toDouble());i.value()->setProperty("_rayAisDisplayCoordinates",UserPrefs().latLongUnitIndex);i.value()->setProperty("_rayAisDisplaySpeed",UserPrefs().speedUnitIndex);
        auto *units=findChild<QLabel*>("labelUnit"+field);
        if(field=="SOG")units->setText(unitsListSpeed.value(UserPrefs().speedUnitIndex));
        if(field=="Latitude"||field=="Longitude") {
            units->setText(QStringList{QString::fromUtf8("DD.DDDDDD°"),QString::fromUtf8("DD°MM’SS”"),QString::fromUtf8("DD°MM.MMM’")}.value(UserPrefs().latLongUnitIndex));
            i.value()->setPlaceholderText(units->text());
        }
    }
    for(const auto &row:parameters())if(!choices(row[0]).isEmpty()){auto *combo=findChild<QComboBox*>(row[0]);QSignalBlocker block(combo);const auto text=values.value(row[0]);combo->setCurrentIndex(combo->findData(text=="true"?1:text=="false"?0:text.toInt()));}
}
void AisClassBPosition129039::setInitialGuiValues(){refresh();}
void AisClassBPosition129039::initializeUnitsLimits(){refresh();}

void AisClassBPosition129039::setNavigationControlled(bool controlled)
{
    if(m_navigationControlled==controlled)return;
    m_navigationControlled=controlled;
    if(!controlled)setObservationTimeControlled(false);
    for(auto it=editors.begin();it!=editors.end();++it)
        if(measurement(it.key()))it.value()->setReadOnly(controlled);
    auto *status=findChild<QLabel*>("aisMovementStatus");
    status->setText(controlled ? tr("Generator vessel controlling AIS position. Navigation is read-only; unfinished manual edits are retained until control stops.") : QString());
    status->setVisible(controlled);
    refresh();emit navigationControlChanged();
}

void AisClassBPosition129039::setObservationTimeControlled(bool controlled)
{
    if(m_observationTimeControlled==controlled)return;
    m_observationTimeControlled=controlled;
    editors.value("Seconds")->setReadOnly(controlled);
    emit navigationControlChanged();
}
