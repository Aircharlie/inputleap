# Windows Client Next-Agent Quickstart

This note is the shortest handoff for the Windows client side of the
keyboard-only switching investigation.

## Current Status

- Date last validated: 2026-04-24
- Result: rebuilt Windows client connected successfully and no sticky
  `Right Win` or sticky `Control` was observed
- Keep the temporary debug logging in place unless there is a strong reason to
  remove it

## Use These Paths

- repo:
  `C:\Users\imocc\Code\InputLeap\InputLeap`
- isolated Windows build dir:
  `C:\Users\imocc\Code\InputLeap\InputLeap\build-win-debug`
- rebuilt client:
  `C:\Users\imocc\Code\InputLeap\InputLeap\build-win-debug\bin\input-leapc.exe`
- isolated Windows dependency dir:
  `C:\Users\imocc\Code\InputLeap\vcpkg-win`
- longer handoff doc:
  `C:\Users\imocc\Code\InputLeap\InputLeap\doc\windows-client-super-stuck-debug-plan.md`

Do not validate with the stock installed client at
`C:\Program Files\InputLeap\input-leapc.exe` unless it has been replaced by the
rebuilt binary.

## Build Command

Use the existing isolated build directory unless there is a reason to reconfigure.

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && "C:\Program Files\CMake\bin\cmake.exe" --build "C:\Users\imocc\Code\InputLeap\InputLeap\build-win-debug" --target input-leapc --parallel'
```

If a clean reconfigure is needed, reuse the same isolated directories and
vcpkg toolchain:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && "C:\Program Files\CMake\bin\cmake.exe" -S "C:\Users\imocc\Code\InputLeap\InputLeap" -B "C:\Users\imocc\Code\InputLeap\InputLeap\build-win-debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DINPUTLEAP_BUILD_GUI=OFF -DINPUTLEAP_BUILD_TESTS=OFF -DCMAKE_RC_COMPILER:FILEPATH="C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\rc.exe" -DCMAKE_MT:FILEPATH="C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\mt.exe" -DCMAKE_TOOLCHAIN_FILE="C:\Users\imocc\Code\InputLeap\vcpkg-win\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static'
```

## Run Command

```powershell
& "C:\Users\imocc\Code\InputLeap\InputLeap\build-win-debug\bin\input-leapc.exe" `
  --no-daemon `
  --no-restart `
  --disable-crypto `
  --debug DEBUG1 `
  --log "C:\Temp\input-leap-client-debug.log" `
  --name "CC-Workstation" `
  192.168.71.173
```

If you get `Connection was refused`, that is a server reachability problem, not
evidence that the rebuilt Windows client is broken.

## Relevant Instrumentation

Temporary logs were added to:

- `src/lib/inputleap/KeyState.cpp`
- `src/lib/platform/MSWindowsKeyState.cpp`

These logs are intended to answer:

- whether `fakeKeyUp(...)` found the correct `serverID -> localID` mapping
- whether direct or fallback key-up handling was used
- whether `m_keys`, `m_syntheticKeys`, and `m_mask` were actually cleared
- whether `pollActiveModifiers()` rebuilt `Control` or `Super` from residual
  Windows shadow state

## If The Bug Reappears

1. Reproduce using the rebuilt client from `build-win-debug`.
2. Check `C:\Temp\input-leap-client-debug.log`.
3. Inspect `fakeKeyUp(...)` and `pollActiveModifiers()` lines first.
4. Compare whether normal typing starts from `start state: 2010`, `2002`, or
   another modifier-bearing state.
5. Read the longer handoff doc before changing code.
