#ifndef TABLEFORM_H
#define TABLEFORM_H

#include <QWidget>
#include <QVector>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QMap>

namespace Ui {
class TableForm;
}

class TableForm : public QWidget
{
    Q_OBJECT

public:
    explicit TableForm(QWidget *parent = nullptr);
    ~TableForm();

    ///
    /// \brief 设置表数据
    ///
    void SetDataStart();

private:
    ///
    /// \brief 创建一行数据行进入控件池
    ///
    void CreatDataRow(QString typeName);

    ///
    /// \brief 初始化控件池
    ///
    void InitDataTable();

    ///
    /// \brief 更新表数据
    ///
    void UpdateTable();

    ///
    /// \brief 设置显示数据（本次/本日）
    ///
    void SetDataState();

private:
    Ui::TableForm *ui;

    QMap<QString, QLabel*> m_typeLabel;   // 品类名label
    QMap<QString, QLabel*> m_ritoLabel;   // 比例label
    QMap<QString, QLabel*> m_countLabel;  // 数量label
    QMap<QString, QProgressBar*> m_progress;  // 进度条

    QTimer* timer;

    bool ShowDate = false;
};

#endif // TABLEFORM_H
