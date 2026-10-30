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

#include <QtTest>
#include <QApplication>
#include <QDomDocument>
#include <QFile>
#include <QDir>
#include <QAction>
#include <QNetworkProxy>
#include <QTemporaryDir>
#include "appsettings.h"
#include "mainwindow.h"
#include "nmeasenderworker.h"
#include "transportcapabilities.h"

class CMakeParityTests : public QObject {
    Q_OBJECT
private slots:
    void resourcesMatchSources() {
        for (const QString &relative : {"../RayNmeaSimulator.qrc", "../resources/nmea2000/nmea2000.qrc"}) {
            const QString path = QFINDTESTDATA(relative);
            QFile source(path); QVERIFY(source.open(QIODevice::ReadOnly));
            QDomDocument document; QVERIFY(document.setContent(source.readAll()));
            auto groups = document.elementsByTagName("qresource");
            for (int g=0; g<groups.size(); ++g) {
                auto group=groups.at(g).toElement();
                QString prefix=group.attribute("prefix"); if(prefix.endsWith('/'))prefix.chop(1);
                auto files=group.elementsByTagName("file");
                for(int i=0; i<files.size(); ++i) {
                    auto entry=files.at(i).toElement();
                    const QString alias=":"+prefix+"/"+entry.attribute("alias",entry.text());
                    QFile compiled(alias), original(QFileInfo(path).dir().filePath(entry.text()));
                    QVERIFY2(compiled.open(QIODevice::ReadOnly),qPrintable(alias));
                    QVERIFY(original.open(QIODevice::ReadOnly));
                    QCOMPARE(compiled.readAll(),original.readAll());
                }
            }
        }
        const auto templates=QDir(":/templates").entryList({"*.json"},QDir::Files);
        QCOMPARE(templates.size(),6);
        for(const auto &name:templates) {
            QFile compiled(":/templates/"+name);
            QFile source(QFINDTESTDATA("../resources/templates/"+name));
            QVERIFY(compiled.open(QIODevice::ReadOnly)); QVERIFY(source.open(QIODevice::ReadOnly));
            QCOMPARE(compiled.readAll(),source.readAll());
        }
    }
    void configurationPolicyAndIsolation() {
        AppSettings settings("RFStateSide","RayNmeaSimulator");
        QCOMPARE(settings.format(),QSettings::IniFormat);
        QVERIFY(!settings.fallbacksEnabled());
        QVERIFY(settings.fileName().startsWith(qEnvironmentVariable("RAYNMEA_TEST_HOME")));
#ifdef QT_DEBUG
        QVERIFY(TransportCapabilities::experimentalActisenseTx);
#else
        QVERIFY(!TransportCapabilities::experimentalActisenseTx);
#endif
        QVERIFY(TransportCapabilities::canTransmit("Waveshare USB-CAN-A"));
        QVERIFY(TransportCapabilities::canTransmit("CANable SLCAN"));
    }
    void startupDoesNotAuthorizeSending() {
        MainWindow window;
        auto *sender=window.findChild<NmeaSenderWorker*>(); QVERIFY(sender);
        QVERIFY(!sender->isSending());
        auto *stop=window.findChild<QAction*>("actionStop_Sending"); QVERIFY(stop);
        QVERIFY(!stop->isEnabled());
    }
};
int main(int argc,char **argv) {
    QTemporaryDir profile; qputenv("RAYNMEA_TEST_HOME",profile.path().toUtf8());
    QApplication app(argc,argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    CMakeParityTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "cmake_parity_tests.moc"
