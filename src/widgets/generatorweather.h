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

#ifndef GENERATORWEATHER_H
#define GENERATORWEATHER_H

#include <QDoubleValidator>
#include <QLabel>
#include <QLineEdit>
#include <QSet>
#include <QMap>
#include <QTimer>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonObject>
#include "N2kTypes.h"
#include "nationaldatabuoycenter.h"
#include "openweathermap.h"
#include "pgnwidgetbase.h"

namespace Ui {
class GeneratorWeather;
}

class GeneratorWeather : public PgnWidgetBase
{
    Q_OBJECT

public:
    explicit GeneratorWeather(int id, QString label, QWidget *parent = nullptr, QNetworkAccessManager *fixtureNetwork = nullptr);
    ~GeneratorWeather();

    Q_PROPERTY(uchar SID READ SID WRITE setSID NOTIFY SIDChanged FINAL)
    Q_PROPERTY(double AirTemperature READ AirTemperature WRITE setAirTemperature NOTIFY AirTemperatureChanged FINAL)
    Q_PROPERTY(double SeaTemperature READ SeaTemperature WRITE setSeaTemperature NOTIFY SeaTemperatureChanged FINAL)
    Q_PROPERTY(double Humidity READ Humidity WRITE setHumidity NOTIFY HumidityChanged FINAL)
    Q_PROPERTY(double AtmosphericPressure READ AtmosphericPressure WRITE setAtmosphericPressure NOTIFY AtmosphericPressureChanged FINAL)
    Q_PROPERTY(double WindSpeed READ WindSpeed WRITE setWindSpeed NOTIFY WindSpeedChanged FINAL)
    Q_PROPERTY(double WindAngle READ WindAngle WRITE setWindAngle NOTIFY WindAngleChanged FINAL)
    Q_PROPERTY(tN2kTempSource TemperatureSource READ TemperatureSource WRITE setTemperatureSource NOTIFY TemperatureSourceChanged FINAL)
    Q_PROPERTY(tN2kHumiditySource HumiditySource READ HumiditySource WRITE setHumiditySource NOTIFY HumiditySourceChanged FINAL)
    Q_PROPERTY(double Latitude READ Latitude WRITE setLatitude NOTIFY LatitudeChanged FINAL)
    Q_PROPERTY(double Longitude READ Longitude WRITE setLongitude NOTIFY LongitudeChanged FINAL)

    // Canonical observation parameters; these do not define transmitted PGNs.
    static QList<QList<QString>> additionalParameters();
    Q_PROPERTY(double WaveHeight READ WaveHeight WRITE setWaveHeight NOTIFY MarineObservationsChanged FINAL)
    double WaveHeight() const { return additionalValues.value("WaveHeight", N2kDoubleNA); }
    void setWaveHeight(double value) { setAdditionalValue("WaveHeight", value); }
    Q_PROPERTY(double DominantWavePeriod READ DominantWavePeriod WRITE setDominantWavePeriod NOTIFY MarineObservationsChanged FINAL)
    double DominantWavePeriod() const { return additionalValues.value("DominantWavePeriod", N2kDoubleNA); }
    void setDominantWavePeriod(double value) { setAdditionalValue("DominantWavePeriod", value); }
    Q_PROPERTY(double AverageWavePeriod READ AverageWavePeriod WRITE setAverageWavePeriod NOTIFY MarineObservationsChanged FINAL)
    double AverageWavePeriod() const { return additionalValues.value("AverageWavePeriod", N2kDoubleNA); }
    void setAverageWavePeriod(double value) { setAdditionalValue("AverageWavePeriod", value); }
    Q_PROPERTY(double MeanWaveDirection READ MeanWaveDirection WRITE setMeanWaveDirection NOTIFY MarineObservationsChanged FINAL)
    double MeanWaveDirection() const { return additionalValues.value("MeanWaveDirection", N2kDoubleNA); }
    void setMeanWaveDirection(double value) { setAdditionalValue("MeanWaveDirection", value); }
    Q_PROPERTY(double WindGust READ WindGust WRITE setWindGust NOTIFY MarineObservationsChanged FINAL)
    double WindGust() const { return additionalValues.value("WindGust", N2kDoubleNA); }
    void setWindGust(double value) { setAdditionalValue("WindGust", value); }
    Q_PROPERTY(double DewPoint READ DewPoint WRITE setDewPoint NOTIFY MarineObservationsChanged FINAL)
    double DewPoint() const { return additionalValues.value("DewPoint", N2kDoubleNA); }
    void setDewPoint(double value) { setAdditionalValue("DewPoint", value); }
    Q_PROPERTY(double Visibility READ Visibility WRITE setVisibility NOTIFY MarineObservationsChanged FINAL)
    double Visibility() const { return additionalValues.value("Visibility", N2kDoubleNA); }
    void setVisibility(double value) { setAdditionalValue("Visibility", value); }
    Q_PROPERTY(double PressureTendency READ PressureTendency WRITE setPressureTendency NOTIFY MarineObservationsChanged FINAL)
    double PressureTendency() const { return additionalValues.value("PressureTendency", N2kDoubleNA); }
    void setPressureTendency(double value) { setAdditionalValue("PressureTendency", value); }
    Q_PROPERTY(double Tide READ Tide WRITE setTide NOTIFY MarineObservationsChanged FINAL)
    double Tide() const { return additionalValues.value("Tide", N2kDoubleNA); }
    void setTide(double value) { setAdditionalValue("Tide", value); }

    double AirTemperature() const;
    void setAirTemperature(double newAirTemperature);

    double Humidity() const;
    void setHumidity(double newHumidity);

    double AtmosphericPressure() const;
    void setAtmosphericPressure(double newAtmosphericPressure);

    double WindSpeed() const;
    void setWindSpeed(double newWindSpeed);

    double WindAngle() const;
    void setWindAngle(double newWindAngle);

    tN2kTempSource TemperatureSource() const;
    void setTemperatureSource(tN2kTempSource newTemperatureSource);

    tN2kHumiditySource HumiditySource() const;
    void setHumiditySource(tN2kHumiditySource newHumiditySource);
    void setOpenWeatherMapApiKey(const QString &apiKey);

    double Latitude() const;
    void setLatitude(double newLatitude);

    double Longitude() const;
    void setLongitude(double newLongitude);

    uchar SID() const;
    void setSID(uchar newSID);

    double SeaTemperature() const;
    void setSeaTemperature(double newSeaTemperature);

    void setInitialGuiValues() override;
    WeatherObservation observation() const { return m_observation; }
    bool hasFreshObservation() const;
    WeatherFieldOrigin fieldOrigin(const QString &field) const { return origins.value(field); }
    QString fieldDescription(const QString &field) const;
    static QStringList measurementFields();
    static bool validLayoutConfiguration(const QJsonObject &);
    QDateTime lastSuccessfulFetch() const { return lastSuccess; }
    QDateTime latestObservationTime() const { return acceptedObservationTime; }
    QDateTime nextAutomaticRefresh() const { return nextRefresh; }
    QString latestObservationSource() const;
    QString selectedProvider() const;
    QJsonObject layoutConfiguration() const;
    void restoreLayoutConfiguration(const QJsonObject &);
    bool automaticRefresh() const { return autoRefresh->isChecked(); }
    int refreshIntervalMinutes() const { return refreshInterval->currentData().toInt(); }
    bool retainObservations() const { return retainWeather->isChecked(); }
    QString observationStatus() const { return weatherStatus->text(); }

signals:
    void MarineObservationsChanged();
    void ObservationMetadataChanged();
    void AirTemperatureChanged();
    void HumidityChanged();
    void AtmosphericPressureChanged();
    void WindSpeedChanged();
    void WindAngleChanged();
    void TemperatureSourceChanged();
    void HumiditySourceChanged();
    void LatitudeChanged();
    void LongitudeChanged();
    void updateApiKey(const QString &apiKey);
    void fetchWeather(double latitude, double longitude); // Fetch weather data from OpenWeatherMap
    //void getNearestStations(double latitude, double longitude, int maxResults = 5); // Get nearest weather stations from National Data Buoy Center
    void getNearestStations(double latitude, double longitude, double radius); // Get nearest weather stations from National Data Buoy Center
    void fetchStationData(const QString &stationId);                           // Fetch weather data from National Data Buoy Center

    void SIDChanged();

    void SeaTemperatureChanged();

protected:
    bool event(QEvent*) override;
    void initializeUnitsLimits() override;
    void initializeClickableLabels() override;

public slots:
    void acceptObservation(const WeatherObservation&);
    void cancelWeather(const QString& reason = "Cancelled — fetch again when ready.");
    // OpenWeatherMap
    void weatherFetched(
        const QString &locationName, const QString &description, double temperature, double humidity, double pressure, double windSpeed, double windDirection);
    // National Data Buoy Center
    void nearestStationsFound(const QList<Station> &nearestStations);
    void dataFetched(const QString &description,
                     double airTemperature,
                     double waterTemperature,
                     double waveHeight,
                     double windSpeed,
                     double windDirection,
                     double pressure,
                     double humidity);
    // Common
    void handleWeatherError(const QString &errorMessage);

protected slots:
    void onTimerTimeout() override;

private slots:
    void on_comboBoxApiSource_currentIndexChanged(int index);
    void on_lineEditAirTemperature_textEdited(const QString &arg1);
    void on_lineEditHumidity_textEdited(const QString &arg1);
    void on_lineEditAtmosphericPressure_textEdited(const QString &arg1);
    void on_lineEditWindSpeed_textEdited(const QString &arg1);
    void on_lineEditWindAngle_textEdited(const QString &arg1);

    void on_pushButtonFetchWeather_clicked();

    void on_lineEditLatitude_textEdited(const QString &arg1);

    void on_lineEditLongitude_textEdited(const QString &arg1);

    void on_lineEditSeaTemperature_textEdited(const QString &arg1);

private:
    Ui::GeneratorWeather *ui;
    QCheckBox *autoRefresh = nullptr, *retainWeather = nullptr;
    QComboBox *refreshInterval = nullptr;
    QLabel *scheduleStatus = nullptr;
    QTimer refreshTimer, cycleDeadline;
    QDateTime lastSuccess, nextRefresh, acceptedObservationTime;
    QString acceptedProvider, acceptedStation;
    QMap<QString, WeatherFieldOrigin> origins;
    QList<Station> candidates;
    bool scheduledCycle = false;
    int candidateAttempts = 0;
    double cycleLatitude = 0, cycleLongitude = 0;
    void initializeRefreshControls();
    void scheduleRefresh();
    void startFetch(bool scheduled);
    void finishFetch();
    void tryNextStation();
    void expireMeasurements();
    void updateProvenanceDisplay();
    void manualOverride(const QString &);
    void locationChanged();
    OpenWeatherMap *openWeatherMap;
    NationalDataBuoyCenter *nationalDataBuoyCenter;
    QLabel* weatherStatus=nullptr;
    QString statusSummary;
    QTimer freshnessTimer;
    WeatherObservation m_observation;
    QSet<QString> observedFields;
    bool applyingObservation=false, fetching=false;
    quint64 requestGeneration=0;
    Station selectedStation{};
    bool generateActive() const;
    void invalidateObservation(const QString&);
    void setWeatherStatus(const QString&);
    void refreshWeatherFields();
    void initializeObservationPages();
    void setAdditionalValue(const QString&, double);
    double displayObservationValue(const QString&, double, bool toCanonical = false) const;
    QString observationUnit(const QString&) const;
    QMap<QString, double> additionalValues;
    QMap<QString, QLineEdit*> observationEdits;
    QMap<QString, QLabel*> observationUnits;
    uchar m_SID=0;
    double m_AirTemperature = N2kDoubleNA;
    double m_Humidity = N2kDoubleNA;
    double m_AtmosphericPressure = N2kDoubleNA;
    double m_WindSpeed = N2kDoubleNA;
    double m_WindAngle = N2kDoubleNA;
    tN2kTempSource m_TemperatureSource=N2kts_OutsideTemperature;
    tN2kHumiditySource m_HumiditySource=N2khs_OutsideHumidity;
    double m_Latitude=0;
    double m_Longitude=0;

    QString m_OpenWeatherMapApiKey = "YOUR_API_KEY";
    QStringList apiSources = {"None", "Open Weather Map", "National Data Buoy Center"};
    QString m_stationName = QString();

    QDoubleValidator *temperatureValidator = new QDoubleValidator(this);
    QDoubleValidator *atmosphericPressureValidator = new QDoubleValidator(this);
    QDoubleValidator *humidityValidator = new QDoubleValidator(this);
    QDoubleValidator *pressureValidator = new QDoubleValidator(this);
    QDoubleValidator *windSpeedValidator = new QDoubleValidator(this);
    QDoubleValidator *windAngleValidator = new QDoubleValidator(this);

    void initilizeComboBoxes();
    double m_SeaTemperature = N2kDoubleNA;
};

#endif // GENERATORWEATHER_H

/*
 *** Weather-Related PGNs *** 
 * PGN      Description                         Key Parameters
 * 130310	Environmental Parameters            Air Temperature, Humidity, Pressure
 * 130311	Environmental Parameters Extended	Temperature, Humidity, Pressure from sources
 * 130312	Temperature                         Temperature from various sources
 * 130313	Humidity                            Humidity, Dew Point
 * 130314	Actual                              Pressure	Atmospheric or water pressure
 * 130306	Wind Data                           Wind Speed, Wind Angle, Reference
 */
