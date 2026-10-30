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
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkProxy>
#include "mainwindow.h"
#include "appsettings.h"
#include "aisclassbstatic129809.h"
#include "pgntreecontroller.h"
#include "nmeasenderworker.h"
#include "N2kMessages.h"
#include "enumcomboboxdelegate.h"
#include "nmea2000handler.h"
namespace {
QMap<QString, QString> fields() {
    QMap<QString, QString> out;
    for (const auto &row : AisClassBStatic129809::parameters()) out[row[0]]=row[1];
    out["UserID"]="123456789"; out["Name"]="SIM TEST: PART A"; return out;
}
QStringList sendFields(const QMap<QString, QString> &fields) {
    QStringList out; for (auto i=fields.begin(); i!=fields.end(); ++i) out << i.key()+":"+i.value(); return out;
}
void confirm() {
    QTimer::singleShot(0, [] { for (auto *w : QApplication::topLevelWidgets())
        if (auto *box=qobject_cast<QMessageBox *>(w)) box->button(QMessageBox::Yes)->click(); });
}
struct Workspace {
    MainWindow window;
    MainController *controller=window.findChild<MainController *>();
    PgnTableController *table=window.findChild<PgnTableController *>();
    PgnTableModel *model=table->getPgnTableModel();
    Workspace() { window.resize(1400,950); window.show(); }
    AisClassBStatic129809 *add() {
        controller->addWidgetAsComboboxTextChanged("129809","AIS Class B Static Part A","Communication");
        return qobject_cast<AisClassBStatic129809 *>(controller->layoutWidgets("Communication").last());
    }
};
class MemoryCan : public SocketCanBackend {
public:
    QList<tN2kMsg> sent;
    std::function<void(const tN2kMsg &)> received;
    void configure(const SocketCanConfig &, std::function<void(const tN2kMsg &)> cb) override {received=cb;}
    void service() override {}
    bool ready() const override {return true;}
    int openingError() const override {return 0;}
    bool send(const tN2kMsg &m) override {sent.append(m);return true;}
    bool close() override {received={};return true;}
};
}
class AisStaticTests : public QObject {
    Q_OBJECT
private slots:
    void init() {
        AppSettings settings(APP_COMPANY,APP_NAME);settings.clear();settings.setValue("Nmea2000/InterfaceType","None");
    }
    void byteFixture() {
        tN2kMsg m; auto f=fields(); f["Repeat"]="2";f["AISInfo"]="1";f["SID"]="12";
        QVERIFY(AisClassBStatic129809::encode(f,m));QCOMPARE(m.PGN,129809UL);QCOMPARE(m.Priority,6);
        // Independently calculated little-endian 123456789 = 0x075BCD15.
        const auto expected=QByteArray::fromHex("9815cd5b07")+QByteArray("SIM TEST: PART A@@@@")+QByteArray::fromHex("e10c");
        QCOMPARE(m.DataLen,27);QCOMPARE(QByteArray(reinterpret_cast<const char *>(m.Data),m.DataLen),expected);
        QStringList parsed;QVERIFY(AisClassBStatic129809::decode(m,parsed));QCOMPARE(parsed[5],QString("123456789"));QCOMPARE(parsed[7],f["Name"]);
        uint8_t id,sid;uint32_t identity;char name[21];tN2kAISRepeat repeat;tN2kAISTransceiverInformation info;
        QVERIFY(ParseN2kPGN129809(m,id,repeat,identity,name,sizeof(name),info,sid));QCOMPARE(id,24);QCOMPARE(int(repeat),2);QCOMPARE(identity,123456789u);QCOMPARE(int(info),1);QCOMPARE(sid,12);
    }
    void invalidFields_data() {
        QTest::addColumn<QString>("field");QTest::addColumn<QString>("value");
        const QList<QPair<QString,QString>> invalid{{"MessageID","23"},{"Repeat","4"},{"UserID","123"},{"UserID","1000000000"},
            {"UserID","0123456789"},{"UserID","-1"},{"UserID","123456789.0"},{"Name","TOO LONG SIMULATED AIS NAME"},{"Name","BAD@NAME"},
            {"Name",QString("CAF")+QChar(0x00c9)},{"Name","lowercase"},{"AISInfo","5"},{"SID","256"},{"SID","abc"}};
        int n=0;for(const auto &item:invalid)QTest::newRow(qPrintable(QString::number(n++)))<<item.first<<item.second;
    }
    void invalidFields() {
        QFETCH(QString,field);QFETCH(QString,value);auto f=fields();f[field]=value;tN2kMsg m;
        QVERIFY(!AisClassBStatic129809::validField(field,value));QVERIFY(!AisClassBStatic129809::encode(f,m));
        AisClassBStatic129809 widget(1,"test");const auto original=widget.value(field);widget.setValue(field,value);QCOMPARE(widget.value(field),original);
    }
    void boundaryAndUnavailable() {
        auto f=fields();tN2kMsg m;QStringList parsed;
        for (const auto &id : {QString("100000000"),QString("999999999")}) {
            f["UserID"]=id;f["Name"]=QString(20,'Z');f["Repeat"]="3";f["AISInfo"]="31";f["SID"]="255";
            QVERIFY(AisClassBStatic129809::encode(f,m));QVERIFY(AisClassBStatic129809::decode(m,parsed));QCOMPARE(parsed[7],QString(20,'Z'));
        }
        for(auto id:{0u,0xffffffffu}) {
            SetN2kPGN129809(m,24,N2kaisr_Initial,id,"",N2kaisown_information_not_broadcast,255);
            QVERIFY(AisClassBStatic129809::decode(m,parsed));QCOMPARE(parsed[5],QString::number(id));QCOMPARE(parsed[7],QString());
            f["UserID"]=QString::number(id);QVERIFY(!AisClassBStatic129809::encode(f,m));
        }
    }
    void malformedReceive() {
        auto f=fields();tN2kMsg m;QVERIFY(AisClassBStatic129809::encode(f,m));QStringList out{"unchanged"};
        auto original=m;for(int length:{0,1,24,26,28}) {m=original;m.DataLen=length;QVERIFY(!AisClassBStatic129809::decode(m,out));QCOMPARE(out,QStringList{"unchanged"});}
        m=original;m.PGN=129810;QVERIFY(!AisClassBStatic129809::decode(m,out));
        m=original;m.Data[0]=23;QVERIFY(!AisClassBStatic129809::decode(m,out));
    }
    void modelWidgetAndTree() {
        Workspace w;auto *ais=w.add();QVERIFY(ais);QVERIFY(!ais->isEnabled());QVERIFY(!w.model->findPgn(ais->Id())->enabled);
        ais->setUserID(123456789);ais->setName("SIM TEST");QTRY_COMPARE(w.model->getPgnParams(ais->Id())[2][1],QString("123456789"));
        w.model->setCurrentPgn(ais->Id());QVERIFY(w.model->setData(w.model->index(3,1),"NEW SIM NAME"));QCOMPARE(ais->Name(),QString("NEW SIM NAME"));
        QVERIFY(!w.model->setData(w.model->index(2,1),"bad"));QVERIFY(!w.model->setData(w.model->index(3,1),"INVALID@NAME"));
        auto *tree=w.window.findChild<PgnTreeController *>()->getTreeModel();QTRY_VERIFY(tree->indexFor(ais,"UserID").isValid());
        QCOMPARE(tree->indexFor(ais,"UserID").siblingAtColumn(1).data().toString(),QString("123456789"));
        QVERIFY(w.controller->selectLayoutWidget(ais,"Name"));QCOMPARE(w.model->IdxHasFocus(),ais->Id());
        ais->setEnabled(true);QVERIFY(w.model->findPgn(ais->Id())->enabled);
        auto prefs=ais->UserPrefs();prefs.speedUnitIndex=3;ais->setUserPrefs(prefs);QCOMPARE(ais->UserID(),123456789u);
        EnumComboBoxDelegate delegate;QStyleOptionViewItem option;
        w.model->setCurrentPgn(ais->Id());
        auto *editor=delegate.createEditor(&w.window,option,w.model->index(4,1));auto *combo=qobject_cast<QComboBox *>(editor);
        QVERIFY(combo);QCOMPARE(combo->count(),6);combo->setCurrentIndex(combo->findData(31));
        delegate.setModelData(combo,w.model,w.model->index(4,1));QCOMPARE(ais->AISInfo(),31);delete editor;
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void pendingEdits() {
        Workspace w;auto *ais=w.add();auto *edit=ais->findChild<QLineEdit *>("Name");
        // Selecting table parameters alone does not activate a hidden tab page.
        auto *tree=w.window.findChild<PgnTreeController *>();
        QTRY_VERIFY(tree->getTreeModel()->indexFor(ais).isValid());
        QVERIFY(QMetaObject::invokeMethod(tree,"navigate",Qt::DirectConnection,Q_ARG(QModelIndex,tree->getTreeModel()->indexFor(ais))));
        w.window.activateWindow();QApplication::setActiveWindow(&w.window);
        QVERIFY(w.controller->selectLayoutWidget(ais,"Name"));edit->setFocus();QTRY_VERIFY(edit->hasFocus());
        edit->setText("PENDING SIM");edit->setModified(true);ais->setSID(10);QCOMPARE(edit->text(),QString("PENDING SIM"));
        QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));QCOMPARE(ais->Name(),QString("PENDING SIM"));
        edit->setText("bad@name");edit->setModified(true);QVERIFY(QMetaObject::invokeMethod(edit,"editingFinished",Qt::DirectConnection));
        QCOMPARE(ais->Name(),QString("PENDING SIM"));QVERIFY(!ais->findChild<QLabel *>("aisValidation")->text().isEmpty());
    }
    void receiveIdentityAndMode() {
        Workspace w;w.controller->setMode(Viewer);auto *handler=w.window.findChild<Nmea2000Handler *>();handler->setMode(Viewer);
        auto receive=[&](uint32_t identity,int source,const char *name) {
            tN2kMsg m;SetN2kPGN129809(m,24,N2kaisr_Initial,identity,name,N2kaischannel_A_VDL_reception,255);
            m.Source=source;handler->nmea2000MessageReceived(m);
        };
        receive(123456789,23,"SIM ONE");receive(123456790,23,"SIM TWO");receive(123456789,24,"SIM RELAY");receive(123456789,23,"SIM RENAMED");
        QTRY_COMPARE(w.window.findChildren<AisClassBStatic129809 *>().size(),3);
        for(auto *ais:w.window.findChildren<AisClassBStatic129809 *>()) {
            const auto *data=w.model->findPgn(ais->Id());QVERIFY(data);QVERIFY(data->source=="23"||data->source=="24");
            if(ais->UserID()==123456789 && data->source=="23")QCOMPARE(ais->Name(),QString("SIM RENAMED"));
            QVERIFY(w.model->parameterUpdatedAt(ais->Id(),"Name").isValid() || ais->Name()!="SIM RENAMED");
        }
        w.controller->setMode(Sender);handler->setMode(Sender);receive(123456791,25,"IGNORED");QCOMPARE(w.window.findChildren<AisClassBStatic129809 *>().size(),3);
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
    }
    void layoutRoundTripAndFailure() {
        Workspace w;auto *ais=w.add();ais->setUserID(123456789);ais->setName("SAVED SIM");ais->setEnabled(true);ais->setAISInfo(31);ais->setSID(7);
        QTemporaryDir dir;const auto path=dir.filePath("static.json");QVERIFY(w.controller->saveLayoutToJson(path));
        confirm();QVERIFY(w.controller->loadLayoutFromJson(path));auto all=w.window.findChildren<AisClassBStatic129809 *>();QCOMPARE(all.size(),1);ais=all.first();
        QCOMPARE(ais->UserID(),123456789u);QCOMPARE(ais->Name(),QString("SAVED SIM"));QCOMPARE(ais->AISInfo(),31);QCOMPARE(ais->SID(),7);QVERIFY(ais->isEnabled());
        w.controller->setMode(Viewer);w.controller->setMode(Sender);QCOMPARE(ais->Name(),QString("SAVED SIM"));
        QVERIFY(!w.window.findChild<QAction *>("actionStart_Sending")->isChecked());
        // Failed atomic save leaves the valid file intact and names the offending field.
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto saved=file.readAll();file.close();
        w.model->changeParam(ais->Id(),"UserID","bad");QVERIFY(!w.controller->saveLayoutToJson(path));
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),saved);
    }
    void authorizedInMemorySending() {
        Workspace w;auto *ais=w.add();ais->setUserID(123456789);ais->setName("MEMORY: SIM");
        MemoryCan backend;Nmea2000Handler handler(nullptr,nullptr,&backend);handler.SetInterfaceType("SocketCan");handler.InitializeSocketCan({"testcan",42});
        QTRY_VERIFY(handler.IsCanInterfaceOpen());NmeaSenderWorker sender;sender.setPgnTableController(w.table);
        connect(&sender,&NmeaSenderWorker::sendPgn,&handler,&Nmea2000Handler::sendPgn);
        sender.sendNmeaMessage();QVERIFY(backend.sent.isEmpty());sender.startSending();sender.sendNmeaMessage();QVERIFY(backend.sent.isEmpty());
        ais->setEnabled(true);sender.sendNmeaMessage();QCOMPARE(backend.sent.size(),1);QCOMPARE(backend.sent.first().PGN,129809UL);
        QStringList decoded;QVERIFY(AisClassBStatic129809::decode(backend.sent.first(),decoded));QCOMPARE(decoded[7],QString("MEMORY: SIM"));
        sender.stopSending();sender.sendNmeaMessage();QCOMPARE(backend.sent.size(),1);
        sender.setMode(Viewer);sender.startSending();QVERIFY(!sender.isSending());
        handler.setMode(Viewer);QVERIFY(!handler.sendPgn(129809,sendFields(fields())));
        QCOMPARE(backend.sent.size(),1);
    }
    void nativePublisherDoesNotRewriteSelf() {
        Nmea2000Handler handler;handler.SetInterfaceType("SignalK");

        QVERIFY(!handler.sendPgn(129809,sendFields(fields())));QVERIFY(!handler.IsSignalKActive());
    }
    void renderThemes() {
        Workspace w;auto *ais=w.add();ais->setUserID(123456789);QVERIFY(w.controller->selectLayoutWidget(ais));
        for(const QString name:{"Dark","Light"}) {
            AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Preference/StyleSheetName",name);settings.sync();w.window.HotReloadSettings();QTest::qWait(80);
            auto *tree=w.window.findChild<PgnTreeController *>();
            QTRY_VERIFY(tree->getTreeModel()->indexFor(ais).isValid());
            QVERIFY(QMetaObject::invokeMethod(tree,"navigate",Qt::DirectConnection,Q_ARG(QModelIndex,tree->getTreeModel()->indexFor(ais))));
            tree->getTreeView()->expandAll();QTest::qWait(40);QVERIFY(ais->isVisible());
            for(auto *edit:ais->findChildren<QLineEdit *>())QVERIFY(ais->rect().contains(QRect(edit->mapTo(ais,QPoint()),edit->size())));
            const auto dir=qEnvironmentVariable("M229B_EVIDENCE");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(w.window.grab().save(dir+"/"+name+"-ais.png"));}
        }
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile(QDir::currentPath()+"/build/m229b/profile-XXXXXX");qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QApplication app(argc,argv);app.setStyle("Fusion");QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    AisStaticTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "ais_static_tests.moc"
