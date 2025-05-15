using System;
using System.Runtime.InteropServices;
using System.Text;

namespace AikitWpfDemo
{
    internal static class NativeMethods
    {
        private const string DllPath = @"D:\AIKITDLL\x64\Debug\AIKITDLL.dll";

        #region DLL导入 - 基础功能
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)] 
        public static extern int GetPgsResult(StringBuilder buffer, int bufferSize, out bool isNewResult);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int InitializeSDK();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void CleanupSDK();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr GetLastResult();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StartEsrMicrophone();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StartWakeup();
        #endregion

        #region 语音助手循环控制
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StartVoiceAssistantLoop();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int StopVoiceAssistantLoop();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetVoiceAssistantState();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetLastCommandResult();
        #endregion

        #region 获取唤醒状态相关接口
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetWakeupStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ResetWakeupStatus();

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetWakeupInfoString();
          [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr GetWakeupStatusDetails();
        
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl)]
        public static extern int TestWakeupDetection();
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

        // 更新以下结果获取函数的P/Invoke声明
        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int GetHtkResult(StringBuilder buffer, int bufferSize, out bool isNewResult);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int GetPlainResult(StringBuilder buffer, int bufferSize, out bool isNewResult);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int GetVadResult(StringBuilder buffer, int bufferSize, out bool isNewResult);

        [DllImport(DllPath, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int GetReadableResult(StringBuilder buffer, int bufferSize, out bool isNewResult);

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
        

        public static string GetLatestPgsResult()
        {
            StringBuilder buffer = new StringBuilder(8192); // 假设默认缓冲区大小
            bool isNewResult;
            // 调用新的P/Invoke签名
            int len = GetPgsResult(buffer, buffer.Capacity, out isNewResult);

            if (len > 0 && isNewResult)
            {
                // 处理新结果
                return buffer.ToString();
            }
            return string.Empty; // 或者根据需要返回 null 或其他
        }

        // 更新以下辅助方法以使用新的P/Invoke签名
        public static string GetHtkResultString( )
        {
            StringBuilder buffer = new StringBuilder(8192); // 假设默认缓冲区大小
            bool isNewResult;
            int len = GetHtkResult(buffer, buffer.Capacity, out isNewResult);

            if (len > 0) // isNewResult 条件可以由调用者判断
            {
                return buffer.ToString();
            }
            return string.Empty;
        }

        public static string GetPlainResultString()
        {
            StringBuilder buffer = new StringBuilder(8192); // 假设默认缓冲区大小
            bool isNewResult;
            int len = GetPlainResult(buffer, buffer.Capacity, out isNewResult);

            if (len > 0)
            {
                return buffer.ToString();
            }
            return string.Empty;
        }

        public static string GetVadResultString( )
        {
            StringBuilder buffer = new StringBuilder(8192); // 假设默认缓冲区大小
            bool isNewResult;
            int len = GetVadResult(buffer, buffer.Capacity, out isNewResult);

            if (len > 0)
            {
                return buffer.ToString();
            }
            return string.Empty;
        }

        public static string GetReadableResultString( )
        {
            StringBuilder buffer = new StringBuilder(8192); // 假设默认缓冲区大小
            bool isNewResult;
            int len = GetReadableResult(buffer, buffer.Capacity, out isNewResult);

            if (len > 0)
            {
                return buffer.ToString();
            }
            return string.Empty;
        }


        #endregion
    }
}