#include "stdafx.h"
#include "ToolDraw.h"
#include "cJSON.h"
#include "adscodes.h"
#include "actrans.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "dbelipse.h"
#include "dbspline.h"
#include "acedads.h"

#include "utils.h"

ToolDrawBase::ToolDrawBase(void):pDoc(NULL),pModelSpace(NULL),pDb(NULL)
{
}

ToolDrawBase::~ToolDrawBase(void)
{
}

void ToolDrawBase::regen()
{
	acutPrintf(_T("regen\n"));
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
    // 获取实体的句柄，这是保存在磁盘上的唯一永久标识
    AcDbHandle dbHandle = entityId.handle();
    TCHAR handleStr[32] = { 0 };
	dbHandle.getIntoAsciiBuffer(handleStr);

    // 转换为 UTF-8 以便存入 cJSON
    char utf8Handle[32] = { 0 };
#ifdef _UNICODE
    WideCharToMultiByte(CP_UTF8, 0, handleStr, -1, utf8Handle, sizeof(utf8Handle), NULL, NULL);
#else
    strcpy_s(utf8Handle, handleStr);
#endif

    // 构建 cJSON 返回值
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

void ToolDrawBase::dispatch_command(AiToolCommandData* data){
	/*CAcModuleResourceOverride resOverride;
	actionBefore();
	int ret = acedCommand(RTSTR, _T("_LINE"), RTSTR, _T("0,0"), RTSTR, _T("100,100"), RTNONE);
	actionEnd();
	acutPrintf(_T("dispatch_command ref :%d \n"),ret);*/
	data->ret = 0;


	::PostMessage(
            acedGetAcadFrame()->GetSafeHwnd(), 
            WM_USER_TOOL_TEST, 
            0,                          // WPARAM 未使用，传 0
            0             // LPARAM 传递动态指针
        );
	
}