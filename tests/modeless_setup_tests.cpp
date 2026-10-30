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
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include "mainwindow.h"
#include "appsettings.h"
#include "apppaths.h"
#include "engineprofiledialog.h"
#include "runtimeresources.h"
#include "signalk_testserver.h"

namespace {
class MemoryPort : public QSerialPort {
public:
    QByteArray output;
    bool open(OpenMode mode) override { return QIODevice::open(mode | QIODevice::Unbuffered); }
    void close() override { QIODevice::close(); }
    qint64 bytesToWrite() const override { return 0; }
protected:
    qint64 writeData(const char *data, qint64 count) override { output.append(data, count); return count; }
};
QAction *action(MainWindow &w, const char *name) { return w.findChild<QAction *>(name); }
NmeaSenderWorker *worker(MainWindow &w) { return w.findChild<NmeaSenderWorker *>(); }
DialogSetup *openSetup(MainWindow &w) {
    action(w, "actionOpen_Setup_Form")->trigger(); return w.findChild<DialogSetup *>();
}
void click(QObject *root, const char *name) { auto *b = root->findChild<QPushButton *>(name); Q_ASSERT(b); b->click(); }
void serialSettings() {
    AppSettings s(APP_COMPANY, APP_NAME);
    s.setValue("Nmea0183/Nmea0183Enabled", true); s.setValue("Nmea0183/Nmea0183Port", "MEMORY"); s.sync();
}
void addSentence(MainWindow &w) {
    w.findChild<PgnTableModel *>()->addPgn(123, "Fixture", "HDT", true,
        {{"Heading", "1", "rad", "double"}}, {}, 250);
}
void answer(QMessageBox::StandardButton choice) {
    QTimer::singleShot(0, [choice] {
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *box = qobject_cast<QMessageBox *>(widget)) box->button(choice)->click();
    });
}
void capture(QWidget &widget, const QString &name) {
    const auto directory = qEnvironmentVariable("M223_EVIDENCE");
    if (!directory.isEmpty()) { QVERIFY(QDir().mkpath(directory)); QVERIFY(widget.grab().save(directory + "/" + name + ".png")); }
}
}

class ModelessSetupTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> home;
private slots:
    void init() {
        home = std::make_unique<QTemporaryDir>(QDir::currentPath() + "/build/m223-test-XXXXXX");
        QVERIFY(home->isValid()); qputenv("RAYNMEA_TEST_HOME", home->path().toUtf8());
        AppSettings s(APP_COMPANY, APP_NAME); s.clear(); s.setValue("Preservation/Sentinel", "keep"); s.sync();
    }
    void cleanup() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        CredentialStore::remove("SignalKToken"); CredentialStore::remove("OpenWeatherMapApiKey");
        qunsetenv("RAYNMEA_TEST_HOME"); home.reset();
    }
    void modelessSingletonAndDraft() {
        MainWindow w; w.show(); auto *setup = openSetup(w);
        QVERIFY(setup->isWindow()); QCOMPARE(setup->windowModality(), Qt::NonModal);
        QVERIFY(!setup->isModal()); QVERIFY(!QApplication::activeModalWidget()); QVERIFY(w.isEnabled());
        QVERIFY(setup->windowFlags().testFlag(Qt::WindowMinimizeButtonHint));
        QVERIFY(setup->windowFlags().testFlag(Qt::WindowMaximizeButtonHint));
        auto *size = setup->findChild<QSpinBox *>("spinBoxWidgetColumns"); size->setValue(3);
        setup->resize(setup->width() + 50, setup->height() + 30);
        const auto geometry = setup->size();
        for (int i = 0; i < 3; ++i) { setup->showMinimized(); QCOMPARE(openSetup(w), setup); }
        QCOMPARE(size->value(), 3); QCOMPARE(setup->size(), geometry);
        QCOMPARE(w.findChildren<DialogSetup *>().size(), 1);
        action(w, "actionViewer_Rx")->setChecked(true); QVERIFY(!worker(w)->isSending());
        QVERIFY(setup->isVisible()); setup->reject(); QVERIFY(w.isVisible());
    }
    void stopWithSetupAndRealMemoryWrites() {
        serialSettings(); auto *port = new MemoryPort; MainWindow w(nullptr, nullptr, port); w.show(); addSentence(w);
        action(w, "actionStart_Sending")->trigger(); QTRY_VERIFY(!port->output.isEmpty());
        auto *setup = openSetup(w); QVERIFY(worker(w)->isSending());
        auto *stop = setup->findChild<QToolButton *>("setupStopSending"); QVERIFY(stop);
        QCOMPARE(stop->defaultAction(), action(w, "actionStop_Sending")); QVERIFY(stop->isEnabled());
        // Main-window action remains accessible with modeless Setup open.
        action(w, "actionStop_Sending")->trigger(); QVERIFY(!worker(w)->isSending());
        const auto output = port->output; QTest::qWait(350); QCOMPARE(port->output, output);
        QVERIFY(!stop->isEnabled()); QVERIFY(!action(w, "actionStart_Sending")->isChecked());
        for (int i = 0; i < 3; ++i) QTest::keyClick(setup, Qt::Key_F12);
        QVERIFY(setup->isVisible());
        setup->findChild<QSpinBox *>("spinBoxWidgetColumns")->setValue(3);
        click(setup, "pushButtonSave"); QVERIFY(!worker(w)->isSending()); setup->reject();
    }
    void draftsDoNotReconfigureAndRequireExplicitStop() {
        serialSettings(); auto *port = new MemoryPort; MainWindow w(nullptr, nullptr, port); w.show(); addSentence(w);
        action(w, "actionStart_Sending")->trigger(); QTRY_VERIFY(!port->output.isEmpty());
        auto *setup = openSetup(w);
        setup->findChild<QCheckBox *>("checkBoxNmea0183Enabled")->setChecked(false);
        setup->findChild<QLineEdit *>("signalKToken")->setText("synthetic-unsaved-token");
        for (auto name : {"pushButtonSave", "pushButtonLoad", "pushButtonClose"}) {
            click(setup, name); QVERIFY(setup->isVisible()); QVERIFY(worker(w)->isSending());
            QVERIFY(setup->findChild<QLabel *>("credentialStorageStatus")->text().contains("Stop Sending"));
        }
        AppSettings s(APP_COMPANY, APP_NAME); QVERIFY(s.value("Nmea0183/Nmea0183Enabled").toBool());
        QVERIFY(!CredentialStore::read("SignalKToken").found);
        const auto count = port->output.size(); QTRY_VERIFY(port->output.size() > count);
        setup->findChild<QToolButton *>("setupStopSending")->click(); QVERIFY(!worker(w)->isSending());
        // Cancel discards both transport and credential drafts.
        click(setup, "setupCancel"); setup = openSetup(w);
        QVERIFY(setup->findChild<QCheckBox *>("checkBoxNmea0183Enabled")->isChecked());
        QVERIFY(setup->findChild<QLineEdit *>("signalKToken")->text().isEmpty());
        QVERIFY(!worker(w)->isSending()); setup->reject();
    }
    void saveReloadCloseAndCancel() {
        MainWindow w; w.show(); auto *setup = openSetup(w);
        QSignalSpy reload(setup, &DialogSetup::ReloadSettings);
        auto *columns = setup->findChild<QSpinBox *>("spinBoxWidgetColumns"); columns->setValue(3);
        click(setup, "pushButtonSave"); QCOMPARE(reload.size(), 1);
        AppSettings s(APP_COMPANY, APP_NAME); QCOMPARE(s.value("Preference/WidgetColumns").toInt(), 3);
        columns->setValue(4); click(setup, "pushButtonLoad"); QCOMPARE(columns->value(), 3);
        columns->setValue(4); QTest::keyClick(setup, Qt::Key_Escape); QVERIFY(!setup->isVisible());
        setup = openSetup(w); QCOMPARE(columns->value(), 3);
        columns->setValue(2); click(setup, "pushButtonClose"); QVERIFY(!setup->isVisible());
        s.sync(); QCOMPARE(s.value("Preference/WidgetColumns").toInt(), 2);
        QCOMPARE(s.value("Preservation/Sentinel").toString(), QString("keep"));
    }
    void unchangedClosePreservesSending() {
        serialSettings(); MainWindow w(nullptr, nullptr, new MemoryPort); w.show();
        action(w, "actionStart_Sending")->trigger(); auto *setup = openSetup(w);
        QSignalSpy reload(setup, &DialogSetup::ReloadSettings);
        setup->findChild<QComboBox *>("electricalBattery")->setCurrentIndex(1);
        QVERIFY(setup->close()); QVERIFY(worker(w)->isSending()); QCOMPARE(reload.size(), 0);
        setup = openSetup(w); setup->findChild<QSpinBox *>("spinBoxWidgetColumns")->setValue(3);
        setup->reject(); QVERIFY(worker(w)->isSending()); action(w, "actionStop_Sending")->trigger();
    }
    void shutdownResolvesDrafts() {
        MainWindow w; w.show(); auto *setup = openSetup(w);
        auto *columns = setup->findChild<QSpinBox *>("spinBoxWidgetColumns"); columns->setValue(3);
        answer(QMessageBox::Cancel); QVERIFY(!w.close()); QVERIFY(w.isVisible()); QVERIFY(setup->isVisible()); QCOMPARE(columns->value(), 3);
        answer(QMessageBox::Save); QVERIFY(w.close());
        AppSettings s(APP_COMPANY, APP_NAME); QCOMPARE(s.value("Preference/WidgetColumns").toInt(), 3);
    }
    void shutdownDiscardAndActiveSaveProtection() {
        serialSettings(); MainWindow w(nullptr, nullptr, new MemoryPort); w.show();
        auto *setup = openSetup(w); setup->findChild<QSpinBox *>("spinBoxWidgetColumns")->setValue(3);
        action(w, "actionStart_Sending")->trigger();
        answer(QMessageBox::Save); QVERIFY(!w.close()); QVERIFY(worker(w)->isSending()); QVERIFY(setup->isVisible());
        answer(QMessageBox::Discard); QVERIFY(w.close()); QVERIFY(!worker(w)->isSending());
        AppSettings s(APP_COMPANY, APP_NAME); QVERIFY(s.value("Preference/WidgetColumns").toInt() != 3);
    }
    void qtModalPointerAndKeyboardStop() {
        serialSettings(); auto *port = new MemoryPort; MainWindow w(nullptr, nullptr, port); w.show(); addSentence(w);
        action(w, "actionStart_Sending")->trigger(); QTRY_VERIFY(!port->output.isEmpty());
        QMessageBox box(QMessageBox::Question, "Fixture confirmation", "Keep this confirmation open while stopping.", QMessageBox::Yes | QMessageBox::Cancel, &w);
        box.setModal(true); box.show(); QTest::qWait(30);
        auto *stop = box.findChild<QToolButton *>("applicationStopSending"); QVERIFY(stop); QVERIFY(stop->isVisible());
        QVERIFY(box.rect().contains(stop->geometry())); stop->click(); QVERIFY(box.isVisible()); QVERIFY(!worker(w)->isSending());
        const auto output = port->output; QTest::qWait(300); QCOMPARE(port->output, output); box.reject();
        action(w, "actionStart_Sending")->trigger(); QVERIFY(worker(w)->isSending());
        // Real nested event loop, not a shortcut assumed to cross a native OS dialog.
        QTimer::singleShot(0, &box, [&] { QTest::keyClick(&box, Qt::Key_F12); QVERIFY(!worker(w)->isSending()); box.reject(); });
        box.exec(); QVERIFY(!worker(w)->isSending());
    }
    void fileFontAndUnparentedDialogs() {
        serialSettings(); MainWindow w(nullptr, nullptr, new MemoryPort); w.show();
        QVERIFY(QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs));
        QFileDialog files(&w); QFontDialog fonts(&w); QDialog selection; QVBoxLayout layout(&selection); layout.addWidget(new QLabel("Layout selection"));
        for (auto *dialog : {static_cast<QDialog *>(&files), static_cast<QDialog *>(&fonts), &selection}) {
            action(w, "actionStart_Sending")->trigger(); QVERIFY(worker(w)->isSending());
            dialog->setModal(true); dialog->show(); QTest::qWait(20);
            auto *stop = dialog->findChild<QToolButton *>("applicationStopSending"); QVERIFY(stop); QVERIFY(stop->isVisible());
            QCOMPARE(stop->defaultAction(), action(w, "actionStop_Sending"));
            QTest::keyClick(dialog, Qt::Key_F12); QVERIFY(!worker(w)->isSending()); dialog->reject();
        }
    }
    void pendingSignalKStopCancelsRetry() {
        TestServer server; server.sendHello = false;
        AppSettings s(APP_COMPANY, APP_NAME); s.setValue("Nmea2000/Nmea2000Enabled", true);
        s.setValue("Nmea2000/InterfaceType", "SignalK"); s.setValue("Nmea2000/SignalKServer", server.config().server); s.sync();
        QString error; QVERIFY2(CredentialStore::write("SignalKToken", server.config().token, &error), qPrintable(error));
        MainWindow w; w.show(); action(w, "actionStart_Sending")->trigger();
        auto *setup = openSetup(w); QVERIFY(action(w, "actionStop_Sending")->isEnabled());
        QTest::keyClick(setup, Qt::Key_F12); QVERIFY(!action(w, "actionStart_Sending")->isChecked());
        QTest::qWait(150); QVERIFY(!worker(w)->isSending()); setup->reject();
    }
    void cachedCredentialsAndExplicitReload() {
        QString error;
        QVERIFY2(CredentialStore::write("OpenWeatherMapApiKey", "synthetic-initial", &error), qPrintable(error));
        serialSettings(); MainWindow w(nullptr, nullptr, new MemoryPort); w.show();
        // External replacement is a fixture proving opening uses the startup snapshot.
        QVERIFY2(CredentialStore::write("OpenWeatherMapApiKey", "synthetic-replacement", &error), qPrintable(error));
        action(w, "actionStart_Sending")->trigger(); auto *setup = openSetup(w);
        auto *key = setup->findChild<QLineEdit *>("lineEditOpenWeatherMapKey");
        QVERIFY(key->text() == "synthetic-initial");
        QCOMPARE(key->echoMode(), QLineEdit::Password);
        click(setup, "pushButtonLoad"); QVERIFY(key->text() == "synthetic-initial");
        setup->findChild<QToolButton *>("setupStopSending")->click(); click(setup, "pushButtonLoad");
        QVERIFY(key->text() == "synthetic-replacement");
        click(setup, "pushButtonShowOpenWeatherMapKey"); QCOMPARE(key->echoMode(), QLineEdit::Normal);
        setup->reject(); setup = openSetup(w); QCOMPARE(key->echoMode(), QLineEdit::Password);
        QVERIFY(key->text() == "synthetic-replacement"); setup->reject();
    }
    void editorCoexistsAndRefreshesSelection() {
        MainWindow w; w.show(); auto *setup = openSetup(w);
        action(w, "actionEngineProfiles")->trigger(); auto *editor = w.findChild<EngineProfileDialog *>(); QVERIFY(editor); QVERIFY(editor->isVisible());
        auto *combo = setup->findChild<QComboBox *>("comboBoxVesselEngine"); combo->setCurrentIndex(1); const auto chosen = combo->currentText();
        auto *columns = setup->findChild<QSpinBox *>("spinBoxWidgetColumns"); columns->setValue(3);
        // The same catalogSaved connection used by the editor must preserve Setup's draft.
        emit editor->catalogSaved(); QCOMPARE(combo->currentText(), chosen); QCOMPARE(columns->value(), 3);
        QCOMPARE(openSetup(w), setup); action(w, "actionEngineProfiles")->trigger(); QCOMPARE(w.findChildren<EngineProfileDialog *>().size(), 1);
        setup->reject(); QVERIFY(editor->isVisible()); QVERIFY(editor->close());
    }
    void renderedThemes_data() {
        QTest::addColumn<QString>("theme"); QTest::newRow("Dark") << QString("Dark"); QTest::newRow("Light") << QString("Light");
    }
    void renderedThemes() {
        QFETCH(QString, theme); AppSettings s(APP_COMPANY, APP_NAME); s.setValue("Preference/StyleSheetName", theme); s.sync();
        MainWindow w; w.show(); auto *setup = openSetup(w); QTest::qWait(50);
        capture(*setup, "setup-" + theme);
        setup->findChild<QTabWidget *>("tabWidget")->setCurrentIndex(1);
        setup->findChild<QComboBox *>("comboBoxNmea2000Interface")->setCurrentText("SignalK");
        QTest::qWait(30); capture(*setup, "setup-signalk-" + theme);
        setup->findChild<QTabWidget *>("tabWidget")->setCurrentIndex(0);
        QMessageBox box(QMessageBox::Question, "STOP accessibility", "F12 or Stop Sending remains available without accepting this confirmation.", QMessageBox::Yes | QMessageBox::Cancel, &w);
        box.setModal(true); box.show(); QTest::qWait(30); capture(box, "confirmation-" + theme); box.reject(); setup->reject();
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    ModelessSetupTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "modeless_setup_tests.moc"
