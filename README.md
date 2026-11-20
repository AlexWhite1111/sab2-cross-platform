# SAB2 Cross-Platform Foundation — v0.1.0

**Status: working portable prototype, NOT a 1:1 port of Sunone Aimbot 2.**

Windows 10 x64 Debug build and 120-frame synthetic dry-run were verified during the handoff. See the [test report](TEST_REPORT.md), [source provenance](PROVENANCE.md), and [feature matrix](docs/PORTING_STATUS.md). macOS Apple Silicon and the optional video and ONNX path still need validation.

This is an independently written C++17 cross-platform **migration foundation** using the architecture of SunOner/sunone_aimbot_2 as a reference. It does **not** contain or modify the upstream source tree. The upstream commit inspected was `03059d5ed2bc2b30f0140b48c01b8992b3e46ffd` (2026-11-06). Download the original from <https://github.com/SunOner/sunone_aimbot_2> separately. The latest code observed at packaging time has a CMake gate that explicitly rejects non-Windows builds.

## Implemented

- C++17 build, tests, and deterministic synthetic trajectory demo. Build targets are Windows, macOS, and Linux.
- Portable single-target temporal association, brief occlusion hold, velocity estimation, proportional controller with smoothing and motion limit.
- Optional OpenCV camera/video capture (supports an OS-visible USB HDMI capture card when correctly configured).
- Optional OpenCV DNN ONNX inference for common **YOLOv8/YOLO11 Detect** `[1,84,N]` or `[1,N,84]` float tensors. An output family requiring another decoder is not supported. OpenCV DNN CPU backend is the portable baseline, **not Core ML acceleration**.
- Optional green-marker tracking without a neural model, for end-to-end testing using recorded frames/camera.
- Dry-run CSV mouse motion by default. OS-level mouse-move adapter available behind explicit `--enable-input` flag on Windows (SendInput) and macOS (Quartz), subject to OS permission/game input constraints. Linux is dry-run only.

## Not implemented / not verified

- Existing SAB2 ImGui/node editor, configuration compatibility, TensorRT, DirectML, Core ML, WinRT, DX11, ScreenCaptureKit, all special input devices (KMBOX/MAKCU), and remote control/dual-machine HID bridge are **NOT migrated**.
- This project is **not a signed, notarized macOS .app**, and contains no trained models.
- Windows verification covers the core and synthetic demo with OpenCV disabled. macOS remains an unverified build target.
- The tracker is a simple baseline rather than original SAB2 tracker code. This is a new starting point for progressive upstream migration.

## Build on macOS (Apple Silicon; target macOS 27)

Requires Xcode Command Line Tools, CMake >= 3.20, C++17 compiler; Ninja recommended.

```bash
xcode-select --install
brew install cmake ninja opencv
./scripts/build_mac.sh
./build-mac/sab2_demo --demo --frames 10
./build-mac/sab2_demo --camera 0 --no-gui --frames 100
```

For camera feed, macOS requires Camera permission. For actual Quartz pointer output (only if you opt in), Accessibility permission may also be required. Some fullscreen games use raw HID and may ignore Quartz pointer moves.

## Build on Windows 10/11

Requires Visual Studio Build Tools with C++17/CMake. To enable OpenCV, install OpenCV via vcpkg or provide `OpenCV_DIR` when configuring; otherwise the project still builds its core and synthetic demo.

```powershell
.\scripts\build_windows.ps1
.\build-windows\Release\sab2_demo.exe --demo --frames 10
```

## Build on Linux (CI/core validation only)

```bash
cmake -S . -B build -G Ninja -DSAB2_WITH_OPENCV=OFF -DSAB2_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/sab2_demo --demo --frames 10
```

## Test real video / ONNX models

```bash
# WITHOUT pretrained model: tracks a bright green test marker
./build-mac/sab2_demo --video clip.mp4 --frames 300
# WITH compatible YOLO Detect ONNX model (not included):
./build-mac/sab2_demo --video clip.mp4 --onnx model.onnx --input-size 640 --frames 300
```

`--enable-input` moves the current computer's OS pointer and is intentionally opt-in. **It does not move the pointer on a different computer.** For double-PC use, a separate compliant HID/USB output adapter and validation are necessary. The CSV output allows testing without issuing any live input.

## Follow-up work to approach upstream parity

See `docs/PORTING_STATUS.md` and `docs/ARCHITECTURE.md`. The recommended plan is incremental migration with explicit performance and behavior acceptance checks per subsystem. This repository is useful right away for testing core motion behavior, but should not be misrepresented as a finished SAB2/macOS executable.

## License and attribution

The new code in this archive is MIT licensed. It does **not** contain upstream Sunone Aimbot 2 source. Upstream project's `LICENSE` at the inspected commit is MIT; source attribution: SunOner, 2024–2026. See `LICENSE` and `UPSTREAM_BASELINE.json`.
