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
#include <QHelpEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QProcess>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QToolTip>
#include <QTreeView>
#include <QTableView>
#include "mainwindow.h"
#include "appsettings.h"
#include "engineprofiledialog.h"
#include "generatorengine.h"
#include "generatorvesselmovement.h"
#include "generatorweather.h"
#include "generatortranslowerunit.h"

namespace {
class HelpProbe : public QWidget {
public:
    int deliveries = 0;
    bool event(QEvent *e) override { if (e->type() == QEvent::ToolTip) { ++deliveries; e->accept(); return true; } return QWidget::event(e); }
};
class ViewProbe : public QObject {
public:
    int deliveries = 0;
    bool eventFilter(QObject *, QEvent *e) override { if (e->type() == QEvent::ToolTip) ++deliveries; return false; }
};
void hover(QWidget *widget) {
    const auto point = widget->rect().center(); QHelpEvent event(QEvent::ToolTip, point, widget->mapToGlobal(point));
    QApplication::sendEvent(widget, &event);
}
QAction *action(MainWindow &w, const char *name) { return w.findChild<QAction *>(name); }
DialogSetup *setup(MainWindow &w) { action(w,"actionOpen_Setup_Form")->trigger(); return w.findChild<DialogSetup *>(); }
void click(QObject *root, const char *name) { auto *button = root->findChild<QPushButton *>(name); Q_ASSERT(button); button->click(); }
void apply(MainWindow &w, bool on) { auto *s = setup(w); s->findChild<QCheckBox *>("checkBoxShowTooltips")->setChecked(on); click(s,"pushButtonSave"); }
NmeaSenderWorker *worker(MainWindow &w) { return w.findChild<NmeaSenderWorker *>(); }
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode mode) override { return QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { QIODevice::close(); }
    qint64 bytesToWrite() const override { return 0; }
protected:
    qint64 writeData(const char *data, qint64 count) override { output.append(data,count); return count; }
};
void capture(QWidget &widget,const QString &name) {
    const auto directory=qEnvironmentVariable("M224_EVIDENCE");
    if (!directory.isEmpty()) { QVERIFY(QDir().mkpath(directory)); QVERIFY(widget.grab().save(directory+"/"+name+".png")); }
}
}
class TooltipTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> home;
private slots:
    void init() {
        home=std::make_unique<QTemporaryDir>(QDir::currentPath()+"/build/m224-test-XXXXXX");
        QVERIFY(home->isValid()); qputenv("RAYNMEA_TEST_HOME",home->path().toUtf8());
        AppSettings s(APP_COMPANY,APP_NAME); s.clear(); s.setValue("Preservation/Sentinel","keep"); s.sync();
    }
    void cleanup() { QToolTip::hideText(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); qunsetenv("RAYNMEA_TEST_HOME"); home.reset(); }
    void defaultEnabledAndDraftSemantics() {
        MainWindow w; w.show(); auto *s=setup(w); auto *box=s->findChild<QCheckBox *>("checkBoxShowTooltips"); QVERIFY(box); QVERIFY(box->isChecked());
        HelpProbe probe; probe.setToolTip("Original guidance"); hover(&probe); QCOMPARE(probe.deliveries,1);
        box->setChecked(false); hover(&probe); QCOMPARE(probe.deliveries,2); // Unsaved change has no effect.
        s->reject(); s=setup(w); QVERIFY(box->isChecked());
        box->setChecked(false); click(s,"pushButtonSave"); hover(&probe); QCOMPARE(probe.deliveries,2);
        AppSettings settings(APP_COMPANY,APP_NAME); QVERIFY(!settings.value("Preference/ShowTooltips").toBool());
        box->setChecked(true); click(s,"pushButtonLoad"); QVERIFY(!box->isChecked());
        QCOMPARE(settings.value("Preservation/Sentinel").toString(),QString("keep")); s->reject();
    }
    void existingDynamicAndNewControls() {
        MainWindow w; HelpProbe existing; existing.setToolTip("Static hint");
        hover(&existing); QCOMPARE(existing.deliveries,1); apply(w,false); hover(&existing); QCOMPARE(existing.deliveries,1);
        existing.setToolTip("Updated while disabled"); HelpProbe created; created.setToolTip("New hint"); hover(&created); QCOMPARE(created.deliveries,0);
        QCOMPARE(existing.toolTip(),QString("Updated while disabled"));
        apply(w,true); hover(&existing); hover(&created); QCOMPARE(existing.deliveries,2); QCOMPARE(created.deliveries,1);
        QCOMPARE(existing.toolTip(),QString("Updated while disabled")); w.findChild<DialogSetup *>()->reject();
    }
    void modelViewsAndRecreatedLayouts() {
        MainWindow w; w.show(); action(w,"actionLoad_Default")->trigger(); QTest::qWait(60);
        auto *tree=w.findChild<QTreeView *>("treeView"); auto *table=w.findChild<QTableView *>("tableView");
        ViewProbe treeEvents,tableEvents; tree->viewport()->installEventFilter(&treeEvents); table->viewport()->installEventFilter(&tableEvents);
        apply(w,false); hover(tree->viewport()); hover(table->viewport()); QCOMPARE(treeEvents.deliveries,0); QCOMPARE(tableEvents.deliveries,0);
        const auto index=tree->model()->index(0,0); QVERIFY(index.isValid());
        action(w,"actionViewer_Rx")->setChecked(true); action(w,"actionSimulator_Tx")->setChecked(true);
        auto *controller=w.findChild<MainController *>(); QVERIFY(controller);
        QTimer::singleShot(0,[] { for (auto *widget:QApplication::topLevelWidgets()) if (auto *box=qobject_cast<QMessageBox *>(widget)) box->button(QMessageBox::Yes)->click(); });
        action(w,"actionLoad_Default")->trigger(); QTest::qWait(60);
        hover(tree->viewport()); QCOMPARE(treeEvents.deliveries,0);
        apply(w,true); hover(tree->viewport()); hover(table->viewport()); QCOMPARE(treeEvents.deliveries,1); QCOMPARE(tableEvents.deliveries,1);
        w.findChild<DialogSetup *>()->reject();
    }
    void auxiliaryWindowsAndContent() {
        MainWindow w; w.show(); auto *s=setup(w); action(w,"actionEngineProfiles")->trigger(); auto *editor=w.findChild<EngineProfileDialog *>(); QVERIFY(editor); QVERIFY(!editor->isModal());
        const QStringList ids={"newProfile","duplicateProfile","deleteProfile","saveCatalog","backupCatalog","restoreCatalog","restoreBundled","fuelCurve","fuelCurveGraph","fuelType","alternatorVoltage","catalogLocation"};
        for (const auto &id:ids) { auto *control=editor->findChild<QWidget *>(id); QVERIFY2(control,qPrintable(id)); QVERIFY2(!control->toolTip().isEmpty(),qPrintable(id)); }
        auto *newProfile=editor->findChild<QPushButton *>("newProfile"); const auto original=newProfile->toolTip();
        apply(w,false); ViewProbe seen; newProfile->installEventFilter(&seen); hover(newProfile); QCOMPARE(seen.deliveries,0);
        apply(w,true); hover(newProfile); QCOMPARE(seen.deliveries,1); QCOMPARE(newProfile->toolTip(),original);
        QVERIFY(s->isVisible()); QVERIFY(!s->isModal()); s->reject(); editor->close(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        action(w,"actionEngineProfiles")->trigger(); editor=w.findChild<EngineProfileDialog *>(); QVERIFY(editor); QVERIFY(!editor->findChild<QPushButton *>("newProfile")->toolTip().isEmpty()); editor->close();
    }
    void generatorCoverageAndDynamicValues() {
        MainWindow w; apply(w,false);
        GeneratorEngine engine(101,"Engine"); GeneratorVesselMovement vessel(102,"Movement",nullptr); GeneratorWeather weather(103,"Weather"); GeneratorTransLowerUnit gear(104,"Transmission");
        for (auto pair:QList<QPair<QWidget *,QString>>{{&engine,"checkBoxStartEngine"},{&engine,"comboBoxVesselEngine"},{&vessel,"checkBoxManualMovementEnabled"},{&vessel,"checkBoxGamePadEnabled"},{&weather,"pushButtonFetchWeather"},{&weather,"weatherAutoRefresh"},{&gear,"radioButtonNeutral"}}) {
            auto *control=pair.first->findChild<QWidget *>(pair.second); QVERIFY(control); QVERIFY(!control->toolTip().isEmpty());
            ViewProbe probe; control->installEventFilter(&probe); hover(control); QCOMPARE(probe.deliveries,0);
        }
        for (const auto *name : {"horizontalSliderHeading", "horizontalSliderRudder", "verticalSliderThrottle", "verticalSliderThrottleStbd"})
            QVERIFY(!vessel.findChild<QSlider *>(name)->toolTip().isEmpty());
        auto *heading=vessel.findChild<QSlider *>("horizontalSliderHeading"); heading->setValue(100); const auto tip=heading->toolTip(); QVERIFY(tip.contains("Heading"));
        apply(w,true); QCOMPARE(heading->toolTip(),tip); ViewProbe probe; heading->installEventFilter(&probe); hover(heading); QCOMPARE(probe.deliveries,1);
        w.findChild<DialogSetup *>()->reject();
    }
    void warningsAccessibilityAndStop() {
        AppSettings s(APP_COMPANY,APP_NAME); s.setValue("Nmea0183/Nmea0183Enabled",true); s.setValue("Nmea0183/Nmea0183Port","MEMORY"); s.setValue("Preference/ShowTooltips",false); s.sync();
        auto *port=new MemoryPort; MainWindow w(nullptr,nullptr,port); w.show(); auto *dialog=setup(w);
        w.findChild<PgnTableModel *>()->addPgn(1,"Fixture","HDT",true,{{"Heading","1","rad","double"}},{},250);
        action(w,"actionStart_Sending")->trigger(); QTRY_VERIFY(!port->output.isEmpty());
        dialog->findChild<QCheckBox *>("checkBoxShowTooltips")->setChecked(true); click(dialog,"pushButtonSave");
        QVERIFY(dialog->findChild<QLabel *>("credentialStorageStatus")->text().contains("Stop Sending")); QVERIFY(worker(w)->isSending());
        auto *status=w.findChild<QLabel *>("sendingStatus"); QVERIFY(!status->accessibleDescription().isEmpty());
        QMessageBox box(QMessageBox::Warning,"Validation","Essential validation remains visible",QMessageBox::Ok,&w); box.setModal(true); box.show();
        QTest::qWait(25); QVERIFY(box.isVisible()); QTest::keyClick(&box,Qt::Key_F12); QVERIFY(box.isVisible()); QVERIFY(!worker(w)->isSending());
        const auto output=port->output; QTest::qWait(300); QCOMPARE(port->output,output); box.reject();
        action(w,"actionStart_Sending")->trigger(); QVERIFY(worker(w)->isSending()); dialog->findChild<QToolButton *>("setupStopSending")->click(); QVERIFY(!worker(w)->isSending());
        click(dialog,"pushButtonSave"); QVERIFY(!worker(w)->isSending()); dialog->reject();
    }
    void separateProcessPersistence_data() { QTest::addColumn<bool>("on"); QTest::newRow("enabled")<<true; QTest::newRow("disabled")<<false; }
    void separateProcessPersistence() {
        QFETCH(bool,on); { MainWindow w; apply(w,on); w.findChild<DialogSetup *>()->reject(); }
        QProcess child; child.start(QCoreApplication::applicationFilePath(),{"--tooltip-probe",on?"on":"off"});
        QVERIFY(child.waitForFinished(30000)); QCOMPARE(child.exitStatus(),QProcess::NormalExit); QCOMPARE(child.exitCode(),0);
    }
    void nativePresentation_data() { QTest::addColumn<QString>("theme"); QTest::newRow("Dark")<<QString("Dark"); QTest::newRow("Light")<<QString("Light"); }
    void nativePresentation() {
        QFETCH(QString,theme); AppSettings settings(APP_COMPANY,APP_NAME); settings.setValue("Preference/StyleSheetName",theme); settings.sync();
        MainWindow w; w.show(); auto *s=setup(w); s->findChild<QTabWidget *>("tabWidget")->setCurrentIndex(3); QTest::qWait(40); capture(*s,"preferences-"+theme);
        auto *box=s->findChild<QCheckBox *>("checkBoxShowTooltips"); hover(box); QTRY_VERIFY(QToolTip::isVisible());
        QCOMPARE(QToolTip::palette().color(QPalette::Inactive,QPalette::ToolTipBase),qApp->palette().color(QPalette::Inactive,QPalette::ToolTipBase));
        QCOMPARE(QToolTip::palette().color(QPalette::Inactive,QPalette::ToolTipText),qApp->palette().color(QPalette::Inactive,QPalette::ToolTipText));
        for (auto *widget:QApplication::topLevelWidgets()) if (widget->windowType()==Qt::ToolTip && widget->isVisible()) capture(*widget,"tooltip-"+theme);
        box->setChecked(false); click(s,"pushButtonSave"); QTRY_VERIFY(!QToolTip::isVisible());
        hover(box); QTest::qWait(30); QVERIFY(!QToolTip::isVisible());
        QCOMPARE(box->toolTip().contains("STOP"),true); s->reject();
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    if (app.arguments().contains("--tooltip-probe")) {
        const bool on=app.arguments().last()=="on"; MainWindow w; auto *s=setup(w);
        if (s->findChild<QCheckBox *>("checkBoxShowTooltips")->isChecked()!=on) return 2;
        HelpProbe p; hover(&p); s->reject(); return p.deliveries==(on?1:0)?0:3;
    }
    TooltipTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "tooltip_tests.moc"
