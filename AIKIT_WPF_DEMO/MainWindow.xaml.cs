using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using System.Runtime.InteropServices;
using WinForms = System.Windows.Forms;
using Microsoft.Win32;

namespace AikitWpfDemo
{
    public partial class MainWindow : Window
    {
        private CancellationTokenSource _cts;
        private bool _engineInitialized = false;

        // 各种管理器的引用
        private LogManager _logManager;
        private PopupManager _popupManager;
        private ResultMonitor _resultMonitor;
        private VoiceAssistantManager _voiceManager;

        public MainWindow()
        {
            InitializeComponent();
            _cts = new CancellationTokenSource();

            // 初始化各个管理器
            _logManager = LogManager.Instance;
            _logManager.Initialize(TxtLog);

            _popupManager = PopupManager.Instance;
            _resultMonitor = ResultMonitor.Instance;
            _voiceManager = VoiceAssistantManager.Instance;

            // 注册语音助手事件
            _voiceManager.StateChanged += VoiceManager_StateChanged;
            _voiceManager.CommandRecognized += VoiceManager_CommandRecognized;

            // 注册窗口关闭事件
            this.Closing += MainWindow_Closing;
        }

        // 启动唤醒测试按钮点击事件处理
        private void BtnStartWakeup_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 清空日志
                _logManager.ClearLog();
                _logManager.LogMessage("开始唤醒测试...");

                // 重置唤醒状态
                NativeMethods.ResetWakeupStatus();

                // 禁用按钮防止重复点击
                BtnStartWakeup.IsEnabled = false;

                // 启动唤醒测试
                int result = NativeMethods.StartWakeup();
                string detailedResult = NativeMethods.GetLastResultString();
                _logManager.LogMessage($"唤醒测试启动结果: {detailedResult}");

                if (NativeMethods.GetWakeupStatus() == 1)
                {
                    string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                    _logManager.LogMessage($"检测到唤醒词: {wakeupInfo}");

                    // 使用弹窗管理器显示弹窗
                    _popupManager.ManagePopupDisplay("你好，请问你需要做什么操作？", true);

                    // 重置唤醒状态，避免重复弹窗
                    NativeMethods.ResetWakeupStatus();

                    _logManager.LogMessage("已启动实时语音识别，等待用户命令...");
                }
                else
                {
                    _logManager.LogMessage("未检测到唤醒词，弹窗未显示");
                }

                // 重新启用按钮
                BtnStartWakeup.IsEnabled = true;
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"启动唤醒测试时发生异常: {ex.Message}");
                MessageBox.Show($"启动唤醒测试时发生异常: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                BtnStartWakeup.IsEnabled = true;

                // 确保清理资源
                try
                {
                    _resultMonitor.Stop();
                }
                catch
                {
                    // 忽略清理过程中的异常
                }
            }
        }

        // 运行完整测试并显示弹窗
        private void BtnRunFullTest_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 清空日志
                _logManager.ClearLog();
                _logManager.LogMessage("开始运行完整测试...");

                // 禁用按钮，防止重复点击
                BtnRunFullTest.IsEnabled = false;

                // 启动结果监控
                _resultMonitor.Start();
                _logManager.LogMessage("已启动实时识别结果监控");

                // 执行测试
                int result = NativeMethods.StartEsrMicrophone();

                // 获取并记录详细结果信息
                string detailedResult = NativeMethods.GetLastResultString();
                _logManager.LogMessage($"测试启动结果: {detailedResult}");

                if (result == 0)
                {
                    _logManager.LogMessage("测试完成，执行成功！");
                }
                else
                {
                    _logManager.LogMessage($"测试失败，错误码: {result}");
                }

                // 重新启用按钮
                BtnRunFullTest.IsEnabled = true;
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"启动测试时发生异常: {ex.Message}");
                MessageBox.Show($"启动测试时发生异常: {ex.Message}", "异常", MessageBoxButton.OK, MessageBoxImage.Error);
                BtnRunFullTest.IsEnabled = true;

                // 确保停止所有监控
                try
                {
                    _resultMonitor.Stop();
                }
                catch
                {
                    // 忽略清理过程中的异常
                }
            }
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
                _logManager.LogMessage($"识别到命令: {e.Command}, 成功: {e.Success}");

                // 在这里可以添加命令处理逻辑
                // ...
            });
        }

        // 窗口关闭事件
        protected override void OnClosed(EventArgs e)
        {
            try
            {
                // 取消任何正在进行的操作
                _cts?.Cancel();

                // 停止识别结果监控
                _resultMonitor.Stop();

                // 清理弹窗资源
                _popupManager.Cleanup();
            }
            catch
            {
                // 忽略关闭时的异常
            }
            base.OnClosed(e);
        }

        // 启动语音助手循环按钮点击事件
        private void BtnStartVoiceAssistant_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 清空日志并准备启动
                _logManager.ClearLog();
                _logManager.LogMessage("开始启动语音交互循环...");

                // 验证语音资源
                //if (!ValidateVoiceResources())
                //{
                //    return; // 验证失败，不继续执行
                //}

                // 禁用启动按钮，启用停止按钮
                BtnStartVoiceAssistant.IsEnabled = false;
                BtnStopVoiceAssistant.IsEnabled = true;

                // 重置唤醒和ESR状态
                NativeMethods.ResetWakeupStatus();
                NativeMethods.ResetEsrStatus();

                // 启动语音助手管理器
                bool success = _voiceManager.Start();
                if (!success)
                {
                    _logManager.LogMessage("启动语音助手失败");
                    BtnStartVoiceAssistant.IsEnabled = true;
                    BtnStopVoiceAssistant.IsEnabled = false;
                    return;
                }

                // 启动结果监控
                _resultMonitor.Start();

                _logManager.LogMessage("语音交互循环已启动，等待唤醒词...");
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"启动语音交互循环异常: {ex.Message}");

                // 确保在出错时UI状态正确
                BtnStartVoiceAssistant.IsEnabled = true;
                BtnStopVoiceAssistant.IsEnabled = false;

                // 如果有问题，尝试停止已启动的组件
                try
                {
                    _resultMonitor.Stop();
                    _voiceManager.Stop();
                }
                catch
                {
                    // 忽略清理过程中的异常
                }
            }
        }

        // 停止语音助手循环按钮点击事件
        private void BtnStopVoiceAssistant_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                _logManager.LogMessage("正在停止语音交互循环...");

                // 停止结果监控
                _resultMonitor.Stop();

                // 隐藏弹窗
                _popupManager.ManagePopupDisplay("", false);

                // 停止语音助手管理器
                _voiceManager.Stop();

                // 手动清理资源，确保安全停止
                NativeMethods.ResetWakeupStatus();
                NativeMethods.ResetEsrStatus();

                _logManager.LogMessage("语音交互循环已停止");

                // 更新UI状态
                BtnStartVoiceAssistant.IsEnabled = true;
                BtnStopVoiceAssistant.IsEnabled = false;
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"停止语音交互循环异常: {ex.Message}");

                // 确保按钮状态正确
                BtnStartVoiceAssistant.IsEnabled = true;
                BtnStopVoiceAssistant.IsEnabled = false;

                // 尝试强制停止所有组件
                try
                {
                    _resultMonitor.Stop();
                    _popupManager.ManagePopupDisplay("", false);
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
            // 使用环境验证器进行验证
            return EnvironmentValidator.Instance.ValidateVoiceResources();
        }

        // 测试唤醒检测功能
        private async void BtnTestWakeupDetection_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 禁用按钮，防止重复点击
                BtnTestWakeupDetection.IsEnabled = false;
                BtnTestWakeupDetection.Content = "测试中...";

                _logManager.LogMessage("开始测试唤醒检测功能...");

                // 异步调用测试函数
                await Task.Run(() =>
                {
                    int result = NativeMethods.TestWakeupDetection();
                    Dispatcher.Invoke(() =>
                    {
                        if (result == 1)
                        {
                            _logManager.LogMessage("唤醒检测测试成功：成功通过多路径检测到模拟的唤醒事件");
                            _popupManager.ManagePopupDisplay("唤醒检测测试成功", true);
                        }
                        else
                        {
                            _logManager.LogMessage("唤醒检测测试失败：未能检测到模拟的唤醒事件");
                            _popupManager.ManagePopupDisplay("唤醒检测测试失败", true);
                        }
                    });
                });
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"测试唤醒检测时出错: {ex.Message}");
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
                _logManager.LogMessage("获取唤醒状态详情...");

                // 获取唤醒状态详情
                IntPtr detailsPtr = NativeMethods.GetWakeupStatusDetails();
                string details = Marshal.PtrToStringAnsi(detailsPtr);

                // 格式化显示
                string formattedDetails = "=== 唤醒状态详情 ===\n" + details.Replace("\n", "\n");

                _logManager.LogMessage(formattedDetails);

                // 如果状态显示已唤醒，显示唤醒成功的提示
                if (details.Contains("WakeupFlag: 1") || details.Contains("LastEventType: 1"))
                {
                    _popupManager.ManagePopupDisplay("当前已处于唤醒状态", true);
                }
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"获取唤醒状态详情时出错: {ex.Message}");
                MessageBox.Show($"获取唤醒状态详情时出错: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
    }
}