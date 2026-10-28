#include "LogManager.h"

#include <QThread>
#include <QScrollBar>

LogManager::LogManager(QObject *parent)
    : QObject(parent)
    , m_logFile(nullptr)
    , m_textStream(nullptr)
    , m_logLevel(QtDebugMsg)
    , m_initialized(false)
    , m_shutdown(false)
    , m_logWidget(nullptr)
    , m_maxWidgetLines(20)  // 默认10行
{
    // 设置日志目录为程序路径下的Log文件夹
    m_logDirPath = QCoreApplication::applicationDirPath() + "/Log";

    // 连接信号槽，确保跨线程安全
    connect(this, &LogManager::logMessageToWidget,
            this, &LogManager::onLogMessageToWidget,
            Qt::QueuedConnection);
}

LogManager::~LogManager()
{
    shutdown();
}

LogManager& LogManager::instance()
{
    static LogManager instance;
    return instance;
}

void LogManager::init()
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized) {
        return;
    }

    // 创建日志目录
    if (!createLogDirectory()) {
        qWarning() << "Failed to create log directory:" << m_logDirPath;
        return;
    }

    // 创建初始日志文件
    if (!createNewLogFile()) {
        qWarning() << "Failed to create log file";
        return;
    }

    // 安装消息处理器
    qInstallMessageHandler(messageHandler);

    m_initialized = true;

    //qDebug() << "Log system initialized. Log directory:" << m_logDirPath;
}

void LogManager::shutdown()
{
    QMutexLocker locker(&m_mutex);

    if (m_shutdown) {
        return;
    }

    m_shutdown = true;

    // 恢复默认消息处理器
    qInstallMessageHandler(nullptr);

    // 关闭文件
    if (m_textStream) {
        m_textStream->flush();
        delete m_textStream;
        m_textStream = nullptr;
    }

    if (m_logFile) {
        if (m_logFile->isOpen()) {
            m_logFile->close();
        }
        delete m_logFile;
        m_logFile = nullptr;
    }

    m_initialized = false;

    qDebug() << "Log system shutdown";
}

void LogManager::setLogLevel(QtMsgType level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

QString LogManager::getCurrentLogPath() const
{
    QMutexLocker locker(&m_mutex);
    return m_logFile ? m_logFile->fileName() : QString();
}

bool LogManager::createLogDirectory()
{
    QDir logDir(m_logDirPath);
    if (!logDir.exists()) {
        return logDir.mkpath(".");
    }
    return true;
}

QString LogManager::getLogFilePath() const
{
    QString dateStr = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    return m_logDirPath + "/" + dateStr + ".log";
}

bool LogManager::shouldCreateNewFile()
{
    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    return currentDate != m_currentDate;
}

bool LogManager::createNewLogFile()
{
    // 关闭现有文件
    if (m_textStream) {
        m_textStream->flush();
        delete m_textStream;
        m_textStream = nullptr;
    }

    if (m_logFile) {
        if (m_logFile->isOpen()) {
            m_logFile->close();
        }
        delete m_logFile;
        m_logFile = nullptr;
    }

    // 创建新文件
    QString filePath = getLogFilePath();
    m_logFile = new QFile(filePath);

    if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Failed to open log file:" << filePath;
        delete m_logFile;
        m_logFile = nullptr;
        return false;
    }

    m_textStream = new QTextStream(m_logFile);
    //m_textStream->setCodec("UTF-8");

    m_currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");

    // 写入文件头
    *m_textStream << QString("=== Log Started at %1 ===\n")
                         .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                  << Qt::endl;

    return true;
}

void LogManager::writeToFile(const QString &logMessage)
{
    QMutexLocker locker(&m_mutex);

    if (m_shutdown || !m_initialized) {
        return;
    }

    // 检查是否需要创建新的日志文件（按日期）
    if (shouldCreateNewFile()) {
        if (!createNewLogFile()) {
            return;
        }
    }

    if (m_textStream && m_logFile && m_logFile->isOpen()) {
        *m_textStream << logMessage << Qt::endl;
        m_textStream->flush();
    }
}

void LogManager::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context)

    LogManager& logger = LogManager::instance();

    // 检查日志级别
    if (type < logger.m_logLevel) {
        return;
    }

    // 格式化日志消息
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString logLevelStr;

    switch (type) {
    case QtDebugMsg:
        logLevelStr = "DEBUG";
        break;
    case QtInfoMsg:
        logLevelStr = "INFO";
        break;
    case QtWarningMsg:
        logLevelStr = "WARNING";
        break;
    case QtCriticalMsg:
        logLevelStr = "CRITICAL";
        break;
    case QtFatalMsg:
        logLevelStr = "FATAL";
        break;
    }

    QString logMessage = QString("[%1][%2]: %3")
                             .arg(timestamp)
                             .arg(logLevelStr)
                             .arg(msg);

    // 输出到控制台（带颜色）
    QTextStream consoleStream(stdout);
    switch (type) {
    case QtDebugMsg:
        consoleStream << "\033[37m" << logMessage << "\033[0m" << Qt::endl; // 白色
        break;
    case QtInfoMsg:
        consoleStream << "\033[32m" << logMessage << "\033[0m" << Qt::endl; // 绿色
        break;
    case QtWarningMsg:
        consoleStream << "\033[33m" << logMessage << "\033[0m" << Qt::endl; // 黄色
        break;
    case QtCriticalMsg:
        consoleStream << "\033[31m" << logMessage << "\033[0m" << Qt::endl; // 红色
        break;
    case QtFatalMsg:
        consoleStream << "\033[35m" << logMessage << "\033[0m" << Qt::endl; // 紫色
        break;
    }

    // 写入文件
    logger.writeToFile(logMessage);

    // 写入界面
    logger.writeToWidget(logMessage);

    // 如果是致命错误，终止程序
    if (type == QtFatalMsg) {
        abort();
    }
}


void LogManager::setLogWidget(QPlainTextEdit* widget)
{
    QMutexLocker locker(&m_mutex);
    m_logWidget = widget;

    if (m_logWidget) {
        // 设置控件属性
        m_logWidget->setReadOnly(true);
        m_logWidget->setTextInteractionFlags(Qt::NoTextInteraction);
        m_logWidget->setUndoRedoEnabled(false);

        // 设置等宽字体，便于对齐
//        QFont font("Consolas", 9);
//        m_logWidget->setFont(font);

        // 初始消息
        //writeToWidget("[SYSTEM] 日志界面已连接");
    }
}

void LogManager::clearWidgetLog()
{
    QMutexLocker locker(&m_mutex);
    if (m_logWidget) {
        m_logWidget->clear();
        //writeToWidget("[SYSTEM] 日志已清空");
    }
}

void LogManager::onLogMessageToWidget(const QString& message)
{
    // 这个槽函数在主线程中执行，可以安全地操作UI
    writeToWidget(message);
}


void LogManager::writeToWidget(const QString &logMessage)
{
    if (!m_logWidget) {
        return;
    }

    // 确保在UI线程中操作
    if (QThread::currentThread() != m_logWidget->thread()) {
        // 通过信号槽机制确保线程安全
        emit logMessageToWidget(logMessage);
        return;
    }

    QMutexLocker locker(&m_mutex);

    // 添加到控件
    m_logWidget->appendPlainText(logMessage);

    // 自动滚动到底部
    QScrollBar* scrollBar = m_logWidget->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());

    // 限制最大行数，防止内存过度使用
    if (m_logWidget->document()->lineCount() > m_maxWidgetLines) {
        QTextCursor cursor(m_logWidget->document());
        cursor.movePosition(QTextCursor::Start);
        //cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 100);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }
}
