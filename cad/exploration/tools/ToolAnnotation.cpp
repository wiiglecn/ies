#include "stdafx.h"
#include "ToolAnnotation.h"
#include "cJSON.h"
#include "arxHeaders.h"
#include "utils.h"



ToolAnnotation::ToolAnnotation(void):ToolDrawBase()
{
}

ToolAnnotation::~ToolAnnotation(void)
{
}
ToolAnnotation& ToolAnnotation::getInstance()
{
    static ToolAnnotation instance;
    return instance;
}

void ToolAnnotation::create_dimension_linear(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

	cJSON* x1Item = cJSON_GetObjectItem(params, "x1");
	cJSON* y1Item = cJSON_GetObjectItem(params, "y1");
	cJSON* x2Item = cJSON_GetObjectItem(params, "x2");
	cJSON* y2Item = cJSON_GetObjectItem(params, "y2");
	cJSON* dimXItem = cJSON_GetObjectItem(params, "dim_x");
	cJSON* dimYItem = cJSON_GetObjectItem(params, "dim_y");
	cJSON* rotationItem = cJSON_GetObjectItem(params, "rotation");

	if (!x1Item || !y1Item || !x2Item || !y2Item || !dimXItem || !dimYItem ||
		!cJSON_IsNumber(x1Item) || !cJSON_IsNumber(y1Item) ||
		!cJSON_IsNumber(x2Item) || !cJSON_IsNumber(y2Item) ||
		!cJSON_IsNumber(dimXItem) || !cJSON_IsNumber(dimYItem)) {
		acutPrintf(_T("[ERROR] create_dimension_linear: invalid parameters.\n"));
		return;
	}

	double x1 = x1Item->valuedouble;
	double y1 = y1Item->valuedouble;
	double x2 = x2Item->valuedouble;
	double y2 = y2Item->valuedouble;
	double dimX = dimXItem->valuedouble;
	double dimY = dimYItem->valuedouble;
	double rotation = 0.0;
	if (rotationItem && cJSON_IsNumber(rotationItem))
		rotation = rotationItem->valuedouble;

	if (this->actionBefore()) return;

	AcGePoint3d pt1(x1, y1, 0.0);
	AcGePoint3d pt2(x2, y2, 0.0);
	AcGePoint3d dimLinePt(dimX, dimY, 0.0);

	AcDbRotatedDimension* pDim = new AcDbRotatedDimension(rotation, pt1, pt2, dimLinePt);

	AcDbObjectId dimId;
	if (pModelSpace->appendAcDbEntity(dimId, pDim) == Acad::eOk) {
		pDim->close();
		appendHandleToResult(dimId, data);
		data->ret = 0;
	} else {
		acutPrintf(_T("[ERROR] create_dimension_linear: failed to append to Model Space.\n"));
		delete pDim;
	}

	this->actionEnd();
}

void ToolAnnotation::create_dimension_aligned(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

	cJSON* x1Item = cJSON_GetObjectItem(params, "x1");
	cJSON* y1Item = cJSON_GetObjectItem(params, "y1");
	cJSON* x2Item = cJSON_GetObjectItem(params, "x2");
	cJSON* y2Item = cJSON_GetObjectItem(params, "y2");
	cJSON* dimXItem = cJSON_GetObjectItem(params, "dim_x");
	cJSON* dimYItem = cJSON_GetObjectItem(params, "dim_y");

	if (!x1Item || !y1Item || !x2Item || !y2Item || !dimXItem || !dimYItem ||
		!cJSON_IsNumber(x1Item) || !cJSON_IsNumber(y1Item) ||
		!cJSON_IsNumber(x2Item) || !cJSON_IsNumber(y2Item) ||
		!cJSON_IsNumber(dimXItem) || !cJSON_IsNumber(dimYItem)) {
		acutPrintf(_T("[ERROR] create_dimension_aligned: invalid parameters.\n"));
		return;
	}

	double x1 = x1Item->valuedouble;
	double y1 = y1Item->valuedouble;
	double x2 = x2Item->valuedouble;
	double y2 = y2Item->valuedouble;
	double dimX = dimXItem->valuedouble;
	double dimY = dimYItem->valuedouble;

	if (this->actionBefore()) return;

	AcGePoint3d pt1(x1, y1, 0.0);
	AcGePoint3d pt2(x2, y2, 0.0);
	AcGePoint3d dimLinePt(dimX, dimY, 0.0);

	AcDbAlignedDimension* pDim = new AcDbAlignedDimension(pt1, pt2, dimLinePt);

	AcDbObjectId dimId;
	if (pModelSpace->appendAcDbEntity(dimId, pDim) == Acad::eOk) {
		pDim->close();
		appendHandleToResult(dimId, data);
		data->ret = 0;
	} else {
		acutPrintf(_T("[ERROR] create_dimension_aligned: failed to append to Model Space.\n"));
		delete pDim;
	}

	this->actionEnd();
}

void ToolAnnotation::create_dimension_angular(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

	cJSON* cxItem = cJSON_GetObjectItem(params, "center_x");
	cJSON* cyItem = cJSON_GetObjectItem(params, "center_y");
	cJSON* x1Item = cJSON_GetObjectItem(params, "x1");
	cJSON* y1Item = cJSON_GetObjectItem(params, "y1");
	cJSON* x2Item = cJSON_GetObjectItem(params, "x2");
	cJSON* y2Item = cJSON_GetObjectItem(params, "y2");
	cJSON* arcXItem = cJSON_GetObjectItem(params, "arc_x");
	cJSON* arcYItem = cJSON_GetObjectItem(params, "arc_y");

	if (!cxItem || !cyItem || !x1Item || !y1Item || !x2Item || !y2Item ||
		!cJSON_IsNumber(cxItem) || !cJSON_IsNumber(cyItem) ||
		!cJSON_IsNumber(x1Item) || !cJSON_IsNumber(y1Item) ||
		!cJSON_IsNumber(x2Item) || !cJSON_IsNumber(y2Item)) {
		acutPrintf(_T("[ERROR] create_dimension_angular: invalid parameters.\n"));
		return;
	}

	double cx = cxItem->valuedouble;
	double cy = cyItem->valuedouble;
	double x1 = x1Item->valuedouble;
	double y1 = y1Item->valuedouble;
	double x2 = x2Item->valuedouble;
	double y2 = y2Item->valuedouble;

	if (this->actionBefore()) return;

	AcGePoint3d center(cx, cy, 0.0);
	AcGePoint3d pt1(x1, y1, 0.0);
	AcGePoint3d pt2(x2, y2, 0.0);

	AcGePoint3d arcPt;
	if (arcXItem && cJSON_IsNumber(arcXItem) && arcYItem && cJSON_IsNumber(arcYItem)) {
		arcPt.set(arcXItem->valuedouble, arcYItem->valuedouble, 0.0);
	} else {
		// 默认弧线位置：两射线中间角度方向，距中心点与pt1等距处
		AcGeVector3d v1(pt1.x - cx, pt1.y - cy, 0.0);
		AcGeVector3d v2(pt2.x - cx, pt2.y - cy, 0.0);
		AcGeVector3d midVec = v1.normal() + v2.normal();
		if (midVec.length() < 1e-10) {
			midVec = v1.normal().perpVector();
		}
		double radius = v1.length();
		arcPt = center + midVec.normal() * radius;
	}

	AcDb3PointAngularDimension* pDim = new AcDb3PointAngularDimension(center, pt1, pt2, arcPt);

	AcDbObjectId dimId;
	if (pModelSpace->appendAcDbEntity(dimId, pDim) == Acad::eOk) {
		pDim->close();
		appendHandleToResult(dimId, data);
		data->ret = 0;
	} else {
		acutPrintf(_T("[ERROR] create_dimension_angular: failed to append to Model Space.\n"));
		delete pDim;
	}

	this->actionEnd();
}

void ToolAnnotation::create_dimension_radius(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;

	cJSON* cxItem = cJSON_GetObjectItem(params, "center_x");
	cJSON* cyItem = cJSON_GetObjectItem(params, "center_y");
	cJSON* radiusItem = cJSON_GetObjectItem(params, "radius");
	cJSON* angleItem = cJSON_GetObjectItem(params, "angle");
	cJSON* leaderLenItem = cJSON_GetObjectItem(params, "leader_length");

	if (!cxItem || !cyItem || !radiusItem || !angleItem ||
		!cJSON_IsNumber(cxItem) || !cJSON_IsNumber(cyItem) ||
		!cJSON_IsNumber(radiusItem) || !cJSON_IsNumber(angleItem)) {
		acutPrintf(_T("[ERROR] create_dimension_radius: invalid parameters.\n"));
		return;
	}

	double cx = cxItem->valuedouble;
	double cy = cyItem->valuedouble;
	double radius = radiusItem->valuedouble;
	double angle = angleItem->valuedouble;
	double leaderLength = 0.0;
	if (leaderLenItem && cJSON_IsNumber(leaderLenItem))
		leaderLength = leaderLenItem->valuedouble;

	if (radius <= 0.0) {
		acutPrintf(_T("[ERROR] create_dimension_radius: radius must be positive.\n"));
		return;
	}

	if (this->actionBefore()) return;

	AcGePoint3d center(cx, cy, 0.0);
	// 根据半径和角度计算圆上的弦点
	AcGePoint3d chordPoint(cx + radius * cos(angle), cy + radius * sin(angle), 0.0);

	AcDbRadialDimension* pDim = new AcDbRadialDimension(center, chordPoint, leaderLength);

	AcDbObjectId dimId;
	if (pModelSpace->appendAcDbEntity(dimId, pDim) == Acad::eOk) {
		pDim->close();
		appendHandleToResult(dimId, data);
		data->ret = 0;
	} else {
		acutPrintf(_T("[ERROR] create_dimension_radius: failed to append to Model Space.\n"));
		delete pDim;
	}

	this->actionEnd();
}

void ToolAnnotation::create_leader(AiToolCommandData* data)
{
	cJSON* params = data->jsonRoot;
	cJSON* pointsItem = cJSON_GetObjectItem(params, "points");
	cJSON* textItem = cJSON_GetObjectItem(params, "text");

	if (!pointsItem || !cJSON_IsArray(pointsItem)) {
		acutPrintf(_T("[ERROR] create_leader: invalid or missing 'points' array.\n"));
		return;
	}

	int numPoints = cJSON_GetArraySize(pointsItem);
	if (numPoints < 2) {
		acutPrintf(_T("[ERROR] create_leader: at least 2 points are required.\n"));
		return;
	}

	if (this->actionBefore()) return;

	AcGePoint3dArray vertices;
	bool valid = true;

	for (int i = 0; i < numPoints; ++i) {
		cJSON* pt = cJSON_GetArrayItem(pointsItem, i);
		cJSON* xItem = cJSON_GetObjectItem(pt, "x");
		cJSON* yItem = cJSON_GetObjectItem(pt, "y");

		if (xItem && cJSON_IsNumber(xItem) && yItem && cJSON_IsNumber(yItem)) {
			vertices.append(AcGePoint3d(xItem->valuedouble, yItem->valuedouble, 0.0));
		} else {
			acutPrintf(_T("[ERROR] create_leader: invalid point data at index %d.\n"), i);
			valid = false;
			break;
		}
	}

	if (!valid) {
		this->actionEnd();
		return;
	}

	AcDbLeader* pLeader = new AcDbLeader();
	for (int i = 0; i < numPoints; ++i) {
		pLeader->appendVertex(vertices[i]);
	}

	if (textItem && cJSON_IsString(textItem)) {
		TCHAR textStr[512] = { 0 };

		if (CUtils::utf8ToTChar(textItem->valuestring, textStr, ARRAYSIZE(textStr))) {
			AcDbMText* pMText = new AcDbMText();
			pMText->setContents(textStr);
			pMText->setTextHeight(1.0);
			pMText->setLocation(vertices[numPoints - 1]);

			AcDbObjectId mtextId;
			if (pModelSpace->appendAcDbEntity(mtextId, pMText) == Acad::eOk) {
				pMText->close();
				pLeader->attachAnnotation(mtextId);
			}
		}
	}

	AcDbObjectId leaderId;
	if (pModelSpace->appendAcDbEntity(leaderId, pLeader) == Acad::eOk) {
		pLeader->close();
		appendHandleToResult(leaderId, data);
		data->ret = 0;
	} else {
		acutPrintf(_T("[ERROR] create_leader: failed to append to Model Space.\n"));
		delete pLeader;
	}

	this->actionEnd();
}
