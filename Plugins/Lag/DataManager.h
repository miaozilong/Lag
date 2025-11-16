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

struct SettingData
{
    bool show_Lag_in_tooltip{};
};

// 简单的线程池类
class ThreadPool
{
public:
    ThreadPool(size_t numThreads = 4);
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

class CDataManager
{
private:
    CDataManager();
    ~CDataManager();

public:
    static CDataManager& Instance();

    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;
    const CString& StringRes(UINT id);      //根据资源id获取一个字符串资源
    void DPIFromWindow(CWnd* pWnd);
    int DPI(int pixel);
    float DPIF(float pixel);
    int RDPI(int pixel);
    HICON GetIcon(UINT id);

    // 网络延迟
    void RefreshLatency();
    double GetAverageLatencyMs() const;
    double GetMinLatencyMs() const;
    double GetDomesticMinLatencyMs() const;
    double GetInternationalMinLatencyMs() const;
    std::wstring GetLatencyTooltipText() const;
    std::wstring GetDomesticLatencyText() const;
    std::wstring GetInternationalLatencyText() const;

    SettingData m_setting_data;
    ULONG_PTR m_gdiplusToken;

    // 持久连接管理（需要被静态函数访问）
    struct PersistentConnection
    {
        HINTERNET hSession{ nullptr };
        HINTERNET hConnect{ nullptr };
        bool useHttps{ true };
        DWORD64 lastUsedTick{ 0 };
        bool isValid{ false };
        
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

    // 延迟测量数据
    std::vector<std::wstring> m_domesticHosts{
        L"lol.qq.com",      // 英雄联盟
        L"www.douyin.com",  // 抖音
        L"www.jd.com",      // 京东
        L"www.ctrip.com",   // 携程
        L"www.toutiao.com"  // 今日头条
    };
    std::vector<std::wstring> m_domesticHostNames{
        L"英雄联盟",
        L"抖音",
        L"京东",
        L"携程",
        L"今日头条"
    };
    std::vector<std::wstring> m_internationalHosts{
        L"www.google.com",     // Google
        L"www.instagram.com",  // Instagram
        L"www.youtube.com",    // YouTube
        L"www.twitter.com",    // Twitter
        L"www.bbc.com"         // BBC
    };
    std::vector<std::wstring> m_internationalHostNames{
        L"Google",
        L"Instagram",
        L"YouTube",
        L"Twitter",
        L"BBC"
    };
    std::vector<double> m_domesticLatencyMs; // 国内站点延迟，失败为 -1
    std::vector<double> m_internationalLatencyMs; // 国际站点延迟，失败为 -1
    ULONGLONG m_latencyLastUpdateTick{ 0 };

    // 多线程相关
    mutable std::mutex m_latencyMutex; // 保护延迟数据的互斥锁
    std::atomic<bool> m_isRefreshing{ false }; // 是否正在刷新
    std::vector<std::future<double>> m_futures; // 异步任务
    static constexpr int NETWORK_TIMEOUT_MS = 3000; // 网络超时时间（毫秒）
    
    // 线程池
    std::unique_ptr<ThreadPool> m_threadPool; // 线程池实例
    
    mutable std::mutex m_connectionMutex; // 保护连接池的互斥锁
    std::map<std::wstring, std::unique_ptr<PersistentConnection>> m_connections; // 每个host的持久连接
    
    void CloseAllConnections();
};
