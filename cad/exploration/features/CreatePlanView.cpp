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

	const TCHAR* layerName = _T("勘察点方案");        // ← 按需改图层名
	if (CUtils::EnsureLayer(layerName).isNull()){
		CUtils::acutPrintf(_T("[CAD ERROR] 创建图层失败，请检查！ %s\n"), layerName);
        return;
	}
	AcDbObjectId styleId=CUtils::EnsureTextStyle( _T("勘察孔"), _T("宋体"),3.0,0.7);
	if (styleId.isNull()){
		CUtils::acutPrintf(_T("[CAD ERROR] 创建样式失败，请检查！\n"));
        return;
	}


	if (this->startTrans()) return;

    int ret = this->actionBefore();
	if (ret != 0 ) {
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
			cadBlockUrl.Format(_T("support/block/%.dwg"), blockSingleName);
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
					cJSON* columnTry = cJSON_GetArrayItem(rowTry, 11); 
					if (columnTry != NULL && cJSON_IsNumber(columnTry)){
						CUtils::utf8ToTChar(columnTry->valueString, elevation_str, 128);
					}
				}
				{
					cJSON* columnTry = cJSON_GetArrayItem(rowTry, 18); 
					if (columnTry != NULL && cJSON_IsNumber(columnTry)){
						CUtils::utf8ToTChar(columnTry->valueString, depth_str, 128);
					}
				}
			}

			ChangeText(blkDefId,styleId,holeName,elevation_str,depth_str);


            break;
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

	// 创建文本实体 - 顶部文本 (holeName)
	CUtils::AddDbText(pBTR,holeName,
                            AcGePoint3d(0.0, 2.0, 0.0),height,pszStyleName,DBTA_CENTER,dWidthFactor);

	// 创建文本实体 - 中间文本 (elevation)
	CUtils::AddDbText(pBTR,elevation,
                            AcGePoint3d(0.0, 0.0, 0.0),height,pszStyleName,DBTA_CENTER,dWidthFactor);

	// 创建文本实体 - 底部文本 (depth)
	CUtils::AddDbText(pBTR,depth,
                            AcGePoint3d(0.0, -2.0, 0.0),height,pszStyleName,DBTA_CENTER,dWidthFactor);

	pBTR->close();
}