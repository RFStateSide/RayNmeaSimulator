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

#include "applicationlog.h"
#include <QLineEdit>
#include <QKeyEvent>
#include <QMetaMethod>
#include <QToolTip>
#include <memory>
#include <cmath>
#include <vector>
#include <QSignalBlocker>
#include <QValidator>
#include <QScopeGuard>
#include "pgnwidgetbase.h"
#include <QFocusEvent>
#include <QLabel>
#include <QLayout>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QTabWidget>
#include <QTabBar>
#include <QPushButton>
#include <QCheckBox>
#include <QBoxLayout>
#include <QStyleOptionButton>

PgnWidgetBase::PgnWidgetBase(int id, const QString &label, QWidget *parent)
    : QWidget(parent)
    , m_Id(id)
    , m_Label(label)
{
    this->setFocusPolicy(Qt::StrongFocus);

    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, &PgnWidgetBase::onTimerTimeout);

    // Initilize default user preferences and vessle data
    m_UserPrefs = UserUnitPrefs();
    QTimer::singleShot(0, this, &PgnWidgetBase::updateControlGuidance);
}

PgnWidgetBase::~PgnWidgetBase()
{
    timer->stop();
    disconnect(timer, &QTimer::timeout, this, &PgnWidgetBase::onTimerTimeout);
}

void PgnWidgetBase::setDispatch(bool value)
{
    if (m_dispatch == value) return;
    m_dispatch = value; ++m_dispatchEpoch;
    ApplicationLog::event("raynmea.generator",QString("Widget %1 Dispatch=%2 (connection remains established)").arg(Id()).arg(value));
    emit dispatchChanged();
}

void PgnWidgetBase::initializeGeneratorControls()
{
    if (!isGenerator() || property("GeneratorControlsPrepared").toBool()) return;
    setProperty("GeneratorControlsPrepared",true);
    if(auto *enable=findChild<QCheckBox*>("checkBoxEnabled"))enable->setToolTip(tr("Enable local generator updates. Dispatch controls delivery; Enable Sending controls external NMEA transmission."));
    auto *row=new QHBoxLayout;
    if (!findChild<QCheckBox*>("checkBoxEnabled")) {
        auto *enable=new QCheckBox(tr("Enable"),this); enable->setObjectName("checkBoxEnabled");
        enable->setChecked(isEnabled()); row->addWidget(enable);
        connect(enable,&QCheckBox::toggled,this,&PgnWidgetBase::setEnabled);
        connect(this,&PgnWidgetBase::enabledChanged,enable,[this,enable]{QSignalBlocker b(enable);enable->setChecked(isEnabled());});
    }
    if(auto *enable=findChild<QCheckBox*>("checkBoxEnabled"))enable->setToolTip(tr("Run local generator updates. Dispatch controls delivery to instruments; external output requires Enable Sending."));
    auto *dispatchBox=new QCheckBox(tr("Dispatch"),this); dispatchBox->setObjectName("checkBoxDispatch");
    dispatchBox->setToolTip(tr("Deliver generated values to dependent widgets. External transmission still requires Enable Sending."));
    dispatchBox->setChecked(dispatch()); row->addWidget(dispatchBox); row->addStretch();
    connect(dispatchBox,&QCheckBox::toggled,this,&PgnWidgetBase::setDispatch);
    connect(this,&PgnWidgetBase::dispatchChanged,dispatchBox,[this,dispatchBox]{QSignalBlocker b(dispatchBox);dispatchBox->setChecked(dispatch());});
    if(auto *box=qobject_cast<QBoxLayout*>(layout())) box->insertLayout(1,row); else delete row;
    // All supported generator timers are owned by their source widget. Holding
    // timer events preserves intervals/state and never affects dependent widgets.
    for(auto *clock:findChildren<QTimer*>()) clock->installEventFilter(this);
}

bool PgnWidgetBase::eventFilter(QObject *watched,QEvent *event)
{
    if (isGenerator() && event->type()==QEvent::Timer && !isEnabled()) return true;
    if (auto *editor = qobject_cast<QLineEdit*>(watched);
        editor && editor->property("_rayInstrumentCommitSlot").isValid() && !editor->isReadOnly()) {
        if (event->type() == QEvent::FocusOut) finishInstrumentEdit(editor);
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                finishInstrumentEdit(editor);
                return true; // One commit; do not run validator fixup or a parent default action.
            }
        }
    }
    return QWidget::eventFilter(watched,event);
}

bool PgnWidgetBase::event(QEvent *event)
{
    const bool result = QWidget::event(event);
    if (event->type()==QEvent::Polish) { updateControlGuidance(); initializeInstrumentEdits(); }
    if ((event->type() == QEvent::Polish || event->type() == QEvent::FontChange
         || event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange
         || event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize)
        && !textSizingPending) {
        textSizingPending = true;
        // Designer setup and inherited fonts/styles must be resolved first.
        // Coalesce layout notifications; no polling, value changes or focus changes.
        QTimer::singleShot(0, this, [this] { updateTextSizing(); textSizingPending = false; });
    }
    return result;
}

// Legacy Designer instruments use auto-connected textEdited(QString) slots.
// Adapt only those validated, editable fields to a transaction; newer code-built
// editors, spin boxes, editable combos and generators retain their own lifecycle.
void PgnWidgetBase::initializeInstrumentEdits()
{
    if (isGenerator()) return;
    for (auto *editor : findChildren<QLineEdit*>()) {
        if (editor->isReadOnly() || !editor->validator()
            || qobject_cast<QAbstractSpinBox*>(editor->parentWidget())
            || qobject_cast<QComboBox*>(editor->parentWidget())
            || editor->property("_rayInstrumentCommitSlot").isValid()) continue;
        const QByteArray name = "on_" + editor->objectName().toUtf8() + "_textEdited";
        const int index = metaObject()->indexOfSlot((name + "(QString)").constData());
        if (index < 0) continue;
        // Disconnect only the established per-field slot, not other observers.
        const auto slot = metaObject()->method(index);
        QObject::disconnect(editor, QMetaMethod::fromSignal(&QLineEdit::textEdited), this, slot);
        editor->setProperty("_rayInstrumentCommitSlot", name);
        editor->installEventFilter(this);
        connect(editor, &QLineEdit::editingFinished, this, [this, editor] { finishInstrumentEdit(editor); });
    }
}

void PgnWidgetBase::renderInstrumentEdits(QLineEdit *editor)
{
    struct Draft { QLineEdit *edit; QString text; };
    QList<Draft> others;
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (auto *field : findChildren<QLineEdit*>()) {
        if (!field->property("_rayInstrumentCommitSlot").isValid()) continue;
        if (field != editor && field->isModified()) others.append({field, field->text()});
        blockers.push_back(std::make_unique<QSignalBlocker>(field));
    }
    // Render from current authoritative widget properties, never cached text.
    // Suppress presentation writeback through the established model guard.
    emit presentationRefreshStarted();
    const auto finish = qScopeGuard([this] { emit presentationRefreshFinished(); });
    setInitialGuiValues();
    for (const auto &draft : others) { draft.edit->setText(draft.text); draft.edit->setModified(true); }
    editor->setModified(false);
}

void PgnWidgetBase::finishInstrumentEdit(QLineEdit *editor)
{
    if (!editor->isModified() || editor->property("_rayInstrumentCommitBusy").toBool()) return;
    editor->setProperty("_rayInstrumentCommitBusy", true);
    const auto finish = qScopeGuard([editor] { editor->setProperty("_rayInstrumentCommitBusy", false); });
    const QString draft = editor->text();
    bool acceptable = acceptsEdit(editor, draft);
    if (qobject_cast<const QDoubleValidator*>(editor->validator())
        || qobject_cast<const QIntValidator*>(editor->validator())) {
        // Legacy numeric slots use QString::toDouble, not locale/group parsing.
        // Do not validate a grouped number and then silently convert it to zero.
        bool parsed = false;
        const double value = draft.toDouble(&parsed);
        acceptable = acceptable && parsed && std::isfinite(value);
    }
    renderInstrumentEdits(editor);
    // Re-entering an unchanged rounded readout must not round the canonical value.
    if (draft == editor->text()) return;
    if (acceptable) {
        const auto slot = editor->property("_rayInstrumentCommitSlot").toByteArray();
        QMetaObject::invokeMethod(this, slot.constData(), Qt::DirectConnection, Q_ARG(QString, draft));
        renderInstrumentEdits(editor);
        editor->setProperty("_rayInputFeedback", QString());
    } else {
        const auto message = tr("Invalid or out-of-range input. Current value restored.");
        editor->setProperty("_rayInputFeedback", message);
        if (editor->isVisible()) QToolTip::showText(editor->mapToGlobal(QPoint(0, editor->height())), message, editor);
    }
}

void PgnWidgetBase::updateControlGuidance()
{
    if (!isGenerator()) {
        if(auto *enable=findChild<QCheckBox*>("checkBoxEnabled"))
            if(enable->toolTip().isEmpty() || enable->toolTip()=="Enable "+Label())
                enable->setToolTip(tr("Include this instrument in external Sending. Generate mode and explicit Enable Sending are required; received updates remain available."));
    }
}
void PgnWidgetBase::updateTextSizing()
{
    if (!layout()) return;
    // Legacy Designer root caps otherwise leave the expanding frame wider/taller
    // than its instrument, so even a trailing header button misses the frame corner.
    if (maximumWidth()!=QWIDGETSIZE_MAX || maximumHeight()!=QWIDGETSIZE_MAX)
        setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
    // One surrounding presentation for Designer and code-built instruments.
    // Internal editors, units and specialized generator controls remain intact.
    if(sizePolicy().horizontalPolicy()!=QSizePolicy::Expanding || sizePolicy().verticalPolicy()!=QSizePolicy::Preferred)
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    const QMargins margins(style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, this),
        style()->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, this),
        style()->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, this),
        style()->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, this));
    if(layout()->contentsMargins()!=margins)layout()->setContentsMargins(margins);
    if(!property("_rayCommonLayout").toBool()) {
        setProperty("_rayCommonLayout",true);
        layout()->setSpacing(-1);
    }
    if (auto *header = qobject_cast<QBoxLayout*>(layout()->itemAt(0) ? layout()->itemAt(0)->layout() : nullptr)) {
        if (auto *title = findChild<QLabel*>("labelTitle")) {
            if (header->indexOf(title)>=0) {
                // A shorter instrument can share a tall grid row with a neighbour.
                // Align each control, retaining the layout's height-for-width
                // negotiation so wrapped titles get all their required lines.
                // Do not align/cap the whole layout to its single-line size hint.
                if(header->alignment()!=Qt::Alignment{})header->setAlignment(Qt::Alignment{});
                if(header->contentsMargins()!=QMargins())header->setContentsMargins(0,0,0,0);
                if(!header->property("_rayCommonHeaderSpacing").toBool()) {
                    header->setProperty("_rayCommonHeaderSpacing",true);
                    header->setSpacing(-1);
                }
                // Old headers center titles between two expanding spacers;
                // AIS headers give that space to the title instead.
                const bool titleRow=header->direction()==QBoxLayout::LeftToRight
                    || header->direction()==QBoxLayout::RightToLeft;
                if(titleRow)for (int i=header->count()-1;i>=0;--i)
                    if (header->itemAt(i)->spacerItem()) delete header->takeAt(i);
                // The legacy Date/Time generator puts its title and fields in
                // one vertical layout: title stretch there would consume height.
                const int titleStretch=titleRow?1:0;
                if(header->stretch(header->indexOf(title))!=titleStretch)
                    header->setStretch(header->indexOf(title),titleStretch);
                if(title->alignment()!=(Qt::AlignLeft | Qt::AlignTop))title->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                if(header->itemAt(header->indexOf(title))->alignment()!=Qt::AlignTop)header->setAlignment(title,Qt::AlignTop);
                if(!title->wordWrap())title->setWordWrap(true);
                if(auto *enabled=findChild<QCheckBox*>("checkBoxEnabled"); enabled && header->indexOf(enabled)>=0)
                    if(header->itemAt(header->indexOf(enabled))->alignment()!=(Qt::AlignLeft | Qt::AlignTop))
                        header->setAlignment(enabled,Qt::AlignLeft | Qt::AlignTop);
                for (auto *button : findChildren<QPushButton*>("pushButtonRemove")) {
                    const int index=header->indexOf(button);
                    if (index>=0 && header->itemAt(index)->alignment()!=(Qt::AlignRight | Qt::AlignTop))
                        header->setAlignment(button,Qt::AlignRight | Qt::AlignTop);
                }
            }
        }
    }
    // Form-only instruments have no vertically expanding body. When their
    // grid row is taller than the content, QBoxLayout otherwise distributes
    // surplus space among the header, notices and fields. Keep natural content
    // spacing and let a trailing stretch absorb only that surplus. Preserve
    // existing stretches and expanding bodies (tabs, plots, scroll areas).
    if(auto *box=qobject_cast<QBoxLayout*>(layout());box && box->direction()==QBoxLayout::TopToBottom) {
        bool expands=false;
        for(int i=1;i<box->count();++i)
            expands|=box->stretch(i)>0 || box->itemAt(i)->expandingDirections().testFlag(Qt::Vertical);
        if(!expands)box->addStretch(1);
    }
    for (auto *button : findChildren<QPushButton*>("pushButtonRemove")) {
        const auto fontKey=font().key();
        if(button->property("_rayCloseFont").toString()!=fontKey) {
            button->setProperty("_rayCloseFont",fontKey);
            if(button->font()!=font())button->setFont(font());
        }
        if (!button->property("_rayCommonCloseButton").toBool()) {
            button->setProperty("_rayCommonCloseButton",true);
            // Override only padding/minimums, retaining theme hover/focus/borders.
            button->setStyleSheet("QPushButton#pushButtonRemove { padding: 0; min-width: 0; min-height: 0; }");
            button->setAccessibleName(tr("Remove %1").arg(Label()));
            button->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        }
        const auto paletteKey=button->palette().cacheKey();
        const auto styleKey=qulonglong(reinterpret_cast<quintptr>(button->style()));
        if(button->property("_rayClosePalette").toULongLong()!=paletteKey
            || button->property("_rayCloseStyle").toULongLong()!=styleKey) {
            button->setProperty("_rayClosePalette",paletteKey);
            button->setProperty("_rayCloseStyle",styleKey);
            button->setIcon(button->style()->standardIcon(QStyle::SP_DialogCloseButton,nullptr,button));
        }
        const int extent=qMax(style()->pixelMetric(QStyle::PM_SmallIconSize,nullptr,button),button->fontMetrics().height());
        if(button->iconSize()!=QSize(extent,extent))button->setIconSize(QSize(extent,extent));
        QStyleOptionButton option;option.initFrom(button);option.icon=button->icon();option.iconSize=button->iconSize();
        const auto hint=button->style()->sizeFromContents(QStyle::CT_PushButton,&option,QSize(extent,extent),button);
        const int side=qMax(qMax(hint.width(),hint.height()),qMax(
            style()->pixelMetric(QStyle::PM_ToolBarIconSize,nullptr,this),
            extent+style()->pixelMetric(QStyle::PM_ButtonMargin,nullptr,button)));
        if(button->size()!=QSize(side,side) || button->minimumSize()!=QSize(side,side))button->setFixedSize(side,side);
    }
    // Explicit ColumnSpan pixel minima must not override the form's real minimum.
    // The owning grid/scroll area still controls placement and saved column counts.
    // Spacer owns an intentional Rows-based minimum, unlike instrument forms.
    const auto constraint=property("Rows").isValid() ? QLayout::SetDefaultConstraint : QLayout::SetMinimumSize;
    if (layout()->sizeConstraint()!=constraint) layout()->setSizeConstraint(constraint);
    for (auto *child : findChildren<QWidget *>()) {
        // Qt stylesheets do not reliably propagate a changed application font
        // through an already-polished form. Use the configured font, retaining
        // the title emphasis without inheriting a previous preference's weight.
        QFont controlFont=font();
        if(child->objectName()=="labelTitle")controlFont.setBold(true);
        const auto fontKey=controlFont.key();
        if(child->property("_rayInstrumentFont").toString()!=fontKey) {
            child->setProperty("_rayInstrumentFont",fontKey);
            if(child->font()!=controlFont)child->setFont(controlFont);
        }
        if(child->objectName()=="pushButtonRemove")continue;
        auto *label = qobject_cast<QLabel *>(child);
        auto *edit = qobject_cast<QLineEdit *>(child);
        auto *spin = qobject_cast<QAbstractSpinBox *>(child);
        auto *combo = qobject_cast<QComboBox *>(child);
        auto *button=qobject_cast<QAbstractButton*>(child);
        if (edit && qobject_cast<QAbstractSpinBox *>(edit->parentWidget())) continue;
        if (!label && !edit && !spin && !combo && !button) continue;
        if (label && (!label->pixmap().isNull() || label->text().isEmpty())) continue;
        // Preserve the established minimum as a floor, but let Qt calculate
        // text size instead of capping labels/fields at Designer's 75/100 pixels.
        if (child->maximumWidth() < QWIDGETSIZE_MAX) child->setMaximumWidth(QWIDGETSIZE_MAX);
        if (child->maximumHeight() < child->minimumSizeHint().height())
            child->setMaximumHeight(QWIDGETSIZE_MAX);
        if(button) {
            constexpr auto originalMinimum="_rayOriginalButtonMinimum";
            if(!button->property(originalMinimum).isValid())button->setProperty(originalMinimum,button->minimumSize());
            const auto minimum=button->property(originalMinimum).toSize().expandedTo(button->minimumSizeHint());
            if(button->minimumSize()!=minimum)button->setMinimumSize(minimum);
        }
        if (edit) {
            constexpr auto originalMinimum = "_rayOriginalEditorMinimum";
            if (!edit->property(originalMinimum).isValid()) edit->setProperty(originalMinimum,edit->minimumWidth());
            QString sample = "Unavailable";
            if (edit->objectName().contains("Latitude") || edit->objectName().contains("Longitude"))
                sample = "180° 59′ 59.99″ W";
            // Use the editor's own font and native frame/text margins, not DPI pixels.
            const int frame = edit->minimumSizeHint().width() - edit->fontMetrics().maxWidth();
            const int width = qMax(edit->property(originalMinimum).toInt(),edit->fontMetrics().horizontalAdvance(sample)+frame);
            if (edit->minimumWidth()!=width) edit->setMinimumWidth(width);
        }
        if (label && !label->wordWrap() && !label->text().contains('<')) {
            constexpr auto originalMinimum = "_rayOriginalLabelMinimum";
            if (!label->property(originalMinimum).isValid()) label->setProperty(originalMinimum,label->minimumWidth());
            const auto margins=label->contentsMargins();
            const int textWidth=label->fontMetrics().horizontalAdvance(label->text())
                + margins.left()+margins.right()+2*label->margin()+qMax(0,label->indent());
            const int width=qMax(label->property(originalMinimum).toInt(),qMax(label->sizeHint().width(),textWidth));
            if(label->minimumWidth()!=width)label->setMinimumWidth(width);
            constexpr auto originalHeight = "_rayOriginalLabelHeight";
            if (!label->property(originalHeight).isValid()) label->setProperty(originalHeight,label->minimumHeight());
            const int height=qMax(label->property(originalHeight).toInt(),label->fontMetrics().height()
                + margins.top()+margins.bottom()+2*label->margin());
            if(label->maximumHeight()<height)label->setMaximumHeight(QWIDGETSIZE_MAX);
            if(label->minimumHeight()!=height)label->setMinimumHeight(height);
        }
        if (label && label->objectName()=="labelTitle") {
            QFont title=font();title.setBold(true);
            if(label->property("_rayTitleFont").toString()!=title.key()) {
                label->setProperty("_rayTitleFont",title.key());
                if(label->font()!=title)label->setFont(title);
            }
        }
    }
    // Grid spans may otherwise leave a short form at its single-line minimum
    // even when its title wraps. Use QLabel's own height-for-width calculation
    // as a minimum, never a fixed header height. Reset the original floor before
    // measuring so widening the window or reducing the font can shrink it again.
    if(auto *title=findChild<QLabel*>("labelTitle");title && title->wordWrap() && title->width()>0) {
        const auto m=title->contentsMargins();
        const QStringList metrics{QString::number(title->width()),title->font().key(),title->text(),
            QString::number(m.left()),QString::number(m.top()),QString::number(m.right()),QString::number(m.bottom()),
            QString::number(title->margin()),QString::number(title->indent())};
        const auto key=metrics.join(QChar(0x1f));
        if(title->property("_rayHeaderTitleMetrics").toString()!=key) {
            title->setProperty("_rayHeaderTitleMetrics",key);
            constexpr auto original="_rayHeaderOriginalTitleMinimum";
            if(!title->property(original).isValid())title->setProperty(original,title->minimumHeight());
            const int floor=title->property(original).toInt();
            if(title->minimumHeight()!=floor)title->setMinimumHeight(floor);
            const int required=qMax(floor,title->heightForWidth(title->width()));
            if(title->maximumHeight()<required)title->setMaximumHeight(QWIDGETSIZE_MAX);
            if(title->minimumHeight()!=required)title->setMinimumHeight(required);
        }
    }
    for (auto *tabs : findChildren<QTabWidget *>()) {
        if (!tabs->usesScrollButtons()) tabs->setUsesScrollButtons(true);
        if (tabs->elideMode()!=Qt::ElideNone) tabs->setElideMode(Qt::ElideNone); // Scroll captions instead of truncating their identity.
    }
    if(property("Rows").isValid()) {
        // Keep the Spacer's established 30-logical-pixel row contract, while
        // allowing its header to grow when a larger font requires more room.
        const int height=qMax(property("Rows").toInt()*30,layout()->minimumSize().height());
        if(minimumHeight()!=height)setMinimumHeight(height);
    }
    layout()->activate();
}

int PgnWidgetBase::Id() const
{
    return m_Id;
}
void PgnWidgetBase::setId(int newId)
{
    if (m_Id != newId) {
        m_Id = newId;
        emit IdChanged();
    }
}

int PgnWidgetBase::Pgn() const
{
    return m_Pgn;
}
void PgnWidgetBase::setPgn(int newPgn)
{
    if (m_Pgn != newPgn) {
        m_Pgn = newPgn;
        emit PgnChanged();
    }
}

int PgnWidgetBase::ColumnSpan() const
{
    return m_ColumnSpan;
}
void PgnWidgetBase::setColumnSpan(int newColumnSpan)
{
    if (m_ColumnSpan != newColumnSpan) {
        m_ColumnSpan = newColumnSpan;
        emit ColumnSpanChanged();
    }
}

int PgnWidgetBase::RowSpan() const
{
    return m_RowSpan;
}
void PgnWidgetBase::setRowSpan(int newRowSpan)
{
    if (m_RowSpan != newRowSpan) {
        m_RowSpan = newRowSpan;
        emit RowSpanChanged();
    }
}

UserUnitPrefs PgnWidgetBase::UserPrefs() const
{
    return m_UserPrefs;
}

void PgnWidgetBase::refreshPresentation()
{
    emit presentationRefreshStarted();
    const auto finish=qScopeGuard([this]{emit presentationRefreshFinished();});
    emit UserPrefsChanged();
    setInitialGuiValues();
}

void PgnWidgetBase::setUserPrefs(const UserUnitPrefs &newUserPrefs)
{
    if (m_UserPrefs != newUserPrefs) {
        m_UserPrefs = newUserPrefs;
        refreshPresentation();
    }
}

void PgnWidgetBase::focusInEvent(QFocusEvent *event)
{
    if (event->gotFocus()) {
        emit WidgetHasFocus(m_Id);
    }
    QWidget::focusInEvent(event);
}

void PgnWidgetBase::onTimerTimeout()
{
    // Placeholder for timer behavior, to be overridden by derived classes
}

void PgnWidgetBase::ChangeSpeedUnits_triggered(int index)
{
    if (index == m_UserPrefs.speedUnitIndex) {
        return;
    }

    m_UserPrefs.speedUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeEconomyUnits_triggered(int index)
{
    if (index == m_UserPrefs.fuelEconomyUnitIndex) {
        return;
    }

    m_UserPrefs.fuelEconomyUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeDepthUnits_triggered(int index)
{
    if (index == m_UserPrefs.depthUnitIndex) {
        return;
    }

    m_UserPrefs.depthUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeLatLongUnits_triggered(int index)
{
    if (index == m_UserPrefs.latLongUnitIndex) {
        return;
    }

    m_UserPrefs.latLongUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeHeadingUnits_triggered(int index)
{
    if (index == m_UserPrefs.headingReferenceIndex) {
        return;
    }

    m_UserPrefs.headingReferenceIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeFuelRateUnits_triggered(int index)
{
    if (index == m_UserPrefs.flowRateUnitIndex) {
        return;
    }

    m_UserPrefs.flowRateUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeTempUnits_triggered(int index)
{
    if (index == m_UserPrefs.tempUnitIndex) {
        return;
    }

    m_UserPrefs.tempUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeVolumeUnits_triggered(int index)
{
    if (index == m_UserPrefs.volumeUnitIndex) {
        return;
    }

    m_UserPrefs.volumeUnitIndex = index;
    refreshPresentation();
    qDebug() << "Volume Units changed to " << index;
}

void PgnWidgetBase::ChangeDistanceUnits_triggered(int index)
{
    if (index == m_UserPrefs.distanceUnitIndex) {
        return;
    }

    m_UserPrefs.distanceUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangePressureUnits_triggered(int index)
{
    if (index == m_UserPrefs.pressureUnitIndex) {
        return;
    }

    m_UserPrefs.pressureUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeRateOfTurnUnits_triggered(int index)
{
    if (index == m_UserPrefs.rateOfTurnUnitIndex) {
        return;
    }

    m_UserPrefs.rateOfTurnUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeTimeUnits_triggered(int index)
{
    if (index == m_UserPrefs.timeUnitIndex) {
        return;
    }

    m_UserPrefs.timeUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeDateUnits_triggered(int index)
{
    if (index == m_UserPrefs.dateUnitIndex) {
        return;
    }

    m_UserPrefs.dateUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::ChangeAtmosphericPressureUnits_triggered(int index)
{
    if (index == m_UserPrefs.atmosphericPressureUnitIndex) {
        return;
    }

    m_UserPrefs.atmosphericPressureUnitIndex = index;
    refreshPresentation();
}

void PgnWidgetBase::initializeUnitsLimits()
{
    // Placeholder for implemenation to be overridden by derived classes
}

void PgnWidgetBase::initializeClickableLabels()
{
    // Placeholder for implemenation to be overridden by derived classes
}

void PgnWidgetBase::setInitialGuiValues()
{
    // Placeholder for implemenation to be overridden by derived classes
}

QString PgnWidgetBase::Label() const
{
    return m_Label;
}

void PgnWidgetBase::setLabel(const QString &newLabel)
{
    if (m_Label == newLabel)
        return;
    m_Label = newLabel;
    emit LabelChanged();
}

bool PgnWidgetBase::isEnabled() const
{
    return m_Enabled;
}

void PgnWidgetBase::setEnabled(bool newEnabled)
{
    if (m_Enabled == newEnabled)
        return;
    m_Enabled = newEnabled;
    if(isGenerator())ApplicationLog::event("raynmea.generator",QString("Widget %1 Enable=%2").arg(Id()).arg(newEnabled));
    ++m_dispatchEpoch;
    emit enabledChanged();
}

QString PgnWidgetBase::Source() const
{
    return m_Source;
}

void PgnWidgetBase::setSource(const QString &newSource)
{
    if (m_Source == newSource)
        return;
    m_Source = newSource;
    emit SourceChanged();
}

bool PgnWidgetBase::acceptsEdit(QLineEdit *editor, const QString &text) const
{
    QString candidate=text; int position=0;
    const auto *validator=editor->validator();
    return !validator || validator->validate(candidate,position)==QValidator::Acceptable;
}
