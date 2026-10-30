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
#include <limits>
#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QInputDialog>
#include <QJsonArray>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QLocale>
#include "mainwindow.h"
#include "appsettings.h"
#include "pgntreecontroller.h"
#include "latlong129025.h"
#include "depth128267.h"
#include "N2kMessages.h"
#include "n2khelper.h"
#include "convert.h"

namespace {
struct Responses {
    QTimer timer;
    bool yes=false;
    Responses() {QObject::connect(&timer,&QTimer::timeout,[this]{
        auto *d=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!d)return;
        if(auto *box=qobject_cast<QMessageBox*>(d)){
            if(box->objectName()=="viewerEntryDialog")box->findChild<QAbstractButton*>("viewerKeepExisting")->click();
            else if(auto *button=box->button(yes ? QMessageBox::Yes : QMessageBox::No))button->click();else box->reject();
        }else if(auto *spin=d->findChild<QSpinBox*>()){spin->setValue(1);d->accept();}
        else if(d->findChild<QComboBox*>())d->accept();else d->reject();
    });timer.start(0);}
};
struct Workspace {
    Responses responses;
    MainWindow window;
    MainController *c=window.findChild<MainController*>();
    PgnTableModel *parameters=window.findChild<PgnTableController*>()->getPgnTableModel();
    PgnTreeController *tree=window.findChild<PgnTreeController*>();
    PgnTreeModel *model=tree->getTreeModel();
    PgnWidgetBase *add(const QString &pgn,const QString &group) {
        if(pgn=="-129025")c->addGeneratorWidget("Vessel Movement (PGN -129025)");
        else if(pgn=="-130311")c->addGeneratorWidget("Weather (PGN -130311)");
        else c->addWidgetAsComboboxTextChanged(pgn,pgn,group);
        QTest::qWait(25);return qobject_cast<PgnWidgetBase*>(c->layoutWidgets(group).last());
    }
    QString value(PgnWidgetBase *widget,const QString &field,int role=Qt::DisplayRole) {
        return model->data(model->indexFor(widget,field).siblingAtColumn(1),role).toString();
    }
    QByteArray snapshot() {
        QJsonArray result;for(const auto &p:parameters->getPgnList()){
            QJsonArray rows;for(const auto &row:p.paramDetails)rows.append(QJsonArray::fromStringList(row));
            result.append(QJsonObject{{"id",p.idX},{"enabled",p.enabled},{"pgn",p.pgn},{"source",p.source},{"interval",p.sendInterval},{"rows",rows}});
        }return QJsonDocument(result).toJson(QJsonDocument::Compact);
    }
};
QByteArray bytes(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray{};}
}

#include "coordinateeditor.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "enumcomboboxdelegate.h"
#include "actisensecodec.h"
#include "nmeareceiver.h"
#include "gnsspositiondata129029.h"
#include "globalpositioninggga.h"
#include "geographicpositiongll.h"
#include "recommendedmingnssrmc.h"
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode mode) override {return QIODevice::open(mode|QIODevice::Unbuffered);}
    qint64 bytesToWrite() const override {return 0;}
protected:
    qint64 writeData(const char *data,qint64 size) override {output.append(data,size);return size;}
};
namespace {
void settle(){for(int i=0;i<4;++i)QCoreApplication::sendPostedEvents(nullptr,QEvent::MetaCall);}
QString canonical(Workspace &w,PgnWidgetBase *p,const QString &field){for(const auto &row:w.parameters->getPgnParams(p->Id()))if(row[0]==field)return row[1];return {};}
QModelIndex parameter(Workspace &w,PgnWidgetBase *p,const QString &field){w.parameters->setCurrentPgn(p->Id());for(int row=0;row<w.parameters->rowCount();++row)if(w.parameters->data(w.parameters->index(row,0)).toString()==field)return w.parameters->index(row,1);return {};}
bool edit(Workspace &w,PgnWidgetBase *p,const QString &field,const QString &text){const auto index=parameter(w,p,field);return index.isValid() && w.parameters->setData(index,text);}
QStringList outbound(Workspace &w,PgnWidgetBase *p){QStringList result;for(const auto &row:w.parameters->getPgnParams(p->Id()))result<<row[0]+":"+row[1];return result;}
}
class CoordinatePrecisionTests : public QObject {
    Q_OBJECT
private slots:
    void init(){AppSettings s(APP_COMPANY,APP_NAME);s.clear();s.setValue("Nmea2000/Nmea2000Enabled",false);s.setValue("Nmea0183/Nmea0183Enabled",false);s.sync();}
    void propertyCommitAndLayout_data(){
        QTest::addColumn<QString>("pgn");QTest::addColumn<QString>("group");
        for(const auto &code:QStringList{"129025","129029","129284","GGA","RMC","GLL","-129025","-130311"})
            QTest::newRow(qPrintable(code))<<code<<QString(code.startsWith('-')?"Generate":"Navigation");
    }
    void propertyCommitAndLayout(){
        QFETCH(QString,pgn);QFETCH(QString,group);Workspace w;auto *p=w.add(pgn,group);QVERIFY(p);
        const QString lat=pgn=="129284"?"DestinationLatitude":"Latitude",lon=pgn=="129284"?"DestinationLongitude":"Longitude";
        for(const auto &text:QStringList{"43.1234567","-43.123456789","43.12345678912345","43.12345678912346","43.12345678912345"}){QVERIFY(edit(w,p,lat,text));settle();QCOMPARE(canonical(w,p,lat),text);QVERIFY2(p->property(lat.toUtf8()).toDouble()==text.toDouble(),qPrintable("Widget must retain the entered double: "+text));}
        const QString enteredLat="43.12345678912345",enteredLon="-70.98765432198765";
        QVERIFY(edit(w,p,lon,enteredLon));settle();QCOMPARE(canonical(w,p,lon),enteredLon);QCOMPARE(p->property(lon.toUtf8()).toDouble(),enteredLon.toDouble());
        for(int repeat=0;repeat<3;++repeat)for(int unit=0;unit<3;++unit){auto prefs=p->UserPrefs();prefs.latLongUnitIndex=unit;p->setUserPrefs(prefs);for(int read=0;read<10;++read){w.value(p,lat);w.value(p,lon);}QCOMPARE(canonical(w,p,lat),enteredLat);QCOMPARE(canonical(w,p,lon),enteredLon);QCOMPARE(w.parameters->data(parameter(w,p,lat),Qt::EditRole).toString(),enteredLat);}
        QTemporaryDir dir;const auto file=dir.filePath("precision.json");QVERIFY(w.c->saveLayoutToJson(file));const int id=p->Id();w.responses.yes=true;QVERIFY(w.c->loadLayoutFromJson(file));
        PgnWidgetBase *loaded=nullptr;for(auto *candidate:w.c->layoutWidgets(group))if(candidate->property("Id").toInt()==id)loaded=qobject_cast<PgnWidgetBase*>(candidate);QVERIFY(loaded);
        QCOMPARE(canonical(w,loaded,lat),enteredLat);QCOMPARE(canonical(w,loaded,lon),enteredLon);QCOMPARE(loaded->property(lat.toUtf8()).toDouble(),enteredLat.toDouble());QCOMPARE(loaded->property(lon.toUtf8()).toDouble(),enteredLon.toDouble());
        QVERIFY(!w.window.findChild<QAction*>("actionStart_Sending")->isChecked());QVERIFY(!w.window.findChild<NmeaSenderWorker*>()->isSending());
    }
    void coordinateRanges(){
        Workspace w;auto *p=w.add("129025","Navigation");QVERIFY(p);
        for(const auto &pair:QList<QPair<QString,QString>>{{"Latitude","90"},{"Latitude","-90"},{"Longitude","180"},{"Longitude","-180"},{"Latitude","0"},{"Longitude","-0.000000001"}}){QVERIFY(edit(w,p,pair.first,pair.second));QCOMPARE(canonical(w,p,pair.first),pair.second);}
        for(const auto &field:QStringList{"Latitude","Longitude"}){
            const QString before=canonical(w,p,field);QSignalSpy changes(w.parameters,&PgnTableModel::parameterChanged);
            const QString outside=field=="Latitude"?"90.000000001":"180.000000001";
            for(const auto &invalid:QStringList{outside,"-"+outside,"nan","inf","-inf","bad","","43,1234567"})QVERIFY(!edit(w,p,field,invalid));
            QCOMPARE(canonical(w,p,field),before);QCOMPARE(changes.size(),0);
            QVERIFY(edit(w,p,field,QString::number(N2kDoubleNA)));QCOMPARE(p->property(field.toUtf8()).toDouble(),N2kDoubleNA);
        }
    }
    void actualEditorLifecycle(){
        Workspace w;w.window.resize(1400,900);w.window.show();auto *p=w.add("129029","Navigation");const QString original="43.12345678912345";QVERIFY(edit(w,p,"Latitude",original));
        QVERIFY(w.c->selectLayoutWidget(p,"Latitude"));auto *table=w.window.findChild<QTableView*>("tableView");QVERIFY(table);QSignalSpy changes(w.parameters,&PgnTableModel::parameterChanged);
        for(int cycle=0;cycle<3;++cycle){
            table->setFocus();table->edit(table->currentIndex());QTest::qWait(10);auto *editor=table->viewport()->findChild<QLineEdit*>();QVERIFY(editor);QCOMPARE(editor->text(),original);QVERIFY(!editor->isModified());
            QTest::keyClick(editor,Qt::Key_Return);QTest::qWait(5);QCOMPARE(canonical(w,p,"Latitude"),original);QCOMPARE(changes.size(),0);
        }
        table->edit(table->currentIndex());QTest::qWait(10);auto *editor=table->viewport()->findChild<QLineEdit*>();QVERIFY(editor);editor->setFocus();QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"44.987654321");QVERIFY(editor->isModified());
        for(int unit=0;unit<3;++unit){auto prefs=p->UserPrefs();prefs.latLongUnitIndex=unit;p->setUserPrefs(prefs);QCOMPARE(editor->text(),QString("44.987654321"));QCOMPARE(canonical(w,p,"Latitude"),original);}
        QTest::keyClick(editor,Qt::Key_Escape);QTest::qWait(10);QCOMPARE(canonical(w,p,"Latitude"),original);QCOMPARE(changes.size(),0);
        table->edit(table->currentIndex());QTest::qWait(10);editor=table->viewport()->findChild<QLineEdit*>();QVERIFY(editor);QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(editor,"43.1234567");QTest::keyClick(editor,Qt::Key_Return);QTest::qWait(10);QCOMPARE(canonical(w,p,"Latitude"),QString("43.1234567"));QCOMPARE(changes.size(),1);
    }
    void compactAndSourcePrecision(){
        Workspace w;auto *p=w.add("129025","Navigation");QVERIFY(p);auto prefs=p->UserPrefs();prefs.latLongUnitIndex=0;p->setUserPrefs(prefs);
        auto *line=p->findChild<QLineEdit*>("lineEditLatitude");QVERIFY(line);line->clear();QTest::keyClicks(line,"43.12345678912345");settle();QVERIFY(line->hasAcceptableInput());QTest::keyClick(line,Qt::Key_Return);settle();QCOMPARE(p->property("Latitude").toDouble(),QString("43.12345678912345").toDouble());QCOMPARE(canonical(w,p,"Latitude").toDouble(),p->property("Latitude").toDouble());
        auto *g=qobject_cast<GeneratorVesselMovement*>(w.add("-129025","Generate"));QVERIFY(g);g->setEnabled(true);g->setLatitude(44.12345678912345);g->setLongitude(-70.98765432198765);settle();
        QCOMPARE(canonical(w,g,"Latitude").toDouble(),g->Latitude());QCOMPARE(canonical(w,p,"Latitude").toDouble(),g->Latitude());QCOMPARE(canonical(w,p,"Longitude").toDouble(),g->Longitude());QCOMPARE(p->property("Latitude").toDouble(),g->Latitude());
        const double adjacent=std::nextafter(g->Latitude(),std::numeric_limits<double>::infinity());g->setLatitude(adjacent);settle();
        QVERIFY(g->Latitude()==adjacent);QVERIFY(canonical(w,g,"Latitude").toDouble()==adjacent);QVERIFY(p->property("Latitude").toDouble()==adjacent);QVERIFY(canonical(w,p,"Latitude").toDouble()==adjacent);
        QSignalSpy repeated(g,&GeneratorVesselMovement::LatitudeChanged);g->setLatitude(adjacent);settle();QCOMPARE(repeated.size(),0);
        QVERIFY(edit(w,g,"Latitude","45.123456700000"));settle();QCOMPARE(canonical(w,g,"Latitude"),QString("45.123456700000"));QCOMPARE(p->property("Latitude").toDouble(),45.1234567);
    }
    void rxDoesNotRoundOnOpen(){
        Workspace w;w.window.resize(1400,900);w.window.show();w.c->setMode(Viewer);
        Nmea2000Handler handler;handler.setMode(Viewer);Nmea0183Handler serial;NmeaReceiver receiver(w.c,w.window.findChild<PgnTableController*>(),&handler,&serial);
        tN2kMsg m;SetN2kGNSS(m,1,20000,43200,43.12345678912345,-70.98765432198765,10,N2kGNSSt_GPS,N2kGNSSm_GNSSfix,8,1);m.Source=23;handler.nmea2000MessageReceived(m);settle();
        auto *p=w.window.findChild<GnssPositionData129029*>();QVERIFY(p);const auto before=w.snapshot();const auto original=canonical(w,p,"Latitude");QCOMPARE(p->Latitude(),original.toDouble());
        w.c->setMode(Sender);QVERIFY(w.c->selectLayoutWidget(p,"Latitude"));auto *table=w.window.findChild<QTableView*>("tableView");table->edit(table->currentIndex());QTest::qWait(10);auto *editor=table->viewport()->findChild<QLineEdit*>();QVERIFY(editor);QCOMPARE(editor->text(),original);QTest::keyClick(editor,Qt::Key_Return);QTest::qWait(5);QCOMPARE(w.snapshot(),before);
        QVERIFY(edit(w,p,"Latitude","43.1234567"));QCOMPARE(canonical(w,p,"Latitude"),QString("43.1234567"));
    }
    void n2kManualWire_data(){QTest::addColumn<QString>("pgn");QTest::newRow("rapid-1e-7")<<QString("129025");QTest::newRow("gnss-1e-16")<<QString("129029");}
    void n2kManualWire(){
        QFETCH(QString,pgn);Workspace w;auto *p=w.add(pgn,"Navigation");QVERIFY(p);const QString lat="43.12345678912345",lon="-70.98765432198765";QVERIFY(edit(w,p,"Latitude",lat));QVERIFY(edit(w,p,"Longitude",lon));
        auto *port=new MemoryPort;Nmea2000Handler handler(nullptr,port);handler.SetInterfaceType("Actisense");QVERIFY(handler.InitializeActisense("MEMORY",QSerialPort::Baud115200,"0x93"));QVERIFY(handler.sendPgn(pgn.toInt(),outbound(w,p)));
        ActisenseCodec::Parser parser;ActisenseCodec::Message frame;bool parsed=false;for(auto c:port->output)if(parser.feed(quint8(c),frame))parsed=true;QVERIFY(parsed);QCOMPARE(frame.pgn,pgn.toUInt());
        tN2kMsg wire;wire.SetPGN(frame.pgn);wire.DataLen=frame.length;std::copy_n(frame.data.begin(),frame.length,wire.Data);int offset=pgn=="129025"?0:7;
        const double resolution=pgn=="129025"?1e-7:1e-16;const double decodedLat=pgn=="129025"?wire.Get4ByteDouble(resolution,offset):wire.Get8ByteDouble(resolution,offset);const double decodedLon=pgn=="129025"?wire.Get4ByteDouble(resolution,offset):wire.Get8ByteDouble(resolution,offset);
        const double tolerance=pgn=="129025"?1.01e-7:2e-14;QVERIFY(qAbs(decodedLat-lat.toDouble())<=tolerance);QVERIFY(qAbs(decodedLon-lon.toDouble())<=tolerance);QVERIFY(qAbs(decodedLat-43.123457)>1e-7);QVERIFY(qAbs(decodedLon-(-70.987654))>1e-7);
        QCOMPARE(canonical(w,p,"Latitude"),lat);QCOMPARE(canonical(w,p,"Longitude"),lon);QVERIFY(w.c->layoutWidgets("Generate").isEmpty());
    }
    void serialManualWire(){
        Workspace w;auto *p=w.add("GGA","Navigation");QVERIFY(p);const QString lat="43.1234584",lon="-70.9876584";QVERIFY(edit(w,p,"Latitude",lat));QVERIFY(edit(w,p,"Longitude",lon));
        auto *port=new MemoryPort;Nmea0183Handler handler(nullptr,port);QVERIFY(handler.Initialize("MEMORY"));QVERIFY(handler.OpenDevice());QVERIFY(handler.sendSnt("GGA",outbound(w,p)));
        QVERIFY2(port->output.contains(",4307.408,N,07059.260,W,"),port->output.constData());
        QCOMPARE(canonical(w,p,"Latitude"),lat);QCOMPARE(canonical(w,p,"Longitude"),lon);QVERIFY(w.c->layoutWidgets("Generate").isEmpty());
    }
};
int main(int argc,char **argv){QTemporaryDir profile;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());QApplication app(argc,argv);app.setStyle("Fusion");app.setQuitOnLastWindowClosed(false);QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));CoordinatePrecisionTests t;return QTest::qExec(&t,argc,argv);}
#include "coordinate_precision_tests.moc"
