// coding: gb18030
#pragma once


#include <afxhtml.h>

class CPaletteCleanup
{
public:
    // 在卸载插件时调用此方法
    static void SafeCleanupHtmlView(CHtmlView* pHtmlView);

private:
    static void DisconnectComConnections(CWnd* pWnd);
};