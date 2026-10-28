#ifndef SETTINGFORM_H
#define SETTINGFORM_H

#include <QWidget>

#include "Server/Server.h"

namespace Ui {
class SettingForm;
}

class SettingForm : public QWidget
{
    Q_OBJECT

public:
    explicit SettingForm(QWidget *parent = nullptr);
    ~SettingForm();

private slots:
    void on_SetTab_currentChanged(int index);

    void on_CameraSetBtn_clicked();

    void on_Tab2_OtherSet_Btn_clicked();

    void on_Tab3_Ok_Btn_clicked();

    void on_Tab3_PLC_Clear_Btn_clicked();

    void on_Tab3_AutoBlow_Btn_clicked();

    void on_Tab3_OnePortBlow_Btn_clicked();

    void on_Tab3_PLC_SpeedSave_Btn_clicked();

private:
    ///
    /// \brief 初始化下拉框
    ///
    void InitComboBox();

    ///
    /// \brief 初始化控件数据
    /// \param index    页面索引
    ///
    void InitValue(int index);

    void InitTab2Value();

    void InitTab3Value();


private:
    Ui::SettingForm *ui;

};

#endif // SETTINGFORM_H
