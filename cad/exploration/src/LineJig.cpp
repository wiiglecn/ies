// coding: gb18030
#include "LineJig.h"
#include "arxHeaders.h"

CLineJig::CLineJig()
    : m_pLine(NULL)
    , m_nStep(0)
    , m_bClosed(false)
{
}

CLineJig::~CLineJig()
{
    if (m_pLine)
    {
        delete m_pLine;
        m_pLine = NULL;
    }
}

Acad::ErrorStatus CLineJig::startJig()
{
    m_nStep = 0;
    m_bClosed = false;
    
    // 初始化第一个点之前，我们需要先获取第一个点
    // 注意：Jig通常用于拖动，第一个点通常通过 acedGetPoint 获取，或者在Jig外部获取
    // 为了完全模拟LINE，我们在Jig内部处理所有逻辑，但第一步比较特殊
    
    Acad::ErrorStatus es;
    
    // Optional: Save current CursorSize to restore later
    struct resbuf rbOldCursorSize;
    bool bCursorSaved = false;
    if (acedGetVar(_T("CURSORSIZE"), &rbOldCursorSize) == RTNORM)
    {
        bCursorSaved = true;
        // Set CursorSize to 100 (full screen crosshairs) or a large value like 50
        // This makes it very obvious the user is in drawing mode
        acedSetVar(_T("CURSORSIZE"), acutBuildList(RTSHORT, 100, RTNONE));
    }
    // 1. 获取第一个点 (不使用Jig拖动，因为还没有基准线)
    ads_point ptRes;
    int rc = acedGetPoint(NULL, _T("\n指定第一点: "), ptRes);
    if (rc != RTNORM)
        return Acad::eUserBreak;
        
    m_ptStart = asPnt3d(ptRes);
    m_ptPrevious = m_ptStart;
    
    // 进入循环，直到用户取消或闭合
    //while (!m_bClosed)
    //{
        // 创建一个新的Line对象用于预览
        if (m_pLine)
            delete m_pLine;
        m_pLine = new AcDbLine(m_ptPrevious, m_ptPrevious); // 初始化为零长度
        
        // 设置关键字
        setKeywordList(_T("Undo Close"));
        //setKeywordHandling(AcEdJig::kKeywordHandled); // 允许处理关键字
        
        // 启动拖动
        DragStatus ds = drag();
        
        if (ds == kNormal)
        {
            // 用户确认了一个点
            // 将上一段线正式加入数据库
            /*es = appendLineToDatabase(m_ptPrevious, m_ptCurrent);
            if (es != Acad::eOk)
                return es;*/
                
            // 更新状态
            m_ptPrevious = m_ptCurrent;
            m_nStep++;
            
            // 清理预览线，准备下一次循环
            delete m_pLine;
            m_pLine = NULL;
        }
        //else if (ds == kKeyword)
        //{
        //    // 处理关键字
        //    const ACHAR* pKeyword = inputKeyword();
        //    if (_tcscmp(pKeyword, _T("Undo")) == 0 || _tcscmp(pKeyword, _T("U")) == 0)
        //    {
        //        // 简单实现：仅提示，实际Undo需要更复杂的实体管理（删除最后添加的线）
        //        // 这里为了代码简洁，仅做演示，实际项目中需维护一个ObjectId数组来删除
        //        acutPrintf(_T("\n[演示] Undo功能需维护实体ID列表进行删除。"));
        //        // 保持上一点不变，继续循环
        //        if (m_pLine) delete m_pLine;
        //        m_pLine = NULL;
        //        continue; 
        //    }
        //    else if (_tcscmp(pKeyword, _T("Close")) == 0 || _tcscmp(pKeyword, _T("C")) == 0)
        //    {
        //        // 闭合：从当前点到起点画一条线
        //        if (m_ptPrevious.distanceTo(m_ptStart) > 1e-6) // 避免重合点
        //        {
        //            es = appendLineToDatabase(m_ptPrevious, m_ptStart);
        //            if (es != Acad::eOk)
        //                return es;
        //        }
        //        m_bClosed = true;
        //        if (m_pLine) delete m_pLine;
        //        m_pLine = NULL;
        //        return Acad::eOk;
        //    }
        //}
        else if (ds == kCancel)
        {
            // 用户按Esc取消
            if (m_pLine)
            {
                delete m_pLine;
                m_pLine = NULL;
            }
            return Acad::eUserBreak;
        }
    //}
    
    return Acad::eOk;
}

AcEdJig::DragStatus CLineJig::sampler()
{
    // 设置提示信息
    setUserInputControls((UserInputControls)(
        AcEdJig::kAccept3dCoordinates | 
        AcEdJig::kNoZeroResponseAccepted | 
        AcEdJig::kNoNegativeResponseAccepted));

    // 获取点
    AcGePoint3d ptTemp;
    DragStatus ds = acquirePoint(ptTemp, m_ptPrevious);
    
    if (ds == kNormal)
    {
        // 如果点发生了变化，才更新，避免不必要的重绘
        if (ptTemp != m_ptCurrent)
        {
            m_ptCurrent = ptTemp;
        }
    }
    
    return ds;
}

Adesk::Boolean CLineJig::update()
{
    // 更新预览线的终点
    if (m_pLine)
    {
        m_pLine->setStartPoint(m_ptPrevious);
        m_pLine->setEndPoint(m_ptCurrent);
    }
    return Adesk::kTrue;
}

AcDbEntity* CLineJig::entity() const
{
    // 返回当前正在拖动的实体指针
    return static_cast<AcDbEntity*>(m_pLine);
}

Acad::ErrorStatus CLineJig::appendLineToDatabase(const AcGePoint3d& ptStart, const AcGePoint3d& ptEnd)
{
    // 创建新的直线实体
    AcDbLine* pNewLine = new AcDbLine(ptStart, ptEnd);
    if (!pNewLine)
        return Acad::eOutOfMemory;

    // 获取当前数据库和模型空间
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb)
    {
        delete pNewLine;
        return Acad::eInvalidInput;
    }

    AcDbBlockTable* pBlockTable = NULL;
    Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
    if (es != Acad::eOk)
    {
        delete pNewLine;
        return es;
    }

    AcDbBlockTableRecord* pBlockTableRecord = NULL;
    es = pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite);
    pBlockTable->close(); // 及时关闭不需要的表

    if (es != Acad::eOk)
    {
        delete pNewLine;
        return es;
    }

    // 将实体添加到模型空间
    AcDbObjectId lineId;
    es = pBlockTableRecord->appendAcDbEntity(lineId, pNewLine);
    
    // 关闭对象
    pBlockTableRecord->close();
    pNewLine->close();

    return es;
}

