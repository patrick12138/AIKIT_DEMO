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
        
        // 语音助手管理器
        private VoiceAssistantManager _voiceManager;

        public MainWindow()
        {
            InitializeComponent();
            _cortanaPopup = new CortanaLikePopup();
            _cts = new CancellationTokenSource();

            // 初始化识别结果监控定时器
            _resultMonitorTimer = new DispatcherTimer();
            _resultMonitorTimer.Interval = TimeSpan.FromMilliseconds(50); // 每50ms检查一次，保证实时性
            _resultMonitorTimer.Tick += ResultMonitorTimer_Tick;
            
            // 初始化语音助手管理器
            _voiceManager = VoiceAssistantManager.Instance;
            _voiceManager.StateChanged += VoiceManager_StateChanged;
            _voiceManager.CommandRecognized += VoiceManager_CommandRecognized;
            
            // 注册窗口关闭事件
            this.Closing += MainWindow_Closing;
        }
        
        // 窗口关闭事件
        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            // 停止语音助手循环
            if (_voiceManager != null)
            {
                _voiceManager.Stop();
            }
        }
        
        // 语音助手状态变化事件处理
        private void VoiceManager_StateChanged(object sender, VoiceAssistantStateEventArgs e)
        {
            // 更新UI显示当前状态
            Dispatcher.Invoke(() =>
            {
                switch (e.State)
                {
                    case VoiceAssistantManager.VoiceAssistantState.Idle:
                        StatusTextBlock.Text = "空闲状态";
                        break;
                    case VoiceAssistantManager.VoiceAssistantState.WakeupListening:
                        StatusTextBlock.Text = "等待唤醒词...";
                        break;
                    case VoiceAssistantManager.VoiceAssistantState.CommandListening:
                        StatusTextBlock.Text = "正在聆听命令...";
                        break;
                    case VoiceAssistantManager.VoiceAssistantState.Processing:
                        StatusTextBlock.Text = "正在处理命令...";
                        break;
                }
            });
        }
        
        // 命令识别事件处理
        private void VoiceManager_CommandRecognized(object sender, CommandRecognizedEventArgs e)
        {
            Dispatcher.Invoke(() =>
            {
                // 记录识别到的命令
                LogMessage($"识别到命令: {e.Command}, 成功: {e.Success}");
                
                // 在这里可以添加命令处理逻辑
                // ...
            });
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
                        _ = ShowPopupWithAutoCloseAsync(lastLine.Substring(4)); // 去掉"pgs："前缀
                    }
                    else
                    {
                        _ = ShowPopupWithAutoCloseAsync(lastLine);
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
            }            // 确保弹窗可见
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
        }        // 记录日志到界面
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
        
        // 显示弹窗并在指定时间后自动关闭
        private async Task ShowPopupWithAutoCloseAsync(string text, int autoCloseMilliseconds = 5000)
        {
            // 确保在UI线程上更新界面
            if (!Dispatcher.CheckAccess())
            {
                await Dispatcher.InvokeAsync(() => ShowPopupWithAutoCloseAsync(text, autoCloseMilliseconds));
                return;
            }
            
            // 如果弹窗实例无效或已关闭，创建新实例
            if (_cortanaPopup == null || !_cortanaPopup.IsLoaded)
            {
                _cortanaPopup = new CortanaLikePopup();
            }
            
            // 更新弹窗文本并显示
            _cortanaPopup.UpdateText(text);
            
            if (!_cortanaPopup.IsVisible)
            {
                _cortanaPopup.Show();
                _cortanaPopup.Activate();
            }
            
            // 等待指定时间后自动关闭弹窗
            // 注意：如果在此期间有新的调用，将取消先前的自动关闭
            await Task.Delay(autoCloseMilliseconds);
            
            // 检查是否仍需要关闭弹窗（可能已被新的调用取代）
            // 这里可以添加更复杂的逻辑来决定是否关闭弹窗
            if (_cortanaPopup != null && _cortanaPopup.IsVisible)
            {
                _cortanaPopup.Hide();
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
                }                // 清空所有结果缓冲区
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
        
        // 启动语音助手循环按钮点击事件
        private void BtnStartVoiceAssistant_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 启动语音助手循环
                if (_voiceManager.Start())
                {
                    LogMessage("语音助手循环已启动");
                    
                    // 更新UI
                    BtnStartVoiceAssistant.IsEnabled = false;
                    BtnStopVoiceAssistant.IsEnabled = true;
                }
                else
                {
                    LogMessage("启动语音助手循环失败");
                }
            }
            catch (Exception ex)
            {
                LogMessage($"启动语音助手循环异常: {ex.Message}");
            }
        }
        
        // 停止语音助手循环按钮点击事件
        private void BtnStopVoiceAssistant_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 停止语音助手循环
                _voiceManager.Stop();
                LogMessage("语音助手循环已停止");
                
                // 更新UI
                BtnStartVoiceAssistant.IsEnabled = true;
                BtnStopVoiceAssistant.IsEnabled = false;
            }
            catch (Exception ex)
            {
                LogMessage($"停止语音助手循环异常: {ex.Message}");
            }
        }
    }
}