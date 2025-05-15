using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;

namespace AikitWpfDemo
{
    /// <summary>
    /// 负责日志记录和管理
    /// </summary>
    public class LogManager
    {
        private static LogManager _instance;
        private TextBox _logTextBox;
        
        private LogManager() { }
        
        /// <summary>
        /// 获取LogManager的单例实例
        /// </summary>
        public static LogManager Instance 
        { 
            get 
            { 
                if (_instance == null)
                {
                    _instance = new LogManager();
                }
                return _instance; 
            }        }        
        /// <summary>
        /// 初始化日志管理器，设置用于显示日志的TextBox控件
        /// </summary>
        /// <param name="logTextBox">用于显示日志的TextBox控件</param>
        public void Initialize(TextBox logTextBox)
        {
            _logTextBox = logTextBox;
        }
        
        /// <summary>
        /// 记录日志消息到界面
        /// </summary>
        /// <param name="message">要记录的消息</param>
        public void LogMessage(string message)
        {
            if (_logTextBox == null)
            {
                return; // 如果未初始化，则不执行操作
            }
            
            // 确保在UI线程上更新界面
            if (!_logTextBox.Dispatcher.CheckAccess())
            {
                _logTextBox.Dispatcher.Invoke(() => LogMessage(message));
                return;
            }

            _logTextBox.Text += $"[{DateTime.Now:HH:mm:ss}] {message}\n";
            
            // 自动滚动到底部
            (_logTextBox.Parent as ScrollViewer)?.ScrollToEnd();
        }
        
        /// <summary>
        /// 清空日志
        /// </summary>
        public void ClearLog()
        {
            if (_logTextBox == null)
            {
                return;
            }
            
            // 确保在UI线程上更新界面
            if (!_logTextBox.Dispatcher.CheckAccess())
            {
                _logTextBox.Dispatcher.Invoke(() => ClearLog());
                return;
            }
            
            _logTextBox.Text = string.Empty;
        }
    }
}
