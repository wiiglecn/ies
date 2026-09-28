
#include "StdAfx.h"
#include "ApiMessageWnd.h"
#include "PaletteSet.h"
#include "PolylineMarker.h"
#include "LineHolePlacer.h"
#include "Utils.h"
#include "PaletteCleanup.h"
#include "AiApiRequest.h"
#include "declare.h"

#include "AiChat.h"
#include "Logger.h"
#include "CircleFilter.h"
#include "PreSelectScale.h"

#include "../tools/ToolDraw.h"
#include "../tools/ToolLayer.h"
#include "../tools/ToolEntity.h"
#include "../tools/ToolGlobal.h"
#include "../tools/ToolBlock.h"
#include "../tools/ToolAnnotation.h"
#include "../tools/ToolView.h"
#include "../tools/ToolStyle.h"

#include "../features/CreatePlanView.h"
#include "../features/CreateColumnView.h"
#include "../features/CreateSectionView.h"


//CApiMessageWnd::CApiMessageWnd(void)
//{
//}
//
//CApiMessageWnd::~CApiMessageWnd(void)
//{
//}

struct ThreadProcData {
    AiToolCommandData* params;
};
static DWORD WINAPI DispatchCommandThreadProc(LPVOID lpParam) {
    /*ThreadProcData* pData = (ThreadProcData*)lpParam;
    ToolGlobal::getInstance().dispatch_command(pData->params);

    SetEvent(pData->params->hEvent);
    delete pData;*/
    return 0;
}

BEGIN_MESSAGE_MAP(CApiMessageWnd, CWnd)
    ON_MESSAGE(WM_USER_TOOL, HandleApiCommand)
END_MESSAGE_MAP()

BOOL CApiMessageWnd::Create()
{
    CString className = AfxRegisterWndClass(0);

    return CWnd::CreateEx(
        0,
        className,
        _T("SLToolsApiMessageWnd"),
        WS_POPUP,
        0, 0, 0, 0,
        NULL,
        0
    );
}
LRESULT CApiMessageWnd::HandleApiCommand(WPARAM wParam, LPARAM lParam)
{
    AiToolCommandData* params=(AiToolCommandData*) lParam;
    switch(wParam){
        case WM_USER_A_CREATELINE:
            ToolDraw::getInstance().create_line(params);
            break;
        case WM_USER_A_CREATECIRCLE:
            ToolDraw::getInstance().create_circle(params);
            break;
        case WM_USER_A_CREATERECTANGLE:
            ToolDraw::getInstance().create_rectangle(params);
            break;
        case WM_USER_A_CREATERTEXT:
            ToolDraw::getInstance().create_text(params);
            break;
        case WM_USER_A_CREATEPOLYLINE:
            ToolDraw::getInstance().create_polyline(params);
            break;
        case WM_USER_A_CREATEELLIPSE:
            ToolDraw::getInstance().create_ellipse(params);
            break;
        case WM_USER_A_CREATESPLINE:
            ToolDraw::getInstance().create_spline(params);
            break;
        case WM_USER_A_CREATEARC:
            ToolDraw::getInstance().create_arc(params);
            break;
        case WM_USER_A_CREATEMTEXT:
            ToolDraw::getInstance().create_mtext(params);
            break;
        case WM_USER_A_CREATEHATCH:
            ToolDraw::getInstance().create_hatch(params);
            break;

		//annotation
		case WM_USER_A_CREATE_DIM_LINEAR:
            ToolAnnotation::getInstance().create_dimension_linear(params);
            break;
		case WM_USER_A_CREATE_DIM_ALIGNED:
            ToolAnnotation::getInstance().create_dimension_aligned(params);
            break;
		case WM_USER_A_CREATE_DIM_ANGULAR:
            ToolAnnotation::getInstance().create_dimension_angular(params);
            break;
		case WM_USER_A_CREATE_DIM_RADIUS:
            ToolAnnotation::getInstance().create_dimension_radius(params);
            break;
		case WM_USER_A_CREATE_LEADER:
            ToolAnnotation::getInstance().create_leader(params);
            break;

		//entity

        case WM_USER_A_DELETEENTITIES:
            ToolEntity::getInstance().delete_entities(params);
            break;
        case WM_USER_A_LISTENTITIES:
            ToolEntity::getInstance().list_entities(params);
            break;

        case WM_USER_A_ENTITYCOPY:
            ToolEntity::getInstance().entity_copy(params);
            break;
        case WM_USER_A_ENTITYMOVE:
            ToolEntity::getInstance().entity_move(params);
            break;
        case WM_USER_A_ENTITYROTATE:
            ToolEntity::getInstance().entity_rotate(params);
            break;
        case WM_USER_A_ENTITYSCALE:
            ToolEntity::getInstance().entity_scale(params);
            break;
        case WM_USER_A_ENTITYMIRROR:
            ToolEntity::getInstance().entity_mirror(params);
            break;
        case WM_USER_A_ENTITYOFFSET:
            ToolEntity::getInstance().entity_offset(params);
            break;
        case WM_USER_A_ENTITYFILLET:
            ToolEntity::getInstance().entity_fillet(params);
            break;
        case WM_USER_A_ENTITYCHAMFER:
            ToolEntity::getInstance().entity_chamfer(params);
            break;
        case WM_USER_A_ENTITYARRAY:
            ToolEntity::getInstance().entity_array(params);
            break;
        case WM_USER_A_ENTITYGET:
            ToolEntity::getInstance().entity_get(params);
            break;
        case WM_USER_A_ENTITYSETCOLOR:
            ToolEntity::getInstance().entity_set_color(params);
            break;
        case WM_USER_A_ENTITYGETSELECTION:
            ToolEntity::getInstance().get_selected_entity(params);
            break;
        case WM_USER_A_ENTITYSETSELECTION:
            ToolEntity::getInstance().set_selected_entity(params);
            break;

		//layer
		case WM_USER_A_CREATELAYER:
            ToolLayer::getInstance().create_layer(params);
            break;
        case WM_USER_A_LISTLAYERS:
            ToolLayer::getInstance().list_layers(params);
            break;
        case WM_USER_A_QUERYLAYER:
            ToolLayer::getInstance().query_layer(params);
            break;
        case WM_USER_A_GETCURRENTLAYER:
            ToolLayer::getInstance().get_current_layer(params);
            break;
        case WM_USER_A_SETCURRENTLAYER:
            ToolLayer::getInstance().set_current_layer(params);
            break;
        case WM_USER_A_GETCURRENTLAYERCOLOR:
            ToolLayer::getInstance().get_current_layer_color(params);
            break;
        case WM_USER_A_QUERYALLLAYERS:
            ToolLayer::getInstance().query_all_layers(params);
            break;
        case WM_USER_A_LAYERSETPROPERTIES:
            ToolLayer::getInstance().layer_set_properties(params);
            break;
        case WM_USER_A_LAYERFREEZE:
            ToolLayer::getInstance().layer_freeze(params);
            break;
        case WM_USER_A_LAYERTHAW:
            ToolLayer::getInstance().layer_thaw(params);
            break;
        case WM_USER_A_LAYERLOCK:
            ToolLayer::getInstance().layer_lock(params);
            break;
        case WM_USER_A_LAYERUNLOCK:
            ToolLayer::getInstance().layer_unlock(params);
            break;

		//block
		case WM_USER_A_BLOCKLIST:
            ToolBlock::getInstance().block_list(params);
            break;
        case WM_USER_A_BLOCKINSERT:
            ToolBlock::getInstance().block_insert(params);
            break;
        case WM_USER_A_BLOCKINSERTWITHATTRS:
            ToolBlock::getInstance().block_insert_with_attributes(params);
            break;
        case WM_USER_A_BLOCKGETATTRS:
            ToolBlock::getInstance().block_get_attributes(params);
            break;
        case WM_USER_A_BLOCKUPDATEATTRS:
            ToolBlock::getInstance().block_update_attributes(params);
            break;
        case WM_USER_A_BLOCKDEFINE:
            ToolBlock::getInstance().block_define(params);
            break;

		case WM_USER_TOOL_CMD:
            ToolGlobal::getInstance().dispatch_command(params);
            return 0;
        case WM_USER_A_GETDRAWINGUNITS:
            ToolGlobal::getInstance().get_drawing_units(params);
            break;

        case WM_USER_A_REGEN:
            ToolDraw::getInstance().regen();
			params->ret = 0;
            break;

		//view
		case WM_USER_A_ZOOM_EXTENTS:
            ToolView::getInstance().zoom_extents(params);
            break;
		case WM_USER_A_ZOOM_WINDOW:
            ToolView::getInstance().zoom_window(params);
            break;
		case WM_USER_A_GET_SCREENSHOT:
            ToolView::getInstance().get_screenshot(params);
            break;

        //style
        case WM_USER_A_ADD_TEXT_STYLE:
            ToolStyle::getInstance().add_text_style(params);
            break;
        case WM_USER_A_MODIFY_TEXT_STYLE:
            ToolStyle::getInstance().modify_text_style(params);
            break;
        case WM_USER_A_DELETE_TEXT_STYLE:
            ToolStyle::getInstance().delete_text_style(params);
            break;
        case WM_USER_A_ADD_DIM_STYLE:
            ToolStyle::getInstance().add_dim_style(params);
            break;
        case WM_USER_A_MODIFY_DIM_STYLE:
            ToolStyle::getInstance().modify_dim_style(params);
            break;
        case WM_USER_A_DELETE_DIM_STYLE:
            ToolStyle::getInstance().delete_dim_style(params);
            break;
        case WM_USER_A_LIST_TEXT_STYLES:
            ToolStyle::getInstance().list_text_styles(params);
            break;
        case WM_USER_A_GET_TEXT_STYLES:
            ToolStyle::getInstance().get_text_styles(params);
            break;
        case WM_USER_A_LIST_DIM_STYLES:
            ToolStyle::getInstance().list_dim_styles(params);
            break;
        case WM_USER_A_GET_DIM_STYLES:
            ToolStyle::getInstance().get_dim_styles(params);
            break;

		case WM_USER_TOOL_TEST:
            {
                ThreadProcData* pThreadData = new ThreadProcData();
                pThreadData->params = params;
                
                HANDLE hThread = CreateThread(NULL, 0, DispatchCommandThreadProc, pThreadData, 0, NULL);
                if (hThread) {
                    while (true) {
                        DWORD res = MsgWaitForMultipleObjects(1, &hThread, FALSE, 5000, QS_ALLINPUT);
                        if (res == WAIT_OBJECT_0) {
                            break; // Thread finished
                        } else if (res == WAIT_OBJECT_0 + 1) {
                            MSG msg;
                            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                        } else {
                            break; // Timeout or failure
                        }
                    }
                    CloseHandle(hThread);
                }
            }
            break;
		case WM_USER_FEATURE_PLANVIEW:
			{
				//CUtils::FocusAcadDrawing();
				CCreatePlanView view;
				params->ret = view.Execute(params->planViewRequestPath) ? 0 : 1;
				// Remove the one-shot payload after CAD has consumed it.
				if (params->planViewRequestPath[0] != _T('\0'))
					DeleteFile(params->planViewRequestPath);
				//acedPostCommandPrompt(); 
			}
			break;
		case WM_USER_FEATURE_COLUMNVIEW:
			{
				//CUtils::FocusAcadDrawing();
				CCreateColumnView view;
				view.Execute();
				params->ret = 0;
				//acedPostCommandPrompt(); 
			}
			break;
		case WM_USER_FEATURE_SECTIONVIEW:
			{
				//CUtils::FocusAcadDrawing();
				CCreateSectionView view;
				view.Execute();
				params->ret = 0;
				//acedPostCommandPrompt(); 
			}
			break;
    }
    //acedUpdateDisplay();
    // Signal the waiting thread
    SetEvent(params->hEvent);

    return 0;
}