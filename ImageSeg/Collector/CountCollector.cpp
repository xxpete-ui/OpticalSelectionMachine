#include "CountCollector.h"

#include <QObject>
#include <QSqlQuery>
#include <QDate>
#include <QSqlError>

#include "Server/State.h"
#include "Database/Table/TableInfo.h"
#include "Database/SqlMapping.h"

CountCollector::CountCollector(std::shared_ptr<QSqlDatabase> db)
{
    m_totalCount = 0;
    m_dateTotalCount = 0;
    m_typeCounts.clear();
    m_dateTypeCounts.clear();
    if(db != nullptr)
    {
        QSqlQuery query(*db);
        query.prepare("select * from DailyOutput where Date = :date");
        query.bindValue(":date", QDate::currentDate());
        if(query.exec())
        {
            if(query.next())
            {
                DatabaseTable::DailyOutput daily = DatabaseTable::DailyOutput::fromQuery(query);
                //mapToEntity(daily, query);
                m_dateTotalCount = daily.Count;
                QSqlQuery subQuery(*db);
                subQuery.prepare("select * from DailyOutputDetail where MainId = :mainId");
                subQuery.bindValue(":mainId", daily.MainId);
                if(subQuery.exec())
                {
                    while(subQuery.next())
                    {
                        DatabaseTable::DailyOutputDetail detail = DatabaseTable::DailyOutputDetail::fromQuery(subQuery);
                        //mapToEntity(detail, subQuery);
                        m_dateTypeCounts[detail.TypeName] = detail.Count;
                        State::typeCounts[detail.TypeName.toStdString()] = detail.Count;
                    }
                }
            }
        }
    }
}



void CountCollector::catchCount(std::shared_ptr<QSqlDatabase> db)
{
    // 更新数据
    int totalCount = 0;
    for (std::map<std::string, std::atomic<int>>::iterator it = State::typeCounts.begin(); it != State::typeCounts.end(); ++it)
    {
        m_typeCounts[QString::fromStdString(it->first)] = it->second - m_dateTypeCounts[QString::fromStdString(it->first)];
        totalCount += m_typeCounts[QString::fromStdString(it->first)];
    }
    m_totalCount = totalCount;

    if (!db || !db->isOpen()) {
        qWarning() << "数据库连接无效";
        return;
    }

    QDate today = QDate::currentDate();
    QDateTime now = QDateTime::currentDateTime();

    QSqlQuery query(*db);

    // 开始事务
    if (!db->transaction()) {
        qWarning() << "无法开始事务：" << db->lastError().text();
        return;
    }

    // 1. 获取或创建当天的主表记录
    int mainId = -1;
    query.prepare("SELECT MainId, Count FROM DailyOutput WHERE Date = :date");
    query.bindValue(":date", today);
    if (!query.exec()) {
        qWarning() << "查询主表失败：" << query.lastError().text();
        db->rollback();
        return;
    }

    if (query.next()) {
        mainId = query.value("MainId").toInt();
    } else {
        // 主表不存在，插入新记录
        query.prepare("INSERT INTO DailyOutput (Date, Count, LastDate) VALUES (:date, 0, :lastDate)");
        query.bindValue(":date", today);
        query.bindValue(":lastDate", now);
        if (!query.exec()) {
            qWarning() << "插入主表失败：" << query.lastError().text();
            db->rollback();
            return;
        }
        mainId = query.lastInsertId().toInt();
    }

    // 2. 处理每个品类的明细
    //int totalCount = 0;
    QMap<QString, int> typeCounts = m_typeCounts;
    QMap<QString, int> dateTypeCounts = m_dateTypeCounts;
    QSqlQuery subQuery(*db);
    // if (subQuery.exec("PRAGMA table_info(DailyOutputDetail)")) {
    //     while (subQuery.next()) {
    //         qDebug() << subQuery.value("name").toString();
    //     }
    // }
    for (auto it = typeCounts.begin(); it != typeCounts.end(); ++it) {
        QString typeName = it.key();
        int addCount = it.value();
        //totalCount += addCount;

        // 查询明细是否存在
        subQuery.prepare("SELECT SubId, Count FROM DailyOutputDetail WHERE MainId=? AND TypeName=? AND Date=?");
        subQuery.addBindValue(mainId);
        subQuery.addBindValue(typeName);
        subQuery.addBindValue(today);
        // query.bindValue(":mainId", mainId);
        // query.bindValue(":typeName", typeName);
        // query.bindValue(":date", today);
        if (!subQuery.exec()) {
            qWarning() << "查询明细失败：" << subQuery.lastError().text();
            db->rollback();
            return;
        }

        if (subQuery.next()) {
            // 明细存在，更新数量
            int subId = subQuery.value("SubId").toInt();
            int oldCount = dateTypeCounts.find(typeName) != dateTypeCounts.end() ? dateTypeCounts[it.key()] : 0;
            int newCount = oldCount + addCount;

            subQuery.clear();
            subQuery.prepare("UPDATE DailyOutputDetail SET Count = :count, LastDate = :lastDate WHERE SubId = :subId");
            subQuery.bindValue(":count", newCount);
            subQuery.bindValue(":lastDate", now);
            subQuery.bindValue(":subId", subId);
            if (!subQuery.exec()) {
                qWarning() << "更新明细失败：" << subQuery.lastError().text();
                db->rollback();
                return;
            }
        } else {
            // 明细不存在，插入新记录
            subQuery.clear();
            subQuery.prepare("INSERT INTO DailyOutputDetail (MainId, Date, TypeName, Count, LastDate) VALUES (:mainId, :date, :typeName, :count, :lastDate)");
            subQuery.bindValue(":mainId", mainId);
            subQuery.bindValue(":date", today);
            subQuery.bindValue(":typeName", typeName);
            subQuery.bindValue(":count", addCount);
            subQuery.bindValue(":lastDate", now);
            if (!subQuery.exec()) {
                qWarning() << "插入明细失败：" << subQuery.lastError().text();
                db->rollback();
                return;
            }
        }
    }

    // 3. 更新主表的总数量（累加本次新增的数量）
    // 注意：主表原有数量 + totalCount
    totalCount += m_dateTotalCount;
    query.clear();
    query.prepare("UPDATE DailyOutput SET Count = :addTotal, LastDate = :lastDate WHERE MainId = :mainId");
    query.bindValue(":addTotal", totalCount);
    query.bindValue(":lastDate", now);
    query.bindValue(":mainId", mainId);
    if (!query.exec()) {
        qWarning() << "更新主表总数量失败：" << query.lastError().text();
        db->rollback();
        return;
    }

    // 提交事务
    if (!db->commit()) {
        qWarning() << "提交事务失败：" << db->lastError().text();
        db->rollback();
        return;
    }

    qDebug() << "数据更新成功，总数量：" << totalCount;
}


void CountCollector::startRun()
{
    for(auto it = m_typeCounts.begin(); it != m_typeCounts.end(); ++it)
    {
        if(m_dateTypeCounts.find(it.key()) != m_dateTypeCounts.end())
        {
            m_dateTypeCounts[it.key()] += it.value();
        }
        else
        {
            m_dateTypeCounts[it.key()] = it.value();
        }
    }
    m_typeCounts.clear();
    //State::RunTime = QDateTime::currentDateTime();
}

QMap<QString, int> CountCollector::getDateTypeCount()
{
    auto dateMap = m_dateTypeCounts;
    auto runMap = m_typeCounts;
    for(auto it = runMap.begin(); it != runMap.end(); ++it)
    {
        if(dateMap.find(it.key()) != dateMap.end())
        {
            dateMap[it.key()] += it.value();
        }
        else
        {
            dateMap[it.key()] = it.value();
        }
    }
    return dateMap;
}

QMap<QString, int> CountCollector::getRunTypeCount()
{
    return m_typeCounts;
}
