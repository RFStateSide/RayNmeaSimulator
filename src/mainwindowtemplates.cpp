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

#include "navigationstatistics.h"
#include "applicationlog.h"
#include "bundledtemplatesource.h"
#include "templatevaluedelegate.h"
#include <QCheckBox>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "layoutdocument.h"
#include "appsettings.h"
#include "apppaths.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QUuid>
#include <QStatusBar>
#include <QRegularExpression>

bool MainWindow::checkTemplate(const QJsonObject &root,QString &error) const
{
    if(root["documentType"]!="template" || root["formatVersion"]!=2) {error=tr("Expected a format 2 Template.");return false;}
    auto concrete=root;concrete["documentType"]="layout";concrete.remove("template");concrete.remove("setupPolicies");
    if(!mainController->validateDocument(concrete,&error))return false;
    // Validate all references without requiring this machine's Setup to instantiate
    // a portable Template. Resolution against real Setup is a separate operation.
    QTemporaryFile settingsFile;
    if(!settingsFile.open()) {error=tr("Cannot validate Template policies.");return false;}
    QSettings fixture(settingsFile.fileName(),QSettings::IniFormat);
    fixture.setValue("Vessel/MMSI",0); fixture.setValue("Vessel/VesselType",0);
    fixture.setValue("Vessel/RadioCallsign",QString());fixture.setValue("Vessel/Length",0);fixture.setValue("Vessel/Beam",0);
    return LayoutDocument::resolve(root,fixture,concrete,error);
}

bool MainWindow::saveTemplate(QJsonObject root,const QString &existingPath,bool readOnly)
{
    readOnly=readOnly || existingPath.startsWith(":/");
    QString inputError;
    const bool valid=root["documentType"]=="template" ? checkTemplate(root,inputError)
        : mainController->validateDocument(root,&inputError);
    if(!valid) {QMessageBox::warning(this,tr("Cannot Edit Template"),inputError);return false;}
    root["formatVersion"]=2;root["documentType"]="template";
    auto metadata=root["template"].toObject();
    if(metadata["id"].toString().isEmpty())metadata["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(!root.contains("setupPolicies"))root["setupPolicies"]=QJsonArray();
    bool source=false;
#ifdef QT_DEBUG
    const auto sourceProject=BundledTemplateSource::project();
    source=!readOnly && !existingPath.isEmpty() && BundledTemplateSource::contains(sourceProject,existingPath);
#endif
    QDialog dialog(this);dialog.setObjectName("templateEditorDialog");dialog.setProperty("rayHelpTopic","templates");dialog.setWindowTitle(readOnly?tr("Template details — read-only"):source?tr("Edit bundled Template source"):existingPath.isEmpty()?tr("Save Template"):tr("Edit Template"));dialog.resize(980,680);
    QVBoxLayout form(&dialog);
    QLabel origin(readOnly?tr("Bundled compiled snapshot — read-only. Duplicate to customize."):source?tr("Bundled source — editable in Debug; rebuild to ship changes."):tr("User Template — editable; independent of bundled resources."));origin.setObjectName("templateOrigin");origin.setWordWrap(true);form.addWidget(&origin);
    QComboBox destination;destination.setObjectName("templateDestination");destination.addItem(tr("User Template"),"user");
#ifdef QT_DEBUG
    destination.addItem(tr("Bundled Template source (Debug; rebuild required)"),"bundled");
#endif
    destination.setCurrentIndex(source?1:0);destination.setEnabled(existingPath.isEmpty()&&!readOnly);destination.setVisible(!readOnly);form.addWidget(&destination);
    connect(&destination,&QComboBox::currentIndexChanged,&dialog,[&]{
        origin.setText(destination.currentData()=="bundled"?tr("Bundled source — editable in Debug; rebuild to ship changes."):tr("User Template — editable; independent of bundled resources."));
    });
#ifdef QT_DEBUG
    QWidget projectRow;QHBoxLayout projectForm(&projectRow);projectForm.setContentsMargins(0,0,0,0);
    QLineEdit projectPath(sourceProject);projectPath.setObjectName("templateProjectRoot");projectPath.setReadOnly(!existingPath.isEmpty());projectForm.addWidget(&projectPath);
    QPushButton chooseProject(tr("Choose project…"));chooseProject.setEnabled(existingPath.isEmpty());projectForm.addWidget(&chooseProject);form.addWidget(&projectRow);
    auto showProject=[&]{projectRow.setVisible(!readOnly && destination.currentData()=="bundled");};showProject();connect(&destination,&QComboBox::currentIndexChanged,&dialog,showProject);
    connect(&chooseProject,&QPushButton::clicked,&dialog,[&]{const auto folder=QFileDialog::getExistingDirectory(&dialog,tr("Bundled Template project"),projectPath.text());if(!folder.isEmpty())projectPath.setText(folder);});
#endif
    QString suggestedName=metadata["name"].toString();
    if(existingPath.isEmpty())suggestedName.replace(QRegularExpression(R"([.<>:"/\\|?*\x00-\x1f])"),"-");
    QLineEdit name(suggestedName);name.setObjectName("templateName");name.setMaxLength(120);name.setReadOnly(readOnly || !existingPath.isEmpty());
    QPlainTextEdit description(metadata["description"].toString());description.setMaximumHeight(75);description.setReadOnly(readOnly);
    form.addWidget(new QLabel(tr("Template name:")));form.addWidget(&name);form.addWidget(new QLabel(tr("Description:")));form.addWidget(&description);
    auto *policyHint=new QLabel(tr("Setup defaults are resolved once when creating a Layout. Unconfigured values remain unconfigured. Independent AIS targets should use saved identities. Enable/Dispatch never authorize external Sending."));
    if(readOnly)policyHint->setText(tr("Read-only snapshot. Duplicate it to edit values and initialization policies. Values and units shown here are canonical."));
    else policyHint->setText(policyHint->text()+tr("\nEdit: click a saved-value cell, double-click, or press F2. Boolean/enum values have dropdowns. Units are canonical; Save validates ranges. Setup Default ignores the saved value during application, but retains it for switching back."));
    policyHint->setWordWrap(true);form.addWidget(policyHint);
    QTableWidget table;table.setColumnCount(5);table.setHorizontalHeaderLabels({tr("Widget"),tr("Parameter"),tr("Saved value"),tr("Units"),tr("Initialization")});
    table.setObjectName("templateParameters");table.setItemDelegateForColumn(2,new TemplateValueDelegate(&table));
    table.setEditTriggers(readOnly?QAbstractItemView::NoEditTriggers:QAbstractItemView::DoubleClicked|QAbstractItemView::SelectedClicked|QAbstractItemView::EditKeyPressed|QAbstractItemView::AnyKeyPressed);
    table.horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table.horizontalHeader()->setStretchLastSection(true);form.addWidget(&table);
    struct Ref{int group,widget,parameter;QString kind;};QList<Ref> refs;
    auto groups=root["layouts"].toArray();
    for(int g=0;g<groups.size();++g) {const auto widgets=groups[g].toObject()["widgets"].toArray();
        for(int w=0;w<widgets.size();++w) {const auto widget=widgets[w].toObject();const auto rows=widget["parameters"].toArray();
            auto add=[&](const QString &field,const QString &value,int parameter,const QString &kind) {
                const int r=table.rowCount();table.insertRow(r);refs.append({g,w,parameter,kind});
                auto *label=new QTableWidgetItem(widget["label"].toString()+QString(" [id %1]").arg(widget["idX"].toInt()));label->setFlags(label->flags()&~Qt::ItemIsEditable);table.setItem(r,0,label);
                auto *key=new QTableWidgetItem(field);key->setFlags(key->flags()&~Qt::ItemIsEditable);table.setItem(r,1,key);
                auto *savedValue=new QTableWidgetItem(value);
                if(parameter>=0)savedValue->setToolTip(tr("Canonical value (%1)").arg(rows[parameter].toArray()[3].toString()));
                const auto type=kind.isEmpty()?rows[parameter].toArray()[3].toString():QString("bool");
                savedValue->setData(TemplateValueDelegate::TypeRole,type);savedValue->setForeground(table.palette().brush(QPalette::Link));
                if(readOnly)savedValue->setFlags(savedValue->flags()&~Qt::ItemIsEditable);
                table.setItem(r,2,savedValue);
                if(auto *values=TemplateValueDelegate::choices(&table,field,type,value)) {
                    values->setObjectName("templateValueChoice");values->setEnabled(!readOnly);table.setCellWidget(r,2,values);
                    savedValue->setFlags(savedValue->flags()&~Qt::ItemIsEditable);
                    connect(values,&QComboBox::currentIndexChanged,&dialog,[values,savedValue]{savedValue->setText(values->currentData().toString());});
                    if (values->property("datumEditor").toBool())
                        connect(values, &QComboBox::editTextChanged, &dialog, [values,savedValue] { savedValue->setText(NavigationStatistics::datumValue(values)); });
                }
                auto *units=new QTableWidgetItem(parameter>=0?rows[parameter].toArray()[2].toString():QString());
                units->setFlags(units->flags()&~Qt::ItemIsEditable);table.setItem(r,3,units);
                const auto setting=kind.isEmpty()?LayoutDocument::mapping(widget["pgn"].toString(),field):QString();
                if(!setting.isEmpty()) {auto *choice=new QComboBox;choice->addItem(tr("Use Saved Value"),QString());choice->addItem(tr("Use Setup Default — %1").arg(setting),setting);
                    for(const auto &p:root["setupPolicies"].toArray())if(p.toObject()["widgetId"]==widget["idX"] && p.toObject()["field"]==field)choice->setCurrentIndex(1);
                    choice->setEnabled(!readOnly);table.setCellWidget(r,4,choice);
                    auto explain=[&,r,choice,savedValue]{const bool setup=choice->currentIndex()==1;auto font=savedValue->font();font.setItalic(setup);savedValue->setFont(font);savedValue->setForeground(table.palette().brush(setup?QPalette::PlaceholderText:QPalette::Link));choice->setToolTip(setup?tr("Apply Vessel/%1 once; the saved value is retained but is not applied.").arg(choice->currentData().toString()):tr("Apply the exact saved canonical value; do not read Setup for this field."));};
                    explain();connect(choice,&QComboBox::currentIndexChanged,&dialog,explain);
                } else {
                    auto *fixed=new QTableWidgetItem(tr("Use Saved Value"));fixed->setFlags(fixed->flags()&~Qt::ItemIsEditable);fixed->setToolTip(tr("No supported Setup mapping for this field."));table.setItem(r,4,fixed);
                }
            };
            add("Enable",widget["enabled"].toBool()?"true":"false",-1,"enabled");
            if(widget["type"].toString().startsWith("Generator"))add("Dispatch",widget["dispatch"].toBool(true)?"true":"false",-1,"dispatch");
            for(int p=0;p<rows.size();++p){const auto row=rows[p].toArray();add(row[0].toString(),row[1].toString(),p,{});}
        }
    }
    QPushButton editValue(tr("Edit selected value…"));editValue.setVisible(!readOnly);form.addWidget(&editValue);
    connect(&editValue,&QPushButton::clicked,&dialog,[&]{const int row=table.currentRow();if(row<0)return;table.setCurrentCell(row,2);if(auto *choice=qobject_cast<QComboBox*>(table.cellWidget(row,2))){choice->setFocus();choice->showPopup();}else table.editItem(table.item(row,2));});
    QLabel errorLabel;errorLabel.setObjectName("templateValidationError");errorLabel.setTextFormat(Qt::PlainText);errorLabel.setWordWrap(true);errorLabel.setText(tr("Edits affect this document only. Save validates; Cancel leaves the original unchanged."));form.addWidget(&errorLabel);
    QDialogButtonBox buttons(readOnly?QDialogButtonBox::StandardButtons(QDialogButtonBox::Close):QDialogButtonBox::StandardButtons(QDialogButtonBox::Save|QDialogButtonBox::Cancel));form.addWidget(&buttons);
    connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&] {
        if(readOnly)return;
        for(auto *editor:table.findChildren<QLineEdit*>("templateValueEditor"))
            if(editor->isVisible() && !editor->hasAcceptableInput()) {errorLabel.setText(tr("Finish a valid numeric value for %1 before saving.").arg(editor->property("templateField").toString()));editor->setFocus();return;}
        table.setFocus(); // Commit a text editor through the normal delegate FocusOut path.
        auto edited=groups;QJsonArray policies;QString error;
        for(int r=0;r<refs.size();++r) {const auto ref=refs[r];auto group=edited[ref.group].toObject();auto widgets=group["widgets"].toArray();auto widget=widgets[ref.widget].toObject();
            const auto *choice=qobject_cast<QComboBox*>(table.cellWidget(r,2));
            const auto value=choice?(choice->property("datumEditor").toBool()?NavigationStatistics::datumValue(choice):choice->currentData().toString()):table.item(r,2)->text();
            if(!ref.kind.isEmpty()) {if(value!="true" && value!="false") {errorLabel.setText(tr("Enable and Dispatch must be true or false."));return;}widget[ref.kind]=(value=="true");}
            else {auto rows=widget["parameters"].toArray();auto row=rows[ref.parameter].toArray();row[1]=value;rows[ref.parameter]=row;widget["parameters"]=rows;
                if(auto *choice=qobject_cast<QComboBox*>(table.cellWidget(r,4));choice && choice->currentIndex()==1)
                    policies.append(QJsonObject{{"widgetId",widget["idX"]},{"field",row[0]},{"setupKey",choice->currentData().toString()}});
            }
            widgets[ref.widget]=widget;group["widgets"]=widgets;edited[ref.group]=group;
        }
        auto candidate=root;metadata["name"]=name.text().trimmed();metadata["description"]=description.toPlainText();candidate["template"]=metadata;candidate["layouts"]=edited;candidate["setupPolicies"]=policies;
        // Observations are not portable Template configuration or credentials.
        auto sanitized=candidate["layouts"].toArray();
        for(int g=0;g<sanitized.size();++g){auto group=sanitized[g].toObject();auto widgets=group["widgets"].toArray();for(int w=0;w<widgets.size();++w){auto widget=widgets[w].toObject();if(widget["type"]=="GeneratorWeather") {auto config=widget["configuration"].toObject();config.remove("origins");widget["configuration"]=config;}widgets[w]=widget;}group["widgets"]=widgets;sanitized[g]=group;}candidate["layouts"]=sanitized;
        bool bundled=false;
#ifdef QT_DEBUG
        bundled=destination.currentData()=="bundled";
        const auto path=existingPath.isEmpty()?(bundled?BundledTemplateSource::savePath(projectPath.text(),metadata["name"].toString(),error):LayoutDocument::savePath(metadata["name"].toString(),error)):existingPath;
#else
        const auto path=existingPath.isEmpty()?LayoutDocument::savePath(metadata["name"].toString(),error):existingPath;
#endif
        if(path.isEmpty() || !checkTemplate(candidate,error)) {errorLabel.setText(error);return;}
        if(!bundled && existingPath.isEmpty() && QFileInfo::exists(path) && QMessageBox::question(&dialog,tr("Template Exists"),tr("Replace this Template file?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
#ifdef QT_DEBUG
        if(bundled) {
            if(!BundledTemplateSource::contains(projectPath.text(),path)){errorLabel.setText(tr("Source is outside the selected project catalog."));return;}
            if(QMessageBox::question(&dialog,tr("Save bundled Template source"),tr("Save to %1?\n\nThis source will ship in subsequent Debug and Release builds. Review saved identities, coordinates and external file references. The current compiled snapshot is unchanged. Reconfigure CMake, then rebuild and restart after authoring.").arg(path),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            if(!BundledTemplateSource::write(projectPath.text(),path,candidate,error)){errorLabel.setText(error);return;}
            AppSettings authoring("RFStateSide","RayNmeaSimulator");authoring.setValue("Authoring/TemplateProject",QDir(projectPath.text()).absolutePath());authoring.sync();
            ApplicationLog::event("raynmea.template","Bundled Template source saved; reconfigure/rebuild required");
            statusBar()->showMessage(tr("Bundled source saved. Reconfigure CMake, then rebuild and restart; the running snapshot is unchanged."),15000);dialog.accept();return;
        }
#endif
        Q_UNUSED(bundled);
        if(!LayoutDocument::write(path,candidate,error)){errorLabel.setText(error);return;}
        ApplicationLog::event("raynmea.template","User Template saved");
        statusBar()->showMessage(tr("Template saved. Working Layout save destination is unchanged."),8000);dialog.accept();
    });
    return dialog.exec()==QDialog::Accepted;
}

bool MainWindow::applyTemplate(const QString &path)
{
    stopApplicationSending();QJsonObject source,layout;QString error;AppSettings settings("RFStateSide","RayNmeaSimulator");
    if(!LayoutDocument::read(path,source,error) || !checkTemplate(source,error) || !LayoutDocument::resolve(source,settings,layout,error)
        || !mainController->validateDocument(layout,&error)) {QMessageBox::warning(this,tr("Cannot Apply Template"),error);return false;}
    const bool loaded=mainController->loadLayoutDocument(layout,{},[this](const QString &warning){return prepareLayoutDestruction(tr("Create Layout from Template"),true,warning);});
    ApplicationLog::event("raynmea.template",loaded?"Template applied as independent Layout":"Template application cancelled or rejected");return loaded;
}

void MainWindow::manageTemplates()
{
    QDialog dialog(this); dialog.setObjectName("templateManagerDialog");dialog.setProperty("rayHelpTopic","templates");dialog.setWindowTitle(tr("Layout Templates")); dialog.resize(1000,620);
    QVBoxLayout form(&dialog);
    auto *help=new QLabel(tr("Select a Template, then open its detailed widget/parameter table. Compiled bundled snapshots are read-only; duplicate them to customize."));
    help->setWordWrap(true); form.addWidget(help);
#ifdef QT_DEBUG
    QCheckBox showSources(tr("Show editable bundled source files (Debug only)")); showSources.setObjectName("showBundledSources");
    QPushButton chooseProject(tr("Choose source project…"));
    QHBoxLayout sourceActions;sourceActions.addWidget(&showSources);sourceActions.addWidget(&chooseProject);form.addLayout(&sourceActions);
    QLabel sourceLocation;sourceLocation.setWordWrap(true);sourceLocation.setTextFormat(Qt::PlainText);form.addWidget(&sourceLocation);
    QString project=BundledTemplateSource::project();
#endif
    QListWidget list;list.setObjectName("templateList");form.addWidget(&list);
    QLabel preview;preview.setTextFormat(Qt::PlainText);preview.setWordWrap(true);form.addWidget(&preview);
    auto path=[&]{return list.currentItem()?list.currentItem()->data(Qt::UserRole).toString():QString();};
    auto kind=[&]{return list.currentItem()?list.currentItem()->data(Qt::UserRole+1).toString():QString();};
    auto refresh=[&] {
        const auto selected=path();list.clear();
        auto add=[&](const QString &file,const QString &origin,const QString &suffix) {
            QJsonObject root;QString error;const bool readable=LayoutDocument::read(file,root,error);
            const auto name=readable?root["template"].toObject()["name"].toString():QFileInfo(file).completeBaseName();
            auto *item=new QListWidgetItem(name+suffix,&list);item->setData(Qt::UserRole,file);item->setData(Qt::UserRole+1,origin);item->setToolTip(file);
        };
        for(const auto &file:LayoutDocument::templates())add(file,file.startsWith(":/")?"compiled":"user",file.startsWith(":/")?tr(" [Bundled compiled snapshot, read-only]"):tr(" [User]"));
#ifdef QT_DEBUG
        project=BundledTemplateSource::project();QString error;
        if(showSources.isChecked())for(const auto &file:BundledTemplateSource::files(project,error))add(file,"source",tr(" [Bundled source, editable in Debug]"));
        sourceLocation.setVisible(showSources.isChecked());
        sourceLocation.setText(error.isEmpty()?tr("Source project: %1\nEdits to approved catalog entries affect subsequent builds. Other saved sources are not compiled until the approved catalog is updated. Reconfigure CMake, then rebuild and restart; the running snapshot is unchanged.").arg(project):error);
#endif
        if(list.count())list.setCurrentRow(0);
        for(int i=0;i<list.count();++i)if(list.item(i)->data(Qt::UserRole)==selected)list.setCurrentRow(i);
    };
    connect(&list,&QListWidget::currentRowChanged,&dialog,[&]{QJsonObject root;QString error;if(path().isEmpty()){preview.clear();return;}preview.setText(LayoutDocument::read(path(),root,error)?LayoutDocument::summary(root):error);});
#ifdef QT_DEBUG
    connect(&showSources,&QCheckBox::toggled,&dialog,refresh);
    connect(&chooseProject,&QPushButton::clicked,&dialog,[&]{
        const auto selected=QFileDialog::getExistingDirectory(&dialog,tr("Bundled Template source project"),project);if(selected.isEmpty())return;
        QString error;if(BundledTemplateSource::directory(selected,error).isEmpty()){QMessageBox::warning(&dialog,tr("Source project"),error);return;}
        AppSettings settings("RFStateSide","RayNmeaSimulator");settings.setValue("Authoring/TemplateProject",selected);settings.sync();showSources.setChecked(true);refresh();
    });
#endif
    QHBoxLayout actions;form.addLayout(&actions);
    auto button=[&](const QString &label,auto action){auto *b=new QPushButton(label,&dialog);actions.addWidget(b);connect(b,&QPushButton::clicked,&dialog,action);return b;};
    auto *create=button(tr("Create Layout"),[&]{if(!path().isEmpty() && applyTemplate(path()))dialog.accept();});
    auto *edit=button(tr("Edit details…"),[&]{QJsonObject root;QString error;
#ifdef QT_DEBUG
        if(kind()=="source" && !BundledTemplateSource::contains(project,path())) {QMessageBox::warning(&dialog,tr("Template source"),tr("This is not an editable file in the selected source catalog."));return;}
#endif
        if(LayoutDocument::read(path(),root,error))saveTemplate(root,path(),kind()=="compiled");else QMessageBox::warning(&dialog,tr("Template details"),error);refresh();});
    auto *duplicate=button(tr("Duplicate"),[&]{QJsonObject root;QString error;if(LayoutDocument::read(path(),root,error)){auto meta=root["template"].toObject();meta["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);meta["name"]=meta["name"].toString()+tr(" copy");root["template"]=meta;saveTemplate(root);}refresh();});
    auto *rename=button(tr("Rename"),[&]{
        const auto old=path();const auto origin=kind();QJsonObject root;QString error;
        if(!LayoutDocument::read(old,root,error)||!checkTemplate(root,error)){QMessageBox::warning(&dialog,tr("Cannot Rename"),error);return;}
        bool ok=false;const auto name=QInputDialog::getText(&dialog,tr("Rename Template"),tr("New name:"),QLineEdit::Normal,root["template"].toObject()["name"].toString(),&ok);if(!ok)return;
#ifdef QT_DEBUG
        if(origin=="source") {
            if(QMessageBox::question(&dialog,tr("Rename bundled source"),tr("Rename this source? Its resource path changes after reconfigure/rebuild; existing startup references may need updating. The running snapshot is unchanged."),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            if(!BundledTemplateSource::rename(project,old,name,error))QMessageBox::warning(&dialog,tr("Rename source"),error);
            refresh();return;
        }
#endif
        if(origin!="user")return;
        const auto dest=LayoutDocument::savePath(name,error);
        if(dest.isEmpty() || (dest!=old && QFileInfo::exists(dest))){QMessageBox::warning(&dialog,tr("Cannot Rename"),error.isEmpty()?tr("A Template already has this filename."):error);return;}
        auto meta=root["template"].toObject();meta["name"]=name;root["template"]=meta;
        if(LayoutDocument::write(dest,root,error) && dest!=old && !QFile::remove(old))error=tr("New copy saved; original could not be removed.");
        if(!error.isEmpty())QMessageBox::warning(&dialog,tr("Rename Template"),error);refresh();
    });
    auto *remove=button(tr("Delete"),[&]{
        const auto file=path();const auto origin=kind();if(origin=="compiled")return;
        const auto message=origin=="source"?tr("Delete this bundled source file? A backup is retained under the project build directory. An approved entry also requires a catalog update before building; the current compiled snapshot remains unchanged."):tr("Delete the selected user Template?");
        if(QMessageBox::question(&dialog,tr("Delete Template"),message,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        QString error;
#ifdef QT_DEBUG
        if(origin=="source")BundledTemplateSource::remove(project,file,error);else
#endif
        if(origin=="user" && !QFile::remove(file))error=tr("Could not delete the file.");
        if(!error.isEmpty())QMessageBox::warning(&dialog,tr("Delete Template"),error);refresh();
    });
    button(tr("Import"),[&]{const auto file=QFileDialog::getOpenFileName(&dialog,tr("Import Template"),{},tr("Template JSON (*.json)"));if(file.isEmpty())return;QJsonObject root;QString error;if(!LayoutDocument::read(file,root,error)||!checkTemplate(root,error)){QMessageBox::warning(&dialog,tr("Import Template"),error);return;}auto meta=root["template"].toObject();meta["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);root["template"]=meta;saveTemplate(root);refresh();});
    auto *exportButton=button(tr("Export"),[&]{QJsonObject root;QString error;if(!LayoutDocument::read(path(),root,error))return;const auto file=QFileDialog::getSaveFileName(&dialog,tr("Export Template"),{},tr("Template JSON (*.json)"));if(!file.isEmpty() && !LayoutDocument::write(file,root,error))QMessageBox::warning(&dialog,tr("Export Template"),error);});
    auto updateActions=[&]{const bool selected=!path().isEmpty();const bool writable=selected && kind()!="compiled";create->setEnabled(selected);edit->setEnabled(selected);edit->setText(writable?tr("Edit details…"):tr("View details…"));rename->setEnabled(writable);remove->setEnabled(writable);duplicate->setEnabled(selected);exportButton->setEnabled(selected);};
    connect(&list,&QListWidget::currentRowChanged,&dialog,updateActions);
    QDialogButtonBox close(QDialogButtonBox::Close);form.addWidget(&close);connect(&close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);refresh();updateActions();dialog.exec();
}

void MainWindow::configureStartupWorkspace()
{
    AppSettings settings("RFStateSide","RayNmeaSimulator");QDialog dialog(this);dialog.setObjectName("startupWorkspaceDialog");dialog.setProperty("rayHelpTopic","default-layout");dialog.setWindowTitle(tr("Startup Workspace"));QVBoxLayout form(&dialog);
    QComboBox mode;mode.addItems({tr("Empty workspace"),tr("Load a selected Layout"),tr("Apply a selected Template")});mode.setCurrentIndex(qBound(0,settings.value("Workspace/StartupMode",0).toInt(),2));form.addWidget(&mode);
    QLineEdit path(settings.value("Workspace/StartupPath").toString());form.addWidget(&path);QPushButton browse(tr("Browse…"));form.addWidget(&browse);
    QLabel hint(tr("Sending remains stopped. Enabled generators operate locally; Dispatch follows the document."));hint.setWordWrap(true);form.addWidget(&hint);
    connect(&browse,&QPushButton::clicked,&dialog,[&]{if(mode.currentIndex()==2){QDialog selection(&dialog);selection.setObjectName("startupTemplateDialog");selection.setProperty("rayHelpTopic","templates");selection.setWindowTitle(tr("Startup Template"));QVBoxLayout selectionForm(&selection);QComboBox targets;
            for(const auto &f:LayoutDocument::templates()){QJsonObject root;QString error;LayoutDocument::read(f,root,error);targets.addItem(root["template"].toObject()["name"].toString()+(f.startsWith(":/")?tr(" [Bundled]"):tr(" [User]"))+" — "+QFileInfo(f).fileName(),f);}
            selectionForm.addWidget(&targets);QDialogButtonBox choose(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);selectionForm.addWidget(&choose);
            connect(&choose,&QDialogButtonBox::accepted,&selection,&QDialog::accept);connect(&choose,&QDialogButtonBox::rejected,&selection,&QDialog::reject);
            if(selection.exec()==QDialog::Accepted && targets.currentIndex()>=0)path.setText(targets.currentData().toString());}else{const auto file=QFileDialog::getOpenFileName(&dialog,tr("Startup Layout"),AppPaths::layoutsDirectory(),tr("Layout JSON (*.json)"));if(!file.isEmpty())path.setText(file);}});
    QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form.addWidget(&buttons);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{QJsonObject root,layout;QString error;if(mode.currentIndex()!=0 && (!LayoutDocument::read(path.text(),root,error) || (mode.currentIndex()==2?!checkTemplate(root,error):!mainController->validateDocument(root,&error)))){QMessageBox::warning(&dialog,tr("Startup Workspace"),error);return;}settings.setValue("Workspace/StartupMode",mode.currentIndex());settings.setValue("Workspace/StartupPath",mode.currentIndex()?path.text():QString());settings.sync();if(settings.status()!=QSettings::NoError){QMessageBox::warning(&dialog,tr("Startup Workspace"),tr("Could not save startup preference."));return;}dialog.accept();});dialog.exec();
}

void MainWindow::restoreStartupWorkspace()
{
    AppSettings settings("RFStateSide","RayNmeaSimulator");const int mode=settings.value("Workspace/StartupMode",0).toInt();
    if(mode==0)return;
    stopApplicationSending();QJsonObject root,layout;QString error;const auto path=settings.value("Workspace/StartupPath").toString();
    if(!LayoutDocument::read(path,root,error) || (mode!=1 && mode!=2)
        || (mode==2 && (!checkTemplate(root,error)||!LayoutDocument::resolve(root,settings,layout,error)))) {
        statusBar()->showMessage(tr("Startup workspace unavailable; using an empty workspace. %1").arg(error),30000);return;
    }
    if(mode==1)layout=root;
    if(!mainController->loadLayoutDocument(layout,mode==1?path:QString(),[](const QString &){return true;}))
        statusBar()->showMessage(tr("Startup workspace could not load; using an empty workspace. Choose Layout → Startup Workspace."),30000);
}

void MainWindow::initializeTemplateUi()
{
    ui->menuLayout->addSeparator();
    auto *create=ui->menuLayout->addAction(tr("Save as Template…"));connect(create,&QAction::triggered,this,[this]{saveTemplate(mainController->captureLayout());});
    auto *templates=ui->menuLayout->addAction(tr("Templates…"));connect(templates,&QAction::triggered,this,&MainWindow::manageTemplates);
    auto *startup=ui->menuLayout->addAction(tr("Startup Workspace…"));connect(startup,&QAction::triggered,this,&MainWindow::configureStartupWorkspace);
    create->setToolTip(tr("Capture current widgets as a Template. Choose saved values or supported Setup defaults; this does not authorize Sending."));
    templates->setToolTip(tr("Load, duplicate or manage Templates. Bundled compiled Templates are read-only; source authoring is Debug-only."));
    startup->setToolTip(tr("Choose the next startup workspace. Startup never authorizes Sending."));
}
