# Windows 与 Apple Silicon 移植

本分支以 [SunOner/sunone_aimbot_2 的 `03059d5`](https://github.com/SunOner/sunone_aimbot_2/commit/03059d5ed2bc2b30f0140b48c01b8992b3e46ffd) 为基线，保留上游 [MIT 许可](../LICENSE)。GitHub `main` 保存独立编写的 v0.1 交接原型。

公开移植分支从这个上游提交的源码快照起步。应用源码和测试保持上游内容，模型目录中的两个可选二进制权重仍通过[上游该提交的模型目录](https://github.com/SunOner/sunone_aimbot_2/tree/03059d5ed2bc2b30f0140b48c01b8992b3e46ffd/sunone_aimbot_2/depth_models)获取。

| 功能 | Windows | macOS Apple Silicon |
|---|---|---|
| 节点图计算、工作区序列化、旧版节点迁移 | 上游回归通过 | 已接入独立构建，待验证 |
| 配置迁移与旧键位编号 | 上游迁移测试通过，86 个 Win32 键位编号与 Windows SDK 一致 | 构建时无需 Win32 头文件，待验证 |
| 工作流 MCP 协议 | Python 协议回归通过 | 待验证 |
| 窗口几何计算 | 上游回归通过 | 待验证 |
| 节点编辑器、热键、透明叠加窗 | 保留上游构建入口，交互待验证 | 待移植 |
| DXGI / WinRT、虚拟摄像头、UDP | 保留上游逻辑，待实测 | 待移植 |
| ScreenCaptureKit、录制视频回放 | 回放待接入 | 待接入 |
| DirectML / TensorRT / ONNX / Core ML | 保留上游 DirectML/CUDA 构建入口，推理待实测 | 待接入 |
| 目标跟踪、预测、位移输出 | 节点逻辑由回归覆盖，实时输入与实际外设待实测 | 实时输入和系统输出待接入 |
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

## 实测记录

2026-11-20 在 Windows 10 Pro x64（19045）使用 MSVC 19.44.35228、CMake 3.31.6-msvc6、Windows SDK 10.0.26100.0 跑通四项上游回归，CTest 报告 4/4 通过。

- `node_graph_regressions`
- `node_config_migration`
- `overlay_window_geometry_regressions`
- `workflow_mcp_protocol_regressions`

本轮独立构建未加载 GPU 推理库，也未运行桌面应用、屏幕采集、实际输入设备或远端硬件。Windows 桌面版仍走 [上游构建流程](build.md)。
