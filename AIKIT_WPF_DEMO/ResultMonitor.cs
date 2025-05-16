using System;
using System.Text;
using System.Windows.Threading;

namespace AikitWpfDemo
{
    /// <summary>
    /// 识别结果监控辅助类，负责定时器和结果处理
    /// </summary>
    public class ResultMonitor
    {
        private DispatcherTimer _timer;
        private Action<string> _onNewResult;
        private string _lastPgsResult = string.Empty;
        private string _lastPlainResult = string.Empty;
        private string _lastReadableResult = string.Empty;
        private bool _mergeResults = true;

        public ResultMonitor(Action<string> onNewResult, bool mergeResults = true)
        {
            _onNewResult = onNewResult;
            _mergeResults = mergeResults;
            _timer = new DispatcherTimer();
            _timer.Interval = TimeSpan.FromMilliseconds(50);
            _timer.Tick += Timer_Tick;
        }

        public void Start() => _timer.Start();
        public void Stop() => _timer.Stop();
        public bool IsEnabled => _timer.IsEnabled;

        private void Timer_Tick(object sender, EventArgs e)
        {
            StringBuilder resultBuilder = null;
            bool hasAnyNewResult = false;
            if (_mergeResults) resultBuilder = new StringBuilder();
            string currentPgsVal = NativeMethods.GetLatestPgsResult();
            if (currentPgsVal != null && currentPgsVal != _lastPgsResult)
            {
                _lastPgsResult = currentPgsVal;
                hasAnyNewResult = true;
                if (_mergeResults) resultBuilder.AppendLine(_lastPgsResult);
                else _onNewResult?.Invoke(_lastPgsResult);
            }
            string currentResult = NativeMethods.GetPlainResultString();
            if (!string.IsNullOrEmpty(currentResult) && currentResult != _lastPlainResult)
            {
                _lastPlainResult = currentResult;
                hasAnyNewResult = true;
                if (_mergeResults) resultBuilder.AppendLine(currentResult);
                else _onNewResult?.Invoke(currentResult);
            }
            string currentResult2 = NativeMethods.GetReadableResultString();
            if (!string.IsNullOrEmpty(currentResult2) && currentResult2 != _lastReadableResult)
            {
                _lastReadableResult = currentResult2;
                hasAnyNewResult = true;
                if (_mergeResults) resultBuilder.AppendLine(currentResult2);
                else _onNewResult?.Invoke(currentResult2);
            }
            if (hasAnyNewResult && _mergeResults && resultBuilder != null && resultBuilder.Length > 0)
            {
                _onNewResult?.Invoke(resultBuilder.ToString().TrimEnd());
            }
        }
    }
}
