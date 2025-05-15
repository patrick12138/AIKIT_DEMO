using System;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;

namespace AikitWpfDemo
{
    /// <summary>
    /// 弹窗管理器类，负责所有语音交互弹窗的管理
    /// </summary>
    public class PopupManager
    {
        private static PopupManager _instance;
        private CortanaLikePopup _cortanaPopup;
        
        private PopupManager()
        {
            _cortanaPopup = new CortanaLikePopup();
        }
        
        /// <summary>
        /// 获取PopupManager的单例实例
        /// </summary>
        public static PopupManager Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = new PopupManager();
                }
                return _instance;
            }
        }
        
        /// <summary>
        /// 显示弹窗并在指定时间后自动关闭
        /// </summary>
        /// <param name="text">要显示的文本</param>
        /// <param name="autoCloseMilliseconds">自动关闭的毫秒数，0表示不自动关闭</param>
        public async Task ShowPopupWithAutoCloseAsync(string text, int autoCloseMilliseconds = 5000)
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
        
        /// <summary>
        /// 控制弹窗显示的方法，集中处理弹窗逻辑
        /// </summary>
        /// <param name="text">要显示的文本</param>
        /// <param name="show">是否显示弹窗</param>
        public void ManagePopupDisplay(string text, bool show)
        {
            // 确保在UI线程上更新界面
            if (!Application.Current.Dispatcher.CheckAccess())
            {
                Application.Current.Dispatcher.Invoke(() => ManagePopupDisplay(text, show));
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
        
        /// <summary>
        /// 关闭并清理弹窗资源
        /// </summary>
        public void Cleanup()
        {
            try
            {
                // 安全关闭弹窗
                if (_cortanaPopup != null && _cortanaPopup.IsLoaded)
                {
                    _cortanaPopup.Hide();
                    _cortanaPopup = null;
                }
            }
            catch
            {
                // 忽略关闭时的异常
            }
        }
    }
}
