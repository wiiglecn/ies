#include "StdAfx.h"        // ����ĿԼ����cpp �������Ȱ�����MFC _DEBUG ��ܣ�
#include "arxHeaders.h"
#include "cJSON.h"
#include "utils.h"
#include "Init.h"
#include "adslib.h"
#include "AiApiRequest.h"
#include "ApiMessageWnd.h"

AiApiRequest* CInit::hServer = NULL;
CApiMessageWnd* CInit::pApiWnd = NULL;

void CInit::Prepare()
{
	loadMenu();

	pApiWnd = new CApiMessageWnd();

    if (!pApiWnd->Create())
    {
        delete pApiWnd;
        pApiWnd = NULL;

        CUtils::acutPrintf(_T("[ERROR] Create API message window failed\n"));
        return;
    }

    hServer = new AiApiRequest();
    hServer->setAcadWnd(pApiWnd);
    hServer->Start();


}
void CInit::Unload()
{
	if (hServer != NULL)
    {
        hServer->Stop();
        delete hServer;
        hServer = NULL;
    }

	if (pApiWnd != NULL)
    {
        if (::IsWindow(pApiWnd->GetSafeHwnd()))
        {
            pApiWnd->DestroyWindow();
        }

        delete pApiWnd;
        pApiWnd = NULL;
    }


	//unloadMenu();
    /*int ret =acedCommand(
        RTSTR, _T("_.MENUUNLOAD"),
        RTSTR, _T("SLTOOLS"),
        RTNONE
    );*/
	/*int ret = acedMenuCmd(_T("UNLOAD=SLTOOLS"));

    CUtils::acutPrintf(
        _T("[SLUNLOAD] acedCommand result = %d\n"),
        ret
    );*/
}

void CInit::loadMenu()
{
	// 1. ����˵��������ļ���

	CString strFilePath = CUtils::GetArxFolder();
	strFilePath.Replace(_T("\\"), _T("/"));

    CString szMenuPath;
	szMenuPath.Format(_T("support/%s/SLTools.mnu"), CADYEAR);
	szMenuPath = strFilePath + szMenuPath;


    // 1. ���� LISP ����ʽ
    // ע�⣺·��������пո�������ַ�����Ҫ�ʵ����������ڼ�·����ֱ��ƴ�Ӽ���
    //// menuload ���� T ����ɹ���nil ���ʧ��
    //CString strLisp;
    //strLisp.Format(_T("(if (not (menugroup \"%s\")) (menuload \"%s\"))"), 
    //               szGroupName, szMenuPath);

	Adesk::Boolean es = acedIsMenuGroupLoaded(MENUGROUP);

	if (es == Adesk::kTrue) return;

	int ret = acedCommand(
        RTSTR, _T("_.MENULOAD"),
        RTSTR, szMenuPath,
        RTSTR, _T(""),
        RTNONE
    );

	//AcApDocument* pDoc = acDocManager->curDocument();

 //   if (pDoc == NULL)
 //       return;

 //   // ���� MENULOAD ����
 //   CString strCmd;

 //   strCmd.Format(
 //       _T("_.MENULOAD \"%s\"\n"),
 //       szMenuPath
 //   );
	////strCmd = _T("_.MENULOAD \"") + szMenuPath + _T("\"\n");
	//CUtils::acutPrintf(_T("��������ز˵�: %s\n"), strCmd);

 //   Acad::ErrorStatus es =
 //       acDocManager->sendStringToExecute(
 //           pDoc,
 //           strCmd,
 //           true,
 //           false,
 //           false
 //       );

    // 2. ִ�� LISP ����ʽ
    // ads_queueexpr ������ʽ��������첽ִ�У�����������ǰ ARX ����ջ
    // ���� AutoCAD 2005 ʱ�����õļ���
    //ads_queueexpr(strLisp);
    
    // 3. (��ѡ) �����Ҫ������ʾ�˵������Խ����Ŷ�ִ�� menucmd
    // ע�⣺�������첽ִ�У�menuload ���ܻ�û��ɣ�menucmd �Ϳ���ִ����
    // ����ȫ�����������û����������зֲ�ִ�У�����ʹ�ö�ʱ��/��Ӧ��������״̬
    
	//CUtils::acutPrintf(_T("��������ز˵�: %s\n"), szGroupName);

	//return;
}
void CInit::unloadMenu()
{
	acedMenuCmd(_T("UNLOAD=SLTOOLS"));
	/*Adesk::Boolean es = acedIsMenuGroupLoaded(MENUGROUP);
	if (es == Adesk::kTrue)
    {
        acedCommand(
            RTSTR, _T("_.MENUUNLOAD"),
            RTSTR, MENUGROUP,
            RTSTR, _T(""),
            RTNONE
        );
    }*/
}

// ========== ARX Entry Point moved from Palette2005.cpp ==========
#include "../tools/ToolGlobal.h"
#include "./OwnerMenu.h"

void *pPaletteSet = NULL;
AC_IMPLEMENT_EXTENSION_MODULE(PaletteDLL);

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		PaletteDLL.AttachInstance(hInstance);
		InitAcUiDLL();
	} else if (dwReason == DLL_PROCESS_DETACH) {
		PaletteDLL.DetachInstance();
	}
	return TRUE;
}

void paletteShow() { return; }
void paletteHide() { return; }

static void AddCommand2(const TCHAR* cmdGroup, const TCHAR* cmdInt, const TCHAR* cmdLoc,
				const int cmdFlags, const AcRxFunctionPtr cmdProc)
{
	char szCmdGroup[256], szCmdInt[256], szCmdLoc[256];
	strcpy(szCmdGroup, cmdGroup);
	strcpy(szCmdInt, cmdInt);
	strcpy(szCmdLoc, cmdLoc);
    acedRegCmds->addCommand(szCmdGroup, szCmdInt, szCmdLoc, cmdFlags, cmdProc);
}

static void InitApplication()
{
	AddCommand2(MENUGROUP, _T("SLCMD"), _T("SLCMD"), ACRX_CMD_MODAL, ToolGlobal::CMD);
	AddCommand2(MENUGROUP, _T("SLCCV"), _T("SLCCV"), ACRX_CMD_MODAL, ToolGlobal::CreateColumnView);
	AddCommand2(MENUGROUP, _T("SLINIT"), _T("SLINIT"), ACRX_CMD_MODAL, CInit::Prepare);
	AddCommand2(MENUGROUP, _T("SLPLANVIEW"),  _T("SLPLANVIEW"), ACRX_CMD_MODAL, COwnerMenu::SLPlanView);
	AddCommand2(MENUGROUP, _T("SLSECTIONVIEW"), _T("SLSECTIONVIEW"), ACRX_CMD_MODAL, COwnerMenu::SLSectionView);
	AddCommand2(MENUGROUP, _T("SLCOLUMNVIEW"), _T("SLCOLUMNVIEW"), ACRX_CMD_MODAL, COwnerMenu::SLColumnView);
	AddCommand2(MENUGROUP, _T("SLHOLEDIAMETER"), _T("SLHOLEDIAMETER"), ACRX_CMD_MODAL, COwnerMenu::SLHoleDiameter);
	AddCommand2(MENUGROUP, _T("SLMAPPING"), _T("SLMAPPING"), ACRX_CMD_MODAL, COwnerMenu::SLMapping);
	AddCommand2(MENUGROUP, _T("SLCHECK"), _T("SLCHECK"), ACRX_CMD_MODAL, COwnerMenu::SLCheck);
	AddCommand2(MENUGROUP, _T("SLIMPORTDATA"), _T("SLIMPORTDATA"), ACRX_CMD_MODAL, COwnerMenu::SLImportData);
	AddCommand2(MENUGROUP, _T("SLBLOCKLINESELECT"), _T("SLBLOCKLINESELECT"), ACRX_CMD_MODAL, COwnerMenu::SLBlockLineSelect);
	AddCommand2(MENUGROUP, _T("SLPOLYLINEHOLE"), _T("SLPOLYLINEHOLE"), ACRX_CMD_MODAL, COwnerMenu::SLPolylineHole);
	AddCommand2(MENUGROUP, _T("SLARCPICK"), _T("SLARCPICK"), ACRX_CMD_MODAL, COwnerMenu::SLArcPick);
	AddCommand2(MENUGROUP, _T("SLSINGLESCALE"), _T("SLSINGLESCALE"), ACRX_CMD_MODAL, COwnerMenu::SLSingleScale);
	AddCommand2(MENUGROUP, _T("SLALLSCALE"), _T("SLALLSCALE"), ACRX_CMD_MODAL, COwnerMenu::SLAllScale);
}

static void UnloadApplication()
{
	acedRegCmds->removeGroup(_T("ASDK"));
	CInit::Unload();
}

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
void CInit::CmdUnloadSLTools()
{
	acedMenuCmd(_T("UNLOAD=SLTOOLS"));
}