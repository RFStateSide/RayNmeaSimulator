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
#include <QApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QCryptographicHash>
#include <QTextList>
#include <QListWidget>
#include <QComboBox>
#include <QScopeGuard>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableView>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSignalSpy>
#include <QShortcut>
#include <QTemporaryDir>
#include <QTimer>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextTable>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QXmlStreamReader>
#include "appsettings.h"
#include "apppaths.h"
#include "helpcatalog.h"
#include "helpdialog.h"
#include "mainwindow.h"
#include "themes.h"
#include "maincontroller.h"
#include "dialogsetup.h"
#include "nmeasenderworker.h"
#include "nmea2000handler.h"
#include "N2kMessages.h"
#include "expansioninstruments.h"

class HelpTests:public QObject {
    Q_OBJECT
    QList<QUrl> opened;
private slots:
    void captureUrl(const QUrl &url){opened.append(url);}
    void catalogAndContent() {
        QCOMPARE(HelpCatalog::topics().size(),52);QSet<QString> ids,groups;
        for(const auto &info:HelpCatalog::topics()) {
            QVERIFY(!ids.contains(info.id));ids.insert(info.id);groups.insert(info.group);
            QVERIFY(QFile::exists(info.resource));const auto html=HelpCatalog::article(info.id);
            QVERIFY(!html.contains("Phase 2 content pending."));QVERIFY(!html.contains("<script"));
            QVERIFY(html.contains("id=\"related-topics\""));
            QTextDocument content;content.setHtml(html);QVERIFY(content.toPlainText().size()>700);
            QCOMPARE(html.count("<img"),html.count("Click to enlarge."));
            QRegularExpression pictures("<img[^>]+alt=\"([^\"]+)\"[^>]*>");
            auto descriptions=pictures.globalMatch(html);int described=0;
            while(descriptions.hasNext()){QVERIFY(!descriptions.next().captured(1).trimmed().isEmpty());++described;}
            QCOMPARE(described,html.count("<img"));
            QCOMPARE(HelpCatalog::topicForUrl(HelpCatalog::topicUrl(info.id)),info.id);
            // Validate every HTML anchor and image, not just the landing page.
            QRegularExpression links("(?:href|src)=\"([^\"]+)\"");auto matches=links.globalMatch(html);
            while(matches.hasNext()) {
                const QUrl link(matches.next().captured(1));if(link.scheme()=="https")continue;
                const auto resolved=HelpCatalog::resolve(link,HelpCatalog::topicUrl(info.id));
                QVERIFY(HelpCatalog::isImage(resolved)||!HelpCatalog::topicForUrl(resolved).isEmpty());
                if(HelpCatalog::isImage(resolved))QVERIFY2(!QImage(":"+resolved.path()).isNull(),qPrintable(resolved.toString()));
                if(!resolved.fragment().isEmpty()){
                    const auto target=HelpCatalog::article(HelpCatalog::topicForUrl(resolved));
                    QVERIFY2(QRegularExpression("(?:id|name)=\""+QRegularExpression::escape(resolved.fragment())+"\"").match(target).hasMatch(),qPrintable(resolved.toString()));
                }
            }
        }
        QCOMPARE(groups.size(),7);QVERIFY(!HelpCatalog::topic("bridge-mode"));
        QVERIFY(HelpCatalog::article("known-limitations").contains("Bridge Mode is not available"));
        QFile file(":/help/topics.json");QVERIFY(file.open(QIODevice::ReadOnly));
    }
    void ownerSafetyTextRenderingAndSearch() {
        const auto html=HelpCatalog::article("engineering-reference");
        const auto begin=html.indexOf("<h2 id=\"safety\">");QVERIFY(begin>=0);
        const auto end=html.indexOf("<h2",begin+4);QVERIFY(end>begin);
        const auto section=html.mid(begin,end-begin);QCOMPARE(section.count("<li>"),6);
        QTextDocument document;document.setHtml(section);
        QCOMPARE(QCryptographicHash::hash(document.toPlainText().simplified().toUtf8(),QCryptographicHash::Sha256).toHex(),QByteArray("17b5639c0d597ed54a47d45555ef7b82d94def904beb3d7ffce955a98f41c70b"));
        QSet<QTextList*> lists;for(auto block=document.begin();block.isValid();block=block.next())if(block.textList())lists.insert(block.textList());
        QCOMPARE(lists.size(),1);QCOMPARE((*lists.begin())->count(),6);
        for(const auto &phrase:QStringList{
            "Do not use RayNmeaSimulator as a source of navigation, collision-avoidance, vessel-control, engine-control, or other safety-critical information on an operating vessel.",
            "F12 / Stop is an application control, not an emergency-stop or safety system.",
            QString::fromUtf8("Public Engineering Reference — AS-IS")}){
            auto cursor=document.find(phrase);QVERIFY(!cursor.isNull());QVERIFY(cursor.charFormat().fontWeight()>=QFont::Bold);
        }
        for(const auto &term:QStringList{"collision-avoidance","Already queued or transmitted data","F12 / Stop"})QVERIFY(HelpCatalog::search(term).contains("engineering-reference"));
        HelpDialog dialog;dialog.resize(1150,950);dialog.showTopic("engineering-reference","safety");dialog.show();QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QCOMPARE(dialog.currentTopic(),QString("engineering-reference"));auto *browser=dialog.findChild<HelpBrowser*>();QVERIFY(browser);QVERIFY(browser->toPlainText().contains("Already queued or transmitted data cannot be recalled."));
        const auto capture=qEnvironmentVariable("RAY_SAFETY_CAPTURE");if(!capture.isEmpty())QVERIFY(dialog.grab().save(capture));
    }
    void protocolReferenceMatchesRegisteredInstruments() {
        MainWindow window;auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        QSet<QString> pgns,sentences;
        const QRegularExpression code("\\((?:PGN|SNT) ([0-9]+|[A-Z]{3})\\)");
        for(auto *combo:window.findChildren<QComboBox*>())for(int i=0;i<combo->count();++i){
            const auto match=code.match(combo->itemText(i));if(!match.hasMatch())continue;
            const auto id=match.captured(1);if(id=="000000")continue;
            (id.size()==3?sentences:pgns).insert(id);
        }
        QCOMPARE(pgns.size(),48);QCOMPARE(sentences.size(),18);
        for(const auto &pair:{qMakePair(QString("nmea2000-pgns"),pgns),qMakePair(QString("nmea0183-sentences"),sentences)}){
            const auto html=HelpCatalog::article(pair.first);QSet<QString> documented;
            auto it=QRegularExpression("id=\"message-([0-9]+|[A-Z]{3})\"").globalMatch(html);
            while(it.hasNext()){const auto id=it.next().captured(1);QVERIFY(!documented.contains(id));documented.insert(id);}
            QCOMPARE(documented,pair.second);
            QVERIFY(HelpCatalog::search(pair.first=="nmea2000-pgns"?"65359":"RMC").contains(pair.first));
        }
        QVERIFY(HelpCatalog::article("nmea2000-pgns").contains("Release is RX-only"));
        QVERIFY(HelpCatalog::article("nmea0183-sentences").contains("VDM/VDO"));
    }
    void viewerFirstArrivalPlacementAndReuse() {
        MainWindow window;auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        QVERIFY(!controller->hasLayoutWidgets());window.findChild<QAction*>("actionViewer_Rx")->trigger();
        QCOMPARE(window.currentMode(),Viewer);auto *handler=window.findChild<Nmea2000Handler*>();QVERIFY(handler);
        tN2kMsg m;
        SetN2kLatLonRapid(m,0.01,0.02);m.Source=120;handler->nmea2000MessageReceived(m);
        SetN2kTrueHeading(m,1,0.75);m.Source=120;handler->nmea2000MessageReceived(m);
        SetN2kWaterDepth(m,1,7.2,0);m.Source=120;handler->nmea2000MessageReceived(m);
        const auto widgets=controller->layoutWidgets("Navigation");QCOMPARE(widgets.size(),3);
        QCOMPARE(widgets[0]->property("Pgn").toInt(),129025);QCOMPARE(widgets[1]->property("Pgn").toInt(),127250);QCOMPARE(widgets[2]->property("Pgn").toInt(),128267);
        handler->nmea2000MessageReceived(m);QCOMPARE(controller->layoutWidgets("Navigation"),widgets);
        m.Source=121;handler->nmea2000MessageReceived(m);QCOMPARE(controller->layoutWidgets("Navigation").size(),4);
        m.SetPGN(130999);m.DataLen=8;handler->nmea2000MessageReceived(m);QCOMPARE(controller->layoutWidgets("Navigation").size(),4);
        QVERIFY(controller->layoutWidgets("Generate").isEmpty());QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
    }
    void editorialVoiceAndOperatingLimits() {
        const QStringList excluded{"codex","chatgpt","audit","qualification","fixture","milestone", "owner-approved","owner acceptance","test coverage","test evidence", "historical acceptance","synthetic","not independently qualified","not a claim"};
        for(const auto &info:HelpCatalog::topics()){
            const auto article=HelpCatalog::article(info.id);QTextDocument document;document.setHtml(article);
            QString visible=info.title+"\n"+document.toPlainText();
            auto alt=QRegularExpression("alt=\"([^\"]*)\"").globalMatch(article);
            while(alt.hasNext())visible+='\n'+alt.next().captured(1);
            for(const auto &term:excluded)QVERIFY2(!visible.contains(term,Qt::CaseInsensitive),qPrintable(info.id+": "+term));
        }
        const auto autopilot=HelpCatalog::article("autopilot");
        for(const auto &term:QStringList{"OpenCPN","Enable Sending","Stop Sending/F12","compatible","fix/status/time"})QVERIFY2(autopilot.contains(term),qPrintable(term));
        const auto actisense=HelpCatalog::article("actisense");
        QVERIFY(actisense.contains("Release builds receive only"));QVERIFY(actisense.contains("Debug transmission is experimental"));
        QVERIFY(actisense.contains("Active chartplotter route requests are disabled in both builds"));
        QVERIFY(HelpCatalog::article("signal-k").contains("does not implement Signal K subscription into Viewer"));
        QVERIFY(HelpCatalog::article("route-authorization").contains("separate authorization"));
        QVERIFY(HelpCatalog::article("game-controller").contains("F12 is not a local-motion stop"));
        QVERIFY(HelpCatalog::article("nmea2000-pgns").contains("They are not transmitted PGNs"));
    }
    void searchAndNavigation() {
        const auto imports=HelpCatalog::search("  GPX/KML Import ");QVERIFY(imports.contains("route-import"));QCOMPARE(QSet<QString>(imports.begin(),imports.end()).size(),imports.size());
        QVERIFY(HelpCatalog::search("outgoing data can be corrupted").contains("actisense"));
        QVERIFY(HelpCatalog::search("Captured vessel Layouts are snapshots").contains("layouts"));
        QVERIFY(HelpCatalog::search("no-match-483941").isEmpty());
        HelpDialog dialog;auto *browser=dialog.findChild<HelpBrowser*>();auto *search=dialog.findChild<QLineEdit*>("helpSearch");auto *tree=dialog.findChild<QTreeWidget*>();
        dialog.showTopic("route-import");dialog.showTopic("route-export");QVERIFY(browser->isBackwardAvailable());
        QVERIFY(tree->currentItem());QCOMPARE(tree->currentItem()->data(0,Qt::UserRole).toString(),QString("route-export"));
        dialog.findChild<QPushButton*>("helpBack")->click();QCOMPARE(dialog.currentTopic(),QString("route-import"));
        QCOMPARE(tree->currentItem()->data(0,Qt::UserRole).toString(),QString("route-import"));
        dialog.findChild<QPushButton*>("helpForward")->click();QCOMPARE(dialog.currentTopic(),QString("route-export"));
        QCOMPARE(tree->currentItem()->data(0,Qt::UserRole).toString(),QString("route-export"));
        search->setText("outgoing data can be corrupted");QCOMPARE(tree->topLevelItemCount(),1);QCOMPARE(tree->topLevelItem(0)->data(0,Qt::UserRole).toString(),QString("actisense"));tree->setCurrentItem(tree->topLevelItem(0));QCOMPARE(dialog.currentTopic(),QString("actisense"));
        QCOMPARE(browser->extraSelections().size(),1);
        search->clear();QCOMPARE(tree->topLevelItemCount(),7);QCOMPARE(dialog.currentTopic(),QString("actisense"));
        dialog.showTopic("actisense","top");QCOMPARE(browser->source().fragment(),QString("top"));
        dialog.showTopic("missing-topic");QVERIFY(dialog.findChild<QLabel*>("helpStatus")->text().contains("unavailable"));QCOMPARE(dialog.currentTopic(),QString("actisense"));
        dialog.findChild<QPushButton*>("helpHome")->click();QCOMPARE(dialog.currentTopic(),QString("introduction"));
    }
    void searchEnterStaysOpen() {
        QDialog origin;origin.setWindowTitle("Unrelated authoring draft");origin.setModal(true);
        origin.show();QVERIFY(QTest::qWaitForWindowExposed(&origin));
        QSignalSpy accepted(&origin,&QDialog::accepted), rejected(&origin,&QDialog::rejected);
        HelpDialog dialog(&origin);dialog.show();dialog.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&dialog));
        auto *search=dialog.findChild<QLineEdit*>("helpSearch");
        auto *tree=dialog.findChild<QTreeWidget*>("helpContents");
        QSignalSpy finished(&dialog,&QDialog::finished);
        for(const auto &query:QStringList{"GPX/KML Import","outgoing data can be corrupted","no-match-483941","GPX/KML Import"}) {
            search->setFocus();QTRY_VERIFY(search->hasFocus());search->selectAll();QTest::keyClicks(search,query);
            QTest::keyClick(search,Qt::Key_Return);
            QVERIFY(dialog.isVisible());QCOMPARE(finished.size(),0);QVERIFY(search->hasFocus());
            if(query=="GPX/KML Import")QCOMPARE(dialog.currentTopic(),HelpCatalog::search(query).first());
            if(query=="outgoing data can be corrupted")QCOMPARE(dialog.currentTopic(),QString("actisense"));
            QTest::keyClick(search,Qt::Key_Enter);QVERIFY(dialog.isVisible());
        }
        QVERIFY(tree->currentItem());QVERIFY(dialog.findChild<HelpBrowser*>()->isBackwardAvailable());
        dialog.findChild<QPushButton*>("helpBack")->click();QCOMPARE(dialog.currentTopic(),QString("actisense"));
        dialog.findChild<QPushButton*>("helpForward")->click();QCOMPARE(dialog.currentTopic(),HelpCatalog::search("GPX/KML Import").first());
        QCOMPARE(accepted.size(),0);QCOMPARE(rejected.size(),0);QVERIFY(origin.isVisible());
        QTest::keyClick(search,Qt::Key_Escape);QVERIFY(!dialog.isVisible());QVERIFY(origin.isVisible());
        dialog.show();dialog.activateWindow();QVERIFY(QTest::qWaitForWindowActive(&dialog));
        dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close)->click();
        QVERIFY(!dialog.isVisible());QVERIFY(origin.isVisible());QCOMPARE(accepted.size(),0);QCOMPARE(rejected.size(),0);
    }
    void resourceBoundaryAndExplicitLinks() {
        HelpDialog dialog;auto *browser=dialog.findChild<HelpBrowser*>();QSignalSpy blocked(browser,&HelpBrowser::blockedResource);
        QDesktopServices::setUrlHandler("https",this,"captureUrl");opened.clear();
        for(const auto &info:HelpCatalog::topics())dialog.showTopic(info.id);
        QCOMPARE(opened.size(),0);
        const auto previous=browser->source();
        for(const auto &value:QStringList{"https://example.invalid/private.png","file:///etc/passwd","qrc:/resources/ABOUT-NOTICES.txt","qrc:/help/../images/RayNmeaSimulator_1024.png","qrc:/help/%2e%2e/private","data:image/png;base64,AAAA","javascript:alert(1)","qrc:/help/introduction.html?file=secret","qrc://host/help/introduction.html"}) {
            const QUrl url(value);QVERIFY(!browser->loadResource(QTextDocument::ImageResource,url).isValid());browser->setSource(url);QCOMPARE(browser->source(),previous);
        }
        QVERIFY(blocked.size()>=18);QVERIFY(HelpCatalog::topicForUrl({}).isEmpty());
        browser->anchorClicked(QUrl("file:///private/key"));QCOMPARE(opened.size(),0);
        browser->anchorClicked(QUrl("https://www.rfstateside.com/"));QCOMPARE(opened,QList<QUrl>{QUrl("https://www.rfstateside.com/")});QCOMPARE(browser->source(),previous);
        browser->anchorClicked(QUrl("https://user:password@example.invalid/"));QCOMPARE(opened.size(),1);
        QDesktopServices::unsetUrlHandler("https");
    }
    void imagesAndPersistence() {
        HelpDialog dialog;dialog.show();QVERIFY(QTest::qWaitForWindowExposed(&dialog));auto *browser=dialog.findChild<HelpBrowser*>();
        browser->anchorClicked(QUrl("images/boat.png"));auto *image=dialog.findChild<QDialog*>("helpImageDialog");QVERIFY(image);QVERIFY(image->isVisible());QCOMPARE(image->windowTitle(),QString("Help Image"));
        QVERIFY(QTest::qWaitForWindowExposed(image));QTest::qWait(30);QVERIFY(image->frameGeometry().height()<=image->screen()->availableGeometry().height()+4);
        auto *picture=image->findChild<QWidget*>("helpImage");QVERIFY(picture);QCOMPARE(picture->property("sourceSize").toSize(),QSize(1024,1024));
        image->findChild<QPushButton*>("helpImageActual")->click();QCOMPARE(picture->size(),QSize(1024,1024));
        image->findChild<QPushButton*>("helpImageZoomIn")->click();QVERIFY(picture->width()>1024);
        image->findChild<QPushButton*>("helpImageFit")->click();QVERIFY(picture->width()<1024);image->close();
        dialog.resize(800,540);dialog.close();AppSettings settings(APP_COMPANY,APP_NAME);QVERIFY(!settings.value("Help/Geometry").toByteArray().isEmpty());
        HelpDialog restored;restored.show();QTest::qWait(25);
        // Native/offscreen window decorations can adjust client geometry by a
        // few device-independent pixels when Qt restores the saved frame.
        const auto decoration=restored.frameGeometry().size()-restored.size();
        const auto available=restored.screen()->availableGeometry();
        if(available.width()>=800+decoration.width()+32 && available.height()>=540+decoration.height()+32){
            QVERIFY(qAbs(restored.width()-800)<=4);QVERIFY(qAbs(restored.height()-540)<=4);
        }else{
            // At 200% scaling the saved frame may exceed the desktop. Qt's
            // restoreGeometry also reserves a native safety margin; require
            // a usable, bounded frame rather than a platform-specific pixel.
            QVERIFY(restored.width()>=restored.minimumWidth());QVERIFY(restored.height()>=restored.minimumHeight());
            QVERIFY(restored.frameGeometry().width()<=available.width()+4);QVERIFY(restored.frameGeometry().height()<=available.height()+4);
            QVERIFY(available.contains(restored.frameGeometry().center()));
        }
        restored.close();
    }
    void allTopicsRender() {
        const auto folder=qEnvironmentVariable("RAY_HELP_EDITORIAL_RENDER");
        if(!folder.isEmpty()){Themes::apply("Darkeum");qApp->setStyleSheet(Themes::styleSheet("Darkeum"));QVERIFY(QDir().mkpath(folder));}
        HelpDialog dialog;auto *browser=dialog.findChild<HelpBrowser*>();
        if(!folder.isEmpty()){dialog.resize(1100,760);dialog.show();QVERIFY(QTest::qWaitForWindowExposed(&dialog));}
        for(const auto &info:HelpCatalog::topics()){
            dialog.showTopic(info.id);QCOMPARE(dialog.currentTopic(),info.id);QVERIFY(browser->toPlainText().contains(info.title));QVERIFY(dialog.findChild<QLabel*>("helpStatus")->text().isEmpty());
            if(!folder.isEmpty()){
                QTest::qWait(20);auto *scroll=browser->verticalScrollBar();int page=0;
                for(int y=0;;y=qMin(scroll->maximum(),y+qMax(1,browser->viewport()->height()-40))){
                    scroll->setValue(y);QTest::qWait(10);QVERIFY(dialog.grab().save(folder+"/"+info.id+QString("-%1.png").arg(page++,2,10,QChar('0'))));
                    if(y>=scroll->maximum())break;
                }
            }
        }
        if(!folder.isEmpty()){dialog.close();qApp->setStyleSheet({});Themes::apply("System");}
    }
    void illustratedManualLayout() {
        HelpDialog dialog;dialog.resize(680,500);dialog.show();QVERIFY(QTest::qWaitForWindowExposed(&dialog));auto *browser=dialog.findChild<HelpBrowser*>();
        browser->setHtml("<h1>Illustrated procedure fixture</h1><ol><li>Inspect the current state.</li><li>Choose the next step.</li></ol>"
            "<table border='1'><tr><th>Field</th><th>Meaning</th></tr><tr><td><code>value</code></td><td>Canonical measurement</td></tr></table>"
            "<p><img src='qrc:/help/images/boat.png' width='360' /><br/><i>Figure 1. First illustration.</i></p>"
            "<p><img src='qrc:/help/images/boat.png' width='960' /><br/><i>Figure 2. Annotated step.</i></p>"
            "<p><img src='qrc:/help/images/boat.png' /><br/><i>Figure 3. Detailed reference.</i></p>");
        dialog.resize(700,510);QTest::qWait(70);int images=0;
        for(auto block=browser->document()->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){const auto fragment=it.fragment();if(fragment.charFormat().isImageFormat()){const auto image=fragment.charFormat().toImageFormat();QVERIFY(image.width()<=browser->viewport()->width());QVERIFY(qAbs(image.width()-image.height())<1);++images;}}
        QCOMPARE(images,3);QVERIFY(browser->toPlainText().contains("Figure 2. Annotated step."));QVERIFY(browser->verticalScrollBar()->maximum()>0);dialog.close();
    }
    void realIllustratedPages() {
        Themes::apply("Darkeum");qApp->setStyleSheet(Themes::styleSheet("Darkeum"));
        HelpDialog dialog;dialog.resize(780,540);dialog.show();QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *browser=dialog.findChild<HelpBrowser*>();
        for(const QString topic:{"templates","autopilot","weather","network-nmea2000","route-import","traffic-recording","game-controller","viewer-generate"}) {
            dialog.showTopic(topic);QTest::qWait(40);int images=0;
            for(auto block=browser->document()->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){
                const auto fragment=it.fragment();if(!fragment.charFormat().isImageFormat())continue;
                const auto format=fragment.charFormat().toImageFormat();++images;
                QVERIFY(format.width()>0);QVERIFY(format.width()<=browser->viewport()->width());
                const auto url=HelpCatalog::resolve(QUrl(format.name()),HelpCatalog::topicUrl(topic));
                QVERIFY(HelpCatalog::isImage(url));QVERIFY(!QImage(":"+url.path()).isNull());
            }
            QVERIFY2(images>0,qPrintable(topic));QVERIFY(browser->verticalScrollBar()->maximum()>0);
            const auto folder=qEnvironmentVariable("RAY_HELP_IMAGES");if(!folder.isEmpty()){
                browser->verticalScrollBar()->setValue(0);QVERIFY(dialog.grab().save(folder+"/manual-"+topic+".png"));
                browser->verticalScrollBar()->setValue(browser->verticalScrollBar()->maximum()/2);QTest::qWait(30);QVERIFY(dialog.grab().save(folder+"/manual-"+topic+"-figures.png"));
            }
        }
        dialog.close();qApp->setStyleSheet({});
    }
    void themes_data(){QTest::addColumn<QString>("theme");for(const auto &theme:Themes::builtInThemes())QTest::newRow(qPrintable(theme))<<theme;}
    void themes() {
        QFETCH(QString,theme);Themes::apply(theme);qApp->setStyleSheet(Themes::styleSheet(theme));
        HelpDialog dialog;dialog.showTopic("templates");dialog.show();QVERIFY(QTest::qWaitForWindowExposed(&dialog));QTest::qWait(40);
        QVERIFY(dialog.frameGeometry().height()<=dialog.screen()->availableGeometry().height()+4);
        auto *browser=dialog.findChild<HelpBrowser*>();QVERIFY(!browser->toPlainText().isEmpty());
        const auto source=browser->source();Themes::apply(theme);QTest::qWait(20);QCOMPARE(browser->source(),source);
        dialog.resize(680,440);QTest::qWait(30);auto *close=dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close);QVERIFY(dialog.rect().contains(close->mapTo(&dialog,QPoint(0,0))));
        const auto images=qEnvironmentVariable("RAY_HELP_IMAGES");if(!images.isEmpty()){QDir().mkpath(images);dialog.resize(1050,720);QTest::qWait(35);QVERIFY(dialog.grab().save(images+"/"+theme+".png"));}
        dialog.findChild<QLineEdit*>("helpSearch")->setFocus();QTest::keyClick(&dialog,Qt::Key_Escape);QVERIFY(!dialog.isVisible());qApp->setStyleSheet({});
    }
    void customThemeAndLiveSwitch() {
        Themes::apply("Dark");HelpDialog dialog;dialog.show();dialog.showTopic("route-import");dialog.showTopic("route-export");auto *browser=dialog.findChild<HelpBrowser*>();const auto source=browser->source();
        const auto folder=AppPaths::themesDirectory();QVERIFY(folder.startsWith(TestProfile::root()));QVERIFY(QDir().mkpath(folder));
        QFile custom(folder+"/HelpFixtureCustom.qss");QVERIFY(custom.open(QIODevice::WriteOnly));
        QVERIFY(custom.write("QTextBrowser { background: #203040; color: #f0f0f0; } QTreeWidget { background: #182838; color: white; }")>0);custom.close();
        QVERIFY(Themes::available().contains("HelpFixtureCustom"));Themes::apply("HelpFixtureCustom");qApp->setStyleSheet(Themes::styleSheet("HelpFixtureCustom"));
        QTest::qWait(35);QCOMPARE(browser->source(),source);QVERIFY(browser->isBackwardAvailable());QCOMPARE(browser->palette().color(QPalette::Base),QColor("#203040"));dialog.close();qApp->setStyleSheet({});Themes::apply("System");
    }
    void endUserGettingStarted() {
        for(const QString topic:{"introduction","system-requirements","first-launch","viewer-generate","quick-start","default-layout"}){
            const auto article=HelpCatalog::article(topic);QVERIFY(!article.contains("dependencies.py"));QVERIFY(!article.contains("cmake --"));QVERIFY(!article.contains("CMAKE_PREFIX_PATH"));
        }
        const auto requirements=HelpCatalog::article("system-requirements");
        for(const QString hardware:{"CANable","Waveshare","Raspberry Pi 5","250000","termination"})QVERIFY(requirements.contains(hardware));
        QVERIFY(HelpCatalog::article("first-launch").contains("id=\"profile-paths\""));
        const auto images=qEnvironmentVariable("RAY_HELP_IMAGES");if(!images.isEmpty()){
            Themes::apply("Darkeum");qApp->setStyleSheet(Themes::styleSheet("Darkeum"));HelpDialog dialog;dialog.resize(1050,720);dialog.show();QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            for(const QString topic:{"system-requirements","first-launch"}){dialog.showTopic(topic);QTest::qWait(40);QVERIFY(dialog.grab().save(images+"/end-user-"+topic+".png"));}
            dialog.close();qApp->setStyleSheet({});
        }
    }
    void modalContextHelp_data() {
        QTest::addColumn<QString>("actionText");QTest::addColumn<QString>("topic");QTest::addColumn<bool>("preexisting");
        QTest::newRow("template-manager")<<QString("Templates…")<<QString("templates")<<false;
        QTest::newRow("template-manager-reuse")<<QString("Templates…")<<QString("templates")<<true;
        QTest::newRow("template-editor")<<QString("Save as Template…")<<QString("templates")<<false;
        QTest::newRow("startup-workspace")<<QString("Startup Workspace…")<<QString("default-layout")<<false;
        QTest::newRow("startup-template")<<QString("Startup Workspace…")<<QString("templates")<<false;
    }
    void modalContextHelp() {
        QFETCH(QString,actionText);QFETCH(QString,topic);QFETCH(bool,preexisting);
        AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Workspace/StartupMode",0);settings.setValue("Preference/StyleSheetName","Darkeum");
        MainWindow window;window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *controller=window.findChild<MainController*>();QVERIFY(controller);
        controller->setMode(Viewer);controller->addWidgetAsComboboxTextChanged("130312","Retained measurement","Environment");controller->setMode(Sender);
        const auto layout=controller->captureLayout();QSignalSpy replaced(controller,&MainController::layoutObjectsChanged);
        auto *start=window.findChild<QAction*>("actionStart_Sending");QSignalSpy started(start,&QAction::triggered);
        QPointer<HelpDialog> original;
        if(preexisting){window.findChild<QAction*>("actionHelpContents")->trigger();original=window.findChild<HelpDialog*>();QVERIFY(original);}
        QAction *command=nullptr;for(auto *action:window.findChildren<QAction*>())if(action->text()==actionText){command=action;break;}QVERIFY(command);
        bool visited=false,nestedQueued=false;QPointer<QDialog> outer;QTimer driver;driver.setInterval(30);
        connect(&driver,&QTimer::timeout,&window,[&]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog || visited)return;
            if(actionText=="Startup Workspace…" && topic=="templates" && dialog->objectName()=="startupWorkspaceDialog"){
                if(nestedQueued)return;nestedQueued=true;outer=dialog;dialog->findChild<QComboBox*>()->setCurrentIndex(2);dialog->findChild<QLineEdit*>()->setText("Unsaved startup path");
                for(auto *button:dialog->findChildren<QPushButton*>())if(button->text()=="Browse…")QTimer::singleShot(0,button,&QPushButton::click);return;
            }
            visited=true;driver.stop();
            const auto cleanup=qScopeGuard([&]{if(!preexisting)if(auto *help=window.findChild<HelpDialog*>())help->close();dialog->reject();if(outer)QTimer::singleShot(0,outer,&QDialog::reject);});
            QTimer::singleShot(8000,dialog,&QDialog::reject);
            auto *list=dialog->findChild<QListWidget*>("templateList");QString selection;
            if(list){QVERIFY(list->count()>1);list->setCurrentRow(1);selection=list->currentItem()->data(Qt::UserRole).toString();}
            auto *draft=dialog->findChild<QLineEdit*>("templateName");if(draft)draft->setText("Unsaved contextual Help draft");
            auto *targets=outer?dialog->findChild<QComboBox*>():nullptr;QString target;if(targets){QVERIFY(targets->count()>1);targets->setCurrentIndex(1);target=targets->currentData().toString();}
            QWidget *focus=list?static_cast<QWidget*>(list):draft?static_cast<QWidget*>(draft):targets?static_cast<QWidget*>(targets):static_cast<QWidget*>(dialog);
            dialog->activateWindow();focus->setFocus();QVERIFY(QTest::qWaitForWindowActive(dialog));
            QTest::keyClick(focus,Qt::Key_F1);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<HelpDialog*>(),700);
            auto *help=window.findChild<HelpDialog*>();QCOMPARE(help->currentTopic(),topic);
            if(preexisting)QCOMPARE(help,original.data());
            QCOMPARE(help->parentWidget(),dialog);QVERIFY(!help->isModal());
            QVERIFY(QTest::qWaitForWindowActive(help));auto *search=help->findChild<QLineEdit*>("helpSearch");search->setFocus();QTRY_VERIFY(search->hasFocus());
            auto *contents=help->findChild<QTreeWidget*>();QVERIFY(contents->currentItem());QCOMPARE(contents->currentItem()->data(0,Qt::UserRole).toString(),topic);
            QTest::keyClick(search,Qt::Key_F1);QCOMPARE(window.findChildren<HelpDialog*>().size(),1);QCOMPARE(help->currentTopic(),topic);
            if(list && !preexisting){
                QDialog notice(help);notice.setModal(true);notice.show();QVERIFY(QTest::qWaitForWindowActive(&notice));QTest::keyClick(&notice,Qt::Key_F1);
                QCOMPARE(help->parentWidget(),dialog);QCOMPARE(window.findChildren<HelpDialog*>().size(),1);QVERIFY(notice.isVisible());notice.reject();help->activateWindow();QVERIFY(QTest::qWaitForWindowActive(help));
            }
            QVERIFY(dialog->isVisible());if(list)QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(),selection);if(draft)QCOMPARE(draft->text(),QString("Unsaved contextual Help draft"));
            if(targets)QCOMPARE(targets->currentData().toString(),target);if(outer)QCOMPARE(outer->findChild<QLineEdit*>()->text(),QString("Unsaved startup path"));
            QCOMPARE(controller->captureLayout(),layout);QCOMPARE(replaced.size(),0);QCOMPARE(started.size(),0);QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
            const auto images=qEnvironmentVariable("RAY_HELP_IMAGES");if(!images.isEmpty()){
                const auto name=images+"/modal-"+QString::fromLatin1(QTest::currentDataTag());QVERIFY(help->grab().save(name+".png"));QVERIFY(dialog->grab().save(name+"-origin.png"));
            }
            if(preexisting){dialog->reject();QVERIFY(!dialog->isVisible());return;}
            QTest::keyClick(help,Qt::Key_Escape);QVERIFY(!help->isVisible());QVERIFY(dialog->isVisible());
            if(list)QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(),selection);if(draft)QCOMPARE(draft->text(),QString("Unsaved contextual Help draft"));
            if(targets)QCOMPARE(targets->currentData().toString(),target);if(outer)QCOMPARE(outer->findChild<QComboBox*>()->currentIndex(),2);
        });driver.start();command->trigger();driver.stop();QVERIFY(visited);
        QCOMPARE(controller->captureLayout(),layout);QCOMPARE(replaced.size(),0);QCOMPARE(started.size(),0);QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);window.hide();
        if(preexisting)QVERIFY(original.isNull());
    }
    void setupContextHelp() {
        MainWindow window;window.show();auto *setup=window.findChild<DialogSetup*>();QVERIFY(setup);
        setup->show();setup->activateWindow();QVERIFY(QTest::qWaitForWindowActive(setup));
        auto *focus=setup->findChild<QLineEdit*>();QVERIFY(focus);const auto draft=focus->text();focus->setFocus();QTest::keyClick(focus,Qt::Key_F1);
        QTRY_VERIFY(window.findChild<HelpDialog*>());auto *help=window.findChild<HelpDialog*>();QCOMPARE(help->currentTopic(),QString("connection-troubleshooting"));
        QVERIFY(QTest::qWaitForWindowActive(help));QVERIFY(setup->isVisible());QCOMPARE(focus->text(),draft);
        help->close();setup->reject();QVERIFY(!window.findChild<NmeaSenderWorker*>()->isSending());window.hide();
    }
    void mainWindowSafetyAndKeyboard() {
        MainWindow window;window.show();QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *start=window.findChild<QAction*>("actionStart_Sending");auto *help=window.findChild<QAction*>("actionHelpContents");QVERIFY(help);QVERIFY(help->shortcuts().contains(QKeySequence(Qt::Key_F1)));QVERIFY(window.findChild<QAction*>("actionAbout"));QVERIFY(window.findChild<QAction*>("actionSupportRayNmeaSimulator"));
        QSignalSpy started(start,&QAction::triggered);QVERIFY(!start->isChecked());help->trigger();auto *dialog=window.findChild<HelpDialog*>();QVERIFY(dialog);QVERIFY(dialog->isVisible());QVERIFY(!dialog->isModal());help->trigger();QCOMPARE(window.findChildren<HelpDialog*>().size(),1);
        dialog->activateWindow();QVERIFY(QTest::qWaitForWindowActive(dialog));
        auto *article=dialog->findChild<HelpBrowser*>();article->setFocus();QTRY_VERIFY(article->hasFocus());
        auto *search=dialog->findChild<QLineEdit*>("helpSearch");QTest::keyClick(article,Qt::Key_F,Qt::ControlModifier);QTRY_VERIFY(search->hasFocus());QTest::keyClick(dialog,Qt::Key_F12);QVERIFY(!start->isChecked());QCOMPARE(started.size(),0);
        dialog->close();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        window.activateWindow();QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *property=window.findChild<QTableView*>("tableView");QVERIFY(property);property->setFocus();QTRY_VERIFY(property->hasFocus());QTest::keyClick(property,Qt::Key_F1);QTRY_VERIFY(window.findChild<HelpDialog*>());QCOMPARE(window.findChild<HelpDialog*>()->currentTopic(),QString("property-editor"));window.findChild<HelpDialog*>()->close();window.hide();
    }
};
int main(int argc,char **argv){
    QTemporaryDir profile;if(!profile.isValid())return 2;
    qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());qputenv("RAYNMEA_TEST_CREDENTIALS","1");
    QList<QByteArray> args;for(int i=0;i<argc;++i)args<<argv[i];args<<"--no-nmea";
    QList<char*> pointers;for(auto &arg:args)pointers<<arg.data();int count=pointers.size();pointers<<nullptr;
    QApplication app(count,pointers.data());QApplication::setApplicationName(APP_NAME);QApplication::setOrganizationName(APP_COMPANY);QApplication::setApplicationVersion(APP_VERSION);Themes::initializeSystemStyle();
    // Owner preview reuses the real MainWindow with the existing isolated test
    // object/resource variant; production Windows settings are never selected.
    if(app.arguments().contains("--help-preview")){
        AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Preference/StyleSheetName","Darkeum");settings.sync();
        MainWindow window;window.show();
        QTimer::singleShot(0,&window,[&window]{window.findChild<QAction*>("actionHelpContents")->trigger();});
        return app.exec();
    }
    HelpTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "help_tests.moc"
