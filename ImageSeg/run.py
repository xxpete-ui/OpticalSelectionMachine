"""
ImageSeg 一键运行脚本

顺序：构建(可选) -> 部署依赖 -> 启动程序

用法:
    python run.py            # Release：编译 + 部署 + 运行
    python run.py debug      # Debug
    python run.py --no-build # 跳过编译，直接部署并运行
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def main():
    args = [a.lower() for a in sys.argv[1:]]
    config = "debug" if "debug" in args else "release"
    exe = ROOT / config / "ImageSeg.exe"

    if "--no-build" not in args:
        rc = subprocess.call([sys.executable, str(ROOT / "build.py"), config])
        if rc != 0:
            return rc

    rc = subprocess.call([sys.executable, str(ROOT / "deploy.py"), config])
    if rc != 0:
        return rc

    if not exe.exists():
        print("[ERROR] 找不到可执行文件:", exe)
        return 1

    print("\n启动:", exe)
    subprocess.Popen([str(exe)], cwd=str(exe.parent))
    return 0


if __name__ == "__main__":
    sys.exit(main())
