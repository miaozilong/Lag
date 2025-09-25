#include "pch.h"
#include "LagItem.h"
#include "DataManager.h"
#include <afxwin.h>

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
    // 显示两行：国内站点和国际站点最低延迟
    std::wstring domestic = g_data.GetDomesticLatencyText();
    std::wstring international = g_data.GetInternationalLatencyText();
    text = domestic + L"\n" + international;
    return text.c_str();
}

const wchar_t* CLagItem::GetItemValueSampleText() const
{
    return L"000ms";
}

bool CLagItem::IsCustomDraw() const
{
    return true;
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

void CLagItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    if (!pDC) return;

    // 设置文本颜色
    COLORREF textColor = dark_mode ? RGB(255, 255, 255) : RGB(0, 0, 0);
    pDC->SetTextColor(textColor);
    pDC->SetBkMode(TRANSPARENT);

    // 获取字体信息
    CFont* pOldFont = pDC->SelectObject(pDC->GetCurrentFont());
    
    // 计算文本尺寸
    CSize textSize = pDC->GetTextExtent(L"000ms");
    int lineHeight = textSize.cy;
    int totalHeight = lineHeight * 2; // 两行文本
    
    // 计算垂直居中位置
    int startY = y + (h - totalHeight) / 2;
    
    // 绘制第一行：国内站点延迟
    std::wstring domestic = g_data.GetDomesticLatencyText();
    pDC->TextOut(x, startY, domestic.c_str());
    
    // 绘制第二行：国际站点延迟
    std::wstring international = g_data.GetInternationalLatencyText();
    pDC->TextOut(x, startY + lineHeight, international.c_str());
    
    // 恢复字体
    pDC->SelectObject(pOldFont);
}
