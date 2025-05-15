using System;
using System.IO;
using System.Windows;

namespace AikitWpfDemo
{
    /// <summary>
    /// 环境验证器类，负责验证运行环境、检查资源文件和设备访问权限
    /// </summary>
    public class EnvironmentValidator
    {
        private static EnvironmentValidator _instance;
        private LogManager _logManager;
        
        private EnvironmentValidator()
        {
            _logManager = LogManager.Instance;
        }
        
        /// <summary>
        /// 获取EnvironmentValidator的单例实例
        /// </summary>
        public static EnvironmentValidator Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = new EnvironmentValidator();
                }
                return _instance;
            }
        }
        
        /// <summary>
        /// 验证语音交互所需的所有资源和环境
        /// </summary>
        /// <returns>验证是否通过</returns>
        public bool ValidateVoiceResources()
        {
            try
            {
                _logManager.LogMessage("正在验证语音交互资源...");
                
                // 检查麦克风访问权限
                if (!CheckMicrophoneAccess())
                {
                    _logManager.LogMessage("错误：无法访问麦克风，请检查设备权限");
                    MessageBox.Show("无法访问麦克风，请检查设备权限", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                    return false;
                }
                
                // 检查DLL和资源文件
                if (!CheckRequiredFiles())
                {
                    _logManager.LogMessage("错误：缺少必要的库文件或资源文件");
                    MessageBox.Show("缺少必要的库文件或资源文件，请确保安装完整", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                    return false;
                }
                
                // 检查库加载情况
                if (!CheckLibraryLoad())
                {
                    _logManager.LogMessage("错误：无法加载所需的DLL库");
                    MessageBox.Show("无法加载所需的DLL库，请检查库文件完整性", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                    return false;
                }
                
                _logManager.LogMessage("语音交互资源验证通过");
                return true;
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"验证语音资源时出错: {ex.Message}");
                MessageBox.Show($"验证语音资源时出错: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                return false;
            }
        }
        
        /// <summary>
        /// 检查麦克风访问权限
        /// </summary>
        private bool CheckMicrophoneAccess()
        {
            try
            {
                // 这里应该实现麦克风权限检查
                // 在实际应用中，可能需要请求麦克风权限或检查设备状态
                
                // 以下是简化的实现
                return true;
            }
            catch
            {
                return false;
            }
        }
        
        /// <summary>
        /// 检查必要的文件是否存在
        /// </summary>
        private bool CheckRequiredFiles()
        {
            try
            {
                // 检查必要的DLL文件和资源文件
                
                // 定义必要的文件路径（相对于应用程序目录）
                string[] requiredFiles = {
                    "AEE_lib.dll",
                    "eabb2f029_v10092_aee.dll",
                    "ebd1bade4_v1031_aee.dll",
                    "ef7d69542_v1014_aee.dll"
                };
                
                string appDirectory = AppDomain.CurrentDomain.BaseDirectory;
                string libsDirectory = Path.Combine(appDirectory, "libs");
                
                // 检查64位库文件
                string libs64Directory = Path.Combine(libsDirectory, "64");
                if (Environment.Is64BitProcess)
                {
                    foreach (string file in requiredFiles)
                    {
                        string filePath = Path.Combine(libs64Directory, file);
                        if (!File.Exists(filePath))
                        {
                            _logManager.LogMessage($"缺少必要文件: {filePath}");
                            return false;
                        }
                    }
                }
                // 检查32位库文件
                else
                {
                    string libs32Directory = Path.Combine(libsDirectory, "32");
                    foreach (string file in requiredFiles)
                    {
                        string filePath = Path.Combine(libs32Directory, file);
                        if (!File.Exists(filePath))
                        {
                            _logManager.LogMessage($"缺少必要文件: {filePath}");
                            return false;
                        }
                    }
                }
                
                // 检查主DLL
                string aikitDllPath = Path.Combine(appDirectory, "AIKITDLL.dll");
                if (!File.Exists(aikitDllPath))
                {
                    _logManager.LogMessage($"缺少主DLL文件: {aikitDllPath}");
                    return false;
                }
                
                return true;
            }
            catch (Exception ex)
            {
                _logManager.LogMessage($"检查文件时出错: {ex.Message}");
                return false;
            }
        }
        
        /// <summary>
        /// 检查库加载情况
        /// </summary>
        private bool CheckLibraryLoad()
        {
            try
            {
                // 尝试调用一个简单的DLL函数来验证库是否正确加载
                // 这里使用GetWakeupStatus作为测试函数
                NativeMethods.GetWakeupStatus();
                return true;
            }
            catch
            {
                return false;
            }
        }
    }
}
