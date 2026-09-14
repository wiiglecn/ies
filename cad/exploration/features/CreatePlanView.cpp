#include "StdAfx.h"        // 本项目约定：cpp 必须最先包含（MFC _DEBUG 规避）
#include "arxHeaders.h"
#include <shlwapi.h>
#include "cJSON.h"
#include "utils.h"
#include "CreatePlanView.h"

CCreatePlanView::CCreatePlanView():ToolDrawBase()
{
    
}

CCreatePlanView::~CCreatePlanView()
{
     //ClearExtDbCache();   // 防御性释放
}