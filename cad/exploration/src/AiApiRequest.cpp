// coding: gb18030
#include "stdafx.h"
#include "AiApiRequest.h"
#include "AiApiServer.h"
#include "cJSON.h"
#include "Utils.h"
#include "../tools/ToolGlobal.h"
#include <afx.h>
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

// VS2005 兼容
#define snprintf _snprintf

// 用特殊值标记无效坐标（不依赖系统 NAN/isnan，避免 VS2005 兼容问题）
static const double kInvalidCoord = -999999.0;
#define IsInvalid(v) ((v) <= -999999.0)
#define kNAN kInvalidCoord

struct ToolRouteInfo {
    const char* uriPath;
    const char* uriSuffix;
    UINT commandID;
};

static const ToolRouteInfo toolRoutes[] = {
    { "/ai/tool/create_line", "create_line", WM_USER_A_CREATELINE },
    { "/ai/tool/create_circle", "create_circle", WM_USER_A_CREATECIRCLE },
    { "/ai/tool/create_rectangle", "create_rectangle", WM_USER_A_CREATERECTANGLE },
    { "/ai/tool/create_text", "create_text", WM_USER_A_CREATERTEXT },
    { "/ai/tool/create_polyline", "create_polyline", WM_USER_A_CREATEPOLYLINE },
    { "/ai/tool/create_ellipse", "create_ellipse", WM_USER_A_CREATEELLIPSE },
    { "/ai/tool/create_spline", "create_spline", WM_USER_A_CREATESPLINE },
    { "/ai/tool/create_arc", "create_arc", WM_USER_A_CREATEARC },
    { "/ai/tool/create_mtext", "create_mtext", WM_USER_A_CREATEMTEXT },
    { "/ai/tool/create_hatch", "create_hatch", WM_USER_A_CREATEHATCH },

    { "/ai/tool/create_dimension_linear", "create_dimension_linear", WM_USER_A_CREATE_DIM_LINEAR },
    { "/ai/tool/create_dimension_aligned", "create_dimension_aligned", WM_USER_A_CREATE_DIM_ALIGNED },
    { "/ai/tool/create_dimension_angular", "create_dimension_angular", WM_USER_A_CREATE_DIM_ANGULAR },
    { "/ai/tool/create_dimension_radius", "create_dimension_radius", WM_USER_A_CREATE_DIM_RADIUS },
    { "/ai/tool/create_leader", "create_leader", WM_USER_A_CREATE_LEADER },

    { "/ai/tool/delete_entities", "delete_entities", WM_USER_A_DELETEENTITIES },
    { "/ai/tool/list_entities", "list_entities", WM_USER_A_LISTENTITIES },

    { "/ai/tool/entity_copy", "entity_copy", WM_USER_A_ENTITYCOPY },
    { "/ai/tool/entity_move", "entity_move", WM_USER_A_ENTITYMOVE },
    { "/ai/tool/entity_rotate", "entity_rotate", WM_USER_A_ENTITYROTATE },
    { "/ai/tool/entity_scale", "entity_scale", WM_USER_A_ENTITYSCALE },
    { "/ai/tool/entity_mirror", "entity_mirror", WM_USER_A_ENTITYMIRROR },
    { "/ai/tool/entity_offset", "entity_offset", WM_USER_A_ENTITYOFFSET },
    { "/ai/tool/entity_fillet", "entity_fillet", WM_USER_A_ENTITYFILLET },
    { "/ai/tool/entity_chamfer", "entity_chamfer", WM_USER_A_ENTITYCHAMFER },
    { "/ai/tool/entity_array", "entity_array", WM_USER_A_ENTITYARRAY },
    { "/ai/tool/entity_get", "entity_get", WM_USER_A_ENTITYGET },
    { "/ai/tool/entity_set_color", "entity_set_color", WM_USER_A_ENTITYSETCOLOR },
    { "/ai/tool/get_selected_entity", "get_selected_entity", WM_USER_A_ENTITYGETSELECTION },
    { "/ai/tool/set_selected_entity", "set_selected_entity", WM_USER_A_ENTITYSETSELECTION },

    { "/ai/tool/create_layer", "create_layer", WM_USER_A_CREATELAYER },
    { "/ai/tool/list_layers", "list_layers", WM_USER_A_LISTLAYERS },
    { "/ai/tool/query_layer", "query_layer", WM_USER_A_QUERYLAYER },
    { "/ai/tool/get_current_layer", "get_current_layer", WM_USER_A_GETCURRENTLAYER },
    { "/ai/tool/set_current_layer", "set_current_layer", WM_USER_A_SETCURRENTLAYER },
    { "/ai/tool/get_current_layer_color", "get_current_layer_color", WM_USER_A_GETCURRENTLAYERCOLOR },
    { "/ai/tool/query_all_layers", "query_all_layers", WM_USER_A_QUERYALLLAYERS },
    { "/ai/tool/layer_set_properties", "layer_set_properties", WM_USER_A_LAYERSETPROPERTIES },
    { "/ai/tool/layer_freeze", "layer_freeze", WM_USER_A_LAYERFREEZE },
    { "/ai/tool/layer_thaw", "layer_thaw", WM_USER_A_LAYERTHAW },
    { "/ai/tool/layer_lock", "layer_lock", WM_USER_A_LAYERLOCK },
    { "/ai/tool/layer_unlock", "layer_unlock", WM_USER_A_LAYERUNLOCK },
    { "/ai/tool/regen", "regen", WM_USER_A_REGEN },

    { "/ai/tool/block_list", "block_list", WM_USER_A_BLOCKLIST },
    { "/ai/tool/block_insert", "block_insert", WM_USER_A_BLOCKINSERT },
    { "/ai/tool/block_insert_with_attributes", "block_insert_with_attributes", WM_USER_A_BLOCKINSERTWITHATTRS },
    { "/ai/tool/block_get_attributes", "block_get_attributes", WM_USER_A_BLOCKGETATTRS },
    { "/ai/tool/block_update_attributes", "block_update_attributes", WM_USER_A_BLOCKUPDATEATTRS },
    { "/ai/tool/block_define", "block_define", WM_USER_A_BLOCKDEFINE },

    { "/ai/tool/zoom_extents", "zoom_extents", WM_USER_A_ZOOM_EXTENTS },
    { "/ai/tool/zoom_window", "zoom_window", WM_USER_A_ZOOM_WINDOW },
    { "/ai/tool/get_screenshot", "get_screenshot", WM_USER_A_GET_SCREENSHOT },

    { "/ai/tool/add_text_style", "add_text_style", WM_USER_A_ADD_TEXT_STYLE },
    { "/ai/tool/modify_text_style", "modify_text_style", WM_USER_A_MODIFY_TEXT_STYLE },
    { "/ai/tool/delete_text_style", "delete_text_style", WM_USER_A_DELETE_TEXT_STYLE },
    { "/ai/tool/add_dim_style", "add_dim_style", WM_USER_A_ADD_DIM_STYLE },
    { "/ai/tool/modify_dim_style", "modify_dim_style", WM_USER_A_MODIFY_DIM_STYLE },
    { "/ai/tool/delete_dim_style", "delete_dim_style", WM_USER_A_DELETE_DIM_STYLE },
    { "/ai/tool/list_text_styles", "list_text_styles", WM_USER_A_LIST_TEXT_STYLES },
    { "/ai/tool/get_text_styles", "get_text_styles", WM_USER_A_GET_TEXT_STYLES },
    { "/ai/tool/list_dim_styles", "list_dim_styles", WM_USER_A_LIST_DIM_STYLES },
    { "/ai/tool/get_dim_styles", "get_dim_styles", WM_USER_A_GET_DIM_STYLES },

	{ "/ai/tool/dispatch_command", "dispatch_command", WM_USER_TOOL_CMD },
    { "/ai/tool/get_drawing_units", "get_drawing_units", WM_USER_A_GETDRAWINGUNITS },

	{ "/ai/tool/test", "test", WM_USER_TOOL_TEST }

	// 注：/ai/feature/create_*_view 三个出图路由已单独注册到 HandleAiFeatureView，
	// 不在此数组中（需要支持 body 里带 borehole_data，不走通用 HandleAiTool）。
};	
bool EndsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

void HandleAiChat(struct mg_connection* nc, 
				  struct mg_http_message* hm,
                  const std::map<std::string, std::string>& params,
                  const std::string& body,
				  void* user_data)
{
    
    
    std::string response = "{\"status\":\"success\",\"message\":\"AI reply placeholder\"}";
    AiApiServer::SendJsonResponse(nc, 200, response);
}


void HandleAiGenerate(struct mg_connection* nc,
					  struct mg_http_message* hm,
                      const std::map<std::string, std::string>& params,
                      const std::string& body,
					  void* user_data)
{
   
    std::string response = "{\"text\":\"Generated content...\"}";
    AiApiServer::SendJsonResponse(nc, 200, response);
}

void HandleAiCommand(struct mg_connection* nc,
					 struct mg_http_message* hm,
                      const std::map<std::string, std::string>& params,
                      const std::string& body,
					  void* user_data)
{
    std::string response;
    
    // 1. Parse JSON body
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) {
        response = "{\"status\":\"error\",\"message\":\"Invalid JSON format\"}";
        AiApiServer::SendJsonResponse(nc, 400, response);
        return;
    }

    // 2. Extract 'command' and 'params'
    cJSON* cmdItem = cJSON_GetObjectItem(root, "command");
    cJSON* paramsItem = cJSON_GetObjectItem(root, "params");

    if (!cmdItem || !cJSON_IsString(cmdItem)) {
        cJSON_Delete(root);
        response = "{\"status\":\"error\",\"message\":\"Missing or invalid 'command' field\"}";
        AiApiServer::SendJsonResponse(nc, 400, response);
        return;
    }

    std::string commandStr = cmdItem->valuestring;
    std::string paramsStr = "";
    
    if (paramsItem && cJSON_IsString(paramsItem)) {
        paramsStr = paramsItem->valuestring;
    } else if (paramsItem && cJSON_IsArray(paramsItem)) {
        // If params is an array, you might want to serialize it back to string 
        // or handle it specifically. For now, let's assume it's a string argument list.
        char* printed = cJSON_PrintUnformatted(paramsItem);
        if (printed) {
            paramsStr = printed;
            free(printed);
        }
    }

    cJSON_Delete(root);

    // 3. Construct the full command string
    // Note: AutoCAD commands often require spaces or specific formatting.
    // Example: "LINE 0,0 10,10 "
    std::string fullCommand = commandStr + " " + paramsStr + "\n\003"; 

    // 4. Execute AutoCAD Command
    // IMPORTANT: The method to execute commands depends on your specific CAD environment.
    // Common methods include acedCommand, acDocManager->sendStringToExecute, etc.
    // Below is a generic example using acedCommand (requires <aced.h> or similar headers).
    
    try {
        // Convert std::string to TCHAR* or wchar_t* depending on your build configuration (Unicode/Multi-byte)
        #ifdef _UNICODE
            std::wstring wCmd(fullCommand.begin(), fullCommand.end());
            // Example using acedCommand (RTSTR expects wide char in Unicode builds)
            // acedCommand(RTSTR, wCmd.c_str(), RTNONE);
            
            // Alternative: sendStringToExecute is often safer for asynchronous execution
            CUtils::sendStringToExecute(acDocManager->curDocument(), wCmd.c_str());
        #else
            // Example using acedCommand (Multi-byte)
            // acedCommand(RTSTR, fullCommand.c_str(), RTNONE);
            
            // Alternative: sendStringToExecute
            // acDocManager->sendStringToExecute(acDocManager->curDocument(), fullCommand.c_str(), false, false);
        #endif
        
        // Since we don't know the exact CAD API header included in your project,
        // I will use a placeholder function call. You should replace this with 
        // the actual API call used in your CAD plugin environment.
        
        // Placeholder for demonstration:
        // ExecuteAcadCommand(fullCommand); 

        response = "{\"status\":\"success\",\"message\":\"Command executed\"}";
        AiApiServer::SendJsonResponse(nc, 200, response);

    } catch (...) {
        response = "{\"status\":\"error\",\"message\":\"Failed to execute command\"}";
        AiApiServer::SendJsonResponse(nc, 500, response);
    }
}
// Helper to check if string ends with suffix
// Compatible with VC8

void HandleAiTool(struct mg_connection* nc,
				  struct mg_http_message* hm,
                  const std::map<std::string, std::string>& params,
                  const std::string& body,
				  void* user_data)
{

	std::string response;
	// 1. Parse JSON body
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) {
        response = "{\"status\":\"error\",\"message\":\"Invalid JSON format\"}";
        AiApiServer::SendJsonResponse(nc, 400, response);
        return;
    }
	std::string uri(hm->uri.buf, hm->uri.len);
    AiApiRequest* apiRequest = (AiApiRequest*)user_data;
    CWnd* pAcadWnd = apiRequest->getAcadWnd();

    if (!pAcadWnd) {
        cJSON_Delete(root);
        response = "{\"status\":\"error\",\"message\":\"AutoCAD Window not available\"}";
        AiApiServer::SendJsonResponse(nc, 500, response);
        return;
    }

    // 2. Create a manual-reset event, initially non-signaled
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hEvent == NULL) {
        cJSON_Delete(root);
        response = "{\"status\":\"error\",\"message\":\"Failed to create sync event\"}";
        AiApiServer::SendJsonResponse(nc, 500, response);
        return;
    }

    // 3. Package data
    AiToolCommandData* pData = new AiToolCommandData();
    pData->jsonRoot = root;
    pData->hEvent = hEvent;

    // 4. Determine the specific command based on URI by iterating through the array
    UINT nCommandID = 0;
    size_t routeCount = sizeof(toolRoutes) / sizeof(toolRoutes[0]);
    for (size_t i = 0; i < routeCount; ++i) {
        std::string slashSuffix = std::string("/") + toolRoutes[i].uriSuffix;
        if (uri == toolRoutes[i].uriPath || EndsWith(uri, slashSuffix) || EndsWith(uri, toolRoutes[i].uriSuffix)) {
            nCommandID = toolRoutes[i].commandID;
            break;
        }
    }

    if (nCommandID == 0) {
        // Cleanup for unknown command
        delete pData;
        CloseHandle(hEvent);
        response = "{\"status\":\"error\",\"message\":\"Unknown tool endpoint: " + uri + "\"}";
        AiApiServer::SendJsonResponse(nc, 404, response);
        return;
    }

    // 5. Post Message to AutoCAD Main Thread
    // WPARAM: Command ID, LPARAM: Pointer to our data structure
	DWORD waitResult;
	/*if (EndsWith(uri, "/dispatch_command")){
		ToolGlobal::getInstance().dispatch_command(pData);
		waitResult = WAIT_OBJECT_0;
	}else{*/
		pAcadWnd->PostMessage(WM_USER_TOOL, nCommandID, (LPARAM)pData);
		waitResult = WaitForSingleObject(hEvent, 60*1000); 
	//}

     // 6. Wait for the event to be signaled by the main thread
    // Timeout after 10 seconds to prevent indefinite hanging
    //DWORD waitResult = WaitForSingleObject(hEvent, 10000); 

    // 7. Construct Response using cJSON
    if (waitResult == WAIT_OBJECT_0) {
        // Create the root response object
        cJSON* responseRoot = cJSON_CreateObject();
        
        if (pData->ret == 0) {
            // Add status
            cJSON_AddStringToObject(responseRoot, "status", "success");
            
            if (pData->resultJson){
                cJSON_AddItemToObject(responseRoot, "data", pData->resultJson);
                pData->resultJson = NULL; 
            }
            //cJSON_AddStringToObject(responseRoot, "message", "Tool executed successfully");
            
        } else {
            // No specific data returned
            cJSON_AddStringToObject(responseRoot, "status", "error");
            cJSON_AddStringToObject(responseRoot, "message", "Failed to execute");
        }

        // Serialize the JSON object to a string
        char* printed = cJSON_PrintUnformatted(responseRoot);
        if (printed) {
            response = printed;
            free(printed);
        } else {
            response = "{\"status\":\"error\",\"message\":\"Failed to serialize response\"}";
        }
        
        // Send the response
        AiApiServer::SendJsonResponse(nc, 200, response);

        
        // Clean up the response root object
        cJSON_Delete(responseRoot);
    } 
    else {
        // Timeout or Error
        response = "{\"status\":\"error\",\"message\":\"Execution timeout\"}";
        AiApiServer::SendJsonResponse(nc, 500, response);
    }

    // 8. Cleanup
    CloseHandle(hEvent);
    
    // Clean up the result JSON if it was NOT attached to the response (e.g. in error cases or if logic changed)
    if (pData->resultJson) {
        cJSON_Delete(pData->resultJson);
    }
    
    cJSON_Delete(root); // Clean up input JSON
    delete pData;       // Clean up the struct
}
// ---------- /api/draw: 一次调用出图（一体化，替代外部 python 网关） ----------
// 把 cJSON 对象序列化成 UTF-8 字符串并写到 project/ 下的指定文件。
// 返回 true=写成功。
static CString g_lastFailedPath;

static bool _WriteJsonToProjectFile(const wchar_t* fileNameW, const std::string& contentUtf8)
{
    CString arxDir = CUtils::GetArxFolder();
    arxDir.Replace(_T("\\"), _T("/"));
    if (arxDir.Right(1) != _T("/")) arxDir += _T("/");
    CString path = arxDir + CString(fileNameW);
    g_lastFailedPath = path;

    CStdioFile f;
    if (!f.Open(path, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)) {
        return false;
    }
    if (!contentUtf8.empty()) {
        f.Write((const void*)contentUtf8.data(), (UINT)contentUtf8.size());
    }
    f.Close();
    return true;
}

// 追加写日志文件（UTF-8），自动加时间戳
static void _LogToFile(const wchar_t* fileNameW, const std::string& contentUtf8)
{
    CString arxDir = CUtils::GetArxFolder();
    arxDir.Replace(_T("\\"), _T("/"));
    if (arxDir.Right(1) != _T("/")) arxDir += _T("/");
    CString path = arxDir + CString(fileNameW);

    CStdioFile f;
    BOOL ok = f.Open(path, CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite | CFile::typeBinary);
    if (!ok) {
        acutPrintf(_T("[LOG DEBUG] failed to open log file: %s\n"), (LPCTSTR)path);
        return;
    }
    f.SeekToEnd();

    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeBuf[64];
    snprintf(timeBuf, sizeof(timeBuf), "[%04d-%02d-%02d %02d:%02d:%02d] ",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    f.Write(timeBuf, (UINT)strlen(timeBuf));
    f.Write(contentUtf8.data(), (UINT)contentUtf8.size());
    f.Write("\r\n", 2);
    f.Close();

    acutPrintf(_T("[LOG DEBUG] wrote %d bytes to %s\n"), (int)contentUtf8.size(), (LPCTSTR)path);
}

// 同步执行一个 tool（PostMessage + WaitForSingleObject），模仿 HandleAiTool 内部逻辑。
// 返回 0=成功，非 0=失败/超时。
static int _ExecToolSync(CWnd* pAcadWnd, UINT cmdId, DWORD timeoutMs = 120000)
{
    cJSON* root = cJSON_CreateObject();
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hEvent == NULL) {
        cJSON_Delete(root);
        return 1;
    }
    AiToolCommandData* pData = new AiToolCommandData();
    pData->jsonRoot = root;
    pData->hEvent = hEvent;
    pData->ret = 1;
    pData->resultJson = NULL;

    pAcadWnd->PostMessage(WM_USER_TOOL, cmdId, (LPARAM)pData);
    DWORD wr = WaitForSingleObject(hEvent, timeoutMs);

    int ret = (wr == WAIT_OBJECT_0) ? pData->ret : 2;

    if (pData->resultJson) cJSON_Delete(pData->resultJson);
    CloseHandle(hEvent);
    cJSON_Delete(root);
    delete pData;
    return ret;
}

// --- auto-generated english->chinese key renamer ---
static void RenameChildKey(cJSON* parent, const char* oldKey, const char* newKeyUtf8) {
    if (!parent || !oldKey || !newKeyUtf8) return;
    cJSON* c = cJSON_GetObjectItem(parent, oldKey);
    if (!c) return;
    if (c->string) { free(c->string); c->string = NULL; }
    size_t n = strlen(newKeyUtf8);
    c->string = (char*)malloc(n+1);
    memcpy(c->string, newKeyUtf8, n);
    c->string[n] = 0;
}

static void ConvertBoreholeDataEnToCn(cJSON* root) {
    if (!root || !cJSON_IsObject(root)) return;
    cJSON* hole = NULL;
    cJSON_ArrayForEach(hole, root) {
        if (!cJSON_IsObject(hole)) continue;
    RenameChildKey(hole, "hole_no", "\xE9\x92\xBB\xE5\xAD\x94\xE7\xBC\x96\xE5\x8F\xB7");
    RenameChildKey(hole, "x", "\x58\xE5\x9D\x90\xE6\xA0\x87\x28\x6D\x29");
    RenameChildKey(hole, "y", "\x59\xE5\x9D\x90\xE6\xA0\x87\x28\x6D\x29");
    RenameChildKey(hole, "elevation", "\xE5\xAD\x94\xE5\x8F\xA3\xE9\xAB\x98\xE7\xA8\x8B\x28\x6D\x29");
    RenameChildKey(hole, "depth", "\xE7\xBB\x88\xE5\xAD\x94\xE6\xB7\xB1\xE5\xBA\xA6\x28\x6D\x29");
    RenameChildKey(hole, "hole_type", "\xE9\x92\xBB\xE5\xAD\x94\xE7\xB1\xBB\xE5\x9E\x8B");
    RenameChildKey(hole, "project_name", "\xE5\xB7\xA5\xE7\xA8\x8B\xE5\x90\x8D\xE7\xA7\xB0");
    RenameChildKey(hole, "company", "\xE5\x85\xAC\xE5\x8F\xB8\xE5\x90\x8D\xE7\xA7\xB0");
    RenameChildKey(hole, "site", "\xE5\xB7\xA5\xE7\xA8\x8B\xE5\x9C\xB0\xE7\x82\xB9");
    RenameChildKey(hole, "date_start", "\xE5\xBC\x80\xE5\xAD\x94\xE6\x97\xA5\xE6\x9C\x9F");
    RenameChildKey(hole, "date_end", "\xE7\xBB\x88\xE5\xAD\x94\xE6\x97\xA5\xE6\x9C\x9F");
    RenameChildKey(hole, "page_current", "\xE5\xBD\x93\xE5\x89\x8D\xE9\xA1\xB5");
    RenameChildKey(hole, "page_total", "\xE6\x80\xBB\xE9\xA1\xB5\xE7\xA0\x81");
    RenameChildKey(hole, "water_level_initial", "\xE5\x88\x9D\xE8\xA7\x81\xE6\xB0\xB4\xE4\xBD\x8D\x28\x6D\x29");
    RenameChildKey(hole, "water_level_stable", "\xE7\xA8\xB3\xE5\xAE\x9A\xE6\xB0\xB4\xE4\xBD\x8D\x28\x6D\x29");
    RenameChildKey(hole, "manager", "\xE9\xA1\xB9\xE7\x9B\xAE\xE8\xB4\x9F\xE8\xB4\xA3\xE4\xBA\xBA");
    RenameChildKey(hole, "driller", "\xE9\x92\xBB\xE6\x8E\xA2\xE6\x9C\xBA\xE9\x95\xBF");
    RenameChildKey(hole, "recorder", "\xE7\xBC\x96\xE5\xBD\x95\xE5\x91\x98");
        cJSON* layers = cJSON_GetObjectItem(hole, "layers");
        if (layers && cJSON_IsArray(layers)) {
            cJSON* layer = NULL;
            cJSON_ArrayForEach(layer, layers) {
                if (!cJSON_IsObject(layer)) continue;
    RenameChildKey(layer, "layer_id", "\xE5\x9C\xB0\xE5\xB1\x82\xE7\xBC\x96\xE5\x8F\xB7");
    RenameChildKey(layer, "from", "\xE8\xBF\x9B\xE5\xBA\x95\x87\xAA\x28\x6D\x29");
    RenameChildKey(layer, "to", "\xE5\x88\xB0\xE5\xBA\x95\xE6\xB7\xB1\xE5\xBA\xA6\x28\x6D\x29");
    RenameChildKey(layer, "name", "\xE5\x9C\xB0\xE5\xB1\x82\xE5\x90\x8D");
    RenameChildKey(layer, "color", "\xE9\xA2\x9C\xE8\x89\xB2");
    RenameChildKey(layer, "state", "\xE7\x8A\xB6\xE6\x80\x81");
    RenameChildKey(layer, "density", "\xE5\xAF\x86\xE5\xAE\x9E\xE5\xBA\xA6");
    RenameChildKey(layer, "moisture", "\xE6\xB9\xBF\xE5\xBA\xA6");
    RenameChildKey(layer, "description", "\xE6\x8F\x8F\xE8\xBF\xB0\xE5\x86\x85\xE5\xAE\xB9");
                cJSON* sample = cJSON_GetObjectItem(layer, "sample");
                if (sample && cJSON_IsObject(sample)) {
    RenameChildKey(sample, "no", "\xE7\xBC\x96\xE5\x8F\xB7");
    RenameChildKey(sample, "depth", "\xE5\x8F\x96\xE6\xA0\xB7\xE6\xB7\xB1\xE5\xBA\xA6");
                    RenameChildKey(layer, "sample", "\xE5\x8F\x96\xE5\xB2\xA9\x28\xE5\x9C\x9F\xE3\x80\x81\xE6\xB0\xB4\x29\xE6\xA0\xB7");
                }
                cJSON* test = cJSON_GetObjectItem(layer, "test");
                if (test && cJSON_IsObject(test)) {
    RenameChildKey(test, "type", "\xE7\xB1\xBB\xE5\x9E\x8B");
    RenameChildKey(test, "from", "\xE8\xB5\xB7\x28\x6D\x29");
    RenameChildKey(test, "to", "\xE6\xAD\xA2\x28\x6D\x29");
    RenameChildKey(test, "rod_length", "\xE6\x9D\x86\xE9\x95\xBF\x28\x6D\x29");
    RenameChildKey(test, "hits", "\xE9\x94\xA4\xE5\x87\xBB\xE6\x95\xB0\x28\xE5\x87\xBB\x29");
                    RenameChildKey(layer, "test", "\xE5\x8E\x9F\xE4\xBD\x8D\xE6\xB5\x8B\xE8\xAF\x95");
                }
            }
            // finally rename layers key itself
            RenameChildKey(hole, "layers", "\xE5\x9C\xB0\xE5\xB1\x82\xE5\x88\x97\xE8\xA1\xA8");
        }
    }
}
// --- end auto-generated ---

// ============================================================
// 二维数组 JSON → 英文键结构化 归一化层
// 移植自 cad_server/draw_gateway.py::_normalize_one_hole
// 行/列偏移基于 cad_client/阳光幸福里-钻探编录表.json 真实数据
// ============================================================

// 行偏移（0-based）
static const int ROW_INFO_1        = 2;   // 工程名称 / 孔口高程 / 钻孔类型 / 终孔深度
static const int ROW_INFO_2        = 3;   // 工程地点 / 开孔日期 / 初见水位
static const int ROW_INFO_3        = 4;   // 孔号 / X坐标 / Y坐标 / 终孔日期 / 稳定水位
static const int ROW_STRATA_START  = 7;   // 第一个地层数据行

// 列偏移（0-based）
static const int COL_PROJ_NAME    = 1;
static const int COL_ELEVATION    = 11;
static const int COL_HOLE_TYPE    = 14;
static const int COL_DEPTH        = 18;
static const int COL_PROJ_LOC     = 1;
static const int COL_DATE_START   = 11;
static const int COL_HOLE_NO      = 1;
static const int COL_X_SURVEY     = 4;   // 测量 X = 北 → CAD y（纵向）
static const int COL_Y_SURVEY     = 9;   // 测量 Y = 东 → CAD x（横向）
static const int COL_DATE_END     = 11;
static const int COL_WATER_INIT   = 14;
static const int COL_WATER_STABLE = 14;

// 地层字段列偏移
static const int COL_STRATA_ID     = 0;
static const int COL_STRATA_FROM   = 1;
static const int COL_STRATA_TO     = 2;
static const int COL_STRATA_NAME   = 3;
static const int COL_STRATA_COLOR  = 4;
static const int COL_STRATA_STATE  = 5;
static const int COL_STRATA_DENS   = 6;
static const int COL_STRATA_MOIST  = 7;
static const int COL_STRATA_DESC   = 8;
static const int COL_SAMPLE_NO     = 13;
static const int COL_SAMPLE_DEPTH  = 14;
static const int COL_TEST_TYPE     = 15;
static const int COL_TEST_FROM     = 16;
static const int COL_TEST_TO      = 17;
static const int COL_TEST_ROD      = 18;
static const int COL_TEST_HITS     = 19;

// --- 安全取值辅助 ---
static std::string JsonToStringSafe(cJSON* item) {
    if (!item) return "";
    if (cJSON_IsString(item) && item->valuestring) return item->valuestring;
    if (cJSON_IsNumber(item)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", item->valuedouble);
        return buf;
    }
    return "";
}

static double JsonToFloatSafe(cJSON* item) {
    if (!item) return kNAN;
    if (cJSON_IsNumber(item)) return item->valuedouble;
    if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
        return atof(item->valuestring);
    }
    return kNAN;
}

// --- 形态判定：第一个 value 是 array → 二维数组形态 ---
static bool Is2DArrayBorehole(cJSON* borehole) {
    if (!borehole || !cJSON_IsObject(borehole)) return false;
    cJSON* first = NULL;
    cJSON_ArrayForEach(first, borehole) {
        return cJSON_IsArray(first) == 1;
    }
    return false;
}

// --- 单孔归一化：rows（array of array）→ 英文键结构化 object ---
// 失败返回 NULL，outErrMsg 返回错误描述
static cJSON* NormalizeOneHole(cJSON* rows, const char* holeNo, std::string& outErrMsg) {
    if (!rows || !cJSON_IsArray(rows)) {
        outErrMsg = "rows is not an array";
        return NULL;
    }
    int rowCount = cJSON_GetArraySize(rows);
    if (rowCount < ROW_STRATA_START) {
        outErrMsg = "row count < 7, template mismatch";
        return NULL;
    }

    // row[4] = 孔号 + 坐标行
    cJSON* row4 = cJSON_GetArrayItem(rows, ROW_INFO_3);
    if (!row4 || !cJSON_IsArray(row4)) {
        outErrMsg = "row[4] not array";
        return NULL;
    }

    // 校验 row[4][0] == "钻孔编号"（UTF-8: \xE9\x92\xBB\xE5\xAD\x94\xE7\xBC\x96\xE5\x8F\xB7）
    std::string label0 = JsonToStringSafe(cJSON_GetArrayItem(row4, 0));
    if (label0 != "\xE9\x92\xBB\xE5\xAD\x94\xE7\xBC\x96\xE5\x8F\xB7") {
        outErrMsg = "row[4][0] label mismatch (expect '钻孔编号')";
        return NULL;
    }

    // 校验 row[4][1] == 顶层 key（孔号双校验）
    std::string holeNoInRow = JsonToStringSafe(cJSON_GetArrayItem(row4, COL_HOLE_NO));
    if (holeNoInRow != holeNo) {
        outErrMsg = "hole_no mismatch: row='" + holeNoInRow + "' key='" + holeNo + "'";
        return NULL;
    }

    cJSON* row2 = cJSON_GetArrayItem(rows, ROW_INFO_1);
    cJSON* row3 = cJSON_GetArrayItem(rows, ROW_INFO_2);

    // X/Y 轴交换：CAD x = 测量东(row[4][9])，CAD y = 测量北(row[4][4])
    double xCad = JsonToFloatSafe(cJSON_GetArrayItem(row4, COL_Y_SURVEY));
    double yCad = JsonToFloatSafe(cJSON_GetArrayItem(row4, COL_X_SURVEY));

    cJSON* hole = cJSON_CreateObject();
    cJSON_AddStringToObject(hole, "hole_no", holeNoInRow.c_str());
    if (!IsInvalid(xCad)) cJSON_AddNumberToObject(hole, "x", xCad);
    if (!IsInvalid(yCad)) cJSON_AddNumberToObject(hole, "y", yCad);

    double elev = JsonToFloatSafe(cJSON_GetArrayItem(row2, COL_ELEVATION));
    if (!IsInvalid(elev)) cJSON_AddNumberToObject(hole, "elevation", elev);

    double depth = JsonToFloatSafe(cJSON_GetArrayItem(row2, COL_DEPTH));
    if (!IsInvalid(depth)) cJSON_AddNumberToObject(hole, "depth", depth);

    cJSON_AddStringToObject(hole, "hole_type",
        JsonToStringSafe(cJSON_GetArrayItem(row2, COL_HOLE_TYPE)).c_str());
    cJSON_AddStringToObject(hole, "project_name",
        JsonToStringSafe(cJSON_GetArrayItem(row2, COL_PROJ_NAME)).c_str());
    cJSON_AddStringToObject(hole, "site",
        JsonToStringSafe(cJSON_GetArrayItem(row3, COL_PROJ_LOC)).c_str());
    cJSON_AddStringToObject(hole, "date_start",
        JsonToStringSafe(cJSON_GetArrayItem(row3, COL_DATE_START)).c_str());
    cJSON_AddStringToObject(hole, "date_end",
        JsonToStringSafe(cJSON_GetArrayItem(row4, COL_DATE_END)).c_str());

    double wInit = JsonToFloatSafe(cJSON_GetArrayItem(row3, COL_WATER_INIT));
    if (!IsInvalid(wInit)) cJSON_AddNumberToObject(hole, "water_level_initial", wInit);
    else cJSON_AddStringToObject(hole, "water_level_initial", "");

    double wStable = JsonToFloatSafe(cJSON_GetArrayItem(row4, COL_WATER_STABLE));
    if (!IsInvalid(wStable)) cJSON_AddNumberToObject(hole, "water_level_stable", wStable);
    else cJSON_AddStringToObject(hole, "water_level_stable", "");

    // 地层列表：从 row[ROW_STRATA_START] 起
    cJSON* layers = cJSON_CreateArray();
    for (int r = ROW_STRATA_START; r < rowCount; r++) {
        cJSON* row = cJSON_GetArrayItem(rows, r);
        if (!row || !cJSON_IsArray(row)) continue;

        // 空行跳过
        bool allEmpty = true;
        int rowLen = cJSON_GetArraySize(row);
        for (int c = 0; c < rowLen; c++) {
            std::string s = JsonToStringSafe(cJSON_GetArrayItem(row, c));
            if (!s.empty()) { allEmpty = false; break; }
        }
        if (allEmpty) continue;

        std::string idCell = JsonToStringSafe(cJSON_GetArrayItem(row, COL_STRATA_ID));
        std::string nameCell = JsonToStringSafe(cJSON_GetArrayItem(row, COL_STRATA_NAME));

        // "终孔"行跳过（UTF-8: \xE7\xBB\x88\xE5\xAD\x94）
        if (nameCell == "\xE7\xBB\x88\xE5\xAD\x94") continue;

        // "项目负责人"行跳过（UTF-8: \xE9\xA1\xB9\xE7\x9B\xAE\xE8\xB4\x9F\xE8\xB4\xA3\xE4\xBA\xBA）
        if (idCell.find("\xE9\xA1\xB9\xE7\x9B\xAE\xE8\xB4\x9F\xE8\xB4\xA3\xE4\xBA\xBA") != std::string::npos) continue;

        // from/to 都为空跳过
        double sFrom = JsonToFloatSafe(cJSON_GetArrayItem(row, COL_STRATA_FROM));
        double sTo = JsonToFloatSafe(cJSON_GetArrayItem(row, COL_STRATA_TO));
        if (IsInvalid(sFrom) && IsInvalid(sTo)) continue;

        cJSON* layer = cJSON_CreateObject();
        cJSON_AddStringToObject(layer, "layer_id", idCell.c_str());
        if (!IsInvalid(sFrom)) cJSON_AddNumberToObject(layer, "from", sFrom);
        if (!IsInvalid(sTo)) cJSON_AddNumberToObject(layer, "to", sTo);
        cJSON_AddStringToObject(layer, "name", nameCell.c_str());
        cJSON_AddStringToObject(layer, "color",
            JsonToStringSafe(cJSON_GetArrayItem(row, COL_STRATA_COLOR)).c_str());
        cJSON_AddStringToObject(layer, "state",
            JsonToStringSafe(cJSON_GetArrayItem(row, COL_STRATA_STATE)).c_str());
        cJSON_AddStringToObject(layer, "density",
            JsonToStringSafe(cJSON_GetArrayItem(row, COL_STRATA_DENS)).c_str());
        cJSON_AddStringToObject(layer, "moisture",
            JsonToStringSafe(cJSON_GetArrayItem(row, COL_STRATA_MOIST)).c_str());
        cJSON_AddStringToObject(layer, "description",
            JsonToStringSafe(cJSON_GetArrayItem(row, COL_STRATA_DESC)).c_str());

        // 取样子对象：任一非空才输出
        std::string sampleNo = JsonToStringSafe(cJSON_GetArrayItem(row, COL_SAMPLE_NO));
        std::string sampleDepth = JsonToStringSafe(cJSON_GetArrayItem(row, COL_SAMPLE_DEPTH));
        if (!sampleNo.empty() || !sampleDepth.empty()) {
            cJSON* sample = cJSON_CreateObject();
            cJSON_AddStringToObject(sample, "no", sampleNo.c_str());
            cJSON_AddStringToObject(sample, "depth", sampleDepth.c_str());
            cJSON_AddItemToObject(layer, "sample", sample);
        }

        // 原位测试子对象：type 空且其他都空才不输出；from/to/rod_length/hits 必须为 string
        std::string testType  = JsonToStringSafe(cJSON_GetArrayItem(row, COL_TEST_TYPE));
        std::string testFrom  = JsonToStringSafe(cJSON_GetArrayItem(row, COL_TEST_FROM));
        std::string testTo    = JsonToStringSafe(cJSON_GetArrayItem(row, COL_TEST_TO));
        std::string testRod    = JsonToStringSafe(cJSON_GetArrayItem(row, COL_TEST_ROD));
        std::string testHits   = JsonToStringSafe(cJSON_GetArrayItem(row, COL_TEST_HITS));
        if (!testType.empty() || !testFrom.empty() || !testTo.empty() ||
            !testRod.empty() || !testHits.empty()) {
            cJSON* test = cJSON_CreateObject();
            cJSON_AddStringToObject(test, "type", testType.c_str());
            cJSON_AddStringToObject(test, "from", testFrom.c_str());
            cJSON_AddStringToObject(test, "to", testTo.c_str());
            cJSON_AddStringToObject(test, "rod_length", testRod.c_str());
            cJSON_AddStringToObject(test, "hits", testHits.c_str());
            cJSON_AddItemToObject(layer, "test", test);
        }

        cJSON_AddItemToArray(layers, layer);
    }
    cJSON_AddItemToObject(hole, "layers", layers);

    return hole;
}

// --- 顶层归一化入口：二维数组 → 结构化英文 JSON ---
static cJSON* NormalizeBorehole2DArray(cJSON* borehole, std::string& outErrMsg) {
    if (!borehole || !cJSON_IsObject(borehole)) {
        outErrMsg = "borehole_data must be a JSON object";
        return NULL;
    }
    cJSON* out = cJSON_CreateObject();
    cJSON* hole = NULL;
    cJSON_ArrayForEach(hole, borehole) {
        const char* holeNo = hole->string;
        if (!holeNo) continue;
        std::string err;
        cJSON* normalized = NormalizeOneHole(hole, holeNo, err);
        if (!normalized) {
            outErrMsg = std::string("hole '") + holeNo + "': " + err;
            cJSON_Delete(out);
            return NULL;
        }
        cJSON_AddItemToObject(out, holeNo, normalized);
    }
    return out;
}

void HandleApiDraw(struct mg_connection* nc,
                   struct mg_http_message* hm,
                   const std::map<std::string, std::string>& params,
                   const std::string& body,
                   void* user_data)
{
    std::string response;

    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) {
        AiApiServer::SendJsonResponse(nc, 400, "{\"status\":\"error\",\"message\":\"invalid JSON\"}");
        return;
    }

    // 0. convert english keys to chinese (UTF-8 keys for C++ downstream)
    if (root) ConvertBoreholeDataEnToCn(cJSON_GetObjectItem(root, "borehole_data"));

    // 1. borehole_data（必填）
    cJSON* jBorehole = cJSON_GetObjectItem(root, "borehole_data");
    if (!jBorehole || !cJSON_IsObject(jBorehole)) {
        cJSON_Delete(root);
        AiApiServer::SendJsonResponse(nc, 400, "{\"status\":\"error\",\"message\":\"missing borehole_data object\"}");
        return;
    }

    // 2. views（可选，默认全画）
    std::vector<std::string> views;
    cJSON* jViews = cJSON_GetObjectItem(root, "views");
    if (jViews && cJSON_IsArray(jViews)) {
        cJSON* v;
        cJSON_ArrayForEach(v, jViews) {
            if (cJSON_IsString(v) && v->valuestring) views.push_back(v->valuestring);
        }
    } else {
        views.push_back("plan");
        views.push_back("section");
        views.push_back("column");
    }

    // 3. auto_zoom（默认 true）
    bool autoZoom = true;
    cJSON* jZoom = cJSON_GetObjectItem(root, "auto_zoom");
    if (jZoom && cJSON_IsBool(jZoom)) autoZoom = cJSON_IsTrue(jZoom);

    // 4. 拿到 acad 主窗口
    AiApiRequest* apiRequest = (AiApiRequest*)user_data;
    CWnd* pAcadWnd = apiRequest ? apiRequest->getAcadWnd() : NULL;
    if (!pAcadWnd) {
        cJSON_Delete(root);
        AiApiServer::SendJsonResponse(nc, 500, "{\"status\":\"error\",\"message\":\"AutoCAD window not available\"}");
        return;
    }

    // 5. 写 borehole JSON 落盘
    char* bhStr = cJSON_PrintUnformatted(jBorehole);
    std::string bhUtf8 = bhStr ? bhStr : "{}";
    if (bhStr) free(bhStr);
    bool bhOk = _WriteJsonToProjectFile(L"project/\u94BB\u63A2\u7F16\u5F55\u8868.json", bhUtf8);
    if (!bhOk) {
        cJSON_Delete(root);
        // 返回实际拼出的 path，方便排查
        CT2CA pszPath(g_lastFailedPath, CP_UTF8);
        std::string resp = "{\"status\":\"error\",\"message\":\"failed to write borehole json\",\"path\":\"";
        resp += pszPath;
        resp += "\"}";
        AiApiServer::SendJsonResponse(nc, 500, resp);
        return;
    }

    // 6. section_params（可选）
    cJSON* jSection = cJSON_GetObjectItem(root, "section_params");
    if (jSection && cJSON_IsObject(jSection)) {
        char* secStr = cJSON_PrintUnformatted(jSection);
        std::string secUtf8 = secStr ? secStr : "{}";
        if (secStr) free(secStr);
        _WriteJsonToProjectFile(L"project/\u5256\u9762\u56FE\u53C2\u6570.json", secUtf8);
    }

    // 7. 串行执行 views
    cJSON* results = cJSON_CreateObject();
    cJSON* errors = cJSON_CreateObject();
    for (size_t i = 0; i < views.size(); ++i) {
        const std::string& v = views[i];
        UINT cmdId = 0;
        if (v == "plan")          cmdId = WM_USER_FEATURE_PLANVIEW;
        else if (v == "section")  cmdId = WM_USER_FEATURE_SECTIONVIEW;
        else if (v == "column")   cmdId = WM_USER_FEATURE_COLUMNVIEW;
        else {
            cJSON_AddStringToObject(errors, v.c_str(), "unknown view");
            continue;
        }
        int ret = _ExecToolSync(pAcadWnd, cmdId, 120000);
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "status", ret == 0 ? "success" : "error");
        cJSON_AddItemToObject(results, v.c_str(), item);
        if (ret != 0) {
            cJSON_AddStringToObject(errors, v.c_str(), ret == 2 ? "timeout" : "failed");
        }
    }

    // 8. auto zoom
    if (autoZoom && cJSON_GetArraySize(errors) == 0) {
        _ExecToolSync(pAcadWnd, WM_USER_A_ZOOM_EXTENTS, 30000);
    }

    // 9. 拼响应
    cJSON* resp = cJSON_CreateObject();
    bool allOk = (cJSON_GetArraySize(errors) == 0);
    cJSON_AddStringToObject(resp, "status", allOk ? "success" : "partial");
    cJSON_AddItemToObject(resp, "results", results);
    cJSON_AddItemToObject(resp, "errors", errors);

    char* printed = cJSON_PrintUnformatted(resp);
    response = printed ? printed : "{}";
    if (printed) free(printed);
    cJSON_Delete(resp);
    cJSON_Delete(root);

    AiApiServer::SendJsonResponse(nc, allOk ? 200 : 207, response);
}


// ---------- /ai/feature/create_*_view：支持直接传 borehole_data（二维数组或结构化） ----------
// 改造点：原来这三个路由走 HandleAiTool，只收空 body 触发出图；
// 现在支持 body 里带 borehole_data，内部完成归一化（含 X/Y 轴交换）+ 英译中 + 落盘 + 出图。
void HandleAiFeatureView(struct mg_connection* nc,
                         struct mg_http_message* hm,
                         const std::map<std::string, std::string>& params,
                         const std::string& body,
                         void* user_data)
{
    std::string response;
    std::string uri(hm->uri.buf, hm->uri.len);

    // 1. 根据 URI 判断 view 类型
    UINT cmdId = 0;
    if (uri.find("create_plan_view") != std::string::npos) {
        cmdId = WM_USER_FEATURE_PLANVIEW;
    } else if (uri.find("create_column_view") != std::string::npos) {
        cmdId = WM_USER_FEATURE_COLUMNVIEW;
    } else if (uri.find("create_section_view") != std::string::npos) {
        cmdId = WM_USER_FEATURE_SECTIONVIEW;
    } else {
        AiApiServer::SendJsonResponse(nc, 404, "{\"status\":\"error\",\"message\":\"unknown feature endpoint\"}");
        return;
    }

    // 2. 拿到 acad 主窗口
    AiApiRequest* apiRequest = (AiApiRequest*)user_data;
    CWnd* pAcadWnd = apiRequest ? apiRequest->getAcadWnd() : NULL;
    if (!pAcadWnd) {
        AiApiServer::SendJsonResponse(nc, 500, "{\"status\":\"error\",\"message\":\"AutoCAD window not available\"}");
        return;
    }

    cJSON* root = NULL;
    bool wroteBorehole = false;

    // === 日志：收到请求 ===
    {
        std::string logLine = "=== RECEIVED " + uri + " ===\n";
        char lenBuf[32];
        snprintf(lenBuf, sizeof(lenBuf), "%d", (int)body.size());
        logLine += "body length: ";
        logLine += lenBuf;
        logLine += "\n";
        logLine += "body content:\n" + body + "\n";
        _LogToFile(L"7100.log", logLine);
    }

    // 3. 如果 body 非空，尝试解析 borehole_data
    if (!body.empty()) {
        root = cJSON_Parse(body.c_str());
        if (!root) {
            _LogToFile(L"7100.log", "ERROR: invalid JSON body\n");
            AiApiServer::SendJsonResponse(nc, 400, "{\"status\":\"error\",\"message\":\"invalid JSON body\"}");
            return;
        }

        cJSON* jBorehole = cJSON_GetObjectItem(root, "borehole_data");
        if (jBorehole && cJSON_IsObject(jBorehole)) {
            // 判定形态：二维数组 → 归一化；结构化 → 直接英译中
            cJSON* jBoreholeEn = jBorehole;
            std::string err;
            bool is2D = Is2DArrayBorehole(jBorehole);
            _LogToFile(L"7100.log", std::string("borehole_data format: ") + (is2D ? "2D_ARRAY" : "STRUCTURED"));

            if (is2D) {
                jBoreholeEn = NormalizeBorehole2DArray(jBorehole, err);
                if (!jBoreholeEn) {
                    std::string errMsg = "NORMALIZE FAILED: " + err;
                    _LogToFile(L"7100.log", errMsg);
                    std::string resp = "{\"status\":\"error\",\"message\":\"borehole_data normalize failed: ";
                    resp += err;
                    resp += "\"}";
                    AiApiServer::SendJsonResponse(nc, 400, resp);
                    cJSON_Delete(root);
                    return;
                }
                // 日志：归一化后的英文 JSON
                char* normStr = cJSON_PrintUnformatted(jBoreholeEn);
                if (normStr) {
                    _LogToFile(L"7100.log", std::string("normalized (EN):\n") + normStr);
                    free(normStr);
                }
            }

            // 结构化 JSON 路径：服务端内部做 X/Y 轴交换
            // （测量北=x / 测量东=y → CAD 东=x / 北=y）
            // 二维数组路径已在 NormalizeOneHole 里做过交换，跳过
            if (!is2D) {
                cJSON* holeIt = NULL;
                cJSON_ArrayForEach(holeIt, jBoreholeEn) {
                    if (!cJSON_IsObject(holeIt)) continue;
                    cJSON* jx = cJSON_GetObjectItem(holeIt, "x");
                    cJSON* jy = cJSON_GetObjectItem(holeIt, "y");
                    if (jx && jy && cJSON_IsNumber(jx) && cJSON_IsNumber(jy)) {
                        double tmp = jx->valuedouble;
                        jx->valuedouble = jy->valuedouble;
                        jy->valuedouble = tmp;
                    }
                }
                _LogToFile(L"7100.log", "applied X/Y axis swap (structured input)");
            }

            // 英译中键名
            ConvertBoreholeDataEnToCn(jBoreholeEn);

            // 落盘到 project/钻探编录表.json
            char* bhStr = cJSON_PrintUnformatted(jBoreholeEn);
            std::string bhUtf8 = bhStr ? bhStr : "{}";
            if (bhStr) free(bhStr);

            // 先删除旧的落盘文件，避免残留旧全量数据污染本次出图
            DeleteFileW(L"D:/project/ies/cad/exploration/Debug/project/\u94BB\u63A2\u7F16\u5F55\u8868.json");
            // 日志：落盘内容（中文键）
            _LogToFile(L"7100.log", std::string("written to D:/project/ies/cad/exploration/Debug/project/钻探编录表.json (CN keys):\n") + bhUtf8);

            bool bhOk = _WriteJsonToProjectFile(L"D:/project/ies/cad/exploration/Debug/project/\u94BB\u63A2\u7F16\u5F55\u8868.json", bhUtf8);
            if (!bhOk) {
                CT2CA pszPath(g_lastFailedPath, CP_UTF8);
                std::string resp = "{\"status\":\"error\",\"message\":\"failed to write borehole json\",\"path\":\"";
                resp += pszPath;
                resp += "\"}";
                _LogToFile(L"7100.log", std::string("WRITE FAILED: ") + (const char*)pszPath);
                AiApiServer::SendJsonResponse(nc, 500, resp);
                if (jBoreholeEn != jBorehole) cJSON_Delete(jBoreholeEn);
                cJSON_Delete(root);
                return;
            }
            wroteBorehole = true;

            // 如果是归一化产生的临时对象，释放
            if (jBoreholeEn != jBorehole) cJSON_Delete(jBoreholeEn);
        }
    }

    // 4. 执行出图（PostMessage + 等待完成）
    int ret = _ExecToolSync(pAcadWnd, cmdId, 120000);
    _LogToFile(L"7100.log", std::string("draw result: ") + (ret == 0 ? "SUCCESS" : (ret == 2 ? "TIMEOUT" : "FAILED")));

    // 5. auto zoom（默认 true；只有出图成功才 zoom）
    if (ret == 0) {
        _ExecToolSync(pAcadWnd, WM_USER_A_ZOOM_EXTENTS, 30000);
    }

    // 6. 返回响应
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", ret == 0 ? "success" : "error");
    if (ret != 0) {
        cJSON_AddStringToObject(resp, "message", ret == 2 ? "execution timeout" : "execution failed");
    }
    char* printed = cJSON_PrintUnformatted(resp);
    response = printed ? printed : "{}";
    if (printed) free(printed);
    cJSON_Delete(resp);

    _LogToFile(L"7100.log", std::string("=== RESPONSE: ") + response + " ===\n\n");

    if (root) cJSON_Delete(root);

    AiApiServer::SendJsonResponse(nc, ret == 0 ? 200 : 500, response);
}


AiApiRequest::AiApiRequest(void):m_Server(NULL),pAcadWnd(NULL)
{
}

AiApiRequest::~AiApiRequest(void)
{
	if (m_Server){
		delete m_Server;
	}
}
void AiApiRequest::Start()
{
	m_Server=new AiApiServer();
	
    m_Server->RegisterRoute("POST", "/ai/chat", HandleAiChat);
    m_Server->RegisterRoute("POST", "/ai/generate", HandleAiGenerate);
	m_Server->RegisterRoute("POST", "/ai/command", HandleAiCommand);
	//tool

	
    size_t routeCount = sizeof(toolRoutes) / sizeof(toolRoutes[0]);
    for (size_t i = 0; i <  routeCount; ++i) {
        m_Server->RegisterRoute("POST", toolRoutes[i].uriPath, HandleAiTool, this);
    }

	// feature 出图三接口：支持 body 带 borehole_data（二维数组或结构化），内部归一化+落盘+出图
	m_Server->RegisterRoute("POST", "/ai/feature/create_plan_view",   HandleAiFeatureView, this);
	m_Server->RegisterRoute("POST", "/ai/feature/create_column_view", HandleAiFeatureView, this);
	m_Server->RegisterRoute("POST", "/ai/feature/create_section_view", HandleAiFeatureView, this);

	// 一体化出图：一次 HTTP 调用完成"收参数 -> 落盘 -> 出 plan/section/column"
	m_Server->RegisterRoute("POST", "/api/draw", HandleApiDraw, this);

	m_Server->StartHttpServer();
}
void AiApiRequest::Stop()
{
	if (m_Server)
	{
		m_Server->Stop();
	}
}
void AiApiRequest::setAcadWnd(CWnd* pWnd)
{
	this->pAcadWnd=pWnd;
}
CWnd* AiApiRequest::getAcadWnd()
{
	return this->pAcadWnd;
}