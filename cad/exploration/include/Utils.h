#pragma once

#include "dbents.h"
#include "gedll.h"
#include "gepnt3d.h"
#include <string>
#include "declare.h"


#ifdef _CAD2005
    typedef std::string KString;
#else
    typedef std::wstring KString;
#endif


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
	static std::string DecryptFileWithPassword(const KString& filePath, const std::string& password);
	static Acad::ErrorStatus sendStringToExecute(AcApDocument* pAcTargetDocument,
                                       const ACHAR * pszExecute,
                                       bool bActivate = true,
                                       bool bWrapUpInactiveDoc = false,
                                       bool bEchoString = true);  
	static int  acutPrintf (const ACHAR *format, ...);
	static bool IsIllegalBlockNameChar(ACHAR ch);
	static void SafeCopy(ACHAR* pszDst, int nDstCount, const ACHAR* pszSrc, int nCount);
#ifdef _CAD2005
	static void safeVsnprintfVC7(char* szBuffer, size_t bufferSize, const char* pszFmt, va_list args);
#endif

};
