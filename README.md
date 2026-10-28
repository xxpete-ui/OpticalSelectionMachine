# OpticalSelectionMachine / ImageSeg

工业视觉选别系统。相机采图 → YOLOv8-seg 定位物体 → YOLOv8-cls 判定品类 → 运动控制卡按编码器位置吹气剔除。

Qt 6 + qmake + C++17，ONNX Runtime 推理，OpenCV 图像处理。

---

## 处理链路

```
相机回调 (MVS SDK)
  2048×512 RGB  ──►  两帧纵向拼接成 2048×1024  ──►  frameQueue(20, 覆盖)
                                                        │
                          ┌─────────────────────────────┘
                          ▼
            ImageSegWorker  [线程优先级 HIGHEST]
            LetterBox 缩放到 640×640 → YOLOv8-seg 推理 → decode → NMS → mask
                          │  m_resultYolo(100, 覆盖)
                          ▼
            ImageClsWorker  [HIGHEST]
            旋转矩形抠图 → 5 道尺寸/区域过滤 → 跨帧去重 → 缩放到 224×224 → YOLOv8-cls
                          │
              ┌───────────┴────────────┐
              ▼                        ▼
        showImgQueue(20, 阻塞)    m_clsResult(100, 覆盖)
              │                        │
      UI 50ms 定时器拉取         ImageRangeWorker
      大图 + 小图刷新           编码器位置换算 → 逐孔吹气
```

另有 `Timer_1s` 线程每秒把累计产量写入 SQLite。

**设计取向**：队列「满了覆盖最老的」，实时优先、允许丢帧。只有 `showImgQueue` 是阻塞队列，作为唯一的背压点。

---

## 目录结构

```
.
├─ ImageSeg/                    主工程（qmake .pro）
│  ├─ main.cpp / Widget.cpp/h   程序入口 + 主窗口
│  ├─ Server/                   核心业务
│  │  ├─ Server.cpp             生命周期、配置加载、本地图片调度
│  │  ├─ State.cpp/h            全局状态与 4 个环形队列
│  │  ├─ ImageProcessing.cpp    分割线程 / 分类线程 / 吹气线程（业务规则集中地）
│  │  └─ Camera.cpp             海康相机 SDK 封装
│  ├─ Yolo/                     ONNX Runtime 推理封装
│  │  ├─ Yolov8SegOnnx.cpp      分割：预处理 + decode + mask 后处理
│  │  ├─ Yolov8ClsOnnx.cpp      分类
│  │  └─ Yolov8_Utils.cpp       LetterBox / GetMask2 / DrawPred
│  ├─ Form/                     自定义控件与各功能页
│  ├─ Collector/                产量统计、PLC 客户端
│  ├─ Database/                 SQLite 连接池（配置中心）
│  ├─ NetWork/                  Modbus TCP 从站
│  ├─ Tools/                    CircularQueue 等基础组件
│  ├─ widget.ui                 主界面
│  ├─ images/                   UI 资源（被 resouarce.qrc 引用）
│  ├─ runtime_data/             运行数据母本：数据库 + ONNX 模型
│  ├─ docs/                     深入讲解文档
│  ├─ build.py / deploy.py / run.py   构建、部署、一键运行
│  └─ gen_vs.py                 生成 Visual Studio 工程
│
├─ learn/                       学习材料（多线程练功房 + 学习计划）
└─ dll/                         第三方 SDK（不入库，见下节）
```

---

## 配置在哪

**没有 ini / json，唯一配置源是 SQLite 数据库**：

```
<exe 同级>\database\TaiChuGX.db
```

| 表 | 作用 |
|---|---|
| `ConfigInfo` | 30 项运行参数（阈值、相机 IP、吹气孔、PLC 地址…），界面「设置」页保存时写回 |
| `ModeInfo` | 模式定义：分割模型名、分类模型名、通道数、通道分界线 |
| `ModeDetail` | 各模式下的品类、对应分类模型序号、是否吹气、吹气孔排序 |
| `DailyOutput` / `DailyOutputDetail` | 产量统计，程序运行自动写 |

模型文件名取自 `ModeInfo` 表，**不是写死在代码里**：

```sql
select MainId, Channel, SegModel, ClsModels from ModeInfo;
-- 1 | 1 | hei_huizong_250428.onnx | hezhuang5.onnx
```

`SegModel` 去 `mode\seg\` 下找，`ClsModels` 去 `mode\cls\` 下找（多通道逗号分隔）。
**换模型 = 丢文件进目录 + 改表，不用重新编译。**

> ⚠ 分类模型输出的类别顺序，与 `ModeDetail` 按 `SubId` 排序后的 `TypeName` 顺序**必须一致**。
> 换分类模型时如果没同步重排这张表，不会报错，只会静默错判。

---

## 依赖（dll/ 不入库，需自行下载）

仓库不含第三方二进制（合计约 2.2 GB）。按下列路径放置后才能构建：

| 路径 | 内容 | 说明 |
|---|---|---|
| `dll/opencv-4.8.1/build/x64/vc16/` | OpenCV 4.8.1 | 需 `include/`、`lib/`、`bin/` |
| `dll/onnxruntime-win-x64-gpu-1.18.0/` | ONNX Runtime 1.18.0 GPU | 需 `include/`、`lib/` |
| `dll/MvCameraControl/` | 海康 MVS SDK | `include/` + `lib/` |
| `dll/MotionSample/` | 运动控制卡 SDK (`MCDLL_NET`) | `include/` + `bin/` |
| `dll/libmodbus/` | libmodbus | `include/` + `bin/` |
| `dll/yaml-cpp/` | yaml-cpp | 头文件 + 库 |
| `dll/nlohmann/` | nlohmann/json | 纯头文件 |

其余环境要求：

- **Qt 6.5+**（msvc 套件），含 `core gui widgets sql concurrent`
- **Visual Studio 2022** + Windows SDK 10
- **Npcap**（可选）：`MCDLL_NET.dll` 依赖 `wpcap.dll`。未安装时可正常启动（已配 `/DELAYLOAD`），仅吹气功能不可用
- **CUDA 12.x + cuDNN 8.9**（可选）：`UseGPU=1` 时必需；只装 CUDA 11.8 需在设置页把推理后端切到 CPU

---

## 构建与运行

### 命令行（推荐）

```bat
cd ImageSeg
python build.py            :: 编译 Release
python build.py debug      :: 编译 Debug
python deploy.py           :: 部署 Qt DLL / 第三方 DLL / 模型 / 数据库到 release\
python run.py              :: 编译 + 部署 + 启动
```

产物在 `ImageSeg\release\ImageSeg.exe`，可独立分发整个 `release\` 目录。

### Visual Studio 调试

```bat
python gen_vs.py           :: 由 .pro 生成 ImageSeg.sln / .vcxproj
```

打开 `ImageSeg.sln` → 选 **Debug + x64** → F5。
脚本已自动补上两处 VS 需要但 qmake 会丢的配置：`/DELAYLOAD:MCDLL_NET.dll` 与调试工作目录 `$(OutDir)`。

### Qt Creator

打开 `ImageSeg.pro`，选 Qt 6.5.x MSVC 套件，**取消勾选 Shadow build**。
本机 qmake 会把 Qt 头文件写成五级越界相对路径（`..\..\..\..\..\Qt\...`），影子构建会报 `dependent does not exist`。

---

## 用本地图片回放（不打相机）

界面左侧「图像」页 → 选择文件夹 → 勾选「使用本地图片」→ 上一张 / 下一张。

- 支持 `jpg / jpeg / png / bmp / tif / tiff`，只扫一层，按文件名排序
- 一张图 = 一帧完整大图，不做相机那种纵向拼接
- 本地模式**强制不吹气**，避免调用不存在的运动控制卡

**分辨率限制**：模型输入 640×640，整图先做 LetterBox 等比缩放到 640 宽。

- 宽 2048（与相机一致）→ 缩放比 0.31，效果等同产线
- 宽 8000+ 的长条原图 → 物体只剩几十像素，基本检不出，需先按 2048 宽切段

此外 `Yolo/Yolov8_Utils.cpp` 的 `GetMask2()` 里坐标钳制硬编码为 `2048×1024`，
**图片高度超过 1024 时超出部分的框会被压扁**，分类必然出错。

---

## 文档

| 文档 | 内容 |
|---|---|
| [`ImageSeg/README_本机构建说明.md`](ImageSeg/README_本机构建说明.md) | 本机环境实测、为跑通所做的修改清单、目录说明 |
| [`ImageSeg/docs/本地图片运行逻辑.md`](ImageSeg/docs/本地图片运行逻辑.md) | 14 章完整链路讲解：从读配置、加载模型到结果输出，带行号定位与断点表 |
| [`learn/学习计划.md`](learn/学习计划.md) | 多线程队列设计 + ONNX Runtime C++ 部署的 5 周计划 |

---

## 已知限制

| 项 | 说明 |
|---|---|
| 忙轮询 | 三个 worker 的 `pop()` 非阻塞且循环不 sleep，空闲时实测约 185% 单核占用。框架内有 `pop_wait()` 未使用 |
| 坐标硬编码 | `GetMask2` 钳制 2048×1024，界面分界线写死 `y=512`。换分辨率需改代码 |
| `SizeLimit` 无效 | 配置项读入内存但代码中从未判断，界面开关是摆设 |
| 尺寸过滤逻辑 | `cols < 20 && rows < 20` 用的是 `&&`，长条物（如 10×500）不会被过滤 |
| 线程退出 | worker 线程 `detach()` 后无 stop / join 流程 |
| `SaveImage()` | 声明未实现，属预留接口 |
