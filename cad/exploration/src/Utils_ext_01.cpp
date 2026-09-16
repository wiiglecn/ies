#include "StdAfx.h"
#include "Utils.h"
#include "arxHeaders.h"
#include <wincrypt.h>
#include <string>
#include <fstream>
#include <vector>
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
    if (pDb == NULL)
        return AcDbObjectId::kNull;

    // 1. 已存在：需要时切换为当前层后直接返回
    AcDbLayerTable* pLyrTbl = NULL;
    if (pDb->getLayerTable(pLyrTbl, AcDb::kForRead) != Acad::eOk)
        return AcDbObjectId::kNull;

    AcDbObjectId layerId = AcDbObjectId::kNull;
    pLyrTbl->getAt(pszLayerName, layerId);
    pLyrTbl->close();

    if (!layerId.isNull())
    {
        if (bSetCurrent && pDb->clayer() != layerId)
            pDb->setClayer(layerId);
        return layerId;
    }

    // 2. 不存在：写方式打开图层表，新建记录（默认颜色/线型，继承 0 层以外的默认值）
    if (pDb->getLayerTable(pLyrTbl, AcDb::kForWrite) != Acad::eOk)
        return AcDbObjectId::kNull;

    AcDbLayerTableRecord* pLyrRec = new AcDbLayerTableRecord;
    pLyrRec->setName(pszLayerName);

    AcDbObjectId newId = AcDbObjectId::kNull;
    if (pLyrTbl->add(pLyrRec) == Acad::eOk)
    {
        newId = pLyrRec->objectId();
        pLyrRec->close();           // 项目约定：入库成功 close，失败 delete
    }
    else
        delete pLyrRec;
    pLyrTbl->close();

    // 3. 切换为当前层（之后新建的实体默认落在该层）
    if (!newId.isNull() && bSetCurrent)
        pDb->setClayer(newId);
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