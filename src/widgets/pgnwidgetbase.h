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

#ifndef PGNWIDGETBASE_H
#define PGNWIDGETBASE_H

#include <QFocusEvent>
#include <QStyle>
#include <QTimer>
#include <QWidget>
#include "datastructs.h"

class QLineEdit;

class PgnWidgetBase : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int Id READ Id WRITE setId NOTIFY IdChanged FINAL)
    Q_PROPERTY(int Pgn READ Pgn WRITE setPgn NOTIFY PgnChanged FINAL)
    Q_PROPERTY(QString Source READ Source WRITE setSource NOTIFY SourceChanged FINAL)
    Q_PROPERTY(int ColumnSpan READ ColumnSpan WRITE setColumnSpan NOTIFY ColumnSpanChanged FINAL)
    Q_PROPERTY(int RowSpan READ RowSpan WRITE setRowSpan NOTIFY RowSpanChanged FINAL)
    Q_PROPERTY(UserUnitPrefs UserPrefs READ UserPrefs WRITE setUserPrefs NOTIFY UserPrefsChanged FINAL)
    Q_PROPERTY(QString Label READ Label WRITE setLabel NOTIFY LabelChanged FINAL)
    Q_PROPERTY(bool Enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged FINAL)

public:
    explicit PgnWidgetBase(int id, const QString &label, QWidget *parent = nullptr);
    ~PgnWidgetBase();

    int Id() const;
    void setId(int newId);

    int Pgn() const;
    void setPgn(int newPgn);

    int ColumnSpan() const;
    void setColumnSpan(int newColumnSpan);

    int RowSpan() const;
    void setRowSpan(int newRowSpan);

    UserUnitPrefs UserPrefs() const;
    void setUserPrefs(const UserUnitPrefs &newUserPrefs);

    QString Label() const;
    void setLabel(const QString &newLabel);

    bool isGenerator() const { return QString(metaObject()->className()).startsWith("Generator"); }
    bool dispatch() const { return m_dispatch; }
    quint64 dispatchEpoch() const { return m_dispatchEpoch; }
    bool deliveryAllowed() const { return !isGenerator() || (isEnabled() && dispatch()); }
    void setDispatch(bool value);
    void initializeGeneratorControls();
    bool isEnabled() const;
    void setEnabled(bool newEnabled);

    virtual void setInitialGuiValues();
    bool acceptsEdit(QLineEdit *editor, const QString &text) const;

    QString Source() const;
    void setSource(const QString &newSource);

signals:
    void presentationRefreshStarted();
    void presentationRefreshFinished();
    // A genuine local sample may equal the previous value; this never drives numeric bindings.
    void parametersSampled(const QStringList &fields);
    void IdChanged();
    void PgnChanged();
    void ColumnSpanChanged();
    void RowSpanChanged();
    void LabelChanged();
    void UserPrefsChanged();
    void WidgetHasFocus(int id);
    void enabledChanged();
    void dispatchChanged();
    void MyVesselChanged();

    void SourceChanged();

protected:
    void focusInEvent(QFocusEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    QTimer *timer;
    const int DELAY_1S = 1000;
    const int DELAY_250MS = 250;
    QString m_SignalToEmit = "";
    QString m_ValueToEmit = "";

    QList<QString> unitsListTemp = {"Kelvin (K)", "Celsius (°C)", "Fahrenheit (°F)"};
    QList<QString> unitsListPressure = {"Pa", "kPa", "Bar", "PSI"};
    QList<QString> unitsListFlowRate = {"Liters/hour (L/h)", "US gallons/hour (US gal/h)"};
    QList<QString> unitsListVolume = {"Liters (L)", "US gallons (gal)"};
    QList<QString> unitsListSpeed = {"m/s", "km/h", "MPH", "knots"};
    QList<QString> unitsListDepth = {"Meters", "Feet", "Fathoms"};
    QList<QString> unitsListLatLong = {"DD.DDDD", "DD°MM’SS", "DD°MM.MMM’"};
    QList<QString> unitsListDistance = {"km & m", "NM & ft", "mi & ft"};
    QList<QString> unitsListEconomy = {"L/km", "L/nm", "gal/nm", "MPG", "nm/L", "nm/gal"};
    QList<QString> unitsListHeading = {"True", "Magnetic"};
    QList<QString> unitsListRateOfTurn = {"rad/s", "deg/s"};
    QList<QString> unitsListTime = {"HH:MM", "HH:MM AM/PM", "HH:MM:SS", "HH:MM:SS AM/PM"};
    QList<QString> unitsListDate = {"MM/DD/YYYY", "DD/MM/YYYY", "YYYY/MM/DD"};
    QList<QString> unitsListAtmosphericPressure = {"hPa", "inHg", "mmHg", "Pa", "atm"};

protected slots:
    virtual void onTimerTimeout();
    void ChangeSpeedUnits_triggered(int index);
    void ChangeEconomyUnits_triggered(int index);
    void ChangeDepthUnits_triggered(int index);
    void ChangeLatLongUnits_triggered(int index);
    void ChangeHeadingUnits_triggered(int index);
    void ChangeFuelRateUnits_triggered(int index);
    void ChangeTempUnits_triggered(int index);
    void ChangeVolumeUnits_triggered(int index);
    void ChangeDistanceUnits_triggered(int index);
    void ChangePressureUnits_triggered(int index);
    void ChangeRateOfTurnUnits_triggered(int index);
    void ChangeTimeUnits_triggered(int index);
    void ChangeDateUnits_triggered(int index);
    void ChangeAtmosphericPressureUnits_triggered(int index);

private:
    void initializeInstrumentEdits();
    void finishInstrumentEdit(QLineEdit *editor);
    void renderInstrumentEdits(QLineEdit *editor);
    void refreshPresentation();
    void updateTextSizing();
    bool textSizingPending = false;
    int m_Id;
    int m_Pgn = 0;
    int m_ColumnSpan = 3;
    int m_RowSpan = 3;
    UserUnitPrefs m_UserPrefs;

    virtual void initializeUnitsLimits();
    virtual void initializeClickableLabels();

    QString m_Label;
    bool m_dispatch = true;
    quint64 m_dispatchEpoch = 0;
    void updateControlGuidance();
    bool m_Enabled = true; // default widget enabled to send data to NMEA2000
    QString m_Source;
};

#endif // PGNWIDGETBASE_H
