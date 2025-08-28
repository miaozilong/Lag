#include "pch.h"
#include "Lag.h"
#include "Common.h"
#include <fstream>
#include "DataManager.h"
#include "OptionsDlg.h"
#include <sstream>

CLag CLag::m_instance;

CLag::CLag()
{
}

CLag& CLag::Instance()
{
    return m_instance;
}

IPluginItem* CLag::GetItem(int index)
{
    switch (index)
    {
    case 0:
        return &m_item;
    default:
        break;
    }
    return nullptr;
}

const wchar_t* CLag::GetTooltipInfo()
{
    if (g_data.m_setting_data.show_Lag_in_tooltip)
        return m_tooltop_info.c_str();
    else
        return L"";
}

void CLag::DataRequired()
{
    // 刷新延迟并生成鼠标提示信息
    g_data.RefreshLatency();
    m_tooltop_info = g_data.GetLatencyTooltipText();
}

ITMPlugin::OptionReturn CLag::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CWnd* pParent = CWnd::FromHandle((HWND)hParent);
    COptionsDlg dlg(pParent);
    dlg.m_data = g_data.m_setting_data;
    if (dlg.DoModal() == IDOK)
    {
        g_data.m_setting_data = dlg.m_data;
        g_data.SaveConfig();
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

const wchar_t* CLag::GetInfo(PluginInfoIndex index)
{
    static CString str;
    switch (index)
    {
    case TMI_NAME:
        return g_data.StringRes(IDS_PLUGIN_NAME).GetString();
    case TMI_DESCRIPTION:
        return g_data.StringRes(IDS_PLUGIN_DESCRIPTION).GetString();
    case TMI_AUTHOR:
        return L"zhongyang219";
    case TMI_COPYRIGHT:
        return L"Copyright (C) by Zhong Yang 2025";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
        break;
    case TMI_VERSION:
        return L"1.03";
    default:
        break;
    }
    return L"";
}

void CLag::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        //从配置文件读取配置
        g_data.LoadConfig(std::wstring(data));

        break;
    default:
        break;
    }
}

void* CLag::GetPluginIcon()
{
    return g_data.GetIcon(IDI_Lag_DARK);
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CLag::Instance();
}
