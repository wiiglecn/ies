#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include "cJSON.h"
#include "utils.h"
#include "OwnerMenu.h"

#include "../features/CreateColumnView.h"
#include "../features/CreatePlanView.h"
#include "../features/CreateSectionView.h"
#include "../features/BaseDataSet.h"

#include "PolylineMarker.h"
#include "LineHolePlacer.h"
#include "CircleFilter.h"
#include "PreSelectScale.h"

void SetDiameter(CBaseMarker& marker)
{
    cJSON* pRootConfig = CUtils::GetJsonConfig();
    if (pRootConfig) {
        // Check if "profile" attribute exists
        cJSON* pProfile = cJSON_GetObjectItem(pRootConfig, "profile");
        
        if (pProfile) {
            cJSON* hole_diameter = cJSON_GetObjectItem(pProfile, "hole_diameter");
             // Check if the item exists and is a string
            if (hole_diameter && cJSON_IsString(hole_diameter) && (hole_diameter->valuestring != NULL))
            {
                // Convert char* (from cJSON) to CString (handles Unicode/MBCS conversion)
                CString strDiameter(hole_diameter->valuestring);
                
                // Convert CString to float
                float diameter = (float)_tstof(strDiameter);
                
                // Example: Set the diameter to the marker object
                // Assuming BaseMarker has a method like SetDiameter or a public member m_Diameter
                // marker.SetDiameter(diameter); 
                // OR if m_Diameter is public:
                // marker.m_Diameter = diameter;
                
                CUtils::acutPrintf(_T("[INFO] Hole Diameter set to: %.3f\n"), diameter);
                marker.SetDiameter(diameter);
            }
            else
            {
                CUtils::acutPrintf(_T("[WARNING] hole_diameter is not a valid string in JSON.\n"));
            }
        }
    }
}

COwnerMenu::COwnerMenu(void)
{
}

COwnerMenu::~COwnerMenu(void)
{
}

// 加载自定义菜单
void COwnerMenu::SLPlanView()
{
	CCreatePlanView view;
	view.Execute();
}
void COwnerMenu::SLSectionView()
{
	CCreateSectionView view;
	view.Execute();
}
void COwnerMenu::SLColumnView()
{
    CCreateColumnView view;
    view.Execute();
}

void COwnerMenu::SLHoleDiameter()
{
	ACHAR* szCommand=_T("HoleDiameter");
	CBaseDataSet dataSet;
	dataSet.Execute(szCommand);
}

void COwnerMenu::SLMapping()
{
    ACHAR* szCommand=_T("Drafting");
	CBaseDataSet dataSet;
	dataSet.Execute(szCommand);
}

void COwnerMenu::SLCheck()
{
    ACHAR* szCommand=_T("Proofread");
	CBaseDataSet dataSet;
	dataSet.Execute(szCommand);
}

void COwnerMenu::SLImportData()
{
    CUtils::acutPrintf(_T("SLImportData\n"));
}

void COwnerMenu::SLBlockLineSelect()
{
	CPolylineMarker marker;
    SetDiameter(marker);
    marker.ExecuteSelectionAndMark();
}

void COwnerMenu::SLPolylineHole()
{
    CLineHolePlacer marker;
    SetDiameter(marker);
    marker.Execute();
}

void COwnerMenu::SLArcPick()
{
	CCircleFilter circleFilter;
    circleFilter.Execute();

}

void COwnerMenu::SLSingleScale()
{
	CPreSelectScale scaleObj;
	scaleObj.Execute();
}

void COwnerMenu::SLAllScale()
{
    CPreSelectScale scaleObj;
	scaleObj.ExecuteModelScale();
}
