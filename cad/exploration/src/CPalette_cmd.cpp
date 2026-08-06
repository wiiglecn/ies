#include "StdAfx.h"
#include "CPalette.h"
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

// Remove #include thread> and add a helper thread procedure
struct ThreadProcData {
    AiToolCommandData* params;
};

static DWORD WINAPI DispatchCommandThreadProc(LPVOID lpParam) {
    ThreadProcData* pData = (ThreadProcData*)lpParam;
    ToolGlobal::getInstance().dispatch_command(pData->params);

    SetEvent(pData->params->hEvent);
    delete pData;
    return 0;
}
LRESULT CDataPalette::HandleApiCommand(WPARAM wParam, LPARAM lParam)
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

		case WM_USER_TOOL_CMD:
            ToolGlobal::getInstance().dispatch_command(params);
            return 0;

        case WM_USER_A_REGEN:
            ToolDraw::getInstance().regen();
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
    }
    //acedUpdateDisplay();
    // Signal the waiting thread
    SetEvent(params->hEvent);

    return 0;
}