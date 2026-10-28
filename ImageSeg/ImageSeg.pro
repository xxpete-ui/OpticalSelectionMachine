QT  += core gui sql concurrent
# 本机 Qt 6.5.3 未安装 Charts 模块，且代码中未使用任何 QtCharts 类，故移除
# QT += charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Collector/CountCollector.cpp \
    Collector/PlcClient.cpp \
    Database/DatabasePool.cpp \
    Form/BaseForm.cpp \
    Form/Blow/BlowPortForm.cpp \
    Form/BlurredFrame.cpp \
    Form/ImageDisplayWidget.cpp \
    Form/SettingForm.cpp \
    Form/TableForm.cpp \
    NetWork/ModbusTcpSlave.cpp \
    Server/Camera.cpp \
    Server/ImageProcessing.cpp \
    Server/Server.cpp \
    Server/State.cpp \
    Server/logmanager.cpp \
    Server/timebasedfoldermanager.cpp \
    Yolo/Yolov8ClsOnnx.cpp \
    Yolo/Yolov8SegOnnx.cpp \
    Yolo/Yolov8_Utils.cpp \
    main.cpp \
    Widget.cpp

HEADERS += \
    Collector/CountCollector.h \
    Collector/PlcClient.h \
    Database/DatabasePool.h \
    Database/SqlMapping.h \
    Database/Table/TableInfo.h \
    Form/BaseForm.h \
    Form/Blow/BlowPortForm.h \
    Form/BlurredFrame.h \
    Form/ImageDisplayWidget.h \
    Form/SettingForm.h \
    Form/TableForm.h \
    NetWork/ModbusTcpSlave.h \
    Server/Camera.h \
    Server/ImageProcessing.h \
    Server/Server.h \
    Server/State.h \
    Server/logmanager.h \
    Server/timebasedfoldermanager.h \
    Tools/CircularQueue.h \
    Widget.h \
    Yolo/Yolov8ClsOnnx.h \
    Yolo/Yolov8SegOnnx.h \
    Yolo/Yolov8_Utils.h

FORMS += \
    Form/BaseForm.ui \
    Form/Blow/BlowPortForm.ui \
    Form/ImageDisplayWidget.ui \
    Form/SettingForm.ui \
    Form/TableForm.ui \
    widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resouarce.qrc

win32 {
    DEFINES += WIN32_LEAN_AND_MEAN
}

# qmake writes out-of-range relative Qt paths ( ..\..\..\..\..\Qt\... ) on this
# machine, which breaks shadow builds with "dependent ... does not exist".
# Force absolute paths so both in-source and shadow builds work.
unix|win32: LIBS += -L$$PWD/../dll/onnxruntime-win-x64-gpu-1.18.0/lib/ -lonnxruntime

INCLUDEPATH += $$PWD/../dll/onnxruntime-win-x64-gpu-1.18.0/include
DEPENDPATH += $$PWD/../dll/onnxruntime-win-x64-gpu-1.18.0/include

# win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../opencv/opencv-4.8.1/build/x64/vc16/lib/ -lopencv_world481
# else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../opencv/opencv-4.8.1/build/x64/vc16/lib/ -lopencv_world481d
# else:unix: LIBS += -L$$PWD/../../../opencv/opencv-4.8.1/build/x64/vc16/lib/ -lopencv_world481

# INCLUDEPATH += $$PWD/../../../opencv/opencv-4.8.1/build/include
# DEPENDPATH += $$PWD/../../../opencv/opencv-4.8.1/build/include

unix|win32: LIBS += -L$$PWD/../dll/MvCameraControl/lib/ -lMvCameraControl

INCLUDEPATH += $$PWD/../dll/MvCameraControl/include
DEPENDPATH += $$PWD/../dll/MvCameraControl/include

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../dll/opencv-4.8.1/build/x64/vc16/bin/ -lopencv_world481
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../dll/opencv-4.8.1/build/x64/vc16/bin/ -lopencv_world481d
else:unix: LIBS += -L$$PWD/../dll/opencv-4.8.1/build/x64/vc16/bin/ -lopencv_world481

INCLUDEPATH += $$PWD/../dll/opencv-4.8.1/build/include
DEPENDPATH += $$PWD/../dll/opencv-4.8.1/build/include



unix|win32: LIBS += -L$$PWD/../dll/MotionSample/bin/ -lMCDLL_NET

INCLUDEPATH += $$PWD/../dll/MotionSample/include
DEPENDPATH += $$PWD/../dll/MotionSample/include

# MCDLL_NET.dll 静态依赖 WinPcap 的 wpcap.dll
# 未安装 Npcap/WinPcap 的机器上静态链接会导致进程启动即失败(0xC0000135)
# 改为延迟加载: 没有运动卡/抓包驱动时程序仍可正常启动, 仅在真正吹气时才报错
win32: QMAKE_LFLAGS += /DELAYLOAD:MCDLL_NET.dll
win32: LIBS += delayimp.lib


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../dll/libmodbus/bin/ -lmodbus
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../dll/libmodbus/bin/ -lmodbus
else:unix: LIBS += -L$$PWD/../dll/libmodbus/ -lmodbus
INCLUDEPATH += $$PWD/../dll/libmodbus/include
DEPENDPATH += $$PWD/../dll/libmodbus/include
# LIBS += -lws2_32
# unix|win32: LIBS += -L$$PWD/../dll/libmodbus/bin/ -lmodbus
# INCLUDEPATH += $$PWD/../dll/libmodbus/include
# DEPENDPATH += $$PWD/../dll/libmodbus/include




