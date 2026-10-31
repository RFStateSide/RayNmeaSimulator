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

// Original implementation informed by RF StateSide BrickSuite Updater conventions.
#include "updatechecker.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSysInfo>
#include <QVersionNumber>
#include <QMap>
#if defined(Q_OS_LINUX) && defined(__GLIBC__)
#include <gnu/libc-version.h>
#endif
namespace {
const QString repository=QStringLiteral("RFStateSide/RayNmeaSimulator");
const QString web=QStringLiteral("https://github.com/")+repository+"/releases/";
const QString metadataName=QStringLiteral("RayNmeaSimulator-update.json");
struct Version { QStringList core,pre; };
std::optional<Version> version(const QString &s) {
    if(s.size()>128)return {};
    static const QRegularExpression rx(QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*))?(?:\\+[0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)?$"));
    const auto m=rx.match(s);if(!m.hasMatch())return {};
    Version v{{m.captured(1),m.captured(2),m.captured(3)}, {}};
    if(!m.captured(4).isEmpty())v.pre=m.captured(4).split('.');
    for(const auto &p:v.pre)if(QRegularExpression("^[0-9]+$").match(p).hasMatch()&&p.size()>1&&p.startsWith('0'))return {};
    return v;
}
int numeric(const QString &a,const QString &b) { return a.size()!=b.size()?(a.size()>b.size()?1:-1):QString::compare(a,b); }
bool cleanUrl(const QUrl &u) { return u.isValid()&&u.scheme()=="https"&&u.userInfo().isEmpty()&&u.port()==-1&&u.fragment().isEmpty(); }
QJsonObject asset(const QJsonObject &r,const QString &name) {
    QJsonObject found;for(const auto &v:r.value("assets").toArray())if(v.toObject().value("name").toString()==name){if(!found.isEmpty())return {};found=v.toObject();}return found;
}
bool numberVersion(const QString &s) { return QRegularExpression("^[0-9]+(?:\\.[0-9]+){1,3}$").match(s).hasMatch()&&s.size()<32; }
}
std::optional<int> UpdateChecker::compareVersions(const QString &a,const QString &b) {
    auto x=version(a),y=version(b);if(!x||!y)return {};
    for(int i=0;i<3;++i){const int c=numeric(x->core[i],y->core[i]);if(c)return c;}
    if(x->pre.isEmpty()!=y->pre.isEmpty())return x->pre.isEmpty()?1:-1;
    for(int i=0;i<qMin(x->pre.size(),y->pre.size());++i){
        const QString p=x->pre[i],q=y->pre[i];if(p==q)continue;
        const bool pn=QRegularExpression("^[0-9]+$").match(p).hasMatch(),qn=QRegularExpression("^[0-9]+$").match(q).hasMatch();
        return pn&&qn?numeric(p,q):(pn!=qn?(pn?-1:1):QString::compare(p,q));
    }
    return x->pre.size()==y->pre.size()?0:(x->pre.size()>y->pre.size()?1:-1);
}
bool UpdateChecker::officialPage(const QUrl &u,const QString &tag) {
    return cleanUrl(u)&&u.query().isEmpty()&&u.toString(QUrl::FullyEncoded)==web+"tag/"+tag;
}
bool UpdateChecker::officialAsset(const QUrl &u,const QString &tag,const QString &name) {
    return QRegularExpression("^[A-Za-z0-9][A-Za-z0-9._-]*$").match(name).hasMatch()&&name!="."&&name!=".."
        &&cleanUrl(u)&&u.query().isEmpty()&&u.toString(QUrl::FullyEncoded)==web+"download/"+tag+"/"+name;
}
UpdateHost UpdateHost::current() {
    UpdateHost h;
#if defined(Q_OS_WIN)
    h.os="windows";
#elif defined(Q_OS_LINUX)
    h.os="linux";
#else
    h.os="unsupported";
#endif
#if defined(Q_PROCESSOR_X86_64)
    h.architecture="x86_64";
#elif defined(Q_PROCESSOR_ARM_64)
    h.architecture="arm64";
#else
    h.architecture="unsupported";
#endif
    h.distribution=QSysInfo::productType();h.distributionVersion=QSysInfo::productVersion();
#if defined(Q_OS_LINUX) && defined(__GLIBC__)
    h.abi.insert("GLIBC",QString::fromLatin1(gnu_get_libc_version()));
    // Inspect only the already loaded C++ runtime, read-only. Unknown ABI fails closed.
    QFile maps("/proc/self/maps");if(maps.open(QIODevice::ReadOnly)){
        const auto lines=maps.readAll().split('\n');
        for(const auto &line:lines){const int start=line.indexOf('/');if(start<0)continue;
            const auto path=QString::fromLocal8Bit(line.mid(start));if(!path.contains("/libstdc++.so."))continue;
            QFile lib(path);if(lib.size()>64*1024*1024||!lib.open(QIODevice::ReadOnly))continue;
            const auto data=QString::fromLatin1(lib.readAll());
            for(const auto &key:{QString("GLIBCXX"),QString("CXXABI")}){
                QRegularExpression rx(key+"_([0-9]+(?:\\.[0-9]+)+)");auto matches=rx.globalMatch(data);QString best;
                while(matches.hasNext()){const auto v=matches.next().captured(1);if(best.isEmpty()||QVersionNumber::compare(QVersionNumber::fromString(v),QVersionNumber::fromString(best))>0)best=v;}
                if(!best.isEmpty())h.abi.insert(key,best);
            }break;
        }
    }
#endif
    return h;
}
QString UpdateHost::platformKey() const {
    if(os=="windows"&&architecture=="x86_64")return "windows-x86_64";
    if(os=="linux"&&architecture=="x86_64")return "linux-x86_64";
    if(os=="linux"&&architecture=="arm64")return distribution=="debian"&&distributionVersion.section('.',0,0)=="12"?"linux-arm64-debian12":"linux-arm64";
    return {};
}
UpdateResult UpdateChecker::parseRelease(const QByteArray &data,const QString &installed) {
    UpdateResult r;r.installed=installed;r.message=tr("The release service returned invalid information.");
    if(data.size()>512*1024)return r;
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(data,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return r;
    const auto obj=doc.object();const auto tag=obj.value("tag_name").toString();const auto latest=tag.startsWith('v')?tag.mid(1):tag;
    if(!version(latest))return r;
    r.tag=tag;r.latest=latest;
    const auto c=compareVersions(r.latest,installed);if(!c||!obj.value("draft").isBool()||!obj.value("prerelease").isBool()||!obj.value("assets").isArray()
       ||obj.value("id").toDouble()<=0||!officialPage(QUrl(obj.value("html_url").toString(),QUrl::StrictMode),r.tag))return r;
    r.release=obj;r.releaseUrl=QUrl(obj.value("html_url").toString());r.notes=obj.value("body").toString().left(32000);
    if(obj.value("draft").toBool()||obj.value("prerelease").toBool()||!version(r.latest)->pre.isEmpty()){
        r.state=UpdateResult::Unavailable;r.message=tr("No stable release information is available. Prereleases are not offered here.");r.releaseUrl=QUrl();return r;
    }
    r.state=*c>0?UpdateResult::Newer:(*c<0?UpdateResult::Older:UpdateResult::Current);
    r.message=*c>0?tr("A newer official release is available."):(*c<0?tr("This build is newer than the latest published stable release."):tr("You have the latest published stable version."));
    return r;
}
UpdateResult UpdateChecker::selectPackage(UpdateResult r,const QByteArray &data,const UpdateHost &host) {
    r.state=UpdateResult::Incompatible;r.message=tr("No verified compatible package is listed for this build and runtime. Read the release notes before choosing a package.");r.packageName.clear();r.checksum.clear();
    if(data.size()>128*1024)return r;
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(data,&error);if(error.error!=QJsonParseError::NoError||!doc.isObject())return r;
    const auto obj=doc.object();
    if(obj.value("schema")!=QJsonValue(1)||obj.value("repository").toString()!=repository||obj.value("version").toString()!=r.latest||!obj.value("downloads").isObject()||host.platformKey().isEmpty())return r;
    const auto p=obj.value("downloads").toObject().value(host.platformKey()).toObject();const auto name=p.value("name").toString();const auto a=asset(r.release,name);const auto sha=p.value("sha256").toString();const auto url=QUrl(p.value("url").toString(),QUrl::StrictMode);
    const QMap<QString,QString> names{{"windows-x86_64","Windows-x64.zip"},{"linux-x86_64","Linux-x86_64.tar.gz"},{"linux-arm64","Linux-aarch64.tar.gz"},{"linux-arm64-debian12","Linux-aarch64-Debian12.tar.gz"}};
    if(name!="RayNmeaSimulator-"+r.latest+"-"+names.value(host.platformKey()))return r;

    if(a.isEmpty()||a.value("state").toString()!="uploaded"||!officialAsset(url,r.tag,name)||a.value("browser_download_url").toString()!=url.toString()||p.value("bytes").toDouble()<=0||p.value("bytes")!=a.value("size")||!QRegularExpression("^[a-f0-9]{64}$").match(sha).hasMatch())return r;
    const auto digest=a.value("digest").toString();if(!digest.isEmpty()&&digest!="sha256:"+sha)return r;
    if(host.os=="linux"){
        const auto required=p.value("required_abi").toObject();
        for(const auto &key:{QString("GLIBC"),QString("GLIBCXX"),QString("CXXABI")}){
            const auto min=required.value(key).toString(),have=host.abi.value(key).toString();
            if(!numberVersion(min)||!numberVersion(have)||QVersionNumber::compare(QVersionNumber::fromString(min),QVersionNumber::fromString(have))>0)return r;
        }
        for(auto it=required.begin();it!=required.end();++it)if(it.key()!="GLIBC"&&it.key()!="GLIBCXX"&&it.key()!="CXXABI")return r;
    }
    r.state=UpdateResult::Newer;r.message=tr("A newer release has a matching package for this build and runtime.");r.packageName=name;r.checksum=sha;return r;
}
QUrl UpdateChecker::endpoint(){return QUrl("https://api.github.com/repos/RFStateSide/RayNmeaSimulator/releases/latest");}
UpdateChecker::UpdateChecker(QObject *parent,QNetworkAccessManager *transport,int timeout):QObject(parent),network(transport?transport:new QNetworkAccessManager(this)),timeoutMs(timeout){
    qRegisterMetaType<UpdateResult>();deadline.setSingleShot(true);
    connect(&deadline,&QTimer::timeout,this,[this]{UpdateResult r;r.installed=QStringLiteral(APP_VERSION);r.message=tr("The update check timed out. Check your connection and try again.");finish(r);});
}
UpdateChecker::~UpdateChecker(){cancel();}
void UpdateChecker::cancel(){deadline.stop();if(reply){auto *p=reply.data();reply=nullptr;p->disconnect(this);p->abort();p->deleteLater();}}
void UpdateChecker::finish(UpdateResult r){cancel();emit completed(r);}
void UpdateChecker::check(){if(busy())return;metadata=false;redirects=0;result={};result.installed=QStringLiteral(APP_VERSION);deadline.start(timeoutMs);request(endpoint());}
void UpdateChecker::request(const QUrl &url){
    buffer.clear();QNetworkRequest request(url);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);request.setRawHeader("Accept",metadata?"application/octet-stream":"application/vnd.github+json");request.setRawHeader("X-GitHub-Api-Version","2022-11-28");request.setHeader(QNetworkRequest::UserAgentHeader,QString("RayNmeaSimulator/%1 UpdateCheck").arg(APP_VERSION));request.setTransferTimeout(timeoutMs);
    reply=network->get(request);reply->setReadBufferSize(64*1024);auto *p=reply.data();
    connect(p,&QIODevice::readyRead,this,[this,p]{if(reply!=p)return;buffer+=p->readAll();if(buffer.size()>(metadata?128:512)*1024){auto r=result;r.state=UpdateResult::Error;r.message=tr("The update service response is too large.");finish(r);}});
    connect(p,&QNetworkReply::finished,this,[this,p]{
        if(reply!=p)return;buffer+=p->readAll();reply=nullptr;p->deleteLater();
        const int status=p->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto fail=[this](const QString &text){auto r=result;r.state=UpdateResult::Error;r.message=text;finish(r);};
        if(buffer.size()>(metadata?128:512)*1024){fail(tr("The update service response is too large."));return;}
        if(status>=300&&status<400){const auto next=p->url().resolved(p->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            const bool allowed=metadata&&cleanUrl(next)&&(next.host()=="release-assets.githubusercontent.com"||next.host()=="objects.githubusercontent.com"||next.host()=="github.com");
            if(!allowed||++redirects>3){fail(tr("The update service returned an unexpected redirect."));return;}this->request(next);return;}
        if(status==404&&!metadata){result.state=UpdateResult::Unavailable;result.message=tr("No public release information is available. The project may be private or have no published releases.");finish(result);return;}
        if(status==403||status==429){fail(tr("GitHub is limiting update checks or access is unavailable. Please try again later; no sign-in is required for public releases."));return;}
        if(p->error()!=QNetworkReply::NoError||status!=200){fail(tr("Could not retrieve release information. Check your connection and try again later."));return;}
        if(!metadata){result=parseRelease(buffer,QStringLiteral(APP_VERSION));if(result.state!=UpdateResult::Newer){finish(result);return;}
            const auto a=asset(result.release,metadataName);const auto u=QUrl(a.value("browser_download_url").toString(),QUrl::StrictMode);
            if(a.value("state").toString()!="uploaded"||a.value("size").toDouble()<=0||a.value("size").toDouble()>128*1024||!officialAsset(u,result.tag,metadataName)){result.state=UpdateResult::Incompatible;result.message=tr("A newer release exists, but compatible package information is not available. Read its release notes before installing.");finish(result);return;}
            metadata=true;redirects=0;this->request(u);
        }else{const auto a=asset(result.release,metadataName);const auto digest=a.value("digest").toString();
            if(a.value("size").toDouble()!=buffer.size()||(!digest.isEmpty()&&digest!="sha256:"+QString::fromLatin1(QCryptographicHash::hash(buffer,QCryptographicHash::Sha256).toHex()))){fail(tr("The release package information failed its integrity check."));return;}
            finish(selectPackage(result,buffer,UpdateHost::current()));}
    });
}
