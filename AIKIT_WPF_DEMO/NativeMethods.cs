using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace AikitWpfDemo
{
    internal static class NativeMethods
    {
        private const string DllPath = @"C:\AIKITDLL\x64\Debug\AIKITDLL.dll";

        // Define ESR Status constants to match C++ definitions
        public const int ESR_STATUS_NONE_INTERNAL = 0;
        public const int ESR_STATUS_PROCESSING_INTERNAL = 1;
        public const int ESR_STATUS_SUCCESS_INTERNAL = 2;
        public const int ESR_STATUS_FAILED_INTERNAL = 3;
        public const int ESR_STATUS_NO_MATCH_INTERNAL = 4;
        public const int ESR_STATUS_INITIALIZED_INTERNAL = 5;

        #region DLL导入 - 基础功能
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetPgsResult(byte[] buffer, int bufferSize, out bool isNewResult);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int InitializeSDK(string appID, string apiKey, string apiSecret, string workDir);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void CleanupSDK();

        // 新增: SDK逆初始化接口
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int UnInitSDK();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetLastResult();        
          // 新增: 获取ESR最终结果字符串接口 (修复函数名匹配问题)
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetLastEsrResult();
        
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StartEsrMicrophone();
        
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StartEsrMicrophoneDetection();
        
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StopEsrMicrophoneDetection();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StartWakeup();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StartWakeupDetection();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StopWakeupDetection();
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
        private static extern int GetEsrResult(byte[] buffer, int bufferSize);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetHtkResult(byte[] buffer, int bufferSize, out bool isNewResult);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetPlainResult(byte[] buffer, int bufferSize, out bool isNewResult);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetVadResult(byte[] buffer, int bufferSize, out bool isNewResult);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ClearAllResultBuffers();
        #endregion

        #region 辅助方法 - 结果字符串转换
        // 辅助方法：获取上次结果字符串 (确保UTF-8解码)
        public static string GetLastResultString()
        {
            IntPtr ptr = GetLastResult();
            if (ptr != IntPtr.Zero)
            {
                int len = 0;
                while (Marshal.ReadByte(ptr, len) != 0) len++;
                if (len > 0)
                {
                    byte[] buffer = new byte[len];
                    Marshal.Copy(ptr, buffer, 0, len);
                    return System.Text.Encoding.UTF8.GetString(buffer); // 显式UTF-8解码
                }
            }
            return "无结果"; // 或者 string.Empty
        }

        // 辅助方法：获取唤醒词信息字符串（修正中文乱码问题，采用UTF-8解码）
        public static string GetWakeupInfoStringResult()
        {
            IntPtr ptr = GetWakeupInfoString();
            if (ptr != IntPtr.Zero)
            {
                int len = 0;
                while (Marshal.ReadByte(ptr, len) != 0) len++;
                if (len > 0)
                {
                    byte[] buffer = new byte[len];
                    Marshal.Copy(ptr, buffer, 0, len);
                    return System.Text.Encoding.UTF8.GetString(buffer); // 显式UTF-8解码
                }
            }
            return "无唤醒信息"; // 或者 string.Empty
        }

        // New helper method to get final ESR result (keyword or error) using the buffer method
        public static string GetEsrFinalDisplayResult()
        {
            byte[] buffer = new byte[1024 * 2]; // 2KB buffer, adjust if necessary
            int length = GetEsrResult(buffer, buffer.Length);
            if (length > 0)
            {
                return System.Text.Encoding.UTF8.GetString(buffer, 0, length);
            }
            return string.Empty; // Or a default message like "No result available"
        }
        #endregion

        #region 辅助方法 - 各种格式识别结果转换
        

        public static string GetLatestPgsResult()
        {
            byte[] buffer = new byte[8192]; // 使用 byte[]
            bool isNewResult;
            // 调用新的P/Invoke签名
            int len = GetPgsResult(buffer, buffer.Length, out isNewResult);

            if (len > 0 && isNewResult)
            {
                // 处理新结果，使用UTF-8解码
                return System.Text.Encoding.UTF8.GetString(buffer, 0, len);
            }
            return string.Empty; // 或者根据需要返回 null 或其他
        }

        // 更新以下辅助方法以使用新的P/Invoke签名
        public static string GetHtkResultString( )
        {
            byte[] buffer = new byte[8192]; // 使用 byte[]
            bool isNewResult;
            int len = GetHtkResult(buffer, buffer.Length, out isNewResult);

            if (len > 0) // isNewResult 条件可以由调用者判断
            {
                return System.Text.Encoding.UTF8.GetString(buffer, 0, len); // 使用UTF-8解码
            }
            return string.Empty;
        }

        public static string GetPlainResultString()
        {
            byte[] buffer = new byte[8192]; // 使用 byte[]
            bool isNewResult;
            int len = GetPlainResult(buffer, buffer.Length, out isNewResult);

            if (len > 0)
            {
                return System.Text.Encoding.UTF8.GetString(buffer, 0, len); // 使用UTF-8解码
            }
            return string.Empty;
        }

        public static string GetVadResultString( )
        {
            byte[] buffer = new byte[8192]; // 使用 byte[]
            bool isNewResult;
            int len = GetVadResult(buffer, buffer.Length, out isNewResult);

            if (len > 0)
            {
                return System.Text.Encoding.UTF8.GetString(buffer, 0, len); // 使用UTF-8解码
            }
            return string.Empty;
        }

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern IntPtr GetLastPgsResult();

        // 辅助方法：获取最新的PGS结果字符串(实时渐进式识别结果)
        public static string GetLastPgsResultString()
        {
            IntPtr ptr = GetLastPgsResult();
            if (ptr != IntPtr.Zero)
            {
                int len = 0;
                while (Marshal.ReadByte(ptr, len) != 0) len++;
                if (len > 0)
                {
                    byte[] buffer = new byte[len];
                    Marshal.Copy(ptr, buffer, 0, len);
                    return System.Text.Encoding.UTF8.GetString(buffer);
                }
            }
            return string.Empty;
        }


        // public static string GetReadableResultString( )
        // {
        //     StringBuilder buffer = new StringBuilder(8192); // 假设默认缓冲区大小
        //     bool isNewResult;
        //     int len = GetReadableResult(buffer, buffer.Capacity, out isNewResult);

        //     if (len > 0)
        //     {
        //         return buffer.ToString();
        //     }
        //     return string.Empty;
        // }        // 新增: 获取ESR最终结果字符串辅助方法 (修复函数名匹配问题)
        public static string GetLastEsrResultStringResult()
        {
            IntPtr ptr = GetLastEsrResult();
            if (ptr != IntPtr.Zero)
            {
                int len = 0;
                while (Marshal.ReadByte(ptr, len) != 0) len++;
                if (len > 0)
                {
                    byte[] buffer = new byte[len];
                    Marshal.Copy(ptr, buffer, 0, len);
                    return System.Text.Encoding.UTF8.GetString(buffer);
                }
            }
            return string.Empty;
        }

        #endregion

        #region 统一语音交互接口
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StartUnifiedVoiceInteraction(int wakeupThreshold, int esrTimeout);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StopUnifiedVoiceInteraction();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetUnifiedVoiceState();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int IsUnifiedVoiceInteractionRunning();
        #endregion
    }
}