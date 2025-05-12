using System;
using System.Runtime.InteropServices;

namespace AikitWpfDemo
{
    internal static class NativeMethods
    {
        private const string DllPath = @"D:\AIKITDLL\x64\Debug\AIKITDLL.dll";

        #region DLL导入 - 基础功能
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int InitializeSDK(string appID, string apiKey, string apiSecret, string workDir);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void CleanupSDK();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetLastResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RunFullTest();
        #endregion

        #region 获取唤醒状态相关接口
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetWakeupStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ResetWakeupStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetWakeupInfoString();
        #endregion

        #region ESR命令词识别相关接口
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetEsrStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ResetEsrStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetEsrKeywordResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetEsrErrorInfo();
        
        // 各种格式命令词识别结果接口
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetPgsResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetHtkResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetPlainResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetVadResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetReadableResult();

        // 检查是否有新结果
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]  // 将C++ bool映射为C# bool
        public static extern bool HasNewPgsResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool HasNewHtkResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool HasNewPlainResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool HasNewVadResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool HasNewReadableResult();

        //  清空所有结果缓冲区
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ClearAllResultBuffers();
        #endregion

        #region 辅助方法 - 结果字符串转换
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

        // 辅助方法：获取唤醒词信息字符串（修正中文乱码问题，采用UTF-8解码）
        public static string GetWakeupInfoStringResult()
        {
            IntPtr ptr = GetWakeupInfoString();
            if (ptr != IntPtr.Zero)
            {
                // 计算字符串长度（遇到\0为止）
                int len = 0;
                while (Marshal.ReadByte(ptr, len) != 0) len++;
                if (len > 0)
                {
                    byte[] buffer = new byte[len];
                    Marshal.Copy(ptr, buffer, 0, len);
                    // 用UTF-8解码，防止中文乱码
                    string result = System.Text.Encoding.UTF8.GetString(buffer);
                    return result;
                }
            }
            return "无唤醒信息";
        }

        // 辅助方法：获取ESR命令词识别结果
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

        // 辅助方法：获取ESR错误信息
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
        #endregion

        #region 辅助方法 - 各种格式识别结果转换
        public static string GetPgsResultString()
        {
            IntPtr ptr = GetPgsResult();
            if (ptr != IntPtr.Zero)
            {
            string result = Marshal.PtrToStringAnsi(ptr);
            return result;
            }
            return "";
        }

        public static string GetHtkResultString()
        {
            IntPtr ptr = GetHtkResult();
            if (ptr != IntPtr.Zero)
            {
            string result = Marshal.PtrToStringAnsi(ptr);
            return result;
            }
            return "";
        }

        public static string GetPlainResultString()
        {
            IntPtr ptr = GetPlainResult();
            if (ptr != IntPtr.Zero)
            {
            string result = Marshal.PtrToStringAnsi(ptr);
            return result;
            }
            return "";
        }

        public static string GetVadResultString()
        {
            IntPtr ptr = GetVadResult();
            if (ptr != IntPtr.Zero)
            {
            string result = Marshal.PtrToStringAnsi(ptr);
            return result;
            }
            return "";
        }

        public static string GetReadableResultString()
        {
            IntPtr ptr = GetReadableResult();
            if (ptr != IntPtr.Zero)
            {
            string result = Marshal.PtrToStringAnsi(ptr);
            return result;
            }
            return "";
        }
        #endregion
    }
}