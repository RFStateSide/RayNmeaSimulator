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

// Adapted from RF StateSide BrickSuite; owner-attested internal reuse (2026-10-28).
// BrickSuite retains its existing license; this adaptation is AGPL-3.0-or-later.
#include "aboutdialog.h"
#include "desktopicons.h"
#include "aboutlinks.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QSysInfo>
#include <QScrollArea>
#include <QScreen>
#include <QEvent>
#include <QVBoxLayout>

namespace {
class AboutText final : public QLabel {
    QString markup;
    void refresh() {
        const auto value=markup.contains("<a ") ? QString("<style>a { color: %1; }</style>").arg(palette().color(QPalette::Link).name())+markup : markup;
        if(text()!=value)QLabel::setText(value);
    }
protected:
    void changeEvent(QEvent *event) override {
        QLabel::changeEvent(event);
        if(event->type()==QEvent::PaletteChange || event->type()==QEvent::StyleChange)refresh();
    }
public:
    AboutText(const QString &text,QWidget *parent):QLabel(parent),markup(text){refresh();}
};
QLabel *wrapped(const QString &text, QWidget *parent) {
    auto *label = new AboutText(text, parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    // Explicit user activation only; report browser launch failure locally.
    label->setOpenExternalLinks(false);
    QObject::connect(label, &QLabel::linkActivated, parent, [parent](const QString &link) {
        AboutLinks::open(parent,QUrl(link));
    });
    return label;
}
}

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
    setObjectName("aboutDialog");
    setWindowTitle(tr("About RayNmeaSimulator"));
    setWindowIcon(DesktopIcons::forPalette(palette()));
    setModal(true);
    setMinimumWidth(560);
    // Follow BrickSuite's existing About header, font hierarchy and spacing.
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 20);
    layout->setSpacing(14);
    auto *header = new QHBoxLayout;
    header->setSpacing(18);
    auto *icon = new QLabel(this);
    icon->setObjectName("aboutIcon");
    icon->setFixedSize(112,112);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(windowIcon().pixmap(QSize(112,112), devicePixelRatioF()));
    header->addWidget(icon,0,Qt::AlignTop);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(4);
    auto *name = new QLabel(QStringLiteral(APP_NAME),this);
    auto font = name->font(); font.setPointSize(font.pointSize()+7);font.setBold(true);name->setFont(font);
    titles->addWidget(name);
    auto *status = wrapped(tr("NMEA2000 / NMEA0183 simulation and monitoring"),this);
    status->setObjectName("aboutTagline");
    font=status->font();font.setItalic(true);status->setFont(font);titles->addWidget(status);
    titles->addSpacing(8);
#ifdef QT_DEBUG
    const QString configuration=tr("Debug build");
#else
    const QString configuration=tr("Release build");
#endif
    auto *version=wrapped(tr("Version %1 · %2").arg(QStringLiteral(APP_VERSION),configuration),this);
    version->setObjectName("aboutVersion");font=version->font();font.setBold(true);version->setFont(font);titles->addWidget(version);
    titles->addWidget(wrapped(tr("Copyright © 2024–2026 RF StateSide LLC"),this));
    titles->addStretch();header->addLayout(titles,1);layout->addLayout(header);
    auto *separator=new QFrame(this);separator->setFrameShape(QFrame::HLine);separator->setFrameShadow(QFrame::Sunken);layout->addWidget(separator);
    textArea = new QScrollArea(this);
    textArea->setFrameShape(QFrame::NoFrame);textArea->setWidgetResizable(true);
    textArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *body=new QWidget(textArea);auto *bodyLayout=new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0,0,0,0);bodyLayout->setSpacing(14);
    bodyLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);bodyLayout->setAlignment(Qt::AlignTop);
    textArea->setWidget(body);layout->addWidget(textArea,1);
    auto *summary=wrapped(tr("Inspect supported marine data in Viewer RX, or build a virtual vessel in Generate. Combine instruments, optional generators, route-following Autopilot and game controller movement."),this);
    summary->setObjectName("aboutSummary");bodyLayout->addWidget(summary);
    auto *guide=wrapped(tr("<b>User Guide</b><br>Open Help → RayNmeaSimulator Help, or press F1, for the complete offline manual. Search topics and select screenshots to enlarge them."),this);
    guide->setObjectName("aboutGuide");bodyLayout->addWidget(guide);
    auto *designation=wrapped(tr("<b>Public Engineering Reference — AS-IS</b><br>Provided without warranty. Use on an appropriate test network; this is not a certified navigation system. External instrument Sending requires explicit authorization."),this);
    designation->setObjectName("aboutDesignation");bodyLayout->addWidget(designation);
    bodyLayout->addWidget(wrapped(tr("<b>Support RayNmeaSimulator</b><br>Voluntary contributions help with development, testing, documentation and maintenance."),this));
    auto *support=wrapped(tr(R"html(<a href="%1">Support RayNmeaSimulator with PayPal</a>)html").arg(QString::fromLatin1(AboutLinks::SupportUrl)),this);support->setObjectName("aboutSupport");
    support->setAccessibleName(tr("Support RayNmeaSimulator"));support->setFocusPolicy(Qt::StrongFocus);
    disconnect(support,&QLabel::linkActivated,this,nullptr);
    connect(support,&QLabel::linkActivated,this,[this](const QString &){
        AboutLinks::open(this,QUrl(QString::fromLatin1(AboutLinks::SupportUrl)));
    });
    support->setToolTip(tr("Open the voluntary PayPal support page in your web browser. Contributions do not unlock features or services."));
    bodyLayout->addWidget(support);
    bodyLayout->addWidget(wrapped(tr("Support is entirely voluntary. Contributions do not unlock features, technical support, warranties or other services."),this));

    bodyLayout->addWidget(wrapped(tr(R"html(<b>Software license:</b> GNU Affero General Public License, version 3 or later (AGPL-3.0-or-later).<br><a href="https://www.gnu.org/licenses/agpl-3.0.html">View the GNU AGPL</a>)html"),this));
    bodyLayout->addWidget(wrapped(tr(R"html(<b>RF StateSide LLC:</b> <a href="https://www.rfstateside.com/">www.rfstateside.com</a><br><b>Source code:</b> <a href="https://github.com/RFStateSide/RayNmeaSimulator">RFStateSide/RayNmeaSimulator</a>)html"),this));
    auto *details=wrapped(tr("Qt %1 (compiled with %3) · %2").arg(QString::fromLatin1(qVersion()),QSysInfo::buildCpuArchitecture(),QString::fromLatin1(QT_VERSION_STR)),this);
    details->setToolTip(tr("Runtime Qt version, build architecture and compile-time Qt version."));bodyLayout->addWidget(details);
    auto *credits=new QPushButton(tr("Licenses and Acknowledgments…"),this);credits->setObjectName("aboutCredits");bodyLayout->addWidget(credits,0,Qt::AlignLeft);
    connect(credits,&QPushButton::clicked,this,[this]{
        QDialog dialog(this);dialog.setWindowTitle(tr("Licenses and Acknowledgments"));dialog.resize(700,520);
        auto *content=new QVBoxLayout(&dialog);auto *text=new QTextBrowser(&dialog);text->setOpenExternalLinks(false);
        QFile file(":/resources/ABOUT-NOTICES.txt");
        text->setPlainText(file.open(QIODevice::ReadOnly)?QString::fromUtf8(file.readAll()):tr("Acknowledgments could not be loaded."));
        content->addWidget(text);auto *close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);content->addWidget(close);dialog.exec();
    });
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);layout->addWidget(buttons);
    adjustSize();
}

QSize AboutDialog::sizeHint() const {
    const auto base=QDialog::sizeHint();if(!textArea)return base;
    const auto available=screen()->availableGeometry().size()*0.9;
    const int width=qMin(available.width(),qMax(minimumSizeHint().width(),fontMetrics().averageCharWidth()*100));
    const auto margins=layout()->contentsMargins();
    const int textHeight=textArea->widget()->heightForWidth(width-margins.left()-margins.right()-2*textArea->frameWidth());
    return QSize(width,qMin(available.height(),base.height()-textArea->sizeHint().height()+textHeight));
}
