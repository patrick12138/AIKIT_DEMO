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
        public static extern int GetWakeupStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ResetWakeupStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RunFullTest();
        // 辅助方法：获取上次结果字符串
        public static string GetLastResultString()
        {
            IntPtr ptr = GetLastResult();
            if (ptr != IntPtr.Zero)
            {
                string result = Marshal.PtrToStringAnsi(ptr);
                // 注意：此处应该有一个释放内存的操作，但我们的C++ DLL未提供
                // 这可能会导致内存泄漏，实际应用中应该修改C++ DLL提供释放方法
                return result;
            }
            return "无结果";
        }
    }
}