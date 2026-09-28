// coding: gb18030
// MyHtmlView.cpp
#include "stdafx.h"
#include "MySafeHtmlView.h"

#ifndef WM_GETOBJECT
#define WM_GETOBJECT 0x003D
#endif

IMPLEMENT_DYNCREATE(CMySafeHtmlView, CHtmlView);

BEGIN_MESSAGE_MAP(CMySafeHtmlView, CHtmlView)
    ON_WM_DESTROY()
    ON_MESSAGE(WM_GETOBJECT, OnGetObject)
END_MESSAGE_MAP()

CMySafeHtmlView::CMySafeHtmlView()
{
    m_bDestroying = FALSE;
}

CMySafeHtmlView::~CMySafeHtmlView()
{
    m_bDestroying = TRUE;
}

void CMySafeHtmlView::OnDestroy()
{
    m_bDestroying = TRUE;

    CHtmlView::OnDestroy();
}

void CMySafeHtmlView::PostNcDestroy()
{
    m_bDestroying = TRUE;

    CHtmlView::PostNcDestroy();
}

LRESULT CMySafeHtmlView::OnGetObject(WPARAM wParam, LPARAM lParam)
{
    //
    // 已经进入销毁阶段
    // 不允许再访问 MFC 的 Accessible 对象
    //
    if (m_bDestroying)
        return 0;

    //
    // 窗口已经不存在
    //
    if (!::IsWindow(m_hWnd))
        return 0;

    //
    // 继续交给 CHtmlView 处理
    //
    return CHtmlView::DefWindowProc(
        WM_GETOBJECT,
        wParam,
        lParam);
}
HRESULT CMySafeHtmlView::get_accName(VARIANT varChild, BSTR *pszName)
{
    // 防御性编程：如果窗口已销毁，直接返回错误，不再访问任何成员变量
    if (!::IsWindow(m_hWnd))
    {
        return DISP_E_MEMBERNOTFOUND;
    }
    return CHtmlView::get_accName(varChild, pszName);
}

HRESULT CMySafeHtmlView::get_accValue(VARIANT varChild, BSTR *pszValue)
{
    if (!::IsWindow(m_hWnd))
    {
        return DISP_E_MEMBERNOTFOUND;
    }
    return CHtmlView::get_accValue(varChild, pszValue);
}

HRESULT CMySafeHtmlView::get_accRole(VARIANT varChild, VARIANT *pvarRole)
{
    if (!::IsWindow(m_hWnd))
    {
        return DISP_E_MEMBERNOTFOUND;
    }
    return CHtmlView::get_accRole(varChild, pvarRole);
}
