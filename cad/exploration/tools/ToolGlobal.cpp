#include "stdafx.h"
#include "ToolGlobal.h"
#include <queue>
#include "cJSON.h"
#include "arxHeaders.h"
#include "utils.h"


// Global FIFO queue
static std::queue<AiToolCommandData*> g_cmdQueue;


// Synchronization Event
//HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);


ToolGlobal::ToolGlobal(void):ToolDrawBase()
{
}

ToolGlobal::~ToolGlobal(void)
{
}
ToolGlobal& ToolGlobal::getInstance()
{
    static ToolGlobal instance;
    return instance;
}
void ToolGlobal::dispatch_command(AiToolCommandData* data){
	acutPrintf(_T("dispatch_command \n"));
	// 1. Add to global FIFO queue
	g_cmdQueue.push(data);

	// 2. Send string to execute
	AcApDocument* pDoc = acDocManager->curDocument();
	if (pDoc) {
		//ResetEvent(hEvent);
		//acutPrintf(_T("SLCMD run \n"));
		acDocManager->sendStringToExecute(pDoc, _T("SLCMD\n"));

		//DWORD waitResult = WaitForSingleObject(hEvent, 5000);
		//if (waitResult != WAIT_OBJECT_0) {
		//	if (data->resultJson == NULL) {
		//		data->resultJson = cJSON_CreateObject();
		//		cJSON_AddStringToObject(data->resultJson, 
		//			"message", "Execution timeout!");
		//	}
		//}else{
		//	acutPrintf(_T("SLCMD time out \n"));
		//}
	}
}

void ToolGlobal::CMD()
{
	//acutPrintf(_T("SLCMD call \n"));
	if (g_cmdQueue.empty()) {
		//SetEvent(hEvent);
		return;
	}
	
	// 4. Pop from queue
	AiToolCommandData* data = g_cmdQueue.front();
	g_cmdQueue.pop();


	// Initialize resultJson if it is null
	if (data->resultJson == NULL) {
		data->resultJson = cJSON_CreateObject();
	}

	cJSON* cmdItem = cJSON_GetObjectItem(data->jsonRoot, "cmd");
	cJSON* argsItem = cJSON_GetObjectItem(data->jsonRoot, "args");

	if (!cmdItem || !cJSON_IsString(cmdItem)) {
		cJSON_AddStringToObject(data->resultJson, "message", "Missing or invalid 'cmd' parameter.");
		SetEvent(data->hEvent);
		return;
	}

	TCHAR cmdStr[256] = { 0 };
	if (!CUtils::utf8ToTChar(cmdItem->valuestring, cmdStr, ARRAYSIZE(cmdStr))) {
		cJSON_AddStringToObject(data->resultJson, "message", "Failed to convert cmd string.");
		SetEvent(data->hEvent);
		return;
	}

	// Build resbuf
	struct resbuf* rbCmd = acutBuildList(RTSTR, cmdStr, RTNONE);
	struct resbuf* rbTail = rbCmd;

	if (argsItem && cJSON_IsArray(argsItem)) {
		int numArgs = cJSON_GetArraySize(argsItem);
		for (int i = 0; i < numArgs; ++i) {
			cJSON* argItem = cJSON_GetArrayItem(argsItem, i);
			TCHAR argStr[256] = { 0 };

			if (argItem && cJSON_IsString(argItem)) {
				CUtils::utf8ToTChar(argItem->valuestring, argStr, ARRAYSIZE(argStr));
			} else if (argItem && cJSON_IsNumber(argItem)) {
				_stprintf_s(argStr, _T("%f"), argItem->valuedouble);
			} else {
				continue;
			}

			rbTail->rbnext = acutBuildList(RTSTR, argStr, RTNONE);
			rbTail = rbTail->rbnext;
		}
	}

	// 5. Execute using acedCmd and check if successful
	if (rbCmd) {
		int ret = acedCmd(rbCmd);
		acutPrintf(_T("\n--- cmd ret :%d ---\n"),ret);
		if (ret == RTNORM) {
            if (data){
                data->ret = 0;
                // Release the old JSON object to prevent memory leaks
                if (data->resultJson != NULL) {
                    cJSON_Delete(data->resultJson);
                }
                data->resultJson = NULL;
            }
		} else {
			cJSON_AddNumberToObject(data->resultJson, "code", data->ret);
		}
		acutRelRb(rbCmd);
	}
	
	// 6. Signal event to unblock dispatch_command
	SetEvent(data->hEvent);
}