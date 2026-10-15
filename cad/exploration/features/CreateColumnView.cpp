// coding: gb18030
//=============================================================================
// CDwgImporter ʵ�� - ��ȡ DWG ����Ϊ�鵼�뵽ָ��λ�ã�AutoCAD 2007 / acdb17��
//=============================================================================
#include "StdAfx.h"        // ����ĿԼ����cpp �������Ȱ�����MFC _DEBUG ��ܣ�
#include "arxHeaders.h"
#include <shlwapi.h>
#include "cJSON.h"
#include "utils.h"
#include "CreateColumnView.h"
#include "PatHatchLoader.h"

// ֵ���ͣ�Ҳ����ֱ���� int 0/1��ö�ٿɶ��Ը��ã�
enum JsonValType { JV_STRING, JV_NUMBER };

struct FieldMap
{
    const TCHAR* fieldName;   // ռλ�������� "[page_no]"��Ҳ�ɲ������ţ�
    const TCHAR* firstName;
    const TCHAR* secondName;       // �����±꣨�� JSON ʵ��λ�ã��������⡢�ɲ�������
    JsonValType  type;        // ȡֵ��ʽ
    const TCHAR* defaultVal; // Ĭ��ֵ����Ϊ NULL��
};
// ���ͳ�ƽ������˳����飩
struct SPT_STAT
{
    double rodLength;    // �˳����������
    double minVal;       // ������Сֵ
    double maxVal;       // �������ֵ
    int    totalCount;   // ����ͳ��������
    SPT_STAT() : rodLength(0), minVal(0), maxVal(0), totalCount(0) {}
};

// ӳ������������ļ��� static��д�� CollectTexts/Execute ֮ǰ
static const FieldMap s_fieldMap[] = {
    { _T("[page_no]"),   _T("��ǰҳ"), _T(""), JV_STRING, _T("1") },
    { _T("[page_count]"),  _T("��ҳ��"),  _T(""), JV_STRING, _T("1") },
    { _T("[project_name]"),  _T("��������"),  _T(""), JV_STRING, _T("") },
    { _T("[project_no]"),  _T(""),  _T(""), JV_STRING , _T("") },
    { _T("[hole_no]"),  _T("��ױ��"), _T(""), JV_STRING, _T("") },
    { _T("[elevation]"),  _T("�׿ڸ߳�(m)"),  _T(""), JV_NUMBER, _T("") },
    { _T("[diameter]"),  _T(""),  _T(""), JV_STRING, _T("") },
    { _T("[coordinate_x]"),  _T("X����(m)"),  _T(""), JV_NUMBER, _T("") },
    { _T("[coordinate_y]"),  _T("Y����(m)"),   _T(""), JV_NUMBER, _T("") },
    { _T("[start_date]"),  _T("��������"),  _T(""), JV_STRING, _T("") },
    { _T("[completion_date]"),  _T("�տ�����"),  _T(""), JV_STRING, _T("") },

	{ _T("[water_depth]"),  _T("�ȶ�ˮλ(m)"),  _T(""), JV_STRING, _T("") },
    { _T("[survey_unit]"),  _T("��˾����"),  _T(""), JV_STRING, _T("") },
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
        if (pBlkTbl->getAt(szTry, existingId) != Acad::eOk)   // ������ �� ����
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
	cadBlockUrl.Format(_T("support/%s/�����״ͼ.dwg"), CADYEAR);
	cadBlockUrl = strFilePath + cadBlockUrl;
	CString cadNoBlockUrl;
	cadNoBlockUrl.Format(_T("support/%s/���.dwg"), CADYEAR);
	cadNoBlockUrl = strFilePath + cadNoBlockUrl;
    CString cadPatUrl = CUtils::GetArxFolder() + _T("support\\pat");
    CString cadSoilConfigUrl= strFilePath + _T("support/pat_define.json");

    if (!PathFileExists(cadBlockUrl)
		|| !PathFileExists(cadNoBlockUrl))
    {
        // ���ڣ��ļ���Ŀ¼���ɣ���� PathIsDirectory ���֣�
        CUtils::acutPrintf(_T("[CAD ERROR] cad�ļ�������! %s\n"), cadBlockUrl.GetString());
		return;
    }

    // ���� JSON��ʧ��ʱ�ڲ��Ѵ�ӡ�������ﱣ��ԭ��ҵ����ʾ��
    s_patConfigData = CUtils::LoadJsonFile(cadSoilConfigUrl.GetString());   // ��Ĭģʽ
    if (s_patConfigData == NULL) {
        CUtils::acutPrintf(_T("[CAD ERROR] û��pat�����ļ������飡 %s\n"), cadSoilConfigUrl.GetString());
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

	ACHAR* pszStyleName = _T("�ɹ�ͼ����");
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

    int holeIdx = 0;
    cJSON_ArrayForEach(item, s_pJsonData) {
        // ����ÿ������
        if (item != NULL) {
            const char* blockName = (item->string != NULL) ? item->string : "(��������)";
            CUtils::acutPrintf(_T("[����] %hs���� %d ��Ԫ��\n"),
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

            // ���ͳ�����������˳����飨���׷�Χ��
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
                    CUtils::utf8ToTChar(elem_col->valuestring, buf, 512);  // UTF-8 �� TCHAR
                    v = buf;
                }
				else if (fm.type == JV_STRING && cJSON_IsNumber(elem_col))
                {
                    TCHAR buf[512];
                    CUtils::utf8ToTChar(elem_col->valueString, buf, 512);  // UTF-8 �� TCHAR
                    v = buf;
                }
                else if (fm.type == JV_NUMBER && cJSON_IsNumber(elem_col))
                {
                    TCHAR buf[512];
                    CUtils::utf8ToTChar(elem_col->valueString, buf, 512);  // UTF-8 �� TCHAR
                    v = buf;
                }
                else if (cJSON_IsNull(elem_col))
                {
                    v = fm.defaultVal;                         // ��ֵ �� Ĭ��ֵ
                }
                else
                {
                    continue;                           // ��������������������
                }
				CUtils::acutPrintf(_T("[����] key: %s : %s \n"),fm.fieldName, v);

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
			

			//������
			int dataRow = 0;
            //���������ÿ�еĸ߶�
            // �����У��� 7 �� ~ �����ڶ���(rowSize-2)��col1=�ԣ�col2=��
            double totalDepth = 1.0;
			// DEBUG: 打印当前钻孔对象所有key，定位地层列表为何找不到
			{
				acutPrintf(_T("[DEBUG] hole object keys:\n"));
				cJSON* k = NULL;
				cJSON_ArrayForEach(k, item) {
					if (k->string) acutPrintf(_T("[DEBUG]   - %hs\n"), k->string);
				}
			}
			cJSON* layerObj = CUtils::GetJsonObjectItemUtf8(item,_T("地层列表"));
			if (layerObj == NULL) {
				acutPrintf(_T("[DEBUG] WARNING: layerObj(地层列表) is NULL, skip layer drawing\n"));
				continue;
			}
			acutPrintf(_T("[DEBUG] found 地层列表, total layers: %d\n"), cJSON_GetArraySize(layerObj));
			dataRow = cJSON_GetArraySize(layerObj);   // ����Ԫ�ظ���
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
                    continue;                                   // ���� �� ��������
                cJSON* cTo = CUtils::GetJsonObjectItemUtf8(rowTry,_T("������(m)"));    // ������=�տ����
                if (cTo != NULL && cJSON_IsNumber(cTo) && cTo->valuedouble > 0.0)
                {
                    totalDepth = cTo->valuedouble;              // ������������ valuedouble
                    break;
                }
            }
            if (totalDepth <= 0.0) totalDepth = 1.0;                      // ������

            double scale = realHeight / totalDepth;                       // ��ȡ�ͼ�����
            double yTop  = ptInsert.y + offsetY + gridHeight;             // ��=���0���׿ڣ�
            double prevBottom = 0.0;                                      // ��һ�еײ����


            mapVals.Lookup(_T("[elevation]"), val);
			double elevation = CUtils::CStringToDouble(val);	//�߳�
			

			double preTop;
            for (int i = 0; i < dataRow; ++i)
            {
                double toDepth = prevBottom;
				cJSON* rowTry = cJSON_GetArrayItem(layerObj, i);
				cJSON* cTo = CUtils::GetJsonObjectItemUtf8(rowTry,_T("������(m)"));          // ��ǰ�С�����
                if (cTo != NULL && cJSON_IsNumber(cTo))
                    toDepth = cTo->valuedouble;
				else
					continue;
                double thick   = toDepth - prevBottom;                    // ����һ�бȽϡ�ʵ�ʲ��
                double hDraw   = thick * scale;                           // ����ͼ��ʵ�ʸ߶�
                double yBottom = yTop - toDepth * scale;                  // ���еײ�Y
                CUtils::acutPrintf(_T("[��%d] ��=%.2f ���=%.2f ͼ���=%.3f �ײ�Y=%.3f\n"),
                                i, toDepth, thick, hDraw, yBottom);
                prevBottom = toDepth;
                // TODO: �� hDraw / yBottom �������ߡ���λ���֡�ѹ�� hatch����

				//�ײ����ߣ����=���еײ���˽ǵ㣬�������� gridWidth
                AcGePoint3d ptDraw(ptInsert);
                ptDraw.x += offsetX;
				//ptDraw.y += offsetY;
                ptDraw.y = yBottom;                 // �� yBottom ���Ǿ���Y��ֱ�Ӹ�ֵ����Ҫ += offsetY �� += yBottom��

                CUtils::acutPrintf(_T("��ʼ x=%.2f y=%.2f \n"),
                                ptDraw.x, ptDraw.y);

                AcGePoint3d ptEnd(ptDraw);
                ptEnd.x  += gridWidth - lastWidth - space_desc;               // ��������һ���������

                
                // pLine->setDatabaseDefaults();     // ��ѡ���浱ǰͼ��/��ɫĬ��ֵ
                
				{
					//��ͷ�����������ĺ���
					AcDbLine* pLine = new AcDbLine(ptDraw, ptEnd);
					AcDbObjectId lineId = AcDbObjectId::kNull;
					Acad::ErrorStatus es = pModelSpace->appendAcDbEntity(lineId, pLine);
					if (es == Acad::eOk)
						pLine->close();                  // ���ɹ� �� close������ delete��
					else
						delete pLine;                    // ���ʧ�� �� δ��⣬ֻ�� delete
				}

				//�������
				AcGePoint3d ptDrawXh(ptDraw);
				ptDrawXh.x += space_no/2;
				ptDrawXh.y += hDraw/2;

				CString no_name;
				no_name.Format(_T("%s_%d"), arrayName,i);

				AcDbObjectId blkDefId_xh = ImportDwgAsBlock(cadNoBlockUrl.GetString(),ptDrawXh, no_name.GetString());
				if (blkDefId_xh != AcDbObjectId::kNull){
					cJSON* cIndex = CUtils::GetJsonObjectItemUtf8(rowTry,_T("�ز���"));  // ��ǰ�С�������
					if (cIndex != NULL && cJSON_IsString(cIndex)){
                        std::vector<CString> arrSeg;
                        if (CUtils::SplitUtf8(cIndex->valuestring, '-', arrSeg) > 0)
                        {
                            ChangeNo(blkDefId_xh, (ACHAR*)arrSeg[0].GetString());   // "1-0-0" �� "1"
                        }
					}
				}

                CString soilPatName;
				//������������
				{
					cJSON* cSoi = CUtils::GetJsonObjectItemUtf8(rowTry,_T("�ز�����"));  // ��ǰ�С��������֡�
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

				//���Ӹ̣߳���ȣ����
				{
					//�߳�
					AcGePoint3d ptDraw_elevation(ptDraw);
					ptDraw_elevation.x += space_no + space_mc + space_sd + space_sd/2;
					ptDraw_elevation.y += 3;
					double elevation_cur = elevation - toDepth;
	
					CString str;
					str.Format(_T("%.2f"), elevation_cur);
					CUtils::AddDbText(pModelSpace,(ACHAR*)str.GetString(),ptDraw_elevation,
                    2.1,pszStyleName,DBTA_CENTER,dWidthFactor);


					//���
					AcGePoint3d ptDraw_depth(ptDraw);
					ptDraw_depth.x += space_no + space_mc + space_sd * 2 + space_sd/2;
					ptDraw_depth.y += 3;
	
					str.Format(_T("%.2f"), toDepth);
					CUtils::AddDbText(pModelSpace,(ACHAR*)str.GetString(),ptDraw_depth,2.1,
                    pszStyleName,DBTA_CENTER,dWidthFactor);


					//���
					AcGePoint3d ptDraw_thick(ptDraw);
					ptDraw_thick.x += space_no + space_mc + space_sd * 3 + space_sd/2;
					ptDraw_thick.y += 3;
	
					str.Format(_T("%.2f"), thick);
					CUtils::AddDbText(pModelSpace,(ACHAR*)str.GetString(),ptDraw_thick,2.1,
                    pszStyleName,DBTA_CENTER,dWidthFactor);
				}

				{
					//��������
					//��Ҫ�������ߵ�����
					cJSON* cDesc  = CUtils::GetJsonObjectItemUtf8(rowTry,_T("�ɷּ�����"));  
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
									//�� \_/����
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
									//ֱ�ӻ�ֱ��
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
                    //�������ͼ��
                    double soilLeft = ptDraw.x + space_no + space_mc + space_sd * 4;
                    AcGePoint2dArray rect;                         
                    rect.append(AcGePoint2d(soilLeft,   ptDraw.y));
                    rect.append(AcGePoint2d(soilLeft+space_chat,  ptDraw.y));
                    rect.append(AcGePoint2d(soilLeft+space_chat, ptDraw.y + hDraw));
                    rect.append(AcGePoint2d(soilLeft,  ptDraw.y + hDraw));
                    rect.append(AcGePoint2d(soilLeft,   ptDraw.y));

                    AcGeDoubleArray bulges;
                    for (int b = 0; b < rect.length(); ++b)     // �������� b�������ڱ������ѭ���� i
                        bulges.append(0.0);

                    // ������ �� ͼ�������� "����" �� "gt"
                    TCHAR patName[128] = { 0 };
                    if (CUtils::FindPatNameBySoilName(s_patConfigData, soilPatName.GetString(),
                                              patName, ARRAYSIZE(patName)))
                    {
                        AcDbObjectId hatchId = AcDbObjectId::kNull;
                        Acad::ErrorStatus es = patLoader.CreateHatchByName(
                            patName, rect, bulges, 0.5, 0.0, NULL, hatchId);
                        if (es != Acad::eOk)
                            CUtils::acutPrintf(_T("[CAD WARN] ���ʧ��(%d)������ \"%s\" ͼ�� \"%s\"\n"),
                                (int)es, soilPatName.GetString(), patName);
                    }
                    else
                    {
                        CUtils::acutPrintf(_T("[CAD WARN] pat_define.json δ�ҵ����� \"%s\" ��ͼ�����ƣ��������\n"),
                            soilPatName.GetString());
                    }
                }

				//ȡ��
				{
					cJSON* cSampling_obj =  CUtils::GetJsonObjectItemUtf8(rowTry,_T("ȡ��(����ˮ)��"));
					if (cSampling_obj != NULL)
					{
						cJSON* cSampling_no  = CUtils::GetJsonObjectItemUtf8(cSampling_obj,_T("���"));  
						cJSON* cSampling_num  = CUtils::GetJsonObjectItemUtf8(cSampling_obj,_T("ȡ�����"));  

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
								ptDraw_sampling_end.x  +=space_chat;               // ��������һ���������

								AcDbLine* pLine = new AcDbLine(ptDraw_sampling, ptDraw_sampling_end);

								AcDbObjectId lineId = AcDbObjectId::kNull;
								Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
								if (es == Acad::eOk)
									pLine->close();                  // ���ɹ� �� close������ delete��
								else
									delete pLine;                    // ���ʧ�� �� δ��⣬ֻ�� delete

								ACHAR sampling_txt[256] = { 0 };
								CUtils::utf8ToTChar(cSampling_no->valuestring, sampling_txt, ARRAYSIZE(sampling_txt));

								//ȡ�����
								AcGePoint3d ptDraw_sampling_txt_no(ptDraw_sampling);
								ptDraw_sampling_txt_no.x += space_chat/2;
								ptDraw_sampling_txt_no.y += 2.5;

								CUtils::AddDbText(pModelSpace,sampling_txt,ptDraw_sampling_txt_no,
									2.1,pszStyleName,DBTA_CENTER,dWidthFactor);


								//ȡ����ֵ
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
				
				//ͳ�Ʊ��
				{
					cJSON* ctesting_obj =  CUtils::GetJsonObjectItemUtf8(rowTry,_T("ԭλ����"));
					if (ctesting_obj != NULL)
					{
						cJSON* cStandard_type  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("����"));

						if (cStandard_type != NULL && cJSON_IsString(cStandard_type))
						{
							// JSON ֵ�� UTF-8��ת TCHAR �������������Ƚ�
							TCHAR szType[64] = { 0 };
							CUtils::utf8ToTChar(cStandard_type->valuestring, szType, ARRAYSIZE(szType));

							if (_tcscmp(szType, _T("���")) == 0)
							{
								cJSON* cStandard_rod_length  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("�˳�(m)"));

								cJSON* cStandard_rod_min  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("��(m)"));
								cJSON* cStandard_rod_max  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("ֹ(m)"));
								cJSON* cStandard_rod_count  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("������(��)"));

								double rodLength = CUtils::JsonValToDouble(cStandard_rod_length);
								double rodMin    = CUtils::JsonValToDouble(cStandard_rod_min);
								double rodMax    = CUtils::JsonValToDouble(cStandard_rod_max);
								int    rodCount  = (int)(CUtils::JsonValToDouble(cStandard_rod_count) + 0.5);

								// �鲢���˳���ͬ�����з��飨�����ݲ�Ƚϣ�
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
								if (!bFound)      // �¸˳� �� �½�����
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

                //ͳ�ƶ�̽
                {
					cJSON* ctesting_obj =  CUtils::GetJsonObjectItemUtf8(rowTry,_T("ԭλ����"));
					if (ctesting_obj != NULL)
					{
						cJSON* cDT_type  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("����"));

						if (cDT_type != NULL && cJSON_IsString(cDT_type))
						{
							// JSON ֵ�� UTF-8��ת TCHAR �������������Ƚ�
							TCHAR szType[64] = { 0 };
							CUtils::utf8ToTChar(cDT_type->valuestring, szType, ARRAYSIZE(szType));

							if (_tcscmp(szType, _T("���Ͷ�̽")) == 0)
							{
								cJSON* cDT_min  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("��(m)"));
								cJSON* cDT_max  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("ֹ(m)"));
								cJSON* cDT_desc  = CUtils::GetJsonObjectItemUtf8(ctesting_obj,_T("������(��)"));

								double dtMin    = CUtils::JsonValToDouble(cDT_min);
								double dtMax    = CUtils::JsonValToDouble(cDT_max);

								AcGePoint3d ptDraw_Dt(ptDraw);
								ptDraw_Dt.x += space_no + space_mc + space_sd * 4 + space_chat + space_desc + space_chat*2;
								ptDraw_Dt.y = yTop - dtMax * scale;

								AcGePoint3d ptDraw_Dt_end(ptDraw_Dt);
								ptDraw_Dt_end.x  +=space_last;               // ��������һ���������

								AcDbLine* pLine = new AcDbLine(ptDraw_Dt, ptDraw_Dt_end);

								AcDbObjectId lineId = AcDbObjectId::kNull;
								Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
								if (es == Acad::eOk)
									pLine->close();                  
								else
									delete pLine;      

								ACHAR desc_txt[256] = { 0 };
								CUtils::utf8ToTChar(cDT_desc->valuestring, desc_txt, ARRAYSIZE(desc_txt));

								//��̽����
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
									if (pText->upgradeOpen() == Acad::eOk)        // �� �� д
									{
										AcGePoint3d ptLoc = pText->location();     // ȡ��ǰ���ŵ�
										ptLoc.x = ptDraw_dt_head.x;                
										ptLoc.y += height_desc;                
										pText->setLocation(ptLoc);
									}
									pText->close();   // entity_desc �� pText ��ͬһ����close һ�μ���
								}else{
									entity_desc->close();
								}

								//��Сֵ-���ֵ
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

			//������
            for (size_t k = 0; k < sptStats.size(); ++k)
            {
                const SPT_STAT& st = sptStats[k];
                CUtils::acutPrintf(_T("[���ͳ��] �˳�=%.2f  ��Сֵ=%.1f  ���ֵ=%.1f  ͳ����=%d\n"),
                    st.rodLength, st.minVal, st.maxVal, st.totalCount);

				AcGePoint3d ptDraw_Standard(ptInsert);
				ptDraw_Standard.x += offsetX + space_no + space_mc + space_sd * 4 + space_chat + space_desc + space_chat;
				ptDraw_Standard.y = yTop - st.maxVal * scale;

				AcGePoint3d ptDraw_Standard_end(ptDraw_Standard);
				ptDraw_Standard_end.x  +=space_chat;               // ��������һ���������

				AcDbLine* pLine = new AcDbLine(ptDraw_Standard, ptDraw_Standard_end);

				AcDbObjectId lineId = AcDbObjectId::kNull;
				Acad::ErrorStatus es = this->pModelSpace->appendAcDbEntity(lineId, pLine);
				if (es == Acad::eOk)
					pLine->close();                  // ���ɹ� �� close������ delete��
				else
					delete pLine;                    // ���ʧ�� �� δ��⣬ֻ�� delete

				CString standard_txt;
				standard_txt.Format(_T("=%d"), st.totalCount);

				//�˳�
				AcGePoint3d ptDraw_standard_head(ptDraw_Standard);
				ptDraw_standard_head.x += space_chat/2;
				ptDraw_standard_head.y += 2.1;

				CUtils::AddDbText(pModelSpace,standard_txt,ptDraw_standard_head,2.1,
                    pszStyleName,DBTA_CENTER,dWidthFactor);


				//��Сֵ-���ֵ
				standard_txt.Format(_T("%.2f-%.2f"), st.minVal, st.maxVal);

				AcGePoint3d ptDraw_standard_len(ptDraw_Standard);
				ptDraw_standard_len.x += space_chat/2;
				ptDraw_standard_len.y -=2.1;

				CUtils::AddDbText(pModelSpace,standard_txt,ptDraw_standard_len,2.1,
                    pszStyleName,DBTA_CENTER,dWidthFactor);
            }

			//��̽����

			ptInsert.x += space;

			++holeIdx;
			if (holeIdx % 10 == 0) {
				MSG msg;
				while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

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
    if (blkDefId.isNull() || depth > 8)      // ��ȱ���������������ѭ��
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

            //--- 1) �����ı� ---
            AcDbText* pText = AcDbText::cast(pEnt);
            if (pText != NULL)
            {
                CString s(pText->textString());          // ָ���ʵ�����У��ȿ���
                AcGePoint3d pt = pText->position();
                CUtils::acutPrintf(_T("%*s[�����ı�] \"%s\" @ (%.1f, %.1f)\n"),
                                   depth * 2, _T(""), s.GetString(), pt.x, pt.y);
                

				// �ж��Ƿ� "[xxxx]" ռλ������ [ ��ͷ��] ��β���м��� ]
                int len = s.GetLength();
                if (len >= 2 && s[0] == _T('[') && s[len - 1] == _T(']')
                    && s.Find(_T(']'), 1) == len - 1)
                {
                    CString val;
                    if (mapVals.Lookup(s, val))          // ֱ���� "[page_no]" ȫ����
                    {
                        if (pEnt->upgradeOpen() == Acad::eOk)   // �� �� д
                        {
                            pText->setTextString(val.GetString());
                            CUtils::acutPrintf(_T("    ? ���滻 %s = \"%s\"\n"),
                                            s.GetString(), val.GetString());
                        }
                        else
                        {
                            CUtils::acutPrintf(_T("    ? ���棺%s ����д��ʧ�ܣ�δ�滻\n"),
                                            s.GetString());
                        }
                    }
                    else
                    {
                        CUtils::acutPrintf(_T("    ? ռλ�� %s �� mapVals ��δ�ҵ�\n"),
                                        s.GetString());
                    }
					//��ͼ��У��
					if (_tcscmp(s, _T("[mapping]")) == 0)
					{
						CollectMapping(pBTR,pt,_T("��ͼ��"));
					}
					else if (_tcscmp(s, _T("[check]")) == 0)
					{
						CollectMapping(pBTR,pt,_T("У����"));
					}
                }


                pEnt->close();
                continue;
            }

            //--- 2) �����ı� ---
            //AcDbMText* pMText = AcDbMText::cast(pEnt);
            //if (pMText != NULL)
            //{
            //    CString s = MTextToPlain(pMText->contents());   // contents() ����ʽ��
            //    AcGePoint3d pt = pMText->location();
            //    CUtils::acutPrintf(_T("%*s[�����ı�] \"%s\" @ (%.1f, %.1f)\n"),
            //                       depth * 2, _T(""), s.GetString(), pt.x, pt.y);
            //    pEnt->close();
            //    continue;
            //}

            ////--- 3) ���Զ��壨��ģ����� tag ��ռλ�ı���---
            //AcDbAttributeDefinition* pAttDef = AcDbAttributeDefinition::cast(pEnt);
            //if (pAttDef != NULL)
            //{
            //    CString tag(pAttDef->tag());             // ��ǩ���� "ZK_NAME"
            //    CString val(pAttDef->textString());      // Ĭ��ֵ
            //    CUtils::acutPrintf(_T("%*s[���Զ���] tag=\"%s\" Ĭ��ֵ=\"%s\"\n"),
            //                       depth * 2, _T(""), tag.GetString(), val.GetString());
            //    pEnt->close();
            //    continue;
            //}

            //--- 4) Ƕ�׿���գ��ݹ�����Ŀ鶨�� ---
            /*AcDbBlockReference* pNested = AcDbBlockReference::cast(pEnt);
            if (pNested != NULL)
            {
                pEnt->close();
                CollectTexts(pNested->blockTableRecordId(), depth + 1);
                continue;
            }*/

            pEnt->close();   // �������ͣ��ߡ�Բ��������
        }
        delete pIter;        // �������� new �����ģ��� delete
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
            else   // \f..; \H..; \W..; \A..; \C..; ���� ';' ��β�ĸ�ʽ�룺���ζ���
            {
                ++p;
                while (*p != _T('\0') && *p != _T(';')) ++p;
            }
        }
        else if (*p == _T('{') || *p == _T('}'))
        {
            // �������ţ�����
        }
        else if (p[0] == _T('%') && p[1] == _T('%') && p[2] != _T('\0'))
        {
            if      (p[2] == _T('d')) out += (ACHAR)0x00B0;  // %%d �� ��
            else if (p[2] == _T('c')) out += (ACHAR)0x00D8;  // %%c �� ?
            else if (p[2] == _T('p')) out += (ACHAR)0x00B1;  // %%p �� ��
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

            //--- 1) �����ı� ---
            AcDbText* pText = AcDbText::cast(pEnt);
			CString s(pText->textString());          // ָ���ʵ�����У��ȿ���
			if (s == _T("1")){
				if (pEnt->upgradeOpen() == Acad::eOk)   // �� �� д
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
	//	// getOrigin() ���ص��ǿ鶨��Ļ��㣨�ֲ����꣩
	//	basePoint = pBTR_mapping->origin(); 
	//	pBTR_mapping->close();
	//}

	// ��������գ����뵽���ڲ� pt λ��
    AcDbBlockReference* pBlkRef = new AcDbBlockReference;
    pBlkRef->setBlockTableRecord(block_mapping);   // ����"��ͼ��"�鶨��
	AcGePoint3d pos(pt);
	pos.x +=2.5;
	pos.y +=2.5;
    pBlkRef->setPosition(pos);                      // pt Ϊ�鶨���ڲ�����ϵ

    AcDbObjectId refId = AcDbObjectId::kNull;
    Acad::ErrorStatus es = pBTR->appendAcDbEntity(refId, pBlkRef);
    if (es != Acad::eOk)
    {
        delete pBlkRef;                            // δ��� �� delete
        CUtils::acutPrintf(_T("[DWG����] ���󣺲�����ͼ�˿����ʧ��(������ %d)��\n"), (int)es);
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
    if (w < 0.0001 || h < 0.0001)          // �տ�/�˻� �� ������
	{
		pBlkRef->close();
        return;
	}

    // 2) Ŀ��ߴ� �� ����ϵ�����ǵȱȣ��ֱ����쵽 23��8��
    double sx = widthBlock  / w;
    double sy = widthHeight / h;
    // ��Ҫ�ȱ����ţ������Σ������ã� double s = min(sx, sy); sx = sy = s;

    // 3) ����㲹��������Χ�ƿ������У�
    //    �����ź�İ�Χ�С����ġ�����ռλ��λ�� pt
   /* AcGePoint3d pos;
    pos.x = pt.x + w * 0.5 * sx;
    pos.y = pt.y + h * 0.5 * sy;
    pos.z = pt.z;*/

	//pBlkRef->setPosition(pos);
    pBlkRef->setScaleFactors(AcGeScale3d(sx, sy, 1.0));
    
	pBlkRef->close();
}