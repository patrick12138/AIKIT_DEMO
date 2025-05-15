using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Windows;
using System.Text;
using System.Diagnostics;

namespace AikitWpfDemo
{
    /// <summary>
    /// 命令处理器类，负责处理语音识别到的命令
    /// </summary>
    public class CommandProcessor
    {
        private static CommandProcessor _instance;
        private LogManager _logManager;
        private PopupManager _popupManager;
        
        // 命令处理字典
        private Dictionary<string, Func<Task>> _commandHandlers;
        
        private CommandProcessor()
        {
            _logManager = LogManager.Instance;
            _popupManager = PopupManager.Instance;
            
            // 初始化命令处理字典
            InitializeCommandHandlers();
        }
        
        /// <summary>
        /// 获取CommandProcessor的单例实例
        /// </summary>
        public static CommandProcessor Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = new CommandProcessor();
                }
                return _instance;
            }
        }
        
        /// <summary>
        /// 初始化命令处理字典
        /// </summary>
        private void InitializeCommandHandlers()
        {
            _commandHandlers = new Dictionary<string, Func<Task>>
            {
                // 定义常用命令的处理方法
                { "打开设置", OpenSettingsAsync },
                { "关闭程序", CloseApplicationAsync },
                { "显示时间", ShowCurrentTimeAsync },
                { "帮助信息", ShowHelpInfoAsync },
                { "当前状态", ShowCurrentStatusAsync }
                // 可以根据需要添加更多命令
            };
        }
        
        /// <summary>
        /// 处理识别到的命令
        /// </summary>
        /// <param name="command">识别到的命令文本</param>
        /// <returns>处理结果，成功为true，失败为false</returns>
        public async Task<bool> ProcessCommandAsync(string command)
        {
            if (string.IsNullOrEmpty(command))
            {
                _logManager.LogMessage("无效的空命令");
                return false;
            }
            
            _logManager.LogMessage($"正在处理命令: {command}");
            
            // 尝试精确匹配
            if (_commandHandlers.TryGetValue(command, out var handler))
            {
                await handler();
                return true;
            }
            
            // 如果没有精确匹配，尝试包含匹配
            foreach (var pair in _commandHandlers)
            {
                if (command.Contains(pair.Key))
                {
                    await pair.Value();
                    return true;
                }
            }
            
            // 未找到匹配的命令
            _logManager.LogMessage($"未找到匹配的命令处理程序: {command}");
            await _popupManager.ShowPopupWithAutoCloseAsync($"抱歉，我不明白", 3000);
            return false;
        }
        
        #region 命令处理方法
        
        /// <summary>
        /// 打开设置命令处理
        /// </summary>
        private async Task OpenSettingsAsync()
        {
            _logManager.LogMessage("执行打开设置命令");
            await _popupManager.ShowPopupWithAutoCloseAsync("正在打开设置...", 2000);
            
            // 这里可以实现打开设置的逻辑
            // 例如打开设置窗口
            
            // 模拟设置窗口打开
            await Task.Delay(500);
            MessageBox.Show("设置功能尚未实现", "提示", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        
        /// <summary>
        /// 关闭程序命令处理
        /// </summary>
        private async Task CloseApplicationAsync()
        {
            _logManager.LogMessage("执行关闭程序命令");
            await _popupManager.ShowPopupWithAutoCloseAsync("正在关闭程序...", 2000);
            
            // 确认是否关闭
            var result = MessageBox.Show("确定要关闭程序吗？", "确认", MessageBoxButton.YesNo, MessageBoxImage.Question);
            if (result == MessageBoxResult.Yes)
            {
                Application.Current.Shutdown();
            }
        }
        
        /// <summary>
        /// 显示当前时间命令处理
        /// </summary>
        private async Task ShowCurrentTimeAsync()
        {
            _logManager.LogMessage("执行显示时间命令");
            
            string timeString = DateTime.Now.ToString("yyyy年MM月dd日 HH:mm:ss");
            await _popupManager.ShowPopupWithAutoCloseAsync($"当前时间是：{timeString}", 5000);
        }
        
        /// <summary>
        /// 显示帮助信息命令处理
        /// </summary>
        private async Task ShowHelpInfoAsync()
        {
            _logManager.LogMessage("执行显示帮助信息命令");
            
            string helpInfo = "可用命令:\n- 打开设置\n- 关闭程序\n- 显示时间\n- 当前状态";
            await _popupManager.ShowPopupWithAutoCloseAsync(helpInfo, 8000);
        }
        
        /// <summary>
        /// 显示当前状态命令处理
        /// </summary>
        private async Task ShowCurrentStatusAsync()
        {
            _logManager.LogMessage("执行显示当前状态命令");
            
            // 获取当前系统状态信息
            var stateInfo = new StringBuilder();
            stateInfo.AppendLine("系统状态信息:");
            stateInfo.AppendLine($"- 内存使用: {GC.GetTotalMemory(false) / (1024 * 1024)}MB");
            stateInfo.AppendLine($"- 程序运行时间: {(DateTime.Now - Process.GetCurrentProcess().StartTime).ToString(@"hh\:mm\:ss")}");
            stateInfo.AppendLine($"- 当前线程数: {Process.GetCurrentProcess().Threads.Count}");
            
            await _popupManager.ShowPopupWithAutoCloseAsync(stateInfo.ToString(), 8000);
        }
        
        #endregion
    }
}
