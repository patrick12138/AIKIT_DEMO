# AIKIT Demo (WPF + Native DLL)

[English](./README.md) | [简体中文](./README.zh-CN.md)

This repository contains a Windows voice-assistant demo built with:

- `AIKITDLL`: native C++ DLL (Visual C++ project)
- `AIKIT_WPF_DEMO`: .NET 8 WPF app that calls the DLL via P/Invoke

## Project Structure

- `AIKITDLL.sln`: Visual Studio solution containing both projects
- `AIKITDLL/`: native wakeup + command recognition wrapper
- `AIKIT_WPF_DEMO/`: WPF UI and voice assistant loop manager

## Requirements

- Windows 10/11
- Visual Studio 2022 (v143 toolset)
- .NET SDK 8.0+
- Native dependencies expected by `AIKITDLL`:
  - headers under `../include`
  - libs under `../libs/64`
  - runtime DLL output accessible by WPF app

## Build

1. Open `AIKITDLL.sln` in Visual Studio 2022.
2. Build `AIKITDLL` in `x64` configuration.
3. Ensure the built `AIKITDLL.dll` can be found by `AIKIT_WPF_DEMO`.
4. Build and run `AIKIT_WPF_DEMO`.

## Important Local Path Configuration

The WPF app currently references an absolute DLL path in:

- `AIKIT_WPF_DEMO/NativeMethods.cs`

Update `DllPath` to your local output path before running.

## Notes

- This is a local demo repository and includes machine-specific path assumptions.
- If you move the build output location, update the DLL reference path accordingly.
