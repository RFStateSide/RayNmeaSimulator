/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
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

#include <QStandardItemModel>
#include "expansioninstruments.h"
#include "windlassstatus.h"
#include "aisclassbposition129039.h"
#include "aisclassbstatic129810.h"
#include "aisclassbstatic129809.h"
#include "vesselelectricalsystem.h"
#include "maincontroller.h"
#include "desktopicons.h"
#include <QCheckBox>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QSignalBlocker>
#include <QSaveFile>
#include <QScopeGuard>
#include <QScopedValueRollback>
#include <QJsonParseError>
#include <algorithm>
#include <cmath>
#include <functional>
#include <type_traits>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include "batterystatus127508.h"
#include "chargerstatus127507.h"
#include "cogsogvtg.h"
#include "datetimezda.h"
#include "geographicpositiongll.h"
#include "globalpositioninggga.h"
#include "headinghdg.h"
#include "rudder127245.h"
#include "spacer000000.h"
#include "QThread"
#include "attitude127257.h"
#include "cogsog129026.h"
#include "crosstrack129283.h"
#include "datetime129033.h"
#include "enginedynamic127489.h"
#include "enginerapid127488.h"
#include "fluidlevel127505.h"
#include "generatordatetime.h"
#include "gnsspositiondata129029.h"
#include "heave127252.h"
#include "latlong129025.h"
#include "magneticvariation127258.h"
#include "mainwindow.h"
#include "applicationlog.h"
#include "navigation129284.h"
#include "outsideenvironmental130310.h"
#include "environmentalinstruments.h"
#include "rateofturn127251.h"
#include "speed128259.h"
#include "systemtime126992.h"
#include "temperatureextended130316.h"
#include "temperaturewidget130312.h"
#include "transmissiondynamic127493.h"
#include "trimtab130576.h"
#include "trueheadinghdt.h"
#include "uihelper.h"
#include "vesselheading127250.h"
#include "n2kconst.h"
#include "generatorengine.h"
#include "helper.h"
#include "dcstatus127506.h"
#include "depth128267.h"
#include "generatorweather.h"
#include "recommendedmingnssrmc.h"


namespace {
// Coordinate properties are doubles, but an explicit Property Editor entry is
// canonical text. Do not replace that text during model->widget signal feedback.
void updateCoordinateParameter(PgnTableModel *model, int id, const QString &field, double value)
{
    for (const auto &row : model->getPgnParams(id)) {
        if (row[0] != field) continue;
        bool valid = false;
        const double current = row[1].toDouble(&valid);
        if (valid && current == value) return;
        break;
    }
    model->changeParam(id, field, QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest));
}
// Movement stores heading/course in HeadingReference. Sentence fields declare
// their own reference; use the same variation/deviation contract as own-vessel AIS.
double movementSentenceAngle(const GeneratorVesselMovement *movement, bool heading, bool trueReference)
{
    double value = heading ? movement->Heading() : movement->COG();
    if (!std::isfinite(value) || value == N2kDoubleNA) return N2kDoubleNA;
    const auto reference = movement->HeadingReference();
    if (reference != N2khr_true && reference != N2khr_magnetic) return N2kDoubleNA;
    if ((reference == N2khr_true) != trueReference) {
        const double variation = movement->Variation();
        const double deviation = heading ? movement->Deviation() : 0.0;
        if (!std::isfinite(variation) || variation == N2kDoubleNA
            || !std::isfinite(deviation) || deviation == N2kDoubleNA) return N2kDoubleNA;
        value += (trueReference ? 1.0 : -1.0) * (variation + deviation);
    }
    return Convert::DegreesToRadians(Convert::NormalizeHeading(Convert::RadiansToDegrees(value)));
}
// Matches NMEA0183SetGGA/ParseGGA: UTC seconds, decimal degrees, metres.
QList<QList<QString>> ggaParameters(double latitude, double longitude)
{
    return {{"GPSTime", "0.0", N2kConst::S, "double"},
            {"Latitude", QString::number(latitude, 'f', 6), N2kConst::DEG, "double"},
            {"Longitude", QString::number(longitude, 'f', 6), N2kConst::DEG, "double"},
            {"GPSQualityIndicator", "0", "", "int"},
            {"SatelliteCount", "0", "", "int"},
            {"HDOP", "0.0", "", "double"},
            {"Altitude", "0.0", N2kConst::M, "double"},
            {"GeoidalSeparation", "0.0", N2kConst::M, "double"},
            {"DGPSAge", "0.0", N2kConst::S, "double"},
            {"DGPSReferenceStationID", "0", "", "int"}};
}
QList<QPair<double, double>> ggaRanges()
{
    // Application limits; height/geoid separation may be negative. No valid fix by default.
    return {{0, 86399.9999}, {-90, 90}, {-180, 180}, {0, 8}, {0, 99},
            {0, 999.9}, {-10000, 100000}, {-1000, 1000}, {0, 9999.9}, {0, 1023}};
}

void normalizeAtmosphericPressure(QList<QList<QString>> &parameters, QList<QPair<double, double>> &ranges)
{
    for (int row = 0; row < parameters.size(); ++row) {
        auto &parameter = parameters[row];
        if (parameter.size() < 4 || parameter[0] != "AtmosphericPressure") continue;
        // Legacy weather/outside layouts explicitly recorded hPa. New layouts record Pa,
        // so conversion occurs once, without guessing units from the numeric magnitude.
        if (parameter[2].compare(N2kConst::HPA, Qt::CaseInsensitive) == 0) {
            bool valid = false;
            const double hPa = parameter[1].toDouble(&valid);
            if (valid) parameter[1] = QString::number(hPa * 100.0, 'g', 15);
        }
        parameter[2] = N2kConst::PA;
        // Preserve existing ranges for unrelated fields and the model's missing-range fallback.
        while (ranges.size() <= row) ranges.append({0, 255});
        ranges[row] = {0, 110000};
    }
}


}

MainController::MainController(
    PgnTableController *pgnTableCtrl, PgnTreeController *pgnTreeCtrl, QMdiArea *mdiAreaPtr, NmeaSenderWorker *nmeaSenderWorker, QObject *parent)
    : QObject(parent)
    , pgnTableController(pgnTableCtrl)
    , pgnTreeController(pgnTreeCtrl)
    , mdiArea(mdiAreaPtr)
    , nmeaSenderWorker(nmeaSenderWorker)
{
    ApplicationLog::onBindingsEnabled(this,[this]{bindingSnapshot();});
    m_electrical = new VesselElectricalSystem(pgnTableController->getPgnTableModel(),
        [this](int instance) { return generatorEngines.value(instance).first; }, this);
    connect(this, &MainController::layoutAboutToReplace, m_electrical, [this] { m_electrical->setGenerate(false); });
    connect(this, &MainController::layoutReplaced, m_electrical, [this] { m_electrical->setGenerate(m_mode == Sender); });
    connect(this, &MainController::layoutObjectsChanged, m_electrical, [this] { m_electrical->synchronize(); });
    connect(pgnTableController->getPgnTableModel(), &PgnTableModel::parameterChanged, this,
        [this](int, const QString &field, const QVariant &) {
            if (field != "EngineInstance" || m_restoringLayout || m_rebindingOnly) return;
            invalidateGeneratorBindings("engine instance edit");
            // Named widget display setters block their own notifications. Rebind after
            // they have consumed this model edit, without allowing any old queued update.
            QTimer::singleShot(0, this, [this] { rebuildGeneratorBindings(); });
        });
    // mainWindow = qobject_cast<MainWindow*>(parent); // Safe cast
    // if (!mainWindow) {
    //     qWarning() << "Parent object is not of type MainWindow";
    // }
}

void MainController::setNumberOfWidgetWide(int columns)
{
    if (columns < 1 || columns > 4 || columns == numberOfWidgetWide) return;
    numberOfWidgetWide = columns;
    reflowLayouts();
}

void MainController::addSubWindowForGroup(const QString &groupName, const QString &groupDescription)
{
    // Create a new widget to act as the content of the sub-window
    QWidget *subWindowContent = new QWidget;

    // Create a main vertical layout for the scroll area
    QVBoxLayout *mainVerticalLayout = new QVBoxLayout(subWindowContent);

    // Create a grid layout for placing widgets dynamically
    QGridLayout *gridLayout = new QGridLayout();


    gridLayout->setContentsMargins(0,0,0,0);
    gridLayout->setAlignment(Qt::AlignTop);


    // Create a label and combo box for selecting items
    QLabel *comboLabel = new QLabel("Select " + groupName + " Item:");
    comboLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QComboBox *comboBox = new QComboBox();
    addComboBoxItems(groupName, *comboBox);

    // Add the combo box and label to the grid (row 0, column 0)
    auto *header = new QHBoxLayout;
    header->addWidget(comboLabel); // Label spans 2 columns
    header->addWidget(comboBox, 1);
    mainVerticalLayout->addLayout(header);   // ComboBox spans 3 columns

    mainVerticalLayout->addLayout(gridLayout);
    mainVerticalLayout->addStretch(1);
    mainVerticalLayout->setSizeConstraint(QLayout::SetMinimumSize);

    // Create a QScrollArea and add the sub-window content to it
    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidget(subWindowContent); // Add the widget to be scrolled
    scrollArea->setWidgetResizable(true);    // Allow resizing
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Create the sub-window and set its content as the scroll area
    QMdiSubWindow *subWindow = new QMdiSubWindow;
    subWindow->setWidget(scrollArea);
    subWindow->setWindowTitle(groupName);
    subWindow->setToolTip(groupDescription);

    // Ensure both the sub-window and content expand to fill available space
    subWindowContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    subWindow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Store the combo box and sub-window in the map for later access
    comboBoxSubWindowMap.insert(comboBox, subWindow);

    connect(comboBox, &QComboBox::currentTextChanged, this, &MainController::onComboBoxTextChanged);

    // Add the sub-window to the mdiArea
    mdiArea->addSubWindow(subWindow);
    subWindow->show();
    DesktopIcons::clearLayoutTabs(*mdiArea);

    // Initialize layout state for this sub-form (grid layout)
    SubFormLayoutState layoutState = {gridLayout, 1, 0, QSet<QPair<int, int>>()}; // Start after the combo box in row 1
    subFormLayoutMap.insert(groupName, layoutState);
    emit layoutObjectsChanged();
}

void MainController::addComboBoxItems(const QString &groupName, QComboBox &comboBox)
{
    comboBox.addItem("None");

    if (groupName == "Navigation") {
        comboBox.addItem("System Date Time (PGN 126992)");
        comboBox.addItem("Vessel Heading (PGN 127250)");
        comboBox.addItem("Rate of Turn (PGN 127251)");
        comboBox.addItem("Heave (PGN 127252)");
        comboBox.addItem("Attitude (PGN 127257)");
        comboBox.addItem("Magnetic Variation (PGN 127258)");
        comboBox.addItem("Speed (PGN 128259)");
        comboBox.addItem("Depth (PGN 128267)");
        comboBox.addItem("Lat Long (PGN 129025)");
        comboBox.addItem("SOG COG (PGN 129026)");
        comboBox.addItem("GNSS Position (PGN 129029)");
        comboBox.addItem("Local Date Time (PGN 129033)");
        comboBox.addItem("Cross Track Error (PGN 129283)");
        comboBox.addItem("Navigation (PGN 129284)");
        comboBox.addItem("Global Positioning (SNT GGA)");
        comboBox.addItem("Recommended Min Gnss (SNT RMC)");
        comboBox.addItem("Heading (SNT HDG)");
        comboBox.addItem("True Heading (SNT HDT)");
        comboBox.addItem("Geographic Position (SNT GLL)");
        comboBox.addItem("SOG COG (SNT VTG)");
        comboBox.addItem("Date Time (SNT ZDA)");
        comboBox.addItem("Spacer (PGN 000000)");
    } else if (groupName == "Propulsion") {
        comboBox.addItem("Engine Rapid (PGN 127488)");
        comboBox.addItem("Engine Dynamic (PGN 127489)");
        comboBox.addItem("Transmission Dynamic (PGN 127493)");
        comboBox.addItem("Rudder (PGN 127245)");
        comboBox.addItem("Trim Tab (PGN 130576)");
        comboBox.addItem("Spacer (PGN 000000)");
    } else if (groupName == "Environment") {
        comboBox.addItem("Wind (PGN 130306)");
        comboBox.addItem("Humidity (PGN 130313)");
        comboBox.addItem("Atmospheric Pressure (PGN 130310)");
        comboBox.addItem("Temperature (PGN 130312)");
        comboBox.addItem("Temperature Extened (PGN 130316)");
        comboBox.addItem("Spacer (PGN 000000)");
    } else if (groupName == "Tanks") {
        comboBox.addItem("Fluid Level (PGN 127505)");
        comboBox.addItem("Spacer (PGN 000000)");
    } else if (groupName == "Electrical") {
        comboBox.addItem("DC Status (PGN 127506)");
        comboBox.addItem("Charger Status (PGN 127507)");
        comboBox.addItem("Battery Status (PGN 127508)");
        comboBox.addItem("Spacer (PGN 000000)");
    } else if (groupName == "Communication") {
        comboBox.addItem("Spacer (PGN 000000)");
        comboBox.addItem("AIS Class B Static Part A (PGN 129809)");
        comboBox.addItem("AIS Class B Static Part B (PGN 129810)");
        comboBox.addItem("AIS Class B Position (PGN 129039)");
    } else if (groupName == "Other") {
        comboBox.addItem("Spacer (PGN 000000)");
        comboBox.addItem("Windlass Operating Status (PGN 128777)");
        comboBox.addItem("Windlass Monitoring Status (PGN 128778)");
    } else if (groupName == "Generate") {
        comboBox.addItem("Engine / Outboard (PGN -127489)");
        comboBox.addItem("Trans / Lower Unit (PGN -127493)");
        comboBox.addItem("Vessel Movement (PGN -129025)");
        comboBox.addItem("Date Time (PGN -129033)");
        comboBox.addItem("Weather (PGN -130311)");
        comboBox.addItem("Spacer (PGN 000000)");
    } else {
        qWarning() << "Unknown group name";
    }
    for(const auto &d:Expansion::definitions())if(d.group==groupName)
        comboBox.addItem(d.label+(d.code.size()==3?" (SNT ":" (PGN ")+d.code+")");
    // Preserve the placeholder and complete item data; reorder existing model
    // rows rather than rebuilding strings or routing through selection signals.
    // Generate contains optional sources, not the instrument catalog.
    if (groupName != "Generate") {
        auto *items = qobject_cast<QStandardItemModel *>(comboBox.model());
        Q_ASSERT(items);
        QList<QList<QStandardItem *>> rows;
        while (items->rowCount() > 1) rows.append(items->takeRow(1));
        auto numeric = [](const QString &text) {
            static const QRegularExpression pgn("\\(PGN ([0-9]+)\\)");
            const auto match = pgn.match(text);
            return match.hasMatch() ? match.captured(1).toInt() : -1;
        };
        std::stable_sort(rows.begin(), rows.end(), [&](const auto &a, const auto &b) {
            const auto at = a.first()->text(), bt = b.first()->text();
            const int ap = numeric(at), bp = numeric(bt);
            if ((ap >= 0) != (bp >= 0)) return ap >= 0;
            if (ap >= 0) return ap < bp;
            return QString::compare(at, bt, Qt::CaseInsensitive) < 0;
        });
        for (const auto &row : rows) items->appendRow(row);
    }
}

void MainController::setOpenWeatherMapApiKey(const QString &apiKey)
{
    m_openWeatherMapApiKey = apiKey;
    for (const auto &group : groupWidgetsMap) for (auto *widget : group)
        if (auto *weather = qobject_cast<GeneratorWeather*>(widget)) weather->setOpenWeatherMapApiKey(apiKey);
}

void MainController::onComboBoxTextChanged(const QString &selectedItem)
{
    if (selectedItem == "None")
        return;

    QComboBox *comboBox = qobject_cast<QComboBox *>(sender());
    if (!comboBox) {
        return;
    }

    // Find the positions of (PGN and (SNT tags
    int startOfPgn = selectedItem.indexOf("(PGN");
    int startOfSnt = selectedItem.indexOf("(SNT");

    // Determine the position of the first tag, whether (PGN or (SNT
    int firstTagPos = std::min(startOfPgn != -1 ? startOfPgn : selectedItem.length(),
                               startOfSnt != -1 ? startOfSnt : selectedItem.length());

    // Extract the label up to the first tag, whichever comes first
    QString selectedItemLabel = selectedItem.left(firstTagPos).trimmed();

    // Extract PGN and SNT values if available
    QString selectedItemPgn = (startOfPgn != -1) ?
                              selectedItem.mid(startOfPgn + 5, selectedItem.indexOf(')', startOfPgn) - startOfPgn - 5)
                              : QString();
    QString selectedItemSnt = (startOfSnt != -1) ? selectedItem.mid(startOfSnt + 5, 3) : QString();

    // Support addGeneratorWidget when on other group sub-windows
    bool isGenerateGroup = selectedItemPgn.contains("-");

    // Reset the combo box to "None" after selecting an item
    comboBox->setCurrentIndex(0);

    // Use the existing PgnTableModel and PgnTreeModel instead of creating new ones
    if (comboBox && comboBoxSubWindowMap.contains(comboBox)) {
        QMdiSubWindow *subWindow = comboBoxSubWindowMap.value(comboBox);
        QString groupName = isGenerateGroup ? "Generate" : subWindow->windowTitle();

        bool pgnOk = false;
        const int numericPgn = selectedItemPgn.toInt(&pgnOk);
        if (pgnOk && numericPgn >= -0x3ffff && numericPgn <= 0x3ffff) {
            addWidgetAsComboboxTextChanged(selectedItemPgn, selectedItemLabel, groupName);
        } else if (selectedItemSnt.length() == 3) {
            addWidgetAsComboboxTextChanged(selectedItemSnt, selectedItemLabel, groupName);
        } else {
            qWarning() << "Invalid PGN or SNT number";
        }
    }
}

void MainController::onWidgetInstanceValueChanged(int idX)
{
    //qDebug() << "MainController::onWidgetInstanceValueChanged" << idX;
    if (currentIdX == idX) {
        pgnTableController->refreshPgnParameters(idX);
    }

    /*    // TODO: Implement the following logic once tree view is implemented
    // Find the corresponding index in the tree view
    QModelIndex instanceIndex = pgnTreeController->findInstanceIndexByIdX(idX);

    if (instanceIndex.isValid()) {
        // Expand the tree view to show the found instance
        pgnTreeController->getTreeView()->expand(instanceIndex);
        // Select the found instance
        pgnTreeController->getTreeView()->setCurrentIndex(instanceIndex);
    } else {
        qWarning() << "Invalid instanceIndex. Cannot expand the tree.";
    }
*/
}

QMdiSubWindow *MainController::createSubWindow(const QString &groupName, const QString &groupDescription)
{
    // Create a widget to serve as the content of the sub-window
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);

    // Add a QLabel for the group description
    QLabel *label = new QLabel(groupDescription);
    layout->addWidget(label);

    widget->setLayout(layout);

    // Create a new MdiSubWindow and set the widget as its central widget
    QMdiSubWindow *subWindow = new QMdiSubWindow;
    subWindow->setWidget(widget);
    subWindow->setWindowTitle(groupName);
    //subWindow->setToolTip(groupDescription);

    return subWindow;
}

void MainController::bindWidgetsBasedOnEngineInstanceToGenerator(int engineInstance)
{
    if (m_restoringLayout) return;
    auto genEnginePair = generatorEngines.value(engineInstance, qMakePair(nullptr, false));
    auto engRapidPair = engineRapids.value(engineInstance, qMakePair(nullptr, false));
    auto genTransLowerUnitPair = generatorTransLowerUnits.value(engineInstance, qMakePair(nullptr, false));
    auto engDynamicPair = engineDynamics.value(engineInstance, qMakePair(nullptr, false));
    auto transDynamicPair = transmissionDynamics.value(engineInstance, qMakePair(nullptr, false));

    // Check if GeneratorEngine is present for the given engine instance and if not prompt the user to add it
    if (!genEnginePair.first && m_mode == Sender && !m_rebindingOnly && !m_promptingMovementDependencies) {
        // Ask user create a GeneratorEngine widget if it doesn't exist
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this->mdiArea,
                                      "Optional GeneratorEngine for Engine " + QString::number(engineInstance + 1),
                                      "Create an Engine / Outboard simulation source?\n\n"
                                      "Existing engine instruments display measurements; they do not simulate an engine. "
                                      "No keeps those instruments and their captured values.",
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            addGeneratorWidget("Engine / Outboard (PGN -127489)");
        } else {
            qWarning() << "GeneratorEngine widget is null for engine instance:" << engineInstance;
        }
    }

    genEnginePair = generatorEngines.value(engineInstance, qMakePair(nullptr, false));
    genTransLowerUnitPair = generatorTransLowerUnits.value(engineInstance, qMakePair(nullptr, false));
    engRapidPair = engineRapids.value(engineInstance, qMakePair(nullptr, false));
    engDynamicPair = engineDynamics.value(engineInstance, qMakePair(nullptr, false));
    transDynamicPair = transmissionDynamics.value(engineInstance, qMakePair(nullptr, false));

    if (genEnginePair.first && !genEnginePair.second) {
        // Do nothing, GeneratorEngine widget is present
    }

    // Check if EngineRapid127488 is present and not bound to generatorEngineWidget and generatorEngineWidget is present
    if (genEnginePair.first && engRapidPair.first && !engRapidPair.second/* && genEnginePair.second*/) {
        qDebug() << "Binding EngineRapid127488 to GeneratorEngine for engine instance:" << engineInstance;

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineSpeedChanged,
            engRapidPair.first,
            [genEnginePair, engRapidPair]() { engRapidPair.first->setEngineSpeed(genEnginePair.first->EngineSpeed()); },
            Qt::QueuedConnection, "setEngineSpeed, EngineSpeed");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineBoostPressureChanged,
            engRapidPair.first,
            [genEnginePair, engRapidPair]() { engRapidPair.first->setEngineBoostPressure(genEnginePair.first->EngineBoostPressure()); },
            Qt::QueuedConnection, "setEngineBoostPressure, EngineBoostPressure");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineTiltTrimChanged,
            engRapidPair.first,
            [genEnginePair, engRapidPair]() { engRapidPair.first->setEngineTiltTrim(genEnginePair.first->EngineTiltTrim()); },
            Qt::QueuedConnection, "setEngineTiltTrim, EngineTiltTrim");

        // Mark EngineRapid127488 as bound
        engineRapids[engineInstance].second = true;
    }

    // Check if EngineDynamic127489 is present and not bound and generatorEngineWidget is present
    if (genEnginePair.first && engDynamicPair.first && !engDynamicPair.second/* && genEnginePair.second*/) {
        qDebug() << "Binding EngineDynamic127489 to GeneratorEngine for engine instance:" << engineInstance;

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineOilPressChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
                engDynamicPair.first->setEngineOilPress(genEnginePair.first->EngineOilPress());
            }
        },
            Qt::QueuedConnection, "setEngineOilPress, EngineOilPress");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineOilTempChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
                engDynamicPair.first->setEngineOilTemp(genEnginePair.first->EngineOilTemp());
            }
        },
            Qt::QueuedConnection, "setEngineOilTemp, EngineOilTemp");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineCoolantTempChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
                engDynamicPair.first->setEngineCoolantTemp(genEnginePair.first->EngineCoolantTemp());
            }
        },
            Qt::QueuedConnection, "setEngineCoolantTemp, EngineCoolantTemp");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::AltenatorVoltageChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
                engDynamicPair.first->setAltenatorVoltage(genEnginePair.first->AltenatorVoltage());
            }
        },
            Qt::QueuedConnection, "setAltenatorVoltage, AltenatorVoltage");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::FuelRateChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
                engDynamicPair.first->setFuelRate(genEnginePair.first->FuelRate());
            }
        },
            Qt::QueuedConnection, "setFuelRate, FuelRate");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineHoursChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
            engDynamicPair.first->setEngineHours(genEnginePair.first->EngineHours());
            }
        },
            Qt::QueuedConnection, "setEngineHours, EngineHours");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineCoolantPressChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
            engDynamicPair.first->setEngineCoolantPress(genEnginePair.first->EngineCoolantPress());
            }
        },
            Qt::QueuedConnection, "setEngineCoolantPress, EngineCoolantPress");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineFuelPressChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
            engDynamicPair.first->setEngineFuelPress(genEnginePair.first->EngineFuelPress());
            }
        },
            Qt::QueuedConnection, "setEngineFuelPress, EngineFuelPress");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineLoadChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
            engDynamicPair.first->setEngineLoad(genEnginePair.first->EngineLoad());
            }
        },
            Qt::QueuedConnection, "setEngineLoad, EngineLoad");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineTorqueChanged,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
            engDynamicPair.first->setEngineTorque(genEnginePair.first->EngineTorque());
            }
        },
            Qt::QueuedConnection, "setEngineTorque, EngineTorque");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineDiscreteStatus1Changed,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
            engDynamicPair.first->setEngineDiscreteStatus1(genEnginePair.first->EngineDiscreteStatus1());
            }
        },
            Qt::QueuedConnection, "setEngineDiscreteStatus1, EngineDiscreteStatus1");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineDiscreteStatus2Changed,
            engDynamicPair.first,
            [genEnginePair, engDynamicPair]() {
            if(genEnginePair.first->isEnabled()) {
            engDynamicPair.first->setEngineDiscreteStatus2(genEnginePair.first->EngineDiscreteStatus2());
            }
        },
            Qt::QueuedConnection, "setEngineDiscreteStatus2, EngineDiscreteStatus2");

        // Mark EngineDynamic127489 as bound
        engineDynamics[engineInstance].second = true;
    }

    // Check if TransmissionDynamic127493 is present and not bound
    if (genTransLowerUnitPair.first && transDynamicPair.first && !transDynamicPair.second) {
        qDebug() << "Binding TransmissionDynamic127493 to GeneratorTransLowerUnit:" << engineInstance;

        connectWhileSenderAlive(
            genTransLowerUnitPair.first,
            &GeneratorTransLowerUnit::TransmissionGearChanged,
            transDynamicPair.first,
            [genTransLowerUnitPair, transDynamicPair]() {
            if(genTransLowerUnitPair.first->isEnabled()) {
            transDynamicPair.first->setTransmissionGear(genTransLowerUnitPair.first->TransmissionGear());
            }
        },
            Qt::QueuedConnection, "setTransmissionGear, TransmissionGear");

        connectWhileSenderAlive(
            genTransLowerUnitPair.first,
            &GeneratorTransLowerUnit::OilPressureChanged,
            transDynamicPair.first,
            [genTransLowerUnitPair, transDynamicPair]() {
            if(genTransLowerUnitPair.first->isEnabled()) {
            transDynamicPair.first->setOilPressure(genTransLowerUnitPair.first->OilPressure());
            }
        },
            Qt::QueuedConnection, "setOilPressure, OilPressure");

        connectWhileSenderAlive(
            genTransLowerUnitPair.first,
            &GeneratorTransLowerUnit::OilTemperatureChanged,
            transDynamicPair.first,
            [genTransLowerUnitPair, transDynamicPair]() {
            if(genTransLowerUnitPair.first->isEnabled()) {
            transDynamicPair.first->setOilTemperature(genTransLowerUnitPair.first->OilTemperature());
            }
        },
            Qt::QueuedConnection, "setOilTemperature, OilTemperature");

        connectWhileSenderAlive(
            genTransLowerUnitPair.first,
            &GeneratorTransLowerUnit::DiscreteStatus1Changed,
            transDynamicPair.first,
            [genTransLowerUnitPair, transDynamicPair]() {
            if(genTransLowerUnitPair.first->isEnabled()) {
            transDynamicPair.first->setDiscreteStatus1(genTransLowerUnitPair.first->DiscreteStatus1());
            }
        },
            Qt::QueuedConnection, "setDiscreteStatus1, DiscreteStatus1");

        // Mark TransmissionDynamic127493 as bound
        transmissionDynamics[engineInstance].second = true;
    }

    // Check if GeneratorTransLowerUnit is present and not bound to generatorEngineWidget
    if (genEnginePair.first && genTransLowerUnitPair.first && !genTransLowerUnitPair.second) {
        qDebug() << "Binding GeneratorTransLowerUnit to GeneratorEngine for engine instance:" << engineInstance;

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineRunningChanged,
            genTransLowerUnitPair.first,
            [genEnginePair, genTransLowerUnitPair]() {
            if(genEnginePair.first->isEnabled()) {
            genTransLowerUnitPair.first->setEngineRunning(genEnginePair.first->EngineRunning());
            }
        },
            Qt::QueuedConnection, "setEngineRunning, EngineRunning");

        connectWhileSenderAlive(
            genEnginePair.first,
            &GeneratorEngine::EngineLoadChanged,
            genTransLowerUnitPair.first,
            [genEnginePair, genTransLowerUnitPair]() {
            if(genEnginePair.first->isEnabled()) {
            genTransLowerUnitPair.first->setEngineLoad(genEnginePair.first->EngineLoad());
            }
        },
            Qt::QueuedConnection, "setEngineLoad, EngineLoad");

        // Mark GeneratorTransLowerUnit as bound
        generatorTransLowerUnits[engineInstance].second = true;
    }
}

void MainController::unBindWidgetsBasedOnEngineInstanceToGenerator(int engineInstance)
{
    auto genEnginePair = generatorEngines.value(engineInstance, qMakePair(nullptr, false));
    auto engRapidPair = engineRapids.value(engineInstance, qMakePair(nullptr, false));
    auto genTransLowerUnitPair = generatorTransLowerUnits.value(engineInstance, qMakePair(nullptr, false));
    auto engDynamicPair = engineDynamics.value(engineInstance, qMakePair(nullptr, false));
    auto transDynamicPair = transmissionDynamics.value(engineInstance, qMakePair(nullptr, false));

    // Check if EngineRapid127488 is present and bound to generatorEngineWidget
    if (engRapidPair.first && engRapidPair.second/* && genEnginePair.second*/) {
        qDebug() << "Un-Binding EngineRapid127488 to GeneratorEngine for engine instance:" << engineInstance;

        QObject::disconnect(genEnginePair.first, &GeneratorEngine::EngineSpeedChanged, engRapidPair.first, NULL);

        // Mark EngineRapid127488 as un-bound
        engineRapids[engineInstance].second = false;
    }

    // Check if EngineDynamic127489 is present and bound and generatorEngineWidget is present
    if (engDynamicPair.first && engDynamicPair.second/* && genEnginePair.second*/) {
        qDebug() << "Un-Binding EngineDynamic127489 to GeneratorEngine for engine instance:" << engineInstance;

        QObject::disconnect(genEnginePair.first, &GeneratorEngine::EngineOilPressChanged, engDynamicPair.first, NULL);

        // Mark EngineDynamic127489 as un-bound
        engineDynamics[engineInstance].second = false;
    }

    // Check if TransmissionDynamic127493 is present and bound
    if (transDynamicPair.first && transDynamicPair.second) {
        qDebug() << "Un-Binding TransmissionDynamic127493 to GeneratorTransLowerUnit:" << engineInstance;

        QObject::disconnect(genTransLowerUnitPair.first, &GeneratorTransLowerUnit::TransmissionGearChanged, transDynamicPair.first, NULL);

        // Mark TransmissionDynamic127493 as un-bound
        transmissionDynamics[engineInstance].second = false;
    }

    // Check if GeneratorTransLowerUnit is present and bound to generatorEngineWidget
    if (genTransLowerUnitPair.first && genTransLowerUnitPair.second) {
        qDebug() << "Un-Binding GeneratorTransLowerUnit to GeneratorEngine for engine instance:" << engineInstance;

        QObject::disconnect(genEnginePair.first, &GeneratorEngine::EngineRunningChanged, genTransLowerUnitPair.first, NULL);

        // Mark GeneratorTransLowerUnit as un-bound
        generatorTransLowerUnits[engineInstance].second = false;
    }

}

void MainController::bindGeneratorVesselMovementToGeneratorEngine(QWidget *widget, int engineInstance)
{
    if (m_restoringLayout) return;
    // Check if GeneratorEngine is present for the given engine instance
    if (!generatorEngines.contains(engineInstance) && m_mode == Sender && !m_rebindingOnly) {
        // Ask user create a GeneratorEngine widget if it doesn't exist
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this->mdiArea,
                                      "Optional GeneratorEngine for Engine " + QString::number(engineInstance + 1),
                                      "Create an Engine / Outboard simulation source for Vessel Movement?\n\n"
                                      "This source supplies RPM and engine measurements. Existing engine instruments do not replace it. "
                                      "No keeps existing instruments and navigation connections. Starting vessel movement still requires "
                                      "the configured engine/transmission sources and engine profiles.",
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            addGeneratorWidget("Engine / Outboard (PGN -127489)");
        } else {
            qWarning() << "GeneratorEngine widget is null for engine instance:" << engineInstance;
        }
    }

    // Check if GeneratorTransLowerUnit is present for the given engine instance
    if (!generatorTransLowerUnits.contains(engineInstance) && m_mode == Sender && !m_rebindingOnly) {
        // Ask user create a GeneratorTransLowerUnit widget if it doesn't exist
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this->mdiArea,
                                      "Optional GeneratorTransLowerUnit for Engine " + QString::number(engineInstance + 1),
                                      "Create a Transmission / Lower Unit simulation source for Vessel Movement?\n\n"
                                      "This source supplies transmission measurements. Existing transmission instruments do not replace it. "
                                      "No keeps existing instruments and navigation connections. Starting vessel movement still requires "
                                      "the configured engine/transmission sources and engine profiles.",
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            addGeneratorWidget("Transmission / Lower Unit (PGN -127493)");
        } else {
            qWarning() << "GeneratorTransLowerUnit widget is null for engine instance:" << engineInstance;
        }
    }

    GeneratorVesselMovement *generatorVesselMovementWidget = qobject_cast<GeneratorVesselMovement *>(widget);
    if (!generatorVesselMovementWidget) return;
    generatorVesselMovementWidget->setActiveRouteChooser([guard=QPointer<MainController>(this)](QWidget *parent){return guard&&guard->activeRouteChooser?guard->activeRouteChooser(parent):ReceivedRoutes::Route{};});
    generatorVesselMovementWidget->setReceivedRouteProvider([guard=QPointer<MainController>(this)] {
        return guard ? guard->receivedRoutes.collection() : QVector<ReceivedRoutes::Route>{};
    });
    generatorVesselMovementWidget->setEngineReadinessCheck([guard = QPointer<MainController>(this)] {
        const QString unavailable = QStringLiteral("Vessel Movement unavailable: create all Engine/Transmission generators and select valid engine profiles.");
        if (!guard) return unavailable;
        QStringList missingProfiles;
        bool missingGenerators = false;
        for (int i = 0; i < guard->myVessel.NumEngines; ++i) {
            auto *engine = guard->generatorEngines.value(i).first;
            if (!engine || !guard->generatorTransLowerUnits.value(i).first) missingGenerators = true;
            if (engine && !engine->profileReady()) missingProfiles.append(QStringLiteral("Engine %1").arg(i + 1));
        }
        if (!missingProfiles.isEmpty()) {
            return QStringLiteral("Cannot start vessel movement: Select %1 for %2 in the Generate tab.")
                .arg(missingProfiles.size() == 1 ? QStringLiteral("an engine profile") : QStringLiteral("engine profiles"),
                     missingProfiles.join(QStringLiteral(", ")));
        }
        return missingGenerators ? unavailable : QString();
    });
    if (generatorEngines.contains(engineInstance)) {
    // Bind GeneratorVesselMovement -> GeneratorEngine for engine instance
    // Connect the StartEngineEnabledChanged signal from GeneratorVesselMovement to the setEngineRunning slot of GeneratorEngine
    qDebug() << "Binding GeneratorVesselMovement to GeneratorEngine for engine instance:" << engineInstance;
    connectWhileSenderAlive(
        generatorVesselMovementWidget,
        &GeneratorVesselMovement::StartEngineEnabledChanged,
        generatorEngines.value(engineInstance).first,
        [generatorVesselMovementWidget, this, engineInstance]() {
        if (this->generatorVesselMovementWidget->isEnabled()) {
            generatorEngines.value(engineInstance).first->setEngineRunning(generatorVesselMovementWidget->StartEngineEnabled());
        }
    },
        Qt::QueuedConnection, "setEngineRunning, StartEngineEnabled");
    }

    // If engineInstance is 0, 2, 4, 6 then bind ThrottlePositionChanged signal from GeneratorVesselMovement to setThrottlePosition slot of GeneratorEngine
    if (generatorEngines.contains(engineInstance) && (engineInstance == 0 || engineInstance == 2 || engineInstance == 4 || engineInstance == 6)) {
        // Connect signal UseGenVessel to setUseGenVessel slot of GeneratorEngine
        connectWhileSenderAlive(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::UseGenVessel,
            generatorEngines.value(engineInstance).first,
            &GeneratorEngine::setUsingGenVessel, Qt::AutoConnection, "setUsingGenVessel");

        qDebug() << "Binding GeneratorVesselMovement (ThrottlePositionChanged) to GeneratorEngine (setRpmFromThrottlePosition) for engine instance:" << engineInstance;
        connectWhileSenderAlive(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::ThrottlePositionChanged,
            generatorEngines.value(engineInstance).first,
            [generatorVesselMovementWidget, this, engineInstance]() {
            if (generatorVesselMovementWidget->isEnabled()) {
                generatorEngines.value(engineInstance).first->setRpmFromThrottlePosition(generatorVesselMovementWidget->ThrottlePosition());
            }
        },
            Qt::QueuedConnection, "setRpmFromThrottlePosition, ThrottlePosition");
    }

    // If engineInstance is 1, 3, 5, 7 then bind ThrottlePositionChanged signal from GeneratorVesselMovement to setThrottlePosition slot of GeneratorEngine
    if (generatorEngines.contains(engineInstance) && (engineInstance == 1 || engineInstance == 3 || engineInstance == 5 || engineInstance == 7)) {
        // Connect signal UseGenVessel to setUseGenVessel slot of GeneratorEngine
        connectWhileSenderAlive(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::UseGenVessel,
            generatorEngines.value(engineInstance).first,
            &GeneratorEngine::setUsingGenVessel, Qt::AutoConnection, "setUsingGenVessel");

        qDebug() << "Binding GeneratorVesselMovement (ThrottlePositionChanged) to GeneratorEngine (setRpmFromThrottlePosition) for engine instance:" << engineInstance;
        connectWhileSenderAlive(
            generatorVesselMovementWidget,
            &::GeneratorVesselMovement::ThrottlePositionStbdChanged,
            generatorEngines.value(engineInstance).first,
            [generatorVesselMovementWidget, this, engineInstance]() {
            if (generatorVesselMovementWidget->isEnabled()) {
            generatorEngines.value(engineInstance).first->setRpmFromThrottlePosition(generatorVesselMovementWidget->ThrottlePositionStbd());
            }
        },
            Qt::QueuedConnection, "setRpmFromThrottlePosition, ThrottlePositionStbd");
    }

    // Bind GeneratorVesselMovement -> GeneratorTransLowerUnit for engine instance
    if (generatorTransLowerUnits.contains(engineInstance)) {
        qDebug() << "Binding GeneratorVesselMovement to GeneratorTransLowerUnit for engine instance:" << engineInstance;
        if (engineInstance == 0 || engineInstance == 2 || engineInstance == 4 || engineInstance == 6) {
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::TransmissionGearChanged,
                generatorTransLowerUnits.value(engineInstance).first,
                [generatorVesselMovementWidget, this, engineInstance]() {
                if (generatorVesselMovementWidget->isEnabled()) {
                    generatorTransLowerUnits.value(engineInstance).first->setTransmissionGear(generatorVesselMovementWidget->TransmissionGear());
                }
            },
                Qt::QueuedConnection, "setTransmissionGear, TransmissionGear");
        }

        if (engineInstance == 1 || engineInstance == 3 || engineInstance == 5 || engineInstance == 7) {
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::TransmissionGearStbdChanged,
                generatorTransLowerUnits.value(engineInstance).first,
                [generatorVesselMovementWidget, this, engineInstance]() {
                if (generatorVesselMovementWidget->isEnabled()) {
                generatorTransLowerUnits.value(engineInstance).first->setTransmissionGear(generatorVesselMovementWidget->TransmissionGearStbd());
                }
            },
                Qt::QueuedConnection, "setTransmissionGear, TransmissionGearStbd");
        }
    }
}

void MainController::unBindGeneratorVesselMovementToGeneratorEngine(QWidget *widget, int engineInstance)
{

    GeneratorVesselMovement *generatorVesselMovementWidget = qobject_cast<GeneratorVesselMovement *>(widget);
    if (generatorEngines.contains(engineInstance)) {
    // Un-Bind GeneratorVesselMovement -> GeneratorEngine for engine instance
    // Disconnect the StartEngineEnabledChanged signal from GeneratorVesselMovement to the setEngineRunning slot of GeneratorEngine
    qDebug() << "Un-Binding GeneratorVesselMovement to GeneratorEngine for engine instance:" << engineInstance;
    QObject::disconnect(generatorVesselMovementWidget, &GeneratorVesselMovement::StartEngineEnabledChanged, generatorEngines.value(engineInstance).first, &GeneratorEngine::EngineRunning);
    }

    // If engineInstance is 0, 2, 4, 6 then un-bind ThrottlePositionChanged signal from GeneratorVesselMovement to setThrottlePosition slot of GeneratorEngine
    if (generatorEngines.contains(engineInstance) && (engineInstance == 0 || engineInstance == 2 || engineInstance == 4 || engineInstance == 6)) {
        // Disconnect signal UseGenVessel to setUseGenVessel slot of GeneratorEngine
        QObject::disconnect(generatorVesselMovementWidget, &GeneratorVesselMovement::UseGenVessel, generatorEngines.value(engineInstance).first, &GeneratorEngine::setUsingGenVessel);

        qDebug() << "Un-Binding GeneratorVesselMovement (ThrottlePositionChanged) to GeneratorEngine (setRpmFromThrottlePosition) for engine instance:" << engineInstance;
        //QObject::disconnect(generatorVesselMovementWidget, &GeneratorVesselMovement::ThrottlePositionChanged,generatorEngines.value(engineInstance).first, &GeneratorEngine::setRpmFromThrottlePosition);
    }
    // If engineInstance is 1, 3, 5, 7 then un-bind ThrottlePositionChanged signal from GeneratorVesselMovement to setThrottlePosition slot of GeneratorEngine
    if (generatorEngines.contains(engineInstance) && (engineInstance == 1 || engineInstance == 3 || engineInstance == 5 || engineInstance == 7)) {
        // Disconnect signal UseGenVessel to setUseGenVessel slot of GeneratorEngine
        QObject::disconnect(generatorVesselMovementWidget,&GeneratorVesselMovement::UseGenVessel, generatorEngines.value(engineInstance).first, &GeneratorEngine::setUsingGenVessel);

        qDebug() << "Un-Binding GeneratorVesselMovement (ThrottlePositionChanged) to GeneratorEngine (setRpmFromThrottlePosition) for engine instance:" << engineInstance;
        //QObject::disconnect(generatorVesselMovementWidget, &::GeneratorVesselMovement::ThrottlePositionStbdChanged, generatorEngines.value(engineInstance).first, &::GeneratorEngine::setRpmFromThrottlePosition);
    }

    // Un-Bind GeneratorVesselMovement -> GeneratorTransLowerUnit for engine instance
    if (generatorTransLowerUnits.contains(engineInstance)) {
        qDebug() << "Un-Binding GeneratorVesselMovement to GeneratorTransLowerUnit for engine instance:" << engineInstance;
        if (engineInstance == 0 || engineInstance == 2 || engineInstance == 4 || engineInstance == 6) {
            QObject::disconnect( generatorVesselMovementWidget, &GeneratorVesselMovement::TransmissionGearChanged, generatorTransLowerUnits.value(engineInstance).first, &GeneratorTransLowerUnit::TransmissionGearChanged);
        }

        if (engineInstance == 1 || engineInstance == 3 || engineInstance == 5 || engineInstance == 7) {
            QObject::disconnect( generatorVesselMovementWidget, &GeneratorVesselMovement::TransmissionGearStbdChanged, generatorTransLowerUnits.value(engineInstance).first, &GeneratorTransLowerUnit::TransmissionGearChanged);
        }
    }
}

QString MainController::layoutSaveName() const
{
    return m_layoutFilePath.isEmpty() ? QStringLiteral("defaultLayout") : QFileInfo(m_layoutFilePath).completeBaseName();
}

bool MainController::hasLayoutWidgets() const
{
    for (const auto &widgets : groupWidgetsMap)
        if (!widgets.isEmpty()) return true;
    return false;
}

void MainController::noteLayoutStructureChange()
{
    if (!m_restoringLayout && !m_receivingWidget) m_layoutStructurallyModified = true;
}

QJsonObject MainController::captureLayout() const
{
    QJsonObject root{{"formatVersion", 2}, {"documentType", "layout"}, {"numberOfWidgetColumns", numberOfWidgetWide}};
    QJsonArray groups;
    QStringList order = groupWidgetsMap.keys();
    order.removeAll("Generate"); if (groupWidgetsMap.contains("Generate")) order.prepend("Generate");
    for (const auto &group : order) {
        QJsonArray widgets;
        for (auto *widget : groupWidgetsMap.value(group)) {
            const auto data = pgnTableController->getPgnTableModel()->getPgnDataToSend(widget->property("Id").toInt());
            QJsonArray parameters, ranges;
            for (const auto &row : data.paramDetails) { QJsonArray values; for (const auto &v : row) values.append(v); parameters.append(values); }
            for (const auto &range : data.paramRanges) ranges.append(QJsonArray{range.first, range.second});
            QJsonObject item{{"idX", data.idX}, {"type", widget->metaObject()->className()}, {"label", widget->windowTitle()},
                             {"pgn", data.pgn == "000000" ? QString("0") : data.pgn}, {"interval", data.sendInterval}, {"parameters", parameters}, {"ranges", ranges},
                             {"rowSpan", widget->property("RowSpan").toInt()}, {"columnSpan", widget->property("ColumnSpan").toInt()}};
            auto *grid=subFormLayoutMap.value(group).gridLayout;
            int row,column,rows,columns;
            grid->getItemPosition(grid->indexOf(widget->parentWidget()),&row,&column,&rows,&columns);
            item["row"]=row;item["column"]=column;
            if (auto *base = qobject_cast<PgnWidgetBase*>(widget)) {
                item["enabled"] = base->isEnabled();
                if (base->isGenerator()) item["dispatch"] = base->dispatch();
            }
            if (widget->property("ElectricalSimulation").isValid()) item["electricalSimulation"] = widget->property("ElectricalSimulation").toBool();
            if (auto *engine = qobject_cast<GeneratorEngine*>(widget)) item["engineProfile"] = engine->engineProfile();
            if (auto *movement = qobject_cast<GeneratorVesselMovement*>(widget)) item["configuration"] = movement->layoutConfiguration();
            if (auto *weather = qobject_cast<GeneratorWeather*>(widget)) item["configuration"] = weather->layoutConfiguration();
            widgets.append(item);
        }
        groups.append(QJsonObject{{"groupName",group},{"widgets",widgets}});
    }
    root["layouts"] = groups;
    return root;
}

bool MainController::saveLayoutToJson(const QString &filePath)
{
    const auto root = captureLayout();
    QString validationError;
    if (!validateLayout(root, &validationError)) {
        const QString message = "Layout save failed: " + validationError;
        qWarning().noquote() << message;
        emit ShowStatusBarMessage(message, 15000);
        return false;
    }
    QSaveFile file(filePath); // No direct-write fallback: failed replacement must preserve the previous file.
    const auto bytes = QJsonDocument(root).toJson();
    if (bytes.size()>4*1024*1024) {
        emit ShowStatusBarMessage("Layout save failed: document exceeds the 4 MiB load limit.",15000);
        return false;
    }
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        const auto message = tr("Layout save failed: %1. Check the path, permissions and available disk space.").arg(file.errorString());
        qWarning().noquote() << message;
        emit ShowStatusBarMessage(message, 15000); return false;
    }
    m_layoutFilePath = filePath.startsWith(":/") ? filePath : QFileInfo(filePath).absoluteFilePath();
    m_layoutDisplayName = QFileInfo(filePath).completeBaseName();
    if (m_layoutDisplayName.compare("defaultLayout", Qt::CaseInsensitive) == 0) m_layoutDisplayName = "Default Layout";
    m_hasLayoutBaseline = true;
    m_layoutStructurallyModified = false;
    emit layoutIdentityChanged();
    ApplicationLog::event("raynmea.layout","Layout saved successfully");
    emit ShowStatusBarMessage("Layout saved successfully", 5000); return true;
}

// Load widgetConfigs from a JSON file and store them in layoutConfigMap
bool MainController::loadLayoutFromJson(const QString &filePath,
                                       const std::function<bool(const QString &)> &approveReplacement)
{

    QFile input(filePath);
    QJsonParseError parseError;
    if (!input.open(QIODevice::ReadOnly) || input.size() > 4*1024*1024) {
        emit ShowStatusBarMessage("Cannot load layout: file is missing, unreadable or too large.",10000); return false;
    }
    const auto document = QJsonDocument::fromJson(input.readAll(), &parseError);
    input.close(); // The decision may atomically save to this same file on Windows.
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit ShowStatusBarMessage("Cannot load Layout: invalid JSON.",10000); return false;
    }
    return loadLayoutDocument(document.object(), filePath, approveReplacement);
}

bool MainController::loadLayoutDocument(const QJsonObject &rootObject, const QString &filePath,
                                        const std::function<bool(const QString &)> &approveReplacement)
{
    QString error;
    if (!validateLayout(rootObject, &error)) {
        emit ShowStatusBarMessage(tr("Cannot load Layout: %1").arg(error),15000); return false;
    }
    // Preflight the same factory before destroying the working document. These
    // staged widgets have no model bindings and remain locally disabled until adopted.
    QMap<int,QWidget*> probes;
    const auto disposeProbes=qScopeGuard([&]{qDeleteAll(probes);});
    for (const auto &g : rootObject["layouts"].toArray()) for (const auto &v : g.toObject()["widgets"].toArray()) {
        const auto w=v.toObject(); WidgetConfig c{};
        c.idX=w["idX"].toInt(); c.label=w["label"].toString(); c.widgetType=w["type"].toString(); c.pgn=w["pgn"].toString();
        auto *probe=createWidgetFromConfig(c);
        if (!probe) { emit ShowStatusBarMessage("Cannot construct the selected Layout; current work retained.",15000); return false; }
        if(auto *base=qobject_cast<PgnWidgetBase*>(probe)) {base->initializeGeneratorControls();base->setEnabled(false);}
        probes.insert(c.idX,probe);
    }

    bool legacyDepth = false;
    for (const auto group : rootObject["layouts"].toArray())
        for (const auto widget : group.toObject()["widgets"].toArray())
            if (widget.toObject()["pgn"].toString() == "128267")
                for (const auto row : widget.toObject()["parameters"].toArray())
                    if (row.toArray()[0].toString() == "DepthBelowTransducer" && row.toArray()[2].toString() == "dm"
                        && row.toArray()[1].toString().toDouble() != 0 && row.toArray()[1].toString().toDouble() != N2kDoubleNA) legacyDepth = true;
    const QString conversionWarning = legacyDepth
        ? tr("This layout stores legacy depth in decimeters. Convert those depth values to meters (100 dm becomes 10 m) and load?\n\n"
             "Older versions displayed and transmitted inconsistent scales. Verify the intended depth after loading. The original file will not be changed.")
        : QString();
    if (approveReplacement) {
        if (!approveReplacement(conversionWarning)) return false;
    } else if (legacyDepth || !pgnTableController->getPgnTableModel()->getPgnList().isEmpty()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this->mdiArea,
                                      "Widgets are Loaded",
                                      legacyDepth
                                          ? conversionWarning + tr("\n\nLoading replaces the current widgets.")
                                          : "Clear all widgets and load the new layout?",
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return false;
        }

    }

    auto *timestampModel=pgnTableController->getPgnTableModel();
    timestampModel->beginParameterInitialization();
    const auto timestampInitialization=qScopeGuard([timestampModel]{timestampModel->endParameterInitialization();});

    emit layoutAboutToReplace();
    invalidateGeneratorBindings("Layout replacement");
    removeAllWidgets();
    setNumberOfWidgetWide(rootObject.value("numberOfWidgetColumns").toInt(2));
    m_restoringLayout = true;
    QJsonArray layoutsArray = rootObject["layouts"].toArray();
    // Loop through groups in JSON
    for (const QJsonValue &layoutValue : layoutsArray) {
        QJsonObject groupObject = layoutValue.toObject();
        QString groupName = groupObject["groupName"].toString();

        QList<WidgetConfig> widgetConfigs;

        QJsonArray widgetsArray = groupObject["widgets"].toArray();
        // Historical files repeated the last cursor position for every widget.
        // Reserve unique valid coordinates before placing any legacy fallback;
        // a fallback early in the file must not displace a later saved object.
        QMap<QPair<int,int>,int> starts;
        auto coordinate=[](const QJsonValue &v) {
            const double n=v.toDouble(-1);
            return v.isDouble() && std::isfinite(n) && n>=0 && n<=1000000 && std::floor(n)==n;
        };
        for(const auto &v:widgetsArray) {
            const auto w=v.toObject();
            if(coordinate(w["row"]) && coordinate(w["column"]))++starts[{w["row"].toInt(),w["column"].toInt()}];
        }
        QHash<int,QPair<int,int>> savedPositions;
        auto &placement=subFormLayoutMap[groupName];
        for(const auto &v:widgetsArray) {
            const auto w=v.toObject();
            if(!coordinate(w["row"]) || !coordinate(w["column"]))continue;
            const QPair<int,int> start{w["row"].toInt(),w["column"].toInt()};
            if(starts.value(start)!=1)continue;
            bool free=true;
            for(int r=start.first;r<start.first+w["rowSpan"].toInt();++r)
                for(int c=start.second;c<start.second+w["columnSpan"].toInt();++c)
                    if(placement.occupiedCells.contains({r,c}))free=false;
            if(!free)continue;
            savedPositions[w["idX"].toInt()]=start;
            for(int r=start.first;r<start.first+w["rowSpan"].toInt();++r)
                for(int c=start.second;c<start.second+w["columnSpan"].toInt();++c)placement.occupiedCells.insert({r,c});
        }
        const int fallbackCount=widgetsArray.size()-savedPositions.size();
        if(fallbackCount)qInfo().noquote()<<"Layout placement:"<<groupName<<fallbackCount
            <<"widget(s) use legacy automatic placement; valid saved positions reserved.";
        for (const QJsonValue &widgetValue : widgetsArray) {
            QJsonObject widgetObject = widgetValue.toObject();

            WidgetConfig config;
            config.idX = widgetObject["idX"].toInt(); // Retrieve idX
            config.label = widgetObject["label"].toString();
            config.widgetType = widgetObject["type"].toString();
            config.pgn = widgetObject["pgn"].toString();
            config.interval = widgetObject["interval"].toInt();
            const auto position=savedPositions.value(config.idX,{-1,-1});
            config.row = position.first;
            config.column = position.second;
            config.rowSpan = widgetObject["rowSpan"].toInt();
            config.columnSpan = widgetObject["columnSpan"].toInt();
            if (widgetObject.contains("electricalSimulation")) config.properties.insert("electricalSimulation", widgetObject["electricalSimulation"].toBool());
            if (widgetObject.contains("engineProfile"))
                config.properties.insert("engineProfile", widgetObject["engineProfile"].toString());
            if (widgetObject["dispatch"].isBool()) config.properties.insert("dispatch", widgetObject["dispatch"].toBool());
            if (widgetObject["enabled"].isBool())
                config.properties.insert("enabled", widgetObject["enabled"].toBool());

            if (widgetObject["configuration"].isObject()) config.properties.insert("configuration", widgetObject["configuration"].toObject().toVariantMap());

            // Parse parameters
            QJsonArray parametersArray = widgetObject["parameters"].toArray();
            for (const QJsonValue &paramValue : parametersArray) {
                QJsonArray paramArray = paramValue.toArray();
                QList<QString> param;
                for (const QJsonValue &value : paramArray) {
                    param.append(value.toString());
                }
                config.parameters.append(param);
            }

            // Parse ranges
            QJsonArray rangesArray = widgetObject["ranges"].toArray();
            for (const QJsonValue &rangeValue : rangesArray) {
                QJsonArray rangeArray = rangeValue.toArray();
                if (rangeArray.size() == 2) {
                    config.ranges.append(qMakePair(rangeArray[0].toDouble(), rangeArray[1].toDouble()));
                }
            }

            if (config.widgetType == "GeneratorWeather") {
                qDebug() << "Creating GeneratorWeather widget from JSON";
            }
            restoreLayoutFromWidgetConfig(config, groupName, probes.take(config.idX)); // Adopt preflighted widget without constructing twice.
        }

        layoutConfigMap[groupName] = widgetConfigs; // Store configs for each group
    }

    // Set idX to the largest idX value in the pgnData list
    idX = pgnTableController->getPgnTableModel()->getMaxIdX();

    m_restoringLayout = false;
    // All saved generators now exist, independent of their serialized order.
    rebuildGeneratorBindings();
    if (m_mode == Viewer) suspendGenerators(true);
    m_isWidgetsLoaded = true;
    m_layoutFilePath = filePath.startsWith(":/") ? filePath : QFileInfo(filePath).absoluteFilePath();
    m_layoutDisplayName = filePath.isEmpty() ? tr("Layout from Template") : QFileInfo(filePath).completeBaseName();
    if (m_layoutDisplayName.compare("defaultLayout", Qt::CaseInsensitive) == 0) m_layoutDisplayName = "Default Layout";
    if (filePath.isEmpty()) m_layoutFilePath.clear();
    m_hasLayoutBaseline = !filePath.isEmpty();
    m_layoutStructurallyModified = filePath.isEmpty();
    emit layoutIdentityChanged();
    emit layoutReplaced();
    emit ShowStatusBarMessage("Layout loaded successfully", 5000);
    return true;
}

void MainController::addWidgetToLayoutConfig(const QString &groupName, const WidgetConfig &widgetConfig)
{
    // Check if the group already exists in the layoutConfigMap
    if (!layoutConfigMap.contains(groupName)) {
        // If not, create an empty list for this group
        layoutConfigMap.insert(groupName, QList<WidgetConfig>());
    }

    // Append the new widgetConfig to the list for the group
    layoutConfigMap[groupName].append(widgetConfig);

    qDebug() << "Added widget to layout config for group:" << groupName << "with idX:" << widgetConfig.idX;
}


void MainController::removeWidgetFromLayoutConfig(const QString &groupName, int idX)
{
    // Check if the group exists in the layoutConfigMap
    if (!layoutConfigMap.contains(groupName)) {
        qWarning() << "Group" << groupName << "not found in layout config.";
        return;
    }

    // Retrieve the list of WidgetConfig objects for the group
    QList<WidgetConfig> &widgetConfigs = layoutConfigMap[groupName];

    // Use an iterator to find and remove the WidgetConfig with the matching idX
    auto it = std::remove_if(widgetConfigs.begin(), widgetConfigs.end(),
                             [idX](const WidgetConfig &config) { return config.idX == idX; });

    if (it != widgetConfigs.end()) {
        widgetConfigs.erase(it, widgetConfigs.end()); // Remove the matching config(s)
        qDebug() << "Removed widget from layout config for group:" << groupName << "with idX:" << idX;

        // If the list is empty after removal, remove the group from the map
        if (widgetConfigs.isEmpty()) {
            layoutConfigMap.remove(groupName);
        }
    } else {
        qWarning() << "No widget with idX:" << idX << "found in group:" << groupName;
    }
}

void MainController::removeAllWidgetsFromGroup(const QString &groupName)
{
    QList<QWidget *> widgets = groupWidgetsMap[groupName];
    // If no widgets are found, inform the user and return
    if (widgets.isEmpty()) return;

    qDebug() << "Removing all widgets (" << widgets.count() <<  ")from group:" << groupName;
    for (QWidget *widget : widgets) {
        // Remove the widget from the sub-form layout
        removeWidgetFromSubForm(widget, groupName);
    }

    // After removing all widgets from the group, reset subFormLayoutMap to top-left grid position
    if (subFormLayoutMap.contains(groupName)) {
        SubFormLayoutState &layoutState = subFormLayoutMap[groupName];
        layoutState.lastRow = 1;
        layoutState.lastColumn = 0;
        layoutState.occupiedCells.clear();
    }

    // Check all groups for widgets and flag layout as loaded if widgets are found
    m_isWidgetsLoaded = false;
    foreach (auto group, subFormLayoutMap.keys()) {
        if (subFormLayoutMap[group].occupiedCells.size() > 0) {
            m_isWidgetsLoaded = true;
            break;
        }
    }
}

void MainController::removeAllWidgets()
{
    invalidateGeneratorBindings("remove all widgets");
    const bool restoring = m_restoringLayout; m_restoringLayout = true;
    const auto finishRemoval = qScopeGuard([this,restoring] { m_restoringLayout=restoring; });
    QList<QString> groupNames = groupWidgetsMap.keys();
    for (const QString &groupName : groupNames) {
        removeAllWidgetsFromGroup(groupName);
    }

    // New blank layouts take the global default, loaded layouts then supply their own columns.
    setNumberOfWidgetWide(userOptionPrefs.widgetColumns);
    idX = 0;

    m_isWidgetsLoaded = false;
    m_hasLayoutBaseline = false;
    m_layoutStructurallyModified = false;
    m_layoutDisplayName = "Layout";
    m_layoutFilePath.clear();
    emit layoutIdentityChanged();
}

QStringList MainController::getGroupList()
{
    return groupWidgetsMap.keys();
}


PgnAsCombobox MainController::createPgnAsCombobox(const QString &pgn)
{
    PgnAsCombobox pgnAsCombobox = PgnAsCombobox();
    if(const auto *d=Expansion::definition(pgn)){pgnAsCombobox.pgn=pgn;pgnAsCombobox.label=d->label;pgnAsCombobox.group=d->group;return pgnAsCombobox;}

    if (pgn == "127257") {
        pgnAsCombobox.pgn = "127257";
        pgnAsCombobox.label = "Attitude";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "129026") {
        pgnAsCombobox.pgn = "129026";
        pgnAsCombobox.label = "COG/SOG";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "129283") {
        pgnAsCombobox.pgn = "129283";
        pgnAsCombobox.label = "Cross Track Error";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "129033") {
        pgnAsCombobox.pgn = "129033";
        pgnAsCombobox.label = "Date/Time";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "127489") {
        pgnAsCombobox.pgn = "127489";
        pgnAsCombobox.label = "Engine Dynamic";
        pgnAsCombobox.group = "Propulsion";
    } else if (pgn == "127488") {
        pgnAsCombobox.pgn = "127488";
        pgnAsCombobox.label = "Engine Rapid";
        pgnAsCombobox.group = "Propulsion";
    } else if (pgn == "127505") {
        pgnAsCombobox.pgn = "127505";
        pgnAsCombobox.label = "Fluid Level";
        pgnAsCombobox.group = "Tanks";
    } else if (pgn == "129029") {
        pgnAsCombobox.pgn = "129029";
        pgnAsCombobox.label = "GNSS Position Data";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "127252") {
        pgnAsCombobox.pgn = "127252";
        pgnAsCombobox.label = "Heave";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "129025") {
        pgnAsCombobox.pgn = "129025";
        pgnAsCombobox.label = "Lat/Long";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "127258") {
        pgnAsCombobox.pgn = "127258";
        pgnAsCombobox.label = "Magnetic Variation";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "129284") {
        pgnAsCombobox.pgn = "129284";
        pgnAsCombobox.label = "Navigation";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "128777" || pgn == "128778") {
        pgnAsCombobox.pgn=pgn;pgnAsCombobox.label=pgn=="128777"?"Windlass Operating Status":"Windlass Monitoring Status";pgnAsCombobox.group="Other";
    } else if (pgn == "129039") {
        pgnAsCombobox.pgn=pgn;pgnAsCombobox.label="AIS Class B Position";pgnAsCombobox.group="Communication";
    } else if (pgn == "129810") {
        pgnAsCombobox.pgn=pgn;pgnAsCombobox.label="AIS Class B Static Part B";pgnAsCombobox.group="Communication";
    } else if (pgn == "129809") {
        pgnAsCombobox.pgn = pgn;
        pgnAsCombobox.label = "AIS Class B Static Part A";
        pgnAsCombobox.group = "Communication";
    } else if (pgn == "130306" || pgn == "130313") {
        pgnAsCombobox.pgn = pgn;
        pgnAsCombobox.label = pgn == "130306" ? "Wind" : "Humidity";
        pgnAsCombobox.group = "Environment";
    } else if (pgn == "130310") {
        pgnAsCombobox.pgn = "130310";
        pgnAsCombobox.label = "Outside Environmental";
        pgnAsCombobox.group = "Environment";
    } else if (pgn == "127251") {
        pgnAsCombobox.pgn = "127251";
        pgnAsCombobox.label = "Rate of Turn";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "128259") {
        pgnAsCombobox.pgn = "128259";
        pgnAsCombobox.label = "Speed";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "128267") {
        pgnAsCombobox.pgn = "128227";
        pgnAsCombobox.label = "Depth";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "126992") {
        pgnAsCombobox.pgn = "126992";
        pgnAsCombobox.label = "System Time";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "130316") {
        pgnAsCombobox.pgn = "130316";
        pgnAsCombobox.label = "Temperature Extended";
        pgnAsCombobox.group = "Environment";
    } else if (pgn == "130312") {
        pgnAsCombobox.pgn = "130312";
        pgnAsCombobox.label = "Temperature";
        pgnAsCombobox.group = "Environment";
    } else if (pgn == "127493") {
        pgnAsCombobox.pgn = "127493";
        pgnAsCombobox.label = "Transmission Dynamic";
        pgnAsCombobox.group = "Propulsion";
    } else if (pgn == "127245") {
        pgnAsCombobox.pgn = "127245";
        pgnAsCombobox.label = "Rudder";
        pgnAsCombobox.group = "Propulsion";
    } else if (pgn == "130576") {
        pgnAsCombobox.pgn = "130576";
        pgnAsCombobox.label = "Trim Tab";
        pgnAsCombobox.group = "Propulsion";
    } else if (pgn == "127250") {
        pgnAsCombobox.pgn = "127250";
        pgnAsCombobox.label = "Vessel Heading";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "127506") {
        pgnAsCombobox.pgn = "127506";
        pgnAsCombobox.label = "DC Status";
        pgnAsCombobox.group = "Electrical";
    } else if (pgn == "127507") {
        pgnAsCombobox.pgn = "127507";
        pgnAsCombobox.label = "Charger Status";
        pgnAsCombobox.group = "Electrical";
    } else if (pgn == "127508") {
        pgnAsCombobox.pgn = "127508";
        pgnAsCombobox.label = "Battery Status";
        pgnAsCombobox.group = "Electrical";
    } else if (pgn == "GGA") {
        pgnAsCombobox.pgn = "GGA";
        pgnAsCombobox.label = "Global Positioning";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "RMC") {
        pgnAsCombobox.pgn = "RMC";
        pgnAsCombobox.label = "Recommended Min Gnss";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "HDG") {
        pgnAsCombobox.pgn = "HDG";
        pgnAsCombobox.label = "Heading";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "HDT") {
        pgnAsCombobox.pgn = "HDT";
        pgnAsCombobox.label = "True Heading";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "GLL") {
        pgnAsCombobox.pgn = "GLL";
        pgnAsCombobox.label = "Geographic Position";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "VTG") {
        pgnAsCombobox.pgn = "VTG";
        pgnAsCombobox.label = "SOG COG";
        pgnAsCombobox.group = "Navigation";
    } else if (pgn == "ZDA") {
        pgnAsCombobox.pgn = "ZDA";
        pgnAsCombobox.label = "Date Time";
        pgnAsCombobox.group = "Navigation";
    }
    else {
        pgnAsCombobox.pgn = "";
        pgnAsCombobox.label = "";
        pgnAsCombobox.group = "";
        qWarning() << "UnSupported Received PGN:" << pgn;
    }

    return pgnAsCombobox;
}

void MainController::setMode(Mode mode)
{
    pgnTableController->SetMode(mode);
    if (m_mode == mode) return;
    invalidateGeneratorBindings("mode transition"); // Invalidate queued calls before stopping their producers.
    suspendGenerators(true);
    m_mode = mode;
    suspendGenerators(mode == Viewer);
    rebuildGeneratorBindings();
    emit ShowStatusBarMessage(mode == Viewer ? "Mode: Viewer (generators suspended)" : "Mode: Generate (stopped)", 5000);
}

QWidget *MainController::createWidgetFromConfig(const WidgetConfig &config)
{
    QWidget *widget = nullptr;

    // Create the widget based on the widgetType
    if (config.widgetType == "Attitude127257") {
        widget = new Attitude127257(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "CogSog129026") {
        widget = new CogSog129026(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "CrossTrack129283") {
        widget = new CrossTrack129283(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "DateTime129033") {
        widget = new DateTime129033(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "EngineDynamic127489") {
        widget = new EngineDynamic127489(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "EngineRapid127488") {
        widget = new EngineRapid127488(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "FluidLevel127505") {
        widget = new FluidLevel127505(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "GeneratorDateTime") {
        widget = new GeneratorDateTime(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "GeneratorEngine") {
        widget = new GeneratorEngine(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "GeneratorTransLowerUnit") {
        widget = new GeneratorTransLowerUnit(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "GeneratorVesselMovement") {
        widget = new GeneratorVesselMovement(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "GeneratorWeather") {
        auto *weather = new GeneratorWeather(config.idX, config.label, this->mdiArea);
        weather->setOpenWeatherMapApiKey(m_openWeatherMapApiKey);
        widget = weather;
    } else if (config.widgetType == "GnssPositionData129029") {
        widget = new GnssPositionData129029(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "Heave127252") {
        widget = new Heave127252(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "LatLong129025") {
        widget = new LatLong129025(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "MagneticVariation127258") {
        widget = new MagneticVariation127258(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "Navigation129284") {
        widget = new Navigation129284(config.idX, config.label, this->mdiArea);
    } else if (Expansion::definition(config.pgn) && Expansion::definition(config.pgn)->type==config.widgetType) {
        widget=ExpansionInstrument::create(config.idX,config.label,config.pgn,mdiArea);
    } else if (config.widgetType == "WindlassOperating128777") {
        widget=new WindlassOperating128777(config.idX,config.label,mdiArea);
    } else if (config.widgetType == "WindlassMonitoring128778") {
        widget=new WindlassMonitoring128778(config.idX,config.label,mdiArea);
    } else if (config.widgetType == "AisClassBPosition129039") {
        widget=new AisClassBPosition129039(config.idX,config.label,mdiArea);
    } else if (config.widgetType == "AisClassBStatic129810") {
        widget=new AisClassBStatic129810(config.idX,config.label,mdiArea);
    } else if (config.widgetType == "AisClassBStatic129809") {
        widget = new AisClassBStatic129809(config.idX, config.label, mdiArea);
    } else if (config.widgetType == "WindData130306") {
        widget = new WindData130306(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "Humidity130313") {
        widget = new Humidity130313(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "OutsideEnvironmental130310") {
        widget = new OutsideEnvironmental130310(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "RateOfTurn127251") {
        widget = new RateOfTurn127251(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "Speed128259") {
        widget = new Speed128259(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "Speed128267") {
        widget = new Depth128267(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "SystemTime126992") {
        widget = new SystemTime126992(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "TemperatureExtended130316") {
        widget = new TemperatureExtended130316(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "TemperatureWidget130312") {
        widget = new TemperatureWidget130312(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "TransmissionDynamic127493") {
        widget = new TransmissionDynamic127493(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "VesselHeading127250") {
        widget = new VesselHeading127250(config.idX, config.label, this->mdiArea);
    } else if ((config.widgetType == "DcStatus127506" || config.widgetType == "DCStatus127506")) {
        widget = new DcStatus127506(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "ChargerStatus127507") {
        widget = new ChargerStatus127507(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "BatteryStatus127508") {
        widget = new BatteryStatus127508(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "Depth128267") {
        widget = new Depth128267(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "Rudder127245") {
        widget = new Rudder127245(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "TrimTab130576") {
        widget = new TrimTab130576(config.idX, config.label, this->mdiArea);
    } else if ((config.widgetType == "GlobalPositioningGGA" || config.widgetType == "GGA")) {
        widget = new GlobalPositioningGGA(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "RecommendedMinGnssRMC") {
        widget = new RecommendedMinGnssRMC(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "HeadingHDG") {
        widget = new HeadingHDG(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "TrueHeadingHDT") {
        widget = new TrueHeadingHDT(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "GeographicPositionGLL") {
        widget = new GeographicPositionGLL(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "CogSogVTG") {
        widget = new CogSogVTG(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "DateTimeZDA") {
        widget = new DateTimeZDA(config.idX, config.label, this->mdiArea);
    } else if (config.widgetType == "Spacer000000") {
        widget = new Spacer000000(config.idX, config.label, this->mdiArea);
    }
    else {
        qWarning() << "Unknown widget type:" << config.widgetType;
    }

    return widget;
}

void MainController::restoreLayoutFromWidgetConfig(const WidgetConfig &savedConfig, QString groupName, QWidget *prepared)
{
    auto *timestampModel=pgnTableController->getPgnTableModel();
    timestampModel->beginParameterInitialization(savedConfig.idX,savedConfig.idX);
    const auto timestampInitialization=qScopeGuard([timestampModel]{timestampModel->endParameterInitialization();});

    WidgetConfig config = savedConfig;
    // Legacy depth conversion is disclosed before transactional file loading.
    // Units identify the old contract; canonical-meter layouts are never rescaled.
    for (auto &row : config.parameters) {
        if (config.pgn == "128267" && row[0] == "DepthBelowTransducer" && row[2] == "dm") {
            const double value = row[1].toDouble();
            if (value != N2kDoubleNA) row[1] = QString::number(value / 10.0, 'g', 17);
            row[2] = N2kConst::M;
        }
        if (config.pgn == "127506") {
            if (row[0] == "TimeRemaining") row[2] = N2kConst::S;
            if (row[0] == "Capacity") row[2] = "C"; // Coulombs, not percent or amp-hours.
            if (row[0] == "StateOfCharge" || row[0] == "StateOfHealth") row[3] = "uint8_t";
        }
        if (config.pgn == "127507" && row[0] == "EqualizationTimeRemaining") row[2] = N2kConst::S;
    }
    if (config.pgn == "GGA") {
        // Old layouts used the RMC template. Retain shared fields by name, never by position.
        config.parameters = ggaParameters(myVessel.Latitude, myVessel.Longitude);
        for (auto &parameter : config.parameters)
            for (const auto &saved : savedConfig.parameters)
                if (saved.size() >= 2 && saved[0] == parameter[0]) parameter[1] = saved[1];
        config.ranges = ggaRanges();
    } else if (config.pgn == "127493" || config.pgn == "-127493") {
        for (auto &parameter : config.parameters) {
            if (parameter.size() < 2) continue;
            if (parameter[0] == "TransmissionInstance") parameter[0] = "EngineInstance";
            if (parameter[0] == "TransmissionOilPressure") parameter[0] = "OilPressure";
            if (parameter[0] == "TransmissionOilTemperature") parameter[0] = "OilTemperature";
            // Repair the historical zero-Kelvin default; preserve saved operating temperatures.
            if (parameter[0] == "OilTemperature" && parameter[1].toDouble() == 0.0)
                parameter[1] = "295.372";
        }
    }
    if (config.pgn == "130310" || config.pgn == "-130311")
        normalizeAtmosphericPressure(config.parameters, config.ranges);

    if (config.pgn == "-130311") {
        for (const auto& extra : GeneratorWeather::additionalParameters()) {
            bool found = false;
            for (const auto& saved : config.parameters) if (saved[0] == extra[0]) { found = true; break; }
            if (!found) config.parameters.append(extra);
        }
    }
    if (config.pgn == "-130311")
        while (config.ranges.size() < config.parameters.size()) config.ranges.append({N2kDoubleNA, 1e12});
    for (auto &parameter : config.parameters) {
        if (config.pgn == "127489" && parameter[0] == "EngineHours") parameter[2] = N2kConst::S;
        if (config.pgn == "-129025" && parameter[0] == "RateOfTurn") parameter[2] = N2kConst::RPS;
    }
    // Old GLL snapshots predate the explicit validity property. Preserve their
    // historical availability-derived behavior without changing saved coordinates.
    if (config.pgn == "GLL") {
        bool hasStatus = false, available = true;
        for (const auto &parameter : config.parameters) {
            if (parameter[0] == "Status") hasStatus = true;
            if (parameter[0] == "GPSTime" || parameter[0] == "Latitude" || parameter[0] == "Longitude")
                available &= !parameter[1].isEmpty() && parameter[1].toDouble() != NMEA0183DoubleNA;
        }
        if (!hasStatus) {
            config.parameters.append({"Status", available ? "A" : "V", "", "char"});
            config.ranges.append({0.0, 0.0});
        }
    }
    // Create the widget
    QWidget *widget = prepared ? prepared : createWidgetFromConfig(config);
    if (!widget) {
        qWarning() << "Failed to create widget from config for group:" << groupName;
        return;
    }

    if(auto *source=qobject_cast<GeneratorDateTime*>(widget))generatorDateTimeWidget=source;
    if(auto *source=qobject_cast<GeneratorEngine*>(widget))generatorEngineWidget=source;
    if(auto *source=qobject_cast<GeneratorTransLowerUnit*>(widget))generatorTransLowerUnitWidget=source;
    if(auto *source=qobject_cast<GeneratorVesselMovement*>(widget))generatorVesselMovementWidget=source;
    if(auto *source=qobject_cast<GeneratorWeather*>(widget))generatorWeatherWidget=source;
    if (config.pgn == "127506" || config.pgn == "127507" || config.pgn == "127508")
        widget->setProperty("ElectricalSimulation", config.properties.value("electricalSimulation", false));
    // Ensure that the group exists in `groupWidgetsMap` before adding a new widget
    // This map is used for saving and restoring the layout state
    if (!groupWidgetsMap.contains(groupName)) {
        groupWidgetsMap.insert(groupName, QList<QWidget*>());
    }

    // Save instance number for when user dynamically adds widgets using comboBox
    idX = config.idX;
    addedWidgetCount++;
    // Add PGN to both models
    auto restoredParameters=config.parameters;
    if(const auto *d=Expansion::definition(config.pgn))for(auto &row:restoredParameters)
        for(const auto &field:d->fields)if(row[0]==field.name){row[2]=field.unit;row[3]=field.type;}
    pgnTableController->addPgn(config.idX, config.label, config.pgn, config.properties.value("enabled", userOptionPrefs.newWidgetsEnabled).toBool(), restoredParameters, config.ranges, config.interval);

    // TODO: Implement the following logic once tree view is implemented
    //pgnTreeController->addPgn(config.idX, config.label, config.pgn);

    // Set properties for the widget
    widget->setProperty("Id", config.idX);
    widget->setProperty("Pgn", config.pgn);
    widget->setProperty("ColumnSpan", config.columnSpan);
    widget->setProperty("RowSpan", config.rowSpan);
    widget->setProperty("Interval", config.interval);
    widget->setWindowTitle(config.label);
    widget->setToolTip(config.label);

    // Set user preferences and enable/disable new widgets based on userOptionPrefs
    if (auto *pgnWidget = qobject_cast<PgnWidgetBase *>(widget)) {
        pgnWidget->setUserPrefs(userPrefs);
        pgnWidget->setDispatch(config.properties.value("dispatch",true).toBool());
        pgnWidget->setEnabled(config.properties.value("enabled", userOptionPrefs.newWidgetsEnabled).toBool());
    }

    // Add widget to sub-form and map
    addWidgetToSubForm(widget, groupName, m_restoringLayout ? config.row : -1, m_restoringLayout ? config.column : -1);
    groupWidgetsMap[groupName].append(widget);

    // Use the same data binding approach as `onComboBoxTextChanged`
    // Sentence identifiers need the same dispatch keys used during creation.
    static const QMap<QString, int> sentenceIds = {
        {"GGA", SNT_GGA}, {"RMC", SNT_RMC}, {"HDG", SNT_HDG},
        {"HDT", SNT_HDT}, {"GLL", SNT_GLL}, {"VTG", SNT_VTG}, {"ZDA", SNT_ZDA}
    };
    const int messageId = Expansion::definition(config.pgn)?Expansion::definition(config.pgn)->id:sentenceIds.value(config.pgn, config.pgn.toInt());
    if (auto *engine = qobject_cast<GeneratorEngine *>(widget)) {
        // Initialize the curve before applying model values. Explicit selections
        // take precedence; layouts without this optional field retain Setup defaults.
        engine->setMyVessel(myVessel);
        if (config.properties.contains("engineProfile"))
            engine->setEngineProfile(config.properties.value("engineProfile").toString());
    }
    bindWidgetToModel(widget, messageId, pgnTableController->getPgnTableModel(), idX);
    initializeWidgetFromModel(widget, messageId, pgnTableController->getPgnTableModel(), idX);
    if (auto *weather = qobject_cast<GeneratorWeather*>(widget))
        weather->restoreLayoutConfiguration(QJsonObject::fromVariantMap(config.properties.value("configuration").toMap()));
    if (config.properties.contains("enabled")) {
        const bool enabled = config.properties.value("enabled").toBool();
        if (auto *sentence = qobject_cast<PgnWidgetBase *>(widget)) sentence->setEnabled(enabled);
        if (auto *checkbox = widget->findChild<QCheckBox *>("checkBoxEnabled")) checkbox->setChecked(enabled);
        pgnTableController->setPgnEnabled(idX, enabled);
    }

    // Handle special cases where additional bindings are required
    //int engineInstance = pgnTableController->getPgnInstanceCount(config.pgn);
    if (config.pgn == "-127489") {          // GeneratorEngine127489
        int engineInstance = 0; for (const auto &parameter : config.parameters) if (parameter[0] == "EngineInstance") engineInstance = parameter[1].toInt();
        qobject_cast<GeneratorEngine *>(widget)->setEngineInstance(engineInstance);
        // Add the engine instance to the list of engine instances
        generatorEngines.insert(engineInstance, qMakePair(qobject_cast<GeneratorEngine *>(widget), false));
    } else if (config.pgn == "-127493") {   // GeneratorTransLowerUnit127493
        int engineInstance = 0; for (const auto &parameter : config.parameters) if (parameter[0] == "EngineInstance") engineInstance = parameter[1].toInt();
        qobject_cast<GeneratorTransLowerUnit *>(widget)->setEngineInstance(engineInstance);
        generatorTransLowerUnits.insert(engineInstance, qMakePair(qobject_cast<GeneratorTransLowerUnit *>(widget), false));
        if (m_mode == Sender) {
            // Bind widgets based on engine instance to engine generator
            bindWidgetsBasedOnEngineInstanceToGenerator(engineInstance);
        }
    } else if (config.pgn == "-129025") {   // GeneratorVesselMovement129025
        // Bind the generator engine movement to the generator engines
        for (int i = 0; i < myVessel.NumEngines; i++) {
            bindGeneratorVesselMovementToGeneratorEngine(generatorVesselMovementWidget, i);
        }
    } else if (config.pgn == "127488") {    // EngineRapid127488
        int engineInstance = 0; for (const auto &parameter : config.parameters) if (parameter[0] == "EngineInstance") engineInstance = parameter[1].toInt();
        qobject_cast<EngineRapid127488 *>(widget)->setEngineInstance(engineInstance);
        // Add EngineDynamicWidget to the map
        engineRapids.insert(engineInstance, qMakePair(qobject_cast<EngineRapid127488 *>(widget), false));
        if (m_mode == Sender) {
            // Bind widgets based on engine instance to engine generator
            bindWidgetsBasedOnEngineInstanceToGenerator(engineInstance);
        }
    } else if (config.pgn == "127489") {    // EngineDynamic127489
        int engineInstance = 0; for (const auto &parameter : config.parameters) if (parameter[0] == "EngineInstance") engineInstance = parameter[1].toInt();
        qobject_cast<EngineDynamic127489 *>(widget)->setEngineInstance(engineInstance);
        // Add EngineRapidWidget to the map
        engineDynamics.insert(engineInstance, qMakePair(qobject_cast<EngineDynamic127489 *>(widget), false));
        if (m_mode == Sender) {
            // Bind widgets based on engine instance to engine generator
            bindWidgetsBasedOnEngineInstanceToGenerator(engineInstance);
        }
    } else if (config.pgn == "127493") {    // TransmissionDynamic127493
        int engineInstance = 0; for (const auto &parameter : config.parameters) if (parameter[0] == "EngineInstance") engineInstance = parameter[1].toInt();
        qobject_cast<TransmissionDynamic127493 *>(widget)->setEngineInstance(engineInstance);
        // Add TransmissionDynamicWidget to the map
        transmissionDynamics.insert(engineInstance, qMakePair(qobject_cast<TransmissionDynamic127493 *>(widget), false));
        if (m_mode == Sender) {
            // Bind widgets based on engine instance to engine generator
            bindWidgetsBasedOnEngineInstanceToGenerator(engineInstance);
        }
    }

    // Lastly set initial values is GUI
    qobject_cast<PgnWidgetBase *>(widget)->setInitialGuiValues();
    if (auto *movement = qobject_cast<GeneratorVesselMovement*>(widget)) {
        movement->setMyVessel(myVessel);
        if (config.properties.contains("configuration"))
            movement->restoreLayoutConfiguration(QJsonObject::fromVariantMap(config.properties["configuration"].toMap()));
    }
    if(m_restoringLayout) {
        widget->setProperty("ColumnSpan",config.columnSpan);
        widget->setProperty("RowSpan",config.rowSpan);
    }
    prepareWidget(widget);
    if (!m_restoringLayout) rebuildGeneratorBindings();
}

void MainController::addWidgetToSubForm(QWidget *widget, const QString &groupName, int row, int column)
{
    if (!subFormLayoutMap.contains(groupName)) return;
    widget->setFont(mdiArea->window()->font());
    auto *frame = new QFrame();
    frame->setFrameShape(QFrame::Box);
    frame->setFrameShadow(QFrame::Plain);
    frame->setLineWidth(userOptionPrefs.borderWidth);
    frame->setLayout(new QVBoxLayout);
    frame->layout()->setContentsMargins(0, 0, 0, 0);
    frame->layout()->addWidget(widget);
    auto &state=subFormLayoutMap[groupName];
    if(row>=0 && column>=0) {
        const int rows=qMax(1,widget->property("RowSpan").toInt()),columns=qMax(1,widget->property("ColumnSpan").toInt());
        state.gridLayout->addWidget(frame,row,column,rows,columns);
        for(int r=row;r<row+rows;++r)for(int c=column;c<column+columns;++c)state.occupiedCells.insert({r,c});
    } else placeWidgetFrame(widget, frame, state);
    updateGridPresentation(subFormLayoutMap[groupName]);
    widget->show();
    emit layoutObjectsChanged(); // Observers refresh after registration finishes.
}

void MainController::removeWidgetFromSubForm(QWidget *widget, const QString &groupName)
{
    invalidateGeneratorBindings("widget removal");
    const auto refreshBindings = qScopeGuard([this] { rebuildGeneratorBindings(); });

/*    // Check if the sub-form exists
    if (!subFormLayoutMap.contains(groupName)) {
        qWarning() << "Sub-form not found:" << groupName;
        return;
    }

    // Check if the widget is not null
    if (!widget) {
        qWarning() << "Widget is null";
        return;
    }

    int id = widget->property("Id").toInt();

    //Un-bind the widget from other widgets
    disconnect(widget, nullptr, nullptr, nullptr);

    // Remove the widget from the PGN table model
    pgnTableController->removePgnInstance(id);

    // Remove the widget from the groupWidgetsMap
    if (groupWidgetsMap.contains(groupName)) {
        groupWidgetsMap[groupName].removeOne(widget);
    }

    SubFormLayoutState &layoutState = subFormLayoutMap[groupName];
    QGridLayout *gridLayout = layoutState.gridLayout;

    if (gridLayout->indexOf(widget) == -1) {
        qWarning() << "Widget not found in sub-form:" << groupName;
        return;
    }

    // Get the position of the widget before removal
    int row, column, rowSpan, columnSpan;
    gridLayout->getItemPosition(gridLayout->indexOf(widget), &row, &column, &rowSpan, &columnSpan);

    //qDebug() << "Removing widget from sub-form:" << groupName << "at row:" << row << "column:" << column << "rowSpan:" << rowSpan << "columnSpan:" << columnSpan;

    // Remove the widget from the layout
    gridLayout->removeWidget(widget);

    // Hide the widget (optional)
    widget->hide();

    // Free up the cells that the widget was occupying
    auto markCellsFree = [&](int startRow, int startCol, int rowSpan, int colSpan) {
        for (int r = startRow; r < startRow + rowSpan; ++r) {
            for (int c = startCol; c < startCol + colSpan; ++c) {
                layoutState.occupiedCells.remove(qMakePair(r, c));
            }
        }
    };

    // Mark the cells previously occupied by this widget as free
    markCellsFree(row, column, rowSpan, columnSpan);

    // Optionally delete the widget
    delete widget;

    // Check if the next widget fits before setting the lastRow and lastColumn
    auto isSpaceFree = [&](int startRow, int startCol, int rowSpan, int colSpan) {
        for (int r = startRow; r < startRow + rowSpan; ++r) {
            for (int c = startCol; c < startCol + colSpan; ++c) {
                if (layoutState.occupiedCells.contains(qMakePair(r, c))) {
                    return false;
                }
            }
        }
        return true;
    };

    // Check if the last removed space can fit the next widget
    if (isSpaceFree(row, column, rowSpan, columnSpan)) {
        layoutState.lastRow = row;
        layoutState.lastColumn = column;
    } else {
        //qDebug() << "Last removed space is too small for the next widget. Adjusting placement.";
        // Handle case where it doesn't fit, reset to find the next available space
        layoutState.lastColumn += columnSpan;
        if (layoutState.lastColumn >= numberOfWidgetWide * numberColumnsWidgetSpans) {
            layoutState.lastColumn = 0;
            layoutState.lastRow++;
        }
    }

    // Save the updated layout state
    subFormLayoutMap[groupName] = layoutState;
*/
    // Check if the sub-form exists
    if (!subFormLayoutMap.contains(groupName)) {
        qWarning() << "Sub-form not found:" << groupName;
        return;
    }

    if (!widget) {
        qWarning() << "Widget is null";
        return;
    }

    int id = widget->property("Id").toInt();
    // Forget non-owning generator/instrument references before deleting a widget.
    // A subsequent add/load must not bind to an object from the previous layout.
    auto forget = [widget](auto &map) {
        for (auto it = map.begin(); it != map.end();) {
            if (it.value().first == widget) it = map.erase(it);
            else ++it;
        }
    };
    for (auto it = generatorEngines.cbegin(); it != generatorEngines.cend(); ++it) {
        if (it.value().first != widget) continue;
        if (engineRapids.contains(it.key())) engineRapids[it.key()].second = false;
        if (engineDynamics.contains(it.key())) engineDynamics[it.key()].second = false;
        if (generatorTransLowerUnits.contains(it.key())) generatorTransLowerUnits[it.key()].second = false;
    }
    for (auto it = generatorTransLowerUnits.cbegin(); it != generatorTransLowerUnits.cend(); ++it) {
        if (it.value().first == widget && transmissionDynamics.contains(it.key()))
            transmissionDynamics[it.key()].second = false;
    }
    forget(generatorEngines);
    forget(generatorTransLowerUnits);
    forget(generatorDateTimes);
    forget(generatorVesselMovements);
    forget(engineRapids);
    forget(engineDynamics);
    forget(transmissionDynamics);
    if (generatorDateTimeWidget == widget) generatorDateTimeWidget = nullptr;
    if (generatorVesselMovementWidget == widget) generatorVesselMovementWidget = nullptr;
    if (generatorEngineWidget == widget) generatorEngineWidget = nullptr;
    if (generatorTransLowerUnitWidget == widget) generatorTransLowerUnitWidget = nullptr;
    if (generatorWeatherWidget == widget) generatorWeatherWidget = nullptr;
    generatorEngineWidgets.removeAll(qobject_cast<GeneratorEngine *>(widget));
    generatorTransLowerUnitWidgets.removeAll(qobject_cast<GeneratorTransLowerUnit *>(widget));
    // Preserve destroyed(): Qt uses it to unregister accessibility interfaces.
    // QObject destruction already disconnects the widget's connections.

    // Remove the widget from the PGN table model
    pgnTableController->removePgnInstance(id);

    if (groupWidgetsMap.contains(groupName)) {
        groupWidgetsMap[groupName].removeOne(widget);
    }

    SubFormLayoutState &layoutState = subFormLayoutMap[groupName];
    QGridLayout *gridLayout = layoutState.gridLayout;

    // Find the QFrame containing the widget
    QFrame *frame = qobject_cast<QFrame*>(widget->parentWidget());
    if (!frame || gridLayout->indexOf(frame) == -1) {
        qWarning() << "Frame containing widget not found in sub-form:" << groupName;
        return;
    }

    // Get the position of the QFrame in the grid layout
    int row, column, rowSpan, columnSpan;
    gridLayout->getItemPosition(gridLayout->indexOf(frame), &row, &column, &rowSpan, &columnSpan);

    // Remove the frame (and thus the widget) from the layout
    gridLayout->removeWidget(frame);
    frame->hide();

    // Mark cells previously occupied by this frame as free
    auto markCellsFree = [&](int startRow, int startCol, int rowSpan, int colSpan) {
        for (int r = startRow; r < startRow + rowSpan; ++r) {
            for (int c = startCol; c < startCol + colSpan; ++c) {
                layoutState.occupiedCells.remove(qMakePair(r, c));
            }
        }
    };

    markCellsFree(row, column, rowSpan, columnSpan);

    // Optionally delete the frame, which deletes the widget inside
    delete frame;
    emit layoutObjectsChanged();

    // Adjust layout state
    auto isSpaceFree = [&](int startRow, int startCol, int rowSpan, int colSpan) {
        for (int r = startRow; r < startRow + rowSpan; ++r) {
            for (int c = startCol; c < startCol + colSpan; ++c) {
                if (layoutState.occupiedCells.contains(qMakePair(r, c))) {
                    return false;
                }
            }
        }
        return true;
    };

    if (isSpaceFree(row, column, rowSpan, columnSpan)) {
        layoutState.lastRow = row;
        layoutState.lastColumn = column;
    } else {
        layoutState.lastColumn += columnSpan;
        if (layoutState.lastColumn >= numberOfWidgetWide * numberColumnsWidgetSpans) {
            layoutState.lastColumn = 0;
            layoutState.lastRow++;
        }
    }

    subFormLayoutMap[groupName] = layoutState;
    updateGridPresentation(subFormLayoutMap[groupName]);

}

QList<QWidget *> *MainController::getListWidgetsOnSubForm(const QString &groupName)
{
    // Check if the sub-form exists
    if (!subFormLayoutMap.contains(groupName)) {
        qWarning() << "Sub-form not found:" << groupName;
        return nullptr;
    }

    return &groupWidgetsMap[groupName];
}

void MainController::removeWidgetFromFromModel(int id)
{
    // Get the groupName for the widget with the given id
    QString groupName = pgnTableController->getPgnTableModel()->getLabel(id);
    // Remove the widget from the PGN table model
    pgnTableController->removePgnInstance(id);
}


void MainController::bindWidgetToModel(QWidget *widget, int Pgn, PgnTableModel *model, int idX)
{
    model->setParameterOwner(idX,widget);
    if (auto *base=qobject_cast<PgnWidgetBase*>(widget)) {
        connectModel(base,&PgnWidgetBase::presentationRefreshStarted,base,[model,idX]{model->beginParameterInitialization(idX,idX);});
        connectModel(base,&PgnWidgetBase::presentationRefreshFinished,base,[model]{model->endParameterInitialization();});
        connectModel(base,&PgnWidgetBase::parametersSampled,base,[this,model,idX](const QStringList &fields) {
            if (m_mode!=Sender) return;
            for (const auto &field:fields) model->recordParameterSample(idX,field);
        });
    }

    // Initial display setters must not enqueue edits of the model they are reading.
    // Widget edits commit on the GUI thread before a later receive update;
    // generator-to-instrument delivery is guarded separately by binding epoch.
    const auto parameterEditConnection = Qt::AutoConnection;
    const QSignalBlocker initializationSignals(Pgn > 0 ? widget : nullptr);

    if (Pgn == SNT_GGA || Pgn == SNT_RMC || Pgn == SNT_HDG || Pgn == SNT_HDT
        || Pgn == SNT_GLL || Pgn == SNT_VTG || Pgn == SNT_ZDA) {
        auto *sentence = qobject_cast<PgnWidgetBase *>(widget);
        connectModel(sentence, &PgnWidgetBase::enabledChanged, sentence, [=] {
            model->setPgnEnabled(idX, sentence->isEnabled());
        });
    }

    if (Pgn==129809 || Pgn==129810 || Pgn==129039) {
        auto *association=widget->findChild<QLabel*>("aisAssociation");
        auto update=[model,idX,association] {
            const auto *self=model->findPgn(idX);if(!self || !association)return;
            QString identity;for(const auto &row:self->paramDetails)if(row[0]=="UserID")identity=row[1];
            QStringList matches;
            const QMap<QString,QString> names{{"129809","Part A"},{"129810","Part B"},{"129039","Position"}};
            // Stable report order, independent of receive/creation order, preserves
            // the established Part A-first guidance on Part B reports.
            for(const auto &pgn:QStringList{"129809","129810","129039"})
                for(const auto &candidate:model->getPgnList())if(candidate.idX!=idX && candidate.pgn==pgn)
                    for(const auto &row:candidate.paramDetails)if(row[0]=="UserID" && row[1]==identity && identity!="0" && identity!="4294967295")
                        matches<<names.value(candidate.pgn)+" ["+QString::number(candidate.idX)+(candidate.source.isEmpty()?QString():" / RX "+candidate.source)+"]";
            association->setText(matches.isEmpty()?"Standalone AIS report: no matching AIS MMSI.":"Matching "+matches.join(", ")+" by MMSI. Independent fields; no values copied.");
        };
        connectModel(model,&PgnTableModel::valuesChanged,widget,[model,update](int changed){const auto *data=model->findPgn(changed);if(data && QStringList{"129809","129810","129039"}.contains(data->pgn))update();});
        connectModel(this,&MainController::layoutObjectsChanged,widget,update);update();
    }

    // Widget notifications must not outlive the widget: use it as the
    // connection context so pending queued calls are removed on destruction.
    // Two-way bind for widgets based on the PGN
    if (Pgn == 126992) {
        // Cast widget to SystemTime126992* and call the initialize function
        auto *systemTimeWidget = qobject_cast<SystemTime126992 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            systemTimeWidget,
            &Attitude127257::enabledChanged,
            systemTimeWidget,
            [=]() {
            pgnTableController->setPgnEnabled(systemTimeWidget->Id(), systemTimeWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(systemTimeWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, systemTimeWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(systemTimeWidget->Id());
                disconnect(systemTimeWidget, nullptr, nullptr, nullptr);
                removeWidgetFromSubForm(systemTimeWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            systemTimeWidget,
            &SystemTime126992::SIDChanged,
            systemTimeWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(systemTimeWidget->SID()));
                onWidgetInstanceValueChanged(systemTimeWidget->Id());
            },
            parameterEditConnection);

        // Connect Date changes (widget -> model)
        connectModel(
            systemTimeWidget,
            &SystemTime126992::SystemDateChanged,
            systemTimeWidget,
            [=]() {
                model->changeParam(idX, "SystemDate", QString::number(systemTimeWidget->SystemDate()));
                onWidgetInstanceValueChanged(systemTimeWidget->Id());
            },
            parameterEditConnection);

        // Connect Time changes (widget -> model)
        connectModel(
            systemTimeWidget,
            &SystemTime126992::SystemTimeChanged,
            systemTimeWidget,
            [=]() {
                model->changeParam(idX, "SystemTime", QString::number(systemTimeWidget->SystemTime(), 'g', 15));
                onWidgetInstanceValueChanged(systemTimeWidget->Id());
            },
            parameterEditConnection);

        // Connect TimeSource changes (widget -> model)
        connectModel(
            systemTimeWidget,
            &SystemTime126992::TimeSourceChanged,
            systemTimeWidget,
            [=]() {
                model->changeParam(idX, "TimeSource", QString::number(systemTimeWidget->TimeSource()));
                onWidgetInstanceValueChanged(systemTimeWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, systemTimeWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? systemTimeWidget : nullptr);
                if (paramName == "SID") {
                    systemTimeWidget->setSID(value.toUInt());
                } else if (paramName == "SystemDate") {
                    systemTimeWidget->setSystemDate(value.toUInt());
                } else if (paramName == "SystemTime") {
                    systemTimeWidget->setSystemTime(value.toDouble());
                } else if (paramName == "TimeSource") {
                    systemTimeWidget->setTimeSource(static_cast<tN2kTimeSource>(value.toUInt()));
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorDateTimeWidget to systemTimeWidget
            if (generatorDateTimeWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorDateTime widget if it doesn't exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorDateTime Widget",
                                              "Would you like to create a GeneratorDateTime widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Date Time (PGN -129033)");
                } else {
                    qWarning() << "GeneratorDateTime widget is null";
                    return;
                }
            }

            connectWhileSenderAlive(
                generatorDateTimeWidget,
                &GeneratorDateTime::SIDChanged,
                systemTimeWidget,
                [this, systemTimeWidget]() { systemTimeWidget->updateSID(this->generatorDateTimeWidget->SID()); },
                Qt::QueuedConnection, "updateSID, SID");

            connectWhileSenderAlive(
                generatorDateTimeWidget,
                &GeneratorDateTime::DaysSince1970Changed,
                systemTimeWidget,
                [this, systemTimeWidget]() { systemTimeWidget->updateDaysSince1970(this->generatorDateTimeWidget->DaysSince1970()); },
                Qt::QueuedConnection, "updateDaysSince1970, DaysSince1970");

            connectWhileSenderAlive(
                generatorDateTimeWidget,
                &GeneratorDateTime::SecondsSinceMidnightChanged,
                systemTimeWidget,
                [this, systemTimeWidget]() { systemTimeWidget->updateSecondsSinceMidnight(this->generatorDateTimeWidget->SecondsSinceMidnight()); },
                Qt::QueuedConnection, "updateSecondsSinceMidnight, SecondsSinceMidnight");
        }
    } else if (Pgn == 127245) {
        // Cast widget to Rudder127245* and call the initialize function
        auto *rudderWidget = qobject_cast<Rudder127245 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            rudderWidget,
            &Rudder127245::enabledChanged,
            rudderWidget,
            [=]() {
            pgnTableController->setPgnEnabled(rudderWidget->Id(), rudderWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(rudderWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, rudderWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(rudderWidget->Id());
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect RudderPosition changes (widget -> model)
        connectModel(
            rudderWidget,
            &Rudder127245::RudderPositionChanged,
            rudderWidget,
            [=]() {
                model->changeParam(idX, "RudderPosition", QString::number(rudderWidget->RudderPosition(), 'g', 15));
                onWidgetInstanceValueChanged(rudderWidget->Id());
            },
            parameterEditConnection);

        // Connect Instance changes (widget -> model)
        connectModel(
            rudderWidget,
            &Rudder127245::InstanceChanged,
            rudderWidget,
            [=]() {
                model->changeParam(idX, "Instance", QString::number(rudderWidget->Instance()));
                onWidgetInstanceValueChanged(rudderWidget->Id());
            },
            parameterEditConnection);

        // Connect RudderDirectionOrder changes (widget -> model)
        connectModel(
            rudderWidget,
            &Rudder127245::RudderDirectionOrderChanged,
            rudderWidget,
            [=]() {
                model->changeParam(idX, "RudderDirectionOrder", QString::number(rudderWidget->RudderDirectionOrder()));
                onWidgetInstanceValueChanged(rudderWidget->Id());
            },
            parameterEditConnection);

        // Connect AngleOrder changes (widget -> model)
        connectModel(
            rudderWidget,
            &Rudder127245::AngleOrderChanged,
            rudderWidget,
            [=]() {
                model->changeParam(idX, "AngleOrder", QString::number(rudderWidget->AngleOrder(), 'g', 15));
                onWidgetInstanceValueChanged(rudderWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, rudderWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? rudderWidget : nullptr);
                if (paramName == "RudderPosition") {
                    rudderWidget->setRudderPosition(value.toDouble());
                } else if (paramName == "Instance") {
                    rudderWidget->setInstance(value.toUInt());
                } else if (paramName == "RudderDirectionOrder") {
                    rudderWidget->setRudderDirectionOrder(static_cast<tN2kRudderDirectionOrder>(value.toUInt()));
                } else if (paramName == "AngleOrder") {
                    rudderWidget->setAngleOrder(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovement to rudderWidget for the rudder position
            if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorVesselMovement widget if it doesn't exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorVesselMovement Widget",
                                              "Would you like to create a GeneratorVesselMovement widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Vessel Movement (PGN -129025)");
                } else {
                    qWarning() << "GeneratorVesselMovement widget is null";
                    return;
                }
            }

            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::RudderPositionChanged,
                rudderWidget,
                [this, rudderWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    rudderWidget->setRudderPosition(this->generatorVesselMovementWidget->RudderPosition());
                }
            },
                Qt::QueuedConnection, "setRudderPosition, RudderPosition");
        }
    } else if (Pgn == 130576) {
        // Cast widget to TrimTab130576* and call the initialize function
        auto *trimTabWidget = qobject_cast<TrimTab130576 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            trimTabWidget,
            &TrimTab130576::enabledChanged,
            trimTabWidget,
            [=]() {
            pgnTableController->setPgnEnabled(trimTabWidget->Id(), trimTabWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Propulsion";
        connectModel(trimTabWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, trimTabWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(trimTabWidget->Id());
                removeWidgetFromSubForm(trimTabWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect PortTrimTab changes (widget -> model)
        connectModel(
            trimTabWidget,
            &TrimTab130576::PortTrimTabChanged,
            trimTabWidget,
            [=]() {
                model->changeParam(idX, "PortTrimTab", QString::number(trimTabWidget->PortTrimTab()));
                onWidgetInstanceValueChanged(trimTabWidget->Id());
            },
            parameterEditConnection);

        // Connect StbdTrimTab changes (widget -> model)
        connectModel(
            trimTabWidget,
            &::TrimTab130576::StbdTrimTabChanged,
            this,
            [=]() {
                model->changeParam(idX, "StbdTrimTab", QString::number(trimTabWidget->StbdTrimTab()));
                onWidgetInstanceValueChanged(trimTabWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, trimTabWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? trimTabWidget : nullptr);
                if (paramName == "PortTrimTab") {
                    trimTabWidget->setPortTrimTab(value.toInt());
                } else if (paramName == "StbdTrimTab") {
                    trimTabWidget->setStbdTrimTab(value.toInt());
                }
            }
            //pgnTableController->updatePgnParameters(idX);
        });

        // Set widget properties to table model

        // // TODO: Add TrimTab properties to the GeneratorVesselMovement Widget
        // if(m_mode == Sender) {
        //     // Connect generatorVesselMovement to trimTabWidget for the trim tab positions
        //     if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
        //         // Ask user create a GeneratorVesselMovement widget if it doesn't exist
        //         QMessageBox::StandardButton reply;
        //         reply = QMessageBox::question(this->mdiArea,
        //                                       "No GeneratorVesselMovement Widget",
        //                                       "Would you like to create a GeneratorVesselMovement widget?",
        //                                       QMessageBox::Yes | QMessageBox::No);
        //         if (reply == QMessageBox::Yes) {
        //             addGeneratorWidget("Vessel Movement (PGN -129025)");
        //         } else {
        //             qWarning() << "GeneratorVesselMovement widget is null";
        //             return;
        //         }
        //     }

        //     connectWhileSenderAlive(
        //         generatorVesselMovementWidget,
        //         &GeneratorVesselMovement::PortTrimTabChanged,
        //         trimTabWidget,
        //         [this, trimTabWidget]() {
        //         if (this->generatorVesselMovementWidget->isEnabled()) {
        //             trimTabWidget->setPortTrimTab(this->generatorVesselMovementWidget->PortTrimTab());
        //         }
        //     },
        //         Qt::QueuedConnection);

        //     connectWhileSenderAlive(
        //         generatorVesselMovementWidget,
        //         &GeneratorVesselMovement::StbdTrimTabChanged,
        //         trimTabWidget,
        //         [this, trimTabWidget]() {
        //         if (this->generatorVesselMovementWidget->isEnabled()) {
        //             trimTabWidget->setStbdTrimTab(this->generatorVesselMovementWidget->StbdTrimTab());
        //         }
        //     },
        //         Qt::QueuedConnection);
        // }

    }
    else if (Pgn == 127250) {
        // Cast widget to VesselHeading127250* and call the initialize function
        auto *vesselHeadingWidget = qobject_cast<VesselHeading127250 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            vesselHeadingWidget,
            &Attitude127257::enabledChanged,
            vesselHeadingWidget,
            [=]() {
            pgnTableController->setPgnEnabled(vesselHeadingWidget->Id(), vesselHeadingWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(vesselHeadingWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, vesselHeadingWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(vesselHeadingWidget->Id());
                removeWidgetFromSubForm(vesselHeadingWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            vesselHeadingWidget,
            &VesselHeading127250::SIDChanged,
            vesselHeadingWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(vesselHeadingWidget->SID()));
                onWidgetInstanceValueChanged(vesselHeadingWidget->Id());
            },
            parameterEditConnection);

        // Connect Heading changes (widget -> model)
        connectModel(
            vesselHeadingWidget,
            &VesselHeading127250::HeadingChanged,
            vesselHeadingWidget,
            [=]() {
                model->changeParam(idX, "Heading", QString::number(vesselHeadingWidget->Heading(), 'g', 15));
                onWidgetInstanceValueChanged(vesselHeadingWidget->Id());
            },
            parameterEditConnection);

        // Connect Deviation changes (widget -> model)
        connectModel(
            vesselHeadingWidget,
            &VesselHeading127250::DeviationChanged,
            vesselHeadingWidget,
            [=]() {
                model->changeParam(idX, "Deviation", QString::number(vesselHeadingWidget->Deviation(), 'g', 15));
                onWidgetInstanceValueChanged(vesselHeadingWidget->Id());
            },
            parameterEditConnection);

        // Connect Variation changes (widget -> model)
        connectModel(
            vesselHeadingWidget,
            &VesselHeading127250::VariationChanged,
            vesselHeadingWidget,
            [=]() {
                model->changeParam(idX, "Variation", QString::number(vesselHeadingWidget->Variation(), 'g', 15));
                onWidgetInstanceValueChanged(vesselHeadingWidget->Id());
            },
            parameterEditConnection);

        // Connect HeadingReference changes (widget -> model)
        connectModel(
            vesselHeadingWidget,
            &VesselHeading127250::HeadingReferenceChanged,
            vesselHeadingWidget,
            [=]() {
                model->changeParam(idX, "HeadingReference", QString::number(vesselHeadingWidget->HeadingReference()));
                onWidgetInstanceValueChanged(vesselHeadingWidget->Id());
            },
            parameterEditConnection);

        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, vesselHeadingWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? vesselHeadingWidget : nullptr);
                if (paramName == "SID") {
                    vesselHeadingWidget->setSID(value.toUInt());
                } else if (paramName == "Heading") {
                    vesselHeadingWidget->setHeading(value.toDouble());
                } else if (paramName == "Deviation") {
                    vesselHeadingWidget->setDeviation(value.toDouble());
                } else if (paramName == "Variation") {
                    vesselHeadingWidget->setVariation(value.toDouble());
                } else if (paramName == "HeadingReference") {
                    vesselHeadingWidget->setHeadingReference(static_cast<tN2kHeadingReference>(value.toUInt()));
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovementWidget to vesselHeadingWidget
            if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorVesselMovement widget if it doesn't exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorVesselMovement Widget",
                                              "Would you like to create a GeneratorVesselMovement widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Vessel Movement (PGN -129025)");
                } else {
                    qWarning() << "GeneratorVesselMovement widget is null";
                    return;
                }
            }

            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::SIDChanged,
                vesselHeadingWidget,
                [this, vesselHeadingWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    vesselHeadingWidget->setSID(this->generatorVesselMovementWidget->SID());
                }
            },
                Qt::QueuedConnection, "setSID, SID");

            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::HeadingChanged,
                vesselHeadingWidget,
                [this, vesselHeadingWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    vesselHeadingWidget->setHeading(this->generatorVesselMovementWidget->Heading());
                }
            },
                Qt::QueuedConnection, "setHeading, Heading");

            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::DeviationChanged,
                vesselHeadingWidget,
                [this, vesselHeadingWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    vesselHeadingWidget->setDeviation(this->generatorVesselMovementWidget->Deviation());
                }
            },
                Qt::QueuedConnection, "setDeviation, Deviation");

            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::VariationChanged,
                vesselHeadingWidget,
                [this, vesselHeadingWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    vesselHeadingWidget->setVariation(this->generatorVesselMovementWidget->Variation());
                }
            },
                Qt::QueuedConnection, "setVariation, Variation");

            // Don't connect the heading reference generator to the vessel heading widget. Use True Heading instead.
            /*        connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::HeadingReferenceChanged,
                vesselHeadingWidget,
                [this, vesselHeadingWidget]() { vesselHeadingWidget->setHeadingReference(this->generatorVesselMovementWidget->HeadingReference()); },
                Qt::QueuedConnection, "setHeadingReference, HeadingReference");
    */
        }

    } else if (Pgn == 127251) {
        // Cast widget to RateOfTurn127251* and call the initialize function
        auto *rateOfTurnWidget = qobject_cast<RateOfTurn127251 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            rateOfTurnWidget,
            &Attitude127257::enabledChanged,
            rateOfTurnWidget,
            [=]() {
            pgnTableController->setPgnEnabled(rateOfTurnWidget->Id(), rateOfTurnWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect SID changes (widget -> model)
        connectModel(
            rateOfTurnWidget,
            &RateOfTurn127251::SIDChanged,
            rateOfTurnWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(rateOfTurnWidget->SID()));
                onWidgetInstanceValueChanged(rateOfTurnWidget->Id());
            },
            parameterEditConnection);

        // Connect RateOfTurn changes (widget -> model)
        connectModel(
            rateOfTurnWidget,
            &RateOfTurn127251::RateOfTurnChanged,
            rateOfTurnWidget,
            [=]() {
                model->changeParam(idX, "RateOfTurn", QString::number(rateOfTurnWidget->RateOfTurn(), 'g', 15));
                onWidgetInstanceValueChanged(rateOfTurnWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, rateOfTurnWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? rateOfTurnWidget : nullptr);
                if (paramName == "SID") {
                    rateOfTurnWidget->setSID(value.toUInt());
                } else if (paramName == "RateOfTurn") {
                    rateOfTurnWidget->setRateOfTurn(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovementWidget to rateOfTurnWidget
            if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorVesselMovement widget if it doesn't exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorVesselMovement Widget",
                                              "Would you like to create a GeneratorVesselMovement widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Vessel Movement (PGN -129025)");
                } else {
                    qWarning() << "GeneratorVesselMovement widget is null";
                    return;
                }
            }

            // Bind GeneratorVesselMovement to RateOfTurn
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::SIDChanged,
                rateOfTurnWidget,
                [this, rateOfTurnWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    rateOfTurnWidget->setSID(this->generatorVesselMovementWidget->SID());
                }
            },
                Qt::QueuedConnection, "setSID, SID");

            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::RateOfTurnChanged,
                rateOfTurnWidget,
                [this, rateOfTurnWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    rateOfTurnWidget->setRateOfTurn(this->generatorVesselMovementWidget->RateOfTurn());
                }
            },
                Qt::QueuedConnection, "setRateOfTurn, RateOfTurn");
        }


    } else if (Pgn == 127252) {
        // Cast widget to Heave127252* and call the initialize function
        auto *heaveWidget = qobject_cast<Heave127252 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            heaveWidget,
            &Attitude127257::enabledChanged,
            heaveWidget,
            [=]() {
            pgnTableController->setPgnEnabled(heaveWidget->Id(), heaveWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(heaveWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, heaveWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(heaveWidget->Id());
                removeWidgetFromSubForm(heaveWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            heaveWidget,
            &Heave127252::SIDChanged,
            heaveWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(heaveWidget->SID()));
                onWidgetInstanceValueChanged(heaveWidget->Id());
            },
            parameterEditConnection);

        // Connect Heave changes (widget -> model)
        connectModel(
            heaveWidget,
            &Heave127252::HeaveChanged,
            heaveWidget,
            [=]() {
                model->changeParam(idX, "Heave", QString::number(heaveWidget->Heave(), 'g', 15));
                onWidgetInstanceValueChanged(heaveWidget->Id());
            },
            parameterEditConnection);

        // Connect Delay changes (widget -> model)
        connectModel(
            heaveWidget,
            &Heave127252::DelayChanged,
            heaveWidget,
            [=]() {
                model->changeParam(idX, "Delay", QString::number(heaveWidget->Delay(), 'g', 15));
                onWidgetInstanceValueChanged(heaveWidget->Id());
            },
            parameterEditConnection);

        // Connect DelaySource changes (widget -> model)
        connectModel(
            heaveWidget,
            &Heave127252::DelaySourceChanged,
            heaveWidget,
            [=]() {
                model->changeParam(idX, "DelaySource", QString::number(heaveWidget->DelaySource()));
                onWidgetInstanceValueChanged(heaveWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, heaveWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? heaveWidget : nullptr);
                if (paramName == "SID") {
                    heaveWidget->setSID(value.toUInt());
                } else if (paramName == "Heave") {
                    heaveWidget->setHeave(value.toDouble());
                } else if (paramName == "Delay") {
                    heaveWidget->setDelay(value.toDouble());
                } else if (paramName == "DelaySource") {
                    heaveWidget->setDelaySource(static_cast<tN2kDelaySource>(value.toUInt()));
                }
            }
            //pgnTableController->updatePgnParameters(idX);
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovementWidget to heaveWidget
            if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorVesselMovement widget if it doesn't exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorVesselMovement Widget",
                                              "Would you like to create a GeneratorVesselMovement widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Vessel Movement (PGN -129025)");
                } else {
                    qWarning() << "GeneratorVesselMovement widget is null";
                    return;
                }
            }

            // Bind GeneratorVesselMovement to Heave
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::HeaveChanged,
                heaveWidget,
                [this, heaveWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    heaveWidget->setHeave(this->generatorVesselMovementWidget->Heave());
                }
            },
                Qt::QueuedConnection, "setHeave, Heave");
        }

    } else if (Pgn == 127257) {
        // Cast widget to Attitude127257* and call the initialize function
        auto *attitudeWidget = qobject_cast<Attitude127257 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            attitudeWidget,
            &Attitude127257::enabledChanged,
            attitudeWidget,
            [=]() {
            pgnTableController->setPgnEnabled(attitudeWidget->Id(), attitudeWidget->isEnabled());
        },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(attitudeWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, attitudeWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(attitudeWidget->Id());
                removeWidgetFromSubForm(attitudeWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            attitudeWidget,
            &Attitude127257::SIDChanged,
            attitudeWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(attitudeWidget->SID()));
                onWidgetInstanceValueChanged(attitudeWidget->Id());
            },
            parameterEditConnection);

        // Connect Yaw changes (widget -> model)
        connectModel(
            attitudeWidget,
            &Attitude127257::YawChanged,
            attitudeWidget,
            [=]() {
                model->changeParam(idX, "Yaw", QString::number(attitudeWidget->Yaw(), 'g', 15));
                onWidgetInstanceValueChanged(attitudeWidget->Id());
            },
            parameterEditConnection);

        // Connect Pitch changes (widget -> model)
        connectModel(
            attitudeWidget,
            &Attitude127257::PitchChanged,
            attitudeWidget,
            [=]() {
                model->changeParam(idX, "Pitch", QString::number(attitudeWidget->Pitch(), 'g', 15));
                onWidgetInstanceValueChanged(attitudeWidget->Id());
            },
            parameterEditConnection);

        // Connect Roll changes (widget -> model)
        connectModel(
            attitudeWidget,
            &Attitude127257::RollChanged,
            attitudeWidget,
            [=]() {
                model->changeParam(idX, "Roll", QString::number(attitudeWidget->Roll(), 'g', 15));
                onWidgetInstanceValueChanged(attitudeWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, attitudeWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? attitudeWidget : nullptr);
                if (paramName == "SID") {
                    attitudeWidget->setSID(value.toUInt());
                } else if (paramName == "Yaw") {
                    attitudeWidget->setYaw(value.toDouble());
                } else if (paramName == "Pitch") {
                    attitudeWidget->setPitch(value.toDouble());
                } else if (paramName == "Roll") {
                    attitudeWidget->setRoll(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovementWidget to attitudeWidget
            if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorVesselMovement widget if it doesn't exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorVesselMovement Widget",
                                              "Would you like to create a GeneratorVesselMovement widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Vessel Movement (PGN -129025)");
                } else {
                    qWarning() << "GeneratorVesselMovement widget is null";
                    return;
                }
            }

            qDebug() << "Binding GeneratorVesselMovement to Attitude Widget";
            // Connect Pitch
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::PitchChanged,
                attitudeWidget,
                [this, attitudeWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    attitudeWidget->setPitch(this->generatorVesselMovementWidget->Pitch());
                }
            },
                Qt::QueuedConnection, "setPitch, Pitch");

            // Connect Roll
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::RollChanged,
                attitudeWidget,
                [this, attitudeWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    attitudeWidget->setRoll(this->generatorVesselMovementWidget->Roll());
                }
            },
                Qt::QueuedConnection, "setRoll, Roll");
        }

    } else if (Pgn == 127258) {
        // Cast widget to MagneticVariation127258* and call the initialize function
        auto *magneticVariationWidget = qobject_cast<MagneticVariation127258 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            magneticVariationWidget,
            &Attitude127257::enabledChanged,
            magneticVariationWidget,
            [=]() {
            pgnTableController->setPgnEnabled(magneticVariationWidget->Id(), magneticVariationWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(magneticVariationWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, magneticVariationWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(magneticVariationWidget->Id());
                removeWidgetFromSubForm(magneticVariationWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            magneticVariationWidget,
            &MagneticVariation127258::SIDChanged,
            magneticVariationWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(magneticVariationWidget->SID()));
                onWidgetInstanceValueChanged(magneticVariationWidget->Id());
            },
            parameterEditConnection);

        // Connect Source changes (widget -> model)
        connectModel(
            magneticVariationWidget,
            &MagneticVariation127258::SourceChanged,
            magneticVariationWidget,
            [=]() {
                model->changeParam(idX, "Source", QString::number(magneticVariationWidget->Source()));
                onWidgetInstanceValueChanged(magneticVariationWidget->Id());
            },
            parameterEditConnection);

        // Connect DaysSince1970 changes (widget -> model)
        connectModel(
            magneticVariationWidget,
            &MagneticVariation127258::DaysSince1970Changed,
            magneticVariationWidget,
            [=]() {
                model->changeParam(idX, "DaysSince1970", QString::number(magneticVariationWidget->DaysSince1970()));
                onWidgetInstanceValueChanged(magneticVariationWidget->Id());
            },
            parameterEditConnection);

        // Connect MagneticVariation changes (widget -> model)
        connectModel(
            magneticVariationWidget,
            &MagneticVariation127258::VariationChanged,
            magneticVariationWidget,
            [=]() {
                model->changeParam(idX, "Variation", QString::number(magneticVariationWidget->Variation(), 'g', 15));
                onWidgetInstanceValueChanged(magneticVariationWidget->Id());
            },
            parameterEditConnection);

        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, magneticVariationWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? magneticVariationWidget : nullptr);
                if (paramName == "SID") {
                    magneticVariationWidget->setSID(value.toUInt());
                } else if (paramName == "Source") {
                    magneticVariationWidget->setSource(static_cast<tN2kMagneticVariation>(value.toUInt()));
                } else if (paramName == "DaysSince1970") {
                    magneticVariationWidget->setDaysSince1970(value.toUInt());
                } else if (paramName == "Variation") {
                    magneticVariationWidget->setVariation(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model

    } else if (Pgn == 127505) {
        // Cast widget to FluidLevel127505* and call the initialize function
        auto *fluidLevelWidget = qobject_cast<FluidLevel127505 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            fluidLevelWidget,
            &Attitude127257::enabledChanged,
            fluidLevelWidget,
            [=]() {
            pgnTableController->setPgnEnabled(fluidLevelWidget->Id(), fluidLevelWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Tanks";
        connectModel(fluidLevelWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, fluidLevelWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(fluidLevelWidget->Id());
                removeWidgetFromSubForm(fluidLevelWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect Instance changes (widget -> model)
        connectModel(
            fluidLevelWidget,
            &FluidLevel127505::InstanceChanged,
            fluidLevelWidget,
            [=]() {
                model->changeParam(idX, "Instance", QString::number(fluidLevelWidget->Instance()));
                onWidgetInstanceValueChanged(fluidLevelWidget->Id());
            },
            parameterEditConnection);

        // Connect FluidType changes (widget -> model)
        connectModel(
            fluidLevelWidget,
            &FluidLevel127505::FluidTypeChanged,
            fluidLevelWidget,
            [=]() {
                model->changeParam(idX, "FluidType", QString::number(fluidLevelWidget->FluidType()));
                onWidgetInstanceValueChanged(fluidLevelWidget->Id());
            },
            parameterEditConnection);

        // Connect Level changes (widget -> model)
        connectModel(
            fluidLevelWidget,
            &FluidLevel127505::LevelChanged,
            fluidLevelWidget,
            [=]() {
                model->changeParam(idX, "Level", QString::number(fluidLevelWidget->Level(), 'g', 15));
                onWidgetInstanceValueChanged(fluidLevelWidget->Id());
            },
            parameterEditConnection);

        // Connect Capacity changes (widget -> model)
        connectModel(
            fluidLevelWidget,
            &FluidLevel127505::CapacityChanged,
            fluidLevelWidget,
            [=]() {
                model->changeParam(idX, "Capacity", QString::number(fluidLevelWidget->Capacity(), 'g', 15));
                onWidgetInstanceValueChanged(fluidLevelWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, fluidLevelWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? fluidLevelWidget : nullptr);
                if (paramName == "Instance") {
                    fluidLevelWidget->setInstance(value.toUInt());
                } else if (paramName == "FluidType") {
                    fluidLevelWidget->setFluidType(static_cast<tN2kFluidType>(value.toUInt()));
                } else if (paramName == "Level") {
                    fluidLevelWidget->setLevel(value.toDouble());
                } else if (paramName == "Capacity") {
                    fluidLevelWidget->setCapacity(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model

    } else if (Pgn == 127506) {
        // Cast widget to DcStatus127506* and call the initialize function
        auto *dcStatusWidget = qobject_cast<DcStatus127506 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            dcStatusWidget,
            &DcStatus127506::enabledChanged,
            dcStatusWidget,
            [=]() {
            pgnTableController->setPgnEnabled(dcStatusWidget->Id(), dcStatusWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Electrical";
        connectModel(dcStatusWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, dcStatusWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(dcStatusWidget->Id());
                removeWidgetFromSubForm(dcStatusWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            dcStatusWidget,
            &DcStatus127506::SIDChanged,
            dcStatusWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(dcStatusWidget->SID()));
                onWidgetInstanceValueChanged(dcStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect DCInstance changes (widget -> model)
        connectModel(
            dcStatusWidget,
            &DcStatus127506::DCInstanceChanged,
            dcStatusWidget,
            [=]() {
                model->changeParam(idX, "DCInstance", QString::number(dcStatusWidget->DCInstance()));
                onWidgetInstanceValueChanged(dcStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect DCType changes (widget -> model)
        connectModel(
            dcStatusWidget,
            &DcStatus127506::DCTypeChanged,
            dcStatusWidget,
            [=]() {
                model->changeParam(idX, "DCType", QString::number(dcStatusWidget->DCType()));
                onWidgetInstanceValueChanged(dcStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect StateOfCharge changes (widget -> model)
        connectModel(
            dcStatusWidget,
            &DcStatus127506::StateOfChargeChanged,
            dcStatusWidget,
            [=]() {
                model->changeParam(idX, "StateOfCharge", QString::number(dcStatusWidget->StateOfCharge()));
                onWidgetInstanceValueChanged(dcStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect StateOfHealth changes (widget -> model)
        connectModel(
            dcStatusWidget,
            &DcStatus127506::StateOfHealthChanged,
            dcStatusWidget,
            [=]() {
                model->changeParam(idX, "StateOfHealth", QString::number(dcStatusWidget->StateOfHealth()));
                onWidgetInstanceValueChanged(dcStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect TimeRemaining changes (widget -> model)
        connectModel(
            dcStatusWidget,
            &DcStatus127506::TimeRemainingChanged,
            dcStatusWidget,
            [=]() {
                model->changeParam(idX, "TimeRemaining", QString::number(dcStatusWidget->TimeRemaining(), 'g', 15));
                onWidgetInstanceValueChanged(dcStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect RippleVoltage changes (widget -> model)
        connectModel(
            dcStatusWidget,
            &DcStatus127506::RippleVoltageChanged,
            dcStatusWidget,
            [=]() {
                model->changeParam(idX, "RippleVoltage", QString::number(dcStatusWidget->RippleVoltage(), 'g', 15));
                onWidgetInstanceValueChanged(dcStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect Capacity changes (widget -> model)
        connectModel(
            dcStatusWidget,
            &DcStatus127506::CapacityChanged,
            dcStatusWidget,
            [=]() {
                model->changeParam(idX, "Capacity", QString::number(dcStatusWidget->Capacity(), 'g', 15));
                onWidgetInstanceValueChanged(dcStatusWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, dcStatusWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? dcStatusWidget : nullptr);
                if (paramName == "SID") {
                    dcStatusWidget->setSID(value.toUInt());
                } else if (paramName == "DCInstance") {
                    dcStatusWidget->setDCInstance(value.toUInt());
                } else if (paramName == "DCType") {
                    dcStatusWidget->setDCType(static_cast<tN2kDCType>(value.toUInt()));
                } else if (paramName == "StateOfCharge") {
                    dcStatusWidget->setStateOfCharge(value.toDouble());
                } else if (paramName == "StateOfHealth") {
                    dcStatusWidget->setStateOfHealth(value.toDouble());
                } else if (paramName == "TimeRemaining") {
                    dcStatusWidget->setTimeRemaining(value.toDouble());
                } else if (paramName == "RippleVoltage") {
                    dcStatusWidget->setRippleVoltage(value.toDouble());
                } else if (paramName == "Capacity") {
                    dcStatusWidget->setCapacity(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model
    } else if (Pgn == 127507) {
        // Cast widget to ChargerStatus127507* and call the initialize function
        auto *chargerStatusWidget = qobject_cast<ChargerStatus127507 *>(widget);

        // Connect the EnabledChanged signal in the base class to table controller setPgnEnabled
        connectModel(
            chargerStatusWidget,
            &ChargerStatus127507::enabledChanged,
            chargerStatusWidget,
            [=]() {
            pgnTableController->setPgnEnabled(chargerStatusWidget->Id(), chargerStatusWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Electrical";
        connectModel(chargerStatusWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, chargerStatusWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(chargerStatusWidget->Id());
                removeWidgetFromSubForm(chargerStatusWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect Instance changes (widget -> model)
        connectModel(
            chargerStatusWidget,
            &ChargerStatus127507::InstanceChanged,
            chargerStatusWidget,
            [=]() {
                model->changeParam(idX, "Instance", QString::number(chargerStatusWidget->Instance()));
                onWidgetInstanceValueChanged(chargerStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect BatteryInstance changes (widget -> model)
        connectModel(
            chargerStatusWidget,
            &ChargerStatus127507::BatteryInstanceChanged,
            chargerStatusWidget,
            [=]() {
                model->changeParam(idX, "BatteryInstance", QString::number(chargerStatusWidget->BatteryInstance()));
                onWidgetInstanceValueChanged(chargerStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect ChargeState changes (widget -> model)
        connectModel(
            chargerStatusWidget,
            &ChargerStatus127507::ChargeStateChanged,
            chargerStatusWidget,
            [=]() {
                model->changeParam(idX, "ChargeState", QString::number(chargerStatusWidget->ChargeState()));
                onWidgetInstanceValueChanged(chargerStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect ChargerMode changes (widget -> model)
        connectModel(
            chargerStatusWidget,
            &ChargerStatus127507::ChargerModeChanged,
            chargerStatusWidget,
            [=]() {
                model->changeParam(idX, "ChargerMode", QString::number(chargerStatusWidget->ChargerMode()));
                onWidgetInstanceValueChanged(chargerStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect Enabled changes (widget -> model)
        connectModel(
            chargerStatusWidget,
            &ChargerStatus127507::EnabledChanged,
            chargerStatusWidget,
            [=]() {
                model->changeParam(idX, "Enabled", QString::number(chargerStatusWidget->Enabled()));
                onWidgetInstanceValueChanged(chargerStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect EqualizationPending changes (widget -> model)
        connectModel(
            chargerStatusWidget,
            &ChargerStatus127507::EqualizationPendingChanged,
            chargerStatusWidget,
            [=]() {
                model->changeParam(idX, "EqualizationPending", QString::number(chargerStatusWidget->EqualizationPending()));
                onWidgetInstanceValueChanged(chargerStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect EqualizationTimeRemaining changes (widget -> model)
        connectModel(
            chargerStatusWidget,
            &ChargerStatus127507::EqualizationTimeRemainingChanged,
            chargerStatusWidget,
            [=]() {
                model->changeParam(idX, "EqualizationTimeRemaining", QString::number(chargerStatusWidget->EqualizationTimeRemaining(), 'g', 15));
                onWidgetInstanceValueChanged(chargerStatusWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, chargerStatusWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? chargerStatusWidget : nullptr);
                if (paramName == "Instance") {
                    chargerStatusWidget->setInstance(value.toUInt());
                } else if (paramName == "BatteryInstance") {
                    chargerStatusWidget->setBatteryInstance(value.toUInt());
                } else if (paramName == "ChargeState") {
                    chargerStatusWidget->setChargeState(static_cast<tN2kChargeState>(value.toUInt()));
                } else if (paramName == "ChargerMode") {
                    chargerStatusWidget->setChargerMode(static_cast<tN2kChargerMode>(value.toUInt()));
                } else if (paramName == "Enabled") {
                    chargerStatusWidget->setEnabled(static_cast<tN2kOnOff>(value.toUInt()));
                } else if (paramName == "EqualizationPending") {
                    chargerStatusWidget->setEqualizationPending(static_cast<tN2kOnOff>(value.toUInt()));
                } else if (paramName == "EqualizationTimeRemaining") {
                    chargerStatusWidget->setEqualizationTimeRemaining(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model

    } else if (Pgn == 127508) {
        // Cast widget to BatteryStatus127508* and call the initialize function
        auto *batteryStatusWidget = qobject_cast<BatteryStatus127508 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            batteryStatusWidget,
            &BatteryStatus127508::enabledChanged,
            batteryStatusWidget,
            [=]() {
            pgnTableController->setPgnEnabled(batteryStatusWidget->Id(), batteryStatusWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Electrical";
        connectModel(batteryStatusWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, batteryStatusWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(batteryStatusWidget->Id());
                removeWidgetFromSubForm(batteryStatusWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect BatteryInstance changes (widget -> model)
        connectModel(
            batteryStatusWidget,
            &BatteryStatus127508::BatteryInstanceChanged,
            batteryStatusWidget,
            [=]() {
                model->changeParam(idX, "BatteryInstance", QString::number(batteryStatusWidget->BatteryInstance()));
                onWidgetInstanceValueChanged(batteryStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect BatteryVoltage changes (widget -> model)
        connectModel(
            batteryStatusWidget,
            &BatteryStatus127508::BatteryVoltageChanged,
            batteryStatusWidget,
            [=]() {
                model->changeParam(idX, "BatteryVoltage", QString::number(batteryStatusWidget->BatteryVoltage(), 'g', 15));
                onWidgetInstanceValueChanged(batteryStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect BatteryCurrent changes (widget -> model)
        connectModel(
            batteryStatusWidget,
            &BatteryStatus127508::BatteryCurrentChanged,
            batteryStatusWidget,
            [=]() {
                model->changeParam(idX, "BatteryCurrent", QString::number(batteryStatusWidget->BatteryCurrent(), 'g', 15));
                onWidgetInstanceValueChanged(batteryStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect BatteryTemperature changes (widget -> model)
        connectModel(
            batteryStatusWidget,
            &BatteryStatus127508::BatteryTemperatureChanged,
            batteryStatusWidget,
            [=]() {
                model->changeParam(idX, "BatteryTemperature", QString::number(batteryStatusWidget->BatteryTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(batteryStatusWidget->Id());
            },
            parameterEditConnection);

        // Connect SID changes (widget -> model)
        connectModel(
            batteryStatusWidget,
            &BatteryStatus127508::SIDChanged,
            batteryStatusWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(batteryStatusWidget->SID()));
                onWidgetInstanceValueChanged(batteryStatusWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, batteryStatusWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? batteryStatusWidget : nullptr);
                if (paramName == "BatteryInstance") {
                    batteryStatusWidget->setBatteryInstance(value.toUInt());
                } else if (paramName == "BatteryVoltage") {
                    batteryStatusWidget->setBatteryVoltage(value.toDouble());
                } else if (paramName == "BatteryCurrent") {
                    batteryStatusWidget->setBatteryCurrent(value.toDouble());
                } else if (paramName == "BatteryTemperature") {
                    batteryStatusWidget->setBatteryTemperature(value.toDouble());
                } else if (paramName == "SID") {
                    batteryStatusWidget->setSID(value.toUInt());
                }
            }
            //pgnTableController->updatePgnParameters(idX);
        });

        // Set widget properties to table model
    } else if (Pgn == 128259) {
        // Cast widget to Speed128259* and call the initialize function
        auto *speedWidget = qobject_cast<Speed128259 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            speedWidget,
            &Attitude127257::enabledChanged,
            speedWidget,
            [=]() {
            pgnTableController->setPgnEnabled(speedWidget->Id(), speedWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(speedWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, speedWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(speedWidget->Id());
                removeWidgetFromSubForm(speedWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            speedWidget,
            &Speed128259::SIDChanged,
            speedWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(speedWidget->SID()));
                onWidgetInstanceValueChanged(speedWidget->Id());
            },
            parameterEditConnection);

        // Connect WaterReferenced changes (widget -> model)
        connectModel(
            speedWidget,
            &Speed128259::WaterReferencedChanged,
            speedWidget,
            [=]() {
                model->changeParam(idX, "WaterReferenced", QString::number(speedWidget->WaterReferenced(), 'g', 15));
                onWidgetInstanceValueChanged(speedWidget->Id());
            },
            parameterEditConnection);

        // Connect GroundReferenced changes (widget -> model)
        connectModel(
            speedWidget,
            &Speed128259::GroundReferencedChanged,
            speedWidget,
            [=]() {
                model->changeParam(idX, "GroundReferenced", QString::number(speedWidget->GroundReferenced(), 'g', 15));
                onWidgetInstanceValueChanged(speedWidget->Id());
            },
            parameterEditConnection);

        // Connect SWRT changes (widget -> model)
        connectModel(
            speedWidget,
            &Speed128259::SWRTChanged,
            speedWidget,
            [=]() {
                model->changeParam(idX, "SWRT", QString::number(speedWidget->SWRT()));
                onWidgetInstanceValueChanged(speedWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, speedWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? speedWidget : nullptr);
                if (paramName == "SID") {
                    speedWidget->setSID(value.toUInt());
                } else if (paramName == "WaterReferenced") {
                    speedWidget->setWaterReferenced(value.toDouble());
                } else if (paramName == "GroundReferenced") {
                    speedWidget->setGroundReferenced(value.toDouble());
                } else if (paramName == "SWRT") {
                    speedWidget->setSWRT(static_cast<tN2kSpeedWaterReferenceType>(value.toUInt()));
                }
            }
            //pgnTableController->updatePgnParameters(idX);

        });

        // Set widget properties to table model
    } else if (Pgn == 128267) {
        // Cast widget to Depth128267* and call the initialize function
        auto *depthWidget = qobject_cast<Depth128267 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            depthWidget,
            &Attitude127257::enabledChanged,
            depthWidget,
            [=]() {
            pgnTableController->setPgnEnabled(depthWidget->Id(), depthWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(depthWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, depthWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(depthWidget->Id());
                removeWidgetFromSubForm(depthWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            depthWidget,
            &Depth128267::SIDChanged,
            depthWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(depthWidget->SID()));
                onWidgetInstanceValueChanged(depthWidget->Id());
            },
            parameterEditConnection);

        // Connect DepthBelowTransducer changes (widget -> model)
        connectModel(
            depthWidget,
            &Depth128267::DepthBelowTransducerChanged,
            depthWidget,
            [=]() {
                model->changeParam(idX, "DepthBelowTransducer", QString::number(depthWidget->DepthBelowTransducer(), 'g', 15));
                onWidgetInstanceValueChanged(depthWidget->Id());
            },
            parameterEditConnection);

        // Connect Offset changes (widget -> model)
        connectModel(
            depthWidget,
            &Depth128267::OffsetChanged,
            depthWidget,
            [=]() {
                model->changeParam(idX, "Offset", QString::number(depthWidget->Offset(), 'g', 15));
                onWidgetInstanceValueChanged(depthWidget->Id());
            },
            parameterEditConnection);

        // Connect Range changes (widget -> model)
        connectModel(
            depthWidget,
            &Depth128267::RangeChanged,
            depthWidget,
            [=]() {
                model->changeParam(idX, "Range", QString::number(depthWidget->Range(), 'g', 15));
                onWidgetInstanceValueChanged(depthWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, depthWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? depthWidget : nullptr);
                if (paramName == "SID") {
                    depthWidget->setSID(value.toUInt());
                } else if (paramName == "DepthBelowTransducer") {
                    depthWidget->setDepthBelowTransducer(value.toDouble());
                } else if (paramName == "Offset") {
                    depthWidget->setOffset(value.toDouble());
                } else if (paramName == "Range") {
                    depthWidget->setRange(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);
        });

        // Set widget properties to table model
    }
    else if (Pgn == 129025) {
        // Cast widget to LatLong129026* and call the initialize function
        auto *latLongWidget = qobject_cast<LatLong129025 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            latLongWidget,
            &Attitude127257::enabledChanged,
            latLongWidget,
            [=]() {
            pgnTableController->setPgnEnabled(latLongWidget->Id(), latLongWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(latLongWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, latLongWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(latLongWidget->Id());
                removeWidgetFromSubForm(latLongWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect Latitude changes (widget -> model)
        connectModel(
            latLongWidget,
            &LatLong129025::LatitudeChanged,
            latLongWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Latitude", latLongWidget->Latitude());
                onWidgetInstanceValueChanged(latLongWidget->Id());
            },
            parameterEditConnection);

        // Connect Longitude changes (widget -> model)
        connectModel(
            latLongWidget,
            &LatLong129025::LongitudeChanged,
            latLongWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Longitude", latLongWidget->Longitude());
                onWidgetInstanceValueChanged(latLongWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, latLongWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? latLongWidget : nullptr);
                if (paramName == "Latitude") {
                    latLongWidget->setLatitude(value.toDouble());
                } else if (paramName == "Longitude") {
                    latLongWidget->setLongitude(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);


        });

        // Set widget properties to table model

        if (m_mode == Sender) {
            // Connect generatorVesselMovement Widget signals to latLongWidget
            if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorVesselMovement widget if it doesn't exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorVesselMovement Widget",
                                              "Would you like to create a GeneratorVesselMovement widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Vessel Movement (PGN -129025)");
                } else {
                    qWarning() << "generatorVesselMovementWidget is null";
                    return;
                }
            }

            // Connect Latitude changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::LatitudeChanged,
                latLongWidget,
                [this, latLongWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    latLongWidget->setLatitude(this->generatorVesselMovementWidget->Latitude());
                }
            },
                Qt::QueuedConnection, "setLatitude, Latitude");

            // Connect Longitude changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::LongitudeChanged,
                latLongWidget,
                [this, latLongWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    latLongWidget->setLongitude(this->generatorVesselMovementWidget->Longitude());
                }
            },
                Qt::QueuedConnection, "setLongitude, Longitude");
        }

    } else if (Pgn == 129026) {
        // Cast widget to CogSog129026* and call the initialize function
        auto *cogSogWidget = qobject_cast<CogSog129026 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            cogSogWidget,
            &Attitude127257::enabledChanged,
            cogSogWidget,
            [=]() {
            pgnTableController->setPgnEnabled(cogSogWidget->Id(), cogSogWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(cogSogWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, cogSogWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(cogSogWidget->Id());
                removeWidgetFromSubForm(cogSogWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            cogSogWidget,
            &CogSog129026::SIDChanged,
            cogSogWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(cogSogWidget->SID()));
                onWidgetInstanceValueChanged(cogSogWidget->Id());
            },
            parameterEditConnection);

        // Connect HeadingReference changes (widget -> model)
        connectModel(
            cogSogWidget,
            &CogSog129026::HeadingReferenceChanged,
            cogSogWidget,
            [=]() {
                model->changeParam(idX, "HeadingReference", QString::number(cogSogWidget->HeadingReference()));
                onWidgetInstanceValueChanged(cogSogWidget->Id());
            },
            parameterEditConnection);

        // Connect COG changes (widget -> model)
        connectModel(
            cogSogWidget,
            &CogSog129026::COGChanged,
            cogSogWidget,
            [=]() {
                model->changeParam(idX, "COG", QString::number(cogSogWidget->COG(), 'g', 15));
                onWidgetInstanceValueChanged(cogSogWidget->Id());
            },
            parameterEditConnection);

        // Connect SOG changes (widget -> model)
        connectModel(
            cogSogWidget,
            &CogSog129026::SOGChanged,
            cogSogWidget,
            [=]() {
                model->changeParam(idX, "SOG", QString::number(cogSogWidget->SOG(), 'g', 15));
                onWidgetInstanceValueChanged(cogSogWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, cogSogWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? cogSogWidget : nullptr);
                if (paramName == "SID") {
                    cogSogWidget->setSID(value.toUInt());
                } else if (paramName == "HeadingReference") {
                    cogSogWidget->setHeadingReference(static_cast<tN2kHeadingReference>(value.toUInt()));
                } else if (paramName == "COG") {
                    cogSogWidget->setCOG(value.toDouble());
                } else if (paramName == "SOG") {
                    cogSogWidget->setSOG(value.toDouble());
                }
            }
            //pgnTableController->updatePgnParameters(idX);
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovement Widget signals to cogSogWidget
            if (generatorVesselMovementWidget != nullptr) {
                // Connect HeadingReference changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::HeadingReferenceChanged,
                    cogSogWidget,
                    [this, cogSogWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        cogSogWidget->setHeadingReference(this->generatorVesselMovementWidget->HeadingReference());
                    }
                },
                    Qt::QueuedConnection, "setHeadingReference, HeadingReference");

                // Connect COG changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::COGChanged,
                    cogSogWidget,
                    [this, cogSogWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        cogSogWidget->setCOG(this->generatorVesselMovementWidget->COG());
                    }
                },
                    Qt::QueuedConnection, "setCOG, COG");

                // Connect SOG changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::SOGChanged,
                    cogSogWidget,
                    [this, cogSogWidget]() {
                    if(this->generatorVesselMovementWidget->isEnabled()) {
                        cogSogWidget->setSOG(this->generatorVesselMovementWidget->SOG());
                    }
                },
                    Qt::QueuedConnection, "setSOG, SOG");
            }
        }

    } else if (Pgn == 129029) {
        // Cast widget to GnssPositionData129029* and call the initialize function
        auto *gnssPositionDataWidget = qobject_cast<GnssPositionData129029 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            gnssPositionDataWidget,
            &Attitude127257::enabledChanged,
            gnssPositionDataWidget,
            [=]() {
            pgnTableController->setPgnEnabled(gnssPositionDataWidget->Id(), gnssPositionDataWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(gnssPositionDataWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, gnssPositionDataWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(gnssPositionDataWidget->Id());
                removeWidgetFromSubForm(gnssPositionDataWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::SIDChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(gnssPositionDataWidget->SID()));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect DaysSince1970 changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::DaysSince1970Changed,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "DaysSince1970", QString::number(gnssPositionDataWidget->DaysSince1970()));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect SecondsSinceMidnight changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::SecondsSinceMidnightChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "SecondsSinceMidnight", QString::number(gnssPositionDataWidget->SecondsSinceMidnight(), 'g', 15));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect Latitude changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::LatitudeChanged,
            gnssPositionDataWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Latitude", gnssPositionDataWidget->Latitude());
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect Longitude changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::LongitudeChanged,
            gnssPositionDataWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Longitude", gnssPositionDataWidget->Longitude());
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect Altitude changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::AltitudeChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "Altitude", QString::number(gnssPositionDataWidget->Altitude(), 'g', 15));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect GNSStype changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::GNSStypeChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "GNSStype", QString::number(gnssPositionDataWidget->GNSStype()));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect GNSSmethod changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::GNSSmethodChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "GNSSmethod", QString::number(gnssPositionDataWidget->GNSSmethod()));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect nSatellites changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::nSatellitesChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "nSatellites", QString::number(gnssPositionDataWidget->nSatellites()));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect HDOP changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::HDOPChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "HDOP", QString::number(gnssPositionDataWidget->HDOP(), 'g', 15));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect PDOP changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::PDOPChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "PDOP", QString::number(gnssPositionDataWidget->PDOP(), 'g', 15));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect GeoidalSeparation changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::GeoidalSeparationChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "GeoidalSeparation", QString::number(gnssPositionDataWidget->GeoidalSeparation(), 'g', 15));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect nReferenceStations changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::nReferenceStationsChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "nReferenceStations", QString::number(gnssPositionDataWidget->nReferenceStations()));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect ReferenceStationType changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::ReferenceStationTypeChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "ReferenceStationType", QString::number(gnssPositionDataWidget->ReferenceStationType()));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect DGPSStationID changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::ReferenceSationIDChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "ReferenceSationID", QString::number(gnssPositionDataWidget->ReferenceSationID()));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);

        // Connect AgeOfDGPSdata changes (widget -> model)
        connectModel(
            gnssPositionDataWidget,
            &GnssPositionData129029::AgeOfCorrectionChanged,
            gnssPositionDataWidget,
            [=]() {
                model->changeParam(idX, "AgeOfCorrection", QString::number(gnssPositionDataWidget->AgeOfCorrection(), 'g', 15));
                onWidgetInstanceValueChanged(gnssPositionDataWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, gnssPositionDataWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? gnssPositionDataWidget : nullptr);
                if (paramName == "SID") {
                    gnssPositionDataWidget->setSID(value.toUInt());
                } else if (paramName == "DaysSince1970") {
                    gnssPositionDataWidget->setDaysSince1970(value.toUInt());
                } else if (paramName == "SecondsSinceMidnight") {
                    gnssPositionDataWidget->setSecondsSinceMidnight(value.toDouble());
                } else if (paramName == "Latitude") {
                    gnssPositionDataWidget->setLatitude(value.toDouble());
                } else if (paramName == "Longitude") {
                    gnssPositionDataWidget->setLongitude(value.toDouble());
                } else if (paramName == "Altitude") {
                    gnssPositionDataWidget->setAltitude(value.toDouble());
                } else if (paramName == "GNSStype") {
                    gnssPositionDataWidget->setGNSStype(static_cast<tN2kGNSStype>(value.toUInt()));
                } else if (paramName == "GNSSmethod") {
                    gnssPositionDataWidget->setGNSSmethod(static_cast<tN2kGNSSmethod>(value.toUInt()));
                } else if (paramName == "nSatellites") {
                    gnssPositionDataWidget->setnSatellites(value.toUInt());
                } else if (paramName == "HDOP") {
                    gnssPositionDataWidget->setHDOP(value.toDouble());
                } else if (paramName == "PDOP") {
                    gnssPositionDataWidget->setPDOP(value.toDouble());
                } else if (paramName == "GeoidalSeparation") {
                    gnssPositionDataWidget->setGeoidalSeparation(value.toDouble());
                } else if (paramName == "AgeOfCorrection") {
                    gnssPositionDataWidget->setAgeOfCorrection(value.toDouble());
                } else if (paramName == "ReferenceSationID") {
                    gnssPositionDataWidget->setReferenceSationID(value.toUInt());
                } else if (paramName == "nReferenceStations") {
                    gnssPositionDataWidget->setnReferenceStations(value.toUInt());
                } else if (paramName == "ReferenceStationType") {
                    gnssPositionDataWidget->setReferenceStationType(static_cast<tN2kGNSStype>(value.toUInt()));
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorDateTimeWidget to gnssPositionDataWidget
            if (generatorDateTimeWidget != nullptr) {
                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::SIDChanged,
                    gnssPositionDataWidget,
                    [this, gnssPositionDataWidget]() { gnssPositionDataWidget->updateSID(this->generatorDateTimeWidget->SID()); },
                    Qt::QueuedConnection, "updateSID, SID");

                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::DaysSince1970Changed,
                    gnssPositionDataWidget,
                    [this, gnssPositionDataWidget]() { gnssPositionDataWidget->updateDaysSince1970(this->generatorDateTimeWidget->DaysSince1970()); },
                    Qt::QueuedConnection, "updateDaysSince1970, DaysSince1970");

                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::SecondsSinceMidnightChanged,
                    gnssPositionDataWidget,
                    [this, gnssPositionDataWidget]() { gnssPositionDataWidget->updateSecondsSinceMidnight(this->generatorDateTimeWidget->SecondsSinceMidnight()); },
                    Qt::QueuedConnection, "updateSecondsSinceMidnight, SecondsSinceMidnight");
            }

            // Connect generatorVesselMovement Widget signals to gnssPositionDataWidget
            if (generatorVesselMovementWidget != nullptr) {
                // Connect Latitude changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::LatitudeChanged,
                    gnssPositionDataWidget,
                    [this, gnssPositionDataWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        gnssPositionDataWidget->setLatitude(this->generatorVesselMovementWidget->Latitude());
                    }
                },
                    Qt::QueuedConnection, "setLatitude, Latitude");

                // Connect Longitude changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::LongitudeChanged,
                    gnssPositionDataWidget,
                    [this, gnssPositionDataWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        gnssPositionDataWidget->setLongitude(this->generatorVesselMovementWidget->Longitude());
                    }
                },
                    Qt::QueuedConnection, "setLongitude, Longitude");
            }
        }

    } else if (Pgn == 129033) {
        // Cast widget to DateTime129033* and call the initialize function
        auto *dateTimeWidget = qobject_cast<DateTime129033 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            dateTimeWidget,
            &Attitude127257::enabledChanged,
            dateTimeWidget,
            [=]() {
            pgnTableController->setPgnEnabled(dateTimeWidget->Id(), dateTimeWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(dateTimeWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, dateTimeWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(dateTimeWidget->Id());
                removeWidgetFromSubForm(dateTimeWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });
        // Connect DaysSince1970 changes (widget -> model)
        connectModel(
            dateTimeWidget,
            &DateTime129033::DaysSince1970Changed,
            dateTimeWidget,
            [=]() {
                model->changeParam(idX, "DaysSince1970", QString::number(dateTimeWidget->DaysSince1970()));
                onWidgetInstanceValueChanged(dateTimeWidget->Id());
            },
            parameterEditConnection);

        // Connect SecondsSinceMidnight changes (widget -> model)
        connectModel(
            dateTimeWidget,
            &DateTime129033::SecondsSinceMidnightChanged,
            dateTimeWidget,
            [=]() {
                model->changeParam(idX, "SecondsSinceMidnight", QString::number(dateTimeWidget->SecondsSinceMidnight(), 'g', 15));
                onWidgetInstanceValueChanged(dateTimeWidget->Id());
            },
            parameterEditConnection);

        // Connect LocalOffset changes (widget -> model)
        connectModel(
            dateTimeWidget,
            &DateTime129033::LocalOffsetChanged,
            dateTimeWidget,
            [=]() {
                model->changeParam(idX, "LocalOffset", QString::number(dateTimeWidget->LocalOffset()));
                onWidgetInstanceValueChanged(dateTimeWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, dateTimeWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? dateTimeWidget : nullptr);
                if (paramName == "DaysSince1970") {
                    dateTimeWidget->setDaysSince1970(value.toUInt());
                } else if (paramName == "SecondsSinceMidnight") {
                    dateTimeWidget->setSecondsSinceMidnight(value.toDouble());
                } else if (paramName == "LocalOffset") {
                    dateTimeWidget->setLocalOffset(value.toInt());
                }
            }
            //pgnTableController->updatePgnParameters(idX);
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorDateTimeWidget signals to dateTimeWidget properties
            if (generatorDateTimeWidget != nullptr) {
                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::DaysSince1970Changed,
                    dateTimeWidget,
                    [this, dateTimeWidget]() { dateTimeWidget->updateDaysSince1970(this->generatorDateTimeWidget->DaysSince1970()); },
                    Qt::QueuedConnection, "updateDaysSince1970, DaysSince1970");

                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::SecondsSinceMidnightChanged,
                    dateTimeWidget,
                    [this, dateTimeWidget]() { dateTimeWidget->updateSecondsSinceMidnight(this->generatorDateTimeWidget->SecondsSinceMidnight()); },
                    Qt::QueuedConnection, "updateSecondsSinceMidnight, SecondsSinceMidnight");

                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::LocalOffsetChanged,
                    dateTimeWidget,
                    [this, dateTimeWidget]() { dateTimeWidget->updateLocalOffset(this->generatorDateTimeWidget->LocalOffset()); },
                    Qt::QueuedConnection, "updateLocalOffset, LocalOffset");
            }
        }

    } else if (auto *instrument=qobject_cast<ExpansionInstrument*>(widget)) {
        const auto *d=Expansion::definition(instrument->code());
        connectModel(instrument,&ExpansionInstrument::parameterValueChanged,instrument,[this,model,idX](const QString &field,const QString &text){model->changeParam(idX,field,text);onWidgetInstanceValueChanged(idX);},parameterEditConnection);
        connectModel(instrument,&PgnWidgetBase::enabledChanged,instrument,[model,idX,instrument]{model->setPgnEnabled(idX,instrument->isEnabled());});
        connectModel(model,&PgnTableModel::parameterChanged,instrument,[instrument,idX](int id,const QString &field,const QVariant &value){if(id!=idX)return;const QSignalBlocker b(instrument);instrument->setValue(field,value.toString());});
        connectModel(instrument->findChild<QPushButton*>("pushButtonRemove"),&QPushButton::clicked,instrument,[this,instrument,group=d->group]{
            if(QMessageBox::question(mdiArea,"Remove Widget","Remove this instrument?",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return;
            pgnTableController->removePgnInstance(instrument->Id());removeWidgetFromSubForm(instrument,group);
        });
        // Optional sources use the same epoch-guarded connections and full
        // rebuild on source creation/removal or Layout/mode changes.
        if(m_mode==Sender && generatorDateTimeWidget && instrument->code()=="128275") {
            auto *source=generatorDateTimeWidget;
            connectWhileSenderAlive(source,&GeneratorDateTime::DaysSince1970Changed,instrument,[source,instrument]{if(source->isEnabled())instrument->setValue("DaysSince1970",QString::number(source->DaysSince1970()));},Qt::QueuedConnection, "setValue, DaysSince1970");
            connectWhileSenderAlive(source,&GeneratorDateTime::SecondsSinceMidnightChanged,instrument,[source,instrument]{if(source->isEnabled())instrument->setValue("SecondsSinceMidnight",QString::number(source->SecondsSinceMidnight(),'g',17));},Qt::QueuedConnection, "setValue, SecondsSinceMidnight");
        }
        if(m_mode==Sender && generatorVesselMovementWidget && (instrument->code()=="APB" || instrument->code()=="RMB")) {
            auto *source=generatorVesselMovementWidget;
            const auto numeric=[&](auto signal,const QString &field,auto getter) {
                connectWhileSenderAlive(source,signal,instrument,[source,instrument,field,getter]{
                    if(source->isEnabled())instrument->setValue(field,QString::number(double((source->*getter)()),'g',17));
                },Qt::QueuedConnection, "setValue");
            };
            const auto arrival=[source,instrument]{if(source->isEnabled())instrument->setValue("ArrivalAlarm",source->ArrivalCircleEntered()?"A":"V");};
            connectWhileSenderAlive(source,&GeneratorVesselMovement::ArrivalCircleEnteredChanged,instrument,arrival,Qt::QueuedConnection, "setValue.ArrivalAlarm");
            numeric(&GeneratorVesselMovement::DestinationWaypointNumberChanged,"DestinationID",&GeneratorVesselMovement::DestinationWaypointNumber);
            if(instrument->code()=="RMB") {
                numeric(&GeneratorVesselMovement::OriginWaypointNumberChanged,"OriginID",&GeneratorVesselMovement::OriginWaypointNumber);
                numeric(&GeneratorVesselMovement::DestinationLatitudeChanged,"Latitude",&GeneratorVesselMovement::DestinationLatitude);
                numeric(&GeneratorVesselMovement::DestinationLongitudeChanged,"Longitude",&GeneratorVesselMovement::DestinationLongitude);
                numeric(&GeneratorVesselMovement::DistanceToWaypointChanged,"DistanceToWaypoint",&GeneratorVesselMovement::DistanceToWaypoint);
                numeric(&GeneratorVesselMovement::WaypointClosingVelocityChanged,"ClosingVelocity",&GeneratorVesselMovement::WaypointClosingVelocity);
            } else {
                connectWhileSenderAlive(source,&GeneratorVesselMovement::PerpendicularCrossedChanged,instrument,[source,instrument]{if(source->isEnabled())instrument->setValue("PerpendicularPassed",source->PerpendicularCrossed()?"A":"V");},Qt::QueuedConnection, "setValue, PerpendicularCrossed");
            }
            const auto angle=[source,instrument](bool origin) {
                if(!source->isEnabled() || source->BearingReference()>1)return;
                const QString reference=source->BearingReference()==0?"T":"M";
                const QString field=instrument->code()=="RMB"?"BearingToWaypoint":origin?"OriginBearing":"PositionBearing";
                const QString desired=instrument->code()=="RMB"?"T":instrument->value(origin?"OriginReference":"PositionReference");
                if(reference!=desired)return;
                instrument->setValue(field,QString::number(origin?source->BearingOriginToDestinationWaypoint():source->BearingPositionToDestinationWaypoint(),'g',17));
            };
            connectWhileSenderAlive(source,&GeneratorVesselMovement::BearingPositionToDestinationWaypointChanged,instrument,[angle]{angle(false);},Qt::QueuedConnection, "setValue.BearingToDestination");
            if(instrument->code()=="APB")connectWhileSenderAlive(source,&GeneratorVesselMovement::BearingOriginToDestinationWaypointChanged,instrument,[angle]{angle(true);},Qt::QueuedConnection, "setValue.BearingOriginToDestination");
            // XTE steering polarity, validity and heading-to-steer are not inferred.
        }
        if(m_mode==Sender && generatorWeatherWidget) {
            auto *source=generatorWeatherWidget;
            if(instrument->code()=="MWV") {
                connectWhileSenderAlive(source,&GeneratorWeather::WindSpeedChanged,instrument,[source,instrument]{
                    if(source->isEnabled() && instrument->value("Reference")=="T")
                        instrument->setValue("WindSpeed",QString::number(source->WindSpeed(),'g',17));
                },Qt::QueuedConnection, "setValue, WindSpeed");
                if(generatorVesselMovementWidget) {
                    auto *movement=generatorVesselMovementWidget;
                    auto angle=[source,movement,instrument]{
                        if(!source->isEnabled() || !movement->isEnabled() || instrument->value("Reference")!="T")return;
                        const double heading=movementSentenceAngle(movement,true,true);
                        if(heading==N2kDoubleNA || source->WindAngle()==N2kDoubleNA)return;
                        const double relative=std::fmod(source->WindAngle()-heading+2*M_PI,2*M_PI);
                        instrument->setValue("WindAngle",QString::number(relative,'g',17));
                    };
                    connectWhileSenderAlive(source,&GeneratorWeather::WindAngleChanged,instrument,angle,Qt::QueuedConnection, "setValue.WindAngle (true-relative reference)");
                    connectWhileSenderAlive(movement,&GeneratorVesselMovement::HeadingChanged,instrument,angle,Qt::QueuedConnection, "setValue.WindAngle (true-relative reference)");
                    connectWhileSenderAlive(movement,&GeneratorVesselMovement::HeadingReferenceChanged,instrument,angle,Qt::QueuedConnection, "setValue.WindAngle (true-relative reference)");
                    connectWhileSenderAlive(movement,&GeneratorVesselMovement::VariationChanged,instrument,angle,Qt::QueuedConnection, "setValue.WindAngle (true-relative reference)");
                    connectWhileSenderAlive(movement,&GeneratorVesselMovement::DeviationChanged,instrument,angle,Qt::QueuedConnection, "setValue.WindAngle (true-relative reference)");
                }
                // Apparent wind is not the true Weather signal. R stays manual.
            }
            auto update=[source,instrument]{
                if(!source->isEnabled())return;
                const auto code=instrument->code();
                auto set=[instrument](const QString &f,double n){instrument->setValue(f,QString::number(n,'g',17));};
                if(code=="MTW")set("WaterTemperature",source->SeaTemperature());
                if(code=="130311") {
                    const auto selected=instrument->value("TemperatureSource").toInt();
                    if(selected==N2kts_SeaTemperature)set("Temperature",source->SeaTemperature());
                    if(selected==N2kts_OutsideTemperature)set("Temperature",source->AirTemperature());
                    if(instrument->value("HumiditySource").toInt()==source->HumiditySource())set("Humidity",source->Humidity());
                    set("AtmosphericPressure",source->AtmosphericPressure());
                }
                if(code=="130314" && instrument->value("PressureSource").toInt()==N2kps_Atmospheric)set("ActualPressure",source->AtmosphericPressure());
            };
            // Each source signal updates only its semantic field; do not copy
            // unrelated edited values when another measurement changes.
            if(instrument->code()=="MTW")connectWhileSenderAlive(source,&GeneratorWeather::SeaTemperatureChanged,instrument,update,Qt::QueuedConnection, "setValue environmental sentence fields (source-selected)");
            if(instrument->code()=="130311" || instrument->code()=="130314") {
                connectWhileSenderAlive(source,&GeneratorWeather::AtmosphericPressureChanged,instrument,[source,instrument]{if(source->isEnabled() && (instrument->code()=="130311" || instrument->value("PressureSource").toInt()==N2kps_Atmospheric))instrument->setValue(instrument->code()=="130311"?"AtmosphericPressure":"ActualPressure",QString::number(source->AtmosphericPressure(),'g',17));},Qt::QueuedConnection, "setValue, AtmosphericPressure");
            }
            if(instrument->code()=="130311") {
                connectWhileSenderAlive(source,&GeneratorWeather::SeaTemperatureChanged,instrument,[source,instrument]{if(source->isEnabled()&&instrument->value("TemperatureSource").toInt()==N2kts_SeaTemperature)instrument->setValue("Temperature",QString::number(source->SeaTemperature(),'g',17));},Qt::QueuedConnection, "setValue, SeaTemperature");
                connectWhileSenderAlive(source,&GeneratorWeather::AirTemperatureChanged,instrument,[source,instrument]{if(source->isEnabled()&&instrument->value("TemperatureSource").toInt()==N2kts_OutsideTemperature)instrument->setValue("Temperature",QString::number(source->AirTemperature(),'g',17));},Qt::QueuedConnection, "setValue, AirTemperature");
                connectWhileSenderAlive(source,&GeneratorWeather::HumidityChanged,instrument,[source,instrument]{if(source->isEnabled()&&instrument->value("HumiditySource").toInt()==source->HumiditySource())instrument->setValue("Humidity",QString::number(source->Humidity(),'g',17));},Qt::QueuedConnection, "HumiditySource, setValue, Humidity");
            }
        }
        if(m_mode==Sender && generatorVesselMovementWidget && (instrument->code()=="VHW" || instrument->code()=="65359")) {
            auto *source=generatorVesselMovementWidget;
            auto update=[source,instrument]{if(!source->isEnabled())return;instrument->setValue("TrueHeading",QString::number(movementSentenceAngle(source,true,true),'g',17));instrument->setValue("MagneticHeading",QString::number(movementSentenceAngle(source,true,false),'g',17));};
            connectWhileSenderAlive(source,&GeneratorVesselMovement::HeadingChanged,instrument,update,Qt::QueuedConnection, "setValue.Heading (reference conversion)");
            connectWhileSenderAlive(source,&GeneratorVesselMovement::VariationChanged,instrument,update,Qt::QueuedConnection, "setValue.Heading (reference conversion)");
            connectWhileSenderAlive(source,&GeneratorVesselMovement::DeviationChanged,instrument,update,Qt::QueuedConnection, "setValue.Heading (reference conversion)");
            connectWhileSenderAlive(source,&GeneratorVesselMovement::HeadingReferenceChanged,instrument,update,Qt::QueuedConnection, "setValue.Heading (reference conversion)");
            // SOG is not water-referenced speed. WaterSpeed remains manual.
        }
    } else if (WindlassStatus::supported(Pgn)) {
        auto *instrument=qobject_cast<WindlassStatus*>(widget);
        for(const auto &row:WindlassStatus::parameters(Pgn))model->setParameterDescription(idX,row[0],WindlassStatus::description(row[0]));
        connectModel(instrument,&WindlassStatus::parameterValueChanged,instrument,[model,idX](const QString &field,const QString &value){model->changeParam(idX,field,value);},parameterEditConnection);
        connectModel(model,&PgnTableModel::parameterChanged,instrument,[instrument,idX](int id,const QString &field,const QVariant &value){if(id!=idX)return;const QSignalBlocker block(instrument);instrument->setValue(field,value.toString());});
        auto *association=instrument->findChild<QLabel*>("windlassAssociation");
        auto update=[model,association,idX]{
            const auto *self=model->findPgn(idX);if(!self)return;
            QString identity;for(const auto &row:self->paramDetails)if(row[0]=="WindlassIdentifier")identity=row[1];
            QStringList matches;
            for(const auto &other:model->getPgnList())if(other.idX!=idX && WindlassStatus::supported(other.pgn.toInt()) && other.source==self->source)
                for(const auto &row:other.paramDetails)if(row[0]=="WindlassIdentifier" && row[1].toUInt()==identity.toUInt())matches.append(QString::number(other.idX));
            association->setText(matches.isEmpty()?"No matching status instrument.":"Matching windlass ID (widget IDs "+matches.join(", ")+"). Independent fields; no values copied.");
        };
        connectModel(model,&PgnTableModel::valuesChanged,instrument,[model,update](int id){const auto *data=model->findPgn(id);if(data && WindlassStatus::supported(data->pgn.toInt()))update();});
        connectModel(this,&MainController::layoutObjectsChanged,instrument,update);update();
        connectModel(instrument->findChild<QPushButton*>("pushButtonRemove"),&QPushButton::clicked,instrument,[this,instrument]{
            if(QMessageBox::question(mdiArea,"Remove Widget","Remove this windlass status instrument?",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return;
            pgnTableController->removePgnInstance(instrument->Id());removeWidgetFromSubForm(instrument,"Other");
        });
    } else if (Pgn == 129039) {
        auto *instrument=qobject_cast<AisClassBPosition129039*>(widget);
        for(const auto &row:AisClassBPosition129039::parameters())model->setParameterDescription(idX,row[0],AisClassBPosition129039::fieldDescription(row[0]));
        connectModel(instrument,&AisClassBPosition129039::parameterValueChanged,instrument,[model,idX](const QString &field,const QString &value){model->changeParam(idX,field,value);},parameterEditConnection);
        connectModel(model,&PgnTableModel::parameterChanged,instrument,[instrument,idX](int id,const QString &field,const QVariant &v){if(id!=idX)return;QSignalBlocker block(instrument);instrument->setValue(field,v.toString());});
        connectModel(instrument->findChild<QPushButton*>("pushButtonRemove"),&QPushButton::clicked,instrument,[this,instrument]{
            if(QMessageBox::question(mdiArea,"Remove Widget","Remove this AIS position report?",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return;
            pgnTableController->removePgnInstance(instrument->Id());removeWidgetFromSubForm(instrument,"Communication");
        });
    } else if (Pgn == 129810) {
        auto *instrument=qobject_cast<AisClassBStatic129810 *>(widget);
        for(const auto &row:AisClassBStatic129810::parameters())model->setParameterDescription(idX,row[0],AisClassBStatic129810::fieldDescription(row[0]));
        connectModel(instrument,&AisClassBStatic129810::parameterValueChanged,instrument,
            [model,idX](const QString &field,const QString &value){model->changeParam(idX,field,value);},parameterEditConnection);
        connectModel(model,&PgnTableModel::parameterChanged,instrument,
            [instrument,idX](int id,const QString &field,const QVariant &v){if(id!=idX)return;const QSignalBlocker block(instrument);instrument->setValue(field,v.toString());});
        connectModel(instrument->findChild<QPushButton *>("pushButtonRemove"),&QPushButton::clicked,instrument,[this,instrument]{
            if(QMessageBox::question(mdiArea,"Remove Widget","Remove this static AIS Part B report?",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return;
            pgnTableController->removePgnInstance(instrument->Id());removeWidgetFromSubForm(instrument,"Communication");
        });
    } else if (Pgn == 129809) {
        auto *instrument = qobject_cast<AisClassBStatic129809 *>(widget);
        for (const auto &row : AisClassBStatic129809::parameters())
            model->setParameterDescription(idX, row[0], AisClassBStatic129809::fieldDescription(row[0]));
        connectModel(instrument, &AisClassBStatic129809::parameterValueChanged, instrument,
            [model, idX](const QString &field, const QString &value) { model->changeParam(idX, field, value); }, parameterEditConnection);
        connectModel(model, &PgnTableModel::parameterChanged, instrument,
            [instrument, idX](int changedId, const QString &field, const QVariant &value) {
                if (changedId != idX) return;
                const QSignalBlocker blocker(instrument); instrument->setValue(field, value.toString());
            });
        connectModel(instrument->findChild<QPushButton *>("pushButtonRemove"), &QPushButton::clicked, instrument,
            [this, instrument] {
                if (QMessageBox::question(mdiArea, "Remove Widget", "Remove this static AIS report?",
                    QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
                pgnTableController->removePgnInstance(instrument->Id()); removeWidgetFromSubForm(instrument, "Communication");
            });
    } else if (Pgn == 130306 || Pgn == 130313) {
        auto *instrument = qobject_cast<EnvironmentalInstrument*>(widget);
        connectModel(instrument, &EnvironmentalInstrument::parameterValueChanged, instrument,
            [this, instrument, model, idX](const QString &field, double v) {
                model->changeParam(idX, field, QString::number(v, 'g', 17));
                onWidgetInstanceValueChanged(instrument->Id());
            }, parameterEditConnection);
        connectModel(model, &PgnTableModel::parameterChanged, instrument,
            [instrument, idX](int changedId, const QString &field, const QVariant &v) {
                if (changedId != idX) return;
                const QSignalBlocker blocker(instrument);
                instrument->setValue(field, v.toDouble());
            });
        connectModel(instrument->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked,
            instrument, [this, instrument] {
                if (QMessageBox::question(mdiArea, "Remove Widget", "Are you sure you want to remove this widget?",
                    QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;
                pgnTableController->removePgnInstance(instrument->Id());
                removeWidgetFromSubForm(instrument, "Environment");
            });
        // Reference/source selection is authoritative. No apparent-wind transformation.
        if (generatorWeatherWidget && m_mode == Sender) {
            auto *weather = generatorWeatherWidget;
            auto update = [weather, instrument, Pgn] {
                if (!weather->isEnabled()) return;
                if (Pgn == 130306 && instrument->value("WindReference") == 0) {
                    instrument->setValue("WindSpeed", weather->WindSpeed());
                    instrument->setValue("WindAngle", weather->WindAngle());
                } else if (Pgn == 130313 && instrument->value("HumiditySource") == 1 && weather->HumiditySource() == N2khs_OutsideHumidity) {
                    instrument->setValue("ActualHumidity", weather->Humidity());
                }
            };
            if (Pgn == 130306) {
                connectWhileSenderAlive(weather, &GeneratorWeather::WindSpeedChanged, instrument, update, Qt::QueuedConnection, "setValue.WindSpeed/WindAngle/ActualHumidity (source-selected)");
                connectWhileSenderAlive(weather, &GeneratorWeather::WindAngleChanged, instrument, update, Qt::QueuedConnection, "setValue.WindSpeed/WindAngle/ActualHumidity (source-selected)");
            } else connectWhileSenderAlive(weather, &GeneratorWeather::HumidityChanged, instrument, update, Qt::QueuedConnection, "setValue.WindSpeed/WindAngle/ActualHumidity (source-selected)");
        }
    } else if (Pgn == 130310) {
        // Cast widget to OutsideEnvironmental130310* and call the initialize function
        auto *outsideenvironmentalWidget = qobject_cast<OutsideEnvironmental130310 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            outsideenvironmentalWidget,
            &Attitude127257::enabledChanged,
            outsideenvironmentalWidget,
            [=]() {
            pgnTableController->setPgnEnabled(outsideenvironmentalWidget->Id(), outsideenvironmentalWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Environment";
        connectModel(outsideenvironmentalWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, outsideenvironmentalWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(outsideenvironmentalWidget->Id());
                removeWidgetFromSubForm(outsideenvironmentalWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        connectModel(outsideenvironmentalWidget, &OutsideEnvironmental130310::SIDChanged,
                     outsideenvironmentalWidget, [=] {
            model->changeParam(idX, "SID", QString::number(outsideenvironmentalWidget->SID()));
        }, parameterEditConnection);

        // Add connections here for the OutsideEnvironmental130310 widget -> model
        connectModel(
            outsideenvironmentalWidget,
            &OutsideEnvironmental130310::OutsideAmbientAirTemperatureChanged,
            outsideenvironmentalWidget,
            [=]() {
                model->changeParam(idX, "OutsideAmbientAirTemperature", QString::number(outsideenvironmentalWidget->OutsideAmbientAirTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(outsideenvironmentalWidget->Id());
            },
            parameterEditConnection);

        connectModel(
            outsideenvironmentalWidget,
            &OutsideEnvironmental130310::AtmosphericPressureChanged,
            outsideenvironmentalWidget,
            [=]() {
                model->changeParam(idX, "AtmosphericPressure", QString::number(outsideenvironmentalWidget->AtmosphericPressure(), 'g', 15));
                onWidgetInstanceValueChanged(outsideenvironmentalWidget->Id());
            },
            parameterEditConnection);

        connectModel(
            outsideenvironmentalWidget,
            &OutsideEnvironmental130310::WaterTemperatureChanged,
            outsideenvironmentalWidget,
            [=]() {
                model->changeParam(idX, "WaterTemperature", QString::number(outsideenvironmentalWidget->WaterTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(outsideenvironmentalWidget->Id());
            },
            parameterEditConnection);



        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, outsideenvironmentalWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? outsideenvironmentalWidget : nullptr);
                if (paramName == "SID") {
                    outsideenvironmentalWidget->setSID(value.toUInt());
                } else if (paramName == "OutsideAmbientAirTemperature") {
                    outsideenvironmentalWidget->setOutsideAmbientAirTemperature(value.toDouble());
                } else if (paramName == "AtmosphericPressure") {
                    outsideenvironmentalWidget->setAtmosphericPressure(value.toDouble());
                } else if (paramName == "WaterTemperature") {
                    outsideenvironmentalWidget->setWaterTemperature(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX); 
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorWeatherWidget to outsideenvironmentalWidget
            if (generatorWeatherWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a WeatherGeneratorwidget if it does not exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "Create Weather Generator Widget",
                                              "No Weather Generator widget found. Do you want to create one?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Weather (PGN -130311)");
                } else {
                    qWarning() << "Weather Generator not placed. OutsideEnvironmental widget will not be updated.";
                    return;
                }
            }

            connectWhileSenderAlive(
                        generatorWeatherWidget,
                        &GeneratorWeather::AirTemperatureChanged,
                        outsideenvironmentalWidget,
                        [this, outsideenvironmentalWidget]() {
                if (this->generatorWeatherWidget->isEnabled()) {
                    outsideenvironmentalWidget->setOutsideAmbientAirTemperature(this->generatorWeatherWidget->AirTemperature());
                }
            },
            Qt::QueuedConnection, "setOutsideAmbientAirTemperature, AirTemperature");

            connectWhileSenderAlive(
                        generatorWeatherWidget,
                        &GeneratorWeather::AtmosphericPressureChanged,
                        outsideenvironmentalWidget,
                        [this, outsideenvironmentalWidget]() {
                if (this->generatorWeatherWidget->isEnabled()) {
                    outsideenvironmentalWidget->setAtmosphericPressure(this->generatorWeatherWidget->AtmosphericPressure());
                }
            },
            Qt::QueuedConnection, "setAtmosphericPressure, AtmosphericPressure");

            connectWhileSenderAlive(
                        generatorWeatherWidget,
                        &GeneratorWeather::SeaTemperatureChanged,
                        outsideenvironmentalWidget,
                        [this, outsideenvironmentalWidget]() {
                if (this->generatorWeatherWidget->isEnabled()) {
                    outsideenvironmentalWidget->setWaterTemperature(this->generatorWeatherWidget->SeaTemperature());
                }
            },
            Qt::QueuedConnection, "setWaterTemperature, SeaTemperature");

        }

    } else if (Pgn == 130312) {
        // Cast widget to TemperatureWidget130312* and call the initialize function
        auto *temperatureWidget = qobject_cast<TemperatureWidget130312 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            temperatureWidget,
            &Attitude127257::enabledChanged,
            temperatureWidget,
            [=]() {
            pgnTableController->setPgnEnabled(temperatureWidget->Id(), temperatureWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Environment";
        connectModel(temperatureWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, temperatureWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(temperatureWidget->Id());
                removeWidgetFromSubForm(temperatureWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect TemperatureInstance changes (widget -> model)
        connectModel(
            temperatureWidget,
            &TemperatureWidget130312::TemperatureInstanceChanged,
            temperatureWidget,
            [=]() {
                model->changeParam(idX, "TemperatureInstance", QString::number(temperatureWidget->TemperatureInstance()));
                onWidgetInstanceValueChanged(temperatureWidget->Id());
            },
            parameterEditConnection);

        // Connect SID changes (widget -> model)
        connectModel(
            temperatureWidget,
            &TemperatureWidget130312::SIDChanged,
            temperatureWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(temperatureWidget->SID()));
                onWidgetInstanceValueChanged(temperatureWidget->Id());
            },
            parameterEditConnection);

        // Connect TemperatureSource changes (widget -> model)
        connectModel(
            temperatureWidget,
            &TemperatureWidget130312::TemperatureSourceChanged,
            temperatureWidget,
            [=]() {
                model->changeParam(idX, "TemperatureSource", QString::number(temperatureWidget->TemperatureSource()));
                onWidgetInstanceValueChanged(temperatureWidget->Id());
            },
            parameterEditConnection);

        // Connect ActualTemperature changes (widget -> model)
        connectModel(
            temperatureWidget,
            &TemperatureWidget130312::ActualTemperatureChanged,
            temperatureWidget,
            [=]() {
                model->changeParam(idX, "ActualTemperature", QString::number(temperatureWidget->ActualTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(temperatureWidget->Id());
            },
            parameterEditConnection);

        // Connect SetTemperature changes
        connectModel(
            temperatureWidget,
            &TemperatureWidget130312::SetTemperatureChanged,
            temperatureWidget,
            [=]() {
                model->changeParam(idX, "SetTemperature", QString::number(temperatureWidget->SetTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(temperatureWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, temperatureWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? temperatureWidget : nullptr);
                if (paramName == "TemperatureInstance") {
                    temperatureWidget->setTemperatureInstance(value.toUInt());
                } else if (paramName == "SID") {
                    temperatureWidget->setSID(value.toUInt());
                } else if (paramName == "TemperatureSource") {
                    temperatureWidget->setTemperatureSource(static_cast<tN2kTempSource>(value.toUInt()));
                } else if (paramName == "ActualTemperature") {
                    temperatureWidget->setActualTemperature(value.toDouble());
                } else if (paramName == "SetTemperature") {
                    temperatureWidget->setSetTemperature(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorWeatherWidget to outsideenvironmentalWidget
            if (generatorWeatherWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a WeatherGeneratorwidget if it does not exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "Create Weather Generator Widget",
                                              "No Weather Generator widget found. Do you want to create one?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Weather (PGN -130311)");
                } else {
                    qWarning() << "Weather Generator not placed. OutsideEnvironmental widget will not be updated.";
                    return;
                }
            }

            // Connect generatorWeather SID to temperatureWidget
            if (generatorWeatherWidget != nullptr) {
                connectWhileSenderAlive(
                            generatorWeatherWidget,
                            &GeneratorWeather::SIDChanged,
                            temperatureWidget,
                            [this, temperatureWidget]() { temperatureWidget->setSID(this->generatorWeatherWidget->SID());
                },
                Qt::QueuedConnection, "setSID, SID");

                connectWhileSenderAlive(generatorWeatherWidget, &GeneratorWeather::MarineObservationsChanged,
                    temperatureWidget, [this, temperatureWidget] {
                        if (generatorWeatherWidget->isEnabled() && temperatureWidget->TemperatureSource() == N2kts_DewPointTemperature)
                            temperatureWidget->setActualTemperature(generatorWeatherWidget->DewPoint());
                    }, Qt::QueuedConnection, "TemperatureSource, setActualTemperature, DewPoint");
                connectWhileSenderAlive(
                    generatorWeatherWidget, &GeneratorWeather::AirTemperatureChanged, temperatureWidget,
                    [this, temperatureWidget] {
                        if (generatorWeatherWidget->isEnabled() &&
                            temperatureWidget->TemperatureSource() == N2kts_OutsideTemperature)
                            temperatureWidget->setActualTemperature(generatorWeatherWidget->AirTemperature());
                    },
                    Qt::QueuedConnection, "TemperatureSource, setActualTemperature, AirTemperature");
                connectWhileSenderAlive(
                    generatorWeatherWidget, &GeneratorWeather::SeaTemperatureChanged, temperatureWidget,
                    [this, temperatureWidget] {
                        if (generatorWeatherWidget->isEnabled() &&
                            temperatureWidget->TemperatureSource() == N2kts_SeaTemperature)
                            temperatureWidget->setActualTemperature(generatorWeatherWidget->SeaTemperature());
                    },
                    Qt::QueuedConnection, "TemperatureSource, setActualTemperature, SeaTemperature");
            }
        }

    } else if (Pgn == 130316) {
        // Cast widget to TemperatureExtended130316* and call the initialize function
        auto *temperatureExtendedWidget = qobject_cast<TemperatureExtended130316 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            temperatureExtendedWidget,
            &Attitude127257::enabledChanged,
            temperatureExtendedWidget,
            [=]() {
            pgnTableController->setPgnEnabled(temperatureExtendedWidget->Id(), temperatureExtendedWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(temperatureExtendedWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, temperatureExtendedWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(temperatureExtendedWidget->Id());
                removeWidgetFromSubForm(temperatureExtendedWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            temperatureExtendedWidget,
            &TemperatureExtended130316::SIDChanged,
            temperatureExtendedWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(temperatureExtendedWidget->SID()));
                onWidgetInstanceValueChanged(temperatureExtendedWidget->Id());
            },
            parameterEditConnection);

        // Connect TempInstance changes (widget -> model)
        connectModel(
            temperatureExtendedWidget,
            &TemperatureExtended130316::TempInstanceChanged,
            temperatureExtendedWidget,
            [=]() {
                model->changeParam(idX, "TempInstance", QString::number(temperatureExtendedWidget->TempInstance()));
                onWidgetInstanceValueChanged(temperatureExtendedWidget->Id());
            },
            parameterEditConnection);

        // Connect TempSource changes (widget -> model)
        connectModel(
            temperatureExtendedWidget,
            &TemperatureExtended130316::TempSourceChanged,
            temperatureExtendedWidget,
            [=]() {
                model->changeParam(idX, "TempSource", QString::number(temperatureExtendedWidget->TempSource()));
                onWidgetInstanceValueChanged(temperatureExtendedWidget->Id());
            },
            parameterEditConnection);

        // Connect ActualTemperature changes (widget -> model)
        connectModel(
            temperatureExtendedWidget,
            &TemperatureExtended130316::ActualTemperatureChanged,
            temperatureExtendedWidget,
            [=]() {
                model->changeParam(idX, "ActualTemperature", QString::number(temperatureExtendedWidget->ActualTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(temperatureExtendedWidget->Id());
            },
            parameterEditConnection);

        // Connect SetTemperature changes (widget -> model)
        connectModel(
            temperatureExtendedWidget,
            &TemperatureExtended130316::SetTemperatureChanged,
            temperatureExtendedWidget,
            [=]() {
                model->changeParam(idX, "SetTemperature", QString::number(temperatureExtendedWidget->SetTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(temperatureExtendedWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, temperatureExtendedWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? temperatureExtendedWidget : nullptr);
                if (paramName == "SID") {
                    temperatureExtendedWidget->setSID(value.toUInt());
                } else if (paramName == "TempInstance") {
                    temperatureExtendedWidget->setTempInstance(value.toUInt());
                } else if (paramName == "TempSource") {
                    temperatureExtendedWidget->setTempSource(static_cast<tN2kTempSource>(value.toUInt()));
                } else if (paramName == "ActualTemperature") {
                    temperatureExtendedWidget->setActualTemperature(value.toDouble());
                } else if (paramName == "SetTemperature") {
                    temperatureExtendedWidget->setSetTemperature(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            if (generatorWeatherWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a WeatherGeneratorwidget if it does not exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "Create Weather Generator Widget",
                                              "No Weather Generator widget found. Do you want to create one?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Weather (PGN -130311)");
                } else {
                    qWarning() << "Weather Generator not placed. OutsideEnvironmental widget will not be updated.";
                    return;
                }
            }

            // Connect generatorDateTimeWidget to temperatureExtendedWidget
            if (generatorWeatherWidget != nullptr) {
                connectWhileSenderAlive(
                    generatorWeatherWidget,
                    &GeneratorWeather::SIDChanged,
                    temperatureExtendedWidget,
                    [this, temperatureExtendedWidget]() { temperatureExtendedWidget->setSID(this->generatorWeatherWidget->SID()); },
                    Qt::QueuedConnection, "setSID, SID");
            }

            if(generatorWeatherWidget){
                connectWhileSenderAlive(generatorWeatherWidget, &GeneratorWeather::MarineObservationsChanged,
                    temperatureExtendedWidget, [this, temperatureExtendedWidget] {
                        if (generatorWeatherWidget->isEnabled() && temperatureExtendedWidget->TempSource() == N2kts_DewPointTemperature)
                            temperatureExtendedWidget->setActualTemperature(generatorWeatherWidget->DewPoint());
                    }, Qt::QueuedConnection, "TempSource, setActualTemperature, DewPoint");
                connectWhileSenderAlive(
                    generatorWeatherWidget, &GeneratorWeather::AirTemperatureChanged,
                    temperatureExtendedWidget,
                    [this, temperatureExtendedWidget] {
                        if (generatorWeatherWidget->isEnabled() &&
                            temperatureExtendedWidget->TempSource() == N2kts_OutsideTemperature)
                            temperatureExtendedWidget->setActualTemperature(
                                generatorWeatherWidget->AirTemperature());
                    },
                    Qt::QueuedConnection, "TempSource, setActualTemperature, AirTemperature");
                connectWhileSenderAlive(
                    generatorWeatherWidget, &GeneratorWeather::SeaTemperatureChanged,
                    temperatureExtendedWidget,
                    [this, temperatureExtendedWidget] {
                        if (generatorWeatherWidget->isEnabled() &&
                            temperatureExtendedWidget->TempSource() == N2kts_SeaTemperature)
                            temperatureExtendedWidget->setActualTemperature(
                                generatorWeatherWidget->SeaTemperature());
                    },
                    Qt::QueuedConnection, "TempSource, setActualTemperature, SeaTemperature");
            }
        }

    } else if (Pgn == 127488) {
        // Cast widget to EngineRapid127488* and call the initialize function
        auto *engineRapidWidget = qobject_cast<EngineRapid127488 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            engineRapidWidget,
            &Attitude127257::enabledChanged,
            engineRapidWidget,
            [=]() {
            pgnTableController->setPgnEnabled(engineRapidWidget->Id(), engineRapidWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Propulsion";
        connectModel(engineRapidWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, engineRapidWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(engineRapidWidget->Id());
                removeWidgetFromSubForm(engineRapidWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        connectModel(
            engineRapidWidget,
            &EngineRapid127488::EngineInstanceChanged,
            engineRapidWidget,
            [=]() {
                model->changeParam(idX, "EngineInstance", QString::number(engineRapidWidget->EngineInstance()));
                onWidgetInstanceValueChanged(engineRapidWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineSpeed changes (widget -> model)
        connectModel(
            engineRapidWidget,
            &EngineRapid127488::EngineSpeedChanged,
            engineRapidWidget,
            [=]() {
                model->changeParam(idX, "EngineSpeed", QString::number(engineRapidWidget->EngineSpeed(), 'g', 15));
                onWidgetInstanceValueChanged(engineRapidWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineBoostPressure changes (widget -> model)
        connectModel(
            engineRapidWidget,
            &EngineRapid127488::EngineBoostPressureChanged,
            engineRapidWidget,
            [=]() {
                model->changeParam(idX, "EngineBoostPressure", QString::number(engineRapidWidget->EngineBoostPressure(), 'g', 15));
                onWidgetInstanceValueChanged(engineRapidWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineTiltTrim changes (widget -> model)
        connectModel(
            engineRapidWidget,
            &EngineRapid127488::EngineTiltTrimChanged,
            engineRapidWidget,
            [=]() {
                model->changeParam(idX, "EngineTiltTrim", QString::number(engineRapidWidget->EngineTiltTrim()));
                onWidgetInstanceValueChanged(engineRapidWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, engineRapidWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? engineRapidWidget : nullptr);
                if (paramName == "EngineInstance") {
                    engineRapidWidget->setEngineInstance(value.toUInt());
                } else if (paramName == "EngineSpeed") {
                    engineRapidWidget->setEngineSpeed(value.toDouble());
                } else if (paramName == "EngineBoostPressure") {
                    engineRapidWidget->setEngineBoostPressure(value.toDouble());
                } else if (paramName == "EngineTiltTrim") {
                    engineRapidWidget->setEngineTiltTrim(value.toInt());
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model

    } else if (Pgn == 127489) {
        // Cast widget to EngineDynamic127489* and call the initialize function
        auto *engineDynamicWidget = qobject_cast<EngineDynamic127489 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            engineDynamicWidget,
            &Attitude127257::enabledChanged,
            engineDynamicWidget,
            [=]() {
            pgnTableController->setPgnEnabled(engineDynamicWidget->Id(), engineDynamicWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Propulsion";
        connectModel(engineDynamicWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, engineDynamicWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(engineDynamicWidget->Id());
                removeWidgetFromSubForm(engineDynamicWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect EngineInstance changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineInstanceChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineInstance", QString::number(engineDynamicWidget->EngineInstance()));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineOilPress changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineOilPressChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineOilPress", QString::number(engineDynamicWidget->EngineOilPress(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineOilTemp changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineOilTempChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineOilTemp", QString::number(engineDynamicWidget->EngineOilTemp(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineCoolantTemp changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineCoolantTempChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineCoolantTemp", QString::number(engineDynamicWidget->EngineCoolantTemp(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect AltenatorVoltage changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::AltenatorVoltageChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "AltenatorVoltage", QString::number(engineDynamicWidget->AltenatorVoltage(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect FuelRate changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::FuelRateChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "FuelRate", QString::number(engineDynamicWidget->FuelRate(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineHours changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineHoursChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineHours", QString::number(engineDynamicWidget->EngineHours(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineCoolantPress changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineCoolantPressChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineCoolantPress", QString::number(engineDynamicWidget->EngineCoolantPress(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineFuelPress changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineFuelPressChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineFuelPress", QString::number(engineDynamicWidget->EngineFuelPress(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineLoad changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineLoadChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineLoad", QString::number(engineDynamicWidget->EngineLoad(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineTorque changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineTorqueChanged,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineTorque", QString::number(engineDynamicWidget->EngineTorque(), 'g', 15));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect tN2kEngineDiscreteStatus1 changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineDiscreteStatus1Changed,
            engineDynamicWidget,
            [=]() {
                //model->changeParam(idX, "Status1", QString::number(engineDynamicWidget->EngineDiscreteStatus1().Status));
                model->changeParam(idX, "Status1", QString::number(engineDynamicWidget->EngineDiscreteStatus1().Status));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect tN2kEngineDiscreteStatus2 changes (widget -> model)
        connectModel(
            engineDynamicWidget,
            &EngineDynamic127489::EngineDiscreteStatus2Changed,
            engineDynamicWidget,
            [=]() {
                model->changeParam(idX, "Status2", QString::number(engineDynamicWidget->EngineDiscreteStatus2().Status));
                onWidgetInstanceValueChanged(engineDynamicWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, engineDynamicWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? engineDynamicWidget : nullptr);
                if (paramName == "EngineInstance") {
                    engineDynamicWidget->setEngineInstance(value.toUInt());
                } else if (paramName == "EngineOilPress") {
                    engineDynamicWidget->setEngineOilPress(value.toDouble());
                } else if (paramName == "EngineOilTemp") {
                    engineDynamicWidget->setEngineOilTemp(value.toDouble());
                } else if (paramName == "EngineCoolantTemp") {
                    engineDynamicWidget->setEngineCoolantTemp(value.toDouble());
                } else if (paramName == "AltenatorVoltage") {
                    engineDynamicWidget->setAltenatorVoltage(value.toDouble());
                } else if (paramName == "FuelRate") {
                    engineDynamicWidget->setFuelRate(value.toDouble());
                } else if (paramName == "EngineHours") {
                    engineDynamicWidget->setEngineHours(value.toDouble());
                } else if (paramName == "EngineCoolantPress") {
                    engineDynamicWidget->setEngineCoolantPress(value.toDouble());
                } else if (paramName == "EngineFuelPress") {
                    engineDynamicWidget->setEngineFuelPress(value.toDouble());
                } else if (paramName == "EngineLoad") {
                    engineDynamicWidget->setEngineLoad(value.toDouble());
                } else if (paramName == "EngineTorque") {
                    engineDynamicWidget->setEngineTorque(value.toDouble());
                } else if (paramName == "Status1") {
                    tN2kEngineDiscreteStatus1 status1;
                    status1.Status = value.toUInt();
                    engineDynamicWidget->setEngineDiscreteStatus1(status1);
                } else if (paramName == "Status2") {
                    tN2kEngineDiscreteStatus2 status2;
                    status2.Status = value.toUInt();
                    engineDynamicWidget->setEngineDiscreteStatus2(status2);
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model
        tN2kEngineDiscreteStatus1 status1;
        engineDynamicWidget->setEngineDiscreteStatus1(status1);
        tN2kEngineDiscreteStatus2 status2;
        engineDynamicWidget->setEngineDiscreteStatus2(status2);

    } else if (Pgn == 127493) {
        // Cast widget to TransmissionDynamic127493* and call the initialize function
        auto *transmissionDynamicWidget = qobject_cast<TransmissionDynamic127493 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            transmissionDynamicWidget,
            &Attitude127257::enabledChanged,
            transmissionDynamicWidget,
            [=]() {
            pgnTableController->setPgnEnabled(transmissionDynamicWidget->Id(), transmissionDynamicWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Propulsion";
        connectModel(transmissionDynamicWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, transmissionDynamicWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(transmissionDynamicWidget->Id());
                removeWidgetFromSubForm(transmissionDynamicWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect EngineInstance changes (widget -> model)
        connectModel(
            transmissionDynamicWidget,
            &TransmissionDynamic127493::EngineInstanceChanged,
            transmissionDynamicWidget,
            [=]() {
                model->changeParam(idX, "EngineInstance", QString::number(transmissionDynamicWidget->EngineInstance()));
                onWidgetInstanceValueChanged(transmissionDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect TransmissionGear changes (widget -> model)
        connectModel(
            transmissionDynamicWidget,
            &TransmissionDynamic127493::TransmissionGearChanged,
            transmissionDynamicWidget,
            [=]() {
                model->changeParam(idX, "TransmissionGear", QString::number(transmissionDynamicWidget->TransmissionGear()));
                onWidgetInstanceValueChanged(transmissionDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect OilPressure changes (widget -> model)
        connectModel(
            transmissionDynamicWidget,
            &TransmissionDynamic127493::OilPressureChanged,
            transmissionDynamicWidget,
            [=]() {
                model->changeParam(idX, "OilPressure", QString::number(transmissionDynamicWidget->OilPressure(), 'g', 15));
                onWidgetInstanceValueChanged(transmissionDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect OilTemperature changes (widget -> model)
        connectModel(
            transmissionDynamicWidget,
            &TransmissionDynamic127493::OilTemperatureChanged,
            transmissionDynamicWidget,
            [=]() {
                model->changeParam(idX, "OilTemperature", QString::number(transmissionDynamicWidget->OilTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(transmissionDynamicWidget->Id());
            },
            parameterEditConnection);

        // Connect DiscreteStatus1 changes (widget -> model)
        connectModel(
            transmissionDynamicWidget,
            &TransmissionDynamic127493::DiscreteStatus1Changed,
            transmissionDynamicWidget,
            [=]() {
                model->changeParam(idX, "DiscreteStatus1", QString::number(transmissionDynamicWidget->DiscreteStatus1()));
                onWidgetInstanceValueChanged(transmissionDynamicWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, transmissionDynamicWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? transmissionDynamicWidget : nullptr);
                if (paramName == "EngineInstance") {
                    transmissionDynamicWidget->setEngineInstance(value.toUInt());
                } else if (paramName == "TransmissionGear") {
                    transmissionDynamicWidget->setTransmissionGear(static_cast<tN2kTransmissionGear>(value.toUInt()));
                } else if (paramName == "OilPressure") {
                    transmissionDynamicWidget->setOilPressure(value.toDouble());
                } else if (paramName == "OilTemperature") {
                    transmissionDynamicWidget->setOilTemperature(value.toDouble());
                } else if (paramName == "DiscreteStatus1") {
                    transmissionDynamicWidget->setDiscreteStatus1(value.toUInt());
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Named initialization follows in initializeWidgetFromModel.

        if(m_mode == Sender) {
            // Connect generatorVesselMovementWidget to transmissionDynamicWidget
            if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorVesselMovement widget if it doesn't exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorVesselMovement Widget",
                                              "Would you like to create a GeneratorVesselMovement widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Vessel Movement (PGN -129025)");
                } else {
                    qWarning() << "GeneratorVesselMovement widget is null";
                    return;
                }
            }

            // Gear follows the existing per-instance GeneratorTransLowerUnit
            // binding. An attitude update must not copy port gear to starboard.

        }

    } else if (Pgn == 129283) {
        // Cast widget to CrossTrack129283* and call the initialize function
        auto *crossTrackWidget = qobject_cast<CrossTrack129283 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            crossTrackWidget,
            &Attitude127257::enabledChanged,
            crossTrackWidget,
            [=]() {
            pgnTableController->setPgnEnabled(crossTrackWidget->Id(), crossTrackWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(crossTrackWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, crossTrackWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(crossTrackWidget->Id());
                removeWidgetFromSubForm(crossTrackWidget, groupName);
                // Qt handles the deletion of the widget, so no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            crossTrackWidget,
            &CrossTrack129283::SIDChanged,
            crossTrackWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(crossTrackWidget->SID()));
                onWidgetInstanceValueChanged(crossTrackWidget->Id());
            },
            parameterEditConnection);

        // Connect XTEMode changes (widget -> model)
        connectModel(
            crossTrackWidget,
            &CrossTrack129283::XTEModeChanged,
            crossTrackWidget,
            [=]() {
                model->changeParam(idX, "XTEMode", QString::number(crossTrackWidget->XTEMode()));
                onWidgetInstanceValueChanged(crossTrackWidget->Id());
            },
            parameterEditConnection);

        // Connect NavigationTerminated changes (widget -> model)
        connectModel(
            crossTrackWidget,
            &CrossTrack129283::NavigationTerminatedChanged,
            crossTrackWidget,
            [=]() {
                model->changeParam(idX, "NavigationTerminated", QString::number(crossTrackWidget->NavigationTerminated()));
                onWidgetInstanceValueChanged(crossTrackWidget->Id());
            },
            parameterEditConnection);

        // Connect XTE changes (widget -> model)
        connectModel(
            crossTrackWidget,
            &CrossTrack129283::XTEChanged,
            crossTrackWidget,
            [=]() {
                model->changeParam(idX, "XTE", QString::number(crossTrackWidget->XTE(), 'g', 15));
                onWidgetInstanceValueChanged(crossTrackWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, crossTrackWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? crossTrackWidget : nullptr);
                if (paramName == "SID") {
                    crossTrackWidget->setSID(value.toUInt());
                } else if (paramName == "XTEMode") {
                    crossTrackWidget->setXTEMode(static_cast<tN2kXTEMode>(value.toUInt()));
                } else if (paramName == "NavigationTerminated") {
                    crossTrackWidget->setNavigationTerminated(value.toUInt());
                } else if (paramName == "XTE") {
                    crossTrackWidget->setXTE(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model
        if (generatorVesselMovementWidget) {
            connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::XTEChanged,
                crossTrackWidget, [this, crossTrackWidget] {
                    if (generatorVesselMovementWidget->isEnabled()) {
                        crossTrackWidget->setXTE(generatorVesselMovementWidget->XTE());
                        // A first sample also supplies an unchanged runtime state.
                        crossTrackWidget->setNavigationTerminated(generatorVesselMovementWidget->NavigationTerminated());
                    }
                }, Qt::QueuedConnection, "setXTE, XTE");
            connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::NavigationTerminatedChanged,
                crossTrackWidget, [this, crossTrackWidget] {
                    crossTrackWidget->setNavigationTerminated(generatorVesselMovementWidget->NavigationTerminated());
                }, Qt::QueuedConnection, "setNavigationTerminated, NavigationTerminated");
            connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::SIDChanged,
                crossTrackWidget, [this, crossTrackWidget] { crossTrackWidget->setSID(generatorVesselMovementWidget->SID()); }, Qt::QueuedConnection, "setSID, SID");
            // Binding/restoration is not a measurement. Keep the saved model and
            // presentation until guarded source delivery supplies a real sample
            // or an explicit navigation-state change.
        }
    } else if (Pgn == 129284) {
        // Cast widget to NavigationData129284* and call the initialize function
        auto *navigationDataWidget = qobject_cast<Navigation129284 *>(widget);

        // Connect the EnabledChanged signal to table controller setPgnEnabled
        connectModel(
            navigationDataWidget,
            &Attitude127257::enabledChanged,
            navigationDataWidget,
            [=]() {
            pgnTableController->setPgnEnabled(navigationDataWidget->Id(), navigationDataWidget->isEnabled());
            },
            Qt::QueuedConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(navigationDataWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, navigationDataWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(navigationDataWidget->Id());
                removeWidgetFromSubForm(navigationDataWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

        // Connect SID changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::SIDChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(navigationDataWidget->SID()));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect DistanceToWaypoint changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::DistanceToWaypointChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "DistanceToWaypoint", QString::number(navigationDataWidget->DistanceToWaypoint(), 'g', 15));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect BearingReference changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::BearingReferenceChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "BearingReference", QString::number(navigationDataWidget->BearingReference()));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect PerpendicularCrossed changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::PerpendicularCrossedChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "PerpendicularCrossed", QString::number(navigationDataWidget->PerpendicularCrossed()));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect ArrivalCircleEntered changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::ArrivalCircleEnteredChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "ArrivalCircleEntered", QString::number(navigationDataWidget->ArrivalCircleEntered()));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect CalculationType changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::CalculationTypeChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "CalculationType", QString::number(navigationDataWidget->CalculationType()));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect ETATime changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::ETATimeChanged,
            navigationDataWidget,
            [=]() {
                // PGN129284 carries UTC seconds at 0.0001-second resolution.
                model->changeParam(idX, "ETATime", QString::number(navigationDataWidget->ETATime(), 'g', 17));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect ETADate changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::ETADateChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "ETADate", QString::number(navigationDataWidget->ETADate()));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect BearingOriginToDestinationWaypoint changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::BearingOriginToDestinationWaypointChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "BearingOriginToDestinationWaypoint", QString::number(navigationDataWidget->BearingOriginToDestinationWaypoint(), 'g', 15));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect BearingPositionToDestinationWaypoint changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::BearingPositionToDestinationWaypointChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "BearingPositionToDestinationWaypoint", QString::number(navigationDataWidget->BearingPositionToDestinationWaypoint(), 'g', 15));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect OriginWaypointNumber changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::OriginWaypointNumberChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "OriginWaypointNumber", QString::number(navigationDataWidget->OriginWaypointNumber()));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect DestinationWaypointNumber changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::DestinationWaypointNumberChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "DestinationWaypointNumber", QString::number(navigationDataWidget->DestinationWaypointNumber()));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect DestinationLatitude changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::DestinationLatitudeChanged,
            navigationDataWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "DestinationLatitude", navigationDataWidget->DestinationLatitude());
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect DestinationLongitude changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::DestinationLongitudeChanged,
            navigationDataWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "DestinationLongitude", navigationDataWidget->DestinationLongitude());
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);

        // Connect WaypointClosingVelocity changes (widget -> model)
        connectModel(
            navigationDataWidget,
            &Navigation129284::WaypointClosingVelocityChanged,
            navigationDataWidget,
            [=]() {
                model->changeParam(idX, "WaypointClosingVelocity", QString::number(navigationDataWidget->WaypointClosingVelocity(), 'g', 15));
                onWidgetInstanceValueChanged(navigationDataWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, navigationDataWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? navigationDataWidget : nullptr);
                if (paramName == "SID") {
                    navigationDataWidget->setSID(value.toUInt());
                } else if (paramName == "DistanceToWaypoint") {
                    navigationDataWidget->setDistanceToWaypoint(value.toDouble());
                } else if (paramName == "BearingReference") {
                    navigationDataWidget->setBearingReference(static_cast<tN2kHeadingReference>(value.toUInt()));
                } else if (paramName == "PerpendicularCrossed") {
                    navigationDataWidget->setPerpendicularCrossed(value.toBool());
                } else if (paramName == "ArrivalCircleEntered") {
                    navigationDataWidget->setArrivalCircleEntered(value.toBool());
                } else if (paramName == "CalculationType") {
                    navigationDataWidget->setCalculationType(static_cast<tN2kDistanceCalculationType>(value.toUInt()));
                } else if (paramName == "ETATime") {
                    navigationDataWidget->setETATime(value.toDouble());
                } else if (paramName == "ETADate") {
                    navigationDataWidget->setETADate(value.toInt());
                } else if (paramName == "BearingOriginToDestinationWaypoint") {
                    navigationDataWidget->setBearingOriginToDestinationWaypoint(value.toDouble());
                } else if (paramName == "BearingPositionToDestinationWaypoint") {
                    navigationDataWidget->setBearingPositionToDestinationWaypoint(value.toDouble());
                } else if (paramName == "OriginWaypointNumber") {
                    navigationDataWidget->setOriginWaypointNumber(value.toUInt());
                } else if (paramName == "DestinationWaypointNumber") {
                    navigationDataWidget->setDestinationWaypointNumber(value.toUInt());
                } else if (paramName == "DestinationLatitude") {
                    navigationDataWidget->setDestinationLatitude(value.toDouble());
                } else if (paramName == "DestinationLongitude") {
                    navigationDataWidget->setDestinationLongitude(value.toDouble());
                } else if (paramName == "WaypointClosingVelocity") {
                    navigationDataWidget->setWaypointClosingVelocity(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovementWidget to navigationDataWidget
            if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorVesselMovement widget if it does not exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorVesselMovement Widget",
                                              "Would you like to create a GeneratorVesselMovement widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Vessel Movement (PGN -129025)");
                } else {
                    qWarning() << "GeneratorVesselMovement widget is null";
                    return;
                }
            }

            // Connect DistanceToWaypoint changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::DistanceToWaypointChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    navigationDataWidget->setDistanceToWaypoint(this->generatorVesselMovementWidget->DistanceToWaypoint());
                }
            },
                Qt::QueuedConnection, "setDistanceToWaypoint, DistanceToWaypoint");

            // Connect BearingReference changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::BearingReferenceChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setBearingReference(this->generatorVesselMovementWidget->BearingReference());
                }
            },
                Qt::QueuedConnection, "setBearingReference, BearingReference");

            // Connect PerpendicularCrossed changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::PerpendicularCrossedChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setPerpendicularCrossed(this->generatorVesselMovementWidget->PerpendicularCrossed());
                }
            },
                Qt::QueuedConnection, "setPerpendicularCrossed, PerpendicularCrossed");

            // Connect ArrivalCircleEntered changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::ArrivalCircleEnteredChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setArrivalCircleEntered(this->generatorVesselMovementWidget->ArrivalCircleEntered());
                }
            },
                Qt::QueuedConnection, "setArrivalCircleEntered, ArrivalCircleEntered");

            // Connect CalculationType changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::CalculationTypeChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setCalculationType(this->generatorVesselMovementWidget->CalculationType());
                }
            },
                Qt::QueuedConnection, "setCalculationType, CalculationType");

            // Connect ETATime changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::ETATimeChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setETATime(this->generatorVesselMovementWidget->ETATime());
                }
            },
                Qt::QueuedConnection, "setETATime, ETATime");

            // Connect ETADate changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::ETADateChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setETADate(this->generatorVesselMovementWidget->ETADate());
                }
            },
                Qt::QueuedConnection, "setETADate, ETADate");

            // Connect BearingOriginToDestinationWaypoint changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::BearingOriginToDestinationWaypointChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setBearingOriginToDestinationWaypoint(this->generatorVesselMovementWidget->BearingOriginToDestinationWaypoint());
                }
            },
                Qt::QueuedConnection, "setBearingOriginToDestinationWaypoint, BearingOriginToDestinationWaypoint");

            // Connect BearingPositionToDestinationWaypoint changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::BearingPositionToDestinationWaypointChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setBearingPositionToDestinationWaypoint(this->generatorVesselMovementWidget->BearingPositionToDestinationWaypoint());
                }
            },
                Qt::QueuedConnection, "setBearingPositionToDestinationWaypoint, BearingPositionToDestinationWaypoint");

            // Connect OriginWaypointNumber changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::OriginWaypointNumberChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setOriginWaypointNumber(this->generatorVesselMovementWidget->OriginWaypointNumber());
                }
            },
                Qt::QueuedConnection, "setOriginWaypointNumber, OriginWaypointNumber");

            // Connect DestinationWaypointNumber changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::DestinationWaypointNumberChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setDestinationWaypointNumber(this->generatorVesselMovementWidget->DestinationWaypointNumber());
                }
            },
                Qt::QueuedConnection, "setDestinationWaypointNumber, DestinationWaypointNumber");

            // Connect DestinationLatitude changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::DestinationLatitudeChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setDestinationLatitude(this->generatorVesselMovementWidget->DestinationLatitude());
                }
            },
                Qt::QueuedConnection, "setDestinationLatitude, DestinationLatitude");

            // Connect DestinationLongitude changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::DestinationLongitudeChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setDestinationLongitude(this->generatorVesselMovementWidget->DestinationLongitude());
                }
            },
                Qt::QueuedConnection, "setDestinationLongitude, DestinationLongitude");

            // Connect WaypointClosingVelocity changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &::GeneratorVesselMovement::WaypointClosingVelocityChanged,
                navigationDataWidget,
                [this, navigationDataWidget] {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                navigationDataWidget->setWaypointClosingVelocity(this->generatorVesselMovementWidget->WaypointClosingVelocity());
                }
            },
                Qt::QueuedConnection, "setWaypointClosingVelocity, WaypointClosingVelocity");
        }

    } else if (Pgn == 0) {
        // Cast widget to Spacer000000* and call the initialize function
        auto *spacerWidget = qobject_cast<Spacer000000 *>(widget);

        // Connect the showTitleRow (widget -> model)
        connectModel(
            spacerWidget,
            &Spacer000000::showTitleRowChanged,
            spacerWidget,
            [=]() {
                model->changeParam(idX, "ShowTitleRow", QString::number(spacerWidget->showTitleRow()));
            },
            parameterEditConnection);

        // Connect the showTitleRow (model -> widget)
        connectModel(
            model,
            &PgnTableModel::parameterChanged,
            spacerWidget,
            [=](int changedIdX, const QString &paramName, const QVariant &value) {
                if (changedIdX == idX) {
                    if (paramName == "ShowTitleRow") {
                        spacerWidget->setShowTitleRow(value.toBool());
                    }
                }
            });

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName;
        for (auto it = groupWidgetsMap.cbegin(); it != groupWidgetsMap.cend(); ++it)
            if (it.value().contains(spacerWidget)) { groupName = it.key(); break; }
        connectModel(spacerWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, spacerWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(spacerWidget->Id());
                removeWidgetFromSubForm(spacerWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });

    }
    else if (Pgn == -129033) {
        // Cast widget to GeneratorDateTime129033* and call the initialize function
        auto *generatorDateTimeWidget = qobject_cast<GeneratorDateTime *>(widget);

        // Connect SID changes (widget -> model)
        connectModel(
            generatorDateTimeWidget,
            &GeneratorDateTime::SIDChanged,
            generatorDateTimeWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(generatorDateTimeWidget->SID()));
            },
            parameterEditConnection);

        // Connect DaysSince1970 changes (widget -> model)
        connectModel(
            generatorDateTimeWidget,
            &GeneratorDateTime::DaysSince1970Changed,
            generatorDateTimeWidget,
            [=]() {
                model->changeParam(idX, "DaysSince1970", QString::number(generatorDateTimeWidget->DaysSince1970()));
                onWidgetInstanceValueChanged(generatorDateTimeWidget->Id());
            },
            parameterEditConnection);

        // Connect SecondsSinceMidnight changes (widget -> model)
        connectModel(
            generatorDateTimeWidget,
            &GeneratorDateTime::SecondsSinceMidnightChanged,
            generatorDateTimeWidget,
            [=]() {
                model->changeParam(idX, "SecondsSinceMidnight", QString::number(generatorDateTimeWidget->SecondsSinceMidnight(), 'g', 15));
                onWidgetInstanceValueChanged(generatorDateTimeWidget->Id());
            },
            parameterEditConnection);

        // Connect LocalOffset changes (widget -> model)
        connectModel(
            generatorDateTimeWidget,
            &GeneratorDateTime::LocalOffsetChanged,
            generatorDateTimeWidget,
            [=]() {
                model->changeParam(idX, "LocalOffset", QString::number(generatorDateTimeWidget->LocalOffset()));
                onWidgetInstanceValueChanged(generatorDateTimeWidget->Id());
            },
            parameterEditConnection);

        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, generatorDateTimeWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? generatorDateTimeWidget : nullptr);
                if (paramName == "DaysSince1970") {
                    generatorDateTimeWidget->setDaysSince1970(value.toUInt());
                } else if (paramName == "SecondsSinceMidnight") {
                    generatorDateTimeWidget->setSecondsSinceMidnight(value.toDouble());
                } else if (paramName == "LocalOffset") {
                    generatorDateTimeWidget->setLocalOffset(value.toInt());
                } else if (paramName == "SID") {
                    generatorDateTimeWidget->setSID(value.toUInt());
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model

    } else if (Pgn == -129025) {
        // Cast widget to GeneratorVesselMovement129025* and call the initialize function
        auto *generatorVesselMovementWidget = qobject_cast<GeneratorVesselMovement *>(widget);

        // Connect SID changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::SIDChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(generatorVesselMovementWidget->SID()));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Latitude changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::LatitudeChanged,
            generatorVesselMovementWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Latitude", generatorVesselMovementWidget->Latitude());
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Longitude changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::LongitudeChanged,
            generatorVesselMovementWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Longitude", generatorVesselMovementWidget->Longitude());
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Altitude changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::AltitudeChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "Altitude", QString::number(generatorVesselMovementWidget->Altitude(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect HeadingReference changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::HeadingReferenceChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "HeadingReference", QString::number(generatorVesselMovementWidget->HeadingReference()));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Heading changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::HeadingChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "Heading", QString::number(generatorVesselMovementWidget->Heading(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Deviation changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::DeviationChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "Deviation", QString::number(generatorVesselMovementWidget->Deviation(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Variation changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::VariationChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "Variation", QString::number(generatorVesselMovementWidget->Variation(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect COG changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::COGChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "COG", QString::number(generatorVesselMovementWidget->COG(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect SOG changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::SOGChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "SOG", QString::number(generatorVesselMovementWidget->SOG(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect RateOfTurn changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::RateOfTurnChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "RateOfTurn", QString::number(generatorVesselMovementWidget->RateOfTurn(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Yaw changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::YawChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "Yaw", QString::number(generatorVesselMovementWidget->Yaw(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Pitch changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::PitchChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "Pitch", QString::number(generatorVesselMovementWidget->Pitch(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Roll changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::RollChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "Roll", QString::number(generatorVesselMovementWidget->Roll(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect Heave changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::HeaveChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "Heave", QString::number(generatorVesselMovementWidget->Heave(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);

        // Connect RudderPosition changes (widget -> model)
        connectModel(
            generatorVesselMovementWidget,
            &GeneratorVesselMovement::RudderPositionChanged,
            generatorVesselMovementWidget,
            [=]() {
                model->changeParam(idX, "RudderPosition", QString::number(generatorVesselMovementWidget->RudderPosition(), 'g', 15));
                onWidgetInstanceValueChanged(generatorVesselMovementWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, generatorVesselMovementWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? generatorVesselMovementWidget : nullptr);
                if (paramName == "SID") {
                    generatorVesselMovementWidget->setSID(value.toUInt());
                } else if (paramName == "Latitude") {
                    generatorVesselMovementWidget->setLatitude(value.toDouble());
                } else if (paramName == "Longitude") {
                    generatorVesselMovementWidget->setLongitude(value.toDouble());
                } else if (paramName == "Altitude") {
                    generatorVesselMovementWidget->setAltitude(value.toDouble());
                } else if (paramName == "HeadingReference") {
                    generatorVesselMovementWidget->setHeadingReference(static_cast<tN2kHeadingReference>(value.toUInt()));
                } else if (paramName == "Heading") {
                    generatorVesselMovementWidget->setHeading(value.toDouble());
                } else if (paramName == "Deviation") {
                    generatorVesselMovementWidget->setDeviation(value.toDouble());
                } else if (paramName == "Variation") {
                    generatorVesselMovementWidget->setVariation(value.toDouble());
                } else if (paramName == "COG") {
                    generatorVesselMovementWidget->setCOG(value.toDouble());
                } else if (paramName == "SOG") {
                    generatorVesselMovementWidget->setSOG(value.toDouble());
                } else if (paramName == "RateOfTurn") {
                    generatorVesselMovementWidget->setRateOfTurn(value.toDouble());
                } else if (paramName == "Yaw") {
                    generatorVesselMovementWidget->setYaw(value.toDouble());
                } else if (paramName == "Pitch") {
                    generatorVesselMovementWidget->setPitch(value.toDouble());
                } else if (paramName == "Roll") {
                    generatorVesselMovementWidget->setRoll(value.toDouble());
                } else if (paramName == "Heave") {
                    generatorVesselMovementWidget->setHeave(value.toDouble());
                } else if (paramName == "RudderPosition") {
                    generatorVesselMovementWidget->setRudderPosition(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model

        // Connect ShowStatusBarMessage signal to MainWindow slot
        connectModel(generatorVesselMovementWidget,
                &GeneratorVesselMovement::ShowStatusBarMessage,
                this,
                &MainController::ShowStatusBarMessage);

    } else if (Pgn == -127489) {
        // Cast widget to GeneratorEngineWidget*
        auto *generatorEngineWidget = qobject_cast<GeneratorEngine *>(widget);

        // Connect EngineInstance changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineInstanceChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineInstance", QString::number(generatorEngineWidget->EngineInstance()));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineSpeed changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineSpeedChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineSpeed", QString::number(generatorEngineWidget->EngineSpeed(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineBoostPressure changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineBoostPressureChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineBoostPressure", QString::number(generatorEngineWidget->EngineBoostPressure(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineTiltTrim changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineTiltTrimChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineTiltTrim", QString::number(generatorEngineWidget->EngineTiltTrim()));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineOilPress changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineOilPressChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineOilPress", QString::number(generatorEngineWidget->EngineOilPress(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineOilTemp changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineOilTempChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineOilTemp", QString::number(generatorEngineWidget->EngineOilTemp(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineCoolantTemp changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineCoolantTempChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineCoolantTemp", QString::number(generatorEngineWidget->EngineCoolantTemp(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect AltenatorVoltage changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::AltenatorVoltageChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "AltenatorVoltage", QString::number(generatorEngineWidget->AltenatorVoltage(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect FuelRate changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::FuelRateChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "FuelRate", QString::number(generatorEngineWidget->FuelRate(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineHours changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineHoursChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineHours", QString::number(generatorEngineWidget->EngineHours(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineCoolantPress changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineCoolantPressChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineCoolantPress", QString::number(generatorEngineWidget->EngineCoolantPress(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineFuelPress changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineFuelPressChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineFuelPress", QString::number(generatorEngineWidget->EngineFuelPress(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineLoad changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineLoadChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineLoad", QString::number(generatorEngineWidget->EngineLoad(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect EngineTorque changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineTorqueChanged,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "EngineTorque", QString::number(generatorEngineWidget->EngineTorque(), 'g', 15));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect Status1 changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineDiscreteStatus1Changed,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "Status1", QString::number(generatorEngineWidget->EngineDiscreteStatus1().Status));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);

        // Connect Status2 changes (widget -> model)
        connectModel(
            generatorEngineWidget,
            &GeneratorEngine::EngineDiscreteStatus2Changed,
            generatorEngineWidget,
            [=]() {
                model->changeParam(idX, "Status2", QString::number(generatorEngineWidget->EngineDiscreteStatus2().Status));
                onWidgetInstanceValueChanged(generatorEngineWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, generatorEngineWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? generatorEngineWidget : nullptr);
                if (paramName == "EngineInstance") {
                    generatorEngineWidget->setEngineInstance(value.toInt());
                } else if (paramName == "EngineSpeed") {
                    generatorEngineWidget->setEngineSpeed(value.toDouble());
                } else if (paramName == "EngineBoostPressure") {
                    generatorEngineWidget->setEngineBoostPressure(value.toDouble());
                } else if (paramName == "EngineTiltTrim") {
                    generatorEngineWidget->setEngineTiltTrim(value.toInt());
                } else if (paramName == "EngineOilPress") {
                    generatorEngineWidget->setEngineOilPress(value.toDouble());
                } else if (paramName == "EngineOilTemp") {
                    generatorEngineWidget->setEngineOilTemp(value.toDouble());
                } else if (paramName == "EngineCoolantTemp") {
                    generatorEngineWidget->setEngineCoolantTemp(value.toDouble());
                } else if (paramName == "AltenatorVoltage") {
                    generatorEngineWidget->setAltenatorVoltage(value.toDouble());
                } else if (paramName == "FuelRate") {
                    generatorEngineWidget->setFuelRate(value.toDouble());
                } else if (paramName == "EngineHours") {
                    generatorEngineWidget->setEngineHours(value.toDouble());
                } else if (paramName == "EngineCoolantPress") {
                    generatorEngineWidget->setEngineCoolantPress(value.toDouble());
                } else if (paramName == "EngineFuelPress") {
                    generatorEngineWidget->setEngineFuelPress(value.toDouble());
                } else if (paramName == "EngineLoad") {
                    generatorEngineWidget->setEngineLoad(value.toDouble());
                } else if (paramName == "EngineTorque") {
                    generatorEngineWidget->setEngineTorque(value.toDouble());
                } else if (paramName == "Status1") {
                    tN2kEngineDiscreteStatus1 status1;
                    generatorEngineWidget->setEngineDiscreteStatus1(status1);
                } else if (paramName == "Status2") {
                    tN2kEngineDiscreteStatus2 status2;
                    generatorEngineWidget->setEngineDiscreteStatus2(status2);
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model
        tN2kEngineDiscreteStatus1 status1;
        generatorEngineWidget->setEngineDiscreteStatus1(status1);
        tN2kEngineDiscreteStatus2 status2;
        generatorEngineWidget->setEngineDiscreteStatus2(status2);
    } else if (Pgn == -127493) {
        // Cast widget to GeneratorTransmissionWidget*
        auto *generatorTransLowerUnitWidget = qobject_cast<GeneratorTransLowerUnit *>(widget);

        // Connect TransmissionInstance changes (widget -> model)
        connectModel(
            generatorTransLowerUnitWidget,
            &GeneratorTransLowerUnit::EngineInstanceChanged,
            generatorTransLowerUnitWidget,
            [=]() {
                model->changeParam(idX, "EngineInstance", QString::number(generatorTransLowerUnitWidget->EngineInstance()));
                onWidgetInstanceValueChanged(generatorTransLowerUnitWidget->Id());
            },
            parameterEditConnection);

        // Connect TransmissionGear changes (widget -> model)
        connectModel(
            generatorTransLowerUnitWidget,
            &GeneratorTransLowerUnit::TransmissionGearChanged,
            generatorTransLowerUnitWidget,
            [=]() {
                model->changeParam(idX, "TransmissionGear", QString::number(generatorTransLowerUnitWidget->TransmissionGear()));
                onWidgetInstanceValueChanged(generatorTransLowerUnitWidget->Id());
            },
            parameterEditConnection);

        // Connect Oil Pressure changes (widget -> model)
        connectModel(
            generatorTransLowerUnitWidget,
            &GeneratorTransLowerUnit::OilPressureChanged,
            generatorTransLowerUnitWidget,
            [=]() {
                model->changeParam(idX, "OilPressure", QString::number(generatorTransLowerUnitWidget->OilPressure(), 'g', 15));
                onWidgetInstanceValueChanged(generatorTransLowerUnitWidget->Id());
            },
            parameterEditConnection);

        // Connect Oil Temperature changes (widget -> model)
        connectModel(
            generatorTransLowerUnitWidget,
            &GeneratorTransLowerUnit::OilTemperatureChanged,
            generatorTransLowerUnitWidget,
            [=]() {
                model->changeParam(idX, "OilTemperature", QString::number(generatorTransLowerUnitWidget->OilTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(generatorTransLowerUnitWidget->Id());
            },
            parameterEditConnection);

        // Connect DiscreteStatus1 changes (widget -> model)
        connectModel(
            generatorTransLowerUnitWidget,
            &GeneratorTransLowerUnit::DiscreteStatus1Changed,
            generatorTransLowerUnitWidget,
            [=]() {
                model->changeParam(idX, "DiscreteStatus1", QString::number(generatorTransLowerUnitWidget->DiscreteStatus1()));
                onWidgetInstanceValueChanged(generatorTransLowerUnitWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, generatorTransLowerUnitWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? generatorTransLowerUnitWidget : nullptr);
                if (paramName == "EngineInstance") {
                    generatorTransLowerUnitWidget->setEngineInstance(value.toInt());
                } else if (paramName == "TransmissionGear") {
                    generatorTransLowerUnitWidget->setTransmissionGear(static_cast<tN2kTransmissionGear>(value.toInt()));
                } else if (paramName == "OilPressure") {
                    generatorTransLowerUnitWidget->setOilPressure(value.toDouble());
                } else if (paramName == "OilTemperature") {
                    generatorTransLowerUnitWidget->setOilTemperature(value.toDouble());
                } else if (paramName == "DiscreteStatus1") {
                    generatorTransLowerUnitWidget->setDiscreteStatus1(value.toUInt());
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Named initialization follows in initializeWidgetFromModel.

    } else if (Pgn == -130311) {
        // Cast widget to GeneratorWeather* and call the initialize function
        auto *generatorWeatherWidget = qobject_cast<GeneratorWeather *>(widget);
        connectModel(generatorWeatherWidget, &GeneratorWeather::ObservationMetadataChanged, generatorWeatherWidget,
            [model, generatorWeatherWidget, idX] {
                for (const auto &row : model->getPgnParams(idX))
                    model->setParameterDescription(idX, row[0], generatorWeatherWidget->fieldDescription(row[0]));
            });


        connectModel(generatorWeatherWidget, &GeneratorWeather::MarineObservationsChanged,
                     generatorWeatherWidget, [=] {
            for (const auto& row : GeneratorWeather::additionalParameters())
                model->changeParam(idX, row[0], QString::number(generatorWeatherWidget->property(row[0].toLatin1().constData()).toDouble(), 'g', 17));
            onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
        }, parameterEditConnection);

        // Connect SID changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::SIDChanged,
            generatorWeatherWidget,
            [=]() {
                model->changeParam(idX, "SID", QString::number(generatorWeatherWidget->SID()));
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect AirTemperature changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::AirTemperatureChanged,
            generatorWeatherWidget,
            [=]() {
                model->changeParam(idX, "AirTemperature", QString::number(generatorWeatherWidget->AirTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect SeaTemperature changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::SeaTemperatureChanged,
            generatorWeatherWidget,
            [=]() {
                model->changeParam(idX, "SeaTemperature", QString::number(generatorWeatherWidget->SeaTemperature(), 'g', 15));
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect Humidity changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::HumidityChanged,
            generatorWeatherWidget,
            [=]() {
                model->changeParam(idX, "Humidity", QString::number(generatorWeatherWidget->Humidity(), 'g', 15));
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect AtmosphericPressure changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::AtmosphericPressureChanged,
            generatorWeatherWidget,
            [=]() {
                model->changeParam(idX, "AtmosphericPressure", QString::number(generatorWeatherWidget->AtmosphericPressure(), 'g', 15));
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect WindSpeed changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::WindSpeedChanged,
            generatorWeatherWidget,
            [=]() {
                model->changeParam(idX, "WindSpeed", QString::number(generatorWeatherWidget->WindSpeed(), 'g', 15));
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect WindAngle changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::WindAngleChanged,
            generatorWeatherWidget,
            [=]() {
                model->changeParam(idX, "WindAngle", QString::number(generatorWeatherWidget->WindAngle(), 'g', 15));
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect TemperatureSource changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::TemperatureSourceChanged,
            generatorWeatherWidget,
            [=]() {
                model->changeParam(idX, "TemperatureSource", QString::number(generatorWeatherWidget->TemperatureSource()));
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect HumiditySource changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::HumiditySourceChanged,
            generatorWeatherWidget,
            [=]() {
                model->changeParam(idX, "HumiditySource", QString::number(generatorWeatherWidget->HumiditySource()));
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect Latitude changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::LatitudeChanged,
            generatorWeatherWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Latitude", generatorWeatherWidget->Latitude());
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);

        // Connect Longitude changes (widget -> model)
        connectModel(
            generatorWeatherWidget,
            &GeneratorWeather::LongitudeChanged,
            generatorWeatherWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Longitude", generatorWeatherWidget->Longitude());
                onWidgetInstanceValueChanged(generatorWeatherWidget->Id());
            },
            parameterEditConnection);


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, generatorWeatherWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? generatorWeatherWidget : nullptr);
                for (const auto& row : GeneratorWeather::additionalParameters())
                    if (row[0] == paramName) generatorWeatherWidget->setProperty(paramName.toLatin1().constData(), value.toDouble());
                if (paramName == "AirTemperature") {
                    generatorWeatherWidget->setAirTemperature(value.toDouble());
                } else if (paramName == "SeaTemperature") {
                    generatorWeatherWidget->setSeaTemperature(value.toDouble());
                } else if (paramName == "Humidity") {
                    generatorWeatherWidget->setHumidity(value.toDouble());
                } else if (paramName == "AtmosphericPressure") {
                    generatorWeatherWidget->setAtmosphericPressure(value.toDouble());
                } else if (paramName == "WindSpeed") {
                    generatorWeatherWidget->setWindSpeed(value.toDouble());
                } else if (paramName == "WindAngle") {
                    generatorWeatherWidget->setWindAngle(value.toDouble());
                } else if (paramName == "TemperatureSource") {
                    generatorWeatherWidget->setTemperatureSource(static_cast<tN2kTempSource>(value.toInt()));
                } else if (paramName == "HumiditySource") {
                    generatorWeatherWidget->setHumiditySource(static_cast<tN2kHumiditySource>(value.toInt()));
                } else if (paramName == "Latitude") {
                    generatorWeatherWidget->setLatitude(value.toDouble());
                } else if (paramName == "Longitude") {
                    generatorWeatherWidget->setLongitude(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);

        });

        // Set widget properties to table model

        // Connect GeneratorVesselMovementWidget Latitude and Londitude changes to GeneratorWeatherWidget Latitude and Longitude if enabled
        if (m_mode == Sender) {
            // Connect generatorVesselMovement Widget signals to latLongWidget
            if (generatorVesselMovementWidget == nullptr) {
                if (m_rebindingOnly || m_restoringLayout) return;
                // Ask user create a GeneratorVesselMovement widget if it doesn't exist
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this->mdiArea,
                                              "No GeneratorVesselMovement Widget",
                                              "Would you like to create a GeneratorVesselMovement widget?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    addGeneratorWidget("Vessel Movement (PGN -129025)");
                } else {
                    qWarning() << "generatorVesselMovementWidget is null";
                    return;
                }
            }

            // Connect Latitude changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::LatitudeChanged,
                generatorWeatherWidget,
                [this, generatorWeatherWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    generatorWeatherWidget->setLatitude(this->generatorVesselMovementWidget->Latitude());
                }
            },
                Qt::QueuedConnection, "setLatitude, Latitude");

            // Connect Longitude changes (generator widget -> widget)
            connectWhileSenderAlive(
                generatorVesselMovementWidget,
                &GeneratorVesselMovement::LongitudeChanged,
                generatorWeatherWidget,
                [this, generatorWeatherWidget]() {
                if (this->generatorVesselMovementWidget->isEnabled()) {
                    generatorWeatherWidget->setLongitude(this->generatorVesselMovementWidget->Longitude());
                }
            },
                Qt::QueuedConnection, "setLongitude, Longitude");
        }
    } else if (Pgn == SNT_GGA) {
        // Cast widget to GlobalPositioningGGA*
        auto *globalPositioningGGAWidget = qobject_cast<GlobalPositioningGGA *>(widget);

        // Connect GPSTime changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::GPSTimeChanged,
            globalPositioningGGAWidget,
            [=]() {
                model->changeParam(idX, "GPSTime", QString::number(globalPositioningGGAWidget->GPSTime(), 'g', 15));
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect Latitude changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::LatitudeChanged,
            globalPositioningGGAWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Latitude", globalPositioningGGAWidget->Latitude());
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect Longitude changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::LongitudeChanged,
            globalPositioningGGAWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Longitude", globalPositioningGGAWidget->Longitude());
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect GPSQualityIndicator changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::GPSQualityIndicatorChanged,
            globalPositioningGGAWidget,
            [=]() {
                model->changeParam(idX, "GPSQualityIndicator", QString::number(globalPositioningGGAWidget->GPSQualityIndicator()));
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect SatelliteCount changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::SatelliteCountChanged,
            globalPositioningGGAWidget,
            [=]() {
                model->changeParam(idX, "SatelliteCount", QString::number(globalPositioningGGAWidget->SatelliteCount()));
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect HDOP changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::HDOPChanged,
            globalPositioningGGAWidget,
            [=]() {
                model->changeParam(idX, "HDOP", QString::number(globalPositioningGGAWidget->HDOP(), 'g', 15));
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect Altitude changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::AltitudeChanged,
            globalPositioningGGAWidget,
            [=]() {
                model->changeParam(idX, "Altitude", QString::number(globalPositioningGGAWidget->Altitude(), 'g', 15));
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect GeoidalSeparation changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::GeoidalSeparationChanged,
            globalPositioningGGAWidget,
            [=]() {
                model->changeParam(idX, "GeoidalSeparation", QString::number(globalPositioningGGAWidget->GeoidalSeparation(), 'g', 15));
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect DGPSAge changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::DGPSAgeChanged,
            globalPositioningGGAWidget,
            [=]() {
                model->changeParam(idX, "DGPSAge", QString::number(globalPositioningGGAWidget->DGPSAge(), 'g', 15));
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect DGPSReferenceStationID changes (widget -> model)
        connectModel(
            globalPositioningGGAWidget,
            &GlobalPositioningGGA::DGPSReferenceStationIDChanged,
            globalPositioningGGAWidget,
            [=]() {
                model->changeParam(idX, "DGPSReferenceStationID", QString::number(globalPositioningGGAWidget->DGPSReferenceStationID()));
                onWidgetInstanceValueChanged(globalPositioningGGAWidget->Id());
            },
            parameterEditConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(globalPositioningGGAWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, globalPositioningGGAWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(globalPositioningGGAWidget->Id());
                removeWidgetFromSubForm(globalPositioningGGAWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, globalPositioningGGAWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? globalPositioningGGAWidget : nullptr);
                if (paramName == "GPSTime") {
                    globalPositioningGGAWidget->setGPSTime(value.toDouble());
                } else if (paramName == "Latitude") {
                    globalPositioningGGAWidget->setLatitude(value.toDouble());
                } else if (paramName == "Longitude") {
                    globalPositioningGGAWidget->setLongitude(value.toDouble());
                } else if (paramName == "GPSQualityIndicator") {
                    globalPositioningGGAWidget->setGPSQualityIndicator(value.toInt());
                } else if (paramName == "SatelliteCount") {
                    globalPositioningGGAWidget->setSatelliteCount(value.toInt());
                } else if (paramName == "HDOP") {
                    globalPositioningGGAWidget->setHDOP(value.toDouble());
                } else if (paramName == "Altitude") {
                    globalPositioningGGAWidget->setAltitude(value.toDouble());
                } else if (paramName == "GeoidalSeparation") {
                    globalPositioningGGAWidget->setGeoidalSeparation(value.toDouble());
                } else if (paramName == "DGPSAge") {
                    globalPositioningGGAWidget->setDGPSAge(value.toDouble());
                } else if (paramName == "DGPSReferenceStationID") {
                    globalPositioningGGAWidget->setDGPSReferenceStationID(value.toInt());
                }
            }
            pgnTableController->refreshPgnParameters(idX);
        });

        // Named initialization follows in initializeWidgetFromModel.

        if(m_mode == Sender) {
            // Connect generatorDateTimeWidget to gnssPositionDataWidget
            if (generatorDateTimeWidget != nullptr) {

                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::SecondsSinceMidnightChanged,
                    globalPositioningGGAWidget,
                    [this, globalPositioningGGAWidget]() { globalPositioningGGAWidget->setGPSTime(this->generatorDateTimeWidget->SecondsSinceMidnight()); },
                    Qt::QueuedConnection, "setGPSTime, SecondsSinceMidnight");
            }

            // Connect generatorVesselMovement Widget signals to gnssPositionDataWidget
            if (generatorVesselMovementWidget != nullptr) {
                // Connect Latitude changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::LatitudeChanged,
                    globalPositioningGGAWidget,
                    [this, globalPositioningGGAWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        globalPositioningGGAWidget->setLatitude(this->generatorVesselMovementWidget->Latitude());
                    }
                },
                    Qt::QueuedConnection, "setLatitude, Latitude");

                // Connect Longitude changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::LongitudeChanged,
                    globalPositioningGGAWidget,
                    [this, globalPositioningGGAWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        globalPositioningGGAWidget->setLongitude(this->generatorVesselMovementWidget->Longitude());
                    }
                },
                    Qt::QueuedConnection, "setLongitude, Longitude");
            }
        }

    } else if (Pgn == SNT_RMC) {
        // Cast widget to RecommendedMinGnssRMC*
        auto *recommendedMinGnssRMCWidget = qobject_cast<RecommendedMinGnssRMC *>(widget);

        // Connect GPSTime changes (widget -> model)
        connectModel(
            recommendedMinGnssRMCWidget,
            &RecommendedMinGnssRMC::GPSTimeChanged,
            recommendedMinGnssRMCWidget,
            [=]() {
                model->changeParam(idX, "GPSTime", QString::number(recommendedMinGnssRMCWidget->GPSTime(), 'g', 15));
                onWidgetInstanceValueChanged(recommendedMinGnssRMCWidget->Id());
            },
            parameterEditConnection);

        // Connect Status changes (widget -> model)
        connectModel(
            recommendedMinGnssRMCWidget,
            &RecommendedMinGnssRMC::StatusChanged,
            recommendedMinGnssRMCWidget,
            [=]() {
            char status = recommendedMinGnssRMCWidget->Status();
            model->changeParam(idX, "Status", QString::fromLatin1(&status, 1));
                onWidgetInstanceValueChanged(recommendedMinGnssRMCWidget->Id());
            },
            parameterEditConnection);

        // Connect Latitude changes (widget -> model)
        connectModel(
            recommendedMinGnssRMCWidget,
            &RecommendedMinGnssRMC::LatitudeChanged,
            recommendedMinGnssRMCWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Latitude", recommendedMinGnssRMCWidget->Latitude());
                onWidgetInstanceValueChanged(recommendedMinGnssRMCWidget->Id());
            },
            parameterEditConnection);

        // Connect Longitude changes (widget -> model)
        connectModel(
            recommendedMinGnssRMCWidget,
            &RecommendedMinGnssRMC::LongitudeChanged,
            recommendedMinGnssRMCWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Longitude", recommendedMinGnssRMCWidget->Longitude());
                onWidgetInstanceValueChanged(recommendedMinGnssRMCWidget->Id());
            },
            parameterEditConnection);

        // Connect TrueCOG changes (widget -> model)
        connectModel(
            recommendedMinGnssRMCWidget,
            &RecommendedMinGnssRMC::TrueCOGChanged,
            recommendedMinGnssRMCWidget,
            [=]() {
                model->changeParam(idX, "TrueCOG", QString::number(recommendedMinGnssRMCWidget->TrueCOG(), 'g', 15));
                onWidgetInstanceValueChanged(recommendedMinGnssRMCWidget->Id());
            },
            parameterEditConnection);

        // Connect SOG changes (widget -> model)
        connectModel(
            recommendedMinGnssRMCWidget,
            &RecommendedMinGnssRMC::SOGChanged,
            recommendedMinGnssRMCWidget,
            [=]() {
                model->changeParam(idX, "SOG", QString::number(recommendedMinGnssRMCWidget->SOG(), 'g', 15));
                onWidgetInstanceValueChanged(recommendedMinGnssRMCWidget->Id());
            },
            parameterEditConnection);

        // Connect DaysSince1970 changes (widget -> model)
        connectModel(
            recommendedMinGnssRMCWidget,
            &RecommendedMinGnssRMC::DaysSince1970Changed,
            recommendedMinGnssRMCWidget,
            [=]() {
                model->changeParam(idX, "DaysSince1970", QString::number(recommendedMinGnssRMCWidget->DaysSince1970()));
                onWidgetInstanceValueChanged(recommendedMinGnssRMCWidget->Id());
            },
            parameterEditConnection);

        // Connect Variation changes (widget -> model)
        connectModel(
            recommendedMinGnssRMCWidget,
            &RecommendedMinGnssRMC::VariationChanged,
            recommendedMinGnssRMCWidget,
            [=]() {
                model->changeParam(idX, "Variation", QString::number(recommendedMinGnssRMCWidget->Variation(), 'g', 15));
                onWidgetInstanceValueChanged(recommendedMinGnssRMCWidget->Id());
            },
            parameterEditConnection);

        // Connect DateTime changes (widget -> model)
        connectModel(
            recommendedMinGnssRMCWidget,
            &RecommendedMinGnssRMC::DateTimeChanged,
            recommendedMinGnssRMCWidget,
            [=]() {
                model->changeParam(idX, "DateTime", QString::number(recommendedMinGnssRMCWidget->DateTime()));
                onWidgetInstanceValueChanged(recommendedMinGnssRMCWidget->Id());
            },
            parameterEditConnection);

        // Connect the remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(recommendedMinGnssRMCWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, recommendedMinGnssRMCWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(recommendedMinGnssRMCWidget->Id());
                removeWidgetFromSubForm(recommendedMinGnssRMCWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, recommendedMinGnssRMCWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? recommendedMinGnssRMCWidget : nullptr);
                if (paramName == "GPSTime") {
                    recommendedMinGnssRMCWidget->setGPSTime(value.toDouble());
                } else if (paramName == "Status") {
                    QString stringValue = value.toString(); // Convert QVariant to QString
                        if (!stringValue.isEmpty()) { // Ensure the QString is not empty
                            recommendedMinGnssRMCWidget->setStatus(stringValue.toLatin1().at(0));
                        } else {
                            qWarning() << "Status string is empty, defaulting to 'A'";
                            recommendedMinGnssRMCWidget->setStatus('A');
                        }

                } else if (paramName == "Latitude") {
                    recommendedMinGnssRMCWidget->setLatitude(value.toDouble());
                } else if (paramName == "Longitude") {
                    recommendedMinGnssRMCWidget->setLongitude(value.toDouble());
                } else if (paramName == "TrueCOG") {
                    recommendedMinGnssRMCWidget->setTrueCOG(value.toDouble());
                } else if (paramName == "SOG") {
                    recommendedMinGnssRMCWidget->setSOG(value.toDouble());
                } else if (paramName == "DaysSince1970") {
                    recommendedMinGnssRMCWidget->setDaysSince1970(value.toInt());
                } else if (paramName == "Variation") {
                    recommendedMinGnssRMCWidget->setVariation(value.toDouble());
                } else if (paramName == "DateTime") {
                    recommendedMinGnssRMCWidget->setDateTime(value.toULongLong());
                }
            }
            pgnTableController->refreshPgnParameters(idX);
        });

        // Set widget properties to table model

        if (m_mode == Sender) {
            if (generatorDateTimeWidget) {
                connectWhileSenderAlive(generatorDateTimeWidget, &GeneratorDateTime::DaysSince1970Changed,
                    recommendedMinGnssRMCWidget, [this, recommendedMinGnssRMCWidget]() {
                        if (generatorDateTimeWidget->isEnabled())
                            recommendedMinGnssRMCWidget->setDaysSince1970(generatorDateTimeWidget->DaysSince1970());
                    }, Qt::QueuedConnection, "setDaysSince1970, DaysSince1970");
                connectWhileSenderAlive(generatorDateTimeWidget, &GeneratorDateTime::SecondsSinceMidnightChanged,
                    recommendedMinGnssRMCWidget, [this, recommendedMinGnssRMCWidget]() {
                        if (generatorDateTimeWidget->isEnabled())
                            recommendedMinGnssRMCWidget->setGPSTime(generatorDateTimeWidget->SecondsSinceMidnight());
                    }, Qt::QueuedConnection, "setGPSTime, SecondsSinceMidnight");
            }
            if (generatorVesselMovementWidget) {
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::LatitudeChanged,
                    recommendedMinGnssRMCWidget, [this, recommendedMinGnssRMCWidget]() {
                        if (generatorVesselMovementWidget->isEnabled())
                            recommendedMinGnssRMCWidget->setLatitude(generatorVesselMovementWidget->Latitude());
                    }, Qt::QueuedConnection, "setLatitude, Latitude");
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::LongitudeChanged,
                    recommendedMinGnssRMCWidget, [this, recommendedMinGnssRMCWidget]() {
                        if (generatorVesselMovementWidget->isEnabled())
                            recommendedMinGnssRMCWidget->setLongitude(generatorVesselMovementWidget->Longitude());
                    }, Qt::QueuedConnection, "setLongitude, Longitude");
                const auto updateTrueCourse = [this, recommendedMinGnssRMCWidget]() {
                    if (generatorVesselMovementWidget->isEnabled())
                        recommendedMinGnssRMCWidget->setTrueCOG(movementSentenceAngle(generatorVesselMovementWidget, false, true));
                };
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::COGChanged,
                    recommendedMinGnssRMCWidget, updateTrueCourse, Qt::QueuedConnection, "updateTrueCourse");
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::HeadingReferenceChanged,
                    recommendedMinGnssRMCWidget, updateTrueCourse, Qt::QueuedConnection, "updateTrueCourse");
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::VariationChanged,
                    recommendedMinGnssRMCWidget, updateTrueCourse, Qt::QueuedConnection, "updateTrueCourse");
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::SOGChanged,
                    recommendedMinGnssRMCWidget, [this, recommendedMinGnssRMCWidget]() {
                        if (generatorVesselMovementWidget->isEnabled())
                            recommendedMinGnssRMCWidget->setSOG(generatorVesselMovementWidget->SOG());
                    }, Qt::QueuedConnection, "setSOG, SOG");
            }
        }
    } else if (Pgn == SNT_HDG) {
        // Cast widget to HeadingHDG*
        auto *headingHDGWidget = qobject_cast<HeadingHDG *>(widget);

        // Connect Heading changes (widget -> model)
        connectModel(
            headingHDGWidget,
            &HeadingHDG::HeadingChanged,
            headingHDGWidget,
            [=]() {
                model->changeParam(idX, "Heading", QString::number(headingHDGWidget->Heading(), 'g', 15));
                onWidgetInstanceValueChanged(headingHDGWidget->Id());
            },
            parameterEditConnection);

        // Connect Deviation changes (widget -> model)
        connectModel(
            headingHDGWidget,
            &HeadingHDG::DeviationChanged,
            headingHDGWidget,
            [=]() {
                model->changeParam(idX, "Deviation", QString::number(headingHDGWidget->Deviation(), 'g', 15));
                onWidgetInstanceValueChanged(headingHDGWidget->Id());
            },
            parameterEditConnection);

        // Connect Variation changes (widget -> model)
        connectModel(
            headingHDGWidget,
            &HeadingHDG::VariationChanged,
            headingHDGWidget,
            [=]() {
                model->changeParam(idX, "Variation", QString::number(headingHDGWidget->Variation(), 'g', 15));
                onWidgetInstanceValueChanged(headingHDGWidget->Id());
            },
            parameterEditConnection);

        // Connect the Remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(headingHDGWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, headingHDGWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(headingHDGWidget->Id());
                removeWidgetFromSubForm(headingHDGWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, headingHDGWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? headingHDGWidget : nullptr);
                if (paramName == "Heading") {
                    headingHDGWidget->setHeading(value.toDouble());
                } else if (paramName == "Deviation") {
                    headingHDGWidget->setDeviation(value.toDouble());
                } else if (paramName == "Variation") {
                    headingHDGWidget->setVariation(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovement Widget signals to headingHDGWidget
            if (generatorVesselMovementWidget != nullptr) {
                // Connect Heading changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::HeadingChanged,
                    headingHDGWidget,
                    [this, headingHDGWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        headingHDGWidget->setHeading(this->generatorVesselMovementWidget->Heading());
                    }
                },
                    Qt::QueuedConnection, "setHeading, Heading");
            }
        }

    } else if (Pgn == SNT_HDT) {
        // Cast widget to TrueHeadingHDT*
        auto *trueHeadingHDTWidget = qobject_cast<TrueHeadingHDT *>(widget);

        // Connect Heading changes (widget -> model)
        connectModel(
            trueHeadingHDTWidget,
            &TrueHeadingHDT::HeadingChanged,
            trueHeadingHDTWidget,
            [=]() {
                model->changeParam(idX, "Heading", QString::number(trueHeadingHDTWidget->Heading(), 'g', 15));
                onWidgetInstanceValueChanged(trueHeadingHDTWidget->Id());
            },
            parameterEditConnection);

        // Connect the Remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(trueHeadingHDTWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, trueHeadingHDTWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(trueHeadingHDTWidget->Id());
                removeWidgetFromSubForm(trueHeadingHDTWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, trueHeadingHDTWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? trueHeadingHDTWidget : nullptr);
                if (paramName == "Heading") {
                    trueHeadingHDTWidget->setHeading(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovement Widget signals to trueHeadingHDTWidget
            if (generatorVesselMovementWidget != nullptr) {
                const auto updateTrueHeading = [this, trueHeadingHDTWidget]() {
                    if (generatorVesselMovementWidget->isEnabled())
                        trueHeadingHDTWidget->setHeading(movementSentenceAngle(generatorVesselMovementWidget, true, true));
                };
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::HeadingChanged,
                    trueHeadingHDTWidget, updateTrueHeading, Qt::QueuedConnection, "updateTrueHeading");
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::HeadingReferenceChanged,
                    trueHeadingHDTWidget, updateTrueHeading, Qt::QueuedConnection, "updateTrueHeading");
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::VariationChanged,
                    trueHeadingHDTWidget, updateTrueHeading, Qt::QueuedConnection, "updateTrueHeading");
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::DeviationChanged,
                    trueHeadingHDTWidget, updateTrueHeading, Qt::QueuedConnection, "updateTrueHeading");
            }
        }
    } else if (Pgn == SNT_GLL) {
        // Cast widget to GeographicPositionGLL*
        auto *geographicPositionGLLWidget = qobject_cast<GeographicPositionGLL *>(widget);

        connectModel(geographicPositionGLLWidget, &GeographicPositionGLL::StatusChanged,
            geographicPositionGLLWidget, [=]() {
                model->changeParam(idX, "Status", QString(QChar(geographicPositionGLLWidget->Status())));
                onWidgetInstanceValueChanged(geographicPositionGLLWidget->Id());
            }, parameterEditConnection);

        // Connect GPSTime changes (widget -> model)
        connectModel(
            geographicPositionGLLWidget,
            &GeographicPositionGLL::GPSTimeChanged,
            geographicPositionGLLWidget,
            [=]() {
                model->changeParam(idX, "GPSTime", QString::number(geographicPositionGLLWidget->GPSTime(), 'g', 15));
                onWidgetInstanceValueChanged(geographicPositionGLLWidget->Id());
            },
            parameterEditConnection);

        // Connect Latitude changes (widget -> model)
        connectModel(
            geographicPositionGLLWidget,
            &GeographicPositionGLL::LatitudeChanged,
            geographicPositionGLLWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Latitude", geographicPositionGLLWidget->Latitude());
                onWidgetInstanceValueChanged(geographicPositionGLLWidget->Id());
            },
            parameterEditConnection);

        // Connect Longitude changes (widget -> model)
        connectModel(
            geographicPositionGLLWidget,
            &GeographicPositionGLL::LongitudeChanged,
            geographicPositionGLLWidget,
            [=]() {
                updateCoordinateParameter(model, idX, "Longitude", geographicPositionGLLWidget->Longitude());
                onWidgetInstanceValueChanged(geographicPositionGLLWidget->Id());
            },
            parameterEditConnection);

        // Connect the Remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(geographicPositionGLLWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, geographicPositionGLLWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(geographicPositionGLLWidget->Id());
                removeWidgetFromSubForm(geographicPositionGLLWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, geographicPositionGLLWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? geographicPositionGLLWidget : nullptr);
                if (paramName == "Status") {
                    geographicPositionGLLWidget->setStatus((value.toString().isEmpty() ? 'V' : value.toString().at(0).toLatin1()));
                } else if (paramName == "GPSTime") {
                    geographicPositionGLLWidget->setGPSTime(value.toDouble());
                } else if (paramName == "Latitude") {
                    geographicPositionGLLWidget->setLatitude(value.toDouble());
                } else if (paramName == "Longitude") {
                    geographicPositionGLLWidget->setLongitude(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorDateTimeWidget to gnssPositionDataWidget
            if (generatorDateTimeWidget != nullptr) {

                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::SecondsSinceMidnightChanged,
                    geographicPositionGLLWidget,
                    [this, geographicPositionGLLWidget]() { geographicPositionGLLWidget->setGPSTime(this->generatorDateTimeWidget->SecondsSinceMidnight()); },
                    Qt::QueuedConnection, "setGPSTime, SecondsSinceMidnight");
            }

            // Connect generatorVesselMovement Widget signals to gnssPositionDataWidget
            if (generatorVesselMovementWidget != nullptr) {
                // Connect Latitude changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::LatitudeChanged,
                    geographicPositionGLLWidget,
                    [this, geographicPositionGLLWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        geographicPositionGLLWidget->setLatitude(this->generatorVesselMovementWidget->Latitude());
                    }
                },
                    Qt::QueuedConnection, "setLatitude, Latitude");

                // Connect Longitude changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::LongitudeChanged,
                    geographicPositionGLLWidget,
                    [this, geographicPositionGLLWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        geographicPositionGLLWidget->setLongitude(this->generatorVesselMovementWidget->Longitude());
                    }
                },
                    Qt::QueuedConnection, "setLongitude, Longitude");
            }
        }
    } else if (Pgn == SNT_VTG) {
        // Cast widget to CogSogVTG*
        auto *cogSogVTGWidget = qobject_cast<CogSogVTG *>(widget);

        // Connect TrueCOG changes (widget -> model)
        connectModel(
            cogSogVTGWidget,
            &CogSogVTG::TrueCOGChanged,
            cogSogVTGWidget,
            [=]() {
                model->changeParam(idX, "TrueCOG", QString::number(cogSogVTGWidget->TrueCOG(), 'g', 15));
                onWidgetInstanceValueChanged(cogSogVTGWidget->Id());
            },
            parameterEditConnection);

        // Connect MagneticCOG changes (widget -> model)
        connectModel(
            cogSogVTGWidget,
            &CogSogVTG::MagneticCOGChanged,
            cogSogVTGWidget,
            [=]() {
                model->changeParam(idX, "MagneticCOG", QString::number(cogSogVTGWidget->MagneticCOG(), 'g', 15));
                onWidgetInstanceValueChanged(cogSogVTGWidget->Id());
            },
            parameterEditConnection);

        // Connect SOG changes (widget -> model)
        connectModel(
            cogSogVTGWidget,
            &CogSogVTG::SOGChanged,
            cogSogVTGWidget,
            [=]() {
                model->changeParam(idX, "SOG", QString::number(cogSogVTGWidget->SOG(), 'g', 15));
                onWidgetInstanceValueChanged(cogSogVTGWidget->Id());
            },
            parameterEditConnection);

        // Connect the Remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(cogSogVTGWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, cogSogVTGWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(cogSogVTGWidget->Id());
                removeWidgetFromSubForm(cogSogVTGWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, cogSogVTGWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? cogSogVTGWidget : nullptr);
                if (paramName == "TrueCOG") {
                    cogSogVTGWidget->setTrueCOG(value.toDouble());
                } else if (paramName == "MagneticCOG") {
                    cogSogVTGWidget->setMagneticCOG(value.toDouble());
                } else if (paramName == "SOG") {
                    cogSogVTGWidget->setSOG(value.toDouble());
                }
            }
            pgnTableController->refreshPgnParameters(idX);
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorVesselMovement Widget signals to cogSogVTGWidget
            if (generatorVesselMovementWidget != nullptr) {
                const auto updateCourses = [this, cogSogVTGWidget]() {
                    if (generatorVesselMovementWidget->isEnabled()) {
                        cogSogVTGWidget->setTrueCOG(movementSentenceAngle(generatorVesselMovementWidget, false, true));
                        cogSogVTGWidget->setMagneticCOG(movementSentenceAngle(generatorVesselMovementWidget, false, false));
                    }
                };
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::COGChanged,
                    cogSogVTGWidget, updateCourses, Qt::QueuedConnection, "updateCourses");
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::HeadingReferenceChanged,
                    cogSogVTGWidget, updateCourses, Qt::QueuedConnection, "updateCourses");
                connectWhileSenderAlive(generatorVesselMovementWidget, &GeneratorVesselMovement::VariationChanged,
                    cogSogVTGWidget, updateCourses, Qt::QueuedConnection, "updateCourses");

                // Connect Speed changes (generator widget -> widget) SOG
                connectWhileSenderAlive(
                    generatorVesselMovementWidget,
                    &GeneratorVesselMovement::SOGChanged,
                    cogSogVTGWidget,
                    [this, cogSogVTGWidget]() {
                    if (this->generatorVesselMovementWidget->isEnabled()) {
                        cogSogVTGWidget->setSOG(this->generatorVesselMovementWidget->SOG());
                    }
                },
                    Qt::QueuedConnection, "setSOG, SOG");
            }
        }
    } else if (Pgn == SNT_ZDA) {
        // Cast widget to DateTimeZDA*
        auto *dateTimeZDAWidget = qobject_cast<DateTimeZDA *>(widget);

        // Connect GPSTime changes (widget -> model)
        connectModel(
            dateTimeZDAWidget,
            &DateTimeZDA::GPSTimeChanged,
            dateTimeZDAWidget,
            [=]() {
                model->changeParam(idX, "GPSTime", QString::number(dateTimeZDAWidget->GPSTime(), 'g', 15));
                onWidgetInstanceValueChanged(dateTimeZDAWidget->Id());
            },
            parameterEditConnection);

        // Connect GPSDay changes (widget -> model)
        connectModel(
            dateTimeZDAWidget,
            &DateTimeZDA::GPSDayChanged,
            dateTimeZDAWidget,
            [=]() {
                model->changeParam(idX, "GPSDay", QString::number(dateTimeZDAWidget->GPSDay()));
                onWidgetInstanceValueChanged(dateTimeZDAWidget->Id());
            },
            parameterEditConnection);

        // Connect GPSMonth changes (widget -> model)
        connectModel(
            dateTimeZDAWidget,
            &DateTimeZDA::GPSMonthChanged,
            dateTimeZDAWidget,
            [=]() {
                model->changeParam(idX, "GPSMonth", QString::number(dateTimeZDAWidget->GPSMonth()));
                onWidgetInstanceValueChanged(dateTimeZDAWidget->Id());
            },
            parameterEditConnection);

        // Connect GPSYear changes (widget -> model)
        connectModel(
            dateTimeZDAWidget,
            &DateTimeZDA::GPSYearChanged,
            dateTimeZDAWidget,
            [=]() {
                model->changeParam(idX, "GPSYear", QString::number(dateTimeZDAWidget->GPSYear()));
                onWidgetInstanceValueChanged(dateTimeZDAWidget->Id());
            },
            parameterEditConnection);

        // Connect LZD changes (widget -> model)
        connectModel(
            dateTimeZDAWidget,
            &DateTimeZDA::LZDChanged,
            dateTimeZDAWidget,
            [=]() {
                model->changeParam(idX, "LZD", QString::number(dateTimeZDAWidget->LZD()));
                onWidgetInstanceValueChanged(dateTimeZDAWidget->Id());
            },
            parameterEditConnection);

        // Connect LZMD changes (widget -> model)
        connectModel(
            dateTimeZDAWidget,
            &DateTimeZDA::LZMDChanged,
            dateTimeZDAWidget,
            [=]() {
                model->changeParam(idX, "LZMD", QString::number(dateTimeZDAWidget->LZMD()));
                onWidgetInstanceValueChanged(dateTimeZDAWidget->Id());
            },
            parameterEditConnection);

        // Connect the Remove button to the removeWidgetFromSubForm
        QString groupName = "Navigation";
        connectModel(dateTimeZDAWidget->findChild<QPushButton*>("pushButtonRemove"), &QPushButton::clicked, [this, dateTimeZDAWidget, groupName]() {
            // Ask user to confirm the removal
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this->mdiArea,
                                          "Remove Widget",
                                          "Are you sure you want to remove this widget?",
                                          QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                pgnTableController->removePgnInstance(dateTimeZDAWidget->Id());
                removeWidgetFromSubForm(dateTimeZDAWidget, groupName);
                // Qt handles the deletion of the widget, no need to delete it manually
            }
        });


        // Connect model changes to widget for updates in the opposite direction (model -> widget)
        connectModel(model, &PgnTableModel::parameterChanged, dateTimeZDAWidget, [=](int changedIdX, const QString &paramName, const QVariant &value) {
            if (changedIdX == idX) {
                const QSignalBlocker displaySignals(Pgn > 0 ? dateTimeZDAWidget : nullptr);
                if (paramName == "GPSTime") {
                    dateTimeZDAWidget->setGPSTime(value.toDouble());
                } else if (paramName == "GPSDay") {
                    dateTimeZDAWidget->setGPSDay(value.toInt());
                } else if (paramName == "GPSMonth") {
                    dateTimeZDAWidget->setGPSMonth(value.toInt());
                } else if (paramName == "GPSYear") {
                    dateTimeZDAWidget->setGPSYear(value.toInt());
                } else if (paramName == "LZD") {
                    dateTimeZDAWidget->setLZD(value.toInt());
                } else if (paramName == "LZMD") {
                    dateTimeZDAWidget->setLZMD(value.toInt());
                }
            }
            pgnTableController->refreshPgnParameters(idX);
        });

        // Set widget properties to table model

        if(m_mode == Sender) {
            // Connect generatorDateTimeWidget to gnssPositionDataWidget
            if (generatorDateTimeWidget != nullptr) {

                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::UnixTimeChanged,
                    dateTimeZDAWidget,
                    [this, dateTimeZDAWidget]() { dateTimeZDAWidget->setUnixTime(this->generatorDateTimeWidget->UnixTime()); },
                    Qt::QueuedConnection, "setUnixTime, UnixTime");

                // Connect offset changes (generator widget -> widget)
                connectWhileSenderAlive(
                    generatorDateTimeWidget,
                    &GeneratorDateTime::LocalOffsetChanged,
                    dateTimeZDAWidget,
                    [this, dateTimeZDAWidget]() {
                    if (this->generatorDateTimeWidget->isEnabled()) {
                        dateTimeZDAWidget->setLZD(this->generatorDateTimeWidget->LocalOffset() / 60);
                        dateTimeZDAWidget->setLZMD(qAbs(this->generatorDateTimeWidget->LocalOffset() % 60));
                    }
                },
                    Qt::QueuedConnection, "setLZD, LocalOffset, setLZMD");
            }
        }
    }
    else {
        qWarning() << "PGN not supported for binding";
    }
}

void MainController::addGeneratorWidget(const QString &selectedItem)
{
    // Nested creation may run without a combo-box sender (e.g. layout restoration).
    const QRegularExpression pattern(R"(^(.+) \(PGN (-[0-9]+)\)$)");
    const auto match = pattern.match(selectedItem);
    if (match.hasMatch())
        addWidgetAsComboboxTextChanged(match.captured(2), match.captured(1), "Generate");
}

void MainController::EnableNewWidgets(bool isEnable)
{
    isNewWidgetsEnabled = isEnable;
}

void MainController::setUserPrefs(UserUnitPrefs userPrefs)
{
    this->userPrefs = userPrefs;
    for (const auto &group : groupWidgetsMap) for (auto *widget : group)
        if (auto *base = qobject_cast<PgnWidgetBase*>(widget)) base->setUserPrefs(userPrefs);
}

void MainController::setMyVessel(const MyVessel &vessel)
{
    m_electrical->configure(vessel.Electrical);
    myVessel = vessel;
    for(const auto &group:groupWidgetsMap) for(auto *widget:group)
        if(auto *movement=qobject_cast<GeneratorVesselMovement*>(widget)) movement->setEnvironmentalInfluence(vessel.EnvironmentalInfluence);
    updateAisVesselMovement();
}

void MainController::setUserOptionPrefs(const UserOptionPrefs &userOptionPrefs)
{
    this->userOptionPrefs = userOptionPrefs;
    for (const auto &group : groupWidgetsMap) for (auto *widget : group)
        if (auto *frame = qobject_cast<QFrame*>(widget->parentWidget())) frame->setLineWidth(userOptionPrefs.borderWidth);
}

void MainController::addRcvdPgn(const QString &pgn, const QStringList &paramList, const QString &source)
{
    QScopedValueRollback<bool> receiving(m_receivingWidget, true);
    PgnAsCombobox pgnAsCombobox = createPgnAsCombobox(pgn);

    if (pgnAsCombobox.pgn.isEmpty()) {
        qWarning() << "Received PGN not supported";
        return;
    }

    // Add the widget using same code as comboxbox text changed
    addWidgetAsComboboxTextChanged(pgn, pgnAsCombobox.label, pgnAsCombobox.group, paramList);
    // The initial received sample is authoritative, unlike widget initialization.
    pgnTableController->getPgnTableModel()->changeParams(idX,paramList);
    // Set the source of the PGN
    pgnTableController->getPgnTableModel()->setSource(idX, source);

}

void MainController::updateRcvdPgn(int idX, const QStringList &paramList)
{
    pgnTableController->getPgnTableModel()->changeParams(idX, paramList);
    if(currentIdX == idX) {
        pgnTableController->refreshPgnParameters(idX);
    }
}

void MainController::addWidgetAsComboboxTextChanged(const QString &selectedItemPgn, const QString &selectedItemLabel, const QString &groupName, const QStringList &paramList)
{
    auto *timestampModel=pgnTableController->getPgnTableModel();
    timestampModel->beginParameterInitialization(timestampModel->getMaxIdX()+1);
    const auto timestampInitialization=qScopeGuard([timestampModel]{timestampModel->endParameterInitialization();});

    const auto finalize = qScopeGuard([this] {
        for (const auto &group : groupWidgetsMap) for (auto *widget : group) prepareWidget(widget);
        rebuildGeneratorBindings();
        if (m_mode == Viewer) suspendGenerators(true);
        m_isWidgetsLoaded = !pgnTableController->getPgnTableModel()->getPgnList().isEmpty();
    });

    // Ensure that the group exists in `groupWidgetsMap` before adding a new widget
    // This map is used for saving and restoring the layout state
    if (!groupWidgetsMap.contains(groupName)) {
        groupWidgetsMap.insert(groupName, QList<QWidget*>());
    }

    if(const auto *d=Expansion::definition(selectedItemPgn)) {
        if(groupName!=d->group || (!paramList.isEmpty()&&!Expansion::validParameters(selectedItemPgn,paramList)))return;
        idX++;currentIdX=idX;auto rows=Expansion::parameters(selectedItemPgn);
        for(int i=0;i+1<paramList.size();i+=2)for(auto &row:rows)if(row[0]==paramList[i])row[1]=paramList[i+1];
        QList<QPair<double,double>> ranges;for(const auto &f:d->fields)ranges.append({f.minimum,f.maximum});
        pgnTableController->addPgn(idX,selectedItemLabel,selectedItemPgn,userOptionPrefs.newWidgetsEnabled,rows,ranges,1000);
        auto *instrument=ExpansionInstrument::create(idX,selectedItemLabel,selectedItemPgn,mdiArea);
        instrument->setWindowTitle(selectedItemLabel);instrument->setUserPrefs(userPrefs);instrument->setEnabled(userOptionPrefs.newWidgetsEnabled);
        addWidgetToSubForm(instrument,groupName);groupWidgetsMap[groupName].append(instrument);
        bindWidgetToModel(instrument,d->id,timestampModel,idX);initializeWidgetFromModel(instrument,d->id,timestampModel,idX);
        return;
    }
    if (groupName == "Environment") {
        //qDebug() << "Environment sub-window, " + selectedItem + " item selected";

        if (selectedItemPgn == "130306" || selectedItemPgn == "130313") {
            ++idX; currentIdX = idX;
            const int pgn = selectedItemPgn.toInt();
            auto rows = EnvironmentalInstrument::parameters(pgn);
            for (int i=0; i+1<paramList.size(); i+=2)
                for (auto &row : rows) if (row[0] == paramList[i]) row[1] = paramList[i+1];
            QList<QPair<double,double>> ranges;
            for (int i = 0; i < rows.size(); ++i) ranges.append({N2kDoubleNA, 1e12});
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, rows, ranges, 2000);
            EnvironmentalInstrument *instrument = pgn == 130306
                ? static_cast<EnvironmentalInstrument*>(new WindData130306(idX, selectedItemLabel, mdiArea))
                : static_cast<EnvironmentalInstrument*>(new Humidity130313(idX, selectedItemLabel, mdiArea));
            instrument->setWindowTitle(selectedItemLabel); instrument->setUserPrefs(userPrefs);
            instrument->setEnabled(userOptionPrefs.newWidgetsEnabled);
            addWidgetToSubForm(instrument, groupName); groupWidgetsMap[groupName].append(instrument);
            bindWidgetToModel(instrument, pgn, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(instrument, pgn, pgnTableController->getPgnTableModel(), idX);
            // Seed only new Generate widgets; Layout restoration and Viewer keep their own values.
            if (paramList.isEmpty() && m_mode == Sender && generatorWeatherWidget && generatorWeatherWidget->isEnabled()) {
                if (pgn == 130306) {
                    instrument->setValue("WindSpeed", generatorWeatherWidget->WindSpeed());
                    instrument->setValue("WindAngle", generatorWeatherWidget->WindAngle());
                } else if (generatorWeatherWidget->HumiditySource() == N2khs_OutsideHumidity)
                    instrument->setValue("ActualHumidity", generatorWeatherWidget->Humidity());
            }
        } else if (selectedItemPgn == "130310") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters130310 = {{"SID", "0", "", "unsigned char"},
                                                      {"OutsideAmbientAirTemperature", QString::number(N2kDoubleNA, 'g', 17), N2kConst::K, "double"},
                                                      {"WaterTemperature", QString::number(N2kDoubleNA, 'g', 17), N2kConst::K, "double"},
                                                      {"AtmosphericPressure", QString::number(N2kDoubleNA, 'g', 17), N2kConst::PA, "double"}};

            // If paramList has values, update the default values
            for (int i = 0; i + 1 < paramList.size(); i += 2)
                for (auto &row : parameters130310)
                    if (row[0] == paramList[i]) row[1] = paramList[i + 1];

            QList<QPair<double, double>> ranges130310 = {{-273.15, 1000.0}, {-273.15, 1000.0}, {800.0, 1100.0}};

            // Add PGN to both models
            normalizeAtmosphericPressure(parameters130310, ranges130310);
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters130310, ranges130310, 5000);

            // TODO: Implement the following logic once tree view is implemented
            //pgnTreeController->addPgn(idX, selectedItemLabel, selectedItemPgn);

            // Create and add the OutsideEnvironmental130310 to the layout
            OutsideEnvironmental130310 *outsideEnvironmentalWidget = new OutsideEnvironmental130310(idX, selectedItemLabel, this->mdiArea);
            outsideEnvironmentalWidget->setId(idX);
            outsideEnvironmentalWidget->setWindowTitle(selectedItemLabel);
            outsideEnvironmentalWidget->setToolTip(selectedItemLabel);
            outsideEnvironmentalWidget->setUserPrefs(userPrefs);
            outsideEnvironmentalWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(outsideEnvironmentalWidget, groupName);
            groupWidgetsMap[groupName].append(outsideEnvironmentalWidget);

            // Bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(outsideEnvironmentalWidget, 130310, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(outsideEnvironmentalWidget, 130310, pgnTableController->getPgnTableModel(), idX);
            if (paramList.isEmpty() && m_mode == Sender && generatorWeatherWidget && generatorWeatherWidget->isEnabled()) {
                outsideEnvironmentalWidget->setWaterTemperature(generatorWeatherWidget->SeaTemperature());
                outsideEnvironmentalWidget->setOutsideAmbientAirTemperature(generatorWeatherWidget->AirTemperature());
                outsideEnvironmentalWidget->setAtmosphericPressure(generatorWeatherWidget->AtmosphericPressure());
            }

        } else if (selectedItemPgn == "130312") {
            int tempSourceIndex = 0;
            if (m_mode == Sender) {
                // Ask the user to select a temperature source
                tempSourceIndex = UiHelper::ShowTemperatureSourcePicker("Select Temperature Source", "Please choose a temperature source:", 0);
                if (tempSourceIndex == -1) {
                    //User pressed Cancel, don't continue with adding widget to form.
                    return;
                }
            }

            idX++;
            currentIdX = idX;

            QList<QList<QString>> parameters130312 = {{"SID", "0", "", "unsigned char"},
                                                      {"TemperatureInstance", "0", "", "unsigned char"},
                                                      {"TemperatureSource", QString::number(tempSourceIndex), N2kConst::ENUM, "tN2kTempSource"},
                                                      {"ActualTemperature", "300.0", N2kConst::K, "double"},
                                                      {"SetTemperature", "300.0", N2kConst::K, "double"}};

            // If paramList has values, update the default values
            for (int i = 0; i + 1 < paramList.size(); i += 2)
                for (auto &row : parameters130312)
                    if (row[0] == paramList[i]) row[1] = paramList[i + 1];

            QList<QPair<double, double>> ranges130312 = {{0.0, 255.0}, {0.0, 255.0}, {0.0, 255.0}, {0.0, 1000.0}, {0.0, 1000.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters130312, ranges130312, 5000);

            // TODO: Implement the following logic once tree view is implemented
            //pgnTreeController->addPgn(idX, selectedItemLabel, selectedItemPgn);

            // Create and add the TemperatureWidget130312 to the layout
            TemperatureWidget130312 *temperatureWidget = new TemperatureWidget130312(idX, selectedItemLabel, this->mdiArea);
            temperatureWidget->setId(idX);
            temperatureWidget->setWindowTitle(selectedItemLabel);
            temperatureWidget->setToolTip(selectedItemLabel);
            temperatureWidget->setUserPrefs(userPrefs);
            temperatureWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(temperatureWidget, groupName);
            groupWidgetsMap[groupName].append(temperatureWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(temperatureWidget, 130312, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(temperatureWidget, 130312, pgnTableController->getPgnTableModel(), idX);
            if (paramList.isEmpty() && m_mode == Sender && generatorWeatherWidget && generatorWeatherWidget->isEnabled()) {
                const auto source = temperatureWidget->TemperatureSource();
                if (source == N2kts_SeaTemperature) temperatureWidget->setActualTemperature(generatorWeatherWidget->SeaTemperature());
                if (source == N2kts_OutsideTemperature) temperatureWidget->setActualTemperature(generatorWeatherWidget->AirTemperature());
                if (source == N2kts_DewPointTemperature) temperatureWidget->setActualTemperature(generatorWeatherWidget->DewPoint());
            }
            temperatureWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "130316") {
            int tempSourceIndex = 0;
            if (m_mode == Sender) {
                // Ask the user to select a temperature source
                tempSourceIndex = UiHelper::ShowTemperatureSourcePicker("Select Temperature Source", "Please choose a temperature source:", 0);
                if (tempSourceIndex == -1) {
                    //User pressed Cancel, don't continue with adding widget to form.
                    return;
                }
            }

            idX++;
            currentIdX = idX;

            // Get number of instances for the selected PGN
            int instance = pgnTableController->getPgnInstanceCount(selectedItemPgn);

            // Populate the pgn table model
            QList<QList<QString>> parameters130316 = {{"SID", "0", "", "unsigned char"},
                                                      {"TempInstance", QString::number(instance), "", "unsigned char"},
                                                      {"TempSource", QString::number(tempSourceIndex), N2kConst::ENUM, "tN2kTempSource"},
                                                      {"ActualTemperature", "300.0", N2kConst::K, "double"},
                                                      {"SetTemperature", "300.0", N2kConst::K, "double"}};

            // If paramList has values, update the default values
            for (int i = 0; i + 1 < paramList.size(); i += 2)
                for (auto &row : parameters130316)
                    if (row[0] == paramList[i]) row[1] = paramList[i + 1];

            QList<QPair<double, double>> ranges130316 = {{0.0, 255.0}, {0.0, 255.0}, {0.0, 255.0}, {0.0, 1000.0}, {0.0, 1000.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters130316, ranges130316, 5000);

            // Create and add the TemperatureWidget130316 to the layout
            TemperatureExtended130316 *temperatureExtendedWidget = new TemperatureExtended130316(idX, selectedItemLabel, this->mdiArea);
            temperatureExtendedWidget->setId(idX);
            temperatureExtendedWidget->setWindowTitle(selectedItemLabel);
            temperatureExtendedWidget->setToolTip(selectedItemLabel);
            temperatureExtendedWidget->setUserPrefs(userPrefs);
            temperatureExtendedWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(temperatureExtendedWidget, groupName);
            groupWidgetsMap[groupName].append(temperatureExtendedWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(temperatureExtendedWidget, 130316, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(temperatureExtendedWidget, 130316, pgnTableController->getPgnTableModel(), idX);
            if (paramList.isEmpty() && m_mode == Sender && generatorWeatherWidget && generatorWeatherWidget->isEnabled()) {
                const auto source = temperatureExtendedWidget->TempSource();
                if (source == N2kts_SeaTemperature) temperatureExtendedWidget->setActualTemperature(generatorWeatherWidget->SeaTemperature());
                if (source == N2kts_OutsideTemperature) temperatureExtendedWidget->setActualTemperature(generatorWeatherWidget->AirTemperature());
                if (source == N2kts_DewPointTemperature) temperatureExtendedWidget->setActualTemperature(generatorWeatherWidget->DewPoint());
            }
            temperatureExtendedWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "000000") {
            idX++;
            currentIdX = idX;
            // Ask user number of rows to span
            int rowSpan = UiHelper::ShowSpinBoxDialog("Select Spacer Row Span", "Please choose number of rows to span:", 1, 20, 3);

            // Populate the pgn table model
            QList<QList<QString>> parameters000000 = {{"Rows", QString::number(rowSpan), "", "int"},
                                                      {"ShowTitleRow", "true", "", "bool"}};
            QList<QPair<double, double>> ranges000000 = {{1.0, 20.0}, {0.0, 1.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, "0", true, parameters000000, ranges000000, 0);

            // Create and add the SpacerWidget to the layout
            Spacer000000 *spacerWidget = new Spacer000000(idX, selectedItemLabel, this->mdiArea);
            spacerWidget->setId(idX);
            spacerWidget->setWindowTitle(selectedItemLabel);
            spacerWidget->setToolTip(selectedItemLabel);

            addWidgetToSubForm(spacerWidget, groupName);
            groupWidgetsMap[groupName].append(spacerWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            spacerWidget->setInitialGuiValues();

        } else {
            qWarning() << "Unknown PGN:" << selectedItemPgn;
        }
    } else if (groupName == "Propulsion") {
        int engineInstance = 0;
        if (selectedItemPgn == "127488") {
            if (m_mode == Sender) {
                // Get number of instances for the selected PGN
                int instance = pgnTableController->getPgnInstanceCount(selectedItemPgn);
                // Ask the user to select an engine instance
                engineInstance = UiHelper::ShowSpinBoxDialog("Select Engine (1-8)", "Please choose engine number:", 1, 8, instance + 1);
                if (engineInstance != -1) {
                    engineInstance -= 1;
                } else {
                    //User pressed Cancel, don't continue with adding widget to form.
                    return;
                }
            }

            idX++;
            currentIdX = idX;
            // Populate the pgn table model
            QList<QList<QString>> parameters127488 = {{"EngineInstance", QString::number(engineInstance), "", "unsigned char"},
                                                      {"EngineSpeed", "0.0", N2kConst::RPM, "double"},
                                                      {"EngineBoostPressure", "0.0", N2kConst::PA, "double"},
                                                      {"EngineTiltTrim", "0", N2kConst::DEG, "char"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127488.size(); i++) {
                    if (parameters127488[i][0] == paramList[i * 2]) {
                        parameters127488[i][1] = paramList[i * 2 + 1];
                    }
                }
            }



            QList<QPair<double, double>> ranges127488 = {{0.0, 16383.25}, {0.0, 6553300.0}, {0.0, 100.0}};

            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127488, ranges127488, 250);

            // TODO: Implement the following logic once tree view is implemented
            //pgnTreeController->addPgn(idX, selectedItemLabel, selectedItemPgn);

            // Create and add the EngineRapid127488 to the layout
            EngineRapid127488 *engineRapidWidget = new EngineRapid127488(idX, selectedItemLabel, this->mdiArea);
            engineRapidWidget->setId(idX);
            engineRapidWidget->setWindowTitle(selectedItemLabel);
            engineRapidWidget->setToolTip(selectedItemLabel);
            engineRapidWidget->setUserPrefs(userPrefs);
            engineRapidWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(engineRapidWidget, groupName);
            groupWidgetsMap[groupName].append(engineRapidWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(engineRapidWidget, 127488, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(engineRapidWidget, 127488, pgnTableController->getPgnTableModel(), idX);
            engineRapidWidget->setInitialGuiValues();

            // Add EngineRapidWidget to the map
            engineRapids.insert(engineInstance, qMakePair(engineRapidWidget, false));

            if (m_mode == Sender) {
                // Bind the EngineRapidWidget to the EngineGenerator
                bindWidgetsBasedOnEngineInstanceToGenerator(engineInstance);
            }

        } else if (selectedItemPgn == "127489") {
            int engineInstance = 0;
            if (m_mode == Sender) {
                // Get number of instances for the selected PGN
                int instance = pgnTableController->getPgnInstanceCount(selectedItemPgn);
                // Ask the user to select an engine instance
                engineInstance = UiHelper::ShowSpinBoxDialog("Select Engine (1-8)", "Please choose engine number:", 1, 8, instance + 1);
                if (engineInstance != -1) {
                    engineInstance -= 1;
                } else {
                    //User pressed Cancel, don't continue with adding widget to form.
                    return;
                }
            }

            idX++;
            currentIdX = idX;
            // Populate the pgn table model
            QList<QList<QString>> parameters127489 = {{"EngineInstance", QString::number(engineInstance), "", "unsigned char"},
                                                      {"EngineOilPress", "0.0", N2kConst::PA, "double"},
                                                      {"EngineOilTemp", "0.0", N2kConst::K, "double"},
                                                      {"EngineCoolantTemp", "0.0", N2kConst::K, "double"},
                                                      {"AltenatorVoltage", "0.0", N2kConst::V, "double"},
                                                      {"FuelRate", "0.0", N2kConst::LPH, "double"},
                                                      {"EngineHours", "0.0", N2kConst::S, "double"},
                                                      {"EngineCoolantPress", "0.0", N2kConst::PA, "double"},
                                                      {"EngineFuelPress", "0.0", N2kConst::PA, "double"},
                                                      {"EngineLoad", "0", N2kConst::PCT, "char"},
                                                      {"EngineTorque", "0", N2kConst::PCT, "char"},
                                                      {"Status1", "0", N2kConst::ENUM, "tN2kEngineDiscreteStatus1"},
                                                      {"Status2", "0", N2kConst::ENUM, "tN2kEngineDiscreteStatus2"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127489.size(); i++) {
                    if (parameters127489[i][0] == paramList[i * 2]) {
                        parameters127489[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127489 = {{0.0, 16383.25}, {0.0, 6553300.0}, {0.0, 100.0}};

            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127489, ranges127489, 1000);

            // TODO: Implement the following logic once tree view is implemented
            //pgnTreeController->addPgn(idX, selectedItemLabel, selectedItemPgn);

            // Create and add the EngineDynamic127489 to the layout
            EngineDynamic127489 *engineDynamicWidget = new EngineDynamic127489(idX, selectedItemLabel, this->mdiArea);
            engineDynamicWidget->setId(idX);
            engineDynamicWidget->setWindowTitle(selectedItemLabel);
            engineDynamicWidget->setToolTip(selectedItemLabel);
            engineDynamicWidget->setUserPrefs(userPrefs);
            engineDynamicWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(engineDynamicWidget, groupName);
            groupWidgetsMap[groupName].append(engineDynamicWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(engineDynamicWidget, 127489, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(engineDynamicWidget, 127489, pgnTableController->getPgnTableModel(), idX);
            engineDynamicWidget->setInitialGuiValues();

            // Add EngineDynamicWidget to the map
            engineDynamics.insert(engineInstance, qMakePair(engineDynamicWidget, false));

            if (m_mode == Sender) {
                // Bind the EngineDynamicWidget to the EngineGenerator
                bindWidgetsBasedOnEngineInstanceToGenerator(engineInstance);
            }

        } else if (selectedItemPgn == "127493") {
            int transmissionInstance = 0;
            if (m_mode == Sender) {
                // Get number of instances for the selected PGN
                int instance = pgnTableController->getPgnInstanceCount(selectedItemPgn);
                // Ask the user to select a transmission instance
                transmissionInstance = UiHelper::ShowSpinBoxDialog("Select Engine Transmission (1-8)", "Please choose transmission number:", 1, 8, instance + 1);
                if (transmissionInstance != -1) {
                    transmissionInstance -= 1;
                } else {
                    //User pressed Cancel, don't continue with adding widget to form.
                    return;
                }

                // Check that the transmission instance is not already in use
                if (transmissionDynamics.contains(transmissionInstance)) {
                    QMessageBox::warning(nullptr, "Transmission Instance Already In Use", "Transmission instance already in use. Please choose another instance.");

                    return;
                }
            }

            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters127493 = {{"EngineInstance", QString::number(transmissionInstance), "", "unsigned char"},
                                                      {"TransmissionGear", "1", N2kConst::ENUM, "tN2kTransmissionGear"},
                                                      {"OilPressure", "0.0", N2kConst::PA, "double"},
                                                      {"OilTemperature", "295.372", N2kConst::K, "double"},
                                                      {"DiscreteStatus1", "0", N2kConst::ENUM, "unsigned char"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127493.size(); i++) {
                    if (parameters127493[i][0] == paramList[i * 2]) {
                        parameters127493[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127493 = {{0.0, 255.0}, {0.0, 1000.0}, {0.0, 1000.0}, {0.0, 255.0}};

            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127493, ranges127493, 500);

            // Create and add the TransmissionDynamic127493 to the layout
            TransmissionDynamic127493 *transmissionDynamicWidget = new TransmissionDynamic127493(idX, selectedItemLabel, this->mdiArea);
            transmissionDynamicWidget->setId(idX);
            transmissionDynamicWidget->setWindowTitle(selectedItemLabel);
            transmissionDynamicWidget->setToolTip(selectedItemLabel);
            transmissionDynamicWidget->setUserPrefs(userPrefs);
            transmissionDynamicWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(transmissionDynamicWidget, groupName);
            groupWidgetsMap[groupName].append(transmissionDynamicWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(transmissionDynamicWidget, 127493, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(transmissionDynamicWidget, 127493, pgnTableController->getPgnTableModel(), idX);
            transmissionDynamicWidget->setInitialGuiValues();

            // Add the TransmissionDynamicWidget to the pair list of transmissionDynamics
            transmissionDynamics.insert(transmissionInstance, qMakePair(transmissionDynamicWidget, false));

            if (m_mode == Sender) {
                // Bind the TransmissionDynamicWidget to the GeneratorTransLowerUnit widget
                bindWidgetsBasedOnEngineInstanceToGenerator(transmissionInstance);
            }
        } else if (selectedItemPgn == "127245") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters127245 = {{"RudderPosition", "0.0", N2kConst::RAD, "double"},
                                                      {"Instance", "0", "", "unsigned char"},
                                                      {"RudderDirectionOrder", "0", N2kConst::ENUM, "tN2kRudderDirectionOrder"},
                                                      {"AngleOrder", "0.0", N2kConst::RAD, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127245.size(); i++) {
                    if  (parameters127245[i][0] == paramList[i * 2]) {
                        parameters127245[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127245 = {{0.0, 255.0}, {-3.14159, 3.14159}, {0.0, 255.0}, {0.0, 255.0}, {-3.14159, 3.14159}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127245, ranges127245, 500);

            // Create and add the Rudder127245 to the layout
            Rudder127245 *rudderWidget = new Rudder127245(idX, selectedItemLabel, this->mdiArea);
            rudderWidget->setId(idX);
            rudderWidget->setWindowTitle(selectedItemLabel);
            rudderWidget->setToolTip(selectedItemLabel);
            rudderWidget->setUserPrefs(userPrefs);
            rudderWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(rudderWidget, groupName);
            groupWidgetsMap[groupName].append(rudderWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(rudderWidget, 127245, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(rudderWidget, 127245, pgnTableController->getPgnTableModel(), idX);
            rudderWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "130576") {
            idX++;
            currentIdX = idX;

            QList<QList<QString>> parameters130576 = {{"PortTrimTab", "0", N2kConst::PCT, "char"},
                                                      {"StbdTrimTab", "0", N2kConst::PCT, "char"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters130576.size(); i++) {
                    if  (parameters130576[i][0] == paramList[i * 2]) {
                        parameters130576[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges130576 = {{0.0, 100.0}, {0.0, 100.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters130576, ranges130576, 500);

            // Create and add the TrimTab130576 to the layout
            TrimTab130576 *trimTabWidget = new TrimTab130576(idX, selectedItemLabel, this->mdiArea);
            trimTabWidget->setId(idX);
            trimTabWidget->setWindowTitle(selectedItemLabel);
            trimTabWidget->setToolTip(selectedItemLabel);
            trimTabWidget->setUserPrefs(userPrefs);
            trimTabWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(trimTabWidget, groupName);
            groupWidgetsMap[groupName].append(trimTabWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(trimTabWidget, 130576, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(trimTabWidget, 130576, pgnTableController->getPgnTableModel(), idX);
            trimTabWidget->setInitialGuiValues();
        }
        else if (selectedItemPgn == "000000") {
            idX++;
            currentIdX = idX;
            // Ask user number of rows to span
            int rowSpan = UiHelper::ShowSpinBoxDialog("Select Spacer Row Span", "Please choose number of rows to span:", 1, 20, 3);

            // Populate the pgn table model
            QList<QList<QString>> parameters000000 = {{"Rows", QString::number(rowSpan), "", "int"},
                                                      {"ShowTitleRow", "true", "", "bool"}};
            QList<QPair<double, double>> ranges000000 = {{1.0, 20.0}, {0.0, 1.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, "0", true, parameters000000, ranges000000, 0);

            // Create and add the SpacerWidget to the layout
            Spacer000000 *spacerWidget = new Spacer000000(idX, selectedItemLabel, this->mdiArea);
            spacerWidget->setId(idX);
            spacerWidget->setWindowTitle(selectedItemLabel);
            spacerWidget->setToolTip(selectedItemLabel);

            addWidgetToSubForm(spacerWidget, groupName);
            groupWidgetsMap[groupName].append(spacerWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            spacerWidget->setInitialGuiValues();

        } else {
            qWarning() << "Unknown PGN:" << selectedItemPgn;
        }
    } else if (groupName == "Navigation") {
        if (selectedItemPgn == "126992") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters126992 = {{"SID", "0", "", "unsigned char"},
                                                      {"SystemDate", "0", N2kConst::DAY, "uint16_t"},
                                                      {"SystemTime", "0.0", N2kConst::S, "double"},
                                                      {"TimeSource", "0", N2kConst::ENUM, "tN2kTimeSource"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters126992.size(); i++) {
                    if  (parameters126992[i][0] == paramList[i * 2]) {
                        parameters126992[i][1] = paramList[i * 2 + 1];
                    }
                }
            }


            QList<QPair<double, double>> ranges126992 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters126992, ranges126992);

            // Create and add the SystemTime126992 to the layout
            SystemTime126992 *systemTimeWidget = new SystemTime126992(idX, selectedItemLabel, this->mdiArea);
            systemTimeWidget->setId(idX);
            systemTimeWidget->setWindowTitle(selectedItemLabel);
            systemTimeWidget->setToolTip(selectedItemLabel);
            systemTimeWidget->setUserPrefs(userPrefs);
            systemTimeWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(systemTimeWidget, groupName);
            groupWidgetsMap[groupName].append(systemTimeWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(systemTimeWidget, 126992, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(systemTimeWidget, 126992, pgnTableController->getPgnTableModel(), idX);
        } else if (selectedItemPgn == "127250") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters127250 = {{"SID", "0", "", "unsigned char"},
                                                      {"Heading", "0.0", N2kConst::RAD, "double"},
                                                      {"Deviation", "0.0", N2kConst::RAD, "double"},
                                                      {"Variation", "0.0", N2kConst::RAD, "double"},
                                                      {"HeadingReference", "0", N2kConst::ENUM, "tN2kHeadingReference"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127250.size(); i++) {
                    if  (parameters127250[i][0] == paramList[i * 2]) {
                        parameters127250[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127250 = {{0.0, 255.0}, {0.0, 360.0}, {0.0, 360.0}, {0.0, 360.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127250, ranges127250, 500);

            // Create and add the VesselHeading127250 to the layout
            VesselHeading127250 *vesselHeadingWidget = new VesselHeading127250(idX, selectedItemLabel, this->mdiArea);
            vesselHeadingWidget->setId(idX);
            vesselHeadingWidget->setWindowTitle(selectedItemLabel);
            vesselHeadingWidget->setToolTip(selectedItemLabel);
            vesselHeadingWidget->setUserPrefs(userPrefs);
            vesselHeadingWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(vesselHeadingWidget, groupName);
            groupWidgetsMap[groupName].append(vesselHeadingWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(vesselHeadingWidget, 127250, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(vesselHeadingWidget, 127250, pgnTableController->getPgnTableModel(), idX);
            vesselHeadingWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "127251") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters127251 = {{"SID", "0", "", "unsigned char"},
                                                      {"RateOfTurn", "0.0", "radians/s", "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127251.size(); i++) {
                    if  (parameters127251[i][0] == paramList[i * 2]) {
                        parameters127251[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127251 = {{0.0, 255.0}, {-3.14159, 3.14159}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127251, ranges127251, 500);

            // Create and add the RateOfTurn127251 to the layout
            RateOfTurn127251 *rateOfTurnWidget = new RateOfTurn127251(idX, selectedItemLabel, this->mdiArea);
            rateOfTurnWidget->setId(idX);
            rateOfTurnWidget->setWindowTitle(selectedItemLabel);
            rateOfTurnWidget->setToolTip(selectedItemLabel);
            rateOfTurnWidget->setUserPrefs(userPrefs);
            rateOfTurnWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(rateOfTurnWidget, groupName);
            groupWidgetsMap[groupName].append(rateOfTurnWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(rateOfTurnWidget, 127251, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(rateOfTurnWidget, 127251, pgnTableController->getPgnTableModel(), idX);
            rateOfTurnWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "127252") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters127252 = {{"SID", "0", "", "unsigned char"},
                                                      {"Heave", "0.0", N2kConst::M, "double"},
                                                      {"Delay", "0", N2kConst::S, "double"},
                                                      {"DelaySource", QString::number(N2kDD374_UserDefined), N2kConst::ENUM, "tN2kDelaySource"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127252.size(); i++) {
                    if  (parameters127252[i][0] == paramList[i * 2]) {
                        parameters127252[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127252 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127252, ranges127252);

            // Create and add the Heave127252 to the layout
            Heave127252 *heaveWidget = new Heave127252(idX, selectedItemLabel, this->mdiArea);
            heaveWidget->setId(idX);
            heaveWidget->setWindowTitle(selectedItemLabel);
            heaveWidget->setToolTip(selectedItemLabel);
            heaveWidget->setUserPrefs(userPrefs);
            heaveWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(heaveWidget, groupName);
            groupWidgetsMap[groupName].append(heaveWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(heaveWidget, 127252, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(heaveWidget, 127252, pgnTableController->getPgnTableModel(), idX);
            heaveWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "127257") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters127257 = {{"SID", "0", "", "unsigned char"},
                                                      {"Yaw", "0.0", N2kConst::RAD, "double"},
                                                      {"Pitch", "0.0", N2kConst::RAD, "double"},
                                                      {"Roll", "0.0", N2kConst::RAD, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127257.size(); i++) {
                    if  (parameters127257[i][0] == paramList[i * 2]) {
                        parameters127257[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127257 = {{0.0, 255.0}, {-3.14159, 3.14159}, {-3.14159, 3.14159}, {-3.14159, 3.14159}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127257, ranges127257);

            // Create and add the Attitude127257 to the layout
            Attitude127257 *attitudeWidget = new Attitude127257(idX, selectedItemLabel, this->mdiArea);
            attitudeWidget->setId(idX);
            attitudeWidget->setWindowTitle(selectedItemLabel);
            attitudeWidget->setToolTip(selectedItemLabel);
            attitudeWidget->setUserPrefs(userPrefs);
            attitudeWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(attitudeWidget, groupName);
            groupWidgetsMap[groupName].append(attitudeWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(attitudeWidget, 127257, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(attitudeWidget, 127257, pgnTableController->getPgnTableModel(), idX);
            attitudeWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "127258") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters127258 = {{"SID", "0", "", "unsigned char"},
                                                      {"Source", "0", N2kConst::ENUM, "tN2kMagneticVariation"},
                                                      {"DaysSince1970", "0", N2kConst::DAY, "uint16_t"},
                                                      {"Variation", "-0.147", N2kConst::RAD, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127258.size(); i++) {
                    if  (parameters127258[i][0] == paramList[i * 2]) {
                        parameters127258[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127258 = {{0.0, 255.0}, {0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127258, ranges127258);

            // Create and add the MagneticVariation127258 to the layout
            MagneticVariation127258 *magneticVariationWidget = new MagneticVariation127258(idX, selectedItemLabel, this->mdiArea);
            magneticVariationWidget->setId(idX);
            magneticVariationWidget->setWindowTitle(selectedItemLabel);
            magneticVariationWidget->setToolTip(selectedItemLabel);
            magneticVariationWidget->setUserPrefs(userPrefs);
            magneticVariationWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(magneticVariationWidget, groupName);
            groupWidgetsMap[groupName].append(magneticVariationWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(magneticVariationWidget, 127258, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(magneticVariationWidget, 127258, pgnTableController->getPgnTableModel(), idX);
            magneticVariationWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "128259") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters128259 = {{"SID", "0", "", "unsigned char"},
                                                      {"WaterReferenced", "0.0", N2kConst::MPS, "double"},
                                                      {"GroundReferenced", "0.0", N2kConst::MPS, "double"},
                                                      {"SWRT", "0", N2kConst::ENUM, "tN2kSpeedWaterReferencedType"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters128259.size(); i++) {
                    if  (parameters128259[i][0] == paramList[i * 2]) {
                        parameters128259[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges128259 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters128259, ranges128259);

            // Create and add the Speed128259 to the layout
            Speed128259 *speedWidget = new Speed128259(idX, selectedItemLabel, this->mdiArea);
            speedWidget->setId(idX);
            speedWidget->setWindowTitle(selectedItemLabel);
            speedWidget->setToolTip(selectedItemLabel);
            speedWidget->setUserPrefs(userPrefs);
            speedWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(speedWidget, groupName);
            groupWidgetsMap[groupName].append(speedWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(speedWidget, 128259, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(speedWidget, 128259, pgnTableController->getPgnTableModel(), idX);
            speedWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "128267") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters128267 = {{"SID", "0", "", "unsigned char"},
                                                      {"DepthBelowTransducer", "0.0", N2kConst::M, "double"},
                                                      {"Offset", "0.0", N2kConst::M, "double"},
                                                      {"Range", "0.0", N2kConst::M, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters128267.size(); i++) {
                    if  (parameters128267[i][0] == paramList[i * 2]) {
                        parameters128267[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges128267 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters128267, ranges128267);

            // Create and add the Depth129267 to the layout
            Depth128267 *depthWidget = new Depth128267(idX, selectedItemLabel, this->mdiArea);
            depthWidget->setId(idX);
            depthWidget->setWindowTitle(selectedItemLabel);
            depthWidget->setToolTip(selectedItemLabel);
            depthWidget->setUserPrefs(userPrefs);
            depthWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(depthWidget, groupName);
            groupWidgetsMap[groupName].append(depthWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(depthWidget, 128267, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(depthWidget, 128267, pgnTableController->getPgnTableModel(), idX);
            depthWidget->setInitialGuiValues();

        } else if (selectedItemPgn == "129025") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters129025 = {{"Latitude", QString::number(myVessel.Latitude, 'f', 6), "degrees", "double"},
                                                      {"Longitude", QString::number(myVessel.Longitude, 'f', 6), "degrees", "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters129025.size(); i++) {
                    if (parameters129025[i][0] == paramList[i * 2]) {
                        parameters129025[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges129025 = {{-3.14159, 3.14159}, {-3.14159, 3.14159}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters129025, ranges129025, 250);

            // Create and add the LatLong129025 to the layout
            LatLong129025 *latLongWidget = new LatLong129025(idX, selectedItemLabel, this->mdiArea);
            latLongWidget->setId(idX);
            latLongWidget->setWindowTitle(selectedItemLabel);
            latLongWidget->setToolTip(selectedItemLabel);
            latLongWidget->setUserPrefs(userPrefs);
            latLongWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(latLongWidget, groupName);
            groupWidgetsMap[groupName].append(latLongWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(latLongWidget, 129025, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(latLongWidget, 129025, pgnTableController->getPgnTableModel(), idX);
            latLongWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "129026") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters129026 = {{"SID", "0", "", "unsigned char"},
                                                      {"HeadingReference", "0", N2kConst::ENUM, "tN2kHeadingReference"},
                                                      {"COG", "0.0", N2kConst::RAD, "double"},
                                                      {"SOG", "0.0", N2kConst::MPS, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters129026.size(); i++) {
                    if (parameters129026[i][0] == paramList[i * 2]) {
                        parameters129026[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges129026 = {{0.0, 255.0}, {0.0, 360.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters129026, ranges129026, 250);

            // Create and add the CogSog129026 to the layout
            CogSog129026 *cogSogWidget = new CogSog129026(idX, selectedItemLabel, this->mdiArea);
            cogSogWidget->setId(idX);
            cogSogWidget->setWindowTitle(selectedItemLabel);
            cogSogWidget->setToolTip(selectedItemLabel);
            cogSogWidget->setUserPrefs(userPrefs);
            cogSogWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(cogSogWidget, groupName);
            groupWidgetsMap[groupName].append(cogSogWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(cogSogWidget, 129026, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(cogSogWidget, 129026, pgnTableController->getPgnTableModel(), idX);
            cogSogWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "129029") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters129029 = {{"SID", "1", "", "unsigned char"},
                                                      {"DaysSince1970", "0", N2kConst::DAY, "uint16_t"},
                                                      {"SecondsSinceMidnight", "0.0", N2kConst::S, "double"},
                                                      {"Latitude", QString::number(myVessel.Latitude, 'f', 6), N2kConst::DEG, "double"},
                                                      {"Longitude", QString::number(myVessel.Longitude, 'f', 6), N2kConst::DEG, "double"},
                                                      {"Altitude", "3.2", N2kConst::M, "double"},
                                                      {"GNSStype", "0", N2kConst::ENUM, "tN2kGNSStype"},
                                                      {"GNSSmethod", "1", N2kConst::ENUM, "tN2kGNSSmethod"},
                                                      {"nSatellites", "11", "", "unsigned char"},
                                                      {"HDOP", "1.3", "", "double"},
                                                      {"PDOP", "1.3", "", "double"},
                                                      {"GeoidalSeparation", "15.5", N2kConst::M, "double"},
                                                      {"nReferenceStations", "2", "", "unsigned char"},
                                                      {"ReferenceStationType", "0", N2kConst::ENUM, "tN2kGNSStype"},
                                                      {"ReferenceSationID", "123", "", "uint16_t"},
                                                      {"AgeOfCorrection", "0.50", N2kConst::S, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters129029.size(); i++) {
                    if (parameters129029[i][0] == paramList[i * 2]) {
                        parameters129029[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges129029
                = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}, {-3.14159, 3.14159}, {-3.14159, 3.14159}, {0.0, 65535}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters129029, ranges129029);

            // Create and add the GnssPositionData129029 to the layout
            GnssPositionData129029 *gnssPositionDataWidget = new GnssPositionData129029(idX, selectedItemLabel, this->mdiArea);
            gnssPositionDataWidget->setId(idX);
            gnssPositionDataWidget->setWindowTitle(selectedItemLabel);
            gnssPositionDataWidget->setToolTip(selectedItemLabel);
            gnssPositionDataWidget->setUserPrefs(userPrefs);
            gnssPositionDataWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(gnssPositionDataWidget, groupName);
            groupWidgetsMap[groupName].append(gnssPositionDataWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(gnssPositionDataWidget, 129029, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(gnssPositionDataWidget, 129029, pgnTableController->getPgnTableModel(), idX);
            gnssPositionDataWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "129033") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters129033 = {{"DaysSince1970", "0", N2kConst::DAY, "uint16_t"},
                                                      {"SecondsSinceMidnight", "0.0", N2kConst::S, "double"},
                                                      {"LocalOffset", "0", N2kConst::MIN, "int16_t"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters129033.size(); i++) {
                    if (parameters129033[i][0] == paramList[i * 2]) {
                        parameters129033[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges129033 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters129033, ranges129033);

            // Create and add the DateTime129033 to the layout
            DateTime129033 *dateTimeWidget = new DateTime129033(idX, selectedItemLabel, this->mdiArea);
            dateTimeWidget->setId(idX);
            dateTimeWidget->setWindowTitle(selectedItemLabel);
            dateTimeWidget->setToolTip(selectedItemLabel);
            dateTimeWidget->setUserPrefs(userPrefs);
            dateTimeWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(dateTimeWidget, groupName);
            groupWidgetsMap[groupName].append(dateTimeWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(dateTimeWidget, 129033, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(dateTimeWidget, 129033, pgnTableController->getPgnTableModel(), idX);
        } else if (selectedItemPgn == "129283") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters129283 = {{"SID", "0", "", "unsigned char"},
                                                      {"XTEMode", "3", N2kConst::ENUM, "tN2kXTEMode"},
                                                      {"NavigationTerminated", "true", "", "bool"},
                                                      {"XTE", "0.0", N2kConst::M, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters129283.size(); i++) {
                    if (parameters129283[i][0] == paramList[i * 2]) {
                        parameters129283[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges129283 = {{0.0, 255.0}, {0.0, 255.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters129283, ranges129283);

            // Create and add the CrossTrack129283 to the layout
            CrossTrack129283 *crossTrackWidget = new CrossTrack129283(idX, selectedItemLabel, this->mdiArea);
            crossTrackWidget->setId(idX);
            crossTrackWidget->setWindowTitle(selectedItemLabel);
            crossTrackWidget->setToolTip(selectedItemLabel);
            crossTrackWidget->setUserPrefs(userPrefs);
            crossTrackWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(crossTrackWidget, groupName);
            groupWidgetsMap[groupName].append(crossTrackWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(crossTrackWidget, 129283, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(crossTrackWidget, 129283, pgnTableController->getPgnTableModel(), idX);
            crossTrackWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "129284") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters129284 = {{"SID", "0", "", "unsigned char"},
                                                      {"DistanceToWaypoint", "0.0", N2kConst::M, "double"},
                                                      {"BearingReference", "0", N2kConst::ENUM, "tN2kHeadingReference"},
                                                      {"PerpendicularCrossed", "false", "", "bool"},
                                                      {"ArrivalCircleEntered", "false", "", "bool"},
                                                      {"CalculationType", "0", N2kConst::ENUM, "tN2kDistanceCalculationType"},
                                                      {"ETATime", "0.0", N2kConst::S, "double"},
                                                      {"ETADate", "0", N2kConst::DAY, "int16_t"},
                                                      {"BearingOriginToDestinationWaypoint", "0.0", N2kConst::RAD, "double"},
                                                      {"BearingPositionToDestinationWaypoint", "0.0", N2kConst::RAD, "double"},
                                                      {"OriginWaypointNumber", "0", "", "uint32_t"},
                                                      {"DestinationWaypointNumber", "0", "", "uint32_t"},
                                                      {"DestinationLatitude", "0.0", N2kConst::DEG, "double"},
                                                      {"DestinationLongitude", "0.0", N2kConst::DEG, "double"},
                                                      {"WaypointClosingVelocity", "0.0", N2kConst::MPS, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters129284.size(); i++) {
                    if (parameters129284[i][0] == paramList[i * 2]) {
                        parameters129284[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges129284 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters129284, ranges129284);

            // Create and add the Navigation129284 to the layout
            Navigation129284 *navigationWidget = new Navigation129284(idX, selectedItemLabel, this->mdiArea);
            navigationWidget->setId(idX);
            navigationWidget->setWindowTitle(selectedItemLabel);
            navigationWidget->setToolTip(selectedItemLabel);
            navigationWidget->setUserPrefs(userPrefs);
            navigationWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(navigationWidget, groupName);
            groupWidgetsMap[groupName].append(navigationWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(navigationWidget, 129284, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(navigationWidget, 129284, pgnTableController->getPgnTableModel(), idX);
            navigationWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "GGA") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            auto parametersGGA = ggaParameters(myVessel.Latitude, myVessel.Longitude);
            // Received name/value pairs need not be ordered or complete.
            for (auto &parameter : parametersGGA)
                for (int i = 0; i + 1 < paramList.size(); i += 2)
                    if (parameter[0] == paramList[i]) parameter[1] = paramList[i + 1];
            const auto rangesGGA = ggaRanges();
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parametersGGA, rangesGGA, 1000);

            // Create and add the GgaWidget to the layout
            GlobalPositioningGGA *ggaWidget = new GlobalPositioningGGA(idX, selectedItemLabel, this->mdiArea);
            ggaWidget->setId(idX);
            ggaWidget->setWindowTitle(selectedItemLabel);
            ggaWidget->setToolTip(selectedItemLabel);
            ggaWidget->setUserPrefs(userPrefs);
            ggaWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(ggaWidget, groupName);
            groupWidgetsMap[groupName].append(ggaWidget);


            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(ggaWidget, SNT_GGA, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(ggaWidget, SNT_GGA, pgnTableController->getPgnTableModel(), idX);
            ggaWidget->setInitialGuiValues();

        } else if (selectedItemPgn == "RMC") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parametersRMC = {{"GPSTime", "0.0", N2kConst::S, "double"},
                                                      {"Status", "A", "", "char"},
                                                      {"Latitude", QString::number(myVessel.Latitude, 'f', 6), N2kConst::DEG, "double"},
                                                      {"Longitude", QString::number(myVessel.Longitude, 'f', 6), N2kConst::DEG, "double"},
                                                      {"TrueCOG", "0.0", N2kConst::RAD, "double"},
                                                      {"SOG", "0.0", N2kConst::MPS, "double"},
                                                      {"DaysSince1970", "0", N2kConst::DAY, "ulong"},
                                                      {"Variation", "0.0", N2kConst::RAD, "double"},
                                                      {"DateTime", "0", N2kConst::S, "time_t"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parametersRMC.size(); i++) {
                    if (parametersRMC[i][0] == paramList[i * 2]) {
                        parametersRMC[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> rangesRMC = {{0.0, 65535.0}, {0.0, 255.0}, {-3.14159, 3.14159}, {-3.14159, 3.14159}, {0.0, 65535.0}, {0.0, 360.0}, {0.0, 65535.0}, {0.0, 360.0}, {0.0, 255.0}, {0.0, 255.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parametersRMC, rangesRMC, 1000);

            // Create and add the RecommendedMinGnssRMCWidget to the layout
            RecommendedMinGnssRMC *recommendedMinGnssRmcWidget = new RecommendedMinGnssRMC(idX, selectedItemLabel, this->mdiArea);

            recommendedMinGnssRmcWidget->setId(idX);
            recommendedMinGnssRmcWidget->setWindowTitle(selectedItemLabel);
            recommendedMinGnssRmcWidget->setToolTip(selectedItemLabel);
            recommendedMinGnssRmcWidget->setUserPrefs(userPrefs);
            recommendedMinGnssRmcWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(recommendedMinGnssRmcWidget, groupName);
            groupWidgetsMap[groupName].append(recommendedMinGnssRmcWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(recommendedMinGnssRmcWidget, SNT_RMC, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(recommendedMinGnssRmcWidget, SNT_RMC, pgnTableController->getPgnTableModel(), idX);
            recommendedMinGnssRmcWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "HDG") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parametersHDG = {{"Heading", "0.0", N2kConst::RAD, "double"},
                                                      {"Deviation", "0.0", N2kConst::RAD, "double"},
                                                      {"Variation", "0.0", N2kConst::RAD, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parametersHDG.size(); i++) {
                    if (parametersHDG[i][0] == paramList[i * 2]) {
                        parametersHDG[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> rangesHDG = {{0.0, 6.283}, {-6.283, 6.283}, {-6.283, 6.283}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parametersHDG, rangesHDG, 1000);

            // Create and add the HeadingHDGWidget to the layout
            HeadingHDG *headingHDGWidget = new HeadingHDG(idX, selectedItemLabel, this->mdiArea);
            headingHDGWidget->setId(idX);
            headingHDGWidget->setWindowTitle(selectedItemLabel);
            headingHDGWidget->setToolTip(selectedItemLabel);
            headingHDGWidget->setUserPrefs(userPrefs);
            headingHDGWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(headingHDGWidget, groupName);
            groupWidgetsMap[groupName].append(headingHDGWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(headingHDGWidget, SNT_HDG, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(headingHDGWidget, SNT_HDG, pgnTableController->getPgnTableModel(), idX);
            headingHDGWidget->setInitialGuiValues();

        } else if (selectedItemPgn == "HDT") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parametersHDT = {{"Heading", "0.0", N2kConst::RAD, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parametersHDT.size(); i++) {
                    if (parametersHDT[i][0] == paramList[i * 2]) {
                        parametersHDT[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> rangesHDT = {{0.0, 6.283}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parametersHDT, rangesHDT, 1000);

            // Create and add the HeadingHDTWidget to the layout
            TrueHeadingHDT *headingHDTWidget = new TrueHeadingHDT(idX, selectedItemLabel, this->mdiArea);
            headingHDTWidget->setId(idX);
            headingHDTWidget->setWindowTitle(selectedItemLabel);
            headingHDTWidget->setToolTip(selectedItemLabel);
            headingHDTWidget->setUserPrefs(userPrefs);
            headingHDTWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(headingHDTWidget, groupName);
            groupWidgetsMap[groupName].append(headingHDTWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(headingHDTWidget, SNT_HDT, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(headingHDTWidget, SNT_HDT, pgnTableController->getPgnTableModel(), idX);
            headingHDTWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "GLL") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parametersGLL = {{"GPSTime", "0.0", N2kConst::S, "double"},
                                                      {"Latitude", QString::number(myVessel.Latitude, 'f', 6), N2kConst::DEG, "double"},
                                                      {"Longitude", QString::number(myVessel.Longitude, 'f', 6), N2kConst::DEG, "double"},
                                                      {"Status", "A", "", "char"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (auto &parameter : parametersGLL) {
                    for (int i = 0; i + 1 < paramList.size(); i += 2)
                        if (parameter[0] == paramList[i]) parameter[1] = paramList[i + 1];
                }
            }

            QList<QPair<double, double>> rangesGLL = {{0.0, 65535.0}, {-3.14159, 3.14159}, {-3.14159, 3.14159}, {0.0, 0.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parametersGLL, rangesGLL, 1000);

            // Create and add the GeographicPositionGLLWidget to the layout
            GeographicPositionGLL *geographicPositioWidget = new GeographicPositionGLL(idX, selectedItemLabel, this->mdiArea);
            geographicPositioWidget->setId(idX);
            geographicPositioWidget->setWindowTitle(selectedItemLabel);
            geographicPositioWidget->setToolTip(selectedItemLabel);
            geographicPositioWidget->setUserPrefs(userPrefs);
            geographicPositioWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(geographicPositioWidget, groupName);
            groupWidgetsMap[groupName].append(geographicPositioWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(geographicPositioWidget, SNT_GLL, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(geographicPositioWidget, SNT_GLL, pgnTableController->getPgnTableModel(), idX);
        } else if (selectedItemPgn == "VTG") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parametersVTG = {{"TrueCOG", "0.0", N2kConst::RAD, "double"},
                                                      {"MagneticCOG", "0.0", N2kConst::RAD, "double"},
                                                      {"SOG", "0.0", N2kConst::MPS, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parametersVTG.size(); i++) {
                    if (parametersVTG[i][0] == paramList[i * 2]) {
                        parametersVTG[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> rangesVTG = {{0.0, 6.283}, {0.0, 6.283}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parametersVTG, rangesVTG, 1000);

            // Create and add the CourseOverGroundVTGWidget to the layout
            CogSogVTG *courseOverGroundWidget = new CogSogVTG(idX, selectedItemLabel, this->mdiArea);
            courseOverGroundWidget->setId(idX);
            courseOverGroundWidget->setWindowTitle(selectedItemLabel);
            courseOverGroundWidget->setToolTip(selectedItemLabel);
            courseOverGroundWidget->setUserPrefs(userPrefs);
            courseOverGroundWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(courseOverGroundWidget, groupName);
            groupWidgetsMap[groupName].append(courseOverGroundWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(courseOverGroundWidget, SNT_VTG, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(courseOverGroundWidget, SNT_VTG, pgnTableController->getPgnTableModel(), idX);
        } else if (selectedItemPgn == "ZDA") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parametersZDA = {{"GPSTime", "0.0", N2kConst::S, "double"},
                                                      {"GPSDay", "0", N2kConst::DAY, "int"},
                                                      {"GPSMonth", "0", N2kConst::MONT, "int"},
                                                      {"GPSYear", "0", N2kConst::YR, "int"},
                                                      {"LZD", "0", N2kConst::HR, "int"},
                                                      {"LZMD", "0", N2kConst::MIN, "int"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parametersZDA.size(); i++) {
                    if (parametersZDA[i][0] == paramList[i * 2]) {
                        parametersZDA[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> rangesZDA = {{0.0, 65535.0}, {0.0, 255.0}, {0.0, 255.0}, {0.0, 65535.0}, {0.0, 255.0}, {0.0, 255.0}};

            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parametersZDA, rangesZDA, 1000);

            // Create and add the DateTimeZDAWidget to the layout
            DateTimeZDA *dateTimeZDAWidget = new DateTimeZDA(idX, selectedItemLabel, this->mdiArea);
            dateTimeZDAWidget->setId(idX);
            dateTimeZDAWidget->setWindowTitle(selectedItemLabel);
            dateTimeZDAWidget->setToolTip(selectedItemLabel);
            dateTimeZDAWidget->setUserPrefs(userPrefs);
            dateTimeZDAWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(dateTimeZDAWidget, groupName);
            groupWidgetsMap[groupName].append(dateTimeZDAWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(dateTimeZDAWidget, SNT_ZDA, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(dateTimeZDAWidget, SNT_ZDA, pgnTableController->getPgnTableModel(), idX);
            dateTimeZDAWidget->setInitialGuiValues();
        }
        else if (selectedItemPgn == "000000") {
            idX++;
            currentIdX = idX;
            // Ask user number of rows to span
            int rowSpan = UiHelper::ShowSpinBoxDialog("Select Spacer Row Span", "Please choose number of rows to span:", 1, 20, 3);

            // Populate the pgn table model
            QList<QList<QString>> parameters000000 = {{"Rows", QString::number(rowSpan), "", "int"},
                                                      {"ShowTitleRow", "true", "", "bool"}};
            QList<QPair<double, double>> ranges000000 = {{1.0, 20.0}, {0.0, 1.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, "0", true, parameters000000, ranges000000, 0);

            // Create and add the SpacerWidget to the layout
            Spacer000000 *spacerWidget = new Spacer000000(idX, selectedItemLabel, this->mdiArea);
            spacerWidget->setId(idX);
            spacerWidget->setWindowTitle(selectedItemLabel);
            spacerWidget->setToolTip(selectedItemLabel);

            addWidgetToSubForm(spacerWidget, groupName);
            groupWidgetsMap[groupName].append(spacerWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            spacerWidget->setInitialGuiValues();

        }
        else {
            qWarning() << "Unknown PGN:" << selectedItemPgn;
        }
    } else if (groupName == "Tanks") {
        if (selectedItemPgn == "127505") {
            int fluidType = 0;
            if (m_mode == Sender) {
                // Ask the user to select a fluid type
                fluidType = UiHelper::ShowFluidLevelSourcePicker("Select Fluid Source", "Please choose a fluid source:", 0);
                if (fluidType == -1) {
                    //User pressed Cancel, don't continue with adding widget to form.
                    return;
                }
            }

            idX++;
            currentIdX = idX;

            // Get number of instances for the selected PGN
            int instance = pgnTableController->getPgnInstanceCount(selectedItemPgn);

            const bool newFuelTank = m_mode == Sender && paramList.isEmpty()
                && (fluidType == N2kft_Fuel || fluidType == N2kft_FuelGasoline);
            double capacity = 0.0, level = 0.0;
            if (newFuelTank) {
                QSet<int> used;
                for (const auto& data : pgnTableController->getPgns("127505")) {
                    int type = -1, number = -1;
                    for (const auto& row : data.paramDetails) {
                        if (row[0] == "FluidType") type = row[1].toInt();
                        if (row[0] == "Instance") number = row[1].toInt();
                    }
                    if (type == N2kft_Fuel || type == N2kft_FuelGasoline) used.insert(number);
                }
                instance = 0; while (used.contains(instance) && instance < 255) ++instance;
                if (instance <= 1) {
                    capacity = instance == 0 ? myVessel.PortFuelTankSize : myVessel.StbdFuelTankSize;
                    const double quantity = instance == 0 ? myVessel.PortFuelTankLevel : myVessel.StbdFuelTankLevel;
                    if (!std::isfinite(capacity) || capacity < 0) capacity = 0;
                    level = capacity > 0 && std::isfinite(quantity) ? qBound(0.0, quantity / capacity * 100.0, 100.0) : 0;
                }
            }

            // Populate the pgn table model
            QList<QList<QString>> parameters127505 = {{"Instance", QString::number(instance), "", "unsigned char"},
                                                      {"FluidType", QString::number(fluidType), N2kConst::ENUM, "tN2kFluidType"},
                                                      {"Level", QString::number(level, 'g', 17), N2kConst::PCT, "double"},
                                                      {"Capacity", QString::number(capacity, 'g', 17), N2kConst::L, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127505.size(); i++) {
                    if (parameters127505[i][0] == paramList[i * 2]) {
                        parameters127505[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127505 = {{0.0, 255.0}, {0.0, 100.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127505, ranges127505, 5000);

            // Create and add the FluidLevel127505 to the layout
            FluidLevel127505 *fluidLevelWidget = new FluidLevel127505(idX, selectedItemLabel, this->mdiArea);
            fluidLevelWidget->setId(idX);
            fluidLevelWidget->setWindowTitle(selectedItemLabel);
            fluidLevelWidget->setToolTip(selectedItemLabel);
            fluidLevelWidget->setUserPrefs(userPrefs);
            fluidLevelWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(fluidLevelWidget, groupName);
            groupWidgetsMap[groupName].append(fluidLevelWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(fluidLevelWidget, 127505, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(fluidLevelWidget, 127505, pgnTableController->getPgnTableModel(), idX);
            fluidLevelWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "000000") {
            idX++;
            currentIdX = idX;
            // Ask user number of rows to span
            int rowSpan = UiHelper::ShowSpinBoxDialog("Select Spacer Row Span", "Please choose number of rows to span:", 1, 20, 3);

            // Populate the pgn table model
            QList<QList<QString>> parameters000000 = {{"Rows", QString::number(rowSpan), "", "int"},
                                                      {"ShowTitleRow", "true", "", "bool"}};
            QList<QPair<double, double>> ranges000000 = {{1.0, 20.0}, {0.0, 1.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, "0", true, parameters000000, ranges000000, 0);

            // Create and add the SpacerWidget to the layout
            Spacer000000 *spacerWidget = new Spacer000000(idX, selectedItemLabel, this->mdiArea);
            spacerWidget->setId(idX);
            spacerWidget->setWindowTitle(selectedItemLabel);
            spacerWidget->setToolTip(selectedItemLabel);

            addWidgetToSubForm(spacerWidget, groupName);
            groupWidgetsMap[groupName].append(spacerWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            spacerWidget->setInitialGuiValues();
        } else {
            qWarning() << "Unknown PGN:" << selectedItemPgn;
        }
    } else if (groupName == "Electrical") {
        if (selectedItemPgn == "000000") {
            idX++;
            currentIdX = idX;
            // Ask user number of rows to span
            int rowSpan = UiHelper::ShowSpinBoxDialog("Select Spacer Row Span", "Please choose number of rows to span:", 1, 20, 3);

            // Populate the pgn table model
            QList<QList<QString>> parameters000000 = {{"Rows", QString::number(rowSpan), "", "int"},
                                                      {"ShowTitleRow", "true", "", "bool"}};
            QList<QPair<double, double>> ranges000000 = {{1.0, 20.0}, {0.0, 1.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, "0", true, parameters000000, ranges000000, 0);

            // Create and add the SpacerWidget to the layout
            Spacer000000 *spacerWidget = new Spacer000000(idX, selectedItemLabel, this->mdiArea);
            spacerWidget->setId(idX);
            spacerWidget->setWindowTitle(selectedItemLabel);
            spacerWidget->setToolTip(selectedItemLabel);

            addWidgetToSubForm(spacerWidget, groupName);
            groupWidgetsMap[groupName].append(spacerWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            spacerWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "127506") {
            idX++;
            currentIdX = idX;

            // Get number of instances for the selected PGN
            int instance = pgnTableController->getPgnInstanceCount(selectedItemPgn);

            // Populate the pgn table model
            const auto& electrical = myVessel.Electrical;
            const bool configured = m_mode == Sender && paramList.isEmpty() && instance < electrical.batteryCount;
            QList<QList<QString>> parameters127506 = {{"SID", "0", "", "unsigned char"},
                                                      {"DCInstance", QString::number(instance), "", "unsigned char"},
                                                      {"DCType", "0", N2kConst::ENUM, "tN2kDCType"},
                                                      {"StateOfCharge", configured ? QString::number(qRound(electrical.initialSoc)) : "0", N2kConst::PCT, "uint8_t"},
                                                      {"StateOfHealth", configured ? "100" : "0", N2kConst::PCT, "uint8_t"},
                                                      {"TimeRemaining", configured ? QString::number(N2kDoubleNA) : "0.0", N2kConst::S, "double"},
                                                      {"RippleVoltage", "0.0", N2kConst::V, "double"},
                                                      {"Capacity", configured ? QString::number(electrical.capacityC, 'g', 17) : "0.0", "C", "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127506.size(); i++) {
                    if (parameters127506[i][0] == paramList[i * 2]) {
                        parameters127506[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127506 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};

            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127506, ranges127506, 2000);

            // TODO: Implement the following logic once tree view is implemented
            //pgnTreeController->addPgn(idX, selectedItemLabel, selectedItemPgn);

            // Create and add the Electrical127506 to the layout
            DcStatus127506 *dcStatusWidget = new DcStatus127506(idX, selectedItemLabel, this->mdiArea);

            dcStatusWidget->setProperty("ElectricalSimulation", configured && electrical.simulateNew);
            dcStatusWidget->setId(idX);
            dcStatusWidget->setWindowTitle(selectedItemLabel);
            dcStatusWidget->setToolTip(selectedItemLabel);
            dcStatusWidget->setUserPrefs(userPrefs);
            dcStatusWidget->setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(dcStatusWidget, groupName);
            groupWidgetsMap[groupName].append(dcStatusWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(dcStatusWidget, 127506, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(dcStatusWidget, 127506, pgnTableController->getPgnTableModel(), idX);
            dcStatusWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "127507") {
            idX++;
            currentIdX = idX;

            // Get number of instances for the selected PGN
            int instance = pgnTableController->getPgnInstanceCount(selectedItemPgn);

            int batteryInstance = instance;
            if (m_mode == Sender) {
                // Ask use to select the Battery Instance
                batteryInstance = UiHelper::ShowSpinBoxDialog("Select Battery Instance", "Please choose a battery instance:", 0, 255, batteryInstance);
                if (batteryInstance == -1) {
                    //User pressed Cancel, default BatteryInstance to 0
                    batteryInstance = 0;
                }
            }

            QList<QList<QString>> parameters127507 = {{"Instance", QString::number(instance), "", "unsigned char"},
                                                      {"BatteryInstance", QString::number(batteryInstance), "", "unsigned char"},
                                                      {"ChargeState", "0", N2kConst::ENUM, "tN2kChargeState"},
                                                      {"ChargerMode", "0", N2kConst::ENUM, "tN2kChargerMode"},
                                                      {"Enabled", "0", N2kConst::ENUM, "tN2kOnOff"},
                                                      {"EqualizationPending", "0", N2kConst::ENUM, "tN2kOnOff"},
                                                      {"EqualizationTimeRemaining", "0.0", N2kConst::S, "double"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127507.size(); i++) {
                    if (parameters127507[i][0] == paramList[i * 2]) {
                        parameters127507[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127507 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};

            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127507, ranges127507, 2000);

            // Create and add the Electrical127507 to the layout
            ChargerStatus127507 *chargerStatusWidget = new ChargerStatus127507(idX, selectedItemLabel, this->mdiArea);

            chargerStatusWidget->setProperty("ElectricalSimulation", m_mode == Sender && paramList.isEmpty() && batteryInstance < myVessel.Electrical.batteryCount && myVessel.Electrical.simulateNew);
            chargerStatusWidget->setId(idX);
            chargerStatusWidget->setWindowTitle(selectedItemLabel);
            chargerStatusWidget->setToolTip(selectedItemLabel);
            chargerStatusWidget->setUserPrefs(userPrefs);
            chargerStatusWidget->PgnWidgetBase::setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(chargerStatusWidget, groupName);
            groupWidgetsMap[groupName].append(chargerStatusWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(chargerStatusWidget, 127507, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(chargerStatusWidget, 127507, pgnTableController->getPgnTableModel(), idX);
            chargerStatusWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "127508") {
            idX++;
            currentIdX = idX;

            int batteryInstance = pgnTableController->getPgnInstanceCount(selectedItemPgn);

            if (m_mode == Sender) {
                // Ask use to select the Battery Instance
                batteryInstance = UiHelper::ShowSpinBoxDialog("Select Battery Instance", "Please choose a battery instance:", 0, 255, batteryInstance);
                if (batteryInstance == -1) {
                    //User pressed Cancel, default BatteryInstance to 0
                    batteryInstance = 0;
                }
            }

            // Only a new simulated instrument gets a nominal starting value.
            // Received/restored/manual values remain authoritative. Weather has
            // no qualified freshness/validity contract for automatic seeding yet.
            const auto& electrical = myVessel.Electrical;
            const bool configured = m_mode == Sender && paramList.isEmpty() && batteryInstance < electrical.batteryCount;
            const double initialBatteryTemperature = m_mode == Sender && paramList.isEmpty()
                ? configured ? electrical.temperatureK : Convert::CelsiusToKelvin(20.0) : N2kDoubleNA;
            QList<QList<QString>> parameters127508 = {{"BatteryInstance", QString::number(batteryInstance), "", "unsigned char"},
                                                      {"BatteryVoltage", configured ? QString::number(electrical.initialVoltage, 'g', 17) : "0.0", N2kConst::V, "double"},
                                                      {"BatteryCurrent", "0.0", N2kConst::A, "double"},
                                                      {"BatteryTemperature", QString::number(initialBatteryTemperature, 'g', 15), N2kConst::K, "double"},
                                                      {"SID", "0", "", "unsigned char"}};

            // If paramList has values, update the default values
            if (paramList.size() > 0) {
                for (int i = 0; i < parameters127508.size(); i++) {
                    if (parameters127508[i][0] == paramList[i * 2]) {
                        parameters127508[i][1] = paramList[i * 2 + 1];
                    }
                }
            }

            QList<QPair<double, double>> ranges127508 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};

            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters127508, ranges127508, 2000);

            // Create and add the BatteryStatus127508 to the layout
            BatteryStatus127508 *batteryStatusWidget = new BatteryStatus127508(idX, selectedItemLabel, this->mdiArea);

            batteryStatusWidget->setProperty("ElectricalSimulation", configured && electrical.simulateNew);
            batteryStatusWidget->setId(idX);
            batteryStatusWidget->setWindowTitle(selectedItemLabel);
            batteryStatusWidget->setToolTip(selectedItemLabel);
            batteryStatusWidget->setUserPrefs(userPrefs);
            batteryStatusWidget->PgnWidgetBase::setEnabled(userOptionPrefs.newWidgetsEnabled);

            addWidgetToSubForm(batteryStatusWidget, groupName);
            groupWidgetsMap[groupName].append(batteryStatusWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(batteryStatusWidget, 127508, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(batteryStatusWidget, 127508, pgnTableController->getPgnTableModel(), idX);
            batteryStatusWidget->setInitialGuiValues();
        }
    } else if (groupName == "Communication") {
        if (selectedItemPgn == "129039") {
            ++idX;currentIdX=idX;auto rows=AisClassBPosition129039::parameters();
            // Reuse the Navigation/GNSS starting position, only for fresh Generate widgets.
            // Layout restoration and received Viewer reports supply their own values.
            if(m_mode==Sender && paramList.isEmpty()) {
                const QMap<QString,QString> defaults{{"UserID",QString::number(myVessel.MMSI)},
                    {"Latitude",QString::number(myVessel.Latitude,'g',17)},
                    {"Longitude",QString::number(myVessel.Longitude,'g',17)}};
                for(auto &row:rows)if(defaults.contains(row[0]) && AisClassBPosition129039::validField(row[0],defaults.value(row[0])))
                    row[1]=defaults.value(row[0]);
            }
            for(int i=0;i+1<paramList.size();i+=2)for(auto &row:rows)if(row[0]==paramList[i])row[1]=paramList[i+1];
            QList<QPair<double,double>> ranges;for(const auto &row:rows)ranges.append({N2kDoubleNA,4294967295.});
            pgnTableController->addPgn(idX,selectedItemLabel,selectedItemPgn,false,rows,ranges,1000);
            auto *instrument=new AisClassBPosition129039(idX,selectedItemLabel,mdiArea);instrument->setWindowTitle(selectedItemLabel);instrument->setUserPrefs(userPrefs);instrument->setEnabled(false);
            addWidgetToSubForm(instrument,groupName);groupWidgetsMap[groupName].append(instrument);
            auto *model=pgnTableController->getPgnTableModel();bindWidgetToModel(instrument,129039,model,idX);initializeWidgetFromModel(instrument,129039,model,idX);
        } else if (selectedItemPgn == "129810") {
            ++idX;currentIdX=idX;auto rows=AisClassBStatic129810::parameters();
            // Only a fresh Generate instrument inherits vessel defaults. Received and
            // restored reports retain their own identity and authoritative fields.
            if (m_mode == Sender && paramList.isEmpty()) {
                const QMap<QString,QString> defaults{{"UserID",QString::number(myVessel.MMSI)},{"VesselType",QString::number(myVessel.VesselType)},
                    {"Callsign",myVessel.RadioCallsign},{"Length",QString::number(myVessel.Length,'g',17)},
                    {"Beam",QString::number(myVessel.Beam,'g',17)}};
                for (auto &row : rows) if (defaults.contains(row[0])) {
                    const auto value = defaults.value(row[0]);
                    if (AisClassBStatic129810::editableField(row[0],value)) row[1] = value;
                    else qWarning().noquote() << "New AIS Part B:" << row[0]
                        << "Vessel Setup default is invalid or outside the supported range; using unspecified/unavailable. Setup is unchanged.";
                }
            }
            for(int i=0;i+1<paramList.size();i+=2)for(auto &row:rows)if(row[0]==paramList[i])row[1]=paramList[i+1];
            QList<QPair<double,double>> ranges;
            for(const auto &row:rows)ranges.append(AisClassBStatic129810::dimension(row[0])?qMakePair(N2kDoubleNA,6553.3):qMakePair(0.,4294967295.));
            pgnTableController->addPgn(idX,selectedItemLabel,selectedItemPgn,false,rows,ranges,30000);
            auto *instrument=new AisClassBStatic129810(idX,selectedItemLabel,mdiArea);instrument->setWindowTitle(selectedItemLabel);instrument->setUserPrefs(userPrefs);instrument->setEnabled(false);
            addWidgetToSubForm(instrument,groupName);groupWidgetsMap[groupName].append(instrument);
            auto *model=pgnTableController->getPgnTableModel();bindWidgetToModel(instrument,129810,model,idX);initializeWidgetFromModel(instrument,129810,model,idX);
        } else if (selectedItemPgn == "129809") {
            ++idX; currentIdX = idX;
            auto rows = AisClassBStatic129809::parameters();
            // Reuse the Navigation/GNSS starting position, only for fresh Generate widgets.
            // Layout restoration and received Viewer reports supply their own values.
            if(m_mode==Sender && paramList.isEmpty()) {
                const QMap<QString,QString> defaults{{"UserID",QString::number(myVessel.MMSI)},
                    {"Latitude",QString::number(myVessel.Latitude,'g',17)},
                    {"Longitude",QString::number(myVessel.Longitude,'g',17)}};
                for(auto &row:rows)if(defaults.contains(row[0]) && AisClassBPosition129039::validField(row[0],defaults.value(row[0])))
                    row[1]=defaults.value(row[0]);
            }
            for (int i=0; i+1<paramList.size(); i+=2)
                for (auto &row : rows) if (row[0] == paramList[i]) row[1] = paramList[i+1];
            const QList<QPair<double,double>> ranges{{24,24}, {0,3}, {0,4294967295.0}, {0,20}, {0,31}, {0,255}};
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, false, rows, ranges, 30000);
            auto *instrument = new AisClassBStatic129809(idX, selectedItemLabel, mdiArea);
            instrument->setWindowTitle(selectedItemLabel); instrument->setUserPrefs(userPrefs);
            // Adding an AIS report is never an authorization to start it.
            instrument->setEnabled(false);
            addWidgetToSubForm(instrument, groupName); groupWidgetsMap[groupName].append(instrument);
            auto *model = pgnTableController->getPgnTableModel();
            bindWidgetToModel(instrument, 129809, model, idX);
            initializeWidgetFromModel(instrument, 129809, model, idX);
        } else if (selectedItemPgn == "000000") {
            idX++;
            currentIdX = idX;
            // Ask user number of rows to span
            int rowSpan = UiHelper::ShowSpinBoxDialog("Select Spacer Row Span", "Please choose number of rows to span:", 1, 20, 3);

            // Populate the pgn table model
            QList<QList<QString>> parameters000000 = {{"Rows", QString::number(rowSpan), "", "int"},
                                                      {"ShowTitleRow", "true", "", "bool"}};
            QList<QPair<double, double>> ranges000000 = {{1.0, 20.0}, {0.0, 1.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, "0", true, parameters000000, ranges000000, 0);

            // Create and add the SpacerWidget to the layout
            Spacer000000 *spacerWidget = new Spacer000000(idX, selectedItemLabel, this->mdiArea);
            spacerWidget->setId(idX);
            spacerWidget->setWindowTitle(selectedItemLabel);
            spacerWidget->setToolTip(selectedItemLabel);

            addWidgetToSubForm(spacerWidget, groupName);
            groupWidgetsMap[groupName].append(spacerWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            spacerWidget->setInitialGuiValues();
        }
    } else if (groupName == "Other") {
        if (WindlassStatus::supported(selectedItemPgn.toInt())) {
            if(!paramList.isEmpty() && !WindlassStatus::validParameters(selectedItemPgn.toInt(),paramList))return;
            ++idX;currentIdX=idX;auto rows=WindlassStatus::parameters(selectedItemPgn.toInt());
            // Creation defaults only: received reports and restored Layouts remain authoritative.
            if(m_mode==Sender && selectedItemPgn=="128777" && paramList.isEmpty()) {
                const QMap<QString,QString> defaults{{"WindlassMotionStatus",QString::number(N2kDD480_WindlassStopped)},
                    {"RodeTypeStatus",QString::number(N2kDD481_ChainPresentlyDetected)},
                    {"AnchorDockingStatus",QString::number(N2kDD482_FullyDocked)},{"OperatingEvents","0"}};
                for(auto &row:rows)if(defaults.contains(row[0]))row[1]=defaults[row[0]];
            }
            for(int i=0;i+1<paramList.size();i+=2)for(auto &row:rows)if(row[0]==paramList[i])row[1]=paramList[i+1];
            QList<QPair<double,double>> ranges;for(const auto &row:rows)ranges.append({N2kDoubleNA,3931980.});
            pgnTableController->addPgn(idX,selectedItemLabel,selectedItemPgn,false,rows,ranges,1000);
            WindlassStatus *instrument=selectedItemPgn=="128777"?static_cast<WindlassStatus*>(new WindlassOperating128777(idX,selectedItemLabel,mdiArea)):static_cast<WindlassStatus*>(new WindlassMonitoring128778(idX,selectedItemLabel,mdiArea));
            instrument->setWindowTitle(selectedItemLabel);instrument->setUserPrefs(userPrefs);instrument->setEnabled(false);
            addWidgetToSubForm(instrument,groupName);groupWidgetsMap[groupName].append(instrument);
            auto *model=pgnTableController->getPgnTableModel();bindWidgetToModel(instrument,selectedItemPgn.toInt(),model,idX);initializeWidgetFromModel(instrument,selectedItemPgn.toInt(),model,idX);
        } else if (selectedItemPgn == "000000") {
            idX++;
            currentIdX = idX;
            // Ask user number of rows to span
            int rowSpan = UiHelper::ShowSpinBoxDialog("Select Spacer Row Span", "Please choose number of rows to span:", 1, 20, 3);

            // Populate the pgn table model
            QList<QList<QString>> parameters000000 = {{"Rows", QString::number(rowSpan), "", "int"},
                                                      {"ShowTitleRow", "true", "", "bool"}};
            QList<QPair<double, double>> ranges000000 = {{1.0, 20.0}, {0.0, 1.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, "0", true, parameters000000, ranges000000, 0);

            // Create and add the SpacerWidget to the layout
            Spacer000000 *spacerWidget = new Spacer000000(idX, selectedItemLabel, this->mdiArea);
            spacerWidget->setId(idX);
            spacerWidget->setWindowTitle(selectedItemLabel);
            spacerWidget->setToolTip(selectedItemLabel);

            addWidgetToSubForm(spacerWidget, groupName);
            groupWidgetsMap[groupName].append(spacerWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
            spacerWidget->setInitialGuiValues();
        }
    } else if (groupName == "Generate") {
        if (selectedItemPgn == "-129033") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters_129033 = {{"SID", "0", "", "unsigned char"},
                                                       {"DaysSince1970", "0", N2kConst::DAY, "uint16_t"},
                                                       {"SecondsSinceMidnight", "0.0", N2kConst::S, "double"},
                                                       {"LocalOffset", "0", N2kConst::MIN, "int16_t"}};
            QList<QPair<double, double>> ranges_129033 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters_129033, ranges_129033);

            // Create and add the GeneratorDateTime widget to the layout
            generatorDateTimeWidget = new GeneratorDateTime(idX, selectedItemLabel, this->mdiArea);
            generatorDateTimeWidget->setId(idX);
            generatorDateTimeWidget->setWindowTitle(selectedItemLabel);
            generatorDateTimeWidget->setToolTip(selectedItemLabel);
            generatorDateTimeWidget->setUserPrefs(userPrefs);

            addWidgetToSubForm(generatorDateTimeWidget, groupName);
            groupWidgetsMap[groupName].append(generatorDateTimeWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(generatorDateTimeWidget, -129033, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(generatorDateTimeWidget, -129033, pgnTableController->getPgnTableModel(), idX);
        } else if (selectedItemPgn == "-129025") {
            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters_129025 = {{"SID", "0", "", "unsigned char"},
                                                       {"Latitude", QString::number(myVessel.Latitude, 'f', 6), N2kConst::DEG, "double"},
                                                       {"Longitude", QString::number(myVessel.Longitude, 'f', 6), N2kConst::DEG, "double"},
                                                       {"Altitude", "0.0", N2kConst::M, "double"},
                                                       {"HeadingReference", "1", N2kConst::ENUM, "tN2kHeadingReference"},
                                                       {"Heading", "0.0", N2kConst::RAD, "double"},
                                                       {"Deviation", "0.0", N2kConst::RAD, "double"},
                                                       {"Variation", "-0.147", N2kConst::RAD, "double"},
                                                       {"COG", "0.0", N2kConst::RAD, "double"},
                                                       {"SOG", "0.0", N2kConst::MPS, "double"},
                                                       {"RateOfTurn", "0.0", N2kConst::RPS, "double"},
                                                       {"Yaw", "0.0", N2kConst::RAD, "double"},
                                                       {"Pitch", "0.0", N2kConst::RAD, "double"},
                                                       {"Roll", "0.0", N2kConst::RAD, "double"},
                                                       {"Heave", "0.0", N2kConst::M, "double"},
                                                       {"RudderPosition", "0.0", N2kConst::RAD, "double"}};
            QList<QPair<double, double>> ranges_129025 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters_129025, ranges_129025);

            // Create and add the GeneratorVesselMovement widget to the layout
            generatorVesselMovementWidget = new GeneratorVesselMovement(idX, selectedItemLabel, this->mdiArea);

            generatorVesselMovementWidget->setId(idX);
            generatorVesselMovementWidget->setWindowTitle(selectedItemLabel);
            generatorVesselMovementWidget->setToolTip(selectedItemLabel);
            generatorVesselMovementWidget->setUserPrefs(userPrefs);

            addWidgetToSubForm(generatorVesselMovementWidget, groupName);
            groupWidgetsMap[groupName].append(generatorVesselMovementWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(generatorVesselMovementWidget, -129025, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(generatorVesselMovementWidget, -129025, pgnTableController->getPgnTableModel(), idX);

            // Ask once per optional source. Nested lower-unit creation must not
            // repeat an engine question already answered in this workflow.
            {
                QScopedValueRollback<bool> prompting(m_promptingMovementDependencies, true);
                for (int i = 0; i < myVessel.NumEngines; i++) {
                    bindGeneratorVesselMovementToGeneratorEngine(generatorVesselMovementWidget, i);
                }
            }

            // Set My Vessel defaults after data binding
            generatorVesselMovementWidget->setMyVessel(myVessel);
            generatorVesselMovementWidget->setInitialGuiValues();

        } else if (selectedItemPgn == "-127489") {
            // Get number of instances for the selected PGN
            int instance = pgnTableController->getPgnInstanceCount(selectedItemPgn);
            // Ask the user to select an engine instance
            int engineInstance = UiHelper::ShowSpinBoxDialog("Select Engine (1-8)", "Please choose engine number:", 1, 8, instance + 1);
            if (engineInstance != -1) {
                engineInstance -= 1;
            } else {
                //User pressed Cancel, don't continue with adding widget to form.
                return;
            }

            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters_127489 = {{"EngineInstance", QString::number(engineInstance), "", "int"},
                                                       {"EngineSpeed", "0.0", N2kConst::RPM, "double"},
                                                       {"EngineBoostPressure", "0.0", N2kConst::PA, "double"},
                                                       {"EngineTiltTrim", "90", N2kConst::PCT, "int"},
                                                       {"EngineOilPress", "0.0", N2kConst::PA, "double"},
                                                       {"EngineOilTemp", "295.372", N2kConst::K, "double"},
                                                       {"EngineCoolantTemp", "295.372", N2kConst::K, "double"},
                                                       {"AltenatorVoltage", "0.0", N2kConst::V, "double"},
                                                       {"FuelRate", "0.0", N2kConst::LPH, "double"},
                                                       {"EngineHours", "0.0", N2kConst::S, "double"},
                                                       {"EngineCoolantPress", "0.0", N2kConst::PA, "double"},
                                                       {"EngineFuelPress", "0.0", N2kConst::PA, "double"},
                                                       {"EngineLoad", "0.0", N2kConst::PCT, "double"},
                                                       {"EngineTorque", "0.0", N2kConst::PCT, "double"},
                                                       {"Status1", "0", N2kConst::ENUM, "tN2kEngineDiscreteStatus1"},
                                                       {"Status2", "0", N2kConst::ENUM, "tN2kEngineDiscreteStatus2"}};
            QList<QPair<double, double>> ranges_127489 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters_127489, ranges_127489);

            // Create and add the GeneratorEngine widget to the layout
            generatorEngineWidget = new GeneratorEngine(idX, selectedItemLabel, this->mdiArea);
            generatorEngineWidget->setId(idX);
            generatorEngineWidget->setWindowTitle(selectedItemLabel);
            generatorEngineWidget->setToolTip(selectedItemLabel);
            generatorEngineWidget->setUserPrefs(userPrefs);
            generatorEngineWidget->setMyVessel(myVessel);
            generatorEngineWidget->setEngineInstance(engineInstance);

            addWidgetToSubForm(generatorEngineWidget, groupName);
            groupWidgetsMap[groupName].append(generatorEngineWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(generatorEngineWidget, -127489, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(generatorEngineWidget, -127489, pgnTableController->getPgnTableModel(), idX);

            // Add the engine instance to the list of engine instances
            generatorEngines.insert(engineInstance, qMakePair(generatorEngineWidget, false));

            // Set My Vessel defaults after data binding
            generatorEngineWidget->setMyVessel(myVessel);
            generatorEngineWidget->setInitialGuiValues();
        } else if (selectedItemPgn == "-127493") {
            // Get number of instances for the selected PGN
            int instance = pgnTableController->getPgnInstanceCount(selectedItemPgn);
            // Ask the user to select an engine instance
            int engineInstance = UiHelper::ShowSpinBoxDialog("Select Engine (1-8)", "Please choose engine number:", 1, 8, instance + 1);
            if (engineInstance != -1) {
                engineInstance -= 1;
            } else {
                //User pressed Cancel, don't continue with adding widget to form.
                return;
            }

            idX++;
            currentIdX = idX;

            // Populate the pgn table model
            QList<QList<QString>> parameters_127493 = {{"EngineInstance", QString::number(engineInstance), "", "int"},
                                                       {"TransmissionGear", "1", N2kConst::ENUM, "tN2kTransmissionGear"},
                                                       {"OilPressure", "0.0", N2kConst::PA, "double"},
                                                       {"OilTemperature", "295.372", N2kConst::K, "double"},
                                                       {"DiscreteStatus1", "0", "", "unsigned char"}};
            QList<QPair<double, double>> ranges_127493 = {{0.0, 255.0}, {0.0, 65535.0}, {0.0, 65535.0}};
            // Add PGN to both models
            pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters_127493, ranges_127493);

            // Create and add the GeneratorEngine widget to the layout
            generatorTransLowerUnitWidget = new GeneratorTransLowerUnit(idX, selectedItemLabel, this->mdiArea);
            generatorTransLowerUnitWidget->setId(idX);
            generatorTransLowerUnitWidget->setWindowTitle(selectedItemLabel);
            generatorTransLowerUnitWidget->setToolTip(selectedItemLabel);
            generatorTransLowerUnitWidget->setUserPrefs(userPrefs);
            generatorTransLowerUnitWidget->setEngineInstance(engineInstance);

            addWidgetToSubForm(generatorTransLowerUnitWidget, groupName);
            groupWidgetsMap[groupName].append(generatorTransLowerUnitWidget);

            // Data bind the widget to the model for two-way data updates and set widget properties to model
            bindWidgetToModel(generatorTransLowerUnitWidget, -127493, pgnTableController->getPgnTableModel(), idX);
            initializeWidgetFromModel(generatorTransLowerUnitWidget, -127493, pgnTableController->getPgnTableModel(), idX);

            // Add the generatorTransLowerUnitWidget to the pair list of generatorTransLowerUnits
            generatorTransLowerUnits.insert(engineInstance, qMakePair(generatorTransLowerUnitWidget, false));

            // Set My Vessel defaults after data binding
            generatorTransLowerUnitWidget->setMyVessel(myVessel);

            if (myVessel.NumEngines > 0) {
                // Bind widgets based on engine instance to engine generator
                bindWidgetsBasedOnEngineInstanceToGenerator(engineInstance);
            }
        } else if (selectedItemPgn == "-130311") {
            idX++;
            currentIdX = idX;

        // Populate the pgn table model
        QList<QList<QString>> parameters130311 = {{"SID", "5", "", "unsigned char"},
                                                  {"AirTemperature", QString::number(N2kDoubleNA, 'g', 17), N2kConst::K, "double"},
                                                  {"SeaTemperature", QString::number(N2kDoubleNA, 'g', 17), N2kConst::K, "double"},
                                                  {"Humidity", QString::number(N2kDoubleNA, 'g', 17), N2kConst::PCT, "double"},
                                                  {"AtmosphericPressure", QString::number(N2kDoubleNA, 'g', 17), N2kConst::PA, "double"},
                                                  {"WindSpeed", QString::number(N2kDoubleNA, 'g', 17), N2kConst::MPS, "double"},
                                                  {"WindAngle", QString::number(N2kDoubleNA, 'g', 17), N2kConst::RAD, "double"},
                                                  {"TemperatureSource", "1", N2kConst::ENUM, "tN2kTempSource"},
                                                  {"HumiditySource", "1", N2kConst::ENUM, "tN2kHumiditySource"},
                                                  {"Latitude", QString::number(myVessel.Latitude, 'f', 6), N2kConst::DEG, "double"},
                                                  {"Longitude", QString::number(myVessel.Longitude, 'f', 6), N2kConst::DEG, "double"}};
        parameters130311.append(GeneratorWeather::additionalParameters());
        QList<QPair<double, double>> ranges130311 = {{-273.15, 100.0}, {0.0, 100.0}, {0.0, 2000.0}, {0.0, 100.0}, {0.0, 2*M_PI}, {0.0, 1.0}, {0.0, 1.0}};

        while (ranges130311.size() < parameters130311.size()) ranges130311.append({N2kDoubleNA, 1e12});
        // Add PGN to both models
        normalizeAtmosphericPressure(parameters130311, ranges130311);
        pgnTableController->addPgn(idX, selectedItemLabel, selectedItemPgn, true, parameters130311, ranges130311, 2000);

        // Create and add the GeneratorWeather to the layout
        generatorWeatherWidget = new GeneratorWeather(idX, selectedItemLabel, this->mdiArea);
        generatorWeatherWidget->setId(idX);
        generatorWeatherWidget->setWindowTitle(selectedItemLabel);
        generatorWeatherWidget->setToolTip(selectedItemLabel);
        generatorWeatherWidget->setUserPrefs(userPrefs);
        generatorWeatherWidget->setOpenWeatherMapApiKey(m_openWeatherMapApiKey);

        addWidgetToSubForm(generatorWeatherWidget, groupName);
        groupWidgetsMap[groupName].append(generatorWeatherWidget);

        // Data bind the widget to the model for two-way data updates and set widget properties to model
        bindWidgetToModel(generatorWeatherWidget, -130311, pgnTableController->getPgnTableModel(), idX);
        initializeWidgetFromModel(generatorWeatherWidget, -130311, pgnTableController->getPgnTableModel(), idX);
        generatorWeatherWidget->setInitialGuiValues();

        } else if (selectedItemPgn == "000000") {
        idX++;
        currentIdX = idX;
        // Ask user number of rows to span
        int rowSpan = UiHelper::ShowSpinBoxDialog("Select Spacer Row Span", "Please choose number of rows to span:", 1, 20, 3);

        // Populate the pgn table model
        QList<QList<QString>> parameters000000 = {{"Rows", QString::number(rowSpan), "", "int"},
                                                  {"ShowTitleRow", "true", "", "bool"}};
        QList<QPair<double, double>> ranges000000 = {{1.0, 20.0}, {0.0, 1.0}};
        // Add PGN to both models
        pgnTableController->addPgn(idX, selectedItemLabel, "0", true, parameters000000, ranges000000, 0);

        // Create and add the SpacerWidget to the layout
        Spacer000000 *spacerWidget = new Spacer000000(idX, selectedItemLabel, this->mdiArea);
        spacerWidget->setId(idX);
        spacerWidget->setWindowTitle(selectedItemLabel);
        spacerWidget->setToolTip(selectedItemLabel);

        addWidgetToSubForm(spacerWidget, groupName);
        groupWidgetsMap[groupName].append(spacerWidget);

        // Data bind the widget to the model for two-way data updates and set widget properties to model
        bindWidgetToModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
        initializeWidgetFromModel(spacerWidget, 0, pgnTableController->getPgnTableModel(), idX);
        spacerWidget->setInitialGuiValues();
        }
    } else {
        qWarning() << "Unknown group name:" << groupName;
    }

    // Ensure that the table and tree views are updated via controllers
    pgnTableController->refreshPgnParameters(idX);

}

void MainController::initializeWidgetFromModel(QWidget *widget, int Pgn, PgnTableModel *model, int idX)
{
    auto *timestampModel=pgnTableController->getPgnTableModel();
    timestampModel->beginParameterInitialization(idX,idX);
    const auto timestampInitialization=qScopeGuard([timestampModel]{timestampModel->endParameterInitialization();});

    // Initial display setters must not enqueue edits of the model they are reading.
    // Generator notifications retain their existing synchronization behavior.
    const QSignalBlocker initializationSignals(Pgn > 0 ? widget : nullptr);

    if(auto *instrument=qobject_cast<ExpansionInstrument*>(widget)) {
        for(const auto &row:model->getPgnParams(idX))instrument->setValue(row[0],row[1]);
    } else if (WindlassStatus::supported(Pgn)) {
        auto *instrument=qobject_cast<WindlassStatus*>(widget);
        for(const auto &row:model->getPgnParams(idX))instrument->setValue(row[0],row[1]);
    } else if (Pgn == 129039) {
        auto *instrument=qobject_cast<AisClassBPosition129039*>(widget);
        for(const auto &row:model->getPgnParams(idX))instrument->setValue(row[0],row[1]);
    } else if (Pgn == 129810) {
        auto *instrument=qobject_cast<AisClassBStatic129810 *>(widget);
        for(const auto &row:model->getPgnParams(idX))instrument->setValue(row[0],row[1]);
    } else if (Pgn == 129809) {
        auto *instrument = qobject_cast<AisClassBStatic129809 *>(widget);
        for (const auto &row : model->getPgnParams(idX)) instrument->setValue(row[0], row[1]);
    } else if (Pgn == 130306 || Pgn == 130313) {
        auto *instrument = qobject_cast<EnvironmentalInstrument*>(widget);
        for (const auto &row : model->getPgnParams(idX)) instrument->setValue(row[0], row[1].toDouble());
    } else if (Pgn == 130576) {
        auto *trim = qobject_cast<TrimTab130576*>(widget);
        for (const auto &param : model->getPgnParams(idX)) {
            if (param[0] == "PortTrimTab") trim->setPortTrimTab(param[1].toInt());
            if (param[0] == "StbdTrimTab") trim->setStbdTrimTab(param[1].toInt());
        }
    } else if (Pgn == 130310) {
        // Cast widget to OutsideEnvironmental130310* and call the initialize function
        auto *outsideenvironmentalWidget = qobject_cast<OutsideEnvironmental130310 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                outsideenvironmentalWidget->setSID(paramValue.toUInt());
            } else if (paramName == "OutsideAmbientAirTemperature") {
                outsideenvironmentalWidget->setOutsideAmbientAirTemperature(paramValue.toDouble());
            } else if (paramName == "WaterTemperature") {
                outsideenvironmentalWidget->setWaterTemperature(paramValue.toDouble());
            } else if (paramName == "AtmosphericPressure") {
                outsideenvironmentalWidget->setAtmosphericPressure(paramValue.toDouble());
            }
        }
    } else if (Pgn == 126992) {
        // Cast widget to SystemTime126992* and call the initialize function
        auto *systemTimeWidget = qobject_cast<SystemTime126992 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                systemTimeWidget->setSID(paramValue.toUInt());
            } else if (paramName == "SystemDate") {
                systemTimeWidget->setSystemDate(paramValue.toUInt());
            } else if (paramName == "SystemTime") {
                systemTimeWidget->setSystemTime(paramValue.toDouble());
            } else if (paramName == "TimeSource") {
                systemTimeWidget->setTimeSource(static_cast<tN2kTimeSource>(paramValue.toUInt()));
            }
        }
    } else if (Pgn == 127245) {
        // Cast widget to Rudder127245* and call the initialize function
        auto *rudderWidget = qobject_cast<Rudder127245 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "RudderPosition") {
                rudderWidget->setRudderPosition(paramValue.toDouble());
            } else if (paramName == "Instance") {
                rudderWidget->setInstance(paramValue.toUInt());
            } else if (paramName == "RudderDirectionOrder") {
                rudderWidget->setRudderDirectionOrder(static_cast<tN2kRudderDirectionOrder>(paramValue.toUInt()));
            } else if (paramName == "AngleOrder") {
                rudderWidget->setAngleOrder(paramValue.toDouble());
            }
        }
    }
    else if (Pgn == 127250) {
        // Cast widget to VesselHeading127250* and call the initialize function
        auto *vesselHeadingWidget = qobject_cast<VesselHeading127250 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                vesselHeadingWidget->setSID(paramValue.toUInt());
            } else if (paramName == "Heading") {
                vesselHeadingWidget->setHeading(paramValue.toDouble());
            } else if (paramName == "Deviation") {
                vesselHeadingWidget->setDeviation(paramValue.toDouble());
            } else if (paramName == "Variation") {
                vesselHeadingWidget->setVariation(paramValue.toDouble());
            } else if (paramName == "HeadingReference") {
                vesselHeadingWidget->setHeadingReference(static_cast<tN2kHeadingReference>(paramValue.toUInt()));
            }
        }
    } else if (Pgn == 127251) {
        // Cast widget to RateOfTurn127251* and call the initialize function
        auto *rateOfTurnWidget = qobject_cast<RateOfTurn127251 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                rateOfTurnWidget->setSID(paramValue.toUInt());
            } else if (paramName == "RateOfTurn") {
                rateOfTurnWidget->setRateOfTurn(paramValue.toDouble());
            }
        }
    } else if (Pgn == 127252) {
        // Cast widget to Heave127252* and call the initialize function
        auto *heaveWidget = qobject_cast<Heave127252 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                heaveWidget->setSID(paramValue.toUInt());
            } else if (paramName == "Heave") {
                heaveWidget->setHeave(paramValue.toDouble());
            } else if (paramName == "Delay") {
                heaveWidget->setDelay(paramValue.toDouble());
            } else if (paramName == "DelaySource") {
                heaveWidget->setDelaySource(static_cast<tN2kDelaySource>(paramValue.toUInt()));
            }
        }
    } else if (Pgn == 127257) {
        // Cast widget to Attitude127257* and call the initialize function
        auto *attitudeWidget = qobject_cast<Attitude127257 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                attitudeWidget->setSID(paramValue.toUInt());
            } else if (paramName == "Yaw") {
                attitudeWidget->setYaw(paramValue.toDouble());
            } else if (paramName == "Pitch") {
                attitudeWidget->setPitch(paramValue.toDouble());
            } else if (paramName == "Roll") {
                attitudeWidget->setRoll(paramValue.toDouble());
            }
        }
    } else if (Pgn == 127258) {
        // Cast widget to MagneticVariation127258* and call the initialize function
        auto *magneticVariationWidget = qobject_cast<MagneticVariation127258 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                magneticVariationWidget->setSID(paramValue.toUInt());
            } else if (paramName == "Source") {
                magneticVariationWidget->setSource(static_cast<tN2kMagneticVariation>(paramValue.toUInt()));
            } else if (paramName == "DaysSince1970") {
                magneticVariationWidget->setDaysSince1970(paramValue.toUInt());
            } else if (paramName == "Variation") {
                magneticVariationWidget->setVariation(paramValue.toDouble());
            }
        }
    } else if (Pgn == 128259) {
        // Cast widget to Speed128259* and call the initialize function
        auto *speedWidget = qobject_cast<Speed128259 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                speedWidget->setSID(paramValue.toUInt());
            } else if (paramName == "WaterReferenced") {
                speedWidget->setWaterReferenced(paramValue.toDouble());
            } else if (paramName == "GroundReferenced") {
                speedWidget->setGroundReferenced(paramValue.toDouble());
            } else if (paramName == "SWRT") {
                speedWidget->setSWRT(static_cast<tN2kSpeedWaterReferenceType>(paramValue.toUInt()));
            }
        }
    } else if (Pgn == 128267) {
        // Cast widget to Depth128267* and call the initialize function
        auto *depthWidget = qobject_cast<Depth128267 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                depthWidget->setSID(paramValue.toUInt());
            } else if (paramName == "DepthBelowTransducer") {
                depthWidget->setDepthBelowTransducer(paramValue.toDouble());
            } else if (paramName == "Offset") {
                depthWidget->setOffset(paramValue.toDouble());
            } else if (paramName == "Range") {
                depthWidget->setRange(paramValue.toDouble());
            }
        }
    } else if (Pgn == 127506) {
        // Cast widget to DcStatus127506* and call the initialize function
        auto *dcStatusWidget = qobject_cast<DcStatus127506 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                dcStatusWidget->setSID(paramValue.toUInt());
            } else if (paramName == "DCInstance") {
                dcStatusWidget->setDCInstance(paramValue.toUInt());
            } else if (paramName == "DCType") {
                dcStatusWidget->setDCType(static_cast<tN2kDCType>(paramValue.toInt()));
            } else if (paramName == "StateOfCharge") {
                dcStatusWidget->setStateOfCharge(paramValue.toDouble());
            } else if (paramName == "StateOfHealth") {
                dcStatusWidget->setStateOfHealth(paramValue.toDouble());
            } else if (paramName == "TimeRemaining") {
                dcStatusWidget->setTimeRemaining(paramValue.toDouble());
            } else if (paramName == "RippleVoltage") {
                dcStatusWidget->setRippleVoltage(paramValue.toDouble());
            } else if (paramName == "Capacity") {
                dcStatusWidget->setCapacity(paramValue.toDouble());
            }
        }
    } else if (Pgn == 127505) {
        // Cast widget to FluidLevel127505* and call the initialize function
        auto *fluidLevelWidget = qobject_cast<FluidLevel127505 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "Instance") {
                fluidLevelWidget->setInstance(paramValue.toUInt());
            } else if (paramName == "FluidType") {
                fluidLevelWidget->setFluidType(static_cast<tN2kFluidType>(paramValue.toUInt()));
            } else if (paramName == "Level") {
                fluidLevelWidget->setLevel(paramValue.toDouble());
            } else if (paramName == "Capacity") {
                fluidLevelWidget->setCapacity(paramValue.toDouble());
            }
        }
    } else if (Pgn == 127508) {
        // Cast widget to BatteryStatus127508* and call the initialize function
        auto *batteryStatusWidget = qobject_cast<BatteryStatus127508 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "BatteryInstance") {
                batteryStatusWidget->setBatteryInstance(paramValue.toUInt());
            } else if (paramName == "BatteryVoltage") {
                batteryStatusWidget->setBatteryVoltage(paramValue.toDouble());
            } else if (paramName == "BatteryCurrent") {
                batteryStatusWidget->setBatteryCurrent(paramValue.toDouble());
            } else if (paramName == "BatteryTemperature") {
                batteryStatusWidget->setBatteryTemperature(paramValue.toDouble());
            } else if (paramName == "SID") {
                batteryStatusWidget->setSID(paramValue.toUInt());
            }
        }
    }
    else if (Pgn == 127507) {
        // Cast widget to ChargerStatus127507* and call the initialize function
        auto *chargerStatusWidget = qobject_cast<ChargerStatus127507 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "Instance") {
                chargerStatusWidget->setInstance(paramValue.toUInt());
            } else if (paramName == "BatteryInstance") {
                chargerStatusWidget->setBatteryInstance(paramValue.toUInt());
            } else if (paramName == "ChargeState") {
                chargerStatusWidget->setChargeState(static_cast<tN2kChargeState>(paramValue.toUInt()));
            } else if (paramName == "ChargerMode") {
                chargerStatusWidget->setChargerMode(static_cast<tN2kChargerMode>(paramValue.toUInt()));
            } else if (paramName == "Enabled") {
                chargerStatusWidget->setEnabled(static_cast<tN2kOnOff>(paramValue.toUInt()));
            } else if (paramName == "EqualizationPending") {
                chargerStatusWidget->setEqualizationPending(static_cast<tN2kOnOff>(paramValue.toUInt()));
            } else if (paramName == "EqualizationTimeRemaining") {
                chargerStatusWidget->setEqualizationTimeRemaining(paramValue.toDouble());
            }
        }
    } else if (Pgn == 129025) {
        // Cast widget to LatLong* and call the initialize function
        auto *latLongWidget = qobject_cast<LatLong129025 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "Latitude") {
                latLongWidget->setLatitude(paramValue.toDouble());
            } else if (paramName == "Longitude") {
                latLongWidget->setLongitude(paramValue.toDouble());
            }
        }
    } else if (Pgn == 129026) {
        // Cast widget to CogSog129026* and call the initialize function
        auto *cogSogWidget = qobject_cast<CogSog129026 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                cogSogWidget->setSID(paramValue.toUInt());
            } else if (paramName == "HeadingReference") {
                cogSogWidget->setHeadingReference(static_cast<tN2kHeadingReference>(paramValue.toUInt()));
            } else if (paramName == "COG") {
                cogSogWidget->setCOG(paramValue.toDouble());
            } else if (paramName == "SOG") {
                cogSogWidget->setSOG(paramValue.toDouble());
            }
        }
    } else if (Pgn == 129029) {
        // Cast widget to GnssPositionData129029* and call the initialize function
        auto *gnssPositionDataWidget = qobject_cast<GnssPositionData129029 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                gnssPositionDataWidget->setSID(paramValue.toUInt());
            } else if (paramName == "DaysSince1970") {
                gnssPositionDataWidget->setDaysSince1970(paramValue.toUInt());
            } else if (paramName == "SecondsSinceMidnight") {
                gnssPositionDataWidget->setSecondsSinceMidnight(paramValue.toDouble());
            } else if (paramName == "Latitude") {
                gnssPositionDataWidget->setLatitude(paramValue.toDouble());
            } else if (paramName == "Longitude") {
                gnssPositionDataWidget->setLongitude(paramValue.toDouble());
            } else if (paramName == "Altitude") {
                gnssPositionDataWidget->setAltitude(paramValue.toDouble());
            } else if (paramName == "GNSStype") {
                gnssPositionDataWidget->setGNSStype(static_cast<tN2kGNSStype>(paramValue.toUInt()));
            } else if (paramName == "GNSSmethod") {
                gnssPositionDataWidget->setGNSSmethod(static_cast<tN2kGNSSmethod>(paramValue.toUInt()));
            } else if (paramName == "nSatellites") {
                gnssPositionDataWidget->setnSatellites(paramValue.toUInt());
            } else if (paramName == "HDOP") {
                gnssPositionDataWidget->setHDOP(paramValue.toDouble());
            } else if (paramName == "PDOP") {
                gnssPositionDataWidget->setPDOP(paramValue.toDouble());
            } else if (paramName == "GeoidalSeparation") {
                gnssPositionDataWidget->setGeoidalSeparation(paramValue.toDouble());
            } else if (paramName == "nReferenceStations") {
                gnssPositionDataWidget->setnReferenceStations(paramValue.toUInt());
            } else if (paramName == "ReferenceStationType") {
                gnssPositionDataWidget->setReferenceStationType(static_cast<tN2kGNSStype>(paramValue.toUInt()));
            } else if (paramName == "ReferenceSationID") {
                gnssPositionDataWidget->setReferenceSationID(paramValue.toUInt());
            } else if (paramName == "AgeOfCorrection") {
                gnssPositionDataWidget->setAgeOfCorrection(paramValue.toDouble());
            }
        }
    } else if (Pgn == 129033) {
        // Cast widget to DateTime129033* and call the initialize function
        auto *dateTimeWidget = qobject_cast<DateTime129033 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "DaysSince1970") {
                dateTimeWidget->setDaysSince1970(paramValue.toUInt());
            } else if (paramName == "SecondsSinceMidnight") {
                dateTimeWidget->setSecondsSinceMidnight(paramValue.toDouble());
            } else if (paramName == "LocalOffset") {
                dateTimeWidget->setLocalOffset(paramValue.toInt());
            }
        }
    } else if (Pgn == 129283) {
        // Cast widget to CrossTrack129283* and call the initialize function
        auto *crossTrackWidget = qobject_cast<CrossTrack129283 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                crossTrackWidget->setSID(paramValue.toUInt());
            } else if (paramName == "XTEMode") {
                crossTrackWidget->setXTEMode(static_cast<tN2kXTEMode>(paramValue.toUInt()));
            } else if (paramName == "NavigationTerminated") {
                crossTrackWidget->setNavigationTerminated(paramValue.toUInt());
            } else if (paramName == "XTE") {
                crossTrackWidget->setXTE(paramValue.toDouble());
            }
        }
    } else if (Pgn == 129284) {
        // Cast widget to NavigationData129284* and call the initialize function
        auto *navigationDataWidget = qobject_cast<Navigation129284 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                navigationDataWidget->setSID(paramValue.toUInt());
            } else if (paramName == "DistanceToWaypoint") {
                navigationDataWidget->setDistanceToWaypoint(paramValue.toDouble());
            } else if (paramName == "BearingReference") {
                navigationDataWidget->setBearingReference(static_cast<tN2kHeadingReference>(paramValue.toDouble()));
            } else if (paramName == "PerpendicularCrossed") {
                navigationDataWidget->setPerpendicularCrossed(paramValue.toUInt());
            } else if (paramName == "ArrivalCircleEntered") {
                navigationDataWidget->setArrivalCircleEntered(paramValue.toUInt());
            } else if (paramName == "CalculationType") {
                navigationDataWidget->setCalculationType(static_cast<tN2kDistanceCalculationType>(paramValue.toUInt()));
            } else if (paramName == "ETATime") {
                navigationDataWidget->setETATime(paramValue.toDouble());
            } else if (paramName == "ETADate") {
                navigationDataWidget->setETADate(paramValue.toUInt());
            } else if (paramName == "BearingOriginToDestinationWaypoint") {
                navigationDataWidget->setBearingOriginToDestinationWaypoint(paramValue.toDouble());
            } else if (paramName == "BearingPositionToDestinationWaypoint") {
                navigationDataWidget->setBearingPositionToDestinationWaypoint(paramValue.toDouble());
            } else if (paramName == "OriginWaypointNumber") {
                navigationDataWidget->setOriginWaypointNumber(paramValue.toUInt());
            } else if (paramName == "DestinationWaypointNumber") {
                navigationDataWidget->setDestinationWaypointNumber(paramValue.toUInt());
            } else if (paramName == "DestinationLatitude") {
                navigationDataWidget->setDestinationLatitude(paramValue.toDouble());
            } else if (paramName == "DestinationLongitude") {
                navigationDataWidget->setDestinationLongitude(paramValue.toDouble());
            } else if (paramName == "WaypointClosingVelocity") {
                navigationDataWidget->setWaypointClosingVelocity(paramValue.toDouble());
            }
        }
    } else if (Pgn == 130312) {
        // Cast widget to TemperatureWidget130312* and call the initialize function
        auto *temperatureWidget = qobject_cast<TemperatureWidget130312 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "TemperatureInstance") {
                temperatureWidget->setTemperatureInstance(paramValue.toUInt());
            } else if (paramName == "SID") {
                temperatureWidget->setSID(paramValue.toUInt());
            } else if (paramName == "TemperatureSource") {
                temperatureWidget->setTemperatureSource(static_cast<tN2kTempSource>(paramValue.toUInt()));
            } else if (paramName == "ActualTemperature") {
                temperatureWidget->setActualTemperature(paramValue.toDouble());
            } else if (paramName == "SetTemperature") {
                temperatureWidget->setSetTemperature(paramValue.toDouble());
            }
        }
    } else if (Pgn == 130316) {
        // Cast widget to TemperatureExtended130316* and call the initialize function
        auto *temperatureExtendedWidget = qobject_cast<TemperatureExtended130316 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "TempInstance") {
                temperatureExtendedWidget->setTempInstance(paramValue.toUInt());
            } else if (paramName == "SID") {
                temperatureExtendedWidget->setSID(paramValue.toUInt());
            } else if (paramName == "TempSource") {
                temperatureExtendedWidget->setTempSource(static_cast<tN2kTempSource>(paramValue.toUInt()));
            } else if (paramName == "ActualTemperature") {
                temperatureExtendedWidget->setActualTemperature(paramValue.toDouble());
            } else if (paramName == "SetTemperature") {
                temperatureExtendedWidget->setSetTemperature(paramValue.toDouble());
            }
        }
    } else if (Pgn == 127488) {
        // Cast widget to EngineRapid127488* and call the initialize function
        auto *engineRapidWidget = qobject_cast<EngineRapid127488 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "EngineInstance") {
                engineRapidWidget->setEngineInstance(paramValue.toUShort());
            } else if (paramName == "EngineSpeed") {
                engineRapidWidget->setEngineSpeed(paramValue.toDouble());
            } else if (paramName == "EngineBoostPressure") {
                engineRapidWidget->setEngineBoostPressure(paramValue.toDouble());
            } else if (paramName == "EngineTiltTrim") {
                engineRapidWidget->setEngineTiltTrim(paramValue.toInt());
            }
        }
    } else if (Pgn == 127489) {
        // Cast widget to EngineDynamic127489* and call the initialize function
        auto *engineDynamicWidget = qobject_cast<EngineDynamic127489 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "EngineInstance") {
                engineDynamicWidget->setEngineInstance(paramValue.toUShort());
            } else if (paramName == "EngineOilPress") {
                engineDynamicWidget->setEngineOilPress(paramValue.toDouble());
            } else if (paramName == "EngineOilTemp") {
                engineDynamicWidget->setEngineOilTemp(paramValue.toDouble());
            } else if (paramName == "EngineCoolantTemp") {
                engineDynamicWidget->setEngineCoolantTemp(paramValue.toDouble());
            } else if (paramName == "AltenatorVoltage") {
                engineDynamicWidget->setAltenatorVoltage(paramValue.toDouble());
            } else if (paramName == "FuelRate") {
                engineDynamicWidget->setFuelRate(paramValue.toDouble());
            } else if (paramName == "EngineHours") {
                engineDynamicWidget->setEngineHours(paramValue.toDouble());
            } else if (paramName == "EngineCoolantPress") {
                engineDynamicWidget->setEngineCoolantPress(paramValue.toDouble());
            } else if (paramName == "EngineFuelPress") {
                engineDynamicWidget->setEngineFuelPress(paramValue.toDouble());
            } else if (paramName == "EngineLoad") {
                engineDynamicWidget->setEngineLoad(paramValue.toDouble());
            } else if (paramName == "EngineTorque") {
                engineDynamicWidget->setEngineTorque(paramValue.toDouble());
            } else if (paramName == "Status1") {
                // Convert paramValue to uint16_t and set the tN2kEngineDiscreteStatus1 status
                tN2kEngineDiscreteStatus1 status1;
                status1.Status = paramValue.toUShort();                 // Convert the string to ushort (uint16_t)
                engineDynamicWidget->setEngineDiscreteStatus1(status1); // Set the status
            } else if (paramName == "Status2") {
                // Convert paramValue to uint16_t and set the tN2kEngineDiscreteStatus2 status
                tN2kEngineDiscreteStatus2 status2;
                status2.Status = paramValue.toUShort();                 // Convert the string to ushort (uint16_t)
                engineDynamicWidget->setEngineDiscreteStatus2(status2); // Set the status
            }
        }
    } else if (Pgn == 127493) {
        // Cast widget to TransmissionDynamic127493* and call the initialize function
        auto *transmissionDynamicWidget = qobject_cast<TransmissionDynamic127493 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "EngineInstance") {
                transmissionDynamicWidget->setEngineInstance(paramValue.toUShort());
            } else if (paramName == "TransmissionGear") {
                transmissionDynamicWidget->setTransmissionGear(static_cast<tN2kTransmissionGear>(paramValue.toInt()));
            } else if (paramName == "OilPressure") {
                transmissionDynamicWidget->setOilPressure(paramValue.toDouble());
            } else if (paramName == "OilTemperature") {
                transmissionDynamicWidget->setOilTemperature(paramValue.toDouble());
            } else if (paramName == "DiscreteStatus1") {
                transmissionDynamicWidget->setDiscreteStatus1(paramValue.toUShort());
            }
        }
    } else if (Pgn == 0) {
        // Spacer Widgets is PGN 0
        // Cast widget to SpacerWidget* and call the initialize function
        auto *spacerWidget = qobject_cast<Spacer000000 *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "Rows") {
                spacerWidget->setRows(paramValue.toInt());
            } else if (paramName == "ShowTitleRow") {
                spacerWidget->setShowTitleRow(helper::QStringToBool(paramValue));
            }
        }
    } else if (Pgn == -129033) {
        // Generator Widgets have negative PGNs
        // Cast widget to GeneratorDateTime* and call the initialize function
        auto *generatorDateTimeWidget = qobject_cast<GeneratorDateTime *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                generatorDateTimeWidget->setSID(paramValue.toUInt());
            } else if (paramName == "DaysSince1970") {
                generatorDateTimeWidget->setDaysSince1970(paramValue.toUInt());
            } else if (paramName == "SecondsSinceMidnight") {
                generatorDateTimeWidget->setSecondsSinceMidnight(paramValue.toDouble());
            } else if (paramName == "LocalOffset") {
                generatorDateTimeWidget->setLocalOffset(paramValue.toInt());
            }
        }
    } else if (Pgn == -129025) {
        // Cast widget to GeneratorVesselMovement* and call the initialize function
        auto *generatorVesselMovementWidget = qobject_cast<GeneratorVesselMovement *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "SID") {
                generatorVesselMovementWidget->setSID(paramValue.toUInt());
            } else if (paramName == "Latitude") {
                generatorVesselMovementWidget->setLatitude(paramValue.toDouble());
            } else if (paramName == "Longitude") {
                generatorVesselMovementWidget->setLongitude(paramValue.toDouble());
            } else if (paramName == "Altitude") {
                generatorVesselMovementWidget->setAltitude(paramValue.toDouble());
            } else if (paramName == "HeadingReference") {
                generatorVesselMovementWidget->setHeadingReference(static_cast<tN2kHeadingReference>(paramValue.toUInt()));
            } else if (paramName == "Heading") {
                generatorVesselMovementWidget->setHeading(paramValue.toDouble());
            } else if (paramName == "Deviation") {
                generatorVesselMovementWidget->setDeviation(paramValue.toDouble());
            } else if (paramName == "Variation") {
                generatorVesselMovementWidget->setVariation(paramValue.toDouble());
            } else if (paramName == "COG") {
                generatorVesselMovementWidget->setCOG(paramValue.toDouble());
            } else if (paramName == "SOG") {
                generatorVesselMovementWidget->setSOG(paramValue.toDouble());
            } else if (paramName == "RateOfTurn") {
                generatorVesselMovementWidget->setRateOfTurn(paramValue.toDouble());
            } else if (paramName == "Yaw") {
                generatorVesselMovementWidget->setYaw(paramValue.toDouble());
            } else if (paramName == "Pitch") {
                generatorVesselMovementWidget->setPitch(paramValue.toDouble());
            } else if (paramName == "Roll") {
                generatorVesselMovementWidget->setRoll(paramValue.toDouble());
            } else if (paramName == "Heave") {
                generatorVesselMovementWidget->setHeave(paramValue.toDouble());
            } else if (paramName == "RudderPosition") {
                generatorVesselMovementWidget->setRudderPosition(paramValue.toDouble());
            }
        }
    } else if (Pgn == -127489) {
        // Cast widget to GeneratorEngineWidget* and call the initialize function
        auto *generatorEngineWidget = qobject_cast<GeneratorEngine *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "EngineInstance") {
                generatorEngineWidget->setEngineInstance(paramValue.toUShort());
            } else if (paramName == "EngineSpeed") {
                generatorEngineWidget->setEngineSpeed(paramValue.toDouble());
            } else if (paramName == "EngineBoostPressure") {
                generatorEngineWidget->setEngineBoostPressure(paramValue.toDouble());
            } else if (paramName == "EngineTiltTrim") {
                generatorEngineWidget->setEngineTiltTrim(paramValue.toInt());
            } else if (paramName == "EngineOilPress") {
                generatorEngineWidget->setEngineOilPress(paramValue.toDouble());
            } else if (paramName == "EngineOilTemp") {
                generatorEngineWidget->setEngineOilTemp(paramValue.toDouble());
            } else if (paramName == "EngineCoolantTemp") {
                generatorEngineWidget->setEngineCoolantTemp(paramValue.toDouble());
            } else if (paramName == "AltenatorVoltage") {
                generatorEngineWidget->setAltenatorVoltage(paramValue.toDouble());
            } else if (paramName == "FuelRate") {
                generatorEngineWidget->setFuelRate(paramValue.toDouble());
            } else if (paramName == "EngineHours") {
                generatorEngineWidget->setEngineHours(paramValue.toDouble());
            } else if (paramName == "EngineCoolantPress") {
                generatorEngineWidget->setEngineCoolantPress(paramValue.toDouble());
            } else if (paramName == "EngineFuelPress") {
                generatorEngineWidget->setEngineFuelPress(paramValue.toDouble());
            } else if (paramName == "EngineLoad") {
                generatorEngineWidget->setEngineLoad(paramValue.toDouble());
            } else if (paramName == "EngineTorque") {
                generatorEngineWidget->setEngineTorque(paramValue.toDouble());
            } else if (paramName == "Status1") {
                // Convert paramValue to uint16_t and set the tN2kEngineDiscreteStatus1 status
                tN2kEngineDiscreteStatus1 status1;
                status1.Status = paramValue.toUShort();                 // Convert the string to ushort (uint16_t)
                generatorEngineWidget->setEngineDiscreteStatus1(status1); // Set the status
            } else if (paramName == "Status2") {
                // Convert paramValue to uint16_t and set the tN2kEngineDiscreteStatus2 status
                tN2kEngineDiscreteStatus2 status2;
                status2.Status = paramValue.toUShort();                 // Convert the string to ushort (uint16_t)
                generatorEngineWidget->setEngineDiscreteStatus2(status2); // Set the status
            }
        }
    } else if (Pgn == -127493) {
        // Cast widget to GeneratorTransmissionWidget* and call the initialize function
        auto *generatorTransLowerUnitWidget = qobject_cast<GeneratorTransLowerUnit *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];

            if (paramName == "EngineInstance") {
                generatorTransLowerUnitWidget->setEngineInstance(paramValue.toUShort());
            } else if (paramName == "TransmissionGear") {
                generatorTransLowerUnitWidget->setTransmissionGear(static_cast<tN2kTransmissionGear>(paramValue.toInt()));
            } else if (paramName == "OilPressure") {
                generatorTransLowerUnitWidget->setOilPressure(paramValue.toDouble());
            } else if (paramName == "OilTemperature") {
                generatorTransLowerUnitWidget->setOilTemperature(paramValue.toDouble());
            } else if (paramName == "DiscreteStatus1") {
                generatorTransLowerUnitWidget->setDiscreteStatus1(paramValue.toUShort());
            }
        }
    } else if (Pgn == -130311) {
        // Cast widget to GeneratorWeatherWidget* and call the initialize function
        auto *generatorWeatherWidget = qobject_cast<GeneratorWeather *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];
            for (const auto& row : GeneratorWeather::additionalParameters())
                if (row[0] == paramName) generatorWeatherWidget->setProperty(paramName.toLatin1().constData(), paramValue.toDouble());

            if (paramName == "SID") {
                generatorWeatherWidget->setSID(paramValue.toUInt());
            } else if (paramName == "AirTemperature") {
                generatorWeatherWidget->setAirTemperature(paramValue.toDouble());
            } else if (paramName == "SeaTemperature") {
                generatorWeatherWidget->setSeaTemperature(paramValue.toDouble());
            } else if (paramName == "Humidity") {
                generatorWeatherWidget->setHumidity(paramValue.toDouble());
            } else if (paramName == "AtmosphericPressure") {
                generatorWeatherWidget->setAtmosphericPressure(paramValue.toDouble());
            } else if (paramName == "WindSpeed") {
                generatorWeatherWidget->setWindSpeed(paramValue.toDouble());
            } else if (paramName == "WindAngle") {
                generatorWeatherWidget->setWindAngle(paramValue.toDouble());
            } else if (paramName == "TemperatureSource") {
                generatorWeatherWidget->setTemperatureSource(static_cast<tN2kTempSource>(paramValue.toUInt()));
            } else if (paramName == "HumiditySource") {
                generatorWeatherWidget->setHumiditySource(static_cast<tN2kHumiditySource>(paramValue.toUInt()));
            } else if (paramName == "Latitude") {
                generatorWeatherWidget->setLatitude(paramValue.toDouble());
            } else if (paramName == "Longitude") {
                generatorWeatherWidget->setLongitude(paramValue.toDouble());
            }
        }
    } else if (Pgn == SNT_GGA) {
        // Cast widget to GlobalPositioningGGA* and call the initialize function
        auto *globalPositioningGGAWidget = qobject_cast<GlobalPositioningGGA *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        // Loop through the params and set the corresponding properties on the widget
        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];
            if (paramName == "GPSTime") {
                globalPositioningGGAWidget->setGPSTime(paramValue.toDouble());
            } else if (paramName == "Latitude") {
                globalPositioningGGAWidget->setLatitude(paramValue.toDouble());
            } else if (paramName == "Longitude") {
                globalPositioningGGAWidget->setLongitude(paramValue.toDouble());
            } else if (paramName == "GPSQualityIndicator") {
                globalPositioningGGAWidget->setGPSQualityIndicator(paramValue.toInt());
            } else if (paramName == "SatelliteCount") {
                globalPositioningGGAWidget->setSatelliteCount(paramValue.toInt());
            } else if (paramName == "HDOP") {
                globalPositioningGGAWidget->setHDOP(paramValue.toDouble());
            } else if (paramName == "Altitude") {
                globalPositioningGGAWidget->setAltitude(paramValue.toDouble());
            } else if (paramName == "GeoidalSeparation") {
                globalPositioningGGAWidget->setGeoidalSeparation(paramValue.toDouble());
            } else if (paramName == "DGPSAge") {
                globalPositioningGGAWidget->setDGPSAge(paramValue.toDouble());
            } else if (paramName == "DGPSReferenceStationID") {
                globalPositioningGGAWidget->setDGPSReferenceStationID(paramValue.toInt());
            }
        }

    } else if (Pgn == SNT_RMC) {
        // Cast widget to RecommendedMinGnssRMC* and call the initialize function
        auto *recommendedMinGnssRMCWidget = qobject_cast<RecommendedMinGnssRMC *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];
            if (paramName == "GPSTime") {
                recommendedMinGnssRMCWidget->setGPSTime(paramValue.toDouble());
            } else if (paramName == "Status") {
                recommendedMinGnssRMCWidget->setStatus(paramValue.toStdString()[0]);
            } else if (paramName == "Latitude") {
                recommendedMinGnssRMCWidget->setLatitude(paramValue.toDouble());
            } else if (paramName == "Longitude") {
                recommendedMinGnssRMCWidget->setLongitude(paramValue.toDouble());
            } else if (paramName == "TrueCOG") {
                recommendedMinGnssRMCWidget->setTrueCOG(paramValue.toDouble());
            } else if (paramName == "SOG") {
                recommendedMinGnssRMCWidget->setSOG(paramValue.toDouble());
            } else if (paramName == "DaysSince1970") {
                recommendedMinGnssRMCWidget->setDaysSince1970(paramValue.toULong());
            } else if (paramName == "Variation") {
                recommendedMinGnssRMCWidget->setVariation(paramValue.toDouble());
            } else if (paramName == "DateTime") {
                recommendedMinGnssRMCWidget->setDateTime(paramValue.toULong());
            }
        }

    } else if (Pgn == SNT_HDG) {
        // Cast widget to HeadingHDG* and call the initialize function
        auto *headingHDGWidget = qobject_cast<HeadingHDG *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];
            if (paramName == "Heading") {
                headingHDGWidget->setHeading(paramValue.toDouble());
            } else if (paramName == "Deviation") {
                headingHDGWidget->setDeviation(paramValue.toDouble());
            } else if (paramName == "Variation") {
                headingHDGWidget->setVariation(paramValue.toDouble());
            }
        }

    } else if (Pgn == SNT_HDT) {
        // Cast widget to HeadingHDT* and call the initialize function
        auto *headingHDTWidget = qobject_cast<TrueHeadingHDT *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];
            if (paramName == "Heading") {
                headingHDTWidget->setHeading(paramValue.toDouble());
            }
        }
    } else if (Pgn == SNT_GLL) {
        // Cast widget to GeographicPositionGLL* and call the initialize function
        auto *geographicPositionGLLWidget = qobject_cast<GeographicPositionGLL *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];
            if (paramName == "Status") {
                geographicPositionGLLWidget->setStatus(paramValue.isEmpty() ? 'V' : paramValue.at(0).toLatin1());
            } else if (paramName == "GPSTime") {
                geographicPositionGLLWidget->setGPSTime(paramValue.toDouble());
            } else if (paramName == "Latitude") {
                geographicPositionGLLWidget->setLatitude(paramValue.toDouble());
            } else if (paramName == "Longitude") {
                geographicPositionGLLWidget->setLongitude(paramValue.toDouble());
            }
        }
    } else if (Pgn == SNT_VTG) {
        // Cast widget to CogSogVTG* and call the initialize function
        auto *cogSogVTGWidget = qobject_cast<CogSogVTG *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];
            if (paramName == "TrueCOG") {
                cogSogVTGWidget->setTrueCOG(paramValue.toDouble());
            } else if (paramName == "MagneticCOG") {
                cogSogVTGWidget->setMagneticCOG(paramValue.toDouble());
            } else if (paramName == "SOG") {
                cogSogVTGWidget->setSOG(paramValue.toDouble());
            }
        }
    } else if (Pgn == SNT_ZDA) {
        // Cast widget to DateTimeZDA* and call the initialize function
        auto *dateTimeZDAWidget = qobject_cast<DateTimeZDA *>(widget);

        // Retrieve the model data for the instance idX
        QList<QList<QString>> params = model->getPgnParams(idX);

        for (const auto &param : params) {
            QString paramName = param[0];
            QString paramValue = param[1];
            if (paramName == "GPSTime") {
                dateTimeZDAWidget->setGPSTime(paramValue.toDouble());
            } else if (paramName == "GPSDay") {
                dateTimeZDAWidget->setGPSDay(paramValue.toInt());
            } else if (paramName == "GPSMonth") {
                dateTimeZDAWidget->setGPSMonth(paramValue.toInt());
            } else if (paramName == "GPSYear") {
                dateTimeZDAWidget->setGPSYear(paramValue.toInt());
            } else if (paramName == "LZD") {
                dateTimeZDAWidget->setLZD(paramValue.toInt());
            } else if (paramName == "LZMD") {
                dateTimeZDAWidget->setLZMD(paramValue.toInt());
            }
        }
    }
    else {
        qWarning() << "PGN not supported for initialization" << Pgn;
        return;
    }
}

