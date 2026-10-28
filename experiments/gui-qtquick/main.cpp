#include <QAbstractListModel>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QQuickItem>
#include <QScreen>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <array>
#include <sys/resource.h>
#include <time.h>
#include <vector>

#ifndef GUI_POC_SOURCE_DIR
#define GUI_POC_SOURCE_DIR "."
#endif

#ifndef COSMOS_WORKSPACE_ROOT
#define COSMOS_WORKSPACE_ROOT "."
#endif

struct SoftwareRecord {
    QString name;
    QString category;
    QString currentVersion;
    QString latestVersion;
    QString source;
    QString instances;
    QString status;
    QString risk;
    QString glyph;
};

class SoftwareModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int recordCount READ recordCount CONSTANT)
    Q_PROPERTY(qint64 buildTimeMs READ buildTimeMs CONSTANT)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        CategoryRole,
        CurrentVersionRole,
        LatestVersionRole,
        SourceRole,
        InstancesRole,
        StatusRole,
        RiskRole,
        GlyphRole
    };

    SoftwareModel()
    {
        QElapsedTimer timer;
        timer.start();
        static const std::array<QString, 8> names = {
            QStringLiteral("Claude Code"), QStringLiteral("Cursor"),
            QStringLiteral("Node.js"), QStringLiteral("Python"),
            QStringLiteral("Git"), QStringLiteral("Firefox"),
            QStringLiteral("Visual Studio Code"), QStringLiteral("Flatpak")
        };
        static const std::array<QString, 8> categories = {
            QStringLiteral("AI Agent / CLI"), QStringLiteral("AI 编程 IDE"),
            QStringLiteral("JavaScript 运行时"), QStringLiteral("Python 运行时"),
            QStringLiteral("版本控制"), QStringLiteral("桌面应用"),
            QStringLiteral("开发工具"), QStringLiteral("应用管理")
        };
        static const std::array<QString, 8> sources = {
            QStringLiteral("npm"), QStringLiteral("官方包"),
            QStringLiteral("mise / npm"), QStringLiteral("uv / 系统包"),
            QStringLiteral("系统包"), QStringLiteral("Flatpak"),
            QStringLiteral("官方包"), QStringLiteral("Flatpak")
        };
        static const std::array<QString, 8> glyphs = {
            QStringLiteral("✳"), QStringLiteral("◆"), QStringLiteral("JS"),
            QStringLiteral("Py"), QStringLiteral("⑂"), QStringLiteral("◉"),
            QStringLiteral("VS"), QStringLiteral("▣")
        };

        m_records.reserve(10000);
        for (int i = 0; i < 10000; ++i) {
            const int app = i % static_cast<int>(names.size());
            const int version = 10 + (i % 89);
            const bool updateAvailable = i % 5 != 0;
            m_records.push_back({
                i < static_cast<int>(names.size())
                    ? names[app]
                    : names[app] + QStringLiteral(" · 实例 %1").arg(i + 1, 5, 10, QLatin1Char('0')),
                categories[app],
                QStringLiteral("%1.%2.%3").arg(version / 10).arg(version % 10).arg(i % 8),
                QStringLiteral("%1.%2.%3").arg(version / 10).arg(version % 10 + (updateAvailable ? 1 : 0)).arg((i + 3) % 9),
                sources[app],
                QStringLiteral("%1 个实例").arg(1 + (i % 3)),
                updateAvailable ? QStringLiteral("可更新") : QStringLiteral("最新"),
                i % 13 == 0 ? QStringLiteral("中") : QStringLiteral("低"),
                glyphs[app]
            });
        }
        m_buildTimeMs = timer.elapsed();
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_records.size());
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
            return {};

        const auto &record = m_records[static_cast<size_t>(index.row())];
        switch (role) {
        case NameRole: return record.name;
        case CategoryRole: return record.category;
        case CurrentVersionRole: return record.currentVersion;
        case LatestVersionRole: return record.latestVersion;
        case SourceRole: return record.source;
        case InstancesRole: return record.instances;
        case StatusRole: return record.status;
        case RiskRole: return record.risk;
        case GlyphRole: return record.glyph;
        default: return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            {NameRole, "displayName"}, {CategoryRole, "category"},
            {CurrentVersionRole, "currentVersion"}, {LatestVersionRole, "latestVersion"},
            {SourceRole, "source"}, {InstancesRole, "instances"},
            {StatusRole, "status"}, {RiskRole, "risk"}, {GlyphRole, "glyph"}
        };
    }

    int recordCount() const { return rowCount(); }
    qint64 buildTimeMs() const { return m_buildTimeMs; }
    bool selfCheck() const
    {
        return recordCount() == 10000
            && recordAt(0).value(QStringLiteral("displayName")).toString() == QStringLiteral("Claude Code")
            && !recordAt(9999).isEmpty()
            && recordAt(10000).isEmpty();
    }

    Q_INVOKABLE QVariantMap recordAt(int row) const
    {
        if (row < 0 || row >= rowCount())
            return {};
        const auto &record = m_records[static_cast<size_t>(row)];
        return {
            {QStringLiteral("displayName"), record.name},
            {QStringLiteral("category"), record.category},
            {QStringLiteral("currentVersion"), record.currentVersion},
            {QStringLiteral("latestVersion"), record.latestVersion},
            {QStringLiteral("source"), record.source},
            {QStringLiteral("instances"), record.instances},
            {QStringLiteral("status"), record.status},
            {QStringLiteral("risk"), record.risk},
            {QStringLiteral("glyph"), record.glyph}
        };
    }

private:
    std::vector<SoftwareRecord> m_records;
    qint64 m_buildTimeMs = 0;
};

static qint64 processCpuTimeNs()
{
    timespec value{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) != 0)
        return 0;
    return static_cast<qint64>(value.tv_sec) * 1000000000LL + value.tv_nsec;
}

static qint64 residentMemoryKiB()
{
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        return -1;
    return usage.ru_maxrss;
}

class RuntimeMetrics final : public QObject {
public:
    RuntimeMetrics(const SoftwareModel &model, QString metricsPath, QString capturePath,
                   QSize requestedSize, int durationSeconds, bool exitWhenDone,
                   QObject *parent = nullptr)
        : QObject(parent),
          m_model(model),
          m_metricsPath(std::move(metricsPath)),
          m_capturePath(std::move(capturePath)),
          m_requestedSize(requestedSize),
          m_durationSeconds(durationSeconds),
          m_exitWhenDone(exitWhenDone)
    {
    }

    void attach(QQuickWindow *window)
    {
        m_window = window;
        m_clock.start();
        m_cpuStart = processCpuTimeNs();
        auto recordGeometry = [this] {
            if (!m_window)
                return;
            m_windowGeometryEvents.append(QJsonObject{
                {QStringLiteral("elapsed_ms"), m_clock.elapsed()},
                {QStringLiteral("x"), m_window->x()},
                {QStringLiteral("y"), m_window->y()},
                {QStringLiteral("width"), m_window->width()},
                {QStringLiteral("height"), m_window->height()},
                {QStringLiteral("visible"), m_window->isVisible()},
                {QStringLiteral("exposed"), m_window->isExposed()}
            });
        };
        recordGeometry();
        QObject::connect(window, &QWindow::widthChanged, this, [recordGeometry](int) { recordGeometry(); });
        QObject::connect(window, &QWindow::heightChanged, this, [recordGeometry](int) { recordGeometry(); });
        QObject::connect(window, &QWindow::xChanged, this, [recordGeometry](int) { recordGeometry(); });
        QObject::connect(window, &QWindow::yChanged, this, [recordGeometry](int) { recordGeometry(); });
        QObject::connect(window, &QQuickWindow::frameSwapped, this, [this] {
            const qint64 now = m_clock.elapsed();
            if (m_lastFrameMs >= 0)
                m_frameIntervals.push_back(static_cast<double>(now - m_lastFrameMs));
            m_lastFrameMs = now;
        });

        if (!m_capturePath.isEmpty()) {
            QTimer::singleShot(1800, this, [this] {
                if (!m_window)
                    return;
                QDir().mkpath(QFileInfo(m_capturePath).absolutePath());
                const QImage image = m_window->grabWindow();
                if (image.isNull() || !image.save(m_capturePath)) {
                    qWarning() << "Screenshot capture failed:" << m_capturePath;
                    return;
                }
                m_capturePixelSize = image.size();
                qInfo() << "Screenshot saved:" << m_capturePath << image.size();
            });
        }

        if (!m_metricsPath.isEmpty()) {
            QTimer::singleShot(m_durationSeconds * 1000, this, [this] {
                writeMetrics();
                if (m_exitWhenDone)
                    QCoreApplication::quit();
            });
        }
    }

private:
    void writeMetrics()
    {
        if (m_metricsPath.isEmpty())
            return;
        QDir().mkpath(QFileInfo(m_metricsPath).absolutePath());
        QVector<double> intervals = m_frameIntervals;
        std::sort(intervals.begin(), intervals.end());
        const qint64 elapsedMs = std::max<qint64>(1, m_clock.elapsed());
        const qint64 cpuDeltaNs = std::max<qint64>(0, processCpuTimeNs() - m_cpuStart);
        const qsizetype p95Index = intervals.isEmpty()
            ? 0
            : std::min<qsizetype>(intervals.size() - 1, static_cast<qsizetype>(intervals.size() * 0.95));
        const qsizetype p99Index = intervals.isEmpty()
            ? 0
            : std::min<qsizetype>(intervals.size() - 1, static_cast<qsizetype>(intervals.size() * 0.99));
        const double fps = intervals.isEmpty() ? 0.0 : intervals.size() * 1000.0 / elapsedMs;
        const bool frameSampleValid = intervals.size() >= 30
            && !(m_window && m_window->property("reducedMotion").toBool());
        const QScreen *screen = m_window ? m_window->screen() : nullptr;
        const QRect screenGeometry = screen ? screen->geometry() : QRect();
        const QRect availableGeometry = screen ? screen->availableGeometry() : QRect();
        const QSize actualWindowSize = m_window ? m_window->size() : QSize();
        const QQuickItem *contentItem = m_window ? m_window->contentItem() : nullptr;
        const QSizeF contentItemSize = contentItem ? QSizeF(contentItem->width(), contentItem->height()) : QSizeF();

        QJsonObject report{
            {QStringLiteral("qt_platform"), QGuiApplication::platformName()},
            {QStringLiteral("requested_window_size_logical"), QJsonObject{
                {QStringLiteral("width"), m_requestedSize.width()},
                {QStringLiteral("height"), m_requestedSize.height()}
            }},
            {QStringLiteral("actual_window_size_logical"), QJsonObject{
                {QStringLiteral("width"), actualWindowSize.width()},
                {QStringLiteral("height"), actualWindowSize.height()}
            }},
            {QStringLiteral("window_geometry_events"), m_windowGeometryEvents},
            {QStringLiteral("window_is_visible"), m_window && m_window->isVisible()},
            {QStringLiteral("window_is_exposed"), m_window && m_window->isExposed()},
            {QStringLiteral("qml_window_size_logical"), QJsonObject{
                {QStringLiteral("width"), m_window ? m_window->property("width").toInt() : 0},
                {QStringLiteral("height"), m_window ? m_window->property("height").toInt() : 0}
            }},
            {QStringLiteral("quick_content_item_size_logical"), QJsonObject{
                {QStringLiteral("width"), contentItemSize.width()},
                {QStringLiteral("height"), contentItemSize.height()}
            }},
            {QStringLiteral("compact_navigation"), m_window && m_window->property("compactNav").toBool()},
            {QStringLiteral("screen_geometry_logical"), QJsonObject{
                {QStringLiteral("x"), screenGeometry.x()},
                {QStringLiteral("y"), screenGeometry.y()},
                {QStringLiteral("width"), screenGeometry.width()},
                {QStringLiteral("height"), screenGeometry.height()}
            }},
            {QStringLiteral("screen_available_geometry_logical"), QJsonObject{
                {QStringLiteral("x"), availableGeometry.x()},
                {QStringLiteral("y"), availableGeometry.y()},
                {QStringLiteral("width"), availableGeometry.width()},
                {QStringLiteral("height"), availableGeometry.height()}
            }},
            {QStringLiteral("window_device_pixel_ratio"), m_window ? m_window->devicePixelRatio() : 0.0},
            {QStringLiteral("capture_size_pixels"), QJsonObject{
                {QStringLiteral("width"), m_capturePixelSize.width()},
                {QStringLiteral("height"), m_capturePixelSize.height()}
            }},
            {QStringLiteral("records"), m_model.recordCount()},
            {QStringLiteral("model_build_ms"), m_model.buildTimeMs()},
            {QStringLiteral("duration_seconds"), elapsedMs / 1000.0},
            {QStringLiteral("frame_swaps"), intervals.size()},
            {QStringLiteral("frame_sample_valid"), frameSampleValid},
            {QStringLiteral("average_fps"), frameSampleValid ? QJsonValue(fps) : QJsonValue(QJsonValue::Null)},
            {QStringLiteral("frame_interval_p95_ms"), frameSampleValid ? QJsonValue(intervals[p95Index]) : QJsonValue(QJsonValue::Null)},
            {QStringLiteral("frame_interval_p99_ms"), frameSampleValid ? QJsonValue(intervals[p99Index]) : QJsonValue(QJsonValue::Null)},
            {QStringLiteral("frame_interval_max_ms"), frameSampleValid ? QJsonValue(intervals.back()) : QJsonValue(QJsonValue::Null)},
            {QStringLiteral("process_cpu_percent_one_core"), cpuDeltaNs * 100.0 / (elapsedMs * 1000000.0)},
            {QStringLiteral("peak_resident_memory_kib"), residentMemoryKiB()},
            {QStringLiteral("reduced_motion"), m_window && m_window->property("reducedMotion").toBool()},
            {QStringLiteral("animation_active"), m_window && m_window->property("sceneRunning").toBool()},
            {QStringLiteral("compare_mode"), m_window && m_window->property("compareMode").toBool()},
            {QStringLiteral("stress_scroll"), m_window && m_window->property("stressEnabled").toBool()},
            {QStringLiteral("stress_scroll_animation_active"), m_window && m_window->property("stressScrollRunning").toBool()}
        };

        QSaveFile file(m_metricsPath);
        if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(report).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
            qWarning() << "Metrics write failed:" << m_metricsPath;
            return;
        }
        qInfo().noquote() << "Metrics saved:" << m_metricsPath;
        qInfo().noquote() << QJsonDocument(report).toJson(QJsonDocument::Compact);
    }

    const SoftwareModel &m_model;
    QString m_metricsPath;
    QString m_capturePath;
    QSize m_requestedSize;
    QSize m_capturePixelSize;
    QJsonArray m_windowGeometryEvents;
    int m_durationSeconds = 10;
    bool m_exitWhenDone = false;
    QQuickWindow *m_window = nullptr;
    QElapsedTimer m_clock;
    qint64 m_lastFrameMs = -1;
    qint64 m_cpuStart = 0;
    QVector<double> m_frameIntervals;
};

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("gui-qtquick-poc"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption sizeOption(QStringList{QStringLiteral("s"), QStringLiteral("size")},
        QStringLiteral("Window size in logical pixels, WIDTHxHEIGHT."), QStringLiteral("size"));
    const QCommandLineOption captureOption(QStringLiteral("capture"),
        QStringLiteral("Save a screenshot of the application window."), QStringLiteral("path"));
    const QCommandLineOption metricsOption(QStringLiteral("metrics"),
        QStringLiteral("Write runtime measurements after the selected duration."), QStringLiteral("path"));
    const QCommandLineOption durationOption(QStringLiteral("duration"),
        QStringLiteral("Measurement duration in seconds."), QStringLiteral("seconds"), QStringLiteral("12"));
    const QCommandLineOption stressOption(QStringLiteral("stress-scroll"),
        QStringLiteral("Sweep through the virtualized 10,000-row list during measurement."));
    const QCommandLineOption reducedMotionOption(QStringLiteral("reduced-motion"),
        QStringLiteral("Disable background and transition animation for a still capture."));
    const QCommandLineOption exitOption(QStringLiteral("exit-after-metrics"),
        QStringLiteral("Exit after writing the metrics file."));
    const QCommandLineOption selfTestOption(QStringLiteral("self-test"),
        QStringLiteral("Check the synthetic model bounds and sample rows, then exit."));
    const QCommandLineOption compareModeOption(QStringList{QStringLiteral("compare-mode")},
        QStringLiteral("Start with the DET3/CMP3 instance comparison view."));
    parser.addOptions({sizeOption, captureOption, metricsOption, durationOption, stressOption, reducedMotionOption, exitOption, selfTestOption, compareModeOption});
    parser.process(app);

    SoftwareModel softwareModel;
    if (parser.isSet(selfTestOption)) {
        qInfo() << (softwareModel.selfCheck() ? "Model self-check passed" : "Model self-check failed");
        return softwareModel.selfCheck() ? 0 : 1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("softwareModel"), &softwareModel);
    engine.rootContext()->setContextProperty(QStringLiteral("appStressMode"), parser.isSet(stressOption));
    engine.rootContext()->setContextProperty(QStringLiteral("appReducedMotion"), parser.isSet(reducedMotionOption));
    engine.rootContext()->setContextProperty(QStringLiteral("appCompareMode"), parser.isSet(compareModeOption));
    const QString assetPath = QDir(QStringLiteral(COSMOS_WORKSPACE_ROOT)).filePath(
        QStringLiteral("Linux全生态版本管理中心_定稿视觉素材库_V3.0"));
    engine.rootContext()->setContextProperty(QStringLiteral("assetRoot"),
        QUrl::fromLocalFile(assetPath + QLatin1Char('/')).toString());
    engine.load(QUrl::fromLocalFile(QStringLiteral(GUI_POC_SOURCE_DIR) + QStringLiteral("/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return 1;

    const QStringList sizeParts = parser.value(sizeOption).split(QLatin1Char('x'));
    QSize requestedSize = window->size();
    if (sizeParts.size() == 2) {
        bool widthOk = false;
        bool heightOk = false;
        const int requestedWidth = sizeParts[0].toInt(&widthOk);
        const int requestedHeight = sizeParts[1].toInt(&heightOk);
        if (widthOk && heightOk && requestedWidth > 0 && requestedHeight > 0) {
            requestedSize = QSize(requestedWidth, requestedHeight);
            window->resize(requestedSize);
        }
    }

    qInfo() << "Mock records:" << softwareModel.recordCount()
            << "model build ms:" << softwareModel.buildTimeMs();

    if (parser.isSet(metricsOption) || parser.isSet(captureOption)) {
        bool ok = false;
        const int seconds = std::clamp(parser.value(durationOption).toInt(&ok), 3, 300);
        auto *metrics = new RuntimeMetrics(softwareModel, parser.value(metricsOption), parser.value(captureOption),
                                           requestedSize, ok ? seconds : 12, parser.isSet(exitOption), &app);
        metrics->attach(window);
    }

    window->show();
    return app.exec();
}

#include "main.moc"
