
#ifndef TABLEINFO_H
#define TABLEINFO_H
#include <QString>
#include <QVariant>
#include <QObject>
#include <QVector>
#include <QSqlQuery>
#include <QDateTime>
#include <QDate>

namespace DatabaseTable
{
class ModeDetail;

class DailyOutputDetail;

///
/// \brief The ConfigInfo class
/// 配置项表
///
class ConfigInfo : public QObject
{
    Q_PROPERTY(int Main MEMBER Main)
    Q_PROPERTY(QString Text MEMBER Text)
    Q_PROPERTY(QString Key MEMBER Key)
    Q_PROPERTY(QString Value MEMBER Value)
public:
    int MainId;
    QString Text;
    QString Key;
    QVariant Value;
};

///
/// \brief The ModeInfo class
/// 模式配置主表
///
class ModeInfo
{
public:
    int MainId;     // 主键
    QString ModeText;   // 模式名称
    int Channel;        // 通道数
    int ChannelBoundary;    // 通道分界线
    QString SegModel;   // 分割模型名
    QVector<QString> ClsModels; // 分类模型（多个模型名使用 "," 隔开）
    QMap<QString, QVector<ModeDetail>> ClsTypes;  // 子表，分类模型的品类数据
};


///
/// \brief The ModeDetail class
/// 模式配置子表
///
class ModeDetail
{
public:
    int SubId;
    int MainId;     // 主表主键
    QString ClsModel; // 模型名
    QString TypeName;  // 品类名
    QString TypeName_CN;    // 中文品类名
    int TypeBlow;       // 是否选中（0-不选中，1-选中）
    int SortId;     //排序编号(modbus中位置排序)
};


///
/// \brief The DailyOutput class
/// 日产出表
///
class DailyOutput //: public QObject
{
    // Q_PROPERTY(int MainId MEMBER MainId)
    // Q_PROPERTY(QDate Date MEMBER Date)
    // Q_PROPERTY(int Count MEMBER Count)
    // Q_PROPERTY(QDateTime LastDate MEMBER LastDate)

public:
    int MainId;     // 主表主键
    QDate Date;   // 日期
    int Count;      // 总数量
    QDateTime LastDate;   // 最后修改时间
    QVector<DailyOutputDetail> DailyOutputDetail;   // 品类明细

    static DailyOutput fromQuery(const QSqlQuery& query)
    {
        DailyOutput o;
        o.MainId = query.value("MainId").toInt();
        o.Date = query.value("Date").toDate();
        o.LastDate = query.value("LastDate").toDateTime();
        o.Count = query.value("Count").toInt();
        return o;
    }
};

///
/// \brief The DailyOutputDetail class
/// 日产出明细表
///
class DailyOutputDetail //: public QObject
{
    // Q_OBJECT
    // Q_PROPERTY(int SunId MEMBER SunId)
    // Q_PROPERTY(int MainId MEMBER MainId)
    // Q_PROPERTY(QDate Date MEMBER Date)
    // Q_PROPERTY(QString TypeName MEMBER TypeName)
    // Q_PROPERTY(int Count MEMBER Count)
    // Q_PROPERTY(QDateTime LastDate MEMBER LastDate)
public:
    int SunId;          // 子表主键
    int MainId;         // 主表主键
    QDate Date;       // 日期
    QString TypeName;   // 品类名
    int Count;          // 数量
    QDateTime LastDate;   // 最后修改时间

    static DailyOutputDetail fromQuery(const QSqlQuery& query)
    {
        DailyOutputDetail o;
        o.MainId = query.value("MainId").toInt();
        o.SunId = query.value("SunId").toInt();
        o.Date = query.value("Date").toDate();
        o.LastDate = query.value("LastDate").toDateTime();
        o.TypeName = query.value("TypeName").toString();
        o.Count = query.value("Count").toInt();
        return o;
    }
};


}


#endif // TABLEINFO_H
