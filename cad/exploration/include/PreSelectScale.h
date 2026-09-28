#pragma once
#include "acdb.h"
#include "dbents.h"

class CPreSelectScale {
public:
    CPreSelectScale();
    ~CPreSelectScale();

    // 主入口函数：执行完整的预选缩放流程
    void Execute();
    void ExecuteModelScale();   // 新增：整图纸张转换

private:
    // 1. 提示用户选择区域，并计算边界
    bool selectRegion();

    // 2. 智能判断并提示用户选择纸张
    bool promptPaperSize();

    // 3. 克隆、缩放并绘制新对象
    void cloneAndScaleObjects();

    // 4. 删除旧的对象
    void deleteOldObjects();

    // 5. 直接在原对象上进行缩放
    bool scaleObjectsDirectly();

    // 辅助函数：计算最匹配的默认纸张尺寸
    CString determineSmartPaperSize(double width, double height) const;

    AcDbObjectIdArray m_selectedObjects;
    AcDbObjectIdArray m_newObjects;
    AcGePoint3d m_minPt;
    AcGePoint3d m_maxPt;
    double m_scaleFactor;
};