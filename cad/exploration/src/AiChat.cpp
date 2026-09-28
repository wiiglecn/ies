#include "stdafx.h"
#include <fstream>
#include <iterator>
#include "AiChat.h"
#include "HttpClient.h"   // 
//#include "HttpClientCurl.h"   // 原: #include "HttpClient.h"
#include "cJSON.h"
#include "Utils.h"
#include "Logger.h"
#include "declare.h"
//#define WORK_URL _T("https://ws-93xgld532jdyrkjf.cn-beijing.maas.aliyuncs.com/compatible-mode/v1/chat/completions")
//#define WORK_URL _T("https://ws-93xgld532jdyrkjf.cn-beijing.maas.aliyuncs.com/compatible-mode/v1/chat/completions")

// test
//#define WORK_URL _T("https://127.0.0.1:7101/chat")
#define WORK_URL _T("https://cad.kskjai.com:7101/chat")

#define DASHSCOPE_API_KEY _T("sl-123456")
#define LOCAL_TOOL_URL_PREFIX _T("http://localhost:7100/ai/tool/")

#define CHAT_KEY "KS123456"

struct AsyncContext {
    AiChat* pAiChat;
    CHttpClient* pClient;
};

static bool IsFinishReasonToolCalls(cJSON* choice)
{
    cJSON* finishReason = cJSON_GetObjectItem(choice, "finish_reason");
    return finishReason != NULL && cJSON_IsString(finishReason) &&
           strcmp(finishReason->valuestring, "tool_calls") == 0;
}

static std::string PrintJsonUnformatted(cJSON* item)
{
    std::string result;
    char* printed = cJSON_PrintUnformatted(item);
    if (printed != NULL)
    {
        result = printed;
        free(printed);
    }
    return result;
}

static bool GetToolArgumentsJson(cJSON* argumentsItem, std::string& argumentsJson)
{
    argumentsJson.clear();
    if (argumentsItem == NULL)
        return false;

    if (cJSON_IsString(argumentsItem))
    {
        cJSON* argsRoot = cJSON_Parse(argumentsItem->valuestring);
        if (argsRoot == NULL)
            return false;

        argumentsJson = PrintJsonUnformatted(argsRoot);
        cJSON_Delete(argsRoot);
        return !argumentsJson.empty();
    }

    if (cJSON_IsObject(argumentsItem) || cJSON_IsArray(argumentsItem))
    {
        argumentsJson = PrintJsonUnformatted(argumentsItem);
        return !argumentsJson.empty();
    }

    return false;
}

static std::string CallLocalTool(const char* functionName, const std::string& argumentsJson)
{
    if (functionName == NULL || functionName[0] == '\0')
        return "{}";

    CString toolUrl(LOCAL_TOOL_URL_PREFIX);
    toolUrl += CString(functionName);

    CHttpClient toolClient;
    toolClient.Initialize();   // 幂等,引用计数
    toolClient.SetHeader(_T("Content-Type"), _T("application/json"));

    std::string toolResponse;
    DWORD toolStatusCode = 0;
    BOOL bToolSuccess = toolClient.Post(toolUrl, argumentsJson, toolResponse, toolStatusCode);

    CString toolName(functionName);
    if (bToolSuccess)
    {
        //CLogger::GetInstance().Debug(_T("[AI TOOL] %s status: %d\n"), toolName, toolStatusCode);
        CLogger::GetInstance().Debug(toolResponse);
    }
    else
    {
        CLogger::GetInstance().Debug(_T("[AI TOOL] %s request failed. Status Code: %d\n"), toolName, toolStatusCode);
    }
    if (toolResponse.empty()) {
        toolResponse = "{}";
    }
    
    return toolResponse;
}

void OnHttpResponse(BOOL bSuccess, const std::string& strResponse,
                    DWORD dwStatusCode, LPVOID pUserData)
{
    AsyncContext* pCtx  = (AsyncContext*)pUserData;
    if (pCtx  != NULL)
    {
        CHttpClient* pClient = pCtx->pClient;
		
        if (bSuccess)
        {
            CLogger::GetInstance().Debug(_T("HTTP Response OK\n"));
            CLogger::GetInstance().Debug(strResponse);
            pCtx->pAiChat->ProcessToolResponse(strResponse);
        }
        else
        {
            CLogger::GetInstance().Debug(_T("HTTP Request failed. Status Code: %d\n"), dwStatusCode);
			pCtx->pAiChat->SendError();
        }

        pClient->Cleanup();   // 新增: 释放全局引用(与 Initialize 配对)
        delete pClient;
        delete pCtx;
    }
}

AiChat::AiChat(void):pAcadWnd(NULL),m_root(NULL), m_messages(NULL)
{
}

AiChat::~AiChat(void)
{
    if (m_root != NULL)
    {
        cJSON_Delete(m_root);
        m_root = NULL;
        m_messages = NULL;
    }
}
void AiChat::setAcadWnd(CWnd* pWnd)
{
	this->pAcadWnd=pWnd;
}
CWnd* AiChat::getAcadWnd()
{
	return this->pAcadWnd;
}
void AiChat::SendChatRequest(const std::string& jsonBody)
{
    CLogger::GetInstance().Debug(_T("[AI] Starting send request...\n"));
    CLogger::GetInstance().Debug(jsonBody);

    CString authorization;
    authorization.Format(_T("Bearer %s"), DASHSCOPE_API_KEY);
    CHttpClient* pClient = new CHttpClient();
    //pClient->Initialize();  
    pClient->SetHeader(_T("Content-Type"), _T("application/json"));
    pClient->SetHeader(_T("Authorization"), authorization);

    AsyncContext* pCtx = new AsyncContext;
    pCtx->pAiChat = this;
    pCtx->pClient = pClient;

    CLogger::GetInstance().Debug(_T("[AI] Calling PostAsync...\n"));
    
    pClient->PostAsync(
        WORK_URL,
        jsonBody,
        OnHttpResponse,
        (LPVOID)pCtx);
}
void AiChat::Execute(CString& message)
{
    // 首次调用时初始化 root 结构
    if (m_root == NULL)
    {
		CString strFilePath = CUtils::GetArxFolder();
		strFilePath.Replace(_T("\\"), _T("/"));
		CString strUrl;
		strUrl = strFilePath + _T("chat.bin");
		std::string chat_prefix = CUtils::DecryptFileWithPassword(strUrl.GetString(), CHAT_KEY);

        m_root = cJSON_CreateObject();
        cJSON_AddStringToObject(m_root, "model", "qwen3.5-flash");

        m_messages = cJSON_CreateArray();

        cJSON* msgSystem = cJSON_CreateObject();
        cJSON_AddStringToObject(msgSystem, "role", "system");
        cJSON_AddStringToObject(msgSystem, "content", chat_prefix.c_str());
        cJSON_AddItemToArray(m_messages, msgSystem);

        cJSON_AddItemToObject(m_root, "messages", m_messages);

        // 加载 tools.json（只需一次）
        strUrl = strFilePath + _T("tools.json");

        std::string filePathStr = std::string(CStringA(strUrl));

        std::ifstream file(filePathStr.c_str());
        std::string fileContent;
        if (file.is_open())
        {
            file.seekg(0, std::ios::end);
            std::streampos fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            fileContent.reserve(static_cast<size_t>(fileSize));
            fileContent.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
        }
        else
        {
            CLogger::GetInstance().Debug(_T("Failed to open tool.json: %s\n"), strUrl);
        }

        if (!fileContent.empty())
        {
            cJSON* toolsJson = cJSON_Parse(fileContent.c_str());
            if (toolsJson != NULL)
            {
                cJSON_AddItemToObject(m_root, "tools", toolsJson);
                // m_bToolsLoaded = true;
            }
            else
            {
                CLogger::GetInstance().Debug(_T("Failed to parse tool.json\n"));
            }
        }
        else
        {
            CLogger::GetInstance().Debug(_T("Failed to read tool.json or file is empty\n"));
        }
    }

    // 每次调用只追加一条 user message
    cJSON* msgUser = cJSON_CreateObject();
    cJSON_AddStringToObject(msgUser, "role", "user");
    std::string userContent = CUtils::CStringToUTF8(message);
    cJSON_AddStringToObject(msgUser, "content", userContent.c_str());
    cJSON_AddItemToArray(m_messages, msgUser);

    char* jsonStr = cJSON_PrintUnformatted(m_root);
    std::string jsonBody(jsonStr);
    free(jsonStr);

    SendChatRequest(jsonBody);
}
void AiChat::ProcessToolResponse(const std::string& strResponse)
{
    cJSON* responseJson = cJSON_Parse(strResponse.c_str());
    if (responseJson == NULL)
    {
        CLogger::GetInstance().Debug(_T("Failed to parse AI response JSON.\n"));
		SendError();
        return;
    }

    cJSON* choices = cJSON_GetObjectItem(responseJson, "choices");
    if (choices == NULL || !cJSON_IsArray(choices))
    {
        CLogger::GetInstance().Debug(_T("AI response missing choices array.\n"));
        cJSON_Delete(responseJson);
		SendError();
        return;
    }

    int choiceCount = cJSON_GetArraySize(choices);
    if (choiceCount > 0)
    {
        cJSON* choice = cJSON_GetArrayItem(choices, choiceCount - 1);
        if (choice != NULL)
        {
            // 选中这行对应的 message
            cJSON* message = cJSON_GetObjectItem(choice, "message");
            if (message != NULL && cJSON_IsObject(message))
            {
                bool isFinishReason = IsFinishReasonToolCalls(choice);
                // 处理 reasoning_content
                cJSON* reasoning_content = cJSON_GetObjectItem(message, "reasoning_content");
                if (reasoning_content && reasoning_content->valuestring)
                {
                    cJSON* content = cJSON_GetObjectItem(message, "content");

                    CWnd* pAcadWnd = getAcadWnd();
                    if (pAcadWnd)
                    {
                        CStringW strReasoning(CA2W(reasoning_content->valuestring, CP_UTF8));
                        
                        // 如果content的值不为空，则将reasoning_content的值加上"\n"再与content的值合并
                        if (content && content->valuestring && content->valuestring[0] != '\0')
                        {
                            CStringW strContent(CA2W(content->valuestring, CP_UTF8));
                            strReasoning += _T("\n") + strContent;
                        }
                        CString* pReasoning = new CString(strReasoning);
                        pAcadWnd->PostMessage(WM_USER_CHAT, isFinishReason ? 0 : 1, (LPARAM)pReasoning);
                    }
                }
                // 将 message 复制并追加到 m_messages 中
                cJSON* msgCopy = cJSON_Duplicate(message, 1);
                cJSON_AddItemToArray(m_messages, msgCopy);

                // 处理 tool_calls，将结果也追加到 m_messages
                cJSON* toolCalls = cJSON_GetObjectItem(message, "tool_calls");
                if (toolCalls != NULL && cJSON_IsArray(toolCalls))
                {
                    int toolCallCount = cJSON_GetArraySize(toolCalls);
                    time_t rawtime;
                    struct tm timeinfo;
                    time(&rawtime);
#ifdef _CAD2005
					struct tm* pTmp = localtime(&rawtime);
					if (pTmp != NULL)
						timeinfo = *pTmp;   // 从静态缓冲区拷贝到本地变量
#else
                    localtime_s(&timeinfo, &rawtime);
#endif
                    char timeBuffer[15];
                    strftime(timeBuffer, sizeof(timeBuffer), "%Y%m%d%H%M%S", &timeinfo);

                    for (int j = 0; j < toolCallCount; ++j)
                    {
                        cJSON* toolCall = cJSON_GetArrayItem(toolCalls, j);
                        cJSON* functionItem = cJSON_GetObjectItem(toolCall, "function");
                        if (functionItem == NULL || !cJSON_IsObject(functionItem))
                            continue;

                        cJSON* nameItem = cJSON_GetObjectItem(functionItem, "name");
                        cJSON* argumentsItem = cJSON_GetObjectItem(functionItem, "arguments");
                        if (nameItem == NULL || !cJSON_IsString(nameItem))
                            continue;

                        std::string argumentsJson;
                        if (!GetToolArgumentsJson(argumentsItem, argumentsJson))
                        {
                            CString toolName(nameItem->valuestring);
                            CLogger::GetInstance().Debug(_T("[AI TOOL] %s arguments invalid.\n"), toolName);
                            continue;
                        }

                        std::string result = CallLocalTool(nameItem->valuestring, argumentsJson);

                        // 优先使用响应中的 id，否则生成
                        const char* toolCallId = NULL;
                        cJSON* idItem = cJSON_GetObjectItem(toolCall, "id");
                        if (idItem != NULL && cJSON_IsString(idItem))
                        {
                            toolCallId = idItem->valuestring;
                        }

                        cJSON* toolMsg = cJSON_CreateObject();
                        cJSON_AddStringToObject(toolMsg, "role", "tool");
                        if (toolCallId != NULL)
                        {
                            cJSON_AddStringToObject(toolMsg, "tool_call_id", toolCallId);
                        }
                        else
                        {
                            char szToolCallId[32];
#ifdef _CAD2005
							_snprintf(szToolCallId, sizeof(szToolCallId) - 1, "call_%s_%03d", timeBuffer, j + 1);
							szToolCallId[sizeof(szToolCallId) - 1] = '\0';   // _snprintf 截断时不保证补 '\0'
#else
							sprintf_s(szToolCallId, sizeof(szToolCallId), "call_%s_%03d", timeBuffer, j + 1);
#endif
                            
                            cJSON_AddStringToObject(toolMsg, "tool_call_id", szToolCallId);
                        }

                        cJSON* contentJson = cJSON_Parse(result.c_str());
                        if (contentJson != NULL)
                        {
                            char* contentStr = cJSON_PrintUnformatted(contentJson);
                            cJSON_Delete(contentJson);
                            if (contentStr != NULL)
                            {
                                cJSON_AddStringToObject(toolMsg, "content", contentStr);
                                free(contentStr);
                            }
                            else
                            {
                                cJSON_AddStringToObject(toolMsg, "content", result.c_str());
                            }
                        }
                        else
                        {
                            cJSON_AddStringToObject(toolMsg, "content", result.c_str());
                        }

                        // 将 tool 结果直接追加到 m_messages
                        cJSON_AddItemToArray(m_messages, toolMsg);
                    }

                    if (toolCallCount > 0)
                    {
                        CallLocalTool("regen", "{}");
						acedPostCommandPrompt();
                    }
                    
                }
            }
        }
    }

    cJSON_Delete(responseJson);
}
void AiChat::ContinueChat()
{
    if (m_root == NULL)
        return;

    char* jsonStr = cJSON_PrintUnformatted(m_root);
    if (jsonStr != NULL)
    {
        std::string jsonBody(jsonStr);
        free(jsonStr);
        SendChatRequest(jsonBody);
    }
}
void AiChat::SendError()
{
	if (pAcadWnd){
		pAcadWnd->PostMessage(WM_USER_CHAT_SEND_ERROR,  0,  0);
	}
}