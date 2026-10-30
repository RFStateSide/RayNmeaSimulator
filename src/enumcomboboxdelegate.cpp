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

#include "expansioninstruments.h"
#include "navigationstatistics.h"
#include "windlassstatus.h"
#include "aisclassbposition129039.h"
#include "aisclassbstatic129810.h"
#include "enumcomboboxdelegate.h"
#include <QLineEdit>
#include <QComboBox>
#include <QDebug>
#include <QHeaderView>

EnumComboBoxDelegate::EnumComboBoxDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

QWidget *EnumComboBoxDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Get the data type from column 4 (index 3)
    QModelIndex dataTypeIndex = index.siblingAtColumn(3); // Get the "Data Type" column value
    QString dataType = dataTypeIndex.model()->data(dataTypeIndex, Qt::DisplayRole).toString();
    if (dataType == "QString" && NavigationStatistics::datumField(index.siblingAtColumn(0).data().toString())) {
        auto *combo = NavigationStatistics::datumEditor(parent, index.data(Qt::EditRole).toString());
        connect(combo, &QComboBox::activated, combo, [combo] { combo->setProperty("pendingUserEdit", true); });
        return combo;
    }

    const auto expansionOptions=Expansion::choices(dataType);
    if(!expansionOptions.isEmpty()) {
        auto *combo=new QComboBox(parent);
        for(const auto &choice:expansionOptions)combo->addItem(choice.second,choice.first);
        connect(combo,&QComboBox::activated,combo,[combo]{combo->setProperty("pendingUserEdit",true);});
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        return combo;
    }
    const auto windlassOptions=WindlassStatus::choices(index.siblingAtColumn(0).data().toString());
    const bool windlassEnum=dataType.startsWith("tN2k") && !windlassOptions.isEmpty();
    const bool aisVesselType=dataType=="uint8_t" && index.siblingAtColumn(0).data().toString()=="VesselType";
    // If the data type is an enum, create a QComboBox
    if (windlassEnum || dataType=="tN2kAISUnit" || dataType=="tN2kAISMode" || aisVesselType || dataType == "tN2kTempSource" || dataType == "tN2kHumiditySource" || dataType == "tN2kTimeSource" || dataType == "tN2kHeadingReference"
        || dataType == "tN2kMagneticVariation" || dataType == "tN2kSpeedWaterReferencedType" || dataType == "tN2kGNSStype" || dataType == "tN2kGNSSmethod"
        || dataType == "tN2kGNSStype" || dataType == "tN2kDistanceCalculationType" || dataType == "tN2kDelaySource" || dataType == "tN2kXTEMode"
        || dataType == "tN2kTransmissionGear" || dataType == "tN2kFluidType" || dataType == "tN2kDCType" || dataType == "tN2kChargeState"
        || dataType == "tN2kAISRepeat" || dataType == "tN2kAISTransceiverInformation"
        || dataType == "tN2kChargerMode" || dataType == "tN2kOnOff" || dataType == "tN2kRudderDirectionOrder") {
        //qDebug() << "Creating QComboBox editor for row" << index.row();

        // Create a QComboBox editor
        QComboBox *comboBox = new QComboBox(parent);

        // Populate with enum values based on the data type
        if(windlassEnum) {
            for(const auto &entry:windlassOptions)comboBox->addItem(QString::number(entry.first)+" - "+entry.second,entry.first);
        } else if(dataType=="tN2kAISUnit" || dataType=="tN2kAISMode") {
            for(const auto &entry:AisClassBPosition129039::choices(dataType=="tN2kAISUnit"?"Unit":"Mode"))comboBox->addItem(entry.second,entry.first);
        } else if (aisVesselType) {
            for(const auto &entry:AisClassBStatic129810::vesselTypes())comboBox->addItem(entry.second,entry.first);
        } else if (dataType == "tN2kAISRepeat") {
            const QStringList names{"Initial", "First", "Second", "Final"};
            for (int i=0; i<names.size(); ++i) comboBox->addItem(names[i], i);
        } else if (dataType == "tN2kAISTransceiverInformation") {
            const QStringList names{"Channel A reception", "Channel B reception", "Channel A transmission", "Channel B transmission", "Own information, not broadcast"};
            for (int i=0; i<names.size(); ++i) comboBox->addItem(names[i], i);
            comboBox->addItem("Unavailable", 31);
        } else if (dataType == "tN2kTempSource") {
            comboBox->addItem("Sea Temperature", N2kts_SeaTemperature);
            comboBox->addItem("Outside Temperature", N2kts_OutsideTemperature);
            comboBox->addItem("Inside Temperature", N2kts_InsideTemperature);
            comboBox->addItem("Engine Room Temperature", N2kts_EngineRoomTemperature);
            comboBox->addItem("Main Cabin Temperature", N2kts_MainCabinTemperature);
            comboBox->addItem("Live Well Temperature", N2kts_LiveWellTemperature);
            comboBox->addItem("Bait Well Temperature", N2kts_BaitWellTemperature);
            comboBox->addItem("Refrigeration Temperature", N2kts_RefridgerationTemperature);
            comboBox->addItem("Heating System Temperature", N2kts_HeatingSystemTemperature);
            comboBox->addItem("Dew Point Temperature", N2kts_DewPointTemperature);
            comboBox->addItem("Apparent Wind Chill Temperature", N2kts_ApparentWindChillTemperature);
            comboBox->addItem("Theoretical Wind Chill Temperature", N2kts_TheoreticalWindChillTemperature);
            comboBox->addItem("Heat Index Temperature", N2kts_HeatIndexTemperature);
            comboBox->addItem("Freezer Temperature", N2kts_FreezerTemperature);
            comboBox->addItem("Exhaust Gas Temperature", N2kts_ExhaustGasTemperature);
            comboBox->addItem("Shaft Seal Temperature", N2kts_ShaftSealTemperature);
        } else if (dataType == "tN2kHumiditySource") {
            comboBox->addItem("Inside Humidity", N2khs_InsideHumidity);
            comboBox->addItem("Outside Humidity", N2khs_OutsideHumidity);
        } else if (dataType == "tN2kTimeSource") {
            comboBox->addItem("GPS", N2ktimes_GPS);
            comboBox->addItem("GLONASS", N2ktimes_GLONASS);
            comboBox->addItem("Radio Station", N2ktimes_RadioStation);
            comboBox->addItem("Local Cesium Clock", N2ktimes_LocalCesiumClock);
            comboBox->addItem("Local Rubidium Clock", N2ktimes_LocalRubidiumClock);
            comboBox->addItem("Local Crystal Clock", N2ktimes_LocalCrystalClock);
        } else if (dataType == "tN2kHeadingReference") {
            comboBox->addItem("True", N2khr_true);
            comboBox->addItem("Magnetic", N2khr_magnetic);
            comboBox->addItem("Error", N2khr_error);
            comboBox->addItem("Unavailable", N2khr_Unavailable);
        } else if (dataType == "tN2kMagneticVariation") {
            comboBox->addItem("Manual", N2kmagvar_Manual);
            comboBox->addItem("Chart", N2kmagvar_Chart);
            comboBox->addItem("Table", N2kmagvar_Table);
            comboBox->addItem("Calculated", N2kmagvar_Calc);
            comboBox->addItem("WMM 2000", N2kmagvar_WMM2000);
            comboBox->addItem("WMM 2005", N2kmagvar_WMM2005);
            comboBox->addItem("WMM 2010", N2kmagvar_WMM2010);
            comboBox->addItem("WMM 2015", N2kmagvar_WMM2015);
            comboBox->addItem("WMM 2020", N2kmagvar_WMM2020);
        } else if (dataType == "tN2kSpeedWaterReferencedType") {
            comboBox->addItem("Paddlewheel", N2kSWRT_Paddle_wheel);
            comboBox->addItem("Pitot Tube", N2kSWRT_Pitot_tube);
            comboBox->addItem("Doppler Log", N2kSWRT_Doppler_log);
            comboBox->addItem("Ultra Sound", N2kSWRT_Ultra_Sound);
            comboBox->addItem("Electromagnetic Log", N2kSWRT_Electro_magnetic);
            comboBox->addItem("Other", N2kSWRT_Error);
            comboBox->addItem("Unavailable", N2kSWRT_Unavailable);
        } else if (dataType == "tN2kGNSStype") {
            comboBox->addItem("GPS", N2kGNSSt_GPS);
            comboBox->addItem("GLONASS", N2kGNSSt_GLONASS);
            comboBox->addItem("GPS + GLONASS", N2kGNSSt_GPSGLONASS);
            comboBox->addItem("GPS + WAAS", N2kGNSSt_GPSSBASWAAS);
            comboBox->addItem("GPS + GLONASS + WAAS", N2kGNSSt_GPSSBASWAASGLONASS);
            comboBox->addItem("Chayka", N2kGNSSt_Chayka);
            comboBox->addItem("Integrated", N2kGNSSt_integrated);
            comboBox->addItem("Surveyed", N2kGNSSt_surveyed);
            comboBox->addItem("Galileo", N2kGNSSt_Galileo);
        } else if (dataType == "tN2kGNSSmethod") {
            comboBox->addItem("No GNSS", N2kGNSSm_noGNSS);
            comboBox->addItem("GNSS Fix", N2kGNSSm_GNSSfix);
            comboBox->addItem("DGNSS", N2kGNSSm_DGNSS);
            comboBox->addItem("Precise GNSS", N2kGNSSm_PreciseGNSS);
            comboBox->addItem("RTK Fixed", N2kGNSSm_RTKFixed);
            comboBox->addItem("RTK Float", N2kGNSSm_RTKFloat);
            comboBox->addItem("Error", N2kGNSSm_Error);
            comboBox->addItem("Unavailable", N2kGNSSm_Unavailable);
        } else if (dataType == "tN2kDistanceCalculationType") {
            comboBox->addItem("Great Circle", N2kdct_GreatCircle);
            comboBox->addItem("Rhumb Line", N2kdct_RhumbLine);
        } else if (dataType == "tN2kDelaySource") {
            comboBox->addItem("User Defined", N2kDD374_UserDefined);
            comboBox->addItem("Minimum Defined", N2kDD374_MinimumDefined);
            comboBox->addItem("Factory Default", N2kDD374_FactoryDefault);
            comboBox->addItem("Maximum Supported", N2kDD374_MaximumSupported);
            comboBox->addItem("Out Of Range", N2kDD374_OutOfRange);
            comboBox->addItem("Data Not Available", N2kDD374_DataNotAvailable);
        } else if (dataType == "tN2kXTEMode") {
            comboBox->addItem("Autonomous", N2kxtem_Autonomous);
            comboBox->addItem("Differential", N2kxtem_Differential);
            comboBox->addItem("Estimated", N2kxtem_Estimated);
            comboBox->addItem("Simulator", N2kxtem_Simulator);
            comboBox->addItem("Manual", N2kxtem_Manual);
        } else if (dataType == "tN2kTransmissionGear") {
            comboBox->addItem("Forward", N2kTG_Forward);
            comboBox->addItem("Neutral", N2kTG_Neutral);
            comboBox->addItem("Reverse", N2kTG_Reverse);
            comboBox->addItem("Unknown", N2kTG_Unknown);
        } else if (dataType == "tN2kFluidType") {
            comboBox->addItem("Fuel", N2kft_Fuel);
            comboBox->addItem("Water", N2kft_Water);
            comboBox->addItem("Gray Water", N2kft_GrayWater);
            comboBox->addItem("Live Well", N2kft_LiveWell);
            comboBox->addItem("Oil", N2kft_Oil);
            comboBox->addItem("Black Water", N2kft_BlackWater);
            comboBox->addItem("Fuel Gasoline", N2kft_FuelGasoline);
            comboBox->addItem("Error", N2kft_Error);
            comboBox->addItem("Unavailable", N2kft_Unavailable);
        } else if (dataType == "tN2kDCType") {
            comboBox->addItem("Battery", N2kDCt_Battery);
            comboBox->addItem("Alternator", N2kDCt_Alternator);
            comboBox->addItem("Converter", N2kDCt_Converter);
            comboBox->addItem("Solar Cell", N2kDCt_SolarCell);
            comboBox->addItem("Wind Generator", N2kDCt_WindGenerator);
        } else if (dataType == "tN2kChargeState") {
            comboBox->addItem("Not Charging", N2kCS_Not_Charging);
            comboBox->addItem("Bulk", N2kCS_Bulk);
            comboBox->addItem("Absorption", N2kCS_Absorption);
            comboBox->addItem("Overcharge", N2kCS_Overcharge);
            comboBox->addItem("Equalise", N2kCS_Equalise);
            comboBox->addItem("Float", N2kCS_Float);
            comboBox->addItem("No Float", N2kCS_No_Float);
            comboBox->addItem("Constant Power", N2kCS_Constant_VI);
            comboBox->addItem("Disabled", N2kCS_Disabled);
            comboBox->addItem("Fault", N2kCS_Fault);
            comboBox->addItem("Error", N2kCS_Error);
            comboBox->addItem("Unavailable", N2kCS_Unavailable);
        } else if (dataType == "tN2kChargerMode") {
            comboBox->addItem("Standalone", N2kCM_Standalone);
            comboBox->addItem("Primary", N2kCM_Primary);
            comboBox->addItem("Secondary", N2kCM_Secondary);
            comboBox->addItem("Echo", N2kCM_Echo);
            comboBox->addItem("Unavailable", N2kCM_Unavailable);
        } else if (dataType == "tN2kOnOff") {
            comboBox->addItem("Off", N2kOnOff_Off);
            comboBox->addItem("On", N2kOnOff_On);
            comboBox->addItem("Error", N2kOnOff_Error);
            comboBox->addItem("Unavailable", N2kOnOff_Unavailable);
        } else if (dataType == "tN2kRudderDirectionOrder") {
            comboBox->addItem("No Direction Order", N2kRDO_NoDirectionOrder);
            comboBox->addItem("Move To Starboard", N2kRDO_MoveToStarboard);
            comboBox->addItem("Move To Port", N2kRDO_MoveToPort);
            comboBox->addItem("Unavailable", N2kRDO_Unavailable);
        }

        connect(comboBox, &QComboBox::activated, comboBox, [comboBox] {
            comboBox->setProperty("pendingUserEdit", true);
        });

        // Adjust the width of the comboBox to fit the longest text
        int maxTextWidth = 0;
        QFontMetrics fontMetrics(comboBox->font());
        for (int i = 0; i < comboBox->count(); ++i) {
            int textWidth = fontMetrics.horizontalAdvance(comboBox->itemText(i));
            if (textWidth > maxTextWidth) {
                maxTextWidth = textWidth;
            }
        }

        // Add some padding and set the width
        comboBox->setMinimumWidth(maxTextWidth + 10);

        return comboBox;
    }

    // If it's not an enum, return the default editor
    return QStyledItemDelegate::createEditor(parent, option, index);
}

void EnumComboBoxDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    // Live telemetry still updates the model, but must not replace an
    // unfinished user value in the active delegate editor. Escape discards it;
    // normal commit uses setModelData and the existing validation/ownership path.
    if (auto* text = qobject_cast<QLineEdit*>(editor); text && text->isModified()) return;
    if (editor->property("pendingUserEdit").toBool()) return;
    // Only cast to QComboBox if the editor is indeed a combo box
    if (QComboBox *comboBox = qobject_cast<QComboBox *>(editor)) {
        if (comboBox->property("datumEditor").toBool()) {
            if (!comboBox->lineEdit()->isModified()) NavigationStatistics::setDatumEditor(comboBox, index.data(Qt::EditRole).toString());
            return;
        }
        QVariant value = index.model()->data(index, Qt::EditRole);
        const auto type=index.siblingAtColumn(3).data().toString();
        if(!( !Expansion::choices(type).isEmpty() && Expansion::choices(type).first().first.metaType().id()==QMetaType::QString) && type!="RTE route type" && type!="GSA selection" && type!="char (A/V)" && type!="tNMEA0183WindReference")value=int(value.toDouble());
        //qDebug() << "Setting editor data for index" << index.row() << "value:" << value;

        // Set the current index of the combo box to match the enum value
        int comboIndex = comboBox->findData(value);
        if (comboIndex >= 0) {
            comboBox->setCurrentIndex(comboIndex);
        }
    } else {
        // Use default implementation if editor is not a QComboBox
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void EnumComboBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    // Only cast to QComboBox if the editor is indeed a combo box
    if (QComboBox *comboBox = qobject_cast<QComboBox *>(editor)) {
        if (comboBox->property("datumEditor").toBool()) {
            model->setData(index, NavigationStatistics::datumValue(comboBox), Qt::EditRole); return;
        }
        QVariant value = comboBox->currentData();
        model->setData(index, value, Qt::EditRole);
    } else {
        // Use default implementation if editor is not a QComboBox
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

void EnumComboBoxDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    editor->setGeometry(option.rect);
    Q_UNUSED(index);
}
