using System;
using System.Collections.Generic;

namespace AikitWpfDemo
{
    /// <summary>
    /// 命令词相关辅助类，负责命令集合加载和PGS文本解析
    /// </summary>
    public static class CommandHelper
    {
        public static HashSet<string> LoadValidCommands()
        {
            return new HashSet<string>
            {
                "播放下一首",
                "播放上一首",
                "开始播放",
                "暂停播放",
                "停止播放",
                "点歌",
                "插播",
                "删除歌曲",
                "提高音量",
                "降低音量",
                "静音",
                "重唱",
                "伴唱",
                "原唱",
                "已点歌曲"
            };
        }

        /// <summary>
        /// 解析PGS结果字符串，提取命令内容（去除前缀如"pgs:"，并去除首尾空白）
        /// 支持多行PGS，自动取最后一行有效命令，并兼容"pgs:"、"pgs "等前缀
        /// </summary>
        public static string ParsePgsText(string pgsRaw)
        {
            if (string.IsNullOrWhiteSpace(pgsRaw))
                return string.Empty;
            string[] lines = pgsRaw.Split(new[] { '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries);
            if (lines.Length == 0)
                return string.Empty;
            string lastLine = lines[lines.Length - 1].Trim();
            if (lastLine.StartsWith("pgs:", StringComparison.OrdinalIgnoreCase))
                lastLine = lastLine.Substring(4).Trim();
            else if (lastLine.StartsWith("pgs ", StringComparison.OrdinalIgnoreCase))
                lastLine = lastLine.Substring(4).Trim();
            return lastLine.Trim();
        }
    }
}
