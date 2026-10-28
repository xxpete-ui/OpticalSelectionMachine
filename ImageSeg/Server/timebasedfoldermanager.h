// TimeBasedFolderManager.h
#ifndef TIMEBASEDFOLDERMANAGER_H
#define TIMEBASEDFOLDERMANAGER_H

#include <QObject>
#include <QDir>
#include <QDateTime>
#include <QStringList>

class TimeBasedFolderManager : public QObject
{
    Q_OBJECT

public:
    explicit TimeBasedFolderManager(QObject *parent = nullptr);

    /**
     * @brief 设置基础路径
     */
    void setBasePath(const QString &path);

    /**
     * @brief 设置文件夹名前缀
     */
    void setPrefix(const QString &prefix);

    /**
     * @brief 设置文件夹名后缀
     */
    void setSuffix(const QString &suffix);

    /**
     * @brief 自动创建当前时间格式的文件夹
     * @return 成功返回文件夹完整路径，失败返回空字符串
     */
    QString createCurrentTimeFolder();

    /**
     * @brief 检查指定时间格式的文件夹是否存在
     * @param dateTime 指定的日期时间
     * @return 存在返回完整路径，不存在返回空字符串
     */
    QString checkTimeFolderExists(const QDateTime &dateTime);

    /**
     * @brief 获取今天创建的所有时间格式文件夹
     * @return 文件夹路径列表
     */
    QStringList getTodayFolders();

    /**
     * @brief 清理旧的文件夹（保留最近N个）
     * @param keepCount 要保留的文件夹数量
     * @return 成功删除的数量
     */
    int cleanupOldFolders(int keepCount);

    QString buildFolderName(const QDateTime &dateTime);
    QString buildFullPath(const QString &folderName);

signals:
    void folderCreated(const QString &path);
    void folderCreationFailed(const QString &path, const QString &error);
    void oldFoldersCleaned(int count);

private:
    QString m_basePath;
    QString m_prefix;
    QString m_suffix;


};

#endif // TIMEBASEDFOLDERMANAGER_H
