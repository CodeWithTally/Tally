# Tally for Windows (x64)

Native, terminal-first Tally 0.4.1 source and installer builder.

## Build the Windows installer

On Windows 10/11 x64 with Python 3, run **from the repository root**:

```powershell
python Windows\native\build-windows.py
```

The script automatically finds or downloads Zig 0.13.0 and NSIS 3.13 as needed.

The installer is written to:

```text
Windows/dist/Tally-Setup-0.4.1-x64.exe
```

The installer includes the native runner, native drawing helper, per-user PATH
helper, `.tally` associations, examples, and first-run guide. It also has
Repair / Update / Uninstall and version checks. Update `Windows/native/VERSION`
for future releases. Test the installer on Windows before publishing.
