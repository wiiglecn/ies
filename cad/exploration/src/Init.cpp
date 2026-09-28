#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
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
	// 1. 定义菜单组名和文件名

	CString strFilePath = CUtils::GetArxFolder();
	strFilePath.Replace(_T("\\"), _T("/"));

    CString szMenuPath;
	szMenuPath.Format(_T("support/%s/SLTools.mnu"), CADYEAR);
	szMenuPath = strFilePath + szMenuPath;


    // 1. 构建 LISP 表达式
    // 注意：路径中如果有空格或特殊字符，需要适当处理，但在简单路径下直接拼接即可
    //// menuload 返回 T 如果成功，nil 如果失败
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

 //   // 构造 MENULOAD 命令
 //   CString strCmd;

 //   strCmd.Format(
 //       _T("_.MENULOAD \"%s\"\n"),
 //       szMenuPath
 //   );
	////strCmd = _T("_.MENULOAD \"") + szMenuPath + _T("\"\n");
	//CUtils::acutPrintf(_T("已请求加载菜单: %s\n"), strCmd);

 //   Acad::ErrorStatus es =
 //       acDocManager->sendStringToExecute(
 //           pDoc,
 //           strCmd,
 //           true,
 //           false,
 //           false
 //       );

    // 2. 执行 LISP 表达式
    // ads_queueexpr 将表达式放入队列异步执行，不会阻塞当前 ARX 调用栈
    // 这是 AutoCAD 2005 时代常用的技巧
    //ads_queueexpr(strLisp);
    
    // 3. (可选) 如果需要立即显示菜单，可以接着排队执行 menucmd
    // 注意：由于是异步执行，menuload 可能还没完成，menucmd 就可能执行了
    // 更安全的做法是在用户交互命令中分步执行，或者使用定时器/反应器检查加载状态
    
	//CUtils::acutPrintf(_T("已请求加载菜单: %s\n"), szGroupName);

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
void CInit::CmdUnloadSLTools()
{
	acedMenuCmd(_T("UNLOAD=SLTOOLS"));
}