// coding: gb18030
// (C) Copyright 2000-2006 by Autodesk, Inc. 
//
// Permission to use, copy, modify, and distribute this software in
// object code form for any purpose and without fee is hereby granted, 
// provided that the above copyright notice appears in all copies and 
// that both that copyright notice and the limited warranty and
// restricted rights notice below appear in all supporting 
// documentation.
//
// AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS. 
// AUTODESK SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTY OF
// MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE.  AUTODESK, INC. 
// DOES NOT WARRANT THAT THE OPERATION OF THE PROGRAM WILL BE
// UNINTERRUPTED OR ERROR FREE.
//
// Use, duplication, or disclosure by the U.S. Government is subject to 
// restrictions set forth in FAR 52.227-19 (Commercial Computer
// Software - Restricted Rights) and DFAR 252.227-7013(c)(1)(ii)
// (Rights in Technical Data and Computer Software), as applicable.
//
//

//-----------------------------------------------------------------------------
#include "StdAfx.h"

#if defined(_DEBUG) && !defined(AC_FULL_DEBUG)
#error _DEBUG should not be defined except in internal Adesk debug builds
#endif

#include "aduiPalette.h"
#include "CPalette.h"
#include "PaletteSet.h"



//Utility functions to add and get the child node from the parent XML-DOM node
extern BOOL GetChildNode(MSXML::IXMLDOMNode* pNode, 
								LPCTSTR pszNodeName, 
								MSXML::IXMLDOMNode** ppChild);

extern BOOL AddChildNode(MSXML::IXMLDOMNode* pNode, LPCTSTR pszChildNodeName,
						 short nNodeType, MSXML::IXMLDOMNode** ppChild);

// The file name of the persisted palette 
#define DATA_PALETTE_FILENAME _T("TestDataPalette.xml")

//-----------------------------------------------------------------------------
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define WM_JS_COMMAND (WM_USER + 100)

//-----------------------------------------------------------------------------
IMPLEMENT_DYNCREATE(CDataPalette, CAdUiPalette)


//-----------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CDataPalette, CAdUiPalette)
//	ON_MESSAGE(WM_CTLCOLORSTATIC, OnCtlColorStatic)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_MESSAGE(WM_JS_COMMAND, HandleJsCommandInternal)
//	ON_CBN_SELCHANGE(ID_COMBO, OnComboChange)
//	ON_EN_UPDATE(ID_STRING_EDIT, OnStringChange)
//	ON_EN_UPDATE(ID_INT_EDIT, OnIntChange)
//	ON_EN_UPDATE(ID_REAL_EDIT, OnRealChange)
END_MESSAGE_MAP()

CDataPalette::CDataPalette()
{
}

CDataPalette::~CDataPalette()
{
	//
	if (m_pHtmlView)
    {
        m_pHtmlView->DestroyWindow();
        delete m_pHtmlView;
        m_pHtmlView = NULL;
    }
}

// Load the data from xml.
BOOL CDataPalette::Load(IUnknown* pUnk) 
{	
	// Call base class first 
	//CAdUiPalette::Load(pUnk);
	//
	//CComQIPtr<MSXML::IXMLDOMNode> pNode(pUnk);
	//MSXML::IXMLDOMNodePtr pChild, pChild1;

	//BSTR text;
	//////////////// Item 1 ///////////////////////
	//GetChildNode(pNode, _T("Item1"), &pChild);
	//GetChildNode(pChild, _T("String"), &pChild1);
	//pChild1->get_text(&text);
	//str[0] = CString(text);

	//GetChildNode(pChild, _T("Integer"), &pChild1);
	//pChild1->get_text(&text);
	//integer[0] = _ttoi(CString(text));

	//GetChildNode(pChild, _T("Real"), &pChild1);
	//pChild1->get_text(&text);
	//real[0] = _tstof(CString(text));

	/////////////////// Item 2 ///////////////////
	//GetChildNode(pNode, _T("Item2"), &pChild);
	//GetChildNode(pChild, _T("String"), &pChild1);
	//pChild1->get_text(&text);
	//str[1] = CString(text);

	//GetChildNode(pChild, _T("Integer"), &pChild1);
	//pChild1->get_text(&text);
	//integer[1] = _ttoi(CString(text));

	//GetChildNode(pChild, _T("Real"), &pChild1);
	//pChild1->get_text(&text);
	//real[1] = _tstof(CString(text));

	return TRUE;
}

// Save the data to xml.
BOOL CDataPalette::Save(IUnknown* pUnk) 
{
	// Call base class first 
	CAdUiPalette::Save(pUnk);
	
	//CComQIPtr<MSXML::IXMLDOMNode> pNode(pUnk);
	//MSXML::IXMLDOMNodePtr pChild, pChild1;
	//CString *pText=new CString;
	//int  n = m_Combo.GetCount();

	/////////////////// Item 1 ///////////////////////
	//AddChildNode(pNode,_T("Item1"),MSXML::NODE_ELEMENT, &pChild);	
	//AddChildNode(pChild, _T("String"), MSXML::NODE_ELEMENT, &pChild1);
	//pChild1->put_text(CString(str[0]).AllocSysString());
	//

	//AddChildNode(pChild, _T("Integer"), MSXML::NODE_ELEMENT, &pChild1);
	//pChild1->put_text(bstr_t(integer[0]));
	//

	//AddChildNode(pChild, _T("Real"), MSXML::NODE_ELEMENT, &pChild1);
	//pChild1->put_text(bstr_t(real[0]));
	//
	//////////////////// Item 2 /////////////////////////
	//AddChildNode(pNode,_T("Item2"),MSXML::NODE_ELEMENT, &pChild);	
	//AddChildNode(pChild, _T("String"), MSXML::NODE_ELEMENT, &pChild1);
	//pChild1->put_text(CString(str[1]).AllocSysString());
	//

	//AddChildNode(pChild, _T("Integer"), MSXML::NODE_ELEMENT, &pChild1);
	//pChild1->put_text(bstr_t(integer[1]));
	//

	//AddChildNode(pChild, _T("Real"), MSXML::NODE_ELEMENT, &pChild1);
	//pChild1->put_text(bstr_t(real[1]));

	/////////////////////////////

	return TRUE;
}


// Called by the palette set when the palette is made active
void CDataPalette::OnSetActive() 
{
	CAdUiPalette::OnSetActive();

	// refresg ui
    UpdateHtmlDisplay();
}


//LRESULT CDataPalette::OnCtlColorStatic(WPARAM wParam, LPARAM lParam)
//{
//	HDC hDC = (HDC)wParam;
//
//	// match the background of all static controls to the palette's background color 
//	CDC *pDC = CDC::FromHandle(hDC);
//	pDC->SetBkMode(TRANSPARENT);
//	return (LRESULT)HBRUSH(m_Brush);
//
//}

// Changes text in edit boxes for "Item1" and "Item2"
//void CDataPalette::OnComboChange() 
//{
//	CString s;
//	if(m_Combo.GetCurSel() == 0) {
//		m_StringEdit.SetWindowText(str[0]);
//		s.Format(_T("%d"), integer[0]);
//		m_IntEdit.SetWindowText(s);
//		s.Format(_T("%.3f"),real[0]);
//		m_RealEdit.SetWindowText(s);
//		m_TextLabel.SetWindowText(_T("Item 1"));
//	} else if (m_Combo.GetCurSel() == 1) {
//		m_StringEdit.SetWindowText(str[1]);
//		s.Format(_T("%d"), integer[1]);
//		m_IntEdit.SetWindowText(s);
//		s.Format(_T("%.3f"),real[1]);
//		m_RealEdit.SetWindowText(s);
//		m_TextLabel.SetWindowText(_T("Item 2"));
//	}
//}

// "Integer" edit box update
//void CDataPalette::OnIntChange()
//{
//	CString s;
//	if(m_Combo.GetCurSel() == 0) { 
//		m_IntEdit.GetWindowText(s);
//		integer[0] = _ttoi((LPCTSTR)s);
//	} else if(m_Combo.GetCurSel() == 1) { 
//		m_IntEdit.GetWindowText(s);
//		integer[1] = _ttoi((LPCTSTR)s);
//	}
//}

// "String" edit box update
//void CDataPalette::OnStringChange()
//{
//	CString s;
//	if(m_Combo.GetCurSel() == 0) { 
//		m_StringEdit.GetWindowText(s);
//		str[0] = s;
//	} else if(m_Combo.GetCurSel() == 1) { 
//		m_StringEdit.GetWindowText(s);
//		str[1] = s;
//	}
//}

// "Real" edit box update
//void CDataPalette::OnRealChange()
//{
//	CString s;
//	if(m_Combo.GetCurSel() == 0) { 
//		m_RealEdit.GetWindowText(s);
//		real[0] = _tstof((LPCTSTR)s);
//	} else if(m_Combo.GetCurSel() == 1) { 
//		m_RealEdit.GetWindowText(s);
//		real[1] = _tstof((LPCTSTR)s);
//	}
//}

//void CDataPalette::addControls()
//{
int CDataPalette::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CAdUiPalette::OnCreate(lpCreateStruct) == -1)
        return -1;

    CRect rectClient;
    GetClientRect(&rectClient);
	// if empty, set a default size for the palette
    if (rectClient.IsRectEmpty())
    {
        rectClient.SetRect(0, 0, 250, 300);
    }

    m_pHtmlView = new CHtmlPaletteView();
    
    if (!m_pHtmlView->Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
                             rectClient, this, AFX_IDW_PANE_FIRST))
    {
		// 3. create CHtmlPaletteView failed
        acutPrintf(_T("\nError: Failed to create CHtmlPaletteView.\n"));
        
        delete m_pHtmlView;
        m_pHtmlView = NULL;
        return -1;
    }
    m_pHtmlView->SetParentPalette(this);

	m_pHtmlView->MoveWindow(&rectClient, TRUE);
	 m_pHtmlView->OnInitialUpdate(); 
    // 
    LoadPalette();
    
    // 
    UpdateHtmlDisplay();

    SetName(_T("Data HTML"));
    return 0;
}

void CDataPalette::OnDestroy()
{
	// Save Palette's persistent data
	SavePalette();
	CAdUiPalette::OnDestroy();
}

//Called by AutoCAD to steal focus from the palette
bool CDataPalette::CanFrameworkTakeFocus()
{
	// not simply calling IsFloating() (a BOOL) avoids warning C4800
	return ( GetPaletteSet()->IsFloating() == TRUE ? true : false );
}

void CDataPalette::LoadPalette()
{
	
}

void CDataPalette::SavePalette()
{
	
}
void CDataPalette::HandleJsCommand(LPCTSTR szCommand, LPCTSTR szParam)
{
    // Check if we are already on the main thread? 
    // Or just always post to be safe.
    
    struct JsCmdData {
        CString cmd;
        CString param;
    };
    
    JsCmdData* pData = new JsCmdData();
    pData->cmd = szCommand;
    pData->param = szParam;
    
    // Post to self. This returns immediately. The actual work happens in OnJsCommand.
    PostMessage(WM_JS_COMMAND, 0, (LPARAM)pData);
}

// 修改函数签名以匹配头文件声明和 MFC 消息映射要求
LRESULT CDataPalette::HandleJsCommandInternal(WPARAM wParam, LPARAM lParam)
{
    // 定义相同的结构体以便解析
    struct JsCmdData {
        CString cmd;
        CString param;
    };

    // 从 lParam 中恢复数据指针
    JsCmdData* pData = reinterpret_cast<JsCmdData*>(lParam);
    if (!pData) {
        return 0;
    }

    // 提取字符串数据
    CString szCommand = pData->cmd;
    CString szParam = pData->param;

    // 重要：删除堆上分配的内存，防止内存泄漏
    delete pData;

    // Output szParam to AutoCAD console
    //acutPrintf(_T("\nDebug: HandleJsCommandInternal szCommand = %s\n"), szCommand);
    
    if (_tcscmp(szCommand, _T("SetProfile")) == 0)
    {
        // Output szParam to AutoCAD console
        acutPrintf(_T("\nDebug: SetProfile Param = %s\n"), szParam);

        // szParam is a JSON string
        if (szParam.IsEmpty()) {
            return 0;
        }

        // 1. Parse the incoming JSON string
        // Convert CString/TCHAR* to UTF-8 char* for cJSON if necessary.
        CStringA utf8Param(CW2A(szParam, CP_UTF8));
        
        cJSON* pIncomingJson = cJSON_Parse(utf8Param.GetString());
        if (!pIncomingJson) {
            // Invalid JSON in szParam
            acutPrintf(_T("\nError: Failed to parse JSON.\n"));
            return 0;
        }

        // 2. Ensure s_pJsonConfig exists
        cJSON* pRootConfig = CMyPaletteSet::GetJsonConfig();
        if (!pRootConfig) {
            // If config is null, we can't save. 
            acutPrintf(_T("\nWarning: JSON Config is NULL. Cannot save profile.\n"));
            cJSON_Delete(pIncomingJson);
            return 0;
        }

        // 3. Get or Create the "profile" object in root config
        cJSON* pProfile = cJSON_GetObjectItemCaseSensitive(pRootConfig, "profile");
        if (!pProfile || !cJSON_IsObject(pProfile)) {
            // Remove old if it's not an object (e.g., string/number)
            if (pProfile) {
                cJSON_DeleteItemFromObject(pRootConfig, "profile");
            }
            // Create new profile object
            pProfile = cJSON_CreateObject();
            cJSON_AddItemToObject(pRootConfig, "profile", pProfile);
        }

        // 4. Merge pIncomingJson into pProfile
        // Iterate through keys in pIncomingJson and add/update them in pProfile
        cJSON* pChild = NULL;
        cJSON_ArrayForEach(pChild, pIncomingJson) {
            if (pChild->string) {
                // Duplicate the item to detach it from incoming json
                cJSON* pDuplicate = cJSON_Duplicate(pChild, 1); // 1 = recursive
                if (pDuplicate) {
                    // Delete existing key in profile if exists
                    cJSON_DeleteItemFromObject(pProfile, pChild->string);
                    // Add new/updated item
                    cJSON_AddItemToObject(pProfile, pChild->string, pDuplicate);
                }
            }
        }

        // 5. Clean up incoming JSON
        cJSON_Delete(pIncomingJson);

        // 6. Save the updated config
        CMyPaletteSet::SavePaletteSet();
        
        // Optional: Update UI if needed
        // UpdateHtmlDisplay();
        
    }
    else if (_tcscmp(szCommand, _T("PreSelectBlock")) == 0)
    {
       
    }
    else if (_tcscmp(szCommand, _T("SwitchItem")) == 0)
    {
        int nIndex = _ttoi(szParam);
        UpdateHtmlDisplay(); 
    }

    return 0;
}
void CDataPalette::UpdateHtmlDisplay()
{
    if (!m_pHtmlView) return;

    COLORREF bgColor = ::GetSysColor(COLOR_3DFACE); 

    CString sBgColor;
    sBgColor.Format(_T("#%02X%02X%02X"), GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor));

	acutPrintf(_T("\nsetBackgroundColor: %s"), sBgColor);
    // 3. Call the JavaScript function to update the background
    m_pHtmlView->CallJsFunction(_T("setBackgroundColor"), sBgColor);
}
void CDataPalette::OnSize(UINT nType, int cx, int cy)
{
    CAdUiPalette::OnSize(nType, cx, cy);

    if (m_pHtmlView && m_pHtmlView->GetSafeHwnd())
    {
        m_pHtmlView->MoveWindow(0, 0, cx, cy, TRUE);
    }
}