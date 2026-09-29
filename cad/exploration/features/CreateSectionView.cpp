#define NOMINMAX 
#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include <cmath>
#include <limits> // 必须包含此头文件
#include "cJSON.h"
#include "utils.h"
#include "CreateSectionView.h"
#include "PatHatchLoader.h"

#define PAGE_WIDTH 420.0

// 标尺标定基准：drawRuler 计算一次，供 drawColumn/drawLineFix 共用
struct SectionRulerInfo
{
	double labelX;        // for 循环中第一个高程文字的 pt2.x（= 标尺线x - 7.5）
	double labelY;
    double rulerX;        // 标尺线本体 x（可选，方便其他函数对齐）
    int    min_zk_int;    // 最低孔底标高（取整）
    int    max_zk_int;    // 最高标高（含零标高，向上取整）
    double mPerGrid;      // 每格高差(米) = ceil((max-min)/divide)
    double unitPerM;      // 比例尺(图面单位/米) = height_ruler/(mPerGrid*divide)，与刻度严格对齐

	int index;
    SectionRulerInfo()
        : labelX(0.0), labelY(0.0), rulerX(0.0), min_zk_int(0), max_zk_int(0)
        , mPerGrid(0.0), unitPerM(0.0), index(0) {}
};
// ---- drawLineDrill 用：钻孔解析结构（文件作用域，VC7 局部类不能做模板参数）----
struct KsDrillLayer
{
    CString code;       // 地层编号
    double depthFrom;   // 进尺自(m)
    double depthTo;     // 进尺至(m)
    KsDrillLayer() : depthFrom(0.0), depthTo(0.0) {}
};
struct KsDrillHole
{
    double top;                // 孔口高程(m)
    double totalDepth;         // 终孔深度
    double x;                  // 柱右缘 = 高度尺位置（与 drawColumn 同公式）
    double xL;                 // 柱左缘 = x - 15（drawColumn 填充矩形起点）
    std::vector<KsDrillLayer> layers;
    KsDrillHole() : top(0.0), totalDepth(0.0), x(0.0), xL(0.0) {}
};
// 按地层编号找层下标；used 非空时跳过已配对的下标（防止同编号层被配两次）
static int KsFindLayer(const KsDrillHole& hole, const CString& code, const std::vector<int>& used)
{
    for (size_t i = 0; i < hole.layers.size(); ++i)
    {
        if (!used.empty() && used[i] != 0) continue;
        if (hole.layers[i].code.Compare(code) == 0) return (int)i;
    }
    return -1;
}
// 线型 ObjectId → 名称（AddDbPolyline 只认名称）；失败返回空串
static CString KsLinetypeNameById(const AcDbObjectId& id)
{
    CString sName;
    if (id.isNull()) return sName;

    AcDbLinetypeTableRecord* pRec = NULL;
    if (acdbOpenObject(pRec, id, AcDb::kForRead) != Acad::eOk)
        return sName;

    ACHAR* pszName = NULL;
    if (pRec->getName(pszName) == Acad::eOk && pszName != NULL)
    {
        sName = pszName;
        acutDelString(pszName);      // getName(ACHAR*&) 分配的内存必须释放
    }
    pRec->close();
    return sName;
}

// 地层编号在去重表中的下标；找不到返回 -1
static int KsFindCode(const std::vector<CString>& codes, const CString& code)
{
	for (size_t i = 0; i < codes.size(); ++i){
        if (codes[i].Compare(code) == 0) return (int)i;
	}
    return -1;
}

// 取编号对应的绘制样式：返回线型名（可为 NULL=ByLayer），颜色经 colorIdx 带回
// ★指针指向 ltNames 向量元素内部，向量在 drawLineDrill 作用域内存活，调用后立即使用
static const ACHAR* KsStyleFor(const std::vector<CString>& codes,
                               const std::vector<RandomPolylineStyle>& styles,
                               const std::vector<CString>& ltNames,
                               const CString& code,
                               Adesk::UInt16& colorIdx,
                               double& lineWeight)          // ★改 double
{
    colorIdx = 256;
    lineWeight = 0.0;                        // ★默认细线
    int ci = KsFindCode(codes, code);
    if (ci < 0 || ci >= (int)styles.size() || ci >= (int)ltNames.size())
        return NULL;
    colorIdx = styles[ci].colorIndex;
    lineWeight = styles[ci].lineWeight;
    if (ltNames[ci].IsEmpty())
        return NULL;
    return (LPCTSTR)ltNames[ci];
}


// 尖灭点高程：优先取 refIdx 层顶面；找不到则取与 elev 最接近的层底/孔底
static double KsPinchElev(const KsDrillHole& T, int refIdx, double elev)
{
    if (refIdx >= 0 && refIdx <  (int)T.layers.size())
        return T.top - T.layers[refIdx].depthFrom;      // ref 层顶面

    double best = T.top - T.totalDepth;                 // 兜底：孔底
    double bestDiff = fabs(best - elev);
    for (size_t i = 0; i < T.layers.size(); ++i)        // 各层底面挑最接近的
    {
        double b = T.top - T.layers[i].depthTo;
        double d = fabs(b - elev);
        if (d < bestDiff) { bestDiff = d; best = b; }
    }
    return best;
}
//
CCreateSectionView::CCreateSectionView(void):CCreateBaseView(),s_ConfigData(NULL),patLoader(NULL)
{
}
//
//CCreateSectionView::~CCreateSectionView(void)
//{
//}
void CCreateSectionView::Execute()
{
	if (!prepare()) return;

	CString cadSectionConfigUrl= strFilePath + _T("project/剖面图参数.json");
	// 解析 JSON（失败时内部已打印错误，这里保留原有业务提示）
    s_ConfigData = CUtils::LoadJsonFile(cadSectionConfigUrl.GetString());   // 静默模式
    if (s_ConfigData == NULL) {
        CUtils::acutPrintf(_T("[CAD ERROR] 没有剖面图配置文件，请检查！ %s\n"), cadSectionConfigUrl.GetString());
        return;
    }
	CString cadSoilConfigUrl= strFilePath + _T("support/pat_define.json");
	// 解析 JSON（失败时内部已打印错误，这里保留原有业务提示）
    s_patConfigData = CUtils::LoadJsonFile(cadSoilConfigUrl.GetString());   // 静默模式
    if (s_patConfigData == NULL) {
        CUtils::acutPrintf(_T("[CAD ERROR] 没有pat配置文件，请检查！ %s\n"), cadSoilConfigUrl.GetString());
        return;
    }

	CString cadLineTypeConfigUrl= strFilePath + _T("support/cad.lin");
	CUtils::LoadAllLinetypesFromFile(cadLineTypeConfigUrl.GetString());


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

	cJSON* item = NULL;
	cJSON* items = CUtils::GetJsonObjectItemUtf8(s_ConfigData,_T("剖面组"));
	if (items == NULL)
	{
		aboutTrans();
		return;
	}
	CString cadPatUrl = CUtils::GetArxFolder() + _T("support\\pat");
	patLoader=new PatHatchLoader();
	patLoader->setDocument(pDoc);
	patLoader->setModelSpace(pModelSpace);
	patLoader->setDatabase(pDb);
	patLoader->RegisterPatFile(cadPatUrl);

	int index = 1;
	cJSON_ArrayForEach(item, items) {
        // 处理每个属性
        if (item != NULL) 
		{
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
	
			cJSON* gpsObj = CUtils::GetJsonObjectItemUtf8(item,_T("勘察组"));

			if (gpsObj == NULL)
				continue;
			
			cJSON* gpObj = NULL;
			double max_zk = 0;
			double min_zk = std::numeric_limits<double>::max();
			cJSON_ArrayForEach(gpObj, gpsObj){
				if (gpObj == NULL || !cJSON_IsString(gpObj)) continue;
				cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);

				{
					cJSON* gpsObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("孔口高程(m)"));
					double top =  CUtils::JsonValToDouble(gpsObj);
					cJSON* layerObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("地层列表"));
					if (layerObj == NULL) continue;
					
					int dataRow = cJSON_GetArraySize(layerObj);
					if (dataRow == 0) continue;
					double totalDepth = 0.0;
					cJSON* rowTry = cJSON_GetArrayItem(layerObj, dataRow - 1);
					cJSON* cTo = CUtils::GetJsonObjectItemUtf8(rowTry,_T("进尺至(m)"));    // “至”=终孔深度
					if (cTo != NULL && cJSON_IsNumber(cTo) && cTo->valuedouble > 0.0)
					{
						totalDepth = cTo->valuedouble;              // ★算术必须用 valuedouble
					}
					double bottom = top - totalDepth;
					if (top>max_zk) max_zk = top;
					if (bottom<min_zk) min_zk=bottom;
				}
			}
			
			int divide = 12;
			double elevation_zero = 208.55;
			if (elevation_zero>max_zk) max_zk = elevation_zero;
			int max_zk_int = static_cast<int>(std::ceil(elevation_zero)); 
			int min_zk_int = (int)min_zk;
			double zk_divide = (double)(divide*10)/(double)(max_zk_int-min_zk_int);

			drawTable(basePt);
			SectionRulerInfo ruler = drawRuler(basePt,min_zk,divide,elevation_zero);
			ruler.index = index;

			drawColumn(basePt,gpsObj,ruler);
			drawLineFix(basePt,item,ruler);
			drawLineDrill(basePt,item,ruler);

			basePt.x +=430.0;
			index ++;

		}
	}


	 this->commitTrans();
	 this->actionEnd();
	 this->regen();

}
void CCreateSectionView::ClearExtDbCache()
{
	if (s_ConfigData)
		cJSON_free(s_ConfigData);
	if (patLoader)
		delete patLoader;

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
SectionRulerInfo CCreateSectionView::drawRuler(AcGePoint3d& basePt,double min_zk,int divide,double elevation_zero)
{
	SectionRulerInfo info;

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
	int max_zk_int = static_cast<int>(std::ceil(elevation_zero)); 

	info.min_zk_int = min_zk_int;
	info.max_zk_int = max_zk_int;
	/*int max_zk_int = (int)(elevation_zero);
	max_zk_int+=1;*/

	double zk_divide = std::ceil((double)(max_zk_int - min_zk_int) / divide);
	info.mPerGrid = zk_divide;
	info.unitPerM   = (height_ruler - 10) / (divide*zk_divide);

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
		if (i ==0 ){
			info.labelX = pt3.x;
			info.labelY = pt3.y;
		}
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

	pt2.x=pt1.x-0.5;
	pt2.y=pt1.y+height_ruler+8.0;

	CUtils::AddDbText(pModelSpace,_T("(1985国家高程基准)"),pt2,3.0,pszStyleName,DBTA_CENTER,1.0);
	pt2.y+=5.0;
	CUtils::AddDbText(pModelSpace,_T("高 程 (m)"),pt2,3.0,pszStyleName,DBTA_CENTER,1.0);
	return info;
}

void CCreateSectionView::drawColumn(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule)
{
	double zk_divide = rule.mPerGrid;
	int min_zk_int = rule.min_zk_int;
	bool is_dt = true;
	int rows = is_dt?3:2;
	double height = 10.0;
	double bottom_tb = 60.0+height*rows+10;
			
	cJSON* gpObj = NULL;

	double left=23.0;
	double width_valid = PAGE_WIDTH-left*2 - 30;

	int gpRows = cJSON_GetArraySize(gpsObj);
	if (gpRows == 0) return;
	
	ACHAR* layerName = _T("地质-钻孔");
	CUtils::EnsureLayer(layerName,true);

	double width_per = (width_valid-23*2) / gpRows;
	double width_b = 3.0;

	AcGePoint3d pt1,pt2;
	double left_pre = basePt.x+left*2+30.0;
	int index=0;

	cJSON_ArrayForEach(gpObj, gpsObj){
		if (gpObj == NULL || !cJSON_IsString(gpObj)) continue;
		cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
		{
			cJSON* elevationObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("孔口高程(m)"));
			if (elevationObj == NULL) continue;
			double top =  CUtils::JsonValToDouble(elevationObj);

			cJSON* layerObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("地层列表"));
			if (layerObj == NULL) continue;
			
			int dataRow = cJSON_GetArraySize(layerObj);
			if (dataRow == 0) continue;
			double totalDepth = 0.0;
			cJSON* rowTry = cJSON_GetArrayItem(layerObj, dataRow - 1);
			cJSON* cTo = CUtils::GetJsonObjectItemUtf8(rowTry,_T("进尺至(m)"));    // “至”=终孔深度
			if (cTo != NULL && cJSON_IsNumber(cTo) && cTo->valuedouble > 0.0)
			{
				totalDepth = cTo->valuedouble;             
			}
			double bottom_max = top - totalDepth;

			rowTry = cJSON_GetArrayItem(layerObj, 0);
			cTo = CUtils::GetJsonObjectItemUtf8(rowTry,_T("进尺自(m)"));    
			totalDepth = CUtils::JsonValToDouble(cTo);
			double bottom_min = top - totalDepth;

			pt1.x = left_pre+index*width_per;
			pt1.y = getRuleHeight(rule,bottom_max);

			pt2.x=pt1.x;
			pt2.y= getRuleHeight(rule,bottom_min);
			std::vector<AcGePoint3d> arrPts;
			arrPts.push_back(pt1);
			arrPts.push_back(pt2);

			//高度尺
			CUtils::AddDbPolyline(pModelSpace,arrPts,0.3);

			pt1.x-=1.5;
			pt2.x=pt1.x+3.0;
			pt2.y = pt1.y;

			arrPts.clear();
			arrPts.push_back(pt1);
			arrPts.push_back(pt2);

			CUtils::AddDbPolyline(pModelSpace,arrPts,0.3);
		}
		index++;
	}

	layerName = _T("地质-文字");
	ACHAR* pszStyleName=_T("层底标注");
	CUtils::EnsureLayer(layerName,true);
	index=0;
	cJSON_ArrayForEach(gpObj, gpsObj){
		if (gpObj == NULL || !cJSON_IsString(gpObj)) continue;
		{
			cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
			cJSON* elevationObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("孔口高程(m)"));
			if (elevationObj == NULL) continue;
			double top =  CUtils::JsonValToDouble(elevationObj);

			cJSON* layerObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("地层列表"));
			cJSON* item;
			cJSON_ArrayForEach(item, layerObj){
				cJSON* bottomObj = CUtils::GetJsonObjectItemUtf8(item,_T("进尺至(m)"));
				if (bottomObj == NULL || !cJSON_IsNumber(bottomObj)) continue;
				double depth = bottomObj->valuedouble;
				double bottom = top - depth;

				pt1.x = left_pre+index*width_per+3.0;
				pt1.y = getRuleHeight(rule,bottom);

				CString txt;
				txt.Format(_T("%.2f(%.2f)"),depth,bottom);
				CUtils::AddDbText(pModelSpace,txt,pt1,1.8,pszStyleName,DBTA_LEFT,0.7);

			}
		}
		index++;
	}

	layerName = _T("地质-填充花纹");
	CUtils::EnsureLayer(layerName,true);
	index=0;
	cJSON_ArrayForEach(gpObj, gpsObj){
		if (gpObj == NULL || !cJSON_IsString(gpObj)) continue;
		{
			cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
			cJSON* elevationObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("孔口高程(m)"));
			if (elevationObj == NULL) continue;
			double top =  CUtils::JsonValToDouble(elevationObj);

			cJSON* layerObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("地层列表"));
			cJSON* item;
			cJSON_ArrayForEach(item, layerObj){
				cJSON* topObj = CUtils::GetJsonObjectItemUtf8(item,_T("进尺自(m)"));
				cJSON* bottomObj = CUtils::GetJsonObjectItemUtf8(item,_T("进尺至(m)"));
				if (bottomObj == NULL || !cJSON_IsNumber(bottomObj)
					|| topObj == NULL || !cJSON_IsNumber(topObj)) continue;

				double depth = topObj->valuedouble;
				double firstBottom=top-depth;

				depth = bottomObj->valuedouble;
				double secondbottom = top - depth;

				cJSON* cSoi = CUtils::GetJsonObjectItemUtf8(item,_T("地层名称"));  // 当前列“土层文字”
				if (cSoi == NULL || !cJSON_IsString(cSoi)) return;
				CString soilPatName;
				{
                    std::vector<CString> arrSeg;
                    if (CUtils::SplitUtf8(cSoi->valuestring, '-', arrSeg) > 1)
                    {
                        soilPatName=arrSeg[1].GetString();
                    }
                }

				TCHAR patName[128] = { 0 };
                if (!soilPatName.IsEmpty() && CUtils::FindPatNameBySoilName(s_patConfigData, soilPatName.GetString(),
                                          patName, ARRAYSIZE(patName)))
                {
					AcGePoint2d r1,r2,r3,r4;
					r1.x = left_pre+index*width_per - 15.0;
					r1.y = getRuleHeight(rule,secondbottom); 

					r2.x = r1.x + 15.0;
					r2.y = r1.y;

					r3.x=r2.x;
					r3.y= getRuleHeight(rule,firstBottom); 

					r4.x = r1.x;
					r4.y = r3.y;

					AcGePoint2dArray rect;                         
                    rect.append(r1);
                    rect.append(r2);
                    rect.append(r3);
                    rect.append(r4);
                    rect.append(r1);

					AcGeDoubleArray bulges;
                    for (int b = 0; b < rect.length(); ++b)     // 变量改名 b，避免遮蔽外层行循环的 i
                        bulges.append(0.0);

                    AcDbObjectId hatchId = AcDbObjectId::kNull;
                    Acad::ErrorStatus es = patLoader->CreateHatchByName(
                        patName, rect, bulges, 0.5, 0.0, NULL, hatchId);
                    if (es != Acad::eOk)
                        CUtils::acutPrintf(_T("[CAD WARN] 填充失败(%d)：岩性 \"%s\" 图例 \"%s\"\n"),
                            (int)es, soilPatName.GetString(), patName);
                }
                else
                {
                    CUtils::acutPrintf(_T("[CAD WARN] pat_define.json 未找到岩性 \"%s\" 的图例名称，跳过填充\n"),
                        soilPatName.GetString());
                }

			}
			index++;
		}
	}

}
void CCreateSectionView::drawLineFix(AcGePoint3d& basePt,cJSON* itemObj,const SectionRulerInfo& rule)
{

	bool is_dt = true;
	int rows = is_dt?3:2;
	double left=23.0;
	double width_valid = PAGE_WIDTH-left*2 - 30;
	double paddingLeft = 30;
	double paddingRight = 7.5;
	double height = 10.0;
	double bottom_tb = 60.0+height*rows+10;

	ACHAR* layerName = _T("地质-水位线");
	CUtils::EnsureLayer(layerName,true);

	std::vector<AcGePoint3d> arrPts;
	AcGePoint3d pt1(basePt),pt2;
	Acad::ErrorStatus es;

	cJSON* zeroObj = CUtils::GetJsonObjectItemUtf8(itemObj,_T("零标高"));
	if (zeroObj && cJSON_IsNumber(zeroObj))
	{
		double bottom = zeroObj->valuedouble;
		pt1.x += left+ paddingLeft + paddingRight;
		pt1.y= getRuleHeight(rule,bottom);

		pt2.x = pt1.x+width_valid - paddingRight*2 ;
		pt2.y= pt1.y;

		arrPts.push_back(pt1);
		arrPts.push_back(pt2);

		CUtils::AddDbPolyline(pModelSpace,arrPts,0.2, false, _T("swx"), 5);

		arrPts.clear();

		CString cadBlockUrl;
		cadBlockUrl = strFilePath + _T("support/block/GC.dwg");

		CString blockName;
		blockName.Format(_T("GC_%02d"),rule.index);

		AcDbObjectId blkDefId = ImportDwgAsBlock(cadBlockUrl.GetString(), blockName);
		if (blkDefId != AcDbObjectId::kNull){
			// 把块内占位文字 "[number]" 替换成水位高程
			CString sVal;
			sVal.Format(_T("(%.2f)"), bottom);          // bottom 即零标高，208.55 这种
			CUtils::replaceBlockText(blkDefId, _T("[number]"), sVal);

			AcGePoint3d ptDraw;

			ptDraw.x = pt1.x + (pt2.x-pt1.x)/2;
			ptDraw.y = pt1.y ;




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
			}else{
				pBlkRef->close();
			}
		}
		
	}

	layerName = _T("地质-地层线");
	CUtils::EnsureLayer(layerName,true);
	
	cJSON* digObj = CUtils::GetJsonObjectItemUtf8(itemObj,_T("开挖标高"));
	if (digObj && cJSON_IsNumber(digObj))
	{
		double bottom = digObj->valuedouble;
		pt1.x = basePt.x + left+ paddingLeft + paddingRight;
		pt1.y= getRuleHeight(rule,bottom);

		pt2.x = pt1.x+width_valid - paddingRight*2 ;
		pt2.y= pt1.y;

		arrPts.push_back(pt1);
		arrPts.push_back(pt2);

		CUtils::AddDbPolyline(pModelSpace,arrPts,0.2, false, _T("swx"), 6);


		AcGePoint3d ptDraw;

		ptDraw.x = pt1.x + (pt2.x-pt1.x)/2;
		ptDraw.y = pt1.y + 3.0 ;
		ACHAR* pszStyleName=_T("SLNormal");
		CUtils::AddDbText(pModelSpace,_T("基础底设计标高线"),ptDraw,3.0,pszStyleName,DBTA_CENTER,0.7);
		//arrPts.clear();


	}

	

	
}
//画钻孔线条
//画钻孔线条（地面线 + 地层连线；每孔两点：柱左缘 x-15 / 柱右缘 x）
void CCreateSectionView::drawLineDrill(AcGePoint3d& basePt,cJSON* itemObj,const SectionRulerInfo& rule)
{
    cJSON* gpsObj = CUtils::GetJsonObjectItemUtf8(itemObj,_T("勘察组"));
    if (gpsObj == NULL) return;

    int gpRows = cJSON_GetArraySize(gpsObj);
    if (gpRows == 0) return;                 // ★不再要求 2 孔：单孔也画柱内横线

    // ---- 横向布点：与 drawColumn 完全一致 ----
    double left = 23.0;
    double width_valid = PAGE_WIDTH - left*2 - 30;
    double width_per  = (width_valid - 23*2) / gpRows;
    double left_pre   = basePt.x + left*2 + 30.0;
    const double COL_W = 15.0;               // 柱宽，与 drawColumn 填充矩形一致

    // ---- 1. 解析勘察组所有钻孔 ----
    std::vector<KsDrillHole> holes;
    {
        cJSON* gpObj = NULL;
        int idx = 0;
        cJSON_ArrayForEach(gpObj, gpsObj){
            if (gpObj == NULL || !cJSON_IsString(gpObj)) { idx++; continue; }
            cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
            if (zkObj == NULL) { idx++; continue; }

            KsDrillHole hole;
            hole.x  = left_pre + idx*width_per;
            hole.xL = hole.x - COL_W;        // ★柱左缘

            cJSON* topObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("孔口高程(m)"));
            if (topObj != NULL) hole.top = CUtils::JsonValToDouble(topObj);

            cJSON* layersObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("地层列表"));
            if (layersObj != NULL)
            {
                cJSON* layerObj = NULL;
                cJSON_ArrayForEach(layerObj, layersObj){
                    cJSON* cBh = CUtils::GetJsonObjectItemUtf8(layerObj,_T("地层编号"));
                    if (cBh == NULL || !cJSON_IsString(cBh)) continue;

                    KsDrillLayer li;
                    TCHAR szCode[64] = { 0 };
                    CUtils::utf8ToTChar(cBh->valuestring, szCode, ARRAYSIZE(szCode));
                    li.code = szCode;
                    li.code.TrimLeft();  li.code.TrimRight();

                    cJSON* cFrom = CUtils::GetJsonObjectItemUtf8(layerObj,_T("进尺自(m)"));
                    if (cFrom != NULL && cJSON_IsNumber(cFrom)) li.depthFrom = cFrom->valuedouble;

                    cJSON* cTo = CUtils::GetJsonObjectItemUtf8(layerObj,_T("进尺至(m)"));
                    if (cTo != NULL && cJSON_IsNumber(cTo)) li.depthTo = cTo->valuedouble;

                    if (li.depthTo > hole.totalDepth) hole.totalDepth = li.depthTo;
                    hole.layers.push_back(li);
                }
            }
            holes.push_back(hole);
            idx++;
        }
    }
    if (holes.empty()) return;

    // ---- 2. 地层编号去重收集 ----
    std::vector<CString> codes;
    for (size_t i = 0; i < holes.size(); ++i)
        for (size_t j = 0; j < holes[i].layers.size(); ++j)
            if (KsFindCode(codes, holes[i].layers[j].code) < 0)
                codes.push_back(holes[i].layers[j].code);

    // ---- 3. 每种地层编号一种随机"线型+颜色" ----
    std::vector<RandomPolylineStyle> styles;
    std::vector<CString> ltNames;            // 与 styles 同下标的线型名
    if (!codes.empty() && CUtils::CreateRandomPolylineStyles(pDb, (int)codes.size(), styles))
    {
        for (size_t i = 0; i < styles.size(); ++i)
            ltNames.push_back(KsLinetypeNameById(styles[i].linetypeId));
    }
    else
    {
        styles.clear();                      // 分配失败 → 全部 ByLayer 默认样式
    }

    // ---- 4. 地面线：每孔 (xL,top)→(x,top)，多孔时自动顺连 ----
    {
        ACHAR* layerName = _T("地质-地面线");
        CUtils::EnsureLayer(layerName,true);

        std::vector<AcGePoint3d> arrPts;
        for (size_t i = 0; i < holes.size(); ++i)
        {
            arrPts.push_back(AcGePoint3d(holes[i].xL, getRuleHeight(rule, holes[i].top), 0.0));
            arrPts.push_back(AcGePoint3d(holes[i].x,  getRuleHeight(rule, holes[i].top), 0.0));
        }
        CUtils::AddDbPolyline(pModelSpace, arrPts, 0.3, false, NULL, 3);
    }

    // ---- 5. 地层线 ----
    {
        ACHAR* layerName = _T("地质-地层线");
        CUtils::EnsureLayer(layerName,true);

        // 5.1 每孔柱内横线（单孔也走这里）
        for (size_t i = 0; i < holes.size(); ++i)
        {
            const KsDrillHole& H = holes[i];
            for (size_t j = 0; j < H.layers.size(); ++j)
            {
                double e = H.top - H.layers[j].depthTo;   // ★实际深度 = 孔口高程 - 进尺至

                std::vector<AcGePoint3d> arrPts;
                arrPts.push_back(AcGePoint3d(H.xL, getRuleHeight(rule, e), 0.0));
                arrPts.push_back(AcGePoint3d(H.x,  getRuleHeight(rule, e), 0.0));

                Adesk::UInt16 colorIdx;
				double lw;
                const ACHAR* pszLt = KsStyleFor(codes, styles, ltNames, H.layers[j].code, colorIdx, lw);
				CUtils::AddDbPolyline(pModelSpace, arrPts, lw, false, pszLt, colorIdx);
            }
        }

        // 5.2 相邻孔跨孔连线：A.x → B.xL（≥2 孔才有）
        for (size_t i = 0; i + 1 < holes.size(); ++i)
        {
            const KsDrillHole& A = holes[i];
            const KsDrillHole& B = holes[i+1];

            std::vector<int> usedB(B.layers.size(), 0);

            // A 有 → 连 B（同层对底；B 缺层则尖灭到 B.xL）
            for (size_t a = 0; a < A.layers.size(); ++a)
            {
                const KsDrillLayer& la = A.layers[a];
                int b = KsFindLayer(B, la.code, usedB);

                double elevA = A.top - la.depthTo;
                double elevB;
                if (b >= 0)
                {
                    usedB[b] = 1;
                    elevB = B.top - B.layers[b].depthTo;
                }
                else
                {
                    int yIdx = -1;
                    if (a + 1 < A.layers.size())
                        yIdx = KsFindLayer(B, A.layers[a+1].code, std::vector<int>());
                    elevB = KsPinchElev(B, yIdx, elevA);
                }

                std::vector<AcGePoint3d> arrPts;
                arrPts.push_back(AcGePoint3d(A.x,  getRuleHeight(rule, elevA), 0.0));
                arrPts.push_back(AcGePoint3d(B.xL, getRuleHeight(rule, elevB), 0.0));

                Adesk::UInt16 colorIdx;
				double lw;                                                   // ★新增
				const ACHAR* pszLt = KsStyleFor(codes, styles, ltNames, la.code, colorIdx, lw);
				CUtils::AddDbPolyline(pModelSpace, arrPts, lw, false, pszLt, colorIdx);
            }

            // B 有 A 无 → 反向尖灭：A.x → B.xL
            for (size_t b2 = 0; b2 < B.layers.size(); ++b2)
            {
                if (usedB[b2] != 0) continue;
                const KsDrillLayer& lb = B.layers[b2];

                double elevB2 = B.top - lb.depthTo;
                int yIdx = -1;
                if (b2 + 1 < B.layers.size())
                    yIdx = KsFindLayer(A, B.layers[b2+1].code, std::vector<int>());
                double elevA2 = KsPinchElev(A, yIdx, elevB2);

                std::vector<AcGePoint3d> arrPts;
                arrPts.push_back(AcGePoint3d(B.xL, getRuleHeight(rule, elevB2), 0.0));
                arrPts.push_back(AcGePoint3d(A.x,  getRuleHeight(rule, elevA2), 0.0));

                Adesk::UInt16 colorIdx;
				double lw;                                                   // ★新增
				const ACHAR* pszLt = KsStyleFor(codes, styles, ltNames, lb.code, colorIdx, lw);
				CUtils::AddDbPolyline(pModelSpace, arrPts, lw, false, pszLt, colorIdx);
            }
        }
    }
}
double CCreateSectionView::getRuleHeight(const SectionRulerInfo& rule,double depth)
{
	return rule.labelY+(depth-rule.min_zk_int)*rule.unitPerM;
}
