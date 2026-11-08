# Tally for Windows (x64)

Native, terminal-first Tally 0.4.1 source and installer builder. This folder
is intended to live at the **root of the Tally GitHub repository** alongside
`macOS/` and the **shared** `examples/` folder:

```text
Tally/
  examples/                 # Existing shared example programs (do not duplicate)
  README.md                 # Project readme (optional for the build)
  Windows/
    native/
      build-windows.py
      setup.nsi
      VERSION
      engine.cpp
      drawing.cpp
      ...
    WINDOWS-START-HERE.txt  # Fallback guide if absent at repository root
    THIRD-PARTY.txt         # Fallback legal notices
  macOS/
```

## Build the Windows installer

On Windows 10/11 x64 with Python 3, run **from the repository root**:

```powershell
python Windows\native\build-windows.py
```

The script automatically finds or downloads Zig 0.13.0 and NSIS 3.13 as needed.
It **does not need Node.js, the web editor, or `build.cjs`**.

The installer is written to:

```text
Windows/dist/Tally-Setup-0.4.1-x64.exe
```

The `.exe` and `Windows/build/` toolchain cache are generated locally and should
be published through **GitHub Releases**, not committed as source files.

The installer includes the native runner, native drawing helper, per-user PATH
helper, `.tally` associations, examples, and first-run guide. It also has
Repair / Update / Uninstall and version checks. Update `Windows/native/VERSION`
for future releases. Test the installer on Windows before publishing.

## Using existing examples and docs

The build reads `examples/` from the **repository root**. The Windows setup
uses root `WINDOWS-START-HERE.txt` and `THIRD-PARTY.txt` when present, otherwise
uses the fallback copies in this folder. No need to reupload examples in this
ZIP if you already have them in GitHub.

## License

Keep the project LICENSE in the GitHub repository root and ensure third-party
notices remain available. This folder does not add a new license.
