#pragma once
#include <string>
#include <map>
#include <vector>
#include "resource.h"

#define g_data CDataManager::Instance()

struct SettingData
{
    bool show_Lag_in_tooltip{};
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
    std::wstring GetLatencyTooltipText() const;

    SettingData m_setting_data;
    ULONG_PTR m_gdiplusToken;

private:
    static CDataManager m_instance;
    std::wstring m_config_path;
    std::map<UINT, CString> m_string_table;
    std::map<UINT, HICON> m_icons;
    int m_dpi{ 96 };

    // 延迟测量数据
    std::vector<std::wstring> m_latencyHosts{
        L"lol.qq.com",      // 英雄联盟
        L"www.douyin.com",  // 抖音
        L"www.jd.com",      // 京东
        L"www.ctrip.com",   // 携程
        L"www.toutiao.com"  // 今日头条
    };
    std::vector<std::wstring> m_latencyHostNames{
        L"英雄联盟",
        L"抖音",
        L"京东",
        L"携程",
        L"今日头条"
    };
    std::vector<double> m_latencyMsPerHost; // 与 m_latencyHosts 对齐，失败为 -1
    ULONGLONG m_latencyLastUpdateTick{ 0 };
};
