# Intended cross-platform architecture

```
OpenCV camera / video / synthetic generator
           | timestamp + pixels
           v
       VisionDetector <--- optional YOLO ONNX (OpenCV CPU)
           |
           v
       SingleTargetTracker --- ID, position, velocity, hold
           |
           v
       ProportionalController --- bounded dx,dy
           |
           +--> Dry-run / CSV (default)
           +--> Windows SendInput (--enable-input)
           +--> macOS Quartz (--enable-input)
```

This is intentionally split into three layers:

- **Core** (`include/sab2/core`, `src/tracker.cpp`, `src/controller.cpp`): dependency-free C++17; can be extended and unit-tested independently.
- **Video/inference** (`src/opencv_pipeline.cpp`): optional OpenCV camera/video/YOLO ONNX decoder; future ScreenCaptureKit/WinRT/UDP adapters fit upstream of this layer.
- **Input** (`src/input_*.cpp`): OS-level adapters, defaulting to a dry-run sink. Device-specific methods should live behind the same interface and must be verified on hardware.

The portable core deliberately does not depend on CUDA, DirectML, WinRT or macOS Quartz. It does not copy SAB2's actual tracking, configuration or graph implementation. For a true 1:1 port, import tested original logic carefully behind these boundaries rather than treating the lightweight example as equivalent.

## Potential multi-PC topology (NOT implemented)

```
PC A (game) -- HDMI or other permitted video output --> capture card --> PC B (Mac)
PC B: capture -> detection -> target tracking -> motion prediction
PC B -- separately validated USB/HID interface --> PC A
```

No HID controller protocol or automatic remote input is bundled; connecting a camera feed to a Mac is **not** sufficient by itself to control another computer.
