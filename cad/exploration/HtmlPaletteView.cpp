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

    // 尝试注入 external 对象
    // 注意：在较新的 IE 内核中，直接赋值 window.external 可能被禁止。
    // 但我们可以通过执行脚本，让 JS 知道如何调用我们，或者使用一个中间层。
    
    // 这里我们采用一种变通方法：
    // 如果 window.external 不可用，我们可以在 JS 中定义一个 fallback，
    // 或者在这里执行一段 JS，将我们的 IDispatch 指针以某种方式暴露。
    
    // 但实际上，CHtmlView 的 default external 往往就是空的或者只支持 IOleCommandTarget。
    // 为了彻底解决，建议修改 JS 调用方式，或者使用下面的“脚本注入”技巧来测试连通性。
    
    // 调试用：在控制台输出，确认 OnDocumentComplete 被触发
    acutPrintf(_T("\n[HtmlView] Document Complete: %s\n"), lpszURL);
    cJSON* pRootConfig = CMyPaletteSet::GetJsonConfig();
    acutPrintf(_T("[DEBUG] GetJsonConfig returned: %p\n"), pRootConfig);

    if (pRootConfig) {
        acutPrintf(_T("[DEBUG] Checking for 'profile' attribute...\n"));
        
        // Check if "profile" attribute exists
        cJSON* pProfile = cJSON_GetObjectItem(pRootConfig, "profile");
        
        if (pProfile) {
            acutPrintf(_T("[DEBUG] 'profile' attribute found.\n"));
            
            // Convert profile object to JSON string
            char* profileJsonStr = cJSON_Print(pProfile);
            acutPrintf(_T("[DEBUG] cJSON_Print returned: %p\n"), profileJsonStr);
            
            if (profileJsonStr) {
                acutPrintf(_T("[DEBUG] Profile JSON String: %S\n"), profileJsonStr); // Use %S for char* in Unicode build

                // Convert char* to CString/TCHAR* for CallJsFunction
#ifdef UNICODE
                CString strProfile(profileJsonStr);
                acutPrintf(_T("[DEBUG] Calling onReady with CString length: %d\n"), strProfile.GetLength());
                CallJsFunction(_T("onReady"), strProfile);
#else
                acutPrintf(_T("[DEBUG] Calling onReady with char*\n"));
                CallJsFunction(_T("onReady"), profileJsonStr);
#endif
                // Free the string allocated by cJSON_Print
                cJSON_free(profileJsonStr);
                acutPrintf(_T("[DEBUG] Freed profileJsonStr.\n"));
            } else {
                acutPrintf(_T("[DEBUG] ERROR: cJSON_Print failed to serialize profile.\n"));
            }
        } else {
            acutPrintf(_T("[DEBUG] 'profile' attribute NOT found in config.\n"));
        }
    } else {
        acutPrintf(_T("[DEBUG] ERROR: pRootConfig is NULL.\n"));
    }
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