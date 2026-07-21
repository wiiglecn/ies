#include "stdafx.h"
#include <fstream>
#include <iterator>
#include "AiChat.h"
#include "HttpClient.h"
#include "cJSON.h"
#include "Utils.h"
#include "Logger.h"
#include "declare.h"
//#define WORK_URL _T("https://ws-93xgld532jdyrkjf.cn-beijing.maas.aliyuncs.com/compatible-mode/v1/chat/completions")
//#define WORK_URL _T("https://ws-93xgld532jdyrkjf.cn-beijing.maas.aliyuncs.com/compatible-mode/v1/chat/completions")

// test
#define WORK_URL _T("http://localhost:7101/compatible-mode/v1/chat/completions")
//#define WORK_URL _T("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions")

#define DASHSCOPE_API_KEY _T("sk-820adfc78edd47608679411d492c0c20")
#define LOCAL_TOOL_URL_PREFIX _T("http://localhost:7100/ai/tool/")

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

static void CallLocalTool(const char* functionName, const std::string& argumentsJson)
{
    if (functionName == NULL || functionName[0] == '\0')
        return;

    CString toolUrl(LOCAL_TOOL_URL_PREFIX);
    toolUrl += CString(functionName);

    CHttpClient toolClient;
    toolClient.SetHeader(_T("Content-Type"), _T("application/json"));

    std::string toolResponse;
    DWORD toolStatusCode = 0;
    BOOL bToolSuccess = toolClient.Post(toolUrl, argumentsJson, toolResponse, toolStatusCode);

    CString toolName(functionName);
    if (bToolSuccess)
    {
        CLogger::GetInstance().Debug(_T("[AI TOOL] %s status: %d\n"), toolName, toolStatusCode);
        CLogger::GetInstance().Debug(toolResponse);
    }
    else
    {
        CLogger::GetInstance().Debug(_T("[AI TOOL] %s request failed. Status Code: %d\n"), toolName, toolStatusCode);
    }
}

static void HandleToolCallsResponse(const std::string& strResponse,AiChat* pChat)
{
    cJSON* responseJson = cJSON_Parse(strResponse.c_str());
    if (responseJson == NULL)
    {
        CLogger::GetInstance().Debug(_T("Failed to parse AI response JSON.\n"));
        return;
    }

    cJSON* choices = cJSON_GetObjectItem(responseJson, "choices");
    if (choices == NULL || !cJSON_IsArray(choices))
    {
        CLogger::GetInstance().Debug(_T("AI response missing choices array.\n"));
        cJSON_Delete(responseJson);
        return;
    }

    int choiceCount = cJSON_GetArraySize(choices);
    for (int i = 0; i < choiceCount; ++i)
    {
        cJSON* choice = cJSON_GetArrayItem(choices, i);
        if (choice == NULL || !IsFinishReasonToolCalls(choice))
            continue;

        cJSON* message = cJSON_GetObjectItem(choice, "message");
        if (message == NULL || !cJSON_IsObject(message))
            continue;

		cJSON* reasoning_content = cJSON_GetObjectItem(choice, "reasoning_content");
		if (reasoning_content && reasoning_content->valuestring){
			CWnd* pAcadWnd = pChat->getAcadWnd();
            if (pAcadWnd){
                CStringW strReasoning(CA2W(reasoning_content->valuestring, CP_UTF8));
                CString* pReasoning = new CString(strReasoning);
                pAcadWnd->PostMessage(WM_USER_CHAT, 0, (LPARAM)pReasoning);
            }
        }
        cJSON* toolCalls = cJSON_GetObjectItem(message, "tool_calls");
        if (toolCalls == NULL || !cJSON_IsArray(toolCalls))
            continue;

        int toolCallCount = cJSON_GetArraySize(toolCalls);
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

            CallLocalTool(nameItem->valuestring, argumentsJson);
        }
        if (toolCallCount > 0){
            CallLocalTool("regen", "{}");
        }
    }

    cJSON_Delete(responseJson);
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
            HandleToolCallsResponse(strResponse,pCtx->pAiChat);
        }
        else
        {
            CLogger::GetInstance().Debug(_T("HTTP Request failed. Status Code: %d\n"), dwStatusCode);
        }

        delete pClient;
        delete pCtx;
    }
}

AiChat::AiChat(void):pAcadWnd(NULL)
{
}

AiChat::~AiChat(void)
{
}
void AiChat::setAcadWnd(CWnd* pWnd)
{
	this->pAcadWnd=pWnd;
}
CWnd* AiChat::getAcadWnd()
{
	return this->pAcadWnd;
}
void AiChat::Execute(CString& message)
{

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "qwen3.5-plus");

    cJSON* messages = cJSON_CreateArray();
    
    cJSON* msgSystem = cJSON_CreateObject();
    cJSON_AddStringToObject(msgSystem, "role", "system");
    cJSON_AddStringToObject(msgSystem, "content", "You are a autocad2007 assistant.");
    cJSON_AddItemToArray(messages, msgSystem);

    cJSON* msgUser = cJSON_CreateObject();
    cJSON_AddStringToObject(msgUser, "role", "user");
    std::string userContent = CUtils::CStringToUTF8(message);
    cJSON_AddStringToObject(msgUser, "content", userContent.c_str());
    cJSON_AddItemToArray(messages, msgUser);

    cJSON_AddItemToObject(root, "messages", messages);

    CString strFilePath = CUtils::GetArxFolder();
    strFilePath.Replace(_T("\\"), _T("/"));
    CString strUrl = strFilePath + _T("tools.json");

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
            cJSON_AddItemToObject(root, "tools", toolsJson);
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

    char* jsonStr = cJSON_PrintUnformatted(root);
    std::string jsonBody(jsonStr);
    free(jsonStr);
    cJSON_Delete(root);

    CLogger::GetInstance().Debug(jsonBody.c_str());

    CString authorization;
    authorization.Format(_T("Bearer %s"), DASHSCOPE_API_KEY);
    CHttpClient* pClient = new CHttpClient;
    pClient->SetHeader(_T("Content-Type"), _T("application/json"));
    pClient->SetHeader(_T("Authorization"), authorization);

    AsyncContext* pCtx = new AsyncContext;
    pCtx->pAiChat = this;
    pCtx->pClient = pClient;

    pClient->PostAsync(
        WORK_URL,
        jsonBody,
        OnHttpResponse,
        (LPVOID)pCtx);
}
