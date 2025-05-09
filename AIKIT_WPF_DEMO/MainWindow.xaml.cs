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

        // 添加各种结果格式的监控定时器
        private DispatcherTimer _resultMonitorTimer;

        // 缓存各种类型的结果
        private string _lastPgsResult = string.Empty;
        private string _lastHtkResult = string.Empty;
        private string _lastPlainResult = string.Empty;
        private string _lastVadResult = string.Empty;
        private string _lastReadableResult = string.Empty;

        // 结果合并展示标志
        private bool _mergeResults = true; // 是否将所有结果合并在一起显示

        public MainWindow()
        {
            InitializeComponent();
            _cortanaPopup = new CortanaLikePopup();
            _cts = new CancellationTokenSource();

            // 初始化识别结果监控定时器
            _resultMonitorTimer = new DispatcherTimer();
            _resultMonitorTimer.Interval = TimeSpan.FromMilliseconds(50); // 每50ms检查一次，保证实时性
            _resultMonitorTimer.Tick += ResultMonitorTimer_Tick;
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
            if (NativeMethods.HasNewPgsResult())
            {
                string currentResult = NativeMethods.GetPgsResultString();
                if (!string.IsNullOrEmpty(currentResult) && currentResult != _lastPgsResult)
                {
                    _lastPgsResult = currentResult;
                    hasAnyNewResult = true;

                    if (_mergeResults)
                    {
                        resultBuilder.AppendLine(currentResult);
                    }
                    else
                    {
                        LogMessage(currentResult);
                    }

                    // 更新弹窗显示PGS实时结果（取最后一行，通常是最新内容）
                    string[] lines = currentResult.Split('\n');
                    if (lines.Length > 0)
                    {
                        string lastLine = lines[lines.Length - 1];
                        if (lastLine.StartsWith("pgs： "))
                        {
                            _cortanaPopup.UpdateText(lastLine.Substring(5)); // 去掉"pgs： "前缀
                        }
                        else
                        {
                            _cortanaPopup.UpdateText(lastLine);
                        }
                    }
                }
            }

            // 检查htk格式结果
            if (NativeMethods.HasNewHtkResult())
            {
                string currentResult = NativeMethods.GetHtkResultString();
                if (!string.IsNullOrEmpty(currentResult) && currentResult != _lastHtkResult)
                {
                    _lastHtkResult = currentResult;
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
            }

            // 检查plain格式结果
            if (NativeMethods.HasNewPlainResult())
            {
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
            }

            // 检查vad格式结果
            if (NativeMethods.HasNewVadResult())
            {
                string currentResult = NativeMethods.GetVadResultString();
                if (!string.IsNullOrEmpty(currentResult) && currentResult != _lastVadResult)
                {
                    _lastVadResult = currentResult;
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
            }

            // 检查readable格式结果
            if (NativeMethods.HasNewReadableResult())
            {
                string currentResult = NativeMethods.GetReadableResultString();
                if (!string.IsNullOrEmpty(currentResult) && currentResult != _lastReadableResult)
                {
                    _lastReadableResult = currentResult;
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
            }

            // 如果有新结果且设置为合并显示，则一次性输出所有结果
            if (hasAnyNewResult && _mergeResults && resultBuilder != null && resultBuilder.Length > 0)
            {
                LogMessage(resultBuilder.ToString().TrimEnd());
            }

            // 确保弹窗可见
            if (hasAnyNewResult && !_cortanaPopup.IsVisible)
            {
                _cortanaPopup.ShowPopup();
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
                NativeMethods.ClearAllResultBuffers();
                // 关闭弹窗
                _cortanaPopup?.Close();
            }
            catch
            {
                // 忽略关闭时的异常
            }
            base.OnClosed(e);
        }

        // 运行完整测试并显示弹窗
        private void BtnRunFullTest_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 清空日志，准备显示新的测试信息
                TxtLog.Text = string.Empty;
                LogMessage("开始执行完整测试...");

                // 清空各种结果缓存
                _lastPgsResult = string.Empty;
                _lastHtkResult = string.Empty;
                _lastPlainResult = string.Empty;
                _lastVadResult = string.Empty;
                _lastReadableResult = string.Empty;

                // 重置状态，避免误报
                NativeMethods.ResetWakeupStatus();
                NativeMethods.ResetEsrStatus();
                NativeMethods.ClearAllResultBuffers();

                // 启动识别结果监控定时器
                if (!_resultMonitorTimer.IsEnabled)
                {
                    _resultMonitorTimer.Start();
                    LogMessage("已启动实时识别结果监控");
                }

                // 禁用按钮，防止重复点击
                BtnRunFullTest.IsEnabled = false;

                // 执行测试
                int result = NativeMethods.RunFullTest();

                // 获取并记录详细结果信息
                string detailedResult = NativeMethods.GetLastResultString();
                LogMessage($"测试启动结果: {detailedResult}");

                // 检查唤醒状态 - 只有唤醒成功才显示弹窗
                if (NativeMethods.GetWakeupStatus() == 1)
                {
                    string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                    LogMessage($"检测到唤醒词: {wakeupInfo}");

                    // 唤醒成功后打开弹窗，等待命令词
                    _cortanaPopup.UpdateText("正在等待命令...");
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

        // 供扩展使用的辅助方法
        private void OnWakeWordDetected()
        {
            _cortanaPopup.UpdateText("正在聆听...");
            _cortanaPopup.ShowPopup();
        }
    }
}