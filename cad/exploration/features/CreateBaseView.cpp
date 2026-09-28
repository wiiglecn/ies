#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include "cJSON.h"
#include "utils.h"
#include "CreateBaseView.h"
#include "PatHatchLoader.h"

CCreateBaseView::CCreateBaseView():ToolDrawBase()
{
    s_pJsonData = NULL;
	s_patConfigData = NULL;
}

CCreateBaseView::~CCreateBaseView()
{
     ClearExtDbCache();   // 防御性释放
}

void CCreateBaseView::ClearExtDbCache()
{
    std::map<CString, AcDbDatabase*>::iterator it;
    for (it = m_extDbCache.begin(); it != m_extDbCache.end(); ++it)
        delete it->second;
    m_extDbCache.clear();

	if (s_pJsonData)
		cJSON_free(s_pJsonData);
	if (s_patConfigData)
		cJSON_free(s_patConfigData);

}


AcDbDatabase* CCreateBaseView::GetExtDb(const ACHAR* pszDwgPath)
{
    CString key = CUtils::NormalizeDwgPath(pszDwgPath);

    // 1) 命中缓存直接返回
    std::map<CString, AcDbDatabase*>::iterator it = m_extDbCache.find(key);
    if (it != m_extDbCache.end())
        return it->second;

    // 2) 未命中：读入新 side database
    AcDbDatabase* pNewDb = new AcDbDatabase(false, true);
    Acad::ErrorStatus es = pNewDb->readDwgFile(key);
    if (es != Acad::eOk)
    {
        acutPrintf(_T("\n[DWG导入] 错误：读取 DWG 失败(错误码 %d)：%s"), (int)es, pszDwgPath);
        delete pNewDb;
        return NULL;
    }
    // 可选：释放文件句柄但保留内存中的库，模板 DWG 不会被锁住
    // pNewDb->closeInput(true);

    m_extDbCache.insert(std::make_pair(key, pNewDb));
    return pNewDb;
}

//-----------------------------------------------------------------------------
AcDbObjectId CCreateBaseView::ImportDwgAsBlock(const ACHAR* pszDwgPath,
                                    const AcGePoint3d& ptInsert,
									const ACHAR* arrayName,
                                    double dScale,
                                    double dRotation,
                                    const ACHAR* pszLayer)
{

    if (pszDwgPath == NULL || pszDwgPath[0] == _T('\0'))
    {
        acutPrintf(_T("\n[DWG导入] 错误：文件路径为空。"));
		return AcDbObjectId::kNull;
    }
    // 2. 把外部 DWG 读入内存侧数据库（side database）
    //    参数：buildDefaultSymbolTable=false, noDocument=true
	Acad::ErrorStatus es;
	// 2. 按路径获取（或缓存）外部数据库
    AcDbDatabase* pExtDb = GetExtDb(pszDwgPath);   // 局部变量，遮蔽旧成员
    if (pExtDb == NULL)
        return false;
    // 3. 当前工作数据库
    AcDbDatabase* pCurDb = this->pDb;
    if (pCurDb == NULL)
    {
        acutPrintf(_T("\n[DWG导入] 错误：无法获取当前工作数据库。"));
        return AcDbObjectId::kNull;
    }

    // 4. 用文件名作为块名（重名时自动加 _1、_2 后缀）
    ACHAR szBlockName[256];
	CUtils::SafeCopy(szBlockName, 256, arrayName, (int)_tcslen(arrayName));

    // 5. 判断块是否已存在
    AcDbBlockTable* pBlkTbl = NULL;
    if (pCurDb->getBlockTable(pBlkTbl, AcDb::kForRead) != Acad::eOk)
    {
        acutPrintf(_T("\n[DWG导入] 错误：无法打开块表。"));
        return false;
    }

    AcDbObjectId blkDefId = AcDbObjectId::kNull;
    AcDbObjectId existingId;
    bool bExist = false;
    if (pBlkTbl->getAt(szBlockName, existingId) == Acad::eOk
        && !existingId.isNull())
    {
        blkDefId = existingId;
        bExist = true;
        CUtils::acutPrintf(_T("\n[DWG导入] 块 \"%s\" 已存在，不插入新参照。"), szBlockName);

		//存在则跳过
		pBlkTbl->close();
		return blkDefId;
    }
    pBlkTbl->close();

    if (!bExist)
    {
        // 不存在：导入块定义
        es = pCurDb->insert(blkDefId, szBlockName, pExtDb, true);
        if (es != Acad::eOk || blkDefId.isNull())
        {
            CUtils::acutPrintf(_T("\n[DWG导入] 错误：导入块定义失败(错误码 %d)，块名：%s"),
                (int)es, szBlockName);
            return blkDefId;
        }

        // 6. 插入块参照 —— ★ 仅新建块执行，已存在块完全跳过（坐标/图层都不动）
        AcDbBlockReference* pBlkRef = new AcDbBlockReference;
        pBlkRef->setBlockTableRecord(blkDefId);
        pBlkRef->setPosition(ptInsert);
        pBlkRef->setScaleFactors(AcGeScale3d(dScale));
        pBlkRef->setRotation(dRotation);
        if (pszLayer != NULL && pszLayer[0] != _T('\0'))
            pBlkRef->setLayer(pszLayer);

        AcDbObjectId refId = AcDbObjectId::kNull;
        es = pModelSpace->appendAcDbEntity(refId, pBlkRef);
        if (es != Acad::eOk)
        {
            CUtils::acutPrintf(_T("\n[DWG导入] 错误：添加块参照失败(错误码 %d)。"), (int)es);
            delete pBlkRef;          // 未入库，delete（不能 close）
            return blkDefId;
        }
        pBlkRef->close();
        CUtils::acutPrintf(_T("\n[DWG导入] 成功：块 \"%s\" 已插入到 (%.3f, %.3f, %.3f)。"),
            szBlockName, ptInsert.x, ptInsert.y, ptInsert.z);
    }

    return blkDefId;
}
//-----------------------------------------------------------------------------
// 确保 blockName 块定义存在：已存在直接返回；不存在则从外部 DWG 导入并命名为
// blockName（不插入块参照）。返回块定义 ObjectId，调用方可用 setBlockTableRecord
// 在任意位置创建块参照。兼容 ObjectARX 2007 (acdb17)。
//-----------------------------------------------------------------------------
AcDbObjectId CCreateBaseView::ImportDwgAsBlock(const ACHAR* pszDwgPath, const ACHAR* blockName)
{
    if (pszDwgPath == NULL || pszDwgPath[0] == _T('\0'))
    {
        CUtils::acutPrintf(_T("[DWG导入] 错误：文件路径为空。\n"));
        return AcDbObjectId::kNull;
    }

    // 1. 规范化块名：非法字符替换为 '_'；为空则退回用文件名生成
    ACHAR szBlockName[256];
    if (blockName != NULL && blockName[0] != _T('\0'))
    {
        CUtils::SafeCopy(szBlockName, 256, blockName, (int)_tcslen(blockName));
    }
    else
    {
        MakeBlockName(pszDwgPath, szBlockName, 256);
    }

    if (szBlockName[0] == _T('\0'))
    {
        CUtils::acutPrintf(_T("[DWG导入] 错误：块名为空。\n"));
        return AcDbObjectId::kNull;
    }

    // 2. 当前工作数据库
    AcDbDatabase* pCurDb = this->pDb;

    // 3. 块已存在：直接返回已有块定义，不重复导入
    AcDbBlockTable* pBlkTbl = NULL;
    if (pCurDb->getBlockTable(pBlkTbl, AcDb::kForRead) != Acad::eOk)
    {
        CUtils::acutPrintf(_T("[DWG导入] 错误：无法打开块表。\n"));
        return AcDbObjectId::kNull;
    }

    AcDbObjectId blkDefId = AcDbObjectId::kNull;
    if (pBlkTbl->getAt(szBlockName, blkDefId) == Acad::eOk && !blkDefId.isNull())
    {
        pBlkTbl->close();
        return blkDefId;                      // 已存在：调用方可直接引用
    }
    pBlkTbl->close();

    // 4. 不存在：读取外部 DWG 到内存库（side database，带缓存）
    AcDbDatabase* pExtDb = GetExtDb(pszDwgPath);
    if (pExtDb == NULL)
        return AcDbObjectId::kNull;

    // 5. 以 blockName 导入块定义（acdb17 的 insert 重载）
    Acad::ErrorStatus es = pCurDb->insert(blkDefId, szBlockName, pExtDb, true);
    if (es != Acad::eOk || blkDefId.isNull())
    {
        CUtils::acutPrintf(_T("[DWG导入] 错误：导入块定义失败(错误码 %d)，块名：%s \n"),
            (int)es, szBlockName);
        return AcDbObjectId::kNull;
    }

    // CUtils::acutPrintf(_T("\n[DWG导入] 成功：块 \"%s\" 已导入，可在其他位置创建块参照。"),
    //     szBlockName);
    return blkDefId;
}



//-----------------------------------------------------------------------------
void CCreateBaseView::MakeBlockName(const ACHAR* pszDwgPath, ACHAR* pszName, int nNameCount)
{
    // 取最后一个 \ 或 / 之后的文件名部分
    const ACHAR* pszFile = pszDwgPath;
    for (const ACHAR* p = pszDwgPath; *p != _T('\0'); ++p)
    {
        if (*p == _T('\\') || *p == _T('/'))
            pszFile = p + 1;
    }

    // 去掉扩展名，替换非法字符；预留 8 字符给可能的 "_NNN" 后缀
    int nMax = nNameCount - 8;
    int n = 0;
    for (const ACHAR* p = pszFile; *p != _T('\0') && *p != _T('.') && n < nMax; ++p)
    {
		pszName[n] = CUtils::IsIllegalBlockNameChar(*p) ? _T('_') : *p;
        ++n;
    }
    pszName[n] = _T('\0');

    if (n == 0)
        CUtils::SafeCopy(pszName, nNameCount, _T("DWG_BLOCK"), 9);
}
bool CCreateBaseView::prepare()
{
	strFilePath = CUtils::GetArxFolder();
	strFilePath.Replace(_T("\\"), _T("/"));
	CString strUrl;
	strUrl = strFilePath + _T("project/钻探编录表.json");

	// 解析 JSON（失败时内部已打印错误，这里保留原有业务提示）
    s_pJsonData = CUtils::LoadJsonFile(strUrl.GetString());   // 静默模式
    if (s_pJsonData == NULL) {
        CUtils::acutPrintf(_T("[CAD ERROR] 没有导入数据，请先完成数据导入! %s\n"), strUrl.GetString());
        return false;
    }


	return true;

}