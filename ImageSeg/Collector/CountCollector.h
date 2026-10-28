#ifndef COUNTCOLLECTOR_H
#define COUNTCOLLECTOR_H

#include <atomic>
#include <QMap>
#include <QString>
#include <QDateTime>
#include <QSqlDatabase>

class CountCollector
{
public:
    CountCollector(std::shared_ptr<QSqlDatabase> db);

    ///
    /// \brief catchCount
    /// 更新数量
    /// \param db
    /// 数据库连接指针
    ///
    void catchCount(std::shared_ptr<QSqlDatabase> db);

    ///
    /// \brief startRun
    /// 重新运行设备，更新本日及本次启动产量
    ///
    void startRun();

    ///
    /// \brief getDateTypeCount
    /// 获取本日运行总产量
    /// \return
    ///
    QMap<QString, int> getDateTypeCount();

    ///
    /// \brief getRunTypeCount
    /// 获取本次运行产量
    /// \return
    ///
    QMap<QString, int> getRunTypeCount();

private:
    std::atomic<int> m_totalCount;       // 本次启动生产数量（以设备启停区分）
    std::atomic<int> m_dateTotalCount;   // 本日总数量
    QMap<QString, int> m_dateTypeCounts;    // 本日各品类生产数量
    QMap<QString, int> m_typeCounts;        // 本次启动各品类生产数量（以设备启停区分）
};

#endif // COUNTCOLLECTOR_H
