"""Build the native-only Tally Windows x64 installer.

Run: python native/build-windows.py

Automatically downloads missing Windows x64 build tools into build/tools:
  - Zig 0.13.0 (the version this project requires)
  - NSIS 3.13 (only if makensis isn't installed)

Does not require administrator privileges or change system PATH.
Explicit ZIG and MAKENSIS environment overrides are respected.
The console pauses at the end so you can read any errors.
"""

from __future__ import annotations

import hashlib
import json
import re
import os
import shutil
import subprocess
import sys
import traceback
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent  # <repository>/Windows
REPO = ROOT.parent  # Shared examples and documentation live at repository root.
os.chdir(ROOT)
OUT = ROOT / "build" / "windows"
TOOLS = ROOT / "build" / "tools"
OUT.mkdir(parents=True, exist_ok=True)
TOOLS.mkdir(parents=True, exist_ok=True)

# Official version-pinned Zig download (verified SHA-256).
ZIG_URL = "https://ziglang.org/download/0.13.0/zig-windows-x86_64-0.13.0.zip"
ZIG_SHA256 = "d859994725ef9402381e557c60bb57497215682e355204d754ee3df75ee3c158"
NSIS_URL = "https://downloads.sourceforge.net/project/nsis/NSIS%203/3.13/nsis-3.13.zip"


def log(message: str) -> None:
    print(message, flush=True)


def run(*args: object) -> None:
    command = [str(arg) for arg in args]
    log("\n" + "=" * 65)
    log("RUNNING: " + subprocess.list2cmdline(command))
    log("=" * 65)
    subprocess.run(command, check=True)


def download(url: str, destination: Path, expected_sha256: str | None = None) -> None:
    if destination.is_file():
        if not expected_sha256 or sha256(destination) == expected_sha256:
            log("Using cached " + destination.name)
            return
        log("Cached archive checksum mismatch; re-downloading.")
        destination.unlink()

    log("Downloading " + url)
    request = urllib.request.Request(
        url, headers={"User-Agent": "Mozilla/5.0 (compatible; TallyBuild/1.0)"}
    )
    partial = destination.with_suffix(destination.suffix + ".partial")
    partial.unlink(missing_ok=True)
    try:
        with urllib.request.urlopen(request, timeout=60) as response, partial.open("wb") as output:
            checksum = hashlib.sha256()
            count = 0
            while True:
                block = response.read(1024 * 1024)
                if not block:
                    break
                output.write(block)
                checksum.update(block)
                count += len(block)
                if count // (10 * 1024 * 1024) != (count - len(block)) // (10 * 1024 * 1024):
                    log(f"  Received {count / (1024 * 1024):.0f} MB")
        if expected_sha256 and checksum.hexdigest() != expected_sha256:
            raise RuntimeError(
                "Downloaded archive SHA-256 mismatch! The file will not be extracted."
            )
        if not zipfile.is_zipfile(partial):
            raise RuntimeError(
                "The download was not a ZIP archive (possibly a blocked download page)."
            )
        partial.replace(destination)
    except Exception:
        partial.unlink(missing_ok=True)
        raise


def sha256(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def portable_tool(
    name: str, url: str, expected_sha256: str | None, exe_name: str
) -> str:
    folder = TOOLS / name
    target = next(folder.rglob(exe_name), None) if folder.exists() else None
    if target is not None and target.is_file():
        log(f"Using cached {name}: {target}")
        return str(target)

    folder.mkdir(parents=True, exist_ok=True)
    archive = TOOLS / f"{name}.zip"
    download(url, archive, expected_sha256)
    log(f"Extracting {name}...")
    with zipfile.ZipFile(archive) as z:
        # zipfile.extractall sanitizes Windows absolute paths and traversal paths.
        z.extractall(folder)
    target = next(folder.rglob(exe_name), None)
    if target is None or not target.is_file():
        raise RuntimeError(f"Cannot find {exe_name} in the {name} archive")
    log(f"Ready: {target}")
    return str(target)


def get_zig() -> str:
    override = os.environ.get("ZIG")
    if override:
        log(f"Using ZIG override: {override}")
        return override

    existing = shutil.which("zig")
    if existing:
        try:
            version = subprocess.check_output([existing, "version"], text=True).strip()
            if version == "0.13.0":
                log("Found Zig 0.13.0 on PATH")
                return existing
            log(f"Found Zig {version}, but Tally requires 0.13.0; downloading that version.")
        except (OSError, subprocess.CalledProcessError):
            log("Zig on PATH could not be checked; using a local 0.13.0 copy.")
    return portable_tool("zig-0.13.0", ZIG_URL, ZIG_SHA256, "zig.exe")


def get_nsis() -> str:
    override = os.environ.get("MAKENSIS")
    if override:
        log(f"Using MAKENSIS override: {override}")
        return override

    existing = shutil.which("makensis") or shutil.which("makensis.exe")
    candidates = [
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "NSIS" / "makensis.exe",
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "NSIS" / "Bin" / "makensis.exe",
        Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "NSIS" / "makensis.exe",
        Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "NSIS" / "Bin" / "makensis.exe",
        Path(os.environ.get("LOCALAPPDATA", str(TOOLS))) / "Programs" / "NSIS" / "makensis.exe",
    ]
    if existing:
        log(f"Using installed NSIS: {existing}")
        return existing
    for candidate in candidates:
        if candidate.is_file():
            log(f"Using installed NSIS: {candidate}")
            return str(candidate)

    log("NSIS was not found. Installing a portable copy for this build...")
    try:
        return portable_tool("nsis-3.13", NSIS_URL, None, "makensis.exe")
    except Exception as download_error:
        # SourceForge mirrors can occasionally block automated ZIP downloads.
        winget = shutil.which("winget")
        if not winget:
            raise RuntimeError(
                "Could not download portable NSIS and winget is not installed. "
                "Download NSIS manually from https://nsis.sourceforge.io/Download"
            ) from download_error
        log(f"Portable NSIS download failed: {download_error}")
        log("Trying Windows Package Manager (winget) instead...")
        run(
            winget, "install", "--id", "NSIS.NSIS", "-e", "--source", "winget",
            "--accept-source-agreements", "--accept-package-agreements", "--silent",
        )
        for candidate in candidates:
            if candidate.is_file():
                return str(candidate)
        raise RuntimeError(
            "winget completed, but makensis.exe was not found. "
            "Open a new terminal and try again, or set MAKENSIS to its full path."
        )



def check_build_artifact(path: Path) -> None:
    """Fail at the actual problem, not later inside the installer."""
    if path.is_file() and path.stat().st_size > 0:
        log(f"Created {path.name}: {path.stat().st_size:,} bytes")
        return
    log(f"\nERROR: compiler returned successfully but did not create {path}")
    log("Files currently in build/windows:")
    for item in sorted(OUT.iterdir()):
        log(f"  {item.name} ({item.stat().st_size:,} bytes)" if item.is_file() else f"  {item.name}/")
    raise FileNotFoundError(
        f"Expected compiled output not found: {path}\n"
        "If the compiler reported success, check Windows Security > Protection history "
        "for a quarantine, then inspect the Zig output directory. "
        "Do not disable antivirus protections.\n"
    )


def get_release_version() -> tuple[str, str]:
    """Use native/VERSION as the source of truth (no Node/package.json needed).

    For older repositories without a VERSION file, package.json is accepted
    as a fallback so the source tree remains backwards compatible.
    """
    version_file = ROOT / "native" / "VERSION"
    if version_file.is_file():
        version = version_file.read_text(encoding="utf-8-sig").strip()
        log(f"Installer version from native/VERSION: {version}")
    elif (ROOT / "package.json").is_file():
        metadata = json.loads((ROOT / "package.json").read_text(encoding="utf-8"))
        version = metadata["version"]
        log(f"Installer version from package.json: {version} (consider adding native/VERSION)")
    else:
        raise FileNotFoundError(
            "Version is missing: create native/VERSION containing 0.4.1 "
            "(or another three-part version)."
        )
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:\.[0-9]+)?", version):
        raise ValueError("native/VERSION must look like 0.4.1 or 0.5.0.0")
    parts = [int(p) for p in version.split(".")]
    if any(p > 65535 for p in parts):
        raise ValueError("Each version component must be 0..65535")
    return version, ".".join(str(p) for p in (parts + [0] * (4 - len(parts))))


def build() -> None:
    if os.name != "nt":
        raise RuntimeError("This build script requires Windows x64.")

    version, file_version = get_release_version()
    log(f"Tally {version} — automatic Windows build")
    zig = get_zig()
    nsis = get_nsis()

    # Remove stale products so a previous build cannot mask a new compilation failure.
    for name in ("tally.exe", "tally-draw.exe", "tally-userpath.exe"):
        (OUT / name).unlink(missing_ok=True)

    run(zig, "rc", "/fo", OUT / "tally.res", "native/tally.rc")
    check_build_artifact(OUT / "tally.res")

    common = ["-target", "x86_64-windows-gnu", "-mcpu=baseline", "-O3", "-s", "-municode"]

    run(
        zig, "c++", *common, "-std=c++17", "-Wno-deprecated-declarations",
        "-Wl,--stack,67108864", "native/engine.cpp", OUT / "tally.res",
        "-luser32", "-o", OUT / "tally.exe",
    )
    check_build_artifact(OUT / "tally.exe")
    run(
        zig, "c++", *common, "-std=c++17", "-Wl,--subsystem,windows",
        "native/drawing.cpp", OUT / "tally.res", "-lgdi32", "-luser32",
        "-lshell32", "-o", OUT / "tally-draw.exe",
    )
    check_build_artifact(OUT / "tally-draw.exe")
    run(
        zig, "cc", *common, "native/userpath.c", "-ladvapi32", "-luser32",
        "-o", OUT / "tally-userpath.exe",
    )
    check_build_artifact(OUT / "tally-userpath.exe")

    # Build a native-only payload: no browser editor, Node runtime, or stale
    # files from earlier builds can accidentally get packaged.
    shutil.rmtree(OUT / "dist", ignore_errors=True)
    shutil.rmtree(OUT / "runtime", ignore_errors=True)
    shutil.rmtree(OUT / "examples", ignore_errors=True)
    for name in ("WINDOWS-START-HERE.txt", "THIRD-PARTY.txt"):
        source = REPO / name
        if not source.is_file():
            source = ROOT / name  # Fallback bundled with the Windows folder.
        if not source.is_file():
            raise FileNotFoundError(
                f"Required installer document is missing: {source}. "
                "Put it in the GitHub repository root or Windows folder."
            )
        shutil.copy2(source, OUT / name)
    # Examples belong to the shared GitHub repository, not the native installer.
    # Intentionally do not look for, require, or bundle an examples/ directory.
    # Above, stale examples from earlier build payloads are removed.

    # README is useful on GitHub but not a requirement for building the installer.
    readme = REPO / "README.md"
    if not readme.is_file():
        readme = ROOT / "README.md"
    if readme.is_file():
        shutil.copy2(readme, OUT / "README.md")

    installer = ROOT / "dist" / f"Tally-Setup-{version}-x64.exe"
    installer.parent.mkdir(parents=True, exist_ok=True)

    # Pass the native artwork location as an absolute Windows path.
    # This fixes NSIS trying native\native\assets\tally.ico.
    asset_dir = (ROOT / "native" / "assets").resolve()
    for asset in ("tally.ico", "tally-wizard.bmp", "tally-header.bmp"):
        asset_file = asset_dir / asset
        if not asset_file.is_file():
            raise FileNotFoundError(
                f"Missing installer artwork: {asset_file}. "
                "Extract the full patch ZIP to the Tally-0.4.1 folder, "
                "keeping the native/assets subfolder."
            )
    run(
        nsis, "-V2", f"-DPAYLOAD={OUT}", f"-DOUTPUT={installer}",
        f"-DTALLY_ASSET_DIR={asset_dir}",
        f"-DTALLY_VERSION={version}",
        f"-DTALLY_FILE_VERSION={file_version}",
        "native/setup.nsi",
    )
    log("\n" + "=" * 65)
    log("BUILD SUCCESSFUL!")
    log(f"Installer: {installer}")
    log("=" * 65)


if __name__ == "__main__":
    try:
        build()
    except Exception:
        log("\n" + "=" * 65)
        log("BUILD FAILED — error details:")
        traceback.print_exc()
        log("=" * 65)
        sys.exit_code = 1
    else:
        sys.exit_code = 0
    finally:
        if sys.stdin.isatty():
            try:
                input("\nPress ENTER to close this window...")
            except EOFError:
                pass
    sys.exit(sys.exit_code)
