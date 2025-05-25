using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using AikitWpfDemo;

namespace AikitWpfDemo
{
    /// <summary>
    /// AIKIT 统一语音交互演示窗口
    /// </summary>
    public partial class MainWindow : Window
    {
        // 私有字段
        private DispatcherTimer? _statusUpdateTimer;

        public MainWindow()
        {
            InitializeComponent();
            InitializeUI();
        }

        /// <summary>
        /// 初始化UI组件
        /// </summary>
        private void InitializeUI()
        {
            try
            {
                LogHelper.LogMessage("初始化AIKIT语音交互演示界面...");
                
                // 设置状态监控
                SetupStatusMonitoring();
                
                LogHelper.LogMessage("界面初始化完成，可以开始语音交互。");
                
                // 更新初始状态显示
                UpdateStatusDisplay();
            }
            catch (Exception ex)
            {
                LogHelper.LogMessage($"界面初始化过程中发生异常: {ex.Message}");
            }
        }

        /// <summary>
        /// 设置状态监控
        /// </summary>
        private void SetupStatusMonitoring()
        {
            _statusUpdateTimer = new DispatcherTimer();
            _statusUpdateTimer.Interval = TimeSpan.FromMilliseconds(500); // 每500ms更新一次状态
            _statusUpdateTimer.Tick += (s, e) => UpdateStatusDisplay();
            _statusUpdateTimer.Start();
        }

        /// <summary>
        /// 更新状态显示
        /// </summary>
        private void UpdateStatusDisplay()
        {
            try
            {
                // 检查统一语音交互是否正在运行
                int isRunning = NativeMethods.IsUnifiedVoiceInteractionRunning();
                bool voiceRunning = (isRunning == 1);

                TxtStatus.Text = $"系统状态: {(voiceRunning ? "运行中" : "空闲")}";

                if (voiceRunning)
                {
                    // 获取当前语音状态
                    int voiceState = NativeMethods.GetUnifiedVoiceState();
                    string stateName = GetVoiceStateName(voiceState);
                    TxtVoiceState.Text = $"语音状态: {stateName}";
                }
                else
                {
                    TxtVoiceState.Text = "语音状态: 未启动";
                }
            }
            catch (Exception)
            {
                LogHelper.LogMessage("更新状态显示时发生异常");
            }
        }

        /// <summary>
        /// 获取语音状态名称
        /// </summary>
        private string GetVoiceStateName(int state)
        {
            switch (state)
            {
                case 0: return "空闲";
                case 1: return "等待唤醒";
                case 2: return "等待命令";
                case 3: return "识别中";
                case 4: return "处理完成";
                default: return "未知状态";
            }
        }

        /// <summary>
        /// 开始语音交互按钮点击事件
        /// </summary>
        private void BtnStartVoice_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                LogHelper.LogMessage("启动统一语音交互...");

                // 获取界面参数
                int wakeupThreshold = 50; // 默认值
                int esrTimeout = 10;      // 默认值

                if (int.TryParse(TxtWakeupThreshold.Text, out int threshold))
                {
                    wakeupThreshold = threshold;
                }

                if (int.TryParse(TxtCommandTimeout.Text, out int timeout))
                {
                    esrTimeout = timeout;
                }

                // 启动统一语音交互
                int result = NativeMethods.StartUnifiedVoiceInteraction(wakeupThreshold, esrTimeout);
                
                if (result == 0)
                {
                    LogHelper.LogMessage($"统一语音交互启动成功！唤醒阈值: {wakeupThreshold}, 命令超时: {esrTimeout}秒");
                    BtnStartVoice.IsEnabled = false;
                    BtnStopVoice.IsEnabled = true;
                }
                else
                {
                    LogHelper.LogMessage($"统一语音交互启动失败，错误码: {result}");
                }
            }
            catch (Exception ex)
            {
                LogHelper.LogMessage($"启动语音交互时发生异常: {ex.Message}");
            }
        }

        /// <summary>
        /// 停止语音交互按钮点击事件
        /// </summary>
        private void BtnStopVoice_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                LogHelper.LogMessage("停止统一语音交互...");

                int result = NativeMethods.StopUnifiedVoiceInteraction();
                
                if (result == 0)
                {
                    LogHelper.LogMessage("统一语音交互已成功停止");
                }
                else
                {
                    LogHelper.LogMessage($"停止统一语音交互失败，错误码: {result}");
                }

                BtnStartVoice.IsEnabled = true;
                BtnStopVoice.IsEnabled = false;
            }
            catch (Exception ex)
            {
                LogHelper.LogMessage($"停止语音交互时发生异常: {ex.Message}");
                BtnStartVoice.IsEnabled = true;
                BtnStopVoice.IsEnabled = false;
            }
        }

        /// <summary>
        /// 清空日志按钮点击事件
        /// </summary>
        private void BtnClearLog_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                TxtLog.Text = string.Empty;
                LogHelper.LogMessage("日志已清空");
            }
            catch (Exception ex)
            {
                MessageBox.Show($"清空日志时发生异常: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        /// <summary>
        /// 窗口关闭事件
        /// </summary>
        protected override void OnClosed(EventArgs e)
        {
            try
            {
                // 停止状态监控定时器
                _statusUpdateTimer?.Stop();

                // 停止统一语音交互
                NativeMethods.StopUnifiedVoiceInteraction();
                LogHelper.LogMessage("程序关闭，已停止语音交互");
            }
            catch (Exception)
            {
                // 忽略清理时的异常
            }
            
            base.OnClosed(e);
        }
    }
}