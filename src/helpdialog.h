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

#pragma once
#include <QDialog>
#include <QTextBrowser>
#include <QPointer>

class QLineEdit;
class QTreeWidget;
class QSplitter;
class QPushButton;
class QLabel;

// RF StateSide internal adaptation of BrickSuite HelpDialog: Qt-only offline HTML,
// topic tree/search, history, theme-aware rich text and persisted window geometry.
class HelpBrowser final:public QTextBrowser {
    Q_OBJECT
public:
    explicit HelpBrowser(QWidget *parent=nullptr);
    QVariant loadResource(int type,const QUrl &url) override;
signals:
    void blockedResource();
protected:
    void doSetSource(const QUrl &url,QTextDocument::ResourceType type) override;
};

class HelpDialog final:public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget *parent=nullptr);
    void showTopic(const QString &id,const QString &anchor={});
    QString currentTopic() const;
protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    void rebuildContents();
    void syncSelection(const QUrl &source = {});
    void updateTheme();
    void updateImageSizes();
    void highlightSearch();
    void activateLink(const QUrl &url);
    void saveState();
    QLineEdit *search;
    QTreeWidget *contents;
    HelpBrowser *browser;
    QSplitter *splitter;
    QPushButton *back, *forward;
    QLabel *status;
    QPointer<QDialog> imageDialog;
};
