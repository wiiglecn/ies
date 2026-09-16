#include "stdafx.h"
#include "ToolGlobal.h"
#include <queue>
#include "cJSON.h"
#include "arxHeaders.h"
#include "utils.h"

#include "../features/CreateColumnView.h"

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
#ifdef _CAD2005
				_stprintf(argStr, _T("%f"), argItem->valuedouble);
#else
				_stprintf_s(argStr, _T("%f"), argItem->valuedouble);
#endif
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
void ToolGlobal::get_drawing_units(AiToolCommandData* data) {
    if (actionBefore() != 0) {
        return;
    }

    cJSON* pJson = cJSON_CreateObject();

    // 1. Get INSUNITS system variable (insertion/drawing units)
    struct resbuf rbInsUnits;
    if (acedGetVar(_T("INSUNITS"), &rbInsUnits) == RTNORM) {
        int insUnits = rbInsUnits.resval.rint;
        cJSON_AddNumberToObject(pJson, "insunits", insUnits);

        const char* unitName = "Unknown";
        switch (insUnits) {
            case 0:  unitName = "Unspecified"; break;
            case 1:  unitName = "Inches"; break;
            case 2:  unitName = "Feet"; break;
            case 3:  unitName = "Miles"; break;
            case 4:  unitName = "Millimeters"; break;
            case 5:  unitName = "Centimeters"; break;
            case 6:  unitName = "Meters"; break;
            case 7:  unitName = "Kilometers"; break;
            case 8:  unitName = "Microinches"; break;
            case 9:  unitName = "Mils"; break;
            case 10: unitName = "Yards"; break;
            case 11: unitName = "Angstroms"; break;
            case 12: unitName = "Nanometers"; break;
            case 13: unitName = "Microns"; break;
            case 14: unitName = "Decimeters"; break;
            case 15: unitName = "Decameters"; break;
            case 16: unitName = "Hectometers"; break;
            case 17: unitName = "Gigameters"; break;
            case 18: unitName = "Astronomical units"; break;
            case 19: unitName = "Light years"; break;
            case 20: unitName = "Parsecs"; break;
            default: break;
        }
        cJSON_AddStringToObject(pJson, "unit_name", unitName);
    }

    // 2. Get MEASUREMENT system variable (0=Imperial, 1=Metric)
    struct resbuf rbMeasurement;
    if (acedGetVar(_T("MEASUREMENT"), &rbMeasurement) == RTNORM) {
        int measurement = rbMeasurement.resval.rint;
        cJSON_AddNumberToObject(pJson, "measurement", measurement);
        cJSON_AddStringToObject(pJson, "measurement_system", (measurement == 0) ? "Imperial" : "Metric");
    }

    // 3. Get LUNITS system variable (linear units display format)
    struct resbuf rbLunits;
    if (acedGetVar(_T("LUNITS"), &rbLunits) == RTNORM) {
        int lunits = rbLunits.resval.rint;
        cJSON_AddNumberToObject(pJson, "lunits", lunits);

        const char* lunitsName = "Unknown";
        switch (lunits) {
            case 1: lunitsName = "Scientific"; break;
            case 2: lunitsName = "Decimal"; break;
            case 3: lunitsName = "Engineering"; break;
            case 4: lunitsName = "Architectural"; break;
            case 5: lunitsName = "Fractional"; break;
            default: break;
        }
        cJSON_AddStringToObject(pJson, "lunits_name", lunitsName);
    }

    // 4. Get LUPREC system variable (linear units precision)
    struct resbuf rbLuprec;
    if (acedGetVar(_T("LUPREC"), &rbLuprec) == RTNORM) {
        int luprec = rbLuprec.resval.rint;
        cJSON_AddNumberToObject(pJson, "luprec", luprec);
    }

    data->resultJson = pJson;
    data->ret = 0;
    actionEnd();
}
void ToolGlobal::CreateColumnView()
{
	CCreateColumnView view;
	view.Execute();
}