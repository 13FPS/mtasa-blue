"""Build the standalone C++17 retail movement parser test and run it against a temporary profile."""
import codecs
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    source = Path(__file__).resolve().with_name("RetailMovementBindingsTests.cpp")
    with tempfile.TemporaryDirectory(prefix="mta-retail-bindings-") as directory:
        work = Path(directory)
        if os.name == "nt":
            locator = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
            install = subprocess.check_output(
                [locator, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                 "-property", "installationPath"], text=True).strip()
            if not install:
                raise RuntimeError("Visual Studio C++ tools are required")
            env = os.environ.copy()
            env.update(MTA_TEST_SOURCE=str(source), MTA_TEST_VSDEV=str(Path(install) / "Common7/Tools/VsDevCmd.bat"))
            for option in ("CL", "_CL_"):
                env.pop(option, None)
            (work / "build.cmd").write_text(
                '@echo off\ncall "%MTA_TEST_VSDEV%" -no_logo -arch=x64 -host_arch=x64\n'
                'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc /W4 /WX "%MTA_TEST_SOURCE%" /Fe:retail-test.exe\n')
            subprocess.run([env["COMSPEC"], "/d", "/c", "build.cmd"], cwd=work, env=env, check=True)
            exe = work / "retail-test.exe"
        else:
            exe = work / "retail-test"
            subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", source, "-o", exe], cwd=work, check=True)
        # Use an independent encoder for the same BOM formats the profile exporter preserves.
        profile = "49 HID_MAPPING_PED_MOVE_Y 1 -1\r\n# véhicule 🚗\r\n54 HID_MAPPING_SPRINT 0 -1\r\n"
        for encoding, bom in (("utf-8", codecs.BOM_UTF8), ("utf-16-le", codecs.BOM_UTF16_LE),
                              ("utf-16-be", codecs.BOM_UTF16_BE), ("utf-32-le", codecs.BOM_UTF32_LE),
                              ("utf-32-be", codecs.BOM_UTF32_BE)):
            (work / (encoding + ".cfg")).write_bytes(bom + profile.encode(encoding))
        valid16 = codecs.BOM_UTF16_LE + profile.encode("utf-16-le")
        valid32 = codecs.BOM_UTF32_BE + profile.encode("utf-32-be")
        invalid = {
            "short-unit": valid16 + b"\x00",
            "embedded-nul": valid16 + b"\x00\x00",
            "unpaired-high": valid16 + b"\x00\xd8",
            "unpaired-low": valid16 + b"\x00\xdc",
            "missing-low": valid16 + b"\x00\xd8A\x00",
            "invalid-scalar": valid32 + bytes.fromhex("00110000"),
            "surrogate-scalar": valid32 + bytes.fromhex("0000d800"),
        }
        for name, data in invalid.items():
            (work / (name + ".cfg")).write_bytes(data)
        subprocess.run([exe, work], cwd=work, check=True)
    print("Retail movement binding parser and read-only file tests passed")


if __name__ == "__main__":
    main()
