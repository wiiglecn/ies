// coding: gb18030
// (C) Copyright 2000-2006 by Autodesk, Inc. 
//
// Permission to use, copy, modify, and distribute this software in
// object code form for any purpose and without fee is hereby granted, 
// provided that the above copyright notice appears in all copies and 
// that both that copyright notice and the limited warranty and
// restricted rights notice below appear in all supporting 
// documentation.
//
// AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS. 
// AUTODESK SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTY OF
// MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE.  AUTODESK, INC. 
// DOES NOT WARRANT THAT THE OPERATION OF THE PROGRAM WILL BE
// UNINTERRUPTED OR ERROR FREE.
//
// Use, duplication, or disclosure by the U.S. Government is subject to 
// restrictions set forth in FAR 52.227-19 (Commercial Computer
// Software - Restricted Rights) and DFAR 252.227-7013(c)(1)(ii)
// (Rights in Technical Data and Computer Software), as applicable.
//
//


//-----------------------------------------------------------------------------
#include "StdAfx.h"

#if defined(_DEBUG) && !defined(AC_FULL_DEBUG)
#error _DEBUG should not be defined except in internal Adesk debug builds
#endif



#include "PaletteSet.h"
#include "resource.h"
#include "CPalette.h"
#include "Utils.h"


// The file name of the persisted paletteset
#define PALETTESET_FILENAME _T("TestPaletteSet.xml")
#define KSPALETTESET_FILENAME _T("KsPaletteSet.json")

//-----------------------------------------------------------------------------
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Initialize static member
cJSON* CMyPaletteSet::s_pJsonConfig = NULL;



//-----------------------------------------------------------------------------
IMPLEMENT_DYNCREATE(CMyPaletteSet, CAdUiPaletteSet)

BEGIN_MESSAGE_MAP(CMyPaletteSet, CAdUiPaletteSet)
	ON_WM_CREATE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------

CMyPaletteSet::CMyPaletteSet()
{
	
}

// Called by the palette set framework to determine size constraints.
// Override these methods to provide minimum and maximum palette set sizes.
void CMyPaletteSet::GetMinimumSize(CSize& size)
{
	size.cx = 300;
	size.cy = 400;
}

void CMyPaletteSet::GetMaximumSize(CSize& size)
{
	size.cx = 400;
	size.cy = 400;
}

BOOL CMyPaletteSet::PreTranslateMessage(MSG *pMsg)
{
	if (pMsg->message == WM_CHAR || pMsg->message == WM_SYSCHAR )
		return CWnd::PreTranslateMessage(pMsg);
	else 
		return CAdUiPaletteSet::PreTranslateMessage(pMsg);
}

int CMyPaletteSet::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CAdUiPaletteSet::OnCreate(lpCreateStruct) == -1)
		return -1;
	return 0;
}

void CMyPaletteSet::OnDestroy()
{
	// Save PaletteSet data
	// SavePaletteSet();
	FreeJsonConfig();
	CAdUiPaletteSet::OnDestroy();
}


void CMyPaletteSet::LoadPaletteSet()
{
	// Free previous config if exists
	if (s_pJsonConfig) {
		cJSON_Delete(s_pJsonConfig);
		s_pJsonConfig = NULL;
	}

	// Get the file where the palette set's properties are saved
	const ACHAR *pRoam;
	TCHAR paletteBuffer[MAX_PATH];
	Acad::ErrorStatus bResult = acdbHostApplicationServices()->getRoamableRootFolder(pRoam);
	if (bResult != Acad::eOk) {
		CUtils::acutPrintf(_T("[DEBUG] getRoamableRootFolder: Failed to get Roamable Root Folder. Result: %d \n"), bResult);
		
		return;
	}
#ifdef _CAD2005
	MultiByteToWideChar(CP_ACP, 0, pRoam, -1, paletteBuffer, MAX_PATH);
#else
	_tcscpy(paletteBuffer, pRoam);
#endif
	_tcscat(paletteBuffer, KSPALETTESET_FILENAME);
	// Print the buffer
    CUtils::acutPrintf(_T("[DEBUG] LoadPaletteSet: Target file path: %s \n"), paletteBuffer);

	// Read file content
	FILE* fp = _tfopen(paletteBuffer, _T("rb")); // Use binary mode for reading
	if (!fp) {
		// File does not exist or cannot be opened
		return;
	}

	// Seek to end to get file size
	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (fileSize <= 0) {
		fclose(fp);
		return;
	}

	// Allocate buffer and read file
	char* fileContent = (char*)malloc(fileSize + 1);
	if (!fileContent) {
		fclose(fp);
		return;
	}

	size_t readSize = fread(fileContent, 1, fileSize, fp);
	fileContent[readSize] = '\0';
	fclose(fp);

	// Parse JSON
	s_pJsonConfig = cJSON_Parse(fileContent);
	free(fileContent);

	if (!s_pJsonConfig) {
		// Parse error
		const char* errorPtr = cJSON_GetErrorPtr();
		if (errorPtr != NULL) {
			// Handle error, e.g., log it
			// acutPrintf(_T("\nJSON Error before: %s\n"), errorPtr);
		}
	}
}


void CMyPaletteSet::SavePaletteSet()
{
	 // 1. 检查是否在有效的 AutoCAD 会话/线程上下文中
    // acdbHostApplicationServices() 应该在主线程且 AutoCAD 运行时有效
    if (!acdbHostApplicationServices()) {
        CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Aborted. Invalid Host Application Services. \n"));
        return;
    }
    CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Starting... \n"));

    if (!s_pJsonConfig) {
        CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Aborted. s_pJsonConfig is NULL. \n"));
        return;
    }

    // Get the file where the palette set's properties are saved
    // const TCHAR *pRoam;
    
    TCHAR paletteBuffer[MAX_PATH];
    const ACHAR *pRoam;
    CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Getting Roamable Root Folder... \n"));
    // 直接接收返回的指针，不需要传入参数
	// 调用 API，传入缓冲区和大小
    // 注意：请确认你的 ObjectARX 版本中该函数的具体签名。
    // 如果是 BOOL getRoamableRootFolder(TCHAR*, int)，则如下调用：
    Acad::ErrorStatus bResult = acdbHostApplicationServices()->getRoamableRootFolder(pRoam);
    
    // 如果 API 签名是 BOOL getRoamableRootFolder(CString&)，则需要使用 CString
    // CString strRoam;
    // BOOL bResult = acdbHostApplicationServices()->getRoamableRootFolder(strRoam);
    // if (bResult) _tcscpy(paletteBuffer, strRoam);

    if (bResult != Acad::eOk) {
		// 处理错误
		CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Failed to get Roamable Root Folder. \n"));
		return;
	}
#ifdef _CAD2005
	MultiByteToWideChar(CP_ACP, 0, pRoam, -1, paletteBuffer, MAX_PATH);
#else
	_tcscpy(paletteBuffer, pRoam);
#endif
    _tcscat(paletteBuffer, KSPALETTESET_FILENAME);
    CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Target file path: %s \n"), paletteBuffer);

    // Serialize JSON to string
    CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Serializing JSON... \n"));
    char* jsonString = cJSON_Print(s_pJsonConfig);
    if (!jsonString) {
        CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Failed to serialize JSON. \n"));
        return;
    }

    // Write to file
    CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Opening file for writing... \n"));
    FILE* fp = _tfopen(paletteBuffer, _T("wb")); // Use binary mode for writing
    if (!fp) {
        CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Failed to open file for writing. Error: %d \n"), errno);
        cJSON_free(jsonString);
        return;
    }

    CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Writing content to file... \n"));
    size_t writeSize = fprintf(fp, "%s", jsonString);
    fclose(fp);
    
    if (writeSize < 0) {
        CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Error writing to file. \n"));
    } else {
        CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Successfully wrote %d bytes. \n"), writeSize);
    }

    // Free the serialized string
    cJSON_free(jsonString);
    CUtils::acutPrintf(_T("[DEBUG] SavePaletteSet: Finished. \n"));
}
// Static accessor implementation
cJSON* CMyPaletteSet::GetJsonConfig()
{
    // 1. Initialize if NULL
    if (!s_pJsonConfig) {
        s_pJsonConfig = cJSON_CreateObject();
    }

    // 2. Check if 'profile' object exists, if not, create it and set defaults
    cJSON* profile = cJSON_GetObjectItem(s_pJsonConfig, "profile");
    
    if (!profile || !cJSON_IsObject(profile)) {
        // Create the profile object
        profile = cJSON_AddObjectToObject(s_pJsonConfig, "profile");
        
        // Add default values
        // Note: cJSON_AddStringToObject creates a copy of the string
        cJSON_AddStringToObject(profile, "hole_diameter", "1");
    } else {
        // 3. Optional: Check if specific keys inside 'profile' are missing
        // If you want to ensure hole_diameter exists even if 'profile' exists but is empty
        cJSON* holeDiameter = cJSON_GetObjectItem(profile, "hole_diameter");
        if (!holeDiameter || !cJSON_IsString(holeDiameter)) {
            cJSON_AddStringToObject(profile, "hole_diameter", "1");
        }
    }

    return s_pJsonConfig;
}

void CMyPaletteSet::FreeJsonConfig()
{
	if (s_pJsonConfig) {
		cJSON_Delete(s_pJsonConfig);
		s_pJsonConfig = NULL;
	}
}
