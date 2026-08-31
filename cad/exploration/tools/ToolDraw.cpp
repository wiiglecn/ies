#include "stdafx.h"
#include "ToolDraw.h"
#include "cJSON.h"
#include "arxHeaders.h"
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
    cJSON* widthItem = cJSON_GetObjectItem(params, "width");

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

    double lineWidth = 0.0;
    if (widthItem && cJSON_IsNumber(widthItem))
        lineWidth = widthItem->valuedouble;

    // 3. Format coordinates into strings
    // Using a buffer size large enough to hold the coordinate string
    TCHAR pt1Str[64];
    TCHAR pt2Str[64];

    // Format as "x,y"
#ifdef _CAD2005
	_stprintf(pt1Str, _T("%f,%f"), x1, y1);
	_stprintf(pt2Str, _T("%f,%f"), x2, y2);
#else
    _stprintf_s(pt1Str, _T("%f,%f"), x1, y1);
    _stprintf_s(pt2Str, _T("%f,%f"), x2, y2);
#endif


	int ret = this->actionBefore();
	if (ret) return;

	AcGePoint3d pt_start(x1,y1,0);
	AcGePoint3d pt_end(x2,y2,0);

	AcDbLine* pLine = new AcDbLine(pt_start, pt_end);
    if (lineWidth > 0.0)
    {
        int lw = (int)lineWidth;
        if (lw < 0) lw = 0;
        if (lw > 211) lw = 211;
        pLine->setLineWeight(static_cast<AcDb::LineWeight>(lw));
    }

	AcDbObjectId lineId;
    if (pModelSpace->appendAcDbEntity(lineId, pLine) == Acad::eOk)
    {
        //acutPrintf(_T("[INFO] create_line: created successfully.\n"));
        pLine->close();

        appendHandleToResult(lineId, data);
        
        data->ret = 0;
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
        
        data->ret = 0;
    }
    else
    {
        acutPrintf(_T("[ERROR] create_circle: failed to append to Model Space.\n"));
        delete pCircle;
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
    cJSON* lineWidthItem = cJSON_GetObjectItem(params, "line_width");

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

    double lineWidth = 0.0;
    if (lineWidthItem && cJSON_IsNumber(lineWidthItem))
        lineWidth = lineWidthItem->valuedouble;

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

    if (lineWidth > 0.0)
        pPoly->setConstantWidth(lineWidth);

	
	AcDbObjectId rectId;
	if (pModelSpace->appendAcDbEntity(rectId, pPoly) == Acad::eOk)
    {
        pPoly->close();
        appendHandleToResult(rectId, data);
        
		data->ret = 0;
    }
    else
    {
        acutPrintf(_T("[ERROR] create_rectangle: failed to append poly \n"));
        delete pPoly;
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
    cJSON* rotationItem = cJSON_GetObjectItem(params, "rotation");

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

    double rotation = 0.0;
    if (rotationItem && cJSON_IsNumber(rotationItem))
        rotation = rotationItem->valuedouble;

    TCHAR textStr[512] = { 0 };
	if (!CUtils::utf8ToTChar(textItem->valuestring, textStr, ARRAYSIZE(textStr)))
    {
        acutPrintf(_T("[ERROR] create_text: failed to convert text string.\n"));
        return;
    }

    if (this->actionBefore()) return;

    AcDbText* pText = new AcDbText(AcGePoint3d(x, y, 0.0), textStr);
    if (rotation != 0.0)
        pText->setRotation(rotation);
    AcDbObjectId textId;
    if (pModelSpace->appendAcDbEntity(textId, pText) == Acad::eOk)
    {
        //acutPrintf(_T("[INFO] create_text: created successfully.\n"));
        pText->close();

        appendHandleToResult(textId, data);
        
        data->ret = 0;
    }
    else
    {
        acutPrintf(_T("[ERROR] create_text: failed to append to Model Space.\n"));
        delete pText;
    }

    this->actionEnd();
}

void ToolDraw::create_polyline(AiToolCommandData* data)
{
    cJSON* params = data->jsonRoot;
    cJSON* pointsItem = cJSON_GetObjectItem(params, "points");
    cJSON* widthItem = cJSON_GetObjectItem(params, "width");

    if (!pointsItem || !cJSON_IsArray(pointsItem)) {
        acutPrintf(_T("[ERROR] create_polyline: invalid or missing 'points' array.\n"));
        return;
    }

    int numPoints = cJSON_GetArraySize(pointsItem);
    if (numPoints < 2) {
        acutPrintf(_T("[ERROR] create_polyline: at least 2 points are required.\n"));
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

    double lineWidth = 0.0;
    if (widthItem && cJSON_IsNumber(widthItem))
        lineWidth = widthItem->valuedouble;

    if (lineWidth > 0.0)
        pPoly->setConstantWidth(lineWidth);

    AcDbObjectId polyId;
    if (valid && pModelSpace->appendAcDbEntity(polyId, pPoly) == Acad::eOk) {
        pPoly->close();
        appendHandleToResult(polyId, data);
        
        data->ret = 0;
    } else {
        delete pPoly;
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
        return;
    }

    double ratio = ratioItem->valuedouble;
    if (ratio <= 0.0 || ratio > 1.0) {
        acutPrintf(_T("[ERROR] create_ellipse: radius ratio must be > 0 and <= 1.\n"));
        return;
    }

    if (this->actionBefore()) return;

    AcGePoint3d center(xItem->valuedouble, yItem->valuedouble, 0.0);
    // majorAxis 向量的长度代表了椭圆长轴的半径
    AcGeVector3d majorAxis(majorXItem->valuedouble, majorYItem->valuedouble, 0.0);

    if (majorAxis.length() <= 0.0) {
        acutPrintf(_T("[ERROR] create_ellipse: major axis length must be > 0.\n"));
        this->actionEnd();
        return;
    }

    AcDbEllipse* pEllipse = new AcDbEllipse(center, AcGeVector3d::kZAxis, majorAxis, ratio);

    AcDbObjectId ellipseId;
    if (pModelSpace->appendAcDbEntity(ellipseId, pEllipse) == Acad::eOk) {
        pEllipse->close();
        appendHandleToResult(ellipseId, data);
        data->ret = 0;
    } else {
        delete pEllipse;
    }

    this->actionEnd();
}

void ToolDraw::create_spline(AiToolCommandData* data)
{
    cJSON* params = data->jsonRoot;
    cJSON* pointsItem = cJSON_GetObjectItem(params, "points");

    if (!pointsItem || !cJSON_IsArray(pointsItem)) {
        acutPrintf(_T("[ERROR] create_spline: invalid or missing 'points' array.\n"));
        return;
    }

    int numPoints = cJSON_GetArraySize(pointsItem);
    if (numPoints < 3) {
        acutPrintf(_T("[ERROR] create_spline: at least 3 points are required for a spline.\n"));
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
            data->ret = 0;
        } else {
            delete pSpline;
        }
    }

    this->actionEnd();
}

void ToolDraw::create_arc(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

    cJSON* xItem = cJSON_GetObjectItem(params, "x");
    cJSON* yItem = cJSON_GetObjectItem(params, "y");
    cJSON* radiusItem = cJSON_GetObjectItem(params, "radius");
    cJSON* startAngleItem = cJSON_GetObjectItem(params, "start_angle");
    cJSON* endAngleItem = cJSON_GetObjectItem(params, "end_angle");

    if (!xItem || !yItem || !radiusItem || !startAngleItem || !endAngleItem ||
        !cJSON_IsNumber(xItem) || !cJSON_IsNumber(yItem) || !cJSON_IsNumber(radiusItem) ||
        !cJSON_IsNumber(startAngleItem) || !cJSON_IsNumber(endAngleItem))
    {
        acutPrintf(_T("[ERROR] create_arc: invalid parameters.\n"));
        return;
    }

    double x = xItem->valuedouble;
    double y = yItem->valuedouble;
    double radius = radiusItem->valuedouble;
    double startAngle = startAngleItem->valuedouble;
    double endAngle = endAngleItem->valuedouble;

    if (radius <= 0.0) {
        acutPrintf(_T("[ERROR] create_arc: radius must be positive.\n"));
        return;
    }

    if (this->actionBefore()) return;

    AcGePoint3d center(x, y, 0.0);
    AcDbArc* pArc = new AcDbArc(center, AcGeVector3d::kZAxis, radius, startAngle, endAngle);

    AcDbObjectId arcId;
    if (pModelSpace->appendAcDbEntity(arcId, pArc) == Acad::eOk) {
        pArc->close();
        appendHandleToResult(arcId, data);
        data->ret = 0;
    } else {
        delete pArc;
    }

    this->actionEnd();
}

void ToolDraw::create_mtext(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

    cJSON* xItem = cJSON_GetObjectItem(params, "x");
    cJSON* yItem = cJSON_GetObjectItem(params, "y");
    cJSON* textItem = cJSON_GetObjectItem(params, "text");
    cJSON* heightItem = cJSON_GetObjectItem(params, "height");
    cJSON* widthItem = cJSON_GetObjectItem(params, "width");

    if (!xItem || !yItem || !textItem ||
        !cJSON_IsNumber(xItem) || !cJSON_IsNumber(yItem) || !cJSON_IsString(textItem))
    {
        acutPrintf(_T("[ERROR] create_mtext: invalid parameters.\n"));
        return;
    }

    double x = xItem->valuedouble;
    double y = yItem->valuedouble;
    double height = 1.0;
    double width = 100.0; // Default width

    if (heightItem && cJSON_IsNumber(heightItem)) height = heightItem->valuedouble;
    if (widthItem && cJSON_IsNumber(widthItem)) width = widthItem->valuedouble;

    TCHAR textStr[1024] = {0};
    if (!CUtils::utf8ToTChar(textItem->valuestring, textStr, ARRAYSIZE(textStr))) {
        acutPrintf(_T("[ERROR] create_mtext: failed to convert text string.\n"));
        return;
    }

    if (this->actionBefore()) return;

    AcDbMText* pMText = new AcDbMText();
    pMText->setLocation(AcGePoint3d(x, y, 0.0));
    pMText->setTextHeight(height);
    pMText->setWidth(width);
    pMText->setContents(textStr);

    AcDbObjectId mtextId;
    if (pModelSpace->appendAcDbEntity(mtextId, pMText) == Acad::eOk) {
        pMText->close();
        appendHandleToResult(mtextId, data);
        
        data->ret = 0;
    } else {
        delete pMText;
    }

    this->actionEnd();
}

void ToolDraw::create_hatch(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;
    cJSON* pointsItem = cJSON_GetObjectItem(params, "points");
    cJSON* patternItem = cJSON_GetObjectItem(params, "pattern");
    cJSON* scaleItem = cJSON_GetObjectItem(params, "scale");

    if (!pointsItem || !cJSON_IsArray(pointsItem)) {
        acutPrintf(_T("[ERROR] create_hatch: invalid or missing 'points' array.\n"));
        return;
    }

    int numPoints = cJSON_GetArraySize(pointsItem);
    if (numPoints < 2) {
        acutPrintf(_T("[ERROR] create_hatch: at least 2 points are required.\n"));
        return;
    }

	if (this->actionBefore()) {
		return;
	}

    AcGeVoidPointerArray edgeArray;
    AcGeIntArray edgeTypes; // Added edge types array
    for (int i = 0; i < numPoints; ++i) {
        cJSON* pt1 = cJSON_GetArrayItem(pointsItem, i);
        cJSON* pt2 = cJSON_GetArrayItem(pointsItem, (i + 1) % numPoints);
        
        cJSON* x1Item = cJSON_GetObjectItem(pt1, "x");
        cJSON* y1Item = cJSON_GetObjectItem(pt1, "y");
        cJSON* x2Item = cJSON_GetObjectItem(pt2, "x");
        cJSON* y2Item = cJSON_GetObjectItem(pt2, "y");

        if(x1Item && y1Item && x2Item && y2Item && 
           cJSON_IsNumber(x1Item) && cJSON_IsNumber(y1Item) && 
           cJSON_IsNumber(x2Item) && cJSON_IsNumber(y2Item)) {
            AcGePoint3d p1(x1Item->valuedouble, y1Item->valuedouble, 0.0);
            AcGePoint3d p2(x2Item->valuedouble, y2Item->valuedouble, 0.0);
            AcGeLineSeg3d* seg = new AcGeLineSeg3d(p1, p2);
            edgeArray.append(seg);
            edgeTypes.append(AcDbHatch::kLine); // Append line edge type
        }
    }

    AcDbHatch* pHatch = new AcDbHatch();
    pHatch->setNormal(AcGeVector3d::kZAxis);
    pHatch->setElevation(0.0);
    pHatch->setAssociative(Adesk::kFalse);
    
    TCHAR patternStr[256] = {0};
    if (patternItem && cJSON_IsString(patternItem)) {
        CUtils::utf8ToTChar(patternItem->valuestring, patternStr, ARRAYSIZE(patternStr));
    } else {
#ifdef _CAD2005
		_tcscpy(patternStr, _T("SOLID"));
#else
        _tcscpy_s(patternStr, _T("SOLID"));
#endif
    }
    pHatch->setPattern(AcDbHatch::kPreDefined, patternStr);
    
    double scale = 1.0;
    if (scaleItem && cJSON_IsNumber(scaleItem)) {
        scale = scaleItem->valuedouble;
    }
    pHatch->setPatternScale(scale);

    pHatch->appendLoop(AcDbHatch::kDefault, edgeArray, edgeTypes); // Passed edge types
    pHatch->evaluateHatch();

    AcDbObjectId hatchId;
    if (pModelSpace->appendAcDbEntity(hatchId, pHatch) == Acad::eOk) {
        pHatch->close();
        appendHandleToResult(hatchId, data);
        
		data->ret = 0;
    } else {
        delete pHatch;
    }

    // Cleanup edges
    for(int i = 0; i < edgeArray.length(); i++) {
        delete (AcGeLineSeg3d*)edgeArray[i];
    }

    this->actionEnd();
}

void ToolDraw::Init()
{
	//CString strFilePath = CUtils::GetArxFolder();
 //   // strFilePath.Replace(_T("\\"), _T("/"));
 //   CString strUrl;
 //   strUrl.Format(_T("%stools.lsp"), strFilePath);

	//struct resbuf* rbArgs = acutBuildList(RTSTR, _T("load"), RTSTR, (LPCTSTR)strUrl, RTNONE);
 //   struct resbuf* rbResult = NULL;
 //   int rc = acedInvoke(rbArgs, &rbResult);
 //   acutRelRb(rbArgs);
 //   if (rbResult) acutRelRb(rbResult);

	//acutPrintf(_T("\nLSP Load Status: %s\n"), (rc == RTNORM) ? _T("Success") : _T("Failed"));
	
}

