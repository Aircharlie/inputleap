# Windows Client Next-Agent Quickstart

This note is the shortest handoff for the Windows client side of the
keyboard-only switching investigation.

## Current Status

- Date last validated: 2026-04-24
- Result: rebuilt Windows client connected successfully and no sticky
  `Right Win` or sticky `Control` was observed
- Keep the temporary debug logging in place unless there is a strong reason to
  remove it
- Date launcher/Bonjour state last validated: 2026-04-27
- Result: Windows launcher now prefers a custom UDP discovery path, then falls
  back to manual host/IP input in the CLI
- Date directed-broadcast discovery last validated: 2026-04-27
- Result: discovery succeeded after updating the Windows launcher to enumerate
  active IPv4 interfaces, compute each interface's directed broadcast address,
  and probe those addresses before falling back to `255.255.255.255`
- Date Windows input-state cleanup last validated: 2026-04-28
- Result: rebuilt Windows client is running and recent logs show ordinary key
  down/up pairs returning to `mask=2000`; no new stuck `Super` / `Alt` /
  `Control` state was observed after the local client-side rebuild
- Caveat: on this machine, Apple's `Bonjour Service` (`mDNSResponder.exe`
  3.1.0.1) still crashes immediately on startup, so Bonjour is not considered a
  dependable discovery option

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

## Daily Launcher

- desktop shortcut:
  `C:\Users\imocc\Desktop\InputLeap Windows Client.lnk`
- launcher script:
  `C:\Users\imocc\Desktop\InputLeap Windows Client Launcher.ps1`

Current launcher behavior:

1. enumerate active IPv4 interfaces on Windows
2. compute each directed broadcast address from `ip + subnet mask`
3. send `INPUTLEAP_DISCOVER_V1` to each directed broadcast on UDP `24801`
4. send the same probe to `255.255.255.255` as a fallback
5. if discovery succeeds, launch the rebuilt client against the replying host
6. if discovery fails, prompt for manual host/IP input in the CLI

The matching server-side responder now lives in:

- `src/lib/inputleap/ServerDiscoveryResponder.h`
- `src/lib/inputleap/ServerDiscoveryResponder.cpp`

Protocol shape:

- request payload: `INPUTLEAP_DISCOVER_V1`
- reply payload: `INPUTLEAP_SERVER_V1<TAB><screen-name><TAB><tcp-port>`

Known practical finding:

- in this network, discovery worked against the directed broadcast targets
  such as `192.168.71.255`, but not when relying on `255.255.255.255` alone

If UDP discovery does not find anything, check whether the server build being
run actually includes the new responder path before spending more time on local
Windows networking.

## 2026-04-28 Input-State Handoff

Observed Windows symptom:

- keyboard looked recovered, but local Windows mouse clicks still behaved
  strangely while InputLeap was running
- the client log showed `LWin` and `LAlt` key-down events without matching
  releases around the hotkey path
- this can make normal mouse clicks behave like `Win`/`Alt` modified clicks, so
  the mouse symptom is probably another expression of the same stuck modifier
  state rather than a mouse hardware issue

Windows-side changes already built:

- `src/lib/inputleap/Screen.cpp`: secondary-screen `disable()` now calls
  `fakeAllKeysUp()` even when the client was not marked as fully entered
- `src/lib/server/Server.cpp`: keyboard-target cleanup now sends key-up for all
  tracked forwarded keys instead of filtering by the primary's current physical
  key poll
- `src/lib/server/Server.cpp`: `switchKeyboardScreen()` and
  `followMouseForKeyboard()` now release forwarded keys/modifiers even when the
  requested target is already current

Important mac-side action:

- rebuild and restart the macOS server before judging the hotkey no-op fix,
  because the `Server.cpp` cleanup path runs on the server side
- Windows client-side rebuild is already running from
  `build-win-debug\bin\input-leapc.exe`, but the complete fix needs the mac
  server binary to include the matching `Server.cpp` changes

Validation performed on Windows after rebuild:

- stopped the old `input-leapc.exe` because it locked the executable during link
- rebuilt `input-leapc.exe` successfully in `build-win-debug`
- relaunched via `C:\Users\imocc\Desktop\InputLeap Windows Client Launcher.ps1`
- UDP discovery found server `192.168.71.185`
- current Windows process was `input-leapc.exe` from the isolated debug build
- recent log tail showed regular key down/up pairs and no new stuck modifier
  sequence

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
