#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include "cJSON.h"
#include "utils.h"
#include "CreatePlanView.h"


void CCreatePlanView::Execute()
{
	if (!prepare()) return;

	CString strFilePath = CUtils::GetArxFolder();
	strFilePath.Replace(_T("\\"), _T("/"));

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


	
	cJSON* item = NULL;
	cJSON_ArrayForEach(item, s_pJsonData) {
        // 处理每个属性
        if (cJSON_IsArray(item)) {
            const char* blockName = (item->string != NULL) ? item->string : "(匿名数组)";
            CUtils::acutPrintf(_T("[数组] %hs，共 %d 个元素\n"),
                            blockName, cJSON_GetArraySize(item));

			ACHAR arrayName[256] = { 0 };
			CUtils::utf8ToTChar(blockName, arrayName, ARRAYSIZE(arrayName));

			CString blockStyleName;
			blockStyleName.Format(_T("孔_%s"),arrayName);

			ACHAR holeName[256] = {0};
			{
				cJSON* rowTry = cJSON_GetArrayItem(item, 2);
				if (rowTry == NULL){
					continue;  
				}
				cJSON* columnTry = cJSON_GetArrayItem(rowTry, 14); 
				if (columnTry == NULL || !cJSON_IsString(columnTry)){
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
			CString cadBlockUrl;
			cadBlockUrl.Format(_T("support/block/%s.dwg"), blockSingleName);
			cadBlockUrl = strFilePath + cadBlockUrl;

			AcGePoint3d ptInsert;
			{
				cJSON* rowTry = cJSON_GetArrayItem(item, 4);
				if (rowTry == NULL)
					continue;  
				{
					cJSON* columnTry = cJSON_GetArrayItem(rowTry, 4); 
					if (columnTry == NULL || !cJSON_IsNumber(columnTry))
						continue;
					ptInsert.x = columnTry->valuedouble;

				}
				{
					cJSON* columnTry = cJSON_GetArrayItem(rowTry, 9); 
					if (columnTry == NULL || !cJSON_IsNumber(columnTry))
						continue;
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
				cJSON* rowTry = cJSON_GetArrayItem(item, 2);
				if (rowTry == NULL)
					continue;  
				{
					double elevation = CUtils::JsonValToDouble(cJSON_GetArrayItem(rowTry, 11));
    				_stprintf(elevation_str, _T("%.2f"), elevation);
				}
				{
					double depth = CUtils::JsonValToDouble(cJSON_GetArrayItem(rowTry, 18));
    				_stprintf(depth_str, _T("%.2f"), depth);
				}
			}

			ChangeText(blkDefId,styleId,arrayName,elevation_str,depth_str);


            //break;
		}
	}


	this->commitTrans();
	this->actionEnd();
	this->regen();

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
	TCHAR* pszStyleName = _T("勘察孔");

	double radius = 2.15;

	// 创建文本实体 - 顶部文本 (holeName)
	AcDbObjectId textId = CUtils::AddDbText(pBTR,holeName,
                            AcGePoint3d(radius*2, 1, 0.0),height,pszStyleName,DBTA_LEFT,dWidthFactor);

	double dW = 0.0, dH = 0.0;						
    CUtils::GetEntExtentById(textId, dW, dH);
	AcGePoint3d ptDraw;
	ptDraw.x = radius*2+dW;
	ptDraw.y =  1-2.5/2;
	// 创建文本实体 -  (elevation)
	textId = CUtils::AddDbText(pBTR,elevation,
                            ptDraw,height,pszStyleName,DBTA_LEFT,dWidthFactor);
	CUtils::GetEntExtentById(textId, dW, dH);

	// 创建文本实体 -  (depth)
	ptDraw.y = 1.5+2.5/2;
	CUtils::AddDbText(pBTR,depth,
                            ptDraw,height,pszStyleName,DBTA_LEFT,dWidthFactor);

	AcGePoint3d ptDraw_start(ptDraw);
	ptDraw_start.y = 1.25+2.5/2;
	AcGePoint3d ptDraw_end(ptDraw_start);
	ptDraw_end.x +=dW;


	AcDbLine* pLine = new AcDbLine(ptDraw_start, ptDraw_end);
	AcDbObjectId lineId = AcDbObjectId::kNull;
	Acad::ErrorStatus es = pBTR->appendAcDbEntity(lineId, pLine);
	if (es == Acad::eOk)
		pLine->close();                  
	else
		delete pLine;    

	AcGePoint3d ptOrigin;
	ptOrigin.x=2.15;
	ptOrigin.y=2.15;
	if (pBTR->setOrigin(ptOrigin) != Acad::eOk)
        CUtils::acutPrintf(_T("[CAD ERROR] 设置块原点失败\n"));


	pBTR->close();
}