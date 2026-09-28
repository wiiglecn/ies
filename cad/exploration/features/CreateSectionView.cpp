#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include <cmath>
#include "cJSON.h"
#include "utils.h"
#include "CreateSectionView.h"

#define PAGE_WIDTH 420.0

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

	
	

	//cJSON* item = NULL;
	//cJSON_ArrayForEach(item, s_pJsonData) {
 //       // 处理每个属性
 //       if (cJSON_IsArray(item)) {
	//	}
	//}
	

	// showAllLayer();
	 CUtils::ClearSelection();

	 if (this->startTrans()) return;

     int ret = this->actionBefore();
	 if (ret != 0 ) {
	 	aboutTrans();
	 	return;
	 }
	 Acad::ErrorStatus es;
	 AcGePoint3d basePt;

	 CString cadBlockUrl;
	 cadBlockUrl.Format(_T("support/%s/钻孔剖面图.dwg"), CADYEAR);
	 cadBlockUrl = strFilePath + cadBlockUrl;

	 TCHAR* blockStyleName=_T("钻孔剖面图");

	//图例的部分
	AcDbObjectId blkDefId = ImportDwgAsBlock(cadBlockUrl.GetString(), blockStyleName);
	if (blkDefId == AcDbObjectId::kNull)
	{
		aboutTrans();
		return;
	}
	{
		// 插入块参照
        AcDbBlockReference* pBlkRef = new AcDbBlockReference;
        pBlkRef->setBlockTableRecord(blkDefId);
        pBlkRef->setPosition(basePt);

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


	 //
	double min_zk = 186.02;
	int divide = 12;
	double elevation_zero = 208.55;
	 drawTable(basePt);
	 drawRuler(basePt,min_zk,divide,elevation_zero);


	 this->commitTrans();
	 this->actionEnd();
	 this->regen();



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

void CCreateSectionView::drawTable(AcGePoint3d& basePt)
{
	double left=23.0;
	double bottom = 60.0;
	double height = 8.0;
	double width_table = PAGE_WIDTH-left*2;
	
	bool is_dt = true;
	int rows = is_dt?3:2;

	ACHAR* layerName = _T("地质-标注栏");
	CUtils::EnsureLayer(layerName,true);

	ACHAR* pszStyleName=_T("标注栏项目");

	//画外框
	std::vector<AcGePoint3d> arrPts;
	
	AcGePoint3d pt1(basePt);
	pt1.x +=left;
	pt1.y +=bottom;
	AcGePoint3d pt2(pt1);
	pt2.x +=width_table;
	AcGePoint3d pt3(pt2);
	pt3.y +=rows*height;
	AcGePoint3d pt4(pt1);
	pt4.y = pt3.y;

	arrPts.push_back(pt1);
	arrPts.push_back(pt2);
	arrPts.push_back(pt3);
	arrPts.push_back(pt4);

	CUtils::AddDbPolyline(pModelSpace, arrPts, 0.3, true);

	//画横线
	//pt2.y -=4.0;
	for(int i=1;i<rows;i++){
		pt2.x = pt1.x;
		pt2.y =pt1.y+i*height;

		pt3.x =pt2.x+width_table;
		pt3.y =pt2.y;
		CUtils::AddLine(pModelSpace, pt2, pt3);
	}
	
	//单条竖线
	{
		pt2.x = pt1.x+ 30;
		pt2.y = pt1.y;

		pt3.x = pt2.x;
		pt3.y = pt2.y + height*rows;
		CUtils::AddLine(pModelSpace, pt2, pt3);
	}

	pt2.x = pt1.x+ 15;
	pt2.y = pt1.y;
	if (is_dt){
		pt2.y += 4.0;
		CUtils::AddDbText(pModelSpace,_T("动探击数"),pt2,2.4,pszStyleName,DBTA_CENTER,1.0);
	}else{
		pt2.y -= 4.0;
	}
	//钻孔间距 (m)
	{
		pt2.y += 8.0;
		CUtils::AddDbText(pModelSpace,_T("钻孔间距 (m)"),pt2,2.4,pszStyleName,DBTA_CENTER,1.0);
	}
	//孔   深 (m)
	{
		pt2.y += 8.0;
		CUtils::AddDbText(pModelSpace,_T("孔   深 (m)"),pt2,2.4,pszStyleName,DBTA_CENTER,1.0);
	}

	

}
void CCreateSectionView::drawRuler(AcGePoint3d& basePt,double min_zk,int divide,double elevation_zero)
{
	double left=23.0;
	double height_ruler = 130.0;
	double height = 8.0;
	
	bool is_dt = true;
	int rows = is_dt?3:2;

	ACHAR* layerName = _T("地质-文字");
	ACHAR* pszStyleName=_T("高程系统");

	CUtils::EnsureLayer(layerName,true);

	double bottom = 60.0+height*rows;
	AcGePoint3d pt1(basePt);
	pt1.x +=left+30-0.5;
	pt1.y +=bottom;

	AcGePoint3d pt2(pt1);
	pt2.y +=height_ruler;

	CUtils::AddLine(pModelSpace, pt1, pt2);
	pt1.x+=1.0;
	pt2.x+=1.0;
	CUtils::AddLine(pModelSpace, pt1, pt2);

	int min_zk_int = (int)min_zk;
	int max_zk_int = (int)(elevation_zero);
	max_zk_int+=1;

	double zk_divide = std::ceil((double)(max_zk_int - min_zk_int) / divide);

	double height_divide=height_ruler/(divide+1);
	double height_txt=2.5;
	double dWidthFactor=1.0;

	pt2.y=pt1.y;
	AcGePoint3d pt3(pt1),pt4(pt1);
	for(int i=0;i<=divide;i++){
		pt2.x=pt1.x-7.5;
		pt2.y+=10;
		int height_detail=(int)(min_zk_int+i*zk_divide);
		CString txt_ruler;
		txt_ruler.Format(_T("%d"), height_detail);
		CUtils::AddDbText(pModelSpace,txt_ruler,
								pt2,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);

		pt3.x=pt1.x-1.5;
		pt3.y=pt2.y;
		pt4.x=pt3.x+1.5;
		pt4.y=pt3.y;
		CUtils::AddLine(pModelSpace, pt3, pt4);
		if (i < divide && i % 2 == 0) {
			std::vector<AcGePoint3d> arrPts;
			pt3.x=pt1.x-0.5;
			pt3.y=pt2.y;
			pt4.x=pt3.x;
			pt4.y=pt2.y+10;
			arrPts.push_back(pt3);
			arrPts.push_back(pt4);
			CUtils::AddDbPolyline(pModelSpace,arrPts,1.0);
		}
	}

	pt2.y=pt1.x-0.5;
	pt2.y=pt1.y+height_ruler+8.0;

	CUtils::AddDbText(pModelSpace,_T("(1985国家高程基准)"),pt2,3.0,pszStyleName,DBTA_CENTER,1.0);
	pt2.y+=5.0;
	CUtils::AddDbText(pModelSpace,_T("高 程 (m)"),pt2,3.0,pszStyleName,DBTA_CENTER,1.0);

	
}

