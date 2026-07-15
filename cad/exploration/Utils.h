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
};
