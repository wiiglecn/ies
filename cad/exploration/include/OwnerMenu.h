#pragma once

class COwnerMenu
{
public:
	COwnerMenu(void);

	static void SLPlanView();
	static void SLSectionView();
	static void SLColumnView();

	static void SLHoleDiameter();
    static void SLMapping();
    static void SLCheck();
    static void SLImportData();
    static void SLBlockLineSelect();
    static void SLPolylineHole();
    static void SLArcPick();
    static void SLSingleScale();
    static void SLAllScale();
	
public:
	~COwnerMenu(void);
};
