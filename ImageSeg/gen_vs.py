r"""
生成 Visual Studio 工程（ImageSeg.sln / ImageSeg.vcxproj）

qmake 自带的 `-tp vc` 能生成 vcxproj，但有两处不适合本机，这里补上：

  1. 链接参数 /DELAYLOAD:MCDLL_NET.dll 会丢
     -> 丢掉的话，生成的 exe 会静态依赖 wpcap.dll（WinPcap/Npcap），
        本机没装，一启动就 0xC0000135。
  2. 调试工作目录默认是工程目录(ImageSeg\)，而模型/数据库在 debug\ 或 release\ 下
     -> 写进 ImageSeg.vcxproj.user，设为 $(OutDir)。

用法:
    python gen_vs.py            # 重新生成 vs 工程
"""

import importlib.util
import re
import subprocess
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PROJECT_GUID = "{7F3C2D8A-9B41-4E7C-A1D2-6F5E8C0B3A91}"
DELAYLOAD = "/DELAYLOAD:MCDLL_NET.dll"


def load_build_module():
    spec = importlib.util.spec_from_file_location("_b", ROOT / "build.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def patch_vcxproj(path: Path):
    s = path.read_text(encoding="utf-8")
    original = s

    def add_delayload(match):
        block = match.group(0)
        if DELAYLOAD in block:
            return block

        def opt(m):
            return m.group(1) + " " + DELAYLOAD + m.group(2)

        if "<AdditionalOptions" in block:
            return re.sub(r"(<AdditionalOptions[^>]*>)(.*?)(</AdditionalOptions>)",
                          lambda m: m.group(1) + m.group(2) + " " + DELAYLOAD + m.group(3),
                          block, flags=re.S)
        # 没有 AdditionalOptions 就插到 </Link> 前
        return block.replace("</Link>", "  <AdditionalOptions>%s %(AdditionalOptions)</AdditionalOptions>\n  </Link>" % DELAYLOAD)

    s = re.sub(r"<Link>.*?</Link>", add_delayload, s, flags=re.S)

    if s != original:
        path.write_text(s, encoding="utf-8")
        print("   patch: Link 增加", DELAYLOAD)
    else:
        print("   patch: 已存在，跳过")


def write_user_file():
    content = """<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="Current" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
    <LocalDebuggerWorkingDirectory>$(OutDir)</LocalDebuggerWorkingDirectory>
    <LocalDebuggerEnvironment>PATH=$(OutDir);%PATH%</LocalDebuggerEnvironment>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
    <LocalDebuggerWorkingDirectory>$(OutDir)</LocalDebuggerWorkingDirectory>
    <LocalDebuggerEnvironment>PATH=$(OutDir);%PATH%</LocalDebuggerEnvironment>
  </PropertyGroup>
</Project>
"""
    (ROOT / "ImageSeg.vcxproj.user").write_text(content, encoding="utf-8")
    print("   写入 ImageSeg.vcxproj.user（调试工作目录 = $(OutDir)）")


def vcxproj_guid(vcx: Path) -> str:
    """sln 里的工程 GUID 必须和 vcxproj 的 ProjectGuid 一致"""
    m = re.search(r"<ProjectGuid>\{?([0-9A-Fa-f-]{36})\}?</ProjectGuid>",
                  vcx.read_text(encoding="utf-8"))
    return "{%s}" % m.group(1).upper() if m else PROJECT_GUID


def write_sln(guid: str):
    sln = f"""
Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
VisualStudioVersion = 17.7.34024.191
MinimumVisualStudioVersion = 10.0.40219.1
Project("{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}") = "ImageSeg", "ImageSeg.vcxproj", "{guid}"
EndProject
Global
\tGlobalSection(SolutionConfigurationPlatforms) = preSolution
\t\tDebug|x64 = Debug|x64
\t\tRelease|x64 = Release|x64
\tEndGlobalSection
\tGlobalSection(ProjectConfigurationPlatforms) = postSolution
\t\t{guid}.Debug|x64.ActiveCfg = Debug|x64
\t\t{guid}.Debug|x64.Build.0 = Debug|x64
\t\t{guid}.Release|x64.ActiveCfg = Release|x64
\t\t{guid}.Release|x64.Build.0 = Release|x64
\tEndGlobalSection
\tGlobalSection(SolutionProperties) = preSolution
\t\tHideSolutionNode = FALSE
\tEndGlobalSection
EndGlobal
"""
    (ROOT / "ImageSeg.sln").write_text(sln.lstrip("\n"), encoding="utf-8")
    print(f"   写入 ImageSeg.sln (project guid {guid})")


def main():
    b = load_build_module()
    b.check_paths()
    env = b.build_env()

    print("[1/3] qmake -tp vc ...")
    rc = subprocess.call([str(b.QT / "bin" / "qmake.exe"), "-tp", "vc",
                          "ImageSeg.pro", "-spec", "win32-msvc"],
                         cwd=str(ROOT), env=env)
    if rc != 0:
        print("[ERROR] qmake -tp vc 失败")
        return rc

    vcx = ROOT / "ImageSeg.vcxproj"
    if not vcx.exists():
        print("[ERROR] 未生成 ImageSeg.vcxproj")
        return 1

    print("[2/3] 修补 vcxproj ...")
    patch_vcxproj(vcx)

    print("[3/3] 生成 sln / user 文件 ...")
    write_user_file()
    write_sln(vcxproj_guid(vcx))

    print("\n[OK] 用 VS2022 打开:", ROOT / "ImageSeg.sln")
    print("     调试前记得先执行: python deploy.py debug   （把 DLL/模型/数据库放到 debug\\）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
