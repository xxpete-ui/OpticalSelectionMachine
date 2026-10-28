"""
ImageSeg 构建脚本（Release x64 / Debug x64）

为什么用 Python 而不是 vcvars64.bat ？
  本机 vcvars64.bat 依赖 reg.exe 探测 Windows SDK，若探测失败会导致
  INCLUDE 缺少 SDK 头文件（stddef.h 找不到）。这里直接按已知路径拼环境变量，
  结果确定、可重复。

用法:
    python build.py            # Release 构建
    python build.py debug      # Debug 构建
    python build.py clean      # 清理产物
"""

import os
import subprocess
import sys
from pathlib import Path

# ---------------- 本机工具链配置 ----------------
VS_MSVC = Path(r"D:\Visual Studio Community\VC\Tools\MSVC\14.37.32822")
SDK = Path(r"C:\Program Files (x86)\Windows Kits\10")
SDK_VER = "10.0.22621.0"
QT = Path(r"D:\Qt\6.5.3\msvc2019_64")
JOM = Path(r"D:\Qt\Tools\QtCreator\bin\jom\jom.exe")

PROJECT_DIR = Path(__file__).resolve().parent
PRO_FILE = PROJECT_DIR / "ImageSeg.pro"


def check_paths():
    need = {
        "MSVC 编译器": VS_MSVC / "bin" / "Hostx64" / "x64" / "cl.exe",
        "Windows SDK (ucrt)": SDK / "Include" / SDK_VER / "ucrt" / "stddef.h",
        "Qt qmake": QT / "bin" / "qmake.exe",
        "jom": JOM,
    }
    missing = [f"{name}: {path}" for name, path in need.items() if not path.exists()]
    if missing:
        print("[ERROR] 以下组件缺失：")
        for m in missing:
            print("   -", m)
        sys.exit(1)


def build_env():
    include = [
        VS_MSVC / "include",
        VS_MSVC / "ATLMFC" / "include",
        SDK / "Include" / SDK_VER / "ucrt",
        SDK / "Include" / SDK_VER / "shared",
        SDK / "Include" / SDK_VER / "um",
        SDK / "Include" / SDK_VER / "winrt",
        SDK / "Include" / SDK_VER / "cppwinrt",
    ]
    lib = [
        VS_MSVC / "lib" / "x64",
        VS_MSVC / "ATLMFC" / "lib" / "x64",
        SDK / "Lib" / SDK_VER / "ucrt" / "x64",
        SDK / "Lib" / SDK_VER / "um" / "x64",
    ]
    path = [
        VS_MSVC / "bin" / "Hostx64" / "x64",
        SDK / "bin" / SDK_VER / "x64",      # rc.exe / mt.exe，链接阶段需要
        QT / "bin",
        JOM.parent,
    ]
    env = os.environ.copy()
    env["INCLUDE"] = ";".join(str(p) for p in include)
    env["LIB"] = ";".join(str(p) for p in lib)
    env["PATH"] = ";".join(str(p) for p in path) + ";" + env.get("PATH", "")
    env["VSLANG"] = "1033"          # 让编译输出保持英文，便于排查
    return env


def run(cmd, env):
    print(">>", " ".join(str(c) for c in cmd))
    return subprocess.call([str(c) for c in cmd], cwd=str(PROJECT_DIR), env=env)


def main():
    config = "debug" if "debug" in [a.lower() for a in sys.argv[1:]] else "release"
    check_paths()
    env = build_env()

    makefile = PROJECT_DIR / ("Makefile.Debug" if config == "debug" else "Makefile.Release")

    if "clean" in [a.lower() for a in sys.argv[1:]]:
        if makefile.exists():
            run([JOM, "-f", makefile.name, "clean"], env)
        for d in (PROJECT_DIR / "release", PROJECT_DIR / "debug"):
            if d.is_dir():
                for f in d.glob("*.obj"):
                    f.unlink()
        print("[OK] 清理完成")
        return 0

    if not makefile.exists():
        print("[qmake] 生成构建文件 ...")
        rc = run([QT / "bin" / "qmake.exe", PRO_FILE.name, "-spec", "win32-msvc",
                  f"CONFIG+={config}"], env)
        if rc != 0:
            print("[ERROR] qmake 失败")
            return rc

    print(f"[jom] 编译 ({config}) ...")
    rc = run([JOM, "-f", makefile.name, "-j8"], env)
    if rc != 0:
        print("[ERROR] 编译失败")
        return rc

    exe = PROJECT_DIR / config / "ImageSeg.exe"
    print(f"\n[OK] 构建成功: {exe}")
    print("     运行: python run.py  或  双击 run.bat（会自动部署依赖）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
