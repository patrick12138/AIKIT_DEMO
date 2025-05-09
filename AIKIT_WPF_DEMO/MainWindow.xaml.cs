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
        }
        private async void SimulateVoiceInteraction()
        {
            // 1. Simulate Wake Word Detected
            await System.Threading.Tasks.Task.Delay(2000); // Wait 2 seconds
            Application.Current.Dispatcher.Invoke(() => // Ensure UI updates on UI thread
            {
                _cortanaPopup.UpdateText("Listening..."); // Or keep the default text
                _cortanaPopup.ShowPopup();
            });


            // 2. Simulate "weather in london" being spoken
            await System.Threading.Tasks.Task.Delay(3000); // Wait 3 more seconds
            Application.Current.Dispatcher.Invoke(() =>
            {
                _cortanaPopup.UpdateText("weather in london");
            });

            // 3. Simulate another command (popup stays open)
            await System.Threading.Tasks.Task.Delay(4000);
            Application.Current.Dispatcher.Invoke(() =>
            {
                _cortanaPopup.UpdateText("what time is it?");
            });

            // The popup will remain open until the 'X' is clicked.
            // You would call _cortanaPopup.Hide() or _cortanaPopup.Close()
            // when your voice assistant determines the interaction is over.
        }

        // You would call this when your wake word is detected
        private void OnWakeWordDetected()
        {
            _cortanaPopup.UpdateText("Listening..."); // Or whatever initial text you want
            _cortanaPopup.ShowPopup();
        }

        // You would call this when your speech recognizer gets a result
        private void OnSpeechRecognized(string recognizedSpeech)
        {
            _cortanaPopup.UpdateText(recognizedSpeech);
            // Add logic here to process the command
        }

        // If you want to close the popup programmatically
        private void EndInteraction()
        {
            _cortanaPopup.Hide();
        }

        // Ensure the popup is closed when the main window closes
 
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

        // 浏览工作目录
        private void BtnBrowse_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new WinForms.FolderBrowserDialog();
            if (dialog.ShowDialog() == WinForms.DialogResult.OK)
            {
                TxtWorkDir.Text = dialog.SelectedPath;
            }
        }

        // 浏览资源文件
        private void BtnBrowseResource_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog();
            dialog.Filter = "文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*";
            if (dialog.ShowDialog() == true)
            {
                TxtResourcePath.Text = dialog.FileName;
            }
        }

        // 初始化SDK
        private void BtnInitSdk_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 获取参数
                string appId = TxtAppId.Text.Trim();
                string apiKey = TxtApiKey.Text.Trim();
                string apiSecret = TxtApiSecret.Text.Trim();
                string workDir = TxtWorkDir.Text.Trim();

                // 验证参数
                if (string.IsNullOrEmpty(appId) || string.IsNullOrEmpty(apiKey) ||
                    string.IsNullOrEmpty(apiSecret) || string.IsNullOrEmpty(workDir))
                {
                    System.Windows.MessageBox.Show("请填写完整的初始化参数", "参数错误", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }

                // 调用DLL初始化SDK
                int result = NativeMethods.InitializeSDK(appId, apiKey, apiSecret, workDir);
                string message = NativeMethods.GetLastResultString();

                if (result == 0)
                {
                    LogMessage($"SDK初始化成功: {message}");
                    BtnInitSdk.IsEnabled = false;
                    BtnInitEngine.IsEnabled = true;
                }
                else
                {
                    LogMessage($"SDK初始化失败: {message}");
                }
            }
            catch (Exception ex)
            {
                LogMessage($"SDK初始化异常: {ex.Message}");
                System.Windows.MessageBox.Show($"发生异常: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        // 初始化引擎
        private void BtnInitEngine_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string resourcePath = TxtResourcePath.Text.Trim();
                if (string.IsNullOrEmpty(resourcePath))
                {
                    System.Windows.MessageBox.Show("请指定资源文件路径", "参数错误", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }

                // 初始化语音唤醒引擎
                int result = NativeMethods.Ivw70Init(resourcePath);
                string message = NativeMethods.GetLastResultString();

                if (result == 0)
                {
                    _engineInitialized = true;
                    LogMessage($"引擎初始化成功: {message}");
                    BtnInitEngine.IsEnabled = false;
                    BtnStartMicrophone.IsEnabled = true;
                }
                else
                {
                    LogMessage($"引擎初始化失败: {message}");
                }
            }
            catch (Exception ex)
            {
                LogMessage($"引擎初始化异常: {ex.Message}");
                System.Windows.MessageBox.Show($"发生异常: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        // 启动麦克风唤醒
        private async void BtnStartMicrophone_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (!_engineInitialized)
                {
                    MessageBox.Show("请先初始化引擎", "操作顺序错误", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }

                // 获取唤醒阈值
                int threshold = (int)SliderThreshold.Value;

                // 更新按钮状态
                BtnStartMicrophone.IsEnabled = false;
                BtnStop.IsEnabled = true;
                BtnExit.IsEnabled = false;

                // 重置唤醒状态
                NativeMethods.ResetWakeupStatus();
                LogMessage($"开始麦克风唤醒监听 (阈值: {threshold})...");

                // 启动唤醒监听任务
                _cts = new CancellationTokenSource();
                await Task.Run(async () =>
                {
                    try
                    {
                        // 调用DLL启动麦克风唤醒
                        int result = NativeMethods.IvwFromMicrophone(threshold);
                        string message = NativeMethods.GetLastResultString();

                        Dispatcher.Invoke(() =>
                        {
                            if (result == 0)
                            {
                                LogMessage($"唤醒成功: {message}");
                                MessageBox.Show("语音唤醒成功！", "成功", MessageBoxButton.OK, MessageBoxImage.Information);
                            }
                            else
                            {
                                LogMessage($"唤醒失败或超时: {message}");
                            }
                        });
                    }
                    catch (Exception ex)
                    {
                        Dispatcher.Invoke(() => LogMessage($"唤醒过程异常: {ex.Message}"));
                    }
                    finally
                    {
                        // 恢复按钮状态
                        Dispatcher.Invoke(() =>
                        {
                            BtnStartMicrophone.IsEnabled = true;
                            BtnStop.IsEnabled = false;
                            BtnExit.IsEnabled = true;
                        });
                    }
                }, _cts.Token);
            }
            catch (Exception ex)
            {
                LogMessage($"启动麦克风唤醒异常: {ex.Message}");
                MessageBox.Show($"发生异常: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);

                // 恢复按钮状态
                BtnStartMicrophone.IsEnabled = true;
                BtnStop.IsEnabled = false;
                BtnExit.IsEnabled = true;
            }
        }

        // 停止唤醒监听
        private void BtnStop_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                _cts.Cancel();
                LogMessage("已停止唤醒监听");

                // 恢复按钮状态
                BtnStartMicrophone.IsEnabled = true;
                BtnStop.IsEnabled = false;
                BtnExit.IsEnabled = true;
            }
            catch (Exception ex)
            {
                LogMessage($"停止唤醒监听异常: {ex.Message}");
            }
        }

        // 退出应用
        private void BtnExit_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // 取消任何正在进行的操作
                _cts.Cancel();

                // 清理资源
                if (_engineInitialized)
                {
                    int result = NativeMethods.Ivw70Uninit();
                    LogMessage($"引擎清理: {NativeMethods.GetLastResultString()}");
                }

                // 关闭窗口
                Close();
            }
            catch (Exception ex)
            {
                LogMessage($"退出异常: {ex.Message}");
                MessageBox.Show($"退出时发生异常: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        // 窗口关闭事件
        protected override void OnClosed(EventArgs e)
        {
            try
            {
                // 确保资源被清理
                if (_engineInitialized)
                {
                    NativeMethods.Ivw70Uninit();
                    _cortanaPopup?.Close();
                }
            }
            catch
            {
                // 忽略关闭时的异常
            }
            base.OnClosed(e);
        }

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

                // 检查唤醒状态 - 这是关键改动
                if (NativeMethods.GetWakeupStatus() == 1)
                {
                    string wakeupInfo = NativeMethods.GetWakeupInfoStringResult();
                    LogMessage($"检测到唤醒词: {wakeupInfo}");
                    
                    // 弹窗显示唤醒成功
                    MessageBox.Show($"检测到唤醒词！\n详细信息: {wakeupInfo}", "唤醒成功", MessageBoxButton.OK, MessageBoxImage.Information);
                    
                    // 重置唤醒状态，避免重复弹窗
                    NativeMethods.ResetWakeupStatus();
                }

                if (result == 0)
                {
                    LogMessage("测试完成，执行成功！");
                    MessageBox.Show("测试完成！", "成功", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                else
                {
                    LogMessage($"测试失败，错误码: {result}");
                    MessageBox.Show($"测试失败，错误码: {result}\n\n详细信息: {detailedResult}",
                        "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                }

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
    }
}