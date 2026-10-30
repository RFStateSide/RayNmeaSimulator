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

#ifndef TEMPLATEVALUEDELEGATE_H
#define TEMPLATEVALUEDELEGATE_H
#include <QStyledItemDelegate>
#include <QComboBox>

// Saved values stay strings at full canonical precision. Enumerations reuse the
// Property Editor's supported choices; unsupported enums remain explicit numbers.
class TemplateValueDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    enum { TypeRole=Qt::UserRole+20 };
    static QComboBox *choices(QWidget *parent, const QString &field, const QString &type, const QString &value);
    void setEditorData(QWidget *editor,const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent,const QStyleOptionViewItem &option,const QModelIndex &index) const override;
};
#endif
