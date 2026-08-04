#include "stdafx.h"
#include "ToolEntity.h"
#include "cJSON.h"
#include "adscodes.h"
#include "actrans.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "dbelipse.h"
#include "dbspline.h"

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
		data->ret = 1;
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

    actionEnd();
}
void ToolEntity::list_entities(AiToolCommandData* data)
{
	if (actionBefore() != 0){
		data->ret = 1;
        return;
	}



    AcDbBlockTableRecordIterator *pIter;
    if (pModelSpace->newIterator(pIter) == Acad::eOk)
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

    actionEnd();
}