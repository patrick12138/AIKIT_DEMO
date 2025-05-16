using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading; // 添加定时器命名空间
using WinForms = System.Windows.Forms; // 为System.Windows.Forms创建别名
using Microsoft.Win32;
using System.IO; // 添加 System.IO 命名空间用于文件操作

namespace AikitWpfDemo
{
    public partial class MainWindow : Window
    {
        private CancellationTokenSource _cts;
        private bool _engineInitialized = false;
        private CortanaLikePopup _cortanaPopup;
        private CancellationTokenSource _popupCloseCts; // 为弹窗自动关闭添加CancellationTokenSource
        private readonly string _logFilePath; // 日志文件路径

        // 添加各种结果格式的监控定时器
        private DispatcherTimer _resultMonitorTimer;

        // 缓存各种类型的结果
        private string _lastPgsResult = string.Empty;
        private string _lastHandledPgsResult = string.Empty;
        private string _lastHtkResult = string.Empty;
        private string _lastPlainResult = string.Empty;
        private string _lastVadResult = string.Empty;
        private string _lastReadableResult = string.Empty;

        // 结果合并展示标志
        private bool _mergeResults = true; // 是否将所有结果合并在一起显示

        // 自动循环流程控制字段
        private bool _autoLoopRunning = false;
        private bool _recognitionCompleted = false;
        private string _lastHandledResult = string.Empty;

        // 存储有效命令词的集合
        private HashSet<string> _validCommands;

        public MainWindow()
        {
            InitializeComponent();
            _cortanaPopup = new CortanaLikePopup();
            _cts = new CancellationTokenSource();
            _logFilePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "wpf_app_log.txt"); // 初始化日志文件路径

            // 初始化识别结果监控定时器
            _resultMonitorTimer = new DispatcherTimer();
            _resultMonitorTimer.Interval = TimeSpan.FromMilliseconds(50); // 每50ms检查一次，保证实时性
            _resultMonitorTimer.Tick += ResultMonitorTimer_Tick;

            // 初始化时只加载一次命令词
            _validCommands = LoadValidCommands();

            // 启动自动语音循环流程（Loaded事件）
            Loaded += async (s, e) => await StartAutoVoiceLoop();
        }

        // 识别结果监控定时器事件处理
        private void ResultMonitorTimer_Tick(object sender, EventArgs e)
        {
            try
            {
                CheckAndProcessNewResults();
            }
            catch (Exception ex)
            {
                LogMessage($"识别结果监控异常: {ex.Message}");
            }
        }

        // 自动关闭弹窗并在关闭后标记识别完成的异步方法
        private async Task ShowPopupWithAutoCloseAndCompleteAsync(string text)
        {
            try
            {
                // 取消之前可能存在的关闭任务
                _popupCloseCts?.Cancel();
                _popupCloseCts = new CancellationTokenSource();
                var token = _popupCloseCts.Token;

                if (_cortanaPopup == null || !_cortanaPopup.IsLoaded)
                {
                    _cortanaPopup = new CortanaLikePopup();
                }

                if (!_cortanaPopup.IsVisible)
                {
                    _cortanaPopup.Show();
                    _cortanaPopup.Activate();
                }
                // 确保更新前清空，即使UpdateText内部也会处理，这里多一步保险
                _cortanaPopup.RecognizedText = "";
                _cortanaPopup.UpdateText(text);

                await Task.Delay(4000, token); // 等待4秒，允许被取消

                if (token.IsCancellationRequested) return; // 如果被取消，则不执行后续操作

                if (_cortanaPopup != null && _cortanaPopup.IsVisible)
                {
                    _cortanaPopup.Hide();
                }

                // 弹窗关闭后再进入下一轮 (或者说标记本次识别交互完成)
                _recognitionCompleted = true;
                // _lastHandledPgsResult = _lastPgsResult; // 移除此行: 由 CheckAndProcessNewResults 负责更新
            }
            catch (TaskCanceledException)
            {
                // 任务被取消是预期的，比如新的PGS结果来了，旧的显示任务被取消
                LogMessage("弹窗显示任务被取消（可能有新的识别结果）。");
            }
            catch (Exception ex)
            {
                LogMessage($"显示弹窗时发生异常: {ex.Message}");
                _recognitionCompleted = true; // 发生异常也认为本次尝试结束
                if (_cortanaPopup != null && _cortanaPopup.IsVisible) _cortanaPopup.Hide();
            }
        }

        // 检查和处理所有类型的新结果
        private void CheckAndProcessNewResults()
        {
            bool hasAnyNewResult = false;
            StringBuilder resultBuilder = null;

            if (_mergeResults)
            {
                resultBuilder = new StringBuilder();
            }

            // 检查pgs格式结果
            string currentPgsVal = NativeMethods.GetLatestPgsResult();

            // 首先，如果从Native获取的结果与我们缓存的_lastPgsResult不同，则更新_lastPgsResult
            if (currentPgsVal != null && currentPgsVal != _lastPgsResult) // currentPgsVal可能是空字符串，所以用 != null 检查
            {
                _lastPgsResult = currentPgsVal;
            }

            // 然后，检查我们最新的_lastPgsResult (非空时) 是否是用户尚未见过的 (与_lastHandledPgsResult比较)
            // 并且确保 _recognitionCompleted 为 true，表示上一轮识别已结束，可以开始处理新的了
            if (!string.IsNullOrEmpty(_lastPgsResult) && _lastPgsResult != _lastHandledPgsResult && _recognitionCompleted)
            {

                _recognitionCompleted = false; // 开始处理新结果，标记为未完成
                _lastHandledPgsResult = _lastPgsResult; // *立即标记为已处理*，防止异步弹窗期间重复触发
                hasAnyNewResult = true;

                if (_mergeResults)
                {
                    resultBuilder.AppendLine(_lastPgsResult);
                }
                else
                {
                    LogMessage(_lastPgsResult);
                }

                // 更新弹窗显示PGS实时结果（取最后一行，通常是最新内容）
                string[] lines = _lastPgsResult.Split('\n');
                if (lines.Length > 0)
                {
                    string lastLine = lines[lines.Length - 1];
                    string popupText = lastLine.StartsWith("pgs") ? lastLine.Substring(4) : lastLine;
                    _ = ShowPopupWithAutoCloseAndCompleteAsync(popupText);
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
                    LogMessage(currentResult);
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
                    LogMessage(currentResult2);
                }
            }


            // 如果有新结果且设置为合并显示，则一次性输出所有结果
            if (hasAnyNewResult && _mergeResults && resultBuilder != null && resultBuilder.Length > 0)
            {
                LogMessage(resultBuilder.ToString().TrimEnd());
            }

            // 确保弹窗可见
            if (hasAnyNewResult)
            {
                // 如果弹窗实例无效或已关闭，创建新实例
                if (_cortanaPopup == null || !_cortanaPopup.IsLoaded)
                {
                    _cortanaPopup = new CortanaLikePopup();
                }

                if (!_cortanaPopup.IsVisible)
                {
                    _cortanaPopup.Show();
                    _cortanaPopup.Activate();
                }
            }
        }

        // 记录日志到界面
        private void LogMessage(string message)
        {
            // 确保在UI线程上更新界面
            if (!Dispatcher.CheckAccess())
            {
                Dispatcher.Invoke(() => LogMessage(message));
                return;
            }

            string logEntry = $"[{DateTime.Now:HH:mm:ss}] {message}\\n";
            TxtLog.Text += logEntry;
            // 自动滚动到底部
            (TxtLog.Parent as ScrollViewer)?.ScrollToEnd();

            try
            {
                File.AppendAllText(_logFilePath, logEntry);
            }
            catch (Exception ex)
            {
                // 如果写入文件失败，可以在UI上显示一个简短的错误提示，或者仅在调试时输出到控制台
                TxtLog.Text += $"[{DateTime.Now:HH:mm:ss}] [CRITICAL] Failed to write to log file: {ex.Message}\\n";
            }
        }

        // 窗口关闭事件
        protected override void OnClosed(EventArgs e)
        {
            try
            {
                // 取消任何正在进行的操作
                _cts?.Cancel();

                // 停止识别结果监控定时器
                if (_resultMonitorTimer != null && _resultMonitorTimer.IsEnabled)
                {
                    _resultMonitorTimer.Stop();
                }

                // 清空所有结果缓冲区
                //NativeMethods.ClearAllResultBuffers();
                // 安全关闭弹窗
                if (_cortanaPopup != null && _cortanaPopup.IsLoaded)
                {
                    _cortanaPopup.Hide(); // 使用Hide代替Close
                    _cortanaPopup = null;
                }
            }
            catch
            {
                // 忽略关闭时的异常
            }
            base.OnClosed(e);
        }

        // 启动唤醒测试按钮点击事件处理
        private void BtnStartWakeup_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 清空日志
                TxtLog.Text = string.Empty;
                LogMessage("开始唤醒测试...");

                // 重置唤醒状态
                NativeMethods.ResetWakeupStatus();

                // 禁用按钮防止重复点击
                BtnStartWakeup.IsEnabled = false;

                // 启动唤醒测试
                int result = NativeMethods.StartWakeup();
                string detailedResult = NativeMethods.GetLastResultString();
                LogMessage($"唤醒测试启动结果: {detailedResult}");

                if (NativeMethods.GetWakeupStatus() == 1)
                {
                    string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                    LogMessage($"检测到唤醒词: {wakeupInfo}");

                    _cortanaPopup.UpdateText("你好，请问你需要做什么操作？");
                    _cortanaPopup.Show();
                    _cortanaPopup.Activate();

                    // 重置唤醒状态，避免重复弹窗
                    NativeMethods.ResetWakeupStatus();

                    LogMessage("已启动实时语音识别，等待用户命令...");
                }
                else
                {
                    LogMessage("未检测到唤醒词，弹窗未显示");
                }

                // 重新启用按钮
                BtnStartWakeup.IsEnabled = true;
            }
            catch (Exception ex)
            {
                LogMessage($"启动唤醒测试时发生异常: {ex.Message}");
                MessageBox.Show($"启动唤醒测试时发生异常: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                BtnStartWakeup.IsEnabled = true;

                // 停止监控定时器
                if (_resultMonitorTimer != null && _resultMonitorTimer.IsEnabled)
                {
                    _resultMonitorTimer.Stop();
                }
            }
        }

        // 运行完整测试并显示弹窗
        private void BtnStartEsr_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 清空各种结果缓存
                _lastPgsResult = string.Empty;
                _lastHtkResult = string.Empty;
                _lastPlainResult = string.Empty;
                _lastVadResult = string.Empty;
                _lastReadableResult = string.Empty;

                // 启动识别结果监控定时器
                if (!_resultMonitorTimer.IsEnabled)
                {
                    _resultMonitorTimer.Start();
                    LogMessage("已启动实时识别结果监控");
                }

                // 禁用按钮，防止重复点击
                BtnRunFullTest.IsEnabled = false;

                // 执行测试
                int result = NativeMethods.StartEsrMicrophone();

                // 获取并记录详细结果信息
                string detailedResult = NativeMethods.GetLastResultString();
                LogMessage($"测试启动结果: {detailedResult}");

                if (result == 0)
                {
                    LogMessage("测试完成，执行成功！");
                }
                else
                {
                    LogMessage($"测试失败，错误码: {result}");
                }

                // 重新启用按钮
                BtnRunFullTest.IsEnabled = true;
            }
            catch (Exception ex)
            {
                LogMessage($"启动测试线程时发生异常: {ex.Message}");
                MessageBox.Show($"启动测试时发生异常: {ex.Message}", "异常", MessageBoxButton.OK, MessageBoxImage.Error);
                BtnRunFullTest.IsEnabled = true;

                // 确保停止所有监控定时器
                if (_resultMonitorTimer != null && _resultMonitorTimer.IsEnabled)
                {
                    _resultMonitorTimer.Stop();
                }
            }
        }

        // 自动语音循环流程
        private async Task StartAutoVoiceLoop()
        {
            if (_autoLoopRunning) // 防止重复启动循环
            {
                LogMessage("命令词识别自动循环已在运行中。");
                return;
            }
            _autoLoopRunning = true; // 标记循环正在运行
            LogMessage("命令词识别自动循环已启动...");

            // _lastHandledResult 作为类成员变量，在循环外持久化，不清空

            while (_autoLoopRunning) // 主识别循环
            {
                try
                {
                    // 每轮开始前重置一些单轮内的状态变量
                    // _lastPlainResult = string.Empty; // 根据需要决定是否每轮清空
                    // _lastPgsResult = string.Empty;   // 根据需要决定是否每轮清空
                    // _lastHandledResult 不再在此处清空，以实现跨轮次处理记忆

                    _popupCloseCts?.Cancel(); // 取消上一个弹窗的关闭令牌
                    if (_cortanaPopup != null && _cortanaPopup.IsVisible) _cortanaPopup.Hide(); // 隐藏弹窗

                    LogMessage("尝试启动ESR麦克风进行命令词识别...");
                    int esrStartResult = NativeMethods.StartEsrMicrophone(); // 启动ESR
                    string esrStartDetails = NativeMethods.GetLastResultString();
                    LogMessage($"ESR麦克风启动结果: {esrStartResult}, 详情: {esrStartDetails}");

                    if (esrStartResult != 0) // ESR启动失败处理
                    {
                        LogMessage($"启动ESR麦克风失败 (错误码: {esrStartResult}). 1秒后重试...");
                        _engineInitialized = false; // 标记引擎未初始化
                        // 无需在此处调用 StopEsrMicrophone，因为启动都失败了
                        if (_cortanaPopup != null && _cortanaPopup.IsVisible) _cortanaPopup.Hide();
                        if (_autoLoopRunning) await Task.Delay(1000, _cts.Token);
                        continue; // 继续下一轮循环
                    }
                    _engineInitialized = true; // ESR启动成功

                    if (!_resultMonitorTimer.IsEnabled) // 确保结果监控定时器运行
                    {
                        _resultMonitorTimer.Start();
                        LogMessage("识别结果监控定时器已启动。");
                    }

                    var commandTimeout = TimeSpan.FromSeconds(2); // 单轮命令识别超时
                    bool pgsMatchedThisTurn = false; // 标记本轮PGS是否匹配成功
                    string currentTurnLastPgsText = string.Empty; // 本轮内获取的最后一个PGS文本，用于避免单轮内对相同PGS的重复解析
                    // string currentTurnLastMatchedCommand = string.Empty; // 如果需要在单轮内防止对同一命令多次弹窗，可以使用此变量

                    var timeoutTime = DateTime.Now + commandTimeout;

                    // 在超时前持续获取和检查PGS结果
                    while (_autoLoopRunning && DateTime.Now < timeoutTime && !pgsMatchedThisTurn)
                    {
                        string currentPgsRaw = NativeMethods.GetLatestPgsResult()?.Trim(); // 获取原始PGS结果

                        // 检查原始PGS是否与本轮上一次获取的不同，且不为空
                        if (!string.IsNullOrEmpty(currentPgsRaw) && currentPgsRaw != currentTurnLastPgsText)
                        {
                            currentTurnLastPgsText = currentPgsRaw; // 更新本轮获取的PGS文本
                            string pgsTextContent = ParsePgsText(currentPgsRaw); // 解析PGS中的命令内容

                            // 进一步检查解析出的命令内容是否有效，并且不是上一个已处理的命令
                            if (!string.IsNullOrEmpty(pgsTextContent) && pgsTextContent != _lastHandledResult)
                            {
                                LogMessage($"[DEBUG] 当前PGS内容: '{pgsTextContent}'");
                                if (_validCommands.Contains(pgsTextContent)) // 检查是否是有效命令
                                {
                                    LogMessage($"[DEBUG] PGS完全匹配命令词: '{pgsTextContent}'，弹窗");

                                    _recognitionCompleted = false; // 在调用弹窗前，标记识别/处理未完成
                                    await ShowPopupWithAutoCloseAndCompleteAsync(pgsTextContent);
                                    // 假设 ShowPopupWithAutoCloseAndCompleteAsync 内部会在结束时设置 _recognitionCompleted = true;

                                    _lastHandledResult = pgsTextContent; // 更新全局已处理命令的记录
                                    pgsMatchedThisTurn = true; // 标记本轮PGS已匹配
                                }
                            }
                        }
                        await Task.Delay(100, _cts.Token); // 短暂延迟，再次检查PGS
                    } // 结束内部PGS检查循环

                    if (_autoLoopRunning && !pgsMatchedThisTurn)
                    {
                        LogMessage("本轮命令识别超时或未获得有效PGS匹配。");
                        if (_cortanaPopup != null && _cortanaPopup.IsVisible) _cortanaPopup.Hide();
                    }

                    // 无论本轮是否匹配成功，都尝试停止/清理当前ESR会话
                    LogMessage("准备停止当前ESR会话...");
                    // 【重要】请与C++开发人员确认 StopEsrMicrophone() 是否是正确的停止/重置方法，
                    // 或者是否有其他如 ResetEsrSession() 等函数需要调用。
                    // 错误的停止方式可能导致资源泄露或下次启动失败。
                    if (_engineInitialized) // 仅当引擎被认为初始化成功时才尝试停止
                    {
                        //NativeMethods.StopEsrMicrophone(); // 停止ESR麦克风
                        LogMessage("已调用停止ESR麦克风。");
                    }
                    _engineInitialized = false; // 标记引擎已停止或需要重新初始化

                    if (_autoLoopRunning)
                    {
                        LogMessage("本轮命令识别结束，1秒后开始下一轮...");
                        await Task.Delay(1000, _cts.Token); // 延迟1秒开始下一轮
                    }
                }
                catch (TaskCanceledException)
                {
                    LogMessage("自动语音循环任务被取消。");
                    _autoLoopRunning = false; // 确保循环终止
                }
                catch (Exception ex)
                {
                    LogMessage($"命令词识别主循环发生严重异常: {ex.Message}");
                    _engineInitialized = false; // 发生异常，标记引擎状态未知
                    if (_resultMonitorTimer.IsEnabled) _resultMonitorTimer.Stop();
                    if (_cortanaPopup != null && _cortanaPopup.IsVisible) _cortanaPopup.Hide();
                    // 考虑是否在此处也调用 StopEsrMicrophone (如果适用且安全)
                    if (_autoLoopRunning)
                    {
                        LogMessage("异常后等待1秒重试...");
                        try { await Task.Delay(1000, _cts.Token); } catch (TaskCanceledException) { _autoLoopRunning = false; }
                    }
                }
            } // 结束主 while (_autoLoopRunning) 循环

            LogMessage("命令词识别自动循环正在停止...");
            if (_resultMonitorTimer.IsEnabled) _resultMonitorTimer.Stop();

            // 在循环结束后，如果引擎仍然被认为是初始化的，最后尝试停止一次。
            if (_engineInitialized)
            {
                //NativeMethods.StopEsrMicrophone();
                LogMessage("自动循环结束，已最后调用停止ESR麦克风。");
                _engineInitialized = false;
            }

            await Dispatcher.InvokeAsync(() =>
            {
                _popupCloseCts?.Cancel();
                if (_cortanaPopup != null && _cortanaPopup.IsVisible)
                {
                    _cortanaPopup.Hide();
                }
            });
            LogMessage("命令词识别自动循环已完全停止。");
        }

        private HashSet<string> LoadValidCommands()
        {
            var commands = new HashSet<string>
            {
                "播放下一首",
                "播放上一首",
                "开始播放",
                "暂停播放",
                "停止播放",
                "点歌",
                "插播",
                "删除歌曲",
                "提高音量",
                "降低音量",
                "静音",
                "重唱",
                "伴唱",
                "原唱",
                "已点歌曲"
            };
            return commands;
        }

        /// <summary>
        /// 解析PGS结果字符串，提取命令内容（去除前缀如"pgs:"，并去除首尾空白）
        /// 新实现：支持多行PGS，自动取最后一行有效命令，并兼容"pgs:"、"pgs "等前缀
        /// </summary>
        /// <param name="pgsRaw">原始PGS字符串</param>
        /// <returns>提取出的命令内容，如果无法提取则返回空字符串</returns>
        private string ParsePgsText(string pgsRaw)
        {
            if (string.IsNullOrWhiteSpace(pgsRaw))
                return string.Empty;

            // 支持多行PGS，取最后一行
            string[] lines = pgsRaw.Split(new[] { '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries);
            if (lines.Length == 0)
                return string.Empty;

            string lastLine = lines[lines.Length - 1].Trim();

            // 去除前缀"pgs:"或"pgs "（不区分大小写）
            if (lastLine.StartsWith("pgs:", StringComparison.OrdinalIgnoreCase))
                lastLine = lastLine.Substring(4).Trim();
            else if (lastLine.StartsWith("pgs ", StringComparison.OrdinalIgnoreCase))
                lastLine = lastLine.Substring(4).Trim();

            return lastLine.Trim();
        }
    }
}