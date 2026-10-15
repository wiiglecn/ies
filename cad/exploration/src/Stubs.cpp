// 桩文件：补全adui16.lib中缺失的CAdUiPalette成员函数实现
#include "StdAfx.h"
#include "..\inc\2005\aduiPalette.h"

// CAdUiPalette::GetClassName - 实现缺失的类名获取函数
CString CAdUiPalette::GetClassName()
{
    return CString(_T("AdUiPalette"));
}

// CAdUiPalette::SetName - 实现缺失的名称设置函数（空实现，不访问私有成员）
void CAdUiPalette::SetName(CString strName)
{
    UNREFERENCED_PARAMETER(strName);
}
