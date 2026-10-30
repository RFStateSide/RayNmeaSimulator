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
#include "applicationlog.h"
#include "NMEA2000.h"
#include "apppaths.h"
#include "appsettings.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSet>
#include <QLockFile>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#ifdef Q_OS_LINUX
#include <linux/can.h>
#endif

namespace ApplicationLog
{
Options Options::load(QSettings &s)
{
    Options o;
    s.beginGroup("Logging");
    o.level = qBound(0, s.value("Level", 1).toInt(), 3);
    o.bindings = s.value("Bindings", false).toBool();
    o.rx = s.value("RX", true).toBool();
    o.tx = s.value("TX", true).toBool();
    o.nmea2000 = s.value("NMEA2000", true).toBool();
    o.nmea0183 = s.value("NMEA0183", true).toBool();
    o.rawCan = s.value("RawCAN", true).toBool();
    o.applicationMiB = qBound(1, s.value("ApplicationMiB", 2).toInt(), 16);
    o.trafficMiB = qBound(1, s.value("TrafficMiB", 8).toInt(), 64);
    o.disabledCategories = s.value("DisabledCategories").toStringList();
    s.endGroup();
    return o;
}
void Options::save(QSettings &s) const
{
    s.beginGroup("Logging");
    s.setValue("Level", level);
    s.setValue("Bindings", bindings);
    s.setValue("RX", rx);
    s.setValue("TX", tx);
    s.setValue("NMEA2000", nmea2000);
    s.setValue("NMEA0183", nmea0183);
    s.setValue("RawCAN", rawCan);
    s.setValue("ApplicationMiB", applicationMiB);
    s.setValue("TrafficMiB", trafficMiB);
    s.setValue("DisabledCategories", disabledCategories);
    s.endGroup();
    // An active recording is intentionally never a persisted preference.
}
namespace
{
int rank(QtMsgType t)
{
    switch (t) {
    case QtDebugMsg:
        return 0;
    case QtInfoMsg:
        return 1;
    case QtWarningMsg:
        return 2;
    case QtCriticalMsg:
        return 3;
    case QtFatalMsg:
        return 4;
    }
    return 3;
}
QString severity(QtMsgType t)
{
    return QStringList{"Debug", "Information", "Warning", "Critical", "Fatal"}.value(rank(t));
}
constexpr int RecordLimit = 16384;
qsizetype queueCapacity(qsizetype normal)
{
#ifdef RAYNMEA_TESTING
    // Deterministic backpressure fixture, analogous to the existing injected
    // serial/backend capacities. Never configurable in a shipping build.
    bool ok = false;
    const auto requested = qEnvironmentVariableIntValue("RAYNMEA_LOG_TEST_QUEUE_LIMIT", &ok);
    if (ok)
        return qBound(qsizetype(1024), qsizetype(requested), normal);
#endif
    return normal;
}
struct Record {
    QByteArray data;
    quint64 generation;
};
// Each channel has its own thread, queue, QFile, lock and retention budget.
class Sink
{
    mutable std::mutex mutex;
    mutable std::mutex catalogMutex;
    std::condition_variable wake, drained;
    std::deque<Record> queue;
    qsizetype queued = 0;
    const qsizetype capacity;
    std::thread worker;
    bool quit = false, desired = false, clearRequested = false, dirty = false, busy = false;
    QString exportPath, detail = "Off";
    qint64 limit;
    std::atomic<quint64> epoch{0};
    std::atomic<bool> accepting{false}, clearInProgress{false};
    std::atomic<quint64> lost{0}, count{0};
    const QString path;
    void setDetail(const QString &s)
    {
        std::lock_guard<std::mutex> lock(mutex);
        detail = s;
    }
    void run()
    {
        QFile file(path);
        std::unique_ptr<QLockFile> ownership;
        qint64 appliedLimit = 0;
        auto close = [&] {
            if (file.isOpen()) {
                file.flush();
                file.close();
            }
            ownership.reset();
            appliedLimit = 0;
        };
        auto fail = [&](const QString &why) {
            close();
            accepting = false;
            std::lock_guard<std::mutex> lock(mutex);
            desired = false;
            lost += queue.size();
            queue.clear();
            queued = 0;
            detail = "Failed: " + why;
            std::fprintf(stderr, "RayNmeaSimulator logging: %s\n", qUtf8Printable(why));
        };
        auto open = [&] {
            const auto directory = QFileInfo(path).absolutePath();
            if (QFileInfo(directory).isSymLink() || QFileInfo(path).isSymLink() ||
                QFileInfo(path + ".lock").isSymLink() || !QDir().mkpath(directory))
                return false;
#ifndef Q_OS_WIN
            QFile::setPermissions(directory,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
#endif
            ownership = std::make_unique<QLockFile>(path + ".lock");
            if (!ownership->tryLock(0))
                return false;
            if (!file.open(QIODevice::ReadWrite | QIODevice::Append))
                return false;
            // An interrupted final write must not swallow the next session's
            // first JSON record. Retain the old bytes and only delimit its tail.
            if (file.size() > 0) {
                const auto end = file.size();
                if (!file.seek(end - 1))
                    return false;
                const auto last = file.read(1);
                if (last.size() != 1 || !file.seek(end))
                    return false;
                if (last != "\n" && file.write("\n", 1) != 1)
                    return false;
            }
            file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            return true;
        };
        auto rotate = [&](qint64 next, qint64 maximum) {
            if (file.size() + next <= maximum)
                return true;
            if (!file.flush())
                return false;
            file.close();
            for (int i = 4; i >= 1; --i) {
                const auto to = path + "." + QString::number(i);
                const auto from = i == 1 ? path : path + "." + QString::number(i - 1);
                if (QFileInfo(to).isSymLink() || QFileInfo(from).isSymLink())
                    return false;
                if (i == 4 && QFile::exists(to) && !QFile::remove(to))
                    return false;
                if (QFile::exists(from) && !QFile::rename(from, to))
                    return false;
            }
            if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
                return false;
            file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            return true;
        };
        for (;;) {
            std::deque<Record> batch;
            bool want, doClear, finish;
            QString destination;
            qint64 maximum;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait_for(lock, std::chrono::milliseconds(250),
                              [&] { return quit || dirty || queued >= 65536; });
                want = desired;
                doClear = clearRequested;
                clearRequested = false;
                destination = exportPath;
                exportPath.clear();
                maximum = limit;
                finish = quit;
                dirty = false;
                busy = true;
                qsizetype bytes = 0;
                while (!queue.empty() && bytes < 65536) {
                    bytes += queue.front().data.size();
                    queued -= queue.front().data.size();
                    batch.push_back(std::move(queue.front()));
                    queue.pop_front();
                }
            }
            const bool hadClear = doClear;
            std::unique_lock<std::mutex> catalog(catalogMutex);
            if ((want || doClear || !destination.isEmpty()) && !file.isOpen()) {
                if (!open()) {
                    fail("Log storage unavailable, read-only, or owned by another application instance");
                    want = false;
                    doClear = false;
                    destination.clear();
                } else if (want) {
                    std::lock_guard<std::mutex> lock(mutex);
                    accepting = desired;
                    detail = desired ? "Recording" : "Stopping";
                }
            }
            if (file.isOpen() && maximum != appliedLimit) {
                bool ok = rotate(0, maximum);
                for (int i = 1; i <= 4 && ok; ++i) {
                    const auto archive = path + "." + QString::number(i);
                    QFileInfo info(archive);
                    if (info.isSymLink())
                        ok = false;
                    else if (info.exists() && info.size() > maximum)
                        ok = QFile::remove(archive);
                }
                if (!ok)
                    fail("Unable to enforce bounded log retention");
                else
                    appliedLimit = maximum;
            }
            if (doClear && file.isOpen()) {
                if (!file.resize(0) || !file.seek(0))
                    fail("Unable to clear current log");
            }
            if (file.isOpen() && want)
                for (size_t index = 0; index < batch.size(); ++index) {
                    const auto &record = batch[index];
                    if (record.generation != epoch.load()) {
                        ++lost;
                        continue;
                    }
                    if (!rotate(record.data.size(), maximum) ||
                        file.write(record.data) != record.data.size()) {
                        lost += batch.size() - index;
                        fail("Log write or rotation failed; recording stopped");
                        break;
                    }
                    ++count;
                }
            else
                lost += batch.size();
            if (file.isOpen() && !file.flush())
                fail("Unable to flush log; recording stopped");
            if (!destination.isEmpty() && file.isOpen()) {
                QFile input(path);
                QSaveFile output(destination);
                bool ok = QFileInfo(destination).absoluteFilePath() != QFileInfo(path).absoluteFilePath();
                ok = ok && input.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly);
                qint64 bytes = 0;
                while (ok && !input.atEnd()) {
                    const auto b = input.read(65536);
                    bytes += b.size();
                    ok = !b.isEmpty() && bytes <= maximum && output.write(b) == b.size();
                }
                if (ok)
                    ok = output.commit();
                if (!ok) {
                    output.cancelWriting();
                    setDetail("Export failed; original log retained");
                }
            }
            if (!want) {
                close();
                accepting = false;
                std::lock_guard<std::mutex> lock(mutex);
                if (!detail.startsWith("Failed") && !detail.startsWith("Export failed"))
                    detail = "Off";
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                busy = false;
                if (hadClear && !clearRequested)
                    clearInProgress = false;
                drained.notify_all();
                if (finish && queue.empty())
                    break;
            }
        }
        close();
    }

  public:
    Sink(QString name, qsizetype bytes, qint64 maximum)
        : capacity(queueCapacity(bytes)), limit(maximum), path(std::move(name))
    {
        worker = std::thread([this] { run(); });
    }
    ~Sink()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            quit = true;
            dirty = true;
        }
        wake.notify_one();
        worker.join();
    }
    void enable(bool value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (value && desired)
            return;
        ++epoch; // Every new/retried capture has a distinct framing generation.
        desired = value;
        dirty = true;
        if (!value) {
            accepting = false;
            lost += queue.size();
            queue.clear();
            queued = 0;
            detail = "Stopping";
        } else
            detail = "Starting";
        wake.notify_one();
    }
    void size(qint64 value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        limit = value;
        dirty = true;
        wake.notify_one();
    }
    bool post(QByteArray data, bool important = false, quint64 expectedGeneration = 0)
    {
        if (data.size() > RecordLimit) {
            ++lost;
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex);
        if (!desired || quit)
            return false;
        if (expectedGeneration && expectedGeneration != epoch.load()) {
            ++lost;
            return false;
        }
        while (important && !queue.empty() && queued + data.size() > capacity) {
            queued -= queue.front().data.size();
            queue.pop_front();
            ++lost;
        }
        if (queued + data.size() > capacity) {
            ++lost;
            return false;
        }
        queued += data.size();
        queue.push_back({std::move(data), epoch.load()});
        wake.notify_one();
        return true;
    }
    bool active() const
    {
        return accepting;
    }
    quint64 generation() const
    {
        return epoch;
    }
    QString status() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return detail;
    }
    QString name() const
    {
        return path;
    }
    int tail(QByteArray &output, int maximumBytes) const
    {
        std::unique_lock<std::mutex> catalog(catalogMutex, std::try_to_lock);
        if (!catalog.owns_lock())
            return 0;
        QFile input(path);
        if (!input.open(QIODevice::ReadOnly))
            return -1;
        const auto maximum = qBound(1, maximumBytes, 256 * 1024);
        const auto offset = qMax<qint64>(0, input.size() - maximum);
        if (!input.seek(offset))
            return -1;
        output = input.read(maximum);
        if (offset > 0)
            output = output.mid(output.indexOf('\n') + 1);
        return 1;
    }
    quint64 drops() const
    {
        return lost;
    }
    quint64 records() const
    {
        return count;
    }
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++epoch;
        lost += queue.size();
        queue.clear();
        queued = 0;
        clearRequested = true;
        clearInProgress = true;
        dirty = true;
        wake.notify_one();
    }
    bool clearing() const
    {
        return clearInProgress;
    }
    void exportTo(const QString &destination)
    {
        std::lock_guard<std::mutex> lock(mutex);
        exportPath = destination;
        dirty = true;
        wake.notify_one();
    }
    bool flush(int ms)
    {
        std::unique_lock<std::mutex> lock(mutex);
        dirty = true;
        wake.notify_one();
        return drained.wait_for(lock, std::chrono::milliseconds(ms),
                                [&] { return queue.empty() && !busy && !dirty; });
    }
};
struct State {
    std::mutex mutex, onceMutex;
    QSet<quint32> reportedPgns;
    Options config;
    QStringList secrets;
    std::function<void()> snapshot;
    QHash<QString, QPair<qint64, quint64>> repeats;
    QString session = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::atomic<quint64> sequence{0};
    std::atomic<bool> traffic{false};
    Sink app, wire;
    explicit State(const QString &directory)
        : app(directory + "/application.jsonl", 256 * 1024, 2 * 1024 * 1024),
          wire(directory + "/traffic.jsonl", 1024 * 1024, 8 * 1024 * 1024)
    {
    }
    QJsonObject stamp()
    {
        return {{"schema", 1},
                {"utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                {"elapsed_us", double(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now() - start)
                                          .count())},
                {"sequence", QString::number(++sequence)},
                {"session", session}};
    }
};
std::shared_ptr<State> state;
QtMessageHandler previous = nullptr;
std::shared_ptr<State> current()
{
    return std::atomic_load(&state);
}
QByteArray bytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}
void handler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    thread_local bool recursion = false;
    if (recursion)
        return;
    recursion = true;
    auto category = QString::fromUtf8(context.category ? context.category : "default");
    if (category == "default") {
        // Reuse legacy Qt messages without duplicating instrumentation. File
        // context is optional in Release; message-family hints are a fallback.
        const auto source = QString::fromUtf8(context.file ? context.file : "");
        if (source.contains("nmea0183handler") || message.contains("NMEA0183"))
            category = "raynmea.protocol.nmea0183";
        else if (source.contains("nmea2000handler") || message.contains("PGN"))
            category = "raynmea.protocol.nmea2000";
        else if (message.contains("SocketCAN"))
            category = "raynmea.transport.socketcan";
        else if (message.contains("Waveshare") || message.contains("CANable") ||
                 message.contains("Actisense"))
            category = "raynmea.transport.serial";
        else if (message.contains("Network CAN"))
            category = "raynmea.transport.network";
        else if (message.contains("Signal K"))
            category = "raynmea.transport.signalk";
        else if (source.contains("layoutcontroller") || message.contains("Layout", Qt::CaseInsensitive))
            category = "raynmea.layout";
        else if (message.contains("Generator") || message.contains("engine", Qt::CaseInsensitive))
            category = "raynmea.generator";
        else if (source.endsWith("main.cpp"))
            category = "raynmea.app";
        else
            category = "qt.default";
    }
    event(category, message, type);
    recursion = false;
}
struct Capture {
    std::shared_ptr<State> owner;
    quint64 generation = 0;
    explicit operator bool() const
    {
        return bool(owner);
    }
};
Capture captureFor(const char *direction, bool nmea2000, bool raw)
{
    auto s = current();
    if (!s)
        return {};
    const auto generation = s->wire.generation();
    if (!s->traffic || !s->wire.active())
        return {};
    std::lock_guard<std::mutex> lock(s->mutex);
    if (!(direction[0] == 'R' ? s->config.rx : s->config.tx) ||
        !(nmea2000 ? s->config.nmea2000 : s->config.nmea0183) || (raw && !s->config.rawCan))
        return {};
    return {s, generation};
}
void trafficRecord(QJsonObject record, const Capture &capture)
{
    const auto &s = capture.owner;
    if (!s || !s->traffic)
        return;
    auto object = s->stamp();
    object["capture_generation"] = QString::number(capture.generation);
    for (auto i = record.begin(); i != record.end(); ++i)
        object[i.key()] = i.value();
    s->wire.post(bytes(object), false, capture.generation);
}
} // namespace
bool initialize(const QString &directory)
{
    if (current())
        return true;
    const auto path = directory.isEmpty() ? AppPaths::logsDirectory() : directory;
    if (!QDir::isAbsolutePath(path))
        return false;
    std::shared_ptr<State> s;
    try {
        s = std::make_shared<State>(path);
    } catch (const std::exception &) {
        std::fprintf(stderr,
                     "RayNmeaSimulator: application logger could not start; application remains usable.\n");
        return false;
    }
    AppSettings settings("RFStateSide", "RayNmeaSimulator");
    s->config = Options::load(settings);
    std::atomic_store(&state, s);
    configure(s->config);
    s->app.enable(true);
    previous = qInstallMessageHandler(handler);
    event("raynmea.app", "Application log started; traffic recording is Off");
    return true;
}
void shutdown()
{
    auto s = current();
    if (!s)
        return;
    stopTraffic();
    event("raynmea.app", "Application log stopped");
    s->app.flush(3000);
    s->wire.flush(3000);
    qInstallMessageHandler(previous);
    std::atomic_store(&state, std::shared_ptr<State>());
}
void configure(const Options &input)
{
    auto s = current();
    if (!s)
        return;
    Options o = input;
    o.level = qBound(0, o.level, 3);
    o.applicationMiB = qBound(1, o.applicationMiB, 16);
    o.trafficMiB = qBound(1, o.trafficMiB, 64);
    std::function<void()> snapshot;
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        if (o.bindings && !s->config.bindings)
            snapshot = s->snapshot;
        s->config = o;
    }
    s->app.size(qint64(o.applicationMiB) * 1024 * 1024);
    s->wire.size(qint64(o.trafficMiB) * 1024 * 1024);
    if (snapshot)
        snapshot();
}
Options options()
{
    auto s = current();
    if (!s)
        return {};
    std::lock_guard<std::mutex> lock(s->mutex);
    return s->config;
}
void registerSecret(const QString &value)
{
    auto s = current();
    if (!s || value.isEmpty())
        return;
    std::lock_guard<std::mutex> lock(s->mutex);
    if (!s->secrets.contains(value))
        s->secrets.append(value);
}
QString sanitized(QString text)
{
    if (text.size() > 4096)
        return QString("[Oversized diagnostic omitted: %1 characters]").arg(text.size());
    if (auto s = current()) {
        std::lock_guard<std::mutex> lock(s->mutex);
        for (const auto &secret : s->secrets)
            text.replace(secret, "[redacted]");
    }
    text.replace(QRegularExpression("https?://[^\\s\\\"<>]+", QRegularExpression::CaseInsensitiveOption),
                 "[URL omitted]");
    text.replace(QRegularExpression("Bearer\\s+[^\\s,;]+", QRegularExpression::CaseInsensitiveOption),
                 "Bearer [redacted]");
    text.replace(
        QRegularExpression(
            R"rx(((?:"|')?(?:authorization|token|api[_ -]?key|password|proof|nonce)(?:"|')?\s*[:=]\s*)(?:"[^"]*"|'[^']*'|[^\s,;}]+))rx",
            QRegularExpression::CaseInsensitiveOption),
        "\\1[redacted]");
    text.replace(
        QRegularExpression(
            "((?:authorization|bearer|token|api[_ -]?key|password|proof|nonce)\\s*[:=]?\\s*)[^\\s,;]+",
            QRegularExpression::CaseInsensitiveOption),
        "\\1[redacted]");
    if (text.contains("Invalid NMEA0183 message:") || text.contains("Invalid parameter format:"))
        text = text.section(':', 0, 0) + ": [payload omitted]";
    text.replace(QDir::homePath(), "~");
    text.replace('\n', "\\n");
    text.replace('\r', "\\r");
    return text.left(4096);
}
void event(const QString &category, const QString &message, QtMsgType type)
{
    auto s = current();
    if (!s)
        return;
    Options o = options();
    if (rank(type) < o.level || (category == "raynmea.autopilot.navigation" && type == QtDebugMsg))
        return;
    if (message.contains("Binding ") || message.contains("Un-Binding "))
        return;
    for (const auto &disabled : o.disabledCategories)
        if (!disabled.isEmpty() && category.startsWith(disabled) && rank(type) < 3)
            return;
    const auto clean = sanitized(message);
    quint64 suppressed = 0;
    if (rank(type) < 3) {
        const auto key = category + '|' + QString::number(rank(type)) + '|' + clean;
        const auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - s->start)
                .count();
        std::lock_guard<std::mutex> lock(s->mutex);
        if (s->repeats.contains(key)) {
            auto &entry = s->repeats[key];
            const int window =
                clean.contains("unsupported", Qt::CaseInsensitive) && clean.contains("PGN") ? 60000 : 1000;
            if (now - entry.first < window) {
                ++entry.second;
                return;
            }
            suppressed = entry.second;
            entry = {now, 0};
        } else {
            if (s->repeats.size() >= 256)
                s->repeats.erase(s->repeats.begin());
            s->repeats.insert(key, {now, 0});
        }
    }
    auto object = s->stamp();
    object["severity"] = severity(type);
    object["category"] = category;
    object["message"] = clean;
    if (suppressed)
        object["suppressed_repeats"] = QString::number(suppressed);
    s->app.post(bytes(object), rank(type) >= 3);
#ifdef QT_DEBUG
    std::fprintf(stderr, "%s %s %s: %s\n", qUtf8Printable(object["utc"].toString()),
                 qUtf8Printable(severity(type)), qUtf8Printable(category),
                 qUtf8Printable(object["message"].toString()));
#endif
    if (type == QtFatalMsg) {
        std::fprintf(stderr, "RayNmeaSimulator fatal: %s\n", qUtf8Printable(sanitized(message)));
    }
}
bool reportUndecodedPgn(quint32 pgn)
{
    auto s = current();
    static QLoggingCategory category("raynmea.protocol.nmea2000");
    if (!s || pgn > 0x3ffff || !category.isDebugEnabled()) return false;
    std::lock_guard<std::mutex> once(s->onceMutex);
    if (s->reportedPgns.contains(pgn)) return false;
    {
        std::lock_guard<std::mutex> config(s->mutex);
        if (s->config.level != 0) return false;
        for (const auto &quiet : s->config.disabledCategories)
            if (!quiet.isEmpty() && QString::fromLatin1(category.categoryName()).startsWith(quiet)) return false;
    }
    auto record = s->stamp();
    record["severity"] = "Debug";
    record["category"] = category.categoryName();
    record["message"] = QString("Received NMEA2000 PGN %1: application decoder unavailable").arg(pgn);
    record["pgn"] = double(pgn);
    if (!s->app.post(bytes(record))) return false;
    s->reportedPgns.insert(pgn);
#ifdef QT_DEBUG
    std::fprintf(stderr,"%s Debug %s: %s\n",qUtf8Printable(record["utc"].toString()),category.categoryName(),qUtf8Printable(record["message"].toString()));
#endif
    return true;
}
bool bindingEnabled()
{
    return current() && options().bindings;
}
void onBindingsEnabled(QObject *context, std::function<void()> callback)
{
    auto s = current();
    if (!s)
        return;
    QPointer<QObject> guard(context);
    auto snapshot = [guard, callback] {
        if (guard)
            QMetaObject::invokeMethod(guard, callback, Qt::QueuedConnection);
    };
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        s->snapshot = snapshot;
    }
    if (bindingEnabled())
        snapshot();
}
void binding(const QJsonObject &fields)
{
    auto s = current();
    if (!s || !bindingEnabled())
        return;
    auto object = s->stamp();
    object["severity"] = "Information";
    object["category"] = "raynmea.binding";
    for (auto i = fields.begin(); i != fields.end(); ++i)
        object[i.key()] = i.value();
    s->app.post(bytes(object));
}
bool startTraffic()
{
    auto s = current();
    if (!s || s->wire.status() == "Stopping" || (s->traffic && !s->wire.status().startsWith("Failed")))
        return false;
    s->traffic = true;
    s->wire.enable(true);
    trafficRecord({{"kind", "capture-session"},
                   {"qt", qVersion()},
                   {"application_version", QCoreApplication::applicationVersion()},
                   {"raw_tx", "not-captured"},
                   {"delivery_semantics", "Software observations do not confirm physical delivery"}},
                  Capture{s, s->wire.generation()});
    event("raynmea.app", "Traffic recording requested (private vessel data)");
    return true;
}
void stopTraffic()
{
    auto s = current();
    if (!s || !s->traffic.exchange(false))
        return;
    s->wire.enable(false);
    event("raynmea.app", "Traffic recording stopped");
}
bool recording()
{
    auto s = current();
    return s && s->traffic && s->wire.active();
}
quint64 trafficGeneration()
{
    auto s = current();
    return s ? s->wire.generation() : 0;
}
QString trafficState()
{
    auto s = current();
    if (!s)
        return "Off";
    if (s->traffic && s->wire.active())
        return "Recording";
    const auto status = s->wire.status();
    if (!s->traffic && status != "Stopping")
        return "Off";
    return status;
}
QString health(bool traffic)
{
    auto s = current();
    return s ? (traffic ? s->wire.status() : s->app.status()) : "Logging is not initialized";
}
QString filePath(bool traffic)
{
    auto s = current();
    return s ? (traffic ? s->wire.name() : s->app.name()) : QString();
}
int readTail(bool traffic, QByteArray &output, int maximumBytes)
{
    auto s = current();
    return s ? (traffic ? s->wire : s->app).tail(output, maximumBytes) : -1;
}
quint64 dropped(bool traffic)
{
    auto s = current();
    return s ? (traffic ? s->wire.drops() : s->app.drops()) : 0;
}
quint64 written(bool traffic)
{
    auto s = current();
    return s ? (traffic ? s->wire.records() : s->app.records()) : 0;
}
void clear(bool traffic)
{
    if (auto s = current())
        (traffic ? s->wire : s->app).clear();
}
bool clearing(bool traffic)
{
    auto s = current();
    return s && (traffic ? s->wire : s->app).clearing();
}
void exportCurrent(bool traffic, const QString &destination)
{
    if (auto s = current()) {
        const auto target = QFileInfo(destination).absoluteFilePath();
#ifdef Q_OS_WIN
        constexpr auto pathCase = Qt::CaseInsensitive;
#else
        constexpr auto pathCase = Qt::CaseSensitive;
#endif
        for (const auto &owned : {s->app.name(), s->wire.name()}) {
            const auto base = QFileInfo(owned).absoluteFilePath();
            if (target.compare(base, pathCase) == 0 || target.startsWith(base + '.', pathCase)) {
                event("raynmea.app", "Export rejected: choose a destination outside live log files/archives",
                      QtWarningMsg);
                return;
            }
        }
        (traffic ? s->wire : s->app).exportTo(destination);
    }
}
void retryApplication()
{
    if (auto s = current())
        s->app.enable(true);
}
bool flush(int timeoutMs)
{
    auto s = current();
    return s && s->app.flush(timeoutMs) && s->wire.flush(timeoutMs);
}
void n2k(const char *direction, const QString &transport, const tN2kMsg &m, const QString &status,
         int sessionSource)
{
    const auto capture = captureFor(direction, true, false);
    if (!capture)
        return;
    trafficRecord(
        {{"kind", "nmea2000-message"},
         {"direction", direction},
         {"protocol", "NMEA2000"},
         {"transport", transport},
         {"status", status},
         {"pgn", double(m.PGN)},
         {"priority", m.Priority},
         {"source", sessionSource >= 0 ? sessionSource : m.Source},
         {"source_basis", sessionSource >= 0    ? "active-session"
                          : direction[0] == 'R' ? "received-message"
                                                : "submitted-message-header"},
         {"submitted_source", m.Source},
         {"destination", m.Destination},
         {"length", m.DataLen},
         {"payload_hex", QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(m.Data),
                                                        qBound(0, m.DataLen, int(tN2kMsg::MaxDataLen)))
                                                 .toHex())}},
        capture);
}
void sentence(const char *direction, const QString &transport, const QByteArray &data, const QString &status)
{
    const auto capture = captureFor(direction, false, false);
    if (!capture)
        return;
    const auto frame = data.left(4096);
    trafficRecord({{"kind", "nmea0183-sentence"},
                   {"direction", direction},
                   {"protocol", "NMEA0183"},
                   {"transport", sanitized(transport)},
                   {"status", status},
                   {"talker", QString::fromLatin1(frame.mid(1, 2))},
                   {"sentence", QString::fromLatin1(frame.mid(3, 3))},
                   {"length", data.size()},
                   {"payload_truncated", data.size() > 4096},
                   {"payload_hex", QString::fromLatin1(frame.toHex())}},
                  capture);
}
void canFrame(const char *direction, const QString &transport, quint32 id, quint8 length,
              const unsigned char *data)
{
    const auto capture = captureFor(direction, true, true);
    if (!capture || length > 8 || id > 0x1fffffff)
        return;
    trafficRecord({{"kind", "can-frame"},
                   {"direction", direction},
                   {"protocol", "NMEA2000"},
                   {"transport", transport},
                   {"status", direction[0] == 'R' ? "transport-received" : "backend-submitted"},
                   {"id", QString::number(id, 16).rightJustified(8, '0')},
                   {"extended", true},
                   {"dlc", length},
                   {"payload_hex",
                    QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(data), length).toHex())}},
                  capture);
}
#ifdef Q_OS_LINUX
void socketCanFrame(quint32 nativeId, quint8 length, const unsigned char *data)
{
    if ((nativeId & CAN_EFF_FLAG) && !(nativeId & (CAN_RTR_FLAG | CAN_ERR_FLAG)))
        canFrame("RX", "SocketCAN", nativeId & CAN_EFF_MASK, length, data);
}
#endif
} // namespace ApplicationLog
