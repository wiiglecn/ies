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


	//画标注表格（含每个钻孔列的孔深/钻孔间距/动探击数）
	void drawTable(AcGePoint3d& basePt, cJSON* gpsObj);
	//画标尺
	SectionRulerInfo drawRuler(AcGePoint3d& basePt,double min_zk,int divide,double elevation_zero);
	//画柱状条
	void drawColumn(AcGePoint3d& basePt,cJSON* gpsObj, const SectionRulerInfo& rule);
	//画固定线条
	void drawLineFix(AcGePoint3d& basePt,cJSON* itemObj,const SectionRulerInfo& rule);
	//画钻孔线条
	void drawLineDrill(AcGePoint3d& basePt,cJSON* itemObj,const SectionRulerInfo& rule);
	//画钻孔号（ZK1/ZK2/ZK3）：每根柱体上方居中标注
	void drawHoleNo(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule);
	//画土层序号+名称：每根柱体右侧、各整层层高中部标注（如 "② 黏土"）
	void drawLayerName(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule);
	//高度尺
	double getRuleHeight(const SectionRulerInfo& rule,double depth);
};
