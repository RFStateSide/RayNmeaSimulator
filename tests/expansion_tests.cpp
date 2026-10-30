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
#include "N2kMessages.h"
#include "appsettings.h"
#include "expansioninstruments.h"
#include "mainwindow.h"
#include "nmeareceiver.h"
#include <QAbstractButton>
#include <QAction>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QTemporaryDir>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QLabel>
#include <QJsonObject>
#include "enumcomboboxdelegate.h"
#include "clickablelabel.h"
#include "n2khelper.h"
#include "pgntreecontroller.h"
#include "themes.h"
#include <QInputDialog>
#include <QToolButton>
#include "temperaturewidget130312.h"

namespace
{
QMap<QString, QString> fixture(const QString &code)
{
    QMap<QString, QString> f;
    for (const auto &row : Expansion::parameters(code))
        f[row[0]] = row[1];
    const QMap<QString, QString> samples{{"Temperature", "293.15"},
                                         {"Humidity", "55.5"},
                                         {"AtmosphericPressure", "101300"},
                                         {"ActualPressure", "101325.2"},
                                         {"HDOP", "1.23"},
                                         {"VDOP", "2.34"},
                                         {"TDOP", "3.45"},
                                         {"Log", "123456"},
                                         {"TripLog", "1234"},
                                         {"DaysSince1970", "20000"},
                                         {"SecondsSinceMidnight", "43210.1234"},
                                         {"TripFuelUsed", "123"},
                                         {"FuelRateAverage", "12.3"},
                                         {"FuelRateEconomy", "14.5"},
                                         {"InstantaneousFuelEconomy", "13.4"},
                                         {"DepthBelowTransducer", "12.3"},
                                         {"Offset", "-1.2"},
                                         {"Range", "100"},
                                         {"WindSpeed", "4.5"},
                                         {"WindAngle", "1.5707963267948966"},
                                         {"WaterTemperature", "293.15"},
                                         {"TrueHeading", "1.5707963267948966"},
                                         {"MagneticHeading", "1.3962634015954636"},
                                         {"WaterSpeed", "3"}};
    for (auto it = f.begin(); it != f.end(); ++it)
        if (samples.contains(it.key()))
            it.value() = samples[it.key()];
    if (code == "129540")
        f["Satellites"] = "[{\"PRN\":12,\"Elevation\":0.5,\"Azimuth\":1.2,\"SNR\":35.5,\"RangeResiduals\":0.12345,"
                          "\"UsageStatus\":2}]";
    if (code == "GSV")
    {
        f["SatelliteCount"] = "1";
        f["Satellites"] = "[{\"PRN\":12,\"Elevation\":40,\"Azimuth\":83,\"SNR\":35}]";
    }
    return f;
}
QStringList pairs(const QMap<QString, QString> &f)
{
    QStringList out;
    for (auto i = f.begin(); i != f.end(); ++i)
        out << i.key() << i.value();
    return out;
}
QStringList wire(const QMap<QString, QString> &f)
{
    QStringList out;
    for (auto i = f.begin(); i != f.end(); ++i)
        out << i.key() + ":" + i.value();
    return out;
}
QMap<QString, QString> unpack(const QStringList &p)
{
    QMap<QString, QString> f;
    for (int i = 0; i + 1 < p.size(); i += 2)
        f[p[i]] = p[i + 1];
    return f;
}
class Port : public QSerialPort
{
  public:
    QByteArray output, input;
    bool open(OpenMode mode) override
    {
        return QIODevice::open(mode | QIODevice::Unbuffered);
    }
    void close() override
    {
        QIODevice::close();
    }
    qint64 bytesToWrite() const override
    {
        return 0;
    }
    qint64 bytesAvailable() const override
    {
        return input.size() + QIODevice::bytesAvailable();
    }
    void feed(const QByteArray &bytes)
    {
        input += bytes;
        emit readyRead();
    }

  protected:
    qint64 writeData(const char *p, qint64 n) override
    {
        output.append(p, n);
        return n;
    }
    qint64 readData(char *p, qint64 n) override
    {
        n = qMin(n, qint64(input.size()));
        memcpy(p, input.constData(), n);
        input.remove(0, n);
        return n;
    }
};
void settle()
{
    for (int i = 0; i < 5; ++i)
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
}
struct Dialogs
{
    QTimer timer;
    Dialogs()
    {
        QObject::connect(&timer, &QTimer::timeout, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box)
                return;
            if (box->objectName() == "viewerEntryDialog")
                box->findChild<QAbstractButton *>("viewerKeepExisting")->click();
            else if (box->windowTitle() == "Widgets are Loaded")
                box->button(QMessageBox::Yes)->click();
            else if (box->objectName() == "layoutOperationDialog")
                box->button(box->standardButtons().testFlag(QMessageBox::Discard) ? QMessageBox::Discard
                                                                                  : QMessageBox::Yes)
                    ->click();
            else if (auto *button = box->button(QMessageBox::No))
                button->click();
            else
                box->reject();
        });
        timer.start(0);
    }
};
struct Workspace
{
    Dialogs dialogs;
    MainWindow window;
    MainController *c = window.findChild<MainController *>();
    PgnTableModel *model = window.findChild<PgnTableModel *>();
    ExpansionInstrument *add(const QString &code)
    {
        const auto *d = Expansion::definition(code);
        c->addWidgetAsComboboxTextChanged(code, d->label, d->group);
        settle();
        return qobject_cast<ExpansionInstrument *>(c->layoutWidgets(d->group).last());
    }
};
} // namespace
class ExpansionTests : public QObject
{
    Q_OBJECT
  private slots:
    void init()
    {
        AppSettings s(APP_COMPANY, APP_NAME);
        s.clear();
        s.setValue("Nmea2000/Nmea2000Enabled", false);
        s.setValue("Nmea0183/Nmea0183Enabled", false);
        s.sync();
    }
    void boundedPayloadRejection()
    {
        auto *serial = new Port;
        Nmea0183Handler sentences(nullptr, serial);
        QVERIFY(sentences.Initialize("MEMORY"));
        QVERIFY(sentences.OpenDevice());
        auto f = fixture("RMB");
        f["OriginID"] = QString(16, 'A');
        f["DestinationID"] = QString(16, 'B');
        f["Latitude"] = "89.123456789";
        f["Longitude"] = "179.123456789";
        f["XTE"] = "185200";
        f["DistanceToWaypoint"] = "185200000";
        f["BearingToWaypoint"] = "6.2";
        f["ClosingVelocity"] = "999";
        QSignalSpy errors(&sentences, &Nmea0183Handler::TransmitError);
        QVERIFY(!sentences.sendSnt("RMB", wire(f)));
        QCOMPARE(errors.size(), 1);
        QVERIFY(serial->output.isEmpty());
        auto *port = new Port;
        Nmea2000Handler can(nullptr, port);
        can.SetInterfaceType("Actisense");
        QVERIFY(can.InitializeActisense("MEMORY", QSerialPort::Baud115200, "0x93"));
        f = fixture("129285");
        QJsonArray points;
        for (int i = 0; i < 18; ++i)
            points.append(
                QJsonObject{{"ID", i}, {"Name", QString(16, 'X')}, {"Latitude", 10.0 + i}, {"Longitude", 20.0 + i}});
        f["Waypoints"] = QString::fromUtf8(QJsonDocument(points).toJson(QJsonDocument::Compact));
        const auto before = port->output;
        QSignalSpy failures(&can, &Nmea2000Handler::TransmitError);
        QVERIFY(!can.sendPgn(129285, wire(f)));
        QCOMPARE(failures.size(), 1);
        QCOMPARE(port->output, before);
    }
    void navigationIndependentSentences()
    {
        const QStringList sentences{QString("$GPAPB,A,A,0.10,R,N,V,V,011,M,D,011,M,011,M*7E"),
                                    QString("$GPRMB,A,0.10,R,O,D,4917.16,N,12310.64,W,1.3,52.5,0.5,V*0A"),
                                    QString("$GPRTE,2,1,c,route,A*20"), QString("$GPWPL,4917.16,N,12310.64,W,A*17")};
        for (const auto &text : sentences)
        {
            tNMEA0183Msg message;
            QVERIFY(message.SetMessage(text.toLatin1().constData()));
            QStringList fields;
            QVERIFY(Expansion::decodeSentence(message, fields));
            const QString code = QString::fromLatin1(message.MessageCode());
            const auto values = unpack(fields);
            tNMEA0183Msg encoded;
            QVERIFY(Expansion::encodeSentence(code, values, encoded));
            QStringList back;
            QVERIFY(Expansion::decodeSentence(encoded, back));
            if (code == "APB")
            {
                QCOMPARE(values["SteerDirection"], QString("R"));
                QVERIFY(qAbs(values["XTE"].toDouble() - 185.2) < 1e-9);
                QCOMPARE(values["OriginReference"], QString("M"));
            }
            if (code == "RMB" || code == "WPL")
            {
                QVERIFY(qAbs(values["Latitude"].toDouble() - (49 + 17.16 / 60)) < 1e-10);
                QVERIFY(qAbs(unpack(back)["Longitude"].toDouble() - values["Longitude"].toDouble()) < 1e-8);
            }
        }
    }
    void navigationSourceBindings_data()
    {
        sourceOrdersAndLifecycle_data();
    }
    void navigationSourceBindings()
    {
        QFETCH(bool, sourceFirst);
        Workspace w;
        auto create = [&] {
            w.c->addGeneratorWidget("Vessel Movement (PGN -129025)");
            settle();
        };
        if (sourceFirst)
            create();
        auto *apb = w.add("APB");
        auto *rmb = w.add("RMB");
        if (!sourceFirst)
            create();
        auto *source = w.window.findChild<GeneratorVesselMovement *>();
        QVERIFY(source);
        source->setEnabled(true);
        apb->setValue("PositionReference", "M");
        apb->setValue("PositionBearing", "2");
        source->setBearingReference(N2khr_true);
        source->setBearingPositionToDestinationWaypoint(1.25);
        source->setDestinationLatitude(41.1234567890123);
        source->setArrivalCircleEntered(true);
        settle();
        QCOMPARE(rmb->value("BearingToWaypoint").toDouble(), 1.25);
        QCOMPARE(apb->value("PositionBearing"), QString("2"));
        QCOMPARE(rmb->value("Latitude").toDouble(), 41.1234567890123);
        QCOMPARE(rmb->value("ArrivalAlarm"), QString("A"));
        QSignalSpy updates(rmb, &ExpansionInstrument::parameterValueChanged);
        source->setDistanceToWaypoint(123.456);
        settle();
        QCOMPARE(rmb->value("DistanceToWaypoint").toDouble(), 123.456);
        QCOMPARE(updates.size(), 1);
        source->setEnabled(false);
        source->setDistanceToWaypoint(999);
        settle();
        QCOMPARE(rmb->value("DistanceToWaypoint").toDouble(), 123.456);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void structuredWaypoints()
    {
        RouteInfo129285 widget(1, "Route", nullptr);
        const auto original =
            QString("[{\"ID\":10,\"Name\":\"A\",\"Latitude\":41.1234567890123,\"Longitude\":-70.1234567890123}]");
        widget.setValue("Waypoints", original);
        bool entered = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            auto *table = dialog->findChild<QTableWidget *>("waypointRecords");
            QVERIFY(table);
            table->editItem(table->item(0, 1));
            auto *editor = table->findChild<QLineEdit *>();
            QVERIFY(editor);
            editor->setFocus();
            QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClicks(editor, "Updated");
            QTest::keyClick(editor, Qt::Key_Return);
            QTest::qWait(10);
            QVERIFY(dialog->isVisible());
            QCOMPARE(table->item(0, 1)->text(), QString("Updated"));
            entered = true;
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        widget.findChild<QPushButton *>("editWaypoints")->click();
        QVERIFY(entered);
        const auto point = QJsonDocument::fromJson(widget.value("Waypoints").toUtf8()).array().first().toObject();
        QCOMPARE(point["Name"].toString(), QString("Updated"));
        QCOMPARE(point["Latitude"].toDouble(), 41.1234567890123);
    }
    void nativePassiveRouteFixtures()
    {
        const auto load = [](unsigned long pgn, const QByteArray &bytes) {
            tN2kMsg m;
            m.SetPGN(pgn);
            m.AddBuf(bytes.constData(), bytes.size());
            return m;
        };
        const auto attributes = load(130066, QByteArray::fromHex("0100020003015264000000e80300020000040000"));
        const auto geometry = load(
            130067, QByteArray::fromHex("000002000200010002000a0003014100e1f50500c2eb0b0b0003014280778e068058840c"));
        const auto route =
            load(129285,
                 QByteArray::fromHex("0000020001000200e0030152ff0a0003014100e1f50500c2eb0b0b0003014280778e068058840c"));
        QStringList a, g, r;
        QVERIFY(Expansion::decodeRouteAttributes(attributes, a));
        QVERIFY(Expansion::decodeRouteGeometry(geometry, g));
        QVERIFY(Expansion::decode(route, r));
        ReceivedRoutes cache;
        cache.receive("130067", unpack(g), "23", 1000);
        QVERIFY(!cache.collection(1000).first().complete);
        cache.receive("130066", unpack(a), "24", 1000); // Other source cannot complete source 23.
        for (const auto &entry : cache.collection(1000))
            QVERIFY(!entry.complete);
        cache.receive("130066", unpack(a), "23", 1000);
        bool complete = false;
        for (const auto &entry : cache.collection(1000))
            if (entry.source == "23")
            {
                complete = entry.complete;
                QCOMPARE(entry.waypoints.size(), 2);
                QCOMPARE(entry.waypoints.first().coordinate.latitude(), 10.0);
            }
        QVERIFY(complete);
        cache.observeCanIdentity("23", "0000000000000001");
        for (const auto &entry : cache.collection(1000))
            QVERIFY(!entry.complete);
        cache.receive("130066", unpack(a), "23", 1000);
        cache.receive("130067", unpack(g), "23", 1000);
        cache.observeCanIdentity("23", "0000000000000001");
        complete = false;
        for (const auto &entry : cache.collection(1000))
            if (entry.source == "23")
                complete = entry.complete;
        QVERIFY(complete);
        cache.observeCanIdentity("23", "0000000000000002");
        for (const auto &entry : cache.collection(1000))
            QVERIFY(!entry.complete);
        cache.receive("130066", unpack(a), "23", 1000);
        cache.receive("130067", unpack(g), "23", 1000);
        auto required = unpack(a);
        required["CriticalParameters"] = "2";
        cache.receive("130066", required, "23", 1001);
        cache.receive("130067", unpack(g), "23", 1001);
        for (const auto &entry : cache.collection(1001))
            QVERIFY(!entry.complete); // Do not discard required geometry.
        auto bad = geometry;
        bad.DataLen--;
        QVERIFY(!Expansion::decodeRouteGeometry(bad, g));
        bad = attributes;
        bad.DataLen--;
        QVERIFY(!Expansion::decodeRouteAttributes(bad, a));
        auto fields = unpack(r);
        tN2kMsg tx;
        QVERIFY(Expansion::encode("129285", fields, tx));
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(tx.Data), tx.DataLen),
                 QByteArray::fromHex("0000020001000200e0030152ff0a0003014100e1f50500c2eb0b0b0003014280778e068058840c"));
        bad = route;
        bad.Data[2] = 18;
        QVERIFY(!Expansion::decode(bad, r));
    }
    void nativeRouteVersionsAndLoops()
    {
        ReceivedRoutes cache;
        QMap<QString, QString> metadata{{"DatabaseID", "1"},           {"RouteID", "2"},
                                        {"RouteName", "Loop"},         {"Datestamp", "1000"},
                                        {"Timestamp", "100"},          {"WaypointCount", "3"},
                                        {"CriticalParameters", "0"},   {"NavigationMethod", "0"},
                                        {"IdentificationMethod", "1"}, {"RouteStatus", "1"}};
        QJsonObject a{{"ID", 10}, {"Name", "A"}, {"Latitude", 10}, {"Longitude", 20}};
        QJsonObject b{{"ID", 11}, {"Name", "B"}, {"Latitude", 11}, {"Longitude", 21}};
        auto geometry = metadata;
        geometry["StartRPS"] = "0";
        geometry["GeometryPGN"] = "130067";
        geometry["TotalWaypoints"] = "3";
        const auto serialize = [](QJsonArray points) {
            return QString::fromUtf8(QJsonDocument(points).toJson(QJsonDocument::Compact));
        };
        geometry["Waypoints"] = serialize({a, b, a});
        cache.receive("130066", metadata, "23", 1000);
        cache.receive("130067", geometry, "23", 1000);
        QVERIFY(cache.collection(1000).first().complete);
        QCOMPARE(cache.collection(1000).first().waypoints.size(), 3);
        auto changed = a;
        changed["Latitude"] = 12;
        geometry["Waypoints"] = serialize({a, b, changed});
        cache.receive("130067", geometry, "23", 1001);
        QVERIFY(!cache.collection(1001).first().complete);
        geometry["Waypoints"] = serialize({a, b, a});
        cache.receive("130067", geometry, "23", 1002);
        QVERIFY(!cache.collection(1002).first().complete); // Same-version conflict stays quarantined.
        metadata["Timestamp"] = "101";
        cache.receive("130066", metadata, "23", 1003);
        cache.receive("130067", geometry, "23", 1003);
        QVERIFY(cache.collection(1003).first().complete);
        auto older = metadata;
        older["Timestamp"] = "99";
        older["RouteName"] = "Stale";
        cache.receive("130066", older, "23", 1004);
        QCOMPARE(cache.collection(1004).first().name, QString("Loop"));
        QVERIFY(cache.collection(1004).first().complete);
        QVERIFY(!cache.collection(31004).first().complete);
    }
    void waypointEditorTransactions()
    {
        WaypointList130074 widget(1, "Catalog", nullptr);
        widget.setValue("TotalWaypoints", "65535");
        const auto original = widget.value("Waypoints");
        for (int operation = 0; operation < 3; ++operation)
        {
            bool checked = false;
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                QTimer::singleShot(3000, dialog, &QDialog::reject);
                auto *table = dialog->findChild<QTableWidget *>("waypointRecords");
                auto *add = dialog->findChild<QPushButton *>("addWaypoint");
                auto *remove = dialog->findChild<QPushButton *>("removeWaypoint");
                auto *buttons = dialog->findChild<QDialogButtonBox *>();
                QVERIFY(table && add && remove && buttons);
                if (operation == 0)
                {
                    for (int n = 0; n < 18; ++n)
                        add->click();
                    QCOMPARE(table->rowCount(), 18);
                    QVERIFY(!add->isEnabled());
                    remove->click();
                    QCOMPARE(table->rowCount(), 17);
                    dialog->reject();
                }
                else if (operation == 1)
                {
                    add->click();
                    table->item(0, 2)->setText("91");
                    buttons->button(QDialogButtonBox::Ok)->click();
                    QVERIFY(dialog->isVisible());
                    table->item(0, 2)->setText("10");
                    table->item(0, 3)->setText("20");
                    dialog->findChild<QSpinBox *>("totalDatabaseWaypoints")->setValue(5);
                    buttons->button(QDialogButtonBox::Ok)->click();
                }
                else
                    buttons->button(QDialogButtonBox::Ok)->click();
                checked = true;
            });
            const auto before = widget.value("Waypoints");
            widget.findChild<QPushButton *>("editWaypoints")->click();
            QVERIFY(checked);
            if (operation == 0)
                QCOMPARE(widget.value("Waypoints"), original);
            if (operation == 1)
            {
                QCOMPARE(widget.value("TotalWaypoints"), QString("5"));
                QCOMPARE(QJsonDocument::fromJson(widget.value("Waypoints").toUtf8()).array().size(), 1);
            }
            if (operation == 2)
                QCOMPARE(widget.value("Waypoints"), before);
        }
    }
    void routeIdentifierEditorTransactions()
    {
        RouteRTE widget(1, "Route", nullptr);
        widget.setValue("WaypointIDs", "[\"A\"]");
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            bool checked = false;
            QTimer::singleShot(0, [&] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                QTimer::singleShot(3000, dialog, &QDialog::reject);
                auto *table = dialog->findChild<QTableWidget *>("routeIDsTable");
                QVERIFY(table);
                QPushButton *add = nullptr, *remove = nullptr;
                for (auto *button : dialog->findChildren<QPushButton *>())
                {
                    if (button->text() == "Add Record")
                        add = button;
                    if (button->text() == "Remove Record")
                        remove = button;
                }
                QVERIFY(add && remove);
                if (attempt == 0)
                {
                    for (int n = 1; n < 12; ++n)
                        add->click();
                    QCOMPARE(table->rowCount(), 12);
                    QVERIFY(!add->isEnabled());
                    remove->click();
                    QCOMPARE(table->rowCount(), 11);
                    dialog->reject();
                }
                else
                {
                    table->editItem(table->item(0, 0));
                    auto *editor = table->findChild<QLineEdit *>();
                    QVERIFY(editor);
                    editor->setFocus();
                    QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
                    QTest::keyClicks(editor, "Revised");
                    QTest::keyClick(editor, Qt::Key_Return);
                    QTest::qWait(10);
                    QVERIFY(dialog->isVisible());
                    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
                }
                checked = true;
            });
            widget.findChild<QPushButton *>("editWaypointIDs")->click();
            QVERIFY(checked);
            QCOMPARE(widget.value("WaypointIDs"), attempt ? QString("[\"Revised\"]") : QString("[\"A\"]"));
        }
    }
    void passiveRoutes()
    {
        ReceivedRoutes cache;
        QMap<QString, QString> a{{"WaypointName", "A"}, {"Latitude", "10"}, {"Longitude", "20"}};
        auto b = a;
        b["WaypointName"] = "B";
        b["Latitude"] = "11";
        QMap<QString, QString> page{{"RouteName", "test"},
                                    {"RouteType", "c"},
                                    {"TotalMessages", "2"},
                                    {"MessageNumber", "2"},
                                    {"WaypointIDs", "[\"B\"]"}};
        cache.receive("RTE", page, "GP", 1000);
        QVERIFY(!cache.collection(1000).first().complete);
        cache.receive("WPL", a, "GP", 1000);
        cache.receive("WPL", b, "II", 1000);
        page["MessageNumber"] = "1";
        page["WaypointIDs"] = "[\"A\"]";
        cache.receive("RTE", page, "GP", 1000);
        QVERIFY(!cache.collection(1000).first().complete); // Never mix talkers.
        cache.receive("WPL", b, "GP", 1000);
        QVERIFY(cache.collection(1000).first().complete);
        QCOMPARE(cache.collection(1000).first().waypoints.size(), 2);
        cache.receive("RTE", page, "GP", 1001); // Exact duplicate retains completeness.
        QVERIFY(cache.collection(1001).first().complete);
        const auto snapshot = cache.collection(1001).first().waypoints;
        b["Latitude"] = "12";
        cache.receive("WPL", b, "GP", 1002);
        QVERIFY(!cache.collection(1002).first().complete); // Changed unversioned coordinates invalidate pages.
        QCOMPARE(snapshot.last().coordinate.latitude(), 11.0);
        QVERIFY(!cache.collection(40000).first().complete);
        cache.receive("RTE", page, "GP", 40000);
        QCOMPARE(cache.collection(40000).size(), 1);
        AutoPilotSimulator pilot;
        QVERIFY(pilot.loadRoute(snapshot));
        QVERIFY(!pilot.isRunning());
        QSignalSpy states(&pilot, &AutoPilotSimulator::RunningStateChanged);
        QVERIFY(!pilot.loadRoute(QVector<Waypoint>{}));
        QCOMPARE(states.size(), 0);
    }
    void passiveRouteSelection()
    {
        GeneratorVesselMovement widget(1, "Movement", nullptr);
        const auto before = widget.layoutConfiguration();
        const QVector<Waypoint> points{{QGeoCoordinate(10, 20), "A", 3}, {QGeoCoordinate(11, 21), "B", 3}};
        widget.setReceivedRouteProvider([points] {
            ReceivedRoutes::Route route;
            route.complete = true;
            route.name = "Route";
            route.source = "GP";
            route.waypoints = points;
            return QVector<ReceivedRoutes::Route>{route};
        });
        QSignalSpy started(&widget, &GeneratorVesselMovement::UseGenVessel);
        bool chosen = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            dialog->setTextValue("Select Chartplotter Route");
            dialog->accept();
            QTimer::singleShot(0, [&] {
                auto *second = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                if (second)
                {
                    chosen = true;
                    second->accept();
                }
            });
        });
        QVERIFY(QMetaObject::invokeMethod(&widget, "on_pushButtonSelectRouteFile_clicked"));
        QVERIFY(chosen);
        QCOMPARE(started.size(), 0);
        QCOMPARE(widget.layoutConfiguration(), before);
        auto *label = widget.findChild<QLabel *>("receivedRouteStatus");
        QVERIFY(label && !label->isHidden());
        const auto selectedLabel = label->text();
        bool fresh = true;
        widget.setReceivedRouteProvider([&] {
            ReceivedRoutes::Route route;
            route.complete = fresh;
            route.name = "Replacement";
            route.id = "replacement";
            route.source = "GP";
            route.waypoints = points;
            return QVector<ReceivedRoutes::Route>{route};
        });
        Dialogs dismissWarning;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            dialog->setTextValue("Select Chartplotter Route");
            dialog->accept();
            QTimer::singleShot(0, [&] {
                auto *second = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                if (second)
                {
                    fresh = false;
                    second->accept();
                }
            });
        });
        QVERIFY(QMetaObject::invokeMethod(&widget, "on_pushButtonSelectRouteFile_clicked"));
        QVERIFY(!fresh);
        QCOMPARE(label->text(), selectedLabel); // Expiry while choosing preserves previous snapshot.
        QCOMPARE(started.size(), 0);
        widget.restoreLayoutConfiguration(before);
        QVERIFY(label->isHidden());
    }
    void chartplotterPickerStopAccess_data()
    {
        QTest::addColumn<bool>("keyboard");
        QTest::newRow("F12") << true;
        QTest::newRow("button") << false;
    }
    void chartplotterPickerStopAccess()
    {
        QFETCH(bool, keyboard);
        AppSettings settings(APP_COMPANY, APP_NAME);
        settings.setValue("Nmea2000/InterfaceType", "None");
        settings.setValue("Nmea0183/Nmea0183Enabled", true);
        settings.setValue("Nmea0183/Nmea0183Port", "MEMORY");
        settings.sync();
        MainWindow window(nullptr, nullptr, new Port);
        window.show();
        auto *model = window.findChild<PgnTableModel *>();
        model->addPgn(99001, "Fixture", "HDT", true, {{"Heading", "1", "rad", "double"}}, {}, 250);
        auto *start = window.findChild<QAction *>("actionStart_Sending");
        auto *worker = window.findChild<NmeaSenderWorker *>();
        start->trigger();
        QVERIFY(worker->isSending());
        GeneratorVesselMovement widget(1, "Movement", &window);
        widget.setReceivedRouteProvider([] {
            ReceivedRoutes::Route route;
            route.complete = true;
            route.id = "fixture";
            route.name = "Fixture";
            route.source = "GP";
            route.waypoints = {{QGeoCoordinate(10, 20), "A", 3}, {QGeoCoordinate(11, 21), "B", 3}};
            return QVector<ReceivedRoutes::Route>{route};
        });
        bool seen = false;
        QTimer::singleShot(20, [&] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            auto *stop = dialog->findChild<QToolButton *>("applicationStopSending");
            QVERIFY(stop);
            if (keyboard)
                QTest::keyClick(dialog, Qt::Key_F12);
            else
                stop->click();
            QVERIFY(!worker->isSending());
            seen = true;
            dialog->reject();
        });
        QVERIFY(QMetaObject::invokeMethod(&widget, "on_pushButtonSelectRouteFile_clicked"));
        QVERIFY(seen);
        QVERIFY(!worker->isSending());
        QVERIFY(widget.findChild<QLabel *>("receivedRouteStatus")->isHidden());
    }
    void remainingIndependentWire()
    {
        tN2kMsg m;
        m.SetPGN(127498);
        const unsigned char raw[] = {1, 0xa0, 0x0f, 5, 1, 'A', 'B', 'C', 3, 1, 'S'};
        m.AddBuf(raw, sizeof(raw));
        QStringList decoded;
        QVERIFY(Expansion::decode(m, decoded));
        auto f = unpack(decoded);
        QCOMPARE(f["EngineInstance"], QString("1"));
        QCOMPARE(f["RatedEngineSpeed"].toDouble(), 1000.0);
        QCOMPARE(f["VIN"], QString("ABC"));
        tN2kMsg out;
        QVERIFY(Expansion::encode("127498", f, out));
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(out.Data), out.DataLen),
                 QByteArray(reinterpret_cast<const char *>(raw), sizeof(raw)));
        m.Data[3] = 50;
        QVERIFY(!Expansion::decode(m, decoded));
        m.SetPGN(127509);
        const unsigned char inv[] = {0, 1, 2, 0xd1};
        m.AddBuf(inv, sizeof(inv));
        QVERIFY(Expansion::decode(m, decoded));
        f = unpack(decoded);
        QCOMPARE(f["OperatingState"], QString("1"));
        QCOMPARE(f["InverterEnable"], QString("1"));
        QVERIFY(Expansion::encode("127509", f, out));
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(out.Data), out.DataLen),
                 QByteArray(reinterpret_cast<const char *>(inv), sizeof(inv)));
        m.DataLen = 3;
        QVERIFY(!Expansion::decode(m, decoded));
        tNMEA0183Msg gsa;
        QVERIFY(gsa.SetMessage("$GNGSA,A,3,21,5,29,25,12,10,26,2,,,,,1.2,0.7,1.0*27"));
        QVERIFY(Expansion::decodeSentence(gsa, decoded));
        f = unpack(decoded);
        QCOMPARE(f["Satellite1"], QString("21"));
        QCOMPARE(f["Satellite9"], QString("255"));
        QCOMPARE(f["HDOP"].toDouble(), 0.7);
        QVERIFY(!Expansion::validField("GSA", "Satellite1", "1.5"));
        QVERIFY(!Expansion::validField("GSA", "Satellite1", "100"));
    }
    void vesselTripIndependentWire()
    {
        // Independently specified little-endian raw values: 1000 ms, 12345 cm,
        // 42 litres and 2000 ms. Do not generate the RX fixture with our encoder.
        const unsigned char bytes[] = {0xe8, 3, 0, 0, 0x39, 0x30, 0, 0, 42, 0, 0xd0, 7, 0, 0};
        tN2kMsg input;
        input.SetPGN(127496);
        input.AddBuf(bytes, sizeof(bytes));
        QStringList fields;
        QVERIFY(Expansion::decode(input, fields));
        QMap<QString, QString> values;
        for (int i = 0; i < fields.size(); i += 2)
            values[fields[i]] = fields[i + 1];
        QCOMPARE(values["TimeToEmpty"].toDouble(), 1.0);
        QCOMPARE(values["DistanceToEmpty"].toDouble(), 123.45);
        QCOMPARE(values["EstimatedFuelRemaining"].toDouble(), 42.0);
        QCOMPARE(values["TripRunTime"].toDouble(), 2.0);
        tN2kMsg output;
        QVERIFY(Expansion::encode("127496", values, output));
        QCOMPARE(output.DataLen, 14);
        QCOMPARE(output.Priority, 5);
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(output.Data), 14),
                 QByteArray(reinterpret_cast<const char *>(bytes), 14));
        input.DataLen = 13;
        QVERIFY(!Expansion::decode(input, fields));
    }
    void supportedMessages_data()
    {
        QTest::addColumn<QString>("code");
        for (const auto &d : Expansion::definitions())
            QTest::newRow(qPrintable(d.code)) << d.code;
    }
    void supportedMessages()
    {
        QFETCH(QString, code);
        const auto f = fixture(code);
        QStringList decoded;
        QVERIFY(Expansion::validParameters(code, pairs(f)));
        if (code.size() != 3)
        {
            tN2kMsg m;
            QVERIFY(Expansion::encode(code, f, m));
            QVERIFY(Expansion::decode(m, decoded));
            Nmea2000Handler handler;
            handler.setMode(Viewer);
            QSignalSpy rx(&handler, &Nmea2000Handler::RcvdPgn);
            m.Source = 23;
            handler.nmea2000MessageReceived(m);
            QCOMPARE(rx.size(), 1);
            QCOMPARE(rx[0][0].toString(), code);
            QCOMPARE(rx[0][2].toString(), QString("23"));
            auto damaged = m;
            damaged.DataLen--;
            QStringList sentinel{"unchanged"};
            QVERIFY(!Expansion::decode(damaged, sentinel));
            QCOMPARE(sentinel, QStringList{"unchanged"});
            if (code == "130311")
                QCOMPARE(QByteArray(reinterpret_cast<const char *>(m.Data), m.DataLen).toHex(),
                         QByteArray("004183723336f503"));
            if (code == "129539")
                QCOMPARE(QByteArray(reinterpret_cast<const char *>(m.Data), m.DataLen).toHex(),
                         QByteArray("00c07b00ea005901"));
            if (code == "129540")
            {
                QCOMPARE(m.DataLen, 15);
                QCOMPARE(m.Data[2], quint8(1));
            }
            auto *p = new Port;
            Nmea2000Handler sender(nullptr, p);
            sender.SetInterfaceType("Actisense");
            QVERIFY(sender.InitializeActisense("MEMORY", QSerialPort::Baud115200, "0x93"));
            QVERIFY(sender.sendPgn(code.toInt(), wire(f)));
            QVERIFY(!p->output.isEmpty());
            sender.setMode(Viewer);
            const auto before = p->output;
            QVERIFY(!sender.sendPgn(code.toInt(), wire(f)));
            QCOMPARE(p->output, before);
        }
        else
        {
            auto *p = new Port;
            Nmea0183Handler h(nullptr, p);
            QVERIFY(h.Initialize("MEMORY"));
            QVERIFY(h.OpenDevice());
            QVERIFY(h.sendSnt(code, wire(f)));
            QVERIFY(p->output.endsWith("\r\n"));
            if (code == "MTW")
                QCOMPARE(p->output.split(',')[1].toDouble(), 20.);
            if (code == "MWV")
            {
                QCOMPARE(p->output.split(',')[1].toDouble(), 90.);
                QCOMPARE(p->output.split(',')[3].toDouble(), 4.5);
            }
            QSignalSpy rx(&h, &Nmea0183Handler::RcvdSnt);
            h.setMode(Viewer);
            p->feed(p->output);
            QCOMPARE(rx.size(), 1);
            decoded = rx[0][1].toStringList();
            QCOMPARE(rx[0][0].toString(), code);
            QCOMPARE(rx[0][2].toString(), QString::fromLatin1(p->output.mid(1, 2)));
            const auto before = p->output;
            QVERIFY(!h.sendSnt(code, wire(f)));
            QCOMPARE(p->output, before);
        }
        const auto values = unpack(decoded);
        QCOMPARE(values.size(), f.size());
        for (const auto &field : Expansion::definition(code)->fields)
        {
            if (field.type == "QString")
                continue;
            const double tolerance = code == "130311" && field.name == "AtmosphericPressure" ? 51
                                     : field.unit == "m/s"                                   ? 0.01
                                     : field.unit == "rad"                                   ? 0.0002
                                                                                             : 0.011;
            QVERIFY2(qAbs(values[field.name].toDouble() - f[field.name].toDouble()) < tolerance,
                     qPrintable(code + " " + field.name));
        }
    }
    void viewerIdentityAndLayout_data()
    {
        supportedMessages_data();
    }
    void viewerIdentityAndLayout()
    {
        QFETCH(QString, code);
        Workspace w;
        Nmea2000Handler can;
        Nmea0183Handler serial;
        NmeaReceiver receiver(w.c, w.window.findChild<PgnTableController *>(), &can, &serial);
        w.c->setMode(Viewer);
        const auto f = fixture(code);
        const auto source = code.size() == 3 ? "GP" : "23";
        receiver.processNmeaPgnSnt(code, pairs(f), source);
        settle();
        QCOMPARE(w.model->getPgns(code).size(), 1);
        QStringList reordered;
        for (auto i = f.begin(); i != f.end(); ++i)
            reordered = QStringList{i.key(), i.value()} + reordered;
        receiver.processNmeaPgnSnt(code, reordered, source);
        settle();
        QCOMPARE(w.model->getPgns(code).size(), 1);
        receiver.processNmeaPgnSnt(code, pairs(f), code.size() == 3 ? "II" : "24");
        settle();
        QCOMPARE(w.model->getPgns(code).size(), 2);
        const auto *d = Expansion::definition(code);
        if (!d->identity.isEmpty())
        {
            auto other = f;
            const auto key = d->identity.first();
            if (key == "Reference")
                other[key] = "R";
            else if (key == "MessageNumber")
            {
                other["TotalMessages"] = "2";
                other["MessageNumber"] = "2";
                other["SatelliteCount"] = "5";
            }
            else
                other[key] = QString::number(other[key].toInt() + 1);
            receiver.processNmeaPgnSnt(code, pairs(other), source);
            settle();
            QCOMPARE(w.model->getPgns(code).size(), 3);
        }
        QVERIFY(w.c->layoutWidgets("Generate").isEmpty());
        QTemporaryDir dir;
        const auto file = dir.filePath("expansion.json");
        QVERIFY(w.c->saveLayoutToJson(file));
        QVERIFY(w.c->loadLayoutFromJson(file));
        settle();
        QCOMPARE(w.model->getPgns(code).size(), d->identity.isEmpty() ? 2 : 3);
        for (const auto &row : w.model->getPgns(code))
        {
            QVERIFY(row.source.isEmpty());
            QVERIFY(row.enabled);
            QCOMPARE(row.sendInterval, 1000.);
        }
        auto *instrument = w.window.findChild<ExpansionInstrument *>();
        QVERIFY(instrument);
        QCOMPARE(instrument->Pgn(), d->id);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void unavailableAndInvalid()
    {
        for (const auto &d : Expansion::definitions())
        {
            auto f = fixture(d.code);
            const auto before = f;
            f["unknown"] = "5";
            QVERIFY(!Expansion::validParameters(d.code, pairs(f)));
            f = before;
            for (const auto &field : d.fields)
                if (field.type != "QString")
                {
                    QVERIFY(!Expansion::validField(d.code, field.name, "nan"));
                    QVERIFY(!Expansion::validField(d.code, field.name, "inf"));
                    QVERIFY(!Expansion::validField(d.code, field.name, QString::number(field.maximum + 1, 'g', 17)));
                    if (field.type == "double")
                        f[field.name] = QString::number(N2kDoubleNA, 'g', 17);
                }
            QStringList out;
            if (d.code.size() != 3)
            {
                tN2kMsg m;
                QVERIFY(Expansion::encode(d.code, f, m));
                QVERIFY(Expansion::decode(m, out));
            }
            else
            {
                tNMEA0183Msg m;
                QVERIFY(Expansion::encodeSentence(d.code, f, m));
                QVERIFY(Expansion::decodeSentence(m, out));
            }
            auto result = unpack(out);
            for (const auto &field : d.fields)
                if (field.type == "double")
                    QCOMPARE(result[field.name].toDouble(), N2kDoubleNA);
        }
        QVERIFY(!Expansion::validField("129540", "Satellites", "[{\"PRN\":1}]"));
        QVERIFY(!Expansion::validField("GSV", "Satellites", "not JSON"));
        tN2kMsg m;
        m.SetPGN(129540);
        m.AddByte(1);
        m.AddByte(0xfc);
        m.AddByte(19);
        QStringList out;
        QVERIFY(!Expansion::decode(m, out));
    }
    void manualPropertiesAndPreferences()
    {
        Workspace w;
        auto *instrument = w.add("130311");
        auto prefs = instrument->UserPrefs();
        instrument->setValue("Temperature", "293.123456789");
        const auto before = w.model->getPgnParams(instrument->Id());
        for (int i = 0; i < 3; ++i)
        {
            prefs.tempUnitIndex = i;
            instrument->setUserPrefs(prefs);
            QCOMPARE(w.model->getPgnParams(instrument->Id()), before);
        }
        prefs.tempUnitIndex = 1;
        QVERIFY(Expansion::displayValue("130311", "Temperature", "293.15", prefs).startsWith("20.00"));
        w.model->setCurrentPgn(instrument->Id());
        int row = 0;
        for (const auto &p : w.model->getPgnParams(instrument->Id()))
        {
            if (p[0] == "Temperature")
                break;
            ++row;
        }
        QVERIFY(!w.model->setData(w.model->index(row, 1), "nan"));
        QVERIFY(w.model->setData(w.model->index(row, 1), "294.987654321"));
        QCOMPARE(instrument->value("Temperature"), QString("294.987654321"));
        auto *editor = instrument->findChild<QLineEdit *>("lineEditTemperature");
        QVERIFY(editor);
        QVERIFY(!editor->isModified());
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void sourceOrdersAndLifecycle_data()
    {
        QTest::addColumn<bool>("sourceFirst");
        QTest::newRow("source-first") << true;
        QTest::newRow("instrument-first") << false;
    }
    void sourceOrdersAndLifecycle()
    {
        QFETCH(bool, sourceFirst);
        Workspace w;
        auto source = [&] {
            w.c->addGeneratorWidget("Weather (PGN -130311)");
            settle();
        };
        if (sourceFirst)
            source();
        auto *legacy = w.add("130311");
        auto *pressure = w.add("130314");
        auto *temp = w.add("MTW");
        if (!sourceFirst)
            source();
        auto *weather = w.window.findChild<GeneratorWeather *>();
        QVERIFY(weather);
        weather->setEnabled(true);
        legacy->setValue("TemperatureSource", "1");
        legacy->setValue("HumiditySource", QString::number(weather->HumiditySource()));
        pressure->setValue("PressureSource", "0");
        QSignalSpy changes(legacy, &ExpansionInstrument::parameterValueChanged);
        weather->setAirTemperature(295.1234);
        weather->setSeaTemperature(289.4321);
        weather->setAtmosphericPressure(100123);
        weather->setHumidity(66.1);
        settle();
        QCOMPARE(legacy->value("Temperature").toDouble(), 295.1234);
        QCOMPARE(temp->value("WaterTemperature").toDouble(), 289.4321);
        QCOMPARE(pressure->value("ActualPressure").toDouble(), 100123.);
        int tempEvents = 0;
        for (const auto &entry : changes)
            if (entry[0].toString() == "Temperature")
                tempEvents++;
        QCOMPARE(tempEvents, 1);
        legacy->setValue("Humidity", "33");
        weather->setAirTemperature(296);
        settle();
        QCOMPARE(legacy->value("Humidity"), QString("33"));
        pressure->setValue("PressureSource", "1");
        pressure->setValue("ActualPressure", "555");
        weather->setAtmosphericPressure(101234);
        settle();
        QCOMPARE(pressure->value("ActualPressure"), QString("555"));
        w.window.findChild<QAction *>("actionViewer_Rx")->setChecked(true);
        const auto previous = legacy->value("Temperature");
        weather->setAirTemperature(297);
        settle();
        QCOMPARE(legacy->value("Temperature"), previous);
        w.window.findChild<QAction *>("actionSimulator_Tx")->setChecked(true);
        weather->setEnabled(true);
        weather->setAirTemperature(298);
        settle();
        QCOMPARE(legacy->value("Temperature").toDouble(), 298.);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
        w.model->removePgn(weather->Id());
        w.c->removeWidgetFromSubForm(weather, "Generate");
        settle();
        source();
        weather = w.window.findChild<GeneratorWeather *>();
        QVERIFY(weather);
        weather->setEnabled(true);
        weather->setAirTemperature(299);
        settle();
        QCOMPARE(legacy->value("Temperature").toDouble(), 299.);
        QTemporaryDir dir;
        const auto layout = dir.filePath("sources.json");
        QVERIFY(w.c->saveLayoutToJson(layout));
        QVERIFY(w.c->loadLayoutFromJson(layout));
        settle();
        weather = w.window.findChild<GeneratorWeather *>();
        QVERIFY(weather);
        weather->setEnabled(true);
        weather->setAirTemperature(300);
        settle();
        legacy = w.window.findChild<EnvironmentalLegacy130311 *>();
        QVERIFY(legacy);
        QCOMPARE(legacy->value("Temperature").toDouble(), 300.);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void navigationSourceSemantics_data()
    {
        sourceOrdersAndLifecycle_data();
    }
    void navigationSourceSemantics()
    {
        QFETCH(bool, sourceFirst);
        Workspace w;
        auto sources = [&] {
            w.c->addGeneratorWidget("Vessel Movement (PGN -129025)");
            w.c->addGeneratorWidget("Weather (PGN -130311)");
            w.c->addGeneratorWidget("Date Time (PGN -129033)");
            settle();
        };
        if (sourceFirst)
            sources();
        auto *wind = w.add("MWV");
        auto *heading = w.add("VHW");
        auto *log = w.add("128275");
        heading->setValue("WaterSpeed", "7.123456789");
        if (!sourceFirst)
            sources();
        auto *movement = w.window.findChild<GeneratorVesselMovement *>();
        auto *weather = w.window.findChild<GeneratorWeather *>();
        auto *clock = w.window.findChild<GeneratorDateTime *>();
        QVERIFY(movement);
        QVERIFY(weather);
        QVERIFY(clock);
        movement->setEnabled(true);
        weather->setEnabled(true);
        clock->setEnabled(true);
        movement->setHeadingReference(N2khr_true);
        movement->setVariation(0);
        movement->setDeviation(0);
        movement->setHeading(0.5);
        movement->setSOG(9);
        weather->setWindAngle(1.5);
        weather->setWindSpeed(8);
        clock->setDaysSince1970(20001);
        clock->setSecondsSinceMidnight(12345);
        settle();
        QVERIFY(qAbs(wind->value("WindAngle").toDouble() - 1) < 1e-10);
        QCOMPARE(wind->value("WindSpeed").toDouble(), 8.);
        QCOMPARE(heading->value("TrueHeading").toDouble(), 0.5);
        QCOMPARE(heading->value("WaterSpeed"), QString("7.123456789"));
        QCOMPARE(log->value("DaysSince1970"), QString("20001"));
        QCOMPARE(log->value("SecondsSinceMidnight").toDouble(), 12345.);
        wind->setValue("Reference", "R");
        wind->setValue("WindAngle", "2.1");
        wind->setValue("WindSpeed", "3.2");
        weather->setWindAngle(2.5);
        weather->setWindSpeed(10);
        settle();
        QCOMPARE(wind->value("WindAngle"), QString("2.1"));
        QCOMPARE(wind->value("WindSpeed"), QString("3.2"));
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void schedulerAuthorizationAndIntervals()
    {
        QTableView view;
        PgnTableController table(&view);
        auto *model = table.getPgnTableModel();
        int id = 0;
        for (const auto &d : Expansion::definitions())
        {
            auto rows = Expansion::parameters(d.code);
            const auto f = fixture(d.code);
            for (auto &row : rows)
                row[1] = f[row[0]];
            ++id;
            model->addPgn(id, d.label, d.code, id != 1, rows, {}, 1000);
        }
        NmeaSenderWorker sender;
        sender.setPgnTableController(&table);
        sender.setMode(Sender);
        QSignalSpy can(&sender, &NmeaSenderWorker::sendPgn);
        QSignalSpy serial(&sender, &NmeaSenderWorker::sendSnt);
        sender.sendNmeaMessage();
        QCOMPARE(can.size() + serial.size(), 0);
        sender.startSending();
        sender.findChild<QTimer *>()->stop();
        sender.sendNmeaMessage();
        QCOMPARE(can.size() + serial.size(), Expansion::definitions().size() - 1);
        sender.sendNmeaMessage();
        QCOMPARE(can.size() + serial.size(), Expansion::definitions().size() - 1);
        QTest::qWait(1100);
        sender.sendNmeaMessage();
        QCOMPARE(can.size() + serial.size(), 2 * (Expansion::definitions().size() - 1));
        sender.stopSending();
        sender.sendNmeaMessage();
        QCOMPARE(can.size() + serial.size(), 2 * (Expansion::definitions().size() - 1));
        sender.setMode(Viewer);
        sender.startSending();
        sender.sendNmeaMessage();
        QCOMPARE(can.size() + serial.size(), 2 * (Expansion::definitions().size() - 1));
        QVERIFY(!sender.isSending());
    }
    void individualDefaultsAndRestore()
    {
        Workspace w;
        UserOptionPrefs options;
        options.newWidgetsEnabled = false;
        w.c->setUserOptionPrefs(options);
        auto *first = w.add("DPT");
        const int firstId = first->Id();
        QVERIFY(!first->isEnabled());
        options.newWidgetsEnabled = true;
        w.c->setUserOptionPrefs(options);
        auto *second = w.add("MTW");
        const int secondId = second->Id();
        QVERIFY(second->isEnabled());
        w.model->setSendInterval(firstId, 750);
        w.model->setSendInterval(secondId, 2300);
        QTemporaryDir dir;
        const auto file = dir.filePath("options.json");
        QVERIFY(w.c->saveLayoutToJson(file));
        QVERIFY(w.c->loadLayoutFromJson(file));
        settle();
        QCOMPARE(w.model->getPgnDataToSend(firstId).enabled, false);
        QCOMPARE(w.model->getPgnDataToSend(secondId).enabled, true);
        QCOMPARE(w.model->getPgnDataToSend(firstId).sendInterval, 750.);
        QCOMPARE(w.model->getPgnDataToSend(secondId).sendInterval, 2300.);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void presentation_data()
    {
        supportedMessages_data();
    }
    void presentation()
    {
        QFETCH(QString, code);
        const auto *d = Expansion::definition(code);
        std::unique_ptr<ExpansionInstrument> widget(ExpansionInstrument::create(1, d->label, code));
        const auto f = fixture(code);
        for (auto i = f.begin(); i != f.end(); ++i)
            widget->setValue(i.key(), i.value());
        widget->resize(440, widget->minimumHeight());
        widget->show();
        QTest::qWait(25);
        auto *remove = widget->findChild<QPushButton *>("pushButtonRemove");
        QVERIFY(remove);
        QVERIFY(remove->text().isEmpty());
        QVERIFY(!remove->icon().isNull());
        QCOMPARE(remove->size(), QSize(24, 24));
        QVERIFY(!remove->toolTip().isEmpty());
        for (const auto &field : d->fields)
        {
            const auto options = Expansion::choices(field.type);
            if (!options.isEmpty())
            {
                auto *combo = widget->findChild<QComboBox *>(field.name);
                QVERIFY(combo);
                QCOMPARE(combo->count(), options.size());
                QVERIFY(!combo->toolTip().isEmpty());
                QCOMPARE(combo->currentData().toString(), f[field.name]);
            }
            else
            {
                auto *edit = widget->findChild<QLineEdit *>("lineEdit" + field.name);
                QVERIFY(edit);
                QVERIFY(!edit->toolTip().isEmpty());
                QVERIFY(widget->rect().contains(edit->mapTo(widget.get(), QPoint(0, 0))));
                QVERIFY(!widget->findChild<QLabel *>("display" +
                                                     field.name)); // one preferred-unit editor, no duplicate raw value
            }
        }
        const auto directory = qEnvironmentVariable("M238A_EVIDENCE");
        if (!directory.isEmpty())
        {
            QVERIFY(QDir().mkpath(directory));
            QVERIFY(widget->grab().save(directory + "/" + code + ".png"));
        }
    }
    void compactLayout_data()
    {
        supportedMessages_data();
    }
    void compactLayout()
    {
        QFETCH(QString, code);
        const auto *d = Expansion::definition(code);
        std::unique_ptr<ExpansionInstrument> widget(ExpansionInstrument::create(1, d->label, code));
        const auto originalPalette = qApp->palette();
        const auto originalStyle = widget->styleSheet();
        for (bool dark : {false, true})
        {
            const QString theme = dark ? "Dark" : "Light";
            qApp->setPalette(Themes::palette(theme));
            widget->setStyleSheet(Themes::styleSheet(theme));
            for (int width : {340, 440, 700})
            {
                widget->resize(width, widget->minimumHeight());
                widget->show();
                QTest::qWait(10);
                for (auto *edit : widget->findChildren<QLineEdit *>())
                {
                    QVERIFY(edit->width() < widget->width() - 150);
                    QVERIFY(edit->width() >= 75);
                    const QRect bounds(edit->mapTo(widget.get(), QPoint()), edit->size());
                    QVERIFY2(widget->rect().contains(bounds), qPrintable(code));
                }
                for (auto *combo : widget->findChildren<QComboBox *>())
                {
                    QVERIFY(combo->width() >= 75);
                    QVERIFY(widget->rect().contains(QRect(combo->mapTo(widget.get(), QPoint()), combo->size())));
                }
                for (auto *label : widget->findChildren<QLabel *>())
                {
                    const QRect bounds(label->mapTo(widget.get(), QPoint()), label->size());
                    QVERIFY2(widget->rect().contains(bounds), qPrintable(code + label->text()));
                    for (auto *edit : widget->findChildren<QLineEdit *>())
                        QVERIFY(!bounds.intersects(QRect(edit->mapTo(widget.get(), QPoint()), edit->size())));
                }
                const auto directory = qEnvironmentVariable("M238A_EVIDENCE");
                if (!directory.isEmpty() && width == 440)
                    QVERIFY(widget->grab().save(directory + "/" + code + (dark ? "-dark.png" : "-light.png")));
            }
        }
        widget->setStyleSheet(originalStyle);
        qApp->setPalette(originalPalette);
    }
    void legacyRowParity()
    {
        TemperatureWidget130312 legacy(1, "Sea Temp", nullptr);
        std::unique_ptr<ExpansionInstrument> added(ExpansionInstrument::create(2, "Water Temperature", "MTW"));
        for (const auto &theme : {QString("Dark"), QString("Light")})
        {
            legacy.setStyleSheet(Themes::styleSheet(theme));
            added->setStyleSheet(Themes::styleSheet(theme));
            for (int width : {440, 700})
            {
                legacy.resize(width, 180);
                added->resize(width, 180);
                legacy.show();
                added->show();
                QTest::qWait(25);
                auto *oldValue = legacy.findChild<QLineEdit *>("lineEdit");
                auto *newValue = added->findChild<QLineEdit *>("lineEditWaterTemperature");
                auto *oldLabel = legacy.findChild<QLabel *>("labelSource");
                auto *newLabel = added->findChild<QLabel *>("labelFieldWaterTemperature");
                auto *oldUnits = legacy.findChild<QLabel *>("labelUnits");
                auto *newUnits = added->findChild<QLabel *>("labelUnitsWaterTemperature");
                QVERIFY(oldValue && newValue && oldLabel && newLabel && oldUnits && newUnits);
                QCOMPARE(newValue->alignment(), oldValue->alignment());
                QCOMPARE(newLabel->width(), oldLabel->width());
                QVERIFY(qAbs(newValue->x() - oldValue->x()) <= 2);
                QVERIFY(qAbs(newValue->width() - oldValue->width()) <= 2);
                QVERIFY(qAbs(newUnits->x() - oldUnits->x()) <= 2);
            }
        }
    }
    void satelliteLoadedLayoutMouseInteraction_data()
    {
        QTest::addColumn<QString>("layoutPath");
        QTest::newRow("generated-snapshot") << QString();
        const auto owner = qEnvironmentVariable("RAYNMEA_TEST_SATELLITE_LAYOUT");
        if (!owner.isEmpty())
            QTest::newRow("owner-snapshot") << owner;
    }
    void satelliteLoadedLayoutMouseInteraction()
    {
        Workspace w;
        auto *instrument = w.add("129540");
        instrument->setValue("Satellites", fixture("129540")["Satellites"]);
        QTemporaryDir dir;
        const auto layout = dir.filePath("testAddNmeaWidgets.json");
        QVERIFY(w.c->saveLayoutToJson(layout));
        QFETCH(QString, layoutPath);
        QVERIFY(w.c->loadLayoutFromJson(layoutPath.isEmpty() ? layout : layoutPath));
        instrument = w.window.findChild<GnssSatellites129540 *>();
        QVERIFY(instrument);
        w.window.show();
        instrument->show();
        QTest::qWait(50);
        bool completed = false;
        QTimer::singleShot(50, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            QTimer::singleShot(5000, dialog, &QDialog::reject);
            auto *add = dialog->findChild<QPushButton *>("addRecord");
            auto *table = dialog->findChild<QTableWidget *>("satelliteRecords");
            QVERIFY(add && table);
            for (int row = 0; row < 5; ++row)
            {
                QTest::mouseClick(add, Qt::LeftButton);
                QTest::qWait(20);
                QCOMPARE(table->rowCount(), row + 2);
                auto *cell = qobject_cast<QLineEdit *>(table->cellWidget(row, 0));
                QTest::mouseClick(cell, Qt::LeftButton);
                QTest::keyClick(cell, Qt::Key_A, Qt::ControlModifier);
                QTest::keyClicks(cell, QString::number(row + 10));
                QTest::keyClick(cell, Qt::Key_Return);
                QTest::qWait(20);
            }
            auto *usage = qobject_cast<QComboBox *>(table->cellWidget(0, 5));
            usage->setFocus();
            QTest::qWait(25);
            QTest::mouseClick(add, Qt::LeftButton);
            QTest::qWait(25);
            auto *angle = table->cellWidget(0, 2);
            QTest::mouseClick(angle, Qt::LeftButton);
            QTest::qWait(25);
            QTest::mouseClick(add, Qt::LeftButton);
            QTest::qWait(25);
            completed = true;
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        instrument->findChild<QPushButton *>("editSatellites")->click();
        QVERIFY(completed);
        QCOMPARE(QJsonDocument::fromJson(instrument->value("Satellites").toUtf8()).array().size(), 8);
    }
    void satelliteInteraction_data()
    {
        QTest::addColumn<QString>("code");
        QTest::newRow("N2k") << QString("129540");
        QTest::newRow("GSV") << QString("GSV");
    }
    void satelliteInteraction()
    {
        QFETCH(QString, code);
        std::unique_ptr<ExpansionInstrument> widget(ExpansionInstrument::create(1, "Satellites", code));
        const auto original = fixture(code)["Satellites"];
        widget->setValue("Satellites", original);
        bool completed = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            // Always close on assertion failure to avoid a stranded modal test.
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            auto *table = dialog->findChild<QTableWidget *>("satelliteRecords");
            auto *add = dialog->findChild<QPushButton *>("addRecord");
            auto *remove = dialog->findChild<QPushButton *>("removeRecord");
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(table && add && remove && buttons);
            table->clearSelection();
            QVERIFY(!remove->isEnabled());
            const int maximum = code == "129540" ? 18 : 4;
            while (table->rowCount() < maximum)
                add->click();
            QCOMPARE(table->rowCount(), maximum);
            QVERIFY(!add->isEnabled());
            QCOMPARE(dialog->findChild<QLabel *>("recordCount")->text(), QString("Records: %1 / %1").arg(maximum));
            table->selectRow(1);
            QVERIFY(remove->isEnabled());
            remove->click();
            QCOMPARE(table->rowCount(), maximum - 1);
            QVERIFY(add->isEnabled());
            QVERIFY(!remove->isEnabled());
            // Retain only original row; removing rows must retain original canonical cell values.
            while (table->rowCount() > 1)
            {
                table->selectRow(1);
                remove->click();
            }
            auto *edit = qobject_cast<QLineEdit *>(table->cellWidget(0, 0));
            edit->setFocus();
            QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClicks(edit, "bad");
            QTest::keyClick(edit, Qt::Key_Return);
            QVERIFY(dialog->isVisible());
            QTest::keyClick(edit, Qt::Key_Enter);
            QVERIFY(dialog->isVisible());
            buttons->button(QDialogButtonBox::Ok)->click();
            QVERIFY(dialog->isVisible());
            QCOMPARE(edit->text(), QString("bad"));
            QCOMPARE(widget->value("Satellites"), original);
            QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClicks(edit, "13");
            QTest::keyClick(edit, Qt::Key_Return);
            QVERIFY(dialog->isVisible());
            auto *snr = qobject_cast<QLineEdit *>(table->cellWidget(0, 3));
            snr->setFocus();
            QTest::keyClick(snr, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClick(snr, Qt::Key_Backspace);
            QTest::keyClick(snr, Qt::Key_Return);
            QVERIFY(dialog->isVisible());
            add->click();
            QCOMPARE(table->rowCount(), 2);
            const auto directory = qEnvironmentVariable("M238A_EVIDENCE");
            if (!directory.isEmpty())
                QVERIFY(dialog->grab().save(directory + "/" + code + "-records.png"));
            completed = true;
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        widget->findChild<QPushButton *>("editSatellites")->click();
        QVERIFY(completed);
        const auto accepted = widget->value("Satellites");
        auto result = QJsonDocument::fromJson(accepted.toUtf8()).array()[0].toObject();
        QCOMPARE(result["PRN"].toInt(), 13);
        QCOMPARE(result["SNR"].toDouble(), N2kDoubleNA);
        QCOMPARE(QJsonDocument::fromJson(accepted.toUtf8()).array().size(), 2);
        QCOMPARE(QJsonDocument::fromJson(accepted.toUtf8()).array()[1].toObject()["PRN"].toInt(), 2);
        QCOMPARE(result["Elevation"].toDouble(),
                 QJsonDocument::fromJson(original.toUtf8()).array()[0].toObject()["Elevation"].toDouble());
        for (bool close : {false, true})
        {
            QTimer::singleShot(0, [close] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                dialog->findChild<QPushButton *>("addRecord")->click();
                if (close)
                    dialog->close();
                else
                    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
            });
            widget->findChild<QPushButton *>("editSatellites")->click();
            QCOMPARE(widget->value("Satellites"), accepted);
        }
    }
    void dopEnumAndPropertyEditor()
    {
        Workspace w;
        auto *widget = qobject_cast<GnssDops129539 *>(w.add("129539"));
        QVERIFY(widget);
        const auto type = widget->metaObject()->property(widget->metaObject()->indexOfProperty("DesiredMode"));
        QCOMPARE(QString::fromLatin1(type.typeName()), QString("tN2kGNSSDOPmode"));
        auto *desired = widget->findChild<QComboBox *>("DesiredMode");
        QVERIFY(desired);
        QCOMPARE(desired->count(), 8);
        desired->setCurrentIndex(desired->findData(N2kGNSSdm_Auto));
        QVERIFY(QMetaObject::invokeMethod(desired, "activated", Q_ARG(int, desired->currentIndex())));
        QCOMPARE(widget->DesiredMode(), N2kGNSSdm_Auto);
        w.model->setCurrentPgn(widget->Id());
        int row = 0;
        for (const auto &field : w.model->getPgnParams(widget->Id()))
        {
            if (field[0] == "DesiredMode")
                break;
            ++row;
        }
        auto index = w.model->index(row, 1);
        QCOMPARE(index.siblingAtColumn(3).data().toString(), QString("tN2kGNSSDOPmode"));
        EnumComboBoxDelegate delegate;
        QWidget parent;
        std::unique_ptr<QWidget> editor(delegate.createEditor(&parent, QStyleOptionViewItem(), index));
        auto *combo = qobject_cast<QComboBox *>(editor.get());
        QVERIFY(combo);
        delegate.setEditorData(combo, index);
        QCOMPARE(combo->currentData().toInt(), 3);
        combo->setCurrentIndex(combo->findData(N2kGNSSdm_Unavailable));
        delegate.setModelData(combo, w.model, index);
        QCOMPARE(widget->value("DesiredMode"), QString("7"));
        for (int mode = 0; mode < 8; ++mode)
        {
            auto f = fixture("129539");
            f["DesiredMode"] = QString::number(mode);
            f["ActualMode"] = QString::number(mode);
            tN2kMsg message;
            QVERIFY(Expansion::encode("129539", f, message));
            QCOMPARE(message.Data[1], quint8(0xc0 | mode | (mode << 3)));
            QStringList out;
            QVERIFY(Expansion::decode(message, out));
            QCOMPARE(unpack(out)["ActualMode"].toInt(), mode);
        }
        QVERIFY(!Expansion::validField("129539", "ActualMode", "8"));
        QVERIFY(!Expansion::validField("129539", "ActualMode", "1.25"));
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void preferredUnitEditingAndDrafts()
    {
        Workspace w;
        auto *widget = w.add("130311");
        w.window.show();
        auto prefs = widget->UserPrefs();
        prefs.tempUnitIndex = 1;
        prefs.atmosphericPressureUnitIndex = 0;
        widget->setUserPrefs(prefs);
        widget->setValue("Temperature", "293.1234567891234");
        widget->setValue("AtmosphericPressure", "101325");
        QSignalSpy updates(widget, &ExpansionInstrument::parameterValueChanged);
        auto *temperature = widget->findChild<QLineEdit *>("lineEditTemperature");
        auto *units = widget->findChild<ClickableLabel *>("labelUnitsTemperature");
        QVERIFY(temperature);
        QVERIFY(units);
        QCOMPARE(temperature->text(), QString("19.97"));
        for (int i = 0; i < 3; ++i)
        {
            QVERIFY(QMetaObject::invokeMethod(units, "actionTriggered", Q_ARG(int, i)));
            QCOMPARE(widget->value("Temperature"), QString("293.1234567891234"));
        }
        QCOMPARE(updates.size(), 0);
        QVERIFY(QMetaObject::invokeMethod(units, "actionTriggered", Q_ARG(int, 1)));
        temperature->setFocus();
        QTest::keyClick(temperature, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(temperature, "25.5");
        prefs = widget->UserPrefs();
        prefs.tempUnitIndex = 2;
        widget->setUserPrefs(prefs);
        QCOMPARE(temperature->text(), QString("25.5"));
        QCOMPARE(units->text(), QString("°C"));
        QVERIFY(QMetaObject::invokeMethod(temperature, "editingFinished"));
        QVERIFY(qAbs(widget->value("Temperature").toDouble() - 298.65) < 1e-10);
        QCOMPARE(units->text(), QString("°F"));
        auto *pressure = widget->findChild<QLineEdit *>("lineEditAtmosphericPressure");
        QCOMPARE(pressure->text(), QString("1013.25"));
        pressure->setFocus();
        QTest::keyClick(pressure, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(pressure, "1000");
        QVERIFY(QMetaObject::invokeMethod(pressure, "editingFinished"));
        QCOMPARE(widget->value("AtmosphericPressure").toDouble(), 100000.);
        temperature->setFocus();
        QTest::keyClick(temperature, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(temperature, "Unavailable");
        QVERIFY(QMetaObject::invokeMethod(temperature, "editingFinished"));
        QCOMPARE(widget->value("Temperature").toDouble(), N2kDoubleNA);
        QCOMPARE(temperature->text(), QString("Unavailable"));
        auto *depth = w.add("DPT");
        prefs = depth->UserPrefs();
        prefs.depthUnitIndex = 2;
        depth->setUserPrefs(prefs);
        depth->setValue("DepthBelowTransducer", "18.288");
        QCOMPARE(depth->findChild<QLineEdit *>("lineEditDepthBelowTransducer")->text(), QString("10.00"));
        auto *log = w.add("128275");
        log->setValue("Log", "1852");
        prefs = log->UserPrefs();
        prefs.distanceUnitIndex = 1;
        log->setUserPrefs(prefs);
        QCOMPARE(log->findChild<QLineEdit *>("lineEditLog")->text(), QString("1.00"));
        log->setValue("SecondsSinceMidnight", "12345.6789");
        for (int i = 0; i < 4; ++i)
        {
            prefs.timeUnitIndex = i;
            log->setUserPrefs(prefs);
            QCOMPARE(log->value("SecondsSinceMidnight"), QString("12345.6789"));
        }
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void allUnitFamiliesAndTree()
    {
        Workspace w;
        w.window.show();
        struct Case
        {
            QString code, field, input;
            double expected;
        };
        const QList<Case> cases{{"MTW", "WaterTemperature", "25", 298.15},
                                {"130314", "ActualPressure", "101.325", 101325},
                                {"MWV", "WindSpeed", "10", 5.144444444444},
                                {"MWV", "WindAngle", "180", M_PI},
                                {"VHW", "TrueHeading", "90", M_PI / 2},
                                {"127497", "TripFuelUsed", "10", 37.85411784},
                                {"127497", "FuelRateAverage", "2", 7.570823568},
                                {"DPT", "Offset", "-2", -0.6096},
                                {"128275", "Log", "5", 9260},
                                {"129539", "HDOP", "1.25", 1.25}};
        for (const auto &test : cases)
        {
            auto *widget = w.add(test.code);
            auto prefs = widget->UserPrefs();
            prefs.tempUnitIndex = 1;
            prefs.pressureUnitIndex = 1;
            prefs.speedUnitIndex = 3;
            prefs.volumeUnitIndex = 1;
            prefs.flowRateUnitIndex = 1;
            prefs.depthUnitIndex = 1;
            prefs.distanceUnitIndex = 1;
            widget->setUserPrefs(prefs);
            auto *edit = widget->findChild<QLineEdit *>("lineEdit" + test.field);
            QVERIFY(edit);
            edit->setFocus();
            QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClicks(edit, test.input);
            QVERIFY(QMetaObject::invokeMethod(edit, "editingFinished"));
            QVERIFY2(qAbs(widget->value(test.field).toDouble() - test.expected) < 0.0001,
                     qPrintable(test.code + " " + test.field + " canonical conversion"));
            QTest::qWait(10);
            auto *tree = w.window.findChild<PgnTreeController *>()->getTreeModel();
            const auto index = tree->indexFor(widget, test.field);
            QVERIFY(index.isValid());
            QCOMPARE(index.siblingAtColumn(1).data().toString(),
                     Expansion::displayValue(test.code, test.field, widget->value(test.field), prefs));
        }
        auto *log = w.add("128275");
        auto prefs = log->UserPrefs();
        prefs.dateUnitIndex = 2;
        prefs.timeUnitIndex = 2;
        log->setUserPrefs(prefs);
        for (const auto &entry :
             QList<QPair<QString, QString>>{{"DaysSince1970", "2025/01/01"}, {"SecondsSinceMidnight", "12:34:56"}})
        {
            auto *edit = log->findChild<QLineEdit *>("lineEdit" + entry.first);
            edit->setFocus();
            QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
            QTest::keyClicks(edit, entry.second);
            QVERIFY(QMetaObject::invokeMethod(edit, "editingFinished"));
        }
        QCOMPARE(log->value("DaysSince1970").toUInt(), 20089u);
        QCOMPARE(log->value("SecondsSinceMidnight").toDouble(), 45296.);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void satelliteRecordEditorPrecision()
    {
        std::unique_ptr<ExpansionInstrument> widget(ExpansionInstrument::create(1, "Satellites", "129540"));
        const auto text = fixture("129540")["Satellites"];
        widget->setValue("Satellites", text);
        QSignalSpy changes(widget.get(), &ExpansionInstrument::parameterValueChanged);
        bool inspected = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto *table = dialog->findChild<QTableWidget *>("satelliteRecords");
            if (table && table->rowCount() == 1)
            {
                auto *combo = qobject_cast<QComboBox *>(table->cellWidget(0, 5));
                if (combo)
                {
                    inspected = true;
                    combo->setCurrentIndex(combo->findData(3));
                    QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, combo->currentIndex()));
                }
            }
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        widget->findChild<QPushButton *>("editSatellites")->click();
        QVERIFY(inspected);
        QCOMPARE(changes.size(), 1);
        auto result = QJsonDocument::fromJson(widget->value("Satellites").toUtf8()).array()[0].toObject();
        QCOMPARE(result["UsageStatus"].toInt(), 3);
        QCOMPARE(result["Elevation"].toDouble(), 0.5);
        QCOMPARE(result["RangeResiduals"].toDouble(), 0.12345);
        const auto accepted = widget->value("Satellites");
        changes.clear();
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (dialog)
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        widget->findChild<QPushButton *>("editSatellites")->click();
        QCOMPARE(widget->value("Satellites"), accepted);
        QCOMPARE(changes.size(), 0);
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (dialog)
                dialog->reject();
        });
        widget->findChild<QPushButton *>("editSatellites")->click();
        QCOMPARE(widget->value("Satellites"), accepted);
    }
    void tokenEnumsAndIntegralSentinels()
    {
        Workspace w;
        auto *wind = qobject_cast<WindMWV *>(w.add("MWV"));
        QVERIFY(wind);
        w.model->setCurrentPgn(wind->Id());
        EnumComboBoxDelegate delegate;
        QWidget parent;
        for (const QString field : {QString("Reference"), QString("Status")})
        {
            int row = 0;
            for (const auto &p : w.model->getPgnParams(wind->Id()))
            {
                if (p[0] == field)
                    break;
                ++row;
            }
            auto index = w.model->index(row, 1);
            std::unique_ptr<QWidget> editor(delegate.createEditor(&parent, QStyleOptionViewItem(), index));
            auto *combo = qobject_cast<QComboBox *>(editor.get());
            QVERIFY(combo);
            delegate.setEditorData(combo, index);
            QCOMPARE(combo->currentData().toString(), field == "Reference" ? QString("T") : QString("A"));
            combo->setCurrentIndex(combo->findData(field == "Reference" ? "R" : "V"));
            delegate.setModelData(combo, w.model, index);
        }
        QCOMPARE(wind->Reference(), NMEA0183Wind_Apparent);
        QCOMPARE(wind->Status(), 'V');
        auto f = fixture("MWV");
        f["Reference"] = wind->value("Reference");
        f["Status"] = wind->value("Status");
        tNMEA0183Msg sentence;
        QVERIFY(Expansion::encodeSentence("MWV", f, sentence));
        QStringList out;
        QVERIFY(Expansion::decodeSentence(sentence, out));
        QCOMPARE(unpack(out)["Reference"], QString("R"));
        QCOMPARE(unpack(out)["Status"], QString("V"));
        auto *legacy = qobject_cast<EnvironmentalLegacy130311 *>(w.add("130311"));
        QVERIFY(legacy);
        legacy->setHumiditySource(N2khs_Undef);
        QCOMPARE(legacy->HumiditySource(), N2khs_Undef);
        QCOMPARE(legacy->value("HumiditySource"), QString("3"));
        legacy->setSID(255);
        QCOMPARE(legacy->findChild<QLineEdit *>("lineEditSID")->text(), QString("Unavailable"));
        auto *log = qobject_cast<DistanceLog128275 *>(w.add("128275"));
        QVERIFY(log);
        log->setValue("Log", "1234.0");
        QCOMPARE(log->Log(), quint32(1234));
        auto values = fixture("128275");
        values["Log"] = log->value("Log");
        tN2kMsg message;
        QVERIFY(Expansion::encode("128275", values, message));
        QVERIFY(Expansion::decode(message, out));
        QCOMPARE(unpack(out)["Log"], QString("1234"));
        QVERIFY(!Expansion::validField("128275", "Log", "1.5"));
        QVERIFY(!Expansion::validField("GSV", "MessageNumber", "0"));
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void closeAndLayoutMetadata()
    {
        Workspace w;
        w.dialogs.timer.stop();
        auto *widget = w.add("129539");
        widget->setValue("DesiredMode", "3");
        const int id = widget->Id();
        QTemporaryDir dir;
        const auto path = dir.filePath("typed.json");
        QVERIFY(w.c->saveLayoutToJson(path));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        auto groups = root["layouts"].toArray();
        for (int g = 0; g < groups.size(); ++g)
        {
            auto group = groups[g].toObject();
            auto widgets = group["widgets"].toArray();
            for (int v = 0; v < widgets.size(); ++v)
            {
                auto object = widgets[v].toObject();
                auto rows = object["parameters"].toArray();
                for (int i = 0; i < rows.size(); ++i)
                {
                    auto row = rows[i].toArray();
                    if (row[0].toString() == "DesiredMode" || row[0].toString() == "ActualMode")
                        row[3] = "int";
                    if (row[0].toString() == "DesiredMode")
                        row[1] = "1.0";
                    rows[i] = row;
                }
                object["parameters"] = rows;
                widgets[v] = object;
            }
            group["widgets"] = widgets;
            groups[g] = group;
        }
        root["layouts"] = groups;
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(root).toJson());
        file.close();
        w.dialogs.timer.start();
        QVERIFY(w.c->loadLayoutFromJson(path));
        w.dialogs.timer.stop();
        widget = w.window.findChild<GnssDops129539 *>();
        QVERIFY(widget);
        QCOMPARE(widget->value("DesiredMode"), QString("1.0"));
        QCOMPARE(widget->findChild<QComboBox *>("DesiredMode")->currentData().toInt(), 1);
        for (const auto &row : w.model->getPgnParams(id))
            if (row[0] == "DesiredMode")
                QCOMPARE(row[3], QString("tN2kGNSSDOPmode"));
        QTimer::singleShot(0, [] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                box->button(QMessageBox::Yes)->click();
        });
        widget->findChild<QPushButton *>("pushButtonRemove")->click();
        QCOMPARE(w.model->getPgnDataToSend(id).idX, -1);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void satelliteBoundsAndNa()
    {
        auto gsv = fixture("GSV");
        auto records = QJsonDocument::fromJson(gsv["Satellites"].toUtf8()).array();
        auto satellite = records[0].toObject();
        satellite["Elevation"] = -1;
        records[0] = satellite;
        gsv["Satellites"] = QJsonDocument(records).toJson(QJsonDocument::Compact);
        QVERIFY(!Expansion::validField("GSV", "Satellites", gsv["Satellites"]));
        tNMEA0183Msg invalid;
        QVERIFY(!Expansion::encodeSentence("GSV", gsv, invalid));

        auto f = fixture("129540");
        auto array = QJsonDocument::fromJson(f["Satellites"].toUtf8()).array();
        auto record = array.first().toObject();
        QJsonArray full;
        for (int i = 0; i < 18; ++i)
        {
            record["PRN"] = i + 1;
            full.append(record);
        }
        f["Satellites"] = QJsonDocument(full).toJson(QJsonDocument::Compact);
        tN2kMsg message;
        QVERIFY(Expansion::encode("129540", f, message));
        QCOMPARE(message.DataLen, 219);
        QStringList fields;
        QVERIFY(Expansion::decode(message, fields));
        full.append(record);
        QVERIFY(!Expansion::validField("129540", "Satellites", QJsonDocument(full).toJson(QJsonDocument::Compact)));
        f = fixture("128275");
        f["Log"] = "4294967295";
        f["TripLog"] = "4294967295";
        QVERIFY(Expansion::encode("128275", f, message));
        QVERIFY(Expansion::decode(message, fields));
        QCOMPARE(unpack(fields)["Log"], QString("4294967295"));
        QCOMPARE(unpack(fields)["TripLog"], QString("4294967295"));
    }
};
int main(int argc, char **argv)
{
    QTemporaryDir profile;
    qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setQuitOnLastWindowClosed(false);
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    ExpansionTests t;
    return QTest::qExec(&t, argc, argv);
}
#include "expansion_tests.moc"
