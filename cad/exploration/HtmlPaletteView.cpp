// coding: gb18030
#include "StdAfx.h"
#include "HtmlPaletteView.h"
#include "CPalette.h" 
#include "PaletteSet.h"

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
//ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

//BEGIN_INTERFACE_MAP(CHtmlPaletteView, CHtmlView)
//    INTERFACE_PART(CHtmlPaletteView, IID_IDispatch, Dispatch)
//END_INTERFACE_MAP()

BEGIN_DISPATCH_MAP(CHtmlPaletteView, CHtmlView)
	DISP_FUNCTION(CHtmlPaletteView, "ShowMessage", OnJsCallCpp, VT_EMPTY, VTS_BSTR)
	DISP_FUNCTION(CHtmlPaletteView, "OnJsCommand", OnJsCommand, VT_EMPTY, VTS_BSTR  VTS_BSTR)
    //DISP_FUNCTION_ID(CHtmlPaletteView, "OnJsCommand", 1, OnJsCommand, VT_EMPTY, VTS_BSTR VTS_BSTR)
END_DISPATCH_MAP()

void CHtmlPaletteView::SetParentPalette(CDataPalette* pParent)
{
    m_pParentPalette = pParent;
}

void CHtmlPaletteView::OnInitialUpdate()
{
    CHtmlView::OnInitialUpdate();

    
    Navigate2(_T("file:///C:/Temp/PalettePage.html")); 
}
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
    acutPrintf(_T("\n[HtmlView] Document Complete: %s\n"), lpszURL);
}
// C++ 函数实现：被 JS 调用
void CHtmlPaletteView::OnJsCallCpp(LPCTSTR szMessage)
{
	AfxMessageBox(szMessage);
}

void CHtmlPaletteView::OnJsCommand(LPCTSTR szCommand, LPCTSTR szParam)
{
    acutPrintf(_T("\n[HtmlView] OnJsCommand Received: %s, %s\n"), szCommand, szParam);
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
            dispparams.rgvarg[0].bstrVal = SysAllocString(szArg);
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

            delete[] dispparams.rgvarg;
            pWindow->Release();
        }
        pDoc2->Release();
    }
    pDispatch->Release();
}
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