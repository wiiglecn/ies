#include "stdafx.h"
#include "ToolLayer.h"
#include "cJSON.h"
#include "arxHeaders.h"
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
        CUtils::acutPrintf(_T("[ERROR] create_layer: invalid parameters.\n"));
        return;
    }

    ACHAR layerName[256] = { 0 };
#ifdef _CAD2005
	strcpy(layerName, nameItem->valuestring);
#else
	if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName)))
    {
        CUtils::acutPrintf(_T("[ERROR] create_layer: failed to convert layer name.\n"));
        return;
    }
#endif

    /*int colorIndex = 7;
    if (colorItem && cJSON_IsNumber(colorItem))
        colorIndex = (int)colorItem->valuedouble;*/

    if (this->actionBefore()) return;

    AcDbLayerTable* pLayerTable = NULL;
    if (this->pDb->getLayerTable(pLayerTable, AcDb::kForWrite) != Acad::eOk)
    {
        CUtils::acutPrintf(_T("[ERROR] create_layer: failed to open Layer Table.\n"));
        this->actionEnd();
        return;
    }

    AcDbObjectId layerId;
    if (pLayerTable->getAt(layerName, layerId) == Acad::eOk)
    {
        CUtils::acutPrintf(_T("[INFO] create_layer: layer \"%s\" already exists.\n"), layerName);
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
        CUtils::acutPrintf(_T("[ERROR] create_layer: failed to add layer record.\n"));
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
        CUtils::acutPrintf(_T("[ERROR] list_layers: failed to open Layer Table.\n"));
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
#ifdef _CAD2005
                   cJSON_AddItemToArray(data->resultJson, cJSON_CreateString(pName));
#else
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
#endif
                    CUtils::acutPrintf(_T("[LAYER] %s\n"), pName);
                }
                else
                {
                    CUtils::acutPrintf(_T("[LAYER] <Unknown Name>\n"));
                }
                pLayerRecord->close();
            }
        }
        delete pIter;
        
		data->ret = 0;
    }
    else
    {
        CUtils::acutPrintf(_T("[ERROR] list_layers: failed to create Layer Table iterator.\n"));
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
        CUtils::acutPrintf(_T("[ERROR] query_all_layers: failed to open Layer Table.\n"));
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
#ifdef _CAD2005
					char* utf8Name=const_cast<char*>(pName);
#else
                    char utf8Name[256] = { 0 };
                    CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));
#endif
                    cJSON_AddStringToObject(pJsonItem, "name", utf8Name);

                    // 2. color index
                    AcCmColor color = pLayerRecord->color();
                    Adesk::UInt16 colorIndex = color.colorIndex();

					// 获取 RGB 值并格式化为16进制字符串
					Adesk::UInt8 r = color.red();
					Adesk::UInt8 g = color.green();
					Adesk::UInt8 b = color.blue();
					char hexColor[8];
#ifdef _CAD2005
					sprintf(hexColor, "#%02X%02X%02X", r, g, b);
#else
					sprintf_s(hexColor, "#%02X%02X%02X", r, g, b);
#endif

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
        CUtils::acutPrintf(_T("[ERROR] query_all_layers: failed to create Layer Table iterator.\n"));
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
        CUtils::acutPrintf(_T("[ERROR] query_layer: invalid parameters.\n"));
        return;
    }

    ACHAR layerName[256] = { 0 };
#ifdef _CAD2005
	strcpy(layerName, nameItem->valuestring);
#else
    if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName))) {
        CUtils::acutPrintf(_T("[ERROR] query_layer: failed to convert layer name.\n"));
        return;
    }
#endif

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
#ifdef _CAD2005
				char* utf8Name=layerName;
#else
                char utf8Name[256] = { 0 };
                CUtils::TCharToUtf8(layerName, utf8Name, sizeof(utf8Name));
#endif
                cJSON_AddStringToObject(data->resultJson, "name", utf8Name);
                
                AcCmColor color = pLayerRec->color();
                Adesk::UInt16 colorIndex = color.colorIndex();
				Adesk::UInt8 r = color.red();
				Adesk::UInt8 g = color.green();
				Adesk::UInt8 b = color.blue();
				char hexColor[8];
#ifdef _CAD2005
				sprintf(hexColor, "#%02X%02X%02X", r, g, b);
#else
				sprintf_s(hexColor, "#%02X%02X%02X", r, g, b);
#endif
                cJSON_AddNumberToObject(data->resultJson, "color", colorIndex);
				cJSON_AddStringToObject(data->resultJson, "hex", hexColor);

                pLayerRec->close();
                
                data->ret = 0;
            } 
        } else {
            CUtils::acutPrintf(_T("[ERROR] query_layer: layer not found.\n"));
        }
        pLayerTable->close();
    } else {
        CUtils::acutPrintf(_T("[ERROR] query_layer: failed to open Layer Table.\n"));
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
#ifdef _CAD2005
			char* utf8Name = const_cast<char*>(pName);
#else
            char utf8Name[256] = { 0 };
            CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));
#endif
            cJSON_AddStringToObject(data->resultJson, "name", utf8Name);
        }
        pLayerRec->close();
        data->ret = 0;
    } else {
        CUtils::acutPrintf(_T("[ERROR] get_current_layer: failed to get current layer.\n"));
        
    }
    
    this->actionEnd();
}

void ToolLayer::set_current_layer(AiToolCommandData* data) {
    cJSON* params = data->jsonRoot;
    cJSON* nameItem = cJSON_GetObjectItem(params, "name");

    if (!nameItem || !cJSON_IsString(nameItem)) {
        CUtils::acutPrintf(_T("[ERROR] set_current_layer: invalid parameters.\n"));
        return;
    }

    ACHAR layerName[256] = { 0 };
#ifdef _CAD2005
	strcpy(layerName, nameItem->valuestring);
#else
    if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName))) {
        CUtils::acutPrintf(_T("[ERROR] set_current_layer: failed to convert layer name.\n"));
        return;
    }
#endif

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
        CUtils::acutPrintf(_T("[ERROR] set_current_layer: failed to set layer.\n"));
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
#ifdef _CAD2005
		sprintf(hexColor, "#%02X%02X%02X", r, g, b);
#else
        sprintf_s(hexColor, "#%02X%02X%02X", r, g, b);
#endif
        data->resultJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(data->resultJson, "color", colorIndex);
        cJSON_AddStringToObject(data->resultJson, "hex", hexColor);
        pLayerRec->close();
        
		data->ret = 0;
    } else {
        CUtils::acutPrintf(_T("[ERROR] get_current_layer_color: failed to get layer color.\n"));
    }
    
    this->actionEnd();
}

void ToolLayer::layer_set_properties(AiToolCommandData* data) {
    cJSON* params = data->jsonRoot;
    cJSON* nameItem = cJSON_GetObjectItem(params, "name");

    if (!nameItem || !cJSON_IsString(nameItem)) {
        CUtils::acutPrintf(_T("[ERROR] layer_set_properties: invalid name parameter.\n"));
        return;
    }

    ACHAR layerName[256] = { 0 };
#ifdef _CAD2005
	strcpy(layerName, nameItem->valuestring);
#else
    if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName))) {
        CUtils::acutPrintf(_T("[ERROR] layer_set_properties: failed to convert layer name.\n"));
        return;
    }
#endif

    if (this->actionBefore()) return;

    AcDbLayerTable* pLayerTable = NULL;
    if (pDb->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
        CUtils::acutPrintf(_T("[ERROR] layer_set_properties: failed to open Layer Table.\n"));
        this->actionEnd();
        return;
    }

    AcDbObjectId layerId;
    if (pLayerTable->getAt(layerName, layerId) != Acad::eOk) {
        CUtils::acutPrintf(_T("[ERROR] layer_set_properties: layer not found.\n"));
        pLayerTable->close();
        this->actionEnd();
        return;
    }
    pLayerTable->close();

    AcDbLayerTableRecord* pLayerRec = NULL;
    if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForWrite) != Acad::eOk) {
        CUtils::acutPrintf(_T("[ERROR] layer_set_properties: failed to open layer for write.\n"));
        this->actionEnd();
        return;
    }

    bool changed = false;

    // Set color
    cJSON* colorItem = cJSON_GetObjectItem(params, "color");
    if (colorItem && cJSON_IsNumber(colorItem)) {
        AcCmColor color;
        color.setColorIndex((Adesk::UInt16)colorItem->valuedouble);
        pLayerRec->setColor(color);
        changed = true;
    }

    // Set linetype
    cJSON* linetypeItem = cJSON_GetObjectItem(params, "linetype");
    if (linetypeItem && cJSON_IsString(linetypeItem)) {
        ACHAR ltName[256] = { 0 };
#ifdef _CAD2005
		strcpy(ltName, linetypeItem->valuestring);
#else
		CUtils::utf8ToTChar(linetypeItem->valuestring, ltName, ARRAYSIZE(ltName));
#endif
        
        AcDbLinetypeTable* pLtTable = NULL;
        if (pDb->getLinetypeTable(pLtTable, AcDb::kForRead) == Acad::eOk) {
            AcDbObjectId ltId;
            if (pLtTable->getAt(ltName, ltId) == Acad::eOk) {
                pLayerRec->setLinetypeObjectId(ltId);
                changed = true;
            }
            pLtTable->close();
        }
    }

    // Set lineweight
    cJSON* lwItem = cJSON_GetObjectItem(params, "lineweight");
    if (lwItem && cJSON_IsNumber(lwItem)) {
        AcDb::LineWeight lw = (AcDb::LineWeight)(int)lwItem->valuedouble;
        pLayerRec->setLineWeight(lw);
        changed = true;
    }

    pLayerRec->close();

    if (changed) {
        data->resultJson = cJSON_CreateObject();
        cJSON_AddStringToObject(data->resultJson, "name", nameItem->valuestring);
        data->ret = 0;
    } else {
        CUtils::acutPrintf(_T("[WARNING] layer_set_properties: no properties were set.\n"));
    }

    this->actionEnd();
}

void ToolLayer::layer_freeze(AiToolCommandData* data) {
    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        CUtils::acutPrintf(_T("[ERROR] layer_freeze: invalid name parameter.\n"));
        return;
    }

    ACHAR layerName[256] = { 0 };
#ifdef _CAD2005
	strcpy(layerName, nameItem->valuestring);
#else
    if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName))) {
        CUtils::acutPrintf(_T("[ERROR] layer_freeze: failed to convert layer name.\n"));
        return;
    }
#endif

    if (this->actionBefore()) return;

    AcDbLayerTable* pLayerTable = NULL;
    if (pDb->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
        this->actionEnd();
        return;
    }

    AcDbObjectId layerId;
    if (pLayerTable->getAt(layerName, layerId) != Acad::eOk) {
        CUtils::acutPrintf(_T("[ERROR] layer_freeze: layer not found.\n"));
        pLayerTable->close();
        this->actionEnd();
        return;
    }
    pLayerTable->close();

    AcDbLayerTableRecord* pLayerRec = NULL;
    if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForWrite) == Acad::eOk) {
        pLayerRec->setIsFrozen(Adesk::kTrue);
        pLayerRec->close();
        data->resultJson = cJSON_CreateObject();
        cJSON_AddStringToObject(data->resultJson, "name", nameItem->valuestring);
        cJSON_AddBoolToObject(data->resultJson, "isFrozen", 1);
        data->ret = 0;
    }

    this->actionEnd();
}

void ToolLayer::layer_thaw(AiToolCommandData* data) {
    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        CUtils::acutPrintf(_T("[ERROR] layer_thaw: invalid name parameter.\n"));
        return;
    }

    ACHAR layerName[256] = { 0 };
#ifdef _CAD2005
	strcpy(layerName, nameItem->valuestring);
#else
    if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName))) {
        CUtils::acutPrintf(_T("[ERROR] layer_thaw: failed to convert layer name.\n"));
        return;
    }
#endif

    if (this->actionBefore()) return;

    AcDbLayerTable* pLayerTable = NULL;
    if (pDb->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
        this->actionEnd();
        return;
    }

    AcDbObjectId layerId;
    if (pLayerTable->getAt(layerName, layerId) != Acad::eOk) {
        CUtils::acutPrintf(_T("[ERROR] layer_thaw: layer not found.\n"));
        pLayerTable->close();
        this->actionEnd();
        return;
    }
    pLayerTable->close();

    AcDbLayerTableRecord* pLayerRec = NULL;
    if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForWrite) == Acad::eOk) {
        pLayerRec->setIsFrozen(Adesk::kFalse);
        pLayerRec->close();
        data->resultJson = cJSON_CreateObject();
        cJSON_AddStringToObject(data->resultJson, "name", nameItem->valuestring);
        cJSON_AddBoolToObject(data->resultJson, "isFrozen", 0);
        data->ret = 0;
    }

    this->actionEnd();
}

void ToolLayer::layer_lock(AiToolCommandData* data) {
    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        CUtils::acutPrintf(_T("[ERROR] layer_lock: invalid name parameter.\n"));
        return;
    }

    ACHAR layerName[256] = { 0 };
#ifdef _CAD2005
	strcpy(layerName, nameItem->valuestring);
#else
    if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName))) {
        CUtils::acutPrintf(_T("[ERROR] layer_lock: failed to convert layer name.\n"));
        return;
    }
#endif

    if (this->actionBefore()) return;

    AcDbLayerTable* pLayerTable = NULL;
    if (pDb->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
        this->actionEnd();
        return;
    }

    AcDbObjectId layerId;
    if (pLayerTable->getAt(layerName, layerId) != Acad::eOk) {
        CUtils::acutPrintf(_T("[ERROR] layer_lock: layer not found.\n"));
        pLayerTable->close();
        this->actionEnd();
        return;
    }
    pLayerTable->close();

    AcDbLayerTableRecord* pLayerRec = NULL;
    if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForWrite) == Acad::eOk) {
        pLayerRec->setIsLocked(Adesk::kTrue);
        pLayerRec->close();
        data->resultJson = cJSON_CreateObject();
        cJSON_AddStringToObject(data->resultJson, "name", nameItem->valuestring);
        cJSON_AddBoolToObject(data->resultJson, "isLocked", 1);
        data->ret = 0;
    }

    this->actionEnd();
}

void ToolLayer::layer_unlock(AiToolCommandData* data) {
    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        CUtils::acutPrintf(_T("[ERROR] layer_unlock: invalid name parameter.\n"));
        return;
    }

    ACHAR layerName[256] = { 0 };
#ifdef _CAD2005
	strcpy(layerName, nameItem->valuestring);
#else
    if (!CUtils::utf8ToTChar(nameItem->valuestring, layerName, ARRAYSIZE(layerName))) {
        CUtils::acutPrintf(_T("[ERROR] layer_unlock: failed to convert layer name.\n"));
        return;
    }
#endif

    if (this->actionBefore()) return;

    AcDbLayerTable* pLayerTable = NULL;
    if (pDb->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) {
        this->actionEnd();
        return;
    }

    AcDbObjectId layerId;
    if (pLayerTable->getAt(layerName, layerId) != Acad::eOk) {
        CUtils::acutPrintf(_T("[ERROR] layer_unlock: layer not found.\n"));
        pLayerTable->close();
        this->actionEnd();
        return;
    }
    pLayerTable->close();

    AcDbLayerTableRecord* pLayerRec = NULL;
    if (acdbOpenAcDbObject((AcDbObject*&)pLayerRec, layerId, AcDb::kForWrite) == Acad::eOk) {
        pLayerRec->setIsLocked(Adesk::kFalse);
        pLayerRec->close();
        data->resultJson = cJSON_CreateObject();
        cJSON_AddStringToObject(data->resultJson, "name", nameItem->valuestring);
        cJSON_AddBoolToObject(data->resultJson, "isLocked", 0);
        data->ret = 0;
    }

    this->actionEnd();
}
