#include "stdafx.h"
#include "ToolDraw.h"
#include "cJSON.h"
#include "adscodes.h"
#include "dbents.h"

ToolDraw::ToolDraw(void):pDoc(NULL),pModelSpace(NULL)
{
}

ToolDraw::~ToolDraw(void)
{
}
ToolDraw& ToolDraw::getInstance()
{
    static ToolDraw instance;
    return instance;
}
int ToolDraw::actionBefore()
{
	this->pDoc = acDocManager->curDocument();

	Acad::ErrorStatus es = acDocManager->lockDocument(pDoc);
    if (es != Acad::eOk) {
        acutPrintf(_T("Failed to lock the document...\n"));
        return 1;
    }
	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) 
    {
        acutPrintf(_T("[ERROR] DrawRedCircle: Failed to get working database.\n"));
        return 2;
    }

	AcDbBlockTable* pBlockTable = NULL;
    this->pModelSpace = NULL;

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
int ToolDraw::actionEnd()
{
	this->pModelSpace->close();
	acDocManager->unlockDocument(this->pDoc);
	return 0;
}
void ToolDraw::create_line(cJSON* params)
{
    // 1. Get JSON items
    cJSON* x1Item = cJSON_GetObjectItem(params, "x1");
    cJSON* y1Item = cJSON_GetObjectItem(params, "y1");
    cJSON* x2Item = cJSON_GetObjectItem(params, "x2");
    cJSON* y2Item = cJSON_GetObjectItem(params, "y2");

    // 2. Validate and extract values
    if (!x1Item || !y1Item || !x2Item || !y2Item ||
        !cJSON_IsNumber(x1Item) || !cJSON_IsNumber(y1Item) ||
        !cJSON_IsNumber(x2Item) || !cJSON_IsNumber(y2Item)) {
        // Handle error: invalid parameters
        return;
    }

    double x1 = x1Item->valuedouble;
    double y1 = y1Item->valuedouble;
    double x2 = x2Item->valuedouble;
    double y2 = y2Item->valuedouble;

    // 3. Format coordinates into strings
    // Using a buffer size large enough to hold the coordinate string
    TCHAR pt1Str[64];
    TCHAR pt2Str[64];

    // Format as "x,y"
    _stprintf_s(pt1Str, _T("%f,%f"), x1, y1);
    _stprintf_s(pt2Str, _T("%f,%f"), x2, y2);

	int ret = this->actionBefore();
	if (ret) return;

	AcGePoint3d pt_start(x1,y1,0);
	AcGePoint3d pt_end(x2,y2,0);

	AcDbLine* pLine = new AcDbLine(pt_start, pt_end);

	AcDbObjectId circleId;
    if (pModelSpace->appendAcDbEntity(circleId, pLine) == Acad::eOk)
    {
        acutPrintf(_T("[INFO] create_line: created successfully.\n"));
        pLine->close();
    }
    else
    {
        acutPrintf(_T("[ERROR] create_line: Failed to append Model Space.\n"));
        delete pLine;
    }

	this->actionEnd();

	/*
	pLine->close();*/

    // 4. Execute the LINE command
    //int result = acedCommand(
    //    RTSTR, _T("_LINE"),      // Start LINE command
    //    RTSTR, pt1Str,           // First point
    //    RTSTR, pt2Str,           // Second point
    //    RTSTR, _T(""),           // End point (empty string to finish)
    //    RTNONE                   // End of command sequence
    //);
    //acutPrintf(_T("create_line Result: %d\n"), result);

	
}
