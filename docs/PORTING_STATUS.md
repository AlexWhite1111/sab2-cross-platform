# Porting matrix and verification gates

Baseline: `SunOner/sunone_aimbot_2` @ `03059d5ed2bc2b30f0140b48c01b8992b3e46ffd`, 2026-11-06.

| Subsystem | Upstream (source inspection) | This package | 1:1 parity |
|---|---|---|---|
| Cross-platform CMake | Explicit `if(NOT WIN32) FATAL_ERROR` | C++17 Windows/macOS/Linux build | NO |
| Capture | WinRT, duplication, virtual camera, UDP | OpenCV camera/video and synthetic | NO |
| macOS screen capture | Not provided upstream | Not implemented; ScreenCaptureKit integration required | NO |
| Model inference | TensorRT/CUDA and DirectML paths | Optional OpenCV DNN ONNX CPU | NO |
| Model input decode | Upstream-specific postprocess | Limited YOLOv8/11 detect layout | NO |
| Target selection | SAB2 nodes/vision runtime | One nearest tracked target | NO |
| Motion | Multiple upstream algorithms | Lightweight P + EMA baseline | NO |
| Input | Windows APIs + KMBOX/MAKCU/etc. | Win SendInput, macOS Quartz, dry-run | NO |
| Overlay / node graphs | ImGui + Win32/DX11 | No GUI editor; optional OpenCV preview | NO |
| Dual-PC | Upstream UDP capture and device methods | Camera-capture-card input only; remote HID not provided | NO |
| Tests | Upstream test suite | Portable core tests and synthetic CSV | PARTIAL |

## Engineering gates before calling a subsystem 'ported'

1. **Build reproducibility**: CI compiles x64 Windows and arm64 macOS 27 with pinned dependencies.
2. **Behavioral parity**: same frame/test fixture -> matching detections/motions within documented tolerance.
3. **Performance**: benchmark capture latency, inference P50/P95, dropped frames, memory footprint, tracking reacquisition under repeatable video clips.
4. **Permissions**: screen recording/camera/accessibility permission UX tested on real Mac.
5. **Multi-machine reliability**: video transport, sync, packet loss, and HID feedback measured using external hardware; no claims based solely on a simulated flow.
6. **Rollback**: release artifacts and config migrations versioned; existing Windows SAB2 remains a separate fallback.

## Concrete outstanding implementation order

1. Extract original upstream platform-independent geometry, graph model, settings and vision postprocess into a shared library, with upstream license preserved.
2. Implement macOS ScreenCaptureKit frames and device enumeration; use a well-defined pixel-buffer interface and timestamped frame metadata.
3. Add Core ML/Metal or validated Apple Silicon ONNX Runtime execution provider; compare detection outputs to Windows reference on the same ONNX test suite.
4. Replace DX11/Win32 ImGui backend with Metal/macOS GLFW or SDL2 backend while preserving behavior of each node type.
5. Port output-device protocols and explicitly document what macOS Quartz does and does not provide.
6. Implement video-over-network and HID adapters for a separately validated two-computer path.

## Verification scope

See the [handoff test report](../TEST_REPORT.md) for the Windows 10 x64 core and synthetic demo results, the historical Linux result, and pending Apple Silicon and video tests.
