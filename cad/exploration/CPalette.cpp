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


#include "CPalette.h"
#include "PaletteSet.h"
#include "PolylineMarker.h"
#include "LineHolePlacer.h"
#include "Utils.h"
#include "PaletteCleanup.h"
#include "AiApiRequest.h"
#include "declare.h"
#include "ToolDraw.h"
#include "AiChat.h"
#include "Logger.h"
#include "CircleFilter.h"
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
	ON_MESSAGE(WM_USER_TOOL, HandleApiCommand)
    ON_MESSAGE(WM_USER_CHAT, HandleChatReply)
	ON_MESSAGE(WM_USER_CHAT_SEND_ERROR, HandleChatError)
	
	//ON_MESSAGE(WM_JS_COMMAND, HandleJsCommandInternal)
//	ON_CBN_SELCHANGE(ID_COMBO, OnComboChange)
//	ON_EN_UPDATE(ID_STRING_EDIT, OnStringChange)
//	ON_EN_UPDATE(ID_INT_EDIT, OnIntChange)
//	ON_EN_UPDATE(ID_REAL_EDIT, OnRealChange)
END_MESSAGE_MAP()

struct JsCmdData {
        CString cmd;
        CString param;
    };

CDataPalette::CDataPalette():hChat(NULL)
{
	

}

CDataPalette::~CDataPalette()
{
	//
	
}

// Load the data from xml.
BOOL CDataPalette::Load(IUnknown* pUnk) 
{	

	return TRUE;
}

// Save the data to xml.
BOOL CDataPalette::Save(IUnknown* pUnk) 
{
	// Call base class first 
	CAdUiPalette::Save(pUnk);

	return TRUE;
}


// Called by the palette set when the palette is made active
void CDataPalette::OnSetActive() 
{
	CAdUiPalette::OnSetActive();

	// refresg ui
    UpdateHtmlDisplay();
}

int CDataPalette::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	CLogger& logger = CLogger::GetInstance();
	logger.SetLevel(LogLevel::LOG_LEVEL_DEBUG);
	logger.Init(_T("ks_"));

	hServer=new AiApiRequest();
	hServer->Start();
	hServer->setAcadWnd(this);

    if (CAdUiPalette::OnCreate(lpCreateStruct) == -1)
        return -1;

    CRect rectClient;
    GetClientRect(&rectClient);
	// if empty, set a default size for the palette
    if (rectClient.IsRectEmpty())
    {
        rectClient.SetRect(0, 0, 260, 300);
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
	 m_pHtmlView->InitializeBrowser(); 
    // 
    LoadPalette();
    
    // 
    UpdateHtmlDisplay();

    SetName(_T("Data HTML"));
    return 0;
}

void CDataPalette::OnDestroy()
{
	if (hChat) delete hChat;

	hServer->Stop();
	delete hServer;
	hServer = NULL;

	acutPrintf(_T("\nCDataPalette::OnDestroy.\n"));

	//CPaletteCleanup::SafeCleanupHtmlView(m_pHtmlView);

	if (m_pHtmlView)
    {
 //       // 停止网页
 //       m_pHtmlView->Stop();

	//	// 3. 显式释放 IWebBrowser2 接口
 //   // 减少 COM 引用计数，促使内部对象提前清理
 //   LPDISPATCH pDisp = m_pHtmlView->GetApplication();
 //   if (pDisp)
 //   {
 //       pDisp->Release();
 //   }

	//	//m_pHtmlView->Detach(); 
       if (::IsWindow(m_pHtmlView->GetSafeHwnd()))
           m_pHtmlView->DestroyWindow();
 //       // 那么这里 delete
        delete m_pHtmlView;
        m_pHtmlView = NULL;
    }
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
//void CDataPalette::HandleJsCommand(LPCTSTR szCommand, LPCTSTR szParam)
//{
//    // Check if we are already on the main thread? 
//    // Or just always post to be safe.
//    
//    
//    
//    JsCmdData* pData = new JsCmdData();
//    pData->cmd = szCommand;
//    pData->param = szParam;
//    
//    // Post to self. This returns immediately. The actual work happens in OnJsCommand.
//    PostMessage(WM_JS_COMMAND, 0, (LPARAM)pData);
//}
// Helper function to set focus to AutoCAD Drawing Area (VC8 compatible)


LRESULT CDataPalette::HandleJsCommand(LPCTSTR szCommand, LPCTSTR szParam)
{
    
    
    // Restore focus to AutoCAD
    
       //if (acedGetAcadFrame())
           //::SetFocus(acedGetAcadFrame()->m_hWnd);
    /*JsCmdData* pData = reinterpret_cast<JsCmdData*>(lParam);
    if (!pData) {
        return 0;
    }

    CString szCommand = pData->cmd;
    CString szParam = pData->param;

    delete pData;*/

    // Output szParam to AutoCAD console
    //acutPrintf(_T("\nDebug: HandleJsCommandInternal szCommand = %s\n"), szCommand);
    
    if (_tcscmp(szCommand, _T("SetProfile")) == 0)
    {
        // Output szParam to AutoCAD console
        acutPrintf(_T("\nDebug: SetProfile Param = %s\n"), szParam);

        // szParam is a JSON string
        /*if (szParam.IsEmpty()) {
            return 0;
        }*/

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
		CUtils::FocusAcadDrawing();
        CPolylineMarker marker;
        SetDiameter(marker);
        marker.ExecuteSelectionAndMark();
    }
    else if (_tcscmp(szCommand, _T("PreSelectLine")) == 0)
    {
		CUtils::FocusAcadDrawing();
        CLineHolePlacer marker;
        SetDiameter(marker);
        marker.Execute();
    }
	else if (_tcscmp(szCommand, _T("PreSelectCircle")) == 0)
    {
		// 实现只保留当前图层上的 Circle 过滤功能
        CCircleFilter circleFilter;
        circleFilter.Execute();
    }
	else if (_tcscmp(szCommand, _T("Init")) == 0)
    {
       this->onPageReady();
    }
	
	else if (_tcscmp(szCommand, _T("Chat")) == 0)
    {
		if(hChat==NULL){
			hChat = new AiChat();
			hChat->setAcadWnd(this);
		}
		CString message_chat(szParam);
		hChat->Execute(message_chat);
    }

	else if (_tcscmp(szCommand, _T("ReplyContinue")) == 0)
    {
		if(hChat==NULL){
			acutPrintf(_T("\nWarning: 调用异常，chat 服务还没有开始.\n"));
			return 0;
		}
		hChat->ContinueChat();
    }

    /*else if (_tcscmp(szCommand, _T("SwitchItem")) == 0)
    {
        int nIndex = _ttoi(szParam);
        UpdateHtmlDisplay(); 
    }*/
    return 0;
}
LRESULT CDataPalette::HandleChatReply(WPARAM wParam, LPARAM lParam)
{
    CString* pReasoning = (CString*)lParam;
    if (pReasoning && m_pHtmlView)
    {
        switch (wParam)
        {
        case 0:
            m_pHtmlView->CallJsFunction(_T("onReplyContinue"), *pReasoning);
            break;
        case 1:
            m_pHtmlView->CallJsFunction(_T("onReplyStop"), *pReasoning);
            break;
        default:
            m_pHtmlView->CallJsFunction(_T("onReply"), *pReasoning);
            break;
        }
        delete pReasoning;
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
void CDataPalette::onPageReady()
{
	// 尝试注入 external 对象
    // 注意：在较新的 IE 内核中，直接赋值 window.external 可能被禁止。
    // 但我们可以通过执行脚本，让 JS 知道如何调用我们，或者使用一个中间层。
    
    // 这里我们采用一种变通方法：
    // 如果 window.external 不可用，我们可以在 JS 中定义一个 fallback，
    // 或者在这里执行一段 JS，将我们的 IDispatch 指针以某种方式暴露。
    
    // 但实际上，CHtmlView 的 default external 往往就是空的或者只支持 IOleCommandTarget。
    // 为了彻底解决，建议修改 JS 调用方式，或者使用下面的“脚本注入”技巧来测试连通性。
    
    
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
                m_pHtmlView->CallJsFunction(_T("onReady"), strProfile);
#else
                acutPrintf(_T("[DEBUG] Calling onReady with char*\n"));
                m_pHtmlView->CallJsFunction(_T("onReady"), profileJsonStr);
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
void CDataPalette::SetDiameter(CBaseMarker& marker)
{
    cJSON* pRootConfig = CMyPaletteSet::GetJsonConfig();
    if (pRootConfig) {
        // Check if "profile" attribute exists
        cJSON* pProfile = cJSON_GetObjectItem(pRootConfig, "profile");
        
        if (pProfile) {
            cJSON* hole_diameter = cJSON_GetObjectItem(pProfile, "hole_diameter");
             // Check if the item exists and is a string
            if (hole_diameter && cJSON_IsString(hole_diameter) && (hole_diameter->valuestring != NULL))
            {
                // Convert char* (from cJSON) to CString (handles Unicode/MBCS conversion)
                CString strDiameter(hole_diameter->valuestring);
                
                // Convert CString to float
                float diameter = (float)_tstof(strDiameter);
                
                // Example: Set the diameter to the marker object
                // Assuming BaseMarker has a method like SetDiameter or a public member m_Diameter
                // marker.SetDiameter(diameter); 
                // OR if m_Diameter is public:
                // marker.m_Diameter = diameter;
                
                acutPrintf(_T("[INFO] Hole Diameter set to: %.3f\n"), diameter);
                marker.SetDiameter(diameter);
            }
            else
            {
                acutPrintf(_T("[WARNING] hole_diameter is not a valid string in JSON.\n"));
            }
        }
    }
}
LRESULT CDataPalette::HandleApiCommand(WPARAM wParam, LPARAM lParam)
{
	AiToolCommandData* params=(AiToolCommandData*) lParam;
	switch(wParam){
		case WM_USER_A_CREATELINE:
			ToolDraw::getInstance().create_line(params);
			break;
		case WM_USER_A_CREATECIRCLE:
			ToolDraw::getInstance().create_circle(params);
			break;
		case WM_USER_A_CREATERECTANGLE:
			ToolDraw::getInstance().create_rectangle(params);
			break;
		case WM_USER_A_CREATERTEXT:
			ToolDraw::getInstance().create_text(params);
			break;
		case WM_USER_A_CREATELAYER:
			ToolDraw::getInstance().create_layer(params);
			break;
		case WM_USER_A_LISTLAYERS:
			ToolDraw::getInstance().list_layers(params);
			break;
		case WM_USER_A_REGEN:
			ToolDraw::getInstance().regen();
			break;
	}
    //acedUpdateDisplay();
	// Signal the waiting thread
    SetEvent(params->hEvent);

	return 0;
}
LRESULT CDataPalette::HandleChatError(WPARAM wParam, LPARAM lParam)
{
	if (m_pHtmlView){
		m_pHtmlView->CallJsFunction(_T("onChatError"), _T(""));
	}
	return 0;
}