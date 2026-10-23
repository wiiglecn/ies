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
	int m_pageCols;   // 本页列数：Execute 按「本页实际钻孔数」赋值，各绘制函数据此把整幅宽度均分给 n 列


	//画标注表格（含每个钻孔列的孔深/钻孔间距/动探击数）
	void drawTable(AcGePoint3d& basePt, cJSON* gpsObj);
	//画标尺
	SectionRulerInfo drawRuler(AcGePoint3d& basePt,double min_zk,int divide,double elevation_zero);
	//画柱状条
	void drawColumn(AcGePoint3d& basePt,cJSON* gpsObj, const SectionRulerInfo& rule);
	//画固定线条
	void drawLineFix(AcGePoint3d& basePt,cJSON* itemObj,const SectionRulerInfo& rule);
	//画钻孔线条
	void drawLineDrill(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule);
	//画钻孔号（ZK1/ZK2/ZK3）：每根柱体上方居中标注
	void drawHoleNo(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule);
	//画土层序号+名称：每根柱体右侧、各整层层高中部标注（如 "② 黏土"）
	void drawLayerName(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule);
	//画柱内标志：取样符号（原状样●/岩样■）与风化程度符号（中风化2杠/强风化3杠等），图例 2026-10-20
	void drawSampleMark(AcGePoint3d& basePt,cJSON* gpsObj,const SectionRulerInfo& rule);
	//画表头（每页）：工程地质剖面图 + 剖面编号(A------A′) + 下划线 + 比例尺行 + 工程名称(黄)
	void drawHeader(AcGePoint3d& basePt,cJSON* itemObj,cJSON* gpsObj,int pageNo,int pageCount);
	//高度尺
	double getRuleHeight(const SectionRulerInfo& rule,double depth);
};
