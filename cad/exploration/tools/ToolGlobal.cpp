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

// ---------------------------------------------------------------------------
// open_document: opens a drawing file (DWG/DXF) in the application context.
//   request : {"filePath":"D:/dir/a.dwg"} or {"path":"D:/dir","fileName":"a.dwg"}
//   response: {"status":"success","data":{"filePath":...,"fileName":...,
//              "alreadyOpen":false,"documentCount":2}}
//   note    : AcApDocManager::appContextOpenDocument() may only be called from
//             the application context, so this handler is dispatched to the CAD
//             main thread through WM_USER_A_OPEN_DOCUMENT.
// ---------------------------------------------------------------------------
static bool KsGetJsonText(cJSON* pRoot, const char* pszKey, std::string& strOut)
{
    if (pRoot == NULL || pszKey == NULL)
        return false;

    cJSON* pItem = cJSON_GetObjectItem(pRoot, pszKey);
    if (pItem == NULL || !cJSON_IsString(pItem) || pItem->valuestring == NULL)
        return false;
    if (pItem->valuestring[0] == '\0')
        return false;

    strOut = pItem->valuestring;
    return true;
}

// Case-insensitive path compare with unified separators.
static bool KsIsSameFilePath(const CString& strLeftIn, const CString& strRightIn)
{
    CString strLeft(strLeftIn);
    CString strRight(strRightIn);
    strLeft.Replace(_T('/'), _T('\\'));
    strRight.Replace(_T('/'), _T('\\'));
    return strLeft.CompareNoCase(strRight) == 0;
}

static CString KsOpenDocumentStatusText(Acad::ErrorStatus es)
{
    switch (es) {
    case Acad::eFileNotFound:
        return CString(_T("file not found"));
    case Acad::eFileAccessErr:
        return CString(_T("file is locked or access denied"));
    case Acad::eInvalidDwgVersion:
        return CString(_T("unsupported drawing version"));
    default:
        break;
    }

    CString strText;
    strText.Format(_T("open failed (Acad::ErrorStatus = %d)"), (int)es);
    return strText;
}

// Fill the failure result; HandleAiTool forwards resultJson.message to the reply.
static void KsSetOpenDocumentError(AiToolCommandData* pData, const CString& strMessage, int nCode)
{
    if (pData == NULL)
        return;

    cJSON* pResult = cJSON_CreateObject();
    if (pResult != NULL) {
        std::string strUtf8 = CUtils::CStringToUTF8(strMessage);
        cJSON_AddStringToObject(pResult, "message", strUtf8.c_str());
        cJSON_AddNumberToObject(pResult, "code", nCode);
        pData->resultJson = pResult;
    }

    pData->ret = 1;
    CUtils::acutPrintf(_T("[7100] open_document error(%d): %s\n"), nCode, strMessage.GetString());
}

void ToolGlobal::open_document(AiToolCommandData* data)
{
    if (data == NULL || data->jsonRoot == NULL) {
        if (data != NULL)
            data->ret = 1;
        return;
    }

    // 1. Collect parameters: filePath first, otherwise path + fileName.
    std::string strFilePath;
    if (!KsGetJsonText(data->jsonRoot, "filePath", strFilePath))
        KsGetJsonText(data->jsonRoot, "file_path", strFilePath);

    std::string strFileName;
    if (!KsGetJsonText(data->jsonRoot, "fileName", strFileName))
        KsGetJsonText(data->jsonRoot, "file_name", strFileName);

    std::string strFolder;
    if (!KsGetJsonText(data->jsonRoot, "path", strFolder))
        KsGetJsonText(data->jsonRoot, "folder", strFolder);

    if (strFilePath.empty()) {
        if (strFileName.empty()) {
            KsSetOpenDocumentError(data, CString(_T("missing parameter: fileName (or filePath)")), -1);
            return;
        }

        strFilePath = strFolder;
        if (!strFilePath.empty()) {
            char cLast = strFilePath[strFilePath.size() - 1];
            if (cLast != '\\' && cLast != '/')
                strFilePath += '\\';
        }
        strFilePath += strFileName;
    }

    // 2. Convert UTF-8 to TCHAR and resolve the absolute path.
    TCHAR szPath[MAX_PATH * 2] = { 0 };
    if (!CUtils::utf8ToTChar(strFilePath.c_str(), szPath, ARRAYSIZE(szPath))) {
        KsSetOpenDocumentError(data, CString(_T("path is too long or contains invalid characters")), -2);
        return;
    }

    TCHAR szFullPath[MAX_PATH * 2] = { 0 };
    DWORD dwFullLen = ::GetFullPathName(szPath, ARRAYSIZE(szFullPath), szFullPath, NULL);

    CString strFullPath;
    if (dwFullLen > 0 && dwFullLen < ARRAYSIZE(szFullPath))
        strFullPath = szFullPath;
    else
        strFullPath = szPath;

    // 3. Validate: the file must exist and be a DWG/DXF drawing.
    DWORD dwAttr = ::GetFileAttributes(strFullPath);
    if (dwAttr == INVALID_FILE_ATTRIBUTES) {
        KsSetOpenDocumentError(data, CString(_T("file not found: ")) + strFullPath, -3);
        return;
    }
    if ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        KsSetOpenDocumentError(data, CString(_T("path is a directory, not a drawing file: ")) + strFullPath, -4);
        return;
    }

    int nDot = strFullPath.ReverseFind(_T('.'));
    int nSep = strFullPath.ReverseFind(_T('\\'));
    if (nSep < 0)
        nSep = strFullPath.ReverseFind(_T('/'));

    CString strExt;
    if (nDot > nSep)
        strExt = strFullPath.Mid(nDot);
    if (strExt.CompareNoCase(_T(".dwg")) != 0 && strExt.CompareNoCase(_T(".dxf")) != 0) {
        KsSetOpenDocumentError(data, CString(_T("only .dwg / .dxf files can be opened: ")) + strFullPath, -5);
        return;
    }

    // 4. appContextOpenDocument() only works in the application context.
    if (acDocManager == NULL) {
        KsSetOpenDocumentError(data, CString(_T("document manager is not available")), -6);
        return;
    }
    if (!acDocManager->isApplicationContext()) {
        KsSetOpenDocumentError(data, CString(_T("AutoCAD is busy (a command or dialog is running), please retry later")), -7);
        return;
    }

    // 5. Activate the document directly if it is already open.
    AcApDocument* pExistingDoc = NULL;
    AcApDocumentIterator* pIter = acDocManager->newAcApDocumentIterator();
    if (pIter != NULL) {
        for (; !pIter->done(); pIter->step()) {
            AcApDocument* pDoc = pIter->document();
            if (pDoc == NULL || pDoc->fileName() == NULL)
                continue;
            if (KsIsSameFilePath(CString(pDoc->fileName()), strFullPath)) {
                pExistingDoc = pDoc;
                break;
            }
        }
        delete pIter;
    }

    bool bAlreadyOpen = false;
    if (pExistingDoc != NULL) {
        acDocManager->activateDocument(pExistingDoc);
        bAlreadyOpen = true;
    } else {
        Acad::ErrorStatus es = acDocManager->appContextOpenDocument(strFullPath);
        if (es != Acad::eOk) {
            KsSetOpenDocumentError(data, KsOpenDocumentStatusText(es) + CString(_T(": ")) + strFullPath, (int)es);
            return;
        }
    }

    // 6. Build the response.
    CString strFileNameOut(strFullPath);
    int nNamePos = strFullPath.ReverseFind(_T('\\'));
    if (nNamePos >= 0)
        strFileNameOut = strFullPath.Mid(nNamePos + 1);

    cJSON* pResult = cJSON_CreateObject();
    if (pResult != NULL) {
        std::string strUtf8Path = CUtils::CStringToUTF8(strFullPath);
        std::string strUtf8Name = CUtils::CStringToUTF8(strFileNameOut);
        cJSON_AddStringToObject(pResult, "filePath", strUtf8Path.c_str());
        cJSON_AddStringToObject(pResult, "fileName", strUtf8Name.c_str());
        cJSON_AddBoolToObject(pResult, "alreadyOpen", bAlreadyOpen ? 1 : 0);
        cJSON_AddNumberToObject(pResult, "documentCount", acDocManager->documentCount());
        data->resultJson = pResult;
    }

    data->ret = 0;
    CUtils::acutPrintf(_T("[7100] open_document ok: %s\n"), strFullPath.GetString());
}