#include "StdAfx.h"
#include "CircleFilter.h"
#include "arxHeaders.h"
#include "Utils.h"

CCircleFilter::CCircleFilter() {}
CCircleFilter::~CCircleFilter() {}

void CCircleFilter::Execute()
{
    // 将焦点切换回 AutoCAD 绘图区域
    CUtils::FocusAcadDrawing();

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) return;

    // 获取当前图层 ID
    AcDbObjectId currentLayerId = pDb->clayer();

    ads_name oldSet;
    // 1. 获取当前的隐式/预选择集合 (PickFirst)
    int rt = acedSSGet(_T("I"), NULL, NULL, NULL, oldSet);

    if (rt == RTNORM) {
        ads_name newSet;
        // 创建一个新的空选择集
        acedSSAdd(NULL, NULL, newSet);


        long len;
        acedSSLength(oldSet, &len);

        bool hasValidCircle = false;

        for (long i = 0; i < len; i++) {
            ads_name ent;
            if (acedSSName(oldSet, i, ent) == RTNORM) {
                AcDbObjectId entId;
                if (acdbGetObjectId(entId, ent) == Acad::eOk) {
                    // 打开实体进行读取
                    AcDbObjectPointer<AcDbEntity> pEnt(entId, AcDb::kForRead);
                    if (pEnt.openStatus() == Acad::eOk) {
                        // 检查是否是 Circle 并且在当前图层
                        AcDbCircle* pCircle = AcDbCircle::cast(pEnt.object());
                        if (pCircle != NULL && pEnt->layerId() == currentLayerId) {
                            acedSSAdd(ent, newSet, newSet);
                            hasValidCircle = true;
                        }
                    }
                }
            }
        }

        // 2. 清除当前所有的选择状态
        acedSSSetFirst(NULL, NULL);

        // 3. 如果找到了符合条件的 Circle，则将其设为新的选择集
        if (hasValidCircle) {
            acedSSSetFirst(newSet, NULL);
        }

        // 释放选择集内存
        acedSSFree(oldSet);
        acedSSFree(newSet);

        // 刷新显示
        acedUpdateDisplay();
    }
}