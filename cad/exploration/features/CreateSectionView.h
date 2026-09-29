#pragma once
#include "arxHeaders.h"
#include "./CreateBaseView.h"

struct SectionRulerInfo;
class PatHatchLoader;
class CCreateSectionView  : public CCreateBaseView
{
public:
	CCreateSectionView(void);
public:
	//~CCreateSectionView(void);


	void Execute();
	virtual void ClearExtDbCache();

	void showAllLayer();
protected:
	cJSON* s_ConfigData;
	PatHatchLoader* patLoader;


	//画标注表格
	void drawTable(AcGePoint3d& basePt);
	//画标尺
	SectionRulerInfo drawRuler(AcGePoint3d& basePt,double min_zk,int divide,double elevation_zero);
	//画柱状条
	void drawColumn(AcGePoint3d& basePt,cJSON* gpsObj, const SectionRulerInfo& rule);
	//画固定线条
	void drawLineFix(AcGePoint3d& basePt,cJSON* itemObj,const SectionRulerInfo& rule);
	//画钻孔线条
	void drawLineDrill(AcGePoint3d& basePt,cJSON* itemObj,const SectionRulerInfo& rule);
	//高度尺
	double getRuleHeight(const SectionRulerInfo& rule,double depth);
};
