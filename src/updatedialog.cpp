// Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
#include "updatedialog.h"
#include "aboutlinks.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
UpdateDialog::UpdateDialog(QWidget *parent,UpdateChecker *service):QDialog(parent),checker(service?service:new UpdateChecker(this)){
    setWindowTitle(tr("Check for Updates — RayNmeaSimulator"));setObjectName("updateDialog");setProperty("rayHelpTopic","check-for-updates");setModal(false);resize(620,520);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(20,20,20,20);layout->setSpacing(12);
    auto *heading=new QLabel(tr("RayNmeaSimulator updates"),this);auto font=heading->font();font.setPointSize(font.pointSize()+4);font.setBold(true);heading->setFont(font);layout->addWidget(heading);
    auto *installed=new QLabel(tr("Installed version: %1").arg(QStringLiteral(APP_VERSION)),this);installed->setObjectName("installedVersion");layout->addWidget(installed);
    latest=new QLabel(tr("Latest stable version: not checked"),this);latest->setObjectName("latestVersion");latest->setTextFormat(Qt::PlainText);layout->addWidget(latest);
    status=new QLabel(tr("Check official releases when you are ready."),this);status->setObjectName("updateStatus");status->setTextFormat(Qt::PlainText);status->setWordWrap(true);status->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(status);
    package=new QLabel(this);package->setObjectName("updatePackage");package->setTextFormat(Qt::PlainText);package->setWordWrap(true);package->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(package);
    notes=new QPlainTextEdit(this);notes->setObjectName("releaseNotes");notes->setReadOnly(true);notes->setPlaceholderText(tr("Release information will appear here."));layout->addWidget(notes,1);
    auto *policy=new QLabel(tr("Only release information is retrieved. Nothing is downloaded for installation, installed or restarted. Your current session continues."),this);policy->setWordWrap(true);layout->addWidget(policy);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);check=buttons->addButton(tr("Check now"),QDialogButtonBox::ActionRole);check->setObjectName("checkNow");open=buttons->addButton(tr("Open official release page"),QDialogButtonBox::ActionRole);open->setObjectName("openReleasePage");open->setEnabled(false);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(check,&QPushButton::clicked,this,&UpdateDialog::startCheck);
    connect(open,&QPushButton::clicked,this,[this]{if(UpdateChecker::officialPage(releaseUrl,releaseTag))AboutLinks::open(this,releaseUrl);});
    connect(this,&QDialog::finished,checker,&UpdateChecker::cancel);
    connect(checker,&UpdateChecker::completed,this,[this](const UpdateResult &r){check->setEnabled(true);latest->setText(r.latest.isEmpty()?tr("Latest stable version: unavailable"):tr("Latest stable version: %1").arg(r.latest));status->setText(r.message);notes->setPlainText(r.notes);releaseUrl=r.releaseUrl;releaseTag=r.tag;open->setEnabled(UpdateChecker::officialPage(releaseUrl,releaseTag));package->setText(r.packageName.isEmpty()?QString():tr("Matching package: %1\nPublished SHA-256: %2\nThe updater has not downloaded or verified the installer itself.").arg(r.packageName,r.checksum));});
}
void UpdateDialog::startCheck(){if(checker->busy())return;check->setEnabled(false);open->setEnabled(false);package->clear();notes->clear();latest->setText(tr("Latest stable version: checking…"));status->setText(tr("Checking official GitHub releases…"));checker->check();}
