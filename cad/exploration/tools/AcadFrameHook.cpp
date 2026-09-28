#include "StdAfx.h"
#include "AcadFrameHook.h"
#include "declare.h" // Required for acedGetAcadFrame()
#include "arxHeaders.h"

CAcadFrameHook::CAcadFrameHook()
{
	StartHook();

}

CAcadFrameHook::~CAcadFrameHook()
{
	StopHook();
}

// 必须在 cpp 中初始化静态变量
WNDPROC CAcadFrameHook::m_pfnOldAcadWndProc = NULL;

void CAcadFrameHook::StartHook()
{
    if (acedGetAcadFrame() != NULL)
    {
        HWND hAcadWnd = acedGetAcadFrame()->GetSafeHwnd();
        if (::IsWindow(hAcadWnd) && m_pfnOldAcadWndProc == NULL)
        {
            // 传入静态函数指针
            m_pfnOldAcadWndProc = (WNDPROC)::SetWindowLongPtr(hAcadWnd, GWLP_WNDPROC, (LONG_PTR)AcadInternalCmdHandler);
        }
    }
}
void CAcadFrameHook::StopHook()
{
    if (m_pfnOldAcadWndProc != NULL && acedGetAcadFrame() != NULL)
    {
        HWND hAcadWnd = acedGetAcadFrame()->GetSafeHwnd();
        if (::IsWindow(hAcadWnd))
        {
            ::SetWindowLongPtr(hAcadWnd, GWLP_WNDPROC, (LONG_PTR)m_pfnOldAcadWndProc);
            m_pfnOldAcadWndProc = NULL; // 复位
        }
    }
}

// 静态消息响应函数实现
LRESULT CALLBACK CAcadFrameHook::AcadInternalCmdHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_USER_TOOL_TEST)
    {
		int ret = acedCommand(RTSTR, _T("_LINE"), RTSTR, _T("0,0"), RTSTR, _T("100,100"), RTNONE);
		acutPrintf(_T("dispatch_command ref :%d \n"),ret);
        //acedCmd(RTSTR, _T("_.LINE"), RTSTR, _T("0,0,0"), RTSTR, _T("100,100,0"), RTSTR, _T(""), RTNONE);
        return 1; 
    }

    // 注意：这里需要访问静态变量 m_pfnOldAcadWndProc
    return ::CallWindowProc(m_pfnOldAcadWndProc, hwnd, msg, wp, lp);
}

