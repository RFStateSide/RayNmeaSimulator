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
#include <cmath>
#include "n2khelper.h"
#include <QAction>
#include <QCheckBox>
#include <QDockWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QNetworkProxy>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "appsettings.h"
#include "temperaturewidget130312.h"
#include "themes.h"
#include "windowstate.h"
#include "N2kMessages.h"

namespace {
void settle() { QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall); QCoreApplication::processEvents(); }
QValidator::State validity(const QLineEdit *edit, QString text) { int position=0; return edit->validator()->validate(text,position); }
void capture(QWidget &widget, const QString &name) {
    const auto out=qEnvironmentVariable("RAY_POLISH_OUTPUT");
    if (!out.isEmpty()) { QDir().mkpath(out); QVERIFY(widget.grab().save(out+"/"+name+".png")); }
}
}
class UiPolishTests : public QObject {
    Q_OBJECT
private slots:
    void init() {
        AppSettings s(APP_COMPANY,APP_NAME); s.clear();
        s.setValue("Nmea2000/InterfaceType","None");
        s.setValue("Nmea2000/Nmea2000Enabled",false);
        s.setValue("Nmea0183/Nmea0183Enabled",false);
        s.setValue("Preference/PromptSaveLayoutOnClose",false); s.sync();
    }
    void temperatureLimits_data() {
        QTest::addColumn<int>("unit"); QTest::addColumn<double>("bottom"); QTest::addColumn<double>("top"); QTest::addColumn<int>("decimals");
        QTest::newRow("kelvin")<<0<<213.05<<523.25<<2;
        QTest::newRow("celsius")<<1<<-60.1<<250.1<<1;
        QTest::newRow("fahrenheit")<<2<<-76.18<<482.18<<2;
    }
    void temperatureLimits() {
        QFETCH(int,unit); QFETCH(double,bottom); QFETCH(double,top); QFETCH(int,decimals);
        TemperatureWidget130312 widget(1,"Temperature",nullptr); UserUnitPrefs prefs; prefs.tempUnitIndex=unit; widget.setUserPrefs(prefs);
        auto *edit=widget.findChild<QLineEdit*>("lineEdit"); QVERIFY(edit);
        auto *validator=qobject_cast<const QDoubleValidator*>(edit->validator()); QVERIFY(validator);
        QCOMPARE(validator->bottom(),bottom); QCOMPARE(validator->top(),top); QCOMPARE(validator->decimals(),decimals);
        const double step=std::pow(10.,-decimals);
        for (double value:{bottom,bottom+step,top-step,top}) QCOMPARE(validity(edit,QString::number(value,'f',decimals)),QValidator::Acceptable);
        for (double value:{bottom-step,top+step}) QVERIFY(validity(edit,QString::number(value,'f',decimals))!=QValidator::Acceptable);
        QVERIFY(std::abs(N2kHelper::convertUserTemperatureToN2kUnits(bottom,prefs)-213.05)<1e-9);
        QVERIFY(std::abs(N2kHelper::convertUserTemperatureToN2kUnits(top,prefs)-523.25)<1e-9);
        for (double canonical:{213.05,223.15,255.15,273.15,300.123456789,523.25}) {
            widget.setActualTemperature(canonical); QSignalSpy changes(&widget,&TemperatureWidget130312::ActualTemperatureChanged);
            for (int other:{0,1,2,0,unit}) { prefs.tempUnitIndex=other; widget.setUserPrefs(prefs); QCOMPARE(validity(edit,edit->text()),QValidator::Acceptable); QCOMPARE(widget.ActualTemperature(),canonical); }
            QCOMPARE(changes.count(),0);
        }
        widget.setActualTemperature(600.123456789); widget.setUserPrefs(prefs); QCOMPARE(widget.ActualTemperature(),600.123456789); // RX/Layout values are not clamped.
        widget.setActualTemperature(N2kDoubleNA); widget.setInitialGuiValues(); QCOMPARE(edit->text(),QString("Unavailable")); QCOMPARE(widget.ActualTemperature(),N2kDoubleNA);
    }
    void manualEntryAndWirePrecision() {
        TemperatureWidget130312 widget(1,"Temperature",nullptr); auto *edit=widget.findChild<QLineEdit*>("lineEdit");
        for (int unit:{0,1,2}) {
            UserUnitPrefs prefs; prefs.tempUnitIndex=unit; widget.setUserPrefs(prefs);
            const QString text=unit==0?"255.15":unit==1?"-18.0":"-0.40";
            QCOMPARE(validity(edit,text),QValidator::Acceptable);
            QVERIFY(QMetaObject::invokeMethod(edit,"textEdited",Q_ARG(QString,text)));
            QVERIFY(std::abs(widget.ActualTemperature()-255.15)<1e-9);
            QSignalSpy changes(&widget,&TemperatureWidget130312::ActualTemperatureChanged);
            for (const QString invalid:{QString("-"),QString("99999"),QString("not a temperature")}) {
                QVERIFY(QMetaObject::invokeMethod(edit,"textEdited",Q_ARG(QString,invalid)));
                QVERIFY(std::abs(widget.ActualTemperature()-255.15)<1e-9);
            }
            QCOMPARE(changes.count(),0);
            tN2kMsg message; SetN2kPGN130312(message,1,0,N2kts_SeaTemperature,widget.ActualTemperature(),N2kDoubleNA);
            unsigned char sid,instance; tN2kTempSource source; double actual,set;
            QVERIFY(ParseN2kPGN130312(message,sid,instance,source,actual,set)); QVERIFY(std::abs(actual-255.15)<.0051); QCOMPARE(set,N2kDoubleNA);
        }
    }
    void layoutAndCanonicalModel_data() {
        QTest::addColumn<double>("canonical");
        QTest::newRow("precision")<<300.123456789;
        QTest::newRow("minimum")<<213.05;
        QTest::newRow("maximum")<<523.25;
        QTest::newRow("outside-manual-range")<<600.123456789;
        QTest::newRow("unavailable")<<N2kDoubleNA;
    }
    void layoutAndCanonicalModel() {
        QFETCH(double,canonical);
        MainWindow window; auto *controller=window.findChild<MainController*>();
        controller->setMode(Viewer); // Bypass the interactive source picker in this RX/restoration fixture.
        controller->addWidgetAsComboboxTextChanged("130312","Temperature","Environment"); settle();
        auto *widget=window.findChild<TemperatureWidget130312*>(); QVERIFY(widget); widget->setActualTemperature(canonical); settle();
        auto *table=window.findChild<PgnTableController*>()->getPgnTableModel(); table->setCurrentPgn(widget->Id());
        const auto before=table->getPgnParams(widget->Id());
        for (int unit:{0,1,2}) { auto prefs=widget->UserPrefs(); prefs.tempUnitIndex=unit; widget->setUserPrefs(prefs); settle(); QCOMPARE(table->getPgnParams(widget->Id()),before); }
        QTemporaryDir temporary; QVERIFY(temporary.isValid()); const auto file=temporary.filePath("temperature.json"); QVERIFY(controller->saveLayoutToJson(file));
        MainWindow restored; QVERIFY(restored.findChild<MainController*>()->loadLayoutFromJson(file)); settle();
        auto *loaded=restored.findChild<TemperatureWidget130312*>(); QVERIFY(loaded); QCOMPARE(loaded->ActualTemperature(),canonical);
        loaded->setInitialGuiValues();
        if(canonical==N2kDoubleNA) QCOMPARE(loaded->findChild<QLineEdit*>("lineEdit")->text(),QString("Unavailable"));
        QVERIFY(!restored.findChild<QAction*>("actionStart_Sending")->isChecked());
    }
    void inspectorColumnsAndTooltips() {
        QTableView view; PgnTableController controller(&view);
        const QString name="LongEngineReferenceStationPropertyIdentifier";
        const QString type="tN2kDetailedReferenceStationIntegrityEnumeration";
        controller.addPgn(1,"Synthetic","130312",true,{{name,"300.1234567890123","K",type}},{});
        controller.showPngParameters(1); view.resize(400,250); view.show(); settle();
        auto *model=controller.getPgnTableModel(); const auto before=model->getPgnParams(1);
        view.setColumnWidth(0,271); view.setColumnWidth(1,239); view.selectRow(0);
        for (int n=0;n<3;++n) {controller.refreshPgnParameters(1);controller.refresh();}
        QCOMPARE(view.columnWidth(0),271); QCOMPARE(view.columnWidth(1),239);
        QVERIFY(view.horizontalScrollBar()->maximum()>0); QVERIFY(view.selectionModel()->hasSelection());
        for (int column=0;column<4;++column) QVERIFY(model->data(model->index(0,column),Qt::ToolTipRole).toString().contains(before[0][column]));
        QCOMPARE(model->getPgnParams(1),before);
        capture(view,"property-long-identifiers");
        controller.addPgn(2,"Another","130312",true,{{"Short","1","K","double"}},{}); QCOMPARE(view.columnWidth(0),271);
    }
    void docksThemesAndSavedGeometry() {
        MainWindow window; window.resize(1280,850); window.show();
        auto *controller=window.findChild<MainController*>(); controller->setMode(Viewer); // Bypass the interactive source picker in this RX/restoration fixture.
        controller->addWidgetAsComboboxTextChanged("130312","Temperature","Environment"); settle();
        auto *temperature=window.findChild<TemperatureWidget130312*>(); QVERIFY(temperature); temperature->setActualTemperature(300.12); settle();
        auto *table=window.findChild<QTableView*>("tableView"); auto *tree=window.findChild<QTreeView*>("treeView");
        auto *dock=window.findChild<QDockWidget*>("dockWidgetTable"); QVERIFY(table&&tree&&dock);
        window.findChild<PgnTableController*>()->showPngParameters(temperature->Id());tree->expandAll();
        table->setColumnWidth(0,277); tree->setColumnWidth(0,283);
        for (const auto &theme:Themes::builtInThemes()) {
            AppSettings settings(APP_COMPANY,APP_NAME); settings.setValue("Preference/StyleSheetName",theme);settings.sync(); window.HotReloadSettings(); settle();
            QCOMPARE(table->columnWidth(0),277); QCOMPARE(tree->columnWidth(0),283);
            QVERIFY(table->horizontalHeader()->sectionResizeMode(0)==QHeaderView::Interactive);
            QVERIFY(tree->header()->sectionResizeMode(0)==QHeaderView::Interactive);
            window.resize(1100,740); settle(); QVERIFY(dock->width()>=dock->minimumWidth());
            table->setFocus(); capture(window,theme+"-inspectors");
            QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
        }
        window.resize(1280,850); window.resizeDocks({dock},{440},Qt::Horizontal); settle();
        AppSettings settings(APP_COMPANY,APP_NAME); WindowState::save(window,settings); settings.sync(); const auto size=window.size();const int width=dock->width();
        MainWindow restored;restored.show();QTest::qWait(100);QCOMPARE(restored.size(),size);QVERIFY(std::abs(restored.findChild<QDockWidget*>("dockWidgetTable")->width()-width)<=2);
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile(QDir::currentPath()+"/polish-profile-XXXXXX");if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*> pointers;for(auto &arg:args)pointers<<arg.data();int count=pointers.size();pointers<<nullptr;
    QTemporaryFile screenConfig(QDir::currentPath()+"/polish-screen-XXXXXX.json");
    if (qEnvironmentVariable("QT_QPA_PLATFORM")=="offscreen") {
        if (!screenConfig.open()) return 2;
        screenConfig.write(R"({"screens":[{"name":"fixture","width":1920,"height":1200}]})");screenConfig.flush();
        qputenv("QT_QPA_PLATFORM",("offscreen:configfile="+QFileInfo(screenConfig.fileName()).fileName()).toUtf8());
    }
    QApplication app(count,pointers.data()); QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    UiPolishTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "ui_polish_tests.moc"
