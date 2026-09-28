#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include "cJSON.h"
#include "utils.h"
#include "BaseDataSet.h"



void CBaseDataSet::Execute(const TCHAR* cmd)
{
	if (_tcscmp(cmd, _T("HoleDiameter")) == 0)
	{
		ExecuteHoleDiameter();
	}else if (_tcscmp(cmd, _T("Drafting")) == 0){
		ExecuteDrafting();
	}else if (_tcscmp(cmd, _T("Proofread")) == 0){
		ExecuteProofread();
	}
}
void CBaseDataSet::ExecuteHoleDiameter()
{
	// 1. 读取当前孔直径（profile.hole_diameter，字符串存储）
	CString dOld = _T("0.0");
	cJSON* pRoot = CUtils::GetJsonConfig();
	cJSON* pProfile = cJSON_GetObjectItem(pRoot, "profile");
	cJSON* pHole = cJSON_GetObjectItem(pProfile, HOLE_DIAMETER_ZK);
	dOld = CString(pHole->valuestring);

	// 2. 提示并输入（原孔直径是:xxx，请输入新孔直径）
	TCHAR szPrompt[128];
	_sntprintf(szPrompt, 128, _T("\n原孔直径是: %s，请输入新孔直径: "), dOld);
	double dNew = 0.0;
	int nRet = acedGetReal(szPrompt, &dNew);
	if (nRet != RTNORM) {
		acedPostCommandPrompt();   // 用户取消（ESC / 回车空输入），恢复命令行
		return;
	}
	if (dNew <= 0.0) {
		CUtils::acutPrintf(_T("\n错误: 孔直径必须大于 0.\n"));
		acedPostCommandPrompt();
		return;
	}

	// 3. 写回 JSON 配置 profile.hole_diameter（保持字符串存储，与 SetDiameter/前端一致）
	if (!pRoot) {
		CUtils::acutPrintf(_T("\n错误: 配置未初始化，无法保存.\n"));
		acedPostCommandPrompt();
		return;
	}
	TCHAR szVal[32];
	_sntprintf(szVal, 32, _T("%.2f"), dNew);

#ifdef _CAD2005
	cJSON_ReplaceItemInObject(pProfile, HOLE_DIAMETER_ZK, cJSON_CreateString(szVal));
#else
	cJSON_ReplaceItemInObject(pProfile, HOLE_DIAMETER_ZK,
			cJSON_CreateString(CW2A(szVal, CP_UTF8)));
#endif

	CUtils::acutPrintf(_T("\n孔直径已更新为: %.3f\n"), dNew);
	acedPostCommandPrompt();
}

void CBaseDataSet::ExecuteDrafting()
{
	ExecuteSaveBlock(_T("制图人"));
}
void CBaseDataSet::ExecuteProofread()
{
	ExecuteSaveBlock(_T("校对人"));
}
void CBaseDataSet::ExecuteSaveBlock(const TCHAR* blockname)
{
	if (this->actionBefore()!=0){
		return;
	}
	// 1. 循环选择：选错类型/点空 → 重新选择；ESC/回车 → 取消退出
    AcDbObjectId btrId = AcDbObjectId::kNull;
    while (true) {
        ads_name ent;
        ads_point pt;
        int rt = acedEntSel(_T("选择要导出的块: \n"), ent, pt);

        // ESC (RTCAN) 或直接回车 (RTNONE)：正常取消
        if (rt == RTCAN || rt == RTNONE) {
            CUtils::acutPrintf(_T("已取消.\n"));
            this->actionEnd();
            return;
        }
        // 点空处等 (RTERROR/ERRNO=7)：提示后继续选择
        if (rt != RTNORM) {
            CUtils::acutPrintf(_T("\n未选中实体，请重试.\n"));
            continue;
        }

        AcDbObjectId entId;
        if (acdbGetObjectId(entId, ent) != Acad::eOk) {
            continue;    // 取 id 失败 → 重新选择
        }

        AcDbEntity* pEnt = NULL;
        if (acdbOpenObject(pEnt, entId, AcDb::kForRead) != Acad::eOk) {
            continue;    // 打开失败 → 重新选择
        }

        AcDbBlockReference* pBlkRef = AcDbBlockReference::cast(pEnt);
        if (pBlkRef == NULL) {
            CUtils::acutPrintf(_T("选中的实体不是块参照(INSERT)，请重新选择.\n"));
            pEnt->close();
            continue;    // ★ 回到选择步骤，而不是退出
        }

        btrId = pBlkRef->blockTableRecord();   // 块定义（BTR）
        pBlkRef->close();
        break;           // 选中有效块，跳出循环
    }
	CString strFilePath = CUtils::GetArxFolder();
	strFilePath.Replace(_T("\\"), _T("/"));
	CString cadBlockUrl;
	cadBlockUrl.Format(_T("support/%s/%s.dwg"), CADYEAR, blockname);
	cadBlockUrl = strFilePath + cadBlockUrl;
	
	if (pModelSpace != NULL)
    {
        pModelSpace->close();
        pModelSpace = NULL;
    }

    // 4. 把块定义内容导出到一个新数据库（等效于 WBLOCK 命令选择块的效果）
    AcDbDatabase* pNewDb = NULL;
    Acad::ErrorStatus es = pDb->wblock(pNewDb, btrId);
    if (es != Acad::eOk || pNewDb == NULL) {
        CUtils::acutPrintf(_T("\n导出块失败(错误码 %d).\n"), (int)es);
        delete pNewDb;
		this->actionEnd();
        return;
    }

    // 5. 保存到目标文件（saveAs 直接覆盖同名文件）
    es = pNewDb->saveAs(cadBlockUrl);
    if (es != Acad::eOk) {
        CUtils::acutPrintf(_T("保存文件失败(错误码 %d): %s\n"), (int)es, cadBlockUrl);
    }
	else {
        CUtils::acutPrintf(_T("块已保存到: %s\n"), cadBlockUrl);
    }
	this->actionEnd();
    delete pNewDb;
}