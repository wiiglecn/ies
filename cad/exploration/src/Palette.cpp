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

// Palette.cpp : Initialization functions
#include "StdAfx.h"

//#if defined(_DEBUG) && !defined(AC_FULL_DEBUG)
//#error _DEBUG should not be defined except in internal Adesk debug builds
//#endif

#include "resource.h"
#include "PaletteSet.h"
#include "CPalette.h"
//#include "InspPalette.h"
#include "Utils.h"
#include "Init.h"

#include "../tools/ToolGlobal.h"
#include "./OwnerMenu.h"

//#include "../tools/AcadFrameHook.h"

// Global Palette Set Instance
CMyPaletteSet  *pPaletteSet=NULL;
//CAcadFrameHook *pHook=NULL;

HINSTANCE _hdllInstance =NULL ;


// Forward declaration of DllMain
extern BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);

// Callback for the command "PALETTESHOW"
void paletteShow();

// This command registers an ARX command.
void AddCommand(const TCHAR* cmdGroup, const TCHAR* cmdInt, const TCHAR* cmdLoc,
				const int cmdFlags, const AcRxFunctionPtr cmdProc, const int idLocal = -1);


// NOTE: DO NOT edit the following lines.
//{{AFX_ARX_MSG
void InitApplication();
void UnloadApplication();
//}}AFX_ARX_MSG


/////////////////////////////////////////////////////////////////////////////
// ObjectARX EntryPoint
extern "C" AcRx::AppRetCode 
acrxEntryPoint(AcRx::AppMsgCode msg, void* pkt)
{
	switch (msg) {
	case AcRx::kInitAppMsg:
//#ifdef _CAD2005
//#else
		DllMain(_hdllInstance, DLL_PROCESS_ATTACH, NULL);
//#endif
		// Comment out the following line if your
		// application should be locked into memory
		acrxDynamicLinker->unlockApplication(pkt);
		acrxDynamicLinker->registerAppMDIAware(pkt);
		InitApplication();
		break;
	case AcRx::kUnloadAppMsg:
//#ifdef _CAD2005
//#else
		
//#endif
		UnloadApplication();
		DllMain(_hdllInstance, DLL_PROCESS_DETACH, NULL);
		break;
	}
	return AcRx::kRetOK;
}
// Command implementation for "PALETTESHOW"
void paletteShow()
{
	CMDIFrameWnd* pAcadFrame = acedGetAcadFrame();
	
	// If the paletteset is already created, show it
	if(pPaletteSet)
	{
		pAcadFrame->ShowControlBar(pPaletteSet, TRUE, FALSE);
		return;
	}

	// If paletteset not created, create new palette set
	
	//CInspPalette *pPalette2;

	pPaletteSet = new CMyPaletteSet;
	CRect rect(0, 0, 300, 500);
	pPaletteSet->Create(_T("昇龙 Palette AI"),
		WS_OVERLAPPED | WS_DLGFRAME, 
		rect, 
		acedGetAcadFrame(), 
		PSS_EDIT_NAME | 
		PSS_PROPERTIES_MENU | 
		PSS_AUTO_ROLLUP |
		PSS_CLOSE_BUTTON
		);


	// Instantiate Palettes
	CDataPalette *pPalette1;
	pPalette1 = new CDataPalette();  // data palette

	//-------------Palette1-----------------------------------------
	pPalette1->Create(WS_CHILD | WS_VISIBLE,
		_T("Palette SL"),
		pPaletteSet,
		PS_EDIT_NAME);


	// Add palette to palette set
	pPaletteSet->AddPalette(pPalette1);

	// Finally show the palette set
	pPaletteSet->EnableDocking(CBRS_ALIGN_ANY);
	pPaletteSet->RestoreControlBar();

	// Load PaletteSet pesistent data
	pPaletteSet->LoadPaletteSet();

	pAcadFrame->ShowControlBar(pPaletteSet, TRUE, FALSE);

	if (pPaletteSet->GetOpacity() !=100)
		pPaletteSet->SetOpacity(100);

	// Change the title of the paletteset
	pPaletteSet->SetName(_T("昇龙 工具条"));

	//pHook=new CAcadFrameHook();
}
// 定时器回调函数，用于延迟执行 paletteShow
VOID CALLBACK PaletteShowTimerProc(HWND hWnd, UINT nMsg, UINT_PTR nTimerid, DWORD dwTime)
{
	if (acDocManager->curDocument() != NULL)
	{
		KillTimer(NULL, nTimerid); // 停止定时器，确保只执行一次
		//paletteShow();
		//CInit::loadMenu();
		AcApDocument* pDoc = acDocManager->curDocument();

		if (pDoc == NULL)
			return;
		Acad::ErrorStatus es =
        acDocManager->sendStringToExecute(
            pDoc,
            _T("SLINIT\n"),
            false,
            false,
            true
        );
		CUtils::acutPrintf(_T("send SLINIT status = %d\n"), es);
	}
    
}
void paletteHide()
{
    if(pPaletteSet)
    {
        CMDIFrameWnd* pAcadFrame = acedGetAcadFrame();
        pAcadFrame->ShowControlBar(pPaletteSet, FALSE, FALSE);
    }
}
// Init this application. Register your
// commands, reactors...
void InitApplication()
{
	// Add a command to show the paletteset
	AddCommand(MENUGROUP, _T("SLSHOW"), _T("SLSHOW"), ACRX_CMD_MODAL, paletteShow);
	AddCommand(MENUGROUP, _T("SLHIDE"), _T("SLHIDE"), ACRX_CMD_MODAL, paletteHide);
	AddCommand(MENUGROUP, _T("SLCMD"), _T("SLCMD"), ACRX_CMD_MODAL, ToolGlobal::CMD);
	AddCommand(MENUGROUP, _T("SLCCV"), _T("SLCCV"), ACRX_CMD_MODAL, ToolGlobal::CreateColumnView);
	AddCommand(MENUGROUP, _T("SLINIT"), _T("SLINIT"), ACRX_CMD_MODAL, CInit::Prepare);

	//AddCommand(MENUGROUP, _T("SLUNLOAD"), _T("SLUNLOAD"), ACRX_CMD_MODAL, CInit::CmdUnloadSLTools);


	AddCommand(MENUGROUP, _T("SLPLANVIEW"),  _T("SLPLANVIEW"),
           ACRX_CMD_MODAL, COwnerMenu::SLPlanView);

	AddCommand(MENUGROUP, _T("SLSECTIONVIEW"), _T("SLSECTIONVIEW"),
			ACRX_CMD_MODAL, COwnerMenu::SLSectionView);

	AddCommand(MENUGROUP, _T("SLCOLUMNVIEW"), _T("SLCOLUMNVIEW"),
			ACRX_CMD_MODAL, COwnerMenu::SLColumnView);

	AddCommand(MENUGROUP, _T("SLHOLEDIAMETER"), _T("SLHOLEDIAMETER"),
           ACRX_CMD_MODAL, COwnerMenu::SLHoleDiameter);

	AddCommand(MENUGROUP, _T("SLMAPPING"), _T("SLMAPPING"),
			ACRX_CMD_MODAL, COwnerMenu::SLMapping);

	AddCommand(MENUGROUP, _T("SLCHECK"), _T("SLCHECK"),
			ACRX_CMD_MODAL, COwnerMenu::SLCheck);

	AddCommand(MENUGROUP, _T("SLIMPORTDATA"), _T("SLIMPORTDATA"),
			ACRX_CMD_MODAL, COwnerMenu::SLImportData);

	AddCommand(MENUGROUP, _T("SLBLOCKLINESELECT"), _T("SLBLOCKLINESELECT"),
			ACRX_CMD_MODAL, COwnerMenu::SLBlockLineSelect);

	AddCommand(MENUGROUP, _T("SLPOLYLINEHOLE"), _T("SLPOLYLINEHOLE"),
			ACRX_CMD_MODAL, COwnerMenu::SLPolylineHole);

	AddCommand(MENUGROUP, _T("SLARCPICK"), _T("SLARCPICK"),
			ACRX_CMD_MODAL, COwnerMenu::SLArcPick);

	AddCommand(MENUGROUP, _T("SLSINGLESCALE"), _T("SLSINGLESCALE"),
			ACRX_CMD_MODAL, COwnerMenu::SLSingleScale);

	AddCommand(MENUGROUP, _T("SLALLSCALE"), _T("SLALLSCALE"),
			ACRX_CMD_MODAL, COwnerMenu::SLAllScale);
#ifdef _CAD2005
#else

	// Register the COM Server upon loading
	 /*HRESULT hr = ::DllRegisterServer();
	 if ( FAILED(hr) )
	 	CUtils::acutPrintf(_T("Failed to register COM server. hr=0x%08lX\n"), hr);*/
#endif

	// 延迟 2000 毫秒（2秒）自动显示工具条，确保 AutoCAD 环境准备完毕
    SetTimer(NULL, 1, 2000, PaletteShowTimerProc);
	// 自动显示工具条
    //paletteShow();
}

// Unload this application. Unregister all objects
// registered in InitApplication.
void UnloadApplication()
{

	CInit::Unload();
	acedRegCmds->removeGroup(MENUGROUP);
	// CUtils::FocusAcadDrawing();
	// Unregister commands


	// Destroy the paletteset
	if(pPaletteSet)
	{
		// 【关键新增 2】：获取 AutoCAD 主框架，清除活动视图！
        // 这能阻止 AutoCAD 失去焦点时去通知已被销毁的 CHtmlView
        CMDIFrameWnd* pAcadFrame = acedGetAcadFrame();
        if (pAcadFrame != NULL && pAcadFrame->GetSafeHwnd() != NULL)
        {
            pAcadFrame->SetActiveView(NULL);
        }


        int count = pPaletteSet->GetPaletteCount();
        for (int i = count - 1; i >= 0; i--)
        {
            // Get the palette at index i
            CAdUiPalette* pPal = pPaletteSet->GetPalette(i);
            if (pPal)
            {
				
                // Remove from the set (this usually destroys the window association)
                pPaletteSet->RemovePalette(i);

				if (pPal->GetSafeHwnd())
				{
					CUtils::acutPrintf (_T("CAdUiPalette DestroyWindow \n")) ;
					pPal->DestroyWindow();
				}

                // Delete the object itself since it was created with 'new'
                delete pPal;
            }
        }
		// 2. 隐藏面板，防止系统轮询
		pPaletteSet->ShowWindow(SW_HIDE);
		//pPaletteSet->RemoveAll();
		pPaletteSet->DestroyWindow();
		
		delete pPaletteSet;

		// pPaletteSet = NULL;
	}
	/*if (pHook){
		delete pHook;
	}*/
	CoFreeUnusedLibraries();
}

// This functions registers an ARX command.
// It can be used to read the localized command name
// from a string table stored in the resources.
void AddCommand(const TCHAR* cmdGroup, const TCHAR* cmdInt, const TCHAR* cmdLoc,
				const int cmdFlags, const AcRxFunctionPtr cmdProc, const int idLocal)
{
	#ifdef _CAD2005
		char szCmdGroup[256], szCmdInt[256], szCmdLoc[256];

        #ifdef _UNICODE
            WideCharToMultiByte(CP_ACP, 0, cmdGroup, -1, szCmdGroup, 256, NULL, NULL);
            WideCharToMultiByte(CP_ACP, 0, cmdInt, -1, szCmdInt, 256, NULL, NULL);
            WideCharToMultiByte(CP_ACP, 0, cmdLoc, -1, szCmdLoc, 256, NULL, NULL);
        #else
            strcpy(szCmdGroup, cmdGroup);
            strcpy(szCmdInt, cmdInt);
            strcpy(szCmdLoc, cmdLoc);
        #endif

        if (idLocal != -1) {
            TCHAR cmdLocRes[65];
            ::LoadString(_hdllInstance, idLocal, cmdLocRes, 64);

            #ifdef _UNICODE
                char cmdLocResA[65];
                WideCharToMultiByte(CP_ACP, 0, cmdLocRes, -1, cmdLocResA, 65, NULL, NULL);
                acedRegCmds->addCommand(szCmdGroup, szCmdInt, cmdLocResA, cmdFlags, cmdProc);
            #else
                acedRegCmds->addCommand(szCmdGroup, szCmdInt, cmdLocRes, cmdFlags, cmdProc);
            #endif
        } else {
            acedRegCmds->addCommand(szCmdGroup, szCmdInt, szCmdLoc, cmdFlags, cmdProc);
        }
	#else
		TCHAR cmdLocRes[65];

		// If idLocal is not -1, it's treated as an ID for
		// a string stored in the resources.
		if (idLocal != -1) {

			// Load strings from the string table and register the command.
			::LoadString(_hdllInstance, idLocal, cmdLocRes, 64);
			acedRegCmds->addCommand(cmdGroup, cmdInt, cmdLocRes, cmdFlags, cmdProc);

		} else
			// idLocal is -1, so the 'hard coded'
			// localized function name is used.
			acedRegCmds->addCommand(cmdGroup, cmdInt, cmdLoc, cmdFlags, cmdProc);
	#endif
}