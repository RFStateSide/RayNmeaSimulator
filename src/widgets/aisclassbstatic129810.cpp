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

#include "aisclassbstatic129810.h"
#include "aisclassbstatic129809.h"
#include "n2khelper.h"
#include "N2kMessages.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include "clickablelabel.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <cmath>

bool AisClassBStatic129810::dimension(const QString &field) {
    return field == "Length" || field == "Beam" || field == "PosRefStbd" || field == "PosRefBow";
}
QList<QList<QString>> AisClassBStatic129810::parameters() {
    const auto na=QString::number(N2kDoubleNA,'g',17);
    return {{"MessageID","24","","uint8_t"},{"Repeat","0","","tN2kAISRepeat"},
        {"UserID","0","MMSI","uint32_t"},{"VesselType","0","","uint8_t"},
        {"Vendor","","","QString"},{"Callsign","","","QString"},
        {"Length",na,"m","double"},{"Beam",na,"m","double"},
        {"PosRefStbd",na,"m","double"},{"PosRefBow",na,"m","double"},
        {"MothershipID","0","MMSI","uint32_t"},{"AISInfo","4","","tN2kAISTransceiverInformation"},{"SID","255","","uint8_t"}};
}
QString AisClassBStatic129810::fieldDescription(const QString &field) {
    if (dimension(field)) return "Configured measurements retain canonical meter precision, range 0-6553.3 m. NMEA2000 output rounds to the nearest 0.1 m only when encoding. Enter a measurement in the displayed units or Unavailable. Offsets are measured from starboard/bow to the position reference point.";
    if (field == "Vendor" || field == "Callsign") return "At most seven uppercase AIS characters (ASCII 32-95, excluding @ padding). Empty is unavailable. Vendor is a literal seven-character library field, not a parsed manufacturer/model structure.";
    if (field == "VesselType") return "Library uint8 vessel type: 0 not specified; 1-99 AIS types; 100-254 reserved (receive/restore only); 255 unavailable. This is not a library C++ enumeration.";
    if (field == "MothershipID") return "Optional mothership MMSI (e.g. auxiliary craft); 0 means not applicable, 4294967295 unavailable. Never used as this report's identity or CAN source.";
    return AisClassBStatic129809::fieldDescription(field);
}
bool AisClassBStatic129810::validField(const QString &field, const QString &text) {
    if (dimension(field)) {
        bool ok=false;const double n=text.toDouble(&ok);
        return ok && std::isfinite(n) && (n==N2kDoubleNA || (n>=0 && n<=6553.3));
    }
    if (field=="Vendor" || field=="Callsign") return text.size()<=7 && AisClassBStatic129809::validField("Name",text);
    if (field=="MothershipID") return AisClassBStatic129809::validField("UserID",text);
    if (field=="VesselType") return AisClassBStatic129809::validField("SID",text);
    return field!="Name" && AisClassBStatic129809::validField(field,text);
}
bool AisClassBStatic129810::editableField(const QString &field, const QString &text) {
    return validField(field,text) && (field!="VesselType" || text.toUInt()<100 || text.toUInt()==255);
}
QList<QPair<int, QString>> AisClassBStatic129810::vesselTypes() {
    const QMap<int,QString> names{{0,"Not specified"},{20,"Wing in ground"},{29,"Wing in ground, no further information"},
        {30,"Fishing"},{31,"Towing"},{32,"Towing, over 200 m or 25 m wide"},{33,"Dredging / underwater operations"},
        {34,"Diving"},{35,"Military operations"},{36,"Sailing"},{37,"Pleasure craft"},{40,"High speed craft"},
        {49,"High speed craft, no further information"},{50,"Pilot vessel"},{51,"Search and rescue"},{52,"Tug"},
        {53,"Port tender"},{54,"Anti-pollution"},{55,"Law enforcement"},{58,"Medical"},{59,"RR Resolution 18"}};
    QList<QPair<int,QString>> out;
    for(int n=0;n<=255;++n) {
        QString label=names.value(n);
        if(label.isEmpty() && n>=60 && n<=99) label=QStringList{"Passenger","Cargo","Tanker","Other"}[(n-60)/10]+QString(" (type %1)").arg(n);
        if(label.isEmpty() && n>=21 && n<=28) label=QString("Wing in ground (type %1)").arg(n);
        if(label.isEmpty() && n>=41 && n<=48) label=QString("High speed craft (type %1)").arg(n);
        if(label.isEmpty())label=n==255?"Unavailable":n>=100?"Reserved, receive only":"Reserved / unspecified type";
        out.append({n,QString::number(n)+" - "+label});
    }
    return out;
}
bool AisClassBStatic129810::encode(const QMap<QString,QString> &fields,tN2kMsg &message) {
    if(fields.size()!=parameters().size())return false;
    for(const auto &row:parameters())if(!fields.contains(row[0])||!editableField(row[0],fields.value(row[0])))return false;
    const auto id=fields.value("UserID").toUInt();if(id<100000000||id>999999999)return false;
    const auto vendor=fields.value("Vendor").toLatin1(),callsign=fields.value("Callsign").toLatin1();
    // Quantize only the packet copy. Multiplying by the decimal scale avoids
    // division by binary 0.1 moving values such as 1.15 just below a half tick.
    const auto wireDimension = [&fields](const QString &field) {
        const double value = fields.value(field).toDouble();
        return value == N2kDoubleNA ? value : std::round(value * 10.0) / 10.0;
    };
    SetN2kPGN129810(message,24,tN2kAISRepeat(fields.value("Repeat").toInt()),id,fields.value("VesselType").toUInt(),
        vendor.constData(),callsign.constData(),wireDimension("Length"),wireDimension("Beam"),
        wireDimension("PosRefStbd"),wireDimension("PosRefBow"),fields.value("MothershipID").toUInt(),
        tN2kAISTransceiverInformation(fields.value("AISInfo")=="31"?4:fields.value("AISInfo").toInt()),fields.value("SID").toUInt());
    if(fields.value("AISInfo")=="31")message.Data[33]=0xff; // NA not named in pinned C++ enum.
    return true;
}
bool AisClassBStatic129810::decode(const tN2kMsg &message,QStringList &out) {
    if(message.PGN!=129810||message.DataLen!=35)return false;
    // Low two reserved bits must be 1. Spare is carried by the library, not an editable field.
    if((message.Data[32]&3)!=3 || (message.Data[33]&0xe0)!=0xe0)return false;
    const int rawInfo=message.Data[33]&0x1f;if(rawInfo>4 && rawInfo!=31)return false;
    auto available=message;if(rawInfo==31)available.Data[33]=0xe4;
    uint8_t messageId,type,sid;uint32_t id,mother;char vendor[8]={},callsign[8]={};
    double length,beam,stbd,bow;tN2kAISRepeat repeat;tN2kAISTransceiverInformation info;
    if(!ParseN2kPGN129810(available,messageId,repeat,id,type,vendor,sizeof(vendor),callsign,sizeof(callsign),length,beam,stbd,bow,mother,info,sid))return false;
    QStringList values{"MessageID",QString::number(messageId),"Repeat",QString::number(repeat),"UserID",QString::number(id),
        "VesselType",QString::number(type),"Vendor",QString::fromLatin1(vendor),"Callsign",QString::fromLatin1(callsign),
        "Length",QString::number(length,'g',17),"Beam",QString::number(beam,'g',17),
        "PosRefStbd",QString::number(stbd,'g',17),"PosRefBow",QString::number(bow,'g',17),
        "MothershipID",QString::number(mother),"AISInfo",QString::number(rawInfo),"SID",QString::number(sid)};
    for(int i=0;i<values.size();i+=2)if(!validField(values[i],values[i+1]))return false;
    out=values;return true;
}
AisClassBStatic129810::AisClassBStatic129810(int id,const QString &label,QWidget *parent):PgnWidgetBase(id,label,parent) {
    setPgn(129810);setColumnSpan(3);setRowSpan(9);
    auto *box=new QVBoxLayout(this);auto *header=new QHBoxLayout;
    auto *enabled=new QCheckBox(this);enabled->setObjectName("checkBoxEnabled");enabled->setChecked(isEnabled());
    enabled->setToolTip("Enable this static report; global Sending also requires explicit authorization.");
    connect(enabled,&QCheckBox::clicked,this,&PgnWidgetBase::setEnabled);
    auto *title=new QLabel(label+" (129810)",this);title->setObjectName("labelTitle"); title->setWordWrap(true);auto font=title->font();font.setBold(true);title->setFont(font);
    auto *remove=new QPushButton(this);remove->setObjectName("pushButtonRemove");remove->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton));remove->setToolTip("Remove Part B report");
    header->addWidget(enabled);header->addWidget(title,1);header->addWidget(remove);box->addLayout(header);
    auto *notice=new QLabel("Static vessel data. Use the same MMSI as Part A and Position for the same simulated vessel. Signal K uses MMSI vessel contexts.",this);notice->setWordWrap(true);box->addWidget(notice);
    association=new QLabel(this);association->setObjectName("aisAssociation");association->setWordWrap(true);box->addWidget(association);
    auto *form=new QGridLayout;form->setColumnStretch(1,1);box->addLayout(form);auto *validation=new QLabel(this);validation->setWordWrap(true);validation->setObjectName("aisValidation");box->addWidget(validation);
    const QMap<QString,QString> titles{{"MessageID","AIS message ID (24)"},{"Repeat","Repeat indicator"},{"UserID","Simulated MMSI"},
        {"VesselType","Vessel type"},{"Vendor","Vendor (7 AIS chars)"},{"Callsign","Radio callsign (7 AIS chars)"},
        {"Length","Length"},{"Beam","Beam"},{"PosRefStbd","Reference from starboard"},{"PosRefBow","Reference from bow"},
        {"MothershipID","Mothership MMSI (0 none)"},{"AISInfo","Transceiver information"},{"SID","SID (255 unavailable)"}};
    for(const auto &row:parameters()) {
        const auto field=row[0];values[field]=row[1];
        auto *caption=new QLabel(titles.value(field),this);labels[field]=caption;
        caption->setObjectName("label"+field);caption->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        const int gridRow=form->rowCount();form->addWidget(caption,gridRow,0);
        QLabel *units=nullptr;
        if(dimension(field)) {
            auto *clickable=new ClickableLabel(this);units=clickable;clickable->setMenuItems(unitsListDistance);
            connect(clickable,&ClickableLabel::actionTriggered,this,&AisClassBStatic129810::ChangeDistanceUnits_triggered);
            units->setToolTip("Left-click to change the instrument's distance display units.");
        } else units=new QLabel(this);
        units->setObjectName("labelUnit"+field);form->addWidget(units,gridRow,2);
        if(field=="UserID"||field=="MothershipID")units->setText("MMSI");
        if(field=="Repeat"||field=="AISInfo"||field=="VesselType") {
            auto *combo=new QComboBox(this);combo->setObjectName(field);combo->setToolTip(fieldDescription(field));
            if(field=="VesselType")for(const auto &entry:vesselTypes())combo->addItem(entry.second,entry.first);
            else {
                const QStringList names=field=="Repeat"?QStringList{"Initial","First","Second","Final"}:QStringList{"Channel A reception","Channel B reception","Channel A transmission","Channel B transmission","Own information, not broadcast"};
                for(int n=0;n<names.size();++n)combo->addItem(names[n],n);
                if(field=="AISInfo")combo->addItem("Unavailable",31);
            }
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);combo->setMinimumContentsLength(12);
            form->addWidget(combo,gridRow,1);
            connect(combo,&QComboBox::activated,this,[this,combo,field,validation] {
                const auto text=QString::number(combo->currentData().toInt());
                if(editableField(field,text)){validation->clear();setValue(field,text);}else validation->setText("Reserved vessel types are receive/restore only; choose 0-99 or Unavailable.");
                refresh();
            });
        } else {
            auto *edit=new QLineEdit(this);edit->setObjectName(field);editors[field]=edit;edit->setReadOnly(field=="MessageID");edit->setToolTip(fieldDescription(field));form->addWidget(edit,gridRow,1);caption->setBuddy(edit);
            if(dimension(field))connect(edit,&QLineEdit::textEdited,this,[this,edit] {
                if(!edit->property("editDistanceUnit").isValid())edit->setProperty("editDistanceUnit",UserPrefs().distanceUnitIndex);
            });
            connect(edit,&QLineEdit::editingFinished,this,[this,edit,field,validation] {
                if(!edit->isModified())return;
                QString text=edit->text();
                if(dimension(field)) {
                    if(text.compare("Unavailable",Qt::CaseInsensitive)==0)text=QString::number(N2kDoubleNA,'g',17);
                    else {
                        auto prefs=UserPrefs();if(edit->property("editDistanceUnit").isValid())prefs.distanceUnitIndex=edit->property("editDistanceUnit").toUInt();
                        bool ok=false;double v=text.toDouble(&ok);
                        // An unchanged rendered edit retains its original canonical value,
                        // including a pending edit protected from later model updates.
                        if (ok && text == edit->property("dimensionDisplayText").toString()
                            && prefs.distanceUnitIndex == edit->property("dimensionDisplayUnit").toUInt())
                            v = edit->property("dimensionCanonical").toDouble();
                        else if (ok) v=N2kHelper::convertUserDistanceNearToN2kUnits(v,prefs);
                        text=ok?QString::number(v,'g',17):QString();
                    }
                } // AIS strings are validated as entered; no substitution, truncation or case rewriting.
                if(editableField(field,text)){validation->clear();setValue(field,text);}
                else validation->setText("Invalid "+field+"; previous value retained. "+fieldDescription(field));
                edit->setProperty("editDistanceUnit",QVariant());edit->setModified(false);refresh();
            });
        }
    }
    connect(this,&PgnWidgetBase::UserPrefsChanged,this,&AisClassBStatic129810::refresh);refresh();
}
void AisClassBStatic129810::setValue(const QString &field,const QString &text) {
    if(!values.contains(field)||!validField(field,text)||values[field]==text)return;
    values[field]=text;refresh();emit parameterValueChanged(field,text);
}
void AisClassBStatic129810::setAssociationText(const QString &text) {association->setText(text);}
void AisClassBStatic129810::refresh() {
    for(auto i=editors.begin();i!=editors.end();++i) {
        if(i.value()->hasFocus()&&i.value()->isModified())continue;
        QString text=values.value(i.key());
        if(dimension(i.key())) {
            const double v=text.toDouble();text=v==N2kDoubleNA?"Unavailable":QString::number(N2kHelper::convertN2kDistanceNearToUserUnits(v,UserPrefs()),'g',12);
            findChild<QLabel*>("labelUnit"+i.key())->setText(UserPrefs().distanceUnitIndex==0?"m":"ft");
            i.value()->setProperty("dimensionDisplayText",text);
            i.value()->setProperty("dimensionDisplayUnit",UserPrefs().distanceUnitIndex);
            i.value()->setProperty("dimensionCanonical",v);
        }
        const QSignalBlocker block(i.value());i.value()->setText(text);
    }
    for(const auto &field:{QString("Repeat"),QString("AISInfo"),QString("VesselType")}) {
        auto *combo=findChild<QComboBox *>(field);const QSignalBlocker block(combo);combo->setCurrentIndex(combo->findData(values.value(field).toInt()));
    }
}
void AisClassBStatic129810::setInitialGuiValues(){refresh();}
void AisClassBStatic129810::initializeUnitsLimits(){refresh();}
