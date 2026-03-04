# AIKIT 演示项目（WPF + 原生 DLL）

[English](./README.md) | [简体中文](./README.zh-CN.md)

该仓库是一个 Windows 语音助手演示项目，包含：

- `AIKITDLL`：C++ 原生动态库（Visual C++ 工程）
- `AIKIT_WPF_DEMO`：通过 P/Invoke 调用 DLL 的 .NET 8 WPF 应用

## 项目结构

- `AIKITDLL.sln`：包含两个项目的 Visual Studio 解决方案
- `AIKITDLL/`：语音唤醒与命令识别相关的原生封装
- `AIKIT_WPF_DEMO/`：WPF 界面与语音助手循环管理逻辑

## 环境要求

- Windows 10/11
- Visual Studio 2022（v143 工具集）
- .NET SDK 8.0 及以上
- `AIKITDLL` 依赖的本地目录：
  - 头文件位于 `../include`
  - 库文件位于 `../libs/64`
  - 生成的运行时 DLL 需要可被 WPF 项目访问

## 构建步骤

1. 用 Visual Studio 2022 打开 `AIKITDLL.sln`。
2. 以 `x64` 配置构建 `AIKITDLL`。
3. 确保 `AIKIT_WPF_DEMO` 可找到生成的 `AIKITDLL.dll`。
4. 构建并运行 `AIKIT_WPF_DEMO`。

## 关键本地路径配置

WPF 项目当前在以下文件中使用了绝对 DLL 路径：

- `AIKIT_WPF_DEMO/NativeMethods.cs`

运行前请将 `DllPath` 改为你本机的 DLL 实际输出路径。

## 说明

- 这是本地演示仓库，默认包含一些机器相关路径配置。
- 若你调整了构建输出目录，请同步更新 DLL 引用路径。
