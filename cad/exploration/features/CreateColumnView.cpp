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

// 值类型（也可以直接用 int 0/1，枚举可读性更好）
enum JsonValType { JV_STRING, JV_NUMBER };

struct FieldMap
{
    const TCHAR* fieldName;   // 占位符名，如 "[page_no]"（也可不带括号）
    int row;
    int col;       // 数组下标（按 JSON 实际位置，可以任意、可不连续）
    JsonValType  type;        // 取值方式
    const TCHAR* defaultVal; // 默认值（可为 NULL）
};
// 映射表：声明成文件级 static，写在 CollectTexts/Execute 之前
static const FieldMap s_fieldMap[] = {
    { _T("[page_no]"),   -1,  -1, JV_STRING, _T("1") },
    { _T("[page_count]"),  -1,  -1, JV_STRING, _T("1") },
    { _T("[project_name]"),   2,  1, JV_STRING, _T("") },
    { _T("[project_no]"),  -1,  -1, JV_STRING , _T("") },
    { _T("[hole_no]"),  4,  1, JV_STRING, _T("") },
    { _T("[elevation]"),  2,  11, JV_NUMBER, _T("") },
    { _T("[diameter]"),  -1,  -1, JV_STRING, _T("") },
    { _T("[coordinate_x]"),  4,  4, JV_NUMBER, _T("") },
    { _T("[coordinate_y]"),  4,  9, JV_NUMBER, _T("") },
    { _T("[start_date]"),  3,  11, JV_STRING, _T("") },
    { _T("[completion_date]"),  4,  11, JV_STRING, _T("") },
    { _T("[water_depth]"),  -1,  -1, JV_STRING, _T("") },
    { _T("[survey_unit]"),  0,  0, JV_STRING, _T("") },
    { _T("[mapping]"),  -1,  -1, JV_STRING, _T("") },
    { _T("[check]"),  -1,  -1, JV_STRING, _T("") },
};
CCreateColumnView::CCreateColumnView():ToolDrawBase(),pExtDb(NULL)
{
    
}

CCreateColumnView::~CCreateColumnView()
{
}


//-----------------------------------------------------------------------------
bool CCreateColumnView::ImportDwgAsBlock(const ACHAR* pszDwgPath,
                                    const AcGePoint3d& ptInsert,
									CMapStringToString& mapVals,
									const char* arrayName,
                                    double dScale,
                                    double dRotation,
                                    const ACHAR* pszLayer,
                                    AcDbObjectId* pBlkRefId)
{
    if (pBlkRefId != NULL)
        *pBlkRefId = AcDbObjectId::kNull;

    if (pszDwgPath == NULL || pszDwgPath[0] == _T('\0'))
    {
        acutPrintf(_T("\n[DWG导入] 错误：文件路径为空。"));
        return false;
    }
    // 2. 把外部 DWG 读入内存侧数据库（side database）
    //    参数：buildDefaultSymbolTable=false, noDocument=true
	Acad::ErrorStatus es;
	if (pExtDb == NULL){
		pExtDb = new AcDbDatabase(false, true);
		es = pExtDb->readDwgFile(pszDwgPath);
		if (es != Acad::eOk)
		{
			acutPrintf(_T("\n[DWG导入] 错误：读取 DWG 失败(错误码 %d)：%s"), (int)es, pszDwgPath);
			delete pExtDb;
			pExtDb = NULL;
			return false;
		}
	}
    // 3. 当前工作数据库
    AcDbDatabase* pCurDb = this->pDb;
    if (pCurDb == NULL)
    {
        acutPrintf(_T("\n[DWG导入] 错误：无法获取当前工作数据库。"));
        return false;
    }

    // 4. 用文件名作为块名（重名时自动加 _1、_2 后缀）
    ACHAR szBlockName[256];
    MakeBlockName(pszDwgPath, szBlockName, 256);
    if (arrayName != NULL && arrayName[0] != '\0')
    {
        // arrayName 来自 cJSON，是 UTF-8 编码，先转成 TCHAR
        ACHAR szArrName[256];
        CUtils::utf8ToTChar(arrayName, szArrName, 256);

        // arrayName 来自 JSON 键名，可能含块名非法字符，替换为 '_'
        for (ACHAR* p = szArrName; *p != _T('\0'); ++p)
        {
            if (CUtils::IsIllegalBlockNameChar(*p))
                *p = _T('_');
        }

        if (szArrName[0] != _T('\0'))
        {
            // 拼接 szBlockName + "_" + szArrName
            // 长度预算：GetUniqueBlockName 之后最多再追加 "_999"（4字符+NUL），
            // 所以总长控制在 250 以内
            int base = (int)_tcslen(szBlockName);
            int maxArr = 250 - base - 1;      // 减 1 是给 '_' 占位
            if (maxArr > 0)
            {
                ACHAR szBuf[256];
                CUtils::SafeCopy(szBuf, 256, szBlockName, base);
                szBuf[base] = _T('_');
                ++base;

                int n = 0;
                while (n < maxArr && szArrName[n] != _T('\0'))
                {
                    szBuf[base + n] = szArrName[n];
                    ++n;
                }
                szBuf[base + n] = _T('\0');

                CUtils::SafeCopy(szBlockName, 256, szBuf, base + n);
            }
            // maxArr <= 0：文件名部分太长放不下，退回用纯文件名
        }
    }

    // 5. 判断块是否已存在
    AcDbBlockTable* pBlkTbl = NULL;
    if (pCurDb->getBlockTable(pBlkTbl, AcDb::kForRead) != Acad::eOk)
    {
        acutPrintf(_T("\n[DWG导入] 错误：无法打开块表。"));
        return false;
    }

    AcDbObjectId blkDefId = AcDbObjectId::kNull;
    AcDbObjectId existingId;
    bool bExist = false;
    if (pBlkTbl->getAt(szBlockName, existingId) == Acad::eOk
        && !existingId.isNull())
    {
        blkDefId = existingId;
        bExist = true;
        CUtils::acutPrintf(_T("\n[DWG导入] 块 \"%s\" 已存在，不插入新参照。"), szBlockName);

		//存在则跳过
		pBlkTbl->close();
		return false;
    }
    pBlkTbl->close();

    if (!bExist)
    {
        // 不存在：导入块定义
        es = pCurDb->insert(blkDefId, szBlockName, pExtDb, true);
        if (es != Acad::eOk || blkDefId.isNull())
        {
            CUtils::acutPrintf(_T("\n[DWG导入] 错误：导入块定义失败(错误码 %d)，块名：%s"),
                (int)es, szBlockName);
            return false;
        }

        // 6. 插入块参照 —— ★ 仅新建块执行，已存在块完全跳过（坐标/图层都不动）
        AcDbBlockTableRecord* pModelSpace = this->pModelSpace;
        AcDbBlockReference* pBlkRef = new AcDbBlockReference;
        pBlkRef->setBlockTableRecord(blkDefId);
        pBlkRef->setPosition(ptInsert);
        pBlkRef->setScaleFactors(AcGeScale3d(dScale));
        pBlkRef->setRotation(dRotation);
        if (pszLayer != NULL && pszLayer[0] != _T('\0'))
            pBlkRef->setLayer(pszLayer);

        AcDbObjectId refId = AcDbObjectId::kNull;
        es = pModelSpace->appendAcDbEntity(refId, pBlkRef);
        if (es != Acad::eOk)
        {
            CUtils::acutPrintf(_T("\n[DWG导入] 错误：添加块参照失败(错误码 %d)。"), (int)es);
            delete pBlkRef;          // 未入库，delete（不能 close）
            return false;
        }
        pBlkRef->close();
        if (pBlkRefId != NULL)
            *pBlkRefId = refId;
        CUtils::acutPrintf(_T("\n[DWG导入] 成功：块 \"%s\" 已插入到 (%.3f, %.3f, %.3f)。"),
            szBlockName, ptInsert.x, ptInsert.y, ptInsert.z);
    }

    // 7. 遍历块内文本 —— ★ 两条路都执行
    CollectTexts(blkDefId, mapVals, 0);
    return true;
}

//-----------------------------------------------------------------------------
void CCreateColumnView::MakeBlockName(const ACHAR* pszDwgPath, ACHAR* pszName, int nNameCount)
{
    // 取最后一个 \ 或 / 之后的文件名部分
    const ACHAR* pszFile = pszDwgPath;
    for (const ACHAR* p = pszDwgPath; *p != _T('\0'); ++p)
    {
        if (*p == _T('\\') || *p == _T('/'))
            pszFile = p + 1;
    }

    // 去掉扩展名，替换非法字符；预留 8 字符给可能的 "_NNN" 后缀
    int nMax = nNameCount - 8;
    int n = 0;
    for (const ACHAR* p = pszFile; *p != _T('\0') && *p != _T('.') && n < nMax; ++p)
    {
		pszName[n] = CUtils::IsIllegalBlockNameChar(*p) ? _T('_') : *p;
        ++n;
    }
    pszName[n] = _T('\0');

    if (n == 0)
        CUtils::SafeCopy(pszName, nNameCount, _T("DWG_BLOCK"), 9);
}

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
	CString strFilePath = CUtils::GetArxFolder();
	strFilePath.Replace(_T("\\"), _T("/"));
	CString strUrl;
	strUrl = strFilePath + _T("/project/钻探编录表.json");
	CString cadBlockUrl;
#ifdef _CAD2005
	cadBlockUrl = strFilePath + _T("support/2005/钻孔柱状图.dwg");
#else
	cadBlockUrl = strFilePath + _T("support/2007/钻孔柱状图.dwg");
#endif
    if (!PathFileExists(cadBlockUrl))
    {
        // 存在（文件或目录均可，配合 PathIsDirectory 区分）
        CUtils::acutPrintf(_T("[CAD ERROR] cad文件不存在! %s\n"), cadBlockUrl.GetString());
		return;
    }

	// Read file content
	FILE* fp = _tfopen(strUrl.GetString(), _T("rb")); // Use binary mode for reading
	if (!fp) {
		// File does not exist or cannot be opened
		CUtils::acutPrintf(_T("[CAD ERROR] 没有导入数据，请先完成数据导入! %s\n"), strUrl.GetString());
		return;
	}

	// Seek to end to get file size
	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (fileSize <= 0) {
		fclose(fp);
		return;
	}

	// Allocate buffer and read file
	char* fileContent = (char*)malloc(fileSize + 1);
	if (!fileContent) {
		fclose(fp);
		return;
	}

	size_t readSize = fread(fileContent, 1, fileSize, fp);
	fileContent[readSize] = '\0';
	fclose(fp);

	// Parse JSON
	cJSON* s_pJsonData = cJSON_Parse(fileContent);
	free(fileContent);

    if (s_pJsonData == NULL) {
        CUtils::acutPrintf(_T("[CAD ERROR] JSON 解析失败: %s\n"), strUrl.GetString());
        return;
    }
    int ret = this->actionBefore();
	if (ret) return;

    cJSON* item = NULL;
    AcGePoint3d ptInsert;
	double space = 222.6865;


	double contentHeight = 212.0;
	double headerHeight = 28.0;
	double gridHeight = contentHeight - headerHeight;
	double gridWidth = 176.0;

	double offsetX = 27.0;
    double offsetY = 17.0;
	
	double space_no = 8.0475;
	double space_mc = 14.4856;
	double space_sd = 12.0713;
	double space_chat = 14.4856;
	double space_desc = 45.6296;
	double space_last = 16.0951;




    cJSON_ArrayForEach(item, s_pJsonData) {
        // 处理每个属性
        if (cJSON_IsArray(item)) {
            const char* arrayName = (item->string != NULL) ? item->string : "(匿名数组)";
            CUtils::acutPrintf(_T("[数组] %hs，共 %d 个元素\n"),
                            arrayName, cJSON_GetArraySize(item));

			CMapStringToString mapVals;
            const int rowSize   = cJSON_GetArraySize(item);
            const int MAP_COUNT = (int)(sizeof(s_fieldMap) / sizeof(s_fieldMap[0]));

            for (int i = 0; i < MAP_COUNT; ++i)
            {
                const FieldMap& fm = s_fieldMap[i];
				if(fm.fieldName == _T("[elevation]")){
					CUtils::acutPrintf(_T("[elevation]"));

				}
                if (fm.row >= rowSize || fm.row < 0)
                {
                    mapVals.SetAt(fm.fieldName, fm.defaultVal);
                    continue;
                }

                cJSON* elem_row = cJSON_GetArrayItem(item, fm.row);
                if (elem_row == NULL){
                    mapVals.SetAt(fm.fieldName, fm.defaultVal);
                    continue;
                }
                const int colSize   = cJSON_GetArraySize(elem_row);
                if (fm.col >= colSize || fm.col < 0)
                {
                    mapVals.SetAt(fm.fieldName, fm.defaultVal);
                    continue;
                }

                cJSON* elem_col = cJSON_GetArrayItem(elem_row, fm.col);
                if (elem_col == NULL){
                    mapVals.SetAt(fm.fieldName, fm.defaultVal);
                    continue;
                }

                CString v;
                if (fm.type == JV_STRING && cJSON_IsString(elem_col))
                {
                    TCHAR buf[512];
                    CUtils::utf8ToTChar(elem_col->valuestring, buf, 512);  // UTF-8 → TCHAR
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

            ImportDwgAsBlock(cadBlockUrl.GetString(),ptInsert, mapVals, arrayName);

			//数据行
			int dataRow = rowSize - 8;
			int realHeight;
			if (dataRow>=5){
				realHeight = gridHeight;
			}else{
				realHeight = gridHeight/(dataRow+1)*dataRow;
			}
            //在这里计算每行的高度
            // 数据行：第 7 行 ~ 倒数第二行(rowSize-2)；col1=自，col2=至
            double totalDepth = 1.0;
            cJSON* lastRowJson = cJSON_GetArrayItem(item, rowSize - 2);   // 倒数第二行=最深一行
            if (lastRowJson != NULL)
            {
                cJSON* cTo = cJSON_GetArrayItem(lastRowJson, 2);          // “至”=终孔深度
                if (cTo != NULL && cJSON_IsNumber(cTo))
                    totalDepth = cTo->valuedouble;                        // ★算术必须用 valuedouble
            }
            if (totalDepth <= 0.0) totalDepth = 1.0;                      // 防除零

            double scale = realHeight / totalDepth;                       // 深度→图面比例
            double yTop  = ptInsert.y + offsetY + gridHeight;             // 格顶=深度0（孔口）
            double prevBottom = 0.0;                                      // 上一行底部深度


			
            for (int i = 0; i < dataRow; ++i)
            {
                cJSON* rowJson = cJSON_GetArrayItem(item, 7 + i);
                double toDepth = prevBottom;
                if (rowJson != NULL)
                {
                    cJSON* cTo = cJSON_GetArrayItem(rowJson, 2);          // 当前行“至”
                    if (cTo != NULL && cJSON_IsNumber(cTo))
                        toDepth = cTo->valuedouble;
                }
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
                ptEnd.x  += gridWidth;               // 向右延伸一个表格宽度

                AcDbLine* pLine = new AcDbLine(ptDraw, ptEnd);
                // pLine->setDatabaseDefaults();     // 可选：随当前图层/颜色默认值

                AcDbObjectId lineId = AcDbObjectId::kNull;
                Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
                if (es == Acad::eOk)
                    pLine->close();                  // 入库成功 → close（不能 delete）
                else
                    delete pLine;                    // 入库失败 → 未入库，只能 delete
				
            }

			ptInsert.x += space;
			break;
        }
    }

	

	this->actionEnd();

	this->regen();
	
}

int CCreateColumnView::actionEnd()
{
	if (pExtDb)
		delete pExtDb;
	return ToolDrawBase::actionEnd();
}
void CCreateColumnView::CollectTexts(AcDbObjectId blkDefId, CMapStringToString& mapVals, int depth)
{
    if (blkDefId.isNull() || depth > 8)      // 深度保护，防自引用死循环
        return;

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


