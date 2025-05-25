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
    }    
    
    public class VoiceInteractionManager
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
        }        // 停止语音交互循环
        public async Task StopInteractionLoop()
        {
            LogMessage("停止语音交互循环");
            _timeoutTimer?.Stop();
            _statusCheckTimer?.Stop();
            
            // 停止统一语音交互
            try
            {
                NativeMethods.StopUnifiedVoiceInteraction();
            }
            catch (Exception ex)
            {
                LogMessage($"停止统一语音交互时出错: {ex.Message}");
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
        }        // 处理待机状态（监听唤醒词）
        private async Task HandleIdleState()
        {
            LogMessage("进入待机状态，等待唤醒词检测...");
            
            // 隐藏弹窗
            _popupManager.HidePopup();
            
            // 重置唤醒状态
            NativeMethods.ResetWakeupStatus();
            
            try
            {
                // 检查统一语音交互是否已经在运行
                int isRunning = NativeMethods.IsUnifiedVoiceInteractionRunning();
                if (isRunning != 1)
                {
                    LogMessage("统一语音交互未运行，请先点击'开始语音交互'按钮启动");
                    return;
                }
                
                LogMessage("统一语音交互已在运行，开始状态监控");
                
                // 开始状态检查
                _statusCheckTimer?.Start();
                
            }
            catch (Exception ex)
            {
                LogMessage($"处理待机状态异常: {ex.Message}");
                await TransitionToState(VoiceState.Error);
            }
        }
          // 处理检测到唤醒词
        private async Task HandleWakeupDetected()
        {
            LogMessage("检测到唤醒词！统一语音交互系统已自动处理");
            
            // 注意：使用统一语音交互系统时，系统会自动处理状态转换
            // 不需要手动停止唤醒检测，系统会自动进入命令词识别状态
            
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
                // 注意：使用统一语音交互系统时，不需要手动启动ESR检测
                // 统一语音交互系统会自动处理从唤醒词到命令词的转换
                LogMessage("统一语音交互系统已自动启动命令词识别，WPF只负责状态监控");
                
                // 设置命令词监听超时（作为备用机制）
                if (_timeoutTimer != null)
                {
                    _timeoutTimer.Interval = TimeSpan.FromSeconds(COMMAND_TIMEOUT);
                    _timeoutTimer.Start();
                }
                
            }
            catch (Exception ex)
            {
                LogMessage($"监听命令词状态设置异常: {ex.Message}");
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
        }          // 处理超时或错误
        private async Task HandleTimeoutOrError()
        {
            LogMessage("处理超时或错误，统一语音交互系统会自动恢复");
            
            // 注意：不强制停止统一语音交互系统，让它自然处理超时和恢复
            // 统一语音交互系统具有自动恢复机制
            
            // 显示超时信息
            if (_currentState == VoiceState.Timeout)
            {
                await _popupManager.ShowPopupWithAutoCloseAsync("监听超时，正在重新启动...", 2000);
                LogMessage("ESR识别超时，统一语音交互系统将自动返回唤醒监听状态");
            }
            else
            {
                await _popupManager.ShowPopupWithAutoCloseAsync("发生错误，正在恢复...", 2000);
                LogMessage("发生错误，统一语音交互系统将自动恢复");
            }
            
            // 短暂等待让用户看到提示信息
            await Task.Delay(1500);
            
            // 返回待机状态，开始下一轮监控
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
