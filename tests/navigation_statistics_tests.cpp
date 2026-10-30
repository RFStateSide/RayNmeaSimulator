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
#include <QTemporaryDir>
#include <QNetworkProxy>
#include <QMessageBox>
#include <QAbstractButton>
#include <QLineEdit>
#include <QFontDatabase>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QComboBox>
#include <QAction>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QPushButton>
#include "enumcomboboxdelegate.h"
#include "templatevaluedelegate.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "appsettings.h"
#include "navigationstatistics.h"
#include "mainwindow.h"
#include "maincontroller.h"
#include "nmeareceiver.h"
#include "nmea2000diagnostics.h"
#include "nmeasenderworker.h"
#include "generatorvesselmovement.h"
#include "NMEA2000.h"
#include "layoutdocument.h"
#include "rawcanprotocol.h"

namespace {
QStringList codes{"129044", "129542", "129547", "65359"};
QMap<QString, QString> fixture(const QString &code)
{
    QMap<QString, QString> f;
    for (const auto &row : Expansion::parameters(code)) f[row[0]] = row[1];
    return f;
}
QMap<QString, QString> unpack(const QStringList &fields)
{
    QMap<QString, QString> f;
    for (int i = 0; i + 1 < fields.size(); i += 2) f[fields[i]] = fields[i + 1];
    return f;
}
tN2kMsg message(int pgn, const QByteArray &bytes, int source = 150)
{
    tN2kMsg m; m.SetPGN(pgn); m.Source = source;
    for (char b : bytes) m.AddByte(quint8(b));
    return m;
}
QByteArray bytes(const tN2kMsg &m)
{
    return QByteArray(reinterpret_cast<const char *>(m.Data), m.DataLen);
}
tN2kMsg claim(int source, int unique)
{
    tNMEA2000::tDeviceInformation d;
    d.SetName(0); d.SetUniqueNumber(unique); d.SetManufacturerCode(1851); d.SetIndustryGroup(4);
    QByteArray b;
    for (int i = 0; i < 8; ++i) b += char(d.GetName() >> (8 * i));
    return message(60928, b, source);
}
void settle()
{
    for (int i = 0; i < 8; ++i) QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
}
struct Workspace {
    QTimer dialogs;
    MainWindow window;
    MainController *controller = window.findChild<MainController *>();
    PgnTableModel *model = window.findChild<PgnTableModel *>();
    Workspace() {
        QObject::connect(&dialogs, &QTimer::timeout, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box) return;
            if (box->objectName() == "viewerEntryDialog") box->findChild<QAbstractButton *>("viewerKeepExisting")->click();
            else if (box->windowTitle() == "Widgets are Loaded") box->button(QMessageBox::Yes)->click();
            else if (box->objectName() == "layoutOperationDialog") box->button(QMessageBox::Discard)->click();
            else if (auto *no = box->button(QMessageBox::No)) no->click();
            else box->reject();
        });
        dialogs.start(0);
    }
    ExpansionInstrument *add(const QString &code) {
        auto *d = Expansion::definition(code);
        controller->addWidgetAsComboboxTextChanged(code, d->label, d->group); settle();
        return qobject_cast<ExpansionInstrument *>(controller->layoutWidgets(d->group).last());
    }
};
}
class NavigationStatisticsTests : public QObject {
    Q_OBJECT
private slots:
    void init() {
        AppSettings settings(APP_COMPANY, APP_NAME); settings.clear();
        settings.setValue("Nmea2000/Nmea2000Enabled", false);
        settings.setValue("Nmea0183/Nmea0183Enabled", false); settings.sync();
        if (!qEnvironmentVariableIsEmpty("RAYNMEA_DIAGNOSTIC_FONT")) {
            settings.setValue("Preference/FontFamily", QApplication::font().family()); settings.sync();
        }
    }
    void independentDatumVector() {
        // Independent little-endian signed field vector: -1e-7 deg, +0.25 deg,
        // -12.34 m. Four ASCII datum bytes, no variable-length/string header.
        const auto expected = QByteArray::fromHex("57383441ffffffffa02526002efbffff57383400");
        auto m = message(129044, expected); QStringList fields;
        QVERIFY(Expansion::decode(m, fields)); auto f = unpack(fields);
        QCOMPARE(f["LocalDatum"], "W84A"); QCOMPARE(f["ReferenceDatum"], "W84");
        QCOMPARE(f["DeltaLatitude"].toDouble(), -1e-7);
        QCOMPARE(f["DeltaLongitude"].toDouble(), .25); QCOMPARE(f["DeltaAltitude"].toDouble(), -12.34);
        tN2kMsg tx; QVERIFY(Expansion::encode("129044", f, tx)); QCOMPARE(bytes(tx), expected); QCOMPARE(tx.Priority, 6);
        QVERIFY(!Expansion::validField("129044", "LocalDatum", "W8"));
        QVERIFY(!Expansion::validField("129044", "ReferenceDatum", "W84AB"));
        m.Data[1] = 0; QVERIFY(!Expansion::decode(m, fields));
    }
    void statisticsVectors_data() {
        QTest::addColumn<QString>("code");
        QTest::newRow("noise") << QString("129542"); QTest::newRow("error") << QString("129547");
    }
    void statisticsVectors() {
        QFETCH(QString, code);
        // SID 7, 0,1,2 metres, 1 radian, 3,4,655.32 metres.
        const auto expected = QByteArray::fromHex("0700006400c80010272c019001fcff");
        auto m = message(code.toInt(), expected); QStringList fields;
        QVERIFY(Expansion::decode(m, fields)); const auto f = unpack(fields); const auto *d = Expansion::definition(code);
        const QList<double> values{7, 0, 1, 2, 1, 3, 4, 655.32};
        for (int i = 0; i < values.size(); ++i) QCOMPARE(f[d->fields[i].name].toDouble(), values[i]);
        tN2kMsg tx; QVERIFY(Expansion::encode(code, f, tx)); QCOMPARE(bytes(tx), expected);
        QCOMPARE(tx.Priority, 6); QCOMPARE(m.DataLen, 15);
    }
    void independentHeadingVector() {
        const auto expected = QByteArray::fromHex("3b9f07ffff1027a5");
        auto m = message(65359, expected, 204); QStringList fields;
        QVERIFY(Expansion::decode(m, fields)); auto f = unpack(fields);
        QCOMPARE(f["TrueHeading"].toDouble(), N2kDoubleNA); QCOMPARE(f["MagneticHeading"].toDouble(), 1.0);
        QCOMPARE(f["HeaderReserved"], "3"); QCOMPARE(f["ReservedByte"], "165");
        tN2kMsg tx; QVERIFY(Expansion::encode("65359", f, tx)); QCOMPARE(bytes(tx), expected); QCOMPARE(tx.Priority, 7);
        f["HeaderReserved"] = "0"; f["ReservedByte"] = "0";
        QVERIFY(Expansion::encode("65359", f, tx));
        QCOMPARE(bytes(tx), QByteArray::fromHex("3b8707ffff102700"));
        QVERIFY(Expansion::decode(tx, fields)); QCOMPARE(unpack(fields)["HeaderReserved"], "0");
        f["ManufacturerCode"] = "2046"; QVERIFY(!Expansion::encode("65359", f, tx));
        m.Data[0] ^= 1; QVERIFY(!Expansion::decode(m, fields));
        m.Data[0] ^= 1; m.Data[1] ^= 0x20; QVERIFY(!Expansion::decode(m, fields));
    }
    void advertisedCapabilities() {
        // Two in-memory pinned cores; their writes never reach a transport.
        RawCanProtocol node(true, 1234, 44), observer(false, 5678, 45);
        QList<tN2kMsg> lists;
        observer.deliver = [&](const tN2kMsg &m) { if (m.PGN == 126464) lists.append(m); };
        node.write = [&](quint32 id, quint8 length, const unsigned char *data) {
            WaveshareCodec::CanFrame frame; frame.id = id; frame.length = length;
            std::copy(data, data + length, frame.data.begin()); observer.frames.enqueue(frame); return true;
        };
        QTimer pump; connect(&pump, &QTimer::timeout, [&] { node.ParseMessages(); observer.ParseMessages(); }); pump.start(5);
        node.Open(); observer.Open(); QTRY_VERIFY_WITH_TIMEOUT(node.ready(), 3000);
        node.SendTxPGNList(255, 0); node.SendRxPGNList(255, 0);
        QTRY_COMPARE_WITH_TIMEOUT(lists.size(), 2, 3000);
        QSet<int> directions;
        for (const auto &m : lists) {
            QCOMPARE((m.DataLen - 1) % 3, 0); directions.insert(m.Data[0]);
            QSet<unsigned long> advertised;
            for (int at = 1; at < m.DataLen;) advertised.insert(m.Get3ByteUInt(at));
            for (auto pgn : NavigationStatistics::QualifiedPgns) if (pgn) QVERIFY(advertised.contains(pgn));
        }
        QCOMPARE(directions, (QSet<int>{0, 1}));
    }
    void actualPilotSelector() {
        Workspace w; w.controller->removeAllWidgets(); w.controller->setMode(Sender);
        auto *area = w.window.findChild<QMdiArea *>(); QVERIFY(area);
        QComboBox *selector = nullptr;
        for (auto *page : area->subWindowList()) if (page->windowTitle() == "Navigation") selector = page->findChild<QComboBox *>();
        QVERIFY(selector);
        const int index = selector->findText("Raymarine SeaTalk Pilot Heading (PGN 65359)"); QVERIFY(index > 0);
        selector->setCurrentIndex(index); settle();
        QCOMPARE(w.model->getPgns("65359").size(), 1); QCOMPARE(selector->currentIndex(), 0);
        QVERIFY(w.window.findChild<PilotHeading65359 *>());
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void rawFramePilotViewer() {
        Workspace w; w.controller->removeAllWidgets(); w.controller->setMode(Viewer);
        auto *handler = w.window.findChild<Nmea2000Handler *>(); QVERIFY(handler); handler->setMode(Viewer);
        RawCanProtocol core(false, 555, 44);
        core.deliver = [handler](const tN2kMsg &m) { handler->nmea2000MessageReceived(m); };
        QTimer pump; connect(&pump, &QTimer::timeout, [&] { core.ParseMessages(); }); pump.start(5);
        core.Open(); QTRY_VERIFY_WITH_TIMEOUT(core.IsOpen(), 3000);
        WaveshareCodec::CanFrame frame; frame.id = (7u << 26) | (65359u << 8) | 204; frame.length = 8;
        const auto data = QByteArray::fromHex("3b9f07ffff1027ff"); std::copy(data.begin(), data.end(), frame.data.begin());
        core.frames.enqueue(frame);
        QTRY_COMPARE_WITH_TIMEOUT(w.model->getPgns("65359").size(), 1, 3000);
        auto *widget = w.window.findChild<PilotHeading65359 *>(); QVERIFY(widget);
        QCOMPARE(widget->TrueHeading(), N2kDoubleNA); QCOMPARE(widget->MagneticHeading(), 1.0);
        frame.data[5] = 0x20; frame.data[6] = 0x4e; core.frames.enqueue(frame);
        QTRY_COMPARE(widget->MagneticHeading(), 2.0); QCOMPARE(w.model->getPgns("65359").size(), 1);
        frame.data[0] ^= 1; core.frames.enqueue(frame); QTest::qWait(30);
        QCOMPARE(widget->MagneticHeading(), 2.0); QCOMPARE(w.model->getPgns("65359").size(), 1);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void datumNamedAndUnlistedEditing() {
        Workspace w; w.controller->removeAllWidgets(); auto *widget = w.add("129044"); QVERIFY(widget);
        auto *combo = widget->findChild<QComboBox *>("LocalDatum"); QVERIFY(combo); QVERIFY(combo->isEditable());
        QCOMPARE(NavigationStatistics::datumValue(combo), "W84");
        const int w72 = combo->findData("W72"); QVERIFY(w72 >= 0); QVERIFY(combo->itemText(w72).contains("WGS 72"));
        combo->setCurrentIndex(w72); QVERIFY(QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, w72)));
        QCOMPARE(widget->value("LocalDatum"), "W72");
        combo->setEditText("ZZZA"); combo->lineEdit()->setModified(true);
        QVERIFY(QMetaObject::invokeMethod(combo->lineEdit(), "editingFinished")); QCOMPARE(widget->value("LocalDatum"), "ZZZA");
        widget->setValue("ReferenceDatum", "W84B");
        auto *ref = widget->findChild<QComboBox *>("ReferenceDatum"); QVERIFY(ref); QCOMPARE(NavigationStatistics::datumValue(ref), "W84B");
        for (int i = 0; i < 50; ++i) widget->setValue("ReferenceDatum", QString("X%1").arg(i, 2, 10, QChar('0')));
        QVERIFY(ref->count() <= 7); widget->setValue("ReferenceDatum", "W84B");
        w.model->setCurrentPgn(widget->Id()); const auto rows = w.model->getPgnParams(widget->Id());
        int row = -1; for (int i = 0; i < rows.size(); ++i) if (rows[i][0] == "ReferenceDatum") row = i;
        QVERIFY(row >= 0); const auto cell = w.model->index(row, 1);
        EnumComboBoxDelegate delegate; std::unique_ptr<QWidget> editor(delegate.createEditor(nullptr, QStyleOptionViewItem(), cell));
        auto *property = qobject_cast<QComboBox *>(editor.get()); QVERIFY(property); delegate.setEditorData(property, cell);
        QCOMPARE(NavigationStatistics::datumValue(property), "W84B");
        property->setEditText("ZZZB"); property->lineEdit()->setModified(true);
        delegate.setModelData(property, w.model, cell); settle(); QCOMPARE(widget->value("ReferenceDatum"), "ZZZB");
        property->setEditText("invalid-code"); delegate.setModelData(property, w.model, cell); settle();
        QCOMPARE(widget->value("ReferenceDatum"), "ZZZB");
        std::unique_ptr<QComboBox> templateEditor(TemplateValueDelegate::choices(nullptr, "LocalDatum", "QString", "ZZZA"));
        QVERIFY(templateEditor); QCOMPARE(NavigationStatistics::datumValue(templateEditor.get()), "ZZZA");
        tN2kMsg message; QVERIFY(Expansion::encode("129044", {{"LocalDatum", "ZZZA"}, {"ReferenceDatum", "ZZZB"},
             {"DeltaLatitude", "0"}, {"DeltaLongitude", "0"}, {"DeltaAltitude", "0"}}, message));
        QCOMPARE(bytes(message).left(4), QByteArray("ZZZA")); QCOMPARE(bytes(message).right(4), QByteArray("ZZZB"));
        QTemporaryDir dir; const auto file = dir.filePath("datum.json"); QVERIFY(w.controller->saveLayoutToJson(file));
        QVERIFY(w.controller->loadLayoutFromJson(file)); settle();
        widget = w.window.findChild<Datum129044 *>(); QVERIFY(widget); QCOMPARE(widget->value("ReferenceDatum"), "ZZZB");
    }
    void selectorOrderAndSelectability() {
        Workspace w; w.controller->removeAllWidgets();
        auto *area = w.window.findChild<QMdiArea *>(); QVERIFY(area);
        QTimer accept; connect(&accept, &QTimer::timeout, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (dialog && !qobject_cast<QMessageBox *>(dialog)) dialog->accept();
        }); accept.start(0);
        int entries = 0;
        for (auto *page : area->subWindowList()) {
            if (page->windowTitle() == "Generate") continue;
            auto *selector = page->findChild<QComboBox *>(); QVERIFY(selector); QCOMPARE(selector->itemText(0), "None");
            int previous = -1; bool sentences = false; QString previousName;
            QStringList labels;
            for (int i = 1; i < selector->count(); ++i) labels.append(selector->itemText(i));
            for (const auto &label : labels) {
                QRegularExpression pgn("\\(PGN ([0-9]+)\\)"); const auto match = pgn.match(label);
                if (match.hasMatch()) { QVERIFY(!sentences); const int number = match.captured(1).toInt(); QVERIFY(number >= previous); previous = number; }
                else { QVERIFY(label.contains("(SNT ")); sentences = true; QVERIFY(QString::compare(previousName, label, Qt::CaseInsensitive) <= 0); previousName = label; }
                const int before = w.model->getPgnList().size();
                selector->setCurrentIndex(selector->findText(label)); settle();
                QVERIFY2(w.model->getPgnList().size() > before, qPrintable(label));
                QCOMPARE(selector->currentIndex(), 0); ++entries;
            }
        }
        QVERIFY(entries > 50); QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void datumTemplateEditorRoundTrip() {
        Workspace w; w.controller->removeAllWidgets(); QVERIFY(w.add("129044"));
        bool visited = false, timeout = false; QElapsedTimer elapsed; elapsed.start();
        QTimer driver; connect(&driver, &QTimer::timeout, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()); if (!dialog) return;
            if (elapsed.elapsed() > 5000) { timeout = true; dialog->reject(); return; }
            if (visited || dialog->windowTitle() != "Save Template") return;
            visited = true;
            dialog->findChild<QLineEdit *>("templateName")->setText("Datum editing disposable");
            auto *table = dialog->findChild<QTableWidget *>("templateParameters"); QVERIFY(table);
            for (int row = 0; row < table->rowCount(); ++row) {
                const auto field = table->item(row, 1)->text();
                if (!NavigationStatistics::datumField(field)) continue;
                auto *editor = qobject_cast<QComboBox *>(table->cellWidget(row, 2)); QVERIFY(editor);
                if (field == "LocalDatum") editor->setCurrentIndex(editor->findData("W72"));
                else { editor->setEditText("ZZZ1"); editor->lineEdit()->setModified(true); }
            }
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        }); driver.start(10);
        QAction *save = nullptr;
        for (auto *action : w.window.findChildren<QAction *>()) if (action->text() == "Save as Template…") save = action;
        QVERIFY(save); save->trigger(); driver.stop(); QVERIFY(visited); QVERIFY(!timeout);
        QString error; const auto path = LayoutDocument::savePath("Datum editing disposable", error);
        QJsonObject document; QVERIFY2(LayoutDocument::read(path, document, error), qPrintable(error));
        QTemporaryDir dir; QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat); QJsonObject resolved;
        QVERIFY(LayoutDocument::resolve(document, settings, resolved, error));
        QVERIFY(w.controller->loadLayoutDocument(resolved, {}, [](const QString &) { return true; })); settle();
        auto *datum = w.window.findChild<Datum129044 *>(); QVERIFY(datum);
        QCOMPARE(datum->LocalDatum(), "W72"); QCOMPARE(datum->ReferenceDatum(), "ZZZ1");
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void fieldValidity_data() {
        QTest::addColumn<QString>("code");
        for (const auto &code : codes) QTest::newRow(qPrintable(code)) << code;
    }
    void fieldValidity() {
        QFETCH(QString, code);
        const auto *d = Expansion::definition(code);
        UserUnitPrefs prefs;
        for (const auto &field : d->fields) {
            if (field.type != "double") continue;
            for (double state : {0.0, N2kDoubleNA, NavigationStatistics::Error, NavigationStatistics::Reserved}) {
                auto f = fixture(code); f[field.name] = QString::number(state, 'g', 17);
                tN2kMsg m; QVERIFY(Expansion::encode(code, f, m)); QStringList out;
                QVERIFY(Expansion::decode(m, out)); QCOMPARE(unpack(out)[field.name].toDouble(), state);
                auto shown = Expansion::displayValue(code, field.name, f[field.name], prefs);
                if (state == N2kDoubleNA) QCOMPARE(shown, "Unavailable");
                if (state == NavigationStatistics::Error) QCOMPARE(shown, "Error");
                if (state == NavigationStatistics::Reserved) QCOMPARE(shown, "Reserved");
            }
            for (double boundary : {field.minimum, field.maximum}) {
                auto f = fixture(code); f[field.name] = QString::number(boundary, 'g', 17);
                tN2kMsg m; QVERIFY(Expansion::encode(code, f, m)); QStringList out; QVERIFY(Expansion::decode(m, out));
                QVERIFY(qAbs(unpack(out)[field.name].toDouble() - boundary) <= (field.unit == "deg" ? 1e-7 : .000001));
            }
            QVERIFY(!Expansion::validField(code, field.name, "nan"));
            QVERIFY(!Expansion::validField(code, field.name, "inf"));
        }
        tN2kMsg m; QVERIFY(Expansion::encode(code, fixture(code), m)); QStringList out;
        --m.DataLen; QVERIFY(!Expansion::decode(m, out)); m.DataLen += 2; QVERIFY(!Expansion::decode(m, out));
    }
    void wireSpecialValues() {
        QStringList out;
        auto m = message(129542, QByteArray::fromHex("fdfffffefffdff0000000000000000"));
        QVERIFY(Expansion::decode(m, out)); auto f = unpack(out);
        QCOMPARE(f["SID"], "253"); QCOMPARE(f["RMSPositionUncertainty"].toDouble(), N2kDoubleNA);
        QCOMPARE(f["MajorAxisDeviation"].toDouble(), NavigationStatistics::Error);
        QCOMPARE(f["MinorAxisDeviation"].toDouble(), NavigationStatistics::Reserved);
        auto datum = message(129044, QByteArray::fromHex("ffffffffffffff7ffeffff7ffdffff7fffffffff"));
        QVERIFY(Expansion::decode(datum, out)); f = unpack(out);
        QVERIFY(f["LocalDatum"].isEmpty()); QVERIFY(f["ReferenceDatum"].isEmpty());
        QCOMPARE(f["DeltaLatitude"].toDouble(), N2kDoubleNA);
        QCOMPARE(f["DeltaLongitude"].toDouble(), NavigationStatistics::Error);
        QCOMPARE(f["DeltaAltitude"].toDouble(), NavigationStatistics::Reserved);
        tN2kMsg tx; QVERIFY(Expansion::encode("129044", f, tx)); QCOMPARE(bytes(tx), bytes(datum));
    }
    void viewerOwnershipPersistence_data() { fieldValidity_data(); }
    void viewerOwnershipPersistence() {
        QFETCH(QString, code); Workspace w; w.controller->removeAllWidgets(); w.controller->setMode(Viewer);
        auto *handler = w.window.findChild<Nmea2000Handler *>(); QVERIFY(handler); handler->setMode(Viewer);
        tN2kMsg m; QVERIFY(Expansion::encode(code, fixture(code), m)); m.Source = 150;
        handler->nmea2000MessageReceived(claim(150, 42)); handler->nmea2000MessageReceived(m); settle();
        QCOMPARE(w.model->getPgns(code).size(), 1);
        const auto id = w.model->getPgns(code).first().idX;
        handler->nmea2000MessageReceived(m); settle(); QCOMPARE(w.model->getPgns(code).size(), 1);
        handler->nmea2000MessageReceived(claim(151, 42)); m.Source = 151; handler->nmea2000MessageReceived(m); settle();
        QCOMPARE(w.model->getPgns(code).size(), 1); QCOMPARE(w.model->getPgns(code).first().idX, id);
        QCOMPARE(w.model->getPgns(code).first().source, "151");
        handler->nmea2000MessageReceived(claim(150, 99)); m.Source = 150; handler->nmea2000MessageReceived(m); settle();
        QCOMPARE(w.model->getPgns(code).size(), 2);
        // Exercise the actual Property Editor model path and retained sentinels.
        const auto firstId = w.model->getPgns(code).first().idX;
        w.model->setCurrentPgn(firstId);
        const auto rows = w.model->getPgnParams(firstId);
        QString editedField;
        for (int i = 0; i < rows.size(); ++i) if (rows[i][3] == "double") {
            editedField = rows[i][0];
            QVERIFY(w.model->setData(w.model->index(i, 1), "Error", Qt::EditRole));
            QVERIFY(!w.model->setData(w.model->index(i, 1), "not-a-number", Qt::EditRole));
            break;
        }
        QVERIFY(!editedField.isEmpty());
        const auto precisionField = code == "129044" ? QString("DeltaLongitude")
                                   : code == "65359" ? QString("MagneticHeading")
                                   : code == "129542" ? QString("MajorAxisDeviation") : QString("MajorErrorEllipseDeviation");
        const QString fullPrecision = "0.12345678912345678";
        w.model->changeParam(firstId, precisionField, fullPrecision);
        QTemporaryDir dir; const auto path = dir.filePath("snapshot.json"); QVERIFY(w.controller->saveLayoutToJson(path));
        QVERIFY(w.controller->loadLayoutFromJson(path)); settle(); QCOMPARE(w.model->getPgns(code).size(), 2);
        QCOMPARE(w.model->getPgns(code).first().paramDetails, w.model->getPgnParams(firstId));
        bool retainedError = false;
        for (const auto &row : w.model->getPgnParams(firstId)) if (row[0] == editedField)
            retainedError = row[1].toDouble() == NavigationStatistics::Error;
        QVERIFY(retainedError);
        bool retainedPrecision = false;
        for (const auto &row : w.model->getPgnParams(firstId)) if (row[0] == precisionField)
            retainedPrecision = row[1] == fullPrecision;
        QVERIFY(retainedPrecision);
        // Same format-2 document through the existing User Template resolver.
        auto source = w.controller->captureLayout(); source["documentType"] = "template";
        source["template"] = QJsonObject{{"id", "disposable-m243d"}, {"name", "Four PGN fixture"}};
        source["setupPolicies"] = QJsonArray{};
        QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat); QJsonObject resolved; QString error;
        QVERIFY2(LayoutDocument::resolve(source, settings, resolved, error), qPrintable(error));
        QVERIFY2(w.controller->validateDocument(resolved, &error), qPrintable(error));
        const auto templatePath = dir.filePath("resolved.json"); QVERIFY(LayoutDocument::write(templatePath, resolved, error));
        QVERIFY(w.controller->loadLayoutFromJson(templatePath)); settle(); QCOMPARE(w.model->getPgns(code).size(), 2);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
        for (auto *instrument : w.window.findChildren<ExpansionInstrument *>()) {
            QCOMPARE(instrument->code(), code);
            for (const auto &field : Expansion::definition(code)->fields) QVERIFY(instrument->metaObject()->indexOfProperty(qPrintable(field.name)) >= 0);
        }
    }
    void unsupportedVariantDiagnostics() {
        Workspace w; w.controller->removeAllWidgets(); w.controller->setMode(Viewer);
        auto *h = w.window.findChild<Nmea2000Handler *>(); QVERIFY(h); h->setMode(Viewer);
        auto m = message(65359, QByteArray::fromHex("3b9f07ffff1027ff"), 204);
        h->nmea2000MessageReceived(m); settle(); QCOMPARE(w.model->getPgns("65359").size(), 1);
        QVERIFY(h->diagnostics()->messages().last().status.contains("decoder"));
        m.Data[0] ^= 1; h->nmea2000MessageReceived(m); settle();
        QCOMPARE(w.model->getPgns("65359").size(), 1);
        for (const auto &row : h->diagnostics()->messages()) if (row.pgn == 65359) {
            QVERIFY(!row.status.contains("instrument decoder")); QVERIFY(!row.payload.isEmpty());
        }
    }
    void directEditsAndDisplay() {
        Workspace w; w.controller->removeAllWidgets(); auto *datum = w.add("129044"); QVERIFY(datum);
        datum->setValue("DeltaLatitude", "-0.0000001");
        QCOMPARE(datum->findChild<QLineEdit *>("lineEditDeltaLatitude")->text(), "-0.0000001");
        auto *heading = w.add("65359"); QVERIFY(heading);
        auto *edit = heading->findChild<QLineEdit *>("lineEditTrueHeading"); QVERIFY(edit);
        edit->setText("Error"); edit->setModified(true); QVERIFY(QMetaObject::invokeMethod(edit, "editingFinished")); settle();
        QCOMPARE(heading->value("TrueHeading").toDouble(), NavigationStatistics::Error); QCOMPARE(edit->text(), "Error");
        heading->setValue("TrueHeading", "0"); QCOMPARE(edit->text(), "0.00");
        QCOMPARE(heading->findChild<QLineEdit *>("lineEditReservedByte")->text(), "255");
        auto *noise = w.add("129542"); QVERIFY(noise);
        noise->setValue("RMSPositionUncertainty", "1.25");
        UserUnitPrefs prefs = noise->UserPrefs(); prefs.depthUnitIndex = 1; noise->setUserPrefs(prefs); noise->setInitialGuiValues();
        QCOMPARE(noise->findChild<QLineEdit *>("lineEditRMSPositionUncertainty")->text(), "4.10");
        QCOMPARE(noise->value("RMSPositionUncertainty"), "1.25");
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
        if (!qEnvironmentVariableIsEmpty("RAYNMEA_INSTRUMENT_PREVIEW")) {
            w.add("129547");
            w.window.show(); w.window.resize(1450, 1000);
            auto *area = w.window.findChild<QMdiArea *>("mdiAreaMain"); QVERIFY(area);
            for (auto *page : area->subWindowList()) if (page->windowTitle() == "Navigation") area->setActiveSubWindow(page);
            QTest::qWait(150);
            QVERIFY(w.window.grab().save(qEnvironmentVariable("RAYNMEA_INSTRUMENT_PREVIEW")));
        }
    }
    void individualEnableAndInterval() {
        Workspace w; w.controller->removeAllWidgets();
        for (const auto &code : codes) w.add(code);
        auto *tables = w.window.findChild<PgnTableController *>(); QVERIFY(tables);
        NmeaSenderWorker scheduler; scheduler.setPgnTableController(tables);
        // Local spy only: never connect this scheduler to any transport handler.
        QSignalSpy submissions(&scheduler, &NmeaSenderWorker::sendPgn);
        for (const auto &data : w.model->getPgnList()) {
            w.model->setPgnEnabled(data.idX, data.pgn != "65359");
            w.model->setSendInterval(data.idX, 10000);
        }
        scheduler.sendNmeaMessage(); QCOMPARE(submissions.size(), 0);
        scheduler.startSending(); scheduler.sendNmeaMessage(); QCOMPARE(submissions.size(), 3);
        scheduler.sendNmeaMessage(); QCOMPARE(submissions.size(), 3); // Not yet due.
        scheduler.stopSending(); scheduler.sendNmeaMessage(); QCOMPARE(submissions.size(), 3);
        scheduler.setMode(Viewer); scheduler.startSending(); QVERIFY(!scheduler.isSending());
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void optionalHeadingBindings_data() { QTest::addColumn<bool>("sourceFirst"); QTest::newRow("generator-first") << true; QTest::newRow("instrument-first") << false; }
    void optionalHeadingBindings() {
        QFETCH(bool, sourceFirst); Workspace w; w.controller->removeAllWidgets(); w.controller->setMode(Sender);
        ExpansionInstrument *heading = nullptr;
        if (!sourceFirst) heading = w.add("65359");
        w.controller->addGeneratorWidget("Vessel Movement (PGN -129025)"); settle();
        auto *movement = w.window.findChild<GeneratorVesselMovement *>(); QVERIFY(movement);
        if (sourceFirst) heading = w.add("65359"); QVERIFY(heading);
        QSignalSpy changes(heading, &ExpansionInstrument::parameterValueChanged);
        movement->setEnabled(true); movement->setDispatch(true);
        movement->setHeadingReference(N2khr_true); movement->setVariation(.1); movement->setHeading(1.2); settle();
        QVERIFY(qAbs(heading->value("TrueHeading").toDouble() - 1.2) < 1e-10);
        int trueDeliveries = 0;
        for (const auto &args : changes) if (args[0].toString() == "TrueHeading") ++trueDeliveries;
        QCOMPARE(trueDeliveries, 1);
        movement->setDispatch(false); movement->setHeading(2.1); settle();
        QVERIFY(qAbs(heading->value("TrueHeading").toDouble() - 1.2) < 1e-10);
        movement->setDispatch(true); movement->setHeading(2.2); settle();
        QVERIFY(qAbs(heading->value("TrueHeading").toDouble() - 2.2) < 1e-10);
        QVERIFY(!w.window.findChild<NmeaSenderWorker *>()->isSending());
    }
    void archivedVesselReplay() {
        const QString path = qEnvironmentVariable("RAYNMEA_DIAGNOSTIC_REPLAY");
        if (path.isEmpty()) QSKIP("Private archived vessel replay not supplied");
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto doc = QJsonDocument::fromJson(file.readAll());
        QMap<int, int> count, invalidAngles;
        for (const auto value : doc.array()) {
            auto o = value.toObject(); const int pgn = o["pgn"].toInt();
            if (!codes.contains(QString::number(pgn))) continue;
            auto m = message(pgn, QByteArray::fromHex(o["payload_hex"].toString().toLatin1()), o["source"].toInt());
            QStringList fields; QVERIFY2(Expansion::decode(m, fields), qPrintable(QString::number(pgn)));
            auto f = unpack(fields); tN2kMsg tx; QVERIFY(Expansion::encode(QString::number(pgn), f, tx));
            const int orientation = pgn == 129542 || pgn == 129547 ? quint8(m.Data[7]) | (quint8(m.Data[8]) << 8) : 0;
            if ((pgn == 129542 || pgn == 129547) && orientation > 62832 && orientation < 65533) {
                ++invalidAngles[pgn];
                QCOMPARE(f[pgn == 129542 ? "MajorAxisOrientation" : "ErrorEllipseOrientation"].toDouble(), NavigationStatistics::Error);
                auto expected = bytes(m); expected[7] = char(0xfe); expected[8] = char(0xff);
                QCOMPARE(bytes(tx), expected);
            } else QCOMPARE(bytes(tx), bytes(m));
            ++count[pgn];
            if (pgn == 65359) QCOMPARE(f["TrueHeading"].toDouble(), N2kDoubleNA);
            if (pgn == 129547) QCOMPARE(f["RMSRangeInputDeviation"].toDouble(), 0.0);
        }
        for (const auto &code : codes) QVERIFY2(count[code.toInt()] > 0, qPrintable(code));
        QCOMPARE(count[129044], 18); QCOMPARE(count[129542], 94); QCOMPARE(count[129547], 94); QCOMPARE(count[65359], 947);
        QCOMPARE(invalidAngles[129542], 10); QCOMPARE(invalidAngles[129547], 10);
    }
};
int main(int argc, char **argv)
{
    QTemporaryDir profile; qputenv("RAYNMEA_TEST_HOME", profile.path().toUtf8());
    QApplication app(argc, argv); app.setStyle("Fusion"); app.setQuitOnLastWindowClosed(false);
    const auto fontPath = qEnvironmentVariable("RAYNMEA_DIAGNOSTIC_FONT");
    if (!fontPath.isEmpty()) {
        const int id = QFontDatabase::addApplicationFont(fontPath);
        const auto families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty()) app.setFont(QFont(families.first(), 10));
    }
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 9));
    NavigationStatisticsTests t; return QTest::qExec(&t, argc, argv);
}
#include "navigation_statistics_tests.moc"
