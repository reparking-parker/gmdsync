# gmdsync

A lightweight [Geode](https://geode-sdk.org/) mod for Geometry Dash that automatically rewinds and restarts playback in [REAPER DAW](https://www.reaper.fm/) via OSC (Open Sound Control) over UDP whenever your level resets or respawns.

## How It Works

Whenever you die, restart, or respawn in Geometry Dash (`PlayLayer::resetLevel`), **gmdsync** sends three non-blocking UDP OSC action datagrams to REAPER:

1. `/action/1016` &mdash; `Transport: Stop` (halts current playback immediately)
2. `/action/40042` &mdash; `Transport: Go to start of project` (rewinds cursor to `0:00.000`)
3. `/action/1007` &mdash; `Transport: Play` (unconditionally initiates playback from project start)

This prevents play/pause toggling issues and ensures REAPER always restarts cleanly from `0:00` on every single attempt.

---

## REAPER Setup (OSC Control Surface)

To configure REAPER to listen for OSC messages from **gmdsync**:

1. Open **REAPER**.
2. Go to **Options** &rarr; **Preferences...** (or press `Ctrl+P` on Windows / `Cmd+,` on macOS).
3. Scroll down the left sidebar to **Control/OSC/web**.
4. Click **Add**.
5. Set the following options in the dialog:
   - **Control surface mode**: `OSC (Open Sound Control)`
   - **Device name**: `gmdsync` (or any label you prefer)
   - **Pattern config**: `Default`
   - **Mode**: `Configure device IP + local port` (or `Receive only`)
   - **REAPER listen port**: `8000` (must match the port in Geode mod settings)
   - *Device IP* / *Device port*: Can be left blank or default, as communication is one-way from GD to REAPER.
6. Click **OK**, then click **Apply**.

---

## Mod Settings

You can configure **gmdsync** in-game via the Geode Mods menu:

- **Enabled** (default: `true`): Toggle OSC synchronization on or off.
- **OSC Port** (default: `8000`): The UDP destination port matching REAPER's OSC listen port.
