// coding: gb18030
//=============================================================================
// CDwgImporter 实现 - 读取 DWG 并作为块导入到指定位置（AutoCAD 2007 / acdb17）
//=============================================================================
#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include "cJSON.h"
#include "utils.h"
#include "CreateColumnView.h"
#include "PatHatchLoader.h"

// 值类型（也可以直接用 int 0/1，枚举可读性更好）
enum JsonValType { JV_STRING, JV_NUMBER };

struct FieldMap
{
    const TCHAR* fieldName;   // 占位符名，如 "[page_no]"（也可不带括号）
    const TCHAR* firstName;
    const TCHAR* secondName;       // 数组下标（按 JSON 实际位置，可以任意、可不连续）
    JsonValType  type;        // 取值方式
    const TCHAR* defaultVal; // 默认值（可为 NULL）
};
// 标贯统计结果项（按杆长分组）
struct SPT_STAT
{
    double rodLength;    // 杆长（分组键）
    double minVal;       // 组内最小值
    double maxVal;       // 组内最大值
    int    totalCount;   // 组内统计数汇总
    SPT_STAT() : rodLength(0), minVal(0), maxVal(0), totalCount(0) {}
};

// 映射表：声明成文件级 static，写在 CollectTexts/Execute 之前
static const FieldMap s_fieldMap[] = {
    { _T("[page_no]"),   _T("当前页"), _T(""), JV_STRING, _T("1") },
    { _T("[page_count]"),  _T("总页码"),  _T(""), JV_STRING, _T("1") },
    { _T("[project_name]"),  _T("工程名称"),  _T(""), JV_STRING, _T("") },
    { _T("[project_no]"),  _T(""),  _T(""), JV_STRING , _T("") },
    { _T("[hole_no]"),  _T("钻孔编号"), _T(""), JV_STRING, _T("") },
    { _T("[elevation]"),  _T("孔口高程(m)"),  _T(""), JV_NUMBER, _T("") },
    { _T("[diameter]"),  _T(""),  _T(""), JV_STRING, _T("") },
    { _T("[coordinate_x]"),  _T("X坐标(m)"),  _T(""), JV_NUMBER, _T("") },
    { _T("[coordinate_y]"),  _T("Y坐标(m)"),   _T(""), JV_NUMBER, _T("") },
    { _T("[start_date]"),  _T("开孔日期"),  _T(""), JV_STRING, _T("") },
    { _T("[completion_date]"),  _T("终孔日期"),  _T(""), JV_STRING, _T("") },

	{ _T("[water_depth]"),  _T("稳定水位(m)"),  _T(""), JV_STRING, _T("") },
    { _T("[survey_unit]"),  _T("公司名称"),  _T(""), JV_STRING, _T("") },
    { _T("[mapping]"),  _T(""),  _T(""), JV_STRING, _T("") },
    { _T("[check]"),  _T(""),  _T(""), JV_STRING, _T("") },
};


//-----------------------------------------------------------------------------
bool CCreateColumnView::GetUniqueBlockName(AcDbDatabase* pDb, ACHAR* pszName, int nNameCount)
{
    AcDbBlockTable* pBlkTbl = NULL;
    if (pDb->getBlockTable(pBlkTbl, AcDb::kForRead) != Acad::eOk)
        return false;

    ACHAR szTry[256];
    for (int i = 0; i < 1000; ++i)
    {
        if (i == 0)
        {
            CUtils::SafeCopy(szTry, 256, pszName, (int)_tcslen(pszName));
        }
        else
        {
            ACHAR szNum[16];
            int m = 0, v = i;
            do { szNum[m++] = (ACHAR)(_T('0') + (v % 10)); v /= 10; } while (v > 0);
            int base = (int)_tcslen(pszName);
            int j = 0;
            ACHAR szBuf[256];
            CUtils::SafeCopy(szBuf, 256, pszName, base);
            szBuf[base + j++] = _T('_');
            while (m > 0) szBuf[base + j++] = szNum[--m];
            szBuf[base + j] = _T('\0');
            CUtils::SafeCopy(szTry, 256, szBuf, base + j);
        }

        AcDbObjectId existingId;
        if (pBlkTbl->getAt(szTry, existingId) != Acad::eOk)   // 不存在 → 可用
        {
            pBlkTbl->close();
            CUtils::SafeCopy(pszName, nNameCount, szTry, (int)_tcslen(szTry));
            return true;
        }
    }

    pBlkTbl->close();
    return false;
}
void CCreateColumnView::Execute()
{
	if (!prepare()) return;

	CString strFilePath = CUtils::GetArxFolder();
	strFilePath.Replace(_T("\\"), _T("/"));
	CString cadBlockUrl;
	cadBlockUrl.Format(_T("support/%s/钻孔柱状图.dwg"), CADYEAR);
	cadBlockUrl = strFilePath + cadBlockUrl;
	CString cadNoBlockUrl;
	cadNoBlockUrl.Format(_T("support/%s/序号.dwg"), CADYEAR);
	cadNoBlockUrl = strFilePath + cadNoBlockUrl;
    CString cadPatUrl = CUtils::GetArxFolder() + _T("support\\pat");
    CString cadSoilConfigUrl= strFilePath + _T("support/pat_define.json");

    if (!PathFileExists(cadBlockUrl)
		|| !PathFileExists(cadNoBlockUrl))
    {
        // 存在（文件或目录均可，配合 PathIsDirectory 区分）
        CUtils::acutPrintf(_T("[CAD ERROR] cad文件不存在! %s\n"), cadBlockUrl.GetString());
		return;
    }

    // 解析 JSON（失败时内部已打印错误，这里保留原有业务提示）
    s_patConfigData = CUtils::LoadJsonFile(cadSoilConfigUrl.GetString());   // 静默模式
    if (s_patConfigData == NULL) {
        CUtils::acutPrintf(_T("[CAD ERROR] 没有pat配置文件，请检查！ %s\n"), cadSoilConfigUrl.GetString());
        return;
    }

	if (this->startTrans()) return;

    int ret = this->actionBefore();
	if (ret != 0 ) {
		aboutTrans();
		return;
	}

    cJSON* item = NULL;
    AcGePoint3d ptInsert;
	double space = 222.6865;


	double contentHeight = 212.0;
	double headerHeight = 28.0;
	double gridHeight = contentHeight - headerHeight;
	double gridWidth = 176.0;
	double lastWidth = 45.0665;

	double offsetX = 27.0;
    double offsetY = 17.0;

	
	double space_no = 8.0475;
	double space_mc = 14.4856;
	double space_sd = 12.0713;
	double space_chat = 14.4856;
	double space_desc = 45.6296;
	double space_last = 16.0951;

    double dWidthFactor = 0.6;

	ACHAR* pszStyleName = _T("成果图表体");
    PatHatchLoader patLoader;
	patLoader.setDocument(pDoc);
	patLoader.setModelSpace(pModelSpace);
	patLoader.setDatabase(pDb);
    patLoader.RegisterPatFile(cadPatUrl);

    cJSON* pRootConfig = CUtils::GetJsonConfig();
    cJSON* pProfile = cJSON_GetObjectItem(pRootConfig, "profile");
    CString strDiameter;
    if (pProfile) {
        cJSON* hole_diameter_zk = cJSON_GetObjectItem(pProfile, "HOLE_DIAMETER_ZK");
        if (hole_diameter_zk && cJSON_IsString(hole_diameter_zk)) {
            strDiameter = hole_diameter_zk->valuestring;
        }
    }

    cJSON_ArrayForEach(item, s_pJsonData) {
        // 处理每个属性
        if (item != NULL) {
            const char* blockName = (item->string != NULL) ? item->string : "(匿名数组)";
            CUtils::acutPrintf(_T("[数组] %hs，共 %d 个元素\n"),
                            blockName, cJSON_GetArraySize(item));

			ACHAR arrayName[256] = { 0 };
			CUtils::utf8ToTChar(blockName, arrayName, ARRAYSIZE(arrayName));

			/*if (_tcscmp(arrayName, _T("ZK15")) != 0){
				ptInsert.x += space;
				break;
			}*/

			/*if (_tcscmp(arrayName, _T("ZK10")) != 0){
				ptInsert.x += space;
				continue;
			}*/

			CMapStringToString mapVals;

            const int rowSize   = cJSON_GetArraySize(item);
            const int MAP_COUNT = (int)(sizeof(s_fieldMap) / sizeof(s_fieldMap[0]));

            // 标贯统计容器：按杆长分组（单孔范围）
            std::vector<SPT_STAT> sptStats;

            for (int i = 0; i < MAP_COUNT; ++i)
            {
                const FieldMap& fm = s_fieldMap[i];
				/*if(fm.fieldName == _T("[elevation]")){
					CUtils::acutPrintf(_T("[elevation]"));
				}*/
				if (CUtils::checkEmpty(fm.firstName))
                {
                    mapVals.SetAt(fm.fieldName, fm.defaultVal);
                    continue;
                }
				cJSON* firstObj = CUtils::GetJsonObjectItemUtf8(item,fm.firstName);
                if (firstObj == NULL)
                {
                    mapVals.SetAt(fm.fieldName, fm.defaultVal);
                    continue;
                }
				cJSON* elem_col= NULL;
				if (CUtils::checkEmpty(fm.secondName))
				{
					elem_col=firstObj;
				}else{
					cJSON* secondObj = CUtils::GetJsonObjectItemUtf8(firstObj,fm.secondName);
					if (secondObj == NULL)
					{
						mapVals.SetAt(fm.fieldName, fm.defaultVal);
						continue;
					}
					elem_col=secondObj;
				}
                CString v;
                if (fm.type == JV_STRING && cJSON_IsString(elem_col))
                {
                    TCHAR buf[512];
                    CUtils::utf8ToTChar(elem_col->valuestring, buf, 512);  // UTF-8 → TCHAR
                    v = buf;
                }
				else if (fm.type == JV_STRING && cJSON_IsNumber(elem_col))
                {
                    TCHAR buf[512];
                    CUtils::utf8ToTChar(elem_col->valueString, buf, 512);  // UTF-8 → TCHAR
                    v = buf;
                }
                else if (fm.type == JV_NUMBER && cJSON_IsNumber(elem_col))
                {
                    TCHAR buf[512];
                    CUtils::utf8ToTChar(elem_col->valueString, buf, 512);  // UTF-8 → TCHAR
                    v = buf;
                }
                else if (cJSON_IsNull(elem_col))
                {
                    v = fm.defaultVal;                         // 空值 → 默认值
                }
                else
                {
                    continue;                           // 类型与声明不符，跳过
                }
				CUtils::acutPrintf(_T("[数组] key: %s : %s \n"),fm.fieldName, v);

                mapVals.SetAt(fm.fieldName, v);
            }
            
            CString val;
            mapVals.Lookup(_T("[coordinate_x]"), val);
            val = _T("x=") + val;
            mapVals.SetAt(_T("[coordinate_x]"), val);
            mapVals.Lookup(_T("[coordinate_y]"), val);
            val = _T("y=") + val;
            mapVals.SetAt(_T("[coordinate_y]"), val);

			mapVals.SetAt(_T("[diameter]"), strDiameter);

            AcDbObjectId blkDefId = ImportDwgAsBlock(cadBlockUrl.GetString(),ptInsert, arrayName);
			if (blkDefId != AcDbObjectId::kNull){
				CollectTexts(blkDefId, mapVals, 0);
			}
			

			//数据行
			int dataRow = 0;
            //在这里计算每行的高度
            // 数据行：第 7 行 ~ 倒数第二行(rowSize-2)；col1=自，col2=至
            double totalDepth = 1.0;
			cJSON* layerObj = CUtils::GetJsonObjectItemUtf8(item,_T("地层列表"));
			if (layerObj == NULL) continue;
			dataRow = cJSON_GetArraySize(layerObj);   // 数组元素个数
			if (dataRow == 0) continue;

			int realHeight;
			if (dataRow>=5){
				realHeight = gridHeight;
			}else{
				realHeight = gridHeight/(dataRow+1)*dataRow;
			}

            for (int r = dataRow -1; r >= 0; --r)
            {
                cJSON* rowTry = cJSON_GetArrayItem(layerObj, r);
                if (rowTry == NULL)
                    continue;                                   // 空行 → 继续向上
                cJSON* cTo = CUtils::GetJsonObjectItemUtf8(rowTry,_T("进尺至(m)"));    // “至”=终孔深度
                if (cTo != NULL && cJSON_IsNumber(cTo) && cTo->valuedouble > 0.0)
                {
                    totalDepth = cTo->valuedouble;              // ★算术必须用 valuedouble
                    break;
                }
            }
            if (totalDepth <= 0.0) totalDepth = 1.0;                      // 防除零

            double scale = realHeight / totalDepth;                       // 深度→图面比例
            double yTop  = ptInsert.y + offsetY + gridHeight;             // 格顶=深度0（孔口）
            double prevBottom = 0.0;                                      // 上一行底部深度


            mapVals.Lookup(_T("[elevation]"), val);
			double elevation = CUtils::CStringToDouble(val);	//高程
			

			double preTop;
            for (int i = 0; i < dataRow; ++i)
            {
                double toDepth = prevBottom;
				cJSON* rowTry = cJSON_GetArrayItem(layerObj, i);
				cJSON* cTo = CUtils::GetJsonObjectItemUtf8(rowTry,_T("进尺至(m)"));          // 当前行“至”
                if (cTo != NULL && cJSON_IsNumber(cTo))
                    toDepth = cTo->valuedouble;
				else
					continue;
                double thick   = toDepth - prevBottom;                    // 与上一行比较→实际层厚
                double hDraw   = thick * scale;                           // 该行图面实际高度
                double yBottom = yTop - toDepth * scale;                  // 该行底部Y
                CUtils::acutPrintf(_T("[行%d] 至=%.2f 层厚=%.2f 图面高=%.3f 底部Y=%.3f\n"),
                                i, toDepth, thick, hDraw, yBottom);
                prevBottom = toDepth;
                // TODO: 用 hDraw / yBottom 画表格线、定位文字、压盖 hatch……

				//底部横线：起点=该行底部左端角点，向右延伸 gridWidth
                AcGePoint3d ptDraw(ptInsert);
                ptDraw.x += offsetX;
				//ptDraw.y += offsetY;
                ptDraw.y = yBottom;                 // ★ yBottom 已是绝对Y，直接赋值（不要 += offsetY 再 += yBottom）

                CUtils::acutPrintf(_T("开始 x=%.2f y=%.2f \n"),
                                ptDraw.x, ptDraw.y);

                AcGePoint3d ptEnd(ptDraw);
                ptEnd.x  += gridWidth - lastWidth - space_desc;               // 向右延伸一个表格宽度

                
                // pLine->setDatabaseDefaults();     // 可选：随当前图层/颜色默认值
                
				{
					//从头到土层描述的横线
					AcDbLine* pLine = new AcDbLine(ptDraw, ptEnd);
					AcDbObjectId lineId = AcDbObjectId::kNull;
					Acad::ErrorStatus es = pModelSpace->appendAcDbEntity(lineId, pLine);
					if (es == Acad::eOk)
						pLine->close();                  // 入库成功 → close（不能 delete）
					else
						delete pLine;                    // 入库失败 → 未入库，只能 delete
				}

				//添加序号
				AcGePoint3d ptDrawXh(ptDraw);
				ptDrawXh.x += space_no/2;
				ptDrawXh.y += hDraw/2;

				CString no_name;
				no_name.Format(_T("%s_%d"), arrayName,i);

				AcDbObjectId blkDefId_xh = ImportDwgAsBlock(cadNoBlockUrl.GetString(),ptDrawXh, no_name.GetString());
				if (blkDefId_xh != AcDbObjectId::kNull){
					cJSON* cIndex = CUtils::GetJsonObjectItemUtf8(rowTry,_T("地层编号"));  // 当前列“索引”
					if (cIndex != NULL && cJSON_IsString(cIndex)){
                        std::vector<CString> arrSeg;
                        if (CUtils::SplitUtf8(cIndex->valuestring, '-', arrSeg) > 0)
                        {
                            ChangeNo(blkDefId_xh, (ACHAR*)arrSeg[0].GetString());   // "1-0-0" → "1"
                        }
					}
				}

                CString soilPatName;
				//添加土层文字
				{
					cJSON* cSoi = CUtils::GetJsonObjectItemUtf8(rowTry,_T("地层名称"));  // 当前列“土层文字”
					if (cSoi != NULL && cJSON_IsString(cSoi)){
                        std::vector<CString> arrSeg;
                        if (CUtils::SplitUtf8(cSoi->valuestring, '-', arrSeg) > 0)
                        {
                            soilPatName=arrSeg[1].GetString();

							AcGePoint3d ptDrawSoi(ptDraw);
							ptDrawSoi.x += space_no + space_mc/2;
							ptDrawSoi.y += hDraw/2;

							CUtils::AddDbText(pModelSpace,(ACHAR*)arrSeg[1].GetString(),
                            ptDrawSoi,2.1,pszStyleName,DBTA_CENTER,dWidthFactor);

                        }
                    }
				}

				//添加高程，深度，厚度
				{
					//高程
					AcGePoint3d ptDraw_elevation(ptDraw);
					ptDraw_elevation.x += space_no + space_mc + space_sd + space_sd/2;
					ptDraw_elevation.y += 3;
					double elevation_cur = elevation - toDepth;
	
					CString str;
					str.Format(_T("%.2f"), elevation_cur);
					CUtils::AddDbText(pModelSpace,(ACHAR*)str.GetString(),ptDraw_elevation,
                    2.1,pszStyleName,DBTA_CENTER,dWidthFactor);


					//深度
					AcGePoint3d ptDraw_depth(ptDraw);
					ptDraw_depth.x += space_no + space_mc + space_sd * 2 + space_sd/2;
					ptDraw_depth.y += 3;
	
					str.Format(_T("%.2f"), toDepth);
					CUtils::AddDbText(pModelSpace,(ACHAR*)str.GetString(),ptDraw_depth,2.1,
                    pszStyleName,DBTA_CENTER,dWidthFactor);


					//厚度
					AcGePoint3d ptDraw_thick(ptDraw);
					ptDraw_thick.x += space_no + space_mc + space_sd * 3 + space_sd/2;
					ptDraw_thick.y += 3;
	
					str.Format(_T("%.2f"), thick);
					CUtils::AddDbText(pModelSpace,(ACHAR*)str.GetString(),ptDraw_thick,2.1,
                    pszStyleName,DBTA_CENTER,dWidthFactor);
				}

				{
					//土层描述
					//需要处理超高的问题
					cJSON* cDesc  = CUtils::GetJsonObjectItemUtf8(rowTry,_T("成分及其他"));  
					ACHAR desc[256] = { 0 };
					CUtils::utf8ToTChar(cDesc->valuestring, desc, ARRAYSIZE(desc));

					AcGePoint3d ptDraw_desc(ptDraw);
					ptDraw_desc.x += space_no + space_mc + space_sd * 4 + space_chat+1.0;
					if (i == 0){
						ptDraw_desc.y += hDraw-0.7;
					}else{
						double calcY = ptDraw.y + hDraw - 0.7;
						if (calcY < preTop){
							ptDraw_desc.y = calcY;
						}else{
							ptDraw_desc.y = preTop - 0.7;
						}
					}
					if (desc[0] != _T('\0')){
						AcDbObjectId desc_obj = CUtils::AddDbMText(pModelSpace,desc,ptDraw_desc,2.1,space_desc-2.0,pszStyleName);
						AcDbEntity* pDesc = NULL;
						if (acdbOpenObject(pDesc, desc_obj, AcDb::kForRead) == Acad::eOk){
							double w,h;
							if (CUtils::GetEntExtent(pDesc,w,h)){
								double calcH = ptDraw_desc.y - h - 1.0;
								AcGePoint3d ptDraw_start_desc(ptInsert),ptDraw_end_desc;
								ptDraw_start_desc.x  += offsetX + gridWidth - lastWidth - space_desc;
								ptDraw_start_desc.y = ptDraw.y;
								if (calcH < ptDraw.y){
									//画 \_/线条
									AcGePoint3d ptDraw_next_1(ptDraw_start_desc);
									ptDraw_next_1.x +=1.0;
									ptDraw_next_1.y = calcH;
									{
										
										AcDbLine* pLine = new AcDbLine(ptDraw_start_desc, ptDraw_next_1);
										AcDbObjectId lineId = AcDbObjectId::kNull;
										Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
										if (es == Acad::eOk)
											pLine->close();                  
										else
											delete pLine;    
									}
									AcGePoint3d ptDraw_next_2(ptDraw_next_1);
									ptDraw_next_2.x +=space_desc-2.0;
									{
										AcDbLine* pLine = new AcDbLine(ptDraw_next_1, ptDraw_next_2);
										AcDbObjectId lineId = AcDbObjectId::kNull;
										Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
										if (es == Acad::eOk)
											pLine->close();                  
										else
											delete pLine;  
									}
									ptDraw_end_desc.x = ptDraw_next_2.x+1.0;
									ptDraw_end_desc.y = ptDraw_start_desc.y;
									{
										AcDbLine* pLine = new AcDbLine(ptDraw_next_2, ptDraw_end_desc);
										AcDbObjectId lineId = AcDbObjectId::kNull;
										Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
										if (es == Acad::eOk)
											pLine->close();                  
										else
											delete pLine; 
									}

								}else{
									//直接画直线
									ptDraw_end_desc.x = ptDraw_start_desc.x+space_desc;
									ptDraw_end_desc.y = ptDraw_start_desc.y;
									AcDbLine* pLine = new AcDbLine(ptDraw_start_desc, ptDraw_end_desc);

									AcDbObjectId lineId = AcDbObjectId::kNull;
									Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
									if (es == Acad::eOk)
										pLine->close();                  
									else
										delete pLine;                   

								}
								preTop = calcH;
							}else{
								preTop = ptDraw_desc.y;
							}
                            pDesc->close();
						}else{
							preTop = ptDraw_desc.y;
						}
					}else{
						preTop = ptDraw_desc.y;
					}

				}

                if (!soilPatName.IsEmpty()){
                    //填充土层图案
                    double soilLeft = ptDraw.x + space_no + space_mc + space_sd * 4;
                    AcGePoint2dArray rect;                         
                    rect.append(AcGePoint2d(soilLeft,   ptDraw.y));
                    rect.append(AcGePoint2d(soilLeft+space_chat,  ptDraw.y));
                    rect.append(AcGePoint2d(soilLeft+space_chat, ptDraw.y + hDraw));
                    rect.append(AcGePoint2d(soilLeft,  ptDraw.y + hDraw));
                    rect.append(AcGePoint2d(soilLeft,   ptDraw.y));

                    AcGeDoubleArray bulges;
                    for (int b = 0; b < rect.length(); ++b)     // 变量改名 b，避免遮蔽外层行循环的 i
                        bulges.append(0.0);

                    // 岩性名 → 图例名：如 "耕土" → "gt"
                    TCHAR patName[128] = { 0 };
                    if (CUtils::FindPatNameBySoilName(s_patConfigData, soilPatName.GetString(),
                                              patName, ARRAYSIZE(patName)))
                    {
                        AcDbObjectId hatchId = AcDbObjectId::kNull;
                        Acad::ErrorStatus es = patLoader.CreateHatchByName(
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

				//取样
				{
					cJSON* cSampling_obj =  CUtils::GetJsonObjectItemUtf8(rowTry,_T("取岩(土、水)样"));
					if (cSampling_obj != NULL)
					{
						cJSON* cSampling_no  = CUtils::GetJsonObjectItemUtf8(cSampling_obj,_T("编号"));  
						cJSON* cSampling_num  = CUtils::GetJsonObjectItemUtf8(cSampling_obj,_T("取样深度"));  

						if (cSampling_no != NULL && cJSON_IsString(cSampling_no)
							&& cSampling_num != NULL && cJSON_IsString(cSampling_num)){

							std::vector<CString> arrSeg;
							if (CUtils::SplitUtf8(cSampling_num->valuestring, '-', arrSeg) > 1)
							{
								double depth_sampling = CUtils::CStringToDouble(arrSeg[1]);

								AcGePoint3d ptDraw_sampling(ptDraw);
								ptDraw_sampling.x += space_no + space_mc + space_sd * 4 + space_chat + space_desc;
								ptDraw_sampling.y = yTop - depth_sampling * scale;

								AcGePoint3d ptDraw_sampling_end(ptDraw_sampling);
								ptDraw_sampling_end.x  +=space_chat;               // 向右延伸一个表格宽度

								AcDbLine* pLine = new AcDbLine(ptDraw_sampling, ptDraw_sampling_end);

								AcDbObjectId lineId = AcDbObjectId::kNull;
								Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
								if (es == Acad::eOk)
									pLine->close();                  // 入库成功 → close（不能 delete）
								else
									delete pLine;                    // 入库失败 → 未入库，只能 delete

								ACHAR sampling_txt[256] = { 0 };
								CUtils::utf8ToTChar(cSampling_no->valuestring, sampling_txt, ARRAYSIZE(sampling_txt));

								//取样编号
								AcGePoint3d ptDraw_sampling_txt_no(ptDraw_sampling);
								ptDraw_sampling_txt_no.x += space_chat/2;
								ptDraw_sampling_txt_no.y += 2.5;

								CUtils::AddDbText(pModelSpace,sampling_txt,ptDraw_sampling_txt_no,
									2.1,pszStyleName,DBTA_CENTER,dWidthFactor);


								//取样数值
								CUtils::utf8ToTChar(cSampling_num->valuestring, sampling_txt, ARRAYSIZE(sampling_txt));

								AcGePoint3d ptDraw_sampling_txt(ptDraw_sampling);
								ptDraw_sampling_txt.x += space_chat/2;
								ptDraw_sampling_txt.y -=2.5;

								CUtils::AddDbText(pModelSpace,sampling_txt,ptDraw_sampling_txt,
									2.1,pszStyleName,DBTA_CENTER,dWidthFactor);

							}
						}
					}
				}
				
				//统计标贯
				{
					cJSON* ctesting_obj =  CUtils::GetJsonObjectItemUtf8(rowTry,_T("原位测试"));
					if (ctesting_obj != NULL)
					{
						cJSON* cStandard_type  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("类型"));

						if (cStandard_type != NULL && cJSON_IsString(cStandard_type))
						{
							// JSON 值是 UTF-8，转 TCHAR 后再与字面量比较
							TCHAR szType[64] = { 0 };
							CUtils::utf8ToTChar(cStandard_type->valuestring, szType, ARRAYSIZE(szType));

							if (_tcscmp(szType, _T("标贯")) == 0)
							{
								cJSON* cStandard_rod_length  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("杆长(m)"));

								cJSON* cStandard_rod_min  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("起(m)"));
								cJSON* cStandard_rod_max  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("止(m)"));
								cJSON* cStandard_rod_count  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("锤击数(击)"));

								double rodLength = CUtils::JsonValToDouble(cStandard_rod_length);
								double rodMin    = CUtils::JsonValToDouble(cStandard_rod_min);
								double rodMax    = CUtils::JsonValToDouble(cStandard_rod_max);
								int    rodCount  = (int)(CUtils::JsonValToDouble(cStandard_rod_count) + 0.5);

								// 归并到杆长相同的已有分组（浮点容差比较）
								bool bFound = false;
								for (size_t k = 0; k < sptStats.size(); ++k)
								{
									double d = sptStats[k].rodLength - rodLength;
									if (d < 0) d = -d;
									if (d < 0.001)
									{
										if (rodMin < sptStats[k].minVal) sptStats[k].minVal = rodMin;
										if (rodMax > sptStats[k].maxVal) sptStats[k].maxVal = rodMax;
										sptStats[k].totalCount += rodCount;
										bFound = true;
										break;
									}
								}
								if (!bFound)      // 新杆长 → 新建分组
								{
									SPT_STAT st;
									st.rodLength  = rodLength;
									st.minVal     = rodMin;
									st.maxVal     = rodMax;
									st.totalCount = rodCount;
									sptStats.push_back(st);
								}
							}
						}
					}
				}

                //统计动探
                {
					cJSON* ctesting_obj =  CUtils::GetJsonObjectItemUtf8(rowTry,_T("原位测试"));
					if (ctesting_obj != NULL)
					{
						cJSON* cDT_type  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("类型"));

						if (cDT_type != NULL && cJSON_IsString(cDT_type))
						{
							// JSON 值是 UTF-8，转 TCHAR 后再与字面量比较
							TCHAR szType[64] = { 0 };
							CUtils::utf8ToTChar(cDT_type->valuestring, szType, ARRAYSIZE(szType));

							if (_tcscmp(szType, _T("重型动探")) == 0)
							{
								cJSON* cDT_min  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("起(m)"));
								cJSON* cDT_max  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("止(m)"));
								cJSON* cDT_desc  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("锤击数(击)"));

								double dtMin    = CUtils::JsonValToDouble(cDT_min);
								double dtMax    = CUtils::JsonValToDouble(cDT_max);

								AcGePoint3d ptDraw_Dt(ptDraw);
								ptDraw_Dt.x += space_no + space_mc + space_sd * 4 + space_chat + space_desc + space_chat*2;
								ptDraw_Dt.y = yTop - dtMax * scale;

								AcGePoint3d ptDraw_Dt_end(ptDraw_Dt);
								ptDraw_Dt_end.x  +=space_last;               // 向右延伸一个表格宽度

								AcDbLine* pLine = new AcDbLine(ptDraw_Dt, ptDraw_Dt_end);

								AcDbObjectId lineId = AcDbObjectId::kNull;
								Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
								if (es == Acad::eOk)
									pLine->close();                  
								else
									delete pLine;      

								ACHAR desc_txt[256] = { 0 };
								CUtils::utf8ToTChar(cDT_desc->valuestring, desc_txt, ARRAYSIZE(desc_txt));

								//动探击数
								AcGePoint3d ptDraw_dt_head(ptDraw_Dt);
								ptDraw_dt_head.x +=0.2;
								AcDbObjectId objId = CUtils::AddDbMText(pModelSpace,desc_txt,ptDraw_dt_head,2.1,4,pszStyleName);
								AcDbEntity* entity_desc = CUtils::OpenEntityById(objId);
								double width_desc,height_desc;
								CUtils::GetEntExtent(entity_desc,width_desc,height_desc);

								ptDraw_dt_head.y += height_desc;
								ptDraw_dt_head.y -=2.1;
								CUtils::AddDbText(pModelSpace,_T("N"),ptDraw_dt_head,2.1,
									pszStyleName,DBTA_LEFT,dWidthFactor);
								ptDraw_dt_head.x = ptDraw_Dt.x + 0.9;
								CUtils::AddDbText(pModelSpace,_T("63.5"),ptDraw_dt_head,1.4,
									pszStyleName,DBTA_LEFT,dWidthFactor);
								ptDraw_dt_head.x = ptDraw_Dt.x + 3.2;
								CUtils::AddDbText(pModelSpace,_T("="),ptDraw_dt_head,2.1,
									pszStyleName,DBTA_LEFT,dWidthFactor);
								ptDraw_dt_head.x = ptDraw_Dt.x + 4.2;
								AcDbMText* pText = AcDbMText::cast(entity_desc);
								if (pText != NULL)
								{
									if (pText->upgradeOpen() == Acad::eOk)        // 读 → 写
									{
										AcGePoint3d ptLoc = pText->location();     // 取当前附着点
										ptLoc.x = ptDraw_dt_head.x;                
										ptLoc.y += height_desc;                
										pText->setLocation(ptLoc);
									}
									pText->close();   // entity_desc 与 pText 是同一对象，close 一次即可
								}else{
									entity_desc->close();
								}

								//最小值-最大值
								CString dt_txt;
								dt_txt.Format(_T("%.2f-%.2f"), dtMin, dtMax);

								AcGePoint3d ptDraw_dt_len(ptDraw_Dt);
								ptDraw_dt_len.x += space_last/2;
								ptDraw_dt_len.y -=2.1;

								CUtils::AddDbText(pModelSpace,dt_txt,ptDraw_dt_len,2.1,
									pszStyleName,DBTA_CENTER,dWidthFactor);

							}
						}
					}
				}
            }

			//标贯击数
            for (size_t k = 0; k < sptStats.size(); ++k)
            {
                const SPT_STAT& st = sptStats[k];
                CUtils::acutPrintf(_T("[标贯统计] 杆长=%.2f  最小值=%.1f  最大值=%.1f  统计数=%d\n"),
                    st.rodLength, st.minVal, st.maxVal, st.totalCount);

				AcGePoint3d ptDraw_Standard(ptInsert);
				ptDraw_Standard.x += offsetX + space_no + space_mc + space_sd * 4 + space_chat + space_desc + space_chat;
				ptDraw_Standard.y = yTop - st.maxVal * scale;

				AcGePoint3d ptDraw_Standard_end(ptDraw_Standard);
				ptDraw_Standard_end.x  +=space_chat;               // 向右延伸一个表格宽度

				AcDbLine* pLine = new AcDbLine(ptDraw_Standard, ptDraw_Standard_end);

				AcDbObjectId lineId = AcDbObjectId::kNull;
				Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
				if (es == Acad::eOk)
					pLine->close();                  // 入库成功 → close（不能 delete）
				else
					delete pLine;                    // 入库失败 → 未入库，只能 delete

				CString standard_txt;
				standard_txt.Format(_T("=%d"), st.totalCount);

				//杆长
				AcGePoint3d ptDraw_standard_head(ptDraw_Standard);
				ptDraw_standard_head.x += space_chat/2;
				ptDraw_standard_head.y += 2.1;

				CUtils::AddDbText(pModelSpace,standard_txt,ptDraw_standard_head,2.1,
                    pszStyleName,DBTA_CENTER,dWidthFactor);


				//最小值-最大值
				standard_txt.Format(_T("%.2f-%.2f"), st.minVal, st.maxVal);

				AcGePoint3d ptDraw_standard_len(ptDraw_Standard);
				ptDraw_standard_len.x += space_chat/2;
				ptDraw_standard_len.y -=2.1;

				CUtils::AddDbText(pModelSpace,standard_txt,ptDraw_standard_len,2.1,
                    pszStyleName,DBTA_CENTER,dWidthFactor);
            }

			//动探击数

			ptInsert.x += space;

			/*if (_tcscmp(arrayName, _T("ZK3")) == 0){
				break;
			}*/
			//break;
        }
    }

    this->commitTrans();
	this->actionEnd();
	this->regen();
	
}

int CCreateColumnView::actionEnd()
{
	return ToolDrawBase::actionEnd();
}
void CCreateColumnView::CollectTexts(AcDbObjectId blkDefId, CMapStringToString& mapVals, int depth)
{
    if (blkDefId.isNull() || depth > 8)      // 深度保护，防自引用死循环
        return;

    AcDbBlockTableRecord* pBTR = NULL;
    if (acdbOpenObject(pBTR, blkDefId, AcDb::kForWrite) != Acad::eOk)
        return;

    AcDbBlockTableRecordIterator* pIter = NULL;
    if (pBTR->newIterator(pIter) == Acad::eOk)
    {
        for (; !pIter->done(); pIter->step())
        {
            AcDbEntity* pEnt = NULL;
            if (pIter->getEntity(pEnt, AcDb::kForRead) != Acad::eOk)
                continue;

            //--- 1) 单行文本 ---
            AcDbText* pText = AcDbText::cast(pEnt);
            if (pText != NULL)
            {
                CString s(pText->textString());          // 指针归实体所有，先拷贝
                AcGePoint3d pt = pText->position();
                CUtils::acutPrintf(_T("%*s[单行文本] \"%s\" @ (%.1f, %.1f)\n"),
                                   depth * 2, _T(""), s.GetString(), pt.x, pt.y);
                

				// 判断是否 "[xxxx]" 占位符：以 [ 开头、] 结尾、中间无 ]
                int len = s.GetLength();
                if (len >= 2 && s[0] == _T('[') && s[len - 1] == _T(']')
                    && s.Find(_T(']'), 1) == len - 1)
                {
                    CString val;
                    if (mapVals.Lookup(s, val))          // 直接用 "[page_no]" 全名查
                    {
                        if (pEnt->upgradeOpen() == Acad::eOk)   // 读 → 写
                        {
                            pText->setTextString(val.GetString());
                            CUtils::acutPrintf(_T("    ? 已替换 %s = \"%s\"\n"),
                                            s.GetString(), val.GetString());
                        }
                        else
                        {
                            CUtils::acutPrintf(_T("    ? 警告：%s 升级写打开失败，未替换\n"),
                                            s.GetString());
                        }
                    }
                    else
                    {
                        CUtils::acutPrintf(_T("    ? 占位符 %s 在 mapVals 中未找到\n"),
                                        s.GetString());
                    }
					//制图和校对
					if (_tcscmp(s, _T("[mapping]")) == 0)
					{
						CollectMapping(pBTR,pt,_T("制图人"));
					}
					else if (_tcscmp(s, _T("[check]")) == 0)
					{
						CollectMapping(pBTR,pt,_T("校对人"));
					}
                }


                pEnt->close();
                continue;
            }

            //--- 2) 多行文本 ---
            //AcDbMText* pMText = AcDbMText::cast(pEnt);
            //if (pMText != NULL)
            //{
            //    CString s = MTextToPlain(pMText->contents());   // contents() 含格式码
            //    AcGePoint3d pt = pMText->location();
            //    CUtils::acutPrintf(_T("%*s[多行文本] \"%s\" @ (%.1f, %.1f)\n"),
            //                       depth * 2, _T(""), s.GetString(), pt.x, pt.y);
            //    pEnt->close();
            //    continue;
            //}

            ////--- 3) 属性定义（块模板里带 tag 的占位文本）---
            //AcDbAttributeDefinition* pAttDef = AcDbAttributeDefinition::cast(pEnt);
            //if (pAttDef != NULL)
            //{
            //    CString tag(pAttDef->tag());             // 标签，如 "ZK_NAME"
            //    CString val(pAttDef->textString());      // 默认值
            //    CUtils::acutPrintf(_T("%*s[属性定义] tag=\"%s\" 默认值=\"%s\"\n"),
            //                       depth * 2, _T(""), tag.GetString(), val.GetString());
            //    pEnt->close();
            //    continue;
            //}

            //--- 4) 嵌套块参照：递归进它的块定义 ---
            /*AcDbBlockReference* pNested = AcDbBlockReference::cast(pEnt);
            if (pNested != NULL)
            {
                pEnt->close();
                CollectTexts(pNested->blockTableRecordId(), depth + 1);
                continue;
            }*/

            pEnt->close();   // 其他类型：线、圆……跳过
        }
        delete pIter;        // 迭代器是 new 出来的，须 delete
    }
    pBTR->close();
}
CString CCreateColumnView::MTextToPlain(const ACHAR* s)
{
    CString out;
    if (s == NULL) return out;
    for (const ACHAR* p = s; *p != _T('\0'); ++p)
    {
        if (*p == _T('\\'))
        {
            ACHAR c = p[1];
            if (c == _T('\0')) break;
            if (c == _T('P'))                        { out += _T('\n'); ++p; }
            else if (c == _T('~'))                   { out += _T(' ');  ++p; }
            else if (c == _T('{') || c == _T('}') || c == _T('\\'))
                                                    { out += c;        ++p; }
            else   // \f..; \H..; \W..; \A..; \C..; 等以 ';' 结尾的格式码：整段丢弃
            {
                ++p;
                while (*p != _T('\0') && *p != _T(';')) ++p;
            }
        }
        else if (*p == _T('{') || *p == _T('}'))
        {
            // 分组括号，丢弃
        }
        else if (p[0] == _T('%') && p[1] == _T('%') && p[2] != _T('\0'))
        {
            if      (p[2] == _T('d')) out += (ACHAR)0x00B0;  // %%d → °
            else if (p[2] == _T('c')) out += (ACHAR)0x00D8;  // %%c → ?
            else if (p[2] == _T('p')) out += (ACHAR)0x00B1;  // %%p → ±
            p += 2;
        }
        else
            out += *p;
    }
    return out;
}



void CCreateColumnView::ChangeNo(AcDbObjectId blkDefId,ACHAR* no)
{
	AcDbBlockTableRecord* pBTR = NULL;
    if (acdbOpenObject(pBTR, blkDefId, AcDb::kForRead) != Acad::eOk)
        return;

    AcDbBlockTableRecordIterator* pIter = NULL;
    if (pBTR->newIterator(pIter) == Acad::eOk)
    {
        for (; !pIter->done(); pIter->step())
        {
            AcDbEntity* pEnt = NULL;
            if (pIter->getEntity(pEnt, AcDb::kForRead) != Acad::eOk)
                continue;

            //--- 1) 单行文本 ---
            AcDbText* pText = AcDbText::cast(pEnt);
			CString s(pText->textString());          // 指针归实体所有，先拷贝
			if (s == _T("1")){
				if (pEnt->upgradeOpen() == Acad::eOk)   // 读 → 写
                {
                    pText->setTextString(no);
				}
				pEnt->close();
				break;
			}
			pEnt->close();
		}
		delete pIter;
	}
	pBTR->close();
}
void CCreateColumnView::CollectMapping(AcDbBlockTableRecord* pBTR,AcGePoint3d& pt,TCHAR* blockname)
{
	CString strFilePath = CUtils::GetArxFolder();
	CString cadBlockUrl;
	cadBlockUrl.Format(_T("support/%s/%s.dwg"), CADYEAR, blockname);
	cadBlockUrl = strFilePath + cadBlockUrl;

    double widthBlock = 23.0;
	double widthHeight = 8.0;

	AcDbObjectId block_mapping = ImportDwgAsBlock(cadBlockUrl,blockname);
	if (block_mapping == AcDbObjectId::kNull)
        return;
	//AcDbBlockTableRecord* pBTR_mapping = NULL;
	//AcGePoint3d basePoint;
	//if (acdbOpenObject(pBTR_mapping, block_mapping, AcDb::kForRead) == Acad::eOk) {
	//	// getOrigin() 返回的是块定义的基点（局部坐标）
	//	basePoint = pBTR_mapping->origin(); 
	//	pBTR_mapping->close();
	//}

	// 创建块参照，插入到块内部 pt 位置
    AcDbBlockReference* pBlkRef = new AcDbBlockReference;
    pBlkRef->setBlockTableRecord(block_mapping);   // 引用"制图人"块定义
	AcGePoint3d pos(pt);
	pos.x +=2.5;
	pos.y +=2.5;
    pBlkRef->setPosition(pos);                      // pt 为块定义内部坐标系

    AcDbObjectId refId = AcDbObjectId::kNull;
    Acad::ErrorStatus es = pBTR->appendAcDbEntity(refId, pBlkRef);
    if (es != Acad::eOk)
    {
        delete pBlkRef;                            // 未入库 → delete
        CUtils::acutPrintf(_T("[DWG导入] 错误：插入制图人块参照失败(错误码 %d)。\n"), (int)es);
		return;
    }

    AcDbExtents ext;
    es = pBlkRef->getGeomExtents(ext);
	if (es != Acad::eOk){
		pBlkRef->close();
        return;
	}

    double w = ext.maxPoint().x - ext.minPoint().x;
    double h = ext.maxPoint().y - ext.minPoint().y;
    if (w < 0.0001 || h < 0.0001)          // 空块/退化 → 防除零
	{
		pBlkRef->close();
        return;
	}

    // 2) 目标尺寸 → 缩放系数（非等比，分别拉伸到 23×8）
    double sx = widthBlock  / w;
    double sy = widthHeight / h;
    // 若要等比缩放（不变形），改用： double s = min(sx, sy); sx = sy = s;

    // 3) 插入点补偿：缩放围绕块基点进行，
    //    让缩放后的包围盒【中心】对齐占位符位置 pt
   /* AcGePoint3d pos;
    pos.x = pt.x + w * 0.5 * sx;
    pos.y = pt.y + h * 0.5 * sy;
    pos.z = pt.z;*/

	//pBlkRef->setPosition(pos);
    pBlkRef->setScaleFactors(AcGeScale3d(sx, sy, 1.0));
    
	pBlkRef->close();
}