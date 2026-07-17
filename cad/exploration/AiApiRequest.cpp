// coding: gb18030
#include "stdafx.h"
#include "AiApiRequest.h"
#include "AiApiServer.h"
#include "cJSON.h"
#include "declare.h"

AiApiRequest::AiApiRequest(void):m_Server(NULL),pAcadWnd(NULL)
{
}

AiApiRequest::~AiApiRequest(void)
{
	if (m_Server){
		delete m_Server;
	}
}

// AI 聊天接口
void HandleAiChat(struct mg_connection* nc, 
				  struct mg_http_message* hm,
                  const std::map<std::string, std::string>& params,
                  const std::string& body,
				  void* user_data)
{
    // 这里可以调用 OpenAI/百度文心/讯飞等 AI API
    // body 包含客户端发来的 JSON 请求数据
    
    std::string response = "{\"status\":\"success\",\"message\":\"AI reply placeholder\"}";
    AiApiServer::SendJsonResponse(nc, 200, response);
}

// AI 文本生成接口
void HandleAiGenerate(struct mg_connection* nc,
					  struct mg_http_message* hm,
                      const std::map<std::string, std::string>& params,
                      const std::string& body,
					  void* user_data)
{
    // 调用 AI 接口生成文本
    std::string response = "{\"text\":\"Generated content...\"}";
    AiApiServer::SendJsonResponse(nc, 200, response);
}
// AI 命令执行接口
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
            acDocManager->sendStringToExecute(acDocManager->curDocument(), wCmd.c_str());
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
bool EndsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}
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

    // 4. Determine the specific command based on URI
    UINT nCommandID = 0;
    if (EndsWith(uri, "/create_line") || EndsWith(uri, "create_line")) {
        nCommandID = WM_USER_A_CREATELINE;
    } 
    else if (EndsWith(uri, "/create_circle") || EndsWith(uri, "create_circle")) {
        nCommandID = WM_USER_A_CREATECIRCLE; // Assume you define this
    }
	else if (EndsWith(uri, "/create_circle") || EndsWith(uri, "create_circle")) {
        nCommandID = WM_USER_A_CREATECIRCLE; // Assume you define this
    }
	else if (EndsWith(uri, "/create_rectangle") || EndsWith(uri, "create_rectangle")) {
        nCommandID = WM_USER_A_CREATERECTANGLE; // Assume you define this
    }
	else if (EndsWith(uri, "/create_text") || EndsWith(uri, "create_text")) {
        nCommandID = WM_USER_A_CREATERTEXT; // Assume you define this
    }
	else if (EndsWith(uri, "/create_layer") || EndsWith(uri, "create_layer")) {
        nCommandID = WM_USER_A_CREATELAYER; // Assume you define this
    }
	else if (EndsWith(uri, "/list_layers") || EndsWith(uri, "list_layers")) {
        nCommandID = WM_USER_A_LISTLAYERS; // Assume you define this
    }
    else {
        // Cleanup for unknown command
        delete pData;
        CloseHandle(hEvent);
        response = "{\"status\":\"error\",\"message\":\"Unknown tool endpoint: " + uri + "\"}";
        AiApiServer::SendJsonResponse(nc, 404, response);
        return;
    }

    // 5. Post Message to AutoCAD Main Thread
    // WPARAM: Command ID, LPARAM: Pointer to our data structure
    pAcadWnd->PostMessage(WM_USER_TOOL, nCommandID, (LPARAM)pData);

     // 6. Wait for the event to be signaled by the main thread
    // Timeout after 10 seconds to prevent indefinite hanging
    DWORD waitResult = WaitForSingleObject(hEvent, 10000); 

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
            cJSON_AddStringToObject(responseRoot, "message", "Tool executed successfully");
            
        } else {
            // No specific data returned
            cJSON_AddStringToObject(responseRoot, "status", "error");
            cJSON_AddStringToObject(responseRoot, "message", "Failed to serialize response");
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
void AiApiRequest::Start()
{
	m_Server=new AiApiServer();
	// 注册路由
    m_Server->RegisterRoute("POST", "/ai/chat", HandleAiChat);
    m_Server->RegisterRoute("POST", "/ai/generate", HandleAiGenerate);
	m_Server->RegisterRoute("POST", "/ai/command", HandleAiCommand);
	//tool

	m_Server->RegisterRoute("POST", "/ai/tool/create_line", HandleAiTool, this);
	m_Server->RegisterRoute("POST", "/ai/tool/create_circle", HandleAiTool, this);
	m_Server->RegisterRoute("POST", "/ai/tool/create_rectangle", HandleAiTool, this);
	m_Server->RegisterRoute("POST", "/ai/tool/create_text", HandleAiTool, this);
	m_Server->RegisterRoute("POST", "/ai/tool/create_layer", HandleAiTool, this);
	m_Server->RegisterRoute("POST", "/ai/tool/list_layers", HandleAiTool, this);

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