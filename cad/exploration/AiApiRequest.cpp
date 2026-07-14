// coding: gb18030
#include "stdafx.h"
#include "AiApiRequest.h"
#include "AiApiServer.h"
#include "cJSON.h"
#include "ToolDraw.h"

AiApiRequest::AiApiRequest(void):m_Server(NULL),pAcadWnd(NULL)
{
}

AiApiRequest::~AiApiRequest(void)
{
}

// AI 聊天接口
void HandleAiChat(struct mg_connection* nc, 
				  struct mg_http_message* hm,
                  const std::map<std::string, std::string>& params,
                  const std::string& body)
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
                      const std::string& body)
{
    // 调用 AI 接口生成文本
    std::string response = "{\"text\":\"Generated content...\"}";
    AiApiServer::SendJsonResponse(nc, 200, response);
}
// AI 命令执行接口
void HandleAiCommand(struct mg_connection* nc,
					 struct mg_http_message* hm,
                      const std::map<std::string, std::string>& params,
                      const std::string& body)
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
                  const std::string& body)
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

    // 2. Check if it matches 'create_line'
    if (EndsWith(uri, "/create_line") || EndsWith(uri, "create_line")) {
        // Handle Create Line Logic
        // Parse 'body' if needed
		pAcadWnd->PostMessage(WM_USER_EXECUTE_COMMAND, 0, (LPARAM)root);

        response = "{\"status\":\"success\",\"message\":\"Line created\"}";
        AiApiServer::SendJsonResponse(nc, 200, response);
    }
    // 3. Check if it matches 'create_circle'
    else if (EndsWith(uri, "/create_circle") || EndsWith(uri, "create_circle")) {
        // Handle Create Circle Logic
        
        response = "{\"status\":\"success\",\"message\":\"Circle created\"}";
        AiApiServer::SendJsonResponse(nc, 200, response);
    }
    else {
        // Unknown tool endpoint
        response = "{\"status\":\"error\",\"message\":\"Unknown tool endpoint: " + uri + "\"}";
        AiApiServer::SendJsonResponse(nc, 404, response);
    }
}
void AiApiRequest::Start()
{
	m_Server=new AiApiServer();
	// 注册路由
    m_Server->RegisterRoute("POST", "/ai/chat", HandleAiChat);
    m_Server->RegisterRoute("POST", "/ai/generate", HandleAiGenerate);
	m_Server->RegisterRoute("POST", "/ai/command", HandleAiCommand);
	m_Server->RegisterRoute("POST", "/ai/tool/create_line", HandleAiTool);
	m_Server->RegisterRoute("POST", "/ai/tool/create_circle", HandleAiTool);
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