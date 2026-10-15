// 桩文件：补全adui16.lib中缺失的CAdUiPalette成员函数实现
#include "StdAfx.h"
#include "..\inc\2005\aduiPalette.h"

// 实现GetClassName（MFC自动映射为GetClassNameA，匹配链接器符号）
CString CAdUiPalette::GetClassName()
{
    return CString(_T("AdUiPalette"));
}

// 实现SetName，参数类型和头文件声明保持一致：LPCTSTR
void CAdUiPalette::SetName(LPCTSTR pszName)
{
    // 空实现，业务接口不调用侧边栏面板功能
}
