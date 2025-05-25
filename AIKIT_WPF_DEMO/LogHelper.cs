using System;
using System.IO;
using System.Windows.Controls;
using System.Windows.Documents;

namespace AikitWpfDemo
{
    /// <summary>
    /// 日志辅助类，负责日志写入和界面日志显示
    /// 支持TextBox和TextBlock两种控件类型
    /// </summary>
    public static class LogHelper
    {
        private static string _logFilePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "wpf_app_log.txt");
        private static object? _logControl; // 允许任意类型，可为null

        /// <summary>
        /// 初始化日志控件，可以是TextBox或TextBlock
        /// </summary>
        public static void Init(object logControl, string? logFilePath = null)
        {
            _logControl = logControl;
            if (!string.IsNullOrEmpty(logFilePath))
                _logFilePath = logFilePath;
        }

        /// <summary>
        /// 写日志到控件和文件
        /// </summary>
        public static void LogMessage(string message)
        {
            string logEntry = $"[{DateTime.Now:HH:mm:ss}] {message}\n";
            
            if (_logControl != null)
            {
                // 确保在UI线程上执行
                System.Windows.Application.Current?.Dispatcher.Invoke(() =>
                {
                    // 兼容TextBox和TextBlock
                    if (_logControl is TextBox tb)
                    {
                        tb.Text += logEntry;
                        (tb.Parent as ScrollViewer)?.ScrollToEnd();
                    }
                    else if (_logControl is TextBlock tblock)
                    {
                        tblock.Text += logEntry;
                        // 对于TextBlock，尝试滚动到底部
                        if (tblock.Parent is ScrollViewer sv)
                        {
                            sv.ScrollToEnd();
                        }
                    }
                });
            }
            
            try
            {
                File.AppendAllText(_logFilePath, logEntry);
            }
            catch (Exception ex)
            {
                // 文件写入失败时的处理
                System.Windows.Application.Current?.Dispatcher.Invoke(() =>
                {
                    if (_logControl != null)
                    {
                        string err = $"[{DateTime.Now:HH:mm:ss}] [CRITICAL] Failed to write to log file: {ex.Message}\n";
                        if (_logControl is TextBox tb)
                            tb.Text += err;
                        else if (_logControl is TextBlock tblock)
                            tblock.Text += err;
                    }
                });
            }
        }

        /// <summary>
        /// 清空日志显示
        /// </summary>
        public static void ClearLog()
        {
            if (_logControl != null)
            {
                System.Windows.Application.Current?.Dispatcher.Invoke(() =>
                {
                    if (_logControl is TextBox tb)
                    {
                        tb.Text = string.Empty;
                    }
                    else if (_logControl is TextBlock tblock)
                    {
                        tblock.Text = string.Empty;
                    }
                });
            }
        }
    }
}
