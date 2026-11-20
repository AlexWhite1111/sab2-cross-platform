# v0.1 交接测试报告

验证日期 2026-11-20。环境为 Windows 10 Pro x64（19045）、Visual Studio 2022 Build Tools、MSVC 19.44.35228、Windows SDK 10.0.26100.0、CMake 3.31.6-msvc6。

| 检查 | 结果 |
|---|---|
| Windows x64 Debug，关闭 OpenCV | 构建成功，Windows 输入适配器通过编译和链接 |
| CTest，Debug 下启用断言 | 1/1 通过，涉及目标关联、失检保持、重新捕获和控制限幅 |
| 合成目标，120 帧，dry-run | CSV 含表头 121 行，与随包样例逐行一致。118 帧有检测结果，120 帧保有有效跟踪 |
| Linux 核心 | 原交接记录为 1/1 通过，本次未复跑 |
| Windows OpenCV、视频、ONNX、实际指针输出 | 未执行 |
| macOS Apple Silicon | 未编译或运行 |

Windows 构建出现 C4005 警告。CMake 和 `src/input_windows.cpp` 重复定义了 `NOMINMAX` 与 `WIN32_LEAN_AND_MEAN`。交接脚本默认跑 Release，而原测试使用 `assert`，因此本次用 Debug 实际执行断言。结果只覆盖 v0.1 独立原型，与上游的功能差异见 [迁移矩阵](docs/PORTING_STATUS.md)。

```powershell
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 -DSAB2_WITH_OPENCV=OFF -DSAB2_BUILD_TESTS=ON
cmake --build build-win --config Debug --parallel
ctest --test-dir build-win -C Debug --output-on-failure
.\build-win\Debug\sab2_demo.exe --demo --frames 120
```
