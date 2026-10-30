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
#include "transportcapabilities.h"
#include <QAction>
#include <QAbstractButton>
#include <QPushButton>
#include <QLayout>
#include <QBoxLayout>
#include <QMenu>
#include "clickablelabel.h"
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "appsettings.h"
#include "windlassstatus.h"
#include "enumcomboboxdelegate.h"
#include "pgntreecontroller.h"
#include "nmeasenderworker.h"
#include "N2kMessages.h"
#include <limits>

namespace {
QMap<QString,QString> fields(int pgn) {
    QMap<QString,QString> result;for(const auto &row:WindlassStatus::parameters(pgn))result[row[0]]=row[1];return result;
}
QStringList wire(const QMap<QString,QString> &f){QStringList out;for(auto it=f.begin();it!=f.end();++it)out<<it.key()+":"+it.value();return out;}
QStringList pairs(const QMap<QString,QString> &f){QStringList out;for(auto it=f.begin();it!=f.end();++it)out<<it.key()<<it.value();return out;}
QByteArray bytes(const tN2kMsg &m){return QByteArray(reinterpret_cast<const char*>(m.Data),m.DataLen);}
QMap<QString,QString> decoded(const QStringList &f){QMap<QString,QString> result;for(int i=0;i+1<f.size();i+=2)result[f[i]]=f[i+1];return result;}
void confirm(){QTimer::singleShot(0,[]{for(auto *w:QApplication::topLevelWidgets())if(auto *b=qobject_cast<QMessageBox*>(w);b && b->button(QMessageBox::Yes))b->button(QMessageBox::Yes)->click();});}
QByteArray read(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
class MemoryCan : public SocketCanBackend {
public:
    QList<tN2kMsg> sent;
    void configure(const SocketCanConfig &,std::function<void(const tN2kMsg &)>)override{}
    void service()override{}
    bool ready()const override{return true;}
    int openingError()const override{return 0;}
    bool send(const tN2kMsg &m)override{sent.append(m);return true;}
    bool close()override{return true;}
};
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode mode)override{return QIODevice::open(mode|QIODevice::Unbuffered);}
    void close()override{QIODevice::close();}
    qint64 bytesToWrite()const override{return 0;}
protected:
    qint64 readData(char*,qint64)override{return 0;}
    qint64 writeData(const char *data,qint64 size)override{output.append(data,size);return size;}
};
class PacketCore : public tNMEA2000 {
public:
    QList<QByteArray> frames;
    PacketCore(){SetMode(N2km_SendOnly,42);OpenScheduler=tN2kScheduler(0);Open();OpenScheduler=tN2kScheduler(0);Open();}
protected:
    bool CANOpen()override{return true;}
    bool CANGetFrame(unsigned long &,unsigned char &,unsigned char *)override{return false;}
    bool CANSendFrame(unsigned long,unsigned char n,const unsigned char *data,bool)override{frames.append(QByteArray(reinterpret_cast<const char*>(data),n));return true;}
};
struct Workspace {
    MainWindow window;
    MainController *controller=window.findChild<MainController*>();
    PgnTableController *table=window.findChild<PgnTableController*>();
    PgnTableModel *model=table->getPgnTableModel();
    Workspace(){window.resize(1500,1050);window.show();}
    WindlassStatus *add(int pgn){controller->addWidgetAsComboboxTextChanged(QString::number(pgn),pgn==128777?"Windlass Operating Status":"Windlass Monitoring Status","Other");return qobject_cast<WindlassStatus*>(controller->layoutWidgets("Other").last());}
    void select(WindlassStatus *w,const QString &field={}){auto *tree=window.findChild<PgnTreeController*>();QTRY_VERIFY(tree->getTreeModel()->indexFor(w,field).isValid());QVERIFY(QMetaObject::invokeMethod(tree,"navigate",Qt::DirectConnection,Q_ARG(QModelIndex,tree->getTreeModel()->indexFor(w,field))));}
};
}
class WindlassStatusTests:public QObject {
    Q_OBJECT
private slots:
    void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/InterfaceType","None");}
    void operatingIndependentRawFixture(){
        auto f=fields(128777);f["SID"]="7";f["RodeCounterValue"]="12.34";f["WindlassLineSpeed"]="2.345";f["WindlassMotionStatus"]="1";f["RodeTypeStatus"]="1";f["AnchorDockingStatus"]="2";f["OperatingEvents"]="21";
        tN2kMsg m;QVERIFY(WindlassStatus::encode(128777,f,m));QCOMPARE(m.PGN,128777UL);QCOMPARE(m.Priority,2);QCOMPARE(m.DataLen,8);
        QCOMPARE(bytes(m).toHex(),QByteArray("0700f57b00eb0056"));
        QStringList out;QVERIFY(WindlassStatus::decode(m,out));auto d=decoded(out);QCOMPARE(d["WindlassIdentifier"],QString("0"));QCOMPARE(d["RodeCounterValue"].toDouble(),12.3);QVERIFY(qAbs(d["WindlassLineSpeed"].toDouble()-2.35)<1e-12);QCOMPARE(d["OperatingEvents"],QString("21"));
        QCOMPARE(f["RodeCounterValue"],QString("12.34"));QCOMPARE(f["WindlassLineSpeed"],QString("2.345"));
    }
    void monitoringIndependentRawFixture(){
        auto f=fields(128778);f["SID"]="8";f["WindlassIdentifier"]="2";f["TotalMotorTime"]="1234";f["ControllerVoltage"]="24.1";f["MotorCurrent"]="8.5";f["MonitoringEvents"]="5";
        tN2kMsg m;QVERIFY(WindlassStatus::encode(128778,f,m));QCOMPARE(m.Priority,2);QCOMPARE(m.DataLen,8);QCOMPARE(bytes(m).toHex(),QByteArray("08020579091500ff"));
        QStringList out;QVERIFY(WindlassStatus::decode(m,out));const auto d=decoded(out);QVERIFY(qAbs(d["ControllerVoltage"].toDouble()-24.2)<1e-12);QCOMPARE(d["MotorCurrent"].toDouble(),9.);QCOMPARE(d["TotalMotorTime"].toDouble(),1260.);QCOMPARE(d["MonitoringEvents"],QString("5"));
    }
    void unavailableZeroAndMaximum(){
        for(int pgn:{128777,128778}) {
            auto f=fields(pgn);tN2kMsg m;QVERIFY(WindlassStatus::encode(pgn,f,m));
            QCOMPARE(bytes(m).toHex(),pgn==128777?QByteArray("ff00ffffffffff03"):QByteArray("ff0000ffffffffff"));
            QStringList out;QVERIFY(WindlassStatus::decode(m,out));auto d=decoded(out);
            for(const auto &row:WindlassStatus::parameters(pgn))if(WindlassStatus::measurement(row[0]))QCOMPARE(d[row[0]].toDouble(),N2kDoubleNA);
            for(auto it=f.begin();it!=f.end();++it)it.value()="0";
            QVERIFY(WindlassStatus::encode(pgn,f,m));QVERIFY(WindlassStatus::decode(m,out));d=decoded(out);
            for(const auto &row:WindlassStatus::parameters(pgn))QCOMPARE(d[row[0]].toDouble(),0.);
        }
        const QMap<QString,QString> bounds{{"RodeCounterValue","6553.3"},{"WindlassLineSpeed","655.33"},{"ControllerVoltage","50.6"},{"MotorCurrent","253"},{"TotalMotorTime","3931980"}};
        for(int pgn:{128777,128778}){auto f=fields(pgn);for(auto it=f.begin();it!=f.end();++it)if(bounds.contains(it.key()))it.value()=bounds[it.key()];tN2kMsg m;QStringList out;QVERIFY(WindlassStatus::encode(pgn,f,m));QVERIFY(WindlassStatus::decode(m,out));}
    }
    void invalidAndReservedAreTransactional(){
        for(int pgn:{128777,128778}) {
            auto f=fields(pgn);tN2kMsg good;QVERIFY(WindlassStatus::encode(pgn,f,good));
            for(int length=0;length<8;++length){auto m=good;m.DataLen=length;QStringList out{"untouched"};QVERIFY(!WindlassStatus::decode(m,out));QCOMPARE(out,QStringList{"untouched"});}
            auto m=good;m.DataLen=9;QStringList out{"untouched"};QVERIFY(!WindlassStatus::decode(m,out));
            m=good;m.Data[pgn==128777?7:2]|=0x80;QVERIFY(!WindlassStatus::decode(m,out));QCOMPARE(out,QStringList{"untouched"});
            m=good;m.Data[pgn==128777?2:7]=0;QVERIFY(!WindlassStatus::decode(m,out));
            m=good;const int offset=pgn==128777?3:5;m.Data[offset]=0xfe;m.Data[offset+1]=0xff;QVERIFY(!WindlassStatus::decode(m,out));
            if(pgn==128778){for(int offset:{3,4}){m=good;m.Data[offset]=0xfe;QVERIFY(!WindlassStatus::decode(m,out));}}
            for(const auto &row:WindlassStatus::parameters(pgn)) {
                for(const QString invalid:{"bad","nan","inf","-1"}){auto bad=f;bad[row[0]]=invalid;QVERIFY(!WindlassStatus::encode(pgn,bad,m));}
                if(WindlassStatus::measurement(row[0])){auto bad=f;bad[row[0]]="99999999";QVERIFY(!WindlassStatus::encode(pgn,bad,m));}
            }
            f["Unexpected"]="1";QVERIFY(!WindlassStatus::encode(pgn,f,m));
        }
        QVERIFY(!WindlassStatus::validField(128777,"WindlassMotionStatus","4"));QVERIFY(!WindlassStatus::validField(128777,"OperatingEvents","32"));QVERIFY(!WindlassStatus::validField(128778,"MonitoringEvents","8"));
        QVERIFY(!WindlassStatus::validField(128777,"WindlassIdentifier","0.5"));QVERIFY(!WindlassStatus::validField(128778,"SID","256"));QVERIFY(!WindlassStatus::validField(128777,"WindlassIdentifier","1e2"));QVERIFY(!WindlassStatus::validField(128777,"OperatingEvents","1.0"));
    }
    void enumsAndEveryEventCombination(){
        for(int pgn:{128777,128778}) {
            const QString event=pgn==128777?"OperatingEvents":"MonitoringEvents";
            for(const auto &choice:WindlassStatus::choices(event)) {
                auto f=fields(pgn);f[event]=QString::number(choice.first);tN2kMsg m;QStringList out;QVERIFY(WindlassStatus::encode(pgn,f,m));QVERIFY(WindlassStatus::decode(m,out));QCOMPARE(decoded(out)[event],f[event]);
            }
        }
        for(int n=0;n<4;++n){auto f=fields(128777);for(const auto &key:{"WindlassMotionStatus","RodeTypeStatus","AnchorDockingStatus"})f[key]=QString::number(n);tN2kMsg m;QStringList out;QVERIFY(WindlassStatus::encode(128777,f,m));QVERIFY(WindlassStatus::decode(m,out));for(const auto &key:{"WindlassMotionStatus","RodeTypeStatus","AnchorDockingStatus"})QCOMPARE(decoded(out)[key],QString::number(n));}
    }
    void widgetsModelTreeAndManualIndependence(){
        Workspace w;auto *op=w.add(128777),*mon=w.add(128778);QVERIFY(op&&mon);QVERIFY(!op->isEnabled()&&!mon->isEnabled());
        op->setRodeCounterValue(12.34567);op->setWindlassLineSpeed(3.14159);op->setWindlassMotionStatus(1);mon->setControllerVoltage(24.1234);mon->setMotorCurrent(6.789);mon->setTotalMotorTime(123.456);
        QCOMPARE(w.model->findPgn(op->Id())->paramDetails[2][1].toDouble(),12.34567);QTest::qWait(180);
        QCOMPARE(op->RodeCounterValue(),12.34567);QCOMPARE(op->WindlassLineSpeed(),3.14159);QCOMPARE(mon->TotalMotorTime(),123.456);QCOMPARE(mon->MotorCurrent(),6.789);
        QVERIFY(op->findChild<QLabel*>("windlassAssociation")->text().startsWith("Matching"));op->setWindlassIdentifier(2);QVERIFY(mon->findChild<QLabel*>("windlassAssociation")->text().startsWith("No matching"));QCOMPARE(mon->WindlassIdentifier(),0);
        w.select(op,"RodeCounterValue");QCOMPARE(w.model->IdxHasFocus(),op->Id());w.model->setCurrentPgn(op->Id());
        QVERIFY(w.model->setData(w.model->index(2,1),"9.87654321"));QCOMPARE(op->RodeCounterValue(),9.87654321);
        QVERIFY(!w.model->setData(w.model->index(2,1),"-1"));QCOMPARE(op->RodeCounterValue(),9.87654321);
        QVERIFY(op->findChild<QLabel*>("windlassValidation")->text().contains("RodeCounterValue"));
        QVERIFY(w.model->setData(w.model->index(2,1),"Unavailable"));QCOMPARE(op->RodeCounterValue(),N2kDoubleNA);QVERIFY(op->findChild<QLabel*>("windlassValidation")->text().isEmpty());
        EnumComboBoxDelegate delegate;QStyleOptionViewItem option;auto *editor=delegate.createEditor(&w.window,option,w.model->index(4,1));auto *combo=qobject_cast<QComboBox*>(editor);QVERIFY(combo);QCOMPARE(combo->count(),4);combo->setCurrentIndex(2);delegate.setModelData(combo,w.model,w.model->index(4,1));QCOMPARE(op->WindlassMotionStatus(),2);delete editor;
        auto *tree=w.window.findChild<PgnTreeController*>()->getTreeModel();QTRY_VERIFY(tree->indexFor(op,"WindlassMotionStatus").isValid());QVERIFY(tree->indexFor(op,"WindlassMotionStatus").siblingAtColumn(1).data().toString().contains("Retrieving"));QVERIFY(w.model->parameterUpdatedAt(op->Id(),"RodeCounterValue").isValid());
        auto *second=w.add(128777);second->setWindlassIdentifier(7);second->setRodeCounterValue(99);QCOMPARE(op->RodeCounterValue(),N2kDoubleNA);QCOMPARE(second->RodeCounterValue(),99.);
    }
    void unitsPrecisionAndPendingEdit(){
        Workspace w;auto *op=w.add(128777);op->setRodeCounterValue(10.668);op->setWindlassLineSpeed(12.345);w.select(op);w.window.activateWindow();QApplication::setActiveWindow(&w.window);
        auto prefs=op->UserPrefs();auto *edit=op->findChild<QLineEdit*>("RodeCounterValue");
        const auto stamp=w.model->parameterUpdatedAt(op->Id(),"RodeCounterValue");
        for(int n=0;n<10;++n){prefs.distanceUnitIndex=n%3;op->setUserPrefs(prefs);QCOMPARE(op->RodeCounterValue(),10.668);QCOMPARE(op->WindlassLineSpeed(),12.345);QCOMPARE(w.model->parameterUpdatedAt(op->Id(),"RodeCounterValue"),stamp);}
        prefs.distanceUnitIndex=2;op->setUserPrefs(prefs);QCOMPARE(edit->text().toDouble(),35.);QVERIFY(qobject_cast<ClickableLabel*>(op->findChild<QLabel*>("labelUnitRodeCounterValue")));
        edit->setFocus();QTRY_VERIFY(edit->hasFocus());edit->selectAll();QTest::keyClicks(edit,"40");prefs.distanceUnitIndex=0;op->setUserPrefs(prefs);op->setSID(1);QCOMPARE(edit->text(),QString("40"));
        QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QVERIFY(qAbs(op->RodeCounterValue()-12.192)<1e-12);
        edit->setText("invalid");edit->setModified(true);QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QVERIFY(qAbs(op->RodeCounterValue()-12.192)<1e-12);QVERIFY(op->findChild<QLabel*>("windlassValidation")->text().contains("RodeCounterValue"));
        QCOMPARE(op->findChild<QLabel*>("labelUnitWindlassLineSpeed")->text(),QString("m/min"));
    }
    void generateDefaultsAndRestorationPrecedence(){
        Workspace w;auto *op=w.add(128777);
        QCOMPARE(op->WindlassMotionStatus(),int(N2kDD480_WindlassStopped));
        QCOMPARE(op->RodeTypeStatus(),int(N2kDD481_ChainPresentlyDetected));
        QCOMPARE(op->AnchorDockingStatus(),int(N2kDD482_FullyDocked));QCOMPARE(op->OperatingEvents(),0);
        QCOMPARE(op->RodeCounterValue(),N2kDoubleNA);QCOMPARE(op->WindlassLineSpeed(),N2kDoubleNA);QCOMPARE(op->SID(),255);QCOMPARE(op->WindlassIdentifier(),0);QVERIFY(!op->isEnabled());
        op->setWindlassMotionStatus(2);op->setRodeTypeStatus(1);op->setAnchorDockingStatus(0);op->setOperatingEvents(17);
        QTemporaryDir dir(QDir::currentPath()+"/build/m230/refine-layout-XXXXXX");QVERIFY(dir.isValid());auto path=dir.filePath("states.json");QVERIFY(w.controller->saveLayoutToJson(path));confirm();QVERIFY(w.controller->loadLayoutFromJson(path));
        op=w.window.findChildren<WindlassOperating128777*>().first();QCOMPARE(op->WindlassMotionStatus(),2);QCOMPARE(op->RodeTypeStatus(),1);QCOMPARE(op->AnchorDockingStatus(),0);QCOMPARE(op->OperatingEvents(),17);
        auto *fresh=w.add(128777);QCOMPARE(fresh->WindlassMotionStatus(),0);QCOMPARE(fresh->RodeTypeStatus(),0);QCOMPARE(fresh->AnchorDockingStatus(),1);QCOMPARE(fresh->OperatingEvents(),0);QCOMPARE(op->OperatingEvents(),17);
        w.controller->setMode(Viewer);auto *unknown=w.add(128777);QCOMPARE(unknown->WindlassMotionStatus(),3);QCOMPARE(unknown->RodeTypeStatus(),3);QCOMPARE(unknown->AnchorDockingStatus(),3);
        auto *handler=w.window.findChild<Nmea2000Handler*>();handler->setMode(Viewer);auto f=fields(128777);f["WindlassMotionStatus"]="1";f["RodeTypeStatus"]="2";f["AnchorDockingStatus"]="0";f["OperatingEvents"]="31";
        tN2kMsg m;QVERIFY(WindlassStatus::encode(128777,f,m));m.Source=23;handler->nmea2000MessageReceived(m);
        bool received=false;for(auto *v:w.window.findChildren<WindlassOperating128777*>())if(w.model->findPgn(v->Id())->source=="23"){received=true;QCOMPARE(v->WindlassMotionStatus(),1);QCOMPARE(v->RodeTypeStatus(),2);QCOMPARE(v->AnchorDockingStatus(),0);QCOMPARE(v->OperatingEvents(),31);}
        QVERIFY(received);QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
    void lineSpeedDistanceUnitsAndPendingInput(){
        Workspace w;auto *op=w.add(128777);op->setWindlassLineSpeed(3.048);w.select(op,"WindlassLineSpeed");w.window.activateWindow();QApplication::setActiveWindow(&w.window);
        auto prefs=op->UserPrefs();prefs.distanceUnitIndex=2;op->setUserPrefs(prefs);auto *edit=op->findChild<QLineEdit*>("WindlassLineSpeed");auto *unit=op->findChild<ClickableLabel*>("labelUnitWindlassLineSpeed");
        QVERIFY(unit);QCOMPARE(unit->text(),QString("ft/min"));QCOMPARE(edit->text().toDouble(),10.);
        const auto updated=w.model->parameterUpdatedAt(op->Id(),"WindlassLineSpeed");
        for(int i=0;i<30;++i){prefs.distanceUnitIndex=i%3;op->setUserPrefs(prefs);QCOMPARE(op->WindlassLineSpeed(),3.048);QCOMPARE(w.model->parameterUpdatedAt(op->Id(),"WindlassLineSpeed"),updated);QCOMPARE(unit->text(),prefs.distanceUnitIndex==0?QString("m/min"):QString("ft/min"));}
        bool picked=false;QTimer::singleShot(0,[&]{auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());if(menu && menu->actions().size()==3){picked=true;QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(menu->actions()[0]).center());}});
        QTest::mouseClick(unit,Qt::LeftButton);QVERIFY(picked);QCOMPARE(op->UserPrefs().distanceUnitIndex,0u);QCOMPARE(unit->text(),QString("m/min"));QCOMPARE(op->WindlassLineSpeed(),3.048);
        edit->setFocus();QTRY_VERIFY(edit->hasFocus());edit->selectAll();QTest::keyClicks(edit,"1.23456789");prefs=op->UserPrefs();prefs.distanceUnitIndex=2;op->setUserPrefs(prefs);op->setSID(9);QCOMPARE(edit->text(),QString("1.23456789"));QCOMPARE(unit->text(),QString("m/min"));
        QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QCOMPARE(op->WindlassLineSpeed(),1.23456789);QCOMPARE(unit->text(),QString("ft/min"));
        edit->selectAll();QTest::keyClicks(edit,"20");QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QVERIFY(qAbs(op->WindlassLineSpeed()-6.096)<1e-12);
        auto f=fields(128777);f["WindlassLineSpeed"]=op->value("WindlassLineSpeed");tN2kMsg wireMessage;QVERIFY(WindlassStatus::encode(128777,f,wireMessage));QCOMPARE(int(wireMessage.Data[5])+(int(wireMessage.Data[6])<<8),610);QVERIFY(qAbs(op->WindlassLineSpeed()-6.096)<1e-12);
        w.model->setCurrentPgn(op->Id());QCOMPARE(w.model->index(3,2).data().toString(),QString("m/min"));
        auto *tree=w.window.findChild<PgnTreeController*>()->getTreeModel();QTRY_VERIFY(tree->indexFor(op,"WindlassLineSpeed").isValid());QVERIFY(tree->indexFor(op,"WindlassLineSpeed").siblingAtColumn(1).data().toString().endsWith(" ft/min"));
        edit->setText("-1");edit->setModified(true);QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QVERIFY(qAbs(op->WindlassLineSpeed()-6.096)<1e-12);
        edit->setText("Unavailable");edit->setModified(true);QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QCOMPARE(op->WindlassLineSpeed(),N2kDoubleNA);QCOMPARE(edit->text(),QString("Unavailable"));
        op->setWindlassLineSpeed(0);QCOMPARE(edit->text().toDouble(),0.);QCOMPARE(unit->text(),QString("ft/min"));
        op->setWindlassLineSpeed(1.23456789);QTemporaryDir dir(QDir::currentPath()+"/build/m230/refine-speed-XXXXXX");QVERIFY(dir.isValid());auto path=dir.filePath("speed.json");QVERIFY(w.controller->saveLayoutToJson(path));confirm();QVERIFY(w.controller->loadLayoutFromJson(path));QCOMPARE(w.window.findChildren<WindlassOperating128777*>().first()->WindlassLineSpeed(),1.23456789);
    }
    void headerControlsUseTopAlignment(){
        Workspace w;auto *op=w.add(128777),*mon=w.add(128778);w.controller->addWidgetAsComboboxTextChanged("129810","AIS Static Part B","Communication");auto *ais=w.controller->layoutWidgets("Communication").last();w.select(op);QTest::qWait(60);
        for(auto *v:QList<QWidget*>{op,mon,ais}){
            auto *header=qobject_cast<QBoxLayout*>(v->layout()->itemAt(0)->layout());auto *title=v->findChild<QLabel*>("labelTitle");auto *check=v->findChild<QCheckBox*>("checkBoxEnabled");auto *close=v->findChild<QPushButton*>("pushButtonRemove");QVERIFY(header && title && check && close);
            qInfo()<<v->metaObject()->className()<<"header"<<header->geometry()<<"title"<<title->geometry()<<"checkbox"<<check->geometry()<<"close"<<close->geometry();
            QVERIFY(title->alignment()&Qt::AlignTop);QVERIFY(header->itemAt(header->indexOf(title))->alignment()&Qt::AlignTop);QVERIFY(header->itemAt(header->indexOf(check))->alignment()&Qt::AlignTop);
        }
    }
    void sourceQualifiedViewerAndMalformedPreservation(){
        Workspace w;w.controller->setMode(Viewer);auto *h=w.window.findChild<Nmea2000Handler*>();h->setMode(Viewer);
        auto receive=[&](int pgn,int id,int source,double value){auto f=fields(pgn);f["WindlassIdentifier"]=QString::number(id);f[pgn==128777?"RodeCounterValue":"MotorCurrent"]=QString::number(value);tN2kMsg m;QVERIFY(WindlassStatus::encode(pgn,f,m));m.Source=source;h->nmea2000MessageReceived(m);};
        receive(128777,0,23,12);receive(128777,1,23,25);receive(128777,0,24,99);receive(128778,0,23,6);receive(128777,0,23,13);
        QCOMPARE(w.window.findChildren<WindlassStatus*>().size(),4);
        for(auto *v:w.window.findChildren<WindlassStatus*>())if(v->Pgn()==128777 && v->WindlassIdentifier()==0 && w.model->findPgn(v->Id())->source=="23") {
            QCOMPARE(v->RodeCounterValue(),13.);QVERIFY(v->findChild<QLabel*>("windlassAssociation")->text().startsWith("Matching"));
            auto params=w.model->getPgnParams(v->Id());tN2kMsg bad;auto f=fields(128777);QVERIFY(WindlassStatus::encode(128777,f,bad));bad.Source=23;bad.DataLen=7;h->nmea2000MessageReceived(bad);QCOMPARE(w.model->getPgnParams(v->Id()),params);
            const auto before=w.model->parameterUpdatedAt(v->Id(),"RodeCounterValue");QTest::qWait(2);receive(128777,0,23,13);QVERIFY(w.model->parameterUpdatedAt(v->Id(),"RodeCounterValue")>before);
        }
        receive(128777,0,255,1);QCOMPARE(w.window.findChildren<WindlassStatus*>().size(),4);
        w.controller->setMode(Sender);h->setMode(Sender);receive(128777,9,23,123);QCOMPARE(w.window.findChildren<WindlassStatus*>().size(),4);QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
    void boundedViewerDiscovery(){
        Workspace w;w.controller->setMode(Viewer);auto *h=w.window.findChild<Nmea2000Handler*>();h->setMode(Viewer);
        for(int id=0;id<66;++id){auto f=fields(128777);f["WindlassIdentifier"]=QString::number(id);tN2kMsg m;QVERIFY(WindlassStatus::encode(128777,f,m));m.Source=23;h->nmea2000MessageReceived(m);}
        QCOMPARE(w.window.findChildren<WindlassOperating128777*>().size(),64);
        auto f=fields(128777);f["RodeCounterValue"]="33";tN2kMsg m;QVERIFY(WindlassStatus::encode(128777,f,m));m.Source=23;h->nmea2000MessageReceived(m);QCOMPARE(w.window.findChildren<WindlassOperating128777*>().first()->RodeCounterValue(),33.);
    }
    void layoutRoundTripsAndInvalidReplacement(){
        for(const QList<int> types:{QList<int>{128777},QList<int>{128778},QList<int>{128777,128778,128777,128778}}) {
            Workspace w;int identity=0;
            for(int pgn:types){auto *v=w.add(pgn);v->setWindlassIdentifier(identity++/2);if(pgn==128777){v->setRodeCounterValue(10.668);v->setWindlassLineSpeed(1.2345);v->setOperatingEvents(31);}else{v->setControllerVoltage(24.123);v->setMonitoringEvents(7);}v->setEnabled(true);w.model->setSendInterval(v->Id(),3456);}
            QTemporaryDir dir(QDir::currentPath()+"/build/m230/layout-XXXXXX");QVERIFY(dir.isValid());auto path=dir.filePath("windlass.json");QVERIFY(w.controller->saveLayoutToJson(path));const auto original=read(path);
            for(int n=0;n<3;++n){confirm();QVERIFY(w.controller->loadLayoutFromJson(path));QCOMPARE(w.window.findChildren<WindlassStatus*>().size(),types.size());QVERIFY(w.controller->saveLayoutToJson(path));}
            for(auto *v:w.window.findChildren<WindlassStatus*>()){QVERIFY(v->isEnabled());QCOMPARE(w.model->findPgn(v->Id())->sendInterval,3456.);if(v->Pgn()==128777){QCOMPARE(v->RodeCounterValue(),10.668);QCOMPARE(v->WindlassLineSpeed(),1.2345);}else QCOMPARE(v->ControllerVoltage(),24.123);}
            auto root=QJsonDocument::fromJson(original).object();auto groups=root["layouts"].toArray();
            for(int g=0;g<groups.size();++g){auto group=groups[g].toObject();auto widgets=group["widgets"].toArray();for(int i=0;i<widgets.size();++i){auto widget=widgets[i].toObject();if(widget["pgn"]!="128777"&&widget["pgn"]!="128778")continue;auto params=widget["parameters"].toArray();auto row=params[2].toArray();row[1]="-1";params[2]=row;widget["parameters"]=params;widgets[i]=widget;}group["widgets"]=widgets;groups[g]=group;}root["layouts"]=groups;
            auto invalid=dir.filePath("bad.json");QFile file(invalid);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();auto *first=w.window.findChildren<WindlassStatus*>().first();QPointer<WindlassStatus> preserved=first;QVERIFY(!w.controller->loadLayoutFromJson(invalid));QVERIFY(preserved);QCOMPARE(read(path),original);
            QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
        }
    }
    void scheduledAuthorizedMemoryOutput(){
        Workspace w;auto *op=w.add(128777),*mon=w.add(128778);op->setRodeCounterValue(12.345);mon->setControllerVoltage(24.1);
        MemoryCan backend;Nmea2000Handler h(nullptr,nullptr,&backend);h.SetInterfaceType("SocketCan");h.InitializeSocketCan({"testcan",42});QTRY_VERIFY(h.IsCanInterfaceOpen());
        NmeaSenderWorker sender;sender.setPgnTableController(w.table);connect(&sender,&NmeaSenderWorker::sendPgn,&h,&Nmea2000Handler::sendPgn);
        sender.sendNmeaMessage();QVERIFY(backend.sent.isEmpty());sender.startSending();sender.sendNmeaMessage();QVERIFY(backend.sent.isEmpty());
        op->setEnabled(true);mon->setEnabled(true);sender.sendNmeaMessage();QCOMPARE(backend.sent.size(),2);QSet<unsigned long> pgns;for(const auto &m:backend.sent)pgns.insert(m.PGN);QVERIFY(pgns.contains(128777)&&pgns.contains(128778));
        op->setRodeCounterValue(55.55);QCOMPARE(backend.sent.size(),2);sender.sendNmeaMessage();QCOMPARE(backend.sent.size(),2);QTest::qWait(1050);sender.sendNmeaMessage();QVERIFY(backend.sent.size()>=4);
        auto bad=wire(fields(128777));bad.append("SID:1");QVERIFY(!h.sendPgn(128777,bad));const int count=backend.sent.size();sender.stopSending();sender.sendNmeaMessage();QCOMPARE(backend.sent.size(),count);sender.setMode(Viewer);sender.startSending();QVERIFY(!sender.isSending());h.setMode(Viewer);QVERIFY(!h.sendPgn(128777,wire(fields(128777))));
    }
    void singleFrameAndActisenseMemoryFraming(){
        for(int pgn:{128777,128778}) {
            auto f=fields(pgn);tN2kMsg expected;QVERIFY(WindlassStatus::encode(pgn,f,expected));
            PacketCore core;QVERIFY(core.SendMsg(expected));QCOMPARE(core.frames.size(),1);QCOMPARE(core.frames.first(),bytes(expected));
            for(const QString preamble:{"0x94","0x93"}) {
                auto *port=new MemoryPort;Nmea2000Handler h(nullptr,port);h.SetInterfaceType("Actisense");QVERIFY(h.InitializeActisense("MEMORY",QSerialPort::Baud115200,preamble));
                if (!TransportCapabilities::experimentalActisenseTx) {
                    QVERIFY(!h.sendPgn(pgn,wire(f)));
                    QVERIFY(port->output.isEmpty());
                    h.ShutdownActisense();
                    continue;
                }
                QVERIFY(h.sendPgn(pgn,wire(f)));const auto framed=port->output;QCOMPARE(framed.left(2),QByteArray::fromHex("1002"));QCOMPARE(framed.right(2),QByteArray::fromHex("1003"));
                QByteArray body;for(int j=2;j<framed.size()-2;++j){body+=framed[j];if(quint8(framed[j])==16){++j;QCOMPARE(quint8(framed[j]),quint8(16));}}
                QCOMPARE(quint8(body[0]),quint8(preamble=="0x94"?0x94:0x93));QCOMPARE(int(quint8(body[3]))+(int(quint8(body[4]))<<8)+(int(quint8(body[5]))<<16),pgn);
                const int offset=preamble=="0x94"?8:13;QCOMPARE(int(quint8(body[offset-1])),8);unsigned sum=0;for(auto c:body)sum+=quint8(c);QCOMPARE(sum&255u,0u);QCOMPARE(body.mid(offset,8),bytes(expected));
                h.ShutdownActisense();const auto before=port->output;QVERIFY(!h.sendPgn(pgn,wire(f)));QCOMPARE(port->output,before);
            }
        }
    }
    void unsupportedSignalKIsExplicit(){
        Nmea2000Handler h;h.SetInterfaceType("SignalK");QSignalSpy errors(&h,&Nmea2000Handler::TransmitError);
        QVERIFY(!h.sendPgn(128777,wire(fields(128777))));QCOMPARE(errors.count(),1);QVERIFY(errors.first().first().toString().contains("unsupported"));QVERIFY(!h.sendPgn(128778,wire(fields(128778))));QCOMPARE(errors.count(),1);QVERIFY(!h.IsSignalKActive());
    }
    void nativeThemesFontsAndRemoval(){
        Workspace w;auto *op=w.add(128777),*mon=w.add(128778);w.select(op);op->setRodeCounterValue(35);mon->setControllerVoltage(24);w.controller->setNumberOfWidgetWide(2);
        for(const QString theme:{"Dark","Light"})for(int points:{9,12,16}) {
            AppSettings s(APP_COMPANY,APP_NAME);s.setValue("Preference/StyleSheetName",theme);s.setValue("Preference/FontSize",points);s.sync();w.window.HotReloadSettings();w.select(op);QTest::qWait(50);QVERIFY(op->isVisible() && mon->isVisible());
            for(auto *v:{op,mon}) {
                QCOMPARE(v->ColumnSpan(),3);auto *button=v->findChild<QPushButton*>("pushButtonRemove");QVERIFY(button);auto *frame=v->parentWidget();const auto rect=QRect(button->mapTo(frame,QPoint()),button->size());const auto margins=v->layout()->contentsMargins();QVERIFY(qAbs(rect.right()-(frame->contentsRect().right()-margins.right()))<=1);QVERIFY(qAbs(rect.top()-(frame->contentsRect().top()+margins.top()))<=1);
                for(auto *edit:v->findChildren<QLineEdit*>())QVERIFY(v->rect().contains(QRect(edit->mapTo(v,QPoint()),edit->size())));
            }
            const auto dir=qEnvironmentVariable("M230_EVIDENCE");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(w.window.grab().save(dir+"/"+theme+QString("-%1pt.png").arg(points)));}
        }
        QPointer<WindlassStatus> removed=op;confirm();op->findChild<QPushButton*>("pushButtonRemove")->click();QTRY_VERIFY(removed.isNull());QVERIFY(mon);QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile(QDir::currentPath()+"/build/m230/profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));WindlassStatusTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "windlass_status_tests.moc"
