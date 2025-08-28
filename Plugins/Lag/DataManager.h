#pragma once
#include <string>
#include <map>
#include <vector>
#include "resource.h"

#define g_data CDataManager::Instance()

#define Lag_COLOR_HIGH RGB(128, 194, 105)   //电池电量颜色——高
#define Lag_COLOR_LOW RGB(241, 197, 0)      //电池电量颜色——低
#define Lag_COLOR_CRITICAL RGB(206, 23, 0)  //电池电量颜色——电量不足

//电量显示样式
enum class LagType
{
    NUMBER,             //仅数字
    ICON,               //仅图标
    NUMBER_BESIDE_ICON  //数字显示在图标旁边
};

struct SettingData
{
    LagType Lag_type{};
    bool show_Lag_in_tooltip{};
    bool show_percent{};
    bool show_charging_animation{};     //显示充电动画
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
    bool IsAcOnline() const;        //电源是否已接通
    bool IsCharging() const;        //是否正在充电

    std::wstring GetLagString() const;
    COLORREF GetLagColor() const;

    // 网络延迟
    void RefreshLatency();
    double GetAverageLatencyMs() const;
    double GetMinLatencyMs() const;
    std::wstring GetLatencyTooltipText() const;

    SettingData m_setting_data;
    SYSTEM_POWER_STATUS m_sysPowerStatus{};   // 系统电量信息
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
