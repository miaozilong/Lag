#pragma once
#include "PluginInterface.h"
#include "LagItem.h"
#include <string>

class CLag : public ITMPlugin
{
private:
    CLag();

public:
    static CLag& Instance();

    virtual IPluginItem* GetItem(int index) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void DataRequired() override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual void* GetPluginIcon() override;

private:

private:
    static CLag m_instance;
    CLagItem m_item;
    std::wstring m_tooltop_info;
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
