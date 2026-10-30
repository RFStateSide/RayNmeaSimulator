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

#pragma once
#include "pgnwidgetbase.h"
#include "N2kMsg.h"
#include "NMEA0183Msg.h"
#include "N2kTypes.h"
#include "NMEA0183Messages.h"
#include <QMap>
#include <QVariant>
class QLineEdit;
class QComboBox;
namespace Expansion
{
struct Field
{
    QString name, unit, type, initial;
    double minimum, maximum;
};
struct Definition
{
    QString code, type, label, group;
    QList<Field> fields;
    QStringList identity;
    int id;
};
const QList<Definition> &definitions();
const Definition *definition(const QString &code);
const Definition *definition(int id);
QList<QList<QString>> parameters(const QString &code);
bool validField(const QString &code, const QString &field, const QString &text);
bool validParameters(const QString &code, const QStringList &fields);
bool encode(const QString &code, const QMap<QString, QString> &fields, tN2kMsg &message);
bool decode(const tN2kMsg &message, QStringList &fields);
bool decodeRouteGeometry(const tN2kMsg &message, QStringList &fields);
bool decodeRouteAttributes(const tN2kMsg &message, QStringList &fields);
bool encodeSentence(const QString &code, const QMap<QString, QString> &fields, tNMEA0183Msg &message);
bool decodeSentence(const tNMEA0183Msg &message, QStringList &fields);
QList<QPair<QVariant, QString>> choices(const QString &type);
QString fieldLabel(const QString &field);
QString displayValue(const QString &code, const QString &field, const QString &text, const UserUnitPrefs &prefs);
} // namespace Expansion
// An ordinary parameter-row presentation adapter, like the existing status instruments.
class ExpansionInstrument : public PgnWidgetBase
{
    Q_OBJECT
  public:
    ExpansionInstrument(int id, const QString &label, const QString &code, QWidget *parent = nullptr);
    static ExpansionInstrument *create(int id, const QString &label, const QString &code, QWidget *parent = nullptr);
    QString code() const
    {
        return messageCode;
    }
    QString value(const QString &field) const
    {
        return values.value(field);
    }
    void setValue(const QString &field, const QString &text);
    void setInitialGuiValues() override;
  signals:
    void parameterValueChanged(const QString &field, const QString &value);

  private:
    QString messageCode;
    QMap<QString, QString> values;
    QMap<QString, QLineEdit *> editors;
    QMap<QString, QComboBox *> selectors;
    QMap<QString, UserUnitPrefs> draftPrefs;
    void editSatelliteRecords();
};
class EnvironmentalLegacy130311 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char SID READ SID WRITE setSID)
    Q_PROPERTY(tN2kTempSource TemperatureSource READ TemperatureSource WRITE setTemperatureSource)
    Q_PROPERTY(double Temperature READ Temperature WRITE setTemperature)
    Q_PROPERTY(tN2kHumiditySource HumiditySource READ HumiditySource WRITE setHumiditySource)
    Q_PROPERTY(double Humidity READ Humidity WRITE setHumidity)
    Q_PROPERTY(double AtmosphericPressure READ AtmosphericPressure WRITE setAtmosphericPressure)
  public:
    EnvironmentalLegacy130311(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "130311", parent)
    {
    }
    unsigned char SID() const
    {
        return static_cast<unsigned char>(value("SID").toDouble());
    }
    void setSID(unsigned char v)
    {
        setValue("SID", QString::number(v));
    }
    tN2kTempSource TemperatureSource() const
    {
        return static_cast<tN2kTempSource>(value("TemperatureSource").toDouble());
    }
    void setTemperatureSource(tN2kTempSource v)
    {
        setValue("TemperatureSource", QString::number(v));
    }
    double Temperature() const
    {
        return value("Temperature").toDouble();
    }
    void setTemperature(double v)
    {
        setValue("Temperature", QString::number(v, 'g', 17));
    }
    tN2kHumiditySource HumiditySource() const
    {
        return value("HumiditySource").toDouble() == 3
                   ? N2khs_Undef
                   : static_cast<tN2kHumiditySource>(value("HumiditySource").toDouble());
    }
    void setHumiditySource(tN2kHumiditySource v)
    {
        setValue("HumiditySource", QString::number(v == N2khs_Undef ? 3 : int(v)));
    }
    double Humidity() const
    {
        return value("Humidity").toDouble();
    }
    void setHumidity(double v)
    {
        setValue("Humidity", QString::number(v, 'g', 17));
    }
    double AtmosphericPressure() const
    {
        return value("AtmosphericPressure").toDouble();
    }
    void setAtmosphericPressure(double v)
    {
        setValue("AtmosphericPressure", QString::number(v, 'g', 17));
    }
};
class GnssDops129539 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char SID READ SID WRITE setSID)
    Q_PROPERTY(tN2kGNSSDOPmode DesiredMode READ DesiredMode WRITE setDesiredMode)
    Q_PROPERTY(tN2kGNSSDOPmode ActualMode READ ActualMode WRITE setActualMode)
    Q_PROPERTY(double HDOP READ HDOP WRITE setHDOP)
    Q_PROPERTY(double VDOP READ VDOP WRITE setVDOP)
    Q_PROPERTY(double TDOP READ TDOP WRITE setTDOP)
  public:
    GnssDops129539(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "129539", parent)
    {
    }
    unsigned char SID() const
    {
        return static_cast<unsigned char>(value("SID").toDouble());
    }
    void setSID(unsigned char v)
    {
        setValue("SID", QString::number(v));
    }
    tN2kGNSSDOPmode DesiredMode() const
    {
        return static_cast<tN2kGNSSDOPmode>(value("DesiredMode").toDouble());
    }
    void setDesiredMode(tN2kGNSSDOPmode v)
    {
        setValue("DesiredMode", QString::number(v));
    }
    tN2kGNSSDOPmode ActualMode() const
    {
        return static_cast<tN2kGNSSDOPmode>(value("ActualMode").toDouble());
    }
    void setActualMode(tN2kGNSSDOPmode v)
    {
        setValue("ActualMode", QString::number(v));
    }
    double HDOP() const
    {
        return value("HDOP").toDouble();
    }
    void setHDOP(double v)
    {
        setValue("HDOP", QString::number(v, 'g', 17));
    }
    double VDOP() const
    {
        return value("VDOP").toDouble();
    }
    void setVDOP(double v)
    {
        setValue("VDOP", QString::number(v, 'g', 17));
    }
    double TDOP() const
    {
        return value("TDOP").toDouble();
    }
    void setTDOP(double v)
    {
        setValue("TDOP", QString::number(v, 'g', 17));
    }
};
class GnssSatellites129540 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char SID READ SID WRITE setSID)
    Q_PROPERTY(tN2kRangeResidualMode Mode READ Mode WRITE setMode)
    Q_PROPERTY(QString Satellites READ Satellites WRITE setSatellites)
  public:
    GnssSatellites129540(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "129540", parent)
    {
    }
    unsigned char SID() const
    {
        return static_cast<unsigned char>(value("SID").toDouble());
    }
    void setSID(unsigned char v)
    {
        setValue("SID", QString::number(v));
    }
    tN2kRangeResidualMode Mode() const
    {
        return static_cast<tN2kRangeResidualMode>(value("Mode").toDouble());
    }
    void setMode(tN2kRangeResidualMode v)
    {
        setValue("Mode", QString::number(v));
    }
    QString Satellites() const
    {
        return value("Satellites");
    }
    void setSatellites(QString v)
    {
        setValue("Satellites", v);
    }
};
class DistanceLog128275 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(uint16_t DaysSince1970 READ DaysSince1970 WRITE setDaysSince1970)
    Q_PROPERTY(double SecondsSinceMidnight READ SecondsSinceMidnight WRITE setSecondsSinceMidnight)
    Q_PROPERTY(quint32 Log READ Log WRITE setLog)
    Q_PROPERTY(quint32 TripLog READ TripLog WRITE setTripLog)
  public:
    DistanceLog128275(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "128275", parent)
    {
    }
    uint16_t DaysSince1970() const
    {
        return static_cast<uint16_t>(value("DaysSince1970").toDouble());
    }
    void setDaysSince1970(uint16_t v)
    {
        setValue("DaysSince1970", QString::number(v));
    }
    double SecondsSinceMidnight() const
    {
        return value("SecondsSinceMidnight").toDouble();
    }
    void setSecondsSinceMidnight(double v)
    {
        setValue("SecondsSinceMidnight", QString::number(v, 'g', 17));
    }
    quint32 Log() const
    {
        return static_cast<quint32>(value("Log").toDouble());
    }
    void setLog(quint32 v)
    {
        setValue("Log", QString::number(v));
    }
    quint32 TripLog() const
    {
        return static_cast<quint32>(value("TripLog").toDouble());
    }
    void setTripLog(quint32 v)
    {
        setValue("TripLog", QString::number(v));
    }
};
class Pressure130314 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char SID READ SID WRITE setSID)
    Q_PROPERTY(unsigned char PressureInstance READ PressureInstance WRITE setPressureInstance)
    Q_PROPERTY(tN2kPressureSource PressureSource READ PressureSource WRITE setPressureSource)
    Q_PROPERTY(double ActualPressure READ ActualPressure WRITE setActualPressure)
  public:
    Pressure130314(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "130314", parent)
    {
    }
    unsigned char SID() const
    {
        return static_cast<unsigned char>(value("SID").toDouble());
    }
    void setSID(unsigned char v)
    {
        setValue("SID", QString::number(v));
    }
    unsigned char PressureInstance() const
    {
        return static_cast<unsigned char>(value("PressureInstance").toDouble());
    }
    void setPressureInstance(unsigned char v)
    {
        setValue("PressureInstance", QString::number(v));
    }
    tN2kPressureSource PressureSource() const
    {
        return static_cast<tN2kPressureSource>(value("PressureSource").toDouble());
    }
    void setPressureSource(tN2kPressureSource v)
    {
        setValue("PressureSource", QString::number(v));
    }
    double ActualPressure() const
    {
        return value("ActualPressure").toDouble();
    }
    void setActualPressure(double v)
    {
        setValue("ActualPressure", QString::number(v, 'g', 17));
    }
};
class EngineTrip127497 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char EngineInstance READ EngineInstance WRITE setEngineInstance)
    Q_PROPERTY(double TripFuelUsed READ TripFuelUsed WRITE setTripFuelUsed)
    Q_PROPERTY(double FuelRateAverage READ FuelRateAverage WRITE setFuelRateAverage)
    Q_PROPERTY(double FuelRateEconomy READ FuelRateEconomy WRITE setFuelRateEconomy)
    Q_PROPERTY(double InstantaneousFuelEconomy READ InstantaneousFuelEconomy WRITE setInstantaneousFuelEconomy)
  public:
    EngineTrip127497(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "127497", parent)
    {
    }
    unsigned char EngineInstance() const
    {
        return static_cast<unsigned char>(value("EngineInstance").toDouble());
    }
    void setEngineInstance(unsigned char v)
    {
        setValue("EngineInstance", QString::number(v));
    }
    double TripFuelUsed() const
    {
        return value("TripFuelUsed").toDouble();
    }
    void setTripFuelUsed(double v)
    {
        setValue("TripFuelUsed", QString::number(v, 'g', 17));
    }
    double FuelRateAverage() const
    {
        return value("FuelRateAverage").toDouble();
    }
    void setFuelRateAverage(double v)
    {
        setValue("FuelRateAverage", QString::number(v, 'g', 17));
    }
    double FuelRateEconomy() const
    {
        return value("FuelRateEconomy").toDouble();
    }
    void setFuelRateEconomy(double v)
    {
        setValue("FuelRateEconomy", QString::number(v, 'g', 17));
    }
    double InstantaneousFuelEconomy() const
    {
        return value("InstantaneousFuelEconomy").toDouble();
    }
    void setInstantaneousFuelEconomy(double v)
    {
        setValue("InstantaneousFuelEconomy", QString::number(v, 'g', 17));
    }
};
class DepthDPT final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(double DepthBelowTransducer READ DepthBelowTransducer WRITE setDepthBelowTransducer)
    Q_PROPERTY(double Offset READ Offset WRITE setOffset)
    Q_PROPERTY(double Range READ Range WRITE setRange)
  public:
    DepthDPT(int id, const QString &label, QWidget *parent = nullptr) : ExpansionInstrument(id, label, "DPT", parent)
    {
    }
    double DepthBelowTransducer() const
    {
        return value("DepthBelowTransducer").toDouble();
    }
    void setDepthBelowTransducer(double v)
    {
        setValue("DepthBelowTransducer", QString::number(v, 'g', 17));
    }
    double Offset() const
    {
        return value("Offset").toDouble();
    }
    void setOffset(double v)
    {
        setValue("Offset", QString::number(v, 'g', 17));
    }
    double Range() const
    {
        return value("Range").toDouble();
    }
    void setRange(double v)
    {
        setValue("Range", QString::number(v, 'g', 17));
    }
};
class DepthDBT final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(double DepthBelowTransducer READ DepthBelowTransducer WRITE setDepthBelowTransducer)
  public:
    DepthDBT(int id, const QString &label, QWidget *parent = nullptr) : ExpansionInstrument(id, label, "DBT", parent)
    {
    }
    double DepthBelowTransducer() const
    {
        return value("DepthBelowTransducer").toDouble();
    }
    void setDepthBelowTransducer(double v)
    {
        setValue("DepthBelowTransducer", QString::number(v, 'g', 17));
    }
};
class WindMWV final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(double WindAngle READ WindAngle WRITE setWindAngle)
    Q_PROPERTY(tNMEA0183WindReference Reference READ Reference WRITE setReference)
    Q_PROPERTY(double WindSpeed READ WindSpeed WRITE setWindSpeed)
    Q_PROPERTY(char Status READ Status WRITE setStatus)
  public:
    WindMWV(int id, const QString &label, QWidget *parent = nullptr) : ExpansionInstrument(id, label, "MWV", parent)
    {
    }
    double WindAngle() const
    {
        return value("WindAngle").toDouble();
    }
    void setWindAngle(double v)
    {
        setValue("WindAngle", QString::number(v, 'g', 17));
    }
    tNMEA0183WindReference Reference() const
    {
        return value("Reference") == "T" ? NMEA0183Wind_True : NMEA0183Wind_Apparent;
    }
    void setReference(tNMEA0183WindReference v)
    {
        if (v == NMEA0183Wind_True || v == NMEA0183Wind_Apparent)
            setValue("Reference", v == NMEA0183Wind_True ? "T" : "R");
    }
    double WindSpeed() const
    {
        return value("WindSpeed").toDouble();
    }
    void setWindSpeed(double v)
    {
        setValue("WindSpeed", QString::number(v, 'g', 17));
    }
    char Status() const
    {
        return value("Status").isEmpty() ? 'V' : value("Status").at(0).toLatin1();
    }
    void setStatus(char v)
    {
        setValue("Status", QString(QChar(v)));
    }
};
class WaterTemperatureMTW final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(double WaterTemperature READ WaterTemperature WRITE setWaterTemperature)
  public:
    WaterTemperatureMTW(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "MTW", parent)
    {
    }
    double WaterTemperature() const
    {
        return value("WaterTemperature").toDouble();
    }
    void setWaterTemperature(double v)
    {
        setValue("WaterTemperature", QString::number(v, 'g', 17));
    }
};
class WaterSpeedHeadingVHW final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(double TrueHeading READ TrueHeading WRITE setTrueHeading)
    Q_PROPERTY(double MagneticHeading READ MagneticHeading WRITE setMagneticHeading)
    Q_PROPERTY(double WaterSpeed READ WaterSpeed WRITE setWaterSpeed)
  public:
    WaterSpeedHeadingVHW(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "VHW", parent)
    {
    }
    double TrueHeading() const
    {
        return value("TrueHeading").toDouble();
    }
    void setTrueHeading(double v)
    {
        setValue("TrueHeading", QString::number(v, 'g', 17));
    }
    double MagneticHeading() const
    {
        return value("MagneticHeading").toDouble();
    }
    void setMagneticHeading(double v)
    {
        setValue("MagneticHeading", QString::number(v, 'g', 17));
    }
    double WaterSpeed() const
    {
        return value("WaterSpeed").toDouble();
    }
    void setWaterSpeed(double v)
    {
        setValue("WaterSpeed", QString::number(v, 'g', 17));
    }
};
class SatellitesGSV final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(uint32_t TotalMessages READ TotalMessages WRITE setTotalMessages)
    Q_PROPERTY(uint32_t MessageNumber READ MessageNumber WRITE setMessageNumber)
    Q_PROPERTY(uint32_t SatelliteCount READ SatelliteCount WRITE setSatelliteCount)
    Q_PROPERTY(QString Satellites READ Satellites WRITE setSatellites)
  public:
    SatellitesGSV(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "GSV", parent)
    {
    }
    uint32_t TotalMessages() const
    {
        return static_cast<uint32_t>(value("TotalMessages").toDouble());
    }
    void setTotalMessages(uint32_t v)
    {
        setValue("TotalMessages", QString::number(v));
    }
    uint32_t MessageNumber() const
    {
        return static_cast<uint32_t>(value("MessageNumber").toDouble());
    }
    void setMessageNumber(uint32_t v)
    {
        setValue("MessageNumber", QString::number(v));
    }
    uint32_t SatelliteCount() const
    {
        return static_cast<uint32_t>(value("SatelliteCount").toDouble());
    }
    void setSatelliteCount(uint32_t v)
    {
        setValue("SatelliteCount", QString::number(v));
    }
    QString Satellites() const
    {
        return value("Satellites");
    }
    void setSatellites(QString v)
    {
        setValue("Satellites", v);
    }
};

class VesselTrip127496 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(double TimeToEmpty READ TimeToEmpty WRITE setTimeToEmpty)
    Q_PROPERTY(double DistanceToEmpty READ DistanceToEmpty WRITE setDistanceToEmpty)
    Q_PROPERTY(double EstimatedFuelRemaining READ EstimatedFuelRemaining WRITE setEstimatedFuelRemaining)
    Q_PROPERTY(double TripRunTime READ TripRunTime WRITE setTripRunTime)
  public:
    VesselTrip127496(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "127496", parent)
    {
    }
    double TimeToEmpty() const
    {
        return static_cast<double>(value("TimeToEmpty").toDouble());
    }
    void setTimeToEmpty(double v)
    {
        setValue("TimeToEmpty", QString::number(v, 'g', 17));
    }
    double DistanceToEmpty() const
    {
        return static_cast<double>(value("DistanceToEmpty").toDouble());
    }
    void setDistanceToEmpty(double v)
    {
        setValue("DistanceToEmpty", QString::number(v, 'g', 17));
    }
    double EstimatedFuelRemaining() const
    {
        return static_cast<double>(value("EstimatedFuelRemaining").toDouble());
    }
    void setEstimatedFuelRemaining(double v)
    {
        setValue("EstimatedFuelRemaining", QString::number(v, 'g', 17));
    }
    double TripRunTime() const
    {
        return static_cast<double>(value("TripRunTime").toDouble());
    }
    void setTripRunTime(double v)
    {
        setValue("TripRunTime", QString::number(v, 'g', 17));
    }
};

class EngineStatic127498 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char EngineInstance READ EngineInstance WRITE setEngineInstance)
    Q_PROPERTY(double RatedEngineSpeed READ RatedEngineSpeed WRITE setRatedEngineSpeed)
    Q_PROPERTY(QString VIN READ VIN WRITE setVIN)
    Q_PROPERTY(QString SoftwareID READ SoftwareID WRITE setSoftwareID)
  public:
    EngineStatic127498(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "127498", parent)
    {
    }
    unsigned char EngineInstance() const
    {
        return static_cast<unsigned char>(value("EngineInstance").toDouble());
    }
    void setEngineInstance(unsigned char v)
    {
        setValue("EngineInstance", QString::number(v));
    }
    double RatedEngineSpeed() const
    {
        return static_cast<double>(value("RatedEngineSpeed").toDouble());
    }
    void setRatedEngineSpeed(double v)
    {
        setValue("RatedEngineSpeed", QString::number(v, 'g', 17));
    }
    QString VIN() const
    {
        return value("VIN");
    }
    void setVIN(QString v)
    {
        setValue("VIN", v);
    }
    QString SoftwareID() const
    {
        return value("SoftwareID");
    }
    void setSoftwareID(QString v)
    {
        setValue("SoftwareID", v);
    }
};

class InverterStatus127509 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char Instance READ Instance WRITE setInstance)
    Q_PROPERTY(unsigned char ACInstance READ ACInstance WRITE setACInstance)
    Q_PROPERTY(unsigned char DCInstance READ DCInstance WRITE setDCInstance)
    Q_PROPERTY(unsigned char OperatingState READ OperatingState WRITE setOperatingState)
    Q_PROPERTY(tN2kOnOff InverterEnable READ InverterEnable WRITE setInverterEnable)
  public:
    InverterStatus127509(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "127509", parent)
    {
    }
    unsigned char Instance() const
    {
        return static_cast<unsigned char>(value("Instance").toDouble());
    }
    void setInstance(unsigned char v)
    {
        setValue("Instance", QString::number(v));
    }
    unsigned char ACInstance() const
    {
        return static_cast<unsigned char>(value("ACInstance").toDouble());
    }
    void setACInstance(unsigned char v)
    {
        setValue("ACInstance", QString::number(v));
    }
    unsigned char DCInstance() const
    {
        return static_cast<unsigned char>(value("DCInstance").toDouble());
    }
    void setDCInstance(unsigned char v)
    {
        setValue("DCInstance", QString::number(v));
    }
    unsigned char OperatingState() const
    {
        return static_cast<unsigned char>(value("OperatingState").toDouble());
    }
    void setOperatingState(unsigned char v)
    {
        setValue("OperatingState", QString::number(v));
    }
    tN2kOnOff InverterEnable() const
    {
        return static_cast<tN2kOnOff>(value("InverterEnable").toDouble());
    }
    void setInverterEnable(tN2kOnOff v)
    {
        setValue("InverterEnable", QString::number(v));
    }
};

class GnssActiveGSA final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(QString SelectionMode READ SelectionMode WRITE setSelectionMode)
    Q_PROPERTY(unsigned char FixType READ FixType WRITE setFixType)
    Q_PROPERTY(unsigned char Satellite1 READ Satellite1 WRITE setSatellite1)
    Q_PROPERTY(unsigned char Satellite2 READ Satellite2 WRITE setSatellite2)
    Q_PROPERTY(unsigned char Satellite3 READ Satellite3 WRITE setSatellite3)
    Q_PROPERTY(unsigned char Satellite4 READ Satellite4 WRITE setSatellite4)
    Q_PROPERTY(unsigned char Satellite5 READ Satellite5 WRITE setSatellite5)
    Q_PROPERTY(unsigned char Satellite6 READ Satellite6 WRITE setSatellite6)
    Q_PROPERTY(unsigned char Satellite7 READ Satellite7 WRITE setSatellite7)
    Q_PROPERTY(unsigned char Satellite8 READ Satellite8 WRITE setSatellite8)
    Q_PROPERTY(unsigned char Satellite9 READ Satellite9 WRITE setSatellite9)
    Q_PROPERTY(unsigned char Satellite10 READ Satellite10 WRITE setSatellite10)
    Q_PROPERTY(unsigned char Satellite11 READ Satellite11 WRITE setSatellite11)
    Q_PROPERTY(unsigned char Satellite12 READ Satellite12 WRITE setSatellite12)
    Q_PROPERTY(double PDOP READ PDOP WRITE setPDOP)
    Q_PROPERTY(double HDOP READ HDOP WRITE setHDOP)
    Q_PROPERTY(double VDOP READ VDOP WRITE setVDOP)
  public:
    GnssActiveGSA(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "GSA", parent)
    {
    }
    QString SelectionMode() const
    {
        return value("SelectionMode");
    }
    void setSelectionMode(QString v)
    {
        setValue("SelectionMode", v);
    }
    unsigned char FixType() const
    {
        return static_cast<unsigned char>(value("FixType").toDouble());
    }
    void setFixType(unsigned char v)
    {
        setValue("FixType", QString::number(v));
    }
    unsigned char Satellite1() const
    {
        return static_cast<unsigned char>(value("Satellite1").toDouble());
    }
    void setSatellite1(unsigned char v)
    {
        setValue("Satellite1", QString::number(v));
    }
    unsigned char Satellite2() const
    {
        return static_cast<unsigned char>(value("Satellite2").toDouble());
    }
    void setSatellite2(unsigned char v)
    {
        setValue("Satellite2", QString::number(v));
    }
    unsigned char Satellite3() const
    {
        return static_cast<unsigned char>(value("Satellite3").toDouble());
    }
    void setSatellite3(unsigned char v)
    {
        setValue("Satellite3", QString::number(v));
    }
    unsigned char Satellite4() const
    {
        return static_cast<unsigned char>(value("Satellite4").toDouble());
    }
    void setSatellite4(unsigned char v)
    {
        setValue("Satellite4", QString::number(v));
    }
    unsigned char Satellite5() const
    {
        return static_cast<unsigned char>(value("Satellite5").toDouble());
    }
    void setSatellite5(unsigned char v)
    {
        setValue("Satellite5", QString::number(v));
    }
    unsigned char Satellite6() const
    {
        return static_cast<unsigned char>(value("Satellite6").toDouble());
    }
    void setSatellite6(unsigned char v)
    {
        setValue("Satellite6", QString::number(v));
    }
    unsigned char Satellite7() const
    {
        return static_cast<unsigned char>(value("Satellite7").toDouble());
    }
    void setSatellite7(unsigned char v)
    {
        setValue("Satellite7", QString::number(v));
    }
    unsigned char Satellite8() const
    {
        return static_cast<unsigned char>(value("Satellite8").toDouble());
    }
    void setSatellite8(unsigned char v)
    {
        setValue("Satellite8", QString::number(v));
    }
    unsigned char Satellite9() const
    {
        return static_cast<unsigned char>(value("Satellite9").toDouble());
    }
    void setSatellite9(unsigned char v)
    {
        setValue("Satellite9", QString::number(v));
    }
    unsigned char Satellite10() const
    {
        return static_cast<unsigned char>(value("Satellite10").toDouble());
    }
    void setSatellite10(unsigned char v)
    {
        setValue("Satellite10", QString::number(v));
    }
    unsigned char Satellite11() const
    {
        return static_cast<unsigned char>(value("Satellite11").toDouble());
    }
    void setSatellite11(unsigned char v)
    {
        setValue("Satellite11", QString::number(v));
    }
    unsigned char Satellite12() const
    {
        return static_cast<unsigned char>(value("Satellite12").toDouble());
    }
    void setSatellite12(unsigned char v)
    {
        setValue("Satellite12", QString::number(v));
    }
    double PDOP() const
    {
        return static_cast<double>(value("PDOP").toDouble());
    }
    void setPDOP(double v)
    {
        setValue("PDOP", QString::number(v, 'g', 17));
    }
    double HDOP() const
    {
        return static_cast<double>(value("HDOP").toDouble());
    }
    void setHDOP(double v)
    {
        setValue("HDOP", QString::number(v, 'g', 17));
    }
    double VDOP() const
    {
        return static_cast<double>(value("VDOP").toDouble());
    }
    void setVDOP(double v)
    {
        setValue("VDOP", QString::number(v, 'g', 17));
    }
};

class RouteRTE final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char TotalMessages READ TotalMessages WRITE setTotalMessages)
    Q_PROPERTY(unsigned char MessageNumber READ MessageNumber WRITE setMessageNumber)
    Q_PROPERTY(char RouteType READ RouteType WRITE setRouteType)
    Q_PROPERTY(QString RouteName READ RouteName WRITE setRouteName)
    Q_PROPERTY(QString WaypointIDs READ WaypointIDs WRITE setWaypointIDs)
  public:
    RouteRTE(int id, const QString &label, QWidget *parent = nullptr) : ExpansionInstrument(id, label, "RTE", parent)
    {
    }
    unsigned char TotalMessages() const
    {
        return static_cast<unsigned char>(value("TotalMessages").toDouble());
    }
    void setTotalMessages(unsigned char v)
    {
        setValue("TotalMessages", QString::number(v));
    }
    unsigned char MessageNumber() const
    {
        return static_cast<unsigned char>(value("MessageNumber").toDouble());
    }
    void setMessageNumber(unsigned char v)
    {
        setValue("MessageNumber", QString::number(v));
    }
    char RouteType() const
    {
        return value("RouteType").isEmpty() ? char(0) : value("RouteType").at(0).toLatin1();
    }
    void setRouteType(char v)
    {
        setValue("RouteType", QString(QChar(v)));
    }
    QString RouteName() const
    {
        return value("RouteName");
    }
    void setRouteName(QString v)
    {
        setValue("RouteName", v);
    }
    QString WaypointIDs() const
    {
        return value("WaypointIDs");
    }
    void setWaypointIDs(QString v)
    {
        setValue("WaypointIDs", v);
    }
};

class WaypointWPL final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(double Latitude READ Latitude WRITE setLatitude)
    Q_PROPERTY(double Longitude READ Longitude WRITE setLongitude)
    Q_PROPERTY(QString WaypointName READ WaypointName WRITE setWaypointName)
  public:
    WaypointWPL(int id, const QString &label, QWidget *parent = nullptr) : ExpansionInstrument(id, label, "WPL", parent)
    {
    }
    double Latitude() const
    {
        return static_cast<double>(value("Latitude").toDouble());
    }
    void setLatitude(double v)
    {
        setValue("Latitude", QString::number(v, 'g', 17));
    }
    double Longitude() const
    {
        return static_cast<double>(value("Longitude").toDouble());
    }
    void setLongitude(double v)
    {
        setValue("Longitude", QString::number(v, 'g', 17));
    }
    QString WaypointName() const
    {
        return value("WaypointName");
    }
    void setWaypointName(QString v)
    {
        setValue("WaypointName", v);
    }
};

class AutopilotAPB final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(char Status READ Status WRITE setStatus)
    Q_PROPERTY(char CycleLockStatus READ CycleLockStatus WRITE setCycleLockStatus)
    Q_PROPERTY(char SteerDirection READ SteerDirection WRITE setSteerDirection)
    Q_PROPERTY(char ArrivalAlarm READ ArrivalAlarm WRITE setArrivalAlarm)
    Q_PROPERTY(char PerpendicularPassed READ PerpendicularPassed WRITE setPerpendicularPassed)
    Q_PROPERTY(char OriginReference READ OriginReference WRITE setOriginReference)
    Q_PROPERTY(char PositionReference READ PositionReference WRITE setPositionReference)
    Q_PROPERTY(char HeadingReference READ HeadingReference WRITE setHeadingReference)
    Q_PROPERTY(double XTE READ XTE WRITE setXTE)
    Q_PROPERTY(double OriginBearing READ OriginBearing WRITE setOriginBearing)
    Q_PROPERTY(double PositionBearing READ PositionBearing WRITE setPositionBearing)
    Q_PROPERTY(double HeadingToSteer READ HeadingToSteer WRITE setHeadingToSteer)
    Q_PROPERTY(QString DestinationID READ DestinationID WRITE setDestinationID)
  public:
    AutopilotAPB(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "APB", parent)
    {
    }
    char Status() const
    {
        return value("Status").isEmpty() ? char(0) : value("Status").at(0).toLatin1();
    }
    void setStatus(char v)
    {
        setValue("Status", QString(QChar(v)));
    }
    char CycleLockStatus() const
    {
        return value("CycleLockStatus").isEmpty() ? char(0) : value("CycleLockStatus").at(0).toLatin1();
    }
    void setCycleLockStatus(char v)
    {
        setValue("CycleLockStatus", QString(QChar(v)));
    }
    char SteerDirection() const
    {
        return value("SteerDirection").isEmpty() ? char(0) : value("SteerDirection").at(0).toLatin1();
    }
    void setSteerDirection(char v)
    {
        setValue("SteerDirection", QString(QChar(v)));
    }
    char ArrivalAlarm() const
    {
        return value("ArrivalAlarm").isEmpty() ? char(0) : value("ArrivalAlarm").at(0).toLatin1();
    }
    void setArrivalAlarm(char v)
    {
        setValue("ArrivalAlarm", QString(QChar(v)));
    }
    char PerpendicularPassed() const
    {
        return value("PerpendicularPassed").isEmpty() ? char(0) : value("PerpendicularPassed").at(0).toLatin1();
    }
    void setPerpendicularPassed(char v)
    {
        setValue("PerpendicularPassed", QString(QChar(v)));
    }
    char OriginReference() const
    {
        return value("OriginReference").isEmpty() ? char(0) : value("OriginReference").at(0).toLatin1();
    }
    void setOriginReference(char v)
    {
        setValue("OriginReference", QString(QChar(v)));
    }
    char PositionReference() const
    {
        return value("PositionReference").isEmpty() ? char(0) : value("PositionReference").at(0).toLatin1();
    }
    void setPositionReference(char v)
    {
        setValue("PositionReference", QString(QChar(v)));
    }
    char HeadingReference() const
    {
        return value("HeadingReference").isEmpty() ? char(0) : value("HeadingReference").at(0).toLatin1();
    }
    void setHeadingReference(char v)
    {
        setValue("HeadingReference", QString(QChar(v)));
    }
    double XTE() const
    {
        return static_cast<double>(value("XTE").toDouble());
    }
    void setXTE(double v)
    {
        setValue("XTE", QString::number(v, 'g', 17));
    }
    double OriginBearing() const
    {
        return static_cast<double>(value("OriginBearing").toDouble());
    }
    void setOriginBearing(double v)
    {
        setValue("OriginBearing", QString::number(v, 'g', 17));
    }
    double PositionBearing() const
    {
        return static_cast<double>(value("PositionBearing").toDouble());
    }
    void setPositionBearing(double v)
    {
        setValue("PositionBearing", QString::number(v, 'g', 17));
    }
    double HeadingToSteer() const
    {
        return static_cast<double>(value("HeadingToSteer").toDouble());
    }
    void setHeadingToSteer(double v)
    {
        setValue("HeadingToSteer", QString::number(v, 'g', 17));
    }
    QString DestinationID() const
    {
        return value("DestinationID");
    }
    void setDestinationID(QString v)
    {
        setValue("DestinationID", v);
    }
};

class NavigationRMB final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(char Status READ Status WRITE setStatus)
    Q_PROPERTY(char SteerDirection READ SteerDirection WRITE setSteerDirection)
    Q_PROPERTY(char ArrivalAlarm READ ArrivalAlarm WRITE setArrivalAlarm)
    Q_PROPERTY(double XTE READ XTE WRITE setXTE)
    Q_PROPERTY(double Latitude READ Latitude WRITE setLatitude)
    Q_PROPERTY(double Longitude READ Longitude WRITE setLongitude)
    Q_PROPERTY(double DistanceToWaypoint READ DistanceToWaypoint WRITE setDistanceToWaypoint)
    Q_PROPERTY(double BearingToWaypoint READ BearingToWaypoint WRITE setBearingToWaypoint)
    Q_PROPERTY(double ClosingVelocity READ ClosingVelocity WRITE setClosingVelocity)
    Q_PROPERTY(QString OriginID READ OriginID WRITE setOriginID)
    Q_PROPERTY(QString DestinationID READ DestinationID WRITE setDestinationID)
  public:
    NavigationRMB(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "RMB", parent)
    {
    }
    char Status() const
    {
        return value("Status").isEmpty() ? char(0) : value("Status").at(0).toLatin1();
    }
    void setStatus(char v)
    {
        setValue("Status", QString(QChar(v)));
    }
    char SteerDirection() const
    {
        return value("SteerDirection").isEmpty() ? char(0) : value("SteerDirection").at(0).toLatin1();
    }
    void setSteerDirection(char v)
    {
        setValue("SteerDirection", QString(QChar(v)));
    }
    char ArrivalAlarm() const
    {
        return value("ArrivalAlarm").isEmpty() ? char(0) : value("ArrivalAlarm").at(0).toLatin1();
    }
    void setArrivalAlarm(char v)
    {
        setValue("ArrivalAlarm", QString(QChar(v)));
    }
    double XTE() const
    {
        return static_cast<double>(value("XTE").toDouble());
    }
    void setXTE(double v)
    {
        setValue("XTE", QString::number(v, 'g', 17));
    }
    double Latitude() const
    {
        return static_cast<double>(value("Latitude").toDouble());
    }
    void setLatitude(double v)
    {
        setValue("Latitude", QString::number(v, 'g', 17));
    }
    double Longitude() const
    {
        return static_cast<double>(value("Longitude").toDouble());
    }
    void setLongitude(double v)
    {
        setValue("Longitude", QString::number(v, 'g', 17));
    }
    double DistanceToWaypoint() const
    {
        return static_cast<double>(value("DistanceToWaypoint").toDouble());
    }
    void setDistanceToWaypoint(double v)
    {
        setValue("DistanceToWaypoint", QString::number(v, 'g', 17));
    }
    double BearingToWaypoint() const
    {
        return static_cast<double>(value("BearingToWaypoint").toDouble());
    }
    void setBearingToWaypoint(double v)
    {
        setValue("BearingToWaypoint", QString::number(v, 'g', 17));
    }
    double ClosingVelocity() const
    {
        return static_cast<double>(value("ClosingVelocity").toDouble());
    }
    void setClosingVelocity(double v)
    {
        setValue("ClosingVelocity", QString::number(v, 'g', 17));
    }
    QString OriginID() const
    {
        return value("OriginID");
    }
    void setOriginID(QString v)
    {
        setValue("OriginID", v);
    }
    QString DestinationID() const
    {
        return value("DestinationID");
    }
    void setDestinationID(QString v)
    {
        setValue("DestinationID", v);
    }
};

class RouteInfo129285 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(uint16_t StartRPS READ StartRPS WRITE setStartRPS)
    Q_PROPERTY(uint16_t DatabaseID READ DatabaseID WRITE setDatabaseID)
    Q_PROPERTY(uint16_t RouteID READ RouteID WRITE setRouteID)
    Q_PROPERTY(tN2kNavigationDirection NavigationDirection READ NavigationDirection WRITE setNavigationDirection)
    Q_PROPERTY(tN2kGenericStatusPair SupplementaryData READ SupplementaryData WRITE setSupplementaryData)
    Q_PROPERTY(QString RouteName READ RouteName WRITE setRouteName)
    Q_PROPERTY(QString Waypoints READ Waypoints WRITE setWaypoints)
  public:
    RouteInfo129285(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "129285", parent)
    {
    }
    uint16_t StartRPS() const
    {
        return static_cast<uint16_t>(value("StartRPS").toDouble());
    }
    void setStartRPS(uint16_t v)
    {
        setValue("StartRPS", QString::number(v));
    }
    uint16_t DatabaseID() const
    {
        return static_cast<uint16_t>(value("DatabaseID").toDouble());
    }
    void setDatabaseID(uint16_t v)
    {
        setValue("DatabaseID", QString::number(v));
    }
    uint16_t RouteID() const
    {
        return static_cast<uint16_t>(value("RouteID").toDouble());
    }
    void setRouteID(uint16_t v)
    {
        setValue("RouteID", QString::number(v));
    }
    tN2kNavigationDirection NavigationDirection() const
    {
        return static_cast<tN2kNavigationDirection>(value("NavigationDirection").toDouble());
    }
    void setNavigationDirection(tN2kNavigationDirection v)
    {
        setValue("NavigationDirection", QString::number(v));
    }
    tN2kGenericStatusPair SupplementaryData() const
    {
        return static_cast<tN2kGenericStatusPair>(value("SupplementaryData").toDouble());
    }
    void setSupplementaryData(tN2kGenericStatusPair v)
    {
        setValue("SupplementaryData", QString::number(v));
    }
    QString RouteName() const
    {
        return value("RouteName");
    }
    void setRouteName(QString v)
    {
        setValue("RouteName", v);
    }
    QString Waypoints() const
    {
        return value("Waypoints");
    }
    void setWaypoints(QString v)
    {
        setValue("Waypoints", v);
    }
};

class WaypointList130074 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(uint16_t StartWaypointID READ StartWaypointID WRITE setStartWaypointID)
    Q_PROPERTY(uint16_t TotalWaypoints READ TotalWaypoints WRITE setTotalWaypoints)
    Q_PROPERTY(uint16_t DatabaseID READ DatabaseID WRITE setDatabaseID)
    Q_PROPERTY(QString Waypoints READ Waypoints WRITE setWaypoints)
  public:
    WaypointList130074(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "130074", parent)
    {
    }
    uint16_t StartWaypointID() const
    {
        return static_cast<uint16_t>(value("StartWaypointID").toDouble());
    }
    void setStartWaypointID(uint16_t v)
    {
        setValue("StartWaypointID", QString::number(v));
    }
    uint16_t TotalWaypoints() const
    {
        return static_cast<uint16_t>(value("TotalWaypoints").toDouble());
    }
    void setTotalWaypoints(uint16_t v)
    {
        setValue("TotalWaypoints", QString::number(v));
    }
    uint16_t DatabaseID() const
    {
        return static_cast<uint16_t>(value("DatabaseID").toDouble());
    }
    void setDatabaseID(uint16_t v)
    {
        setValue("DatabaseID", QString::number(v));
    }
    QString Waypoints() const
    {
        return value("Waypoints");
    }
    void setWaypoints(QString v)
    {
        setValue("Waypoints", v);
    }
};

class Datum129044 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(QString LocalDatum READ LocalDatum WRITE setLocalDatum)
    Q_PROPERTY(double DeltaLatitude READ DeltaLatitude WRITE setDeltaLatitude)
    Q_PROPERTY(double DeltaLongitude READ DeltaLongitude WRITE setDeltaLongitude)
    Q_PROPERTY(double DeltaAltitude READ DeltaAltitude WRITE setDeltaAltitude)
    Q_PROPERTY(QString ReferenceDatum READ ReferenceDatum WRITE setReferenceDatum)
  public:
    Datum129044(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "129044", parent) {}
    QString LocalDatum() const { return value("LocalDatum"); }
    void setLocalDatum(QString v) { setValue("LocalDatum", v); }
    double DeltaLatitude() const { return static_cast<double>(value("DeltaLatitude").toDouble()); }
    void setDeltaLatitude(double v) { setValue("DeltaLatitude", QString::number(v, 'g', 17)); }
    double DeltaLongitude() const { return static_cast<double>(value("DeltaLongitude").toDouble()); }
    void setDeltaLongitude(double v) { setValue("DeltaLongitude", QString::number(v, 'g', 17)); }
    double DeltaAltitude() const { return static_cast<double>(value("DeltaAltitude").toDouble()); }
    void setDeltaAltitude(double v) { setValue("DeltaAltitude", QString::number(v, 'g', 17)); }
    QString ReferenceDatum() const { return value("ReferenceDatum"); }
    void setReferenceDatum(QString v) { setValue("ReferenceDatum", v); }
};

class GnssNoise129542 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char SID READ SID WRITE setSID)
    Q_PROPERTY(double RMSPositionUncertainty READ RMSPositionUncertainty WRITE setRMSPositionUncertainty)
    Q_PROPERTY(double MajorAxisDeviation READ MajorAxisDeviation WRITE setMajorAxisDeviation)
    Q_PROPERTY(double MinorAxisDeviation READ MinorAxisDeviation WRITE setMinorAxisDeviation)
    Q_PROPERTY(double MajorAxisOrientation READ MajorAxisOrientation WRITE setMajorAxisOrientation)
    Q_PROPERTY(double LatitudeDeviation READ LatitudeDeviation WRITE setLatitudeDeviation)
    Q_PROPERTY(double LongitudeDeviation READ LongitudeDeviation WRITE setLongitudeDeviation)
    Q_PROPERTY(double AltitudeDeviation READ AltitudeDeviation WRITE setAltitudeDeviation)
  public:
    GnssNoise129542(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "129542", parent) {}
    unsigned char SID() const { return static_cast<unsigned char>(value("SID").toDouble()); }
    void setSID(unsigned char v) { setValue("SID", QString::number(v)); }
    double RMSPositionUncertainty() const { return static_cast<double>(value("RMSPositionUncertainty").toDouble()); }
    void setRMSPositionUncertainty(double v) { setValue("RMSPositionUncertainty", QString::number(v, 'g', 17)); }
    double MajorAxisDeviation() const { return static_cast<double>(value("MajorAxisDeviation").toDouble()); }
    void setMajorAxisDeviation(double v) { setValue("MajorAxisDeviation", QString::number(v, 'g', 17)); }
    double MinorAxisDeviation() const { return static_cast<double>(value("MinorAxisDeviation").toDouble()); }
    void setMinorAxisDeviation(double v) { setValue("MinorAxisDeviation", QString::number(v, 'g', 17)); }
    double MajorAxisOrientation() const { return static_cast<double>(value("MajorAxisOrientation").toDouble()); }
    void setMajorAxisOrientation(double v) { setValue("MajorAxisOrientation", QString::number(v, 'g', 17)); }
    double LatitudeDeviation() const { return static_cast<double>(value("LatitudeDeviation").toDouble()); }
    void setLatitudeDeviation(double v) { setValue("LatitudeDeviation", QString::number(v, 'g', 17)); }
    double LongitudeDeviation() const { return static_cast<double>(value("LongitudeDeviation").toDouble()); }
    void setLongitudeDeviation(double v) { setValue("LongitudeDeviation", QString::number(v, 'g', 17)); }
    double AltitudeDeviation() const { return static_cast<double>(value("AltitudeDeviation").toDouble()); }
    void setAltitudeDeviation(double v) { setValue("AltitudeDeviation", QString::number(v, 'g', 17)); }
};

class GnssError129547 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(unsigned char SID READ SID WRITE setSID)
    Q_PROPERTY(double RMSRangeInputDeviation READ RMSRangeInputDeviation WRITE setRMSRangeInputDeviation)
    Q_PROPERTY(double MajorErrorEllipseDeviation READ MajorErrorEllipseDeviation WRITE setMajorErrorEllipseDeviation)
    Q_PROPERTY(double MinorErrorEllipseDeviation READ MinorErrorEllipseDeviation WRITE setMinorErrorEllipseDeviation)
    Q_PROPERTY(double ErrorEllipseOrientation READ ErrorEllipseOrientation WRITE setErrorEllipseOrientation)
    Q_PROPERTY(double LatitudeDeviation READ LatitudeDeviation WRITE setLatitudeDeviation)
    Q_PROPERTY(double LongitudeDeviation READ LongitudeDeviation WRITE setLongitudeDeviation)
    Q_PROPERTY(double AltitudeDeviation READ AltitudeDeviation WRITE setAltitudeDeviation)
  public:
    GnssError129547(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "129547", parent) {}
    unsigned char SID() const { return static_cast<unsigned char>(value("SID").toDouble()); }
    void setSID(unsigned char v) { setValue("SID", QString::number(v)); }
    double RMSRangeInputDeviation() const { return static_cast<double>(value("RMSRangeInputDeviation").toDouble()); }
    void setRMSRangeInputDeviation(double v) { setValue("RMSRangeInputDeviation", QString::number(v, 'g', 17)); }
    double MajorErrorEllipseDeviation() const { return static_cast<double>(value("MajorErrorEllipseDeviation").toDouble()); }
    void setMajorErrorEllipseDeviation(double v) { setValue("MajorErrorEllipseDeviation", QString::number(v, 'g', 17)); }
    double MinorErrorEllipseDeviation() const { return static_cast<double>(value("MinorErrorEllipseDeviation").toDouble()); }
    void setMinorErrorEllipseDeviation(double v) { setValue("MinorErrorEllipseDeviation", QString::number(v, 'g', 17)); }
    double ErrorEllipseOrientation() const { return static_cast<double>(value("ErrorEllipseOrientation").toDouble()); }
    void setErrorEllipseOrientation(double v) { setValue("ErrorEllipseOrientation", QString::number(v, 'g', 17)); }
    double LatitudeDeviation() const { return static_cast<double>(value("LatitudeDeviation").toDouble()); }
    void setLatitudeDeviation(double v) { setValue("LatitudeDeviation", QString::number(v, 'g', 17)); }
    double LongitudeDeviation() const { return static_cast<double>(value("LongitudeDeviation").toDouble()); }
    void setLongitudeDeviation(double v) { setValue("LongitudeDeviation", QString::number(v, 'g', 17)); }
    double AltitudeDeviation() const { return static_cast<double>(value("AltitudeDeviation").toDouble()); }
    void setAltitudeDeviation(double v) { setValue("AltitudeDeviation", QString::number(v, 'g', 17)); }
};

class PilotHeading65359 final : public ExpansionInstrument
{
    Q_OBJECT
    Q_PROPERTY(uint16_t ManufacturerCode READ ManufacturerCode WRITE setManufacturerCode)
    Q_PROPERTY(unsigned char HeaderReserved READ HeaderReserved WRITE setHeaderReserved)
    Q_PROPERTY(unsigned char IndustryCode READ IndustryCode WRITE setIndustryCode)
    Q_PROPERTY(unsigned char SID READ SID WRITE setSID)
    Q_PROPERTY(double TrueHeading READ TrueHeading WRITE setTrueHeading)
    Q_PROPERTY(double MagneticHeading READ MagneticHeading WRITE setMagneticHeading)
    Q_PROPERTY(unsigned char ReservedByte READ ReservedByte WRITE setReservedByte)
  public:
    PilotHeading65359(int id, const QString &label, QWidget *parent = nullptr)
        : ExpansionInstrument(id, label, "65359", parent) {}
    uint16_t ManufacturerCode() const { return static_cast<uint16_t>(value("ManufacturerCode").toDouble()); }
    void setManufacturerCode(uint16_t v) { setValue("ManufacturerCode", QString::number(v)); }
    unsigned char HeaderReserved() const { return static_cast<unsigned char>(value("HeaderReserved").toDouble()); }
    void setHeaderReserved(unsigned char v) { setValue("HeaderReserved", QString::number(v)); }
    unsigned char IndustryCode() const { return static_cast<unsigned char>(value("IndustryCode").toDouble()); }
    void setIndustryCode(unsigned char v) { setValue("IndustryCode", QString::number(v)); }
    unsigned char SID() const { return static_cast<unsigned char>(value("SID").toDouble()); }
    void setSID(unsigned char v) { setValue("SID", QString::number(v)); }
    double TrueHeading() const { return static_cast<double>(value("TrueHeading").toDouble()); }
    void setTrueHeading(double v) { setValue("TrueHeading", QString::number(v, 'g', 17)); }
    double MagneticHeading() const { return static_cast<double>(value("MagneticHeading").toDouble()); }
    void setMagneticHeading(double v) { setValue("MagneticHeading", QString::number(v, 'g', 17)); }
    unsigned char ReservedByte() const { return static_cast<unsigned char>(value("ReservedByte").toDouble()); }
    void setReservedByte(unsigned char v) { setValue("ReservedByte", QString::number(v)); }
};
