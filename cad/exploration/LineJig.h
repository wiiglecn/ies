// coding: gb18030
#pragma once
#include "StdAfx.h"
#include "arxHeaders.h"



// 自定义Jig类，用于模拟LINE命令的交互和预览
class CLineJig : public AcEdJig
{
public:
    CLineJig();
    ~CLineJig();

    // 启动Jig的主函数
    Acad::ErrorStatus startJig();

    // --- Add these public getter methods ---
    AcGePoint3d getStartPoint() const { return m_ptStart; }
    AcGePoint3d getEndPoint() const { return m_ptCurrent; }
protected:
    // AcEdJig 必须实现的三个虚函数
    virtual DragStatus sampler();
    virtual Adesk::Boolean update();
    virtual AcDbEntity* entity() const;

private:
    // 内部辅助函数
    Acad::ErrorStatus appendLineToDatabase(const AcGePoint3d& ptStart, const AcGePoint3d& ptEnd);
    
    // 成员变量
    AcDbLine*       m_pLine;        // 当前正在拖动的预览线实体
    AcGePoint3d     m_ptStart;      // 整个命令的起始点（用于闭合）
    AcGePoint3d     m_ptPrevious;   // 上一个确定的点
    AcGePoint3d     m_ptCurrent;    // 当前鼠标位置或新指定的点
    int             m_nStep;        // 当前步骤：0=等待第一点, 1=等待下一点
    bool            m_bClosed;      // 是否已闭合
};

