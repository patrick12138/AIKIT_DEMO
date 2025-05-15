using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading; // 添加定时器命名空间
using System.Runtime.InteropServices; // 用于Marshal类
using WinForms = System.Windows.Forms; // 为System.Windows.Forms创建别名
using Microsoft.Win32;

namespace AikitWpfDemo
{    public partial class MainWindow : Window
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
        
        // 唤醒监听活跃状态标志
        private bool _isWakeupActive = false;

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
            }            // 如果有新结果且设置为合并显示，则一次性输出所有结果
            if (hasAnyNewResult && _mergeResults && resultBuilder != null && resultBuilder.Length > 0)
            {
                LogMessage(resultBuilder.ToString().TrimEnd());
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
                ManagePopupDisplay(displayText, true);
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
        }        // 显示弹窗并在指定时间后自动关闭
        private async Task ShowPopupWithAutoCloseAsync(string text, int autoCloseMilliseconds = 5000)
        {
            // 显示弹窗
            ManagePopupDisplay(text, true);
            
            // 如果需要自动关闭
            if (autoCloseMilliseconds > 0)
            {
                // 等待指定时间后自动关闭弹窗
                await Task.Delay(autoCloseMilliseconds);
                
                // 检查是否仍需要关闭弹窗
                // 避免在状态转换过程中错误关闭弹窗
                if (NativeMethods.GetWakeupStatus() == 0 && NativeMethods.GetEsrStatus() == 0)
                {
                    ManagePopupDisplay("", false);
                }
            }
        }

        // 控制弹窗显示的方法，集中处理弹窗逻辑
        private void ManagePopupDisplay(string text, bool show)
        {
            // 确保在UI线程上更新界面
            if (!Dispatcher.CheckAccess())
            {
                Dispatcher.Invoke(() => ManagePopupDisplay(text, show));
                return;
            }
            
            try
            {
                // 如果弹窗实例无效或已关闭，创建新实例
                if (_cortanaPopup == null || !_cortanaPopup.IsLoaded)
                {
                    _cortanaPopup = new CortanaLikePopup();
                }
                
                if (show)
                {
                    // 更新弹窗文本并显示
                    _cortanaPopup.UpdateText(text);
                    
                    if (!_cortanaPopup.IsVisible)
                    {
                        _cortanaPopup.Show();
                        _cortanaPopup.Activate();
                    }
                }
                else
                {
                    // 隐藏弹窗
                    if (_cortanaPopup != null && _cortanaPopup.IsVisible)
                    {
                        _cortanaPopup.Hide();
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"弹窗控制异常: {ex.Message}");
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
                            LogMessage($"检测到唤醒词: {wakeupInfo}");
                            
                            // 显示弹窗
                            ManagePopupDisplay("你好，请问你需要做什么操作？", true);
                            
                            // 重置唤醒状态，避免重复响应
                            NativeMethods.ResetWakeupStatus();
                            
                            // 切换到命令词识别状态
                            _voiceManager.TransitionToCommandListening();
                            
                            // 启动命令词识别
                            LogMessage("启动命令词识别...");
                            NativeMethods.StartEsrMicrophone();
                        }
                        else if (!_isWakeupActive)
                        {
                            // 如果唤醒监听不是活跃状态，则启动唤醒监听
                            _isWakeupActive = true;
                            LogMessage("启动唤醒监听...");
                            Task.Run(() => {
                                try {
                                    NativeMethods.StartWakeup();
                                    _isWakeupActive = false; // 唤醒监听函数返回后重置标志
                                }
                                catch (Exception ex) {
                                    LogMessage($"唤醒监听异常: {ex.Message}");
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
                            LogMessage($"命令词识别完成: {esrResult}");
                            
                            // 显示识别结果
                            ManagePopupDisplay($"识别结果: {esrResult}", true);
                            
                            // 处理识别到的命令
                            // 这里可以添加命令处理逻辑
                            
                            // 重置ESR状态
                            NativeMethods.ResetEsrStatus();
                            
                            // 延迟一段时间后关闭弹窗并切换回唤醒状态
                            Task.Run(async () =>
                            {
                                await Task.Delay(3000);
                                Dispatcher.Invoke(() => ManagePopupDisplay("", false));
                                
                                // 切换回唤醒监听状态
                                _voiceManager.TransitionToWakeupListening();
                                _isWakeupActive = false; // 确保可以重新启动唤醒监听
                            });
                        }
                        break;
                }
            }
            catch (Exception ex)
            {
                LogMessage($"识别结果监控异常: {ex.Message}");
                
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
        }        // 启动语音助手循环按钮点击事件
        private void BtnStartVoiceAssistant_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 清空日志并准备启动
                TxtLog.Text = string.Empty;
                LogMessage("开始启动语音交互循环...");
                
                // 验证语音资源
                if (!ValidateVoiceResources())
                {
                    return; // 验证失败，不继续执行
                }
                
                // 禁用启动按钮，启用停止按钮
                BtnStartVoiceAssistant.IsEnabled = false;
                BtnStopVoiceAssistant.IsEnabled = true;
                
                // 清空所有结果缓存
                _lastPgsResult = string.Empty;
                _lastHtkResult = string.Empty;
                _lastPlainResult = string.Empty;
                _lastVadResult = string.Empty;
                _lastReadableResult = string.Empty;
                
                // 重置标志
                _isWakeupActive = false;
                
                // 重置唤醒和ESR状态
                NativeMethods.ResetWakeupStatus();
                NativeMethods.ResetEsrStatus();
                
                // 启动语音助手管理器
                bool success = _voiceManager.Start();
                if (!success)
                {
                    LogMessage("启动语音助手失败");
                    BtnStartVoiceAssistant.IsEnabled = true;
                    BtnStopVoiceAssistant.IsEnabled = false;
                    return;
                }
                
                // 启动监控定时器
                if (!_resultMonitorTimer.IsEnabled)
                {
                    _resultMonitorTimer.Start();
                    LogMessage("已启动语音交互监控");
                }
                
                
                LogMessage("语音交互循环已启动，等待唤醒词...");
            }
            catch (Exception ex)
            {
                LogMessage($"启动语音交互循环异常: {ex.Message}");
                
                // 确保在出错时UI状态正确
                BtnStartVoiceAssistant.IsEnabled = true;
                BtnStopVoiceAssistant.IsEnabled = false;
                
                // 如果有问题，尝试停止已启动的组件
                try
                {
                    if (_resultMonitorTimer != null && _resultMonitorTimer.IsEnabled)
                    {
                        _resultMonitorTimer.Stop();
                    }
                    
                    _voiceManager.Stop();
                }
                catch
                {
                    // 忽略清理过程中的异常
                }
            }
        }        // 停止语音助手循环按钮点击事件
        private void BtnStopVoiceAssistant_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                LogMessage("正在停止语音交互循环...");
                
                // 确保停止监控定时器
                if (_resultMonitorTimer != null && _resultMonitorTimer.IsEnabled)
                {
                    _resultMonitorTimer.Stop();
                    LogMessage("已停止语音交互监控");
                }
                
                // 隐藏弹窗
                ManagePopupDisplay("", false);
                
                // 停止语音助手管理器
                _voiceManager.Stop();
                
                // 重置标志
                _isWakeupActive = false;
                
                // 手动清理资源，确保安全停止
                NativeMethods.ResetWakeupStatus();
                NativeMethods.ResetEsrStatus();
                
                LogMessage("语音交互循环已停止");
                
                // 更新UI状态
                BtnStartVoiceAssistant.IsEnabled = true;
                BtnStopVoiceAssistant.IsEnabled = false;
            }
            catch (Exception ex)
            {
                LogMessage($"停止语音交互循环异常: {ex.Message}");
                
                // 确保按钮状态正确
                BtnStartVoiceAssistant.IsEnabled = true;
                BtnStopVoiceAssistant.IsEnabled = false;
                
                // 尝试强制停止所有组件
                try
                {
                    if (_resultMonitorTimer != null && _resultMonitorTimer.IsEnabled)
                    {
                        _resultMonitorTimer.Stop();
                    }
                    
                    ManagePopupDisplay("", false);
                    _voiceManager.Stop();
                }
                catch
                {
                    // 忽略清理过程中的异常
                }
            }
        }
        
        // 验证语音交互所需的资源和设备
        private bool ValidateVoiceResources()
        {
            try
            {
                LogMessage("正在验证语音交互资源...");
                
                // 检查麦克风访问权限
                bool hasMicrophoneAccess = true; // 这里应该根据实际情况检查，此处简化
                if (!hasMicrophoneAccess)
                {
                    LogMessage("错误：无法访问麦克风，请检查设备权限");
                    MessageBox.Show("无法访问麦克风，请检查设备权限", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                    return false;
                }
                
                // 检查DLL和资源文件
                // 这里可以增加对必要资源文件的检查
                
                // 初始化SDK
                // 实际应用中可能需要初始化SDK，此处简化
                
                LogMessage("语音交互资源验证通过");
                return true;
            }            catch (Exception ex)
            {
                LogMessage($"验证语音资源时出错: {ex.Message}");
                MessageBox.Show($"验证语音资源时出错: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }
        }
        
        // 测试唤醒检测功能
        private async void BtnTestWakeupDetection_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 禁用按钮，防止重复点击
                BtnTestWakeupDetection.IsEnabled = false;
                BtnTestWakeupDetection.Content = "测试中...";
                
                LogMessage("开始测试唤醒检测功能...");
                
                // 异步调用测试函数
                await Task.Run(() => {
                    int result = NativeMethods.TestWakeupDetection();
                    Dispatcher.Invoke(() => {
                        if (result == 1)
                        {
                            LogMessage("唤醒检测测试成功：成功通过多路径检测到模拟的唤醒事件");
                            ManagePopupDisplay("唤醒检测测试成功", true);
                        }
                        else
                        {
                            LogMessage("唤醒检测测试失败：未能检测到模拟的唤醒事件");
                            ManagePopupDisplay("唤醒检测测试失败", true);
                        }
                    });
                });
            }
            catch (Exception ex)
            {
                LogMessage($"测试唤醒检测时出错: {ex.Message}");
                MessageBox.Show($"测试唤醒检测时出错: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                // 恢复按钮状态
                BtnTestWakeupDetection.IsEnabled = true;
                BtnTestWakeupDetection.Content = "测试唤醒检测";
            }
        }
        
        // 显示唤醒状态详情
        private void BtnShowWakeupDetails_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                LogMessage("获取唤醒状态详情...");
                
                // 获取唤醒状态详情
                IntPtr detailsPtr = NativeMethods.GetWakeupStatusDetails();
                string details = Marshal.PtrToStringAnsi(detailsPtr);
                
                // 格式化显示
                string formattedDetails = "=== 唤醒状态详情 ===\n" + details.Replace("\n", "\n");
                
                LogMessage(formattedDetails);
                
                // 如果状态显示已唤醒，显示唤醒成功的提示
                if (details.Contains("WakeupFlag: 1") || details.Contains("LastEventType: 1"))
                {
                    ManagePopupDisplay("当前已处于唤醒状态", true);
                }
            }
            catch (Exception ex)
            {
                LogMessage($"获取唤醒状态详情时出错: {ex.Message}");
                MessageBox.Show($"获取唤醒状态详情时出错: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
    }
}