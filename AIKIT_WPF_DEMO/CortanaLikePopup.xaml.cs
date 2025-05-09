using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input; // 添加鼠标输入命名空间
using System.Windows.Media.Animation; // Storyboard 需要
using System.Windows.Threading;

namespace AikitWpfDemo
{
    public partial class CortanaLikePopup : Window, INotifyPropertyChanged
    {
        private string _recognizedText;
        public string RecognizedText
        {
            get => _recognizedText;
            set
            {
                _recognizedText = value;
                OnPropertyChanged(); // 通知UI更改
            }
        }

        // 鼠标拖动相关变量
        private bool _isDragging = false;
        private Point _dragStartPoint;

        // 模拟语音识别的预设指令
        private List<string> _simulatedCommands = new List<string>
        {
            "我想切歌",
            "我想调大音量",
            "下一首歌",
            "暂停播放",
            "播放周杰伦的歌",
            "今天天气怎么样",
            "帮我设置一个5分钟的闹钟"
        };

        private int _currentCommandIndex = 0;
        private DispatcherTimer _displayTimer;  // 显示整句的定时器
        private DispatcherTimer _typingTimer;   // 模拟打字效果的定时器
        private string _currentFullCommand;     // 当前完整的指令
        private int _currentCharIndex = 0;      // 当前显示到的字符索引

        public CortanaLikePopup()
        {
            InitializeComponent();
            // 设置DataContext以支持绑定
            this.DataContext = this;
            
            // 初始化显示定时器
            _displayTimer = new DispatcherTimer();
            _displayTimer.Interval = TimeSpan.FromSeconds(2.5); // 每句话之间的间隔
            _displayTimer.Tick += DisplayTimer_Tick;

            // 初始化打字效果定时器
            _typingTimer = new DispatcherTimer();
            _typingTimer.Interval = TimeSpan.FromMilliseconds(100); // 打字速度
            _typingTimer.Tick += TypingTimer_Tick;

            // 添加鼠标事件处理
            this.MouseLeftButtonDown += Window_MouseLeftButtonDown;
            this.MouseMove += Window_MouseMove;
            this.MouseLeftButtonUp += Window_MouseLeftButtonUp;
        }

        #region 鼠标拖动实现
        
        // 鼠标左键按下事件处理
        private void Window_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            // 记录拖动起始点和状态
            _isDragging = true;
            _dragStartPoint = e.GetPosition(this);
            this.CaptureMouse(); // 捕获鼠标以跟踪移动
        }

        // 鼠标移动事件处理
        private void Window_MouseMove(object sender, MouseEventArgs e)
        {
            if (_isDragging)
            {
                // 计算鼠标移动的偏移量
                Point currentPosition = e.GetPosition(this);
                Vector offset = currentPosition - _dragStartPoint;

                // 更新窗口位置
                this.Left += offset.X;
                this.Top += offset.Y;
            }
        }

        // 鼠标左键释放事件处理
        private void Window_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            // 结束拖动状态
            if (_isDragging)
            {
                _isDragging = false;
                this.ReleaseMouseCapture(); // 释放鼠标捕获
            }
        }
        
        #endregion

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            // 窗口加载时启动动画
            Storyboard pulsingAnimation = (Storyboard)this.Resources["PulsingAnimation"];
            pulsingAnimation?.Begin(this, true);
            
            // 启动模拟识别
            StartSimulation();
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            StopSimulation();
            this.Hide(); // 或者 this.Close(); 如果你想完全释放它
        }

        // 启动模拟语音识别
        public void StartSimulation()
        {
            _currentCommandIndex = 0;
            ShowNextCommand();
        }

        // 停止模拟
        public void StopSimulation()
        {
            _displayTimer.Stop();
            _typingTimer.Stop();
        }

        // 显示下一条指令
        private void ShowNextCommand()
        {
            if (_currentCommandIndex < _simulatedCommands.Count)
            {
                _currentFullCommand = _simulatedCommands[_currentCommandIndex];
                _currentCharIndex = 0;
                RecognizedText = ""; // 清空当前文本
                _typingTimer.Start(); // 开始打字效果
                _currentCommandIndex++;
            }
            else
            {
                // 所有指令显示完毕，重新开始或停止
                _currentCommandIndex = 0; // 循环播放
                _displayTimer.Start();
            }
        }

        // 打字效果定时器事件
        private void TypingTimer_Tick(object sender, EventArgs e)
        {
            if (_currentCharIndex < _currentFullCommand.Length)
            {
                // 逐个字符添加，模拟打字效果
                RecognizedText += _currentFullCommand[_currentCharIndex];
                _currentCharIndex++;
            }
            else
            {
                // 当前指令显示完毕，停止打字效果，准备下一条
                _typingTimer.Stop();
                _displayTimer.Start(); // 启动显示定时器，准备下一条指令
            }
        }

        // 显示定时器事件
        private void DisplayTimer_Tick(object sender, EventArgs e)
        {
            _displayTimer.Stop();
            ShowNextCommand();
        }

        // 当语音被识别时，从你的主应用程序逻辑调用此方法
        public void UpdateText(string newText)
        {
            RecognizedText = newText;
        }

        // 当检测到你的唤醒词时调用此方法
        public void ShowPopup()
        {
            // 重置文本
            RecognizedText = "";
            this.Show();
            this.Activate(); // 置于顶层
            
            // 启动模拟
            StartSimulation();
        }

        // 添加一个自定义的测试方法，在主窗口中可以调用
        public void StartDemoMode()
        {
            this.Show();
            this.Activate();
            StartSimulation();
        }

        // INotifyPropertyChanged 实现
        public event PropertyChangedEventHandler PropertyChanged;
        protected virtual void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}