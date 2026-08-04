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


ToolDraw::ToolDraw(void):ToolDrawBase()
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
