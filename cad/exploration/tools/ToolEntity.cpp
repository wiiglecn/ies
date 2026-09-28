#include "stdafx.h"
#include "ToolEntity.h"
#include "cJSON.h"
#include "arxHeaders.h"
#include "utils.h"


ToolEntity::ToolEntity(void):ToolDrawBase()
{
}

ToolEntity::~ToolEntity(void)
{
}
ToolEntity& ToolEntity::getInstance()
{
    static ToolEntity instance;
    return instance;
}

void ToolEntity::delete_entities(AiToolCommandData* data) {
    if (actionBefore() != 0) {
        return;
    }

    // Assuming data->root is a cJSON object representing the request payload
    // The payload is expected to have an array of strings named "handles"
    cJSON* handlesArray = cJSON_GetObjectItem(data->jsonRoot, "handles");
    if (!handlesArray || !cJSON_IsArray(handlesArray)) {
        // Handle error: invalid input
        actionEnd();
        return;
    }

    
    cJSON* handleItem = NULL;
    cJSON_ArrayForEach(handleItem, handlesArray) {
        if (!cJSON_IsString(handleItem) || !handleItem->valuestring) {
            continue;
        }

        // 将 cJSON 的 char* 转换为 AutoCAD 需要的 TCHAR* (ACHAR*)
        TCHAR handleStr[32] = { 0 };
        if (!CUtils::utf8ToTChar(handleItem->valuestring, handleStr, ARRAYSIZE(handleStr))) {
            continue;
        }

        AcDbHandle dbHandle(handleStr);
        // Convert hexadecimal handle string to AcDbHandle
        /*if (dbHandle.setFromAsciiHandle(handleItem->valuestring) != eOk) {
            continue;
        }*/

        AcDbObjectId objId;
        // 使用 AcDbDatabase 的方法将句柄转换为 ObjectId
        if (pDb->getAcDbObjectId(objId, Adesk::kFalse, dbHandle) != Acad::eOk) {
            continue;
        }

        AcDbObject* pObj = NULL;
        // Open the object for write to erase it
        if (acdbOpenAcDbObject(pObj, objId, AcDb::kForWrite) ==  Acad::eOk) {
            pObj->erase();
            pObj->close();
        }
    }

    data->ret = 0;
    actionEnd();
}
void ToolEntity::list_entities(AiToolCommandData* data)
{
	if (actionBefore() != 0){
        return;
    }

    AcDbBlockTableRecord* pTargetBTR = pModelSpace;
    bool bCloseBTR = false;

    // 检查是否传递了 AcDbBlockReference 的 handle
    cJSON* handleItem = cJSON_GetObjectItem(data->jsonRoot, "handle");
    if (handleItem && cJSON_IsString(handleItem)) {
        AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
        if (!objId.isNull()) {
            AcDbEntity* pEnt = NULL;
            if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForRead) == Acad::eOk) {
                if (pEnt->isKindOf(AcDbBlockReference::desc())) {
                    AcDbBlockReference* pBlkRef = AcDbBlockReference::cast(pEnt);
                    AcDbObjectId btrId = pBlkRef->blockTableRecord();
                    pEnt->close();
                    
                    // 打开块定义以便遍历其内部的实体
                    if (acdbOpenAcDbObject((AcDbObject*&)pTargetBTR, btrId, AcDb::kForRead) == Acad::eOk) {
                        bCloseBTR = true;
                    }
                } else {
                    pEnt->close();
                }
            }
        }
    }

    AcDbBlockTableRecordIterator *pIter;
    if (pTargetBTR && pTargetBTR->newIterator(pIter) == Acad::eOk)
    {
        cJSON* pJsonArray = cJSON_CreateArray();
        
        for (; !pIter->done(); pIter->step())
        {
            AcDbEntity* pEnt = NULL;
            if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk)
            {
                // 1. 获取 Handle
                AcDbHandle handle = pEnt->objectId().handle();
                TCHAR handleStr[20] = {0};
                handle.getIntoAsciiBuffer(handleStr);

                char utf8Handle[32] = { 0 };
                CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));

                // 2. 获取实体类型
                const ACHAR* pName = pEnt->isA()->name();
                char utf8Name[128] = { 0 };
                CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));

                // 3. 获取实体颜色索引
                Adesk::UInt16 colorIndex = pEnt->colorIndex();

                // 4. 获取实体所在图层
                AcDbObjectId layerId = pEnt->layerId();
                TCHAR layerName[256] = { 0 };
                AcDbLayerTableRecord* pLayerRec = NULL;
                if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForRead) == Acad::eOk)
                {
                    const ACHAR* pLayerName = NULL;
                    if (pLayerRec->getName(pLayerName) == Acad::eOk && pLayerName != NULL)
                    {
                        _tcsncpy(layerName, pLayerName, 255);
                    }
                    pLayerRec->close();
                }

                char utf8Layer[256] = { 0 };
                CUtils::TCharToUtf8(layerName, utf8Layer, sizeof(utf8Layer));

                // 5. 构建 JSON 对象
                cJSON* pJsonItem = cJSON_CreateObject();
                cJSON_AddStringToObject(pJsonItem, "handle", utf8Handle);
                cJSON_AddStringToObject(pJsonItem, "type", utf8Name);
                cJSON_AddNumberToObject(pJsonItem, "color", colorIndex);
                cJSON_AddStringToObject(pJsonItem, "layer", utf8Layer);
                
                cJSON_AddItemToArray(pJsonArray, pJsonItem);
                
                pEnt->close();
            }
        }
        delete pIter;
        
        data->resultJson = pJsonArray;
    }

    // 如果是我们自己打开的 BlockTableRecord，需要关闭它
    if (bCloseBTR) {
        pTargetBTR->close();
    }

    data->ret = 0;
    actionEnd();
}

void ToolEntity::entity_get(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
    if (objId.isNull()) { actionEnd(); return; }

    AcDbEntity* pEnt = NULL;
    if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForRead) != Acad::eOk) {
        actionEnd();
        return;
    }

    cJSON* pJson = cJSON_CreateObject();

    // 1. Common properties
    AcDbHandle handle = pEnt->objectId().handle();
    TCHAR handleStr[20] = {0};
    handle.getIntoAsciiBuffer(handleStr);
    char utf8Handle[32] = {0};
    CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));
    cJSON_AddStringToObject(pJson, "handle", utf8Handle);

    const ACHAR* pName = pEnt->isA()->name();
    char utf8Name[128] = {0};
    CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));
    cJSON_AddStringToObject(pJson, "type", utf8Name);

    cJSON_AddNumberToObject(pJson, "color", pEnt->colorIndex());
	//cJSON_AddNumberToObject(pJson, "line_weight", pEnt->lineWeight());

    // Layer
    AcDbObjectId layerId = pEnt->layerId();
    TCHAR layerName[256] = {0};
    AcDbLayerTableRecord* pLayerRec = NULL;
    if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForRead) == Acad::eOk) {
        const ACHAR* pLN = NULL;
        if (pLayerRec->getName(pLN) == Acad::eOk && pLN) {
            _tcsncpy(layerName, pLN, 255);
        }
        pLayerRec->close();
    }
    char utf8Layer[256] = {0};
    CUtils::TCharToUtf8(layerName, utf8Layer, sizeof(utf8Layer));
    cJSON_AddStringToObject(pJson, "layer", utf8Layer);

    // 2. Type-specific properties
    if (pEnt->isKindOf(AcDbLine::desc())) {
        AcDbLine* pLine = AcDbLine::cast(pEnt);
        AcGePoint3d sp = pLine->startPoint();
        AcGePoint3d ep = pLine->endPoint();
        cJSON* spArr = cJSON_CreateArray();
        cJSON_AddItemToArray(spArr, cJSON_CreateNumber(sp.x));
        cJSON_AddItemToArray(spArr, cJSON_CreateNumber(sp.y));
        cJSON_AddItemToObject(pJson, "start_point", spArr);
        cJSON* epArr = cJSON_CreateArray();
        cJSON_AddItemToArray(epArr, cJSON_CreateNumber(ep.x));
        cJSON_AddItemToArray(epArr, cJSON_CreateNumber(ep.y));
        cJSON_AddItemToObject(pJson, "end_point", epArr);
		
    }
    else if (pEnt->isKindOf(AcDbPolyline::desc())) {
        AcDbPolyline* pPline = AcDbPolyline::cast(pEnt);
        cJSON* ptsArr = cJSON_CreateArray();
        int numVerts = pPline->numVerts();
        for (int i = 0; i < numVerts; i++) {
            AcGePoint2d pt;
            if (pPline->getPointAt(i, pt) == Acad::eOk) {
                cJSON* ptObj = cJSON_CreateObject();
                cJSON_AddNumberToObject(ptObj, "x", pt.x);
                cJSON_AddNumberToObject(ptObj, "y", pt.y);
                cJSON_AddItemToArray(ptsArr, ptObj);
            }
        }
        cJSON_AddItemToObject(pJson, "points", ptsArr);
        cJSON_AddBoolToObject(pJson, "closed", pPline->isClosed() ? 1 : 0);
		double line_width;
		pPline->getConstantWidth(line_width);
		cJSON_AddNumberToObject(pJson, "line_width", line_width);

    }
    else if (pEnt->isKindOf(AcDbCircle::desc())) {
        AcDbCircle* pCircle = AcDbCircle::cast(pEnt);
        AcGePoint3d cen = pCircle->center();
        cJSON* cenArr = cJSON_CreateArray();
        cJSON_AddItemToArray(cenArr, cJSON_CreateNumber(cen.x));
        cJSON_AddItemToArray(cenArr, cJSON_CreateNumber(cen.y));
        cJSON_AddItemToObject(pJson, "center", cenArr);
        cJSON_AddNumberToObject(pJson, "radius", pCircle->radius());
    }
    else if (pEnt->isKindOf(AcDbArc::desc())) {
        AcDbArc* pArc = AcDbArc::cast(pEnt);
        AcGePoint3d cen = pArc->center();
        cJSON* cenArr = cJSON_CreateArray();
        cJSON_AddItemToArray(cenArr, cJSON_CreateNumber(cen.x));
        cJSON_AddItemToArray(cenArr, cJSON_CreateNumber(cen.y));
        cJSON_AddItemToObject(pJson, "center", cenArr);
        cJSON_AddNumberToObject(pJson, "radius", pArc->radius());
        cJSON_AddNumberToObject(pJson, "start_angle", pArc->startAngle());
        cJSON_AddNumberToObject(pJson, "end_angle", pArc->endAngle());
    }
    else if (pEnt->isKindOf(AcDbEllipse::desc())) {
        AcDbEllipse* pEllipse = AcDbEllipse::cast(pEnt);
        AcGePoint3d cen = pEllipse->center();
        AcGeVector3d major = pEllipse->majorAxis();
        cJSON* cenArr = cJSON_CreateArray();
        cJSON_AddItemToArray(cenArr, cJSON_CreateNumber(cen.x));
        cJSON_AddItemToArray(cenArr, cJSON_CreateNumber(cen.y));
        cJSON_AddItemToObject(pJson, "center", cenArr);
        cJSON* majArr = cJSON_CreateArray();
        cJSON_AddItemToArray(majArr, cJSON_CreateNumber(major.x));
        cJSON_AddItemToArray(majArr, cJSON_CreateNumber(major.y));
        cJSON_AddItemToObject(pJson, "major_axis", majArr);
        cJSON_AddNumberToObject(pJson, "ratio", pEllipse->radiusRatio());
    }
    else if (pEnt->isKindOf(AcDbSpline::desc())) {
        AcDbSpline* pSpline = AcDbSpline::cast(pEnt);
        cJSON* ptsArr = cJSON_CreateArray();
        int numCP = pSpline->numControlPoints();
        for (int i = 0; i < numCP; i++) {
            AcGePoint3d cp;
            if (pSpline->getControlPointAt(i, cp) == Acad::eOk) {
                cJSON* ptObj = cJSON_CreateObject();
                cJSON_AddNumberToObject(ptObj, "x", cp.x);
                cJSON_AddNumberToObject(ptObj, "y", cp.y);
                cJSON_AddItemToArray(ptsArr, ptObj);
            }
        }
        cJSON_AddItemToObject(pJson, "control_points", ptsArr);
        cJSON_AddNumberToObject(pJson, "degree", pSpline->degree());
    }
    else if (pEnt->isKindOf(AcDbText::desc())) {
        AcDbText* pText = AcDbText::cast(pEnt);
        AcGePoint3d pos = pText->position();
        cJSON* posArr = cJSON_CreateArray();
        cJSON_AddItemToArray(posArr, cJSON_CreateNumber(pos.x));
        cJSON_AddItemToArray(posArr, cJSON_CreateNumber(pos.y));
        cJSON_AddItemToObject(pJson, "position", posArr);
        const ACHAR* pTxt = pText->textString();
        char utf8Txt[512] = {0};
        CUtils::TCharToUtf8(pTxt, utf8Txt, sizeof(utf8Txt));
        cJSON_AddStringToObject(pJson, "text", utf8Txt);
        cJSON_AddNumberToObject(pJson, "height", pText->height());
        cJSON_AddNumberToObject(pJson, "rotation", pText->rotation());
    }
    else if (pEnt->isKindOf(AcDbMText::desc())) {
        AcDbMText* pMText = AcDbMText::cast(pEnt);
        AcGePoint3d pos = pMText->location();
        cJSON* posArr = cJSON_CreateArray();
        cJSON_AddItemToArray(posArr, cJSON_CreateNumber(pos.x));
        cJSON_AddItemToArray(posArr, cJSON_CreateNumber(pos.y));
        cJSON_AddItemToObject(pJson, "position", posArr);
        const ACHAR* pTxt = pMText->contents();
        char utf8Txt[2048] = {0};
        CUtils::TCharToUtf8(pTxt, utf8Txt, sizeof(utf8Txt));
        cJSON_AddStringToObject(pJson, "text", utf8Txt);
        cJSON_AddNumberToObject(pJson, "height", pMText->textHeight());
        cJSON_AddNumberToObject(pJson, "width", pMText->width());
    }
    else if (pEnt->isKindOf(AcDbBlockReference::desc())) {
        AcDbBlockReference* pBlkRef = AcDbBlockReference::cast(pEnt);
        AcDbObjectId blkDefId = pBlkRef->blockTableRecord();
        AcDbBlockTableRecord* pBTR = NULL;
        if (acdbOpenAcDbObject((AcDbObject*&)pBTR, blkDefId, AcDb::kForRead) == Acad::eOk) {
            const ACHAR* pBlkName = NULL;
            if (pBTR->getName(pBlkName) == Acad::eOk && pBlkName) {
                char utf8Blk[256] = {0};
                CUtils::TCharToUtf8(pBlkName, utf8Blk, sizeof(utf8Blk));
                cJSON_AddStringToObject(pJson, "block_name", utf8Blk);
            }
            pBTR->close();
        }
        AcGePoint3d pos = pBlkRef->position();
        cJSON* posArr = cJSON_CreateArray();
        cJSON_AddItemToArray(posArr, cJSON_CreateNumber(pos.x));
        cJSON_AddItemToArray(posArr, cJSON_CreateNumber(pos.y));
        cJSON_AddItemToObject(pJson, "position", posArr);
        cJSON_AddNumberToObject(pJson, "scale_x", pBlkRef->scaleFactors().sx);
        cJSON_AddNumberToObject(pJson, "scale_y", pBlkRef->scaleFactors().sy);
        cJSON_AddNumberToObject(pJson, "rotation", pBlkRef->rotation());
    }

    pEnt->close();

    data->resultJson = pJson;
    data->ret = 0;
    actionEnd();
}

void ToolEntity::entity_set_color(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    // 获取颜色参数（必填）
    cJSON* colorItem = cJSON_GetObjectItem(data->jsonRoot, "color");
    if (!colorItem) { actionEnd(); return; }
    Adesk::UInt16 colorIndex = (Adesk::UInt16)colorItem->valueint;

    cJSON* pResultArray = cJSON_CreateArray();

    // 收集需要处理的 handle 列表
    std::vector<std::pair<AcDbObjectId, std::string> > targets;

    // 1. 优先检查 "handles" 数组
    cJSON* handlesArray = cJSON_GetObjectItem(data->jsonRoot, "handles");
    if (handlesArray && cJSON_IsArray(handlesArray)) {
        cJSON* handleItem = NULL;
        cJSON_ArrayForEach(handleItem, handlesArray) {
            if (!cJSON_IsString(handleItem) || !handleItem->valuestring) {
                continue;
            }
            TCHAR handleStr[32] = { 0 };
            if (!CUtils::utf8ToTChar(handleItem->valuestring, handleStr, ARRAYSIZE(handleStr))) {
                continue;
            }
            AcDbHandle dbHandle(handleStr);
            AcDbObjectId objId;
            if (pDb->getAcDbObjectId(objId, Adesk::kFalse, dbHandle) != Acad::eOk) {
                continue;
            }
            if (objId.isNull()) {
                continue;
            }
            targets.push_back(std::make_pair(objId, handleItem->valuestring));
        }
    }
    // 2. 兼容单个 "handle" 参数
    else {
        AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
        if (!objId.isNull()) {
            cJSON* singleHandle = cJSON_GetObjectItem(data->jsonRoot, "handle");
            if (singleHandle && cJSON_IsString(singleHandle)) {
                targets.push_back(std::make_pair(objId, singleHandle->valuestring));
            }
        }
    }

    // 遍历所有目标实体并设置颜色
    for (std::vector<std::pair<AcDbObjectId, std::string> >::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        cJSON* pResultItem = cJSON_CreateObject();
        cJSON_AddStringToObject(pResultItem, "handle", it->second.c_str());

        AcDbEntity* pEnt = NULL;
        if (acdbOpenAcDbEntity(pEnt, it->first, AcDb::kForWrite) == Acad::eOk) {
            pEnt->setColorIndex(colorIndex);
            pEnt->close();
            cJSON_AddBoolToObject(pResultItem, "success", 1);
        } else {
            cJSON_AddBoolToObject(pResultItem, "success", 0);
        }
        cJSON_AddItemToArray(pResultArray, pResultItem);
    }

    data->resultJson = pResultArray;
    data->ret = 0;
    actionEnd();
}

void ToolEntity::entity_copy(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
    if (objId.isNull()) { actionEnd(); return; }

    double dx = cJSON_GetObjectItem(data->jsonRoot, "dx")->valuedouble;
    double dy = cJSON_GetObjectItem(data->jsonRoot, "dy")->valuedouble;

    AcDbEntity* pEnt;
    if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForRead) == Acad::eOk) {
        AcDbEntity* pClone = AcDbEntity::cast(pEnt->clone());
        pClone->transformBy(AcGeMatrix3d::translation(AcGeVector3d(dx, dy, 0)));
        pModelSpace->appendAcDbEntity(pClone);
        pClone->close();
        pEnt->close();
        data->ret = 0;
    }
    actionEnd();
}

void ToolEntity::entity_move(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
    if (objId.isNull()) { actionEnd(); return; }

    double dx = cJSON_GetObjectItem(data->jsonRoot, "dx")->valuedouble;
    double dy = cJSON_GetObjectItem(data->jsonRoot, "dy")->valuedouble;

    AcDbEntity* pEnt;
    if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForWrite) == Acad::eOk) {
        pEnt->transformBy(AcGeMatrix3d::translation(AcGeVector3d(dx, dy, 0)));
        pEnt->close();
        data->ret = 0;
    }
    actionEnd();
}

void ToolEntity::entity_rotate(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
    if (objId.isNull()) { actionEnd(); return; }

    double x = cJSON_GetObjectItem(data->jsonRoot, "x")->valuedouble;
    double y = cJSON_GetObjectItem(data->jsonRoot, "y")->valuedouble;
    double angle = cJSON_GetObjectItem(data->jsonRoot, "angle")->valuedouble;

    AcDbEntity* pEnt;
    if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForWrite) == Acad::eOk) {
        pEnt->transformBy(AcGeMatrix3d::rotation(angle, AcGeVector3d::kZAxis, AcGePoint3d(x, y, 0)));
        pEnt->close();
        data->ret = 0;
    }
    actionEnd();
}

void ToolEntity::entity_scale(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
    if (objId.isNull()) { actionEnd(); return; }

    double x = cJSON_GetObjectItem(data->jsonRoot, "x")->valuedouble;
    double y = cJSON_GetObjectItem(data->jsonRoot, "y")->valuedouble;
    double scale = cJSON_GetObjectItem(data->jsonRoot, "scale")->valuedouble;

    AcDbEntity* pEnt;
    if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForWrite) == Acad::eOk) {
        pEnt->transformBy(AcGeMatrix3d::scaling(scale, AcGePoint3d(x, y, 0)));
        pEnt->close();
        data->ret = 0;
    }
    actionEnd();
}

void ToolEntity::entity_mirror(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
    if (objId.isNull()) { actionEnd(); return; }

    double x1 = cJSON_GetObjectItem(data->jsonRoot, "x1")->valuedouble;
    double y1 = cJSON_GetObjectItem(data->jsonRoot, "y1")->valuedouble;
    double x2 = cJSON_GetObjectItem(data->jsonRoot, "x2")->valuedouble;
    double y2 = cJSON_GetObjectItem(data->jsonRoot, "y2")->valuedouble;
    
    int delOrig = 0;
    cJSON* delItem = cJSON_GetObjectItem(data->jsonRoot, "delete_original");
    if (delItem) delOrig = delItem->valueint;

    AcGeLine3d line(AcGePoint3d(x1, y1, 0), AcGePoint3d(x2, y2, 0));
    AcGeMatrix3d mat = AcGeMatrix3d::mirroring(line);

    AcDbEntity* pEnt;
    if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForRead) == Acad::eOk) {
        AcDbEntity* pClone = AcDbEntity::cast(pEnt->clone());
        pClone->transformBy(mat);
        pModelSpace->appendAcDbEntity(pClone);
        pClone->close();
        pEnt->close();

        if (delOrig) {
            AcDbEntity* pEntDel;
            if (acdbOpenAcDbEntity(pEntDel, objId, AcDb::kForWrite) == Acad::eOk) {
                pEntDel->erase();
                pEntDel->close();
            }
        }
        data->ret = 0;
    }
    actionEnd();
}

void ToolEntity::entity_offset(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
    if (objId.isNull()) { actionEnd(); return; }

    double dist = cJSON_GetObjectItem(data->jsonRoot, "distance")->valuedouble;

    AcDbCurve* pCurve;
    if (acdbOpenAcDbEntity((AcDbEntity*&)pCurve, objId, AcDb::kForRead) == Acad::eOk) {
        AcDbVoidPtrArray curves;
        Acad::ErrorStatus es = pCurve->getOffsetCurves(dist, curves);
        if (es == Acad::eOk) {
            for (int i = 0; i < curves.length(); ++i) {
                AcDbEntity* pNewEnt = AcDbEntity::cast((AcRxObject*)curves[i]);
                if (pNewEnt) {
                    pModelSpace->appendAcDbEntity(pNewEnt);
                    pNewEnt->close();
                }
            }
            data->ret = 0;
        }
        pCurve->close();
    }
    actionEnd();
}

void ToolEntity::entity_fillet(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    
    cJSON* h1 = cJSON_GetObjectItem(data->jsonRoot, "handle1");
    cJSON* h2 = cJSON_GetObjectItem(data->jsonRoot, "handle2");
    cJSON* rItem = cJSON_GetObjectItem(data->jsonRoot, "radius");
    if (!h1 || !h2 || !rItem) { actionEnd(); return; }

    TCHAR h1Str[32] = {0}, h2Str[32] = {0};
    CUtils::utf8ToTChar(h1->valuestring, h1Str, 32);
    CUtils::utf8ToTChar(h2->valuestring, h2Str, 32);
    double radius = rItem->valuedouble;

    CString cmd;
    // Utilize acedCommand via AutoLISP (handent) to robustly perform fillet
    cmd.Format(_T("(command \"_.FILLET\" \"_R\" %f \"\" \"(handent \\\"%s\\\")\" \"(handent \\\"%s\\\")\" \"\") "), radius, h1Str, h2Str);
    acedCommand(RTSTR, cmd, RTNONE);

    data->ret = 0;
    actionEnd();
}

void ToolEntity::entity_chamfer(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    
    cJSON* h1 = cJSON_GetObjectItem(data->jsonRoot, "handle1");
    cJSON* h2 = cJSON_GetObjectItem(data->jsonRoot, "handle2");
    cJSON* d1Item = cJSON_GetObjectItem(data->jsonRoot, "dist1");
    cJSON* d2Item = cJSON_GetObjectItem(data->jsonRoot, "dist2");
    if (!h1 || !h2 || !d1Item || !d2Item) { actionEnd(); return; }

    TCHAR h1Str[32] = {0}, h2Str[32] = {0};
    CUtils::utf8ToTChar(h1->valuestring, h1Str, 32);
    CUtils::utf8ToTChar(h2->valuestring, h2Str, 32);
    double d1 = d1Item->valuedouble;
    double d2 = d2Item->valuedouble;

    CString cmd;
    // Utilize acedCommand via AutoLISP (handent) to robustly perform chamfer
    cmd.Format(_T("(command \"_.CHAMFER\" \"_D\" %f %f \"\" \"(handent \\\"%s\\\")\" \"(handent \\\"%s\\\")\" \"\") "), d1, d2, h1Str, h2Str);
    acedCommand(RTSTR, cmd, RTNONE);

    data->ret = 0;
    actionEnd();
}

void ToolEntity::entity_array(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
    if (objId.isNull()) { actionEnd(); return; }

    int rows = cJSON_GetObjectItem(data->jsonRoot, "num_rows")->valueint;
    int cols = cJSON_GetObjectItem(data->jsonRoot, "num_cols")->valueint;
    double rowSp = cJSON_GetObjectItem(data->jsonRoot, "row_spacing")->valuedouble;
    double colSp = cJSON_GetObjectItem(data->jsonRoot, "col_spacing")->valuedouble;

    if (rows <= 0) rows = 1;
    if (cols <= 0) cols = 1;

    AcDbEntity* pEnt;
    if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForRead) == Acad::eOk) {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (r == 0 && c == 0) continue;
                AcDbEntity* pClone = AcDbEntity::cast(pEnt->clone());
                AcGeMatrix3d mat = AcGeMatrix3d::translation(AcGeVector3d(c * colSp, r * rowSp, 0));
                pClone->transformBy(mat);
                pModelSpace->appendAcDbEntity(pClone);
                pClone->close();
            }
        }
        pEnt->close();
        data->ret = 0;
    }
    actionEnd();
}
void ToolEntity::get_selected_entity(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    ads_name ssname;
    cJSON* pJsonArray = cJSON_CreateArray();
    
    // 使用 acedSSGet 获取当前被选中的实体（隐式选择集 _I）
    // 这样可以完美避开 acedSSGetFirst 的 resbuf** 参数声明冲突
    if (acedSSGet(_T("_I"), NULL, NULL, NULL, ssname) == RTNORM) {
        long len = 0;
        if (acedSSLength(ssname, &len) == RTNORM) {
            for (long i = 0; i < len; i++) {
                ads_name ent;
                if (acedSSName(ssname, i, ent) == RTNORM) {
                    AcDbObjectId objId;
                    if (acdbGetObjectId(objId, ent) == Acad::eOk) {
                        AcDbEntity* pEnt = NULL;
                        if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForRead) == Acad::eOk) {
                            AcDbHandle handle = pEnt->objectId().handle();
                            TCHAR handleStr[20] = {0};
                            handle.getIntoAsciiBuffer(handleStr);

                            char utf8Handle[32] = { 0 };
                            CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));

                            const ACHAR* pName = pEnt->isA()->name();
                            char utf8Name[128] = { 0 };
                            CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));

                            Adesk::UInt16 colorIndex = pEnt->colorIndex();

                            AcDbObjectId layerId = pEnt->layerId();
                            TCHAR layerName[256] = { 0 };
                            AcDbLayerTableRecord* pLayerRec = NULL;
                            if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForRead) == Acad::eOk) {
                                const ACHAR* pLayerName = NULL;
                                if (pLayerRec->getName(pLayerName) == Acad::eOk && pLayerName != NULL) {
                                    _tcsncpy(layerName, pLayerName, 255);
                                }
                                pLayerRec->close();
                            }

                            char utf8Layer[256] = { 0 };
                            CUtils::TCharToUtf8(layerName, utf8Layer, sizeof(utf8Layer));

                            cJSON* pJsonItem = cJSON_CreateObject();
                            cJSON_AddStringToObject(pJsonItem, "handle", utf8Handle);
                            cJSON_AddStringToObject(pJsonItem, "type", utf8Name);
                            cJSON_AddNumberToObject(pJsonItem, "color", colorIndex);
                            cJSON_AddStringToObject(pJsonItem, "layer", utf8Layer);
                            
                            cJSON_AddItemToArray(pJsonArray, pJsonItem);
                            
                            pEnt->close();
                        }
                    }
                }
            }
        }
        acedSSFree(ssname);
    }

    data->resultJson = pJsonArray;
    data->ret = 0;
    actionEnd();
}

void ToolEntity::set_selected_entity(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    cJSON* handlesArray = cJSON_GetObjectItem(data->jsonRoot, "handles");
    if (handlesArray && cJSON_IsArray(handlesArray)) {
        ads_name ssname;
        bool bFirst = true;
        
        cJSON* handleItem = NULL;
        cJSON_ArrayForEach(handleItem, handlesArray) {
            if (!cJSON_IsString(handleItem) || !handleItem->valuestring) {
                continue;
            }

            TCHAR handleStr[32] = { 0 };
            if (!CUtils::utf8ToTChar(handleItem->valuestring, handleStr, ARRAYSIZE(handleStr))) {
                continue;
            }

            AcDbHandle dbHandle(handleStr);
            AcDbObjectId objId;
            if (pDb->getAcDbObjectId(objId, Adesk::kFalse, dbHandle) != Acad::eOk) {
                continue;
            }

            if (objId.isNull()) {
                continue;
            }

            ads_name ent;
            if (acdbGetAdsName(ent, objId) == Acad::eOk) {
                if (bFirst) {
                    acedSSAdd(ent, NULL, ssname);
                    bFirst = false;
                } else {
                    acedSSAdd(ent, ssname, ssname);
                }
            }
        }

        if (!bFirst) {
            acedSSSetFirst(ssname, NULL); // Highlights and selects the entities with grips
            acedSSFree(ssname);
            data->ret = 0;
        }
    } else {
        // 兼容只传单个 "handle" 的情况
        AcDbObjectId objId = GetIdFromHandle(data->jsonRoot, "handle", pDb);
        if (!objId.isNull()) {
            ads_name ent;
            if (acdbGetAdsName(ent, objId) == Acad::eOk) {
                ads_name ssname;
                acedSSAdd(ent, NULL, ssname);
                acedSSSetFirst(ssname, NULL);
                acedSSFree(ssname);
                data->ret = 0;
            }
        }
    }

    actionEnd();
}