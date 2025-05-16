using System;
using System.Threading;
using System.Threading.Tasks;

namespace AikitWpfDemo
{
    /// <summary>
    /// 弹窗管理辅助类，负责弹窗的显示、隐藏、自动关闭
    /// </summary>
    public class PopupManager
    {
        private CortanaLikePopup _popup;
        private CancellationTokenSource _closeCts;

        public PopupManager()
        {
            _popup = new CortanaLikePopup();
        }

        public async Task ShowPopupWithAutoCloseAsync(string text, int ms = 4000)
        {
            _closeCts?.Cancel();
            _closeCts = new CancellationTokenSource();
            var token = _closeCts.Token;
            if (_popup == null || !_popup.IsLoaded)
                _popup = new CortanaLikePopup();
            if (!_popup.IsVisible)
            {
                _popup.Show();
                _popup.Activate();
            }
            _popup.RecognizedText = "";
            _popup.UpdateText(text);
            try
            {
                await Task.Delay(ms, token);
                if (token.IsCancellationRequested) return;
                if (_popup != null && _popup.IsVisible)
                    _popup.Hide();
            }
            catch (TaskCanceledException)
            {
                // 任务被取消，正常情况
            }
        }

        public void HidePopup()
        {
            _closeCts?.Cancel();
            if (_popup != null && _popup.IsVisible)
                _popup.Hide();
        }
    }
}
