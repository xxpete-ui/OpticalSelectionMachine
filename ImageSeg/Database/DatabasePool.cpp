#include "DatabasePool.h"

#include <QUuid>
#include <QDebug>
#include <QSqlError>

DatabasePool::DatabasePool(const QString &driver, const QString &dbName,
                           int maxConnections, unsigned int timeout, QObject *parent)
    : QObject(parent), m_driver(driver),
    m_dbName(dbName), m_maxConnections(maxConnections),
    m_timeout(timeout)
{
    // 预创建指定数量的连接
    for (int i = 0; i < maxConnections; ++i) {
        QSqlDatabase db = createConnection();
        if (db.isOpen()) {
            m_available.enqueue(db);
        }
    }
    OpenCount = m_available.count();
}

DatabasePool::~DatabasePool()
{
    closeAll();
}

QSqlDatabase DatabasePool::createConnection()
{
    QString connName = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase db = QSqlDatabase::addDatabase(m_driver, connName);
    db.setDatabaseName(m_dbName);
    // 可根据驱动设置其他参数（如用户名、密码、主机名等）
    if (m_driver == "QMYSQL") {
        db.setHostName(m_hostName);
        db.setUserName(m_userName);
        db.setPassword(m_password);
    }
    if (!db.open()) {
        qWarning() << "打开数据库失败:" << db.lastError().text();
    }
    return db;
}

QSqlDatabase DatabasePool::getConnection()
{
    QMutexLocker locker(&m_poolMutex);

//    // 如果有空闲连接，直接返回
//    if (!m_available.isEmpty()) {
//        QSqlDatabase db = m_available.dequeue();
//        m_usedConnNames.insert(db.connectionName());
//        return db;
//    }

//    // 如果当前活跃连接数未达到上限，创建新连接
//    int total = m_usedConnNames.size() + m_available.size();
//    if (total < m_maxConnections) {
//        QSqlDatabase db = createConnection();
//        if (db.isOpen()) {
//            m_usedConnNames.insert(db.connectionName());
//            return db;
//        }
//    }

//    // 无空闲连接且已达上限，阻塞等待
//    m_cond.wait(&m_poolMutex);
//    // 被唤醒后，重新获取一个连接（此时应有空闲）
//    QSqlDatabase db = m_available.dequeue();
//    m_usedConnNames.insert(db.connectionName());
//    return db;

    while(m_available.isEmpty())
    {
        if(m_usedConnNames.size() + m_available.size() < m_maxConnections)
        {
            QSqlDatabase db = createConnection();
            if (db.isOpen())
            {
                m_usedConnNames.insert(db.connectionName());
                return db;
            }
        }

        if(!m_cond.wait(&m_poolMutex, m_timeout))
        {
            // 超时后的反应，目前使用继续等待处理
            continue;
        }
    }

    QSqlDatabase db = m_available.dequeue();
    m_usedConnNames.insert(db.connectionName());
    return db;
}

std::shared_ptr<QSqlDatabase> DatabasePool::getConnectionAutoPtr()
{
    QMutexLocker locker(&m_poolMutex);
    int creatTest = 0;  // 尝试新建连接的次数
    while(m_available.isEmpty())
    {
        if(m_usedConnNames.size() + m_available.size() < m_maxConnections)
        {
            QSqlDatabase db = createConnection();
            if (db.isOpen())
            {
                break;
            }
        }

        if(!m_cond.wait(&m_poolMutex, m_timeout))
        {
            // 超时后的反应

            // 创建新连接
            if(OpenCount < m_maxConnections + 3 && creatTest < 5)
            {
                QSqlDatabase db = createConnection();
                if (db.isOpen()) {
                    m_available.enqueue(db);
                    OpenCount++;
                }
                creatTest++;
            }
            else
            {
                continue;     // 继续等待处理
            }
        }
    }

    std::shared_ptr<QSqlDatabase> conner(new QSqlDatabase(m_available.dequeue()), [this](QSqlDatabase* conn)
        {
            QMutexLocker locker(&m_poolMutex);
            if(!m_usedConnNames.contains(conn->connectionName()))
            {
                return;
            }
            m_usedConnNames.remove(conn->connectionName());
            if(!conn->isOpen())
            {
                // 丢弃失效连接，重新创建一个新连接加入池
                QSqlDatabase::removeDatabase(conn->connectionName());
                QSqlDatabase newDb = createConnection();
                if (newDb.isOpen()) {
                    m_available.enqueue(newDb);
                }
            }
            else
            {
                m_available.enqueue(*conn);
                delete conn;
            }

        });

    m_usedConnNames.insert(conner->connectionName());
    return conner;
}

void DatabasePool::releaseConnection(const QSqlDatabase &db)
{
    QMutexLocker locker(&m_poolMutex);

    // 验证该连接是否属于本池
    if (!m_usedConnNames.contains(db.connectionName())) {
        qWarning() << "试图归还一个不认识的连接";
        return;
    }

    // 可选：检查连接是否仍有效，若失效则重建
    if (!db.isOpen()) {
        // 丢弃失效连接，重新创建一个新连接加入池
        QSqlDatabase::removeDatabase(db.connectionName());
        QSqlDatabase newDb = createConnection();
        if (newDb.isOpen()) {
            m_available.enqueue(newDb);
        }
    } else {
        m_available.enqueue(db);
    }

    m_usedConnNames.remove(db.connectionName());
    m_cond.wakeOne(); // 唤醒一个等待的线程
}


void DatabasePool::closeAll()
{
    QMutexLocker locker(&m_poolMutex);
    while (!m_available.isEmpty()) {
        QSqlDatabase db = m_available.dequeue();
        QString connName = db.connectionName();
        db.close();
        QSqlDatabase::removeDatabase(connName);
    }
    // 注意：正在使用中的连接不会被释放，但通常程序退出时会自然销毁
}

int DatabasePool::freeTimeCount()
{
    return m_available.count();
}
