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

#include "navigationstatistics.h"
#include "expansioninstruments.h"
#include "windlassstatus.h"
#include "aisclassbposition129039.h"
#include "aisclassbstatic129810.h"
#include "aisclassbstatic129809.h"
#include "pgntablemodel.h"
#include "N2kMsg.h"
#include <cmath>

PgnTableModel::PgnTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

PgnTableModel::~PgnTableModel() {}

void PgnTableModel::addPgn(int idX,
                           const QString &label,
                           const QString &pgn,
                           bool enabled,
                           const QList<QList<QString>> &paramDetails,
                           const QList<QPair<double, double>> &ranges,
                           double sendInterval)
{
    beginResetModel();

    // Check if the PGN instance already exists by idX
    for (auto &pgnData : pgnList) {
        if (pgnData.idX == idX) {
            qWarning() << "AddPgn failed: PGN instance with idX" << idX << "already exists!";
            endResetModel();
            return;
        }
    }

    // If the instance is not found, add it
    PgnData newPgn;
    newPgn.idX = idX;
    newPgn.label = label;
    newPgn.enabled = enabled;
    newPgn.sendInterval = sendInterval;
    newPgn.pgn = pgn;
    newPgn.paramDetails = paramDetails;
    newPgn.paramRanges = ranges;

    parameterUpdates.remove(idX); parameterObservations.remove(idX); parameterConfigurations.remove(idX); parameterOwners.remove(idX);
    parameterConfigurations[idX]=QDateTime::currentDateTimeUtc();
    pgnList.append(newPgn);
    //qDebug() << "PgnTableModel: Added new PGN instance:" << label << "idX:" << idX << "PGN:" << pgn;
    endResetModel();

    setCurrentPgn(idX);
}

QDateTime PgnTableModel::parameterUpdatedAt(int idX, const QString &field) const
{
    return parameterUpdates.value(idX).value(field);
}
QDateTime PgnTableModel::parameterObservedAt(int idX, const QString &field) const
{
    return parameterObservations.value(idX).value(field, parameterUpdatedAt(idX,field));
}
QDateTime PgnTableModel::parameterPublicationTime(int idX, const QString &field) const
{
    const auto observed=parameterObservedAt(idX,field);
    // A restored/manual configuration is a known local source snapshot, not a
    // qualified measurement. Freeze its load/creation time, never the send tick.
    return observed.isValid() ? observed : parameterConfigurations.value(idX);
}
void PgnTableModel::setParameterObservationTime(int idX, const QString &field, const QDateTime &time)
{
    if (findPgn(idX) && parameterUpdatedAt(idX,field).isValid() && time.isValid())
        parameterObservations[idX][field]=time.toUTC();
}
void PgnTableModel::setParameterOwner(int idX, QObject *owner)
{
    if (parameterOwners.value(idX)==owner && parameterOwners.contains(idX)) return;
    parameterUpdates.remove(idX); parameterObservations.remove(idX); parameterConfigurations.remove(idX); parameterOwners[idX]=owner;
    parameterConfigurations[idX]=QDateTime::currentDateTimeUtc();
    if(auto *position=qobject_cast<AisClassBPosition129039*>(owner))
        connect(position,&AisClassBPosition129039::navigationControlChanged,this,[this,idX] {
            if(currentPgn.idX==idX && rowCount()>0)
                emit dataChanged(index(0,1),index(rowCount()-1,1)); // No reset: protect active draft editors.
        });
    connect(owner,&QObject::destroyed,this,[this,idX,owner] {
        if (parameterOwners.value(idX)!=owner) return; // an old widget must not clear a reused id
        parameterOwners[idX]=nullptr; parameterUpdates.remove(idX); parameterObservations.remove(idX); parameterConfigurations.remove(idX); parameterDescriptions.remove(idX);
        emit valuesChanged(idX);
    });
}
bool PgnTableModel::stampParameter(int idX, const QString &field)
{
    for (const auto &range:parameterInitializationRanges) if (idX>=range.first && idX<=range.second) return false;
    if (parameterOwners.contains(idX) && !parameterOwners.value(idX)) return false;
    parameterObservations[idX].remove(field);
    parameterUpdates[idX][field]=QDateTime::currentDateTimeUtc(); return true;
}
void PgnTableModel::recordParameterSample(int idX, const QString &field)
{
    const auto *pgn=findPgn(idX); if (!pgn) return;
    for (const auto &row : pgn->paramDetails) if (row[0]==field) {
        if (stampParameter(idX,field)) emit valuesChanged(idX);
        return;
    }
}
void PgnTableModel::setParameterDescription(int idX, const QString &field, const QString &text)
{
    parameterDescriptions[idX][field] = text;
    // Tooltips are read on demand. Do not refresh active value editors for metadata-only changes.
}

void PgnTableModel::setCurrentPgn(int idX)
{
    if (currentPgn.idX == idX) return;
    beginResetModel();
    currentPgn = PgnData{};
    for (const auto &pgnData : pgnList) {
        if (pgnData.idX == idX) {
            currentPgn = pgnData;
            break;
        }
    }
    endResetModel();
}

void PgnTableModel::changeParam(int idX, const QString &paramName, const QString &paramNewValue)
{
    for (auto &pgnData : pgnList) {
        if (pgnData.idX != idX) continue;
        for (int row = 0; row < pgnData.paramDetails.size(); ++row) {
            auto &param = pgnData.paramDetails[row];
            if (param[0] != paramName) continue;
            const bool sampled=stampParameter(idX,paramName);
            if (param[1] == paramNewValue) { if (sampled) emit valuesChanged(idX); return; }
            param[1] = paramNewValue;
            emit valuesChanged(idX);
            if (currentPgn.idX == idX) {
                currentPgn.paramDetails[row][1] = paramNewValue;
                emit dataChanged(index(row, 1), index(row, 1), {Qt::DisplayRole, Qt::EditRole});
            }
            return;
        }
    }
}

void PgnTableModel::changeParams(int idX, const QStringList &paramList)
{
    // Selection controls the Property Editor, not instrument display delivery.
    for (int i = 0; i + 1 < paramList.size(); i += 2) {
        const auto params = getPgnParams(idX);
        const auto found = std::find_if(params.cbegin(), params.cend(), [&](const auto &param) {
            return param[0] == paramList[i];
        });
        if (found == params.cend()) continue;
        const bool changed=(*found)[1] != paramList[i+1];
        changeParam(idX, paramList[i], paramList[i + 1]);
        if (changed) emit parameterChanged(idX, paramList[i], paramList[i + 1]);
    }
}

void PgnTableModel::removePgn(int id)
{
    parameterDescriptions.remove(id); parameterUpdates.remove(id); parameterObservations.remove(id); parameterConfigurations.remove(id); parameterOwners.remove(id);
    auto it = std::find_if(pgnList.begin(), pgnList.end(),
                           [id](const PgnData &data) { return data.idX == id; });
    if (it == pgnList.end()) return;
    // Table rows describe the selected instrument's parameters, not pgnList.
    const bool selected = currentPgn.idX == id;
    if (selected) beginResetModel();
    pgnList.erase(it);
    if (selected) {
        currentPgn = PgnData{};
        m_IdxHasFocus = -1;
        endResetModel();
    }
}

void PgnTableModel::setSource(int idX, QString source)
{
    for (auto &pgnData : pgnList) {
        if (pgnData.idX == idX) {
            pgnData.source = source;
            emit valuesChanged(idX);
            break;
        }
    }
}

QString PgnTableModel::getSource(int idX)
{
    return std::find_if(pgnList.begin(), pgnList.end(), [&](const PgnData &pgnData) { return pgnData.idX == idX; })->source;
}

QList<QList<QString>> PgnTableModel::getPgnParams(int idX) const
{
    for (const auto &pgnData : pgnList) {
        if (pgnData.idX == idX) {
            return pgnData.paramDetails;
        }
    }
    return {};
}

QList<PgnData> &PgnTableModel::getPgnList()
{
    return pgnList;
}

int PgnTableModel::getPgnInstanceCount(QString pgn) const
{
    return std::count_if(pgnList.constBegin(), pgnList.constEnd(), [&](const PgnData &pgnData) { return pgnData.pgn == pgn; });
}

PgnData PgnTableModel::getPgnDataToSend(int idX) const
{
    PgnData pgnDataToSend;

    for (const auto &pgnData : pgnList) {
        if (pgnData.idX == idX) {
            pgnDataToSend = pgnData;
            return pgnDataToSend;
        }
    }

    pgnDataToSend.idX = -1; // Return invalid idX
    return pgnDataToSend;
}

const PgnData* PgnTableModel::findPgn(int idX) const
{
    for (const auto& data : pgnList) if (data.idX == idX) return &data;
    return nullptr;
}

void PgnTableModel::setSendInterval(int idX, double sendInterval)
{
    for (auto &pgnData : pgnList) {
        if (pgnData.idX == idX) {
            pgnData.sendInterval = sendInterval;
            emit valuesChanged(idX);
            break;
        }
    }
}

void PgnTableModel::setPgnEnabled(int idX, bool enabled)
{
    for (auto &pgnData : pgnList) {
        if (pgnData.idX == idX) {
            if (pgnData.enabled == enabled) return;
            pgnData.enabled = enabled;
            emit valuesChanged(idX);
            emit enabledChanged();
            return;
        }
    }
}

bool PgnTableModel::isPgnEnabled(int idX) const
{
    for (const auto &pgnData : pgnList) {
        if (pgnData.idX == idX) {
            return pgnData.enabled;
        }
    }

    return false;
}

int PgnTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return currentPgn.paramDetails.size(); // Return the number of parameters in the current PGN
}

int PgnTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return 4; // We have 4 columns: Parameter, Value, Units, Type
}

QVariant PgnTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= currentPgn.paramDetails.size() || index.column() >= 4) {
        return QVariant();
    }

    const auto &paramData = currentPgn.paramDetails[index.row()];
    if (role == Qt::ToolTipRole) {
        const auto description=parameterDescriptions.value(currentPgn.idX).value(paramData[0]);
        const QString contents = paramData.value(index.column());
        if (index.column()==1 || index.column()==2)
            return contents + "\n" + tr("Stored value uses the units shown here. Instrument display preferences do not change these units.")
                + (description.isEmpty()?QString():"\n"+description);
        return contents + (description.isEmpty()?QString():"\n"+description);
    }
    //qDebug() << "PgnTableModel::data Accessing data for row:" << index.row() << "column:" << index.column() << "role:" << role;

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        //qDebug() << "PgnTableModel::data Accessing data for row" << index.row() << "column" << index.column();
        switch (index.column()) {
        case 0:
            return paramData[0]; // Parameter Name
        case 1:
            if (role == Qt::DisplayRole && QStringList{"128777", "128778", "-130311", "130306", "130313", "129810", "129039"}.contains(currentPgn.pgn) && paramData[1].toDouble() == N2kDoubleNA) return "Unavailable";
            // Presentation is independent from the canonical text supplied to an editor.
            if (role == Qt::DisplayRole && currentPgn.pgn!="129039" && (paramData[0] == "Latitude" || paramData[0] == "Longitude")) {
                return QString::number(paramData[1].toDouble(), 'f', 6);
            } else {
                return paramData[1];
            }
        case 2:
            return paramData[2]; // Units
        case 3:
            return paramData[3]; // Type
        }
    }

    return QVariant();
}

QVariant PgnTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return QString("Parameter");
        case 1:
            return QString("Value");
        case 2:
            return QString("Units");
        case 3:
            return QString("Type");
        }
    }
    return QVariant();
}

bool PgnTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.model() != this || role != Qt::EditRole
        || index.column() != 1 || index.row() < 0 || index.row() >= rowCount()) return false;
    const int id = currentPgn.idX;
    const QString name = currentPgn.paramDetails[index.row()][0];
    if(Expansion::definition(currentPgn.pgn)) {
        QString text=value.toString();
        for(const auto &f:Expansion::definition(currentPgn.pgn)->fields)
            if(f.name==name && f.type=="double" && text.compare("Unavailable",Qt::CaseInsensitive)==0)text=QString::number(N2kDoubleNA,'g',17);
        if (NavigationStatistics::supported(currentPgn.pgn))
            for (const auto &f : Expansion::definition(currentPgn.pgn)->fields)
                if (f.name == name) text = NavigationStatistics::editValue(f, text);
        if(!Expansion::validField(currentPgn.pgn,name,text))return false;
        if(text==currentPgn.paramDetails[index.row()][1])return true;
        changeParam(id,name,text);emit parameterChanged(id,name,text);return true;
    }
    if(WindlassStatus::supported(currentPgn.pgn.toInt())) {
        QString text=value.toString();
        if(WindlassStatus::measurement(name) && text.compare("Unavailable",Qt::CaseInsensitive)==0)text=QString::number(N2kDoubleNA,'g',17);
        if(!WindlassStatus::validField(currentPgn.pgn.toInt(),name,text)) {
            if(auto *instrument=qobject_cast<WindlassStatus*>(parameterOwners.value(id)))instrument->showInvalidEdit(name);
            qWarning().noquote()<<"Invalid windlass"<<name<<"edit; previous value retained."<<WindlassStatus::description(name);
            return false;
        }
        if(text!=value.toString())return setData(index,text,role);
    }
    if(currentPgn.pgn=="129039" && !(flags(index)&Qt::ItemIsEditable))return false;
    if(currentPgn.pgn=="129039" && AisClassBPosition129039::measurement(name) && value.toString().compare("Unavailable",Qt::CaseInsensitive)==0)return setData(index,QString::number(N2kDoubleNA,'g',17),role);
    if(currentPgn.pgn=="129039" && !AisClassBPosition129039::validField(name,value.toString()))return false;
    if (currentPgn.pgn=="129810" && AisClassBStatic129810::dimension(name) && value.toString().compare("Unavailable",Qt::CaseInsensitive)==0)
        return setData(index,QString::number(N2kDoubleNA,'g',17),role);
    if (currentPgn.pgn=="129810" && !AisClassBStatic129810::editableField(name,value.toString())) return false;
    if (currentPgn.pgn == "129809" && !AisClassBStatic129809::validField(name, value.toString())) return false;
    if (QStringList{"128267", "127506", "127507", "127508"}.contains(currentPgn.pgn)) {
        // These protocol parameters are numeric; invalid text must not become
        // zero in the widget while remaining invalid in the authoritative model.
        bool valid = false;
        const double number = value.toString().toDouble(&valid);
        if (!valid || !std::isfinite(number)) return false;
        const QString type = currentPgn.paramDetails[index.row()][3];
        if ((type == "unsigned char" || type == "uint8_t")
            && (number < 0 || number > 255 || std::floor(number) != number)) return false;
        if ((name == "StateOfCharge" || name == "StateOfHealth")
            && (number < 0 || (number > 100 && number != 255) || std::floor(number) != number)) return false;
        if (type.startsWith("tN2k")
            && (number < 0 || std::floor(number) != number
                || number > (type == "tN2kOnOff" ? 3 : type == "tN2kDCType" ? 255 : 15))) return false;
        if (name != "Offset" && name != "BatteryCurrent" && number < 0 && number != N2kDoubleNA) return false;
    }
    if (currentPgn.pgn == "130306" || currentPgn.pgn == "130313") {
        bool ok = false; const double v = value.toString().toDouble(&ok);
        if (!ok || !std::isfinite(v)) return false;
        if ((name == "SID" || name == "HumidityInstance") && (v < 0 || v > 255 || std::floor(v) != v)) return false;
        if (name == "WindReference" && !(v == 0 || v == 1 || v == 2 || v == 3 || v == 4 || v == 6 || v == 7)) return false;
        if (name == "HumiditySource" && !(v == 0 || v == 1 || v == 255)) return false;
        if (v != N2kDoubleNA && ((name == "WindAngle" && (v < 0 || v >= 2*M_PI))
            || (name == "WindSpeed" && (v < 0 || v > 655.32))
            || ((name == "ActualHumidity" || name == "SetHumidity") && (v < 0 || v > 100)))) return false;
    }
    if (currentPgn.pgn == "GLL" && name == "Status" && value.toString() != "A" && value.toString() != "V") return false;
    const QString stored = value.toString();
    const bool coordinate = currentPgn.pgn != "129039"
        && (name == "Latitude" || name == "Longitude" || name == "DestinationLatitude" || name == "DestinationLongitude");
    if (coordinate) {
        bool valid = false;
        const double number = stored.toDouble(&valid);
        const double limit = name.endsWith("Latitude") ? 90.0 : 180.0;
        if (!valid || !std::isfinite(number) || (number != N2kDoubleNA && qAbs(number) > limit)) return false;
        // Preserve entered text; merely accepting an unchanged editor is not an edit.
        if (stored == currentPgn.paramDetails[index.row()][1]) return true;
    }
    // Commit to the authoritative list before notifying widget bindings.
    changeParam(id, name, stored);
    emit parameterChanged(id, name, stored);
    return true;
}

Qt::ItemFlags PgnTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    if (index.column() == 1) {
        if(auto *position=qobject_cast<AisClassBPosition129039*>(parameterOwners.value(currentPgn.idX));
           position && index.row()>=0 && index.row()<currentPgn.paramDetails.size()
           && ((position->navigationControlled() && AisClassBPosition129039::measurement(currentPgn.paramDetails[index.row()][0]))
               || (position->observationTimeControlled() && currentPgn.paramDetails[index.row()][0]=="Seconds")))
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        //return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (m_mode == Sender) {
            return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        }
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

double PgnTableModel::getParamMin(int row) const
{
    if (row >= 0 && row < currentPgn.paramRanges.size()) {
        return currentPgn.paramRanges[row].first; // Min value
    }
    return 0; // Default min value
}

double PgnTableModel::getParamMax(int row) const
{
    if (row >= 0 && row < currentPgn.paramRanges.size()) {
        return currentPgn.paramRanges[row].second; // Max value
    }
    return 255; // Default max value
}

void PgnTableModel::updatePgnParameter(int idX, const QString &paramName, const QVariant &value, bool emitSignal)
{
    changeParam(idX, paramName, value.toString());
    if (emitSignal) emit parameterChanged(idX, paramName, value);
}

bool PgnTableModel::isPgnCurrentPgn(int idX) const
{
    return currentPgn.idX == idX;
}

int PgnTableModel::getMaxIdX() const
{
    int maxIdX = -1; // Initialize to -1, assuming idX values are non-negative

    for (const PgnData &data : pgnList) {
        if (data.idX > maxIdX) {
            maxIdX = data.idX;
        }
    }

    return maxIdX;
}

void PgnTableModel::clearModel()
{
    parameterDescriptions.clear(); parameterUpdates.clear(); parameterObservations.clear(); parameterConfigurations.clear(); parameterOwners.clear();
    beginResetModel();
    pgnList.clear();
    currentPgn = PgnData{};
    m_IdxHasFocus = -1;
    endResetModel();
}

void PgnTableModel::refreashModel()
{
    beginResetModel();
    endResetModel();
}

QString PgnTableModel::getLabel(int idX) const
{
    return std::find_if(pgnList.begin(), pgnList.end(), [&](const PgnData &pgnData) { return pgnData.idX == idX; })->label;
}

bool PgnTableModel::isPgnInList(const QString &pgn) const
{
    return std::any_of(pgnList.begin(), pgnList.end(), [&](const PgnData &pgnData) { return pgnData.pgn == pgn; });
}

QList<PgnData> PgnTableModel::getPgns(const QString &pgn) const
{
    QList<PgnData> pgns;

    for (const auto &pgnData : pgnList) {
        if (pgnData.pgn == pgn) {
            pgns.append(pgnData);
        }
    }

    return pgns;
}

void PgnTableModel::setIdxHasFocus(int idx)
{
    m_IdxHasFocus = idx;
    setCurrentPgn(idx);
}

int PgnTableModel::IdxHasFocus() const
{
    return m_IdxHasFocus;
}

void PgnTableModel::SetMode(Mode mode)
{
    m_mode = mode;
}
