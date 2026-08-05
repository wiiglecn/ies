#include "stdafx.h"
#include "ToolLayer.h"
#include "cJSON.h"
#include "adscodes.h"
#include "actrans.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "dbelipse.h"
#include "dbspline.h"

#include "utils.h"
ToolLayer::ToolLayer(void):ToolDrawBase()
{
}

ToolLayer::~ToolLayer(void)
{
}
ToolLayer& ToolLayer::getInstance()
{
    static ToolLayer instance;
    return instance;
}
void ToolLayer::create_layer(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;
    cJSON* nameItem = cJSON_GetObjectItem(params, "name");
    //cJSON* colorItem = cJSON_GetObjectItem(params, "color");

    if (!nameItem || !cJSON_IsString(nameItem))
    {
        acutPrintf(_T("[ERROR] create_layer: invalid parameters.\n"));
        return;
    }

    TCHAR layerName[256] = { 0 };
	if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName)))
    {
        acutPrintf(_T("[ERROR] create_layer: failed to convert layer name.\n"));
        return;
    }

    /*int colorIndex = 7;
    if (colorItem && cJSON_IsNumber(colorItem))
        colorIndex = (int)colorItem->valuedouble;*/

    if (this->actionBefore()) return;

    AcDbLayerTable* pLayerTable = NULL;
    if (this->pDb->getLayerTable(pLayerTable, AcDb::kForWrite) != Acad::eOk)
    {
        acutPrintf(_T("[ERROR] create_layer: failed to open Layer Table.\n"));
        this->actionEnd();
        return;
    }

    AcDbObjectId layerId;
    if (pLayerTable->getAt(layerName, layerId) == Acad::eOk)
    {
        acutPrintf(_T("[INFO] create_layer: layer \"%s\" already exists.\n"), layerName);
        pLayerTable->close();
        this->actionEnd();
        return;
    }

    AcDbLayerTableRecord* pLayerRecord = new AcDbLayerTableRecord();
    pLayerRecord->setName(layerName);
    //pLayerRecord->setColorIndex(colorIndex);

    if (pLayerTable->add(layerId, pLayerRecord) == Acad::eOk)
    {
        //acutPrintf(_T("[INFO] create_layer: created successfully.\n"));
        pLayerRecord->close();
		data->resultJson = cJSON_CreateObject();
        cJSON_AddStringToObject(data->resultJson, "name", nameItem->valuestring);
        
		data->ret = 0;
    }
    else
    {
        acutPrintf(_T("[ERROR] create_layer: failed to add layer record.\n"));
        delete pLayerRecord;
    }

    pLayerTable->close();
    this->actionEnd();
}

void ToolLayer::list_layers(AiToolCommandData* data)
{
	// cJSON* params = data->jsonRoot;
    if (this->actionBefore())
	{
		return;
	}

    AcDbLayerTable* pLayerTable = NULL;
    if (this->pDb->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk)
    {
        acutPrintf(_T("[ERROR] list_layers: failed to open Layer Table.\n"));
        this->actionEnd();
        return;
    }

    data->resultJson = cJSON_CreateArray();
    AcDbLayerTableIterator* pIter = NULL;
    if (pLayerTable->newIterator(pIter) == Acad::eOk)
    {
        for (; !pIter->done(); pIter->step())
        {
            AcDbLayerTableRecord* pLayerRecord = NULL;
            if (pIter->getRecord(pLayerRecord, AcDb::kForRead) == Acad::eOk)
            {
                 // Use getName to retrieve the layer name
                const ACHAR* pName = NULL;
                if (pLayerRecord->getName(pName) == Acad::eOk && pName != NULL)
                {
                    // Convert ACHAR* (Unicode) to UTF-8 string for cJSON
                    // Assuming TCHAR is wchar_t (Unicode)
#ifdef _UNICODE
                    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pName, -1, NULL, 0, NULL, NULL);
                    if (utf8Len > 0)
                    {
                        char* utf8Str = new char[utf8Len];
                        if (WideCharToMultiByte(CP_UTF8, 0, pName, -1, utf8Str, utf8Len, NULL, NULL) > 0)
                        {
                            // Add string to JSON array
                            cJSON_AddItemToArray(data->resultJson, cJSON_CreateString(utf8Str));
                        }
                        delete[] utf8Str;
                    }
#else
                    // For MBCS, assume it's already ANSI/UTF-8 compatible or convert appropriately
                    cJSON_AddItemToArray(data->resultJson, cJSON_CreateString(pName));
#endif
                    acutPrintf(_T("[LAYER] %s\n"), pName);
                }
                else
                {
                    acutPrintf(_T("[LAYER] <Unknown Name>\n"));
                }
                pLayerRecord->close();
            }
        }
        delete pIter;
        
		data->ret = 0;
    }
    else
    {
        acutPrintf(_T("[ERROR] list_layers: failed to create Layer Table iterator.\n"));
    }

    pLayerTable->close();
    this->actionEnd();
}

void ToolLayer::query_all_layers(AiToolCommandData* data)
{
    if (this->actionBefore())
    {
        return;
    }

    AcDbLayerTable* pLayerTable = NULL;
    if (this->pDb->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk)
    {
        acutPrintf(_T("[ERROR] query_all_layers: failed to open Layer Table.\n"));
        this->actionEnd();
        return;
    }

    cJSON* pJsonArray = cJSON_CreateArray();

    AcDbLayerTableIterator* pIter = NULL;
    if (pLayerTable->newIterator(pIter) == Acad::eOk)
    {
        for (; !pIter->done(); pIter->step())
        {
            AcDbLayerTableRecord* pLayerRecord = NULL;
            if (pIter->getRecord(pLayerRecord, AcDb::kForRead) == Acad::eOk)
            {
                const ACHAR* pName = NULL;
                if (pLayerRecord->getName(pName) == Acad::eOk && pName != NULL)
                {
                    cJSON* pJsonItem = cJSON_CreateObject();


                    // 1. layer name
                    char utf8Name[256] = { 0 };
                    CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));
                    cJSON_AddStringToObject(pJsonItem, "name", utf8Name);

                    // 2. color index
                    AcCmColor color = pLayerRecord->color();
                    Adesk::UInt16 colorIndex = color.colorIndex();

					// 获取 RGB 值并格式化为16进制字符串
					Adesk::UInt8 r = color.red();
					Adesk::UInt8 g = color.green();
					Adesk::UInt8 b = color.blue();
					char hexColor[8];
					sprintf_s(hexColor, "#%02X%02X%02X", r, g, b);

                    cJSON_AddNumberToObject(pJsonItem, "color", colorIndex);
					cJSON_AddStringToObject(pJsonItem, "hex", hexColor);

                    // 3. is off (hidden)
                    Adesk::Boolean isOff = pLayerRecord->isOff();
                    cJSON_AddBoolToObject(pJsonItem, "isOff", isOff ? 1 : 0);

                    // 4. is frozen
                    Adesk::Boolean isFrozen = pLayerRecord->isFrozen();
                    cJSON_AddBoolToObject(pJsonItem, "isFrozen", isFrozen ? 1 : 0);

                    // 5. is locked
                    Adesk::Boolean isLocked = pLayerRecord->isLocked();
                    cJSON_AddBoolToObject(pJsonItem, "isLocked", isLocked ? 1 : 0);

                    cJSON_AddItemToArray(pJsonArray, pJsonItem);
                }
                pLayerRecord->close();
            }
        }
        delete pIter;
        
    }
    else
    {
        acutPrintf(_T("[ERROR] query_all_layers: failed to create Layer Table iterator.\n"));
        cJSON_Delete(pJsonArray);
        pLayerTable->close();
        this->actionEnd();
        return;
    }

    pLayerTable->close();

    data->resultJson = pJsonArray;
    data->ret = 0;

    this->actionEnd();
}

void ToolLayer::query_layer(AiToolCommandData* data) {
    cJSON* params = data->jsonRoot;
    cJSON* nameItem = cJSON_GetObjectItem(params, "name");

    if (!nameItem || !cJSON_IsString(nameItem)) {
        acutPrintf(_T("[ERROR] query_layer: invalid parameters.\n"));
        return;
    }

    TCHAR layerName[256] = { 0 };
    if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName))) {
        acutPrintf(_T("[ERROR] query_layer: failed to convert layer name.\n"));
        return;
    }

    if (this->actionBefore()) {
        return;
    }

    AcDbLayerTable* pLayerTable = NULL;
    if (pDb->getLayerTable(pLayerTable, AcDb::kForRead) == Acad::eOk) {
        AcDbObjectId layerId;
        if (pLayerTable->getAt(layerName, layerId) == Acad::eOk) {
            AcDbLayerTableRecord* pLayerRec = NULL;
            if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForRead) == Acad::eOk) {
                data->resultJson = cJSON_CreateObject();
                
                char utf8Name[256] = { 0 };
                CUtils::TCharToUtf8(layerName, utf8Name, sizeof(utf8Name));
                cJSON_AddStringToObject(data->resultJson, "name", utf8Name);
                
                AcCmColor color = pLayerRec->color();
                Adesk::UInt16 colorIndex = color.colorIndex();
				Adesk::UInt8 r = color.red();
				Adesk::UInt8 g = color.green();
				Adesk::UInt8 b = color.blue();
				char hexColor[8];
				sprintf_s(hexColor, "#%02X%02X%02X", r, g, b);

                cJSON_AddNumberToObject(data->resultJson, "color", colorIndex);
				cJSON_AddStringToObject(data->resultJson, "hex", hexColor);

                pLayerRec->close();
                
                data->ret = 0;
            } 
        } else {
            acutPrintf(_T("[ERROR] query_layer: layer not found.\n"));
        }
        pLayerTable->close();
    } else {
        acutPrintf(_T("[ERROR] query_layer: failed to open Layer Table.\n"));
    }

    this->actionEnd();
}

void ToolLayer::get_current_layer(AiToolCommandData* data) {
    if (this->actionBefore()) return;
    
    AcDbObjectId layerId = pDb->clayer();
    
    AcDbLayerTableRecord* pLayerRec = NULL;
    if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForRead) == Acad::eOk) {
        const ACHAR* pName = NULL;
        if (pLayerRec->getName(pName) == Acad::eOk && pName != NULL) {
            data->resultJson = cJSON_CreateObject();
            
            char utf8Name[256] = { 0 };
            CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));
            cJSON_AddStringToObject(data->resultJson, "name", utf8Name);
        }
        pLayerRec->close();
        data->ret = 0;
    } else {
        acutPrintf(_T("[ERROR] get_current_layer: failed to get current layer.\n"));
        
    }
    
    this->actionEnd();
}

void ToolLayer::set_current_layer(AiToolCommandData* data) {
    cJSON* params = data->jsonRoot;
    cJSON* nameItem = cJSON_GetObjectItem(params, "name");

    if (!nameItem || !cJSON_IsString(nameItem)) {
        acutPrintf(_T("[ERROR] set_current_layer: invalid parameters.\n"));
        return;
    }

    TCHAR layerName[256] = { 0 };
    if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName))) {
        acutPrintf(_T("[ERROR] set_current_layer: failed to convert layer name.\n"));
        return;
    }

    if (this->actionBefore()) return;

    AcDbLayerTable* pLayerTable = NULL;
    AcDbObjectId layerId;
    Acad::ErrorStatus es = Acad::eInvalidInput;

    // Open the layer table to find the ObjectId of the layer
    if (pDb->getLayerTable(pLayerTable, AcDb::kForRead) == Acad::eOk) {
        if (pLayerTable->getAt(layerName, layerId) == Acad::eOk) {
            // Set the current layer using the retrieved ObjectId
            es = pDb->setClayer(layerId);
        }
        pLayerTable->close();
    }

    if (es == Acad::eOk) {
        data->resultJson = cJSON_CreateObject();
        cJSON_AddStringToObject(data->resultJson, "name", nameItem->valuestring);
        
        data->ret = 0;
    } else {
        acutPrintf(_T("[ERROR] set_current_layer: failed to set layer.\n"));
    }

    this->actionEnd();
}

void ToolLayer::get_current_layer_color(AiToolCommandData* data) {
	if (this->actionBefore()) {
		return;
	}
    
    AcDbObjectId layerId = pDb->clayer();
    
    AcDbLayerTableRecord* pLayerRec = NULL;
    if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForRead) == Acad::eOk) {
        AcCmColor color = pLayerRec->color();
        Adesk::UInt16 colorIndex = color.colorIndex();

        // 获取 RGB 值并格式化为16进制字符串
        Adesk::UInt8 r = color.red();
        Adesk::UInt8 g = color.green();
        Adesk::UInt8 b = color.blue();
        char hexColor[8];
        sprintf_s(hexColor, "#%02X%02X%02X", r, g, b);

        data->resultJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(data->resultJson, "color", colorIndex);
        cJSON_AddStringToObject(data->resultJson, "hex", hexColor);
        pLayerRec->close();
        
		data->ret = 0;
    } else {
        acutPrintf(_T("[ERROR] get_current_layer_color: failed to get layer color.\n"));
    }
    
    this->actionEnd();
}
