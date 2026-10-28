"""
ImageSeg 运行时部署脚本

把 exe 运行所需的东西全部放到 release/ (或 debug/) 目录：
  1. Qt 运行库 (windeployqt: Qt6Core/Gui/Widgets/Sql/Concurrent + platforms + sqldrivers)
  2. 第三方 DLL (OpenCV / ONNX Runtime / 海康 MVS / 运动控制卡 / libmodbus)
  3. 运行时数据 (database/TaiChuGX.db 、 mode/cls 、 mode/seg 、 Log)

用法:
    python deploy.py           # 部署 Release
    python deploy.py debug     # 部署 Debug
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

QT = Path(r"D:\Qt\6.5.3\msvc2019_64")
ROOT = Path(__file__).resolve().parent
DLL_DIR = ROOT.parent / "dll"
# 运行时数据（数据库 + 模型）放在项目内固定目录，避免依赖历史构建目录
DATA_SRC = ROOT / "runtime_data"

# 第三方 DLL 来源列表
THIRD_PARTY_DLLS = [
    DLL_DIR / "opencv-4.8.1" / "build" / "x64" / "vc16" / "bin",
    DLL_DIR / "onnxruntime-win-x64-gpu-1.18.0" / "lib",
    DLL_DIR / "MvCameraControl" / "lib",
    DLL_DIR / "MotionSample" / "bin",
    DLL_DIR / "libmodbus" / "bin",
]


def deploy_qt(target: Path, config: str):
    print("[1/3] windeployqt ...")
    exe = target / "ImageSeg.exe"
    env = os.environ.copy()
    env["PATH"] = str(QT / "bin") + ";" + env.get("PATH", "")
    cmd = [
        str(QT / "bin" / "windeployqt.exe"),
        "--no-translations",
        "--no-system-d3d-compiler",
        "--no-opengl-sw",
        "--compiler-runtime",
        "--dir", str(target),
    ]
    cmd.append("--debug" if config == "debug" else "--release")
    cmd.append(str(exe))
    rc = subprocess.call(cmd, env=env)
    if rc != 0:
        print("   [WARN] windeployqt 返回", rc)
    # SQLite 驱动是运行时才加载的插件，windeployqt 有时会漏，这里兜底
    sqldir = target / "sqldrivers"
    sqldir.mkdir(exist_ok=True)
    src = QT / "plugins" / "sqldrivers" / ("qsqlited.dll" if config == "debug" else "qsqlite.dll")
    if src.exists():
        shutil.copy2(src, sqldir / src.name)


def deploy_third_party(target: Path, config: str):
    print("[2/3] 拷贝第三方 DLL ...")
    # Debug 需要带 d 后缀的 opencv
    needed = [
        "opencv_world481d.dll" if config == "debug" else "opencv_world481.dll",
        "opencv_videoio_ffmpeg481_64.dll",
        "opencv_videoio_msmf481_64.dll",
        "onnxruntime.dll",
        "onnxruntime_providers_shared.dll",
        "onnxruntime_providers_cuda.dll",
        "onnxruntime_providers_tensorrt.dll",
        "MCDLL_NET.dll",
        "modbus.dll",
    ]
    # 海康 MVS 的整目录 DLL 全拷（GenICam 依赖链比较长）
    mvs_dir = DLL_DIR / "MvCameraControl" / "lib"
    if mvs_dir.is_dir():
        for f in mvs_dir.glob("*.dll"):
            shutil.copy2(f, target / f.name)
        print(f"   MVS: {len(list(mvs_dir.glob('*.dll')))} 个 DLL")

    copied = 0
    for name in needed:
        for d in THIRD_PARTY_DLLS:
            p = d / name
            if p.exists():
                shutil.copy2(p, target / name)
                copied += 1
                break
        else:
            print(f"   [WARN] 未找到 {name}")
    print(f"   第三方 DLL: {copied} 个")


def deploy_data(target: Path):
    print("[3/3] 部署运行时数据 ...")
    if not DATA_SRC.is_dir():
        print("   [WARN] 未找到数据源目录:", DATA_SRC)
        return
    pairs = [
        (DATA_SRC / "database", target / "database"),
        (DATA_SRC / "mode", target / "mode"),
    ]
    for src, dst in pairs:
        if not src.exists():
            print("   [WARN] 缺少:", src)
            continue
        for f in src.rglob("*"):
            if not f.is_file():
                continue
            out = dst / f.relative_to(src)
            out.parent.mkdir(parents=True, exist_ok=True)
            # 数据库是运行时数据（现场改过的配置、产量记录），已存在就不覆盖
            if f.suffix == ".db" and out.exists():
                print("   skip (已存在):", out.relative_to(ROOT))
                continue
            shutil.copy2(f, out)
        print("   ->", dst.relative_to(ROOT))
    # 程序启动时会在 exe 同级找 Log 目录
    (target / "Log").mkdir(exist_ok=True)


def main():
    config = "debug" if "debug" in [a.lower() for a in sys.argv[1:]] else "release"
    target = ROOT / config
    exe = target / "ImageSeg.exe"
    if not exe.exists():
        print(f"[ERROR] 未找到 {exe}，请先运行 python build.py")
        return 1

    print(f"部署目标: {target}\n")
    deploy_qt(target, config)
    deploy_third_party(target, config)
    deploy_data(target)
    print("\n[OK] 部署完成，可运行:", exe)
    return 0


if __name__ == "__main__":
    sys.exit(main())
