using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using WinForms = System.Windows.Forms;
using Microsoft.Win32;
using System.IO;
using System.Runtime.InteropServices; // 添加对Marshal类的引用
using AikitWpfDemo; // 引入辅助类

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
        private string _lastHandledResult = string.Empty;
        private HashSet<string> _validCommands;

        public MainWindow()
        {
            InitializeComponent();
            _cts = new CancellationTokenSource();
            // 初始化日志辅助类
            LogHelper.Init(TxtLog);
            // 初始化弹窗管理
            _popupManager = new PopupManager();
            // 初始化命令词集合
            _validCommands = CommandHelper.LoadValidCommands();
            // 初始化识别结果监控
            _resultMonitor = new ResultMonitor(msg => LogHelper.LogMessage(msg));
            // 启动自动语音循环流程（Loaded事件）
            //Loaded += async (s, e) => await StartAutoVoiceLoop();

            Loaded += async (s, e) => await StartVoiceInteractionLoop();
        }

        // 窗口关闭事件
        protected override void OnClosed(EventArgs e)
        {
            try
            {
                _cts?.Cancel();
                _resultMonitor?.Stop();
                _popupManager?.HidePopup();
            }
            catch { }
            base.OnClosed(e);
        }

        // 唤醒测试按钮
        private void BtnStartWakeup_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                TxtLog.Text = string.Empty;
                LogHelper.LogMessage("开始唤醒测试...");
                NativeMethods.ResetWakeupStatus();
                BtnStartWakeup.IsEnabled = false;
                int result = NativeMethods.StartWakeup();
                string detailedResult = NativeMethods.GetLastResultString();
                LogHelper.LogMessage($"唤醒测试启动结果: {detailedResult}");
                if (NativeMethods.GetWakeupStatus() == 1)
                {
                    string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                    LogHelper.LogMessage($"检测到唤醒词: {wakeupInfo}");
                    _popupManager.ShowPopupWithAutoCloseAsync("你好，请问你需要做什么操作？");
                    NativeMethods.ResetWakeupStatus();
                    LogHelper.LogMessage("已启动实时语音识别，等待用户命令...");
                }
                else
                {
                    LogHelper.LogMessage("未检测到唤醒词，弹窗未显示");
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
            {
                // 清空各种结果缓存
                //_lastPgsResult = string.Empty;
                //_lastHtkResult = string.Empty;
                //_lastPlainResult = string.Empty;
                //_lastVadResult = string.Empty;
                //_lastReadableResult = string.Empty;

                // 启动识别结果监控定时器
                if (!_resultMonitor.IsEnabled)
                {
                    _resultMonitor.Start();
                    LogHelper.LogMessage("已启动实时识别结果监控");
                }

                // 禁用按钮，防止重复点击
                BtnRunFullTest.IsEnabled = false;

                // 执行测试
                int result = NativeMethods.StartEsrMicrophone();

                // 获取并记录详细结果信息
                string detailedResult = NativeMethods.GetLastResultString();
                LogHelper.LogMessage($"测试启动结果: {detailedResult}");

                if (result == 0)
                {
                    LogHelper.LogMessage("测试完成，执行成功！");
                }
                else
                {
                    LogHelper.LogMessage($"测试失败，错误码: {result}");
                }

                // 重新启用按钮
                BtnRunFullTest.IsEnabled = true;
            }
            catch (Exception ex)
            {
                LogHelper.LogMessage($"启动测试线程时发生异常: {ex.Message}");
                MessageBox.Show($"启动测试时发生异常: {ex.Message}", "异常", MessageBoxButton.OK, MessageBoxImage.Error);
                BtnRunFullTest.IsEnabled = true;
                _resultMonitor?.Stop();
            }
        }

        // 自动语音循环流程
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
                    }
                    _engineInitialized = true;
                    if (!_resultMonitor.IsEnabled)
                    {
                        _resultMonitor.Start();
                        LogHelper.LogMessage("识别结果监控定时器已启动。");
                    }
                    var commandTimeout = TimeSpan.FromSeconds(2);
                    bool pgsMatchedThisTurn = false;
                    string currentTurnLastPgsText = string.Empty;
                    var timeoutTime = DateTime.Now + commandTimeout;
                    while (_autoLoopRunning && DateTime.Now < timeoutTime && !pgsMatchedThisTurn)
                    {
                        string currentPgsRaw = NativeMethods.GetLatestPgsResult()?.Trim();
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

        // 启动语音交互循环，结合唤醒和命令识别
        private async Task StartVoiceInteractionLoop()
        {
            if (_autoLoopRunning)
            {
                LogHelper.LogMessage("语音交互循环已在运行中。");
                return;
            }
            _autoLoopRunning = true;
            LogHelper.LogMessage("语音交互循环已启动...");

            while (_autoLoopRunning)
            {
                try
                {
                    // 启动唤醒检测
                    LogHelper.LogMessage("启动语音唤醒检测...");
                    NativeMethods.ResetWakeupStatus();
                    int wakeupResult = NativeMethods.StartWakeup(); // 使用合适的阈值
                    string wakeupDetails = NativeMethods.GetLastResultString();
                    LogHelper.LogMessage($"语音唤醒启动结果: {wakeupResult}, 详情: {wakeupDetails}");

                    if (wakeupResult != 0)
                    {
                        LogHelper.LogMessage($"启动语音唤醒失败 (错误码: {wakeupResult}). 1秒后重试...");
                        _engineInitialized = false;
                        _popupManager.HidePopup();
                        if (_autoLoopRunning) await Task.Delay(1000, _cts.Token);
                        continue;
                    }
                    _engineInitialized = true;

                    // 等待唤醒事件
                    while (_autoLoopRunning && NativeMethods.GetWakeupStatus() != 1)
                    {
                        await Task.Delay(100, _cts.Token);
                    }

                    if (_autoLoopRunning)
                    {
                        string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                        LogHelper.LogMessage($"检测到唤醒词: {wakeupInfo}");
                        await _popupManager.ShowPopupWithAutoCloseAsync("你好，请问你需要做什么操作？");
                        NativeMethods.ResetWakeupStatus();

                        // 启动命令词识别
                        LogHelper.LogMessage("已启动实时语音识别，等待用户命令...");
                        int esrResult = NativeMethods.StartEsrMicrophone();
                        string esrDetails = NativeMethods.GetLastResultString();
                        LogHelper.LogMessage($"命令词识别启动结果: {esrResult}, 详情: {esrDetails}");

                        if (esrResult != 0)
                        {
                            LogHelper.LogMessage($"启动命令词识别失败 (错误码: {esrResult}).");
                            _popupManager.HidePopup();
                            continue;
                        }

                        // 监控命令词识别结果
                        var commandTimeout = TimeSpan.FromSeconds(5);
                        bool commandMatched = false;
                        string lastCommandText = string.Empty;
                        var timeoutTime = DateTime.Now + commandTimeout;

                        while (_autoLoopRunning && DateTime.Now < timeoutTime && !commandMatched)
                        {
                            IntPtr ptr = NativeMethods.GetEsrKeywordResult();
                            string currentCommandRaw = ptr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ptr)?.Trim() : string.Empty;
                            if (!string.IsNullOrEmpty(currentCommandRaw) && currentCommandRaw != lastCommandText)
                            {
                                lastCommandText = currentCommandRaw;
                                LogHelper.LogMessage($"[DEBUG] 当前命令内容: '{lastCommandText}'");
                                if (_validCommands.Contains(lastCommandText))
                                {
                                    LogHelper.LogMessage($"[DEBUG] 命令完全匹配: '{lastCommandText}'，弹窗");
                                    _recognitionCompleted = false;
                                    await _popupManager.ShowPopupWithAutoCloseAsync(lastCommandText);
                                    _lastHandledResult = lastCommandText;
                                    commandMatched = true;
                                }
                            }
                            await Task.Delay(100, _cts.Token);
                        }

                        if (_autoLoopRunning && !commandMatched)
                        {
                            LogHelper.LogMessage("命令识别超时或未获得有效匹配。");
                            _popupManager.HidePopup();
                        }
                        else if (_autoLoopRunning)
                        {
                            // 等待4秒后关闭弹窗
                            await Task.Delay(4000, _cts.Token);
                            _popupManager.HidePopup();
                            LogHelper.LogMessage("弹窗已关闭，返回唤醒监听状态。");
                        }

                        // 返回唤醒监听状态
                        if (_autoLoopRunning)
                        {
                            LogHelper.LogMessage("准备重新启动唤醒监听...");
                        }
                    }
                }
                catch (TaskCanceledException)
                {
                    LogHelper.LogMessage("语音交互循环任务被取消。");
                    _autoLoopRunning = false;
                }
                catch (Exception ex)
                {
                    LogHelper.LogMessage($"语音交互循环发生严重异常: {ex.Message}");
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
            LogHelper.LogMessage("语音交互循环正在停止...");
            _resultMonitor?.Stop();
            if (_engineInitialized)
            {
                LogHelper.LogMessage("语音交互循环结束，已最后调用停止ESR麦克风。");
                _engineInitialized = false;
            }
            await Dispatcher.InvokeAsync(() => _popupManager.HidePopup());
            LogHelper.LogMessage("语音交互循环已完全停止。");
        }
    }
}