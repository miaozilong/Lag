#pragma once
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <queue>
#include <condition_variable>
#include <functional>
#include <memory>
#include <winhttp.h>
#include "resource.h"

#define g_data CDataManager::Instance()

// ============================================================================
// 配置常量
// ============================================================================
namespace LatencyConfig
{
    constexpr int NETWORK_TIMEOUT_MS = 3000;        // 网络超时时间（毫秒）
    constexpr ULONGLONG REFRESH_INTERVAL_MS = 3000; // 刷新间隔（毫秒）
    constexpr size_t THREAD_POOL_SIZE = 8;          // 线程池大小（国内5个 + 国际3个 = 8个站点）
}

// ============================================================================
// 设置数据结构
// ============================================================================
struct SettingData
{
    bool show_Lag_in_tooltip{};  // 是否在提示中显示延迟信息
};

// ============================================================================
// 站点信息结构
// ============================================================================
struct HostInfo
{
    std::wstring host;      // 主机名
    std::wstring name;      // 显示名称
    
    HostInfo(const wchar_t* h, const wchar_t* n) : host(h), name(n) {}
};

// ============================================================================
// 线程池类：管理并发任务执行
// ============================================================================
class ThreadPool
{
public:
    ThreadPool(size_t numThreads = LatencyConfig::THREAD_POOL_SIZE);
    ~ThreadPool();
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    void shutdown();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};

// ============================================================================
// 数据管理器类：单例模式，管理所有延迟测量数据
// ============================================================================
class CDataManager
{
private:
    CDataManager();
    ~CDataManager();

public:
    static CDataManager& Instance();

    // 配置管理
    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;
    
    // UI 辅助函数
    const CString& StringRes(UINT id);
    void DPIFromWindow(CWnd* pWnd);
    int DPI(int pixel);
    float DPIF(float pixel);
    int RDPI(int pixel);
    HICON GetIcon(UINT id);

    // 延迟测量核心接口
    void RefreshLatency();                          // 刷新延迟数据（异步）
    double GetAverageLatencyMs() const;             // 获取平均延迟
    double GetMinLatencyMs() const;                 // 获取最小延迟（所有站点）
    double GetDomesticMinLatencyMs() const;         // 获取国内站点最小延迟
    double GetInternationalMinLatencyMs() const;    // 获取国际站点最小延迟
    
    // 延迟显示接口
    std::wstring GetLatencyTooltipText() const;     // 获取提示文本（完整信息）
    std::wstring GetDomesticLatencyText() const;    // 获取国内延迟文本
    std::wstring GetInternationalLatencyText() const; // 获取国际延迟文本

    SettingData m_setting_data;
    ULONG_PTR m_gdiplusToken;

    // ========================================================================
    // 持久连接管理（需要被静态函数访问）
    // ========================================================================
    struct PersistentConnection
    {
        HINTERNET hSession{ nullptr };    // WinHTTP 会话句柄
        HINTERNET hConnect{ nullptr };    // WinHTTP 连接句柄
        bool useHttps{ true };            // 是否使用 HTTPS
        DWORD64 lastUsedTick{ 0 };        // 最后使用时间戳
        bool isValid{ false };            // 连接是否有效
        
        ~PersistentConnection()
        {
            Close();
        }
        
        void Close()
        {
            if (hConnect)
            {
                WinHttpCloseHandle(hConnect);
                hConnect = nullptr;
            }
            if (hSession)
            {
                WinHttpCloseHandle(hSession);
                hSession = nullptr;
            }
            isValid = false;
        }
    };
    
    PersistentConnection* GetOrCreateConnection(const std::wstring& host);
    bool IsConnectionValid(PersistentConnection* conn) const;

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    int m_dpi{ 96 };

    // ========================================================================
    // 站点配置数据
    // ========================================================================
    std::vector<HostInfo> m_domesticHosts{
        { L"lol.qq.com",      L"英雄联盟" },
        { L"www.douyin.com",  L"抖音" },
        { L"www.jd.com",      L"京东" },
        { L"www.ctrip.com",   L"携程" },
        { L"www.toutiao.com", L"今日头条" }
    };
    
    std::vector<HostInfo> m_internationalHosts{
        { L"www.google.com",  L"Google" },
        { L"www.youtube.com", L"YouTube" },
        { L"x.com",           L"X" }
    };
    
    // ========================================================================
    // 延迟测量结果数据
    // ========================================================================
    std::vector<double> m_domesticLatencyMs;        // 国内站点延迟（ms），失败为 -1
    std::vector<double> m_internationalLatencyMs;   // 国际站点延迟（ms），失败为 -1
    ULONGLONG m_latencyLastUpdateTick{ 0 };         // 最后更新时间戳

    // ========================================================================
    // 线程同步相关
    // ========================================================================
    mutable std::mutex m_latencyMutex;              // 保护延迟数据的互斥锁
    std::atomic<bool> m_isRefreshing{ false };      // 是否正在刷新（防止重复刷新）
    std::unique_ptr<ThreadPool> m_threadPool;       // 线程池实例

    // ========================================================================
    // 持久连接池管理
    // ========================================================================
    mutable std::mutex m_connectionMutex;           // 保护连接池的互斥锁
    std::map<std::wstring, std::unique_ptr<PersistentConnection>> m_connections; // 连接池
    
    // 私有辅助方法
    void CloseAllConnections();
    
    // 延迟计算辅助方法
    double CalculateMinLatency(const std::vector<double>& latencies) const;
    double CalculateAverageLatency(const std::vector<double>& latencies) const;
};
