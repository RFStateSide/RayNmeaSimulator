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

#ifndef ENGINEPROFILEDIALOG_H
#define ENGINEPROFILEDIALOG_H
#include "engineprofilecatalog.h"
#include <QDialog>
#include <QStringList>
#include <functional>

class QListWidget;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QTableWidget;
class QLabel;
class QChartView;
class QLineSeries;
class QScatterSeries;
class QValueAxis;
class EngineProfileDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EngineProfileDialog(std::function<QStringList()> runningEngines, QWidget *parent = nullptr);
signals:
    void catalogSaved();
protected:
    void reject() override;
    void changeEvent(QEvent *event) override;
private:
    void select(int row);
    void flush();
    void rebuild(int row);
    void create(bool duplicate);
    void remove();
    bool save();
    void backup();
    void restore(bool bundled);
    bool confirm(const QString &message);
    bool canApply();
    void message(const QString &text, bool error = false);
    void paths();
    void refreshGraph();
    QString backupDirectory();
    bool dirty() const;
    EngineProfileCatalog catalog;
    QJsonObject draft;
    std::function<QStringList()> running;
    QListWidget *profiles;
    QLineEdit *name, *horsepower;
    QComboBox *fuelType, *voltage;
    QCheckBox *hasHorsepower, *hasFuel, *hasVoltage;
    QComboBox *injected, *induction;
    QTableWidget *curve;
    QLabel *location, *status;
    QChartView *graph = nullptr;
    QLineSeries *graphLine;
    QScatterSeries *graphPoints;
    QValueAxis *rpmAxis, *gphAxis;
    QLabel *graphStatus;
    int selected = -1;
    bool loading = false;
};
#endif
