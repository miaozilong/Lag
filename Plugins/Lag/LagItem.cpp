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
    // 使用最近一次测量结果，不强制刷新
    double v = g_data.GetMinLatencyMs();
    if (v >= 0)
    {
        int ms = static_cast<int>(v + 0.5);
        text = std::to_wstring(ms) + L"ms";
    }
    else
    {
        text = L"N/A";
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
