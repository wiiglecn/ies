// coding: gb18030
#pragma once
#include <afxhtml.h>
#include <mshtml.h>
//#include "MySafeHtmlView.h"

// Forward Declaration
class CDataPalette;

class CHtmlPaletteView : public CHtmlView
{
protected:
    
    DECLARE_DYNCREATE(CHtmlPaletteView)

public:
	CHtmlPaletteView();           // protected constructor used by dynamic creation
    virtual ~CHtmlPaletteView();
    
    // Set the parent window pointer for callback
    void SetParentPalette(CDataPalette* pParent);
    void CallJsFunction(LPCTSTR szFuncName, LPCTSTR szArg);
	void InitializeBrowser();
protected:
    CDataPalette* m_pParentPalette;

    // {{AFX_VIRTUAL(CHtmlPaletteView)
public:
    //virtual void OnInitialUpdate();
	virtual void OnDocumentComplete(LPCTSTR lpszURL);
	virtual HRESULT OnGetExternal(LPDISPATCH *lppDispatch);
	//virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//virtual void PostNcDestroy();

    // }}AFX_VIRTUAL

    // {{AFX_MSG(CHtmlPaletteView)
    // }}AFX_MSG
    DECLARE_MESSAGE_MAP()
    DECLARE_DISPATCH_MAP()
	//DECLARE_INTERFACE_MAP()

	
    
public:
    // Methods exposed to JS calls
    void OnJsCallCpp(LPCTSTR szMessage);
    afx_msg void OnJsCommand(LPCTSTR szCommand, LPCTSTR szParam);
	afx_msg void OnDestroy();
    // 新增：拦截辅助功能查询
    afx_msg LRESULT OnGetObject(WPARAM, LPARAM);
private:
	LRESULT onAcadKeepFocus(WPARAM wParam, LPARAM lParam);
    
    //DECLARE_EVENTSINK_MAP() 
    // Auxiliary function: calling JS from C++
    

    
};