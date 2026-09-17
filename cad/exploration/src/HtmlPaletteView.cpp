// coding: gb18030
#include "StdAfx.h"
#include "HtmlPaletteView.h"
#include "CPalette.h" 
#include "PaletteSet.h"
#include "Utils.h"
#include "Logger.h"
//#include  "Winuser.h"
 //#ifndef WM_GETOBJECT
 //#define WM_GETOBJECT 0x003D
 //#endif

IMPLEMENT_DYNCREATE(CHtmlPaletteView, CHtmlView)

CHtmlPaletteView::CHtmlPaletteView()
{
    m_pParentPalette = NULL;
	EnableAutomation();
}

CHtmlPaletteView::~CHtmlPaletteView()
{
}

BEGIN_MESSAGE_MAP(CHtmlPaletteView, CHtmlView)
	ON_MESSAGE( WM_ACAD_KEEPFOCUS, onAcadKeepFocus )
//ON_WM_ERASEBKGND()
 ON_WM_DESTROY()
 //ON_MESSAGE(WM_GETOBJECT, OnGetObject)
END_MESSAGE_MAP()

//BEGIN_INTERFACE_MAP(CHtmlPaletteView, CHtmlView)
//    INTERFACE_PART(CHtmlPaletteView, IID_IDispatch, Dispatch)
//END_INTERFACE_MAP()

BEGIN_DISPATCH_MAP(CHtmlPaletteView, CHtmlView)
	DISP_FUNCTION(CHtmlPaletteView, "ShowMessage", OnJsCallCpp, VT_EMPTY, VTS_BSTR)
	DISP_FUNCTION(CHtmlPaletteView, "OnJsCommand", OnJsCommand, VT_EMPTY, VTS_BSTR  VTS_BSTR)
    //DISP_FUNCTION_ID(CHtmlPaletteView, "OnJsCommand", 1, OnJsCommand, VT_EMPTY, VTS_BSTR VTS_BSTR)
END_DISPATCH_MAP()

LRESULT CHtmlPaletteView::onAcadKeepFocus( WPARAM wParam, LPARAM lParam )
{
    return 0;
}


void CHtmlPaletteView::SetParentPalette(CDataPalette* pParent)
{
    m_pParentPalette = pParent;
}
// 新增该函数实现
//LRESULT CHtmlPaletteView::OnGetObject(WPARAM wParam, LPARAM lParam)
//{
//	acutPrintf(_T("CHtmlPaletteView::OnGetObject.\n"));
//
//    // 【关键修复】：直接返回 0。
//    // 这会阻止 MFC 返回其内部的 XAccessible COM 对象。
//    // 强制 AutoCAD 和 Windows 使用默认的系统辅助功能，
//    // 彻底断绝焦点管理器缓存 C++ 对象指针的可能。
//    return 0;
//}
//void CHtmlPaletteView::OnInitialUpdate()
//{
//    CHtmlView::OnInitialUpdate();
//
//    
//    Navigate2(_T("file:///C:/Temp/PalettePage.html")); 
//}
HRESULT CHtmlPaletteView::OnGetExternal(LPDISPATCH *lppDispatch)
{
	*lppDispatch = GetIDispatch(TRUE);
	return S_OK;
}
// 新增：文档加载完成后的处理
void CHtmlPaletteView::OnDocumentComplete(LPCTSTR lpszURL)
{
    CHtmlView::OnDocumentComplete(lpszURL);

    // 调试用：在控制台输出，确认 OnDocumentComplete 被触发
    CUtils::acutPrintf(_T("[昇龙] Document Complete: %s \n"), lpszURL);
}
// C++ 函数实现：被 JS 调用
void CHtmlPaletteView::OnJsCallCpp(LPCTSTR szMessage)
{
	AfxMessageBox(szMessage);
}

void CHtmlPaletteView::OnJsCommand(LPCTSTR szCommand, LPCTSTR szParam)
{
    CLogger::GetInstance().Debug(_T("[昇龙] OnJsCommand Received: %s, %s \n"), szCommand, szParam);
    if (m_pParentPalette)
    {
        // trans command
        m_pParentPalette->HandleJsCommand(szCommand, szParam);
    }
	
}

void CHtmlPaletteView::CallJsFunction(LPCTSTR szFuncName, LPCTSTR szArg)
{
    IDispatch* pDispatch = GetHtmlDocument();
    if (!pDispatch) return;

    IHTMLDocument2* pDoc2 = NULL;
    HRESULT hr = pDispatch->QueryInterface(IID_IHTMLDocument2, (void**)&pDoc2);
    if (SUCCEEDED(hr) && pDoc2)
    {
        IHTMLWindow2* pWindow = NULL;
        hr = pDoc2->get_parentWindow(&pWindow);
        if (SUCCEEDED(hr) && pWindow)
        {
            VARIANT varResult;
            VariantInit(&varResult);
            
            DISPPARAMS dispparams;
            memset(&dispparams, 0, sizeof(dispparams));
            dispparams.cArgs = 1;
            dispparams.rgvarg = new VARIANT[1];
            dispparams.rgvarg[0].vt = VT_BSTR;
#ifdef _CAD2005
			// 1. 声明 USES_CONVERSION 宏，用于支持 ATL 转换栈分配
            USES_CONVERSION;
            
            // 2. 将 LPCTSTR (ANSI) 转换为 wchar_t* (Unicode)
            // T2W 在多字节模式下等价于 A2W (ANSI to Wide)
            LPOLESTR wszArg = T2W(szArg);
            
            // 3. 使用转换后的宽字符指针分配 BSTR
            dispparams.rgvarg[0].bstrVal = SysAllocString(wszArg);
#else
            dispparams.rgvarg[0].bstrVal = SysAllocString(szArg);
#endif
            dispparams.cNamedArgs = 0;

            EXCEPINFO excepInfo;
            memset(&excepInfo, 0, sizeof(excepInfo));
            
            LPOLESTR ptName = (LPOLESTR)szFuncName;
            DISPID dispID;
            hr = pWindow->GetIDsOfNames(IID_NULL, &ptName, 1, LOCALE_USER_DEFAULT, &dispID);
            
            if (SUCCEEDED(hr))
            {
                pWindow->Invoke(dispID, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_METHOD, &dispparams, &varResult, &excepInfo, NULL);
            }

			SysFreeString(dispparams.rgvarg[0].bstrVal);
            delete[] dispparams.rgvarg;
            pWindow->Release();
        }
        pDoc2->Release();
    }
    pDispatch->Release();
}
void CHtmlPaletteView::InitializeBrowser()
{
	CString strFilePath = CUtils::GetArxFolder();
    strFilePath.Replace(_T("\\"), _T("/"));

     // 4. Construct the file:/// URL
    CString strUrl;
    strUrl.Format(_T("file:///%spage/PalettePage.html"), strFilePath);

    // 6. Navigate
    CUtils::acutPrintf(_T("[昇龙] Loading from: %s\n"), strUrl);
    Navigate2(strUrl);


	
	// Navigate2(_T("file:///C:/Temp/PalettePage.html")); 
}
void CHtmlPaletteView::OnDestroy()
{
    //CUtils::acutPrintf(_T("CHtmlPaletteView::OnDestroy.\n"));

    // 1. 确保焦点不在内部子窗口上
   /* if (GetFocus() != NULL && (this == GetFocus() || IsChild(GetFocus())))
    {
        CWnd* pMain = AfxGetMainWnd();
        if (pMain) pMain->SetFocus();
    }*/

    // 2. 切断 C++ 与 HTML 的 DOM 绑定，释放外部引用
    LPDISPATCH pDisp = GetHtmlDocument();
    if (pDisp)
    {
        IHTMLDocument2* pDoc2 = NULL;
        if (SUCCEEDED(pDisp->QueryInterface(IID_IHTMLDocument2, (void**)&pDoc2)))
        {
            // 重置 HTML body，断开 JS 绑定
            pDoc2->close(); 
            pDoc2->Release();
        }
        pDisp->Release();
    }

    // 3. 导航到空白页，彻底销毁内部 DOM 树和焦点元素
    // Navigate2(_T("about:blank"));

    // 4. 获取底层的 WebBrowser 控件并关闭它
    LPDISPATCH pAppDisp = GetApplication();
    if (pAppDisp)
    {
        IWebBrowser2* pBrowser = NULL;
        if (SUCCEEDED(pAppDisp->QueryInterface(IID_IWebBrowser2, (void**)&pBrowser)))
        {
            // 通过 IOleObject 关闭，防止触发 Accessibility 查询
            IOleObject* pOleObj = NULL;
            if (SUCCEEDED(pBrowser->QueryInterface(IID_IOleObject, (void**)&pOleObj)))
            {
                pOleObj->Close(OLECLOSE_NOSAVE);
                pOleObj->SetClientSite(NULL);
                pOleObj->Release();
            }
            pBrowser->Release();
        }
        pAppDisp->Release();
    }

    // 5. CoDisconnectObject - 断开所有外部引用 (MFC XAccessible)
    LPUNKNOWN pUnk = GetInterface(&IID_IUnknown);
    if (pUnk)
    {
        CoDisconnectObject(pUnk, 0);
    }

//#ifdef _CAD2005
	CWnd::OnDestroy();
//#else
//
//
//    // 调用基类
//    CHtmlView::OnDestroy();
//#endif
}
//BOOL CHtmlPaletteView::PreCreateWindow(CREATESTRUCT& cs)
//{
//	// TODO: 在此处通过修改
//	//  CREATESTRUCT cs 来修改窗口类或样式
//
//	return CHtmlView::PreCreateWindow(cs);
//}
//void CHtmlPaletteView::PostNcDestroy()
//{
//    CHtmlView::PostNcDestroy();
//    delete this;
//}

//BOOL CHtmlPaletteView::OnEraseBkgnd(CDC* pDC)
//{
//    // Returning TRUE indicates that we have processed the background erasure to prevent flickering 
// // Or draw the same background color as the parent window here
//    CRect rect;
//    GetClientRect(&rect);
//    
//    
//    CDataPalette* pParent = m_pParentPalette; 
//    if (pParent)
//    {
//         CAdUiPaletteTheme* pTheme = pParent->GetPaletteSet()->GetTheme();
//         if (pTheme)
//         {
//             CBrush brush(pTheme->GetColor(kPaletteBackground));
//             pDC->FillRect(&rect, &brush);
//             return TRUE;
//         }
//    }
//    
//    return CHtmlView::OnEraseBkgnd(pDC);
//}
