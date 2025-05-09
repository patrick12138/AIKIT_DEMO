using System;
using System.Runtime.InteropServices;

namespace AikitWpfDemo
{
    internal static class NativeMethods
    {
        // 指定DLL完整路径
        private const string DllPath = @"D:\AIKITDLL\x64\Debug\AIKITDLL.dll";

        // DLL导入
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int InitializeSDK(string appID, string apiKey, string apiSecret, string workDir);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void CleanupSDK();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetLastResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int Ivw70Init(string resourcePath);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Ivw70Uninit();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int IvwFromMicrophone(int threshold);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int IvwFromFile(string audioFilePath, int threshold);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RunFullTest();

        // 新增 - 获取唤醒状态
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetWakeupStatus();

        // 新增 - 重置唤醒状态
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ResetWakeupStatus();

        // 新增 - 获取唤醒详细信息
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetWakeupInfoString();

        // 辅助方法：获取上次结果字符串
        public static string GetLastResultString()
        {
            IntPtr ptr = GetLastResult();
            if (ptr != IntPtr.Zero)
            {
                string result = Marshal.PtrToStringAnsi(ptr);
                return result;
            }
            return "无结果";
        }

        // 辅助方法：获取唤醒词信息字符串
        public static string GetWakeupInfoStringResult()
        {
            IntPtr ptr = GetWakeupInfoString();
            if (ptr != IntPtr.Zero)
            {
                string result = Marshal.PtrToStringAnsi(ptr);
                return result;
            }
            return "无唤醒信息";
        }
    }
}