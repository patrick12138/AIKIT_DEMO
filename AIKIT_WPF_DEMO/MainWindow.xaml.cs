using System;
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

        // 添加ESR命令词监控定时器
        private DispatcherTimer _esrCheckTimer;
        private int _lastEsrStatus = 0;

        // 添加实时语音识别监控定时器
        private DispatcherTimer _speechRecognitionTimer;
        private string _lastSpeechText = string.Empty;

        public MainWindow()
        {
            InitializeComponent();
            _cortanaPopup = new CortanaLikePopup();
            _cts = new CancellationTokenSource();

            // 初始化命令词监控定时器
            _esrCheckTimer = new DispatcherTimer();
            _esrCheckTimer.Interval = TimeSpan.FromMilliseconds(300); // 每300ms检查一次
            _esrCheckTimer.Tick += EsrCheckTimer_Tick;

            // 初始化实时语音识别监控定时器
            _speechRecognitionTimer = new DispatcherTimer();
            _speechRecognitionTimer.Interval = TimeSpan.FromMilliseconds(100); // 每100ms检查一次
            _speechRecognitionTimer.Tick += SpeechRecognitionTimer_Tick;

            // 窗口加载后自动启动监控
            //Loaded += (s, e) =>
            //{
            //    LogMessage("启动实时语音识别监控...");
            //    _speechRecognitionTimer.Start();

            //    LogMessage("自动开始执行完整测试...");
            //    // 重置唤醒状态，避免误报
            //    NativeMethods.ResetWakeupStatus();

            //    // 执行测试
            //    int result = NativeMethods.RunFullTest();

            //    // 获取并记录详细结果信息
            //    string detailedResult = NativeMethods.GetLastResultString();
            //    LogMessage(detailedResult);

            //    // 检查唤醒状态
            //    if (NativeMethods.GetWakeupStatus() == 1)
            //    {
            //        string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
            //        LogMessage($"检测到唤醒词: {wakeupInfo}");
            //        OnWakeWordDetected();
            //    }
            //};
        }

        // 实时语音识别监控定时器事件处理
        private void SpeechRecognitionTimer_Tick(object sender, EventArgs e)
        {
            // 获取实时语音识别文本
            string currentText = NativeMethods.GetSpeechRecognitionTextString();
            if (!string.IsNullOrEmpty(currentText) && currentText != _lastSpeechText)
            {
                // 更新缓存的最后一次文本
                _lastSpeechText = currentText;

                // 显示在日志中
                LogMessage($"语音识别结果: {currentText}");

                // 显示在弹窗中
                //_cortanaPopup.UpdateText(currentText);
                //if (!_cortanaPopup.IsVisible)
                //{
                //    _cortanaPopup.ShowPopup();
                //}
            }
            //try
            //{
            //    // 检查是否有新的语音识别结果
            //    if (NativeMethods.HasNewSpeechResult())
            //    {
                    
            //    }
            //}
            //catch (Exception ex)
            //{
            //    LogMessage($"实时语音识别监控异常: {ex.Message}");
            //}
        }

        // ESR命令词监控定时器事件处理
        private void EsrCheckTimer_Tick(object sender, EventArgs e)
        {
            try
            {
                // 获取当前ESR状态
                int currentStatus = NativeMethods.GetEsrStatus();

                // 状态变化时进行处理
                if (currentStatus != _lastEsrStatus)
                {
                    _lastEsrStatus = currentStatus;

                    switch (currentStatus)
                    {
                        case 1: // ESR_STATUS_SUCCESS
                            // 获取识别到的命令词
                            string keyword = NativeMethods.GetEsrKeywordResultString();
                            LogMessage($"检测到命令词: {keyword}");

                            // 显示弹窗并更新文本
                            _cortanaPopup.UpdateText(keyword);
                            _cortanaPopup.ShowPopup();

                            // 重置ESR状态，避免重复触发
                            NativeMethods.ResetEsrStatus();
                            break;

                        case 2: // ESR_STATUS_FAILED
                            // 获取错误信息
                            string errorInfo = NativeMethods.GetEsrErrorInfoString();
                            LogMessage($"命令词识别失败: {errorInfo}");

                            // 可选：显示错误信息弹窗
                            // _cortanaPopup.UpdateText($"识别失败: {errorInfo}");
                            // _cortanaPopup.ShowPopup();

                            // 重置ESR状态
                            NativeMethods.ResetEsrStatus();
                            break;

                        case 3: // ESR_STATUS_PROCESSING
                            LogMessage("正在处理语音...");
                            break;
                    }
                }
            }
            catch (Exception ex)
            {
                LogMessage($"ESR监控异常: {ex.Message}");
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

                // 停止ESR命令词监控定时器
                if (_esrCheckTimer != null && _esrCheckTimer.IsEnabled)
                {
                    _esrCheckTimer.Stop();
                }

                // 停止实时语音识别监控定时器
                if (_speechRecognitionTimer != null && _speechRecognitionTimer.IsEnabled)
                {
                    _speechRecognitionTimer.Stop();
                }

                // 清空语音识别缓冲区
                //NativeMethods.ClearSpeechRecognitionBuffer();

                // 确保资源被清理
                if (_engineInitialized)
                {
                    NativeMethods.Ivw70Uninit();
                }

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

                // 清空缓存的上一次识别文本
                _lastSpeechText = string.Empty;
                
                // 重置状态，避免误报
                NativeMethods.ResetWakeupStatus();
                NativeMethods.ResetEsrStatus();
                //NativeMethods.ClearSpeechRecognitionBuffer();

                // 启动ESR命令词监控定时器
                if (!_esrCheckTimer.IsEnabled)
                {
                    _esrCheckTimer.Start();
                    LogMessage("已启动命令词监控");
                }

                // 启动实时语音识别监控定时器
                if (!_speechRecognitionTimer.IsEnabled)
                {
                    _speechRecognitionTimer.Start();
                    LogMessage("已启动实时语音识别监控");
                }

                // 显示弹窗，等待实时语音输入
                _cortanaPopup.UpdateText("正在等待语音输入...");
                _cortanaPopup.Show();
                _cortanaPopup.Activate();

                // 执行测试
                int result = NativeMethods.RunFullTest();

                // 禁用按钮，防止重复点击
                BtnRunFullTest.IsEnabled = false;

                // 获取并记录详细结果信息
                string detailedResult = NativeMethods.GetLastResultString();
                LogMessage(detailedResult);

                // 检查唤醒状态
                if (NativeMethods.GetWakeupStatus() == 1)
                {
                    string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                    LogMessage($"检测到唤醒词: {wakeupInfo}");
                    
                    // 重置唤醒状态，避免重复弹窗
                    NativeMethods.ResetWakeupStatus();
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
                
                LogMessage("已启动实时语音识别，等待用户输入...");
            }
            catch (Exception ex)
            {
                LogMessage($"启动测试线程时发生异常: {ex.Message}");
                MessageBox.Show($"启动测试时发生异常: {ex.Message}", "异常", MessageBoxButton.OK, MessageBoxImage.Error);
                BtnRunFullTest.IsEnabled = true;
                
                // 确保停止所有监控定时器
                if (_esrCheckTimer != null && _esrCheckTimer.IsEnabled)
                {
                    _esrCheckTimer.Stop();
                }
                if (_speechRecognitionTimer != null && _speechRecognitionTimer.IsEnabled)
                {
                    _speechRecognitionTimer.Stop();
                }
            }
        }

        private void BtnRunTest_Click(object sender, RoutedEventArgs e)
        {

        }
        
        // 测试CortanaLikePopup弹窗 - 显示实时语音识别结果
        private void BtnTestPopup_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 清空缓存的上一次识别文本
                _lastSpeechText = string.Empty;
                
                // 清空语音识别缓冲区
                //NativeMethods.ClearSpeechRecognitionBuffer();
                
                // 重置ESR状态
                NativeMethods.ResetEsrStatus();
                
                // 启动ESR命令词监控定时器
                if (!_esrCheckTimer.IsEnabled)
                {
                    _esrCheckTimer.Start();
                    LogMessage("已启动命令词监控");
                }
                
                // 启动实时语音识别监控定时器
                if (!_speechRecognitionTimer.IsEnabled)
                {
                    _speechRecognitionTimer.Start();
                    LogMessage("已启动实时语音识别监控");
                }
                
                // 显示弹窗，等待实时语音输入
                _cortanaPopup.UpdateText("正在等待语音输入...");
                _cortanaPopup.Show();
                _cortanaPopup.Activate();
                
                LogMessage("已启动实时语音识别，等待用户输入...");
            }
            catch (Exception ex)
            {
                LogMessage($"启动实时语音识别出现异常: {ex.Message}");
            }
        }
    }
}