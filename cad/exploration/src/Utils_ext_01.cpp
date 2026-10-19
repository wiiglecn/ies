#include "StdAfx.h"
#include "Utils.h"
#include "arxHeaders.h"
#include <wincrypt.h>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include "cJSON.h"



// 确保文字样式存在：不存在则创建。返回样式 ID（失败返回 kNull）
AcDbObjectId CUtils::EnsureTextStyle(const ACHAR* pszStyleName, const ACHAR* pszFontName,
                                     double dHeight, double dWidthFactor)
{
    if (pszStyleName == NULL || pszStyleName[0] == _T('\0'))
        return AcDbObjectId::kNull;

    // 1. 已存在：直接返回，不改动现有设置
    AcDbObjectId styleId = FindTextStyle(pszStyleName);
    if (!styleId.isNull())
        return styleId;

    // 2. 打开文字样式表（要新建记录，必须写方式打开）
    AcDbTextStyleTable* pTsTbl = NULL;
    if (acdbHostApplicationServices()->workingDatabase()
            ->getTextStyleTable(pTsTbl, AcDb::kForWrite) != Acad::eOk)
        return AcDbObjectId::kNull;

    AcDbTextStyleTableRecord* pTsRec = new AcDbTextStyleTableRecord;
    pTsRec->setName(pszStyleName);

    // 3. 字体：以 .shx 结尾 → 形字体；否则 → Windows 字体（TTF）
    bool bIsShx = false;
    if (pszFontName != NULL && pszFontName[0] != _T('\0'))
    {
        int nLen = (int)_tcslen(pszFontName);
        bIsShx = (nLen > 4 && _tcsicmp(pszFontName + nLen - 4, _T(".shx")) == 0);
    }
    if (bIsShx)
        pTsRec->setFileName(pszFontName);                                  // 如 txt.shx / gbenor.shx
    else if (pszFontName != NULL && pszFontName[0] != _T('\0'))
        pTsRec->setFont(pszFontName, false, false, 0, 0);

    // 4. 字高与宽度比例（字高 <= 0 表示不固定，随实体设置）
    if (dHeight > 0.0)
        pTsRec->setTextSize(dHeight);
    if (dWidthFactor > 0.0)
        pTsRec->setXScale(dWidthFactor);

    // 5. 入表：成功 close（所有权归 CAD），失败 delete
    AcDbObjectId newId = AcDbObjectId::kNull;
    if (pTsTbl->add(pTsRec) == Acad::eOk)
    {
        newId = pTsRec->objectId();
        pTsRec->close();
    }
    else
        delete pTsRec;
    pTsTbl->close();
    return newId;
}
// 确保图层存在：不存在则创建；bSetCurrent 时切换为当前层
AcDbObjectId CUtils::EnsureLayer(const ACHAR* pszLayerName, bool bSetCurrent)
{
    if (pszLayerName == NULL || pszLayerName[0] == _T('\0'))
        return AcDbObjectId::kNull;

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (pDb == NULL) return AcDbObjectId::kNull;

    // 直接以写模式打开图层表（避免先读后写导致 320 锁冲突）
    AcDbLayerTable* pLyrTbl = NULL;
    Acad::ErrorStatus st = pDb->getLayerTable(pLyrTbl, AcDb::kForWrite);
    if (st != Acad::eOk) { CUtils::acutPrintf(_T("[LAYER DEBUG] kForWrite fail: %d, %s\n"), (int)st, pszLayerName); return AcDbObjectId::kNull; }

    AcDbObjectId layerId = AcDbObjectId::kNull;
    if (pLyrTbl->getAt(pszLayerName, layerId) == Acad::eOk && !layerId.isNull())
    {
        pLyrTbl->close();
        if (bSetCurrent && pDb->clayer() != layerId) pDb->setClayer(layerId);
        return layerId;
    }

    AcDbLayerTableRecord* pLyrRec = new AcDbLayerTableRecord;
    pLyrRec->setName(pszLayerName);
    AcDbObjectId newId = AcDbObjectId::kNull;
    if (pLyrTbl->add(pLyrRec) == Acad::eOk) { newId = pLyrRec->objectId(); pLyrRec->close(); }
    else delete pLyrRec;
    pLyrTbl->close();

    if (!newId.isNull() && bSetCurrent) pDb->setClayer(newId);
    return newId;
}

// 在调用方已打开的容器里删除指定图层实体（容器不关，实体用迭代器逐个开/关）
int CUtils::EraseLayerEntities(AcDbBlockTableRecord* pOpenedBtr, const ACHAR* pszLayerName)
{
    if (pOpenedBtr == NULL)
        return -1;

    // 1. 图层名：参数为空则取当前层（CLAYER）
    CString sLayer;
    if (pszLayerName != NULL && pszLayerName[0] != _T('\0'))
    {
        sLayer = pszLayerName;
    }
    else
    {
        AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
        if (pDb == NULL)
            return -1;
        AcDbLayerTableRecord* pLyr = NULL;
        if (acdbOpenObject(pLyr, pDb->clayer(), AcDb::kForRead) != Acad::eOk)
            return -1;
        const ACHAR* pName = NULL;
        if (pLyr->getName(pName) == Acad::eOk && pName != NULL)
            sLayer = pName;
        pLyr->close();
        if (sLayer.IsEmpty())
            return -1;
    }

    // 2. 遍历实体删除（容器指针由调用方管理，这里只遍历）
    int nErased = 0;
    AcDbBlockTableRecordIterator* pIter = NULL;
    if (pOpenedBtr->newIterator(pIter) == Acad::eOk)
    {
        for (pIter->start(); !pIter->done(); pIter->step())
        {
            AcDbEntity* pEnt = NULL;
            if (pIter->getEntity(pEnt, AcDb::kForWrite) != Acad::eOk)
                continue;
            if (CString(pEnt->layer()).CompareNoCase(sLayer) == 0)
            {
                if (pEnt->erase() == Acad::eOk)
                    nErased++;
            }
            pEnt->close();
        }
        delete pIter;
    }
    return nErased;
}
// 在调用方已打开的容器里删除块参照并删块定义（容器不关）
bool CUtils::EraseBlock(AcDbBlockTableRecord* pOpenedBtr, const ACHAR* pszBlockName)
{
    if (pOpenedBtr == NULL || pszBlockName == NULL || pszBlockName[0] == _T('\0'))
        return false;

    AcDbDatabase* pDb = pOpenedBtr->database();   // 从容器反查库，不用调用方再传
    if (pDb == NULL)
        return false;

    // 1. 查块定义；不存在视为成功（块表只读打开，和事务早前的只读打开可叠加）
    AcDbBlockTable* pBT = NULL;
    if (pDb->getBlockTable(pBT, AcDb::kForRead) != Acad::eOk)
        return false;
    AcDbObjectId defId = AcDbObjectId::kNull;
    pBT->getAt(pszBlockName, defId);
    pBT->close();
    if (defId.isNull())
        return true;

    // 2. 在调用方容器里删指向该定义的块参照（容器指针由调用方管理，这里只遍历）
    AcDbBlockTableRecordIterator* pIter = NULL;
    if (pOpenedBtr->newIterator(pIter) == Acad::eOk)
    {
        for (pIter->start(); !pIter->done(); pIter->step())
        {
            AcDbEntity* pEnt = NULL;
            if (pIter->getEntity(pEnt, AcDb::kForWrite) != Acad::eOk)
                continue;
            AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt);
            if (pRef != NULL && pRef->blockId() == defId)
                pRef->erase();
            pEnt->close();
        }
        delete pIter;
    }

    // 3. 删块定义本身（与 ChangeText 里 acdbOpenObject 打开块定义的方式一致，事务中已验证可行）
    AcDbBlockTableRecord* pDef = NULL;
    if (acdbOpenObject(pDef, defId, AcDb::kForWrite) != Acad::eOk)
        return false;
    Acad::ErrorStatus es = pDef->erase();
    pDef->close();
    if (es != Acad::eOk)
    {
        CUtils::acutPrintf(_T("[CAD ERROR] 删除块定义 \"%s\" 失败(错误码 %d)\n"),
                           pszBlockName, (int)es);
        return false;
    }
    return true;
}
bool CUtils::ClearSelection()
{
    return acedSSSetFirst(NULL, NULL) == RTNORM;
}
// 取模型空间全部实体的总包围盒（跳过已删除实体；返回 false 表示图中无有效实体）
bool CUtils::GetModelSpaceBounds(AcDbBlockTableRecord* pBTR, AcDbExtents& extOut)
{
    bool bHasAny = false;
    AcDbBlockTableRecordIterator* pIter = NULL;
    if (pBTR == NULL || pBTR->newIterator(pIter) != Acad::eOk)
        return false;

    for (pIter->start(); !pIter->done(); pIter->step())
    {
        AcDbEntity* pEnt = NULL;
        if (pIter->getEntity(pEnt, AcDb::kForRead) != Acad::eOk)
            continue;

        if (pEnt->isErased()) { pEnt->close(); continue; }   // 刚被 EraseLayerEntities 删掉的旧图例会被跳过

        AcDbExtents ext;
        if (pEnt->getGeomExtents(ext) == Acad::eOk)
        {
            if (!bHasAny) { extOut = ext; bHasAny = true; }
            else
            {
                extOut.addPoint(ext.minPoint());   // addPoint = 扩展包围盒包含该点
                extOut.addPoint(ext.maxPoint());
            }
        }
        pEnt->close();
    }
    delete pIter;        // 迭代器用完必须 delete
    return bHasAny;
}
AcDbObjectId CUtils::AddDbPolyline(AcDbBlockTableRecord* container,
                                   const std::vector<AcGePoint3d>& arrPts,
                                   double dWidth, bool bClosed,
                                   const ACHAR* pszLinetype, Adesk::UInt16 colorIndex)
{
    if (container == NULL || arrPts.size() < 2)
        return AcDbObjectId::kNull;

    AcDbPolyline* pPline = new AcDbPolyline(arrPts.size());
    pPline->setConstantWidth(dWidth);      // ★ 全局线宽（0.35 之类）
    pPline->setClosed(bClosed);

    for (size_t i = 0; i < arrPts.size(); ++i)
    {
        // 顶点是二维点（OCS），Z 由标高决定；平面图直接丢 z 即可
        pPline->addVertexAt((unsigned)i, AcGePoint2d(arrPts[i].x, arrPts[i].y));
    }

    // 可选颜色：256 = ByLayer（默认不改），其余按 ACI 颜色号设置
    if (colorIndex != 256)
        pPline->setColorIndex(colorIndex);

    // 可选线型：按名称设置（setLinetype 内部查线型表），找不到时保持 ByLayer
    if (pszLinetype != NULL && pszLinetype[0] != _T('\0'))
    {
        if (pPline->setLinetype(pszLinetype) != Acad::eOk)
            CUtils::acutPrintf(_T("[CAD WARN] 线型 \"%s\" 不存在，保持 ByLayer\n"), pszLinetype);
    }

    AcDbObjectId plineId = AcDbObjectId::kNull;
    Acad::ErrorStatus es = container->appendAcDbEntity(plineId, pPline);
    if (es == Acad::eOk)
        pPline->close();                   // 成功：所有权归数据库
    else
        delete pPline;                     // 失败：防内存泄漏
    return plineId;
}
// 把已入库实体改到指定层（"0" 层始终存在，无需 EnsureLayer）
bool CUtils::SetEntLayer(const AcDbObjectId& id, const ACHAR* pszLayer)
{
    AcDbEntity* pEnt = CUtils::OpenEntityById(id, AcDb::kForWrite);
    if (pEnt == NULL)
        return false;
    bool bOk = (pEnt->setLayer(pszLayer) == Acad::eOk);
    pEnt->close();
    return bOk;
}
// 以中心点 + 高宽画闭合矩形，直接复用 AddDbPolyline
AcDbObjectId CUtils::AddDbRect(AcDbBlockTableRecord* container,
                               const AcGePoint3d& ptCenter,
                               double dWidth, double dHeight, double dLineWidth)
{
    double dHalfW = dWidth  * 0.5;
    double dHalfH = dHeight * 0.5;

    std::vector<AcGePoint3d> arrPts;
    arrPts.reserve(4);
    arrPts.push_back(AcGePoint3d(ptCenter.x - dHalfW, ptCenter.y - dHalfH, 0));  // 左下
    arrPts.push_back(AcGePoint3d(ptCenter.x + dHalfW, ptCenter.y - dHalfH, 0));  // 右下
    arrPts.push_back(AcGePoint3d(ptCenter.x + dHalfW, ptCenter.y + dHalfH, 0));  // 右上
    arrPts.push_back(AcGePoint3d(ptCenter.x - dHalfW, ptCenter.y + dHalfH, 0));  // 左上

    return AddDbPolyline(container, arrPts, dLineWidth, true);   // true = 闭合 → 矩形
}
AcDbObjectId CUtils::AddLine(AcDbBlockTableRecord* container,AcGePoint3d& ptStart,AcGePoint3d& ptEnd)
{
	//从头到土层描述的横线
	AcDbLine* pLine = new AcDbLine(ptStart, ptEnd);
	AcDbObjectId lineId = AcDbObjectId::kNull;
	Acad::ErrorStatus es = container->appendAcDbEntity(lineId, pLine);
	if (es == Acad::eOk)
		pLine->close();                  // 入库成功 → close（不能 delete）
	else
		delete pLine;                    // 入库失败 → 未入库，只能 delete
	return lineId;
}
/// 
/// 一次性加载lin文件内的全部线型
/// </summary>
Acad::ErrorStatus CUtils::LoadAllLinetypesFromFile(const ACHAR* linFile)
{
    TCHAR fullPath[MAX_PATH] = {0};
    if (ads_findfile(linFile, fullPath) != RTNORM)
    {
        acutPrintf(_T("\n错误：未找到线型文件 %s"), linFile);
        return Acad::eFileNotFound;
    }
    // 第一个参数传"*"即可加载文件内所有定义的线型
    return acdbLoadLineTypeFile(_T("*"), fullPath, NULL);
}
/**
 * @brief 获取当前数据库中所有已加载的线型名称列表 (兼容 VC7/VC8)
 * @param bSort 是否排序
 * @return std::vector<CString> 线型名称列表
 */
std::vector<CString> CUtils::GetAvailableLinetypes(bool bSort)
{
    std::vector<CString> ltList;
    
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb)
        return ltList;

    AcDbLinetypeTable* pLtTable = NULL;
    Acad::ErrorStatus es = pDb->getLinetypeTable(pLtTable, AcDb::kForRead);
    if (es != Acad::eOk || !pLtTable)
        return ltList;

    AcDbLinetypeTableIterator* pIter = NULL;
    es = pLtTable->newIterator(pIter);
    if (es != Acad::eOk || !pIter)
    {
        pLtTable->close();
        return ltList;
    }

    for (; !pIter->done(); pIter->step())
    {
        AcDbLinetypeTableRecord* pLtRecord = NULL;
        es = pIter->getRecord(pLtRecord, AcDb::kForRead);
        if (es == Acad::eOk && pLtRecord)
        {
            ACHAR* pNameRaw = NULL;
            pLtRecord->getName(pNameRaw);
            
            if (pNameRaw)
            {
                // 关键兼容点：在旧版 ARX 中，getName 分配的内存必须手动释放
                // 我们先复制到一个临时 CString，然后释放原始指针
                CString strName(pNameRaw);
                
                // 释放由 getName 分配的内存
				acutDelString(pNameRaw);
                //acutDelString(const_cast<ACHAR*>(pNameRaw));
                
                ltList.push_back(strName);
            }
            pLtRecord->close();
        }
    }

    delete pIter;
    pLtTable->close();

    if (bSort && !ltList.empty())
    {
        // VC7/VC8 的 std::sort 需要严格的弱序比较
        std::sort(ltList.begin(), ltList.end());
    }

    return ltList;
}

 bool CUtils::IsShxLinetype(AcDbLinetypeTableRecord* pRecord)
{
    int dashCount = pRecord->numDashes();

    for (int i = 0; i < dashCount; ++i)
    {
        // 非 0 表示该线型包含 SHX 形状
        if (pRecord->shapeNumberAt(i) != 0)
            return true;
    }

    return false;
}
bool CUtils::CreateRandomPolylineStyles(
    AcDbDatabase* pDb,
    int count,
    std::vector<RandomPolylineStyle>& styles)
{
    styles.clear();

    if (pDb == NULL || count <= 0)
        return false;

    AcDbLinetypeTable* pTable = NULL;
    if (pDb->getLinetypeTable(pTable, AcDb::kForRead) != Acad::eOk)
        return false;

    std::vector<AcDbObjectId> linetypes;

    AcDbLinetypeTableIterator* pIter = NULL;
    if (pTable->newIterator(pIter) != Acad::eOk)
    {
        pTable->close();
        return false;
    }

    for (; !pIter->done(); pIter->step())
    {
        AcDbLinetypeTableRecord* pRecord = NULL;

        if (pIter->getRecord(pRecord, AcDb::kForRead) != Acad::eOk)
            continue;

        if (!IsShxLinetype(pRecord))
            linetypes.push_back(pRecord->objectId());

        pRecord->close();
    }

    delete pIter;
    pTable->close();

    if (linetypes.empty())
        return false;

    // ACI 颜色 1~255；不使用 ByLayer、ByBlock
    const int colorCount = 255;
    long maxTypeCount = (long)linetypes.size() * colorCount;

    if ((long)count > maxTypeCount)
        return false;

    // 构造所有“线型 + 颜色”组合
    std::vector<RandomPolylineStyle> candidates;

    size_t i;
    for (i = 0; i < linetypes.size(); ++i)
    {
        int color;

        for (color = 1; color <= colorCount; ++color)
        {
            // 排除绿(3)/蓝(5)/洋红(6)：保留给地质要素线（水位线、设计标高线等）专用
            if (color == 3 || color == 5 || color == 6)
                continue;

            RandomPolylineStyle style;
            style.linetypeId = linetypes[i];
            style.colorIndex = (Adesk::UInt16)color;
            style.lineWeight = 0.0;
            candidates.push_back(style);
        }
    }

    // 使用当前时间初始化随机种子
    static bool seeded = false;
    if (!seeded)
    {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    // Fisher-Yates 洗牌，兼容旧编译器
    for (int n = (int)candidates.size() - 1; n > 0; --n)
    {
        int j = rand() % (n + 1);

        RandomPolylineStyle temp = candidates[n];
        candidates[n] = candidates[j];
        candidates[j] = temp;
    }

    for (int n = 0; n < count; ++n)
    {
        RandomPolylineStyle style = candidates[n];

        // 线宽：0.00~0.25 连续随机（不参与类型唯一性判断）
        // ★0.25 在前：先乘后除，避免 rand()/RAND_MAX 整除为 0
        style.lineWeight = 0.25 * rand() / RAND_MAX;

        styles.push_back(style);
    }

    return true;
}
