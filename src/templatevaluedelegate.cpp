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

#include "templatevaluedelegate.h"
#include "enumcomboboxdelegate.h"
#include <QStandardItemModel>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QRegularExpressionValidator>
#include <QLocale>
#include <limits>
#include <QPersistentModelIndex>

QComboBox *TemplateValueDelegate::choices(QWidget *parent,const QString &field,const QString &type,const QString &value)
{
    QComboBox *combo=nullptr;
    if(type=="bool") {
        combo=new QComboBox(parent);combo->addItem("False","false");combo->addItem("True","true");
    } else {
        QStandardItemModel metadata(1,4);
        metadata.setData(metadata.index(0,0),field);metadata.setData(metadata.index(0,1),value);metadata.setData(metadata.index(0,3),type);
        EnumComboBoxDelegate existing;
        auto *editor=existing.createEditor(parent,QStyleOptionViewItem(),metadata.index(0,1));
        combo=qobject_cast<QComboBox*>(editor);
        if(!combo){delete editor;return nullptr;}
    }
    int selected=-1;
    for(int i=0;i<combo->count();++i)if(combo->itemData(i).toString()==value){selected=i;break;}
    if(selected<0){combo->addItem(value+" (saved value)",value);selected=combo->count()-1;}
    combo->setCurrentIndex(selected);return combo;
}
void TemplateValueDelegate::setEditorData(QWidget *editor,const QModelIndex &index) const
{
    if(auto *text=qobject_cast<QLineEdit*>(editor);text && text->isModified())return;
    QStyledItemDelegate::setEditorData(editor,index);
}
QWidget *TemplateValueDelegate::createEditor(QWidget *parent,const QStyleOptionViewItem &option,const QModelIndex &index) const
{
    auto *editor=QStyledItemDelegate::createEditor(parent,option,index);
    auto *text=qobject_cast<QLineEdit*>(editor);if(!text)return editor;
    text->setObjectName("templateValueEditor");text->setProperty("templateField",index.siblingAtColumn(1).data().toString());
    const auto type=index.data(TypeRole).toString();
    if(type=="double" || type=="float") {
        auto *validator=new QDoubleValidator(-std::numeric_limits<double>::max(),std::numeric_limits<double>::max(),1000,text);
        validator->setNotation(QDoubleValidator::ScientificNotation);validator->setLocale(QLocale::c());text->setValidator(validator);
    } else if(type.contains("int") || type=="uchar" || type=="unsigned char" || type=="time_t" || type.startsWith("tN2k") || type=="enum") {
        text->setValidator(new QRegularExpressionValidator(QRegularExpression("[+-]?[0-9]+"),text));
    }
    const QPersistentModelIndex cell(index);
    connect(text,&QLineEdit::textEdited,text,[cell](const QString &value){
        if(cell.isValid())const_cast<QAbstractItemModel*>(cell.model())->setData(cell,value,Qt::EditRole);
    });
    text->setToolTip("Canonical value: "+type+". Numeric text is retained without display rounding; Save checks the field range.");
    return editor;
}
