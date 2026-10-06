# Changelog

All notable changes to gmdsync will be documented here.

## [v1.0.0] - 2026-10-07

Initial release of **gmdsync**.

### Added
- Direct OSC (Open Sound Control) over UDP synchronization from Geometry Dash to REAPER DAW.
- Hook into `PlayLayer::resetLevel()` sending `/action/40042` (rewind to project start) and `/play` (start playback).
- Non-blocking UDP socket transmission supporting both Windows (WinSock2) and POSIX systems.
- In-game settings for enabling/disabling sync and configuring the destination UDP port.
- Quick test script `haruta.py` for testing REAPER OSC reception.
