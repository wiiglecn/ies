#include "stdafx.h"
#include <fstream>
#include <iterator> // For std::istreambuf_iterator
#include "AiChat.h"
#include "HttpClient.h"
#include "cJSON.h"
#include "Utils.h"
#include "Logger.h"

#define WORK_URL _T("https://ws-93xgld532jdyrkjf.cn-beijing.maas.aliyuncs.com/compatible-mode/v1/chat/completions")
#define DASHSCOPE_API_KEY _T("sk-820adfc78edd47608679411d492c0c20")

void OnHttpResponse(BOOL bSuccess, const std::string& strResponse,
                    DWORD dwStatusCode, LPVOID pUserData)
{
    // Cast pUserData back to AiChat*
    AiChat* pAiChat = (AiChat*)pUserData;
    if (pAiChat != NULL)
    {
        if (bSuccess)
        {
            // Convert std::string to CString for acutPrintf
            CString strResp(strResponse.c_str());
            acutPrintf(_T("HTTP Response OK\n"));

			CLogger::GetInstance().Debug(strResp);

        }
        else
        {
            acutPrintf(_T("HTTP Request failed. Status Code: %d\n"), dwStatusCode);
        }

        /*std::string* pCopy = new std::string(strResponse);
        ::PostMessage(hWnd, WM_HTTP_RESPONSE,
            (WPARAM)(bSuccess ? 1 : 0), (LPARAM)pCopy);*/
    }
}

AiChat::AiChat(void)
{
}

AiChat::~AiChat(void)
{
}
void AiChat::Execute(CString& message)
{

	acutPrintf(_T("AiChat::Execute :%s.\n"),message);

	


	

     // 1. Create Root Object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "qwen3.5-plus");

    // 2. Create Messages Array
    cJSON* messages = cJSON_CreateArray();
    
    // System Message
    cJSON* msgSystem = cJSON_CreateObject();
    cJSON_AddStringToObject(msgSystem, "role", "system");
    cJSON_AddStringToObject(msgSystem, "content", "You are a autocad2007 assistant.");
    cJSON_AddItemToArray(messages, msgSystem);

    // User Message
    cJSON* msgUser = cJSON_CreateObject();
    cJSON_AddStringToObject(msgUser, "role", "user");
    // Convert CString to std::string then to char* for cJSON
    std::string userContent = std::string(CStringA(message));
    cJSON_AddStringToObject(msgUser, "content", userContent.c_str());
    cJSON_AddItemToArray(messages, msgUser);

    cJSON_AddItemToObject(root, "messages", messages);

    CString strFilePath = CUtils::GetArxFolder();
    strFilePath.Replace(_T("\\"), _T("/"));
    CString strUrl = strFilePath + _T("tools.json");

    // Convert CString path to std::string for file operations
    std::string filePathStr = std::string(CStringA(strUrl));
    
    // Read file content (VC8 compatible)
    std::ifstream file(filePathStr.c_str());
    std::string fileContent;
    if (file.is_open())
    {
        // Get length of file
        file.seekg(0, std::ios::end);
        std::streampos fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Reserve space
        fileContent.reserve(static_cast<size_t>(fileSize));

        // Read into string
        fileContent.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
    }
    else
    {
        acutPrintf(_T("Failed to open tool.json: %s\n"), strUrl);
    }

    if (!fileContent.empty())
    {
        // Parse the JSON content from file
        cJSON* toolsJson = cJSON_Parse(fileContent.c_str());
        if (toolsJson != NULL)
        {
            // Add the parsed tools array/object to the root
            cJSON_AddItemToObject(root, "tools", toolsJson);
            // Note: cJSON_AddItemToObject steals the reference, so do NOT delete toolsJson here.
        }
        else
        {
            acutPrintf(_T("Failed to parse tool.json\n"));
        }
    }
    else
    {
        acutPrintf(_T("Failed to read tool.json or file is empty\n"));
    }

    // 4. Convert Root Object to String
    char* jsonStr = cJSON_PrintUnformatted(root);
	CLogger::GetInstance().Debug(CString(jsonStr));

    std::string jsonBody(jsonStr);
    free(jsonStr); // Free memory allocated by cJSON_PrintUnformatted
    cJSON_Delete(root); // Free the entire JSON tree

	CString authorization;
    authorization.Format(_T("Bearer %s"), DASHSCOPE_API_KEY);
	CHttpClient* pClient = new CHttpClient;
    pClient->SetHeader(_T("Accept"), _T("application/json"));
	pClient->SetHeader(_T("Authorization"), authorization);

    pClient->PostAsync(
        WORK_URL,
		jsonBody,
        OnHttpResponse,
        (LPVOID)this);

}
