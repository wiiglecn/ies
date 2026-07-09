// coding: gb18030
#include "StdAfx.h"
#include "PaletteCleanup.h"
//
//CPaletteCleanup::CPaletteCleanup(void)
//{
//}
//
//CPaletteCleanup::~CPaletteCleanup(void)
//{
//}
void CPaletteCleanup::SafeCleanupHtmlView(CHtmlView* pHtmlView)
{
    if (pHtmlView == NULL || pHtmlView->GetSafeHwnd() == NULL)
        return;

    // 步骤1: 停止 WebBrowser 控件的所有导航
    pHtmlView->Stop();

    // 步骤2: 导航到 about:blank，释放当前页面资源
    //pHtmlView->Navigate2(_T("about:blank"));

    // 步骤3: 泵送消息，等待导航完成
    MSG msg;
    DWORD dwStart = GetTickCount();
    while (GetTickCount() - dwStart < 1000) // 最多等待1秒
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Sleep(10);
        }
    }

    // 步骤4: 获取 IWebBrowser2 接口并断开事件连接
    LPDISPATCH pDisp = pHtmlView->GetApplication();
    if (pDisp != NULL)
    {
        IWebBrowser2* pBrowser = NULL;
        if (SUCCEEDED(pDisp->QueryInterface(IID_IWebBrowser2, (void**)&pBrowser)))
        {
            // 关闭浏览器控件
            pBrowser->put_Visible(VARIANT_FALSE);
            pBrowser->Stop();
            pBrowser->Release();
        }
        pDisp->Release();
    }

    // 步骤5: 断开 COM 连接（关键步骤）
    // 这会断开所有外部对 CHtmlView COM 对象的引用
    DisconnectComConnections(pHtmlView);

    // 步骤6: 隐藏窗口
    pHtmlView->ShowWindow(SW_HIDE);

    // 步骤7: 销毁窗口
    pHtmlView->DestroyWindow();
}
void CPaletteCleanup::DisconnectComConnections(CWnd* pWnd)
{
    if (pWnd == NULL || pWnd->GetSafeHwnd() == NULL)
        return;

    // 获取 CWnd 的 IUnknown 接口
    LPUNKNOWN pUnk = pWnd->GetInterface(&IID_IUnknown);
    if (pUnk != NULL)
    {
        // 断开所有外部 COM 连接
        // 这包括 Accessibility 接口的连接
        CoDisconnectObject(pUnk, 0);
    }

    // 另一种方法：通过 IOleObject 断开
    // CHtmlView 内部有 WebBrowser 控件
    IOleObject* pOleObj = NULL;
    IWebBrowser2* pBrowser = NULL;
    LPDISPATCH pDisp = ((CHtmlView*)pWnd)->GetApplication();
    if (pDisp != NULL)
    {
        if (SUCCEEDED(pDisp->QueryInterface(IID_IOleObject, (void**)&pOleObj)))
        {
            // 关闭 OLE 对象
            pOleObj->Close(OLECLOSE_NOSAVE);
            pOleObj->Release();
        }
        pDisp->Release();
    }
}