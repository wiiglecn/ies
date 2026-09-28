#include "StdAfx.h"
#include "PreSelectScale.h"
#include "arxHeaders.h"
#include "Utils.h"
//#include "../tools/ToolDraw.h"

// Static helper: process text string, return updated text or original if no match
static CString processScaleTextImpl(const CString& text, double scaleFactor)
{
    // 查找冒号 ':' 以确定是否为比例格式
    int colonPos = text.Find(_T(':'));
    if (colonPos == -1) {
        colonPos = text.Find(_T('：'));
    }
    if (colonPos == -1) {
        return text;
    }

    // 向左查找 '1'，允许中间有空格
    int i = colonPos - 1;
    while (i >= 0 && _istspace(text.GetAt(i))) {
        i--;
    }
    
    if (i < 0 || text.GetAt(i) != _T('1')) {
        return text;
    }

    // 向右查找数字开始，允许中间有空格
    int j = colonPos + 1;
    while (j < text.GetLength() && _istspace(text.GetAt(j))) {
        j++;
    }

    if (j >= text.GetLength() || !_istdigit(text.GetAt(j))) {
        return text;
    }

    // 提取连续的数字
    int numStart = j;
    while (j < text.GetLength() && _istdigit(text.GetAt(j))) {
        j++;
    }

    CString numStr = text.Mid(numStart, j - numStart);
    double denominator = (double)_ttoi(numStr);
    
    if (denominator > 0) {
        // 缩放后比例分母 = 原分母 / 缩放因子
        double newDenominator = denominator / scaleFactor;
        int roundedDenom = (int)(newDenominator + 0.5);
        if (roundedDenom < 1) roundedDenom = 1;

        CString replacement;
        replacement.Format(_T("1:%d"), roundedDenom);

        // 将原字符串中的 "1 : xxx" 替换为新的 "1:yyy"
        CString prefix = text.Left(i);
        CString suffix = text.Mid(j);
        
        return prefix + replacement + suffix;
    }

    return text;
}

// Static helper: detect scale-ratio text like "1:200", "1 : 300" and recalculate
static bool tryUpdateScaleContent(AcDbEntity* pEnt, double scaleFactor)
{
    bool updated = false;

    // ---- AcDbText (单行文本) ----
    AcDbText* pText = AcDbText::cast(pEnt);
    if (pText != NULL) {
        CString oldText = pText->textString();
        CString newText = processScaleTextImpl(oldText, scaleFactor);
        if (newText.Compare(oldText) != 0) {
            pText->setTextString(newText);
            updated = true;
        }
    }

    // ---- AcDbMText (多行文本) ----
    AcDbMText* pMText = AcDbMText::cast(pEnt);
    if (pMText != NULL) {
        CString oldText = pMText->contents();
        CString newText = processScaleTextImpl(oldText, scaleFactor);
        if (newText.Compare(oldText) != 0) {
            pMText->setContents(newText);
            updated = true;
        }
    }

    return updated;
}

// Static helper: get paper dimensions from paper name
static bool getPaperDimensions(const CString& paperName, double& width, double& height)
{
    if (paperName.CompareNoCase(_T("A1")) == 0) {
        width = 841.0; height = 594.0;
    } else if (paperName.CompareNoCase(_T("A2")) == 0) {
        width = 594.0; height = 420.0;
    } else if (paperName.CompareNoCase(_T("A3")) == 0) {
        width = 420.0; height = 297.0;
    } else if (paperName.CompareNoCase(_T("A4")) == 0) {
        width = 297.0; height = 210.0;
    } else {
        if (acedGetReal(_T("\n输入自定义纸张长度(mm): "), &width) != RTNORM) return false;
        if (acedGetReal(_T("\n输入自定义纸张宽度(mm): "), &height) != RTNORM) return false;
    }
    return true;
}

CPreSelectScale::CPreSelectScale() 
    : m_scaleFactor(1.0)
{
    m_minPt = AcGePoint3d::kOrigin;
    m_maxPt = AcGePoint3d::kOrigin;
}

CPreSelectScale::~CPreSelectScale()
{
}

void CPreSelectScale::Execute()
{
    // 1. 选中一个区域
    if (!selectRegion()) {
        return;
    }

    // 2 & 3. 判断并提示纸张选择
    if (!promptPaperSize()) {
        return;
    }

    // 4 & 5. 绘制新对象并删除旧对象
    // cloneAndScaleObjects();
    // deleteOldObjects();

    // 4. 直接在原对象上进行缩放
    bool success = scaleObjectsDirectly();

	if (success){
		acutPrintf(_T("\nPreSelectScale 功能执行完毕。\n"));

		//ToolDraw::getInstance().regen();
	}
}


void CPreSelectScale::ExecuteModelScale()
{
    // ========== 1. 选择当前模型使用的纸张 ==========
    acutPrintf(_T("\n===== 步骤 1: 选择当前模型使用的纸张 =====\n"));
    //acedPostCommandPrompt();
    acedInitGet(0, _T("A1 A2 A3 A4 Custom"));
    TCHAR kw1[20];
    int rc1 = acedGetKword(_T("\n请选择当前模型使用的纸张 [A1/A2/A3/A4/自定义(C)] A3>: "), kw1);
    if (rc1 == RTCAN) {
        acutPrintf(_T("\n用户取消操作。\n"));
        return;
    }

    CString curPaper = (rc1 == RTNONE) ? _T("A3") : CString(kw1);
    double curW = 0.0, curH = 0.0;
    if (!getPaperDimensions(curPaper, curW, curH)) {
        acutPrintf(_T("\n无效的纸张尺寸。\n"));
        return;
    }

    acutPrintf(_T("当前选定源纸张: %s (%.0f x %.0f mm)\n"), curPaper, curW, curH);
    //acedPostCommandPrompt();

    // ========== 2. 选择目标纸张 ==========
    acutPrintf(_T("\n===== 步骤 2: 选择要转换的目标纸张 =====\n"));

    acedInitGet(0, _T("A1 A2 A3 A4 Custom"));
    TCHAR kw2[20];
    int rc2 = acedGetKword(_T("\n请选择目标纸张 [A1/A2/A3/A4/自定义(C)] A2>: "), kw2);
    if (rc2 == RTCAN) {
        acutPrintf(_T("\n用户取消操作。\n"));
        return;
    }

    CString tgtPaper = (rc2 == RTNONE) ? _T("A2") : CString(kw2);
    double tgtW = 0.0, tgtH = 0.0;
    if (!getPaperDimensions(tgtPaper, tgtW, tgtH)) {
        acutPrintf(_T("\n无效的纸张尺寸。\n"));
        return;
    }

    acutPrintf(_T("目标纸张: %s (%.0f x %.0f mm)\n"), tgtPaper, tgtW, tgtH);

    // ========== 3. 计算缩放比例 ==========
    // 基于面积比的平方根，保持长宽比不变
    double curArea = curW * curH;
    double tgtArea = tgtW * tgtH;
    m_scaleFactor = sqrt(tgtArea / curArea);

    acutPrintf(_T("\n缩放比例因子: %.4f (面积比: %.4f)\n"), m_scaleFactor, tgtArea / curArea);

    // ========== 4. 遍历模型空间所有对象进行缩放 ==========

    // 锁定文档
    AcApDocument* pDoc = acDocManager->curDocument();
    Acad::ErrorStatus es = acDocManager->lockDocument(pDoc);
    if (es != Acad::eOk) {
        acutPrintf(_T("锁定文档失败。\n"));
        return;
    }

    // 获取当前空间（模型空间或图纸空间）
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    AcDbBlockTable* pBlkTbl = NULL;
    if (pDb->getBlockTable(pBlkTbl, AcDb::kForRead) != Acad::eOk) {
        acDocManager->unlockDocument(pDoc);
        return;
    }

    // 判断当前是在模型空间还是图纸空间
    TCHAR* curSpaceName = NULL;
    AcDbBlockTableRecord* pSpace = NULL;

    if (pDb->tilemode()) {
        // 模型空间
        pBlkTbl->getAt(ACDB_MODEL_SPACE, pSpace, AcDb::kForRead);
    } else {
        // 图纸空间
        pBlkTbl->getAt(ACDB_PAPER_SPACE, pSpace, AcDb::kForRead);
    }
    pBlkTbl->close();

    if (!pSpace) {
        acDocManager->unlockDocument(pDoc);
        acutPrintf(_T("\n无法获取当前空间。\n"));
        return;
    }

    // 让用户选择缩放基点（默认原点）
    AcGePoint3d basePoint = AcGePoint3d::kOrigin;
    // acedInitGet(0, NULL);
    // int rcPt = acedGetPoint(NULL, _T("\n选择缩放基点 0,0,0>: "), asDblArray(basePoint));
    // // 如果用户直接回车或取消，都使用原点

    // acutPrintf(_T("缩放基点: (%.2f, %.2f, %.2f)\n"), basePoint.x, basePoint.y, basePoint.z);

    // 构建缩放矩阵
    AcGeMatrix3d scaleMat = AcGeMatrix3d::scaling(m_scaleFactor, basePoint);

    // 遍历当前空间所有对象
	AcDbBlockTableRecordIterator* pIter = NULL;
    pSpace->newIterator(pIter);
    
    int scaledCount = 0;
    int textUpdatedCount = 0;

    while (!pIter->done()) {
        AcDbEntity* pEnt = NULL;
        // 使用 getEntity 直接以写模式打开对象
        if (pIter->getEntity(pEnt, AcDb::kForWrite) == Acad::eOk) {
            // 先更新比例文本内容（在缩放前更新）
            if (tryUpdateScaleContent(pEnt, m_scaleFactor)) {
                textUpdatedCount++;
            }

            // 应用缩放矩阵
            pEnt->transformBy(scaleMat);
            pEnt->close();
            scaledCount++;
        }
        pIter->step();
    }

    delete pIter;
    pSpace->close();

    // 解锁文档
    acDocManager->unlockDocument(pDoc);

    // 输出结果
    acutPrintf(_T("\n========================================\n"));
    acutPrintf(_T("转换完成:\n"));
    acutPrintf(_T("  源纸张: %s (%.0f x %.0f mm)\n"), curPaper, curW, curH);
    acutPrintf(_T("  目标纸张: %s (%.0f x %.0f mm)\n"), tgtPaper, tgtW, tgtH);
    acutPrintf(_T("  缩放比例: %.4f\n"), m_scaleFactor);
    acutPrintf(_T("  缩放对象数: %d\n"), scaledCount);
    acutPrintf(_T("  更新比例文本数: %d\n"), textUpdatedCount);
    acutPrintf(_T("========================================\n"));

    //ToolDraw::getInstance().regen();
    
}

bool CPreSelectScale::selectRegion()
{
    ads_name ss;
    // 让用户在图纸中选择对象
    if (acedSSGet(NULL, NULL, NULL, NULL, ss) != RTNORM) {
        acutPrintf(_T("\n未选择任何对象。\n"));
        return false;
    }


    m_selectedObjects.setLogicalLength(0);
    
    long len;
    acedSSLength(ss, &len);
    bool firstEnt = true;

    for (long i = 0; i < len; ++i) {
        ads_name ent;
        if (acedSSName(ss, i, ent) == RTNORM) {
            AcDbObjectId objId;
            if (acdbGetObjectId(objId, ent) == Acad::eOk) {
                m_selectedObjects.append(objId);

                // 获取最大物体的边距 (包围盒)
                AcDbEntity* pEnt = NULL;
                if (acdbOpenAcDbEntity(pEnt, objId, AcDb::kForRead) == Acad::eOk) {
                    AcDbExtents ext;
                    if (pEnt->getGeomExtents(ext) == Acad::eOk) {
                        if (firstEnt) {
                            m_minPt = ext.minPoint();
                            m_maxPt = ext.maxPoint();
                            firstEnt = false;
                        } else {
                            m_minPt.x = __min(m_minPt.x, ext.minPoint().x);
                            m_minPt.y = __min(m_minPt.y, ext.minPoint().y);
                            m_minPt.z = __min(m_minPt.z, ext.minPoint().z);
                            m_maxPt.x = __max(m_maxPt.x, ext.maxPoint().x);
                            m_maxPt.y = __max(m_maxPt.y, ext.maxPoint().y);
                            m_maxPt.z = __max(m_maxPt.z, ext.maxPoint().z);
                        }
                    }
                    pEnt->close();
                }
            }
        }
    }
    acedSSFree(ss);

    if (m_selectedObjects.length() == 0 || firstEnt) {
        acutPrintf(_T("\n无法获取有效对象的范围。\n"));
        return false;
    }

    return true;
}

CString CPreSelectScale::determineSmartPaperSize(double width, double height) const
{
    // 1. 首先检查长宽比是否符合标准 A 系列纸张 (比例约为 1.414)
    double currentRatio = __max(width, height) / __min(width, height);
    if (fabs(currentRatio - 1.414) > 0.05) {
        return _T("Custom"); // 如果长宽比相差较大，直接判定为自定义
    }

    // 2. 标准 A 系列纸张面积 (mm^2) - 假设当前图纸单位为 mm
    double a1_area = 594.0 * 841.0;
    double a2_area = 420.0 * 594.0;
    double a3_area = 297.0 * 420.0;
    double a4_area = 210.0 * 297.0;

    double area = width * height;

    // 简单智能判断：计算面积最接近哪个标准纸张
    double diffA1 = fabs(area - a1_area);
    double diffA2 = fabs(area - a2_area);
    double diffA3 = fabs(area - a3_area);
    double diffA4 = fabs(area - a4_area);

    double minDiff = __min(__min(diffA1, diffA2), __min(diffA3, diffA4));

    // 设定一个容错阈值 (面积相差不超过 5%)
    if (minDiff > (area * 0.05)) {
        return _T("Custom");
    }

    if (minDiff == diffA1) return _T("A1");
    if (minDiff == diffA2) return _T("A2");
    if (minDiff == diffA3) return _T("A3");
    return _T("A4");
}

bool CPreSelectScale::promptPaperSize()
{
    double currentWidth = m_maxPt.x - m_minPt.x;
    double currentHeight = m_maxPt.y - m_minPt.y;

    // 智能判断默认选项
    CString defaultPaper = determineSmartPaperSize(currentWidth, currentHeight);
    
    // 判断当前选择区域的方向：横向或纵向
    bool isLandscape = currentWidth > currentHeight;

    acutPrintf(_T("\n选中源目标的纸张: %s, 方向: %s, 宽度: %.2f, 高度: %.2f\n"), 
        defaultPaper, isLandscape ? _T("横向") : _T("纵向"), currentWidth, currentHeight);
    
    CString sPrompt;
    sPrompt.Format(_T("\n请输入要转换的目标纸张 [A1/A2/A3/A4/自定义(C)] <%s>: "), defaultPaper);

    acedInitGet(0, _T("A1 A2 A3 A4 Custom"));
    TCHAR kw[20];
    int rc = acedGetKword(sPrompt, kw);

    if (rc == RTCAN) {
        return false; // 用户取消
    }

    CString selKw = (rc == RTNONE) ? defaultPaper : CString(kw);
    double targetWidth = 0.0, targetHeight = 0.0;

    // 根据当前区域方向，自动匹配目标标准纸张的宽高
    // 如果当前是横向，目标纸张也按横向尺寸缩放；纵向同理
    if (selKw.CompareNoCase(_T("A1")) == 0) { 
        targetWidth = isLandscape ? 841.0 : 594.0; 
        targetHeight = isLandscape ? 594.0 : 841.0; 
    }
    else if (selKw.CompareNoCase(_T("A2")) == 0) { 
        targetWidth = isLandscape ? 594.0 : 420.0; 
        targetHeight = isLandscape ? 420.0 : 594.0; 
    }
    else if (selKw.CompareNoCase(_T("A3")) == 0) { 
        targetWidth = isLandscape ? 420.0 : 297.0; 
        targetHeight = isLandscape ? 297.0 : 420.0; 
    }
    else if (selKw.CompareNoCase(_T("A4")) == 0) { 
        targetWidth = isLandscape ? 297.0 : 210.0; 
        targetHeight = isLandscape ? 210.0 : 297.0; 
    }
    else {
        // 自定义纸张
        acutPrintf(_T("\n当前源区域为 %s"), isLandscape ? _T("横向") : _T("纵向"));
        if (acedGetReal(_T("\n输入自定义纸张长度: "), &targetWidth) != RTNORM) return false;
        if (acedGetReal(_T("\n输入自定义纸张宽度: "), &targetHeight) != RTNORM) return false;
    }

    if (targetWidth <= 0 || targetHeight <= 0) {
        acutPrintf(_T("\n无效的纸张尺寸。\n"));
        return false;
    }

    // 计算比例：取宽度和高度缩放比例中的较小值，以保持长宽比并确保完全包含在目标区域内
    double scaleX = targetWidth / currentWidth;
    double scaleY = targetHeight / currentHeight;
    m_scaleFactor = __min(scaleX, scaleY);
    
    return true;
}
bool CPreSelectScale::scaleObjectsDirectly()
{
	AcApDocument* pDoc = acDocManager->curDocument();

	Acad::ErrorStatus es = acDocManager->lockDocument(pDoc);
    if (es != Acad::eOk) {
        acutPrintf(_T("Failed to lock the document...\n"));
		return false;
    }

    // 基于包围盒左下角作为缩放基点
    AcGePoint3d basePoint = m_minPt;
    AcGeMatrix3d scaleMat = AcGeMatrix3d::scaling(m_scaleFactor, basePoint);

    // 遍历选中对象直接进行缩放
    for (int i = 0; i < m_selectedObjects.length(); ++i) {
        AcDbEntity* pEnt = NULL;
        // 以写模式打开原对象
        if (acdbOpenAcDbEntity(pEnt, m_selectedObjects[i], AcDb::kForWrite) == Acad::eOk) {
            // 先判断是否为比例文本，若是则更新内容（缩放前更新）
            tryUpdateScaleContent(pEnt, m_scaleFactor);
            // 应用缩放矩阵
            pEnt->transformBy(scaleMat);
            pEnt->close();
        }
    }

	acDocManager->unlockDocument(pDoc);

	return true;
}
void CPreSelectScale::cloneAndScaleObjects()
{
    // 获取当前空间（模型空间或图纸空间）以写入新对象
    AcDbBlockTableRecord* pMs = NULL;
    AcDbBlockTable* pBlkTbl;
    if (acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlkTbl, AcDb::kForRead) != Acad::eOk) return;
    pBlkTbl->getAt(ACDB_MODEL_SPACE, pMs, AcDb::kForWrite); // 默认模型空间
    pBlkTbl->close();

    if (!pMs) return;

    // 基于包围盒左下角作为缩放基点
    AcGePoint3d basePoint = m_minPt;
    AcGeMatrix3d scaleMat = AcGeMatrix3d::scaling(m_scaleFactor, basePoint);

    m_newObjects.setLogicalLength(0);

    // 遍历选中对象进行克隆和缩放
    for (int i = 0; i < m_selectedObjects.length(); ++i) {
        AcDbEntity* pEnt = NULL;
        if (acdbOpenAcDbEntity(pEnt, m_selectedObjects[i], AcDb::kForRead) == Acad::eOk) {
            
            // 克隆对象
            AcDbEntity* pClone = AcDbEntity::cast(pEnt->clone());
            if (pClone) {
                // 应用缩放矩阵
                pClone->transformBy(scaleMat);
                
                // 添加到数据库
                AcDbObjectId newId;
                if (pMs->appendAcDbEntity(newId, pClone) == Acad::eOk) {
                    m_newObjects.append(newId);
                }
                pClone->close();
            }
            pEnt->close();
        }
    }
    pMs->close();
}

void CPreSelectScale::deleteOldObjects()
{
    // 遍历并删除旧对象
    for (int i = 0; i < m_selectedObjects.length(); ++i) {
        AcDbEntity* pEnt = NULL;
        if (acdbOpenAcDbEntity(pEnt, m_selectedObjects[i], AcDb::kForWrite) == Acad::eOk) {
            pEnt->erase();
            pEnt->close();
        }
    }
}