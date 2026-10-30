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

#include "coordinateeditor.h"
#include "generatorweather.h"
#include <QMessageBox>
#include "n2khelper.h"
#include <QDynamicPropertyChangeEvent>
#include <QScopedValueRollback>
#include <QFormLayout>
#include <QTabWidget>
#include <QMetaEnum>
#include <QSignalBlocker>
#include <QGeoCoordinate>
#include <algorithm>

#include "ui_generatorweather.h"
#include "uihelper.h"

namespace {
// Application locality limits, not provider accuracy guarantees. A cycle already
// has a 60 s deadline; ordinary vessel motion must not cancel every response.
constexpr double maximumCycleMovement = 5000; // metres from the request snapshot
double observationRadius(const QString &provider) {
    return provider == "NOAA NDBC" ? 80000 : 25000; // station search / point weather
}
bool withinWeatherArea(double latitude, double longitude, double otherLatitude, double otherLongitude, double metres) {
    const QGeoCoordinate position(latitude, longitude), other(otherLatitude, otherLongitude);
    return position.isValid() && other.isValid() && position.distanceTo(other) <= metres;
}
}

GeneratorWeather::GeneratorWeather(int id, QString label, QWidget *parent, QNetworkAccessManager *fixtureNetwork)
    : PgnWidgetBase(id, label, parent), ui(new Ui::GeneratorWeather),
      openWeatherMap(new OpenWeatherMap(this, QUrl("https://api.openweathermap.org/data/2.5/weather"), fixtureNetwork)),
      nationalDataBuoyCenter(new NationalDataBuoyCenter(this, QUrl("https://www.ndbc.noaa.gov/activestations.xml"),
          QUrl("https://www.ndbc.noaa.gov/data/realtime2/"), fixtureNetwork)) {
    ui->setupUi(this);
    setId(id);
    setPgn(-130311);
    setColumnSpan(3);
    setRowSpan(13);
    setMinimumWidth(ColumnSpan() * 100);
    setMinimumHeight(RowSpan() * 30);

    ui->labelTitle->setText("Generate " + label);
    ui->checkBoxEnabled->setToolTip(tr("Enable signals from this generator. Outbound transport output still requires explicit Enable Sending."));
    ui->comboBoxApiSource->setToolTip(tr("Choose a weather provider. NOAA stations supply different measurement subsets; unavailable observations are not invented."));
    ui->pushButtonFetchWeather->setToolTip(tr("Fetch weather for the current coordinates. Provider requests may fail; inspect the visible status and Application Output. Fetching does not enable Sending."));

    initilizeComboBoxes();
    initializeUnitsLimits();
    initializeClickableLabels();

    weatherStatus = new QLabel("No source selected. Manual values retained.", this);
    weatherStatus->setObjectName("weatherStatus");
    weatherStatus->setWordWrap(true);
    ui->verticalLayout_3->insertWidget(2, weatherStatus);
    connect(this, &GeneratorWeather::updateApiKey, openWeatherMap, &OpenWeatherMap::setApiKey);
    connect(this, &GeneratorWeather::fetchWeather, openWeatherMap, &OpenWeatherMap::fetchWeather);
    connect(this, &GeneratorWeather::getNearestStations, nationalDataBuoyCenter,
            &NationalDataBuoyCenter::getNearestStations);
    connect(this, &GeneratorWeather::fetchStationData, nationalDataBuoyCenter,
            &NationalDataBuoyCenter::fetchStationData);
    connect(openWeatherMap, &OpenWeatherMap::observationFetched, this, &GeneratorWeather::acceptObservation);
    connect(nationalDataBuoyCenter, &NationalDataBuoyCenter::observationFetched, this,
            &GeneratorWeather::acceptObservation);
    connect(nationalDataBuoyCenter, &NationalDataBuoyCenter::nearestStationsFound, this,
            &GeneratorWeather::nearestStationsFound);
    connect(openWeatherMap, &OpenWeatherMap::errorOccurred, this, &GeneratorWeather::handleWeatherError);
    connect(nationalDataBuoyCenter, &NationalDataBuoyCenter::errorOccurred, this,
            &GeneratorWeather::handleWeatherError);
    connect(this, &GeneratorWeather::LatitudeChanged, this, &GeneratorWeather::locationChanged);
    connect(this, &GeneratorWeather::LongitudeChanged, this, &GeneratorWeather::locationChanged);
    freshnessTimer.setParent(this);
    freshnessTimer.setSingleShot(true);
    connect(&freshnessTimer, &QTimer::timeout, this, &GeneratorWeather::expireMeasurements);
    ui->pushButtonFetchWeather->setEnabled(false);
    initializeObservationPages();
    initializeRefreshControls();
    connect(this,&PgnWidgetBase::enabledChanged,this,[this] {
        if(!isEnabled())cancelWeather("Generator disabled. Local weather updates stopped.");
        else scheduleRefresh();
        ui->pushButtonFetchWeather->setEnabled(generateActive() && ui->comboBoxApiSource->currentIndex()!=0);
    });
    setInitialGuiValues();
}
GeneratorWeather::~GeneratorWeather() {
    refreshTimer.stop(); cycleDeadline.stop();
    openWeatherMap->cancel();
    nationalDataBuoyCenter->cancel(NationalDataBuoyCenter::Cancellation::Destroyed);
    delete ui;
}
bool GeneratorWeather::generateActive() const {
    return isEnabled() && (!property("GeneratorModeActive").isValid() || property("GeneratorModeActive").toBool());
}
void GeneratorWeather::setWeatherStatus(const QString &text) {
    statusSummary = text;
    if (weatherStatus) {
        weatherStatus->setText(text);
        weatherStatus->setToolTip(text);
    }
}
void GeneratorWeather::invalidateObservation(const QString &text) {
    freshnessTimer.stop();
    QScopedValueRollback<bool> guard(applyingObservation, true);
    auto fields = observedFields;
    observedFields.clear();
    for (const auto &field : fields) origins[field].stale = true;
    { // Clear only provider-owned Weather values; Viewer instrument bindings are detached.
        for (const auto& row : additionalParameters())
            if (fields.contains(row[0])) setAdditionalValue(row[0], N2kDoubleNA);
        if (fields.contains("AirTemperature"))
            setAirTemperature(N2kDoubleNA);
        if (fields.contains("SeaTemperature"))
            setSeaTemperature(N2kDoubleNA);
        if (fields.contains("Humidity"))
            setHumidity(N2kDoubleNA);
        if (fields.contains("AtmosphericPressure"))
            setAtmosphericPressure(N2kDoubleNA);
        if (fields.contains("WindSpeed"))
            setWindSpeed(N2kDoubleNA);
        if (fields.contains("WindAngle"))
            setWindAngle(N2kDoubleNA);
    }
    setWeatherStatus(text);
}
void GeneratorWeather::cancelWeather(const QString &reason) {
    using C = NationalDataBuoyCenter::Cancellation;
    C cause = C::Explicit;
    if (reason.startsWith("Location changed")) cause = C::CoordinatesChanged;
    else if (reason.startsWith("No source") || reason.startsWith("Ready")) cause = C::SourceChanged;
    else if (reason.startsWith("Suspended") || reason.startsWith("Stopped")) cause = C::ModeChanged;
    else if (reason.startsWith("Fetching")) cause = C::Superseded;
    if (nationalDataBuoyCenter->hasActiveRequest() || findChild<QDialog*>("weatherStationPicker") ||
        (fetching && ui->comboBoxApiSource->currentIndex() == 2)) {
        qCInfo(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("workflow", selectedStation.id)
            << "result=cancelled reason=" + QString::fromLatin1(QMetaEnum::fromType<C>().valueToKey(int(cause)));
    }
    ++requestGeneration;
    selectedStation = Station{};
    if(auto* picker=findChild<QDialog*>("weatherStationPicker"))picker->reject();
    openWeatherMap->cancel();
    nationalDataBuoyCenter->cancel(cause);
    fetching = false;
    ui->pushButtonFetchWeather->setText("Fetch Weather");
    cycleDeadline.stop(); candidates.clear(); scheduledCycle = false;
    if (!retainWeather || !retainObservations() || !generateActive() || ui->comboBoxApiSource->currentIndex()==0)
        invalidateObservation(reason);
    else {
        for (const auto &field : observedFields) origins[field].retained = true;
        expireMeasurements(); setWeatherStatus(reason);
    }
    scheduleRefresh();
    updateProvenanceDisplay();
}
bool GeneratorWeather::event(QEvent *event) {
    if (event->type() == QEvent::DynamicPropertyChange &&
        static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "GeneratorModeActive" &&
        weatherStatus) {
        cancelWeather(generateActive() ? "Stopped — fetch when ready." : "Suspended (Viewer)");
    }
    return PgnWidgetBase::event(event);
}
void GeneratorWeather::acceptObservation(const WeatherObservation &incoming) {
    if (!generateActive() || ui->comboBoxApiSource->currentIndex() == 0) {
        if (incoming.provider == "NOAA NDBC") qCDebug(ndbcLog).noquote()
            << nationalDataBuoyCenter->diagnosticContext("apply", incoming.station) << "result=ignored-inactive";
        return;
    }
    const QString expected=ui->comboBoxApiSource->currentIndex()==1?"OpenWeatherMap":"NOAA NDBC";
    if(incoming.provider!=expected) {
        if (incoming.provider == "NOAA NDBC") qCDebug(ndbcLog).noquote()
            << nationalDataBuoyCenter->diagnosticContext("apply", incoming.station) << "result=ignored-source-changed";
        return;
    }
    if(expected=="NOAA NDBC" && !selectedStation.id.isEmpty() && incoming.station!=selectedStation.id) {
        qCDebug(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("apply", incoming.station) << "result=ignored-station-mismatch";
        return;
    }
    if (fetching) {
        const double latitude = expected == "NOAA NDBC" ? selectedStation.latitude : incoming.latitude;
        const double longitude = expected == "NOAA NDBC" ? selectedStation.longitude : incoming.longitude;
        if (!withinWeatherArea(Latitude(), Longitude(), cycleLatitude, cycleLongitude, maximumCycleMovement) ||
            !withinWeatherArea(cycleLatitude, cycleLongitude, latitude, longitude, observationRadius(expected))) {
            cancelWeather("Weather response outside request area — fetch again.");
            return;
        }
    }
    if (!incoming.fresh() && scheduledCycle && fetching) {
        qCWarning(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("fallback", incoming.station) << "result=stale-candidate";
        tryNextStation(); return;
    }
    m_observation = incoming;
    if (incoming.provider == "NOAA NDBC" && !selectedStation.id.isEmpty()) {
        m_observation.location = selectedStation.name;
        m_observation.latitude = selectedStation.latitude;
        m_observation.longitude = selectedStation.longitude;
    }
    ui->lineEditName->setText(m_observation.location +
                              (incoming.station.isEmpty() ? QString() : " (" + incoming.station + ")"));
    const QString identity =
        QString("%1; observed UTC %2; retrieved UTC %3; location %4, %5")
            .arg(incoming.provider,
                 incoming.observed.isValid() ? incoming.observed.toUTC().toString(Qt::ISODate) : "unknown",
                 incoming.retrieved.toUTC().toString(Qt::ISODate),
                 QString::number(m_observation.latitude, 'f', 5),
                 QString::number(m_observation.longitude, 'f', 5));
    ui->textEditDescription->setPlainText(identity + "\n" + incoming.description +
                                          "\nMissing: " + incoming.missing.join(", ") +
                                          "\nInvalid: " + incoming.invalid.join(", "));
    if (!incoming.fresh()) {
        if (incoming.provider == "NOAA NDBC") qCDebug(ndbcLog).noquote()
            << nationalDataBuoyCenter->diagnosticContext("apply", incoming.station) << "result=stale-rejected";
        finishFetch();
        if (retainObservations()) {
            for (const auto &field : observedFields) origins[field].retained = true;
            expireMeasurements(); setWeatherStatus("Stale response; previous fresh measurements retained.");
        }
        else invalidateObservation("Stale or undated observation — provider values unavailable; fetch again.");
        updateProvenanceDisplay(); return;
    }
    QScopedValueRollback<bool> guard(applyingObservation, true);
    acceptedObservationTime=incoming.observed;
    acceptedProvider=incoming.provider; acceptedStation=incoming.station;
    QStringList sampledFields;
    // Only metadata is retained separately; authoritative measurements stay in
    // the existing properties/model and use their ordinary binding/output paths.
    auto apply = [this, &incoming, &sampledFields](const QString& field, double value) {
        if (WeatherObservation::valid(value)) {
            sampledFields.append(field);
            const auto &o = m_observation;
            origins[field] = {o.provider, o.station, o.location, o.observed, o.retrieved,
                              o.latitude, o.longitude, o.maximumAgeSeconds, false, false};
            observedFields.insert(field);
            setProperty(field.toLatin1().constData(), value);
        } else if (observedFields.contains(field)) {
            if (retainObservations() && origins.value(field).fresh()) {
                origins[field].retained = true;
                qCDebug(ndbcLog).noquote() << "Weather field=" + field << "result=retained";
            } else {
                setProperty(field.toLatin1().constData(), N2kDoubleNA);
                observedFields.remove(field); origins[field].stale = true;
            }
        }
    };
    apply("AirTemperature", incoming.air);
    apply("SeaTemperature", incoming.sea);
    apply("Humidity", incoming.humidity);
    apply("AtmosphericPressure", incoming.pressure);
    apply("WindSpeed", incoming.windSpeed);
    apply("WindAngle", incoming.windAngle);
    apply("WaveHeight", incoming.waveHeight);
    apply("DominantWavePeriod", incoming.dominantWavePeriod);
    apply("AverageWavePeriod", incoming.averageWavePeriod);
    apply("MeanWaveDirection", incoming.meanWaveDirection);
    apply("WindGust", incoming.windGust);
    apply("DewPoint", incoming.dewPoint);
    apply("Visibility", incoming.visibility);
    apply("PressureTendency", incoming.pressureTendency);
    apply("Tide", incoming.tide);
    setWeatherStatus(incoming.missing.isEmpty() && incoming.invalid.isEmpty()
                         ? "Observation received."
                         : "Partial observation — some values unavailable.");
    weatherStatus->setToolTip(identity + "\nMissing: " + incoming.missing.join(", ") +
                              "\nInvalid: " + incoming.invalid.join(", "));
    emit parametersSampled(sampledFields);
    lastSuccess = QDateTime::currentDateTimeUtc();
    finishFetch();
    expireMeasurements();
    updateProvenanceDisplay();
}

void GeneratorWeather::initializeUnitsLimits()
{
    setCoordinateValidator(ui->lineEditLatitude, UserPrefs().latLongUnitIndex, true);
    setCoordinateValidator(ui->lineEditLongitude, UserPrefs().latLongUnitIndex, false);
    // kelvin, celsius, fahrenheit (-60 to 250)
    switch (UserPrefs().tempUnitIndex) {
    case 0:
        ui->labelUnitsAirTemperature->setText("K");
        ui->labelUnitsSeaTemperature->setText("K");
        temperatureValidator->setRange(213.15, 523.15, 2);
        break;
    case 1:
        ui->labelUnitsAirTemperature->setText("°C");
        ui->labelUnitsSeaTemperature->setText("°C");
        temperatureValidator->setRange(-60, 250, 1);
        break;
    case 2:
        ui->labelUnitsAirTemperature->setText("°F");
        ui->labelUnitsSeaTemperature->setText("°F");
        temperatureValidator->setRange(-76, 482, 1);
        break;
    }

    ui->lineEditAirTemperature->setValidator(temperatureValidator);
    ui->lineEditSeaTemperature->setValidator(temperatureValidator);

    // AtmosphericPressure hPa, inHg, mmHg, Pa, atm
    switch (UserPrefs().atmosphericPressureUnitIndex) {
    case 0:
        ui->labelUnitsAtmosphericPressure->setText("hPa");
        atmosphericPressureValidator->setRange(0, 1100, 1);
        break;
    case 1:
        ui->labelUnitsAtmosphericPressure->setText("inHg");
        atmosphericPressureValidator->setRange(0, 32.48, 2);
        break;
    case 2:
        ui->labelUnitsAtmosphericPressure->setText("mmHg");
        atmosphericPressureValidator->setRange(0, 825, 1);
        break;
    case 3:
        ui->labelUnitsAtmosphericPressure->setText("Pa");
        atmosphericPressureValidator->setRange(0, 110000, 0);
        break;
    case 4:
        ui->labelUnitsAtmosphericPressure->setText("atm");
        atmosphericPressureValidator->setRange(0, 1.0857, 4);
        break;
    }

    ui->lineEditAtmosphericPressure->setValidator(atmosphericPressureValidator);

    // Humidity (%) (0 to 100)
    humidityValidator->setRange(0, 100, 1);

    // WindSpeed m/s, km/h, mph, knots
    switch (UserPrefs().speedUnitIndex) {
    case 0:
        ui->labelUnitsWindSpeed->setText("m/s");
        windSpeedValidator->setRange(0, 75, 1); // 75 m/s = 270 km/h = 167.77 mph = 130.6 knots
        break;
    case 1:
        ui->labelUnitsWindSpeed->setText("km/h");
        windSpeedValidator->setRange(0, 260, 1);
        break;
    case 2:
        ui->labelUnitsWindSpeed->setText("mph");
        windSpeedValidator->setRange(0, 168, 1);
        break;
    case 3:
        ui->labelUnitsWindSpeed->setText("knots");
        windSpeedValidator->setRange(0, 130, 1);
        break;
    }

    ui->lineEditWindSpeed->setValidator(windSpeedValidator);

    // WindAngle (0 to 360)
    windAngleValidator->setRange(0, 360, 1);

    // "DD.DDDD", "DD°MM’SS", "DD°MM.MMM’"
    switch (UserPrefs().latLongUnitIndex) {
    case 0:
        ui->labelUnitLatitude->setText("DD.DDDDDD°");
        ui->labelUnitLongitude->setText("DD.DDDDDD°");
        break;
    case 1:
        ui->labelUnitLatitude->setText("DD°MM’SS”");
        ui->labelUnitLongitude->setText("DD°MM’SS”");
        break;
    case 2:
        ui->labelUnitLatitude->setText("DD°MM.MMM’");
        ui->labelUnitLongitude->setText("DD°MM.MMM’");
        break;
    }
}

void GeneratorWeather::initializeClickableLabels()
{
    connect(this, &GeneratorWeather::UserPrefsChanged, this, &GeneratorWeather::initializeUnitsLimits);

    // Reminder to promote labels to ClickableLabel for right click context menus to work
    ui->labelUnitsAirTemperature->setMenuItems(unitsListTemp);
    connect(ui->labelUnitsAirTemperature, &ClickableLabel::actionTriggered, this, &GeneratorWeather::ChangeTempUnits_triggered);

    ui->labelUnitsSeaTemperature->setMenuItems(unitsListTemp);
    connect(ui->labelUnitsSeaTemperature, &ClickableLabel::actionTriggered, this, &GeneratorWeather::ChangeTempUnits_triggered);

    ui->labelUnitsAtmosphericPressure->setMenuItems(unitsListAtmosphericPressure);
    connect(ui->labelUnitsAtmosphericPressure, &ClickableLabel::actionTriggered, this, &GeneratorWeather::ChangeAtmosphericPressureUnits_triggered);

    ui->labelUnitsWindSpeed->setMenuItems(unitsListSpeed);
    connect(ui->labelUnitsWindSpeed, &ClickableLabel::actionTriggered, this, &GeneratorWeather::ChangeSpeedUnits_triggered);

    ui->labelUnitLatitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLatitude, &ClickableLabel::actionTriggered, this, &GeneratorWeather::ChangeLatLongUnits_triggered);

    ui->labelUnitLongitude->setMenuItems(unitsListLatLong);
    connect(ui->labelUnitLongitude, &ClickableLabel::actionTriggered, this, &GeneratorWeather::ChangeLatLongUnits_triggered);
}

void GeneratorWeather::weatherFetched(
    const QString &locationName, const QString &description, double temperature, double humidity, double pressure, double windSpeed, double windDirection)
{
    if (property("GeneratorModeActive").isValid() && !property("GeneratorModeActive").toBool()) return;
    ui->lineEditName->setText(locationName);
    ui->textEditDescription->setText(description);
    setAirTemperature(temperature);
    setHumidity(humidity);
    setAtmosphericPressure(WeatherObservation::valid(pressure)?pressure * 100.0:N2kDoubleNA); // Weather providers report hPa; internal pressure is Pa.
    setWindSpeed(windSpeed);
    setWindAngle(windDirection);
}

void GeneratorWeather::nearestStationsFound(const QList<Station> &nearestStations)
{
    if(!generateActive() || ui->comboBoxApiSource->currentIndex()!=2 || !fetching)return;
    if (scheduledCycle) {
        candidates.clear(); QSet<QString> ids;
        // Preserve distance order within availability groups. The NOAA met hint
        // is a preference, not proof of feed validity or a permanent exclusion.
        auto ordered = nearestStations;
        auto rank=[](const Station &s) { return s.standardMet==1 ? 0 : s.standardMet<0 ? 1 : 2; };
        std::stable_sort(ordered.begin(),ordered.end(),[&](const Station &a,const Station &b){return rank(a)<rank(b);});
        for (const auto &station : ordered) {
            if (ids.contains(station.id)) continue;
            ids.insert(station.id); candidates.append(station);
            if (candidates.size()==3) break;
        }
        tryNextStation(); return;
    }
    if(nearestStations.isEmpty()) {
        qCWarning(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("selection") << "result=no-nearby-stations";
        handleWeatherError("No NOAA meteorological stations within 80 km."); return;
    }
    qCDebug(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("selection") << "result=awaiting-selection candidates=" + QString::number(nearestStations.size());
    const auto generation=requestGeneration;setWeatherStatus("Choose a NOAA station.");
    QPointer<GeneratorWeather> self(this);
    const QString id=UiHelper::ShowStationPicker("Nearest Stations","Select a station",nearestStations,this);
    if(!self)return;
    if(generation!=requestGeneration||!generateActive()) {
        qCDebug(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("selection") << "result=ignored-superseded-selection";
        return;
    }
    if(id.isEmpty()) {
        qCInfo(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("selection") << "result=cancelled reason=explicit";
        cancelWeather("Station selection cancelled."); return;
    }
    qCDebug(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("selection", id) << "result=selected";
    for(const auto& station:nearestStations)if(station.id==id){selectedStation=station;m_stationName=station.name;break;}
    setWeatherStatus("Fetching NOAA observation…");emit fetchStationData(id);
}

void GeneratorWeather::dataFetched(const QString &description,
                                   double airTemperature,
                                   double waterTemperature,
                                   double waveHeight,
                                   double windSpeed,
                                   double windDirection,
                                   double pressure,
                                   double humidity)
{
    if (property("GeneratorModeActive").isValid() && !property("GeneratorModeActive").toBool()) return;
    setWaveHeight(waveHeight);
    ui->lineEditName->setText(m_stationName);
    ui->textEditDescription->setText(description);
    setAirTemperature(airTemperature);
    setSeaTemperature(waterTemperature);
    setHumidity(humidity);
    setAtmosphericPressure(WeatherObservation::valid(pressure)?pressure * 100.0:N2kDoubleNA); // Weather providers report hPa; internal pressure is Pa.
    setWindSpeed(windSpeed);
    setWindAngle(windDirection);
}

void GeneratorWeather::handleWeatherError(const QString &errorMessage)
{
    if (scheduledCycle && fetching && nationalDataBuoyCenter->stationFeedUnavailable() && !selectedStation.id.isEmpty()) {
        qCInfo(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("fallback", selectedStation.id) << "result=try-next-station";
        tryNextStation(); return;
    }
    QString message = errorMessage;
    if (!scheduledCycle && ui->comboBoxApiSource->currentIndex()==2 && !selectedStation.id.isEmpty())
        message = QString("NOAA station %1: %2%3").arg(selectedStation.id,
            nationalDataBuoyCenter->stationFeedUnavailable() ? "Observation feed unavailable; choose another station with Fetch Weather. " : "", errorMessage);
    finishFetch();
    if (retainObservations()) {
        for (const auto &field : observedFields) origins[field].retained = true;
        expireMeasurements(); setWeatherStatus(message + " Previous fresh measurements retained where available."); }
    else invalidateObservation(message);
    updateProvenanceDisplay();
}

void GeneratorWeather::onTimerTimeout()
{
    double converted = 0.0;
    switch (UserPrefs().latLongUnitIndex) {
    case 0:
        converted = m_ValueToEmit.toDouble();
        break;
    case 1:
        converted = Convert::DMSToDecimal(m_ValueToEmit, m_SignalToEmit == "LatitudeChanged");
        break;
    case 2:
        converted = Convert::DMMToDecimal(m_ValueToEmit, m_SignalToEmit == "LatitudeChanged");
        break;
    }

    if (m_SignalToEmit == "LatitudeChanged") {
        m_Latitude = converted;
        emit LatitudeChanged();
    } else if (m_SignalToEmit == "LongitudeChanged") {
        m_Longitude = converted;
        emit LongitudeChanged();
    } else if (m_SignalToEmit == "AirTemperatureChanged") {
        setAirTemperature(N2kHelper::convertUserTemperatureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs()));
    } else if (m_SignalToEmit == "SeaTemperatureChanged") {
        setSeaTemperature(N2kHelper::convertUserTemperatureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs()));
    } else if (m_SignalToEmit == "HumidityChanged") {
        setHumidity(m_ValueToEmit.toDouble());
    } else if (m_SignalToEmit == "AtmosphericPressureChanged") {
        setAtmosphericPressure(N2kHelper::convertUserAtmosphericPressureToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs()));
    } else if (m_SignalToEmit == "WindSpeedChanged") {
        setWindSpeed(N2kHelper::convertUserSpeedToN2kUnits(m_ValueToEmit.toDouble(), UserPrefs()));
    } else if (m_SignalToEmit == "WindAngleChanged") {
        setWindAngle(Convert::DegreesToRadians(m_ValueToEmit.toDouble()));
    }

    m_ValueToEmit.clear();
}

double GeneratorWeather::AirTemperature() const
{
    return m_AirTemperature;
}

void GeneratorWeather::setAirTemperature(double newAirTemperature)
{
    manualOverride("AirTemperature");
    if (qFuzzyCompare(m_AirTemperature, newAirTemperature))
        return;
    m_AirTemperature = newAirTemperature;
    refreshWeatherFields();
    emit AirTemperatureChanged();
}

double GeneratorWeather::Humidity() const
{
    return m_Humidity;
}

void GeneratorWeather::setHumidity(double newHumidity)
{
    manualOverride("Humidity");
    if (qFuzzyCompare(m_Humidity, newHumidity))
        return;
    m_Humidity = newHumidity;
    refreshWeatherFields();
    emit HumidityChanged();
}

double GeneratorWeather::AtmosphericPressure() const
{
    return m_AtmosphericPressure;
}

void GeneratorWeather::setAtmosphericPressure(double newAtmosphericPressure)
{
    manualOverride("AtmosphericPressure");
    if (qFuzzyCompare(m_AtmosphericPressure, newAtmosphericPressure))
        return;
    m_AtmosphericPressure = newAtmosphericPressure;
    refreshWeatherFields();
    emit AtmosphericPressureChanged();
}

double GeneratorWeather::WindSpeed() const
{
    return m_WindSpeed;
}

void GeneratorWeather::setWindSpeed(double newWindSpeed)
{
    manualOverride("WindSpeed");
    if (qFuzzyCompare(m_WindSpeed, newWindSpeed))
        return;
    m_WindSpeed = newWindSpeed;
    refreshWeatherFields();
    emit WindSpeedChanged();
}

double GeneratorWeather::WindAngle() const
{
    return m_WindAngle;
}

void GeneratorWeather::setWindAngle(double newWindAngle)
{
    manualOverride("WindAngle");
    if (qFuzzyCompare(m_WindAngle, newWindAngle))
        return;
    m_WindAngle = newWindAngle;
    refreshWeatherFields();
    emit WindAngleChanged();
}

tN2kTempSource GeneratorWeather::TemperatureSource() const
{
    return m_TemperatureSource;
}

void GeneratorWeather::setTemperatureSource(tN2kTempSource newTemperatureSource)
{
    if (m_TemperatureSource == newTemperatureSource)
        return;
    m_TemperatureSource = newTemperatureSource;
    emit TemperatureSourceChanged();
}

tN2kHumiditySource GeneratorWeather::HumiditySource() const
{
    return m_HumiditySource;
}

void GeneratorWeather::setHumiditySource(tN2kHumiditySource newHumiditySource)
{
    if (m_HumiditySource == newHumiditySource)
        return;
    m_HumiditySource = newHumiditySource;
    emit HumiditySourceChanged();
}

void GeneratorWeather::setOpenWeatherMapApiKey(const QString &apiKey)
{
    if(m_OpenWeatherMapApiKey==apiKey)return;
    if(ui->comboBoxApiSource->currentIndex()==1)cancelWeather("API key changed — fetch again.");
    m_OpenWeatherMapApiKey = apiKey;
    emit updateApiKey(apiKey);
}

void GeneratorWeather::on_comboBoxApiSource_currentIndexChanged(int index)
{
    cancelWeather(index==0?"No source selected. Manual values retained.":"Ready — fetch weather manually.");
    ui->pushButtonFetchWeather->setEnabled(index!=0);
}

void GeneratorWeather::on_lineEditAirTemperature_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditAirTemperature, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "AirTemperatureChanged";
    onTimerTimeout();
}

void GeneratorWeather::on_lineEditHumidity_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditHumidity, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "HumidityChanged";
    onTimerTimeout();
}

void GeneratorWeather::on_lineEditAtmosphericPressure_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditAtmosphericPressure, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "AtmosphericPressureChanged";
    onTimerTimeout();
}

void GeneratorWeather::on_lineEditWindSpeed_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditWindSpeed, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "WindSpeedChanged";
    onTimerTimeout();
}

void GeneratorWeather::on_lineEditWindAngle_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditWindAngle, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "WindAngleChanged";
    onTimerTimeout();
}

void GeneratorWeather::initilizeComboBoxes()
{
    ui->comboBoxApiSource->addItems(apiSources);
}

void GeneratorWeather::on_pushButtonFetchWeather_clicked()
{
    if (!generateActive()) return;
    if (fetching) { cancelWeather(); return; }
    startFetch(false);
}

void GeneratorWeather::on_lineEditLatitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLatitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LatitudeChanged";
    onTimerTimeout();
}

void GeneratorWeather::on_lineEditLongitude_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditLongitude, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "LongitudeChanged";
    onTimerTimeout();
}

double GeneratorWeather::Latitude() const
{
    return m_Latitude;
}

void GeneratorWeather::setLatitude(double newLatitude)
{
    const bool changed = m_Latitude != newLatitude;
    m_Latitude = newLatitude;

    switch (UserPrefs().latLongUnitIndex) {
    case 0:
        ui->lineEditLatitude->setText(QString::number(m_Latitude, 'f', 6));
        break;
    case 1:
        ui->lineEditLatitude->setText(Convert::DecimalToDMS(m_Latitude, true));
        break;
    case 2:
        ui->lineEditLatitude->setText(Convert::DecimalToDMM(m_Latitude, true));
        break;
    }

    if(changed)emit LatitudeChanged();
}

double GeneratorWeather::Longitude() const
{
    return m_Longitude;
}

void GeneratorWeather::setLongitude(double newLongitude)
{
    const bool changed = m_Longitude != newLongitude;
    m_Longitude = newLongitude;

    switch (UserPrefs().latLongUnitIndex) {
    case 0:
        ui->lineEditLongitude->setText(QString::number(m_Longitude, 'f', 6));
        break;
    case 1:
        ui->lineEditLongitude->setText(Convert::DecimalToDMS(m_Longitude, false));
        break;
    case 2:
        ui->lineEditLongitude->setText(Convert::DecimalToDMM(m_Longitude, false));
        break;
    }

    if(changed)emit LongitudeChanged();
}

uchar GeneratorWeather::SID() const
{
    return m_SID;
}

void GeneratorWeather::setSID(uchar newSID)
{
    if (m_SID == newSID)
        return;
    m_SID = newSID;
    emit SIDChanged();
}

double GeneratorWeather::SeaTemperature() const
{
    return m_SeaTemperature;
}

void GeneratorWeather::setSeaTemperature(double newSeaTemperature)
{
    manualOverride("SeaTemperature");
    if (qFuzzyCompare(m_SeaTemperature, newSeaTemperature))
        return;
    m_SeaTemperature = newSeaTemperature;
    refreshWeatherFields();
    emit SeaTemperatureChanged();
}

void GeneratorWeather::on_lineEditSeaTemperature_textEdited(const QString &arg1)
{
    if (!acceptsEdit(ui->lineEditSeaTemperature, arg1)) return;
    m_ValueToEmit = arg1;
    m_SignalToEmit = "SeaTemperatureChanged";
    onTimerTimeout();
}


void GeneratorWeather::setInitialGuiValues()
{
    setLatitude(m_Latitude);setLongitude(m_Longitude);
    refreshWeatherFields();
}

void GeneratorWeather::refreshWeatherFields(){
    auto show=[](QLineEdit* edit,double raw,double converted,int decimals){if(!edit->isModified()||!edit->hasFocus())edit->setText(WeatherObservation::valid(raw)?QString::number(converted,'f',decimals):"Unavailable");};
    show(ui->lineEditAirTemperature,m_AirTemperature,N2kHelper::convertN2kTemperatureToUserUnits(m_AirTemperature,UserPrefs()),1);
    show(ui->lineEditSeaTemperature,m_SeaTemperature,N2kHelper::convertN2kTemperatureToUserUnits(m_SeaTemperature,UserPrefs()),1);
    show(ui->lineEditHumidity,m_Humidity,m_Humidity,1);
    show(ui->lineEditAtmosphericPressure,m_AtmosphericPressure,N2kHelper::convertN2kAtmosphericPressureToUserUnits(m_AtmosphericPressure,UserPrefs()),2);
    show(ui->lineEditWindSpeed,m_WindSpeed,N2kHelper::convertN2kSpeedToUserUnits(m_WindSpeed,UserPrefs()),1);
    show(ui->lineEditWindAngle,m_WindAngle,Convert::RadiansToDegrees(m_WindAngle),1);
    for (const auto& row : additionalParameters()) {
        auto* edit = observationEdits.value(row[0]);
        if (!edit) continue;
        const double raw = additionalValues.value(row[0], N2kDoubleNA);
        show(edit, raw, displayObservationValue(row[0], raw), 2);
        observationUnits.value(row[0])->setText(observationUnit(row[0]));
    }
    if (!applyingObservation) updateProvenanceDisplay();
}

QList<QList<QString>> GeneratorWeather::additionalParameters() {
    return {
        {"WaveHeight", QString::number(N2kDoubleNA, 'g', 17), "m", "double"},
        {"DominantWavePeriod", QString::number(N2kDoubleNA, 'g', 17), "s", "double"},
        {"AverageWavePeriod", QString::number(N2kDoubleNA, 'g', 17), "s", "double"},
        {"MeanWaveDirection", QString::number(N2kDoubleNA, 'g', 17), "rad", "double"},
        {"WindGust", QString::number(N2kDoubleNA, 'g', 17), "m/s", "double"},
        {"DewPoint", QString::number(N2kDoubleNA, 'g', 17), "K", "double"},
        {"Visibility", QString::number(N2kDoubleNA, 'g', 17), "m", "double"},
        {"PressureTendency", QString::number(N2kDoubleNA, 'g', 17), "Pa", "double"},
        {"Tide", QString::number(N2kDoubleNA, 'g', 17), "m", "double"}
    };
}

void GeneratorWeather::setAdditionalValue(const QString& name, double value) {
    manualOverride(name);
    if (additionalValues.value(name, N2kDoubleNA) == value) return;
    additionalValues.insert(name, value);
    refreshWeatherFields();
    emit MarineObservationsChanged();
}

double GeneratorWeather::displayObservationValue(const QString& name, double value, bool toCanonical) const {
    const auto prefs = UserPrefs();
    if (name == "WaveHeight" || name == "Tide")
        return toCanonical ? N2kHelper::convertUserDepthToN2kUnits(value, prefs) : N2kHelper::convertN2kDepthToUserUnits(value, prefs);
    if (name == "MeanWaveDirection") return toCanonical ? Convert::DegreesToRadians(value) : Convert::RadiansToDegrees(value);
    if (name == "WindGust") return toCanonical ? N2kHelper::convertUserSpeedToN2kUnits(value, prefs) : N2kHelper::convertN2kSpeedToUserUnits(value, prefs);
    if (name == "DewPoint") return toCanonical ? N2kHelper::convertUserTemperatureToN2kUnits(value, prefs) : N2kHelper::convertN2kTemperatureToUserUnits(value, prefs);
    if (name == "Visibility") return toCanonical ? N2kHelper::convertUserDistanceFarToN2kUnits(value, prefs) : N2kHelper::convertN2kDistanceFarToUserUnits(value, prefs);
    if (name == "PressureTendency") return toCanonical ? N2kHelper::convertUserAtmosphericPressureToN2kUnits(value, prefs) : N2kHelper::convertN2kAtmosphericPressureToUserUnits(value, prefs);
    return value;
}
QString GeneratorWeather::observationUnit(const QString& name) const {
    const auto p = UserPrefs();
    if (name == "WaveHeight" || name == "Tide") return QStringList{"m", "ft", "fathoms"}.value(p.depthUnitIndex);
    if (name == "MeanWaveDirection") return "deg true (from)";
    if (name == "WindGust") return QStringList{"m/s", "km/h", "mph", "knots"}.value(p.speedUnitIndex);
    if (name == "DewPoint") return QStringList{"K", "°C", "°F"}.value(p.tempUnitIndex);
    if (name == "Visibility") return QStringList{"km", "nmi", "mi"}.value(p.distanceUnitIndex);
    if (name == "PressureTendency") return QStringList{"hPa", "inHg", "mmHg", "Pa", "atm"}.value(p.atmosphericPressureUnitIndex) + " / 3 h";
    return "s";
}
void GeneratorWeather::initializeObservationPages() {
    auto* tabs = findChild<QTabWidget*>();
    if (!tabs) return;
    tabs->setTabText(0, "Atmosphere");
    auto* marine = new QWidget(tabs);
    auto* more = new QWidget(tabs);
    auto* marineForm = new QFormLayout(marine);
    auto* moreForm = new QFormLayout(more);
    tabs->insertTab(1, marine, "Marine");
    tabs->insertTab(2, more, "More");
    const QMap<QString, QString> labels = {
        {"WaveHeight", "Significant height (WVHT)"}, {"DominantWavePeriod", "Dominant period (DPD)"},
        {"AverageWavePeriod", "Average period (APD)"}, {"MeanWaveDirection", "Wave direction (MWD)"},
        {"WindGust", "Wind gust (GST)"}, {"DewPoint", "Dew point (DEWP)"},
        {"Visibility", "Visibility (VIS)"}, {"PressureTendency", "Pressure change (PTDY)"},
        {"Tide", "Tide above MLLW (TIDE)"}
    };
    for (const auto& field : additionalParameters()) {
        const QString name = field[0];
        auto* edit = new QLineEdit(this);
        edit->setObjectName("lineEdit" + name);
        edit->setValidator(new QDoubleValidator(-1e12, 1e12, 8, edit));
        auto* units = new QLabel(this);
        auto* row = new QWidget(this);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(edit); layout->addWidget(units);
        const bool isMarine = name.contains("Wave") || name == "Tide";
        (isMarine ? marineForm : moreForm)->addRow(labels.value(name), row);
        observationEdits.insert(name, edit);
        observationUnits.insert(name, units);
        connect(edit, &QLineEdit::textEdited, this, [this, edit, name](const QString& text) {
            if (!acceptsEdit(edit, text)) return;
            const double raw = displayObservationValue(name, text.toDouble(), true);
            if (std::isfinite(raw)) setProperty(name.toLatin1().constData(), raw);
        });
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, name] {
            if (!edit->isModified()) return;
            bool ok = false;
            const double shown = edit->text().toDouble(&ok);
            const double raw = displayObservationValue(name, shown, true);
            if (ok && std::isfinite(raw)) setProperty(name.toLatin1().constData(), raw);
            edit->setModified(false);
            refreshWeatherFields();
        });
    }
}
// A single-shot scheduler measures the full interval from the end of a cycle.
// Enabling/restoring/resuming never performs an immediate provider request.
void GeneratorWeather::initializeRefreshControls() {
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);
    autoRefresh = new QCheckBox("Enable Automatic Refresh", page);
    autoRefresh->setObjectName("weatherAutoRefresh");
    autoRefresh->setToolTip(tr("Schedule bounded weather requests using current vessel coordinates. OFF by default; this does not enable Sending."));
    refreshInterval = new QComboBox(page); refreshInterval->setObjectName("weatherRefreshInterval");
    for (int minutes : {1, 5, 15, 30, 60}) refreshInterval->addItem(QString::number(minutes) + (minutes==1 ? " minute" : " minutes"), minutes);
    refreshInterval->setCurrentIndex(2);
    refreshInterval->setToolTip(tr("Minutes between automatic requests. Requests do not overlap; the next planned time appears in the Object Tree."));
    retainWeather = new QCheckBox("Retain last valid weather measurements\nwhen new data is unavailable", page);
    retainWeather->setObjectName("weatherRetainObservations"); retainWeather->setChecked(true);
    retainWeather->setToolTip("Retain each measurement only until its original observation expires. Original provider, station and time remain unchanged.");
    scheduleStatus = new QLabel(page); scheduleStatus->setWordWrap(true); scheduleStatus->setObjectName("weatherScheduleStatus");
    form->addRow(autoRefresh); form->addRow("Refresh interval", refreshInterval); form->addRow(retainWeather); form->addRow(scheduleStatus);
    findChild<QTabWidget*>()->addTab(page, "Refresh");
    refreshTimer.setParent(this); refreshTimer.setObjectName("weatherRefreshTimer"); refreshTimer.setSingleShot(true);
    cycleDeadline.setParent(this); cycleDeadline.setObjectName("weatherCycleDeadline"); cycleDeadline.setSingleShot(true);
    connect(&refreshTimer, &QTimer::timeout, this, [this] { startFetch(true); });
    connect(&cycleDeadline, &QTimer::timeout, this, [this] { cancelWeather("Weather refresh cycle timed out; retry later."); });
    connect(autoRefresh, &QCheckBox::toggled, this, [this](bool on) {
        if (!on && scheduledCycle) cancelWeather("Automatic refresh disabled.");
        scheduleRefresh();
    });
    connect(refreshInterval, &QComboBox::currentIndexChanged, this, &GeneratorWeather::scheduleRefresh);
    connect(retainWeather, &QCheckBox::toggled, this, [this](bool on) {
        if (!on) {
            QScopedValueRollback<bool> guard(applyingObservation, true);
            for (const auto &field : observedFields.values()) if (origins.value(field).retained) {
                setProperty(field.toLatin1().constData(), N2kDoubleNA);
                observedFields.remove(field); origins[field].stale = true;
            }
        }
        expireMeasurements();
    });
    scheduleRefresh();
}
void GeneratorWeather::scheduleRefresh() {
    if (!autoRefresh) return; // source-combo initialization precedes controls
    refreshTimer.stop(); nextRefresh = {};
    if (automaticRefresh() && generateActive() && ui->comboBoxApiSource->currentIndex()!=0 && !fetching) {
        const int milliseconds = refreshIntervalMinutes()*60000;
        refreshTimer.start(milliseconds);
        nextRefresh = QDateTime::currentDateTimeUtc().addMSecs(milliseconds);
    }
    scheduleStatus->setText(QString("Last successful fetch: %1\nNext refresh: %2")
        .arg(lastSuccess.isValid() ? lastSuccess.toLocalTime().toString(Qt::ISODate) : "none",
             nextRefresh.isValid() ? nextRefresh.toLocalTime().toString(Qt::ISODate) : fetching ? "after this fetch" : "not scheduled"));
    emit ObservationMetadataChanged();
}
void GeneratorWeather::locationChanged() {
    // Manual and automatic cycles share the same immutable request position.
    // A relocation cancels once; coordinate ticks never trigger network retries
    // or postpone the next scheduled refresh.
    if (fetching && !withinWeatherArea(Latitude(), Longitude(), cycleLatitude, cycleLongitude, maximumCycleMovement)) {
        cancelWeather("Location changed beyond weather request area — fetch again.");
        return;
    }
    if (!observedFields.isEmpty()) expireMeasurements();
}
void GeneratorWeather::startFetch(bool scheduled) {
    if (!generateActive() || fetching || (scheduled && !automaticRefresh())) return;
    const int source = ui->comboBoxApiSource->currentIndex(); if (!source) return;
    if (source==1 && (m_OpenWeatherMapApiKey.isEmpty() || m_OpenWeatherMapApiKey=="YOUR_API_KEY")) {
        handleWeatherError("Configure an OpenWeatherMap API key in Setup."); return;
    }
    if (!QGeoCoordinate(Latitude(), Longitude()).isValid()) {
        handleWeatherError("Invalid weather coordinates."); return;
    }
    refreshTimer.stop(); nextRefresh = {};
    fetching = true; scheduledCycle = scheduled; candidateAttempts = 0; candidates.clear(); selectedStation = {};
    cycleLatitude = Latitude(); cycleLongitude = Longitude();
    cycleDeadline.start(60000); // discovery + at most three bounded observation requests
    ui->pushButtonFetchWeather->setText("Cancel Fetch");
    setWeatherStatus(scheduled ? "Automatic weather refresh…" : "Fetching weather…");
    qCDebug(ndbcLog) << "Weather cycle started; scheduled=" << scheduled;
    if (source==1) { emit updateApiKey(m_OpenWeatherMapApiKey); emit fetchWeather(cycleLatitude, cycleLongitude); }
    else emit getNearestStations(cycleLatitude, cycleLongitude, 80);
    scheduleRefresh();
}
void GeneratorWeather::finishFetch() {
    cycleDeadline.stop(); fetching = false; scheduledCycle = false; candidates.clear(); selectedStation = {};
    ui->pushButtonFetchWeather->setText("Fetch Weather"); scheduleRefresh();
}
void GeneratorWeather::tryNextStation() {
    if (!fetching || !scheduledCycle || !generateActive()) return;
    if (candidates.isEmpty() || candidateAttempts>=3) {
        finishFetch();
        handleWeatherError("No usable NOAA observation feed among up to three nearby stations; retry later."); return;
    }
    selectedStation = candidates.takeFirst(); ++candidateAttempts;
    m_stationName = selectedStation.name;
    setWeatherStatus(QString("Automatic NOAA refresh: station %1 (attempt %2/3)…").arg(selectedStation.id).arg(candidateAttempts));
    emit fetchStationData(selectedStation.id);
}
bool GeneratorWeather::hasFreshObservation() const {
    for (const auto &field : observedFields) if (origins.value(field).fresh()) return true;
    return false;
}
void GeneratorWeather::manualOverride(const QString &field) {
    if (applyingObservation) return;
    const bool owned = observedFields.remove(field);
    const bool hadOrigin = origins.remove(field)>0;
    if (owned || hadOrigin) {
        setWeatherStatus("Manual override; other observation fields retained.");
        updateProvenanceDisplay();
    }
}
QString GeneratorWeather::fieldDescription(const QString &field) const {
    if (!origins.contains(field)) return WeatherObservation::valid(property(field.toLatin1().constData()).toDouble()) ? "Manually configured / Layout value" : "Unavailable";
    const auto o = origins.value(field);
    return QString("%1; %2; station %3; %4\nObserved UTC %5; retrieved UTC %6\nObservation location %7, %8; original maximum age %9 s")
        .arg(!o.fresh() ? "Stale / unavailable" : o.retained ? "Retained (still fresh)" : "Current",
             o.provider, o.station.isEmpty() ? "none" : o.station, o.location,
             o.observed.toUTC().toString(Qt::ISODate), o.retrieved.toUTC().toString(Qt::ISODate))
        .arg(o.latitude,0,'f',5).arg(o.longitude,0,'f',5).arg(o.maximumAgeSeconds);
}
void GeneratorWeather::expireMeasurements() {
    freshnessTimer.stop();
    if (!generateActive()) return;
    QScopedValueRollback<bool> guard(applyingObservation, true);
    qint64 next = 14400000;
    const auto now = QDateTime::currentDateTimeUtc();
    bool expired = false;
    for (const auto &field : observedFields.values()) {
        auto &origin = origins[field];
        const bool outsideArea = QGeoCoordinate(origin.latitude, origin.longitude).isValid() &&
            !withinWeatherArea(Latitude(), Longitude(), origin.latitude, origin.longitude,
                               observationRadius(origin.provider) + maximumCycleMovement);
        if (!origin.fresh(now) || outsideArea) {
            origin.stale = true; observedFields.remove(field);
            setProperty(field.toLatin1().constData(), N2kDoubleNA); expired = true;
            qCWarning(ndbcLog).noquote() << nationalDataBuoyCenter->diagnosticContext("freshness", origin.station)
                << "field=" + field << "result=expired age_s=" + QString::number(origin.observed.secsTo(now));
        } else next = qMin(next, now.msecsTo(origin.observed.addSecs(origin.maximumAgeSeconds))+1);
    }
    if (!observedFields.isEmpty()) freshnessTimer.start(int(qMax<qint64>(1,next)));
    if (expired) setWeatherStatus("Expired or out-of-area observations unavailable; fresh and manual measurements preserved.");
    updateProvenanceDisplay();
}
void GeneratorWeather::updateProvenanceDisplay() {
    if (!weatherStatus) return;
    QStringList details;
    const auto fields = measurementFields();
    int retained = 0, stale = 0;
    for (const auto &field : fields) {
        const QString description = fieldDescription(field);
        if (auto *editor = findChild<QLineEdit*>("lineEdit"+field)) editor->setToolTip(description);
        details << field + ": " + description;
        if (origins.contains(field)) {
            if (!origins.value(field).fresh()) ++stale;
            else if (origins.value(field).retained) ++retained;
        }
    }
    weatherStatus->setText(statusSummary + (retained || stale ? QString(" (%1 retained, %2 stale)").arg(retained).arg(stale) : QString()));
    weatherStatus->setToolTip(weatherStatus->text() + "\n" + details.join("\n\n"));
    // Description is the accessible detailed provenance view; no extra value store.
    ui->textEditDescription->setPlainText(m_observation.description + "\n\n" + weatherStatus->toolTip());
    emit ObservationMetadataChanged();
}
QStringList GeneratorWeather::measurementFields() {
    return {"AirTemperature", "SeaTemperature", "Humidity", "AtmosphericPressure", "WindSpeed", "WindAngle",
        "WaveHeight", "DominantWavePeriod", "AverageWavePeriod", "MeanWaveDirection", "WindGust", "DewPoint", "Visibility", "PressureTendency", "Tide"};
}
bool GeneratorWeather::validLayoutConfiguration(const QJsonObject &c) {
    auto integer=[](QJsonValue v,int low,int high){return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble()==v.toInt(-1) && v.toInt()>=low && v.toInt()<=high;};
    if ((c.contains("autoRefresh") && !c["autoRefresh"].isBool()) ||
        (c.contains("retainObservations") && !c["retainObservations"].isBool()) ||
        (c.contains("apiSource") && !integer(c["apiSource"],0,2))) return false;
    if (c.contains("refreshMinutes") && (!integer(c["refreshMinutes"],1,60) || !QList<int>{1,5,15,30,60}.contains(c["refreshMinutes"].toInt()))) return false;
    if (!c.contains("origins")) return true;
    if (!c["origins"].isObject() || c["origins"].toObject().size()>measurementFields().size()) return false;
    const auto fields=c["origins"].toObject();
    for (auto it=fields.begin();it!=fields.end();++it) {
        if (!measurementFields().contains(it.key()) || !it.value().isObject()) return false;
        const auto o=it.value().toObject();
        if (!QStringList{"NOAA NDBC","OpenWeatherMap"}.contains(o["provider"].toString()) ||
            !o["station"].isString() || o["station"].toString().size()>10 ||
            !o["location"].isString() || o["location"].toString().size()>256 ||
            !QDateTime::fromString(o["observed"].toString(),Qt::ISODateWithMs).isValid() ||
            !o["retrieved"].isString() || (!o["retrieved"].toString().isEmpty() && !QDateTime::fromString(o["retrieved"].toString(),Qt::ISODateWithMs).isValid()) ||
            !integer(o["maximumAgeSeconds"],0,86400) || !o["retained"].isBool() || !o["stale"].isBool()) return false;
        for (const QString field:{"latitude","longitude"}) {
            const auto v=o[field];const double bound=field=="latitude" ? 90 : 180;
            if (!v.isDouble() || !std::isfinite(v.toDouble()) || (v.toDouble()!=N2kDoubleNA && qAbs(v.toDouble())>bound)) return false;
        }
    }
    return true;
}
QJsonObject GeneratorWeather::layoutConfiguration() const {
    QJsonObject metadata;
    for (auto it=origins.begin();it!=origins.end();++it) {
        const auto &o=it.value();
        metadata[it.key()]=QJsonObject{{"provider",o.provider},{"station",o.station},{"location",o.location},
            {"observed",o.observed.toUTC().toString(Qt::ISODateWithMs)},{"retrieved",o.retrieved.toUTC().toString(Qt::ISODateWithMs)},
            {"latitude",o.latitude},{"longitude",o.longitude},{"maximumAgeSeconds",o.maximumAgeSeconds},{"retained",o.retained},{"stale",!o.fresh()}};
    }
    return {{"autoRefresh", automaticRefresh()}, {"refreshMinutes", refreshIntervalMinutes()},
            {"retainObservations", retainObservations()}, {"apiSource", ui->comboBoxApiSource->currentIndex()}, {"origins",metadata}};
}
void GeneratorWeather::restoreLayoutConfiguration(const QJsonObject &config) {
    if (!validLayoutConfiguration(config)) return; // controller also validates transactionally before replacement
    const QSignalBlocker a(autoRefresh), b(refreshInterval), c(retainWeather);
    autoRefresh->setChecked(config.value("autoRefresh").toBool(false));
    const int index = refreshInterval->findData(config.value("refreshMinutes").toInt(15));
    refreshInterval->setCurrentIndex(index<0 ? 2 : index);
    retainWeather->setChecked(config.value("retainObservations").toBool(true));
    ui->comboBoxApiSource->setCurrentIndex(qBound(0,config.value("apiSource").toInt(0),2));
    origins.clear();observedFields.clear();
    const auto metadata=config["origins"].toObject();
    QScopedValueRollback<bool> guard(applyingObservation,true);
    for (auto it=metadata.begin();it!=metadata.end();++it) {
        const auto o=it.value().toObject();
        WeatherFieldOrigin origin{o["provider"].toString(),o["station"].toString(),o["location"].toString(),
            QDateTime::fromString(o["observed"].toString(),Qt::ISODateWithMs),QDateTime::fromString(o["retrieved"].toString(),Qt::ISODateWithMs),
            o["latitude"].toDouble(),o["longitude"].toDouble(),o["maximumAgeSeconds"].toInt(),o["retained"].toBool(),o["stale"].toBool()};
        if (!origin.fresh() || (!retainObservations() && origin.retained) || ui->comboBoxApiSource->currentIndex()==0) {
            origin.stale=true;setProperty(it.key().toLatin1().constData(),N2kDoubleNA);
        } else if (WeatherObservation::valid(property(it.key().toLatin1().constData()).toDouble())) observedFields.insert(it.key());
        origins[it.key()]=origin;
    }
    expireMeasurements();scheduleRefresh();updateProvenanceDisplay();
}

QString GeneratorWeather::selectedProvider() const {
    return ui->comboBoxApiSource->currentIndex()==2 ? "NOAA NDBC" : ui->comboBoxApiSource->currentIndex()==1 ? "OpenWeatherMap" : "None";
}
QString GeneratorWeather::latestObservationSource() const {
    return acceptedProvider.isEmpty() ? "No accepted observation this session" : acceptedProvider +
        (acceptedStation.isEmpty() ? QString() : " / " + acceptedStation);
}
