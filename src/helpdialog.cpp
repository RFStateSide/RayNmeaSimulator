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

// RF StateSide internal adaptation of BrickSuite HelpDialog (1e2b8f870200b634806ac37dc3629049162a7915).
// Local resource restrictions and image enlargement are RayNmeaSimulator additions.
#include "helpdialog.h"
#include "helpcatalog.h"
#include "aboutlinks.h"
#include "appsettings.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QSignalBlocker>
#include <cmath>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QShowEvent>
#include <QShortcut>
#include <QSplitter>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextTable>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace {
void fitAvailableScreen(QDialog *dialog) {
    const auto available=dialog->screen()->availableGeometry().adjusted(8,8,-8,-8);
    const auto decoration=dialog->frameGeometry().size()-dialog->size();
    dialog->resize(dialog->size().boundedTo(available.size()-decoration).expandedTo(dialog->minimumSize()));
    // Wayland owns window placement; there we constrain size and let the
    // compositor place the surface. Other desktops also retain a visible frame.
    if(!QGuiApplication::platformName().startsWith("wayland")){
        const auto frame=dialog->frameGeometry();
        if(frame.width()<=available.width() && frame.height()<=available.height()){
            const QPoint target(qBound(available.left(),frame.left(),available.right()-frame.width()+1),qBound(available.top(),frame.top(),available.bottom()-frame.height()+1));
            if(target!=frame.topLeft())dialog->move(dialog->pos()+target-frame.topLeft());
        }
    }
}
QColor readable(const QColor &wanted,const QColor &background) {
    auto luminance=[](const QColor &color){auto c=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};return .2126*c(color.redF())+.7152*c(color.greenF())+.0722*c(color.blueF());};
    auto contrast=[&](const QColor &color){auto a=luminance(color),b=luminance(background);return (qMax(a,b)+.05)/(qMin(a,b)+.05);};
    if (contrast(wanted)>=4.5) return wanted;
    return contrast(Qt::black)>contrast(Qt::white) ? QColor(Qt::black) : QColor(Qt::white);
}
// Image enlargement extends BrickSuite's responsive inline images for the
// illustrated manual. It only receives a catalog-allowlisted compiled resource.
class HelpImageCanvas final:public QWidget {
    QImage image;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);painter.fillRect(rect(),palette().color(QPalette::Base));
        // Paint the visible region at the current device scale instead of
        // allocating an enormous upscaled pixmap for a zoomed 4K screenshot.
        painter.setRenderHint(QPainter::SmoothPixmapTransform);painter.drawImage(rect(),image);
    }
public:
    explicit HelpImageCanvas(const QImage &source):image(source){setProperty("sourceSize",source.size());}
};
class HelpImageDialog final:public QDialog {
    QImage image;
    QScrollArea *scroll;
    HelpImageCanvas *label;
    bool fit=true;
    qreal zoom=1;
    void render() {
        if(image.isNull())return;
        const auto available=scroll->viewport()->size()-QSize(12,12);
        qreal scale=fit ? qMin(qreal(available.width())/image.width(),qreal(available.height())/image.height()) : zoom;
        scale=qBound(.05,scale,8.0);
        label->resize(image.size()*scale);label->update();
    }
protected:
    void showEvent(QShowEvent *event) override {
        QDialog::showEvent(event);QTimer::singleShot(0,this,[this]{if(isVisible()){fitAvailableScreen(this);render();}});
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if (object==scroll->viewport() && event->type()==QEvent::Resize) QTimer::singleShot(0,this,[this]{render();});
        return QDialog::eventFilter(object,event);
    }
public:
    HelpImageDialog(const QUrl &url,QWidget *parent):QDialog(parent),image(':'+url.path()) {
        setObjectName("helpImageDialog");setWindowTitle(tr("Help Image"));
        setWindowIcon(parent->windowIcon());setMinimumSize(360,280);resize(900,650);
        auto *layout=new QVBoxLayout(this);auto *row=new QHBoxLayout;
        auto button=[&](const QString &text,const QString &name,const QKeySequence &key,auto action){auto *b=new QPushButton(text,this);b->setObjectName(name);b->setAutoDefault(false);row->addWidget(b);connect(b,&QPushButton::clicked,this,action);auto *shortcut=new QShortcut(key,this);connect(shortcut,&QShortcut::activated,this,action);};
        button(tr("Fit"),"helpImageFit",QKeySequence("Ctrl+0"),[this]{fit=true;render();});
        button(tr("100%"),"helpImageActual",QKeySequence("Ctrl+1"),[this]{fit=false;zoom=1;render();});
        button(tr("Zoom In"),"helpImageZoomIn",QKeySequence::ZoomIn,[this]{fit=false;zoom=qMin(zoom*1.25,8.0);render();});
        button(tr("Zoom Out"),"helpImageZoomOut",QKeySequence::ZoomOut,[this]{fit=false;zoom=qMax(zoom/1.25,.05);render();});
        row->addStretch();layout->addLayout(row);
        scroll=new QScrollArea(this);scroll->setObjectName("helpImageScroll");scroll->setAlignment(Qt::AlignCenter);
        label=new HelpImageCanvas(image);label->setObjectName("helpImage");label->setAccessibleName(windowTitle());scroll->setWidget(label);scroll->viewport()->installEventFilter(this);layout->addWidget(scroll,1);
        auto *close=new QDialogButtonBox(QDialogButtonBox::Close,this);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,this,&QDialog::reject);
        QTimer::singleShot(0,this,[this]{render();});
    }
};
}

HelpBrowser::HelpBrowser(QWidget *parent):QTextBrowser(parent) {
    setOpenLinks(false);setOpenExternalLinks(false);setReadOnly(true);
}
QVariant HelpBrowser::loadResource(int type,const QUrl &url) {
    const auto resolved=HelpCatalog::resolve(url,source().isEmpty()?HelpCatalog::topicUrl("introduction"):source());
    if (type==QTextDocument::HtmlResource) {
        const auto id=HelpCatalog::topicForUrl(resolved);
        const auto text=HelpCatalog::article(id);
        if (!id.isEmpty() && !text.isEmpty()) return text;
    } else if (type==QTextDocument::ImageResource && HelpCatalog::isImage(resolved)) return QImage(':'+resolved.path());
    emit blockedResource();return {};
}
void HelpBrowser::doSetSource(const QUrl &url,QTextDocument::ResourceType type) {
    const auto resolved=HelpCatalog::resolve(url,source().isEmpty()?HelpCatalog::topicUrl("introduction"):source());
    const auto id=HelpCatalog::topicForUrl(resolved);
    if (id.isEmpty() || HelpCatalog::article(id).isEmpty() || (type!=QTextDocument::UnknownResource && type!=QTextDocument::HtmlResource)) { emit blockedResource();return; }
    QTextBrowser::doSetSource(resolved,QTextDocument::HtmlResource);
}
HelpDialog::HelpDialog(QWidget *parent):QDialog(parent) {
    setObjectName("helpDialog");setWindowTitle(tr("RayNmeaSimulator Help"));setWindowIcon(QApplication::windowIcon());setModal(false);
    resize(1050,720);setMinimumSize(620,400);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(10,10,10,10);layout->setSpacing(8);
    auto *navigation=new QHBoxLayout;
    back=new QPushButton(tr("Back"),this);forward=new QPushButton(tr("Forward"),this);auto *home=new QPushButton(tr("Home"),this);
    back->setObjectName("helpBack");forward->setObjectName("helpForward");home->setObjectName("helpHome");
    for(auto *button:{back,forward,home}){button->setAutoDefault(false);navigation->addWidget(button);}navigation->addStretch();layout->addLayout(navigation);
    splitter=new QSplitter(this);splitter->setObjectName("helpSplitter");auto *left=new QWidget(splitter);auto *leftLayout=new QVBoxLayout(left);leftLayout->setContentsMargins(0,0,0,0);
    auto *searchLabel=new QLabel(tr("Search &Help"),left);search=new QLineEdit(left);search->setObjectName("helpSearch");search->setPlaceholderText(tr("Search topics and article text…"));search->setClearButtonEnabled(true);searchLabel->setBuddy(search);leftLayout->addWidget(searchLabel);leftLayout->addWidget(search);
    contents=new QTreeWidget(left);contents->setObjectName("helpContents");contents->setHeaderLabel(tr("Contents"));contents->setUniformRowHeights(true);leftLayout->addWidget(contents,1);
    browser=new HelpBrowser(splitter);browser->setObjectName("helpArticle");browser->setAccessibleName(tr("Help article"));browser->viewport()->installEventFilter(this);
    browser->installEventFilter(this);search->installEventFilter(this);contents->installEventFilter(this);
    splitter->addWidget(left);splitter->addWidget(browser);splitter->setStretchFactor(1,1);splitter->setSizes({280,750});layout->addWidget(splitter,1);
    status=new QLabel(this);status->setObjectName("helpStatus");status->setWordWrap(true);layout->addWidget(status);
    auto *close=new QDialogButtonBox(QDialogButtonBox::Close,this);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,this,&QDialog::reject);
    AppSettings settings(APP_COMPANY,APP_NAME);restoreGeometry(settings.value("Help/Geometry").toByteArray());splitter->restoreState(settings.value("Help/Splitter").toByteArray());
    const auto available=screen()->availableGeometry();resize(size().boundedTo(available.size()).expandedTo(minimumSize()));if(!available.intersects(frameGeometry()))move(available.topLeft()+QPoint(20,20));
    connect(this,&QDialog::finished,this,[this]{saveState();});
    connect(back,&QPushButton::clicked,browser,&QTextBrowser::backward);connect(forward,&QPushButton::clicked,browser,&QTextBrowser::forward);connect(home,&QPushButton::clicked,this,[this]{showTopic("introduction");});
    connect(browser,&QTextBrowser::backwardAvailable,back,&QPushButton::setEnabled);connect(browser,&QTextBrowser::forwardAvailable,forward,&QPushButton::setEnabled);back->setEnabled(false);forward->setEnabled(false);
    connect(browser,&QTextBrowser::anchorClicked,this,&HelpDialog::activateLink);
    connect(browser,&HelpBrowser::blockedResource,this,[this]{status->setText(tr("Help resource unavailable or blocked. Use Contents or Home."));});
    // sourceChanged can precede QTextBrowser's history cursor update. Use its
    // actual destination instead of reading the previous source() in this slot.
    connect(browser,&QTextBrowser::sourceChanged,this,[this](const QUrl &source){status->clear();syncSelection(source);updateTheme();updateImageSizes();});
    connect(search,&QLineEdit::textChanged,this,[this]{rebuildContents();highlightSearch();});
    // One activation path avoids duplicate navigation from click + activation.
    connect(contents,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item){if(item && !item->data(0,Qt::UserRole).toString().isEmpty()){auto id=item->data(0,Qt::UserRole).toString();if(id!=currentTopic())browser->setSource(HelpCatalog::topicUrl(id));}});
    auto shortcut=[this](const QKeySequence &key,auto action){auto *s=new QShortcut(key,this);connect(s,&QShortcut::activated,this,action);};
    shortcut(QKeySequence::Find,[this]{search->setFocus();search->selectAll();});shortcut(QKeySequence(Qt::ALT|Qt::Key_Left),[this]{browser->backward();});shortcut(QKeySequence(Qt::ALT|Qt::Key_Right),[this]{browser->forward();});
    rebuildContents();showTopic("introduction");
}
void HelpDialog::saveState(){AppSettings settings(APP_COMPANY,APP_NAME);settings.setValue("Help/Geometry",saveGeometry());settings.setValue("Help/Splitter",splitter->saveState());}
QString HelpDialog::currentTopic()const{return HelpCatalog::topicForUrl(browser->source());}
void HelpDialog::showTopic(const QString &id,const QString &anchor) {
    const auto url=HelpCatalog::topicUrl(id);
    if(url.isEmpty() || HelpCatalog::article(id).isEmpty()) {status->setText(tr("This Help topic is unavailable. Choose Contents or Home."));return;}
    search->clear();auto target=url;target.setFragment(anchor);browser->setSource(target);
}
void HelpDialog::rebuildContents() {
    const QSignalBlocker blocker(contents);contents->clear();const bool filtered=!search->text().trimmed().isEmpty();contents->setHeaderLabel(filtered?tr("Search Results"):tr("Contents"));
    const auto matches=HelpCatalog::search(search->text());QTreeWidgetItem *group=nullptr;QString previous;
    for(const auto &info:HelpCatalog::topics()) {
        if(!matches.contains(info.id))continue;
        if(!filtered && info.group!=previous){group=new QTreeWidgetItem(contents,QStringList{info.group});previous=info.group;group->setFlags(group->flags()&~Qt::ItemIsSelectable);}
        auto *item=filtered?new QTreeWidgetItem(contents):new QTreeWidgetItem(group);item->setText(0,info.title);item->setData(0,Qt::UserRole,info.id);item->setToolTip(0,info.title);
    }
    if(matches.isEmpty()){auto *item=new QTreeWidgetItem(contents,QStringList{tr("No Help topics found.")});item->setDisabled(true);}
    contents->expandAll();syncSelection();
}
void HelpDialog::syncSelection(const QUrl &source){const auto topic=source.isEmpty()?currentTopic():HelpCatalog::topicForUrl(source);const QSignalBlocker blocker(contents);QTreeWidgetItemIterator it(contents);while(*it){if((*it)->data(0,Qt::UserRole).toString()==topic){contents->setCurrentItem(*it);contents->scrollToItem(*it);return;}++it;}contents->setCurrentItem(nullptr);}
void HelpDialog::activateLink(const QUrl &url) {
    if(url.scheme()=="https" && url.isValid() && url.host().size() && url.userInfo().isEmpty()){AboutLinks::open(this,url);return;}
    const auto resolved=HelpCatalog::resolve(url,browser->source());
    if(HelpCatalog::isImage(resolved)) {
        if(QImage(':'+resolved.path()).isNull()){status->setText(tr("This Help image could not be decoded. Use Contents or Home."));return;}
        if(imageDialog){imageDialog->close();imageDialog=nullptr;}
        imageDialog=new HelpImageDialog(resolved,this);imageDialog->setAttribute(Qt::WA_DeleteOnClose);imageDialog->show();return;
    }
    if(!HelpCatalog::topicForUrl(resolved).isEmpty()){browser->setSource(resolved);return;}
    status->setText(tr("This link is unavailable or blocked. Only Help resources and explicit HTTPS links are supported."));
}
void HelpDialog::highlightSearch() {
    QList<QTextEdit::ExtraSelection> selected;const auto needle=search->text().trimmed();QTextCursor cursor(browser->document());
    if(!needle.isEmpty())while(!(cursor=browser->document()->find(needle,cursor)).isNull()){QTextEdit::ExtraSelection s;s.cursor=cursor;s.format.setBackground(browser->palette().color(QPalette::Highlight));s.format.setForeground(readable(browser->palette().color(QPalette::HighlightedText),browser->palette().color(QPalette::Highlight)));selected.append(s);}
    browser->setExtraSelections(selected);
}
void HelpDialog::updateTheme() {
    const auto palette=browser->palette();const auto link=readable(palette.color(QPalette::Link),palette.color(QPalette::Base));
    const auto selectionStyle=QString("QTextBrowser#helpArticle { selection-color: %1; }").arg(readable(palette.color(QPalette::HighlightedText),palette.color(QPalette::Highlight)).name());
    if(browser->styleSheet()!=selectionStyle)browser->setStyleSheet(selectionStyle);
    browser->document()->setDefaultStyleSheet(QString("h1 {font-size: x-large;} h2 {font-size: large;} p, li {line-height: 125%;} a {color:%1;} code,pre {font-family:monospace;} table {border-collapse:collapse;}").arg(link.name()));
    const bool modified=browser->document()->isModified();
    for(auto block=browser->document()->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){const auto fragment=it.fragment();if(!fragment.isValid() || !fragment.charFormat().isAnchor())continue;QTextCursor cursor(browser->document());cursor.setPosition(fragment.position());cursor.setPosition(fragment.position()+fragment.length(),QTextCursor::KeepAnchor);QTextCharFormat format;format.setForeground(link);cursor.mergeCharFormat(format);}
    browser->document()->setModified(modified);highlightSearch();
}
void HelpDialog::updateImageSizes() {
    constexpr int PreferredWidth=QTextFormat::UserProperty+51;auto *document=browser->document();const bool modified=document->isModified();
    for(auto block=document->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){const auto fragment=it.fragment();if(!fragment.isValid() || !fragment.charFormat().isImageFormat())continue;auto format=fragment.charFormat().toImageFormat();const auto image=document->resource(QTextDocument::ImageResource,QUrl(format.name())).value<QImage>();if(image.isNull())continue;
        const auto frame=document->rootFrame()->frameFormat();const qreal available=qMax<qreal>(1,browser->viewport()->width()-frame.leftMargin()-frame.rightMargin()-block.blockFormat().leftMargin()-block.blockFormat().rightMargin()-16);
        const qreal preferred=format.hasProperty(PreferredWidth)?format.doubleProperty(PreferredWidth):(format.width()>0?format.width():image.width()/image.devicePixelRatio());format.setProperty(PreferredWidth,preferred);format.setWidth(qMin(preferred,available));format.setHeight(format.width()*image.height()/image.width());
        QTextCursor cursor(document);cursor.setPosition(fragment.position());cursor.setPosition(fragment.position()+fragment.length(),QTextCursor::KeepAnchor);cursor.setCharFormat(format);
    }document->setModified(modified);
}
void HelpDialog::changeEvent(QEvent *event){QDialog::changeEvent(event);if(event->type()==QEvent::PaletteChange || event->type()==QEvent::StyleChange)QTimer::singleShot(0,this,[this]{updateTheme();});}
void HelpDialog::showEvent(QShowEvent *event){QDialog::showEvent(event);QTimer::singleShot(0,this,[this]{if(isVisible())fitAvailableScreen(this);});}
bool HelpDialog::eventFilter(QObject *object,QEvent *event){
    // QLineEdit ignores Return after emitting returnPressed; propagation to
    // QDialog can activate its default Close button. Search is live, so Enter
    // opens the selected match (or first match) and stays in the search field.
    if(object==search && (event->type()==QEvent::ShortcutOverride || event->type()==QEvent::KeyPress)){
        const auto *key=static_cast<QKeyEvent*>(event);
        if(key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter){
            event->accept();
            if(event->type()==QEvent::KeyPress && !search->text().trimmed().isEmpty()){
                auto *item=contents->currentItem();
                if(!item && contents->topLevelItemCount())item=contents->topLevelItem(0);
                if(item && !item->isDisabled()){
                    const auto id=item->data(0,Qt::UserRole).toString();
                    if(!id.isEmpty() && id!=currentTopic())browser->setSource(HelpCatalog::topicUrl(id));
                    search->setFocus();
                }
            }
            return true;
        }
    }
    // Native Unix text controls may claim Ctrl+F as Emacs cursor navigation
    // during ShortcutOverride. Help's explicit Find command takes precedence
    // within these reading/navigation controls, without changing app shortcuts.
    if((object==browser || object==search || object==contents)
        && (event->type()==QEvent::ShortcutOverride || event->type()==QEvent::KeyPress)){
        auto *key=static_cast<QKeyEvent*>(event);
        if(key->matches(QKeySequence::Find)){
            event->accept();
            if(event->type()==QEvent::KeyPress){search->setFocus();search->selectAll();}
            return true;
        }
    }
    if(object==browser->viewport() && event->type()==QEvent::Resize)QTimer::singleShot(0,this,[this]{updateImageSizes();});
    return QDialog::eventFilter(object,event);
}
