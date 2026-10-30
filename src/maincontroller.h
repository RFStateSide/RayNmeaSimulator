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

#ifndef MAINCONTROLLER_H
#define MAINCONTROLLER_H
#include "receivedroutes.h"

#include <QComboBox>
#include <QFile>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QObject>
#include <QPair>
#include <QThread>
#include <QPointer>
#include <QMetaMethod>
#include <functional>
#include <type_traits>
#include <tuple>
#include <QVariantMap>
#include "datastructs.h"
#include "enginedynamic127489.h"
#include "enginerapid127488.h"
#include "generatordatetime.h"
#include "generatorengine.h"
#include "generatortranslowerunit.h"
#include "generatorvesselmovement.h"

#include "generatorweather.h"
#include "nmeasenderworker.h"
#include "pgntablecontroller.h"
#include "pgntreecontroller.h"
#include "transmissiondynamic127493.h"

struct SubFormLayoutState
{
    QGridLayout *gridLayout;
    int lastRow;
    int lastColumn;
    QSet<QPair<int, int>> occupiedCells;
};

struct WidgetConfig
{
    int idX;                             // Unique identifier for the widget
    QString widgetType;                  // Type of the widget (e.g., "EngineWidget", "OutsideEnvironmental130310")
    QString label;                       // Label or name for the widget
    QString pgn;                         // PGN as a string (can be negative for generator widgets)
    int interval;                        // Update interval for the widget in milliseconds
    int row = -1;                        // Missing legacy coordinates use automatic placement
    int column = -1;
    int rowSpan;                         // Number of rows to span
    int columnSpan;                      // Number of columns to span
    QList<QList<QString>> parameters;    // List of parameters for the widget, in QList<QList<QString>> format
    QList<QPair<double, double>> ranges; // List of min-max ranges for each parameter

    QVariantMap properties; // Additional properties as key-value pairs, if needed
};

class VesselElectricalSystem;

class MainController : public QObject
{
    Q_OBJECT

public:
    ReceivedRoutes receivedRoutes;
    std::function<ReceivedRoutes::Route(QWidget*)> activeRouteChooser;
    explicit MainController(
        PgnTableController *pgnTableCtrl, PgnTreeController *pgnTreeCtrl, QMdiArea *mdiArea, NmeaSenderWorker *nmeaSenderWorker, QObject *parent = nullptr);
    ~MainController() override;

    // Add sub-windows for each group in the QMdiArea
    void setNumberOfWidgetWide(int columns);
    void addSubWindowForGroup(const QString &groupName, const QString &groupDescription);
    void addComboBoxItems(const QString &groupName, QComboBox &comboBox);
    void addWidgetToSubForm(QWidget *widget, const QString &groupName, int row = -1, int column = -1);
    QList<QWidget *> *getListWidgetsOnSubForm(const QString &groupName);
    // Initialize widget from model (model N2kMessage parameters)
    void initializeWidgetFromModel(QWidget *widget, int Pgn, PgnTableModel *model, int idX);
    // Data binding methods
    void bindWidgetToModel(QWidget *widget, int Pgn, PgnTableModel *model, int idX); // Also sets the widget properties to the model values
    void addGeneratorWidget(const QString &selectedItem);
    void EnableNewWidgets(bool isEnable = true);

    // Layout methods for saving and loading layout configurations
    QJsonObject captureLayout() const;
    bool validateDocument(const QJsonObject &root, QString *error = nullptr) const { return validateLayout(root,error); }
    bool loadLayoutDocument(const QJsonObject &root, const QString &identity,
                            const std::function<bool(const QString &)> &approveReplacement = {});
    bool saveLayoutToJson(const QString &filePath);
    // Optional UI decision runs against the already validated in-memory target,
    // before replacement; standalone callers retain the existing confirmation.
    bool loadLayoutFromJson(const QString &filePath,
                            const std::function<bool(const QString &)> &approveReplacement = {});
    void addWidgetToLayoutConfig(const QString &groupName, const WidgetConfig &widgetConfig);
    QWidget *createWidgetFromConfig(const WidgetConfig &config);
    void restoreLayoutFromWidgetConfig(const WidgetConfig &config, QString groupName, QWidget *prepared = nullptr);

    // Layout methods for removing widgets
    void removeWidgetFromSubForm(QWidget *widget, const QString &groupName); // Also calls other remove methods
    void removeWidgetFromFromModel(int idX);
    void removeWidgetFromLayoutConfig(const QString &groupName, int idX);
    void removeAllWidgetsFromGroup(const QString &groupName);
    void removeAllWidgets();
    QStringList getGroupList();
    void clearPgnParameters();
    QList<QWidget*> layoutWidgets(const QString& group) const { return orderedWidgets(group); }
    QString layoutDisplayName() const { return m_layoutDisplayName; }
    QString layoutFilePath() const { return m_layoutFilePath; }
    QString layoutSaveName() const;
    bool layoutNeedsSave() const { return m_layoutStructurallyModified || (!m_hasLayoutBaseline && hasLayoutWidgets()); }
    bool hasLayoutWidgets() const;
    bool hasLayoutBaseline() const { return m_hasLayoutBaseline; }
    bool layoutStructurallyModified() const { return m_layoutStructurallyModified; }
    bool selectLayoutWidget(QWidget* widget, const QString& field = {});

    // Received PGN methods
    PgnAsCombobox createPgnAsCombobox(const QString &pgn);
    void setMode(Mode mode);
    void setOpenWeatherMapApiKey(const QString &apiKey);

public slots:
    void setUserPrefs(UserUnitPrefs userPrefs);
    void setMyVessel(const MyVessel &vessel);
    void setUserOptionPrefs(const UserOptionPrefs &userOptionPrefs);
    // Layout methods for adding widgets for receive mode
    void addRcvdPgn(const QString &pgn, const QStringList &paramList, const QString &source);
    void updateRcvdPgn(int idX, const QStringList &paramList);
    void addWidgetAsComboboxTextChanged(const QString &selectedItemPgn,
                                        const QString &selectedItemLabel,
                                        const QString &groupName,
                                        const QStringList &paramList = QStringList());

private slots:
    void onComboBoxTextChanged(const QString &selectedItem);
    void onWidgetInstanceValueChanged(int idX);

signals:
    void layoutAboutToReplace();
    void layoutReplaced();
    void layoutObjectsChanged();
    void layoutIdentityChanged();
    void layoutWidgetSelected(QWidget* widget);
    void ShowStatusBarMessage(QString message, int timeout);

private:
    VesselElectricalSystem* m_electrical = nullptr;
    quint64 m_bindingEpoch = 0;
    QString m_layoutDisplayName = "Layout";
    QString m_layoutFilePath; // Working identity; set only after successful load/save.
    bool m_hasLayoutBaseline = false, m_layoutStructurallyModified = false;
    bool m_receivingWidget = false;
    void noteLayoutStructureChange();
    bool m_rebindingOnly = false;
    bool m_promptingMovementDependencies = false;
    QList<QMetaObject::Connection> m_generatorConnections;
    struct BindingDiagnostic { QJsonObject fields; QObject *source; QObject *destination; bool ended=false; };
    QList<BindingDiagnostic> m_bindingDiagnostics;
    quint64 m_connectionNumber=0;
    void trackGeneratorConnection(const QMetaObject::Connection &,QObject *,QObject *,const QString &,const QString &);
    void bindingObjectDestroyed(QObject *);
    void bindingSnapshot();
    QMap<QObject*, QList<QPair<QPointer<QTimer>, int>>> m_suspendedTimers;
    template <typename... Args> QMetaObject::Connection connectModel(Args&&... args) {
        if (m_rebindingOnly) return {};
        return QObject::connect(std::forward<Args>(args)...);
    }
    template <typename SenderType, typename Signal, typename Receiver, typename Slot>
    QMetaObject::Connection connectWhileSenderAlive(SenderType *sender, Signal signal, Receiver *receiver, Slot slot,
                                                    Qt::ConnectionType type = Qt::AutoConnection, const QString &description = QStringLiteral("callback")) {
        if (!sender || !receiver || m_mode != Sender) return {};
        const auto epoch = m_bindingEpoch;
        auto connection = QObject::connect(sender, signal, receiver,
            [owner=QPointer<MainController>(this), guard=QPointer<SenderType>(sender), receiver, slot, epoch, type](auto... args) {
                if (!owner || !guard || owner->m_mode != Sender || owner->m_bindingEpoch != epoch) return;
                auto *base=qobject_cast<PgnWidgetBase*>(guard.data());
                if (base && !base->deliveryAllowed()) return;
                const auto revision=base ? base->dispatchEpoch() : 0;
                auto deliver=[owner,guard,receiver,slot,epoch,revision,values=std::make_tuple(args...)] {
                    if (!owner || !guard || owner->m_mode!=Sender || owner->m_bindingEpoch!=epoch) return;
                    auto *source=qobject_cast<PgnWidgetBase*>(guard.data());
                    if(source && (!source->deliveryAllowed() || source->dispatchEpoch()!=revision)) return;
                    std::apply([&](auto... value) {
                        if constexpr (std::is_member_function_pointer_v<Slot>) std::invoke(slot,receiver,value...);
                        else slot(value...);
                    },values);
                };
                if (type==Qt::QueuedConnection) QMetaObject::invokeMethod(receiver,deliver,Qt::QueuedConnection);
                else deliver();
            }, Qt::DirectConnection);
        trackGeneratorConnection(connection,sender,receiver,QString::fromLatin1(QMetaMethod::fromSignal(signal).methodSignature()),(type==Qt::QueuedConnection?QStringLiteral("queued: "):QStringLiteral("direct/auto: "))+description);
        return connection;
    }
    bool validateLayout(const QJsonObject &root, QString *error = nullptr) const;
    QList<QWidget*> orderedWidgets(const QString &group) const;
    void reflowLayouts();
    void placeWidgetFrame(QWidget *widget, QWidget *frame, SubFormLayoutState &state);
    void updateGridPresentation(SubFormLayoutState &state);
    void updateWidgetSpan(QWidget *widget);
    void rebuildGeneratorBindings();
    void invalidateGeneratorBindings(const QString &reason);
    void bindAisVesselMovement();
    void updateAisVesselMovement(const QStringList &fields = {});
    void prepareWidget(QWidget *widget);
    void consumeTankFuel(int engineInstance, double liters);
    void suspendGenerators(bool viewer);
    PgnTableController *pgnTableController;
    PgnTreeController *pgnTreeController;
    QMdiArea *mdiArea; // Reference to the QMdiArea
    //MainWindow *mainWindow = nullptr;

    GeneratorDateTime *generatorDateTimeWidget = nullptr;
    GeneratorVesselMovement *generatorVesselMovementWidget = nullptr;
    GeneratorEngine *generatorEngineWidget = nullptr;
    GeneratorTransLowerUnit *generatorTransLowerUnitWidget = nullptr;
    GeneratorWeather *generatorWeatherWidget = nullptr;
    QList<GeneratorEngine *> generatorEngineWidgets;
    QList<GeneratorTransLowerUnit *> generatorTransLowerUnitWidgets;

    // Maps for data binding between widgets and widgets instances
    // Map EngineInstance, (WidgetObject, boundState)
    QMap<int, QPair<GeneratorEngine *, bool>> generatorEngines;
    QMap<int, QPair<GeneratorTransLowerUnit *, bool>> generatorTransLowerUnits;
    QMap<int, QPair<GeneratorDateTime *, bool>> generatorDateTimes;
    QMap<int, QPair<GeneratorVesselMovement *, bool>> generatorVesselMovements;
    QMap<int, QPair<EngineRapid127488 *, bool>> engineRapids;
    QMap<int, QPair<EngineDynamic127489 *, bool>> engineDynamics;
    QMap<int, QPair<TransmissionDynamic127493 *, bool>> transmissionDynamics;

    NmeaSenderWorker *nmeaSenderWorker; // Pointer to the sender worker

    QMap<QComboBox *, QMdiSubWindow *> comboBoxSubWindowMap; // Map to store the combo box and sub

    // Helper function to create a new sub-window with a given group name and description
    QMdiSubWindow *createSubWindow(const QString &groupName, const QString &groupDescription);
    QMap<QString, SubFormLayoutState> subFormLayoutMap;

    int idX = 0; // Unique identifier for linking widget, table and tree
    int currentIdX = 0;
    int addedWidgetCount = 0; // Number of widgets restored from the layout configuration
    int8_t engineInstance = -1;
    UserUnitPrefs userPrefs;
    UserOptionPrefs userOptionPrefs;
    MyVessel myVessel;
    bool isNewWidgetsEnabled = true;
    int numberOfWidgetWide = 2;       // Number of columns for the grid layout
    int numberColumnsWidgetSpans = 3; // Number of columns a widget will span
    void bindWidgetsBasedOnEngineInstanceToGenerator(int engineInstance);
    void unBindWidgetsBasedOnEngineInstanceToGenerator(int engineInstance);
    void bindGeneratorVesselMovementToGeneratorEngine(QWidget *widget, int engineInstance);
    void unBindGeneratorVesselMovementToGeneratorEngine(QWidget *widget, int engineInstance);

    QMap<QString, QList<WidgetConfig>> layoutConfigMap;
    QMap<QString, QList<QWidget *>> groupWidgetsMap; // Maps groupName to a list of widgets in that group
    Mode m_mode = Sender;
    bool m_isWidgetsLoaded = false;
    bool m_restoringLayout = false;
    QString m_openWeatherMapApiKey = "";
};

#endif // MAINCONTROLLER_H
