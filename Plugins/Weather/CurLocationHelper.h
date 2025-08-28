#pragma once
#include <string>
#include "CityCode.h"

class CCurLocationHelper
{
public:
    CCurLocationHelper();
    ~CCurLocationHelper();

    struct Location
    {
        std::wstring province;
        std::wstring city;
        std::wstring conty;
    };

    static std::wstring GetCurrentCity();
    //���ַ����е�xxʡxx��xx��/�в��
    static Location ParseCityName(std::wstring city_string);
    static int FindCityCodeItem(Location location);
    static int FindCityCodeItem(std::wstring city_name);

};

