# Tally native-only Windows build

**Requirements:** Windows 10/11 x64 and Python 3. The build script downloads
Zig 0.13.0 and NSIS 3.13 if they are missing. Node.js and `build.cjs` are **not required**.

From the repository root:

```powershell
python Windows\native\build-windows.py
```

Expected repository layout:

```text
Tally/
  Windows/
    native/                 # sources, setup.nsi, build-windows.py, VERSION, assets/
  examples/               # Shared .tally programs at GitHub repo root
  Windows/WINDOWS-START-HERE.txt  # fallback installed guide
  Windows/THIRD-PARTY.txt         # fallback attribution notices
  README.md               # optional for build, recommended for GitHub
```

Generated installer: `Windows/dist/Tally-Setup-<version>-x64.exe`.
The `build/` and `dist/*.exe` outputs are generated; do not commit them.

To release a newer version, edit `Windows/native/VERSION` (for example, `0.5.0`)
and build again. The installer supports repair, update, uninstall and refuses
an accidental downgrade. Keep your `.tally` files outside the installation
folder. Existing examples in the install folder are not overwritten on repair.

The installer bundles only the native runner, its drawing companion,
PATH helper, examples, getting-started text and third-party notices.
It does not bundle a browser-based editor or any JavaScript runtime.

`native/unicode.hpp` is already checked in, so `generate-unicode.cjs`
is not required for normal builds.

Build status: Python syntax and packaging statically checked; final Windows
compilation and installation should be tested on a Windows PC.
