using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input; // 添加鼠标输入命名空间
using System.Windows.Media.Animation; // Storyboard 需要

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

        // 添加用于打字效果的成员
        private System.Windows.Threading.DispatcherTimer _typingEffectTimer;
        private string _fullTextForTyping;
        private int _currentCharTypingIndex;

        public CortanaLikePopup()
        {
            InitializeComponent();
            // 设置DataContext以支持绑定
            this.DataContext = this;

            // 初始化打字效果定时器
            _typingEffectTimer = new System.Windows.Threading.DispatcherTimer();
            _typingEffectTimer.Interval = TimeSpan.FromMilliseconds(100); // 打字速度，可以调整
            _typingEffectTimer.Tick += TypingEffectTimer_Tick;
            
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
            
            // 移除模拟识别启动
            // StartSimulation();
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            // 移除停止模拟的调用
            // StopSimulation();
            this.Hide(); // 或者 this.Close(); 如果你想完全释放它
        }

        // 当语音被识别时，从你的主应用程序逻辑调用此方法
        public void UpdateText(string newText)
        {
            // 停止任何正在进行的打字动画
            if (_typingEffectTimer != null)
            {
                _typingEffectTimer.Stop();
            }

            _fullTextForTyping = newText;
            _currentCharTypingIndex = 0;
            RecognizedText = ""; // 清空当前文本，准备开始打字

            if (!string.IsNullOrEmpty(_fullTextForTyping))
            {
                if (_typingEffectTimer == null) // 确保定时器已初始化
                {
                    _typingEffectTimer = new System.Windows.Threading.DispatcherTimer();
                    _typingEffectTimer.Interval = TimeSpan.FromMilliseconds(100); // 打字速度
                    _typingEffectTimer.Tick += TypingEffectTimer_Tick;
                }
                _typingEffectTimer.Start(); // 开始打字效果
            }
            else
            {
                RecognizedText = ""; // 如果新文本为空，直接清空
            }
        }

        // 打字效果定时器事件处理
        private void TypingEffectTimer_Tick(object sender, EventArgs e)
        {
            if (_currentCharTypingIndex < _fullTextForTyping.Length)
            {
                // 逐个字符添加
                RecognizedText += _fullTextForTyping[_currentCharTypingIndex];
                _currentCharTypingIndex++;
            }
            else
            {
                // 当前文本显示完毕
                _typingEffectTimer.Stop();
            }
        }

        // 当检测到你的唤醒词时调用此方法
        public void ShowPopup()
        {
            // 重置文本
            RecognizedText = "";
            this.Show();
            this.Activate(); // 置于顶层
            
            // 移除模拟启动
            // StartSimulation();
        }

        // 修改测试方法以适应实时语音识别
        public void StartDemoMode()
        {
            RecognizedText = "正在等待语音输入...";
            this.Show();
            this.Activate();
            // 移除模拟启动
            // StartSimulation();
        }

        // INotifyPropertyChanged 实现
        public event PropertyChangedEventHandler PropertyChanged;
        protected virtual void OnPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}