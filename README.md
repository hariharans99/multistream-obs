# obs-multistream-plugin

An OBS Studio plugin that streams to **multiple RTMP destinations simultaneously**, each with its own independent:

- **Resolution** (e.g., 1920×1080, 1280×720, 720×1280)
- **Bitrate** (e.g., 6000 kbps, 3000 kbps, 1500 kbps)
- **Aspect Ratio** (16:9, 9:16, 1:1 — with Letterbox, Crop, or Stretch handling)

---

## Features

| Feature | Details |
|---|---|
| Dynamic stream targets | Add / remove streams at any time |
| Hardware encoding | NVENC → AMF → QuickSync → x264 auto-fallback |
| Aspect ratio modes | **Letterbox** (black bars), **Crop+Zoom** (center-cut), **Stretch** |
| Dockable UI | Integrated into OBS as a native dock panel |
| Live stats | Bytes sent, dropped frames, FPS — refreshed every second |
| NVENC limit guard | Warns before exceeding your GPU's encoder session limit |
| Reconnect | Auto-reconnects on disconnect (20 retries, 10s delay) |

---

## Requirements

| Tool | Version |
|---|---|
| OBS Studio | 30.0.0 or later |
| Visual Studio | 2022 (C++ Desktop workload) |
| CMake | 3.16 or later |
| Qt | 6.x (bundled with OBS SDK) |

---

## Building on Windows

### Step 1 — Set up the OBS development environment

Follow the official OBS build guide:
<https://github.com/obsproject/obs-studio/wiki/build-instructions-for-windows>

Export the path to your OBS build output:
```powershell
$env:OBS_BUILD_DIR = "C:\path\to\obs-studio\build_x64"
```

### Step 2 — Clone this repository

```powershell
git clone https://github.com/your-username/obs-multistream-plugin.git
cd obs-multistream-plugin
```

### Step 3 — Configure and build

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

### Step 4 — Install into OBS

```powershell
cmake --install build_x64 --prefix "C:\Program Files\obs-studio"
```

Then restart OBS Studio.

---

## Usage

1. Open OBS → **View → Docks → Multi-Stream Output**
2. Click **+ Add Stream**
3. Fill in:
   - RTMP URL and stream key
   - Target resolution and bitrate
   - Aspect ratio handling mode
4. Click **▶ Start All** to begin streaming to all targets simultaneously

Double-click any row to edit its settings (stop the stream first).

---

## Aspect Ratio Modes

| Mode | Behaviour | Best for |
|---|---|---|
| **Letterbox** | Black bars fill unused space | Preserving all content |
| **Crop + Zoom** | Center of frame fills target | Vertical / square clips |
| **Stretch** | Frame is distorted to fit | Legacy 4:3 platforms |

---

## File Structure

```
obs-multistream-plugin/
├── CMakeLists.txt
├── CMakePresets.json
├── buildspec.json
├── cmake/common/buildspec.cmake
├── src/
│   ├── plugin-main.cpp          ← OBS module entry point
│   ├── multistream-output.h/cpp ← Output manager (singleton)
│   ├── stream-target.h/cpp      ← Per-stream encoder + RTMP output
│   ├── stream-config.h          ← StreamConfig + StreamStats structs
│   ├── video-scaler.h/cpp       ← Aspect-ratio-aware frame scaler
│   ├── settings-ui.h/cpp        ← Qt dock panel + per-stream dialog
│   └── util/
│       ├── hw-capability.h/cpp  ← NVENC/AMF/QSV probe + session check
│       └── logger.h/cpp         ← blog() wrapper with module prefix
└── data/locale/en-US.ini
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.
