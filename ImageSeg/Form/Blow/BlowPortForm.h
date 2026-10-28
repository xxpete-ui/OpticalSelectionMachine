#ifndef BLOWPORTFORM_H
#define BLOWPORTFORM_H

#include <QDialog>
#include <QWidget>
#include <QToolButton>
#include <QVector>

#include "Server/Server.h"

namespace Ui {
class BlowPortForm;
}

class BlowPortForm : public QDialog
{
    Q_OBJECT

public:
    explicit BlowPortForm(QWidget *parent = nullptr);
    ~BlowPortForm();

private:
    ///
    /// \brief 初始化按钮
    ///
    void InitPotrBtn();



private:
    Ui::BlowPortForm *ui;

    QVector<QToolButton*> m_btns;

};

#endif // BLOWPORTFORM_H
