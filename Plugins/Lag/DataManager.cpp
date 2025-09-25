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
    m_setting_data.show_Lag_in_tooltip = (GetPrivateProfileInt(L"config", L"show_Lag_in_tooltip", 1, m_config_path.c_str()) != 0);
}

void CDataManager::SaveConfig() const
{
    if (!m_config_path.empty())
    {
        WritePrivateProfileInt(L"config", L"show_Lag_in_tooltip", m_setting_data.show_Lag_in_tooltip, m_config_path.c_str());
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
        m_string_table[id].LoadString(id);
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


// removed battery-related code

static DWORD64 GetTickMs()
{
    return static_cast<DWORD64>(GetTickCount64());
}

// 带超时的网络延迟测量函数
static double MeasureHttpHeadLatencyMsWithTimeout(const std::wstring& host, int timeoutMs = 3000)
{
    HINTERNET hSession = WinHttpOpen(L"LagPlugin/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return -1.0;

    // 设置超时
    DWORD timeout = timeoutMs;
    WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);

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
            return -1.0;
        }
        hRequest = WinHttpOpenRequest(hConnect, L"HEAD", L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return -1.0;
        }
    }

    DWORD64 t0 = GetTickMs();
    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, NULL);
    DWORD64 t1 = GetTickMs();
    
    double result = -1.0;
    if (ok)
    {
        result = static_cast<double>(t1 - t0);
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

void CDataManager::RefreshLatency()
{
    // 智能节流：如果正在刷新，直接返回
    if (m_isRefreshing.load()) return;
    
    // 简单节流：5秒内不重复测量
    DWORD64 now = GetTickMs();
    if (m_latencyLastUpdateTick != 0 && now - m_latencyLastUpdateTick < 5000) return;
    
    m_isRefreshing.store(true);
    m_latencyLastUpdateTick = now;

    // 启动异步任务进行网络延迟测量
    std::thread([this]() {
        // 创建异步任务列表
        std::vector<std::future<double>> futures;
        
        // 为国内站点创建异步任务
        for (const auto& host : m_domesticHosts)
        {
            futures.push_back(std::async(std::launch::async, [host]() {
                return MeasureHttpHeadLatencyMsWithTimeout(host, NETWORK_TIMEOUT_MS);
            }));
        }
        
        // 为国际站点创建异步任务
        for (const auto& host : m_internationalHosts)
        {
            futures.push_back(std::async(std::launch::async, [host]() {
                return MeasureHttpHeadLatencyMsWithTimeout(host, NETWORK_TIMEOUT_MS);
            }));
        }
        
        // 收集所有结果（这里会等待网络请求完成，但在子线程中）
        std::vector<double> domesticResults;
        std::vector<double> internationalResults;
        
        for (size_t i = 0; i < m_domesticHosts.size(); ++i)
        {
            double result = futures[i].get();
            domesticResults.push_back(result);
        }
        
        for (size_t i = 0; i < m_internationalHosts.size(); ++i)
        {
            double result = futures[m_domesticHosts.size() + i].get();
            internationalResults.push_back(result);
        }
        
        // 快速更新数据（短暂锁定）
        {
            std::lock_guard<std::mutex> lock(m_latencyMutex);
            m_domesticLatencyMs = std::move(domesticResults);
            m_internationalLatencyMs = std::move(internationalResults);
        }
        
        m_isRefreshing.store(false);
    }).detach();
}

double CDataManager::GetAverageLatencyMs() const
{
    // 使用try_lock避免阻塞主线程
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return -1.0; // 如果无法获取锁，返回-1
    }
    
    double sum = 0.0;
    int cnt = 0;
    for (double v : m_domesticLatencyMs)
    {
        if (v >= 0) { sum += v; ++cnt; }
    }
    for (double v : m_internationalLatencyMs)
    {
        if (v >= 0) { sum += v; ++cnt; }
    }
    if (cnt == 0) return -1.0;
    return sum / static_cast<double>(cnt);
}

double CDataManager::GetMinLatencyMs() const
{
    // 使用try_lock避免阻塞主线程
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return -1.0; // 如果无法获取锁，返回-1
    }
    
    double best = -1.0;
    for (double v : m_domesticLatencyMs)
    {
        if (v >= 0 && (best < 0 || v < best)) best = v;
    }
    for (double v : m_internationalLatencyMs)
    {
        if (v >= 0 && (best < 0 || v < best)) best = v;
    }
    return best;
}

double CDataManager::GetDomesticMinLatencyMs() const
{
    // 使用try_lock避免阻塞主线程
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return -1.0; // 如果无法获取锁，返回-1
    }
    
    double best = -1.0;
    for (double v : m_domesticLatencyMs)
    {
        if (v >= 0 && (best < 0 || v < best)) best = v;
    }
    return best;
}

double CDataManager::GetInternationalMinLatencyMs() const
{
    // 使用try_lock避免阻塞主线程
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return -1.0; // 如果无法获取锁，返回-1
    }
    
    double best = -1.0;
    for (double v : m_internationalLatencyMs)
    {
        if (v >= 0 && (best < 0 || v < best)) best = v;
    }
    return best;
}

std::wstring CDataManager::GetLatencyTooltipText() const
{
    // 使用try_lock避免阻塞主线程
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return L"正在更新..."; // 如果无法获取锁，返回提示信息
    }
    
    std::wstringstream ss;
    
    // 国内站点
    ss << L"国内站点:\n";
    for (size_t i = 0; i < m_domesticHosts.size(); ++i)
    {
        ss << m_domesticHostNames[i] << L": ";
        double ms = (i < m_domesticLatencyMs.size() ? m_domesticLatencyMs[i] : -1.0);
        if (ms < 0) ss << L"N/A"; else ss << static_cast<int>(ms + 0.5) << L"ms";
        if (i < m_domesticHosts.size() - 1) ss << L"\n";
    }
    
    ss << L"\n\n国际站点:\n";
    for (size_t i = 0; i < m_internationalHosts.size(); ++i)
    {
        ss << m_internationalHostNames[i] << L": ";
        double ms = (i < m_internationalLatencyMs.size() ? m_internationalLatencyMs[i] : -1.0);
        if (ms < 0) ss << L"N/A"; else ss << static_cast<int>(ms + 0.5) << L"ms";
        if (i < m_internationalHosts.size() - 1) ss << L"\n";
    }
    
    return ss.str();
}

std::wstring CDataManager::GetDomesticLatencyText() const
{
    double v = GetDomesticMinLatencyMs();
    if (v >= 0)
    {
        int ms = static_cast<int>(v + 0.5);
        return std::to_wstring(ms) + L"ms";
    }
    else
    {
        return L"N/A";
    }
}

std::wstring CDataManager::GetInternationalLatencyText() const
{
    double v = GetInternationalMinLatencyMs();
    if (v >= 0)
    {
        int ms = static_cast<int>(v + 0.5);
        return std::to_wstring(ms) + L"ms";
    }
    else
    {
        return L"N/A";
    }
}
