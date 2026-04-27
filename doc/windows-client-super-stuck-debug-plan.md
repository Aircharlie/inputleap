# Windows Client Debug Plan for Stuck Right Win

This document is the handoff plan for debugging the Windows client side of the
custom keyboard-only switching patch.

## Latest Status

As of 2026-04-24 on the Windows machine:

- a fresh Windows-only debug client was built from this fork
- the rebuilt client was run from an isolated Windows build directory
- the client successfully connected to the macOS server
- keyboard input worked normally
- `Right Win` was no longer sticky
- `Control` was also no longer observed to stick during the validation run

This means the current Windows debug build is now a good baseline for any
follow-up investigation. Keep the temporary logging in place unless it becomes
too noisy.

As of 2026-04-27, Bonjour is no longer the preferred auto-discovery direction
for this Windows machine. The next path is a lightweight UDP discovery
responder built into `input-leaps`.

## Post-Bonjour Discovery Direction

Server-side discovery now lives in:

- `src/lib/inputleap/ServerDiscoveryResponder.h`
- `src/lib/inputleap/ServerDiscoveryResponder.cpp`

Current protocol:

- server listens on UDP `24801`
- client/launcher sends `INPUTLEAP_DISCOVER_V1`
- server replies `INPUTLEAP_SERVER_V1<TAB><screen-name><TAB><tcp-port>`

The goal is to let Windows discover the macOS InputLeap server without relying
on Apple's broken Windows Bonjour stack.

## Symptom

In the custom `keyboard-only` mode:

- the macOS server switches keyboard routing to the Windows client correctly
- the Windows machine receives keyboard input
- but `Right Win` behaves as if it is permanently held down

This does not reproduce in the original InputLeap behavior where keyboard and
mouse always move together.

## Current Conclusion

The bug is now believed to be on the Windows client side, not the macOS server
side.

The macOS server is already sending release events for `Super_L` and
`Super_R`, but the Windows client still keeps `KeyModifierSuper` in its local
modifier shadow state.

That causes later normal key presses to be synthesized as:

```txt
LWin up
RWin up
<letter> down
LWin down
RWin down
```

which matches the observed "Right Win is stuck" behavior.

## Verified Evidence

### Windows client startup state

Observed in `DEBUG1` logs:

```txt
modifiers on update: 0x2010
```

Where:

- `0x0010` = `KeyModifierSuper`
- `0x2000` = `KeyModifierNumLock`

So the Windows client already believes `Super` is active in its local shadow
state.

### Release events are arriving from the server

Observed in Windows client logs:

```txt
recv key up id=0x0000efeb, mask=0x0000, button=0x0038
recv key up id=0x0000efec, mask=0x0000, button=0x0037
```

These are `Super_L` and `Super_R`.

### The client still restores Win during normal typing

Observed immediately before ordinary character input:

```txt
mapKey 0077 (119) with mask 0000, start state: 2010
keystrokes:
  15b (0000005b) up
  15c (0000005c) up
  011 (00000057) down
  15b (0000005b) down
  15c (0000005c) down
```

This proves the server is not repeatedly sending Win down. The Windows client
itself believes Win should remain active and restores it while mapping normal
keys.

## Why Original InputLeap Did Not Show This

Original InputLeap keeps keyboard and mouse bound to the same active screen.
That means the Windows client only receives keyboard input when the full screen
focus has entered it through the normal `enter()/leave()` lifecycle.

The custom patch creates a new mode:

- mouse remains on macOS
- keyboard alone is routed to Windows
- no full screen enter/leave occurs on the Windows side

This exposes a Windows client assumption that modifier state will be naturally
normalized by the traditional screen-switch lifecycle.

## Code Areas to Inspect

Prioritize these files on the Windows build:

- `src/lib/platform/MSWindowsKeyState.cpp`
- `src/lib/inputleap/KeyState.cpp`
- `src/lib/inputleap/Screen.cpp`
- `src/lib/inputleap/KeyMap.cpp`

## Local Build Target

Only the Windows client needs to be rebuilt first.

Primary target:

- `input-leapc.exe`

Do not validate using the stock installed binary in
`C:\Program Files\InputLeap\input-leapc.exe` unless it has been replaced by the
freshly built client from this fork.

Use the isolated Windows build output instead:

- build dir: `C:\Users\imocc\Code\InputLeap\InputLeap\build-win-debug`
- client exe:
  `C:\Users\imocc\Code\InputLeap\InputLeap\build-win-debug\bin\input-leapc.exe`

Windows-only dependency/tooling used for this build:

- isolated vcpkg dir: `C:\Users\imocc\Code\InputLeap\vcpkg-win`
- OpenSSL triplet: `x64-windows-static`

Do not reuse the repository's generic `build/` directory for Windows debug
validation. Keep Windows artifacts in `build-win-debug` so they stay isolated
from any macOS-oriented outputs or prior mixed builds.

## Current Instrumentation

Temporary debug logging has already been added to:

- `src/lib/inputleap/KeyState.cpp`
- `src/lib/platform/MSWindowsKeyState.cpp`

The added logs print:

- `serverID -> localID` resolution in `fakeKeyUp(...)`
- whether the direct or fallback release path was used
- `m_keys[localID]` and `m_syntheticKeys[localID]` after decrement
- `m_mask` before and after release handling
- whether `Control` / `Super` remain active
- which Win32 modifier buttons contribute to `pollActiveModifiers()`

Keep these logs for later agents unless there is a clear reason to trim them.

## Reproduction Command

Run the rebuilt client in foreground with debug logging:

```powershell
& "C:\\Users\\imocc\\Code\\InputLeap\\InputLeap\\build-win-debug\\bin\\input-leapc.exe" `
  --no-daemon `
  --no-restart `
  --disable-crypto `
  --debug DEBUG1 `
  --log "C:\\Temp\\input-leap-client-debug.log" `
  --name "CC-Workstation" `
  192.168.71.173
```

## Investigation Goals

Trace the full path:

```txt
recv key up
-> Screen::keyUp(...)
-> KeyState::fakeKeyUp(...)
-> MSWindowsKeyState shadow modifier state
-> next normal key mapKey(...) start state
```

The main question is:

Why does `start state` remain `0x2010` after `Super_L` / `Super_R` key-up
events have already been received?

## Specific Things to Check

### 1. `KeyState::fakeKeyUp(KeyButton)`

Verify whether:

- `m_serverKeys[button]` resolves correctly
- `m_keys[localID]` is decremented
- `m_syntheticKeys[localID]` is decremented
- the corresponding `Super` entries are removed from `m_activeModifiers`
- `m_mask` actually loses `KeyModifierSuper`

### 2. `KeyState::fakeKeyUp(KeyID, KeyButton)`

This fallback was added because `button`-only release can fail if the client no
longer has a matching `m_serverKeys[button]` mapping.

Verify whether this fallback:

- is actually being exercised
- finds the correct `localID`
- clears `m_activeModifiers`
- clears `m_mask`
- generates a real fake key-up event on Windows

### 3. `MSWindowsKeyState::pollActiveModifiers()`

This function rebuilds modifier state from the client-side shadow key state.

Check whether `KeyModifierSuper` remains present because:

- `isKeyDown(button)` is still true for `VK_LWIN` or `VK_RWIN`
- `m_keys[]` or `m_syntheticKeys[]` were never really cleared
- some path reintroduces Win as active after release

### 4. `KeyMap` normal-letter mapping

The current logs show normal letters being mapped from:

```txt
start state: 2010
```

and then the client generates Win release/restore keystrokes on its own.

So the goal is not to suppress those generated keystrokes directly. The goal is
to make sure `start state` no longer contains `KeyModifierSuper`.

## Recommended Temporary Logging

Add focused temporary logs in these places:

- `KeyState::fakeKeyUp(KeyButton)`
- `KeyState::fakeKeyUp(KeyID, KeyButton)`
- `MSWindowsKeyState::pollActiveModifiers()`
- any place where `m_mask` changes
- any place where `m_activeModifiers` adds or removes `KeyModifierSuper`

Useful fields to print:

- incoming `id`
- incoming `button`
- resolved `localID`
- `m_mask` before and after
- whether `m_activeModifiers` still contains `Super`
- whether `m_keys[localID]` and `m_syntheticKeys[localID]` are nonzero

## Likely Repair Direction

Most likely fix order:

1. Make sure `Super_L` / `Super_R` release clears Windows client shadow state
   even in keyboard-only mode.
2. If needed, explicitly normalize modifier state when the Windows client begins
   accepting keyboard-only input without a full screen enter/leave.
3. Only if the above fails, look for a narrower Windows-specific distinction
   between physical local Win-key state and remote synthetic Win-key state.

## Most Recent Validation Result

The isolated rebuilt Windows client was launched successfully on
2026-04-24, initially hit a temporary `Connection was refused` error while the
macOS server was unavailable, then connected successfully once the server was
reachable.

After connection:

- no sticky `Right Win` was observed
- no sticky `Control` was observed
- the rebuilt client became the preferred baseline for future debugging

If the bug reappears later, re-run using the same rebuilt binary and inspect the
temporary `fakeKeyUp(...)` and `pollActiveModifiers()` logs before making any
new code changes.

## Bonjour / Launcher Follow-Up

As of 2026-04-27, the Windows daily launcher was updated outside the repo to
use this order:

1. try Bonjour discovery first
2. if that fails, prompt for manual host/IP input in the CLI

That launcher currently cannot rely on Bonjour on this machine because the
Windows `Bonjour Service` process (`C:\Program Files\Bonjour\mDNSResponder.exe`,
version `3.1.0.1`) crashes immediately on startup.

Observed local failure signature:

- `dns-sd -V` -> `DNSServiceGetProperty failed -65563`
- Windows Event Log:
  - faulting app: `mDNSResponder.exe`
  - exception code: `0xc0000409`
  - fault offset: `0x00000000000437c3`

Important scope note:

- this is separate from the InputLeap sticky-modifier bug
- it breaks Windows-side Bonjour auto-discovery, but does not prevent manual
  InputLeap client startup when a host/IP is provided

## Acceptance Criteria

The fix is correct when all of the following are true:

1. Windows client still receives the `Super_L` / `Super_R` key-up packets.
2. After that, ordinary key presses no longer start from `start state: 2010`.
3. Normal letters no longer generate:
   - `15b up`
   - `15c up`
   - `15b down`
   - `15c down`
4. `Right Win` is no longer perceived as permanently held.

## One-Line Task Prompt

Use this prompt for the Windows-side Codex session:

```txt
Please focus only on the Windows client side of the custom InputLeap keyboard-only switching patch. Evidence shows the macOS server is already sending Super_L/Super_R key-up events, but the Windows client keeps KeyModifierSuper in its local shadow state (start state stays 0x2010), causing normal letters to restore LWin/RWin automatically. Trace recv key up -> Screen::keyUp -> KeyState::fakeKeyUp -> MSWindowsKeyState::pollActiveModifiers and fix the client so ordinary key mapping no longer starts with KeyModifierSuper after those releases.
```
