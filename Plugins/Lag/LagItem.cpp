#include "pch.h"
#include "LagItem.h"
#include "DataManager.h"

static double Lag_percent = 0.0;

CLagItem::CLagItem()
{
    // 定时刷新延迟数据（5秒一次）
    SetTimer(NULL, 0, 5000, [](HWND, UINT, UINT_PTR, DWORD)
        {
            g_data.RefreshLatency();
        });
}

const wchar_t* CLagItem::GetItemName() const
{
    return g_data.StringRes(IDS_Lag);
}

const wchar_t* CLagItem::GetItemId() const
{
    return L"b5R30ITQ";
}

const wchar_t* CLagItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CLagItem::GetItemValueText() const
{
    static std::wstring text;
    static int last_ms = -1;                     // 上次成功的最低延迟（四舍五入后的整数毫秒）
    static ULONGLONG last_ok_tick = 0;           // 上次成功时间
    const ULONGLONG fallback_window_ms = 500;    // 回退窗口：500毫秒
    // 使用最近一次测量结果，不强制刷新
    double v = g_data.GetMinLatencyMs();
    if (v >= 0)
    {
        int ms = static_cast<int>(v + 0.5);
        last_ms = ms;
        last_ok_tick = GetTickCount64();
        text = std::to_wstring(last_ms) + L"ms";
    }
    else
    {
        if (last_ms >= 0 && last_ok_tick != 0 && GetTickCount64() - last_ok_tick <= fallback_window_ms)
        {
            text = std::to_wstring(last_ms) + L"ms";
        }
        else
        {
            text = L"N/A";
        }
    }
    return text.c_str();
}

const wchar_t* CLagItem::GetItemValueSampleText() const
{
    return L"000ms";
}

bool CLagItem::IsCustomDraw() const
{
    return false;
}

//int CLagItem::GetItemWidth() const
//{
//    return g_data.RDPI(m_item_width) + 2;
//}

int CLagItem::GetItemWidthEx(void * hDC) const
{
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    CString sample = _T("000ms");
    return pDC->GetTextExtent(sample).cx;
}

void CLagItem::DrawItem(void* hDC, int x, int y, int w, int h, bool)
{
    // 非自绘，留空
}
