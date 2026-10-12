// coding: gb18030
#include "stdafx.h"
#include "AiApiRequest.h"
#include "AiApiServer.h"
#include "cJSON.h"
#include "Utils.h"
#include "../tools/ToolGlobal.h"
#include <afx.h>
#include <windows.h>

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

	{ "/ai/tool/test", "test", WM_USER_TOOL_TEST },

	{ "/ai/feature/create_plan_view", "create_plan_view", WM_USER_FEATURE_PLANVIEW },
	{ "/ai/feature/create_column_view", "create_column_view", WM_USER_FEATURE_COLUMNVIEW },
	{ "/ai/feature/create_section_view", "create_section_view", WM_USER_FEATURE_SECTIONVIEW }
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