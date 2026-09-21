#pragma once
#include "arxHeaders.h"
#include "./CreateBaseView.h"

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


	//画标注表格
	void drawTable(AcGePoint3d& basePt);
	//画标尺
	void drawRuler(AcGePoint3d& basePt,double min_zk,int divide,double elevation_zero);
};
