#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include "cJSON.h"
#include "utils.h"
#include "CreateSectionView.h"

//
CCreateSectionView::CCreateSectionView(void):CCreateBaseView(),s_ConfigData(NULL)
{
}
//
//CCreateSectionView::~CCreateSectionView(void)
//{
//}
void CCreateSectionView::Execute()
{
	if (!prepare()) return;

	CString cadSectionConfigUrl= strFilePath + _T("project/section_view_structure.json");
	// 解析 JSON（失败时内部已打印错误，这里保留原有业务提示）
    s_ConfigData = CUtils::LoadJsonFile(cadSectionConfigUrl.GetString());   // 静默模式
    if (s_ConfigData == NULL) {
        CUtils::acutPrintf(_T("[CAD ERROR] 没有剖面图配置文件，请检查！ %s\n"), cadSectionConfigUrl.GetString());
        return;
    }

	// 需要确保存在的图层列表（遍历创建）
    const TCHAR* arrLayers[] = {
        _T("地质-地面线"),   _T("地质-地层线"),   _T("地质-水位线"),
        _T("地质-地层符号"), _T("地质-填充花纹"), _T("地质-钻孔"),
        _T("地质-图框"),     _T("地质-图例"),     _T("地质-标贯"),
        _T("地质-静探"),     _T("地质-标注栏"),   _T("地质-文字"),
        _T("地质-地层编号"), _T("地质-原位曲线")
    };
    const int nLayerCount = sizeof(arrLayers) / sizeof(arrLayers[0]);

    for (int i = 0; i < nLayerCount; ++i)
    {
        // bSetCurrent=false：批量建层不逐个切换当前层，避免 CLAYER 被反复改写
        if (CUtils::EnsureLayer(arrLayers[i], false).isNull())
        {
            CUtils::acutPrintf(_T("[CAD ERROR] 创建图层失败，请检查！ %s\n"), arrLayers[i]);
            return;   // 若希望失败后继续建剩余图层，把 return 改成 continue
        }
    }

	drawTable();

	//String cadBlockUrl;
	//cadBlockUrl.Format(_T("support/%s/钻孔剖面图.dwg"), CADYEAR);
	//cadBlockUrl = strFilePath + cadBlockUrl;

	//{
	//	//图例的部分
	//	AcDbObjectId blkDefId = ImportDwgAsBlock(cadBlockUrl.GetString(),ptInsert, blockStyleName);
	//	if (blkDefId == AcDbObjectId::kNull)
	//		continue;
	//}

	//cJSON* item = NULL;
	//cJSON_ArrayForEach(item, s_pJsonData) {
 //       // 处理每个属性
 //       if (cJSON_IsArray(item)) {
	//	}
	//}
	

	// showAllLayer();
	//CUtils::ClearSelection();

	// if (this->startTrans()) return;

    // int ret = this->actionBefore();
	// if (ret != 0 ) {
	// 	aboutTrans();
	// 	return;
	// }


	// this->commitTrans();
	// this->actionEnd();
	// this->regen();



}
void CCreateSectionView::ClearExtDbCache()
{
	if (s_ConfigData)
		cJSON_free(s_ConfigData);
	CCreateBaseView::ClearExtDbCache();
}
void CCreateSectionView::showAllLayer()
{
    // ---- 收集当前图形所有图层名，按数组一次性打印 ----
    AcDbDatabase* pCurDb = acdbHostApplicationServices()->workingDatabase();
    AcDbLayerTable* pLayerTable = NULL;
    if (pCurDb->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk)
    {
        CUtils::acutPrintf(_T("[CAD ERROR] 打开图层表失败！\n"));
        return;
    }

    std::vector<CString> arrNames;                      // 先收集，不打印
    AcDbLayerTableIterator* pIter = NULL;
    if (pLayerTable->newIterator(pIter) == Acad::eOk)
    {
        for (; !pIter->done(); pIter->step())
        {
            AcDbLayerTableRecord* pLayerRecord = NULL;
            if (pIter->getRecord(pLayerRecord, AcDb::kForRead) == Acad::eOk)
            {
                const ACHAR* pszName = NULL;
                if (pLayerRecord->getName(pszName) == Acad::eOk && pszName != NULL)
                    arrNames.push_back(CString(pszName));   // ACHAR→CString，两套字符集通吃
                pLayerRecord->close();
            }
        }
        delete pIter;
    }
    else
    {
        CUtils::acutPrintf(_T("[CAD ERROR] 创建图层表迭代器失败！\n"));
    }
    pLayerTable->close();


    // 拼接成数组形式：["a", "b", "c"]
    CString strAll(_T("["));
    for (size_t i = 0; i < arrNames.size(); ++i)
    {
        if (i > 0) strAll += _T(", ");
        strAll += _T("\"");
        strAll += arrNames[i];
        strAll += _T("\"");
    }
    strAll += _T("]");

    // 一次性打印（整个数组只调一次 acutPrintf）
    CUtils::acutPrintf(_T("[LAYER LIST] 共 %d 个图层：%s\n"),
                       (int)arrNames.size(), (LPCTSTR)strAll);

}

void CCreateSectionView::drawTable()
{
	double left=23.0;
	double bottom = 76.0;
	double height = 10.0;


	
	bool is_dt = false;

	ACHAR* layerName = _T("地质-标注栏");
	CUtils::EnsureLayer(layerName,true);


	

}