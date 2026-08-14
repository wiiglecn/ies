#include "stdafx.h"
#include "ToolStyle.h"
#include "cJSON.h"
#include "arxHeaders.h"
#include "utils.h"

ToolStyle::ToolStyle(void):ToolDrawBase()
{
}

ToolStyle::~ToolStyle(void)
{
}
ToolStyle& ToolStyle::getInstance()
{
    static ToolStyle instance;
    return instance;
}

// ==================== Text Style Operations ====================

void ToolStyle::add_text_style(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        actionEnd();
        return;
    }

    TCHAR styleName[256] = {0};
    CUtils::utf8ToTChar(nameItem->valuestring, styleName, 256);

    AcDbTextStyleTable* pTextStyleTable = NULL;
    if (pDb->getTextStyleTable(pTextStyleTable, AcDb::kForWrite) != Acad::eOk) {
        actionEnd();
        return;
    }

    // Check if already exists
    if (pTextStyleTable->has(styleName)) {
        pTextStyleTable->close();
        actionEnd();
        return;
    }

    AcDbTextStyleTableRecord* pRecord = new AcDbTextStyleTableRecord();
    pRecord->setName(styleName);

    // Set font file name (e.g., "simplex.shx" or "arial.ttf")
    cJSON* fontItem = cJSON_GetObjectItem(data->jsonRoot, "font");
    if (fontItem && cJSON_IsString(fontItem)) {
        TCHAR fontFile[256] = {0};
        CUtils::utf8ToTChar(fontItem->valuestring, fontFile, 256);
        pRecord->setFileName(fontFile);
    }

    // Set text height
    cJSON* heightItem = cJSON_GetObjectItem(data->jsonRoot, "height");
    if (heightItem) {
        pRecord->setTextSize(heightItem->valuedouble);
    }

    // Set width factor
    cJSON* widthItem = cJSON_GetObjectItem(data->jsonRoot, "width_factor");
    if (widthItem) {
        pRecord->setXScale(widthItem->valuedouble);
    }

    // Set oblique angle (radians)
    cJSON* obliqueItem = cJSON_GetObjectItem(data->jsonRoot, "oblique_angle");
    if (obliqueItem) {
        pRecord->setObliquingAngle(obliqueItem->valuedouble);
    }

    // Modify upside down & backward using flagBits
    cJSON* upsideDownItem = cJSON_GetObjectItem(data->jsonRoot, "upside_down");
    cJSON* backwardItem = cJSON_GetObjectItem(data->jsonRoot, "backward");
    if (upsideDownItem || backwardItem) {
        Adesk::UInt8 flags = pRecord->flagBits();
        if (upsideDownItem) {
            if (upsideDownItem->valueint != 0) {
                flags |= 0x4; // upside down
            } else {
                flags &= ~0x4;
            }
        }
        if (backwardItem) {
            if (backwardItem->valueint != 0) {
                flags |= 0x2; // backwards
            } else {
                flags &= ~0x2;
            }
        }
        pRecord->setFlagBits(flags);
    }

    AcDbObjectId styleId;
    Acad::ErrorStatus es = pTextStyleTable->add(styleId, pRecord);
    if (es == Acad::eOk) {
        // Return handle
        AcDbHandle handle = pRecord->objectId().handle();
        TCHAR handleStr[20] = {0};
        handle.getIntoAsciiBuffer(handleStr);
        char utf8Handle[32] = {0};
        CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));

        cJSON* pResult = cJSON_CreateObject();
        cJSON_AddStringToObject(pResult, "name", nameItem->valuestring);
        cJSON_AddStringToObject(pResult, "handle", utf8Handle);
        data->resultJson = pResult;
        data->ret = 0;
    }
    pRecord->close();
    pTextStyleTable->close();

    actionEnd();
}

void ToolStyle::modify_text_style(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        actionEnd();
        return;
    }

    TCHAR styleName[256] = {0};
    CUtils::utf8ToTChar(nameItem->valuestring, styleName, 256);

    AcDbTextStyleTable* pTextStyleTable = NULL;
    if (pDb->getTextStyleTable(pTextStyleTable, AcDb::kForRead) != Acad::eOk) {
        actionEnd();
        return;
    }

    AcDbTextStyleTableRecord* pRecord = NULL;
    if (pTextStyleTable->getAt(styleName, pRecord, AcDb::kForWrite) != Acad::eOk) {
        pTextStyleTable->close();
        actionEnd();
        return;
    }
    pTextStyleTable->close();

    // Modify font file name
    cJSON* fontItem = cJSON_GetObjectItem(data->jsonRoot, "font");
    if (fontItem && cJSON_IsString(fontItem)) {
        TCHAR fontFile[256] = {0};
        CUtils::utf8ToTChar(fontItem->valuestring, fontFile, 256);
        pRecord->setFileName(fontFile);
    }

    // Modify text height
    cJSON* heightItem = cJSON_GetObjectItem(data->jsonRoot, "height");
    if (heightItem) {
        pRecord->setTextSize(heightItem->valuedouble);
    }

    // Modify width factor
    cJSON* widthItem = cJSON_GetObjectItem(data->jsonRoot, "width_factor");
    if (widthItem) {
        pRecord->setXScale(widthItem->valuedouble);
    }

    // Modify oblique angle
    cJSON* obliqueItem = cJSON_GetObjectItem(data->jsonRoot, "oblique_angle");
    if (obliqueItem) {
        pRecord->setObliquingAngle(obliqueItem->valuedouble);
    }

    // Modify upside down & backward using flagBits
    cJSON* upsideDownItem = cJSON_GetObjectItem(data->jsonRoot, "upside_down");
    cJSON* backwardItem = cJSON_GetObjectItem(data->jsonRoot, "backward");
    if (upsideDownItem || backwardItem) {
        Adesk::UInt8 flags = pRecord->flagBits();
        if (upsideDownItem) {
            if (upsideDownItem->valueint != 0) {
                flags |= 0x4; // upside down
            } else {
                flags &= ~0x4;
            }
        }
        if (backwardItem) {
            if (backwardItem->valueint != 0) {
                flags |= 0x2; // backwards
            } else {
                flags &= ~0x2;
            }
        }
        pRecord->setFlagBits(flags);
    }

    pRecord->close();
    data->ret = 0;
    actionEnd();
}

void ToolStyle::delete_text_style(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        actionEnd();
        return;
    }

    TCHAR styleName[256] = {0};
    CUtils::utf8ToTChar(nameItem->valuestring, styleName, 256);

    // Cannot delete Standard style
    if (_tcsicmp(styleName, _T("Standard")) == 0) {
        actionEnd();
        return;
    }

    AcDbTextStyleTable* pTextStyleTable = NULL;
    if (pDb->getTextStyleTable(pTextStyleTable, AcDb::kForWrite) != Acad::eOk) {
        actionEnd();
        return;
    }

    AcDbTextStyleTableRecord* pRecord = NULL;
    if (pTextStyleTable->getAt(styleName, pRecord, AcDb::kForWrite) == Acad::eOk) {
        Acad::ErrorStatus es = pRecord->erase();
        pRecord->close();
        if (es == Acad::eOk) {
            data->ret = 0;
        }
    }
    pTextStyleTable->close();

    actionEnd();
}

// ==================== Dimension Style Operations ====================

void ToolStyle::add_dim_style(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        actionEnd();
        return;
    }

    TCHAR styleName[256] = {0};
    CUtils::utf8ToTChar(nameItem->valuestring, styleName, 256);

    AcDbDimStyleTable* pDimStyleTable = NULL;
    if (pDb->getDimStyleTable(pDimStyleTable, AcDb::kForWrite) != Acad::eOk) {
        actionEnd();
        return;
    }

    // Check if already exists
    if (pDimStyleTable->has(styleName)) {
        pDimStyleTable->close();
        actionEnd();
        return;
    }

    AcDbDimStyleTableRecord* pRecord = new AcDbDimStyleTableRecord();
    pRecord->setName(styleName);

    // Set arrow size
    cJSON* arrowItem = cJSON_GetObjectItem(data->jsonRoot, "arrow_size");
    if (arrowItem) {
        pRecord->setDimasz(arrowItem->valuedouble);
    }

    // Set text height
    cJSON* textHeightItem = cJSON_GetObjectItem(data->jsonRoot, "text_height");
    if (textHeightItem) {
        pRecord->setDimtxt(textHeightItem->valuedouble);
    }

    // Set dimension scale
    cJSON* scaleItem = cJSON_GetObjectItem(data->jsonRoot, "dim_scale");
    if (scaleItem) {
        pRecord->setDimscale(scaleItem->valuedouble);
    }

    // Set decimal places
    cJSON* decItem = cJSON_GetObjectItem(data->jsonRoot, "decimal_places");
    if (decItem) {
        pRecord->setDimdec(decItem->valueint);
    }

    // Set text color
    cJSON* textColorItem = cJSON_GetObjectItem(data->jsonRoot, "text_color");
    if (textColorItem) {
        AcCmColor color;
        color.setColorIndex(textColorItem->valueint);
        pRecord->setDimclrt(color);
    }

    // Set dimension line color
    cJSON* dimColorItem = cJSON_GetObjectItem(data->jsonRoot, "dim_line_color");
    if (dimColorItem) {
        AcCmColor color;
        color.setColorIndex(dimColorItem->valueint);
        pRecord->setDimclrd(color);
    }

    // Set extension line offset
    cJSON* extOffsetItem = cJSON_GetObjectItem(data->jsonRoot, "extension_line_offset");
    if (extOffsetItem) {
        pRecord->setDimexo(extOffsetItem->valuedouble);
    }

    // Set extension line extend beyond dim line
    cJSON* extExtendItem = cJSON_GetObjectItem(data->jsonRoot, "extension_line_extend");
    if (extExtendItem) {
        pRecord->setDimexe(extExtendItem->valuedouble);
    }

    // Set text gap
    cJSON* gapItem = cJSON_GetObjectItem(data->jsonRoot, "text_gap");
    if (gapItem) {
        pRecord->setDimgap(gapItem->valuedouble);
    }

    // Set text style (by name)
    cJSON* textStyleItem = cJSON_GetObjectItem(data->jsonRoot, "text_style");
    if (textStyleItem && cJSON_IsString(textStyleItem)) {
        TCHAR textStyleName[256] = {0};
        CUtils::utf8ToTChar(textStyleItem->valuestring, textStyleName, 256);
        
        AcDbTextStyleTable* pTextStyleTable = NULL;
        if (pDb->getTextStyleTable(pTextStyleTable, AcDb::kForRead) == Acad::eOk) {
            AcDbTextStyleTableRecord* pTextStyleRec = NULL;
            if (pTextStyleTable->getAt(textStyleName, pTextStyleRec, AcDb::kForRead) == Acad::eOk) {
                pRecord->setDimtxsty(pTextStyleRec->objectId());
                pTextStyleRec->close();
            }
            pTextStyleTable->close();
        }
    }

    AcDbObjectId styleId;
    Acad::ErrorStatus es = pDimStyleTable->add(styleId, pRecord);
    if (es == Acad::eOk) {
        // Return handle
        AcDbHandle handle = pRecord->objectId().handle();
        TCHAR handleStr[20] = {0};
        handle.getIntoAsciiBuffer(handleStr);
        char utf8Handle[32] = {0};
        CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));

        cJSON* pResult = cJSON_CreateObject();
        cJSON_AddStringToObject(pResult, "name", nameItem->valuestring);
        cJSON_AddStringToObject(pResult, "handle", utf8Handle);
        data->resultJson = pResult;
        data->ret = 0;
    }
    pRecord->close();
    pDimStyleTable->close();

    actionEnd();
}

void ToolStyle::modify_dim_style(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        actionEnd();
        return;
    }

    TCHAR styleName[256] = {0};
    CUtils::utf8ToTChar(nameItem->valuestring, styleName, 256);

    AcDbDimStyleTable* pDimStyleTable = NULL;
    if (pDb->getDimStyleTable(pDimStyleTable, AcDb::kForRead) != Acad::eOk) {
        actionEnd();
        return;
    }

    AcDbDimStyleTableRecord* pRecord = NULL;
    if (pDimStyleTable->getAt(styleName, pRecord, AcDb::kForWrite) != Acad::eOk) {
        pDimStyleTable->close();
        actionEnd();
        return;
    }
    pDimStyleTable->close();

    // Modify arrow size
    cJSON* arrowItem = cJSON_GetObjectItem(data->jsonRoot, "arrow_size");
    if (arrowItem) {
        pRecord->setDimasz(arrowItem->valuedouble);
    }

    // Modify text height
    cJSON* textHeightItem = cJSON_GetObjectItem(data->jsonRoot, "text_height");
    if (textHeightItem) {
        pRecord->setDimtxt(textHeightItem->valuedouble);
    }

    // Modify dimension scale
    cJSON* scaleItem = cJSON_GetObjectItem(data->jsonRoot, "dim_scale");
    if (scaleItem) {
        pRecord->setDimscale(scaleItem->valuedouble);
    }

    // Modify decimal places
    cJSON* decItem = cJSON_GetObjectItem(data->jsonRoot, "decimal_places");
    if (decItem) {
        pRecord->setDimdec(decItem->valueint);
    }

    // Modify text color
    cJSON* textColorItem = cJSON_GetObjectItem(data->jsonRoot, "text_color");
    if (textColorItem) {
        AcCmColor color;
        color.setColorIndex(textColorItem->valueint);
        pRecord->setDimclrt(color);
    }

    // Modify dimension line color
    cJSON* dimColorItem = cJSON_GetObjectItem(data->jsonRoot, "dim_line_color");
    if (dimColorItem) {
        AcCmColor color;
        color.setColorIndex(dimColorItem->valueint);
        pRecord->setDimclrd(color);
    }

    // Modify extension line offset
    cJSON* extOffsetItem = cJSON_GetObjectItem(data->jsonRoot, "extension_line_offset");
    if (extOffsetItem) {
        pRecord->setDimexo(extOffsetItem->valuedouble);
    }

    // Modify extension line extend
    cJSON* extExtendItem = cJSON_GetObjectItem(data->jsonRoot, "extension_line_extend");
    if (extExtendItem) {
        pRecord->setDimexe(extExtendItem->valuedouble);
    }

    // Modify text gap
    cJSON* gapItem = cJSON_GetObjectItem(data->jsonRoot, "text_gap");
    if (gapItem) {
        pRecord->setDimgap(gapItem->valuedouble);
    }

    // Modify text style
    cJSON* textStyleItem = cJSON_GetObjectItem(data->jsonRoot, "text_style");
    if (textStyleItem && cJSON_IsString(textStyleItem)) {
        TCHAR textStyleName[256] = {0};
        CUtils::utf8ToTChar(textStyleItem->valuestring, textStyleName, 256);
        
        AcDbTextStyleTable* pTextStyleTable = NULL;
        if (pDb->getTextStyleTable(pTextStyleTable, AcDb::kForRead) == Acad::eOk) {
            AcDbTextStyleTableRecord* pTextStyleRec = NULL;
            if (pTextStyleTable->getAt(textStyleName, pTextStyleRec, AcDb::kForRead) == Acad::eOk) {
                pRecord->setDimtxsty(pTextStyleRec->objectId());
                pTextStyleRec->close();
            }
            pTextStyleTable->close();
        }
    }

    pRecord->close();
    data->ret = 0;
    actionEnd();
}

void ToolStyle::delete_dim_style(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    cJSON* nameItem = cJSON_GetObjectItem(data->jsonRoot, "name");
    if (!nameItem || !cJSON_IsString(nameItem)) {
        actionEnd();
        return;
    }

    TCHAR styleName[256] = {0};
    CUtils::utf8ToTChar(nameItem->valuestring, styleName, 256);

    // Cannot delete Standard style
    if (_tcsicmp(styleName, _T("Standard")) == 0) {
        actionEnd();
        return;
    }

    AcDbDimStyleTable* pDimStyleTable = NULL;
    if (pDb->getDimStyleTable(pDimStyleTable, AcDb::kForWrite) != Acad::eOk) {
        actionEnd();
        return;
    }

    AcDbDimStyleTableRecord* pRecord = NULL;
    if (pDimStyleTable->getAt(styleName, pRecord, AcDb::kForWrite) == Acad::eOk) {
        Acad::ErrorStatus es = pRecord->erase();
        pRecord->close();
        if (es == Acad::eOk) {
            data->ret = 0;
        }
    }
    pDimStyleTable->close();

    actionEnd();
}


// ==================== Text Style Query Operations ====================

void ToolStyle::list_text_styles(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    AcDbTextStyleTable* pTable = NULL;
    if (pDb->getTextStyleTable(pTable, AcDb::kForRead) != Acad::eOk) {
        actionEnd();
        return;
    }

    cJSON* pResultArray = cJSON_CreateArray();

    AcDbTextStyleTableIterator* pIter = NULL;
    if (pTable->newIterator(pIter) == Acad::eOk) {
        for (; !pIter->done(); pIter->step()) {
            AcDbTextStyleTableRecord* pRec = NULL;
            if (pIter->getRecord(pRec, AcDb::kForRead) == Acad::eOk) {
                ACHAR* pName = NULL;
                pRec->getName(pName);
                char utf8Name[256] = {0};
                if (pName) {
                    CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));
                    acutDelString(pName);
                }

                AcDbHandle handle = pRec->objectId().handle();
                TCHAR handleStr[20] = {0};
                handle.getIntoAsciiBuffer(handleStr);
                char utf8Handle[32] = {0};
                CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));

                cJSON* pJson = cJSON_CreateObject();
                cJSON_AddStringToObject(pJson, "name", utf8Name);
                cJSON_AddStringToObject(pJson, "handle", utf8Handle);
                cJSON_AddItemToArray(pResultArray, pJson);

                pRec->close();
            }
        }
        delete pIter;
    }
    pTable->close();

    data->resultJson = pResultArray;
    data->ret = 0;
    actionEnd();
}

void ToolStyle::get_text_styles(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    cJSON* namesArray = cJSON_GetObjectItem(data->jsonRoot, "names");
    if (!namesArray || !cJSON_IsArray(namesArray)) {
        actionEnd();
        return;
    }

    AcDbTextStyleTable* pTable = NULL;
    if (pDb->getTextStyleTable(pTable, AcDb::kForRead) != Acad::eOk) {
        actionEnd();
        return;
    }

    cJSON* pResultArray = cJSON_CreateArray();

    cJSON* nameItem = NULL;
    cJSON_ArrayForEach(nameItem, namesArray) {
        if (!cJSON_IsString(nameItem)) continue;

        TCHAR styleName[256] = {0};
        CUtils::utf8ToTChar(nameItem->valuestring, styleName, 256);

        AcDbTextStyleTableRecord* pRec = NULL;
        if (pTable->getAt(styleName, pRec, AcDb::kForRead) == Acad::eOk) {
            ACHAR* pFileName = NULL;
            pRec->fileName(pFileName);
            char utf8Font[256] = {0};
            if (pFileName) {
                CUtils::TCharToUtf8(pFileName, utf8Font, sizeof(utf8Font));
                acutDelString(pFileName);
            }

            Adesk::UInt8 flags = pRec->flagBits();

            AcDbHandle handle = pRec->objectId().handle();
            TCHAR handleStr[20] = {0};
            handle.getIntoAsciiBuffer(handleStr);
            char utf8Handle[32] = {0};
            CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));

            cJSON* pJson = cJSON_CreateObject();
            cJSON_AddStringToObject(pJson, "name", nameItem->valuestring);
            cJSON_AddStringToObject(pJson, "handle", utf8Handle);
            cJSON_AddStringToObject(pJson, "font", utf8Font);
            cJSON_AddNumberToObject(pJson, "height", pRec->textSize());
            cJSON_AddNumberToObject(pJson, "width_factor", pRec->xScale());
            cJSON_AddNumberToObject(pJson, "oblique_angle", pRec->obliquingAngle());
            cJSON_AddBoolToObject(pJson, "upside_down", (flags & 0x4) != 0);
            cJSON_AddBoolToObject(pJson, "backward", (flags & 0x2) != 0);

            cJSON_AddItemToArray(pResultArray, pJson);
            pRec->close();
        }
    }
    pTable->close();

    data->resultJson = pResultArray;
    data->ret = 0;
    actionEnd();
}

// ==================== Dimension Style Query Operations ====================

void ToolStyle::list_dim_styles(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    AcDbDimStyleTable* pTable = NULL;
    if (pDb->getDimStyleTable(pTable, AcDb::kForRead) != Acad::eOk) {
        actionEnd();
        return;
    }

    cJSON* pResultArray = cJSON_CreateArray();

    AcDbDimStyleTableIterator* pIter = NULL;
    if (pTable->newIterator(pIter) == Acad::eOk) {
        for (; !pIter->done(); pIter->step()) {
            AcDbDimStyleTableRecord* pRec = NULL;
            if (pIter->getRecord(pRec, AcDb::kForRead) == Acad::eOk) {
                ACHAR* pName = NULL;
                pRec->getName(pName);
                char utf8Name[256] = {0};
                if (pName) {
                    CUtils::TCharToUtf8(pName, utf8Name, sizeof(utf8Name));
                    acutDelString(pName);
                }

                AcDbHandle handle = pRec->objectId().handle();
                TCHAR handleStr[20] = {0};
                handle.getIntoAsciiBuffer(handleStr);
                char utf8Handle[32] = {0};
                CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));

                cJSON* pJson = cJSON_CreateObject();
                cJSON_AddStringToObject(pJson, "name", utf8Name);
                cJSON_AddStringToObject(pJson, "handle", utf8Handle);
                cJSON_AddItemToArray(pResultArray, pJson);

                pRec->close();
            }
        }
        delete pIter;
    }
    pTable->close();

    data->resultJson = pResultArray;
    data->ret = 0;
    actionEnd();
}

void ToolStyle::get_dim_styles(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    cJSON* namesArray = cJSON_GetObjectItem(data->jsonRoot, "names");
    if (!namesArray || !cJSON_IsArray(namesArray)) {
        actionEnd();
        return;
    }

    AcDbDimStyleTable* pTable = NULL;
    if (pDb->getDimStyleTable(pTable, AcDb::kForRead) != Acad::eOk) {
        actionEnd();
        return;
    }

    cJSON* pResultArray = cJSON_CreateArray();

    cJSON* nameItem = NULL;
    cJSON_ArrayForEach(nameItem, namesArray) {
        if (!cJSON_IsString(nameItem)) continue;

        TCHAR styleName[256] = {0};
        CUtils::utf8ToTChar(nameItem->valuestring, styleName, 256);

        AcDbDimStyleTableRecord* pRec = NULL;
        if (pTable->getAt(styleName, pRec, AcDb::kForRead) == Acad::eOk) {
            
            AcDbHandle handle = pRec->objectId().handle();
            TCHAR handleStr[20] = {0};
            handle.getIntoAsciiBuffer(handleStr);
            char utf8Handle[32] = {0};
            CUtils::TCharToUtf8(handleStr, utf8Handle, sizeof(utf8Handle));

            cJSON* pJson = cJSON_CreateObject();
            cJSON_AddStringToObject(pJson, "name", nameItem->valuestring);
            cJSON_AddStringToObject(pJson, "handle", utf8Handle);
            cJSON_AddNumberToObject(pJson, "arrow_size", pRec->dimasz());
            cJSON_AddNumberToObject(pJson, "text_height", pRec->dimtxt());
            cJSON_AddNumberToObject(pJson, "dim_scale", pRec->dimscale());
            cJSON_AddNumberToObject(pJson, "decimal_places", pRec->dimdec());
            cJSON_AddNumberToObject(pJson, "text_color", pRec->dimclrt().colorIndex());
            cJSON_AddNumberToObject(pJson, "dim_line_color", pRec->dimclrd().colorIndex());
            cJSON_AddNumberToObject(pJson, "extension_line_offset", pRec->dimexo());
            cJSON_AddNumberToObject(pJson, "extension_line_extend", pRec->dimexe());
            cJSON_AddNumberToObject(pJson, "text_gap", pRec->dimgap());

            cJSON_AddItemToArray(pResultArray, pJson);
            pRec->close();
        }
    }
    pTable->close();

    data->resultJson = pResultArray;
    data->ret = 0;
    actionEnd();
}