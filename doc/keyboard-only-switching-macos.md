# Keyboard-Only Switching on macOS Server

This repository contains a local patch set that allows the keyboard target to
be switched independently from the mouse target.

## What Was Added

- `switchKeyboardToScreen(<screen>)`
  Switches only the keyboard target.
- `followMouseForKeyboard`
  Re-attaches keyboard routing to the current mouse screen.
- Keyboard follow is on by default, but once `switchKeyboardToScreen(...)` is
  used, it stays detached until `followMouseForKeyboard` is triggered.
- On macOS server, local key events are swallowed while the keyboard target is
  detached from the primary screen. This prevents the Mac from typing locally
  while the keyboard is being sent to a remote client.

## Verified Topology

- Server: macOS
- Client: Windows
- Crypto: disabled on both sides for LAN use

## Verified Config

The working config used during validation lives at:

`/Users/charleschen/Code/InputLeap/input-leap.conf`

Relevant hotkeys:

```txt
keystroke(alt+super+Right) = switchKeyboardToScreen(CC-Workstation)
keystroke(alt+super+Left) = switchKeyboardToScreen(CC-MBP2024.local)
keystroke(alt+super+Down) = followMouseForKeyboard
```

On macOS, `super` corresponds to the Command key, so the tested shortcuts were:

- `Cmd+Option+Right`
- `Cmd+Option+Left`
- `Cmd+Option+Down`

## GUI Settings Used

The installed macOS app was configured with:

- `cryptoEnabled = 0`
- `useExternalConfig = 1`
- `configFile = /Users/charleschen/Code/InputLeap/input-leap.conf`

## Stable Server Launch Command

For debugging and deterministic verification, this foreground launch path was
used:

```bash
/Applications/InputLeap.app/Contents/MacOS/input-leaps \
  -f \
  --debug INFO \
  --name CC-MBP2024.local \
  --enable-drag-drop \
  --disable-crypto \
  --disable-client-cert-checking \
  -c /Users/charleschen/Code/InputLeap/input-leap.conf \
  --address :24800
```

When deeper hotkey tracing is needed, switch `--debug INFO` to `--debug DEBUG1`.

## Validation Results

Validated locally on 2026-04-23:

- Client connection succeeds with crypto disabled.
- `Cmd+Option+Right` triggers `switchKeyboardToScreen(CC-Workstation)`.
- Mouse remains on the Mac while keyboard is routed to Windows.
- macOS local applications no longer receive duplicate keyboard input while the
  keyboard target is detached.
- `Cmd+Option+Down` restores keyboard-follow-mouse behavior.

## Operational Note

Avoid running multiple `input-leap` / `input-leaps` instances at the same time.
During debugging, stale helper processes caused port `24800` conflicts and made
hotkey behavior appear inconsistent.

The GUI now includes a local macOS/Desktop mitigation for this: before the app
starts its server helper, it checks whether port `24800` is still held by a
same-bundle `input-leaps` process and terminates that stale helper before
launching the new one.
