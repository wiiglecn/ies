// coding: gb18030
// MyHtmlView.h
#pragma once
#include <afxhtml.h>

class CMySafeHtmlView : public CHtmlView
{
protected:
    
    DECLARE_DYNCREATE(CMySafeHtmlView)
public:
    CMySafeHtmlView();
    virtual ~CMySafeHtmlView();
protected:
	//DECLARE_DYNCREATE(CMySafeHtmlView)
    // 重写 IAccessible 方法以防止崩溃
    virtual HRESULT get_accName(VARIANT varChild, BSTR *pszName) override;
    virtual HRESULT get_accValue(VARIANT varChild, BSTR *pszValue) override;
    virtual HRESULT get_accRole(VARIANT varChild, VARIANT *pvarRole) override;

	virtual void OnDestroy();
    virtual void PostNcDestroy();
	
protected:

    // 是否已经开始销毁
    BOOL m_bDestroying;
public:
	DECLARE_MESSAGE_MAP()

	afx_msg LRESULT OnGetObject(WPARAM, LPARAM);
};

