#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include "cJSON.h"
#include "utils.h"
#include "CreatePlanView.h"

CCreatePlanView::CCreatePlanView():CCreateBaseView()
{
	pszStyleName = _T("勘察孔");
}
void CCreatePlanView::Execute()
{
	if (!prepare()) return;

	
	CString cadHoleConfigUrl= strFilePath + _T("support/block_define.json");
	// 解析 JSON（失败时内部已打印错误，这里保留原有业务提示）
    s_blockConfigData = CUtils::LoadJsonFile(cadHoleConfigUrl.GetString());   // 静默模式
    if (s_blockConfigData == NULL) {
        CUtils::acutPrintf(_T("[CAD ERROR] 没有pat配置文件，请检查！ %s\n"), cadHoleConfigUrl.GetString());
        return;
    }

	CUtils::ClearSelection();

	if (this->startTrans()) return;

    int ret = this->actionBefore();
	if (ret != 0 ) {
		aboutTrans();
		return;
	}

	const TCHAR* layerName = _T("勘察点方案");        // ← 按需改图层名
	if (CUtils::EnsureLayer(layerName).isNull()){
		CUtils::acutPrintf(_T("[CAD ERROR] 创建图层失败，请检查！ %s\n"), layerName);
		aboutTrans();
        return;
	}
	// 清除当前层旧元素（pModelSpace 由基类管理开关，本函数不动它）
    int n = CUtils::EraseLayerEntities(this->pModelSpace, layerName); 

	AcDbObjectId styleId=CUtils::EnsureTextStyle( _T("勘察孔"), _T("宋体"),1.75,0.7);
	if (styleId.isNull()){
		CUtils::acutPrintf(_T("[CAD ERROR] 创建样式失败，请检查！\n"));
		aboutTrans();
        return;
	}


	std::vector<CString> arrUsedHoles;   // 收集用到的块名（去重）
	std::vector<AcGePoint3d> arrInsert;  // 收集钻孔插入点，供生成后自动缩放视图用
	CString fistCadBlockUrl,firstBlockStyleName; 

	// ★记录底图范围（插入钻孔之前取 EXTMIN/EXTMAX），用于诊断钻孔与底图是否同一坐标系
	AcGePoint3d baseMin, baseMax;
	bool bHasBase = false;
	{
		AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
		if (pDb != NULL)
		{
			baseMin = pDb->extmin();
			baseMax = pDb->extmax();
			if (baseMin.x < baseMax.x && baseMin.y < baseMax.y)
			{
				bHasBase = true;
				CUtils::acutPrintf(_T("[平面图] 底图范围：X[%.2f~%.2f] Y[%.2f~%.2f]\n"),
					baseMin.x, baseMax.x, baseMin.y, baseMax.y);
			}
			else
			{
				CUtils::acutPrintf(_T("[平面图] 底图范围：读取失败（当前图形为空）\n"));
			}
		}
	}

	// 预扫描:先取全部孔的原始坐标,结合底图范围自动判定数据 X/Y 需不需要互换。
	// 原因:数据源在两轮请求里 X/Y 顺序不一样(请求A 数据X=3652万/数据Y=274万;请求B 反之),
	// 写死映射会出现镜像问题。这里在两种方案中选与底图包围盒中心偏移更小的一种;
	// 底图不可用时默认按用户约定互换(数据X->CAD Y, 数据Y->CAD X)。
	bool bSwapXY = true;
	{
		double rMinX=0,rMaxX=0,rMinY=0,rMaxY=0; bool bHasRaw=false;
		cJSON* scan = NULL;
		cJSON_ArrayForEach(scan, s_pJsonData)
		{
			if (scan == NULL) continue;
			cJSON* sx = CUtils::GetJsonObjectItemUtf8(scan, _T("X坐标(m)"));
			cJSON* sy = CUtils::GetJsonObjectItemUtf8(scan, _T("Y坐标(m)"));
			if (sx == NULL || sy == NULL || !cJSON_IsNumber(sx) || !cJSON_IsNumber(sy)) continue;
			double x = sx->valuedouble, y = sy->valuedouble;
			if (!bHasRaw) { rMinX=rMaxX=x; rMinY=rMaxY=y; bHasRaw=true; }
			else {
				if (x<rMinX) rMinX=x; if (x>rMaxX) rMaxX=x;
				if (y<rMinY) rMinY=y; if (y>rMaxY) rMaxY=y;
			}
		}
		if (bHasRaw)
		{
			double rawCx=(rMinX+rMaxX)/2.0, rawCy=(rMinY+rMaxY)/2.0;
			CUtils::acutPrintf(_T("[平面图] 数据包围盒中心:(%.2f, %.2f)\n"), rawCx, rawCy);
			if (bHasBase)
			{
				double baseCx=(baseMin.x+baseMax.x)/2.0, baseCy=(baseMin.y+baseMax.y)/2.0;
				double dNoSwap = fabs(rawCx-baseCx) + fabs(rawCy-baseCy);
				double dSwap   = fabs(rawCy-baseCx) + fabs(rawCx-baseCy);
				bSwapXY = (dSwap < dNoSwap);
				CUtils::acutPrintf(_T("[平面图] 坐标自动判定:%s (底图中心=(%.2f,%.2f), 直接偏移=%.1f, 互换偏移=%.1f)\n"),
					bSwapXY ? _T("数据X->CAD Y, 数据Y->CAD X (互换)")
							: _T("数据X->CAD X, 数据Y->CAD Y (直接对应)"),
					baseCx, baseCy, dNoSwap, dSwap);
			}
			else
			{
				CUtils::acutPrintf(_T("[平面图] 坐标映射:底图不可用,按用户约定默认互换\n"));
			}
		}
	}

	bool bFirstSaved = false;
	cJSON* item = NULL;
	cJSON_ArrayForEach(item, s_pJsonData) {
        // 处理每个属性
        if (item!=NULL) {
            const char* blockName = (item->string != NULL) ? item->string : "(匿名数组)";
            CUtils::acutPrintf(_T("[数组] %hs，共 %d 个元素\n"),
                            blockName, cJSON_GetArraySize(item));

			ACHAR arrayName[256] = { 0 };
			CUtils::utf8ToTChar(blockName, arrayName, ARRAYSIZE(arrayName));

			CString blockStyleName;
			blockStyleName.Format(_T("孔_%s"),arrayName);

			ACHAR holeName[256] = {0};
			{
				cJSON* columnTry = CUtils::GetJsonObjectItemUtf8(item,_T("钻孔类型"));
				if (columnTry == NULL){
					continue;  
				}
				CUtils::utf8ToTChar(columnTry->valuestring, holeName, ARRAYSIZE(holeName));
			}
			

			// 名字 → 图例名：如 "鉴别孔" → "GKJBK"
            TCHAR blockSingleName[128] = { 0 };
            if (!CUtils::FindBlockNameByHoleName(s_blockConfigData, holeName,
                                      blockSingleName, ARRAYSIZE(blockSingleName)))
            {
				//默认值
				_tcscpy(blockSingleName, _T("GKJBK"));
            }
			// ★ 去重收集（放在这里，即使后续 continue 也能收集到）
			bool bExists = false;
			for (size_t k = 0; k < arrUsedHoles.size(); ++k)
			{
				if (arrUsedHoles[k].CompareNoCase(holeName) == 0)
				{
					bExists = true;
					break;
				}
			}
			if (!bExists)
				arrUsedHoles.push_back(CString(holeName));

			CString cadBlockUrl;
			cadBlockUrl.Format(_T("support/block/%s.dwg"), blockSingleName);
			cadBlockUrl = strFilePath + cadBlockUrl;

			AcGePoint3d ptInsert;
			{
				double dX = 0.0, dY = 0.0;
				{
					cJSON* columnTry = CUtils::GetJsonObjectItemUtf8(item,_T("X坐标(m)"));
					if (columnTry == NULL){
						continue;  
					}
					dX = columnTry->valuedouble;

				}
				{
					cJSON* columnTry = CUtils::GetJsonObjectItemUtf8(item,_T("Y坐标(m)"));
					if (columnTry == NULL){
						continue;  
					}
					dY = columnTry->valuedouble;
				}
				// ★坐标映射：实测底图 X≈36524934 / Y≈2747316 与请求数据 X/Y 同序同量级，直接对应，不做互换
				//   （若后续底图坐标系变化需要互换，把下面两行改成 ptInsert.y=dX; ptInsert.x=dY; 即可）
				// 坐标映射:由 bSwapXY 决定(前面预扫描时已自动判定)
				if (bSwapXY)
				{
					ptInsert.x = dY;   // 数据 X -> CAD Y
					ptInsert.y = dX;   // 数据 Y -> CAD X
				}
				else
				{
					ptInsert.x = dX;
					ptInsert.y = dY;
				}

				arrInsert.push_back(ptInsert);
				CUtils::acutPrintf(_T("[平面图] %s 插入点 CAD(X=%.3f, Y=%.3f) ← 数据(X=%.3f, Y=%.3f)\n"),
					arrayName, ptInsert.x, ptInsert.y, dX, dY);
			}
			// 同名旧块先删（含其参照），强制按最新 DWG 重建
            CUtils::EraseBlock(this->pModelSpace, blockStyleName.GetString());

			AcDbObjectId blkDefId = ImportDwgAsBlock(cadBlockUrl.GetString(),ptInsert, blockStyleName);
			if (blkDefId == AcDbObjectId::kNull)
				continue;
			

			TCHAR elevation_str[128] = { 0 };
			TCHAR depth_str[128] = { 0 };
			{
				{
					cJSON* columnTry = CUtils::GetJsonObjectItemUtf8(item,_T("孔口高程(m)"));
					if (columnTry == NULL){
						continue;  
					}
					double elevation = CUtils::JsonValToDouble(columnTry);
					_stprintf(elevation_str, _T("%.2f"), elevation);
				}
				{
					cJSON* columnTry = CUtils::GetJsonObjectItemUtf8(item,_T("终孔深度(m)"));
					if (columnTry == NULL){
						continue;
					}
					double depth = CUtils::JsonValToDouble(columnTry);
    				_stprintf(depth_str, _T("%.2f"), depth);
				}
			}
			// ★ 记录第一个孔的样例参数（CString 赋值 = 深拷贝，栈缓冲区复用也安全）
            if (!bFirstSaved)
            {
				fistCadBlockUrl = cadBlockUrl;
                firstBlockStyleName  = blockStyleName;
                bFirstSaved        = true;
            }

			ChangeText(blkDefId,styleId,arrayName,elevation_str,depth_str);


            //break;
		}
	}

	// ★统一计算「底图 ∪ 钻孔」范围：图例与视图缩放共用，避免图例被杂散实体带偏
	bool   bHasAll  = false;
	double allMinX = 0.0, allMaxX = 0.0, allMinY = 0.0, allMaxY = 0.0;
	if (bHasBase)
	{
		allMinX = baseMin.x; allMaxX = baseMax.x;
		allMinY = baseMin.y; allMaxY = baseMax.y;
		bHasAll = true;
	}
	for (size_t i = 0; i < arrInsert.size(); ++i)
	{
		double px = arrInsert[i].x, py = arrInsert[i].y;
		if (!bHasAll)
		{
			allMinX = allMaxX = px; allMinY = allMaxY = py; bHasAll = true;
		}
		else
		{
			if (px < allMinX) allMinX = px;
			if (px > allMaxX) allMaxX = px;
			if (py < allMinY) allMinY = py;
			if (py > allMaxY) allMaxY = py;
		}
	}
	CUtils::acutPrintf(_T("[平面图] 底图∪钻孔范围：X[%.2f~%.2f] Y[%.2f~%.2f]\n"),
		allMinX, allMaxX, allMinY, allMaxY);

	CreateLegend(arrUsedHoles, fistCadBlockUrl,firstBlockStyleName,
		AcGePoint3d(allMinX, allMinY, 0.0), AcGePoint3d(allMaxX, allMaxY, 0.0), bHasAll);

	this->commitTrans();
	this->actionEnd();
	this->regen();

	// ★生成完成后把视图缩放到（底图 ∪ 钻孔）范围，否则钻孔在大坐标处当前视图看不到
	ZoomToHoles(arrInsert, baseMin, baseMax, bHasBase);
}

//缩放到「底图范围 ∪ 钻孔范围」（含块半径与右侧孔号/高程标注的余量）
//baseMin/baseMax 为插入钻孔前的底图包围盒，同时用于诊断两者是否同一坐标系
void CCreatePlanView::ZoomToHoles(const std::vector<AcGePoint3d>& arrHole,
	const AcGePoint3d& baseMin, const AcGePoint3d& baseMax, bool bHasBase)
{
	bool bHas = false;
	double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;

	if (bHasBase)
	{
		minX = baseMin.x; maxX = baseMax.x;
		minY = baseMin.y; maxY = baseMax.y;
		bHas = true;
	}

	if (!arrHole.empty())
	{
		double hMinX = arrHole[0].x, hMaxX = arrHole[0].x;
		double hMinY = arrHole[0].y, hMaxY = arrHole[0].y;
		for (size_t i = 1; i < arrHole.size(); ++i)
		{
			if (arrHole[i].x < hMinX) hMinX = arrHole[i].x;
			if (arrHole[i].x > hMaxX) hMaxX = arrHole[i].x;
			if (arrHole[i].y < hMinY) hMinY = arrHole[i].y;
			if (arrHole[i].y > hMaxY) hMaxY = arrHole[i].y;
		}

		CUtils::acutPrintf(_T("[平面图] 钻孔范围：X[%.2f~%.2f] Y[%.2f~%.2f]\n"),
			hMinX, hMaxX, hMinY, hMaxY);

		// 诊断：钻孔是否落在底图范围内
		if (bHasBase)
		{
			bool bInside = (hMinX >= baseMin.x && hMaxX <= baseMax.x
						 && hMinY >= baseMin.y && hMaxY <= baseMax.y);
			CUtils::acutPrintf(_T("[平面图] 钻孔%s底图范围内；中心偏移 dX=%.2f dY=%.2f\n"),
				bInside ? _T("在") : _T("不在"),
				(hMinX + hMaxX) / 2.0 - (baseMin.x + baseMax.x) / 2.0,
				(hMinY + hMaxY) / 2.0 - (baseMin.y + baseMax.y) / 2.0);
		}

		if (!bHas)
		{
			minX = hMinX; maxX = hMaxX; minY = hMinY; maxY = hMaxY;
			bHas = true;
		}
		else
		{
			// 底图与钻孔量级相近时才取并集；底图过大时取并集会把钻孔缩成一个像素，
			// 此时以钻孔范围为准，保证一眼能看到钻孔
			double baseSpanX = baseMax.x - baseMin.x;
			double baseSpanY = baseMax.y - baseMin.y;
			double baseSpan = (baseSpanX > baseSpanY) ? baseSpanX : baseSpanY;
			double holeSpanX = hMaxX - hMinX;
			double holeSpanY = hMaxY - hMinY;
			double holeSpan = (holeSpanX > holeSpanY) ? holeSpanX : holeSpanY;

			if (baseSpan <= holeSpan * 10.0 + 500.0 && baseSpan <= 3000.0)
			{
				if (hMinX < minX) minX = hMinX;
				if (hMaxX > maxX) maxX = hMaxX;
				if (hMinY < minY) minY = hMinY;
				if (hMaxY > maxY) maxY = hMaxY;
			}
			else
			{
				CUtils::acutPrintf(_T("[平面图] 底图跨度 %.1fm 远大于钻孔跨度 %.1fm，视图以钻孔为准\n"),
					baseSpan, holeSpan);
				minX = hMinX; maxX = hMaxX; minY = hMinY; maxY = hMaxY;
			}
		}
	}

	if (!bHas) return;

	const double MARGIN = 30.0;              // 块半径 2.15 + 右侧标注宽度，留足余量
	// 窗口过小时给最小窗口，避免零面积窗口被 CAD 拒绝
	if (maxX - minX < 1.0) { minX -= 50.0; maxX += 50.0; }
	if (maxY - minY < 1.0) { minY -= 50.0; maxY += 50.0; }

	CString cmd;
	cmd.Format(_T("_.ZOOM _W %f,%f %f,%f "),
		minX - MARGIN, minY - MARGIN, maxX + MARGIN, maxY + MARGIN);
	acDocManager->sendStringToExecute(acDocManager->curDocument(), cmd);
	CUtils::acutPrintf(_T("[平面图] 视图已缩放到 %.3f,%.3f ~ %.3f,%.3f\n"),
		minX - MARGIN, minY - MARGIN, maxX + MARGIN, maxY + MARGIN);
}
void CCreatePlanView::ClearExtDbCache()
{
	if (s_blockConfigData)
		cJSON_free(s_blockConfigData);
	CCreateBaseView::ClearExtDbCache();

}
void CCreatePlanView::ChangeText(AcDbObjectId blkDefId,AcDbObjectId styleId, ACHAR* holeName,ACHAR* elevation,ACHAR* depth)
{
	AcDbBlockTableRecord* pBTR = NULL;
    if (acdbOpenObject(pBTR, blkDefId, AcDb::kForWrite) != Acad::eOk)
        return;


	double height =2.1;
	double dWidthFactor = 0.6;
	

	double radius = 2.15;

	// 创建文本实体 - 顶部文本 (holeName)
	AcDbObjectId textId = CUtils::AddDbText(pBTR,holeName,
                            AcGePoint3d(radius*2, 1, 0.0),height,pszStyleName,DBTA_LEFT,dWidthFactor);

	CUtils::SetEntLayer(textId, _T("0")); 
	double dW = 0.0, dH = 0.0;						
    CUtils::GetEntExtentById(textId, dW, dH);
	AcGePoint3d ptDraw;
	ptDraw.x = radius*2+dW;
	ptDraw.y =  1-2.5/2;
	// 创建文本实体 -  (elevation)
	textId = CUtils::AddDbText(pBTR,elevation,
                            ptDraw,height,pszStyleName,DBTA_LEFT,dWidthFactor);
	CUtils::SetEntLayer(textId, _T("0")); 
	CUtils::GetEntExtentById(textId, dW, dH);

	// 创建文本实体 -  (depth)
	ptDraw.y = 1.5+2.5/2;
	textId = CUtils::AddDbText(pBTR,depth,
                            ptDraw,height,pszStyleName,DBTA_LEFT,dWidthFactor);
	CUtils::SetEntLayer(textId, _T("0"));

	AcGePoint3d ptDraw_start(ptDraw);
	ptDraw_start.y = 1.25+2.5/2;
	AcGePoint3d ptDraw_end(ptDraw_start);
	ptDraw_end.x +=dW;


	AcDbLine* pLine = new AcDbLine(ptDraw_start, ptDraw_end);
	AcDbObjectId lineId = AcDbObjectId::kNull;
	Acad::ErrorStatus es = pBTR->appendAcDbEntity(lineId, pLine);
	if (es == Acad::eOk){
		pLine->close();   
		CUtils::SetEntLayer(lineId, _T("0"));
	}
	else
		delete pLine;    

	AcGePoint3d ptOrigin;
	ptOrigin.x=2.15;
	ptOrigin.y=2.15;
	if (pBTR->setOrigin(ptOrigin) != Acad::eOk)
        CUtils::acutPrintf(_T("[CAD ERROR] 设置块原点失败\n"));


	pBTR->close();
}

void CCreatePlanView::CreateLegend(std::vector<CString>& arrUsedHoles,
                                const CString& fistCadBlockUrl,
                    			const CString& blockStyleName,
                    			const AcGePoint3d& allMin,
                    			const AcGePoint3d& allMax,
                    			bool bHasAll)
{
	if (arrUsedHoles.empty())
		return;
	const TCHAR* layerName = _T("勘察点图例");        // ← 按需改图层名
	if (CUtils::EnsureLayer(layerName).isNull()){
		CUtils::acutPrintf(_T("[CAD ERROR] 创建图层失败，请检查！ %s\n"), layerName);
        return;
	}
	// 清除当前层旧元素（pModelSpace 由基类管理开关，本函数不动它）
    int n = CUtils::EraseLayerEntities(this->pModelSpace, layerName); 

	// ---- 定位图例锚点：取「底图∪钻孔」范围的左下角 ----
	//  （原逻辑取模型空间全体包围盒的"底部中点"，一旦钻孔与底图分离就会被带到图幅外，例如 X=19636411）
    AcGePoint3d ptLegend;
    if (bHasAll)
    {
        ptLegend.x = allMin.x + 25.0;            // 从图幅左边界内缩，避免压住图框
        ptLegend.y = allMin.y - 10.0;            // 图幅底边下方
    }
    else
    {
        AcDbExtents extAll;
        if (!CUtils::GetModelSpaceBounds(this->pModelSpace, extAll))
        {
            CUtils::acutPrintf(_T("[CAD ERROR] 图形范围为空，无法定位图例\n"));
            return;
        }
        ptLegend.x = extAll.minPoint().x + 25.0;
        ptLegend.y = extAll.minPoint().y - 10.0;
    }
	CUtils::acutPrintf(_T("[平面图] 图例锚点(左下角)：%.3f, %.3f（图幅 X[%.2f~%.2f] Y[%.2f~%.2f]）\n"),
		ptLegend.x, ptLegend.y, allMin.x, allMax.x, allMin.y, allMax.y);
	Acad::ErrorStatus es;
	double radius = 2.15;

	//绘标头
	CUtils::AddDbText(pModelSpace,_T("图    例"),ptLegend,4.0,NULL,DBTA_CENTER,0.825);

	AcGePoint3d ptStart(ptLegend),ptEnd(ptLegend);
	ptStart.y -=5.0;
	ptStart.x -=10.0;

	ptEnd.y -=5.0;
	ptEnd.x +=10.0;

	std::vector<AcGePoint3d> arrPts;
    arrPts.push_back(ptStart);
    arrPts.push_back(ptEnd);
    CUtils::AddDbPolyline(pModelSpace, arrPts, 0.35);

	arrPts.clear();
	ptStart.y -= 1.0;
	ptEnd.y -= 1.0;
	arrPts.push_back(ptStart);
    arrPts.push_back(ptEnd);
	CUtils::AddDbPolyline(pModelSpace, arrPts, 0.75);

	//勘探孔图例举例说明
	AcGePoint3d ptDraw(ptStart);

	AcDbObjectId blkDefId = ImportDwgAsBlock(fistCadBlockUrl.GetString(), blockStyleName);
	if (blkDefId == AcDbObjectId::kNull)
		return;
	{
		ptDraw.y -= 15.0;
		ptDraw.x -= 7.0;

		// 6. 插入块参照
        AcDbBlockReference* pBlkRef = new AcDbBlockReference;
        pBlkRef->setBlockTableRecord(blkDefId);
        pBlkRef->setPosition(ptDraw);

        AcDbObjectId refId = AcDbObjectId::kNull;
        es = pModelSpace->appendAcDbEntity(refId, pBlkRef);
        if (es != Acad::eOk)
        {
            CUtils::acutPrintf(_T("\n[DWG导入] 错误：添加块参照失败(错误码 %d)。"), (int)es);
            delete pBlkRef;          // 未入库，delete（不能 close）
			return;
        }
        pBlkRef->close();
	}

	CString& holeName = arrUsedHoles[0];
	TCHAR blockSingleName[128] = { 0 };
	if (!CUtils::FindBlockNameByHoleName(s_blockConfigData, holeName,
								blockSingleName, ARRAYSIZE(blockSingleName)))
	{
		//默认值
		_tcscpy(blockSingleName, _T("GKJBK"));
	}

	CString cadBlockUrl;
	cadBlockUrl.Format(_T("support/block/%s.dwg"), blockSingleName);
	cadBlockUrl = strFilePath + cadBlockUrl;
	blkDefId = ImportDwgAsBlock(cadBlockUrl.GetString(), holeName);

	double height_txt = 1.75;
	double dWidthFactor = 0.7;
	if (blkDefId == AcDbObjectId::kNull)
		return;
	{
		ptDraw.x = ptStart.x + 15;
		ptDraw.y -= radius;
		// 6. 插入块参照
        AcDbBlockReference* pBlkRef = new AcDbBlockReference;
        pBlkRef->setBlockTableRecord(blkDefId);
        pBlkRef->setPosition(ptDraw);
		AcDbObjectId refId = AcDbObjectId::kNull;
        es = pModelSpace->appendAcDbEntity(refId, pBlkRef);
        if (es != Acad::eOk)
        {
            CUtils::acutPrintf(_T("\n[DWG导入] 错误：添加块参照失败(错误码 %d)。"), (int)es);
            delete pBlkRef;          // 未入库，delete（不能 close）
			return;
        }
        pBlkRef->close();

		// 创建文本实体 - 顶部文本 (holeName)
		ptDraw.x +=radius*2;
		ptDraw.y +=1.5;


		AcDbObjectId textId = CUtils::AddDbText(pModelSpace,_T("钻孔编号"),
								ptDraw,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);

		double dW = 0.0, dH = 0.0;						
		CUtils::GetEntExtentById(textId, dW, dH);
		ptDraw.x += dW;
		ptDraw.y -=  1;
		// 创建文本实体 -  (elevation)
		textId = CUtils::AddDbText(pModelSpace,_T("孔口高程"),
								ptDraw,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);
		//double dW_elevation = 0.0, dH_elevation = 0.0;	
		//CUtils::GetEntExtentById(textId, dW_elevation, dH_elevation);

		// 创建文本实体 -  (depth)
		ptDraw.y += 2.5;
		CUtils::AddDbText(pModelSpace,_T("勘探深度"),
								ptDraw,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);

		AcGePoint3d ptDraw_start(ptDraw);
		//ptDraw_start.x -= dW/2;
		ptDraw_start.y -= 0.5;
		AcGePoint3d ptDraw_end(ptDraw_start);
		ptDraw_end.x +=dW;


		AcDbLine* pLine = new AcDbLine(ptDraw_start, ptDraw_end);
		AcDbObjectId lineId = AcDbObjectId::kNull;
		Acad::ErrorStatus es = pModelSpace->appendAcDbEntity(lineId, pLine);
		if (es == Acad::eOk)
			pLine->close();                  
		else
			delete pLine;    

	}
	ptDraw.y = ptLegend.y - 33.0;
	ptDraw.x = ptLegend.x - 8.0;
	for (size_t k = 0; k < arrUsedHoles.size(); ++k)
    {
        holeName = arrUsedHoles[k];  
		if (!CUtils::FindBlockNameByHoleName(s_blockConfigData, holeName,
								blockSingleName, ARRAYSIZE(blockSingleName)))
		{
			//默认值
			_tcscpy(blockSingleName, _T("GKJBK"));
		}
		cadBlockUrl.Format(_T("support/block/%s.dwg"), blockSingleName);
		cadBlockUrl = strFilePath + cadBlockUrl;
		blkDefId = ImportDwgAsBlock(cadBlockUrl.GetString(), holeName);
		if (blkDefId == AcDbObjectId::kNull)
			return;
		
		AcGePoint3d ptDraw_block(ptDraw);
		ptDraw_block.x -=radius;
		ptDraw_block.y -=radius;

		AcDbBlockReference* pBlkRef = new AcDbBlockReference;
        pBlkRef->setBlockTableRecord(blkDefId);
        pBlkRef->setPosition(ptDraw_block);
		AcDbObjectId refId = AcDbObjectId::kNull;
        es = pModelSpace->appendAcDbEntity(refId, pBlkRef);
        if (es != Acad::eOk)
        {
            CUtils::acutPrintf(_T("\n[DWG导入] 错误：添加块参照失败(错误码 %d)。"), (int)es);
            delete pBlkRef;          // 未入库，delete（不能 close）
		}else{
			pBlkRef->close();
		}

		CUtils::AddDbRect(pModelSpace,ptDraw,16.0,8.0,1.0);

		AcGePoint3d ptDraw_txt(ptDraw);
		ptDraw_txt.x +=13.0;
		//ptDraw_txt.y -=1.5;
		CUtils::AddDbText(pModelSpace,holeName,
								ptDraw_txt,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);


		ptDraw.y -=10.5;
	}

	//地质剖面线及编号
	{
		CUtils::AddDbRect(pModelSpace,ptDraw,16.0,8.0,1.0);

		AcGePoint3d ptDraw_inner(ptDraw);
		ptDraw_inner.x -=3.5;
		CUtils::AddDbText(pModelSpace,_T("1"),
								ptDraw_inner,2.7,pszStyleName,DBTA_CENTER,dWidthFactor);

		ptDraw_inner.x +=7.3;
		CUtils::AddDbText(pModelSpace,_T("1`"),
								ptDraw_inner,2.7,pszStyleName,DBTA_CENTER,dWidthFactor);

		AcGePoint3d ptDraw_line_start(ptDraw),ptDraw_line_end(ptDraw);
		ptDraw_line_start.x -=3.0;
		ptDraw_line_end.x =ptDraw_line_start.x +5.6;

		AcDbLine* pLine = new AcDbLine(ptDraw_line_start, ptDraw_line_end);
		AcDbObjectId lineId = AcDbObjectId::kNull;
		Acad::ErrorStatus es = pModelSpace->appendAcDbEntity(lineId, pLine);
		if (es == Acad::eOk)
			pLine->close();                  
		else
			delete pLine;    


		AcGePoint3d ptDraw_txt(ptDraw);
		ptDraw_txt.x +=13.0;
		CUtils::AddDbText(pModelSpace,_T("地质剖面线及编号"),
								ptDraw_txt,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);


		ptDraw.y -=10.5;
	}

	//已有建筑物及层数
	{
		CUtils::AddDbRect(pModelSpace,ptDraw,16.0,8.0,1.0);

		_tcscpy(blockSingleName, _T("PMYyjz"));
		cadBlockUrl.Format(_T("support/block/%s.dwg"), blockSingleName);
		cadBlockUrl = strFilePath + cadBlockUrl;
		blkDefId = ImportDwgAsBlock(cadBlockUrl.GetString(), blockSingleName);
		if (blkDefId == AcDbObjectId::kNull)
			return;

		AcGePoint3d ptDraw_block(ptDraw);
		ptDraw_block.x -=6.0;
		ptDraw_block.y -=2.5;

		AcDbBlockReference* pBlkRef = new AcDbBlockReference;
        pBlkRef->setBlockTableRecord(blkDefId);
        pBlkRef->setPosition(ptDraw_block);
		AcDbObjectId refId = AcDbObjectId::kNull;
        es = pModelSpace->appendAcDbEntity(refId, pBlkRef);
        if (es != Acad::eOk)
        {
            CUtils::acutPrintf(_T("\n[DWG导入] 错误：添加块参照失败(错误码 %d)。"), (int)es);
            delete pBlkRef;          // 未入库，delete（不能 close）
		}else{
			pBlkRef->close();
		}


		AcGePoint3d ptDraw_txt(ptDraw);
		ptDraw_txt.x +=13.0;
		CUtils::AddDbText(pModelSpace,_T("已有建筑物及层数"),
								ptDraw_txt,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);


		ptDraw.y -=10.5;
	}

	//拟建建筑物及层数
	{
		CUtils::AddDbRect(pModelSpace,ptDraw,16.0,8.0,1.0);

		_tcscpy(blockSingleName, _T("PMXnjz"));
		cadBlockUrl.Format(_T("support/block/%s.dwg"), blockSingleName);
		cadBlockUrl = strFilePath + cadBlockUrl;
		blkDefId = ImportDwgAsBlock(cadBlockUrl.GetString(), blockSingleName);
		if (blkDefId == AcDbObjectId::kNull)
			return;

		AcGePoint3d ptDraw_block(ptDraw);
		ptDraw_block.x -=6.0;
		ptDraw_block.y -=2.5;

		AcDbBlockReference* pBlkRef = new AcDbBlockReference;
        pBlkRef->setBlockTableRecord(blkDefId);
        pBlkRef->setPosition(ptDraw_block);
		AcDbObjectId refId = AcDbObjectId::kNull;
        es = pModelSpace->appendAcDbEntity(refId, pBlkRef);
        if (es != Acad::eOk)
        {
            CUtils::acutPrintf(_T("\n[DWG导入] 错误：添加块参照失败(错误码 %d)。"), (int)es);
            delete pBlkRef;          // 未入库，delete（不能 close）
		}else{
			pBlkRef->close();
		}


		AcGePoint3d ptDraw_txt(ptDraw);
		ptDraw_txt.x +=13.0;
		CUtils::AddDbText(pModelSpace,_T("拟建建筑物及层数"),
								ptDraw_txt,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);


		ptDraw.y -=10.5;
	}

}