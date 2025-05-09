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

        // 唤醒相关接口
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetWakeupStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ResetWakeupStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetWakeupInfoString();

        // ESR命令词识别相关接口 - 新增
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetEsrStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ResetEsrStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetEsrKeywordResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetEsrErrorInfo();

        // 新增 - 实时语音识别结果接口
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetSpeechRecognitionText();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]  // 将C++ bool映射为C# bool
        public static extern bool HasNewSpeechResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ClearSpeechRecognitionBuffer();

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

        // 辅助方法：获取ESR命令词识别结果 - 新增
        public static string GetEsrKeywordResultString()
        {
            IntPtr ptr = GetEsrKeywordResult();
            if (ptr != IntPtr.Zero)
            {
                string result = Marshal.PtrToStringAnsi(ptr);
                return result;
            }
            return "无命令词识别结果";
        }

        // 辅助方法：获取ESR错误信息 - 新增
        public static string GetEsrErrorInfoString()
        {
            IntPtr ptr = GetEsrErrorInfo();
            if (ptr != IntPtr.Zero)
            {
                string result = Marshal.PtrToStringAnsi(ptr);
                return result;
            }
            return "无错误信息";
        }

        // 辅助方法：获取实时语音识别文本
        public static string GetSpeechRecognitionTextString()
        {
            IntPtr ptr = GetSpeechRecognitionText();
            if (ptr != IntPtr.Zero)
            {
                string result = Marshal.PtrToStringAnsi(ptr);
                return result;
            }
            return "";
        }
    }
}