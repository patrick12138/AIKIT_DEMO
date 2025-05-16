using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading; // 添加定时器命名空间
using WinForms = System.Windows.Forms; // 为System.Windows.Forms创建别名
using Microsoft.Win32;

namespace AikitWpfDemo
{
    public partial class MainWindow : Window
    {
        private CancellationTokenSource _cts;
        private bool _engineInitialized = false;
        private CortanaLikePopup _cortanaPopup;
        private CancellationTokenSource _popupCloseCts; // 为弹窗自动关闭添加CancellationTokenSource

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

        // 跟踪唤醒句柄的字段
        private int _currentWakeupThreshold = 900; // 默认唤醒阈值

        public MainWindow()
        {
            InitializeComponent();
            _cortanaPopup = new CortanaLikePopup();
            _cts = new CancellationTokenSource();

            // 初始化识别结果监控定时器
            _resultMonitorTimer = new DispatcherTimer();
            _resultMonitorTimer.Interval = TimeSpan.FromMilliseconds(50); // 每50ms检查一次，保证实时性
            _resultMonitorTimer.Tick += ResultMonitorTimer_Tick;

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
                _lastHandledPgsResult = _lastPgsResult; // 记录已处理的PGS结果
            }
            catch (TaskCanceledException)
            {
                // 任务被取消是预期的，比如新的PGS结果来了，旧的显示任务被取消
                LogMessage("弹窗显示任务被取消（可能有新的识别结果）。");
            }
            catch (Exception ex)
            {
                LogMessage($"显示弹窗时发生异常: {ex.Message}");
                // Consider if _cortanaPopup should be set to null here,
                // or if _recognitionCompleted should be set to true to allow loop to proceed.
                // For now, let's assume _recognitionCompleted should still be true to prevent deadlock in main loop.
                _recognitionCompleted = true; // 发生异常也认为本次尝试结束
                if (_cortanaPopup != null && _cortanaPopup.IsVisible) _cortanaPopup.Hide();
                // _cortanaPopup = null; // 谨慎处理，可能下次又重建
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
                    LogMessage(currentResult1);
                }

                // 更新弹窗显示PGS实时结果（取最后一行，通常是最新内容）
                string[] lines = currentResult1.Split('\n');
                if (lines.Length > 0)
                {
                    string lastLine = lines[lines.Length - 1];
                    if (lastLine.StartsWith("pgs"))
                    {
                        _ = ShowPopupWithAutoCloseAndCompleteAsync(lastLine.Substring(4)); // 去掉"pgs："前缀
                    }
                    else
                    {
                        _ = ShowPopupWithAutoCloseAndCompleteAsync(lastLine);
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

            TxtLog.Text += $"[{DateTime.Now:HH:mm:ss}] {message}\n";
            // 自动滚动到底部
            (TxtLog.Parent as ScrollViewer)?.ScrollToEnd();
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
        private void BtnRunFullTest_Click(object sender, RoutedEventArgs e)
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
            if (_autoLoopRunning) return;
            _autoLoopRunning = true;
            LogMessage("命令词识别自动循环已启动...");            while (_autoLoopRunning)
            {
                try
                {
                    LogMessage("开始新的语音循环...");

                    // 1. 完全重置所有识别相关状态
                    _recognitionCompleted = false;
                    _lastHandledResult = string.Empty;
                    _lastPlainResult = string.Empty;
                    _lastPgsResult = string.Empty;
                    _lastHtkResult = string.Empty;
                    _lastVadResult = string.Empty;
                    _lastReadableResult = string.Empty;
                    _lastHandledPgsResult = string.Empty;                    // 停止可能运行中的识别进程
                    try 
                    {
                        // 先停止定时器，避免在停止过程中继续处理数据
                        if (_resultMonitorTimer.IsEnabled)
                            _resultMonitorTimer.Stop();

                        // 确保按正确的顺序停止
                        if (_engineInitialized)
                        {
                            LogMessage("正在安全停止ESR麦克风...");
                            // 先停止唤醒检测
                            NativeMethods.StopWakeupDetection();
                            
                            // 等待一小段时间确保唤醒检测完全停止
                            await Task.Delay(100);
                            
                            // 再停止ESR麦克风
                            var stopResult = NativeMethods.StopEsrMicrophone();
                            if (stopResult != 0)
                            {
                                LogMessage($"停止ESR麦克风时出现错误，错误码: {stopResult}");
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        LogMessage($"停止识别进程时发生异常: {ex.Message}");
                        // 继续执行，不要中断主循环
                    }

                    // 2. 重置并隐藏弹窗
                    if (_cortanaPopup != null)
                    {
                        _cortanaPopup.RecognizedText = string.Empty;
                        if (_cortanaPopup.IsVisible)
                            _cortanaPopup.Hide();
                    }                    // 3. 开始语音唤醒检测
                    LogMessage("启动语音唤醒检测...");
                    int wakeupResult = NativeMethods.StartWakeup();
                    if (wakeupResult != 0)
                    {
                        LogMessage($"启动语音唤醒检测失败，错误码: {wakeupResult}");
                        await Task.Delay(1000);
                        continue;
                    }
                    LogMessage("语音唤醒检测已启动，等待唤醒词...");

                    // 4. 启动识别结果监控定时器
                    if (!_resultMonitorTimer.IsEnabled)
                        _resultMonitorTimer.Start();                    // 5. 等待唤醒词检测
                    LogMessage("等待用户说出唤醒词...");
                    bool isWoken = false;
                    var wakeupStart = DateTime.Now;
                    int wakeupTimeoutMs = 0; // 设置为0表示持续等待，直到检测到唤醒词

                    while (!isWoken && _autoLoopRunning)
                    {
                        // 检查是否检测到唤醒词
                        if (NativeMethods.GetWakeupStatus() == 1)
                        {
                            isWoken = true;
                            string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                            LogMessage($"检测到唤醒词: {wakeupInfo}");

                            // 停止唤醒检测
                            NativeMethods.StopWakeupDetection();

                            // 开始命令词识别
                            LogMessage("开始命令词识别...");
                            int esrResult = NativeMethods.StartEsrMicrophone();
                            string detailedEsrResult = NativeMethods.GetLastResultString();
                            LogMessage($"命令词识别启动结果: {esrResult}, 详情: {detailedEsrResult}");

                            // 显示唤醒成功的弹窗
                            await Dispatcher.InvokeAsync(() =>
                            {
                                if (_cortanaPopup == null || !_cortanaPopup.IsLoaded)
                                {
                                    _cortanaPopup = new CortanaLikePopup();
                                }
                                _cortanaPopup.UpdateText("已识别到唤醒词，请说出您的指令");
                                if (!_cortanaPopup.IsVisible)
                                {
                                    _cortanaPopup.Show();
                                    _cortanaPopup.Activate();
                                }
                            });                            // 6. 等待命令词识别结果
                            var commandStart = DateTime.Now;
                            var commandTimeoutMs = 10000; // 10秒超时
                            bool hasCommandResult = false;

                            while ((DateTime.Now - commandStart).TotalMilliseconds < commandTimeoutMs && !hasCommandResult)
                            {
                                CheckAndProcessNewResults();

                                if (!string.IsNullOrEmpty(_lastPlainResult))
                                {
                                    hasCommandResult = true;
                                    await Dispatcher.InvokeAsync(() =>
                                    {
                                        string displayText = _lastPlainResult;
                                        _cortanaPopup.UpdateText($"已识别到命令: {displayText}");
                                    });

                                    // 显示识别结果4秒
                                    await Task.Delay(4000);

                                    await Dispatcher.InvokeAsync(() =>
                                    {
                                        if (_cortanaPopup != null && _cortanaPopup.IsVisible)
                                            _cortanaPopup.Hide();
                                    });

                                    break;
                                }

                                await Task.Delay(50);
                            }                            // 结束命令词识别
                            int stopResult = NativeMethods.SafelyStopEsrMicrophone();
                            if (stopResult != 0)
                            {
                                LogMessage($"停止ESR麦克风时出现错误，错误码: {stopResult}");
                            }
                            
                            if (!hasCommandResult)
                            {
                                LogMessage("未识别到有效命令，超时");
                            }
                            else
                            {
                                LogMessage("命令词识别完成");
                            }
                            
                            break; // 退出唤醒检测循环
                        }
                        
                        await Task.Delay(100); // 降低CPU占用
                    }

                    // 7. 短暂延迟后开始下一轮
                    await Task.Delay(1000);
                }
                catch (Exception ex)
                {
                    LogMessage($"循环过程中发生异常: {ex.Message}");
                    await Task.Delay(1000); // 发生异常时等待1秒后继续
                }
            }
        }

        // 安全停止 ESR 和唤醒检测的方法
        private async Task SafelyStopRecognitionAsync()
        {
            try 
            {
                // 先停止定时器，避免在停止过程中继续处理数据
                if (_resultMonitorTimer.IsEnabled)
                {
                    _resultMonitorTimer.Stop();
                }

                // 确保按正确的顺序停止
                if (_engineInitialized)
                {
                    LogMessage("正在安全停止语音识别...");
                    
                    // 先停止唤醒检测
                    try
                    {
                        NativeMethods.StopWakeupDetection();
                        await Task.Delay(100); // 给系统一些时间完成停止操作
                    }
                    catch (Exception ex)
                    {
                        LogMessage($"停止唤醒检测时出现异常: {ex.Message}");
                    }
                    
                    // 再停止ESR麦克风
                    try
                    {
                        var stopResult = NativeMethods.StopEsrMicrophone();
                        if (stopResult != 0)
                        {
                            LogMessage($"停止ESR麦克风时出现错误，错误码: {stopResult}");
                        }
                        await Task.Delay(100); // 给系统一些时间完成停止操作
                    }
                    catch (Exception ex)
                    {
                        LogMessage($"停止ESR麦克风时出现异常: {ex.Message}");
                    }
                }
            }
            catch (Exception ex)
            {
                LogMessage($"安全停止过程中发生异常: {ex.Message}");
            }
        }

    }
}