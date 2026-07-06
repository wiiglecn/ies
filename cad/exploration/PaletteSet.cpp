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

//Utility functions to add and get the child node from the parent XML-DOM node
extern BOOL GetChildNode(MSXML::IXMLDOMNode* pNode, 
						 LPCTSTR pszNodeName, 
						 MSXML::IXMLDOMNode** ppChild);

extern BOOL AddChildNode(MSXML::IXMLDOMNode* pNode, LPCTSTR pszChildNodeName,
						 short nNodeType, MSXML::IXMLDOMNode** ppChild);

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
	const TCHAR *pRoam;
	TCHAR paletteBuffer[MAX_PATH];
	Acad::ErrorStatus bResult = acdbHostApplicationServices()->getRoamableRootFolder(pRoam);
	if (bResult != Acad::eOk) {
		acutPrintf(_T("\n[DEBUG] getRoamableRootFolder: Failed to get Roamable Root Folder. Result: %d "), bResult);
		
		return;
	}
	
	_tcscpy(paletteBuffer, pRoam);
	_tcscat(paletteBuffer, KSPALETTESET_FILENAME);
	// Print the buffer
    acutPrintf(_T("\n[DEBUG] LoadPaletteSet: Target file path: %s"), paletteBuffer);

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
        acutPrintf(_T("\n[DEBUG] SavePaletteSet: Aborted. Invalid Host Application Services."));
        return;
    }
    acutPrintf(_T("\n[DEBUG] SavePaletteSet: Starting..."));

    if (!s_pJsonConfig) {
        acutPrintf(_T("\n[DEBUG] SavePaletteSet: Aborted. s_pJsonConfig is NULL."));
        return;
    }

    // Get the file where the palette set's properties are saved
    // const TCHAR *pRoam;
    
    TCHAR paletteBuffer[MAX_PATH];
    const TCHAR *pRoam;
    acutPrintf(_T("\n[DEBUG] SavePaletteSet: Getting Roamable Root Folder..."));
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
		acutPrintf(_T("\n[DEBUG] SavePaletteSet: Failed to get Roamable Root Folder."));
		return;
	}
    _tcscpy(paletteBuffer, pRoam);
    _tcscat(paletteBuffer, KSPALETTESET_FILENAME);
    acutPrintf(_T("\n[DEBUG] SavePaletteSet: Target file path: %s"), paletteBuffer);

    // Serialize JSON to string
    acutPrintf(_T("\n[DEBUG] SavePaletteSet: Serializing JSON..."));
    char* jsonString = cJSON_Print(s_pJsonConfig);
    if (!jsonString) {
        acutPrintf(_T("\n[DEBUG] SavePaletteSet: Failed to serialize JSON."));
        return;
    }

    // Write to file
    acutPrintf(_T("\n[DEBUG] SavePaletteSet: Opening file for writing..."));
    FILE* fp = _tfopen(paletteBuffer, _T("wb")); // Use binary mode for writing
    if (!fp) {
        acutPrintf(_T("\n[DEBUG] SavePaletteSet: Failed to open file for writing. Error: %d"), errno);
        cJSON_free(jsonString);
        return;
    }

    acutPrintf(_T("\n[DEBUG] SavePaletteSet: Writing content to file..."));
    size_t writeSize = fprintf(fp, "%s", jsonString);
    fclose(fp);
    
    if (writeSize < 0) {
        acutPrintf(_T("\n[DEBUG] SavePaletteSet: Error writing to file."));
    } else {
        acutPrintf(_T("\n[DEBUG] SavePaletteSet: Successfully wrote %d bytes."), writeSize);
    }

    // Free the serialized string
    cJSON_free(jsonString);
    acutPrintf(_T("\n[DEBUG] SavePaletteSet: Finished."));
}
// Static accessor implementation
cJSON* CMyPaletteSet::GetJsonConfig()
{
	if (!s_pJsonConfig) {
        s_pJsonConfig = cJSON_CreateObject();
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
