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

#include "aisclassbstatic129809.h"
#include "N2kMessages.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

QList<QList<QString>> AisClassBStatic129809::parameters() {
    return {{"MessageID", "24", "", "uint8_t"}, {"Repeat", "0", "", "tN2kAISRepeat"},
        {"UserID", "0", "MMSI", "uint32_t"}, {"Name", "SIMULATED VESSEL", "", "QString"},
        {"AISInfo", "4", "", "tN2kAISTransceiverInformation"}, {"SID", "255", "", "uint8_t"}};
}
QString AisClassBStatic129809::fieldDescription(const QString &field) {
    if (field == "Name") return "Use at most 20 uppercase AIS characters (ASCII 32-95, excluding @ padding); empty is unavailable.";
    if (field == "UserID") return "Use a nine-digit simulated MMSI. 0 is unconfigured; 4294967295 is unavailable. Neither is transmitted. Native Signal K requires a vessel MMSI beginning 2-7. This is not the CAN source address.";
    if (field == "MessageID") return "AIS static data report message ID is always 24 for this instrument.";
    if (field == "Repeat") return "Repeat indicator: 0 initial, 1 first, 2 second, 3 final.";
    if (field == "AISInfo") return "Transceiver metadata: 0/1 channel A/B reception, 2/3 A/B transmission, 4 own information not broadcast, 31 unavailable. Does not activate an AIS radio.";
    return "SID is an integer 0-255; 255 means unavailable.";
}
bool AisClassBStatic129809::validField(const QString &field, const QString &text) {
    if (field == "Name") {
        if (text.size() > 20) return false;
        for (const auto ch : text) if (ch.unicode() < 32 || ch.unicode() > 95 || ch == '@') return false;
        return true;
    }
    bool ok = false; const auto n = text.toULongLong(&ok);
    if (!ok || text.isEmpty() || text.trimmed() != text || text.startsWith('-') || text.startsWith('+')) return false;
    if (field == "MessageID") return n == 24;
    if (field == "Repeat") return n <= 3;
    if (field == "UserID") return text == QString::number(n) && (n == 0 || n == 0xffffffffULL || (n >= 100000000 && n <= 999999999));
    if (field == "AISInfo") return n <= 4 || n == 31;
    if (field == "SID") return n <= 255;
    return false;
}
bool AisClassBStatic129809::encode(const QMap<QString, QString> &fields, tN2kMsg &message) {
    if (fields.size() != parameters().size()) return false;
    for (const auto &row : parameters()) if (!fields.contains(row[0]) || !validField(row[0], fields.value(row[0]))) return false;
    // An unconfigured or unavailable identity must never produce a simulated AIS report.
    const auto identity = fields.value("UserID").toUInt();
    if (identity < 100000000 || identity > 999999999 || fields.value("Name").trimmed().isEmpty()) return false;
    const auto name = fields.value("Name").toLatin1();
    SetN2kPGN129809(message, 24, tN2kAISRepeat(fields.value("Repeat").toInt()), identity,
        name.constData(), tN2kAISTransceiverInformation(fields.value("AISInfo") == "31" ? 4 : fields.value("AISInfo").toInt()), fields.value("SID").toUInt());
    // The pinned enum names only 0-4. Preserve the five-bit unavailable value
    // without constructing an out-of-range C++ enumeration.
    if (fields.value("AISInfo") == "31") message.Data[25] = 0xff;
    return true;
}
bool AisClassBStatic129809::decode(const tN2kMsg &message, QStringList &out) {
    // The pinned decoder checks PGN but not payload length. Guard it at the application boundary.
    if (message.PGN != 129809 || message.DataLen != 27) return false;
    uint8_t id, sid; uint32_t identity; char name[21] = {};
    tN2kAISRepeat repeat; tN2kAISTransceiverInformation info;
    const int rawInfo = message.Data[25] & 0x1f;
    if (rawInfo > 4 && rawInfo != 31) return false;
    auto available = message;
    if (rawInfo == 31) available.Data[25] = 0xe4;
    if (!ParseN2kPGN129809(available, id, repeat, identity, name, sizeof(name), info, sid)) return false;
    QStringList candidate{"MessageID", QString::number(id), "Repeat", QString::number(repeat),
        "UserID", QString::number(identity), "Name", QString::fromLatin1(name),
        "AISInfo", QString::number(rawInfo), "SID", QString::number(sid)};
    for (int i = 0; i < candidate.size(); i += 2) if (!validField(candidate[i], candidate[i+1])) return false;
    out = candidate; return true;
}
AisClassBStatic129809::AisClassBStatic129809(int id, const QString &label, QWidget *parent)
    : PgnWidgetBase(id, label, parent) {
    setPgn(129809); setColumnSpan(3); setRowSpan(5);
    auto *box = new QVBoxLayout(this); auto *header = new QHBoxLayout;
    auto *enabled = new QCheckBox(this); enabled->setObjectName("checkBoxEnabled"); enabled->setChecked(isEnabled());
    enabled->setToolTip("Enable this static AIS report; Sending must also be explicitly enabled.");
    connect(enabled, &QCheckBox::clicked, this, &PgnWidgetBase::setEnabled);
    auto *title = new QLabel(label + " (129809)", this); title->setObjectName("labelTitle"); title->setWordWrap(true);
    auto font = title->font(); font.setBold(true); title->setFont(font);
    auto *remove = new QPushButton(this); remove->setObjectName("pushButtonRemove");
    remove->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton)); remove->setToolTip("Remove AIS static report");
    header->addWidget(enabled); header->addWidget(title, 1); header->addWidget(remove); box->addLayout(header);
    auto *notice = new QLabel("Static data only. Configure a simulated MMSI before Sending.\nSignal K uses MMSI vessel contexts; no AIS radio activation.", this);
    notice->setWordWrap(true); box->addWidget(notice);
    auto *association = new QLabel(this); association->setObjectName("aisAssociation"); association->setWordWrap(true); box->addWidget(association);
    auto *form = new QGridLayout; form->setColumnStretch(1,1); box->addLayout(form);
    auto *validation = new QLabel(this); validation->setObjectName("aisValidation");
    validation->setWordWrap(true); box->addWidget(validation);
    const QMap<QString, QString> labels{{"MessageID", "AIS message ID (24)"}, {"Repeat", "Repeat indicator"},
        {"UserID", "Simulated MMSI"}, {"Name", "Vessel name (20 AIS chars)"}, {"AISInfo", "Transceiver information"}, {"SID", "SID (255 unavailable)"}};
    for (const auto &row : parameters()) {
        const auto field = row[0]; values[field] = row[1];
        const int gridRow=form->rowCount();
        auto *caption=new QLabel(labels.value(field),this);caption->setObjectName("label"+field);caption->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        auto *units=new QLabel(field=="UserID"?"MMSI":"",this);units->setObjectName("labelUnit"+field);
        form->addWidget(caption,gridRow,0);form->addWidget(units,gridRow,2);
        if (field == "Repeat" || field == "AISInfo") {
            auto *combo = new QComboBox(this); combo->setObjectName(field);
            const QStringList names = field == "Repeat" ? QStringList{"Initial", "First", "Second", "Final"}
                : QStringList{"Channel A reception", "Channel B reception", "Channel A transmission", "Channel B transmission", "Own information, not broadcast"};
            for (int n=0; n<names.size(); ++n) combo->addItem(names[n], n);
            if (field == "AISInfo") combo->addItem("Unavailable", 31);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); combo->setMinimumContentsLength(12);
            form->addWidget(combo,gridRow,1);
            combo->setToolTip(fieldDescription(field));
            connect(combo, &QComboBox::activated, this, [this, combo, field] { setValue(field, QString::number(combo->currentData().toInt())); });
        } else {
            auto *edit = new QLineEdit(this); edit->setObjectName(field); editors[field] = edit;
            edit->setReadOnly(field == "MessageID"); form->addWidget(edit,gridRow,1);caption->setBuddy(edit);
            edit->setToolTip(fieldDescription(field));
            connect(edit, &QLineEdit::editingFinished, this, [this, edit, field, validation] {
                if (!edit->isModified()) return;
                const auto candidate = field == "Name" ? edit->text().toUpper() : edit->text();
                if (!validField(field, candidate)) {
                    edit->setAccessibleDescription("Invalid AIS field; previous value retained.");
                    validation->setText("Invalid " + field + "; previous value retained. " + fieldDescription(field));
                } else { validation->clear(); edit->setAccessibleDescription(QString()); setValue(field, candidate); }
                edit->setModified(false); refresh();
            });
        }
    }
    refresh();
}
void AisClassBStatic129809::setValue(const QString &field, const QString &text) {
    if (!values.contains(field) || !validField(field, text) || values[field] == text) return;
    values[field] = text; refresh(); emit parameterValueChanged(field, text);
}
void AisClassBStatic129809::refresh() {
    for (auto i = editors.begin(); i != editors.end(); ++i) {
        if (i.value()->hasFocus() && i.value()->isModified()) continue;
        const QSignalBlocker blocker(i.value()); i.value()->setText(values.value(i.key()));
    }
    for (const auto &field : {QString("Repeat"), QString("AISInfo")}) {
        auto *combo = findChild<QComboBox *>(field); const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(combo->findData(values.value(field).toInt()));
    }
}
void AisClassBStatic129809::setInitialGuiValues() { refresh(); }
void AisClassBStatic129809::initializeUnitsLimits() { refresh(); }
