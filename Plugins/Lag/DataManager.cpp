#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>
#include <Gdiplus.h>
#pragma comment(lib,"GdiPlus.lib")
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")

CDataManager CDataManager::m_instance;

CDataManager::CDataManager()
{
    //初始化DPI
    HDC hDC = ::GetDC(HWND_DESKTOP);
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(HWND_DESKTOP, hDC);

    //初始化GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
}

CDataManager::~CDataManager()
{
    SaveConfig();
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

static void WritePrivateProfileInt(const wchar_t* app_name, const wchar_t* key_name, int value, const wchar_t* file_path)
{
    wchar_t buff[16];
    swprintf_s(buff, L"%d", value);
    WritePrivateProfileString(app_name, key_name, buff, file_path);
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
    //获取模块的路径
    HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    std::wstring module_path = path;
    m_config_path = module_path;
    if (!config_dir.empty())
    {
        size_t index = module_path.find_last_of(L"\\/");
        //模块的文件名
        std::wstring module_file_name = module_path.substr(index + 1);
        m_config_path = config_dir + module_file_name;
    }
    m_config_path += L".ini";
    m_setting_data.Lag_type = static_cast<LagType>(GetPrivateProfileInt(L"config", L"Lag_type", static_cast<int>(LagType::NUMBER_BESIDE_ICON), m_config_path.c_str()));
    m_setting_data.show_Lag_in_tooltip = (GetPrivateProfileInt(L"config", L"show_Lag_in_tooltip", 1, m_config_path.c_str()) != 0);
    m_setting_data.show_percent = (GetPrivateProfileInt(L"config", L"show_percent", 1, m_config_path.c_str()) != 0);
    m_setting_data.show_charging_animation = (GetPrivateProfileInt(L"config", L"show_charging_animation", 0, m_config_path.c_str()) != 0);
}

void CDataManager::SaveConfig() const
{
    if (!m_config_path.empty())
    {
        WritePrivateProfileInt(L"config", L"Lag_type", static_cast<int>(m_setting_data.Lag_type), m_config_path.c_str());
        WritePrivateProfileInt(L"config", L"show_Lag_in_tooltip", m_setting_data.show_Lag_in_tooltip, m_config_path.c_str());
        WritePrivateProfileInt(L"config", L"show_percent", m_setting_data.show_percent, m_config_path.c_str());
        WritePrivateProfileInt(L"config", L"show_charging_animation", m_setting_data.show_charging_animation, m_config_path.c_str());
    }
}

const CString& CDataManager::StringRes(UINT id)
{
    auto iter = m_string_table.find(id);
    if (iter != m_string_table.end())
    {
        return iter->second;
    }
    else
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        //m_string_table[id].LoadString(id);
        return m_string_table[id];
    }
}

void CDataManager::DPIFromWindow(CWnd* pWnd)
{
    CWindowDC dc(pWnd);
    HDC hDC = dc.GetSafeHdc();
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
}

int CDataManager::DPI(int pixel)
{
    return m_dpi * pixel / 96;
}

float CDataManager::DPIF(float pixel)
{
    return m_dpi * pixel / 96;
}

int CDataManager::RDPI(int pixel)
{
    return pixel * 96 / m_dpi;
}

HICON CDataManager::GetIcon(UINT id)
{
    auto iter = m_icons.find(id);
    if (iter != m_icons.end())
    {
        return iter->second;
    }
    else
    {
        AFX_MANAGE_STATE(AfxGetStaticModuleState());
        HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(id), IMAGE_ICON, DPI(16), DPI(16), 0);
        m_icons[id] = hIcon;
        return hIcon;
    }
}


bool CDataManager::IsAcOnline() const
{
    return m_sysPowerStatus.ACLineStatus == 1;
}

bool CDataManager::IsCharging() const
{
    return m_sysPowerStatus.BatteryFlag == 8;
}

std::wstring CDataManager::GetLagString() const
{
    if (m_sysPowerStatus.BatteryFlag == 128)
    {
        return L"N/A";
    }
    else
    {
        std::wstring str = std::to_wstring(m_sysPowerStatus.BatteryLifePercent);
        if (m_setting_data.show_percent)
        {
            if (m_sysPowerStatus.BatteryLifePercent < 100)
                str.push_back(L' ');
            str.push_back(L'%');
        }
        return str;
    }
}

COLORREF CDataManager::GetLagColor() const
{
    if (m_sysPowerStatus.BatteryLifePercent < 20)
        return Lag_COLOR_CRITICAL;
    else if (m_sysPowerStatus.BatteryLifePercent < 60)
        return Lag_COLOR_LOW;
    else
        return Lag_COLOR_HIGH;
}

static DWORD64 GetTickMs()
{
    return static_cast<DWORD64>(GetTickCount64());
}

static bool MeasureHttpHeadLatencyMs(const std::wstring& host, double& out_ms)
{
    out_ms = -1.0;
    HINTERNET hSession = WinHttpOpen(L"LagPlugin/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    // 先尝试 HTTPS，不行再退回 HTTP
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET hRequest = hConnect ? WinHttpOpenRequest(hConnect, L"HEAD", L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
    if (!hRequest)
    {
        if (hConnect) WinHttpCloseHandle(hConnect);
        hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTP_PORT, 0);
        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            return false;
        }
        hRequest = WinHttpOpenRequest(hConnect, L"HEAD", L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
    }
    DWORD64 t0 = GetTickMs();
    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, NULL);
    DWORD64 t1 = GetTickMs();
    if (ok)
    {
        out_ms = static_cast<double>(t1 - t0);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok == TRUE;
}

void CDataManager::RefreshLatency()
{
    // 简单节流：5秒内不重复测量
    DWORD64 now = GetTickMs();
    if (m_latencyLastUpdateTick != 0 && now - m_latencyLastUpdateTick < 5000) return;
    m_latencyLastUpdateTick = now;

    m_latencyMsPerHost.assign(m_latencyHosts.size(), -1.0);
    for (size_t i = 0; i < m_latencyHosts.size(); ++i)
    {
        const auto& host = m_latencyHosts[i];
        double ms = -1.0;
        if (MeasureHttpHeadLatencyMs(host, ms) && ms >= 0)
            m_latencyMsPerHost[i] = ms;
    }
}

double CDataManager::GetAverageLatencyMs() const
{
    double sum = 0.0;
    int cnt = 0;
    for (double v : m_latencyMsPerHost)
    {
        if (v >= 0) { sum += v; ++cnt; }
    }
    if (cnt == 0) return -1.0;
    return sum / static_cast<double>(cnt);
}

double CDataManager::GetMinLatencyMs() const
{
    double best = -1.0;
    for (double v : m_latencyMsPerHost)
    {
        if (v >= 0 && (best < 0 || v < best)) best = v;
    }
    return best;
}

std::wstring CDataManager::GetLatencyTooltipText() const
{
    std::wstringstream ss;
    double v = GetMinLatencyMs();
    ss << L"最低: ";
    if (v < 0) ss << L"N/A"; else ss << static_cast<int>(v + 0.5) << L"ms";
    for (size_t i = 0; i < m_latencyHosts.size(); ++i)
    {
        ss << L"\n" << m_latencyHostNames[i] << L": ";
        double ms = (i < m_latencyMsPerHost.size() ? m_latencyMsPerHost[i] : -1.0);
        if (ms < 0) ss << L"N/A"; else ss << static_cast<int>(ms + 0.5) << L"ms";
    }
    return ss.str();
}
