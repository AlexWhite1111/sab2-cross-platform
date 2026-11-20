# Windows 与 Apple Silicon 移植

本分支以 [SunOner/sunone_aimbot_2 的 `03059d5`](https://github.com/SunOner/sunone_aimbot_2/commit/03059d5ed2bc2b30f0140b48c01b8992b3e46ffd) 为基线，保留上游 [MIT 许可](../LICENSE)。GitHub `main` 保存独立编写的 v0.1 交接原型。

公开移植分支从这个上游提交的源码快照起步。应用源码和测试保持上游内容，模型目录中的两个可选二进制权重仍通过[上游该提交的模型目录](https://github.com/SunOner/sunone_aimbot_2/tree/03059d5ed2bc2b30f0140b48c01b8992b3e46ffd/sunone_aimbot_2/depth_models)获取。

用户设备为 Apple M4，验收系统为 macOS 27。CI 使用 [GitHub 的 xcode-27 ARM64 runner](https://github.com/actions/runner-images/blob/main/images/macos/xcode-27-arm64-Readme.md)。M4 的录屏授权、GPU / Neural Engine 分配、输入和外设行为需实机核对。

| 功能 | Windows | macOS Apple Silicon |
|---|---|---|
| 节点图计算、工作区序列化、旧版节点迁移 | 上游回归通过 | ARM64 CI 回归通过 |
| 配置迁移与旧键位编号 | 上游迁移测试通过，86 个 Win32 键位编号与 Windows SDK 一致 | ARM64 CI 回归通过 |
| 工作流 MCP 协议 | Python 协议回归通过 | ARM64 CI 回归通过 |
| 窗口几何计算 | 上游回归通过 | ARM64 CI 回归通过 |
| 节点编辑器、热键、透明叠加窗 | 保留上游构建入口，交互待验证 | 待移植 |
| DXGI / WinRT、虚拟摄像头、UDP | 保留上游逻辑，待实测 | 待移植 |
| 录制视频回放 | 已接入上游 Vision 节点图，实测见回归记录 | 已接入上游 Vision 节点图，实测见回归记录 |
| ScreenCaptureKit | 系统专属 API | 待接入 |
| DirectML / TensorRT / ONNX / Core ML | 保留上游 DirectML/CUDA 构建入口。回放使用 ONNX Runtime CPU | 回放使用 ONNX Runtime CPU。Core ML 可选，依赖支持此提供程序的运行库，尚未验证加速 |
| 目标跟踪、预测、位移输出 | 回放复用上游多目标跟踪器。实时预测、位移与实际外设待实测 | 回放复用上游多目标跟踪器。实时预测和系统输出待接入 |
| 深度遮罩、采集与标注 | 保留上游逻辑，待实测 | 待移植 |
| 双机和外设 | 保留上游逻辑，待实测 | 待移植 |

## 构建回归测试

需要 CMake 3.24+、C++17 和 Python 3（工作流协议测试）。Windows 可使用 Visual Studio 2022，macOS 可使用 Xcode Command Line Tools。

```powershell
cmake -S . -B build-port -G "Visual Studio 17 2022" -A x64 -DSAB2_PORTABLE_TESTS_ONLY=ON -DBUILD_TESTING=ON
cmake --build build-port --config Release --parallel
ctest --test-dir build-port -C Release --output-on-failure
```

```sh
cmake -S . -B build-port -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DSAB2_PORTABLE_TESTS_ONLY=ON -DBUILD_TESTING=ON
cmake --build build-port --parallel
ctest --test-dir build-port --output-on-failure
```

## 视频回放与检测结果

回放目标 `sab2_video_replay` 使用 OpenCV 4 读视频和预览，使用 ONNX Runtime 运行推理。它复用上游的 YOLO 解码器、Vision 图中的置信度过滤、数量限制、NMS 节点，以及多目标跟踪器。当前输入为视频文件和单输入、单输出的 YOLO ONNX。输入张量要求 float32、NCHW、固定宽高。CSV 记录检测框、跟踪 ID、参考点和每帧处理时间。处理时间从解码完成后开始计时。跟踪时间按视频标称帧率计算，读不到帧率时按 60 FPS 回放。

在 macOS ARM64 中使用 Homebrew 安装 OpenCV 4 和 ONNX Runtime 后构建。

```sh
brew install opencv@4 onnxruntime
cmake -S . -B build/replay -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DSAB2_BUILD_REPLAY=ON -DSAB2_ONNXRUNTIME_ROOT="$(brew --prefix onnxruntime)" \
  -DOpenCV_DIR="$(brew --prefix opencv@4)/lib/cmake/opencv4"
cmake --build build/replay --target sab2_video_replay --parallel
./build/replay/sab2_video_replay recording.avi detector.onnx > detections.csv
```

Windows 使用相同的构建选项，加上 `-DSAB2_PORTABLE_TESTS_ONLY=ON`、OpenCV 的 `OpenCV_DIR`，以及包含原生头文件和 x64 库的 ONNX Runtime 根目录。配置使用 Visual Studio 生成器时，在 build 命令中指定 `--config Release`。对于需要额外视频解码插件的 OpenCV 发行包，将其二进制目录加入当前进程的 PATH。

可以传入 `--preview` 打开检测框预览，按 Esc 退出。若 ONNX Runtime 提供 Core ML，可以传入 `--coreml`。实际分配到 Apple GPU 或 Neural Engine 的模型节点需单独验证。

用 Python 3.12 安装 `onnx==1.19.1` 及 `opencv-python-headless==4.12.0.88` 后，运行 `python tests/video_replay_smoke.py <sab2_video_replay路径>` 校验生成的视频和 ONNX 样例。样例验证三帧解码、六个检测框的坐标和置信度、重叠框抑制及跟踪 ID 的稳定性。真实训练模型的精度、位移输出和实机权限需另行验证。

## 实测记录

2026-11-20 在 Windows 10 Pro x64（19045）使用 MSVC 19.44.35228、CMake 3.31.6-msvc6、Windows SDK 10.0.26100.0 跑通四项上游回归，CTest 报告 4/4 通过。

- `node_graph_regressions`
- `node_config_migration`
- `overlay_window_geometry_regressions`
- `workflow_mcp_protocol_regressions`

[双平台 CI](https://github.com/AlexWhite1111/sab2-cross-platform/actions/runs/44699860788) 的 Windows x64 和 macOS ARM64 核心任务均通过 4/4。Windows 桌面版仍走 [上游构建流程](build.md)。本地尝试恢复 DirectML 依赖时出现 NuGet 下载超时，桌面版构建及实时路径尚待验证。
