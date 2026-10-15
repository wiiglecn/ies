// coding: gb18030
// (C) Copyright 2000-2006 by Autodesk, Inc. 
// Permission to use, copy, modify, and distribute this software in
// object code form for any purpose and without fee is hereby granted, 
// provided that the above copyright notice appears in all copies and 
// that both that copyright notice and the limited warranty and
// restricted rights notice below appear in all supporting 
// documentation.

#include "StdAfx.h"
#include "resource.h"
#include "Utils.h"
#include "Init.h"
#include "../tools/ToolGlobal.h"
#include "./OwnerMenu.h"

// Sidebar palette UI disabled, pointer is opaque
void *pPaletteSet = NULL;

AC_IMPLEMENT_EXTENSION_MODULE(PaletteDLL);

HINSTANCE _hdllInstance = NULL;

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
        _hdllInstance = hInstance;
		PaletteDLL.AttachInstance(hInstance);
		InitAcUiDLL();
	} else if (dwReason == DLL_PROCESS_DETACH) {
		PaletteDLL.DetachInstance();
	}
	return TRUE;
}

void paletteShow();
void paletteHide();

void AddCommand(const TCHAR* cmdGroup, const TCHAR* cmdInt, const TCHAR* cmdLoc,
				const int cmdFlags, const AcRxFunctionPtr cmdProc, const int idLocal = -1);

void InitApplication();
void UnloadApplication();

extern "C" AcRx::AppRetCode 
acrxEntryPoint(AcRx::AppMsgCode msg, void* pkt)
{
	switch (msg) {
	case AcRx::kInitAppMsg:
		acrxDynamicLinker->unlockApplication(pkt);
		acrxDynamicLinker->registerAppMDIAware(pkt);
		InitApplication();
		break;
	case AcRx::kUnloadAppMsg:
		UnloadApplication();
		break;
	}
	return AcRx::kRetOK;
}

void paletteShow()
{
	// Sidebar palette UI disabled, no CAdUi dependency
	return;
}

VOID CALLBACK PaletteShowTimerProc(HWND hWnd, UINT nMsg, UINT_PTR nTimerid, DWORD dwTime)
{
	if (acDocManager->curDocument() != NULL)
	{
		KillTimer(NULL, nTimerid);
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
	}
}

void paletteHide()
{
    if(pPaletteSet)
    {
		// UI disabled, no action
    }
}

void InitApplication()
{
	AddCommand(MENUGROUP, _T("SLCMD"), _T("SLCMD"), ACRX_CMD_MODAL, ToolGlobal::CMD);
	AddCommand(MENUGROUP, _T("SLCCV"), _T("SLCCV"), ACRX_CMD_MODAL, ToolGlobal::CreateColumnView);
	AddCommand(MENUGROUP, _T("SLINIT"), _T("SLINIT"), ACRX_CMD_MODAL, CInit::Prepare);

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

    SetTimer(NULL, 1, 2000, PaletteShowTimerProc);
}

void UnloadApplication()
{
	acedRegCmds->removeGroup(_T("ASDK"));
	pPaletteSet = NULL;
}

void AddCommand(const TCHAR* cmdGroup, const TCHAR* cmdInt, const TCHAR* cmdLoc,
				const int cmdFlags, const AcRxFunctionPtr cmdProc, const int idLocal)
{
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

    acedRegCmds->addCommand(szCmdGroup, szCmdInt, szCmdLoc, cmdFlags, cmdProc);
}
