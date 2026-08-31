#include "stdafx.h"
#include "ToolDraw.h"
#include "cJSON.h"
#include "arxHeaders.h"
#include "acadi.h"
#include "utils.h"


VOID CALLBACK UpdateDisplayTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    KillTimer(NULL, idEvent);
    acedUpdateDisplay();
}
ToolDrawBase::ToolDrawBase(void):pDoc(NULL),pModelSpace(NULL),pDb(NULL)
{
}

ToolDrawBase::~ToolDrawBase(void)
{
}

void ToolDrawBase::regen()
{
	//acutPrintf(_T("regen\n"));
	//AcApDocument* doc = curDoc();
	//doc->regen(ACAD::kActiveViewport);
	acDocManager->sendStringToExecute(
    pDoc,
    _T("_.REGEN "),
    TRUE,
    FALSE,
    FALSE);

	

	//actrTransactionManager->queueForGraphicsFlush();
	//actrTransactionManager->flushGraphics();

	//acedRedraw(NULL, 1);
	//acedUpdateDisplay();

    SetTimer(NULL, 1, 2000, UpdateDisplayTimerProc);
}
int ToolDrawBase::actionBefore()
{
	this->pDoc = acDocManager->curDocument();

	Acad::ErrorStatus es = acDocManager->lockDocument(pDoc);
    if (es != Acad::eOk) {
        acutPrintf(_T("Failed to lock the document...\n"));
        return 1;
    }
	this->pDb = acdbHostApplicationServices()->workingDatabase();
    if (!this->pDb) 
    {
        acutPrintf(_T("[ERROR] DrawRedCircle: Failed to get working database.\n"));
        return 2;
    }

	AcDbBlockTable* pBlockTable = NULL;
    //this->pModelSpace = NULL;

    // Open Model Space for write
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
    {
        acutPrintf(_T("[ERROR] DrawRedCircle: Failed to open Block Table.\n"));
        return 3;
    }
    
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, this->pModelSpace, AcDb::kForWrite) != Acad::eOk)
    {
        acutPrintf(_T("[ERROR] DrawRedCircle: Failed to open Model Space for write.\n"));
        pBlockTable->close();
        return 4;
    }
    pBlockTable->close();

	return 0;
}
int ToolDrawBase::actionEnd()
{
	this->pModelSpace->close();
	acDocManager->unlockDocument(this->pDoc);
	return 0;
}

void ToolDrawBase::appendHandleToResult(AcDbObjectId entityId, AiToolCommandData* data)
{
    
    AcDbHandle dbHandle = entityId.handle();
    TCHAR handleStr[32] = { 0 };
	dbHandle.getIntoAsciiBuffer(handleStr);

    char utf8Handle[32] = { 0 };
#ifdef _UNICODE
    WideCharToMultiByte(CP_UTF8, 0, handleStr, -1, utf8Handle, sizeof(utf8Handle), NULL, NULL);
#else

#ifdef _CAD2005
	strcpy(utf8Handle, handleStr);
#else
    strcpy_s(utf8Handle, handleStr);
#endif

#endif

    if (data->resultJson == NULL) {
        data->resultJson = cJSON_CreateObject();
        cJSON_AddStringToObject(data->resultJson, "entityID", utf8Handle);
    } else if (cJSON_IsArray(data->resultJson)) {
        cJSON_AddItemToArray(data->resultJson, cJSON_CreateString(utf8Handle));
    }
    data->ret = 0;
}
AcDbObjectId ToolDrawBase::GetIdFromHandle(cJSON* json, const char* key, AcDbDatabase* pDb) {
    cJSON* item = cJSON_GetObjectItem(json, key);
    if (!item || !cJSON_IsString(item)) return AcDbObjectId::kNull;
    TCHAR handleStr[32] = { 0 };
    CUtils::utf8ToTChar(item->valuestring, handleStr, 32);
    AcDbHandle dbHandle(handleStr);
    AcDbObjectId objId;
    pDb->getAcDbObjectId(objId, Adesk::kFalse, dbHandle);
    return objId;
}
