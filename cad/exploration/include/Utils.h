#pragma once
#include "dbents.h"
#include "gedll.h"
#include "gepnt3d.h"
#include <string>

class CUtils
{
public:
	CUtils(void);
public:
	~CUtils(void);
	static bool IsCircleExisting(const AcGePoint3d& center);
	static void FocusAcadDrawing();
	static CString GetArxFolder();
	static std::wstring StringToWString(const std::string& str);
	static bool utf8ToTChar(const char* utf8, TCHAR* outBuf, int bufLen);
	static std::string CStringToUTF8(const CString& str);
	static void TCharToUtf8(const TCHAR* tStr, char* utf8Str, int utf8Size);
	static std::string DecryptFileWithPassword(const std::wstring& filePath, const std::string& password);
};
