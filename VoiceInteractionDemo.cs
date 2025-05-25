// 语音交互演示代码示例
using System;
using System.Threading.Tasks;
using System.Windows;

namespace AikitWpfDemo
{
    /// <summary>
    /// 语音交互使用示例
    /// </summary>
    public class VoiceInteractionDemo
    {
        private VoiceInteractionManager _voiceManager;
        private PopupManager _popupManager;
        
        public VoiceInteractionDemo()
        {
            // 初始化组件
            _popupManager = new PopupManager();
            _voiceManager = new VoiceInteractionManager(_popupManager);
            
            // 订阅事件
            _voiceManager.OnLogMessage += OnLogMessage;
            _voiceManager.OnCommandDetected += OnCommandDetected;
            _voiceManager.OnStateChanged += OnStateChanged;
        }
        
        /// <summary>
        /// 开始语音交互循环
        /// </summary>
        public async Task StartDemo()
        {
            Console.WriteLine("=== 语音交互演示开始 ===");
            
            // 初始化SDK（需要先调用NativeMethods.InitializeSDK）
            // int ret = NativeMethods.InitializeSDK(appID, apiKey, apiSecret, workDir);
            
            // 启动语音交互循环
            await _voiceManager.StartInteractionLoop();
            
            Console.WriteLine("语音交互循环已启动，等待唤醒词...");
        }
        
        /// <summary>
        /// 停止语音交互循环
        /// </summary>
        public async Task StopDemo()
        {
            Console.WriteLine("=== 停止语音交互演示 ===");
            await _voiceManager.StopInteractionLoop();
            _voiceManager.Dispose();
        }
        
        /// <summary>
        /// 日志事件处理
        /// </summary>
        private void OnLogMessage(string message)
        {
            Console.WriteLine($"[LOG] {DateTime.Now:HH:mm:ss} {message}");
        }
        
        /// <summary>
        /// 命令检测事件处理
        /// </summary>
        private void OnCommandDetected(string command)
        {
            Console.WriteLine($"[COMMAND] 检测到命令: {command}");
            
            // 执行具体的命令处理逻辑
            switch (command.ToLower())
            {
                case "打开设置":
                    ExecuteOpenSettings();
                    break;
                    
                case "关闭程序":
                    ExecuteExitProgram();
                    break;
                    
                case "播放音乐":
                    ExecutePlayMusic();
                    break;
                    
                case "停止音乐":
                    ExecuteStopMusic();
                    break;
                    
                default:
                    Console.WriteLine($"[COMMAND] 未知命令: {command}");
                    break;
            }
        }
        
        /// <summary>
        /// 状态变化事件处理
        /// </summary>
        private void OnStateChanged(VoiceState newState)
        {
            Console.WriteLine($"[STATE] 状态切换到: {newState}");
            
            // 根据状态执行相应的UI更新
            switch (newState)
            {
                case VoiceState.Idle:
                    Console.WriteLine("    → 系统待机，监听唤醒词中...");
                    break;
                    
                case VoiceState.WakeupDetected:
                    Console.WriteLine("    → 唤醒成功！");
                    break;
                    
                case VoiceState.ListeningCommand:
                    Console.WriteLine("    → 请说出命令词...");
                    break;
                    
                case VoiceState.ProcessingResult:
                    Console.WriteLine("    → 正在处理识别结果...");
                    break;
                    
                case VoiceState.Timeout:
                    Console.WriteLine("    → 识别超时，返回待机状态");
                    break;
                    
                case VoiceState.Error:
                    Console.WriteLine("    → 发生错误，请检查日志");
                    break;
            }
        }
        
        // 命令执行方法示例
        private void ExecuteOpenSettings()
        {
            Console.WriteLine("[ACTION] 执行打开设置...");
            // TODO: 实现打开设置的具体逻辑
        }
        
        private void ExecuteExitProgram()
        {
            Console.WriteLine("[ACTION] 执行关闭程序...");
            // TODO: 实现程序退出逻辑
            Application.Current?.Shutdown();
        }
        
        private void ExecutePlayMusic()
        {
            Console.WriteLine("[ACTION] 执行播放音乐...");
            // TODO: 实现音乐播放逻辑
        }
        
        private void ExecuteStopMusic()
        {
            Console.WriteLine("[ACTION] 执行停止音乐...");
            // TODO: 实现音乐停止逻辑
        }
    }
    
    /// <summary>
    /// 简化的使用示例
    /// </summary>
    public class SimpleVoiceDemo
    {
        public static async Task QuickStart()
        {
            // 1. 创建弹窗管理器
            var popupManager = new PopupManager();
            
            // 2. 创建语音交互管理器
            var voiceManager = new VoiceInteractionManager(popupManager);
            
            // 3. 订阅命令检测事件
            voiceManager.OnCommandDetected += (command) => {
                Console.WriteLine($"收到命令: {command}");
            };
            
            // 4. 启动语音交互
            await voiceManager.StartInteractionLoop();
            
            // 现在系统会自动：
            // - 监听唤醒词
            // - 唤醒后播放提示音
            // - 监听命令词
            // - 处理识别结果
            // - 返回待机状态循环
            
            Console.WriteLine("语音交互已启动，说'你好小飞'来唤醒系统");
        }
    }
}
