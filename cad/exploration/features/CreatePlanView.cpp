#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include <float.h>         // _finite（ApplyPlotScale/ComputeRegistration 数值合法性校验）
#include "cJSON.h"
#include "utils.h"
#include "CreatePlanView.h"

// ============================================================================
// 匿名 namespace：底图相关常量与 BaseMapCfg 结构（蓝图 §2.4 冻结，详设 V1.6 §3）
// ============================================================================
namespace {
// 底图整体层（流程二"底图-现状"；ApplyPlotScale 三层集合之一）
const TCHAR* kBaseMapLayer          = _T("底图-现状");
// 底图固定块名（幂等重建删除依据——重复执行必须先 EraseBlock 删定义）
const TCHAR* kBaseMapBlock          = _T("委托方底图");
// 既有：钻孔层（出图比例统一缩放纳入）
const TCHAR* kPlanHoleLayer         = _T("勘察点方案");
// 既有：图例层（出图比例统一缩放纳入）
const TCHAR* kLegendLayer           = _T("勘察点图例");
// 流程二预留图层名（按蓝图 §2.4 冻结保留为常量；本版未启用，仅用于后续 PR 参照）
const TCHAR* kBaseMapProposedLayer  = _T("底图-拟建建筑物");
const TCHAR* kBaseMapShadowLayer    = _T("底图-拟建阴影");
// 底图置灰色（dim=true 时设到块参照；详见 DimBaseMapReference）
const Adesk::UInt16 kDimColor       = 253;

// 底图配置（蓝图 §2.4 冻结）；ImportBaseMap 写入、ApplyPlotScale 读取 BASEPOINT
struct BaseMapCfg {
    CString     dwg;                              // 底图 DWG 路径（相对 ARX_DIR）
    double      insertX       = 0.0;              // 块插入点 X（米）
    double      insertY       = 0.0;                // 块插入点 Y（米）
    double      scale        = 1.0;               // 对齐缩放（坐标对齐用，与 plot_scale 分离）
    double      rotation     = 0.0;               // 对齐旋转（弧度）
    CString     layer        = kBaseMapLayer;     // 块参照所在图层
    bool        dim          = false;             // 是否置灰
    double      plotScale    = 0.0;               // 出图比例分母（<=0 = 未配置）
    // BASEPOINT：源 DWG 块基点；==(0,0) 且 hasBasePoint=true 视为按原点处理
    AcGePoint3d sourceBasePoint = AcGePoint3d::kOrigin;
    bool        hasBasePoint = false;             // 是否成功读出 BASEPOINT
};

// 遍历模型空间，按"层名 ∈ 给定层名集合"过滤累加 extent。
// 返回是否成功（任一实体可读即视为成功；空集返回 false）。
static bool ComputeLayerUnionBounds(AcDbBlockTableRecord* pBtr,
                                    const TCHAR* const* layers, int nLayers,
                                    AcDbExtents& extOut)
{
    if (pBtr == NULL || layers == NULL || nLayers <= 0) return false;

    bool bAny = false;
    AcDbBlockTableRecordIterator* pIt = NULL;
    if (pBtr->newIterator(pIt) != Acad::eOk || pIt == NULL) return false;

    AcDbEntity* pEnt = NULL;
    bool bQuit = false;
    for (; !pIt->done() && !bQuit; pIt->step()) {
        if (pIt->getEntity(pEnt, AcDb::kForRead) != Acad::eOk) continue;
        for (int i = 0; i < nLayers; ++i) {
            if (CompareNoCase(pEnt->layer(), layers[i]) == 0) {
                AcDbExtents extEnt;
                if (pEnt->getGeomExtents(extEnt) == Acad::eOk) {
                    if (!bAny) {
                        extOut = extEnt;
                        bAny = true;
                    } else {
                        extOut.addPoint(extEnt.minPoint());
                        extOut.addPoint(extEnt.maxPoint());
                    }
                }
                break;
            }
        }
        pEnt->close();
    }
    delete pIt;
    return bAny;
}

}  // namespace

// ============================================================================
// 构造
// ============================================================================
CCreatePlanView::CCreatePlanView():CCreateBaseView()
{
	pszStyleName = _T("勘察孔");
	m_pBaseMapBlkRefId = AcDbObjectId::kNull;
}

// ============================================================================
// Execute（V1.6 P0-4：在 actionBefore 之后插入 DrawBaseMap；图例之后插入 ApplyPlotScale）
// ============================================================================
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

    // ★ V1.6 P0-4：底图导入 + 对齐（流程二路径；流程一无底图时 DrawBaseMap()
    //   内部打印告警并跳过，调用结果安全，不阻断后续布孔；详见 §5.1）
    DrawBaseMap();

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

    // ★ V1.6 P0-4：出图比例——对整体成图统一缩放。
    //   流程二缩放"底图几何 + 钻孔 + 图例"三层；
    //   流程一缩放"钻孔 + 图例"两层（"底图-现状"层不存在循环自然跳过）；
    //   m_plotScale<=0（未配置）时内部直接返回，不缩放。
    ApplyPlotScale(m_plotScale);

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

// ============================================================================
// CreateLegend（V1.6 P0-4 / P0-7 / P1-5 三改：双层并集求范围 + 图例文字去"及层数"）
// ============================================================================
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

	// ---- V1.6 P0-4 §5.1 + P1-5：图例范围改按"勘察点方案 + 勘察点图例"双层并集求 ----
	//   原状取 GetModelSpaceBounds 会把用户图内既有其它实体也算进来（图例被"绑架"
	//   到大底图下方），现在限定为本命令产生的两张图层（自身 + 钻孔），更收敛。
	//   流程一无底图时：两层即本图全部成果实体；若有意外为空回退到 GetModelSpaceBounds
	//   （保持与现状一致的兜底行为）。
    const TCHAR* kLegendRangeLayers[2] = { kPlanHoleLayer, kLegendLayer };
    AcDbExtents extAll;
    bool bGotLayers = ComputeLayerUnionBounds(this->pModelSpace, kLegendRangeLayers, 2, extAll);
    if (!bGotLayers)
    {
        // 兜底取全模型空间范围（流程一首次创建、无任何成果实体时也走这里）；
        // 行为与旧版一致，避免图例无法定位。
        if (!CUtils::GetModelSpaceBounds(this->pModelSpace, extAll))
        {
            CUtils::acutPrintf(_T("[CAD ERROR] 图形范围为空，无法定位图例\n"));
            return;
        }
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

	// V1.6 P0-7：图例文字去"及层数"——已有建筑物（去"及层数"）
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
		// ★ V1.6 P0-7：去"及层数"，仅保留"已有建筑物"
		CUtils::AddDbText(pModelSpace,_T("已有建筑物"),
								ptDraw_txt,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);


		ptDraw.y -=10.5;
	}

	// V1.6 P0-7：图例文字去"及层数"——拟建建筑物（去"及层数"）
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
		// ★ V1.6 P0-7：去"及层数"，仅保留"拟建建筑物"
		CUtils::AddDbText(pModelSpace,_T("拟建建筑物"),
								ptDraw_txt,height_txt,pszStyleName,DBTA_LEFT,dWidthFactor);


		ptDraw.y -=10.5;
	}

}

// ============================================================================
// V1.6 P0-4 §5.1：DrawBaseMap 主入口（流程一/二共用）
// ============================================================================
bool CCreatePlanView::DrawBaseMap()
{
    // 1. 清旧底图（幂等：重复生成可重建）
    EraseBaseMapLayers();

    // 2. 加载 plan_base.json（静默模式，失败不阻断）
    CString url = strFilePath + _T("project/plan_base.json");
    cJSON* cfg = CUtils::LoadJsonFile(url.GetString());
    if (cfg == NULL) {
        CUtils::acutPrintf(_T("[CAD INFO] 未找到底图配置 project/plan_base.json，跳过底图\n"));
        return false;
    }

    // 3. 解析 base_map（可选；缺省即流程一）
    bool imported = false;
    cJSON* bm = CUtils::GetJsonObjectItemUtf8(cfg, _T("base_map"));
    if (bm != NULL) {
        imported = ImportBaseMap(bm);
        // 4. 记录出图比例分母（供 Execute 末尾 ApplyPlotScale 使用）
        m_plotScale = GetJsonNumber(bm, _T("plot_scale"), 0.0);   // <=0 = 未配置
        if (m_plotScale <= 0.0) {
            CUtils::acutPrintf(_T("[CAD INFO] 未配置出图比例 plot_scale，不施加比例缩放\n"));
        }
    } else {
        CUtils::acutPrintf(_T("[CAD INFO] plan_base.json 无 base_map 节点，按流程一（无底图）继续\n"));
        m_plotScale = 0.0;
    }

    cJSON_free(cfg);
    return imported;
}

// ============================================================================
// V1.6 P0-4 §5.2：EraseBaseMapLayers——清旧重建
// ============================================================================
void CCreatePlanView::EraseBaseMapLayers()
{
    // 底图-现状层（幂等：层不存在时 EraseLayerEntities 返回 0）
    CUtils::EraseLayerEntities(this->pModelSpace, kBaseMapLayer);
    // 旧底图块（含参照+块定义）；幂等：块不存在返回成功
    // ★ 必须先删块定义：ImportDwgAsBlock 在块名已存在时只返回既有 blkDefId、不插新参照，
    //   否则重复执行会出现"提示成功但图上无底图"——见蓝图 §1.1.1 既有行为备忘。
    CUtils::EraseBlock(this->pModelSpace, kBaseMapBlock);
    // 重置成员状态
    m_pBaseMapBlkRefId = AcDbObjectId::kNull;
}

// ============================================================================
// V1.6 P0-4 §5.5：GetJsonNumber 安全读取 JSON 数字
// ============================================================================
double CCreatePlanView::GetJsonNumber(const cJSON* obj, const TCHAR* key, double def)
{
    if (obj == NULL) return def;
    cJSON* item = CUtils::GetJsonObjectItemUtf8(obj, key);
    if (item == NULL || !cJSON_IsNumber(item)) return def;
    return item->valuedouble;
}

// ============================================================================
// V1.6 P0-6 §5.6：ParseRegistration（数组解析，兼容旧对象写法作软迁移）
// ============================================================================
bool CCreatePlanView::ParseRegistration(const cJSON* bm,
                                       std::vector<AcGePoint2d>& source,
                                       std::vector<AcGePoint2d>& target)
{
    source.clear();
    target.clear();

    if (bm == NULL) return false;

    // V1.6 P0-6：registration 可选；缺省 → false，走手动参数
    cJSON* reg = CUtils::GetJsonObjectItemUtf8(bm, _T("registration"));
    if (reg == NULL || !cJSON_IsObject(reg)) return false;

    cJSON* src = CUtils::GetJsonObjectItemUtf8(reg, _T("source_points"));
    cJSON* tgt = CUtils::GetJsonObjectItemUtf8(reg, _T("target_points"));
    if (src == NULL || tgt == NULL || !cJSON_IsArray(src) || !cJSON_IsArray(tgt)) {
        CUtils::acutPrintf(_T("[CAD WARN] registration 结构无效（需要 source_points/target_points 数组），回退手动对齐参数\n"));
        return false;
    }

    int nSrc = cJSON_GetArraySize(src);
    int nTgt = cJSON_GetArraySize(tgt);
    if (nSrc != nTgt || nSrc < 1 || nSrc > 2) {
        CUtils::acutPrintf(_T("[CAD WARN] registration 点数不匹配或超范围（源 %d / 目标 %d，需 1~2 对），回退手动对齐参数\n"),
                           nSrc, nTgt);
        return false;
    }

    source.reserve(nSrc);
    target.reserve(nTgt);

    int i = 0;
    cJSON* ps = NULL;
    cJSON_ArrayForEach(ps, src) {
        if (!cJSON_IsArray(ps) || cJSON_GetArraySize(ps) != 2) {
            CUtils::acutPrintf(_T("[CAD WARN] registration 源点 %d 不是 [x,y] 数组，回退手动对齐参数\n"), i + 1);
            source.clear(); target.clear();
            return false;
        }
        cJSON* sx = cJSON_GetArrayItem(ps, 0);
        cJSON* sy = cJSON_GetArrayItem(ps, 1);
        if (sx == NULL || sy == NULL || !cJSON_IsNumber(sx) || !cJSON_IsNumber(sy)) {
            CUtils::acutPrintf(_T("[CAD WARN] registration 源点 %d 元素非数字，回退手动对齐参数\n"), i + 1);
            source.clear(); target.clear();
            return false;
        }
        source.push_back(AcGePoint2d(sx->valuedouble, sy->valuedouble));
        ++i;
    }

    i = 0;
    cJSON* pt = NULL;
    cJSON_ArrayForEach(pt, tgt) {
        if (!cJSON_IsArray(pt) || cJSON_GetArraySize(pt) != 2) {
            CUtils::acutPrintf(_T("[CAD WARN] registration 目标点 %d 不是 [x,y] 数组，回退手动对齐参数\n"), i + 1);
            source.clear(); target.clear();
            return false;
        }
        cJSON* tx = cJSON_GetArrayItem(pt, 0);
        cJSON* ty = cJSON_GetArrayItem(pt, 1);
        if (tx == NULL || ty == NULL || !cJSON_IsNumber(tx) || !cJSON_IsNumber(ty)) {
            CUtils::acutPrintf(_T("[CAD WARN] registration 目标点 %d 元素非数字，回退手动对齐参数\n"), i + 1);
            source.clear(); target.clear();
            return false;
        }
        target.push_back(AcGePoint2d(tx->valuedouble, ty->valuedouble));
        ++i;
    }

    return true;
}

// ============================================================================
// V1.6 P0-6 §5.6：ComputeRegistration 四参数最小二乘
//   单点模式（1 对）：s, θ 取入参 sAlign/radAlign 显式值，仅算 T
//   两点模式（2 对）：两点距 dP、dQ → s=|dQ|/|dP|, θ = atan2(dQ)-atan2(dP)
//   T = Q0 - s·R(θ)·P0（块插入点）
// 误差容差：两点距 <1e-6 / s/θ/T 任一非有限 / s≤0 → 返 false
// ============================================================================
bool CCreatePlanView::ComputeRegistration(double sAlign,
                                          double rotAlign,
                                          const std::vector<AcGePoint2d>& source,
                                          const std::vector<AcGePoint2d>& target,
                                          double& sOut,
                                          double& rotOut,
                                          double& insertXOut,
                                          double& insertYOut)
{
    sOut = sAlign;
    rotOut = rotAlign;
    insertXOut = 0.0;
    insertYOut = 0.0;

    const int n = (int)source.size();
    if (n < 1 || n > 2) return false;
    if ((int)target.size() != n) return false;

    double s = sAlign;
    double theta = rotAlign;

    if (n == 2) {
        double dPx = source[1].x - source[0].x;
        double dPy = source[1].y - source[0].y;
        double dQx = target[1].x - target[0].x;
        double dQy = target[1].y - target[0].y;
        double lenP = sqrt(dPx * dPx + dPy * dPy);
        if (lenP < 1e-6) {
            CUtils::acutPrintf(_T("[CAD WARN] registration 两点距离过小（%.6f），无法计算比例/旋转，回退手动对齐参数\n"),
                                lenP);
            return false;
        }
        double lenQ = sqrt(dQx * dQx + dQy * dQy);
        s     = lenQ / lenP;
        theta = atan2(dQy, dQx) - atan2(dPy, dPx);
    }

    double cs = cos(theta);
    double sn = sin(theta);
    double tX = target[0].x - s * ( cs * source[0].x - sn * source[0].y);
    double tY = target[0].y - s * ( sn * source[0].x + cs * source[0].y);

    if (!_finite(s) || !_finite(theta) || !_finite(tX) || !_finite(tY) || s <= 0.0) {
        CUtils::acutPrintf(_T("[CAD WARN] registration 计算结果非法，回退手动对齐参数\n"));
        return false;
    }

    sOut        = s;
    rotOut      = theta;
    insertXOut  = tX;
    insertYOut  = tY;
    CUtils::acutPrintf(_T("[CAD INFO] 两点配准成功：scale=%.6f rotation=%.6f rad insert=(%.3f, %.3f)\n"),
                       s, theta, tX, tY);
    return true;
}

// ============================================================================
// V1.6 P0-4 §5.3.1：BuildImportMatrix（静态纯函数；D6 甲方案中在 ApplyPlotScale
//   不调用此函数——主校正为 ImportBaseMap 中的"前修正插入点"完成。
//   本函数仍提供以满足蓝图 §1.1.2 #6 接口契约，并保留作"扣基点矩阵"的可复用数学工具。
//   数学说明：返回矩阵 M = T(insert) · R(θ) · S · T(B) · T(-B)；
//   详设 V1.6 §5.3.1 已指出 T(B)·T(-B)≡I，故"按字面实现"等于不校正。
//   D6 甲方案采用"插入点前修正 ptInsert' = insert + s·R·B"在数值上等价，逻辑更清晰。
// ============================================================================
AcGeMatrix3d CCreatePlanView::BuildImportMatrix(
        const AcGePoint3d& ptInsert,
        double dScale,
        double dRotation,
        const AcGePoint3d& sourceBasePoint)
{
    AcGeMatrix3d mT  = AcGeMatrix3d::translation(ptInsert);
    AcGeMatrix3d mR  = AcGeMatrix3d::rotation(dRotation, AcGeVector3d::kZAxis, AcGePoint3d::kOrigin);
    AcGeMatrix3d mS  = AcGeMatrix3d::scaling(dScale, AcGePoint3d::kOrigin);
    AcGeMatrix3d mTp = AcGeMatrix3d::translation(sourceBasePoint);
    AcGeMatrix3d mTn = AcGeMatrix3d::translation(-sourceBasePoint);
    // 列向量约定（M·P）：从右往左施加变换
    return mT * mR * mS * mTp * mTn;
}

// ============================================================================
// V1.6 P0-4 §5.3：ImportBaseMap（导入委托方底图 DWG，含对齐 + BASEPOINT）
// ============================================================================
bool CCreatePlanView::ImportBaseMap(const cJSON* cfg)
{
    // 1. 路径（相对 ARX_DIR；缺/空字符串 → 流程一跳过底图环节）
    cJSON* dwgItem = (cfg != NULL) ? CUtils::GetJsonObjectItemUtf8(cfg, _T("dwg")) : NULL;
    if (dwgItem == NULL || !cJSON_IsString(dwgItem) ||
        dwgItem->valuestring == NULL || dwgItem->valuestring[0] == _T('\0'))
    {
        CUtils::acutPrintf(_T("[CAD INFO] base_map.dwg 未配置，跳过底图导入（流程一：按流程一无底图继续）\n"));
        return false;
    }
    CString dwgPath = strFilePath + _T("/") + CString(dwgItem->valuestring);

    // 2. 文件存在性（V1.6 P0-5：FileExists 不存在 → PathFileExists；工程已链接 shlwapi）
    if (!PathFileExists(dwgPath)) {
        CUtils::acutPrintf(_T("[CAD ERROR] 底图文件不存在：%s\n"), dwgPath.GetString());
        return false;
    }

    // 3. 对齐参数（默认 0,0 / 1.0 / 0）
    m_baseCfg.dwg     = dwgItem->valuestring;
    m_baseCfg.insertX = GetJsonNumber(cfg, _T("insert_x"), 0.0);
    m_baseCfg.insertY = GetJsonNumber(cfg, _T("insert_y"), 0.0);
    m_baseCfg.scale   = GetJsonNumber(cfg, _T("scale"), 1.0);
    m_baseCfg.rotation= GetJsonNumber(cfg, _T("rotation"), 0.0);
    cJSON* layerItem  = CUtils::GetJsonObjectItemUtf8(cfg, _T("layer"));
    if (layerItem && cJSON_IsString(layerItem) && layerItem->valuestring) {
        m_baseCfg.layer = CString(layerItem->valuestring);
    } else {
        m_baseCfg.layer = kBaseMapLayer;
    }
    cJSON* dimItem = CUtils::GetJsonObjectItemUtf8(cfg, _T("dim"));
    // V1.6 P0-7：cJSON_True → cJSON_IsTrue（项目惯例）
    if (dimItem && cJSON_IsBool(dimItem)) m_baseCfg.dim = cJSON_IsTrue(dimItem);
    else m_baseCfg.dim = false;

    // 3.5 对齐决策（FR-2.1 默认行为）
    bool hasExplicit =
        CUtils::GetJsonObjectItemUtf8(cfg, _T("insert_x")) != NULL ||
        CUtils::GetJsonObjectItemUtf8(cfg, _T("insert_y")) != NULL ||
        CUtils::GetJsonObjectItemUtf8(cfg, _T("scale"))    != NULL ||
        CUtils::GetJsonObjectItemUtf8(cfg, _T("rotation")) != NULL;

    bool hasReg = ParseRegistration(cfg, m_regSourcePoints, m_regTargetPoints);
    m_regMode = hasReg;

    if (hasReg) {
        double sOut = 0.0, rotOut = 0.0, ix = 0.0, iy = 0.0;
        bool bRegOk = ComputeRegistration(m_baseCfg.scale, m_baseCfg.rotation,
                                          m_regSourcePoints, m_regTargetPoints,
                                          sOut, rotOut, ix, iy);
        if (!bRegOk) {
            // 配准失败：回退显式参数；若亦未显式配置 → 不施加变换
            if (!hasExplicit) {
                m_baseCfg.insertX = 0.0;
                m_baseCfg.insertY = 0.0;
                m_baseCfg.scale   = 1.0;
                m_baseCfg.rotation= 0.0;
            }
            CUtils::acutPrintf(_T("[CAD INFO] 配准未成功，按%s对齐\n"),
                               hasExplicit ? _T("显式参数") : _T("默认同一坐标系"));
            m_regMode = false;
        } else {
            // 配准成功：覆盖 insert/scale/rotation
            m_baseCfg.scale   = sOut;
            m_baseCfg.rotation= rotOut;
            m_baseCfg.insertX = ix;
            m_baseCfg.insertY = iy;
            CUtils::acutPrintf(_T("[CAD INFO] 已按两点配准施加对齐变换（先坐标对齐）\n"));
        }
    } else if (!hasExplicit) {
        // 默认行为：未配置任何对齐 → 不施加变换
        CUtils::acutPrintf(_T("[CAD INFO] 未配置对齐，默认按同一坐标系导入（insert=(0,0), scale=1, rotation=0）\n"));
    }

    // 4. 确保图层存在
    if (CUtils::EnsureLayer(m_baseCfg.layer).isNull()) {
        CUtils::acutPrintf(_T("[CAD ERROR] 创建底图图层失败：%s\n"), m_baseCfg.layer.GetString());
        return false;
    }

    // ★ V1.6 BASEPOINT 处理（P1-4）：从 GetExtDb 缓存的源 DWG 读出块 BASEPOINT；
    //   D6 甲方案：BASEPOINT 校正前移到插入点修正（在 step 6 之前）。
    m_baseCfg.hasBasePoint = false;
    m_baseCfg.sourceBasePoint = AcGePoint3d::kOrigin;
    {
        AcDbDatabase* pSrc = GetExtDb(dwgPath.GetString());
        if (pSrc != NULL) {
            AcDbBlockTable* pBT = NULL;
            if (pSrc->getBlockTable(pBT, AcDb::kForRead) == Acad::eOk) {
                AcDbBlockTableRecord* pModel = NULL;
                if (pBT->getAt(ACDB_MODEL_SPACE, pModel, AcDb::kForRead) == Acad::eOk) {
                    m_baseCfg.sourceBasePoint = pModel->origin();
                    m_baseCfg.hasBasePoint = true;
                    pModel->close();
                }
                pBT->close();
            }
        }
    }
    if (m_baseCfg.hasBasePoint &&
        fabs(m_baseCfg.sourceBasePoint.x) < 1e-9 &&
        fabs(m_baseCfg.sourceBasePoint.y) < 1e-9)
    {
        // 等价原点 → 等同未设基点
        m_baseCfg.hasBasePoint = false;
        CUtils::acutPrintf(_T("[CAD INFO] 源 DWG BASEPOINT=原点，按 V1.5 旧矩阵链处理\n"));
    } else if (m_baseCfg.hasBasePoint) {
        CUtils::acutPrintf(_T("[CAD INFO] 源 DWG BASEPOINT=(%.3f, %.3f, %.3f) ≠ 原点，插入点已前修正（D6 甲方案）\n"),
                           m_baseCfg.sourceBasePoint.x, m_baseCfg.sourceBasePoint.y, m_baseCfg.sourceBasePoint.z);
    }

    // 5. 构造插入点（含 D6 甲方案的 BASEPOINT 前修正：ptInsert' = insert + s·R(θ)·B）
    //    公式：当 BASEPOINT ≠ (0,0) 时，把 BASEPOINT 沿 R(θ)·scale 平移到 insert，
    //    抵消 ImportDwgAsBlock 的 setPosition 相对 BASEPOINT 的隐含偏移。
    //    当 hasBasePoint=false 时退化为 ptInsert 不变（即 V1.5 旧行为）。
    AcGePoint3d ptInsert(m_baseCfg.insertX, m_baseCfg.insertY, 0.0);
    if (m_baseCfg.hasBasePoint) {
        double cs = cos(m_baseCfg.rotation);
        double sn = sin(m_baseCfg.rotation);
        double dx = m_baseCfg.scale * ( cs * m_baseCfg.sourceBasePoint.x
                                       - sn * m_baseCfg.sourceBasePoint.y);
        double dy = m_baseCfg.scale * ( sn * m_baseCfg.sourceBasePoint.x
                                       + cs * m_baseCfg.sourceBasePoint.y);
        ptInsert.x += dx;
        ptInsert.y += dy;
    }

    // 6. 导入为固定名整体块参照（复用基类 6 参重载，CreateBaseView.h:26）
    AcDbObjectId blkDefId = ImportDwgAsBlock(dwgPath.GetString(),
                                             ptInsert,
                                             kBaseMapBlock,
                                             m_baseCfg.scale,
                                             m_baseCfg.rotation,
                                             m_baseCfg.layer.GetString());
    if (blkDefId == AcDbObjectId::kNull) {
        CUtils::acutPrintf(_T("[CAD ERROR] 底图 DWG 导入失败：%s\n"), dwgPath.GetString());
        return false;
    }

    // 7. 记录块参照 ID（用于 ApplyPlotScale 识别底图块参照；D6 甲方案下 ApplyPlotScale
    //    不再特判 BASEPOINT，但仍通过 ID 区分底图块参照 vs 其它实体——便于未来扩展）
    m_pBaseMapBlkRefId = blkDefId;

    // 8. 置灰（dim=true 时按 kDimColor 253 设块参照）
    if (m_baseCfg.dim) {
        DimBaseMapReference();
    }

    CUtils::acutPrintf(_T("[CAD INFO] 底图导入成功（整体块参照，不分解）：%s\n"), dwgPath.GetString());
    return true;
}

// ============================================================================
// V1.6 P0-4 §5.3 置灰辅助：DimBaseMapReference
//   遍历模型空间，找到引用 kBaseMapBlock 的块参照设色 253；找不到则静默返回。
// ============================================================================
void CCreatePlanView::DimBaseMapReference()
{
    if (this->pModelSpace == NULL) return;

    // 先取 kBaseMapBlock 的块定义 ID（用于稳定比对，规避 blockName() 临时缓冲区歧义）
    AcDbBlockTable* pBt = NULL;
    AcDbObjectId bmId = AcDbObjectId::kNull;
    if (this->pDb == NULL) return;
    if (this->pDb->getBlockTable(pBt, AcDb::kForRead) != Acad::eOk) return;
    if (pBt->getAt(kBaseMapBlock, bmId) != Acad::eOk || bmId.isNull()) {
        pBt->close();
        CUtils::acutPrintf(_T("[CAD WARN] 底图块定义 %s 不存在，跳过置灰\n"), kBaseMapBlock);
        return;
    }
    pBt->close();

    std::vector<AcDbObjectId> refIds;
    AcDbBlockTableRecordIterator* pIt = NULL;
    if (this->pModelSpace->newIterator(pIt) != Acad::eOk || pIt == NULL) return;

    AcDbEntity* pEnt = NULL;
    for (; !pIt->done(); pIt->step()) {
        if (pIt->getEntity(pEnt, AcDb::kForRead) != Acad::eOk) continue;
        if (pEnt->isKindOf(AcDbBlockReference::desc())) {
            AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt);
            if (pRef->blockTableRecord() == bmId) {
                refIds.push_back(pRef->objectId());
            }
        }
        pEnt->close();
    }
    delete pIt;

    for (size_t i = 0; i < refIds.size(); ++i) {
        AcDbBlockReference* pRef = NULL;
        if (acdbOpenObject(pRef, refIds[i], AcDb::kForWrite) == Acad::eOk) {
            pRef->setColorIndex(kDimColor);
            pRef->close();
        }
    }

    if (refIds.empty()) {
        CUtils::acutPrintf(_T("[CAD WARN] 底图块参照未找到（dim=true 但无 %s 参照），跳过置灰\n"), kBaseMapBlock);
    }
}

// ============================================================================
// V1.6 P0-4 §5.4：ApplyPlotScale 出图比例：整体成图统一缩放
//   D6 甲方案：BASEPOINT 校正已前移到 ImportBaseMap 内的插入点修正，
//   本函数对所有底图+钻孔+图例实体施加同一缩放矩阵，**无 BASEPOINT 特判分支**。
//   流程二缩放三层；流程一"底图-现状"层不存在循环自然跳过，仅缩"钻孔+图例"两层。
// ============================================================================
bool CCreatePlanView::ApplyPlotScale(double plotScaleDenom)
{
    // 0. 未配置 / 非法 → 不缩放（与现状一致，FR-3.1）
    if (plotScaleDenom <= 0.0) {
        CUtils::acutPrintf(_T("[CAD INFO] 出图比例未配置或非法，保持现状（不缩放）\n"));
        return false;
    }

    // 1. 比例因子换算（D5 默认约定 k = 1000 / N；详见 §2.4 待客户确认）
    double k = 1000.0 / plotScaleDenom;
    if (!_finite(k) || k <= 0.0) {
        CUtils::acutPrintf(_T("[CAD WARN] 出图比例换算非法（plot_scale=%.3f），回退不缩放\n"), plotScaleDenom);
        return false;
    }
    if (fabs(k - 1.0) < 1e-9) {
        CUtils::acutPrintf(_T("[CAD INFO] 出图比例 1:%.0f，比例因子 k=1.0，无需缩放\n"), plotScaleDenom);
        return true;
    }

    // 2. 统一基准点 O（默认坐标原点）与统一缩放矩阵（绕 O 缩放 k）
    AcGePoint3d O(0.0, 0.0, 0.0);
    AcGeMatrix3d m = AcGeMatrix3d::scaling(k, O);     // p' = O + k·(p ? O)

    // 3. 仅对本命令产生的成果图层统一变换，避免影响用户图内其它内容
    const TCHAR* kLayers[3] = { kBaseMapLayer, kPlanHoleLayer, kLegendLayer };
    std::vector<AcDbObjectId> ids;
    if (this->pModelSpace == NULL) return false;
    AcDbBlockTableRecordIterator* pIt = NULL;
    if (this->pModelSpace->newIterator(pIt) != Acad::eOk || pIt == NULL) return false;
    AcDbEntity* pEnt = NULL;
    for (; !pIt->done(); pIt->step()) {
        if (pIt->getEntity(pEnt, AcDb::kForRead) != Acad::eOk) continue;
        // V1.6 P0-7：图层名比对 CompareNoCase（与 EraseLayerEntities 一致）
        for (int i = 0; i < 3; ++i) {
            if (CompareNoCase(pEnt->layer(), kLayers[i]) == 0) {
                ids.push_back(pEnt->objectId());
                break;
            }
        }
        pEnt->close();
    }
    delete pIt;

    // 4. 逐个写打开并施加同一矩阵（D6 甲方案下无 mImport 复合分支——BASEPOINT 已在
    //    ImportBaseMap 内的插入点前修正完成；后续 ApplyPlotScale 仅做统一缩放，
    //    保证底图与钻孔不错位）
    int n = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        AcDbEntity* p = NULL;
        if (acdbOpenObject(p, ids[i], AcDb::kForWrite) == Acad::eOk) {
            p->transformBy(m);
            p->close();
            ++n;
        }
    }
    CUtils::acutPrintf(_T("[CAD INFO] 出图比例 1:%.0f 已应用：k=%.6f，绕基准(%.1f,%.1f) 统一缩放 %d 个成果实体（底图+钻孔+图例）\n"),
                       plotScaleDenom, k, O.x, O.y, n);
    return true;
}