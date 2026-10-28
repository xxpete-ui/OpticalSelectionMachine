// TimeBasedFolderManager.cpp
#include "TimeBasedFolderManager.h"
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>

TimeBasedFolderManager::TimeBasedFolderManager(QObject *parent)
    : QObject(parent)
    , m_basePath(QCoreApplication::applicationDirPath())
{
}

void TimeBasedFolderManager::setBasePath(const QString &path)
{
    m_basePath = QDir::toNativeSeparators(QDir(path).absolutePath());
}

void TimeBasedFolderManager::setPrefix(const QString &prefix)
{
    m_prefix = prefix;
}

void TimeBasedFolderManager::setSuffix(const QString &suffix)
{
    m_suffix = suffix;
}

QString TimeBasedFolderManager::createCurrentTimeFolder()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString folderName = buildFolderName(currentTime);
    QString fullPath = buildFullPath(folderName);

    // 如果文件夹已存在，添加序号
    int counter = 1;
    QString originalPath = fullPath;
    while (QDir(fullPath).exists())
    {
        QString newFolderName = QString("%1_%2").arg(folderName).arg(counter, 2, 10, QChar('0'));
        fullPath = buildFullPath(newFolderName);
        counter++;
    }

    // 创建文件夹
    if (QDir().mkpath(fullPath))
    {
        qDebug() << "Time-based folder created:" << fullPath;
        emit folderCreated(fullPath);
        return fullPath;
    }
    else
    {
        qCritical() << "Failed to create time-based folder:" << fullPath;
        emit folderCreationFailed(fullPath, "权限不足或磁盘空间已满");
        return QString();
    }
}

QString TimeBasedFolderManager::checkTimeFolderExists(const QDateTime &dateTime)
{
    QString folderName = buildFolderName(dateTime);
    QString fullPath = buildFullPath(folderName);

    // 检查精确匹配
    if (QDir(fullPath).exists())
    {
        return fullPath;
    }

    // 检查可能带序号的情况
    QDir baseDir(m_basePath);
    QStringList filters;
    filters << folderName + "*";
    baseDir.setNameFilters(filters);
    baseDir.setFilter(QDir::Dirs);

    QStringList existingFolders = baseDir.entryList();
    if (!existingFolders.isEmpty())
    {
        return QDir::toNativeSeparators(baseDir.absoluteFilePath(existingFolders.first()));
    }

    return QString();
}

QStringList TimeBasedFolderManager::getTodayFolders()
{
    QDir baseDir(m_basePath);
    baseDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    QDate today = QDate::currentDate();
    QString todayPrefix = today.toString("yyyyMMdd");

    QStringList allFolders = baseDir.entryList();
    QStringList todayFolders;

    foreach (const QString &folder, allFolders)
    {
        if (folder.startsWith(todayPrefix))
        {
            QString fullPath = QDir::toNativeSeparators(baseDir.absoluteFilePath(folder));
            todayFolders.append(fullPath);
        }
    }

    // 按名称排序（时间顺序）
    todayFolders.sort();

    return todayFolders;
}

int TimeBasedFolderManager::cleanupOldFolders(int keepCount)
{
    QDir baseDir(m_basePath);
    baseDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    baseDir.setSorting(QDir::Name | QDir::Reversed);

    QStringList allFolders = baseDir.entryList();

    int deletedCount = 0;
    for (int i = keepCount; i < allFolders.size(); ++i)
    {
        QString folderPath = QDir::toNativeSeparators(baseDir.absoluteFilePath(allFolders[i]));
        QDir folderDir(folderPath);

        // 删除文件夹及其内容
        if (folderDir.removeRecursively())
        {
            deletedCount++;
            qDebug() << "Old folder deleted:" << folderPath;
        }
        else
        {
            qWarning() << "Failed to delete folder:" << folderPath;
        }
    }

    emit oldFoldersCleaned(deletedCount);
    return deletedCount;
}

QString TimeBasedFolderManager::buildFolderName(const QDateTime &dateTime)
{
    QString folderName;

    if (!m_prefix.isEmpty())
    {
        folderName += m_prefix + "_";
    }

    folderName += dateTime.toString("yyyyMMdd");

    if (!m_suffix.isEmpty())
    {
        folderName += "_" + m_suffix;
    }

    return folderName;
}

QString TimeBasedFolderManager::buildFullPath(const QString &folderName)
{
    QDir baseDir(m_basePath);
    return QDir::toNativeSeparators(baseDir.absoluteFilePath(folderName));
}
