using System.Windows;
using System.Windows.Threading;

namespace AikitWpfDemo
{
    public partial class MainWindow : Window
    {
        // 只保留必要字段
        private CancellationTokenSource _cts; 
        private bool _engineInitialized = false;
        private PopupManager _popupManager; // 替换原CortanaLikePopup
        private ResultMonitor _resultMonitor; // 替换原DispatcherTimer
        private bool _autoLoopRunning = false;
        private bool _recognitionCompleted = false;
        private string _lastHandledResult = string.Empty;        private HashSet<string> _validCommands;
        
        // 新增: 语音交互管理器
        private VoiceInteractionManager? _voiceManager;

        public MainWindow()
        {
            InitializeComponent();
            _cts = new CancellationTokenSource();
            // 初始化日志辅助类
            LogHelper.Init(TxtLog);
            // 初始化弹窗管理
            _popupManager = new PopupManager();
            // 初始化命令词集合
            _validCommands = CommandHelper.LoadValidCommands();            // 初始化识别结果监控
            _resultMonitor = new ResultMonitor(msg => LogHelper.LogMessage(msg));
            
            // 初始化语音交互管理器
            _voiceManager = new VoiceInteractionManager(_popupManager);
            _voiceManager.OnLogMessage += LogHelper.LogMessage;
            _voiceManager.OnCommandDetected += OnCommandDetected;
            // 启动自动语音循环流程（Loaded事件）
            //Loaded += async (s, e) => await StartAutoVoiceLoop();

            //Loaded += async (s, e) => await StartVoiceInteractionLoop();
        }        // 窗口关闭事件
        protected override void OnClosed(EventArgs e)
        {
            try
            {
                _cts?.Cancel();
                _resultMonitor?.Stop();
                _popupManager?.HidePopup();
                _voiceManager?.Dispose();
            }
            catch { }
            base.OnClosed(e);
        }

        // 命令检测事件处理器
        private void OnCommandDetected(string command)
        {
            LogHelper.LogMessage($"检测到命令: {command}");
            // 这里可以添加具体的命令处理逻辑
            switch (command.ToLower())
            {
                case "打开设置":
                    LogHelper.LogMessage("执行打开设置命令");
                    break;
                case "关闭程序":
                    LogHelper.LogMessage("执行关闭程序命令");
                    Application.Current.Shutdown();
                    break;
                default:
                    LogHelper.LogMessage($"未知命令: {command}");
                    break;
            }
        }

        // 开始语音交互循环按钮
        private async void BtnStartVoiceLoop_Click(object sender, RoutedEventArgs e)
        {
            if (_voiceManager == null) return;
            
            try
            {
                LogHelper.LogMessage("开始完整语音交互循环...");
                await _voiceManager.StartInteractionLoop();
            }
            catch (Exception ex)
            {
                LogHelper.LogMessage($"启动语音交互循环失败: {ex.Message}");
                MessageBox.Show($"启动语音交互循环失败: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        // 停止语音交互循环按钮
        private async void BtnStopVoiceLoop_Click(object sender, RoutedEventArgs e)
        {
            if (_voiceManager == null) return;
            
            try
            {
                LogHelper.LogMessage("停止语音交互循环...");
                await _voiceManager.StopInteractionLoop();
            }
            catch (Exception ex)
            {
                LogHelper.LogMessage($"停止语音交互循环失败: {ex.Message}");
            }
        }        // 唤醒测试按钮
        private DispatcherTimer? _wakeupMonitorTimer; // 新增定时器字段

        private void BtnStartWakeup_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                TxtLog.Text = string.Empty;
                LogHelper.LogMessage("开始唤醒测试...");
                NativeMethods.ResetWakeupStatus();
                BtnStartWakeup.IsEnabled = false;
                int result = NativeMethods.StartWakeupDetection(100);
                string detailedResult = NativeMethods.GetLastResultString();
                LogHelper.LogMessage($"唤醒测试启动结果: {detailedResult}");

                // 启动定时器实时监控唤醒状态
                if (_wakeupMonitorTimer == null)
                {
                    _wakeupMonitorTimer = new DispatcherTimer();
                    _wakeupMonitorTimer.Interval = TimeSpan.FromMilliseconds(100);
                    _wakeupMonitorTimer.Tick += (s, args) =>
                    {
                        if (NativeMethods.GetWakeupStatus() == 1)
                        {
                            string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                            LogHelper.LogMessage($"检测到唤醒词: {wakeupInfo}");
                            _ = _popupManager.ShowPopupWithAutoCloseAsync("你好，请问你需要做什么操作？", 3000);
                            NativeMethods.ResetWakeupStatus();
                            LogHelper.LogMessage("已启动实时语音识别，等待用户命令...");
                        }
                    };
                }
                _wakeupMonitorTimer.Start();

                if (result == 0)
                {
                    LogHelper.LogMessage("唤醒测试启动成功！");
                }
                else
                {
                    LogHelper.LogMessage($"唤醒测试启动失败，错误码: {result}");
                }
                BtnStartWakeup.IsEnabled = true;
            }
            catch (Exception ex)
            {
                LogHelper.LogMessage($"启动唤醒测试时发生异常: {ex.Message}");
                MessageBox.Show($"启动唤醒测试时发生异常: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                BtnStartWakeup.IsEnabled = true;
                _resultMonitor?.Stop();
            }
        }
          // 命令词识别按钮
        private void BtnStartEsr_Click(object sender, RoutedEventArgs e)
        {
            try
            {                // 启动识别结果监控定时器
                if (_resultMonitor != null && !_resultMonitor.IsEnabled)
                {
                    _resultMonitor.Start();
                    LogHelper.LogMessage("实时识别结果监控已启动 (全局)");
                }

                BtnRunFullTest.IsEnabled = false;
                LogHelper.LogMessage("ESR麦克风检测测试开始...");

                int startResult = NativeMethods.StartEsrMicrophoneDetection();
                string startDetailedResult = NativeMethods.GetLastResultString(); // Result of the Start command itself
                LogHelper.LogMessage($"StartEsrMicrophoneDetection 命令结果: {startDetailedResult} (Code: {startResult})");

                if (startResult == 0) // 0 means success for StartEsrMicrophoneDetection
                {
                    LogHelper.LogMessage("麦克风检测已成功启动。现在持续监听命令词识别结果...");

                    // 创建一个定时器来持续监听ESR状态
                    DispatcherTimer esrMonitorTimer = new DispatcherTimer();
                    esrMonitorTimer.Interval = TimeSpan.FromMilliseconds(250); // 每250ms检查一次
                    esrMonitorTimer.Tick += (s, args) =>
                    {
                        int currentEsrStatus = NativeMethods.GetEsrStatus();

                        if (currentEsrStatus == NativeMethods.ESR_STATUS_SUCCESS_INTERNAL)
                        {
                            string finalEsrResult = NativeMethods.GetEsrFinalDisplayResult();
                            LogHelper.LogMessage($"ESR成功: {finalEsrResult}");
                        }
                        else if (currentEsrStatus == NativeMethods.ESR_STATUS_FAILED_INTERNAL ||
                                currentEsrStatus == NativeMethods.ESR_STATUS_NO_MATCH_INTERNAL)
                        {
                            string finalEsrResult = NativeMethods.GetEsrFinalDisplayResult();
                            LogHelper.LogMessage($"ESR结束 (失败/无匹配): {finalEsrResult} (状态: {currentEsrStatus})");
                        }                        // 可以在这里获取PGS结果
                        string pgsResult = NativeMethods.GetLatestPgsResult()?.Trim() ?? string.Empty;
                        if (!string.IsNullOrEmpty(pgsResult))
                        {
                            LogHelper.LogMessage($"[实时] PGS: {pgsResult}");
                        }
                    };

                    esrMonitorTimer.Start();
                    LogHelper.LogMessage("ESR持续监听已启动");
                }
                else
                {
                    LogHelper.LogMessage($"启动麦克风检测失败。错误码: {startResult}");
                }
            }
            catch (Exception ex)
            {
                LogHelper.LogMessage($"ESR测试执行时发生异常: {ex.Message}");
                MessageBox.Show($"ESR测试执行时发生异常: {ex.Message}", "异常", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                BtnRunFullTest.IsEnabled = true;
                LogHelper.LogMessage("ESR测试按钮已重新启用。");
            }
        }

        // 自动命令词循环流程
        private async Task StartAutoVoiceLoop()
        {
            if (_autoLoopRunning)
            {
                LogHelper.LogMessage("命令词识别自动循环已在运行中。");
                return;
            }
            _autoLoopRunning = true;
            LogHelper.LogMessage("命令词识别自动循环已启动...");
            while (_autoLoopRunning)
            {
                try
                {
                    _popupManager.HidePopup();
                    LogHelper.LogMessage("尝试启动ESR麦克风进行命令词识别...");
                    int esrStartResult = NativeMethods.StartEsrMicrophone();
                    string esrStartDetails = NativeMethods.GetLastResultString();
                    LogHelper.LogMessage($"ESR麦克风启动结果: {esrStartResult}, 详情: {esrStartDetails}");
                    if (esrStartResult != 0)
                    {
                        LogHelper.LogMessage($"启动ESR麦克风失败 (错误码: {esrStartResult}). 1秒后重试...");
                        _engineInitialized = false;
                        _popupManager.HidePopup();
                        if (_autoLoopRunning) await Task.Delay(1000, _cts.Token);
                        continue;
                    }                    _engineInitialized = true;
                    if (_resultMonitor != null && !_resultMonitor.IsEnabled)
                    {
                        _resultMonitor.Start();
                        LogHelper.LogMessage("识别结果监控定时器已启动。");
                    }
                    var commandTimeout = TimeSpan.FromSeconds(2);
                    bool pgsMatchedThisTurn = false;
                    string currentTurnLastPgsText = string.Empty;
                    var timeoutTime = DateTime.Now + commandTimeout;                    while (_autoLoopRunning && DateTime.Now < timeoutTime && !pgsMatchedThisTurn)
                    {
                        string currentPgsRaw = NativeMethods.GetLatestPgsResult()?.Trim() ?? string.Empty;
                        if (!string.IsNullOrEmpty(currentPgsRaw) && currentPgsRaw != currentTurnLastPgsText)
                        {
                            currentTurnLastPgsText = currentPgsRaw;
                            string pgsTextContent = CommandHelper.ParsePgsText(currentPgsRaw);
                            if (!string.IsNullOrEmpty(pgsTextContent) && pgsTextContent != _lastHandledResult)
                            {
                                LogHelper.LogMessage($"[DEBUG] 当前PGS内容: '{pgsTextContent}'");
                                if (_validCommands.Contains(pgsTextContent))
                                {
                                    LogHelper.LogMessage($"[DEBUG] PGS完全匹配命令词: '{pgsTextContent}'，弹窗");
                                    _recognitionCompleted = false;
                                    await _popupManager.ShowPopupWithAutoCloseAsync(pgsTextContent);
                                    _lastHandledResult = pgsTextContent;
                                    pgsMatchedThisTurn = true;
                                }
                            }
                        }
                        await Task.Delay(100, _cts.Token);
                    }
                    if (_autoLoopRunning && !pgsMatchedThisTurn)
                    {
                        LogHelper.LogMessage("本轮命令识别超时或未获得有效PGS匹配。");
                        _popupManager.HidePopup();
                    }
                    LogHelper.LogMessage("准备停止当前ESR会话...");
                    if (_engineInitialized)
                    {
                        LogHelper.LogMessage("已调用停止ESR麦克风。");
                    }
                    _engineInitialized = false;
                    if (_autoLoopRunning)
                    {
                        LogHelper.LogMessage("本轮命令识别结束，1秒后开始下一轮...");
                        await Task.Delay(1000, _cts.Token);
                    }
                }
                catch (TaskCanceledException)
                {
                    LogHelper.LogMessage("自动语音循环任务被取消。");
                    _autoLoopRunning = false;
                }
                catch (Exception ex)
                {
                    LogHelper.LogMessage($"命令词识别主循环发生严重异常: {ex.Message}");
                    _engineInitialized = false;
                    _resultMonitor?.Stop();
                    _popupManager.HidePopup();
                    if (_autoLoopRunning)
                    {
                        LogHelper.LogMessage("异常后等待1秒重试...");
                        try { await Task.Delay(1000, _cts.Token); } catch (TaskCanceledException) { _autoLoopRunning = false; }
                    }
                }
            }
            LogHelper.LogMessage("命令词识别自动循环正在停止...");
            _resultMonitor?.Stop();
            if (_engineInitialized)
            {
                LogHelper.LogMessage("自动循环结束，已最后调用停止ESR麦克风。");
                _engineInitialized = false;
            }
            await Dispatcher.InvokeAsync(() => _popupManager.HidePopup());
            LogHelper.LogMessage("命令词识别自动循环已完全停止。");
        }
    }
}