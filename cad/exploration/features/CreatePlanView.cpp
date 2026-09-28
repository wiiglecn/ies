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
bool CCreatePlanView::Execute(const TCHAR* requestPath)
{
	if (requestPath == NULL || requestPath[0] == _T('\0')) {
		CUtils::acutPrintf(_T("[CAD ERROR] 平面图请求参数文件路径为空！\n"));
		return false;
	}
	strFilePath = CUtils::GetArxFolder();
	strFilePath.Replace(_T("\\"), _T("/"));
	s_pJsonData = CUtils::LoadJsonFile(requestPath);
	if (s_pJsonData == NULL || !cJSON_IsArray(s_pJsonData)) {
		CUtils::acutPrintf(_T("[CAD ERROR] 平面图请求参数必须是 JSON 数组，或参数文件无法读取：%s\n"), requestPath);
		return false;
	}

	
	CString cadHoleConfigUrl= strFilePath + _T("support/block_define.json");
	// 解析 JSON（失败时内部已打印错误，这里保留原有业务提示）
    s_blockConfigData = CUtils::LoadJsonFile(cadHoleConfigUrl.GetString());   // 静默模式
    if (s_blockConfigData == NULL) {
        CUtils::acutPrintf(_T("[CAD ERROR] 没有pat配置文件，请检查！ %s\n"), cadHoleConfigUrl.GetString());
        return false;
    }

	CUtils::ClearSelection();

	if (this->startTrans()) return false;

    int ret = this->actionBefore();
	if (ret != 0 ) {
		aboutTrans();
		return false;
	}
	

	const TCHAR* layerName = _T("勘察点方案");        // ← 按需改图层名
	if (CUtils::EnsureLayer(layerName).isNull()){
		CUtils::acutPrintf(_T("[CAD ERROR] 创建图层失败，请检查！ %s\n"), layerName);
		aboutTrans();
        return false;
	}
	// 清除当前层旧元素（pModelSpace 由基类管理开关，本函数不动它）
    int n = CUtils::EraseLayerEntities(this->pModelSpace, layerName); 

	AcDbObjectId styleId=CUtils::EnsureTextStyle( _T("勘察孔"), _T("宋体"),1.75,0.7);
	if (styleId.isNull()){
		CUtils::acutPrintf(_T("[CAD ERROR] 创建样式失败，请检查！\n"));
		aboutTrans();
        return false;
	}


	std::vector<CString> arrUsedHoles;   // 收集用到的块名（去重）
	CString fistCadBlockUrl,firstBlockStyleName; 
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
				{
					cJSON* columnTry = CUtils::GetJsonObjectItemUtf8(item,_T("X坐标(m)"));
					if (columnTry == NULL){
						continue;  
					}
					ptInsert.x = columnTry->valuedouble;

				}
				{
					cJSON* columnTry = CUtils::GetJsonObjectItemUtf8(item,_T("Y坐标(m)"));
					if (columnTry == NULL){
						continue;  
					}
					ptInsert.y = columnTry->valuedouble;
				}
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

	CreateLegend(arrUsedHoles, fistCadBlockUrl,firstBlockStyleName);

	this->commitTrans();
	this->actionEnd();
	this->regen();
	return true;
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
                    			const CString& blockStyleName)
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

	// ---- 计算图形范围，取底部中点作为图例绘制起点 ----
    AcDbExtents extAll;
    if (!CUtils::GetModelSpaceBounds(this->pModelSpace, extAll))
    {
        CUtils::acutPrintf(_T("[CAD ERROR] 图形范围为空，无法定位图例\n"));
        return;
    }
    AcGePoint3d ptMin = extAll.minPoint();
    AcGePoint3d ptMax = extAll.maxPoint();

    AcGePoint3d ptLegend;                        // 图例起点：图形中间底部
    ptLegend.x = (ptMin.x + ptMax.x) * 0.5;      // 水平居中
    ptLegend.y = ptMin.y;                        // 底部

    double dMargin = 10.0;                        // 与图形底边留点间距，按需调
    ptLegend.y -= dMargin;
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