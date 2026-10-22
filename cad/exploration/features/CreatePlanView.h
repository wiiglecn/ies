#pragma once
#include "arxHeaders.h"
#include "./CreateBaseView.h"

class CCreatePlanView : public CCreateBaseView
{
public:
	CCreatePlanView(void);

	void Execute();
public:
	//~CCreatePlanView(void);

protected:
	cJSON* s_blockConfigData;
	TCHAR* pszStyleName;


	virtual void ClearExtDbCache();
	void ChangeText(AcDbObjectId blkDefId,AcDbObjectId styleId, ACHAR* holeName,ACHAR* elevation,ACHAR* depth);
	//创建图例
	//创建图例（样例孔的孔号/标高/孔深取第一个孔的值）
    void CreateLegend(std::vector<CString>& arrUsedHoles,

					const CString& fistCadBlockUrl,

                    const CString& blockStyleName,

                    const AcGePoint3d& allMin,

                    const AcGePoint3d& allMax,

                    bool bHasAll,
                    const std::vector<CString>& arrSectionTitles);

	//画剖切位置线（读本项目剖面图参数）；arrSectionTitles 输出本次实际画出的剖面编号，供图例使用
	void DrawSectionLines(bool bSwapXY, std::vector<CString>& arrSectionTitles);
	//剖线端部的剖切符号（外伸粗实线+短剖切线+方向箭头+编号）
	void DrawCutSymbol(const AcGePoint3d& ptEnd, double dx, double dy,
		double dSign, const CString& sTitle);
	//生成后把视图缩放到「底图范围∪钻孔范围」（否则钻孔落在大坐标处，当前视图看不见）
	//baseMin/baseMax = 插入钻孔前的底图包围盒，同时用于诊断钻孔与底图是否同一坐标系
	void ZoomToHoles(const std::vector<AcGePoint3d>& arrHole,
		const AcGePoint3d& baseMin, const AcGePoint3d& baseMax, bool bHasBase);
};
