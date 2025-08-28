#pragma once
#include <map>
#include <vector>

//��ʽ����ǰʱ��

const std::vector<CString> FormularList
{
    _T("dddd"),	    // ����ʽ�����ڣ�����һ~������ / Monday~Sunday��
    _T("ddd"),	    // �̸�ʽ�����ڣ���һ~���� / Mon~Sun��
    _T("dd"),	    // �գ���ǰ���0��01~31��
    _T("d"),	    // �գ�����ǰ���0��1~31��
    _T("MMMM"),	    // ����ʽ���·ݣ�һ��~ʮ���� / January~December��
    _T("MMM"),	    // �̸�ʽ���·ݣ�1��~12�� / Jan~Dec��
    _T("MM"),	    // �·ݣ���ǰ���0��01~12��
    _T("M"),	    // �·ݣ�����ǰ���0��1~12��
    _T("yyyy"),	    // ��λ�������
    _T("yy"),	    // ��λ�������
    _T("hh"),	    // Сʱ��12Сʱ�ƣ���ǰ���0��01~12��
    _T("h"),	    // Сʱ��12Сʱ�ƣ�����ǰ���0��1~12��
    _T("HH"),	    // Сʱ��24Сʱ�ƣ���ǰ���0��00~23��
    _T("H"),	    // Сʱ��24Сʱ�ƣ�����ǰ���0��0~23��
    _T("mm"),	    // ���ӣ���ǰ���0��00~59��
    _T("m"),	    // ���ӣ�����ǰ���0��0~59��
    _T("ss"),	    // ���ӣ���ǰ���0��00~59��
    _T("s"),	    // ���ӣ�����ǰ���0��00~59��
    _T("AP"),	    // ��ʾ���绹�����磬ֻ���ǡ�AM����PM��
    _T("ap"),	    // ��ʾ���绹�����磬ֻ���ǡ�am����pm��
};

class CDataTimeFormatHelper
{
public:
    CDataTimeFormatHelper();
    ~CDataTimeFormatHelper();

    CString GetCurrentDateTimeByFormat(const CString& formular);
    static CString GetDateTimeByFormat(const CString& formular, SYSTEMTIME sys_time);
    void GetCurrentDateTime();
    const SYSTEMTIME& CurrentDateTime();

private:
    CString GetFormualrValue(CString formular_type);
    static CString GetFormualrValue(CString formular_type, SYSTEMTIME sys_time);
    static CString GetWeekString(WORD day_of_week);
    static CString GetWeekStringShort(WORD day_of_week);
    static CString GetMonthString(DWORD month);
    static CString GetMonthStringShort(DWORD month);

private:
    SYSTEMTIME m_cur_time{};
};

