#ifndef DATABASEPOOL_H
#define DATABASEPOOL_H

#include <QObject>
#include <QSqlDatabase>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QWaitCondition>


class DatabasePool : public QObject
{
    Q_OBJECT
public:
    // 构造函数：指定数据库驱动、连接参数和最大连接数
    explicit DatabasePool(const QString &driver, const QString &dbName,
                          int maxConnections = 5, unsigned int timeout = 3000,  QObject *parent = nullptr);
    ~DatabasePool();

    // 获取一个连接（会阻塞直到有空闲连接）
    QSqlDatabase getConnection();

    // 获取一个连接（会阻塞直到有空闲连接），返回智能指针，无需显式调用归还(releaseConnection)
    std::shared_ptr<QSqlDatabase> getConnectionAutoPtr();

    // 归还连接
    void releaseConnection(const QSqlDatabase &db);

    // 关闭所有连接
    void closeAll();

    // 当前空闲连接数量
    int freeTimeCount();

signals:

private:

    QSqlDatabase createConnection(); // 创建一个新连接

private:
    QString m_driver;                   // 数据库类型
    QString m_dbName;                   // 数据库名
    int m_maxConnections;               // 最大连接数量
    unsigned int m_timeout;             // 超时时间（ms）

    QQueue<QSqlDatabase> m_available;   // 空闲连接队列
    QSet<QString> m_usedConnNames;      // 正在使用的连接名
    QMutex m_poolMutex;                 // 连接池锁
    QWaitCondition m_cond;              // 阻塞

public:
    QString m_hostName;                 // 数据库链接 （Localhost/ 127.0.0.1:3306）
    QString m_userName;                 // 账号
    QString m_password;                 // 密码

    int OpenCount;                      // 成功连接数量
};

#endif // DATABASEPOOL_H
