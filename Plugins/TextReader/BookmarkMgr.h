#pragma once
#include <map>
#include <string>
#include <set>

class CBookmarkMgr
{
public:
    const std::set<int>& GetBookmark(const std::wstring& file_path);     //��ȡһ���ļ�����ǩ
    void AddBookmark(const std::wstring& file_path, int pos);           //���һ����ǩ
    void LoadFromConfig(const std::wstring& config_file_path);
    void SaveToConfig(const std::wstring& config_file_path) const;

private:
    std::map<std::wstring, std::set<int>> m_bookmarks;    //���������ļ�����ǩ
};

