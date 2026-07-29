#include "stdafx.h"
#include "ToolDraw.h"
#include "cJSON.h"
#include "adscodes.h"
#include "actrans.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "dbelipse.h"
#include "dbspline.h"

#include "utils.h"


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

void ToolDraw::appendHandleToResult(AcDbObjectId entityId, AiToolCommandData* data)
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


void ToolDraw::create_line(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

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

	AcDbObjectId lineId;
    if (pModelSpace->appendAcDbEntity(lineId, pLine) == Acad::eOk)
    {
        //acutPrintf(_T("[INFO] create_line: created successfully.\n"));
        pLine->close();

        appendHandleToResult(lineId, data);
        
    }
    else
    {
        acutPrintf(_T("[ERROR] create_line: Failed to append Model Space.\n"));
        delete pLine;
        data->ret = 1;
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
void ToolDraw::create_circle(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

    cJSON* xItem = cJSON_GetObjectItem(params, "x");
    cJSON* yItem = cJSON_GetObjectItem(params, "y");
    cJSON* rItem = cJSON_GetObjectItem(params, "radius");

    if (!xItem || !yItem || !rItem ||
        !cJSON_IsNumber(xItem) || !cJSON_IsNumber(yItem) ||
        !cJSON_IsNumber(rItem))
    {
        acutPrintf(_T("[ERROR] create_circle: invalid parameters.\n"));
        return;
    }

    double x = xItem->valuedouble;
    double y = yItem->valuedouble;
    double radius = rItem->valuedouble;
    if (radius <= 0.0)
    {
        acutPrintf(_T("[ERROR] create_circle: radius must be positive.\n"));
        return;
    }

    if (this->actionBefore()) return;

    AcGePoint3d center(x, y, 0.0);
    AcDbCircle* pCircle = new AcDbCircle(center, AcGeVector3d::kZAxis, radius);

    AcDbObjectId circleId;
    if (pModelSpace->appendAcDbEntity(circleId, pCircle) == Acad::eOk)
    {
        //acutPrintf(_T("[INFO] create_circle: created successfully.\n"));
        pCircle->close();

        appendHandleToResult(circleId, data);
    }
    else
    {
        acutPrintf(_T("[ERROR] create_circle: failed to append to Model Space.\n"));
        delete pCircle;
		data->ret = 1;
    }

    this->actionEnd();
}
void ToolDraw::create_rectangle(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

    cJSON* x1Item = cJSON_GetObjectItem(params, "x1");
    cJSON* y1Item = cJSON_GetObjectItem(params, "y1");
    cJSON* widthItem = cJSON_GetObjectItem(params, "width");
    cJSON* heightItem = cJSON_GetObjectItem(params, "height");

    if (!x1Item || !y1Item || !widthItem || !heightItem ||
        !cJSON_IsNumber(x1Item) || !cJSON_IsNumber(y1Item) ||
        !cJSON_IsNumber(widthItem) || !cJSON_IsNumber(heightItem))
    {
        acutPrintf(_T("[ERROR] create_rectangle: invalid parameters.\n"));
        return;
    }

    double x1 = x1Item->valuedouble;
    double y1 = y1Item->valuedouble;
    double width = widthItem->valuedouble;
    double height = heightItem->valuedouble;

    if (this->actionBefore()) return;


    AcGePoint2d pts[4] =
    {
        AcGePoint2d(x1,y1),
        AcGePoint2d(x1+width,y1),
        AcGePoint2d(x1+width,y1+height),
        AcGePoint2d(x1,y1+height),
    };

	AcDbPolyline* pPoly = new AcDbPolyline();

	for (int i = 0; i < 4; i++)
    {
		pPoly->addVertexAt(i, pts[i]);
	}

	pPoly->setClosed(Adesk::kTrue);

	
	AcDbObjectId rectId;
	if (pModelSpace->appendAcDbEntity(rectId, pPoly) == Acad::eOk)
    {
        pPoly->close();
        appendHandleToResult(rectId, data);
    }
    else
    {
        acutPrintf(_T("[ERROR] create_rectangle: failed to append poly \n"));
        delete pPoly;
        data->ret = 1;
    }

    this->actionEnd();
}

void ToolDraw::create_text(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

    cJSON* xItem = cJSON_GetObjectItem(params, "x");
    cJSON* yItem = cJSON_GetObjectItem(params, "y");
    cJSON* heightItem = cJSON_GetObjectItem(params, "height");
    cJSON* textItem = cJSON_GetObjectItem(params, "text");

    if (!xItem || !yItem || !textItem ||
        !cJSON_IsNumber(xItem) || !cJSON_IsNumber(yItem) ||
        !cJSON_IsString(textItem))
    {
        acutPrintf(_T("[ERROR] create_text: invalid parameters.\n"));
        return;
    }

    double x = xItem->valuedouble;
    double y = yItem->valuedouble;
    double height = 1.0;
    if (heightItem && cJSON_IsNumber(heightItem))
        height = heightItem->valuedouble;

    TCHAR textStr[512] = { 0 };
	if (!CUtils::utf8ToTChar(textItem->valuestring, textStr, ARRAYSIZE(textStr)))
    {
        acutPrintf(_T("[ERROR] create_text: failed to convert text string.\n"));
        return;
    }

    if (this->actionBefore()) return;

    AcDbText* pText = new AcDbText(AcGePoint3d(x, y, 0.0), textStr);
    AcDbObjectId textId;
    if (pModelSpace->appendAcDbEntity(textId, pText) == Acad::eOk)
    {
        //acutPrintf(_T("[INFO] create_text: created successfully.\n"));
        pText->close();

        appendHandleToResult(textId, data);
    }
    else
    {
        acutPrintf(_T("[ERROR] create_text: failed to append to Model Space.\n"));
        delete pText;
		data->ret = 1;
    }

    this->actionEnd();
}

void ToolDraw::create_layer(AiToolCommandData* data)
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

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    AcDbLayerTable* pLayerTable = NULL;
    if (pDb->getLayerTable(pLayerTable, AcDb::kForWrite) != Acad::eOk)
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
    }
    else
    {
        acutPrintf(_T("[ERROR] create_layer: failed to add layer record.\n"));
        delete pLayerRecord;
		data->ret = 1;
    }

    pLayerTable->close();
    this->actionEnd();
}

void ToolDraw::list_layers(AiToolCommandData* data)
{
	// cJSON* params = data->jsonRoot;
    if (this->actionBefore()) return;

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    AcDbLayerTable* pLayerTable = NULL;
    if (pDb->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk)
    {
        acutPrintf(_T("[ERROR] list_layers: failed to open Layer Table.\n"));
        this->actionEnd();
		data->ret = 1;
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
    }
    else
    {
		data->ret = 1;
        acutPrintf(_T("[ERROR] list_layers: failed to create Layer Table iterator.\n"));
    }

    pLayerTable->close();
    this->actionEnd();
}

void ToolDraw::regen()
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
void ToolDraw::create_polyline(AiToolCommandData* data)
{
    cJSON* params = data->jsonRoot;
    cJSON* pointsItem = cJSON_GetObjectItem(params, "points");

    if (!pointsItem || !cJSON_IsArray(pointsItem)) {
        acutPrintf(_T("[ERROR] create_polyline: invalid or missing 'points' array.\n"));
        data->ret = 1;
        return;
    }

    int numPoints = cJSON_GetArraySize(pointsItem);
    if (numPoints < 2) {
        acutPrintf(_T("[ERROR] create_polyline: at least 2 points are required.\n"));
        data->ret = 1;
        return;
    }

    if (this->actionBefore()) return;

    AcDbPolyline* pPoly = new AcDbPolyline();
    bool valid = true;

    for (int i = 0; i < numPoints; ++i) {
        cJSON* pt = cJSON_GetArrayItem(pointsItem, i);
        cJSON* xItem = cJSON_GetObjectItem(pt, "x");
        cJSON* yItem = cJSON_GetObjectItem(pt, "y");

        if (xItem && cJSON_IsNumber(xItem) && yItem && cJSON_IsNumber(yItem)) {
            pPoly->addVertexAt(i, AcGePoint2d(xItem->valuedouble, yItem->valuedouble));
        } else {
            acutPrintf(_T("[ERROR] create_polyline: invalid point data at index %d.\n"), i);
            valid = false;
            break;
        }
    }

    cJSON* closedItem = cJSON_GetObjectItem(params, "closed");
    if (closedItem && cJSON_IsTrue(closedItem)) {
        pPoly->setClosed(Adesk::kTrue);
    }

    AcDbObjectId polyId;
    if (valid && pModelSpace->appendAcDbEntity(polyId, pPoly) == Acad::eOk) {
        pPoly->close();
        appendHandleToResult(polyId, data);
    } else {
        delete pPoly;
        data->ret = 1;
    }

    this->actionEnd();
}

void ToolDraw::create_ellipse(AiToolCommandData* data)
{
    cJSON* params = data->jsonRoot;

    cJSON* xItem = cJSON_GetObjectItem(params, "x");
    cJSON* yItem = cJSON_GetObjectItem(params, "y");
    cJSON* majorXItem = cJSON_GetObjectItem(params, "major_x");
    cJSON* majorYItem = cJSON_GetObjectItem(params, "major_y");
    cJSON* ratioItem = cJSON_GetObjectItem(params, "ratio");

    if (!xItem || !yItem || !majorXItem || !majorYItem || !ratioItem ||
        !cJSON_IsNumber(xItem) || !cJSON_IsNumber(yItem) ||
        !cJSON_IsNumber(majorXItem) || !cJSON_IsNumber(majorYItem) ||
        !cJSON_IsNumber(ratioItem)) 
    {
        acutPrintf(_T("[ERROR] create_ellipse: invalid parameters.\n"));
        data->ret = 1;
        return;
    }

    double ratio = ratioItem->valuedouble;
    if (ratio <= 0.0 || ratio > 1.0) {
        acutPrintf(_T("[ERROR] create_ellipse: radius ratio must be > 0 and <= 1.\n"));
        data->ret = 1;
        return;
    }

    if (this->actionBefore()) return;

    AcGePoint3d center(xItem->valuedouble, yItem->valuedouble, 0.0);
    // majorAxis 向量的长度代表了椭圆长轴的半径
    AcGeVector3d majorAxis(majorXItem->valuedouble, majorYItem->valuedouble, 0.0);

    if (majorAxis.length() <= 0.0) {
        acutPrintf(_T("[ERROR] create_ellipse: major axis length must be > 0.\n"));
        this->actionEnd();
        data->ret = 1;
        return;
    }

    AcDbEllipse* pEllipse = new AcDbEllipse(center, AcGeVector3d::kZAxis, majorAxis, ratio);

    AcDbObjectId ellipseId;
    if (pModelSpace->appendAcDbEntity(ellipseId, pEllipse) == Acad::eOk) {
        pEllipse->close();
        appendHandleToResult(ellipseId, data);
    } else {
        delete pEllipse;
        data->ret = 1;
    }

    this->actionEnd();
}

void ToolDraw::create_spline(AiToolCommandData* data)
{
    cJSON* params = data->jsonRoot;
    cJSON* pointsItem = cJSON_GetObjectItem(params, "points");

    if (!pointsItem || !cJSON_IsArray(pointsItem)) {
        acutPrintf(_T("[ERROR] create_spline: invalid or missing 'points' array.\n"));
        data->ret = 1;
        return;
    }

    int numPoints = cJSON_GetArraySize(pointsItem);
    if (numPoints < 3) {
        acutPrintf(_T("[ERROR] create_spline: at least 3 points are required for a spline.\n"));
        data->ret = 1;
        return;
    }

    if (this->actionBefore()) return;

    AcGePoint3dArray fitPoints;
    bool valid = true;

    for (int i = 0; i < numPoints; ++i) {
        cJSON* pt = cJSON_GetArrayItem(pointsItem, i);
        cJSON* xItem = cJSON_GetObjectItem(pt, "x");
        cJSON* yItem = cJSON_GetObjectItem(pt, "y");

        if (xItem && cJSON_IsNumber(xItem) && yItem && cJSON_IsNumber(yItem)) {
            fitPoints.append(AcGePoint3d(xItem->valuedouble, yItem->valuedouble, 0.0));
        } else {
            acutPrintf(_T("[ERROR] create_spline: invalid point data at index %d.\n"), i);
            valid = false;
            break;
        }
    }

    AcDbObjectId splineId;
    if (valid) {
        // 使用 AcGePoint3dArray 拟合点构造函数，兼容 AutoCAD 2007
        AcDbSpline* pSpline = new AcDbSpline(fitPoints);

        if (pModelSpace->appendAcDbEntity(splineId, pSpline) == Acad::eOk) {
            pSpline->close();
            appendHandleToResult(splineId, data);
        } else {
            delete pSpline;
            data->ret = 1;
        }
    } else {
        data->ret = 1;
    }

    this->actionEnd();
}