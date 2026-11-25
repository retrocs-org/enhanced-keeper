#!/usr/bin/env python3
"""Build a per-user MSI with an embedded cabinet and a Start menu shortcut."""
import argparse
import hashlib
import shutil
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory
from uuid import UUID, uuid5
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
UPGRADE_ID = UUID("2f37e3be-0595-4888-bd7b-78480fdbe74e")
WIX_NAMESPACE = "http://schemas.microsoft.com/wix/2006/wi"


def identity(prefix, relative_path):
    return prefix + hashlib.sha256(relative_path.lower().encode()).hexdigest()[:32]


def add_payload(folder, element, payload, feature):
    for path in sorted(folder.iterdir()):
        relative = path.relative_to(payload).as_posix()
        if path.is_dir():
            directory = ET.SubElement(element, "Directory", Id=identity("D", relative), Name=path.name)
            add_payload(path, directory, payload, feature)
            continue
        component_id = identity("C", relative)
        component = ET.SubElement(
            element, "Component", Id=component_id, Guid=str(uuid5(UPGRADE_ID, relative.lower()))
        )
        file = ET.SubElement(
            component, "File", Id=identity("F", relative),
            Source="payload/" + relative, KeyPath="yes",
        )
        if relative == "keeper-enhanced.exe":
            ET.SubElement(
                file, "Shortcut", Id="GameShortcut", Directory="ProgramMenuFolder",
                Name="Keeper Enhanced", WorkingDirectory="INSTALLDIR", Advertise="yes",
            )
        ET.SubElement(feature, "ComponentRef", Id=component_id)


def build_msi(build, output_dir):
    version = (ROOT / "VERSION").read_text().strip()
    output = output_dir / f"Keeper-Enhanced-{version}-Windows-x86.msi"
    output_dir.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise SystemExit(f"MSI already exists at {output}")

    with TemporaryDirectory(prefix="kfx-msi-") as directory:
        stage = Path(directory)
        payload = stage / "payload"
        shutil.copytree(ROOT / "assets/runtime", payload, ignore=shutil.ignore_patterns(".DS_Store"))
        shutil.copy2(ROOT / "macos/packaging/keeper-title.png", payload)
        shutil.copy2(ROOT / "public-web/assets/avatars/presets.png", payload / "avatar-presets.png")

        executable = build / "keeper-enhanced.exe"
        shutil.copy2(executable, payload)
        for name in ("SDL3", "SDL3_image", "SDL3_mixer"):
            candidates = list((build / "deps").glob(f"*/{name}-*/i686-w64-mingw32/bin/{name}.dll"))
            if len(candidates) != 1:
                raise SystemExit(f"Expected one i686 SDK for {name}, found {len(candidates)}")
            dll = candidates[0]
            shutil.copy2(dll, payload)
            shutil.copy2(dll.parents[2] / "LICENSE.txt", payload / "licenses" / f"{name}.txt")
        for dll in build.glob("*.dll"):
            shutil.copy2(dll, payload)
        for name in ("COPYING.LGPLv2.1", "LICENSE.md"):
            license_file = build / "deps/ffmpeg-work/ffmpeg-9.0.1" / name
            shutil.copy2(license_file, payload / "licenses" / f"FFmpeg-{name}")
        shutil.copy2(build / "deps/openal-prefix/COPYING", payload / "licenses/OpenAL-Soft.txt")
        imports = subprocess.check_output(["i686-w64-mingw32-objdump", "-p", executable], text=True)
        if "DLL Name: libwinpthread-1.dll" in imports:
            import_library = Path(subprocess.check_output(
                ["i686-w64-mingw32-g++", "-print-file-name=libwinpthread.dll.a"], text=True
            ).strip()).resolve()
            pthread = import_library.parent.parent / "bin/libwinpthread-1.dll"
            shutil.copy2(pthread, payload)
        shutil.copy2(build / "keeperfx_icon.ico", stage / "keeperfx_icon.ico")

        wix = ET.Element("Wix", xmlns=WIX_NAMESPACE)
        product = ET.SubElement(
            wix, "Product", Id="*", UpgradeCode=str(UPGRADE_ID), Name="Keeper Enhanced",
            Version=version, Language="1033", Manufacturer="Keeper Enhanced contributors",
        )
        ET.SubElement(product, "Package", InstallerVersion="200", Compressed="yes", InstallScope="perUser")
        ET.SubElement(product, "Media", Id="1", Cabinet="keeper.cab", EmbedCab="yes")
        ET.SubElement(product, "Icon", Id="GameIcon", SourceFile="keeperfx_icon.ico")
        ET.SubElement(product, "Property", Id="ARPPRODUCTICON", Value="GameIcon")
        upgrade = ET.SubElement(product, "Upgrade", Id=str(UPGRADE_ID))
        ET.SubElement(upgrade, "UpgradeVersion", Minimum="0.0.0", IncludeMinimum="yes",
                      Maximum=version, IncludeMaximum="yes", Property="KFX_OLD_VERSIONS")
        sequence = ET.SubElement(product, "InstallExecuteSequence")
        ET.SubElement(sequence, "RemoveExistingProducts", After="InstallInitialize")

        root = ET.SubElement(product, "Directory", Id="TARGETDIR", Name="SourceDir")
        ET.SubElement(root, "Directory", Id="ProgramMenuFolder")
        app_data = ET.SubElement(root, "Directory", Id="LocalAppDataFolder")
        install = ET.SubElement(app_data, "Directory", Id="INSTALLDIR", Name="Keeper Enhanced")
        feature = ET.SubElement(product, "Feature", Id="Game", Title="Keeper Enhanced", Level="1")
        add_payload(payload, install, payload, feature)
        ET.ElementTree(wix).write(stage / "keeper.wxs", encoding="utf-8", xml_declaration=True)
        subprocess.run(["wixl", "-a", "x86", "-o", "keeper.msi", "keeper.wxs"], cwd=stage, check=True)
        shutil.copy2(stage / "keeper.msi", output)
    print(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build_msi(args.build.resolve(), args.output.resolve())
