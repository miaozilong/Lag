#include "pch.h"
#include "DataManager.h"
#include "Common.h"
#include <vector>
#include <sstream>
#include <Gdiplus.h>
#pragma comment(lib,"GdiPlus.lib")
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")

// 线程池实现
ThreadPool::ThreadPool(size_t numThreads) : stop(false)
{
    for (size_t i = 0; i < numThreads; ++i)
    {
        workers.emplace_back([this]
        {
            for (;;)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    if (this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
}

void ThreadPool::shutdown()
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers)
        worker.join();
}

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
    
    //初始化线程池
    m_threadPool = std::make_unique<ThreadPool>(LatencyConfig::THREAD_POOL_SIZE);
}

CDataManager::~CDataManager()
{
    SaveConfig();
    CloseAllConnections();
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

// ============================================================================
// 网络延迟测量函数（使用持久连接）
// ============================================================================
// 功能：测量指定主机的 HTTP HEAD 请求延迟
// 参数：dataManager - 数据管理器实例（用于获取持久连接）
//       host        - 要测量的主机名
//       timeoutMs   - 超时时间（毫秒）
// 返回：延迟时间（毫秒），失败返回 -1.0
// ============================================================================
static double MeasureHttpHeadLatencyMsWithTimeout(CDataManager* dataManager, const std::wstring& host, int timeoutMs = LatencyConfig::NETWORK_TIMEOUT_MS)
{
    // 获取或创建持久连接
    CDataManager::PersistentConnection* conn = dataManager->GetOrCreateConnection(host);
    if (!conn || !conn->isValid)
    {
        // 如果连接无效，尝试重新创建一次
        conn = dataManager->GetOrCreateConnection(host);
        if (!conn || !conn->isValid)
        {
            return -1.0;
        }
    }

    // 创建请求（每次测量都创建新请求，但重用连接）
    HINTERNET hRequest = WinHttpOpenRequest(
        conn->hConnect, 
        L"HEAD", 
        L"/", 
        NULL, 
        WINHTTP_NO_REFERER, 
        WINHTTP_DEFAULT_ACCEPT_TYPES, 
        conn->useHttps ? WINHTTP_FLAG_SECURE : 0
    );
    
    if (!hRequest)
    {
        // 如果请求创建失败，可能是连接已断开，标记为无效并尝试重新创建
        conn->isValid = false;
        conn = dataManager->GetOrCreateConnection(host);
        if (!conn || !conn->isValid)
        {
            return -1.0;
        }
        
        // 重试创建请求
        hRequest = WinHttpOpenRequest(
            conn->hConnect, 
            L"HEAD", 
            L"/", 
            NULL, 
            WINHTTP_NO_REFERER, 
            WINHTTP_DEFAULT_ACCEPT_TYPES, 
            conn->useHttps ? WINHTTP_FLAG_SECURE : 0
        );
        
        if (!hRequest)
        {
            conn->isValid = false;
            return -1.0;
        }
    }

    // 设置请求超时
    DWORD timeout = timeoutMs;
    WinHttpSetTimeouts(hRequest, timeout, timeout, timeout, timeout);

    DWORD64 t0 = GetTickMs();
    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, NULL);
    DWORD64 t1 = GetTickMs();
    
    double result = -1.0;
    if (ok)
    {
        result = static_cast<double>(t1 - t0);
        conn->lastUsedTick = GetTickMs();
    }
    else
    {
        // 如果请求失败，可能是连接已断开，标记为无效
        // 下次调用时会自动重新创建连接
        conn->isValid = false;
    }
    
    WinHttpCloseHandle(hRequest);
    return result;
}

void CDataManager::RefreshLatency()
{
    // 如果正在刷新，直接返回（防止重复刷新，但不限制时间间隔）
    if (m_isRefreshing.load()) return;
    
    m_isRefreshing.store(true);

    // 使用线程池进行网络延迟测量
    std::thread([this]() {
        // 使用 RAII 确保 m_isRefreshing 总是被重置
        struct RefreshingGuard
        {
            std::atomic<bool>* flag;
            RefreshingGuard(std::atomic<bool>* f) : flag(f) {}
            ~RefreshingGuard() { flag->store(false); }
        } guard(&m_isRefreshing);
        
        try
        {
            // 创建异步任务列表
            std::vector<std::future<double>> futures;
            
            // 为国内站点创建线程池任务
            for (const auto& hostInfo : m_domesticHosts)
            {
                futures.push_back(m_threadPool->enqueue([this, hostInfo]() {
                    return MeasureHttpHeadLatencyMsWithTimeout(this, hostInfo.host, LatencyConfig::NETWORK_TIMEOUT_MS);
                }));
            }
            
            // 为国际站点创建线程池任务
            for (const auto& hostInfo : m_internationalHosts)
            {
                futures.push_back(m_threadPool->enqueue([this, hostInfo]() {
                    return MeasureHttpHeadLatencyMsWithTimeout(this, hostInfo.host, LatencyConfig::NETWORK_TIMEOUT_MS);
                }));
            }
            
            // 收集所有结果（这里会等待网络请求完成，但在子线程中）
            std::vector<double> domesticResults;
            std::vector<double> internationalResults;
            
            // 收集国内站点结果
            for (size_t i = 0; i < m_domesticHosts.size(); ++i)
            {
                double result = futures[i].get();
                domesticResults.push_back(result);
            }
            
            // 收集国际站点结果
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
        }
        catch (...)
        {
            // 发生异常时，确保数据不被破坏，保持原有数据不变
            // m_isRefreshing 会由 guard 自动重置
        }
    }).detach();
}

// ============================================================================
// 延迟计算辅助方法
// ============================================================================
double CDataManager::CalculateMinLatency(const std::vector<double>& latencies) const
{
    double best = -1.0;
    for (double v : latencies)
    {
        if (v >= 0 && (best < 0 || v < best))
        {
            best = v;
        }
    }
    return best;
}

double CDataManager::CalculateAverageLatency(const std::vector<double>& latencies) const
{
    double sum = 0.0;
    int cnt = 0;
    for (double v : latencies)
    {
        if (v >= 0)
        {
            sum += v;
            ++cnt;
        }
    }
    return (cnt == 0) ? -1.0 : (sum / static_cast<double>(cnt));
}

// ============================================================================
// 延迟查询接口实现
// ============================================================================
double CDataManager::GetAverageLatencyMs() const
{
    // 使用 try_lock 避免阻塞主线程
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        return -1.0; // 无法获取锁，返回失败
    }
    
    // 合并国内和国际站点计算平均值
    std::vector<double> allLatencies;
    allLatencies.reserve(m_domesticLatencyMs.size() + m_internationalLatencyMs.size());
    allLatencies.insert(allLatencies.end(), m_domesticLatencyMs.begin(), m_domesticLatencyMs.end());
    allLatencies.insert(allLatencies.end(), m_internationalLatencyMs.begin(), m_internationalLatencyMs.end());
    
    return CalculateAverageLatency(allLatencies);
}

double CDataManager::GetMinLatencyMs() const
{
    // 使用 try_lock 避免阻塞主线程
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        return -1.0; // 无法获取锁，返回失败
    }
    
    // 合并国内和国际站点查找最小值
    double domesticMin = CalculateMinLatency(m_domesticLatencyMs);
    double internationalMin = CalculateMinLatency(m_internationalLatencyMs);
    
    if (domesticMin < 0) return internationalMin;
    if (internationalMin < 0) return domesticMin;
    return (domesticMin < internationalMin) ? domesticMin : internationalMin;
}

double CDataManager::GetDomesticMinLatencyMs() const
{
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        return -1.0;
    }
    return CalculateMinLatency(m_domesticLatencyMs);
}

double CDataManager::GetInternationalMinLatencyMs() const
{
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        return -1.0;
    }
    return CalculateMinLatency(m_internationalLatencyMs);
}

std::wstring CDataManager::GetLatencyTooltipText() const
{
    // 使用try_lock避免阻塞主线程
    std::unique_lock<std::mutex> lock(m_latencyMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return L"正在更新..."; // 如果无法获取锁，返回提示信息
    }
    
    std::wstringstream ss;
    
    // 格式化国内站点延迟信息
    ss << L"国内站点:\n";
    for (size_t i = 0; i < m_domesticHosts.size(); ++i)
    {
        ss << m_domesticHosts[i].name << L": ";
        double ms = (i < m_domesticLatencyMs.size() ? m_domesticLatencyMs[i] : -1.0);
        if (ms < 0)
        {
            ss << L"N/A";
        }
        else
        {
            ss << static_cast<int>(ms + 0.5) << L"ms";
        }
        if (i < m_domesticHosts.size() - 1)
        {
            ss << L"\n";
        }
    }
    
    // 格式化国际站点延迟信息
    ss << L"\n\n国际站点:\n";
    for (size_t i = 0; i < m_internationalHosts.size(); ++i)
    {
        ss << m_internationalHosts[i].name << L": ";
        double ms = (i < m_internationalLatencyMs.size() ? m_internationalLatencyMs[i] : -1.0);
        if (ms < 0)
        {
            ss << L"N/A";
        }
        else
        {
            ss << static_cast<int>(ms + 0.5) << L"ms";
        }
        if (i < m_internationalHosts.size() - 1)
        {
            ss << L"\n";
        }
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

// ============================================================================
// 连接池管理方法实现
// ============================================================================
CDataManager::PersistentConnection* CDataManager::GetOrCreateConnection(const std::wstring& host)
{
    std::lock_guard<std::mutex> lock(m_connectionMutex);
    
    // 查找现有连接
    auto it = m_connections.find(host);
    if (it != m_connections.end() && it->second)
    {
        PersistentConnection* conn = it->second.get();
        
        // 检查连接是否仍然有效
        if (conn->isValid && IsConnectionValid(conn))
        {
            return conn;
        }
        else
        {
            // 连接无效，从 map 中移除（会自动关闭连接）
            m_connections.erase(it);
        }
    }
    
    // 创建新连接
    auto conn = std::make_unique<PersistentConnection>();
    
    // 创建会话
    conn->hSession = WinHttpOpen(L"LagPlugin/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!conn->hSession)
    {
        return nullptr;
    }
    
    // 设置超时
    DWORD timeout = LatencyConfig::NETWORK_TIMEOUT_MS;
    WinHttpSetTimeouts(conn->hSession, timeout, timeout, timeout, timeout);
    
    // 先尝试 HTTPS
    conn->hConnect = WinHttpConnect(conn->hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (conn->hConnect)
    {
        conn->useHttps = true;
        conn->isValid = true;
        conn->lastUsedTick = GetTickMs();
        
        // 存储连接
        PersistentConnection* result = conn.get();
        m_connections[host] = std::move(conn);
        return result;
    }
    
    // HTTPS 失败，尝试 HTTP
    conn->hConnect = WinHttpConnect(conn->hSession, host.c_str(), INTERNET_DEFAULT_HTTP_PORT, 0);
    if (conn->hConnect)
    {
        conn->useHttps = false;
        conn->isValid = true;
        conn->lastUsedTick = GetTickMs();
        
        // 存储连接
        PersistentConnection* result = conn.get();
        m_connections[host] = std::move(conn);
        return result;
    }
    
    // 都失败了，清理并返回 nullptr
    WinHttpCloseHandle(conn->hSession);
    return nullptr;
}

bool CDataManager::IsConnectionValid(PersistentConnection* conn) const
{
    if (!conn || !conn->hSession || !conn->hConnect)
    {
        return false;
    }
    
    // 尝试创建一个测试请求来检查连接是否仍然有效
    // 如果连接已断开，WinHttpOpenRequest 可能会失败
    // 但为了不消耗资源，我们只做基本检查
    // 实际的有效性检查在 MeasureHttpHeadLatencyMsWithTimeout 中进行
    
    // 检查句柄是否仍然有效（简单检查）
    // WinHTTP 没有直接的方法检查连接状态，所以我们依赖实际使用时的错误检测
    return true;
}

void CDataManager::CloseAllConnections()
{
    std::lock_guard<std::mutex> lock(m_connectionMutex);
    m_connections.clear(); // unique_ptr 会自动调用析构函数，关闭所有连接
}
