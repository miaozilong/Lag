#pragma once
#include "Common.h"
#include <vector>

//����һ������������Ϣ
struct NetWorkConection
{
	std::wstring description;		//������������ȡ��GetAdapterInfo��
    std::wstring ip_address{ L"-.-.-.-" };	//IP��ַ
    std::wstring subnet_mask{ L"-.-.-.-" };	//��������
    std::wstring default_gateway{ L"-.-.-.-" };	//Ĭ������
};

class CAdapterCommon
{
public:
	CAdapterCommon();
	~CAdapterCommon();

	//��ȡ���������б��������������IP��ַ���������롢Ĭ��������Ϣ
	static void GetAdapterInfo(std::vector<NetWorkConection>& adapters);

	//ˢ�����������б��е�IP��ַ���������롢Ĭ��������Ϣ
	static void RefreshIpAddress(std::vector<NetWorkConection>& adapters);

};

