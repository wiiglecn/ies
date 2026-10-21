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
    CString name;       // 地层名称（跨孔关联的主键，编号是分孔流水号不可比）       // 地层编号
    double depthFrom;   // 进尺自(m)
    double depthTo;     // 进尺至(m)
    bool isRock;        // ★岩石类地层（地层名称首段为"岩石"，如"岩石-灰岩"）
    KsDrillLayer() : depthFrom(0.0), depthTo(0.0), isRock(false) {}
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
// 按地层名称找该层下标；used 非空时跳过已用过的（防止同一层被复用多次）
static int KsFindLayerByName(const KsDrillHole& hole, const CString& name, const std::vector<int>& used)
{
    if (name.IsEmpty()) return -1;
    for (size_t i = 0; i < hole.layers.size(); ++i)
    {
        if (!used.empty() && used[i] != 0) continue;
        if (hole.layers[i].name.Compare(name) == 0) return (int)i;
    }
    return -1;
}
// 跨孔关联主控：按地层名称匹配；名称缺失才退回编号
static int KsMatchLayer(const KsDrillHole& hole, const KsDrillLayer& la, const std::vector<int>& used)
{
    int i = KsFindLayerByName(hole, la.name, used);
    if (i >= 0) return i;
    return KsFindLayer(hole, la.code, used);
}
// 连续两段是否同一层：先按名称，名称缺失才按编号（同一岩性分多段录入要合并）
static bool KsSameLayerKey(const KsDrillLayer& a, const KsDrillLayer& b)
{
    if (!a.name.IsEmpty() && !b.name.IsEmpty())
        return a.name.Compare(b.name) == 0;
    return a.code.Compare(b.code) == 0;
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

    // 未命中：只在"非末层的层底"里取最接近的；末层底 = 孔底，不能当落点，
    // 否则尖灭线会一路画到孔底（ZK4/ZK5 常见症状）
    double best = elev;                                 // 没有候选时保持水平
    double bestDiff = 1e30;
    for (size_t i = 0; i + 1 < T.layers.size(); ++i)
    {
        double b = T.top - T.layers[i].depthTo;
        double d = fabs(b - elev);
        if (d < bestDiff) { bestDiff = d; best = b; }
    }
    return best;
}
// 同一根线只画一次：A 侧尖灭与 B 侧尖灭可能得到相同端点（如两侧都指向对方泥灰岩顶）
static bool KsSegDrawn(std::vector<CString>& drawn, double x1, double y1, double x2, double y2)
{
    if (x1 > x2 || (fabs(x1 - x2) < 0.01 && y1 > y2))
    { double t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    CString k;
    k.Format(_T("%.2f|%.2f|%.2f|%.2f"), x1, y1, x2, y2);
    for (size_t i = 0; i < drawn.size(); ++i)
        if (drawn[i].Compare(k) == 0) return true;
    drawn.push_back(k);
    return false;
}
//
CCreateSectionView::CCreateSectionView(void):CCreateBaseView(),s_ConfigData(NULL),patLoader(NULL)
{
}
//
//CCreateSectionView::~CCreateSectionView(void)
//{
//}
// 本工程 cad.lin 里的自定义线型定义（dash：正=实线段、负=空白、0=点）
struct KsLtDef
{
    const TCHAR* name;
    int    nDash;
    double dashes[6];
    double total;
};
static const KsLtDef g_ksLtDefs[] = {
    { _T("swx"),              4, {  7.0,    -1.5,  0.0,  -1.5 },                 10.0  },
    { _T("swx_long"),         2, {  12.0,   -5.0 },                              17.0  },
    { _T("KS_LONGGDASH_DOT"), 4, {  12.0,   -3.0,  0.0,  -3.0 },                18.0  },
    { _T("DOT"),              2, {  0.0,    -0.25 },                             0.25  },
    { _T("DOT2"),             4, {  0.0,    -0.25, 0.0,  -0.25 },               0.5   },
    { _T("DASHED_SMALL"),     2, {  0.125,  -0.125 },                           0.25  },
    { _T("DASHED_MEDIUM"),    2, {  0.25,   -0.125 },                           0.375 },
    { _T("DASHED_LARGE"),     2, {  0.5,    -0.25 },                            0.75  },
    { _T("CENTER_LINE"),      4, {  0.75,   -0.125, 0.0,  -0.125 },             1.0   },
    { _T("PHANTOM_LINE"),     6, {  0.75,   -0.125, 0.0, -0.125, 0.0, -0.125 }, 1.25  },
    { _T("BORDER_DASH"),      6, {  0.5,    -0.25,  0.5, -0.25,  0.0, -0.25 },  1.75  }
};

// 确保线型存在：先从 cad.lin 单条加载，加载不到就按定义直接建进线型表
// 必须在 actionBefore() 之后调用（之前没有文档写锁，写线型表会失败）
static void KsEnsureLinetype(AcDbDatabase* pDb, const ACHAR* pszName,
                             int nDash, const double* dashes, double dTotal,
                             const ACHAR* pszLinFile)
{
    if (pDb == NULL || pszName == NULL || pszName[0] == _T('\0')) return;
    AcDbLinetypeTable* pLtTbl = NULL;
    if (pDb->getLinetypeTable(pLtTbl, AcDb::kForRead) != Acad::eOk || pLtTbl == NULL) return;
    AcDbLinetypeTableRecord* pOld = NULL;
    bool bHas = (pLtTbl->getAt(pszName, pOld, AcDb::kForRead) == Acad::eOk);
    if (bHas && pOld != NULL) pOld->close();
    pLtTbl->close();
    if (bHas) return;

    // 1) 从 cad.lin 单条加载（只请求缺失的，避免整文件加载时触发重定义提示）
    if (pszLinFile != NULL && pszLinFile[0] != _T('\0'))
    {
        acdbLoadLineTypeFile(pszName, pszLinFile, NULL);
        if (pDb->getLinetypeTable(pLtTbl, AcDb::kForRead) == Acad::eOk && pLtTbl != NULL)
        {
            AcDbLinetypeTableRecord* pNew = NULL;
            if (pLtTbl->getAt(pszName, pNew, AcDb::kForRead) == Acad::eOk)
            {
                if (pNew != NULL) pNew->close();
                pLtTbl->close();
                return;
            }
                pLtTbl->close();
            }
    }

    // 2) 还缺则直接用 dash 定义建
    if (pDb->getLinetypeTable(pLtTbl, AcDb::kForWrite) != Acad::eOk || pLtTbl == NULL) return;
    AcDbLinetypeTableRecord* pRec = new AcDbLinetypeTableRecord;
    Acad::ErrorStatus es = pRec->setName(pszName);
    if (es == Acad::eOk)
    {
        pRec->setPatternLength(dTotal);
        pRec->setNumDashes(nDash);
        for (int i = 0; i < nDash; ++i) pRec->setDashLengthAt(i, dashes[i]);
        es = pLtTbl->add(pRec);
    }
    if (es == Acad::eOk)
    {
        pRec->close();
        CUtils::acutPrintf(_T("[CAD INFO] 线型 %s 已按定义创建\n"), pszName);
    }
    else
    {
        delete pRec;
        CUtils::acutPrintf(_T("[CAD WARN] 线型 %s 创建失败(%d)，回落 ByLayer\n"), pszName, (int)es);
    }
    pLtTbl->close();
}

// 补齐本工程用到的自定义线型（swx / KS_LONGGDASH_DOT / 点线 / 虚线 等）
static void KsEnsureCustomLinetypes(AcDbDatabase* pDb, const ACHAR* pszLinFile)
{
    if (pDb == NULL) return;
    for (int i = 0; i < (int)(sizeof(g_ksLtDefs) / sizeof(g_ksLtDefs[0])); ++i)
        KsEnsureLinetype(pDb, g_ksLtDefs[i].name, g_ksLtDefs[i].nDash,
                         g_ksLtDefs[i].dashes, g_ksLtDefs[i].total, pszLinFile);
}

void CCreateSectionView::Execute()
{
	if (!prepare()) return;

	// 项目级数据文件：{ project }剖面图参数.json（与平面图、钻孔数据同属一个项目）
	CString cadSectionConfigUrl = CUtils::GetProjectFilePath(_T("剖面图参数"));
	if (cadSectionConfigUrl.IsEmpty())
	{
		CUtils::acutPrintf(_T("[CAD ERROR] 未确定项目，无法定位剖面参数文件\n"));
		return;
	}
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


	// 需要确保存在的图层列表（遍历创建）
    const TCHAR* arrLayers[] = {
        _T("地质-地面线"),   _T("地质-地层线"),   _T("地质-水位线"),
        _T("地质-地层符号"), _T("地质-填充花纹"), _T("地质-钻孔"),
        _T("地质-图框"),     _T("地质-图例"),     _T("地质-标贯"),
        _T("地质-静探"),     _T("地质-标注栏"),   _T("地质-文字"),
        _T("地质-地层编号"), _T("地质-原位曲线")
    };
    const int nLayerCount = sizeof(arrLayers) / sizeof(arrLayers[0]);

	// showAllLayer();
	 CUtils::ClearSelection();
	 if (this->startTrans()) return;

	 // ★ 关键：图层创建必须在 actionBefore() 之后。
	 //   actionBefore()（ToolDrawBase）内部才会执行 acDocManager->lockDocument(pDoc) 拿到文档**写锁**；
	 //   在此之前任何 getLayerTable(kForWrite) 都会因文档未取得写锁而失败，
	 //   CAD 返回 320 = eWasOpenForRead → 14 个图层全部创建失败。
	 //   本项目 ToolLayer::create_layer 就是「先 actionBefore() 再 getLayerTable(kForWrite)」，可正常建层，
	 //   所以这里把建层循环整体挪到 actionBefore() 之后，与之一致。
     int ret = this->actionBefore();
	 if (ret != 0 ) {
	 	aboutTrans();
	 	return;
	 }

	 // 线型补齐必须排在 actionBefore() 之后：之前没有文档写锁，写线型表会失败
	 KsEnsureCustomLinetypes(pDb, cadLineTypeConfigUrl.GetString());

    for (int i = 0; i < nLayerCount; ++i)
    {
        // bSetCurrent=false：批量建层不逐个切换当前层，避免 CLAYER 被反复改写
        if (CUtils::EnsureLayer(arrLayers[i], false).isNull())
        {
            CUtils::acutPrintf(_T("[CAD ERROR] 创建图层失败，请检查！ %s\n"), arrLayers[i]);
            continue;
        }
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
	const int HOLES_PER_PAGE = 5;          // 每页钻孔数
	cJSON_ArrayForEach(item, items) {
        // 处理每个剖面
        if (item != NULL) 
		{
			cJSON* gpsObj = CUtils::GetJsonObjectItemUtf8(item,_T("勘察组"));
			if (gpsObj == NULL) continue;

			int holeCount = cJSON_GetArraySize(gpsObj);
			if (holeCount == 0) continue;
			int pageCount = (holeCount + HOLES_PER_PAGE - 1) / HOLES_PER_PAGE;

			// 整条剖面统一算高程范围,保证分页后各页竖向比例一致
			double max_zk = 0;
			double min_zk = std::numeric_limits<double>::max();
			{
				cJSON* gpObj = NULL;
				cJSON_ArrayForEach(gpObj, gpsObj){
					if (gpObj == NULL || !cJSON_IsString(gpObj)) continue;
					cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
					if (zkObj == NULL) continue;
					cJSON* topObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("孔口高程(m)"));
					double top = CUtils::JsonValToDouble(topObj);
					cJSON* layerObj = CUtils::GetJsonObjectItemUtf8(zkObj,_T("地层列表"));
					if (layerObj == NULL) continue;
					int dataRow = cJSON_GetArraySize(layerObj);
					if (dataRow == 0) continue;
					double totalDepth = 0.0;
					cJSON* rowTry = cJSON_GetArrayItem(layerObj, dataRow - 1);
					cJSON* cTo = CUtils::GetJsonObjectItemUtf8(rowTry,_T("进尺至(m)"));    // 终孔深度
					if (cTo != NULL && cJSON_IsNumber(cTo) && cTo->valuedouble > 0.0)
					{
						totalDepth = cTo->valuedouble;
					}
					double bottom = top - totalDepth;
					if (top>max_zk) max_zk = top;
					if (bottom<min_zk) min_zk=bottom;
				}
			}

			int divide = 12;

			// ★标高基准改由接口提供（剖面参数：零标高 / 开挖标高），不再硬编码 208.55
			//   零标高：±0.000 对应的绝对标高；开挖标高：基础底设计标高（画“基础底设计标高线”）
			//   兼容别名：零点标高；基底标高 / 基础底设计标高
			double elevation_zero = max_zk;      // 接口未给时，退回最高孔口高程
			double elevation_dig  = 0.0;
			bool   bHasZero = false, bHasDig = false;
			{
				cJSON* cZero = CUtils::GetJsonObjectItemUtf8(item, _T("零标高"));
				if (cZero == NULL || !cJSON_IsNumber(cZero))
					cZero = CUtils::GetJsonObjectItemUtf8(item, _T("零点标高"));
				if (cZero != NULL && cJSON_IsNumber(cZero))
				{
					elevation_zero = cZero->valuedouble;
					bHasZero = true;
				}

				cJSON* cDig = CUtils::GetJsonObjectItemUtf8(item, _T("开挖标高"));
				if (cDig == NULL || !cJSON_IsNumber(cDig))
					cDig = CUtils::GetJsonObjectItemUtf8(item, _T("基底标高"));
				if (cDig == NULL || !cJSON_IsNumber(cDig))
					cDig = CUtils::GetJsonObjectItemUtf8(item, _T("基础底设计标高"));
				if (cDig != NULL && cJSON_IsNumber(cDig))
				{
					elevation_dig = cDig->valuedouble;
					bHasDig = true;
				}
			}

			// 标尺顶端必须同时覆盖 最高孔口 / 零标高 / 开挖标高，
			// 否则接口给的标高高于原固定值时会画到标尺之外
			double rulerTop = max_zk;
			if (bHasZero && elevation_zero > rulerTop) rulerTop = elevation_zero;
			if (bHasDig  && elevation_dig  > rulerTop) rulerTop  = elevation_dig;

			CUtils::acutPrintf(_T("[剖面图] 标高基准：零标高=%.2f%s  开挖标高=%.2f%s  标尺顶=%.0f\n"),
				elevation_zero, bHasZero ? _T("") : _T("(接口未给,取最高孔口)"),
				elevation_dig,  bHasDig  ? _T("") : _T("(接口未给,不画基础底设计标高线)"),
				std::ceil(rulerTop));

			// 分页绘制:每页最多 HOLES_PER_PAGE 个钻孔
			for (int pg = 0; pg < pageCount; ++pg)
			{
				int startIdx = pg * HOLES_PER_PAGE;
				int endIdx   = startIdx + HOLES_PER_PAGE;
				if (endIdx > holeCount) endIdx = holeCount;

				// 本页孔名子数组
				cJSON* pageGps = cJSON_CreateArray();
				for (int k = startIdx; k < endIdx; ++k)
				{
					cJSON* one = cJSON_GetArrayItem(gpsObj, k);
					if (one != NULL) cJSON_AddItemToArray(pageGps, cJSON_Duplicate(one, 1));
				}

				m_pageCols = HOLES_PER_PAGE;   // 固定列数,末页不足时右侧留白

				// 图框图签
				{
					AcDbBlockReference* pBlkRef = new AcDbBlockReference;
					pBlkRef->setBlockTableRecord(blkDefId);
					pBlkRef->setPosition(basePt);

					AcDbObjectId refId = AcDbObjectId::kNull;
					es = pModelSpace->appendAcDbEntity(refId, pBlkRef);
					if (es != Acad::eOk)
					{
						CUtils::acutPrintf(_T("\n[DWG导入] 图签块添加块参照失败(错误码 %d)。"), (int)es);
						delete pBlkRef;          // 未入库,delete(不能 close)
						return;
					}
					pBlkRef->close();
				}

				drawHeader(basePt,item,pageGps,pg+1,pageCount);
				drawTable(basePt, pageGps);
				SectionRulerInfo ruler = drawRuler(basePt,min_zk,divide,rulerTop);
				ruler.index = index;

				drawColumn(basePt,pageGps,ruler);
				drawHoleNo(basePt,pageGps,ruler);
				drawLayerName(basePt,pageGps,ruler);
				drawSampleMark(basePt,pageGps,ruler);
				drawLineFix(basePt,item,ruler);
				drawLineDrill(basePt,pageGps,ruler);

				CUtils::acutPrintf(_T("[剖面图] 第 %d 页:第 %d~%d 个孔(本剖面共 %d 孔,分 %d 页)\n"),
					index, startIdx+1, endIdx, holeCount, pageCount);

				basePt.x +=430.0;
				index ++;

				cJSON_Delete(pageGps);
			}
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

void CCreateSectionView::drawTable(AcGePoint3d& basePt, cJSON* gpsObj)
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

	// === 表格数据填充：每个钻孔列对应表中一格，列中心与 drawColumn 柱体中心严格对齐 ===
	struct KsHoleTab { bool valid; double depth; double maxBlow; double x; double y; KsHoleTab():valid(false),depth(0),maxBlow(-1),x(0),y(0){} };
	std::vector<KsHoleTab> holes;
	int n = 0;
	if (gpsObj != NULL) n = cJSON_GetArraySize(gpsObj);
	holes.assign(n, KsHoleTab());
	for (int idx = 0; idx < n; ++idx)
	{
		cJSON* gpObj = cJSON_GetArrayItem(gpsObj, idx);
		if (gpObj == NULL || !cJSON_IsString(gpObj)) continue;
		cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
		if (zkObj == NULL) continue;
		holes[idx].valid = true;
		cJSON* xObj = CUtils::GetJsonObjectItemUtf8(zkObj, _T("X坐标(m)"));
		cJSON* yObj = CUtils::GetJsonObjectItemUtf8(zkObj, _T("Y坐标(m)"));
		if (xObj != NULL) holes[idx].x = CUtils::JsonValToDouble(xObj);
		if (yObj != NULL) holes[idx].y = CUtils::JsonValToDouble(yObj);
		cJSON* layersObj = CUtils::GetJsonObjectItemUtf8(zkObj, _T("地层列表"));
		if (layersObj != NULL)
		{
			int last = cJSON_GetArraySize(layersObj) - 1;
			if (last >= 0)
			{
				cJSON* row = cJSON_GetArrayItem(layersObj, last);
				cJSON* cTo = CUtils::GetJsonObjectItemUtf8(row, _T("进尺至(m)"));
				if (cTo != NULL && cJSON_IsNumber(cTo)) holes[idx].depth = cTo->valuedouble;
			}
			// 击数：取该孔所有原位测试的最大锤击数
			cJSON* ly = NULL;
			cJSON_ArrayForEach(ly, layersObj)
			{
				cJSON* yw = CUtils::GetJsonObjectItemUtf8(ly, _T("原位测试"));
				if (yw == NULL || !cJSON_IsObject(yw)) continue;
				cJSON* bObj = CUtils::GetJsonObjectItemUtf8(yw, _T("锤击数(击)"));
				if (bObj == NULL) continue;
				double b = 0;
				if (cJSON_IsString(bObj))
				{
					// 空串（如 "锤击数(击)": ""）不做计数，避免 atof("")=0 显示成0
					if (bObj->valuestring == NULL || bObj->valuestring[0] == '\0') continue;
					b = atof(bObj->valuestring);
				}
				else if (cJSON_IsNumber(bObj)) b = bObj->valuedouble;
				else continue;
				if (holes[idx].maxBlow < 0 || b > holes[idx].maxBlow) holes[idx].maxBlow = b;
			}
		}
	}
	if (n > 0)
	{
		// 横向布点（与 drawColumn 完全一致）
		double width_valid = PAGE_WIDTH - left*2 - 30;
		double width_per   = (width_valid - 23*2) / n;
		double left_pre    = basePt.x + left*2 + 30.0;
		const double COL_W = 15.0;

		// 钻孔列中心（与 drawColumn 柱中心严格一致）
		std::vector<double> c(n);
		for (int i = 0; i < n; ++i) c[i] = left_pre + i*width_per - COL_W*0.5;

		// 数据区 x 范围
		double dataLeft  = pt1.x + 30.0;
		double dataRight = pt1.x + width_table;

		// 竖向分界线：相邻钻孔柱中心的中点
		pt2.y = pt1.y;
		pt3.y = pt1.y + height*rows;
		for (int i = 1; i < n; ++i)
		{
			double bx = (c[i-1] + c[i]) * 0.5;
			if (bx <= dataLeft || bx >= dataRight) continue;
			pt2.x = bx; pt3.x = bx;
			CUtils::AddLine(pModelSpace, pt2, pt3);
		}

		// 行 y 中心（自下而上：动探击数 / 钻孔间距 / 孔深）
		double yDT  = pt1.y + 4.0;
		double yGap = pt1.y + 12.0;
		double yDep = pt1.y + 20.0;

		for (int i = 0; i < n; ++i)
		{
			if (!holes[i].valid) continue;
			AcGePoint3d ptv;
			ptv.x = c[i];

			// 孔深（顶行）
			CString sDep; sDep.Format(_T("%.2f"), holes[i].depth);
			ptv.y = yDep;
			CUtils::AddDbText(pModelSpace, sDep, ptv, 2.4, pszStyleName, DBTA_CENTER, 1.0);

			// 钻孔间距（中间行；首个孔留空）
			if (i > 0)
			{
				double dx = holes[i].x - holes[i-1].x;
				double dy = holes[i].y - holes[i-1].y;
				double sp = sqrt(dx*dx + dy*dy);
				CString sSp; sSp.Format(_T("%.1f"), sp);
				ptv.y = yGap;
				CUtils::AddDbText(pModelSpace, sSp, ptv, 2.4, pszStyleName, DBTA_CENTER, 1.0);
			}

			// 动探击数（底行；maxBlow>=0 表示有数据）
			if (holes[i].maxBlow >= 0)
			{
				CString sB; sB.Format(_T("%.0f"), holes[i].maxBlow);
				ptv.y = yDT;
				CUtils::AddDbText(pModelSpace, sB, ptv, 2.4, pszStyleName, DBTA_CENTER, 1.0);
			}
		}
	}

	

}
//rulerTopElev = 标尺顶端需覆盖的最高标高（已含零标高/开挖标高/最高孔口，单位 m）；内部向上取整
SectionRulerInfo CCreateSectionView::drawRuler(AcGePoint3d& basePt,double min_zk,int divide,double rulerTopElev)
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

	// 标尺范围:数据最低-1 ~ 数据最高+1,避免顶部留出大段空白
	int min_zk_int = (int)std::floor(min_zk) - 1;
	int max_zk_int = static_cast<int>(std::ceil(rulerTopElev)) + 1;
	int diff_zk    = max_zk_int - min_zk_int;
	if (diff_zk < 1) diff_zk = 1;

	info.min_zk_int = min_zk_int;
	info.max_zk_int = max_zk_int;

	// 每格米数取整齐值(1/2/5/10...),格数自适应:保证顶部刻度 >= 最高标高且超出量最小
	double zk_divide = (double)diff_zk / divide;
	int    nGrid     = divide;
	{
		static const int cand[] = {1,2,5,10,20,25,50,100};
		bool hasBest = false;
		int  bestStep = 0, bestGrid = 0, bestScore = 0;
		for (int ci = 0; ci < (int)(sizeof(cand)/sizeof(cand[0])); ++ci)
		{
			int s = cand[ci];
			int n = (int)std::ceil((double)diff_zk / s);
			if (n < 4 || n > divide + 6) continue;              // 格数太少或太多都不合适
			int blank = min_zk_int + n * s - max_zk_int;        // 顶部多出来的米数
			int score = (n > divide ? n - divide : divide - n) * 10 + blank;
			if (!hasBest || score < bestScore)
			{ bestScore = score; bestStep = s; bestGrid = n; hasBest = true; }
		}
		if (hasBest) { zk_divide = (double)bestStep; nGrid = bestGrid; }
	}
	info.mPerGrid = zk_divide;
	info.unitPerM   = (height_ruler - 10) / (nGrid*zk_divide);
	double gridH = (height_ruler - 10) / nGrid;   // 每格在图面上的高度

	CUtils::acutPrintf(_T("[剖面图] 标尺 %.0f~%.0f  每格%.2fm  共%d格\n"),
		(double)min_zk_int, (double)max_zk_int, zk_divide, nGrid);

	double height_divide=height_ruler/(divide+1);
	double height_txt=2.5;
	double dWidthFactor=1.0;

	pt2.y=pt1.y;
	AcGePoint3d pt3(pt1),pt4(pt1);
	for(int i=0;i<=nGrid;i++){
		pt2.x=pt1.x-7.5;
		pt2.y+=gridH;
		int height_detail=(int)(min_zk_int + i*zk_divide + 0.5);
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
		if (i < nGrid && i % 2 == 0) {
			std::vector<AcGePoint3d> arrPts;
			pt3.x=pt1.x-0.5;
			pt3.y=pt2.y;
			pt4.x=pt3.x;
			pt4.y=pt2.y+gridH;
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

// 相邻两个地层记录是否同一地层编号（编录把一层切成多段时编号相同）
static bool KsSameCode(cJSON* a, cJSON* b)
{
    if (a == NULL || b == NULL) return false;
    cJSON* ca = CUtils::GetJsonObjectItemUtf8(a, _T("地层编号"));
    cJSON* cb = CUtils::GetJsonObjectItemUtf8(b, _T("地层编号"));
    if (ca == NULL || cb == NULL) return false;
    if (!cJSON_IsString(ca) || !cJSON_IsString(cb)) return false;
    return strcmp(ca->valuestring, cb->valuestring) == 0;
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
	int nCols = (m_pageCols > gpRows) ? m_pageCols : gpRows;   // 分页列数
	
	ACHAR* layerName = _T("地质-钻孔");
	CUtils::EnsureLayer(layerName,true);

	double width_per = (width_valid-23*2) / nCols;
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
			int segCount = cJSON_GetArraySize(layerObj);
			// ★薄层防重叠:同一孔内相邻层底标注的竖向间距不得小于 LABEL_GAP,
			//   冲突时把后一个标注依次下移(自上而下的读数顺序不变)
			const double LABEL_GAP = 2.2;      // 文字高 1.8 + 0.4 余量
			double lastLabelY = 0.0;           // 上一个标注的基线 y
			bool bHasLastLabel = false;
			for (int ii = 0; ii < segCount; ++ii)
			{
				cJSON* item = cJSON_GetArrayItem(layerObj, ii);
				if (item == NULL) continue;
				// ★同一地层的连续分段只标整层底面（如 ZK5 的 3-2-0 只标 16.12）
				if (ii + 1 < segCount && KsSameCode(item, cJSON_GetArrayItem(layerObj, ii + 1))) continue;

				cJSON* bottomObj = CUtils::GetJsonObjectItemUtf8(item,_T("进尺至(m)"));
				if (bottomObj == NULL || !cJSON_IsNumber(bottomObj)) continue;
				double depth = bottomObj->valuedouble;
				double bottom = top - depth;

				pt1.x = left_pre+index*width_per+3.0;
				double yLabel = getRuleHeight(rule,bottom);
				if (bHasLastLabel && (lastLabelY - yLabel) < LABEL_GAP)
				{
					yLabel = lastLabelY - LABEL_GAP;     // 薄层:下移到与上一个标注不重叠
				}
				pt1.y = yLabel;
				lastLabelY = yLabel;
				bHasLastLabel = true;

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
			int segCount2 = cJSON_GetArraySize(layerObj);
			cJSON* item;
			for (int ii = 0; ii < segCount2; ++ii)
			{
				item = cJSON_GetArrayItem(layerObj, ii);
				if (item == NULL) continue;
				// ★同一地层的连续分段合并：只有组内最后一段参与填充
				if (ii + 1 < segCount2 && KsSameCode(item, cJSON_GetArrayItem(layerObj, ii + 1))) continue;

				cJSON* topObj = CUtils::GetJsonObjectItemUtf8(item,_T("进尺自(m)"));
				cJSON* bottomObj = CUtils::GetJsonObjectItemUtf8(item,_T("进尺至(m)"));
				if (bottomObj == NULL || !cJSON_IsNumber(bottomObj)
					|| topObj == NULL || !cJSON_IsNumber(topObj)) continue;

				double depth = topObj->valuedouble;
				double firstBottom=top-depth;

				depth = bottomObj->valuedouble;
				double secondbottom = top - depth;

				// ★向前回溯同名段，把填充矩形顶界抬到整层顶面（整层一个花纹，不分块）
				for (int back = ii - 1; back >= 0; --back)
				{
					cJSON* prev = cJSON_GetArrayItem(layerObj, back);
					if (!KsSameCode(prev, item)) break;
					cJSON* pFrom = CUtils::GetJsonObjectItemUtf8(prev,_T("进尺自(m)"));
					if (pFrom != NULL && cJSON_IsNumber(pFrom))
						firstBottom = top - pFrom->valuedouble;
				}

				cJSON* cSoi = CUtils::GetJsonObjectItemUtf8(item,_T("地层名称"));  // 当前列“土层文字”
				// ★原为 return：任一层缺地层名称会让整个 drawColumn 提前退出、后续孔全部不画
				if (cSoi == NULL || !cJSON_IsString(cSoi)) continue;
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
//画钻孔标准表示：孔号（上）— 横线（中）— 孔口高程（下），整体悬在地面线上方
//位置与 drawColumn 的柱体严格对齐：柱右缘 x = left_pre + index*width_per，柱左缘 = x - 15
void CCreateSectionView::drawHoleNo(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule)
{
    int gpRows = cJSON_GetArraySize(gpsObj);
    if (gpRows == 0) return;
    int nCols = (m_pageCols > gpRows) ? m_pageCols : gpRows;   // 分页列数

    double left         = 23.0;
    double width_valid  = PAGE_WIDTH - left*2 - 30;
    double width_per    = (width_valid - 23*2) / nCols;
    double left_pre     = basePt.x + left*2 + 30.0;
    const double COL_W  = 15.0;               // 柱宽，与 drawColumn / drawLineDrill 一致

    ACHAR* layerName = _T("地质-钻孔");
    CUtils::EnsureLayer(layerName, true);

    ACHAR* pszStyleName = _T("SLNormal");

    cJSON* gpObj = NULL;
    int index = 0;
    cJSON_ArrayForEach(gpObj, gpsObj)
    {
        if (gpObj == NULL || !cJSON_IsString(gpObj)) { index++; continue; }

        cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
        if (zkObj == NULL) { index++; continue; }
        cJSON* elevationObj = CUtils::GetJsonObjectItemUtf8(zkObj, _T("孔口高程(m)"));
        if (elevationObj == NULL) { index++; continue; }
        double top = CUtils::JsonValToDouble(elevationObj);

        // 孔号：JSON 里是 UTF-8，需转 TCHAR（MBCS/Unicode 双编译通吃）
        TCHAR szNo[64] = { 0 };
        CUtils::utf8ToTChar(gpObj->valuestring, szNo, ARRAYSIZE(szNo));

        double xR  = left_pre + index*width_per;          // 柱右缘
        double xL  = xR - COL_W;                          // 柱左缘
        double lineY = getRuleHeight(rule, top);          // 地面线 y

        // ① 孔号（最上层）
        AcGePoint3d pt(xR - COL_W*0.5, lineY + 8.5, 0.0);
        CUtils::AddDbText(pModelSpace, szNo, pt, 3.0, pszStyleName, DBTA_CENTER, 0.8);

        // ② 横线（中间）：比柱宽两端各伸出 3 个单位
        std::vector<AcGePoint3d> arrPts;
        arrPts.push_back(AcGePoint3d(xL - 3.0, lineY + 7.0, 0.0));
        arrPts.push_back(AcGePoint3d(xR + 3.0, lineY + 7.0, 0.0));
        CUtils::AddDbPolyline(pModelSpace, arrPts, 0.0);

        // ③ 孔口高程（横线下方）
        CString sElev;
        sElev.Format(_T("%.2f"), top);
        pt.y = lineY + 3.0;
        CUtils::AddDbText(pModelSpace, sElev, pt, 2.5, pszStyleName, DBTA_CENTER, 0.8);

        index++;
    }
}
// ---- 时代成因标识（层名后带框代号，如 Q4 上标 al）：由"成分及其他"里的地层年代/堆积年代/地层成因推算 ----
//取描述里 "关键字:值" 的值（到 , ， ; ； 为止）
static bool KsDescField(const CString& sDesc, const CString& sKey, CString& vOut)
{
    vOut.Empty();
    int pos = sDesc.Find(sKey);
    if (pos < 0) return false;
    int k = pos + sKey.GetLength();
    while (k < sDesc.GetLength()
        && (sDesc[k] == _T(':') || sDesc[k] == _T('：') || sDesc[k] == _T(' '))) k++;
    while (k < sDesc.GetLength()
        && sDesc[k] != _T(',') && sDesc[k] != _T('，')
        && sDesc[k] != _T(';') && sDesc[k] != _T('；'))
    { vOut += sDesc[k]; k++; }
    vOut.Trim();
    return !vOut.IsEmpty();
}

//由 地层年代/堆积年代/地层成因 推时代成因代号，识别成功返回 true（如 "Q\Sal^4;"）
//MText 堆叠格式 \S上^下;：成因代号在上标、年代数字在下标
static bool KsEraIdentity(const CString& sEra, const CString& sAccu, const CString& sCause,
                          CString& sMain, CString& sSub, CString& sSup, CString& sTail)
{
    sMain.Empty(); sSub.Empty(); sSup.Empty(); sTail.Empty();
    if (sEra.IsEmpty()) return false;
    TCHAR szBase[4] = { 0 };
    if (sEra.Find(_T("第四")) >= 0)      szBase[0] = _T('Q');
    else if (sEra.Find(_T("石炭")) >= 0) szBase[0] = _T('C');
    else if (sEra.Find(_T("二叠")) >= 0) szBase[0] = _T('P');
    else if (sEra.Find(_T("三叠")) >= 0) szBase[0] = _T('T');
    else if (sEra.Find(_T("侏罗")) >= 0) szBase[0] = _T('J');
    else if (sEra.Find(_T("白垩")) >= 0) szBase[0] = _T('K');
    else if (sEra.Find(_T("奥陶")) >= 0) szBase[0] = _T('O');
    else if (sEra.Find(_T("志留")) >= 0) szBase[0] = _T('S');
    else if (sEra.Find(_T("泥盆")) >= 0) szBase[0] = _T('D');
    else return false;

    if (sEra.Find(_T("全新")) >= 0 || sAccu.Find(_T("全新")) >= 0
        || sAccu.Find(_T("近十年")) >= 0) sSub = _T("4");
    else if (sEra.Find(_T("上更新")) >= 0) sSub = _T("3");
    else if (sEra.Find(_T("中更新")) >= 0) sSub = _T("2");
    else if (sEra.Find(_T("下更新")) >= 0) sSub = _T("1");
    else if (sEra.Find(_T("更新")) >= 0)   sSub = _T("3");
    else if (sEra.Find(_T("下统")) >= 0)   sSub = _T("1");
    else if (sEra.Find(_T("中统")) >= 0)   sSub = _T("2");
    else if (sEra.Find(_T("上统")) >= 0)   sSub = _T("3");


    // 阶代号:完整年代描述如"石炭系下统岩关阶" -> C1Y;与成因互斥(有阶就画阶)
    if      (sEra.Find(_T("岩关阶")) >= 0)   sTail = _T("Y");
    else if (sEra.Find(_T("大塘阶")) >= 0)   sTail = _T("D");
    else if (sEra.Find(_T("摆佐阶")) >= 0)   sTail = _T("B");
    else if (sEra.Find(_T("滑石板阶")) >= 0) sTail = _T("H");
    else if (sEra.Find(_T("达拉阶")) >= 0)   sTail = _T("Dl");
    else if (sEra.Find(_T("马平阶")) >= 0)   sTail = _T("M");
    else if (sEra.Find(_T("栖霞阶")) >= 0)   sTail = _T("Q");
    else if (sEra.Find(_T("茅口阶")) >= 0)   sTail = _T("M");
    else if (sEra.Find(_T("龙潭阶")) >= 0)   sTail = _T("L");
    else if (sEra.Find(_T("长兴阶")) >= 0)   sTail = _T("C");
    else if (sEra.Find(_T("大冶阶")) >= 0)   sTail = _T("D");
    else if (sEra.Find(_T("嘉陵江阶")) >= 0) sTail = _T("J");
    else if (sEra.Find(_T("巴东阶")) >= 0)   sTail = _T("B");
    else if (sEra.Find(_T("莲花山阶")) >= 0) sTail = _T("L");
    else if (sEra.Find(_T("那高岭阶")) >= 0) sTail = _T("N");
    else if (sEra.Find(_T("郁江阶")) >= 0)   sTail = _T("Y");
    else if (sEra.Find(_T("东岗岭阶")) >= 0) sTail = _T("D");
    else if (sEra.Find(_T("融县阶")) >= 0)   sTail = _T("R");
    else if (sEra.Find(_T("珍珠冲阶")) >= 0) sTail = _T("Z");
    else if (sEra.Find(_T("自流井阶")) >= 0) sTail = _T("Zi");
    else if (sEra.Find(_T("新田沟阶")) >= 0) sTail = _T("X");
    else if (sEra.Find(_T("沙溪庙阶")) >= 0) sTail = _T("S");
    else if (sEra.Find(_T("遂宁阶")) >= 0)   sTail = _T("Sn");
    else if (sEra.Find(_T("蓬莱镇阶")) >= 0) sTail = _T("P");

    if (sTail.IsEmpty())
    {
        if (sCause.Find(_T("冲洪积")) >= 0)      sSup = _T("al+pl");
        else if (sCause.Find(_T("残坡积")) >= 0) sSup = _T("el+dl");
        else if (sCause.Find(_T("填土")) >= 0)   sSup = _T("ml");
        else if (sCause.Find(_T("冲积")) >= 0)   sSup = _T("al");
        else if (sCause.Find(_T("洪积")) >= 0)   sSup = _T("pl");
        else if (sCause.Find(_T("坡积")) >= 0)   sSup = _T("dl");
        else if (sCause.Find(_T("残积")) >= 0)   sSup = _T("el");
        else if (sCause.Find(_T("湖积")) >= 0)   sSup = _T("l");
        else if (sCause.Find(_T("海积")) >= 0)   sSup = _T("m");
        else if (sCause.Find(_T("风积")) >= 0)   sSup = _T("eol");
        else if (sCause.Find(_T("冰积")) >= 0)   sSup = _T("gl");
        else if (sCause.Find(_T("沼泽")) >= 0)   sSup = _T("h");
    }


    sMain = szBase;
    return true;
}

//直接画出时代成因代号：主字母 + 下标年代 + 上标成因，排成 Q4ml 的样式（4 在下、ml 在上）
//不用 MText 堆叠、不用 Unicode 上下标字形，三段都是 ASCII，任何字体（含 SHX）都能正常出图
//地层代号专用字体:正体罗马字(Windows 自带),XScale 固定为 1,宽度完全由实体 widthFactor 决定
//避免西文部分被 SLNormal 里的 SHX 单线体带偏(C1Y、Q4ml 这类代号很吃字形)
static const ACHAR* KS_ERA_STYLE = _T("KS_ERA");
static const ACHAR* KS_ERA_FONT  = _T("Times New Roman");

//画一段代号并返回它在图面上的真实宽度;量不到时才退回按字数估算
static double KsDrawEraSeg(AcDbBlockTableRecord* pMS, std::vector<AcDbObjectId>& ids,
                           const CString& s, double x, double y, double h,
                           const ACHAR* pszStyle, double wf)
{
    if (s.IsEmpty()) return 0.0;
    AcDbObjectId id = CUtils::AddDbText(pMS, s, AcGePoint3d(x, y, 0.0), h,
                                        pszStyle, DBTA_LEFT, wf);
    if (id == AcDbObjectId::kNull) return s.GetLength() * h * 0.62;
    ids.push_back(id);
    double w = 0.0, hh = 0.0;
    if (CUtils::GetEntExtentById(id, w, hh) && w > 0.01) return w;
    return s.GetLength() * h * 0.62;
}

//整体横向平移:末孔层名靠右时代号要右端到 ptAnchor,画完才知道总宽,故反向平移补偿
static void KsShiftEraTexts(const std::vector<AcDbObjectId>& ids, double dx)
{
    if (fabs(dx) < 0.001) return;
    AcGeMatrix3d mat = AcGeMatrix3d::translation(AcGeVector3d(dx, 0.0, 0.0));
    for (size_t i = 0; i < ids.size(); ++i)
    {
        AcDbEntity* pEnt = NULL;
        if (acdbOpenObject(pEnt, ids[i], AcDb::kForWrite) == Acad::eOk)
        {
            pEnt->transformBy(mat);
            pEnt->close();
        }
    }
}

//直接画出时代成因代号:主字母 + 下标统 + 阶 + 上标成因
//不用 MText 堆叠、不用 Unicode 上下标字形,各段都是 ASCII,任何字体都不会出方框
static void KsDrawEraCode(AcDbBlockTableRecord* pMS, const CString& sMain, const CString& sSub,
                          const CString& sSup, const AcGePoint3d& ptAnchor,
                          bool bToLeft, const ACHAR* pszStyle, const CString& sTail)
{
    if (pMS == NULL || sMain.IsEmpty()) return;

    const ACHAR* pszEra = pszStyle;
    if (!CUtils::EnsureTextStyle(KS_ERA_STYLE, KS_ERA_FONT, 0.0, 1.0).isNull())
        pszEra = KS_ERA_STYLE;

    const double H  = 2.0;    // 主字母字高
    const double HS = 1.2;    // 上下标字高
    const double HT = 1.5;    // 阶代号字高（与 C 同基线）
    const double WF = 0.8;    // 宽高比

    double x = ptAnchor.x;
    double y = ptAnchor.y;
    std::vector<AcDbObjectId> ids;
    x += KsDrawEraSeg(pMS, ids, sMain, x, y,        H,  pszEra, WF);
    x += KsDrawEraSeg(pMS, ids, sSub,  x, y - 0.4,  HS, pszEra, WF);
    x += KsDrawEraSeg(pMS, ids, sTail, x, y,        HT, pszEra, WF);
    x += KsDrawEraSeg(pMS, ids, sSup,  x, y + 1.05, HS, pszEra, WF);

    if (bToLeft) KsShiftEraTexts(ids, ptAnchor.x - x);
}


//画土层序号+名称：每根柱体右侧、每个整层层高中部标注（如 "② 黏土"）
//序号 = 地层编号首位（"2-0-0"→2→②）；名称 = 地层名称 '-' 后部分（"黏性土-黏土"→黏土）
//同一地层的连续分段合并后只标一次，位置取整层层高中点
void CCreateSectionView::drawLayerName(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule)
{
    int gpRows = cJSON_GetArraySize(gpsObj);
    if (gpRows == 0) return;
    int nCols = (m_pageCols > gpRows) ? m_pageCols : gpRows;   // 分页列数

    double left         = 23.0;
    double width_valid  = PAGE_WIDTH - left*2 - 30;
    double width_per    = (width_valid - 23*2) / nCols;
    double left_pre     = basePt.x + left*2 + 30.0;
    const double COL_W  = 15.0;               // 柱宽，与 drawColumn / drawHoleNo 一致

    ACHAR* layerName = _T("地质-文字");
    CUtils::EnsureLayer(layerName, true);

    ACHAR* pszStyleName = _T("SLNormal");

    static const ACHAR* circled[] = {
        _T("①"),_T("②"),_T("③"),_T("④"),_T("⑤"),
        _T("⑥"),_T("⑦"),_T("⑧"),_T("⑨"),_T("⑩")
    };

    cJSON* gpObj = NULL;
    int index = 0;
    cJSON_ArrayForEach(gpObj, gpsObj)
    {
        if (gpObj == NULL || !cJSON_IsString(gpObj)) { index++; continue; }

        cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
        if (zkObj == NULL) { index++; continue; }
        cJSON* layerObj = CUtils::GetJsonObjectItemUtf8(zkObj, _T("地层列表"));
        if (layerObj == NULL) { index++; continue; }

        cJSON* elevationObj = CUtils::GetJsonObjectItemUtf8(zkObj, _T("孔口高程(m)"));
        if (elevationObj == NULL) { index++; continue; }
        double top = CUtils::JsonValToDouble(elevationObj);

        int segCount = cJSON_GetArraySize(layerObj);
        // 本页最后一列(第 5 个钻孔):土层名称改放柱体左侧并右对齐,
        // 避免名称向右伸出图幅、与右侧高程/标尺等数据重叠
        bool bLastCol = (index == nCols - 1);
        double x = bLastCol ? (left_pre + index*width_per - COL_W - 1.5)   // 柱左缘左侧
                            : (left_pre + index*width_per + 1.5);          // 柱右缘右侧
        int hAlign = bLastCol ? DBTA_RIGHT : DBTA_LEFT;

        for (int ii = 0; ii < segCount; ++ii)
        {
            cJSON* item = cJSON_GetArrayItem(layerObj, ii);
            if (item == NULL) continue;
            // 同一地层连续分段：只标组内首段
            if (ii > 0 && KsSameCode(item, cJSON_GetArrayItem(layerObj, ii - 1))) continue;

            cJSON* cFrom = CUtils::GetJsonObjectItemUtf8(item, _T("进尺自(m)"));
            cJSON* cTo   = CUtils::GetJsonObjectItemUtf8(item, _T("进尺至(m)"));
            if (cFrom == NULL || !cJSON_IsNumber(cFrom)
                || cTo == NULL || !cJSON_IsNumber(cTo)) continue;

            // 向后吃掉同编号段，得到整层顶/底深度
            double depthTop = cFrom->valuedouble;
            double depthBottom = cTo->valuedouble;
            for (int nx = ii + 1; nx < segCount && KsSameCode(item, cJSON_GetArrayItem(layerObj, nx)); ++nx)
            {
                cJSON* nTo = CUtils::GetJsonObjectItemUtf8(cJSON_GetArrayItem(layerObj, nx), _T("进尺至(m)"));
                if (nTo != NULL && cJSON_IsNumber(nTo)) depthBottom = nTo->valuedouble;
            }
            // ★薄层不标土层名称:整层画高不足 4.6 个图面单位时,名称会与上下层底标注重叠
            if ((depthBottom - depthTop) * rule.unitPerM < 4.6) continue;
            double depthMid = (depthTop + depthBottom) / 2.0;

            // 序号 = 地层编号首位
            cJSON* cNo = CUtils::GetJsonObjectItemUtf8(item, _T("地层编号"));
            if (cNo == NULL || !cJSON_IsString(cNo)) continue;
            CString sSeq(cNo->valuestring);
            int dash = sSeq.Find(_T('-'));
            if (dash > 0) sSeq = sSeq.Left(dash);
            long seq = _ttol(sSeq);
            if (seq <= 0) continue;

            // 土层名称 = 地层名称 '-' 后部分（JSON 是 UTF-8，需先转 TCHAR 再切 '-'）
            TCHAR szName[128] = { 0 };
            cJSON* cSoi = CUtils::GetJsonObjectItemUtf8(item, _T("地层名称"));
            if (cSoi != NULL && cJSON_IsString(cSoi) && cSoi->valuestring[0] != '\0')
            {
                TCHAR szSoi[256] = { 0 };
                CUtils::utf8ToTChar(cSoi->valuestring, szSoi, ARRAYSIZE(szSoi));
                CString sName(szSoi);
                int d2 = sName.Find(_T('-'));
                if (d2 >= 0 && d2 + 1 < sName.GetLength()) sName = sName.Mid(d2 + 1);
                _tcsncpy(szName, sName, ARRAYSIZE(szName) - 1);
            }

            CString txt;
            if (seq >= 1 && seq <= 10)
                txt.Format(_T("%s %s"), circled[seq - 1], szName);
            else
                txt.Format(_T("(%d) %s"), (int)seq, szName);

            // ★getRuleHeight 接收标高（不是进尺）：换算 elevation = 孔口高程 - 进尺
            double elevMid = top - depthMid;
            AcGePoint3d pt(x, getRuleHeight(rule, elevMid), 0.0);
            AcDbObjectId txtId = CUtils::AddDbText(pModelSpace, txt, pt, 2.2, pszStyleName, hAlign, 0.8);

            // 时代成因标识：描述里带 地层年代/地层成因 时，层名后接 MText 堆叠代号（如 Q4 上标 al）
            CString sDesc;
            cJSON* cDes = CUtils::GetJsonObjectItemUtf8(item, _T("成分及其他"));
            if (cDes != NULL && cJSON_IsString(cDes) && cDes->valuestring[0] != '\0')
            {
                TCHAR szDes[512] = { 0 };
                CUtils::utf8ToTChar(cDes->valuestring, szDes, ARRAYSIZE(szDes));
                sDesc = szDes;
            }
            CString sEra, sAccu, sCause, sMain, sSub, sSup, sTail;
            KsDescField(sDesc, _T("地层年代"), sEra);
            KsDescField(sDesc, _T("堆积年代"), sAccu);
            KsDescField(sDesc, _T("地层成因"), sCause);
            if (txtId != AcDbObjectId::kNull
                && KsEraIdentity(sEra, sAccu, sCause, sMain, sSub, sSup, sTail))
            {
                double nameW = 0.0, nameH = 0.0;
                CUtils::GetEntExtentById(txtId, nameW, nameH);
                if (nameW <= 0.01) nameW = txt.GetLength() * 1.6;   // 量不到宽度时按字数估
                const double GAP = 1.0;
                // 末孔层名靠左（右对齐），代号接层名左侧；其余孔接层名右侧
                AcGePoint3d ptCode(bLastCol ? (pt.x - nameW - GAP) : (pt.x + nameW + GAP),
                                   pt.y, 0.0);
                KsDrawEraCode(pModelSpace, sMain, sSub, sSup, ptCode, bLastCol, pszStyleName, sTail);
            }
        }
        index++;
    }
}
//==================== 柱内标志（取样/风化）— 符号样式来自用户图例 2026-10-20 ====================
//红色实心填充（SOLID 预定义图案）
static bool KsAddSolidHatch(AcDbBlockTableRecord* pMS, const AcGePoint2dArray& loop, AcDbObjectId& hatchId)
{
    hatchId = AcDbObjectId::kNull;
    if (pMS == NULL || loop.length() < 3) return false;
    AcDbHatch* pHatch = new AcDbHatch();
    pHatch->setNormal(AcGeVector3d::kZAxis);
    pHatch->setElevation(0.0);
    Acad::ErrorStatus es = pHatch->setPattern(AcDbHatch::kPreDefined, _T("SOLID"));
    if (es == Acad::eOk) es = pHatch->setPatternScale(1.0);
    AcGeDoubleArray bulges;
    for (int i = 0; i < loop.length(); ++i) bulges.append(0.0);
    if (es == Acad::eOk) es = pHatch->appendLoop(AcDbHatch::kDefault, loop, bulges);
    if (es == Acad::eOk) es = pHatch->evaluateHatch();
    if (es == Acad::eOk) es = pHatch->setColorIndex(1);   // 红色
    if (es == Acad::eOk) es = pMS->appendAcDbEntity(hatchId, pHatch);
    if (es == Acad::eOk) pHatch->close(); else delete pHatch;
    return es == Acad::eOk;
}

//红色文字：柱内标识的孔内深度标注，颜色与标识一致（1=红）
//AddDbText 没有颜色参数，取回实体 id 后改色
static void KsDrawRedText(AcDbBlockTableRecord* pMS, const CString& sTxt,
                          const AcGePoint3d& pt, double h,
                          const ACHAR* pszStyle, int hAlign)
{
    if (pMS == NULL || sTxt.IsEmpty()) return;
    AcDbObjectId id = CUtils::AddDbText(pMS, sTxt.GetString(), pt, h, pszStyle, hAlign, 0.7);
    if (id == AcDbObjectId::kNull) return;
    AcDbEntity* pEnt = NULL;
    if (acdbOpenObject(pEnt, id, AcDb::kForWrite) == Acad::eOk && pEnt != NULL)
    {
        pEnt->setColorIndex(1);      // 与标识同色（红）
        pEnt->close();
    }
}
//原状样：红色实心圆（直径 2.4）
static void KsDrawSampleCircle(AcDbBlockTableRecord* pMS, double cx, double cy)
{
    const double R = 1.2;
    const int N = 16;
    AcGePoint2dArray loop;
    for (int i = 0; i < N; ++i)
    {
        double a = 6.28318530717958 * i / N;
        loop.append(AcGePoint2d(cx + R*cos(a), cy + R*sin(a)));
    }
    AcDbObjectId id;
    KsAddSolidHatch(pMS, loop, id);
}

//岩样：红色实心方块（边长 2.4）
static void KsDrawSampleSquare(AcDbBlockTableRecord* pMS, double cx, double cy)
{
    const double H = 1.2;
    AcGePoint2dArray loop;
    loop.append(AcGePoint2d(cx - H, cy - H));
    loop.append(AcGePoint2d(cx + H, cy - H));
    loop.append(AcGePoint2d(cx + H, cy + H));
    loop.append(AcGePoint2d(cx - H, cy + H));
    AcDbObjectId id;
    KsAddSolidHatch(pMS, loop, id);
}

//风化程度符号：竖杆 + 顶部开叉 + n 条横杠（红色）
//横杠数：微风化1 / 中等风化(中风化)2 / 强风化3 / 全风化4（图例只给了 2、3，其余按风化越强杠越多递推）
static void KsDrawWeathering(AcDbBlockTableRecord* pMS, double cx, double cy, int nBars)
{
    if (nBars < 1) nBars = 2;
    const double HALF_BAR = 0.6;   // 横杠半长
    std::vector<AcGePoint3d> arr;
    // 竖杆
    arr.push_back(AcGePoint3d(cx, cy - 1.2, 0.0));
    arr.push_back(AcGePoint3d(cx, cy + 0.85, 0.0));
    CUtils::AddDbPolyline(pMS, arr, 0.0, false, NULL, 1);
    // 顶部开叉（两根斜线向上张开）
    arr.clear();
    arr.push_back(AcGePoint3d(cx, cy + 0.85, 0.0));
    arr.push_back(AcGePoint3d(cx - 0.7, cy + 1.5, 0.0));
    CUtils::AddDbPolyline(pMS, arr, 0.0, false, NULL, 1);
    arr.clear();
    arr.push_back(AcGePoint3d(cx, cy + 0.85, 0.0));
    arr.push_back(AcGePoint3d(cx + 0.7, cy + 1.5, 0.0));
    CUtils::AddDbPolyline(pMS, arr, 0.0, false, NULL, 1);
    // 横杠：均匀布置在 cy+0.45 ~ cy-0.85
    double yTop = cy + 0.45;
    double yBot = cy - 0.85;
    for (int i = 0; i < nBars; ++i)
    {
        double yb = (nBars == 1) ? yTop : (yTop + (yBot - yTop) * i / (nBars - 1));
        arr.clear();
        arr.push_back(AcGePoint3d(cx - HALF_BAR, yb, 0.0));
        arr.push_back(AcGePoint3d(cx + HALF_BAR, yb, 0.0));
        CUtils::AddDbPolyline(pMS, arr, 0.0, false, NULL, 1);
    }
}

//解析取样深度 "13-13.3"（兼容全角横线/一/波浪号），取中点；无横线按单值处理
static bool KsParseSampleDepth(CString s, double& dMid)
{
    s.Replace(_T('－'), _T('-'));
    s.Replace(_T('—'), _T('-'));
    s.Replace(_T('一'), _T('-'));
    s.Replace(_T('～'), _T('-'));
    s.Replace(_T('~'), _T('-'));
    s.Trim();
    double a = 0.0, b = 0.0;
    int dash = s.Find(_T('-'));
    if (dash >= 0)
    {
        a = CUtils::CStringToDouble(s.Left(dash));
        b = CUtils::CStringToDouble(s.Mid(dash + 1));
    }
    else
        a = CUtils::CStringToDouble(s);
    if (a <= 0.0 && b <= 0.0) return false;
    if (b > 0.0 && b < a) { double t = a; a = b; b = t; }
    dMid = (b > 0.0) ? ((a + b) / 2.0) : a;
    return true;
}

//风化程度 -> 横杠数（0=无）：先查 成分及其他 的"风化程度:"，再用地层名称兜底
static int KsWeatheringBars(const CString& sDesc, const CString& sName)
{
    CString v;
    int pos = sDesc.Find(_T("风化程度"));
    if (pos >= 0)
    {
        int k = pos + 4;   // "风化程度" 4 个字符
        while (k < sDesc.GetLength() && (sDesc[k] == _T(':') || sDesc[k] == _T('：') || sDesc[k] == _T(' '))) k++;
        while (k < sDesc.GetLength() && sDesc[k] != _T(',') && sDesc[k] != _T('，')
            && sDesc[k] != _T(';') && sDesc[k] != _T('；')) { v += sDesc[k]; k++; }
    }
    if (v.Find(_T("全风化")) >= 0) return 4;
    if (v.Find(_T("强风化")) >= 0) return 3;
    if (v.Find(_T("中风化")) >= 0) return 2;
    if (v.Find(_T("微风化")) >= 0) return 1;
    if (sName.Find(_T("全风化")) >= 0) return 4;
    if (sName.Find(_T("强风化")) >= 0) return 3;
    if (sName.Find(_T("中风化")) >= 0) return 2;
    if (sName.Find(_T("微风化")) >= 0) return 1;
    return 0;
}

//柱内标志条目（取样/风化），用于同孔内竖向避让
//  kind: 1=原状样● 2=岩样■ 3=风化符号
struct KsMarkItem
{
    double yRaw;     // 按真实深度算出的理论基线 y
    double y;        // 避让后的实际基线 y
    double depth;    // 孔内深度（用于左侧文字）
    int    kind;
    int    bars;     // 风化横杠数（kind=3 时有效）
};

//画钻孔柱内标志（图例 2026-10-20）：
//  取样：原状样=红色实心圆、岩样=红色实心方块，画在取样深度中点；水样(QS)图例未定义暂不画
//  风化：开叉+横杠符号，画在该分段层高中点；同层连续分段且风化相同只画一次
//★同一孔内所有标志先收集再统一排版：自上而下排序后级联避让（最小间距 MARK_GAP），
//  避免薄层处取样点与风化符号、或相邻两层的取样点竖向重叠
void CCreateSectionView::drawSampleMark(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule)
{
    int gpRows = cJSON_GetArraySize(gpsObj);
    if (gpRows == 0) return;
    int nCols = (m_pageCols > gpRows) ? m_pageCols : gpRows;   // 分页列数

    double left = 23.0;
    double width_valid = PAGE_WIDTH - left*2 - 30;
    double width_per = (width_valid - 23*2) / nCols;
    double left_pre = basePt.x + left*2 + 30.0;

    ACHAR* symLayer = _T("地质-符号");
    CUtils::EnsureLayer(symLayer, true);

    // 标志竖向最小间距：圆/方块直径 2.4、风化符号高约 2.7，取 3.2 留 0.5 余量
    const double MARK_GAP = 3.2;

    cJSON* gpObj = NULL;
    int index = 0;
    cJSON_ArrayForEach(gpObj, gpsObj)
    {
        if (gpObj == NULL || !cJSON_IsString(gpObj)) { index++; continue; }
        cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
        if (zkObj == NULL) { index++; continue; }
        cJSON* layerObj = CUtils::GetJsonObjectItemUtf8(zkObj, _T("地层列表"));
        if (layerObj == NULL) { index++; continue; }
        cJSON* elevationObj = CUtils::GetJsonObjectItemUtf8(zkObj, _T("孔口高程(m)"));
        if (elevationObj == NULL) { index++; continue; }
        double top = CUtils::JsonValToDouble(elevationObj);

        double xC = left_pre + index*width_per - 7.5;   // 柱体水平中心（柱右缘-7.5）

        std::vector<KsMarkItem> marks;                  // 本孔全部标志
        int segCount = cJSON_GetArraySize(layerObj);
        for (int ii = 0; ii < segCount; ++ii)
        {
            cJSON* item = cJSON_GetArrayItem(layerObj, ii);
            if (item == NULL) continue;
            cJSON* cFrom = CUtils::GetJsonObjectItemUtf8(item, _T("进尺自(m)"));
            cJSON* cTo   = CUtils::GetJsonObjectItemUtf8(item, _T("进尺至(m)"));
            if (cFrom == NULL || !cJSON_IsNumber(cFrom)
                || cTo == NULL || !cJSON_IsNumber(cTo)) continue;

            // ---- 地层名称（判断岩石层；名称也可能带风化，如"岩石-强风化泥岩"）----
            TCHAR szName[128] = { 0 };
            bool bRock = false;
            cJSON* cSoi = CUtils::GetJsonObjectItemUtf8(item, _T("地层名称"));
            if (cSoi != NULL && cJSON_IsString(cSoi) && cSoi->valuestring[0] != _T('\0'))
            {
                CUtils::utf8ToTChar(cSoi->valuestring, szName, ARRAYSIZE(szName));
                bRock = (_tcsncmp(szName, _T("岩石"), 2) == 0);
            }

            // ---- 1) 取样符号（先只收集，统一排版后再画）----
            cJSON* cSample = CUtils::GetJsonObjectItemUtf8(item, _T("取岩(土、水)样"));
            if (cSample == NULL) cSample = CUtils::GetJsonObjectItemUtf8(item, _T("取样(土、水)样"));
            if (cSample != NULL && cJSON_IsObject(cSample))
            {
                cJSON* cDep = CUtils::GetJsonObjectItemUtf8(cSample, _T("取样深度"));
                cJSON* cNo  = CUtils::GetJsonObjectItemUtf8(cSample, _T("编号"));
                if (cDep != NULL && cJSON_IsString(cDep) && cDep->valuestring[0] != _T('\0'))
                {
                    double dMid = 0.0;
                    if (KsParseSampleDepth(CString(cDep->valuestring), dMid))
                    {
                        bool bWater = false;
                        if (!bRock)
                        {
                            CString sNo;
                            if (cNo != NULL && cJSON_IsString(cNo) && cNo->valuestring[0] != '\0')
                            {
                                TCHAR szNo[64] = { 0 };
                                CUtils::utf8ToTChar(cNo->valuestring, szNo, ARRAYSIZE(szNo));
                                sNo = szNo;
                            }
                            bWater = (sNo.Find(_T("QS")) >= 0);   // 水样(QS)图例未定义，暂不画
                        }
                        if (!bWater)
                        {
                            KsMarkItem mk;
                            mk.yRaw  = getRuleHeight(rule, top - dMid);
                            mk.y     = mk.yRaw;
                            mk.depth = dMid;
                            mk.kind  = bRock ? 2 : 1;
                            mk.bars  = 0;
                            marks.push_back(mk);
                        }
                    }
                }
            }

            // ---- 2) 风化程度符号 ----
            int nBars = KsWeatheringBars(_T(""), CString(szName));
            {
                TCHAR szDesc[512] = { 0 };
                cJSON* cDesc = CUtils::GetJsonObjectItemUtf8(item, _T("成分及其他"));
                if (cDesc != NULL && cJSON_IsString(cDesc))
                    CUtils::utf8ToTChar(cDesc->valuestring, szDesc, ARRAYSIZE(szDesc));
                int nb = KsWeatheringBars(CString(szDesc), _T(""));
                if (nb > 0) nBars = nb;   // 成分及其他优先，名称兜底
            }
            if (nBars > 0)
            {
                // 同层连续分段且风化相同 -> 只画一次
                bool bDup = false;
                if (ii > 0)
                {
                    cJSON* prev = cJSON_GetArrayItem(layerObj, ii - 1);
                    if (prev != NULL && KsSameCode(prev, item))
                    {
                        int nb2 = 0;
                        TCHAR szD2[512] = { 0 }, szN2[128] = { 0 };
                        cJSON* cD2 = CUtils::GetJsonObjectItemUtf8(prev, _T("成分及其他"));
                        if (cD2 != NULL && cJSON_IsString(cD2))
                            CUtils::utf8ToTChar(cD2->valuestring, szD2, ARRAYSIZE(szD2));
                        cJSON* cN2 = CUtils::GetJsonObjectItemUtf8(prev, _T("地层名称"));
                        if (cN2 != NULL && cJSON_IsString(cN2))
                            CUtils::utf8ToTChar(cN2->valuestring, szN2, ARRAYSIZE(szN2));
                        nb2 = KsWeatheringBars(CString(szD2), CString(szN2));
                        bDup = (nb2 == nBars);
                    }
                }
                if (!bDup)
                {
                    double dMid = (cFrom->valuedouble + cTo->valuedouble) / 2.0;
                    KsMarkItem mk;
                    mk.yRaw  = getRuleHeight(rule, top - dMid);
                    mk.y     = mk.yRaw;
                    mk.depth = dMid;
                    mk.kind  = 3;
                    mk.bars  = nBars;
                    marks.push_back(mk);
                }
            }
        }

        // ---- 统一排版：自上而下（y 由大到小）排序，再级联避让 ----
        int nMark = (int)marks.size();
        for (int a = 0; a < nMark; ++a)
        {
            for (int b = a + 1; b < nMark; ++b)
            {
                if (marks[b].yRaw > marks[a].yRaw)
                {
                    KsMarkItem t = marks[a]; marks[a] = marks[b]; marks[b] = t;
                }
            }
        }
        // 孔底限制：避让下移不越过孔底
        double yBottom = 0.0;
        bool bHasBottom = false;
        {
            cJSON* last = cJSON_GetArrayItem(layerObj, segCount - 1);
            if (last != NULL)
            {
                cJSON* cLastTo = CUtils::GetJsonObjectItemUtf8(last, _T("进尺至(m)"));
                if (cLastTo != NULL && cJSON_IsNumber(cLastTo))
                {
                    yBottom = getRuleHeight(rule, top - cLastTo->valuedouble);
                    bHasBottom = true;
                }
            }
        }
        for (int a = 0; a < nMark; ++a)
        {
            if (a > 0 && (marks[a - 1].y - marks[a].y) < MARK_GAP)
                marks[a].y = marks[a - 1].y - MARK_GAP;   // 与上一个标志挤在一起时依次下移
            if (bHasBottom && marks[a].y < yBottom)
                marks[a].y = yBottom;                     // 兜底：不画出孔底（极端密集时接受重叠）
        }

        // ---- 按排版结果绘制标志与左侧深度文字（同红色）----
        for (int a = 0; a < nMark; ++a)
        {
            double y = marks[a].y;
            double xText;
            if (marks[a].kind == 2)
            {
                KsDrawSampleSquare(pModelSpace, xC, y);       // 岩样 ■
                xText = xC - 2.0;
            }
            else if (marks[a].kind == 1)
            {
                KsDrawSampleCircle(pModelSpace, xC, y);       // 原状样 ●
                xText = xC - 2.0;
            }
            else
            {
                KsDrawWeathering(pModelSpace, xC, y, marks[a].bars);
                xText = xC - 1.2;
            }

            CString sDep;
            sDep.Format(_T("%.2f"), marks[a].depth);
            KsDrawRedText(pModelSpace, sDep, AcGePoint3d(xText, y - 0.55, 0.0),
                          1.5, _T("SLNormal"), DBTA_RIGHT);
        }

        index++;
    }
}

//画表头（每页，版式来自用户截图 2026-10-20；表头整体水平居中、竖向不贴顶）：
//  第一行：工程地质剖面图 + 剖面编号（总页数------当前页码′，如 8------3′），行下画下划线
//  第二行：比例尺  水平 1:水平比例  垂直 1:垂直比例
//  第三行：工程名称（黄色，取本页第一个钻孔的 工程名称）
void CCreateSectionView::drawHeader(AcGePoint3d& basePt,cJSON* itemObj,cJSON* gpsObj,int pageNo,int pageCount)
{
    ACHAR* layerName = _T("地质-文字");
    CUtils::EnsureLayer(layerName,true);
    ACHAR* pszStyleName=_T("SLNormal");

    const double H_TITLE = 7.0;                            // 标题字高
    const double H_SUB   = 4.2;                            // 副行字高
    const double centerX = basePt.x + PAGE_WIDTH / 2.0;    // 图幅水平中心
    double yTitle = basePt.y + 272.0;                      // 顶部留白区内下移，不贴顶

    // 1) 标题 + 剖面编号：编号优先取接口的「标题」（如 A-A），多页时补组内页码 -N
    CString sNo;
    CString sSectionTitle;
    {
        cJSON* cTitle = CUtils::GetJsonObjectItemUtf8(itemObj, _T("标题"));
        if (cTitle != NULL && cTitle->valuestring != NULL && cTitle->valuestring[0] != '\0')
        {
            TCHAR szTitle[128] = { 0 };
            if (CUtils::utf8ToTChar(cTitle->valuestring, szTitle, ARRAYSIZE(szTitle)))
                sSectionTitle = szTitle;
        }
    }
    if (!sSectionTitle.IsEmpty())
    {
        sNo = sSectionTitle;
        if (pageCount > 1)
        {
            CString sSuffix;
            sSuffix.Format(_T("-%d"), pageNo);
            sNo += sSuffix;
        }
    }
    else
    {
        // 接口未提供标题时才回退页码编号，并告警
        CUtils::acutPrintf(_T("[剖面图 WARN] 剖面组未提供「标题」，编号暂用页码形式\n"));
        sNo.Format(_T("%d------%d′"), pageCount, pageNo);
    }

    // 估宽用于整体居中（汉字宽≈字高；数字/′≈0.6 倍字高，横线≈0.45 倍）
    double wTitle = 7.0 * H_TITLE;                          // “工程地质剖面图”7 个汉字
    double wNo = 0.0;
    for (int k = 0; k < sNo.GetLength(); ++k)
    {
        TCHAR ch = sNo[k];
        if (ch == _T('-'))          wNo += 0.45 * H_TITLE;
        else if (ch > (TCHAR)0x2E80) wNo += 1.00 * H_TITLE;   // 中日韩字符按全宽估
        else                         wNo += 0.60 * H_TITLE;
    }
    const double GAP = 16.0;
    double xTitle = centerX - (wTitle + GAP + wNo) / 2.0;
    double xNo    = xTitle + wTitle + GAP;

    CUtils::AddDbText(pModelSpace,_T("工程地质剖面图"),
        AcGePoint3d(xTitle,yTitle,0.0),H_TITLE,pszStyleName,DBTA_LEFT,1.0);
    CUtils::AddDbText(pModelSpace,sNo.GetString(),
        AcGePoint3d(xNo,yTitle,0.0),H_TITLE,pszStyleName,DBTA_LEFT,1.0);

    // 标题行下划线（两端各外伸 5）
    {
        AcGePoint3d p1(xTitle - 5.0, yTitle - 3.0, 0.0);
        AcGePoint3d p2(xNo + wNo + 5.0, yTitle - 3.0, 0.0);
        CUtils::AddLine(pModelSpace,p1,p2);
    }

    // 页码副标注：总页数------当前页码′（与接口编号并存）
    if (pageCount > 1)
    {
        CString sPage;
        sPage.Format(_T("%d------%d′"), pageCount, pageNo);
        CUtils::AddDbText(pModelSpace, sPage.GetString(),
            AcGePoint3d(centerX, yTitle - 8.0, 0.0), H_SUB, pszStyleName, DBTA_CENTER, 1.0);
    }

    // 2) 比例尺行（水平居中）
    double yScale = yTitle - 13.0;
    {
        int nH = 500, nV = 100;
        cJSON* cH = CUtils::GetJsonObjectItemUtf8(itemObj,_T("水平比例"));
        if (cH != NULL && cJSON_IsNumber(cH) && cH->valuedouble > 0) nH = (int)cH->valuedouble;
        cJSON* cV = CUtils::GetJsonObjectItemUtf8(itemObj,_T("垂直比例"));
        if (cV != NULL && cJSON_IsNumber(cV) && cV->valuedouble > 0) nV = (int)cV->valuedouble;
        CString sScale;
        sScale.Format(_T("比例尺  水平 1:%d     垂直 1:%d"), nH, nV);
        CUtils::AddDbText(pModelSpace,sScale.GetString(),
            AcGePoint3d(centerX,yScale,0.0),H_SUB,pszStyleName,DBTA_CENTER,1.0);
    }

    // 3) 工程名称（黄色，水平居中；取本页第一个钻孔的 工程名称）
    double yProj = yScale - 10.0;
    if (gpsObj != NULL)
    {
        CString sProj;
        cJSON* gpObj = NULL;
        cJSON_ArrayForEach(gpObj, gpsObj)
        {
            if (gpObj == NULL || !cJSON_IsString(gpObj)) continue;
            cJSON* zkObj = cJSON_GetObjectItem(s_pJsonData, gpObj->valuestring);
            if (zkObj == NULL) continue;
            cJSON* cP = CUtils::GetJsonObjectItemUtf8(zkObj,_T("工程名称"));
            if (cP != NULL && cJSON_IsString(cP) && cP->valuestring[0] != '\0')
            {
                TCHAR szP[128] = { 0 };
                CUtils::utf8ToTChar(cP->valuestring, szP, ARRAYSIZE(szP));
                sProj = szP;
            }
            break;   // 只取第一个孔的工程名称
        }
        if (!sProj.IsEmpty())
        {
            AcDbObjectId idTxt = CUtils::AddDbText(pModelSpace,sProj.GetString(),
                AcGePoint3d(centerX,yProj,0.0),H_SUB,pszStyleName,DBTA_CENTER,1.0);
            if (idTxt != AcDbObjectId::kNull)
            {
                AcDbEntity* pEnt = NULL;
                if (acdbOpenObject(pEnt,idTxt,AcDb::kForWrite) == Acad::eOk && pEnt != NULL)
                {
                    pEnt->setColorIndex(2);   // 黄色，与截图一致
                    pEnt->close();
                }
            }
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

	// 零标高（接口提供；兼容别名 零点标高）。缺失则不画该线，避免用错误基准
	cJSON* zeroObj = CUtils::GetJsonObjectItemUtf8(itemObj,_T("零标高"));
	if (zeroObj == NULL || !cJSON_IsNumber(zeroObj))
		zeroObj = CUtils::GetJsonObjectItemUtf8(itemObj,_T("零点标高"));
	if (zeroObj && cJSON_IsNumber(zeroObj))
	{
		double bottom = zeroObj->valuedouble;
		pt1.x += left+ paddingLeft + paddingRight;
		pt1.y= getRuleHeight(rule,bottom);

		pt2.x = pt1.x+width_valid - paddingRight*2 ;
		pt2.y= pt1.y;

		arrPts.push_back(pt1);
		arrPts.push_back(pt2);

		// 标高线统一用单点长划线（颜色 5 蓝）
		CUtils::AddDbPolyline(pModelSpace,arrPts,0.2, false, _T("KS_LONGGDASH_DOT"), 5);

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
	
	// 开挖标高 = 基础底设计标高（接口提供；兼容别名 基底标高 / 基础底设计标高）
	cJSON* digObj = CUtils::GetJsonObjectItemUtf8(itemObj,_T("开挖标高"));
	if (digObj == NULL || !cJSON_IsNumber(digObj))
		digObj = CUtils::GetJsonObjectItemUtf8(itemObj,_T("基底标高"));
	if (digObj == NULL || !cJSON_IsNumber(digObj))
		digObj = CUtils::GetJsonObjectItemUtf8(itemObj,_T("基础底设计标高"));
	if (digObj == NULL || !cJSON_IsNumber(digObj))
		CUtils::acutPrintf(_T("[剖面图] 接口未提供 开挖标高(基底标高)，本页不画基础底设计标高线\n"));
	if (digObj && cJSON_IsNumber(digObj))
	{
		double bottom = digObj->valuedouble;
		pt1.x = basePt.x + left+ paddingLeft + paddingRight;
		pt1.y= getRuleHeight(rule,bottom);

		pt2.x = pt1.x+width_valid - paddingRight*2 ;
		pt2.y= pt1.y;

		arrPts.push_back(pt1);
		arrPts.push_back(pt2);

		// 标高线统一用单点长划线（颜色 6 洋红）
		CUtils::AddDbPolyline(pModelSpace,arrPts,0.2, false, _T("KS_LONGGDASH_DOT"), 6);


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
void CCreateSectionView::drawLineDrill(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule)
{
    if (gpsObj == NULL) return;

    int gpRows = cJSON_GetArraySize(gpsObj);
    if (gpRows == 0) return;                 // ★不再要求 2 孔：单孔也画柱内横线

    // ---- 横向布点：与 drawColumn 完全一致 ----
    double left = 23.0;
    double width_valid = PAGE_WIDTH - left*2 - 30;
    int nCols = (m_pageCols > gpRows) ? m_pageCols : gpRows;   // 分页列数
    double width_per  = (width_valid - 23*2) / nCols;
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

                    // ★岩石类判定：地层名称形如"岩石-灰岩"，取 '-' 前的分类段
                    cJSON* cName = CUtils::GetJsonObjectItemUtf8(layerObj,_T("地层名称"));
                    if (cName != NULL && cJSON_IsString(cName))
                    {
                        TCHAR szNm[128] = { 0 };
                        CUtils::utf8ToTChar(cName->valuestring, szNm, ARRAYSIZE(szNm));
                        li.name = szNm;
                        li.name.TrimLeft();  li.name.TrimRight();
                        std::vector<CString> arrSeg;
                        if (CUtils::SplitUtf8(cName->valuestring, '-', arrSeg) > 0)
                            li.isRock = (arrSeg[0].Compare(_T("岩石")) == 0);
                    }

                    if (li.depthTo > hole.totalDepth) hole.totalDepth = li.depthTo;
                    hole.layers.push_back(li);
                }

                // ★同一地层在编录里被切成多个连续同名段时必须合并为一层
                //   （如 ZK5 的 3-2-0 连着出现 4 段：3.1-4.0-7.0-9.0-16.12）。
                //   不合并的后果：①柱内画出同一层内部的假分界线；
                //   ②与邻孔配对时同编号被反复匹配，在柱间连出多条穿层的假连线。
                if (hole.layers.size() > 1)
                {
                    std::vector<KsDrillLayer> merged;
                    for (size_t k = 0; k < hole.layers.size(); ++k)
                    {
                        if (!merged.empty() && KsSameLayerKey(merged.back(), hole.layers[k]))
                            merged.back().depthTo = hole.layers[k].depthTo;   // 同层 → 延伸底界
                        else
                            merged.push_back(hole.layers[k]);
                    }
                    hole.layers.swap(merged);
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
        // 地面线：与标高线同线型（单点长划线），颜色 3 绿区分
        CUtils::AddDbPolyline(pModelSpace, arrPts, 0.3, false, _T("KS_LONGGDASH_DOT"), 3);
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
                // ★最下面一层的底面 = 孔底，按需求不画（柱底开放不封口）
                if (j + 1 >= H.layers.size()) continue;

                double e = H.top - H.layers[j].depthTo;   // ★实际深度 = 孔口高程 - 进尺至

                std::vector<AcGePoint3d> arrPts;
                arrPts.push_back(AcGePoint3d(H.xL, getRuleHeight(rule, e), 0.0));
                arrPts.push_back(AcGePoint3d(H.x,  getRuleHeight(rule, e), 0.0));

                Adesk::UInt16 colorIdx;
				double lw;
                const ACHAR* pszLt = KsStyleFor(codes, styles, ltNames, H.layers[j].code, colorIdx, lw);
                // ★土层/岩石分界线：紧邻下方是岩石类地层时，固定用带点的虚线 swx
                if (H.layers[j+1].isRock)
                    pszLt = _T("swx");
				CUtils::AddDbPolyline(pModelSpace, arrPts, lw, false, pszLt, colorIdx);
            }
        }

        // 5.2 相邻孔跨孔连线：A.x → B.xL（≥2 孔才有）
        for (size_t i = 0; i + 1 < holes.size(); ++i)
        {
            const KsDrillHole& A = holes[i];
            const KsDrillHole& B = holes[i+1];

            std::vector<int> usedB(B.layers.size(), 0);
            std::vector<CString> drawnSegs;   // 已画过的线段（端点相同不重复画）

            // A 有 → 连 B（同层对底；B 缺层则尖灭到 B.xL）
            for (size_t a = 0; a < A.layers.size(); ++a)
            {
                const KsDrillLayer& la = A.layers[a];

                // ★A 最后一层的底面 = 孔底，跨孔连线同样不画
                if (a + 1 >= A.layers.size()) continue;

                int b = KsMatchLayer(B, la, usedB);
                // 对方同名层已延伸到孔底（是末层）时，这根分界线在对方并不存在，
                // 不能连到对方孔底，改走尖灭：如 ZK4 强风化泥岩 与 ZK5 泥灰岩 同编号 3-2-0 的旧误连
                if (b >= 0 && (size_t)(b + 1) >= B.layers.size())
                    b = -1;

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
                        yIdx = KsMatchLayer(B, A.layers[a+1], std::vector<int>());
                    elevB = KsPinchElev(B, yIdx, elevA);
                }

                double yA = getRuleHeight(rule, elevA);
                double yB = getRuleHeight(rule, elevB);
                if (!KsSegDrawn(drawnSegs, A.x, yA, B.xL, yB))
                {
                    std::vector<AcGePoint3d> arrPts;
                    arrPts.push_back(AcGePoint3d(A.x, yA, 0.0));
                    arrPts.push_back(AcGePoint3d(B.xL, yB, 0.0));

                    Adesk::UInt16 colorIdx;
                    double lw;
                    const ACHAR* pszLt = KsStyleFor(codes, styles, ltNames, la.code, colorIdx, lw);
                    bool rockTop = A.layers[a+1].isRock
                                || (b >= 0 && (size_t)(b+1) < B.layers.size() && B.layers[b+1].isRock);
                    if (rockTop)
                        pszLt = _T("swx");
                    CUtils::AddDbPolyline(pModelSpace, arrPts, lw, false, pszLt, colorIdx);
                }
            }

            // B 有 A 无 → 反向尖灭：A.x → B.xL
            for (size_t b2 = 0; b2 < B.layers.size(); ++b2)
            {
                if (usedB[b2] != 0) continue;
                const KsDrillLayer& lb = B.layers[b2];

                // ★B 最后一层的底面 = 孔底，跨孔连线同样不画
                if (b2 + 1 >= B.layers.size()) continue;

                double elevB2 = B.top - lb.depthTo;
                int yIdx = -1;
                if (b2 + 1 < B.layers.size())
                    yIdx = KsMatchLayer(A, B.layers[b2+1], std::vector<int>());
                double elevA2 = KsPinchElev(A, yIdx, elevB2);

                double yA = getRuleHeight(rule, elevB2);
                double yB = getRuleHeight(rule, elevA2);
                if (!KsSegDrawn(drawnSegs, B.xL, yA, A.x, yB))
                {
                    std::vector<AcGePoint3d> arrPts;
                    arrPts.push_back(AcGePoint3d(B.xL, yA, 0.0));
                    arrPts.push_back(AcGePoint3d(A.x, yB, 0.0));

                    Adesk::UInt16 colorIdx;
                    double lw;
                    const ACHAR* pszLt = KsStyleFor(codes, styles, ltNames, lb.code, colorIdx, lw);
                    bool rockTop = B.layers[b2+1].isRock
                                || (yIdx >= 0 && (size_t)(yIdx+1) < A.layers.size() && A.layers[yIdx+1].isRock);
                    if (rockTop)
                        pszLt = _T("swx");
                    CUtils::AddDbPolyline(pModelSpace, arrPts, lw, false, pszLt, colorIdx);
                }
            }
        }
    }
}
double CCreateSectionView::getRuleHeight(const SectionRulerInfo& rule,double depth)
{
	return rule.labelY+(depth-rule.min_zk_int)*rule.unitPerM;
}
