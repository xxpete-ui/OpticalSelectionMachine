#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>
#include <QPlainTextEdit>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

class LogManager : public QObject
{
    Q_OBJECT

public:
    static LogManager& instance();

    // 初始化日志系统
    void init();

    // 关闭日志系统
    void shutdown();

    // 设置日志级别
    void setLogLevel(QtMsgType level);

    // 获取当前日志文件路径
    QString getCurrentLogPath() const;

    // 设置日志显示控件
    void setLogWidget(QPlainTextEdit* widget);

    // 清空界面日志
    void clearWidgetLog();

signals:
    // 用于跨线程发送日志到界面
    void logMessageToWidget(const QString& message);

public slots:
    // 处理界面日志输出（线程安全）
    void onLogMessageToWidget(const QString& message);


private:
    explicit LogManager(QObject *parent = nullptr);
    ~LogManager();

    // 禁止拷贝和移动
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    // 日志处理函数
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    // 写入日志到文件
    void writeToFile(const QString &logMessage);

    // 输出日志到界面
    void writeToWidget(const QString &logMessage);

    // 创建日志目录
    bool createLogDirectory();

    // 获取日志文件路径
    QString getLogFilePath() const;

    // 检查是否需要创建新的日志文件（按日期）
    bool shouldCreateNewFile();

    // 格式化日志消息
    QString formatLogMessage(QtMsgType type, const QString &msg);

    // 创建新的日志文件
    bool createNewLogFile();

private:
    QFile *m_logFile;
    QTextStream *m_textStream;
    mutable QMutex m_mutex;
    QString m_currentDate;
    QString m_logDirPath;
    QtMsgType m_logLevel;
    bool m_initialized;
    bool m_shutdown;

    // 新增：界面日志控件
    QPlainTextEdit* m_logWidget;
    int m_maxWidgetLines;  // 界面最大显示行数
};

#endif // LOGMANAGER_H
