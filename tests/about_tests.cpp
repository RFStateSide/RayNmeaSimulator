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
#include <QApplication>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QFile>
#include <QTimer>
#include <QScrollArea>
#include <QScrollBar>
#include "aboutdialog.h"
#include "aboutlinks.h"
#include "themes.h"
#include "mainwindow.h"
#include <QAction>
#include <QToolButton>
#include "appsettings.h"

class AboutTests : public QObject {
    Q_OBJECT
    QUrl opened;
public slots:
    void captureUrl(const QUrl &url) { opened=url; }
private slots:
    void metadataAndResources() {
        AboutDialog d;
        QCOMPARE(QApplication::applicationVersion(),QStringLiteral(APP_VERSION));
        QCOMPARE(QStringLiteral(APP_VERSION),QString("0.1.0"));
        QVERIFY(!d.windowIcon().isNull());
        QVERIFY(d.findChild<QLabel*>("aboutIcon")->pixmap().width()>=112);
        QVERIFY(d.findChild<QLabel*>("aboutVersion")->text().contains(APP_VERSION));
        QFile f(":/resources/ABOUT-NOTICES.txt");QVERIFY(f.open(QIODevice::ReadOnly));
        const auto text=f.readAll();QVERIFY(text.contains("AGPL-3.0-or-later"));QVERIFY(text.contains("Timo Lappalainen"));QVERIFY(text.contains("Al Thomason"));QVERIFY(text.contains("SDL2"));QVERIFY(text.contains("CANboat"));
        QVERIFY(text.contains("Qt Charts has GPLv3/commercial"));
        QVERIFY(!text.contains("Historical Git rights"));QVERIFY(!text.contains("reviewed before shipping"));
        QVERIFY(d.findChild<QLabel*>("aboutSupport")->isEnabled());
    }
    void userFacingDescription() {
        AboutDialog d;
        const auto summary=d.findChild<QLabel*>("aboutSummary")->text();
        QVERIFY(summary.contains("Viewer RX"));QVERIFY(summary.contains("Autopilot"));
        QVERIFY(d.findChild<QLabel*>("aboutGuide")->text().contains("offline"));
        QVERIFY(d.findChild<QLabel*>("aboutDesignation")->text().contains("AS-IS"));
        QVERIFY(d.findChild<QLabel*>("aboutDesignation")->text().contains("explicit authorization"));
        for(auto *label:d.findChildren<QLabel*>()) {
            QVERIFY(!label->text().contains("engineering/test workflows"));
            QVERIFY(!label->text().contains("currently private"));
        }
    }
    void explicitLinksOnly() {
        QDesktopServices::setUrlHandler("https",this,"captureUrl");
        AboutDialog d;QVERIFY(opened.isEmpty());
        QLabel *website=nullptr;
        for(auto *label:d.findChildren<QLabel*>()) {
            QVERIFY(!label->openExternalLinks());
            if(label->text().contains("www.rfstateside.com/"))website=label;
        }
        QVERIFY(website);
        QVERIFY(QMetaObject::invokeMethod(website,"linkActivated",Q_ARG(QString,QString("https://www.rfstateside.com/"))));
        QCOMPARE(opened,QUrl("https://www.rfstateside.com/"));opened.clear();
        auto *support=d.findChild<QLabel*>("aboutSupport");QVERIFY(support);
        QVERIFY(support->text().contains(AboutLinks::SupportUrl));
        QVERIFY(QMetaObject::invokeMethod(support,"linkActivated",Q_ARG(QString,QString("https://unrelated.invalid/"))));
        QCOMPARE(opened,QUrl(QString::fromLatin1(AboutLinks::SupportUrl)));opened.clear();
        QDesktopServices::unsetUrlHandler("https");
    }
    void failedBrowserAndSupport() {
        AboutDialog d;QVERIFY(opened.isEmpty());
        auto *support=d.findChild<QLabel*>("aboutSupport");QVERIFY(support->isEnabled());
        QVERIFY(support->text().contains(AboutLinks::SupportUrl));
        bool fallback=false;
        QTimer::singleShot(50,[&]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());fallback=box&&box->textFormat()==Qt::PlainText&&box->text().contains(AboutLinks::SupportUrl);if(box)box->accept();});
        AboutLinks::open(&d,QUrl(QString::fromLatin1(AboutLinks::SupportUrl)),[](const QUrl&){return false;});QVERIFY(fallback);
    }
    void themes_data() {QTest::addColumn<QString>("theme");for(const auto &theme:Themes::builtInThemes())QTest::newRow(qPrintable(theme))<<theme;}
    void themes() {
        QFETCH(QString,theme);Themes::apply(theme);AboutDialog d;d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));QTest::qWait(70);
        QVERIFY(d.width()>=560);QVERIFY(d.height()>300);
        auto *closeButton=d.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close);QVERIFY(d.rect().contains(closeButton->mapTo(&d,QPoint(0,0))));
        QVERIFY(d.findChild<QScrollArea*>());
        auto *support=d.findChild<QLabel*>("aboutSupport");QVERIFY(support->width()>=support->fontMetrics().horizontalAdvance("Support RayNmeaSimulator with PayPal"));
        const auto images=qEnvironmentVariable("RAY_ABOUT_IMAGES");
        if(!images.isEmpty()){QDir().mkpath(images);QVERIFY(d.grab().save(images+"/"+theme+".png"));}
        d.resize(560,350);QTest::qWait(30);
        QVERIFY(d.rect().contains(closeButton->mapTo(&d,QPoint(0,0))));
        QVERIFY(d.findChild<QScrollArea*>()->verticalScrollBar()->maximum()>0);
        d.resize(820,700);QTest::qWait(30);
        auto *close=d.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close);close->setFocus();QTest::keyClick(close,Qt::Key_Enter);QVERIFY(!d.isVisible());
        d.show();QTest::keyClick(&d,Qt::Key_Escape);QVERIFY(!d.isVisible());
    }
    void mainWindowEntryAndStop() {
        MainWindow window;
        auto *start=window.findChild<QAction*>("actionStart_Sending");QVERIFY(start);QVERIFY(!start->isChecked());
        auto *action=window.findChild<QAction*>("actionAbout");QVERIFY(action);bool opened=false,stopAvailable=false;
        QTimer::singleShot(70,[&]{
            auto *dialog=window.findChild<AboutDialog*>();opened=dialog&&dialog->isVisible();
            if(dialog){stopAvailable=dialog->findChild<QToolButton*>("applicationStopSending");QTest::keyClick(dialog,Qt::Key_F12);dialog->reject();}
        });
        action->trigger();QVERIFY(opened);QVERIFY(stopAvailable);QVERIFY(!start->isChecked());
        auto *support=window.findChild<QAction*>("actionSupportRayNmeaSimulator");QVERIFY(support);
        QCOMPARE(support->text(),QString::fromUtf8("Support…"));
        QDesktopServices::setUrlHandler("https",this,"captureUrl");
        support->trigger();QCOMPARE(this->opened,QUrl(QString::fromLatin1(AboutLinks::SupportUrl)));this->opened.clear();
        QDesktopServices::unsetUrlHandler("https");QVERIFY(!start->isChecked());
    }
    void credits() {
        AboutDialog d;d.show();bool found=false;
        QTimer::singleShot(40,[&]{auto *modal=qobject_cast<QDialog*>(QApplication::activeModalWidget());found=modal&&modal->windowTitle()=="Licenses and Acknowledgments";if(modal)modal->reject();});
        d.findChild<QPushButton*>("aboutCredits")->click();QVERIFY(found);
    }
};
int main(int argc,char **argv){
    QTemporaryDir profile;if(!profile.isValid())return 2;qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";QList<char*> pointers;for(auto &arg:args)pointers<<arg.data();int count=pointers.size();pointers<<nullptr;
    QApplication app(count,pointers.data());QApplication::setApplicationName(APP_NAME);QApplication::setOrganizationName(APP_COMPANY);QApplication::setApplicationVersion(APP_VERSION);Themes::initializeSystemStyle();
    AboutTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "about_tests.moc"
