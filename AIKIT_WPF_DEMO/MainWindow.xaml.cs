using System;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using WinForms = System.Windows.Forms; // 为System.Windows.Forms创建别名
using Microsoft.Win32;

namespace AikitWpfDemo
{
    public partial class MainWindow : Window
    {
        private CancellationTokenSource _cts;
        private bool _engineInitialized = false;
        private CortanaLikePopup _cortanaPopup;
        
        public MainWindow()
        {
            InitializeComponent();
            _cortanaPopup = new CortanaLikePopup();
            _cts = new CancellationTokenSource();
            
            // 窗口加载后执行总测试
            Loaded += (s, e) => 
            {
            LogMessage("自动开始执行完整测试...");
            // 重置唤醒状态，避免误报
            NativeMethods.ResetWakeupStatus();
            
            // 执行测试
            int result = NativeMethods.RunFullTest();
            
            // 获取并记录详细结果信息
            string detailedResult = NativeMethods.GetLastResultString();
            LogMessage(detailedResult);

            // 检查唤醒状态
            if (NativeMethods.GetWakeupStatus() == 1)
            {
                string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                LogMessage($"检测到唤醒词: {wakeupInfo}");
                OnWakeWordDetected();
            }
            };
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

        // 运行完整测试
        private void BtnRunFullTest_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 清空日志，准备显示新的测试信息
                TxtLog.Text = string.Empty;
                LogMessage("开始执行完整测试...");
                
                // 重置唤醒状态，避免误报
                NativeMethods.ResetWakeupStatus();
                
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
                    
                    // 使用CortanaLikePopup显示唤醒成功
                    OnWakeWordDetected();
                    // OnSpeechRecognized($"检测到唤醒词: {wakeupInfo}");
                    
                    // // 3秒后自动隐藏弹窗
                    // Task.Delay(3000).ContinueWith(_ => {
                    //     Dispatcher.Invoke(() => EndInteraction());
                    // });
                    
                    // // 重置唤醒状态，避免重复弹窗
                    // NativeMethods.ResetWakeupStatus();
                }

                // if (result == 0)
                // {
                //     LogMessage("测试完成，执行成功！");
                //     MessageBox.Show("测试完成！", "成功", MessageBoxButton.OK, MessageBoxImage.Information);
                // }
                // else
                // {
                //     LogMessage($"测试失败，错误码: {result}");
                //     MessageBox.Show($"测试失败，错误码: {result}\n\n详细信息: {detailedResult}",
                //         "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                // }

                // 重新启用按钮
                BtnRunFullTest.IsEnabled = true;
            }
            catch (Exception ex)
            {
                LogMessage($"启动测试线程时发生异常: {ex.Message}");
                MessageBox.Show($"启动测试时发生异常: {ex.Message}", "异常", MessageBoxButton.OK, MessageBoxImage.Error);
                BtnRunFullTest.IsEnabled = true;
            }
        }

        private void BtnRunTest_Click(object sender, RoutedEventArgs e)
        {
            // 清空日志，准备显示新的测试信息
            TxtLog.Text = string.Empty;
            LogMessage("开始执行完整测试...");

            // 重置唤醒状态，避免误报
            NativeMethods.ResetWakeupStatus();

            // 执行测试
            int result = NativeMethods.RunFullTest();
        }

        // 测试CortanaLikePopup弹窗 - 模拟语音识别效果
        private void BtnTestPopup_Click(object sender, RoutedEventArgs e)
        {
            // 启动模拟演示模式
            _cortanaPopup.StartDemoMode();
            LogMessage("已启动语音识别模拟演示");
        }

        // 供将来扩展使用的辅助方法
        private void OnWakeWordDetected()
        {
            _cortanaPopup.UpdateText("正在聆听...");
            _cortanaPopup.ShowPopup();
        }

        private void OnSpeechRecognized(string recognizedSpeech)
        {
            _cortanaPopup.UpdateText(recognizedSpeech);
        }

        private void EndInteraction()
        {
            _cortanaPopup.Hide();
        }
    }
}