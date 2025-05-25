// VoiceInteractionManager.cs - 语音交互状态管理器
using System;
using System.Threading.Tasks;
using System.Windows.Threading;
using System.Media;
using System.IO;

namespace AikitWpfDemo
{
    public enum VoiceState
    {
        Idle,           // 待机状态（监听唤醒词）
        WakeupDetected, // 检测到唤醒词
        PlayingPrompt,  // 播放提示音
        ListeningCommand, // 监听命令词
        ProcessingResult, // 处理结果
        Timeout,        // 超时状态
        Error           // 错误状态
    }    public class VoiceInteractionManager
    {
        private VoiceState _currentState = VoiceState.Idle;
        private DispatcherTimer? _timeoutTimer;
        private DispatcherTimer? _statusCheckTimer;
        private PopupManager _popupManager;
        private SoundPlayer? _promptPlayer;
        
        // 超时设置（秒）
        private const int WAKEUP_TIMEOUT = 30;      // 唤醒词监听超时
        private const int COMMAND_TIMEOUT = 10;     // 命令词监听超时
        private const int SILENCE_TIMEOUT = 5;      // 无声音超时
          // 事件
        public event Action<string>? OnLogMessage;
        public event Action<string>? OnCommandDetected;
        public event Action<VoiceState>? OnStateChanged;
        
        public VoiceState CurrentState => _currentState;
        
        public VoiceInteractionManager(PopupManager popupManager)
        {
            _popupManager = popupManager;
            InitializeTimers();
            InitializeAudio();
        }
        
        private void InitializeTimers()
        {
            // 超时定时器
            _timeoutTimer = new DispatcherTimer();
            _timeoutTimer.Tick += OnTimeout;
            
            // 状态检查定时器（每100ms检查一次状态）
            _statusCheckTimer = new DispatcherTimer();
            _statusCheckTimer.Interval = TimeSpan.FromMilliseconds(100);
            _statusCheckTimer.Tick += CheckVoiceStatus;
        }
        
        private void InitializeAudio()
        {
            try
            {
                // 初始化提示音（可以是系统提示音或自定义音频文件）
                string promptPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "prompt.wav");
                if (File.Exists(promptPath))
                {
                    _promptPlayer = new SoundPlayer(promptPath);
                    _promptPlayer.Load();
                }
                else
                {
                    // 使用系统提示音
                    _promptPlayer = new SoundPlayer();
                }
            }
            catch (Exception ex)
            {
                LogMessage($"初始化提示音失败: {ex.Message}");
            }
        }
        
        // 开始语音交互循环
        public async Task StartInteractionLoop()
        {
            LogMessage("开始语音交互循环");
            await TransitionToState(VoiceState.Idle);
        }
          // 停止语音交互循环
        public async Task StopInteractionLoop()
        {
            LogMessage("停止语音交互循环");
            _timeoutTimer?.Stop();
            _statusCheckTimer?.Stop();
            
            // 停止所有识别
            try
            {
                NativeMethods.StopWakeupDetection();
                NativeMethods.StopEsrMicrophoneDetection();
            }
            catch (Exception ex)
            {
                LogMessage($"停止识别时出错: {ex.Message}");
            }
            
            await TransitionToState(VoiceState.Idle);
        }
        
        // 状态转换
        private async Task TransitionToState(VoiceState newState)
        {
            var oldState = _currentState;
            _currentState = newState;
              LogMessage($"状态转换: {oldState} -> {newState}");
            OnStateChanged?.Invoke(newState);
            
            // 停止所有定时器
            _timeoutTimer?.Stop();
            
            switch (newState)
            {
                case VoiceState.Idle:
                    await HandleIdleState();
                    break;
                    
                case VoiceState.WakeupDetected:
                    await HandleWakeupDetected();
                    break;
                    
                case VoiceState.PlayingPrompt:
                    await HandlePlayingPrompt();
                    break;
                    
                case VoiceState.ListeningCommand:
                    await HandleListeningCommand();
                    break;
                    
                case VoiceState.ProcessingResult:
                    await HandleProcessingResult();
                    break;
                    
                case VoiceState.Timeout:
                case VoiceState.Error:
                    await HandleTimeoutOrError();
                    break;
            }
        }
        
        // 处理待机状态（监听唤醒词）
        private async Task HandleIdleState()
        {
            LogMessage("进入待机状态，开始监听唤醒词...");
            
            // 隐藏弹窗
            _popupManager.HidePopup();
            
            // 重置唤醒状态
            NativeMethods.ResetWakeupStatus();
            
            try
            {
                // 启动唤醒词检测
                int ret = NativeMethods.StartWakeupDetection(50);
                if (ret != 0)
                {
                    LogMessage($"启动唤醒词检测失败: {ret}");
                    await Task.Delay(2000); // 等待2秒后重试
                    await TransitionToState(VoiceState.Idle);
                    return;                }
                
                // 开始状态检查
                _statusCheckTimer?.Start();
                
                // 设置超时（可选，如果不需要可以不设置）
                // _timeoutTimer.Interval = TimeSpan.FromSeconds(WAKEUP_TIMEOUT);
                // _timeoutTimer.Start();
                
            }
            catch (Exception ex)
            {
                LogMessage($"启动唤醒词检测异常: {ex.Message}");
                await TransitionToState(VoiceState.Error);
            }
        }
        
        // 处理检测到唤醒词
        private async Task HandleWakeupDetected()
        {
            LogMessage("检测到唤醒词！");
            
            // 停止唤醒词检测
            try
            {
                NativeMethods.StopWakeupDetection();
            }
            catch (Exception ex)
            {
                LogMessage($"停止唤醒词检测失败: {ex.Message}");
            }
            
            await TransitionToState(VoiceState.PlayingPrompt);
        }
        
        // 处理播放提示音
        private async Task HandlePlayingPrompt()
        {
            LogMessage("播放提示音...");
            
            try
            {
                // 播放提示音
                if (_promptPlayer != null)
                {
                    await Task.Run(() => _promptPlayer.Play());
                }
                else
                {
                    SystemSounds.Beep.Play();
                }
                
                // 等待提示音播放完成
                await Task.Delay(500);
                
                await TransitionToState(VoiceState.ListeningCommand);
            }
            catch (Exception ex)
            {
                LogMessage($"播放提示音失败: {ex.Message}");
                await TransitionToState(VoiceState.ListeningCommand);
            }
        }
        
        // 处理监听命令词
        private async Task HandleListeningCommand()
        {            LogMessage("开始监听命令词...");
            
            // 显示弹窗
            await _popupManager.ShowPopupWithAutoCloseAsync("正在监听...", 10000);
            
            try
            {
                // 启动命令词识别
                int ret = NativeMethods.StartEsrMicrophoneDetection();
                if (ret != 0)
                {
                    LogMessage($"启动命令词识别失败: {ret}");
                    await TransitionToState(VoiceState.Error);
                    return;                }
                
                // 设置命令词监听超时
                if (_timeoutTimer != null)
                {
                    _timeoutTimer.Interval = TimeSpan.FromSeconds(COMMAND_TIMEOUT);
                    _timeoutTimer.Start();
                }
                
            }
            catch (Exception ex)
            {
                LogMessage($"启动命令词识别异常: {ex.Message}");
                await TransitionToState(VoiceState.Error);
            }
        }
        
        // 处理结果
        private async Task HandleProcessingResult()
        {
            LogMessage("处理识别结果...");
            
            try
            {
                // 停止命令词识别
                NativeMethods.StopEsrMicrophoneDetection();
                  // 获取识别结果
                string result = NativeMethods.GetLastEsrResultStringResult();
                
                if (!string.IsNullOrEmpty(result))
                {
                    LogMessage($"识别到命令: {result}");
                      // 更新弹窗显示结果
                    await _popupManager.ShowPopupWithAutoCloseAsync($"识别结果: {result}", 3000);
                    
                    // 触发命令检测事件
                    OnCommandDetected?.Invoke(result);
                    
                    // 等待2秒显示结果
                    await Task.Delay(2000);
                }                else
                {
                    LogMessage("未识别到有效命令");
                    await _popupManager.ShowPopupWithAutoCloseAsync("未识别到命令", 1500);
                }
                
                // 返回待机状态
                await TransitionToState(VoiceState.Idle);
            }
            catch (Exception ex)
            {
                LogMessage($"处理结果异常: {ex.Message}");
                await TransitionToState(VoiceState.Error);
            }
        }
        
        // 处理超时或错误
        private async Task HandleTimeoutOrError()
        {
            LogMessage("处理超时或错误，返回待机状态");
            
            try
            {
                // 停止所有识别
                NativeMethods.StopWakeupDetection();
                NativeMethods.StopEsrMicrophoneDetection();
            }
            catch { }
              // 显示超时信息
            if (_currentState == VoiceState.Timeout)
            {
                await _popupManager.ShowPopupWithAutoCloseAsync("监听超时", 1500);
            }
            
            // 等待1秒后返回待机
            await Task.Delay(1000);
            await TransitionToState(VoiceState.Idle);
        }
          // 检查语音状态
        private async void CheckVoiceStatus(object? sender, EventArgs e)
        {
            try
            {
                switch (_currentState)
                {
                    case VoiceState.Idle:
                        // 检查是否检测到唤醒词
                        int wakeupStatus = NativeMethods.GetWakeupStatus();                        if (wakeupStatus == 1)
                        {
                            _statusCheckTimer?.Stop();
                            await TransitionToState(VoiceState.WakeupDetected);
                        }
                        break;
                          case VoiceState.ListeningCommand:
                        // 检查ESR状态
                        int esrStatus = NativeMethods.GetEsrStatus();
                        string esrResult = NativeMethods.GetLastEsrResultStringResult();
                          if (esrStatus == NativeMethods.ESR_STATUS_SUCCESS_INTERNAL && !string.IsNullOrEmpty(esrResult))
                        {
                            _statusCheckTimer?.Stop();
                            await TransitionToState(VoiceState.ProcessingResult);
                        }
                        else if (esrStatus == NativeMethods.ESR_STATUS_FAILED_INTERNAL)
                        {
                            _statusCheckTimer?.Stop();
                            await TransitionToState(VoiceState.Timeout);
                        }
                        break;
                }
            }
            catch (Exception ex)
            {
                LogMessage($"状态检查异常: {ex.Message}");
            }
        }
          // 超时处理
        private async void OnTimeout(object? sender, EventArgs e)
        {
            LogMessage($"在状态 {_currentState} 中发生超时");
            _statusCheckTimer?.Stop();
            await TransitionToState(VoiceState.Timeout);
        }
        
        // 手动触发命令识别结果处理（从外部调用）
        public async Task ProcessCommandResult()
        {
            if (_currentState == VoiceState.ListeningCommand)
            {
                await TransitionToState(VoiceState.ProcessingResult);
            }
        }
        
        // 日志消息
        private void LogMessage(string message)
        {
            OnLogMessage?.Invoke($"[VoiceManager] {message}");
        }
        
        // 释放资源
        public void Dispose()
        {
            _timeoutTimer?.Stop();
            _statusCheckTimer?.Stop();
            _promptPlayer?.Dispose();
        }
    }
}
