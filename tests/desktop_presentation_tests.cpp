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
#include <QDockWidget>
#include <QFile>
#include <QGroupBox>
#include <QLineEdit>
#include <QMenu>
#include <QNetworkProxy>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QStyle>
#include <QMessageBox>
#include "appsettings.h"
#include "mainwindow.h"
#include "themes.h"
#include "desktopicons.h"
#include "windowstate.h"

namespace {
void writeFile(const QString& path, const QByteArray& contents)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), contents.size());
}
void capture(QWidget& widget, const QString& name)
{
    const QString directory = qEnvironmentVariable("M210_EVIDENCE");
    if (directory.isEmpty()) return;
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(widget.grab().save(directory + "/" + name + ".png"));
}
}

class DesktopPresentationTests : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.clear();
        settings.setValue("Nmea2000/InterfaceType", "None");
        settings.setValue("Preservation/Sentinel", "unchanged");
        settings.setValue("Preference/PromptSaveLayoutOnClose",false);
    }
    void screenRecovery()
    {
        const QList<QRect> screens {QRect(-1920, 40, 1920, 1040), QRect(0, 40, 1920, 1040)};
        const QRect original(-1700, 100, 1100, 700);
        QCOMPARE(WindowState::boundedFrame(original, screens, 1), original);
        const QRect disconnected = WindowState::boundedFrame(QRect(6000, 200, 1100, 700), screens, 1);
        QVERIFY(screens[1].contains(disconnected));
        const QRect huge = WindowState::boundedFrame(QRect(-100, -100, 4000, 3000), screens, 1);
        QVERIFY(screens[0].contains(huge) || screens[1].contains(huge));
        QVERIFY(screens[1].contains(WindowState::boundedFrame(QRect(), screens, 1)));
        QCOMPARE(WindowState::boundedFrame(original, {}), original);
    }
    void applicationIconAndTextOnlyTabs()
    {
        MainWindow window;
        window.resize(1200, 800); window.show();
        window.findChild<QAction*>("actionLoad_Default")->trigger();
        QTest::qWait(30);
        auto* area = window.findChild<QMdiArea*>("mdiAreaMain"); QVERIFY(area);
        auto* tabs = area->findChild<QTabBar*>(QString(), Qt::FindDirectChildrenOnly); QVERIFY(tabs);
        QVERIFY(tabs->count() > 1);
        tabs->moveTab(0, 1);
        QStringList titles;
        for (int i = 0; i < tabs->count(); ++i) titles << tabs->tabText(i);
        auto* setup = window.findChild<DialogSetup*>(); QVERIFY(setup);
        for (const QString theme : {"Light", "Dark", "None", "missing-theme", "Light", "Dark"}) {
            AppSettings settings(APP_COMPANY, APP_NAME);
            settings.setValue("Preference/StyleSheetName", theme); settings.sync();
            window.HotReloadSettings(); QTest::qWait(30);
            const QImage actual = window.windowIcon().pixmap(64, 64).toImage();
            QVERIFY(!actual.isNull());
            QCOMPARE(actual, DesktopIcons::forPalette(qApp->palette()).pixmap(64, 64).toImage());
            QCOMPARE(setup->windowIcon().pixmap(64, 64).toImage(), actual);
            QDialog secondary(&window);
            QCOMPARE(secondary.windowIcon().pixmap(64, 64).toImage(), actual);
            for (int i = 0; i < tabs->count(); ++i) {
                QVERIFY(tabs->tabIcon(i).isNull());
                QCOMPARE(tabs->tabText(i), titles[i]);
            }
            QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
        }
        window.findChild<MainController*>()->addSubWindowForGroup("Icon check", "Text-only new group");
        QTest::qWait(30);
        QCOMPARE(tabs->tabText(tabs->count() - 1), QString("Icon check"));
        for (int i = 0; i < tabs->count(); ++i) QVERIFY(tabs->tabIcon(i).isNull());
        QTimer::singleShot(0,[] {
            for(auto *top:QApplication::topLevelWidgets())
                if(top->objectName()=="viewerEntryDialog")
                    if(auto *keep=top->findChild<QPushButton*>("viewerKeepExisting"))keep->click();
        });
        window.findChild<QAction*>("actionViewer_Rx")->setChecked(true);
        window.findChild<QAction*>("actionSimulator_Tx")->setChecked(true);
        QTest::qWait(30);
        for (int i = 0; i < tabs->count(); ++i) QVERIFY(tabs->tabIcon(i).isNull());
    }
    void iconContrastAndSmallSizes()
    {
        const QImage light = DesktopIcons::forPalette(Themes::palette("Light")).pixmap(1024, 1024).toImage();
        const QImage dark = DesktopIcons::forPalette(Themes::palette("Dark")).pixmap(1024, 1024).toImage();
        QCOMPARE(light, QPixmap(":/images/RayNmeaSimulator_1024.png").toImage());
        // Compare the compiled resource with this checkout's approved derivative:
        // old qrc/build assets must not pass merely by agreeing with each other.
        const QPixmap approved(QFINDTESTDATA("../images/RayNmeaSimulator_1024.png"));
        QVERIFY(!approved.isNull());
        QCOMPARE(light, approved.toImage());
        QCOMPARE(dark, light); // Transparency must survive Dark as well as Light.
        QCOMPARE(light.pixelColor(0, 0).alpha(), 0);
        QCOMPARE(dark.pixelColor(0, 0).alpha(), 0);
        // Open space outside the ship must not become a white theme backing.
        QCOMPARE(light.pixelColor(50, 50).alpha(), 0);
        QCOMPARE(dark.pixelColor(50, 50).alpha(), 0);
        for (const QString theme : {"Dark", "Light"}) {
            const auto palette = Themes::palette(theme);
            const auto icon = DesktopIcons::forPalette(palette);
            const qreal dpr = qApp->devicePixelRatio();
            QPixmap preview(QSize(420, 100) * dpr); preview.setDevicePixelRatio(dpr);
            preview.fill(palette.color(QPalette::Window));
            QPainter painter(&preview); int x = 10;
            for (int size : {16, 24, 32, 48, 64}) {
                const auto image = icon.pixmap(QSize(size, size), dpr);
                QCOMPARE(image.deviceIndependentSize(), QSizeF(size, size));
                painter.drawPixmap(x, 10, image); x += size + 20;
            }
            painter.end();
            QLabel gallery; gallery.setPixmap(preview); gallery.show();
            capture(gallery, theme + "-icon-sizes");
        }
    }
    void nativeWindowRoundTrip()
    {
        AppSettings settings(APP_COMPANY, APP_NAME);
        QMainWindow first;
        first.setCentralWidget(new QWidget);
        auto* dock = new QDockWidget("Properties", &first);
        dock->setObjectName("properties"); dock->setWidget(new QWidget);
        first.addDockWidget(Qt::RightDockWidgetArea, dock);
        first.resize(760, 520); first.move(QGuiApplication::primaryScreen()->availableGeometry().topLeft() + QPoint(50, 50));
        first.show(); QVERIFY(QTest::qWaitForWindowExposed(&first));
        // Native decoration/configure events can arrive after exposure on X11.
        QTest::qWait(100);
        dock->hide();
        const QSize savedSize = first.size();
        const QPoint savedPosition = first.pos();
        WindowState::save(first, settings);
        QMainWindow second;
        second.setCentralWidget(new QWidget);
        auto* restored = new QDockWidget("Properties", &second);
        restored->setObjectName("properties"); restored->setWidget(new QWidget);
        second.addDockWidget(Qt::RightDockWidgetArea, restored);
        WindowState::restore(second, settings); second.show();
        QVERIFY(QTest::qWaitForWindowExposed(&second)); QTest::qWait(100);
        QCOMPARE(second.size(), savedSize);
        if (!QGuiApplication::platformName().startsWith("wayland")) QCOMPARE(second.pos(), savedPosition);
        QVERIFY(restored->isHidden());
        second.showMaximized(); QTest::qWait(100);
        WindowState::save(second, settings);
        QMainWindow maximized;
        WindowState::restore(maximized, settings); maximized.show(); QTest::qWait(30);
        QVERIFY(maximized.isMaximized());
        QCOMPARE(settings.value("Preservation/Sentinel").toString(), QString("unchanged"));
        settings.setValue("Window/Geometry", QByteArray("invalid"));
        settings.setValue("Window/Panels", QByteArray("invalid"));
        QMainWindow invalid;
        WindowState::restore(invalid, settings); invalid.show(); QTest::qWait(30);
        QVERIFY(!invalid.isMaximized());
        QVERIFY(invalid.width() > 100 && invalid.height() > 100);
        bool visible = false;
        for (auto* screen : QGuiApplication::screens())
            visible |= screen->availableGeometry().contains(invalid.frameGeometry());
        QVERIFY(visible);
    }
    void candidateCatalogAndResources()
    {
        QCOMPARE(Themes::builtInThemes(), QStringList({"System","Light","Dark","Darkeum","Teal Dark","Porcelain"}));
        QCOMPARE(Themes::available(),Themes::builtInThemes());
        QVERIFY(!Themes::available().contains("None"));
        QVERIFY(!QFile::exists(":/stylesheets/darkeum/darkeum.qss"));
        for(const auto &name:Themes::builtInThemes()) {
            if(Themes::isSystem(name)) {QVERIFY(Themes::styleSheet(name).isEmpty());continue;}
            QVERIFY(QFile::exists(Themes::source(name)));
            const auto sheet=Themes::styleSheet(name);
            QVERIFY(!sheet.isEmpty());QVERIFY(!sheet.contains("@COMMON@"));
            QVERIFY(!sheet.contains("@accent@"));QVERIFY(!sheet.contains("url("));
            auto palette=Themes::palette(name);
            QVERIFY(palette.color(QPalette::Text)!=palette.color(QPalette::Base));
            QVERIFY(palette.color(QPalette::HighlightedText)!=palette.color(QPalette::Highlight));
        }
        QVERIFY(Themes::palette("Teal Dark").color(QPalette::Button)==QColor("#006060"));
        QVERIFY(Themes::palette("Porcelain").color(QPalette::Window)!=Themes::palette("Light").color(QPalette::Window));
    }
    void systemRestoresPlatform()
    {
        Themes::apply("System");const auto style=QApplication::style()->objectName();
        QVERIFY(!QCoreApplication::testAttribute(Qt::AA_SetPalette));
        for(const auto &name:QStringList{"Dark","Light","Teal Dark","Darkeum","Porcelain","None"}) {
            Themes::apply(name);
            if(!Themes::isSystem(name)) QVERIFY(QCoreApplication::testAttribute(Qt::AA_SetPalette));
            Themes::apply("System");
            QCOMPARE(QApplication::style()->objectName(),style);
            QVERIFY(!QCoreApplication::testAttribute(Qt::AA_SetPalette));
            QVERIFY(Themes::styleSheet("System").isEmpty());
        }
    }
    void unchangedSeedUpgradeAndOwnerPreservation()
    {
        const auto target=Themes::externalDirectories().first()+"/Dark/Dark.qss";
        QFile old(QFINDTESTDATA("fixtures/theme-dark-v1.qss"));QVERIFY(old.open(QIODevice::ReadOnly));
        writeFile(target,old.readAll());Themes::initializeEditableResources();
        QFile upgraded(target);QVERIFY(upgraded.open(QIODevice::ReadOnly));
        QVERIFY(upgraded.readAll().contains("@COMMON@"));upgraded.close();
        writeFile(target,"/* owner theme */ QLabel { color: #abcdef; }");
        Themes::initializeEditableResources();
        QVERIFY(Themes::styleSheet("Dark").contains("owner theme"));
        QVERIFY(QFile::remove(target)); // Only disposable test profile.
        Themes::initializeEditableResources();
    }
    void systemClearsDescendantPaletteOverrides()
    {
        AppSettings settings(APP_COMPANY,APP_NAME);MainWindow window;
        for(const auto &theme:QStringList{"Dark","Light","Teal Dark","Darkeum","Porcelain"}) {
            settings.setValue("Preference/StyleSheetName",theme);settings.sync();window.HotReloadSettings();
            settings.setValue("Preference/StyleSheetName","System");settings.sync();window.HotReloadSettings();
            for(const auto &object:QStringList{"tableView","treeView","toolBarSending"}) {
                auto *widget=window.findChild<QWidget*>(object);QVERIFY(widget);
                const auto expected=QApplication::palette(widget);
                QCOMPARE(widget->palette().color(QPalette::WindowText),expected.color(QPalette::WindowText));
                QCOMPARE(widget->palette().color(QPalette::Base),expected.color(QPalette::Base));
            }
            QVERIFY(!QCoreApplication::testAttribute(Qt::AA_SetPalette));
        }
    }
    void candidatePreferenceSurvivesRecreation()
    {
        for(const auto &theme:Themes::builtInThemes()) {
            AppSettings settings(APP_COMPANY,APP_NAME);
            settings.setValue("Preference/StyleSheetName",theme);settings.sync();
            { MainWindow first;QCOMPARE(first.styleSheet(),Themes::styleSheet(theme)); }
            { MainWindow reopened;QCOMPARE(reopened.styleSheet(),Themes::styleSheet(theme));
              auto *setup=reopened.findChild<DialogSetup*>();setup->show();
              QCOMPARE(setup->findChild<QComboBox*>("comboBoxAppTheme")->currentText(),theme);
              setup->reject();
              QVERIFY(!reopened.findChild<QAction*>("actionStart_Sending")->isChecked()); }
            QCOMPARE(settings.value("Preference/StyleSheetName").toString(),theme);
        }
    }
    void themeDiscoveryAndPrecedence()
    {
        const QString root = Themes::externalDirectories().first();
        const QString css = root + "/Owner/Owner.qss";
        writeFile(css, "QLabel { image: url(asset.png); } /* owner */");
        QVERIFY(Themes::available().contains("Owner"));
        QCOMPARE(Themes::source("Owner"), css);
        QVERIFY(Themes::styleSheet("Owner").contains(root + "/Owner/asset.png"));
        writeFile(root + "/Dark.qss", "/* owner override */ QLabel { color: red; }");
        QCOMPARE(Themes::source("Dark"), root + "/Dark.qss");
        QVERIFY(Themes::styleSheet("Dark").contains("owner override"));
        QVERIFY(QFile::remove(root + "/Dark.qss"));
        QCOMPARE(Themes::source("Dark"), root + "/Dark/Dark.qss"); // Editable first-run seed.
        QCOMPARE(Themes::styleSheet("missing-theme"), Themes::styleSheet("Dark"));
        writeFile(root + "/Empty.qss", "  ");
        QCOMPARE(Themes::styleSheet("Empty"), Themes::styleSheet("Dark"));
        QVERIFY(QFile::remove(root + "/Empty.qss"));
        const QString probe = "m210-" + QFileInfo(TestProfile::root()).fileName();
        const auto directories = Themes::externalDirectories();
        for (const auto& directory : directories) {
            const auto path = directory + "/" + probe + ".qss";
            QVERIFY(!QFile::exists(path));
            writeFile(path, "/* precedence fixture */");
        }
        for (const auto& directory : directories) {
            const auto path = directory + "/" + probe + ".qss";
            QCOMPARE(Themes::source(probe), path);
            QVERIFY(QFile::remove(path));
        }
        QVERIFY(Themes::source("../Dark").isEmpty());
        QVERIFY(Themes::styleSheet("None").isEmpty());
        for (const QString name : {"darkeum", "Aqua", "ConsoleStyle"}) {
            QVERIFY(!Themes::available().contains(name));
            QVERIFY(Themes::source(name).isEmpty());
            QCOMPARE(Themes::styleSheet(name),Themes::styleSheet("Dark"));
        }
        QVERIFY(QFile::remove(css));
    }
    void themeAndMenuIntegration_data()
    {
        QTest::addColumn<QString>("theme");
        for (const auto& name : {"System", "Dark", "Light", "Darkeum", "Teal Dark", "Porcelain", "None", "darkeum"})
            QTest::newRow(name) << QString(name);
    }
    void themeAndMenuIntegration()
    {
        QFETCH(QString, theme);
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Preference/StyleSheetName", theme); settings.sync();
        MainWindow window;
        window.resize(1200, 800); window.show(); QTest::qWait(50);
        QVERIFY(!window.windowIcon().isNull());
        QCOMPARE(window.styleSheet(), Themes::styleSheet(theme));
        auto* sending = window.findChild<QAction*>("actionStart_Sending");
        QVERIFY(sending && !sending->isChecked() && !sending->isEnabled());
        auto* view = window.findChild<QMenu*>("menuView"); QVERIFY(view);
        auto* dock = window.findChild<QDockWidget*>("dockWidgetTable"); QVERIFY(dock);
        QVERIFY(view->actions().contains(dock->toggleViewAction()));
        dock->toggleViewAction()->trigger(); QVERIFY(dock->isHidden());
        dock->toggleViewAction()->trigger(); QVERIFY(!dock->isHidden());
        view->popup(window.mapToGlobal(QPoint(120, 25)));
        view->setActiveAction(dock->toggleViewAction()); QTest::qWait(20);
        capture(*view, theme + "-view-menu"); view->hide();
        auto* debug = window.findChild<QMenu*>("menuTest"); QVERIFY(debug);
        QVERIFY(!debug->actions().contains(sending));
        QVERIFY(window.findChild<QMenu*>("menuSending")->actions().contains(sending));
        QVERIFY(!window.findChild<QAction*>("actionPrint_Tree_Model")->isEnabled());
        window.findChild<QAction*>("actionLoad_Default")->trigger();
        QTest::qWait(50); capture(window, theme + "-main");
        auto* setup = window.findChild<DialogSetup*>(); QVERIFY(setup);
        setup->show(); QTest::qWait(30);
        auto* choices = setup->findChild<QComboBox*>("comboBoxAppTheme"); QVERIFY(choices);
        QCOMPARE(choices->currentText(), Themes::selectionName(theme));
        for (const auto& name : {"labelVesselCrusingSpeed", "labelPortFuelTankSize", "labelStarboardFuelTankSize", "labelDefaultLatitude"}) {
            auto* label = setup->findChild<QLabel*>(name); QVERIFY(label);
            QVERIFY2(label->width() >= label->fontMetrics().horizontalAdvance(label->text()), name);
        }
        capture(*setup, theme + "-setup");
        auto* preferences = setup->findChild<QWidget*>("comboBoxAppTheme")->parentWidget();
        for (auto* tabs : setup->findChildren<QTabWidget*>()) {
            for (int i = 0; i < tabs->count(); ++i)
                if (tabs->widget(i) == preferences || tabs->widget(i)->isAncestorOf(choices)) tabs->setCurrentIndex(i);
        }
        QTest::qWait(20); capture(*setup, theme + "-preferences");
        setup->reject();
        window.findChild<QAction*>("actionExit")->trigger();
        QVERIFY(!window.isVisible());
        settings.sync(); QVERIFY(!settings.value("Window/Geometry").toByteArray().isEmpty());
        QCOMPARE(settings.value("Preservation/Sentinel").toString(), QString("unchanged"));
        QCOMPARE(settings.value("Preference/StyleSheetName").toString(), theme);
    }
    void liveThemeReloadAndCancel()
    {
        MainWindow window; window.show();
        auto* setup = window.findChild<DialogSetup*>(); setup->show();
        auto* choices = setup->findChild<QComboBox*>("comboBoxAppTheme");
        choices->setCurrentText("Light");
        setup->findChild<QPushButton*>("pushButtonSave")->click();
        QCOMPARE(window.styleSheet(), Themes::styleSheet("Light"));
        QCOMPARE(setup->styleSheet(), window.styleSheet());
        choices->setCurrentText("Dark"); setup->reject();
        AppSettings settings(APP_COMPANY, APP_NAME);
        QCOMPARE(settings.value("Preference/StyleSheetName").toString(), QString("Light"));
        QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
        window.close();
    }
    void externalThemeSetupSelection()
    {
        const QString name="Harbor Custom";
        const QString path=Themes::externalDirectories().first()+"/"+name+"/"+name+".qss";
        writeFile(path,"/* installed at runtime */ QLabel { color: #abcdef; }\n");
        {
            MainWindow window;window.show();
            auto *setup=window.findChild<DialogSetup*>();setup->show();
            auto *choices=setup->findChild<QComboBox*>("comboBoxAppTheme");QVERIFY(choices);
            QVERIFY(choices->findText(name)>=0);
            for(const auto &builtIn:Themes::builtInThemes())QVERIFY(choices->findText(builtIn)>=0);
            choices->setCurrentText(name);
            for(auto *tabs:setup->findChildren<QTabWidget*>())
                for(int i=0;i<tabs->count();++i)
                    if(tabs->widget(i)->isAncestorOf(choices))tabs->setCurrentIndex(i);
            QTest::qWait(30);capture(*setup,"custom-theme-setup");
            choices->showPopup();QTest::qWait(30);capture(*choices->view()->window(),"custom-theme-choices");choices->hidePopup();
            setup->findChild<QPushButton*>("pushButtonSave")->click();
            QCOMPARE(window.styleSheet(),Themes::styleSheet(name));
            QVERIFY(!window.findChild<QAction*>("actionStart_Sending")->isChecked());
        }
        MainWindow reopened;
        auto *setup=reopened.findChild<DialogSetup*>();setup->show();
        QCOMPARE(setup->findChild<QComboBox*>("comboBoxAppTheme")->currentText(),name);
        QCOMPARE(reopened.styleSheet(),Themes::styleSheet(name));setup->reject();
        AppSettings settings(APP_COMPANY,APP_NAME);
        QCOMPARE(settings.value("Preference/StyleSheetName").toString(),name);
        QVERIFY(QFile::remove(path));
    }
    void controlsGallery_data()
    {
        QTest::addColumn<QString>("theme");
        QTest::newRow("Dark") << QString("Dark");
        QTest::newRow("Light") << QString("Light");
        for(const auto &name:QStringList{"Darkeum","Teal Dark","Porcelain"}) QTest::newRow(qPrintable(name)) << name;
    }
    void controlsGallery()
    {
        QFETCH(QString, theme);
        qApp->setPalette(Themes::palette(theme));
        QWidget gallery; gallery.setStyleSheet(Themes::styleSheet(theme));
        auto* layout = new QVBoxLayout(&gallery);
        auto* label = new QLabel(theme + " — controls and selection states"); layout->addWidget(label);
        auto* edit = new QLineEdit("Selected text stays readable"); edit->selectAll(); layout->addWidget(edit);
        auto* disabled = new QLineEdit("Disabled text"); disabled->setEnabled(false); layout->addWidget(disabled);
        auto* combo = new QComboBox; combo->addItems({"One", "Two", "Three"}); layout->addWidget(combo);
        auto* spin = new QSpinBox; spin->setValue(42); layout->addWidget(spin);
        auto* check = new QCheckBox("Checked"); check->setChecked(true); layout->addWidget(check);
        layout->addWidget(new QCheckBox("Unchecked"));
        auto* radio = new QRadioButton("Selected radio"); radio->setChecked(true); layout->addWidget(radio);
        auto* tabs = new QTabWidget; tabs->addTab(new QWidget, "Selected"); tabs->addTab(new QWidget, "Other"); layout->addWidget(tabs);
        auto* table = new QTableWidget(2, 2); table->setHorizontalHeaderLabels({"Parameter", "Value"});
        table->setItem(0, 0, new QTableWidgetItem("Selected row")); table->setItem(0, 1, new QTableWidgetItem("42"));
        table->selectRow(0); layout->addWidget(table);
        auto* button = new QPushButton("Keyboard focus"); layout->addWidget(button);
        gallery.resize(540, 620); gallery.show(); button->setFocus(); QTest::qWait(40);
        QVERIFY(check->isChecked()); QVERIFY(radio->isChecked());
        const QPalette palette = gallery.palette();
        QVERIFY(palette.color(QPalette::Text) != palette.color(QPalette::Base));
        QVERIFY(palette.color(QPalette::HighlightedText) != palette.color(QPalette::Highlight));
        // Check readable normal, disabled and selected text independent of font rendering.
        auto luminance = [](QColor color) {
            auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : qPow((c + 0.055) / 1.055, 2.4); };
            return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
        };
        auto contrast = [luminance](QColor a, QColor b) {
            const double x = luminance(a), y = luminance(b);
            return (qMax(x, y) + .05) / (qMin(x, y) + .05);
        };
        for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            QVERIFY(contrast(palette.color(group, QPalette::Text), palette.color(group, QPalette::Base)) >= 4.5);
            QVERIFY(contrast(palette.color(group, QPalette::HighlightedText), palette.color(group, QPalette::Highlight)) >= 4.5);
        }
        capture(gallery, theme + "-controls");
        combo->showPopup(); QTest::qWait(30);
        capture(*combo->view()->window(), theme + "-combo"); combo->hidePopup();
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir profile(QDir::currentPath() + "/m210-profile-XXXXXX");
    if (!profile.isValid()) return 2;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    // Both normal startup and Setup reload stay off marine interfaces.
    QList<QByteArray> arguments;
    for (int i = 0; i < argc; ++i) arguments << argv[i];
    arguments << "--no-nmea";
    QList<char*> pointers;
    for (auto& argument : arguments) pointers << argument.data();
    int count = pointers.size();
    pointers << nullptr;
    // Qt's default offscreen screen is 800x800, smaller than the recovery
    // window plus its decorations. Own a realistic desktop for this fixture;
    // retain all geometry assertions and leave native displays unchanged.
    QTemporaryFile screenConfig(QDir::currentPath() + "/desktop-screen-XXXXXX.json");
    if (qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        if (!screenConfig.open()) return 2;
        screenConfig.write(R"({"screens":[{"name":"fixture","width":1920,"height":1200}]})");
        screenConfig.flush();
        qputenv("QT_QPA_PLATFORM", ("offscreen:configfile=" + QFileInfo(screenConfig.fileName()).fileName()).toUtf8());
    }
    QApplication app(count, pointers.data()); app.setStyle("Fusion");
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    DesktopPresentationTests tests;
    // QtTest must not interpret the application-only standalone switch.
    return QTest::qExec(&tests, argc, argv);
}
#include "desktop_presentation_tests.moc"
