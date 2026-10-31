// Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <QDialog>
#include "updatechecker.h"
class QLabel;
class QPushButton;
class QPlainTextEdit;
class UpdateDialog final:public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget *parent=nullptr,UpdateChecker *checker=nullptr);
    void startCheck();
private:
    UpdateChecker *checker;
    QLabel *latest,*status,*package;
    QPlainTextEdit *notes;
    QPushButton *check,*open;
    QUrl releaseUrl;
    QString releaseTag;
};
