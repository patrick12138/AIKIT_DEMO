using System;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;

namespace AikitWpfDemo
{
    /// <summary>
    /// 结果监控器，负责监控和处理语音识别结果
    /// </summary>    public class ResultMonitor
    public class ResultMonitor
    {
        private static ResultMonitor _instance;
        private DispatcherTimer _resultMonitorTimer;
        private VoiceAssistantManager _voiceManager;
        private LogManager _logManager;
        private PopupManager _popupManager;
        private CommandProcessor _commandProcessor;
        
        // 缓存各种类型的结果
        private string _lastPgsResult = string.Empty;
        private string _lastHtkResult = string.Empty;
        private string _lastPlainResult = string.Empty;
        private string _lastVadResult = string.Empty;
        private string _lastReadableResult = string.Empty;
        
        // 结果合并展示标志
        private bool _mergeResults = true; // 是否将所有结果合并在一起显示
        
        // 唤醒监听活跃状态标志
        private bool _isWakeupActive = false;
        
        private ResultMonitor()
        {
            // 初始化识别结果监控定时器
            _resultMonitorTimer = new DispatcherTimer();
            _resultMonitorTimer.Interval = TimeSpan.FromMilliseconds(50); // 每50ms检查一次，保证实时性
            _resultMonitorTimer.Tick += ResultMonitorTimer_Tick;
              // 获取其他管理器实例
            _voiceManager = VoiceAssistantManager.Instance;
            _logManager = LogManager.Instance;
            _popupManager = PopupManager.Instance;
            _commandProcessor = CommandProcessor.Instance;
        }
        
        /// <summary>
        /// 获取ResultMonitor的单例实例
        /// </summary>
        public static ResultMonitor Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = new ResultMonitor();
                }
                return _instance;
            }
        }
        
        /// <summary>
        /// 启动结果监控
        /// </summary>
        public void Start()
        {
            // 清空所有结果缓存
            _lastPgsResult = string.Empty;
            _lastHtkResult = string.Empty;
            _lastPlainResult = string.Empty;
            _lastVadResult = string.Empty;
            _lastReadableResult = string.Empty;
            
            // 重置标志
            _isWakeupActive = false;
            
            // 启动监控定时器
            if (!_resultMonitorTimer.IsEnabled)
            {
                _resultMonitorTimer.Start();
                _logManager.LogMessage("已启动语音交互监控");
            }
        }
        
        /// <summary>
        /// 停止结果监控
        /// </summary>
        public void Stop()
        {
            if (_resultMonitorTimer != null && _resultMonitorTimer.IsEnabled)
            {
                _resultMonitorTimer.Stop();
                _logManager.LogMessage("已停止语音交互监控");
            }
        }
        
        /// <summary>
        /// 定时器事件处理函数
        /// </summary>
        private void ResultMonitorTimer_Tick(object sender, EventArgs e)
        {
            try
            {
                // 获取当前语音助手状态
                var currentState = _voiceManager.GetCurrentState();
                
                // 状态变化处理已由VoiceManager处理，此处主要检查结果
                switch (currentState)
                {
                    case VoiceAssistantManager.VoiceAssistantState.WakeupListening:
                        // 处理唤醒状态检查
                        int wakeupStatus = NativeMethods.GetWakeupStatus();
                        if (wakeupStatus == 1)
                        {
                            // 获取唤醒词信息
                            string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                            _logManager.LogMessage($"检测到唤醒词: {wakeupInfo}");
                            
                            // 显示弹窗
                            _popupManager.ManagePopupDisplay("你好，请问你需要做什么操作？", true);
                            
                            // 重置唤醒状态，避免重复响应
                            NativeMethods.ResetWakeupStatus();
                            
                            // 切换到命令词识别状态
                            _voiceManager.TransitionToCommandListening();
                            
                            // 启动命令词识别
                            _logManager.LogMessage("启动命令词识别...");
                            NativeMethods.StartEsrMicrophone();
                        }
                        else if (!_isWakeupActive)
                        {
                            // 如果唤醒监听不是活跃状态，则启动唤醒监听
                            _isWakeupActive = true;
                            _logManager.LogMessage("启动唤醒监听...");
                            Task.Run(() => {
                                try {
                                    NativeMethods.StartWakeup();
                                    _isWakeupActive = false; // 唤醒监听函数返回后重置标志
                                }
                                catch (Exception ex) {
                                    _logManager.LogMessage($"唤醒监听异常: {ex.Message}");
                                    _isWakeupActive = false;
                                }
                            });
                        }
                        break;
                        
                    case VoiceAssistantManager.VoiceAssistantState.CommandListening:
                        // 检查并处理识别结果
                        CheckAndProcessNewResults();
                          // 检查ESR状态，判断命令词识别是否完成
                        int esrStatus = NativeMethods.GetEsrStatus();
                        if (esrStatus == 1)
                        {
                            string esrResult = NativeMethods.GetEsrKeywordResultString();
                            _logManager.LogMessage($"命令词识别完成: {esrResult}");
                            
                            // 显示识别结果
                            _popupManager.ManagePopupDisplay($"识别结果: {esrResult}", true);
                            
                            // 处理识别到的命令
                            _ = ProcessRecognizedCommandAsync(esrResult);
                            
                            // 重置ESR状态
                            NativeMethods.ResetEsrStatus();
                        }
                        break;
                }
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"识别结果监控异常: {ex.Message}");
                
                // 尝试恢复到唤醒监听状态
                try
                {
                    NativeMethods.ResetWakeupStatus();
                    NativeMethods.ResetEsrStatus();
                    NativeMethods.StartWakeup();
                }
                catch
                {
                    // 忽略恢复过程中的异常
                }
            }
        }
        
        /// <summary>
        /// 检查和处理所有类型的新结果
        /// </summary>
        private void CheckAndProcessNewResults()
        {
            bool hasAnyNewResult = false;
            StringBuilder resultBuilder = null;

            if (_mergeResults)
            {
                resultBuilder = new StringBuilder();
            }

            // 检查pgs格式结果
            string currentResult1 = NativeMethods.GetLatestPgsResult();
            if (!string.IsNullOrEmpty(currentResult1) && currentResult1 != _lastPgsResult)
            {
                _lastPgsResult = currentResult1;
                hasAnyNewResult = true;

                if (_mergeResults)
                {
                    resultBuilder.AppendLine(currentResult1);
                }
                else
                {
                    _logManager.LogMessage(currentResult1);
                }

                // 更新弹窗显示PGS实时结果（取最后一行，通常是最新内容）
                string[] lines = currentResult1.Split('\n');
                if (lines.Length > 0)
                {
                    string lastLine = lines[lines.Length - 1];
                    if (lastLine.StartsWith("pgs"))
                    {
                        _ = _popupManager.ShowPopupWithAutoCloseAsync(lastLine.Substring(4)); // 去掉"pgs："前缀
                    }
                    else
                    {
                        _ = _popupManager.ShowPopupWithAutoCloseAsync(lastLine);
                    }
                }
            }

            string currentResult = NativeMethods.GetPlainResultString();
            if (!string.IsNullOrEmpty(currentResult) && currentResult != _lastPlainResult)
            {
                _lastPlainResult = currentResult;
                hasAnyNewResult = true;

                if (_mergeResults)
                {
                    resultBuilder.AppendLine(currentResult);
                }
                else
                {
                    _logManager.LogMessage(currentResult);
                }
            }

            string currentResult2 = NativeMethods.GetReadableResultString();
            if (!string.IsNullOrEmpty(currentResult2) && currentResult2 != _lastReadableResult)
            {
                _lastReadableResult = currentResult2;
                hasAnyNewResult = true;

                if (_mergeResults)
                {
                    resultBuilder.AppendLine(currentResult2);
                }
                else
                {
                    _logManager.LogMessage(currentResult2);
                }
            }
            
            // 如果有新结果且设置为合并显示，则一次性输出所有结果
            if (hasAnyNewResult && _mergeResults && resultBuilder != null && resultBuilder.Length > 0)
            {
                _logManager.LogMessage(resultBuilder.ToString().TrimEnd());
            }
            
            // 确保弹窗可见（如果有新结果）
            if (hasAnyNewResult && _voiceManager.GetCurrentState() == VoiceAssistantManager.VoiceAssistantState.CommandListening)
            {
                // 更新弹窗内容（使用最后获取到的结果）
                string displayText = !string.IsNullOrEmpty(_lastPgsResult) ? _lastPgsResult : 
                                    !string.IsNullOrEmpty(_lastReadableResult) ? _lastReadableResult : 
                                    !string.IsNullOrEmpty(_lastPlainResult) ? _lastPlainResult : "正在聆听...";
                
                // 如果结果太长，只显示最后一部分
                if (displayText.Length > 100)
                {
                    displayText = "..." + displayText.Substring(displayText.Length - 100);
                }
                
                // 显示弹窗
                _popupManager.ManagePopupDisplay(displayText, true);
            }
        }
        
        /// <summary>
        /// 设置结果合并展示标志
        /// </summary>
        public void SetMergeResults(bool merge)
        {
            _mergeResults = merge;
        }

        /// <summary>
        /// 处理识别到的命令
        /// </summary>
        /// <param name="command">识别到的命令文本</param>
        private async Task ProcessRecognizedCommandAsync(string command)
        {
            try
            {
                // 使用命令处理器处理命令
                bool success = await _commandProcessor.ProcessCommandAsync(command);
                
                // 延迟一段时间后关闭弹窗并切换回唤醒状态
                await Task.Delay(success ? 5000 : 3000);
                Application.Current.Dispatcher.Invoke(() => _popupManager.ManagePopupDisplay("", false));
                
                // 切换回唤醒监听状态
                _voiceManager.TransitionToWakeupListening();
                _isWakeupActive = false; // 确保可以重新启动唤醒监听
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"处理命令异常: {ex.Message}");
                
                // 确保即使出错，也能恢复到唤醒监听状态
                await Task.Delay(3000);
                Application.Current.Dispatcher.Invoke(() => _popupManager.ManagePopupDisplay("", false));
                _voiceManager.TransitionToWakeupListening();
                _isWakeupActive = false;
            }
        }
    }
}
