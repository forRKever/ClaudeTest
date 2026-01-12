# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ShinPlayer is a Windows Qt-based video player designed to play proprietary video formats from specific surveillance/security camera vendors. It wraps the PlayM4 SDK (commonly used by Hikvision and similar vendors) with a Qt5 GUI interface.

## Build Commands

```bash
# Generate Makefile from Qt project file
qmake qt_port.pro -spec win32-msvc

# Build release version
nmake release

# Build debug version
nmake debug

# Clean build artifacts
nmake clean
```

**Requirements:**
- Qt 5.12.12 (static build at `C:\Qt\QtStatic\5.12.12`)
- MSVC compiler (Visual Studio)
- Windows platform only

## Architecture

### Core Components

**MediaPlayerWrapper** (`qt_port/src/MediaPlayerWrapper.cpp/.h`)
- Qt wrapper class around the native PlayM4 SDK
- Handles all SDK function calls via the `NAME()` macro (supports both standard PlayM4 and Hikvision variant via `_FOR_HIKPLAYM4_DLL_` define)
- Manages playback state machine: `Stopped → Playing ↔ Paused → Step`
- Provides watermark extraction from video streams via callback mechanism
- Thread-safe watermark data access using `QMutex`

**PlayerDialog** (`qt_port/src/PlayerDialog.cpp/.h`)
- Main application window (inherits `QMainWindow`)
- Handles user interactions: drag-drop files, keyboard shortcuts, mouse events
- Controls playback speed (0.0625x to 16x via `m_speedLevels` vector)
- Timer-based UI updates for position/status display

**WatermarkDialog** (`qt_port/src/watermarkdialog.cpp/.h`)
- Dialog for displaying embedded watermark metadata from video files (device SN, MAC, timestamp, etc.)

### SDK Integration

The `SDK/` folder contains the PlayM4 SDK:
- `ShinPlayCtrl.lib` - Link library
- `*.dll` - Runtime dependencies (must be in output directory)
- `PlayM4.h` → `WindowsPlayM4.h` - SDK header with function declarations

Key SDK patterns:
- Port-based API: acquire port with `PlayM4_GetPort()`, release with `PlayM4_FreePort()`
- DirectDraw rendering: `PlayM4_InitDDrawDevice()`, `PlayM4_SetDDrawDevice()`
- Callbacks for async events: file reference completion, watermark data, encoding changes

### File Structure

```
qt_port/
├── forms/PlayerDialog.ui    # Qt Designer UI file
└── src/
    ├── main.cpp             # Application entry point
    ├── MediaPlayerWrapper.* # SDK wrapper layer
    ├── PlayerDialog.*       # Main window UI logic
    └── watermarkdialog.*    # Watermark display dialog
SDK/                         # PlayM4 SDK binaries and headers
```

## Key Implementation Details

- Speed control uses `PlayM4_Fast()` / `PlayM4_Slow()` functions, not direct multiplier API
- Snapshots captured via `PlayM4_GetBMP()` / `PlayM4_GetJPEG()` to memory buffer, then written to file
- Watermark callback extracts binary data: globalTime (DWORD), deviceSN (DWORD), MAC (6 bytes), deviceType, deviceInfo, channelNum
- UI form defined in `PlayerDialog.ui`, compiled by Qt's uic tool
