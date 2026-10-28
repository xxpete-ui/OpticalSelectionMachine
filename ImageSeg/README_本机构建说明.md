# ImageSeg 本机构建 / 运行说明

> 目标：在**本机**（VS2022 + Qt 6.5.3）上把原开发机（Qt 6.11）的项目跑起来。
> 状态：✅ 已编译通过、已部署、可正常启动（无相机时界面正常，检测/吹气功能需接硬件）。

---

## 一、本机环境实测

| 组件 | 实际路径 / 版本 |
| --- | --- |
| Visual Studio | VS2022 Community 17.7.3，`D:\Visual Studio Community`（MSVC 14.37.32822） |
| 另一套 VS | VS2022 Build Tools 17.7.2，`D:\VS`（只有编译器，无 IDE） |
| Windows SDK | 10.0.22621.0，`C:\Program Files (x86)\Windows Kits\10` |
| Qt | 6.5.3 **msvc2019_64**，`D:\Qt\6.5.3\msvc2019_64` |
| Qt 工具 | Qt Creator + jom：`D:\Qt\Tools\QtCreator\bin\jom\jom.exe` |
| 显卡 | RTX 4070（CUDA Toolkit 只装了 **11.8**，没有 cuDNN） |

> 说明：项目是 **qmake（.pro）+ Qt**，不是 CMake 工程。Qt 6.5.3 只有 msvc2019_64 套件，
> 与 VS2022 的 MSVC 14.37 二进制兼容，可以正常链接。

---

## 二、一键构建 / 运行

```bat
cd D:\Project\OpticalSelectionMachine\ImageSeg

python build.py     :: 编译（Release）
python deploy.py    :: 拷贝 Qt / 第三方 DLL / 数据库 / 模型 到 release\
python run.py       :: 编译 + 部署 + 启动
python build.py debug   :: Debug 版本
```

产物：`ImageSeg\release\ImageSeg.exe`（Debug 在 `debug\`）

脚本要点：**不调用 `vcvars64.bat`**，而是直接按已知路径拼 `INCLUDE / LIB / PATH`。
原因：本机 vcvars 探测 Windows SDK 时会用到被安全策略拦截的 `reg.exe`，
导致 `INCLUDE` 缺 SDK 头文件，编译报 `无法打开包括文件: stddef.h`。

---

## 三、为跑通本机所做的修改（共 11 处）

### 1. `ImageSeg.pro`：依赖库路径 `../../dll` → `../dll`
原工程假设 `ImageSeg` 在两层子目录里（`D:\Project\dll`），实际本机是
`D:\Project\OpticalSelectionMachine\dll`，少一层。

### 2. `ImageSeg.pro`：移除 `charts`
`QT += core gui sql concurrent charts` → 本机 Qt 6.5.3 **没装 Qt Charts**，
且代码里没有任何 QtCharts 类，去掉即可（已注释保留说明）。

### 3. `Widget.cpp`：`#include <QtConcurrentTask>` → `#include <QtConcurrent/QtConcurrent>`
`QtConcurrentTask` 是 Qt 6.11 才有的便捷头，6.5.3 里只有 `qtaskbuilder.h`，
用汇总头即可，`QtConcurrent::task(...)` 在 6.5 里可用。

### 4. `Widget.h`：补 `#include <QLabel>`
`QPointer<QLabel>` 需要完整类型，否则报 `C2923 / C3203`，
并连带 `QList<int>::push_back(QPointer<QLabel>)` 一堆错误。

### 5. `Server/State.cpp` + `Server/Server.cpp`：模型路径不再写死
原代码写死上一台机器的绝对路径：
`E:/work/ImageSeg/build/Desktop_Qt_6_11_0_.../mode/seg`。
改为 `applicationDirPath() + "/mode/seg"`，并且**赋值放在 `Server::Init()` 里**——
放在静态初始化阶段会因为 `QApplication` 还没创建而拿到空串
（日志里的 `QCoreApplication::applicationDirPath: Please instantiate the QApplication object first`）。

### 6. `Server/Server.cpp`：`UseGPU` 解析修正
`ConfigInfo.Value` 是文本 `"0"/"1"`，`QVariant("0").toBool()` 返回 **true**，
导致设置页选 CPU 永远不生效。改为 `toInt() != 0`。

### 7. `ImageSeg.pro`：MCDLL_NET 改为延迟加载
运动控制卡 SDK（`MCDLL_NET.dll`）静态依赖 WinPcap 的 `wpcap.dll`，
本机没装 Npcap/WinPcap → 进程启动即 `0xC0000135`（找不到 DLL）。
加 `/DELAYLOAD:MCDLL_NET.dll` 后，没装抓包驱动也能启动，
只有真正吹气（调用 `MCF_Add_Compare_Point_Net`）时才会报错。

### 8. `Widget.h/.cpp`：本地图片勾选框完全失效（Qt 版本差异）
Qt 的自动连接规则是 `on_<对象名>_<信号名>`。原代码写的是
`on_Tab2_UseImg_CheckBox_checkStateChanged(...)`——`checkStateChanged` 是 **Qt 6.7** 才有的信号名，
本机 Qt 6.5.3 只有 `stateChanged(int)`，名字对不上，`connectSlotsByName` 静默失败：
**勾选「使用本地图片」不会有任何效果**（启动时 stderr 会打一条
`No matching signal for on_Tab2_UseImg_CheckBox_checkStateChanged`）。
用 `#if QT_VERSION >= QT_VERSION_CHECK(6,7,0)` 两个版本各写一个槽，两边都能自动连上。

### 9. `Server::UpImage()`：在第一张时按「上一张」会越界崩溃
`readIndex` 语义是「下一张要播的位置」，原实现 `at(readIndex - 1)`，
在 `readIndex == 0` 时等于 `at(-1)`（`size_t` 下溢），直接抛异常退出。已改成钳制到第一张。

### 10. 本地图片只认 `.jpg`，且中文路径读不出来
原来用 `cv::glob(path + "/*.jpg")`：只匹配 jpg（现场存的多半是 bmp/tif），
而且 `cv::glob` / `cv::imread` 在 Windows 下走窄字符 API，**中文目录一律读不到**。
改成 `QDir` 枚举（6 种后缀）+ `QFile` 读字节 + `cv::imdecode`，中文路径可用。

### 11. `ImageProcessing`：本地回放时不再触发吹气
`State::BlowsSet::IsBlow` 默认 `true`，回放图片时一旦分类通过就会进
`ImageRangeWorker` 调 `MCF_Add_Compare_Point_Net`。本机没运动卡、也没装 Npcap，
延迟加载的 `MCDLL_NET.dll` 加载失败会让**进程直接崩掉**。
已在 `ImageClsWorker` 里加 `&& !IsReadImg`：本地回放只统计不吹气。

---

## 四、运行时依赖怎么来的

`deploy.py` 做三件事：

1. `windeployqt` 拷贝 Qt 运行库 + `platforms\qwindows.dll` + `sqldrivers\qsqlite.dll`
2. 拷贝第三方 DLL：OpenCV、ONNX Runtime（含 CUDA/TensorRT provider）、
   海康 MVS 全套（16 个）、`MCDLL_NET.dll`、`modbus.dll`
3. 拷贝运行时数据到 exe 同级：
   - `database\TaiChuGX.db`（配置 + 产量记录，**已存在不覆盖**）
   - `mode\seg\hei_huizong_250428.onnx`、`mode\cls\hezhuang5.onnx`

数据母本放在 `ImageSeg\runtime_data\`，脚本从这里复制。

---

## 五、配置文件、模型、运行时数据都在哪

### 5.1 没有 ini / json，唯一配置源是这个 SQLite 库

```
<exe 同级>\database\TaiChuGX.db
```

`release\ImageSeg.exe` 读的就是 `release\database\TaiChuGX.db`；
`debug\` 目录下有一份独立的副本。**两份互不影响**，改哪份只影响对应目录跑的程序。

| 表 | 作用 |
|---|---|
| `ConfigInfo` | 30 项运行参数（Key/Value 两列），界面「设置」页保存时写回这里 |
| `ModeInfo` | 模式定义：分割模型名、分类模型名、通道数、通道分界线 |
| `ModeDetail` | 每个模式下的品类、对应的分类模型、该品类是否吹气、吹气孔排序 |
| `DailyOutput` / `DailyOutputDetail` | 产量统计，程序运行自动写，一般别动 |

改配置的两种方式：

- **推荐**：程序界面 →「设置」→ 改 → 保存（会立刻写库并生效）
- 直接用 DB Browser for SQLite 之类工具改（**先退出程序**，否则改动可能被覆盖）

常用键：

| Key | 含义 | 现值 |
|---|---|---|
| `Mode` | 当前模式，对应 `ModeInfo.MainId` | 1 |
| `UseGPU` | 1=GPU，0=CPU | **0**（本机只能 CPU，见第七节） |
| `SegThreshold` / `ClsThreshold` | 分割 / 分类置信度阈值 | 0.6 / 0.7 |
| `MinWidth` `MinHeight` `MaxWidth` `MaxHeight` | 物体尺寸过滤（像素） | 20/20/800/800 |
| `BlowAreaStart` / `BlowAreaEnd` | 只有底边落在这个 Y 区间内才处理 | 450 / 1020 |
| `CameraIP` | 相机 IP | 169.254.166.205 |
| `HightProportion` / `LineRateRatio` | 相机高度比例 / 线率比（决定大图纵向拼接比例） | 4 / 1.3 |
| `PoreCount` `PoreInterval` `BlowStartX` `BlowOffset` `BlowTime` `ZoomRatio` | 吹气孔数量、间距、起始 X、Y 偏移、时长、物体吹气点比例 | 80/22/0/9200/23/0.8 |
| `PLC_IP` `PLC_Port` `Modbus_IP` `Modbus_Port` | 与 PLC / Modbus 通信 | 192.168.8.10:502 |

### 5.2 模型放哪

路径是**跟着 exe 目录**走的（已修掉原代码里写死的 `E:/work/...`）：

```
release\
├─ mode\
│  ├─ seg\hei_huizong_250428.onnx     ← 分割模型（YOLOv8-seg）
│  └─ cls\hezhuang5.onnx              ← 分类模型（YOLOv8-cls）
└─ database\TaiChuGX.db
```

**文件名不是写死在代码里的**，来自 `ModeInfo` 表：

```sql
select MainId, Channel, ChannelBoundary, SegModel, ClsModels from ModeInfo;
-- 1 | 1 | 0 | hei_huizong_250428.onnx | hezhuang5.onnx
```

`SegModel` 去 `mode\seg\` 下找，`ClsModels`（多通道用逗号分隔）去 `mode\cls\` 下找。
所以**换模型 = 丢文件进对应目录 + 改这张表的文件名**，不用改代码重编译。

`mode\cls\hezhuang5.yaml` 是训练导出的副产物，本工程不读它，少了也能跑。

### 5.3 一个能跑起来的目录长这样

```
release\
├─ ImageSeg.exe
├─ database\TaiChuGX.db                  配置 + 模型名绑定
├─ mode\seg\*.onnx  mode\cls\*.onnx     模型
├─ Qt6Core.dll / Qt6Gui.dll / Qt6Widgets.dll / Qt6Sql.dll / Qt6Network.dll
├─ platforms\qwindows.dll                 Qt 平台插件（缺了会报 "could not find platform plugin"）
├─ sqldrivers\qsqlite.dll                 SQLite 驱动（缺了连不上配置库）
├─ onnxruntime.dll / onnxruntime_providers_shared.dll / *_cuda.dll
├─ opencv_world481.dll
├─ MvCameraControl.dll 等海康 MVS 的 16 个 DLL    相机 SDK
├─ MCDLL_NET.dll                        运动控制卡（延迟加载，没卡也能启动）
└─ modbus.dll
```

`python deploy.py` 会一次性把这些凑齐，`runtime_data\` 是数据母本。

> `TaiChuGX.db` 是**运行时数据**，deploy 遇到已存在的库不会覆盖，
> 所以你现场调好的参数不会被下次部署冲掉。

---

## 六、用本地图片回放（不打相机，直接跑图）

程序内置离线回放：把一整个文件夹的图当成大图，一张张送进分割/分类流程。

### 操作

1. 主界面左侧点 **图像** 图标（进入「图像」页）
2. 点 **选择文件夹**，指定放图的目录
3. 勾选 **使用本地图片**
4. 点 **下一张 / 上一张** 逐张跑

### 支持格式与读取规则

- `jpg / jpeg / png / bmp / tif / tiff`，**只扫这一层，不递归子目录**，按文件名排序
- 一张图 = 一帧完整大图，**不做相机那种纵向拼接**
- 本地模式下**不会触发吹气**（已强制屏蔽，避免去调不存在的运动卡）

### ⚠ 两个必须知道的分辨率限制

**限制一：整图会被缩到 640 宽**

模型输入是 **640×640**，进来之前会做 `LetterBox` 等比缩放 + 灰边填充。

- 图片宽 2048（和相机一致）→ 缩放比 0.3125，和产线实际效果相同，可以直接看
- 图片宽 8000~20000（相机存的长条大图）→ 缩放比 0.03~0.08，物体只剩几十个像素，**基本检不出**
- 想把长条大图跑出正确结果，需要先按宽度切成 2048 左右的段再逐段跑

**限制二：高度不能超过 1024**

`GetMask2()` 里坐标是硬编码钳制的（Yolov8_Utils.cpp:230-231、247-248）：

```cpp
if (vertices[i].x > 2048) vertices[i].x = 2048;
if (vertices[i].y > 1024) vertices[i].y = 1024;
```

这个 1024 来自"相机两帧 512 纵向拼接成一张大图"的设计，整条流水线都建立在这个前提上。
**本地图片如果高度超过 1024，超出部分的框会被压扁到 y=1024，裁剪出来的小图是错的，分类必然失败。**

所以本地图片的正确尺寸是 **2048×1024**（宽×高）。宽可以小，高不要超过 1024。

`test_images\` 下我放了 3 张 **2048×1024** 的测试图（黑底白块，物体底边落在 450~1020 的有效区内），
可以用它验证回放流程是否通。这些图只是为了验证「能读、能推理、不崩」，模型未必检出（不是真实瓶型），
确认流程没问题后可以删掉。

---

## 七、当前能跑到什么程度 / 还缺什么

**能跑：** 主界面、设置页、数据表格都正常，两个 ONNX 模型能加载并 warm up，
本地图片回放可用（见第六节）。

**没有相机：** 启动日志 `Camera:Enum devices success,num = 0` → `Find No Devices!`。
程序不会崩；海康 SDK 走的是回调取帧，没设备就没有帧，不存在空转。
接相机时在 设置页 把相机 IP 改成实际值（当前配置 `169.254.166.205`）。

**GPU 推理：** 已把 `UseGPU` 改成 **0（CPU）**。原因：本机只装了 CUDA 11.8，
而 onnxruntime-gpu 1.18 的 CUDA provider 要 **CUDA 12.x + cuDNN 8.9**。
实测 `UseGPU=1` 时日志走到 `Infer model on GPU!` → `Start warming up` 进程就直接退出了
（找不到 `cublas64_12.dll`）。要用 GPU 就装 CUDA 12 + cuDNN 8.9 并把 `bin` 加进 PATH。

**运动卡：** 装 Npcap（https://npcap.com）后 `wpcap.dll` 才在，吹气功能才可用。
本地回放图片时已强制不吹气，所以不装 Npcap 也能安心跑图。

**PLC / Modbus：** 配置是 `PLC_IP=192.168.8.10`、`Modbus_IP=127.0.0.1`，没接设备时连不上，
不影响界面启动。

---

## 八、在 Visual Studio 2022 里调试（推荐，不影响 Qt Creator 用法）

VS 打不开 `.pro`，所以用 qmake 生成一份 **vcxproj**：

```bat
cd D:\Project\OpticalSelectionMachine\ImageSeg
python gen_vs.py              :: 生成/重新生成 ImageSeg.sln + vcxproj
python deploy.py debug        :: 把 Qt/第三方 DLL、模型、数据库放进 debug\
```

然后：

1. 双击 `ImageSeg.sln`（或 VS 里「打开项目/解决方案」）
2. 顶部配置选 **Debug + x64**
3. `F5` 直接跑；断点、监视窗口、调用堆栈、线程窗口都正常

**gen_vs.py 做了什么**（qmake 的 `-tp vc` 有两个坑）：

| 问题 | 处理 |
| --- | --- |
| `/DELAYLOAD:MCDLL_NET.dll` 丢失 → 生成的 exe 又变回静态依赖 `wpcap.dll`，本机没 Npcap 就启动失败 | 脚本自动写回 `Link → 命令行 → 附加选项` |
| 调试工作目录默认是工程目录，找不到 `database\` 和 `mode\` | 写进 `ImageSeg.vcxproj.user`，设为 `$(OutDir)` |

附带好处：VS 调试时 `qDebug/qInfo` 走 `OutputDebugString`，**直接看 VS「输出」窗口**，
比在 Qt Creator 里还方便（相机枚举、数据库刷新、模型加载日志都在那儿）。

改过 `.pro`（增删文件）后：重新跑一次 `python gen_vs.py`，VS 里会提示「重新加载项目」。

**实用断点位置：**

| 位置 | 作用 |
| --- | --- |
| `Widget.cpp:59` | QtConcurrent 里的初始化入口（Init + Start） |
| `Server::Init()` | 读 ConfigInfo、设置模型路径 |
| `ImageProcessing::LoadModel()` | 加载 seg/cls 模型，看模型名和路径 |
| `Camera::OpenRGBCamera()` | 相机枚举、IP 匹配 |
| `ImageProcessing::SetPortBlow()` | 吹气点下发，唯一调运动卡的地方 |

> ⚠️ VS 和 `build.py`/Qt Creator 都是输出到同一个 `debug\`、`release\` 目录，
> 交叉使用前先「清理解决方案」，否则可能出现 obj 陈旧导致的怪问题。

---

## 九、在 Qt Creator 里打开

1. 用 `D:\Qt\Tools\QtCreator\bin\qtcreator.exe` 打开 `ImageSeg.pro`
2. 首次会提示配置套件，选 **Desktop Qt 6.5.3 MSVC2019 64bit**
   （旧的 `*.pro.user` 指向 Qt 6.11 套件，已备份为 `*.qt611.bak`）
3. **项目 → 构建设置 → 取消勾选「Shadow build」**
   原因：本机 qmake 会把 Qt 头文件依赖写成 `..\..\..\..\..\Qt\...` 这种越界相对路径，
   影子构建时 jom 直接报 `dependent '...' does not exist`；源码内构建没这个问题。
4. 运行前先执行一次 `python deploy.py`，把 DLL 和模型拷到 `release\`

---

## 十、目录说明

```
ImageSeg\
├─ build.py            编译脚本（Release/Debug）
├─ deploy.py           部署依赖脚本
├─ run.py              一键：编译+部署+启动
├─ gen_vs.py           生成 VS 工程（ImageSeg.sln / vcxproj / vcxproj.user）
├─ ImageSeg.sln        ★ VS2022 打开这个
├─ runtime_data\       数据库 + ONNX 模型（部署时的母本）
├─ test_images\        3 张 2048×1024 测试图，验证本地回放用，可删
├─ docs\               ★ 本地图片运行逻辑.md（完整链路讲解，学习用）
├─ release\            Release 产物 + 已部署的 DLL/模型/数据库
├─ debug\              Debug  产物 + 已部署的 DLL/模型/数据库  ← VS 调试用
└─ build\              ★ 旧机器(Qt 6.11)遗留的构建目录，可删
```
