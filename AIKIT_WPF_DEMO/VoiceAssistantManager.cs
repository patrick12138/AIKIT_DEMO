using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;

namespace AikitWpfDemo
{
    /// <summary>
    /// 语音助手管理器 - 封装语音循环逻辑和UI状态控制
    /// </summary>
    public class VoiceAssistantManager
    {
        // 单例实例
        private static VoiceAssistantManager _instance;
        
        // 同步锁
        private static readonly object _lock = new object();
        
        // 语音助手状态枚举
        public enum VoiceAssistantState
        {
            Idle = 0,               // 空闲状态
            WakeupListening = 1,    // 唤醒词监听中
            CommandListening = 2,   // 命令词监听中
            Processing = 3          // 命令处理中
        }
        
        // 弹窗UI实例
        private CortanaLikePopup _popup;
        
        // 状态监控定时器
        private DispatcherTimer _stateMonitorTimer;
        
        // 上一次检测到的状态
        private VoiceAssistantState _lastState = VoiceAssistantState.Idle;
        
        // 是否正在运行
        private bool _isRunning = false;
        
        // 状态变化事件
        public event EventHandler<VoiceAssistantStateEventArgs> StateChanged;
        
        // 命令识别结果事件
        public event EventHandler<CommandRecognizedEventArgs> CommandRecognized;
        
        /// <summary>
        /// 获取单例实例
        /// </summary>
        public static VoiceAssistantManager Instance
        {
            get
            {
                if (_instance == null)
                {
                    lock (_lock)
                    {
                        if (_instance == null)
                        {
                            _instance = new VoiceAssistantManager();
                        }
                    }
                }
                return _instance;
            }
        }
        
        /// <summary>
        /// 私有构造函数
        /// </summary>
        private VoiceAssistantManager()
        {
            // 初始化弹窗
            _popup = new CortanaLikePopup();
            
            // 初始化状态监控定时器
            _stateMonitorTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(100) // 每100ms检查一次状态
            };
            _stateMonitorTimer.Tick += StateMonitorTimer_Tick;
        }
        
        /// <summary>
        /// 启动语音助手循环
        /// </summary>
        public bool Start()
        {
            if (_isRunning)
            {
                return false; // 已经在运行
            }
            
            // 启动DLL中的语音助手循环
            int result = NativeMethods.StartVoiceAssistantLoop();
            if (result != 0)
            {
                return false; // 启动失败
            }
            
            // 设置运行标志
            _isRunning = true;
            
            // 启动状态监控
            _stateMonitorTimer.Start();
            
            // 触发状态变化事件
            OnStateChanged(VoiceAssistantState.WakeupListening);
            
            return true;
        }
        
        /// <summary>
        /// 停止语音助手循环
        /// </summary>
        public void Stop()
        {
            if (!_isRunning)
            {
                return; // 未在运行
            }
            
            // 停止状态监控
            _stateMonitorTimer.Stop();
            
            // 停止DLL中的语音助手循环
            NativeMethods.StopVoiceAssistantLoop();
            
            // 重置运行标志
            _isRunning = false;
            
            // 隐藏弹窗
            HidePopup();
            
            // 触发状态变化事件
            OnStateChanged(VoiceAssistantState.Idle);
        }
        
        /// <summary>
        /// 获取当前语音助手状态
        /// </summary>
        public VoiceAssistantState GetCurrentState()
        {
            if (!_isRunning)
            {
                return VoiceAssistantState.Idle;
            }
            
            int state = NativeMethods.GetVoiceAssistantState();
            return (VoiceAssistantState)state;
        }
        
        /// <summary>
        /// 显示弹窗
        /// </summary>
        private void ShowPopup(string text = "正在聆听...")
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                try
                {
                    // 确保弹窗有效
                    if (_popup == null || !_popup.IsLoaded)
                    {
                        _popup = new CortanaLikePopup();
                    }
                    
                    // 显示弹窗
                    if (!_popup.IsVisible)
                    {
                        _popup.Show();
                        _popup.Activate();
                    }
                    
                    // 更新文本
                    _popup.UpdateText(text);
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"显示弹窗异常: {ex.Message}");
                    // 重置弹窗
                    _popup = new CortanaLikePopup();
                }
            });
        }
        
        /// <summary>
        /// 隐藏弹窗
        /// </summary>
        private void HidePopup()
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                try
                {
                    if (_popup != null && _popup.IsVisible)
                    {
                        _popup.Hide();
                    }
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"隐藏弹窗异常: {ex.Message}");
                }
            });
        }
        
        /// <summary>
        /// 状态监控定时器事件处理
        /// </summary>
        private void StateMonitorTimer_Tick(object sender, EventArgs e)
        {
            // 获取当前状态
            VoiceAssistantState currentState = GetCurrentState();
            
            // 检查状态是否变化
            if (currentState != _lastState)
            {
                // 状态变化，处理UI更新
                HandleStateChange(currentState);
                
                // 更新上一次状态
                _lastState = currentState;
                
                // 触发状态变化事件
                OnStateChanged(currentState);
            }
              // 获取最新命令结果
            string commandResult = NativeMethodsExtensions.GetLastCommandResultString();
            if (!string.IsNullOrEmpty(commandResult) && 
                currentState == VoiceAssistantState.CommandListening && 
                commandResult.StartsWith("识别结果:"))
            {
                // 提取实际命令词
                string command = commandResult.Substring("识别结果: ".Length).Trim();
                
                // 更新弹窗显示
                ShowPopup($"命令: {command}");
                
                // 触发命令识别事件
                OnCommandRecognized(command, true);
            }
        }
        
        /// <summary>
        /// 处理状态变化
        /// </summary>
        private void HandleStateChange(VoiceAssistantState newState)
        {
            switch (newState)
            {
                case VoiceAssistantState.Idle:
                    // 空闲状态，隐藏弹窗
                    HidePopup();
                    break;
                    
                case VoiceAssistantState.WakeupListening:
                    // 唤醒词监听状态，隐藏弹窗
                    HidePopup();
                    break;
                    
                case VoiceAssistantState.CommandListening:
                    // 命令词识别状态，显示弹窗
                    ShowPopup("正在聆听命令...");
                    break;
                    
                case VoiceAssistantState.Processing:
                    // 命令处理状态，更新弹窗提示
                    ShowPopup("正在处理命令...");
                    break;
            }
        }
        
        /// <summary>
        /// 触发状态变化事件
        /// </summary>
        protected virtual void OnStateChanged(VoiceAssistantState newState)
        {
            StateChanged?.Invoke(this, new VoiceAssistantStateEventArgs(newState));
        }
        
        /// <summary>
        /// 触发命令识别事件
        /// </summary>
        protected virtual void OnCommandRecognized(string command, bool success)
        {
            CommandRecognized?.Invoke(this, new CommandRecognizedEventArgs(command, success));
        }
    }
    
    /// <summary>
    /// 语音助手状态事件参数
    /// </summary>
    public class VoiceAssistantStateEventArgs : EventArgs
    {
        public VoiceAssistantManager.VoiceAssistantState State { get; }
        
        public VoiceAssistantStateEventArgs(VoiceAssistantManager.VoiceAssistantState state)
        {
            State = state;
        }
    }
    
    /// <summary>
    /// 命令识别事件参数
    /// </summary>
    public class CommandRecognizedEventArgs : EventArgs
    {
        public string Command { get; }
        public bool Success { get; }
        
        public CommandRecognizedEventArgs(string command, bool success)
        {
            Command = command;
            Success = success;
        }
    }
    
    /// <summary>
    /// NativeMethods扩展方法
    /// </summary>
    public static class NativeMethodsExtensions
    {
        /// <summary>
        /// 获取语音助手最后命令结果
        /// </summary>
        public static string GetLastCommandResultString()
        {
            IntPtr ptr = NativeMethods.GetLastCommandResult();
            if (ptr != IntPtr.Zero)
            {
                // 计算字符串长度（遇到\0为止）
                int len = 0;
                while (System.Runtime.InteropServices.Marshal.ReadByte(ptr, len) != 0) len++;
                if (len > 0)
                {
                    byte[] buffer = new byte[len];
                    System.Runtime.InteropServices.Marshal.Copy(ptr, buffer, 0, len);
                    // 用UTF-8解码，防止中文乱码
                    string result = System.Text.Encoding.UTF8.GetString(buffer);
                    return result;
                }
            }
            return string.Empty;
        }
    }
}
