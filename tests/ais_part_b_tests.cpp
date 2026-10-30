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

#include <QtTest>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include "mainwindow.h"
#include "dialogsetup.h"
#include "preferences.h"
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QTabWidget>
#include "appsettings.h"
#include "aisclassbstatic129809.h"
#include "aisclassbstatic129810.h"
#include "enumcomboboxdelegate.h"
#include "pgntreecontroller.h"
#include "nmeasenderworker.h"
#include "N2kMessages.h"
namespace {
QMap<QString,QString> fields() {
    QMap<QString,QString> f;for(const auto &row:AisClassBStatic129810::parameters())f[row[0]]=row[1];
    f["UserID"]="123456789";f["VesselType"]="37";f["Vendor"]="SIMTEST";f["Callsign"]="SIM1";
    f["Length"]="12.3";f["Beam"]="4.5";f["PosRefStbd"]="1.2";f["PosRefBow"]="3.4";f["MothershipID"]="987654321";return f;
}
QStringList wireFields(const QMap<QString,QString> &f) {QStringList out;for(auto i=f.begin();i!=f.end();++i)out<<i.key()+":"+i.value();return out;}
void confirm() {QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto *b=qobject_cast<QMessageBox *>(w))b->button(QMessageBox::Yes)->click();});}
QByteArray read(const QString &p){QFile f(p);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
class MemoryCan:public SocketCanBackend {
public:
    QList<tN2kMsg> sent;
    void configure(const SocketCanConfig &,std::function<void(const tN2kMsg &)>)override{}
    void service()override{}
    bool ready()const override{return true;}
    int openingError()const override{return 0;}
    bool send(const tN2kMsg &m)override{sent.append(m);return true;}
    bool close()override{return true;}
};
struct Workspace {
    MainWindow window;
    MainController *controller=window.findChild<MainController *>();
    PgnTableController *table=window.findChild<PgnTableController *>();
    PgnTableModel *model=table->getPgnTableModel();
    Workspace(){window.resize(1500,1050);window.show();}
    AisClassBStatic129810 *addB(uint32_t id=123456789){controller->addWidgetAsComboboxTextChanged("129810","AIS Class B Static Part B","Communication");auto *b=qobject_cast<AisClassBStatic129810 *>(controller->layoutWidgets("Communication").last());b->setUserID(id);return b;}
    AisClassBStatic129809 *addA(uint32_t id=123456789){controller->addWidgetAsComboboxTextChanged("129809","AIS Class B Static Part A","Communication");auto *a=qobject_cast<AisClassBStatic129809 *>(controller->layoutWidgets("Communication").last());a->setUserID(id);return a;}
    void select(QWidget *widget){auto *tree=window.findChild<PgnTreeController *>();QTRY_VERIFY(tree->getTreeModel()->indexFor(widget).isValid());QVERIFY(QMetaObject::invokeMethod(tree,"navigate",Qt::DirectConnection,Q_ARG(QModelIndex,tree->getTreeModel()->indexFor(widget))));}
};
}
class AisPartBTests:public QObject {
    Q_OBJECT
private slots:
    void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");}
    void setupIdentityPersistenceAndCancel() {
        Workspace w;auto *d=w.window.findChild<DialogSetup *>();d->show();QTest::qWait(30);
        QVERIFY(!d->isModal());auto *type=d->findChild<QComboBox *>("vesselType");auto *call=d->findChild<QLineEdit *>("radioCallsign");
        QVERIFY(type && call);QCOMPARE(type->currentData().toInt(),0);QVERIFY(type->itemText(type->findData(37)).contains("Pleasure"));
        type->setCurrentIndex(type->findData(37));call->setText("SIM1234");
        QSignalSpy saved(d,&DialogSetup::ReloadSettings);QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));QCOMPARE(saved.count(),1);
        AppSettings settings(APP_COMPANY,APP_NAME);QCOMPARE(settings.value("Vessel/VesselType").toInt(),37);QCOMPARE(settings.value("Vessel/RadioCallsign").toString(),QString("SIM1234"));
        for(const auto &invalid:{QString("TOOLONG8"),QString("lower"),QString("BAD@ID"),QString("A")+QChar(0xe9)}) {
            call->setText(invalid);QCOMPARE(call->text(),invalid); // No silent truncation or normalization.
            QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));QCOMPARE(saved.count(),1);
            QVERIFY(d->findChild<QLabel *>("credentialStorageStatus")->text().contains("Vessel identity"));
            QCOMPARE(settings.value("Vessel/RadioCallsign").toString(),QString("SIM1234"));
        }
        call->setText("SIM2");type->setCurrentIndex(type->findData(150));QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));QCOMPARE(saved.count(),1);
        d->reject();d->show();QTest::qWait(10);QCOMPARE(call->text(),QString("SIM1234"));QCOMPARE(type->currentData().toInt(),37);
        call->setText("DRAFT");type->setCurrentIndex(type->findData(36));QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonLoad_clicked"));QCOMPARE(call->text(),QString("SIM1234"));QCOMPARE(type->currentData().toInt(),37);
        type->setCurrentIndex(type->findData(0));call->clear();QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));QCOMPARE(saved.count(),2);
        QCOMPARE(settings.value("Vessel/RadioCallsign").toString(),QString());d->reject();
        DialogSetup reopened;reopened.show();QTest::qWait(10);QCOMPARE(reopened.findChild<QComboBox *>("vesselType")->currentData().toInt(),0);QVERIFY(reopened.findChild<QLineEdit *>("radioCallsign")->text().isEmpty());reopened.reject();
    }
    void vesselDefaultsAndIndependentRestoration() {
        AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Vessel/VesselType",37);s.setValue("Vessel/RadioCallsign","SIM1");s.setValue("Vessel/Length",12.3);s.setValue("Vessel/Beam",4.5);s.sync();
        Workspace w;auto *b=w.addB();QCOMPARE(b->VesselType(),37);QCOMPARE(b->Callsign(),QString("SIM1"));QCOMPARE(b->Length(),12.3);QCOMPARE(b->Beam(),4.5);
        QCOMPARE(b->PosRefStbd(),N2kDoubleNA);QCOMPARE(b->PosRefBow(),N2kDoubleNA);QVERIFY(!b->isEnabled());
        b->setCallsign("OTHER");b->setLength(20.1);b->setVesselType(36);
        auto *d=w.window.findChild<DialogSetup *>();d->show();QTest::qWait(10);
        d->findChild<QComboBox *>("vesselType")->setCurrentIndex(30);d->findChild<QLineEdit *>("radioCallsign")->setText("SIM2");
        auto *units=d->findChild<QComboBox *>("comboBoxUnitsDistance");units->setCurrentIndex(1);
        d->findChild<QDoubleSpinBox *>("doubleSpinBoxLength")->setValue(40); // Preserve 12.192 m; only the wire representation rounds to 12.2.
        d->findChild<QDoubleSpinBox *>("doubleSpinBoxBeam")->setValue(4.5/0.3048);
        QVERIFY(QMetaObject::invokeMethod(d,"on_pushButtonSave_clicked"));d->reject();
        QCOMPARE(b->Callsign(),QString("OTHER"));QCOMPARE(b->Length(),20.1);QCOMPARE(b->VesselType(),36);
        auto *second=w.addB(987654321);QCOMPARE(second->Callsign(),QString("SIM2"));QCOMPARE(second->VesselType(),30);QVERIFY(qAbs(second->Length()-12.192)<1e-12);QVERIFY(qAbs(second->Beam()-4.5)<1e-9);
        QVERIFY(qAbs(s.value("Vessel/Length").toDouble()-12.192)<1e-9);
        QTemporaryDir dir;const auto path=dir.filePath("independent.json");QVERIFY(w.controller->saveLayoutToJson(path));
        s.setValue("Vessel/VesselType",70);s.setValue("Vessel/RadioCallsign","NEW");s.setValue("Vessel/Length",99.9);s.sync();w.window.HotReloadSettings();
        confirm();QVERIFY(w.controller->loadLayoutFromJson(path));
        const auto loaded=w.window.findChildren<AisClassBStatic129810 *>();QCOMPARE(loaded.size(),2);
        for(auto *v:loaded)if(v->UserID()==123456789u){QCOMPARE(v->Callsign(),QString("OTHER"));QCOMPARE(v->Length(),20.1);QCOMPARE(v->VesselType(),36);}else{QCOMPARE(v->Callsign(),QString("SIM2"));QVERIFY(qAbs(v->Length()-12.192)<1e-12);}
        auto *fresh=w.addB(111222333);QCOMPARE(fresh->Callsign(),QString("NEW"));QCOMPARE(fresh->Length(),99.9);QVERIFY(!fresh->isEnabled());
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void unrepresentableSetupDefaults() {
        AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Vessel/Length",6553.4);s.setValue("Vessel/Beam",6553.31);s.setValue("Vessel/RadioCallsign","INVALID8");s.setValue("Vessel/VesselType",150);s.sync();
        Workspace w;auto *b=w.addB(0);QCOMPARE(b->UserID(),0u);QCOMPARE(b->VesselType(),0);QVERIFY(b->Callsign().isEmpty());QCOMPARE(b->Length(),N2kDoubleNA);QCOMPARE(b->Beam(),N2kDoubleNA);
        QCOMPARE(s.value("Vessel/Beam").toDouble(),6553.31);QCOMPARE(s.value("Vessel/RadioCallsign").toString(),QString("INVALID8"));
    }
    void responsiveAisSpans() {
        Workspace w;auto *a=w.addA();auto *b=w.addB();w.controller->setNumberOfWidgetWide(2);w.select(b);
        QCOMPARE(a->ColumnSpan(),3);QCOMPARE(b->ColumnSpan(),3);
        for(const QString theme:{"Dark","Light"})for(int points:{9,12,16}) {
            AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.sync();w.window.HotReloadSettings();
            QFont font=w.window.font();font.setPointSize(points);a->setFont(font);b->setFont(font);QTest::qWait(40);
            QVERIFY2(qAbs(a->width()-b->width())<=1,qPrintable(QString("%1 %2 pt: %3 vs %4").arg(theme).arg(points).arg(a->width()).arg(b->width())));
            for(auto *widget:{static_cast<QWidget *>(a),static_cast<QWidget *>(b)})for(auto *label:widget->findChildren<QLabel *>())
                if(!label->wordWrap() && !label->text().isEmpty())QVERIFY(label->width()>=label->fontMetrics().horizontalAdvance(label->text()));
        }
        a->setColumnSpan(2);w.controller->setNumberOfWidgetWide(3);QTest::qWait(20);QCOMPARE(a->ColumnSpan(),2);QCOMPARE(b->ColumnSpan(),3);
        w.controller->setMode(Viewer);w.controller->setMode(Sender);QCOMPARE(a->ColumnSpan(),2);QCOMPARE(b->ColumnSpan(),3);
    }
    void measurementPrecisionAtEncodingBoundary_data() {
        QTest::addColumn<double>("length");QTest::addColumn<double>("beam");QTest::addColumn<int>("lengthRaw");QTest::addColumn<int>("beamRaw");
        QTest::newRow("35ft-12ft") << 35*.3048 << 12*.3048 << 107 << 37;
        QTest::newRow("40ft") << 40*.3048 << 12*.3048 << 122 << 37;
        QTest::newRow("1.23m") << 1.23 << 1.26 << 12 << 13;
        QTest::newRow("nearest-half-up") << 1.25 << 1.24 << 13 << 12;
        QTest::newRow("decimal-half") << 1.15 << 2.35 << 12 << 24;
        QTest::newRow("below-half") << 1.149999 << 2.349999 << 11 << 23;
        QTest::newRow("limits") << 6553.3 << 0.0 << 65533 << 0;
    }
    void measurementPrecisionAtEncodingBoundary() {
        QFETCH(double,length);QFETCH(double,beam);QFETCH(int,lengthRaw);QFETCH(int,beamRaw);
        AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Vessel/Length",length);settings.setValue("Vessel/Beam",beam);settings.sync();
        Workspace w;auto *b=w.addB();QCOMPARE(b->Length(),length);QCOMPARE(b->Beam(),beam);w.select(b);
        b->setPosRefStbd(length);b->setPosRefBow(beam);
        auto *edit=b->findChild<QLineEdit *>("Length");auto prefs=b->UserPrefs();
        // Display/navigation never quantizes or changes the model. Committing the unchanged
        // rounded display text must not accumulate conversion/display precision loss either.
        for(int n=0;n<12;++n) {
            prefs.distanceUnitIndex=n%3;b->setUserPrefs(prefs);edit->setModified(true);
            QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QCOMPARE(b->Length(),length);
        }
        auto f=fields();for(const auto &row:AisClassBStatic129810::parameters())f[row[0]]=b->value(row[0]);tN2kMsg m;QVERIFY(AisClassBStatic129810::encode(f,m));
        auto raw=[&m](int offset){return int(m.Data[offset])+(int(m.Data[offset+1])<<8);};
        QCOMPARE(raw(20),lengthRaw);QCOMPARE(raw(22),beamRaw);QCOMPARE(raw(24),lengthRaw);QCOMPARE(raw(26),beamRaw);
        QCOMPARE(b->Length(),length);QCOMPARE(b->Beam(),beam); // Encoding has no model side effects.
        QStringList decoded;QVERIFY(AisClassBStatic129810::decode(m,decoded));QVERIFY(qAbs(decoded[13].toDouble()-lengthRaw*.1)<1e-10);
        QTemporaryDir dir;const auto file=dir.filePath("precise.json");
        for(int n=0;n<5;++n){QVERIFY(w.controller->saveLayoutToJson(file));confirm();QVERIFY(w.controller->loadLayoutFromJson(file));b=w.window.findChildren<AisClassBStatic129810 *>().first();QCOMPARE(b->Length(),length);QCOMPARE(b->Beam(),beam);}
        prefs.distanceUnitIndex=0;b->setUserPrefs(prefs);edit=b->findChild<QLineEdit *>("Length");edit->setText("1.23");edit->setModified(true);QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QCOMPARE(b->Length(),1.23);
        prefs.distanceUnitIndex=1;b->setUserPrefs(prefs);edit->setText("35");edit->setModified(true);QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QCOMPARE(b->Length(),35*.3048);
        QVERIFY(!b->isEnabled());QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void independentBytes() {
        auto f=fields();f["Repeat"]="2";f["AISInfo"]="1";f["SID"]="7";tN2kMsg m;
        QVERIFY(AisClassBStatic129810::encode(f,m));QCOMPARE(m.PGN,129810UL);QCOMPARE(m.DataLen,35);QCOMPARE(m.Priority,6);
        const auto expected=QByteArray::fromHex("9815cd5b0725")+QByteArray("SIMTESTSIM1@@@")+QByteArray::fromHex("7b002d000c002200b168de3a03e107");
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(m.Data),m.DataLen),expected);
        uint8_t messageId,type,sid;uint32_t id,mother;char vendor[8],callsign[8];double len,beam,stbd,bow;tN2kAISRepeat repeat;tN2kAISTransceiverInformation info;
        QVERIFY(ParseN2kPGN129810(m,messageId,repeat,id,type,vendor,sizeof(vendor),callsign,sizeof(callsign),len,beam,stbd,bow,mother,info,sid));
        QCOMPARE(messageId,24);QCOMPARE(int(repeat),2);QCOMPARE(id,123456789u);QCOMPARE(type,37);QCOMPARE(QString::fromLatin1(vendor),QString("SIMTEST"));
        QVERIFY(qAbs(len-12.3)<1e-10);QVERIFY(qAbs(beam-4.5)<1e-10);QCOMPARE(mother,987654321u);QCOMPARE(sid,7);
        QStringList out;QVERIFY(AisClassBStatic129810::decode(m,out));QCOMPARE(out[21],QString("987654321"));
    }
    void invalidFields_data() {
        QTest::addColumn<QString>("field");QTest::addColumn<QString>("value");
        const QList<QPair<QString,QString>> invalid{{"MessageID","23"},{"Repeat","4"},{"UserID","123"},{"UserID","0123456789"},{"UserID","1000000000"},
            {"Vendor","TOOLONG8"},{"Callsign","BAD@ID"},{"Callsign","lower"},{"Vendor",QString("A")+QChar(0xe9)},
            {"Length","-1"},{"Length","6553.31"},{"Beam","6553.4"},{"PosRefBow","nan"},{"PosRefStbd","inf"},
            {"MothershipID","12"},{"AISInfo","5"},{"SID","256"},{"VesselType","256"}};
        int n=0;for(const auto &entry:invalid)QTest::newRow(qPrintable(QString::number(n++)))<<entry.first<<entry.second;
    }
    void invalidFields() {
        QFETCH(QString,field);QFETCH(QString,value);auto f=fields();f[field]=value;tN2kMsg m;
        QVERIFY(!AisClassBStatic129810::validField(field,value));QVERIFY(!AisClassBStatic129810::encode(f,m));
        AisClassBStatic129810 b(1,"Test");const auto old=b.value(field);b.setValue(field,value);QCOMPARE(b.value(field),old);
    }
    void boundariesUnavailableAndReserved() {
        auto f=fields();tN2kMsg m;QStringList out;
        f["Length"]="6553.3";f["Beam"]="0";f["Vendor"]="";f["Callsign"]="";f["VesselType"]="255";f["AISInfo"]="31";f["MothershipID"]="4294967295";
        QVERIFY(AisClassBStatic129810::encode(f,m));QCOMPARE(m.Data[20],0xfd);QCOMPARE(m.Data[21],0xff);QCOMPARE(m.Data[33],0xff);
        QVERIFY(AisClassBStatic129810::decode(m,out));QCOMPARE(out[9],QString());QCOMPARE(out[11],QString());QCOMPARE(out[21],QString("4294967295"));
        f["Length"]=QString::number(N2kDoubleNA,'g',17);QVERIFY(AisClassBStatic129810::encode(f,m));QCOMPARE(m.Data[20],0xff);QVERIFY(AisClassBStatic129810::decode(m,out));QCOMPARE(out[13].toDouble(),N2kDoubleNA);
        m.Data[5]=150;QVERIFY(AisClassBStatic129810::decode(m,out));QCOMPARE(out[7],QString("150"));
        f["VesselType"]="150";QVERIFY(AisClassBStatic129810::validField("VesselType","150"));QVERIFY(!AisClassBStatic129810::editableField("VesselType","150"));QVERIFY(!AisClassBStatic129810::encode(f,m));
        f["VesselType"]="0";for(const auto &id:{QString("0"),QString("4294967295")}){f["UserID"]=id;QVERIFY(!AisClassBStatic129810::encode(f,m));}
    }
    void malformedPackets() {
        tN2kMsg m;QVERIFY(AisClassBStatic129810::encode(fields(),m));const auto original=m;QStringList out{"preserved"};
        for(int n:{0,1,20,28,32,33,34,36}){m=original;m.DataLen=n;QVERIFY(!AisClassBStatic129810::decode(m,out));QCOMPARE(out,QStringList{"preserved"});}
        m=original;m.PGN=129809;QVERIFY(!AisClassBStatic129810::decode(m,out));
        m=original;m.Data[0]=23;QVERIFY(!AisClassBStatic129810::decode(m,out));
        m=original;m.Data[32]&=0xfc;QVERIFY(!AisClassBStatic129810::decode(m,out));
        m=original;m.Data[33]=1;QVERIFY(!AisClassBStatic129810::decode(m,out));
        m=original;m.Data[33]=0xe5;QVERIFY(!AisClassBStatic129810::decode(m,out));
        m=original;m.Data[20]=0xfe;m.Data[21]=0xff;QVERIFY(!AisClassBStatic129810::decode(m,out));
        m=original;m.Data[6]=0x7f;QVERIFY(!AisClassBStatic129810::decode(m,out));
    }
    void defaultsAndModel() {
        Workspace w;auto *b=w.addB(0);QVERIFY(!b->isEnabled());QVERIFY(!w.model->findPgn(b->Id())->enabled);QCOMPARE(b->Length(),MyVessel().Length);QCOMPARE(b->Beam(),MyVessel().Beam);QCOMPARE(b->MothershipID(),0u);
        w.model->setCurrentPgn(b->Id());QVERIFY(w.model->setData(w.model->index(2,1),"123456789"));QCOMPARE(b->UserID(),123456789u);
        QVERIFY(w.model->setData(w.model->index(6,1),"12.3"));QVERIFY(qAbs(b->Length()-12.3)<1e-10);QVERIFY(w.model->setData(w.model->index(6,1),"12.34"));QCOMPARE(b->Length(),12.34);
        QVERIFY(w.model->setData(w.model->index(6,1),"Unavailable"));QCOMPARE(b->Length(),N2kDoubleNA);QCOMPARE(w.model->index(6,1).data().toString(),QString("Unavailable"));
        QVERIFY(!w.model->setData(w.model->index(3,1),"150"));
        EnumComboBoxDelegate delegate;QStyleOptionViewItem option;auto *editor=delegate.createEditor(&w.window,option,w.model->index(3,1));auto *combo=qobject_cast<QComboBox *>(editor);QVERIFY(combo);
        combo->setCurrentIndex(combo->findData(37));delegate.setModelData(combo,w.model,w.model->index(3,1));QCOMPARE(b->VesselType(),37);delete editor;
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void strictTextEntryAndReceivedUnavailable() {
        Workspace w;auto *b=w.addB();b->setVendor("SIMTEST");w.select(b);
        auto *edit=b->findChild<QLineEdit *>("Vendor");edit->setText("lower");edit->setModified(true);
        QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QCOMPARE(b->Vendor(),QString("SIMTEST"));
        QVERIFY(b->findChild<QLabel *>("aisValidation")->text().contains("Vendor"));
        tN2kMsg m;SetN2kPGN129810(m,24,N2kaisr_Final,0,0,"","",N2kDoubleNA,N2kDoubleNA,N2kDoubleNA,N2kDoubleNA,0xffffffffu,N2kaisown_information_not_broadcast,255);
        QStringList out;QVERIFY(AisClassBStatic129810::decode(m,out));QCOMPARE(out[5],QString("0"));QCOMPARE(out[21],QString("4294967295"));QCOMPARE(out[13].toDouble(),N2kDoubleNA);
    }
    void preferredUnitsAndPendingEdit() {
        Workspace w;auto *b=w.addB();b->setLength(12.3);b->setMothershipID(987654321);auto prefs=b->UserPrefs();prefs.distanceUnitIndex=0;b->setUserPrefs(prefs);w.select(b);
        auto *edit=b->findChild<QLineEdit *>("Length");QCOMPARE(edit->text().toDouble(),12.3);
        auto *tree=w.window.findChild<PgnTreeController *>()->getTreeModel();QTRY_VERIFY(tree->indexFor(b,"Length").isValid());QVERIFY(tree->indexFor(b,"Length").siblingAtColumn(1).data().toString().endsWith(" m"));
        QCOMPARE(tree->indexFor(b,"MothershipID").siblingAtColumn(1).data().toString(),QString("987654321"));
        const auto before=w.model->parameterUpdatedAt(b->Id(),"Length");
        prefs.distanceUnitIndex=2;b->setUserPrefs(prefs);QCOMPARE(w.model->parameterUpdatedAt(b->Id(),"Length"),before);QVERIFY(qAbs(edit->text().toDouble()-12.3/.3048)<1e-8);QVERIFY(tree->indexFor(b,"Length").siblingAtColumn(1).data().toString().endsWith(" ft"));
        edit->setFocus();QTRY_VERIFY(edit->hasFocus());edit->selectAll();QTest::keyClicks(edit,"40.3543307086614");
        prefs.distanceUnitIndex=0;b->setUserPrefs(prefs);b->setSID(9);QCOMPARE(edit->text(),QString("40.3543307086614"));
        QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QVERIFY(qAbs(b->Length()-12.3)<1e-8);
        QTRY_VERIFY(w.model->parameterUpdatedAt(b->Id(),"SID").isValid());
        // The maximum wire value must survive a preferred-feet display/edit round trip.
        prefs.distanceUnitIndex=2;b->setUserPrefs(prefs);b->setLength(6553.3);
        edit->setModified(true);b->setLength(4.5); // Authoritative update must not replace the unfinished maximum-value edit.
        QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QCOMPARE(b->Length(),6553.3);
        QVERIFY(b->findChild<QLabel *>("aisValidation")->text().isEmpty());
    }
    void coexistenceAndAssociation() {
        Workspace w;auto *a1=w.addA(),*a2=w.addA(987654321);auto *b1=w.addB(),*b2=w.addB(987654321);
        a1->setName("SIM ONE");a2->setName("SIM TWO");b1->setCallsign("SIM1");b2->setCallsign("SIM2");b1->setEnabled(true);
        QVERIFY(b1->findChild<QLabel *>("aisAssociation")->text().startsWith("Matching Part A"));QVERIFY(b2->findChild<QLabel *>("aisAssociation")->text().startsWith("Matching Part A"));
        b1->setUserID(111222333);QVERIFY(b1->findChild<QLabel *>("aisAssociation")->text().startsWith("Standalone"));QCOMPARE(a1->UserID(),123456789u);QCOMPARE(b2->Callsign(),QString("SIM2"));QVERIFY(!b2->isEnabled());
        b1->setUserID(123456789);a1->setUserID(444555666);QVERIFY(b1->findChild<QLabel *>("aisAssociation")->text().startsWith("Standalone"));a1->setUserID(123456789);
        QTemporaryDir dir;const auto path=dir.filePath("two-vessels.json");w.model->setSendInterval(b1->Id(),40000);QVERIFY(w.controller->saveLayoutToJson(path));confirm();QVERIFY(w.controller->loadLayoutFromJson(path));
        QCOMPARE(w.window.findChildren<AisClassBStatic129809 *>().size(),2);QCOMPARE(w.window.findChildren<AisClassBStatic129810 *>().size(),2);
        for(auto *b:w.window.findChildren<AisClassBStatic129810 *>()) {
            QVERIFY(b->findChild<QLabel *>("aisAssociation")->text().startsWith("Matching Part A"));
            if(b->UserID()==123456789){QCOMPARE(b->Callsign(),QString("SIM1"));QVERIFY(b->isEnabled());QCOMPARE(w.model->findPgn(b->Id())->sendInterval,40000.);}
            else{QCOMPARE(b->UserID(),987654321u);QCOMPARE(b->Callsign(),QString("SIM2"));QVERIFY(!b->isEnabled());}
        }
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void receiveMatching() {
        Workspace w;w.controller->setMode(Viewer);auto *handler=w.window.findChild<Nmea2000Handler *>();handler->setMode(Viewer);
        auto receive=[&](uint32_t id,int source,const char *callsign){auto f=fields();f["UserID"]=QString::number(id);f["Callsign"]=QString::fromLatin1(callsign);tN2kMsg m;QVERIFY(AisClassBStatic129810::encode(f,m));m.Source=source;handler->nmea2000MessageReceived(m);};
        receive(123456789,23,"SIM1");receive(987654321,23,"SIM2");receive(123456789,24,"RELAY");receive(123456789,23,"UPDATED");
        QTRY_COMPARE(w.window.findChildren<AisClassBStatic129810 *>().size(),3);
        tN2kMsg a;SetN2kPGN129809(a,24,N2kaisr_Initial,123456789,"SIM ONE",N2kaischannel_A_VDL_reception,255);a.Source=23;handler->nmea2000MessageReceived(a);
        QTRY_COMPARE(w.window.findChildren<AisClassBStatic129809 *>().size(),1);
        for(auto *b:w.window.findChildren<AisClassBStatic129810 *>()) {
            const auto source=w.model->findPgn(b->Id())->source;
            if(b->UserID()==123456789 && source=="23"){QCOMPARE(b->Callsign(),QString("UPDATED"));QVERIFY(b->findChild<QLabel *>("aisAssociation")->text().startsWith("Matching Part A"));}
            if(source=="24"){
                QCOMPARE(b->Callsign(),QString("RELAY"));
                const auto association=b->findChild<QLabel *>("aisAssociation")->text();
                QVERIFY(association.startsWith("Matching Part A"));QVERIFY(association.contains("RX 23"));
            }
        }
        // Valid Layouts restore fields by name, so a different saved row order cannot create a new target.
        for(auto &data:w.model->getPgnList())if(data.pgn=="129810")data.paramDetails.swapItemsAt(0,2);
        receive(123456789,23,"NAMED");QCOMPARE(w.window.findChildren<AisClassBStatic129810 *>().size(),3);
        for(auto *b:w.window.findChildren<AisClassBStatic129810 *>())
            if(b->UserID()==123456789 && w.model->findPgn(b->Id())->source=="23")QCOMPARE(b->Callsign(),QString("NAMED"));
        auto *first=w.window.findChildren<AisClassBStatic129810 *>().first();const auto savedId=first->Id();
        w.controller->setMode(Sender);handler->setMode(Sender);receive(111222333,25,"IGNORED");QCOMPARE(w.window.findChildren<AisClassBStatic129810 *>().size(),3);
        QVERIFY(w.model->findPgn(savedId));QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void layoutAllFieldsAndTransactionalFailure() {
        Workspace w;auto *b=w.addB();
        const auto f=fields();for(auto i=f.begin();i!=f.end();++i)b->setValue(i.key(),i.value());
        QTemporaryDir dir;const auto path=dir.filePath("full.json");QVERIFY(w.controller->saveLayoutToJson(path));const auto original=read(path);
        confirm();QVERIFY(w.controller->loadLayoutFromJson(path));b=w.window.findChildren<AisClassBStatic129810 *>().first();for(auto i=f.begin();i!=f.end();++i)QCOMPARE(b->value(i.key()),i.value());
        auto root=QJsonDocument::fromJson(original).object();auto groups=root["layouts"].toArray();
        for(int g=0;g<groups.size();++g){auto group=groups[g].toObject();auto widgets=group["widgets"].toArray();for(int n=0;n<widgets.size();++n){auto widget=widgets[n].toObject();if(widget["pgn"]!="129810")continue;auto parameters=widget["parameters"].toArray();auto row=parameters[6].toArray();row[1]="-1";parameters[6]=row;widget["parameters"]=parameters;widgets[n]=widget;}group["widgets"]=widgets;groups[g]=group;}root["layouts"]=groups;
        const auto invalid=dir.filePath("invalid.json");QFile file(invalid);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();
        QPointer<AisClassBStatic129810> previous=b;QVERIFY(!w.controller->loadLayoutFromJson(invalid));QVERIFY(previous);QCOMPARE(read(path),original);QCOMPARE(b->Callsign(),QString("SIM1"));
    }
    void authorizedMemoryOutput() {
        Workspace w;auto *a=w.addA();a->setName("SIM ONE");auto *b=w.addB();b->setCallsign("SIM1");
        MemoryCan backend;Nmea2000Handler handler(nullptr,nullptr,&backend);handler.SetInterfaceType("SocketCan");handler.InitializeSocketCan({"testcan",42});QTRY_VERIFY(handler.IsCanInterfaceOpen());
        NmeaSenderWorker sender;sender.setPgnTableController(w.table);connect(&sender,&NmeaSenderWorker::sendPgn,&handler,&Nmea2000Handler::sendPgn);
        sender.sendNmeaMessage();QVERIFY(backend.sent.isEmpty());sender.startSending();sender.sendNmeaMessage();QVERIFY(backend.sent.isEmpty());
        a->setEnabled(true);b->setEnabled(true);sender.sendNmeaMessage();QCOMPARE(backend.sent.size(),2);QSet<unsigned long> pgns;for(const auto &m:backend.sent)pgns.insert(m.PGN);QVERIFY(pgns.contains(129809)&&pgns.contains(129810));
        auto bad=wireFields(fields());bad.append("UserID:987654321");QVERIFY(!handler.sendPgn(129810,bad));QCOMPARE(backend.sent.size(),2);
        sender.stopSending();sender.sendNmeaMessage();QCOMPARE(backend.sent.size(),2);sender.setMode(Viewer);sender.startSending();QVERIFY(!sender.isSending());handler.setMode(Viewer);QVERIFY(!handler.sendPgn(129810,wireFields(fields())));
    }
    void nativeSignalKBoundary() {
        Nmea2000Handler handler;handler.SetInterfaceType("SignalK");
        QVERIFY(!handler.sendPgn(129810,wireFields(fields())));QVERIFY(!handler.IsSignalKActive());
    }
    void renderThemes() {
        Workspace w;auto *a=w.addA();a->setName("SIM ONE");auto *b=w.addB();b->setVendor("SIMTEST");b->setCallsign("SIM1");b->setLength(12.3);b->setBeam(4.5);
        for(const QString theme:{"Dark","Light"}) {
            AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.sync();w.window.HotReloadSettings();w.select(b);QTest::qWait(80);QVERIFY(b->isVisible());
            for(auto *edit:b->findChildren<QLineEdit *>())QVERIFY(b->rect().contains(QRect(edit->mapTo(b,QPoint()),edit->size())));
            const auto dir=qEnvironmentVariable("M229C_EVIDENCE");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(w.window.grab().save(dir+"/"+theme+"-ais-part-b.png"));
                auto *setup=w.window.findChild<DialogSetup *>();setup->show();QTest::qWait(30);QVERIFY(setup->grab().save(dir+"/"+theme+"-vessel-setup.png"));setup->reject();}
        }
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile(QDir::currentPath()+"/m229c-profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    AisPartBTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "ais_part_b_tests.moc"
